/* Copyright (c) (2010,2011,2015-2019,2021,2022) Apple Inc. All rights reserved.
 *
 * corecrypto is licensed under Apple Inc.’s Internal Use License Agreement (which
 * is contained in the License.txt file distributed with corecrypto) and only to
 * people who accept that license. IMPORTANT:  Any license rights granted to you by
 * Apple Inc. (if any) are limited to internal use within your organization only on
 * devices and computers you own or control, for the sole purpose of verifying the
 * security characteristics and correct functioning of the Apple Software.  You may
 * not, directly or indirectly, redistribute the Apple Software or any portions thereof.
 */

/*
 * Parts of this code adapted from LibTomCrypt
 *
 * LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */

#include <corecrypto/ccsha2.h>
#include "cc_internal.h"
#include "ccsha2_internal.h"

#define K   ccsha512_K

/* Various logical functions */
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         CC_ROR64c(x, n)
#define R(x, n)         (((x) & 0xFFFFFFFFFFFFFFFF )>>((uint64_t)n))
#define Sigma0(x)       (S(x, 28) ^ S(x, 34) ^ S(x, 39))
#define Sigma1(x)       (S(x, 14) ^ S(x, 18) ^ S(x, 41))
#define Gamma0(x)       (S(x, 1) ^ S(x, 8) ^ R(x, 7))
#define Gamma1(x)       (S(x, 19) ^ S(x, 61) ^ R(x, 6))

/* compress 1024-bits */
void ccsha512_ltc_compress(ccdigest_state_t state, size_t nblocks, const void *in)
{
    uint64_t S[8], W[80], t0, t1;
    int i;
    uint64_t *s = ccdigest_u64(state);
    const uint8_t *buf = in;

    while(nblocks--) {
        /* copy state into S */
        for (i = 0; i < 8; i++) {
            S[i] = s[i];
        }

        /* copy the state into 1024-bits into W[0..15] */
        for (i = 0; i < 16; i++) {
            W[i] = cc_load64_be(buf + (8 * i));
        }

        /* fill W[16..79] */
        for (i = 16; i < 80; i++) {
            W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
        }

        /* Compress */
    #if CC_SMALL_CODE
        for (i = 0; i < 80; i++) {
            t0 = S[7] + Sigma1(S[4]) + Ch(S[4], S[5], S[6]) + K[i] + W[i];
            t1 = Sigma0(S[0]) + Maj(S[0], S[1], S[2]);
            S[7] = S[6];
            S[6] = S[5];
            S[5] = S[4];
            S[4] = S[3] + t0;
            S[3] = S[2];
            S[2] = S[1];
            S[1] = S[0];
            S[0] = t0 + t1;
        }
    #else
    #define RND(a,b,c,d,e,f,g,h,i)                    \
         t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];   \
         t1 = Sigma0(a) + Maj(a, b, c);                  \
         d += t0;                                        \
         h  = t0 + t1;

         for (i = 0; i < 80; i += 8) {
             RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],i+0);
             RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],i+1);
             RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],i+2);
             RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],i+3);
             RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],i+4);
             RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],i+5);
             RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],i+6);
             RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],i+7);
         }
    #endif


        /* feedback */
        for (i = 0; i < 8; i++) {
            s[i] = s[i] + S[i];
        }

        buf+=CCSHA512_BLOCK_SIZE;
    }
}

void ccsha512_final(const struct ccdigest_info *di, ccdigest_ctx_t ctx, unsigned char *digest)
{
    // Sanity check to recover from ctx corruptions.
    if (ccdigest_num(di, ctx) >= di->block_size) {
        ccdigest_num(di, ctx) = 0;
    }

    // Clone the state.
    ccdigest_di_decl(di, tmp);
    cc_memcpy(tmp, ctx, ccdigest_di_size(di));

    ccdigest_nbits(di, tmp) += ccdigest_num(di, tmp) << 3;
    ccdigest_data(di, tmp)[ccdigest_num(di, tmp)++] = 0x80;

    /* If we don't have at least 16 bytes (for the length) left we need to add
       a second block. */
    if (ccdigest_num(di, tmp) > di->block_size - 16) {
        while (ccdigest_num(di, tmp) < di->block_size) {
            ccdigest_data(di, tmp)[ccdigest_num(di, tmp)++] = 0;
        }
        di->compress(ccdigest_state(di, tmp), 1, ccdigest_data(di, tmp));
        ccdigest_num(di, tmp) = 0;
    }

    /* Pad up to block_size minus 8 with 0s */
    while (ccdigest_num(di, tmp) < di->block_size - 8) {
        ccdigest_data(di, tmp)[ccdigest_num(di, tmp)++] = 0;
    }

    cc_store64_be(ccdigest_nbits(di, tmp), ccdigest_data(di, tmp) + di->block_size - 8);
    di->compress(ccdigest_state(di, tmp), 1, ccdigest_data(di, tmp));

    /* Copy output */
    for (unsigned int i = 0; i < di->output_size / 8; i++) {
        cc_store64_be(ccdigest_state_u64(di, tmp)[i], digest + (8 * i));
    }

    ccdigest_di_clear(di, tmp);
}
