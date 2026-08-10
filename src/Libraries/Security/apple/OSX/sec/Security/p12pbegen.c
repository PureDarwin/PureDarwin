/*
 * Copyright (c) 2007-2008,2010,2012-2013 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdlib.h>
#include <string.h> // memcpy

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

#include <corecrypto/ccn.h>

#include "p12pbegen.h"

#include <security_utilities/simulatecrash_assert.h>

static uint8_t *concatenate_to_blocksize(const uint8_t *data, size_t data_length, 
    size_t blocksize, size_t *blocklength)
{
    size_t block_length = blocksize * ((data_length + blocksize - 1) / blocksize);
    uint8_t *block_ptr, *block;
    block_ptr = block = malloc(block_length);
    if (!block_ptr)
        return NULL;
    while (block_ptr < block + block_length) {
        size_t bytes_to_move = block + block_length - block_ptr;
        memcpy(block_ptr, data, bytes_to_move > data_length ? data_length : bytes_to_move);
        block_ptr += data_length;
    }
    *blocklength = block_length;
    return block;
}

int p12_pbe_gen(CFStringRef passphrase, uint8_t *salt_ptr, size_t salt_length, 
    unsigned iter_count, P12_PBE_ID pbe_id, uint8_t *data, size_t length)
{
    unsigned int hash_blocksize = CC_SHA1_BLOCK_BYTES;
    unsigned int hash_outputsize = CC_SHA1_DIGEST_LENGTH;

    if (!passphrase)
        return -1;

    /* generate diversifier block */
    unsigned char diversifier[hash_blocksize];    
    memset(diversifier, pbe_id, sizeof(diversifier));

    /* convert passphrase to BE UTF16 and append double null */
    CFDataRef passphrase_be_unicode = CFStringCreateExternalRepresentation(kCFAllocatorDefault, passphrase, kCFStringEncodingUTF16BE, '\0');
    if (!passphrase_be_unicode)
        return -1;
    uint8_t null_termination[2] = { 0, 0 };
    CFMutableDataRef passphrase_be_unicode_null_term = CFDataCreateMutableCopy(NULL, 0, passphrase_be_unicode);
    CFRelease(passphrase_be_unicode);
    if (!passphrase_be_unicode_null_term)
        return -1;
    CFDataAppendBytes(passphrase_be_unicode_null_term, null_termination, sizeof(null_termination));

    /* generate passphrase block */
    uint8_t *passphrase_data = NULL;
    size_t passphrase_data_len = 0;
    size_t passphrase_length = CFDataGetLength(passphrase_be_unicode_null_term);
    const unsigned char *passphrase_ptr = CFDataGetBytePtr(passphrase_be_unicode_null_term);
    passphrase_data = concatenate_to_blocksize(passphrase_ptr, passphrase_length, hash_blocksize, &passphrase_data_len);
    CFRelease(passphrase_be_unicode_null_term);
    if (!passphrase_data)
        return -1;

    /* generate salt block */
    uint8_t *salt_data = NULL;
    size_t salt_data_len = 0;
    if (salt_length)
        salt_data = concatenate_to_blocksize(salt_ptr, salt_length, hash_blocksize, &salt_data_len);
    if (!salt_data){
        free(passphrase_data);
        return -1;
    }
    /* generate S||P block */
    size_t I_length = salt_data_len + passphrase_data_len;
    uint8_t *I_data = malloc(I_length);
    if (!I_data){
        free(salt_data);
        free(passphrase_data);
        return -1;
    }
    memcpy(I_data + 0, salt_data, salt_data_len);
    memcpy(I_data + salt_data_len, passphrase_data, passphrase_data_len);
    free(salt_data);
    free(passphrase_data);

    /* round up output buffer to multiple of hash block size and allocate */
    size_t hash_output_blocks = (length + hash_outputsize - 1) / hash_outputsize;
    size_t temp_buf_size = hash_output_blocks * hash_outputsize;
    uint8_t *temp_buf = malloc(temp_buf_size);
    uint8_t *cursor = temp_buf;
    if (!temp_buf){
        free(I_data);
        return -1;
    }
    /* 64 bits cast(s): worst case here is we dont hash all the data and incorectly derive the wrong key,
       when the passphrase + salt are over 2^32 bytes long */
    /* loop over output in hash_output_size increments */
    while (cursor < temp_buf + temp_buf_size) {
        CC_SHA1_CTX ctx;
        CC_SHA1_Init(&ctx);
        CC_SHA1_Update(&ctx, diversifier, (CC_LONG)sizeof(diversifier));
        assert(I_length<=UINT32_MAX); /* debug check. Correct as long as CC_LONG is uint32_t */
        CC_SHA1_Update(&ctx, I_data, (CC_LONG)I_length);
        CC_SHA1_Final(cursor, &ctx);

        /* run block through SHA-1 for iteration count */
        unsigned int i;
        for (i = 1; /*first round done above*/ i < iter_count; i++)
            CCDigest(kCCDigestSHA1, cursor, hash_outputsize, cursor);

        /*
         * b) Concatenate copies of A[i] to create a string B of 
         *    length v bits (the final copy of A[i]i may be truncated 
         *    to create B).
         */
        size_t A_i_len = 0;
        uint8_t *A_i = concatenate_to_blocksize(cursor, 
            hash_outputsize, hash_blocksize, &A_i_len);
        if (!A_i){
            free(I_data);
            free(temp_buf);
            return -1;
        }
        /*
         * c) Treating I as a concatenation I[0], I[1], ..., 
         *    I[k-1] of v-bit blocks, where k = ceil(s/v) + ceil(p/v),
         *    modify I by setting I[j]=(I[j]+B+1) mod (2 ** v)
         *    for each j.
         */

        /* tmp1 = B+1 */

        const cc_size tmp_n = ccn_nof_size(A_i_len + 1) > ccn_nof_size(hash_blocksize) ? ccn_nof_size(A_i_len + 1) : ccn_nof_size(hash_blocksize);
        cc_unit *tmp1 = (cc_unit *)malloc(tmp_n * sizeof(cc_unit));
        if (!tmp1) {
            free(A_i);
            free(I_data);
            free(temp_buf);
            return -1;
        }
        ccn_read_uint(tmp_n, tmp1, A_i_len, A_i);
        ccn_add1(tmp_n, tmp1, tmp1, 1);

        free(A_i);

        cc_unit *tmp2 = (cc_unit *)malloc(tmp_n * sizeof(cc_unit));
        if (!tmp2) {
            free(I_data);
            free(temp_buf);
            free(tmp1);
            return -1;
        }
        unsigned int j;
        for (j = 0; j < I_length; j+=hash_blocksize) {
            /* tempg = I[j];  */
            ccn_read_uint(tmp_n, tmp2, hash_blocksize, I_data + j);
            /* tempg += tmp1 */
            ccn_add(tmp_n, tmp2, tmp2, tmp1);
            
            /* I[j] = tempg mod 2**v
               Just clear all the high bits above 2**v
               In practice at most it rolled over by 1 bit, since all we did was add so
               we should only clear one bit at most.
             */
            size_t bitSize;
            const size_t hash_blocksize_bits = hash_blocksize * 8;
            while ((bitSize = ccn_bitlen(tmp_n, tmp2)) > hash_blocksize_bits)
            {
                ccn_set_bit(tmp2, bitSize - 1, 0);
            }

            ccn_write_uint_padded(tmp_n, tmp2, hash_blocksize, I_data + j);
        }

        cursor += hash_outputsize;
        free(tmp1);
        free(tmp2);
    }

    /*
     * 7. Concatenate A[1], A[2], ..., A[c] together to form a 
     *    pseudo-random bit string, A.
     *
     * 8. Use the first n bits of A as the output of this entire 
     *    process.
     */
    memmove(data, temp_buf, length);
    free(temp_buf);
    free(I_data);
    return 0;
}

#if 0
bool test() 
{
	//smeg => 0073006D006500670000
	CFStringRef password = CFSTR("smeg");
	//Salt (length 8):
	unsigned char salt_bytes[] = { 0x0A, 0x58, 0xCF, 0x64, 0x53, 0x0D, 0x82, 0x3F };
	CFDataRef salt = CFDataCreate(NULL, salt_bytes, sizeof(salt_bytes));
	// ID 1, ITER 1
	// Output KEY (length 24)
	unsigned char correct_result[] = { 0x8A, 0xAA, 0xE6, 0x29, 0x7B, 0x6C, 0xB0, 0x46, 0x42, 0xAB, 0x5B, 0x07, 0x78, 0x51, 0x28, 0x4E, 0xB7, 0x12, 0x8F, 0x1A, 0x2A, 0x7F, 0xBC, 0xA3 };
	unsigned char result[24];
	p12PbeGen(password, salt, 1, PBE_ID_Key, result, sizeof(result));
	if (memcmp(correct_result, result, sizeof(correct_result))) {
		printf("test failure\n");
		return false;
	}
	return true;
}

bool test2()
{
	CFStringRef password = CFSTR("queeg");
	unsigned char salt_bytes[] = { 0x05,0xDE,0xC9,0x59,0xAC,0xFF,0x72,0xF7 };
	CFDataRef salt = CFDataCreate(NULL, salt_bytes, sizeof(salt_bytes));
	unsigned char correct_result[] = { 0xED,0x20,0x34,0xE3,0x63,0x28,0x83,0x0F,0xF0,0x9D,0xF1,0xE1,0xA0,0x7D,0xD3,0x57,0x18,0x5D,0xAC,0x0D,0x4F,0x9E,0xB3,0xD4 };
	unsigned char result[24];
	p12PbeGen(password, salt, 1000, PBE_ID_Key, result, sizeof(result));
	if (memcmp(correct_result, result, sizeof(correct_result))) {
		printf("test failure\n");
		return false;
	}
	return true;
}

int main(int argc, char *argv[])
{
	test();
	test2();
}

#endif

/* http://www.drh-consultancy.demon.co.uk/test.txt

	Test Vectors set 1.

	Password: smeg

KEYGEN DEBUG
ID 1, ITER 1
Password (length 10):
0073006D006500670000
Salt (length 8):
0A58CF64530D823F
ID 1, ITER 1
Output KEY (length 24)
8AAAE6297B6CB04642AB5B077851284EB7128F1A2A7FBCA3

KEYGEN DEBUG
ID 2, ITER 1
Password (length 10):
0073006D006500670000
Salt (length 8):
0A58CF64530D823F
ID 2, ITER 1
Output KEY (length 8)
79993DFE048D3B76

KEYGEN DEBUG
ID 1, ITER 1
Password (length 10):
0073006D006500670000
Salt (length 8):
642B99AB44FB4B1F
ID 1, ITER 1
Output KEY (length 24)
F3A95FEC48D7711E985CFE67908C5AB79FA3D7C5CAA5D966

KEYGEN DEBUG
ID 2, ITER 1
Password (length 10):
0073006D006500670000
Salt (length 8):
642B99AB44FB4B1F
ID 2, ITER 1
Output KEY (length 8)
C0A38D64A79BEA1D

KEYGEN DEBUG
ID 3, ITER 1
Password (length 10):
0073006D006500670000
Salt (length 8):
3D83C0E4546AC140
ID 3, ITER 1
Output KEY (length 20)
8D967D88F6CAA9D714800AB3D48051D63F73A312

Test Vectors set 2.
Password: queeg

KEYGEN DEBUG
ID 1, ITER 1000
Password (length 12):
007100750065006500670000
Salt (length 8):
05DEC959ACFF72F7
ID 1, ITER 1000
Output KEY (length 24)
ED2034E36328830FF09DF1E1A07DD357185DAC0D4F9EB3D4

KEYGEN DEBUG
ID 2, ITER 1000
Password (length 12):
007100750065006500670000
Salt (length 8):
05DEC959ACFF72F7
ID 2, ITER 1000
Output KEY (length 8)
11DEDAD7758D4860

KEYGEN DEBUG
ID 1, ITER 1000
Password (length 12):
007100750065006500670000
Salt (length 8):
1682C0FC5B3F7EC5
ID 1, ITER 1000
Output KEY (length 24)
483DD6E919D7DE2E8E648BA8F862F3FBFBDC2BCB2C02957F

KEYGEN DEBUG
ID 2, ITER 1000
Password (length 12):
007100750065006500670000
Salt (length 8):
1682C0FC5B3F7EC5
ID 2, ITER 1000
Output KEY (length 8)
9D461D1B00355C50

KEYGEN DEBUG
ID 3, ITER 1000
Password (length 12):
007100750065006500670000
Salt (length 8):
263216FCC2FAB31C
ID 3, ITER 1000
Output KEY (length 20)
5EC4C7A80DF652294C3925B6489A7AB857C83476
*/

