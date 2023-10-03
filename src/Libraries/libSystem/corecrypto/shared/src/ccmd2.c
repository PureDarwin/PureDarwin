/*
 * Copyright (c) 2016 Lubos Dolezel
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
#include <corecrypto/ccmd2.h>
#include <string.h>

const static uint8_t md2_initial_state[CCMD2_STATE_SIZE]; // all zeroes

static void md2_final(const struct ccdigest_info* di, ccdigest_ctx_t ctx, unsigned char* out);
static void md2_compress(ccdigest_state_t state, size_t nblocks, const void* in);

const struct ccdigest_info ccmd2_ltc_di = {
	.initial_state = md2_initial_state,
	.compress = md2_compress,
	.final = md2_final,
	.output_size = CCMD2_OUTPUT_SIZE,
	.state_size = CCMD2_STATE_SIZE,
	.block_size = CCMD2_BLOCK_SIZE,
	.oid = CC_DIGEST_OID_MD2,
	.oid_size = 10,
};

// Taken from RFC1319
static const unsigned char PI_SUBST[256] = {
		41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6,
		19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188,
		76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24,
		138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222, 73, 160, 251,
		245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63,
		148, 194, 16, 137, 11, 34, 95, 33, 128, 127, 93, 154, 90, 144, 50,
		39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165,
		181, 209, 215, 94, 146, 42, 172, 86, 170, 198, 79, 184, 56, 210,
		150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157,
		112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2, 27,
		96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
		85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197,
		234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65,
		129, 77, 82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123,
		8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
		203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228,
		166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180, 143, 237,
		31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

static const unsigned char *PADDING[] = {
		(unsigned char *)"",
		(unsigned char *)"\001",
		(unsigned char *)"\002\002",
		(unsigned char *)"\003\003\003",
		(unsigned char *)"\004\004\004\004",
		(unsigned char *)"\005\005\005\005\005",
		(unsigned char *)"\006\006\006\006\006\006",
		(unsigned char *)"\007\007\007\007\007\007\007",
		(unsigned char *)"\010\010\010\010\010\010\010\010",
		(unsigned char *)"\011\011\011\011\011\011\011\011\011",
		(unsigned char *)"\012\012\012\012\012\012\012\012\012\012",
		(unsigned char *)"\013\013\013\013\013\013\013\013\013\013\013",
		(unsigned char *)"\014\014\014\014\014\014\014\014\014\014\014\014",
		(unsigned char *)
				"\015\015\015\015\015\015\015\015\015\015\015\015\015",
		(unsigned char *)
				"\016\016\016\016\016\016\016\016\016\016\016\016\016\016",
		(unsigned char *)
				"\017\017\017\017\017\017\017\017\017\017\017\017\017\017\017",
		(unsigned char *)
				"\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020"
};

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned long int UINT4;


struct MD2_CTX {
	unsigned char state[16];                                 /* state */
	unsigned char checksum[16];                           /* checksum */
	unsigned int count;                 /* number of bytes, modulo 16 */
	unsigned char buffer[16];                         /* input buffer */
};

static void MD2Transform (unsigned char state[16], unsigned char checksum[16], const unsigned char block[16])
{
	unsigned int i, j, t;
	unsigned char x[48];

	/* Form encryption block from state, block, state ^ block.
	 */
	memcpy ((POINTER)x, (POINTER)state, 16);
	memcpy ((POINTER)x+16, (POINTER)block, 16);
	for (i = 0; i < 16; i++)
		x[i+32] = state[i] ^ block[i];

	/* Encrypt block (18 rounds).
	 */
	t = 0;
	for (i = 0; i < 18; i++) {
		for (j = 0; j < 48; j++)
			t = x[j] ^= PI_SUBST[t];
		t = (t + i) & 0xff;
	}
	/* Save new state */
	memcpy ((POINTER)state, (POINTER)x, 16);

	/* Update checksum.
	 */
	t = checksum[15];
	for (i = 0; i < 16; i++)
		t = checksum[i] ^= PI_SUBST[block[i] ^ t];

	/* Zeroize sensitive information.
	 */
	memset ((POINTER)x, 0, sizeof (x));
}


static void MD2Update(struct MD2_CTX* context, const unsigned char* input, unsigned int inputLen)
{
	unsigned int i, index, partLen;

	/* Update number of bytes mod 16 */
	index = context->count;
	context->count = (index + inputLen) & 0xf;

	partLen = 16 - index;

	/* Transform as many times as possible.
	  */
	if (inputLen >= partLen) {
		memcpy((POINTER)&context->buffer[index], (POINTER)input, partLen);
		MD2Transform (context->state, context->checksum, context->buffer);

		for (i = partLen; i + 15 < inputLen; i += 16)
			MD2Transform (context->state, context->checksum, &input[i]);

		index = 0;
	}
	else
		i = 0;

	/* Buffer remaining input */
	memcpy((POINTER)&context->buffer[index], (POINTER)&input[i], inputLen-i);
}

/* MD2 finalization. Ends an MD2 message-digest operation, writing the
     message digest and zeroizing the context.
 */
void MD2Final (unsigned char digest[16], struct MD2_CTX* context)
{
	unsigned int index, padLen;

	/* Pad out to multiple of 16.
	 */
	index = context->count;
	padLen = 16 - index;
	MD2Update (context, PADDING[padLen], padLen);

	/* Extend with checksum */
	MD2Update (context, context->checksum, 16);

	/* Store state in digest */
	memcpy ((POINTER)digest, (POINTER)context->state, 16);

	/* Zeroize sensitive information.
	 */
	memset ((POINTER)context, 0, sizeof (*context));
}

static void md2_final(const struct ccdigest_info* di, ccdigest_ctx_t ctx, unsigned char* out)
{
	struct MD2_CTX* mdctx = (struct MD2_CTX*) ccdigest_u8(ccdigest_state(di, ctx));
	struct ccdigest_state* state = ccdigest_state(di, ctx);

	// Push the remaining data
	MD2Update(mdctx, ccdigest_data(di, ctx), ccdigest_num(di, ctx));
	MD2Final(out, mdctx);
}

static void md2_compress(ccdigest_state_t state, size_t nblocks, const void* in)
{
	struct MD2_CTX* ctx = (struct MD2_CTX*) ccdigest_u8(state);
	unsigned int i;
	const unsigned char* pos = (const unsigned char*) in;

	for (i = 0; i < nblocks; i++)
	{
		MD2Update(ctx, pos, CCMD2_BLOCK_SIZE);
		pos += CCMD2_BLOCK_SIZE;
	}
}
