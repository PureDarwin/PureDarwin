/*
 * Copyright (c) 2016-2017 Lubos Dolezel
 *
 * This file is part of Darling CoreCrypto.
 *
 * Darling is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <corecrypto/ccdigest.h>
#include <corecrypto/ccdigest_priv.h>
#include <string.h>
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#else
#include <endian.h>
#endif

#include <corecrypto/ccmd2.h>
#include <corecrypto/ccmd4.h>
#include <corecrypto/ccmd5.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccripemd.h>
#include <corecrypto/cc_priv.h>

#if CC_LITTLE_ENDIAN
static uint64_t swap64le(uint64_t v) { return v; }
static uint64_t swap64be(uint64_t v) { return CC_BSWAP64(v); }
#elif CC_BIG_ENDIAN
static uint64_t swap64be(uint64_t v) { return v; }
static uint64_t swap64le(uint64_t v) { return CC_BSWAP64(v); }
#else
#error Unknown endianess!
#endif

static void store32le(uint32_t v, void* dest) { CC_STORE32_LE(v, dest); }
static void store32be(uint32_t v, void* dest) { CC_STORE32_BE(v, dest); }

void ccdigest_final_64(const struct ccdigest_info *di, ccdigest_ctx_t ctx,
		                 unsigned char *digest,
						 uint64_t (*swap64)(uint64_t),
						 void (*store32)(uint32_t,void*))
{
	uint64_t nbits;

	// Add what we have left in to buffer to the total bits processed
	nbits = ccdigest_nbits(di, ctx) += ccdigest_num(di, ctx) * 8;

	// "Terminating" byte, see SHA1_Final()
	ccdigest_data(di, ctx)[ccdigest_num(di, ctx)++] = 0200;

	// Push in zeroes until there are exactly 56 bytes in the internal buffer
	if (ccdigest_num(di, ctx) != 56)
	{
		uint8_t zeroes[64];
		int count;

		count = (56 - ccdigest_num(di, ctx) + 64) % 64;
		memset(zeroes, 0, count);

		ccdigest_update(di, ctx, count, zeroes);
	}

	nbits = swap64(nbits);

	// This should flush the block
	ccdigest_update(di, ctx, sizeof(nbits), &nbits);

	for (int i = 0; i < di->output_size / sizeof(uint32_t); i++)
		store32(ccdigest_state_u32(di, ctx)[i], &digest[i*4]);
}

void ccdigest_final_64le(const struct ccdigest_info *di, ccdigest_ctx_t ctx,
                 unsigned char *digest) {
	ccdigest_final_64(di, ctx, digest, swap64le, store32le);
}
void ccdigest_final_64be(const struct ccdigest_info *di, ccdigest_ctx_t ctx,
                 unsigned char *digest) {
	ccdigest_final_64(di, ctx, digest, swap64be, store32be);
}

void ccdigest(const struct ccdigest_info* di, size_t len, const void* data, void* digest)
{
	ccdigest_di_decl(di, context);

	ccdigest_init(di, context);
	ccdigest_update(di, context, len, data);
	ccdigest_final(di, context, digest);

	ccdigest_di_clear(di, context);
}

void ccdigest_init(const struct ccdigest_info* di, ccdigest_ctx_t ctx)
{
	ccdigest_nbits(di, ctx) = 0;
	ccdigest_num(di, ctx) = 0;
	memcpy(ccdigest_state(di, ctx), di->initial_state, di->state_size);
}

static inline void ccdigest_submit_block(const struct ccdigest_info* di, ccdigest_ctx_t ctx, int nblocks, const void* data)
{
	di->compress(ccdigest_state(di, ctx), nblocks, data);
	ccdigest_nbits(di, ctx) += nblocks * di->block_size * 8;
}

void ccdigest_update(const struct ccdigest_info* di, ccdigest_ctx_t ctx, size_t len, const void* data)
{
	const uint8_t* ptr = (const uint8_t*) data;

	while (len > 0)
	{
		int buffered = ccdigest_num(di, ctx);
		// Do we have leftover bytes from last time?
		if (buffered > 0)
		{
			int tocopy = di->block_size - buffered;
			if (tocopy > len)
				tocopy = len;

			memcpy(ccdigest_data(di, ctx) + buffered, ptr, tocopy);

			buffered += tocopy;
			len -= tocopy;
			ptr += tocopy;

			ccdigest_num(di, ctx) = buffered;

			if (buffered == di->block_size)
			{
				// Submit to compression
				ccdigest_submit_block(di, ctx, 1, ccdigest_data(di, ctx));
				ccdigest_num(di, ctx) = 0; // nothing left in buffer
			}
		}
		else if (len >= di->block_size)
		{
			int nblocks = len / di->block_size;
			int bytes = nblocks * di->block_size;

			ccdigest_submit_block(di, ctx, nblocks, ptr);
			len -= bytes;
			ptr += bytes;
		}
		else
		{
			// Buffer the remaining data
			memcpy(ccdigest_data(di, ctx), ptr, len);
			ccdigest_num(di, ctx) = len;
			break;
		}
	}
}


int ccdigest_test(const struct ccdigest_info* di, size_t len, const void* data, const void* digest)
{
	return ccdigest_test_chunk(di, len, data, digest, len);
}

int ccdigest_test_chunk(const struct ccdigest_info* di, size_t len, const void* data, const void *digest, size_t chunk)
{
	const uint8_t* ptr = (const uint8_t*) data;
	unsigned long done;
	unsigned char* calc_digest = (unsigned char*) __builtin_alloca(di->output_size);
	ccdigest_di_decl(di, context);

	ccdigest_init(di, context);

	for (done = 0; done < len; done += chunk)
	{
		ccdigest_update(di, context, chunk, ptr);
		ptr += chunk;
	}

	ccdigest_final(di, context, calc_digest);

	ccdigest_di_clear(di, context);
	return memcmp(calc_digest, digest, di->output_size) == 0;
}

int ccdigest_test_vector(const struct ccdigest_info* di, const struct ccdigest_vector* v)
{
	return ccdigest_test(di, v->len, v->message, v->digest);
}

int ccdigest_test_chunk_vector(const struct ccdigest_info* di, const struct ccdigest_vector* v, size_t chunk)
{
	return ccdigest_test_chunk(di, v->len, v->message, v->digest, chunk);
}


const struct ccdigest_info* ccdigest_oid_lookup(ccoid_t oid)
{
	#define CC_OID_DI_PTR(x) &cc ## x ## _di
	#define CC_OID_DI_FUNC(x) cc ## x ## _di()
	#define CC_OID_CMP(x) if (ccdigest_oid_equal(x, oid)) { return x; }
	#define CC_OID_CMP_PTR(x) CC_OID_CMP(CC_OID_DI_PTR(x))
	#define CC_OID_CMP_FUNC(x) CC_OID_CMP(CC_OID_DI_FUNC(x))

	CC_OID_CMP_PTR(md2);
	CC_OID_CMP_PTR(md4);
	CC_OID_CMP_FUNC(md5);
	CC_OID_CMP_FUNC(sha1);
	CC_OID_CMP_FUNC(sha224);
	CC_OID_CMP_FUNC(sha256);
	CC_OID_CMP_FUNC(sha384);
	CC_OID_CMP_FUNC(sha512);
	CC_OID_CMP_PTR(rmd128);
	CC_OID_CMP_PTR(rmd160);
	CC_OID_CMP_PTR(rmd256);
	CC_OID_CMP_PTR(rmd320);

	return NULL;
};
