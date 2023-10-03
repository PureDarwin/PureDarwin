/* Code adapted from */
/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

#include <corecrypto/ccdes.h>
#include <corecrypto/ccmode_factory.h>
#include <corecrypto/cc_error.h>
#include <stdio.h>
#include <string.h>

CCMODE_GCM_FACTORY(des, encrypt);
CCMODE_GCM_FACTORY(des, decrypt);

CCMODE_CBC_FACTORY(des, encrypt);
CCMODE_CBC_FACTORY(des, decrypt);

CCMODE_CFB_FACTORY(des, cfb, encrypt);
CCMODE_CFB_FACTORY(des, cfb, decrypt);

CCMODE_CFB_FACTORY(des, cfb8, encrypt);
CCMODE_CFB_FACTORY(des, cfb8, decrypt);

CCMODE_XTS_FACTORY(des, encrypt);
CCMODE_XTS_FACTORY(des, decrypt);

CCMODE_CCM_FACTORY(des, encrypt);
CCMODE_CCM_FACTORY(des, decrypt);

CCMODE_CTR_FACTORY(des);

CCMODE_OFB_FACTORY(des);

static int ccdes_ecb_init(const struct ccmode_ecb* info, ccecb_ctx* ctx, size_t key_size, const void* key);
static int ccdes_ecb_encrypt(const ccecb_ctx* ctx, size_t block_count, const void* in, void* out);
static int ccdes_ecb_decrypt(const ccecb_ctx* ctx, size_t block_count, const void* in, void* out);

struct the_real_ccecb_ctx {
    uint32_t ek[32], dk[32];
};

const struct ccmode_ecb ccdes_ltc_ecb_encrypt_mode = {
	.block_size = 8,
	.size = sizeof(struct the_real_ccecb_ctx),
	.init = ccdes_ecb_init,
	.ecb = ccdes_ecb_encrypt,
};

const struct ccmode_ecb ccdes_ltc_ecb_decrypt_mode = {
	.block_size = 8,
	.size = sizeof(struct the_real_ccecb_ctx),
	.init = ccdes_ecb_init,
	.ecb = ccdes_ecb_decrypt,
};

const struct ccmode_ecb* ccdes_ecb_encrypt_mode(void) {
	return &ccdes_ltc_ecb_encrypt_mode;
};

const struct ccmode_ecb* ccdes_ecb_decrypt_mode(void) {
	return &ccdes_ltc_ecb_decrypt_mode;
};

#define EN0 0
#define DE1 1

#define STORE32H(x, y)                                                                     \
  do { (y)[0] = (uint8_t)(((x)>>24)&255); (y)[1] = (uint8_t)(((x)>>16)&255);   \
       (y)[2] = (uint8_t)(((x)>>8)&255); (y)[3] = (uint8_t)((x)&255); } while(0)

#define LOAD32H(x, y)                            \
  do { x = ((uint32_t)((y)[0] & 255)<<24) | \
           ((uint32_t)((y)[1] & 255)<<16) | \
           ((uint32_t)((y)[2] & 255)<<8)  | \
           ((uint32_t)((y)[3] & 255)); } while(0)

#define ROL(x, y) ( (((uint32_t)(x)<<(uint32_t)((y)&31)) | (((uint32_t)(x)&0xFFFFFFFFUL)>>(uint32_t)((32-((y)&31))&31))) & 0xFFFFFFFFUL)
#define ROR(x, y) ( ((((uint32_t)(x)&0xFFFFFFFFUL)>>(uint32_t)((y)&31)) | ((uint32_t)(x)<<(uint32_t)((32-((y)&31))&31))) & 0xFFFFFFFFUL)
#define ROLc(x, y) ( (((uint32_t)(x)<<(uint32_t)((y)&31)) | (((uint32_t)(x)&0xFFFFFFFFUL)>>(uint32_t)((32-((y)&31))&31))) & 0xFFFFFFFFUL)
#define RORc(x, y) ( ((((uint32_t)(x)&0xFFFFFFFFUL)>>(uint32_t)((y)&31)) | ((uint32_t)(x)<<(uint32_t)((32-((y)&31))&31))) & 0xFFFFFFFFUL)

static const uint8_t pc1[56] = {
    56, 48, 40, 32, 24, 16,  8,  0, 57, 49, 41, 33, 25, 17,
     9,  1, 58, 50, 42, 34, 26, 18, 10,  2, 59, 51, 43, 35,
    62, 54, 46, 38, 30, 22, 14,  6, 61, 53, 45, 37, 29, 21,
    13,  5, 60, 52, 44, 36, 28, 20, 12,  4, 27, 19, 11,  3
};

static const uint8_t totrot[16] = {
    1,   2,  4,  6,
    8,  10, 12, 14,
    15, 17, 19, 21,
    23, 25, 27, 28
};

static const uint8_t pc2[48] = {
    13, 16, 10, 23,  0,  4,      2, 27, 14,  5, 20,  9,
    22, 18, 11,  3, 25,  7,     15,  6, 26, 19, 12,  1,
    40, 51, 30, 36, 46, 54,     29, 39, 50, 44, 32, 47,
    43, 48, 38, 55, 33, 52,     45, 41, 49, 35, 28, 31
};

static const uint32_t SP1[64] =
{
    0x01010400UL, 0x00000000UL, 0x00010000UL, 0x01010404UL,
    0x01010004UL, 0x00010404UL, 0x00000004UL, 0x00010000UL,
    0x00000400UL, 0x01010400UL, 0x01010404UL, 0x00000400UL,
    0x01000404UL, 0x01010004UL, 0x01000000UL, 0x00000004UL,
    0x00000404UL, 0x01000400UL, 0x01000400UL, 0x00010400UL,
    0x00010400UL, 0x01010000UL, 0x01010000UL, 0x01000404UL,
    0x00010004UL, 0x01000004UL, 0x01000004UL, 0x00010004UL,
    0x00000000UL, 0x00000404UL, 0x00010404UL, 0x01000000UL,
    0x00010000UL, 0x01010404UL, 0x00000004UL, 0x01010000UL,
    0x01010400UL, 0x01000000UL, 0x01000000UL, 0x00000400UL,
    0x01010004UL, 0x00010000UL, 0x00010400UL, 0x01000004UL,
    0x00000400UL, 0x00000004UL, 0x01000404UL, 0x00010404UL,
    0x01010404UL, 0x00010004UL, 0x01010000UL, 0x01000404UL,
    0x01000004UL, 0x00000404UL, 0x00010404UL, 0x01010400UL,
    0x00000404UL, 0x01000400UL, 0x01000400UL, 0x00000000UL,
    0x00010004UL, 0x00010400UL, 0x00000000UL, 0x01010004UL
};

static const uint32_t SP2[64] =
{
    0x80108020UL, 0x80008000UL, 0x00008000UL, 0x00108020UL,
    0x00100000UL, 0x00000020UL, 0x80100020UL, 0x80008020UL,
    0x80000020UL, 0x80108020UL, 0x80108000UL, 0x80000000UL,
    0x80008000UL, 0x00100000UL, 0x00000020UL, 0x80100020UL,
    0x00108000UL, 0x00100020UL, 0x80008020UL, 0x00000000UL,
    0x80000000UL, 0x00008000UL, 0x00108020UL, 0x80100000UL,
    0x00100020UL, 0x80000020UL, 0x00000000UL, 0x00108000UL,
    0x00008020UL, 0x80108000UL, 0x80100000UL, 0x00008020UL,
    0x00000000UL, 0x00108020UL, 0x80100020UL, 0x00100000UL,
    0x80008020UL, 0x80100000UL, 0x80108000UL, 0x00008000UL,
    0x80100000UL, 0x80008000UL, 0x00000020UL, 0x80108020UL,
    0x00108020UL, 0x00000020UL, 0x00008000UL, 0x80000000UL,
    0x00008020UL, 0x80108000UL, 0x00100000UL, 0x80000020UL,
    0x00100020UL, 0x80008020UL, 0x80000020UL, 0x00100020UL,
    0x00108000UL, 0x00000000UL, 0x80008000UL, 0x00008020UL,
    0x80000000UL, 0x80100020UL, 0x80108020UL, 0x00108000UL
};

static const uint32_t SP3[64] =
{
    0x00000208UL, 0x08020200UL, 0x00000000UL, 0x08020008UL,
    0x08000200UL, 0x00000000UL, 0x00020208UL, 0x08000200UL,
    0x00020008UL, 0x08000008UL, 0x08000008UL, 0x00020000UL,
    0x08020208UL, 0x00020008UL, 0x08020000UL, 0x00000208UL,
    0x08000000UL, 0x00000008UL, 0x08020200UL, 0x00000200UL,
    0x00020200UL, 0x08020000UL, 0x08020008UL, 0x00020208UL,
    0x08000208UL, 0x00020200UL, 0x00020000UL, 0x08000208UL,
    0x00000008UL, 0x08020208UL, 0x00000200UL, 0x08000000UL,
    0x08020200UL, 0x08000000UL, 0x00020008UL, 0x00000208UL,
    0x00020000UL, 0x08020200UL, 0x08000200UL, 0x00000000UL,
    0x00000200UL, 0x00020008UL, 0x08020208UL, 0x08000200UL,
    0x08000008UL, 0x00000200UL, 0x00000000UL, 0x08020008UL,
    0x08000208UL, 0x00020000UL, 0x08000000UL, 0x08020208UL,
    0x00000008UL, 0x00020208UL, 0x00020200UL, 0x08000008UL,
    0x08020000UL, 0x08000208UL, 0x00000208UL, 0x08020000UL,
    0x00020208UL, 0x00000008UL, 0x08020008UL, 0x00020200UL
};

static const uint32_t SP4[64] =
{
    0x00802001UL, 0x00002081UL, 0x00002081UL, 0x00000080UL,
    0x00802080UL, 0x00800081UL, 0x00800001UL, 0x00002001UL,
    0x00000000UL, 0x00802000UL, 0x00802000UL, 0x00802081UL,
    0x00000081UL, 0x00000000UL, 0x00800080UL, 0x00800001UL,
    0x00000001UL, 0x00002000UL, 0x00800000UL, 0x00802001UL,
    0x00000080UL, 0x00800000UL, 0x00002001UL, 0x00002080UL,
    0x00800081UL, 0x00000001UL, 0x00002080UL, 0x00800080UL,
    0x00002000UL, 0x00802080UL, 0x00802081UL, 0x00000081UL,
    0x00800080UL, 0x00800001UL, 0x00802000UL, 0x00802081UL,
    0x00000081UL, 0x00000000UL, 0x00000000UL, 0x00802000UL,
    0x00002080UL, 0x00800080UL, 0x00800081UL, 0x00000001UL,
    0x00802001UL, 0x00002081UL, 0x00002081UL, 0x00000080UL,
    0x00802081UL, 0x00000081UL, 0x00000001UL, 0x00002000UL,
    0x00800001UL, 0x00002001UL, 0x00802080UL, 0x00800081UL,
    0x00002001UL, 0x00002080UL, 0x00800000UL, 0x00802001UL,
    0x00000080UL, 0x00800000UL, 0x00002000UL, 0x00802080UL
};

static const uint32_t SP5[64] =
{
    0x00000100UL, 0x02080100UL, 0x02080000UL, 0x42000100UL,
    0x00080000UL, 0x00000100UL, 0x40000000UL, 0x02080000UL,
    0x40080100UL, 0x00080000UL, 0x02000100UL, 0x40080100UL,
    0x42000100UL, 0x42080000UL, 0x00080100UL, 0x40000000UL,
    0x02000000UL, 0x40080000UL, 0x40080000UL, 0x00000000UL,
    0x40000100UL, 0x42080100UL, 0x42080100UL, 0x02000100UL,
    0x42080000UL, 0x40000100UL, 0x00000000UL, 0x42000000UL,
    0x02080100UL, 0x02000000UL, 0x42000000UL, 0x00080100UL,
    0x00080000UL, 0x42000100UL, 0x00000100UL, 0x02000000UL,
    0x40000000UL, 0x02080000UL, 0x42000100UL, 0x40080100UL,
    0x02000100UL, 0x40000000UL, 0x42080000UL, 0x02080100UL,
    0x40080100UL, 0x00000100UL, 0x02000000UL, 0x42080000UL,
    0x42080100UL, 0x00080100UL, 0x42000000UL, 0x42080100UL,
    0x02080000UL, 0x00000000UL, 0x40080000UL, 0x42000000UL,
    0x00080100UL, 0x02000100UL, 0x40000100UL, 0x00080000UL,
    0x00000000UL, 0x40080000UL, 0x02080100UL, 0x40000100UL
};

static const uint32_t SP6[64] =
{
    0x20000010UL, 0x20400000UL, 0x00004000UL, 0x20404010UL,
    0x20400000UL, 0x00000010UL, 0x20404010UL, 0x00400000UL,
    0x20004000UL, 0x00404010UL, 0x00400000UL, 0x20000010UL,
    0x00400010UL, 0x20004000UL, 0x20000000UL, 0x00004010UL,
    0x00000000UL, 0x00400010UL, 0x20004010UL, 0x00004000UL,
    0x00404000UL, 0x20004010UL, 0x00000010UL, 0x20400010UL,
    0x20400010UL, 0x00000000UL, 0x00404010UL, 0x20404000UL,
    0x00004010UL, 0x00404000UL, 0x20404000UL, 0x20000000UL,
    0x20004000UL, 0x00000010UL, 0x20400010UL, 0x00404000UL,
    0x20404010UL, 0x00400000UL, 0x00004010UL, 0x20000010UL,
    0x00400000UL, 0x20004000UL, 0x20000000UL, 0x00004010UL,
    0x20000010UL, 0x20404010UL, 0x00404000UL, 0x20400000UL,
    0x00404010UL, 0x20404000UL, 0x00000000UL, 0x20400010UL,
    0x00000010UL, 0x00004000UL, 0x20400000UL, 0x00404010UL,
    0x00004000UL, 0x00400010UL, 0x20004010UL, 0x00000000UL,
    0x20404000UL, 0x20000000UL, 0x00400010UL, 0x20004010UL
};

static const uint32_t SP7[64] =
{
    0x00200000UL, 0x04200002UL, 0x04000802UL, 0x00000000UL,
    0x00000800UL, 0x04000802UL, 0x00200802UL, 0x04200800UL,
    0x04200802UL, 0x00200000UL, 0x00000000UL, 0x04000002UL,
    0x00000002UL, 0x04000000UL, 0x04200002UL, 0x00000802UL,
    0x04000800UL, 0x00200802UL, 0x00200002UL, 0x04000800UL,
    0x04000002UL, 0x04200000UL, 0x04200800UL, 0x00200002UL,
    0x04200000UL, 0x00000800UL, 0x00000802UL, 0x04200802UL,
    0x00200800UL, 0x00000002UL, 0x04000000UL, 0x00200800UL,
    0x04000000UL, 0x00200800UL, 0x00200000UL, 0x04000802UL,
    0x04000802UL, 0x04200002UL, 0x04200002UL, 0x00000002UL,
    0x00200002UL, 0x04000000UL, 0x04000800UL, 0x00200000UL,
    0x04200800UL, 0x00000802UL, 0x00200802UL, 0x04200800UL,
    0x00000802UL, 0x04000002UL, 0x04200802UL, 0x04200000UL,
    0x00200800UL, 0x00000000UL, 0x00000002UL, 0x04200802UL,
    0x00000000UL, 0x00200802UL, 0x04200000UL, 0x00000800UL,
    0x04000002UL, 0x04000800UL, 0x00000800UL, 0x00200002UL
};

static const uint32_t SP8[64] =
{
    0x10001040UL, 0x00001000UL, 0x00040000UL, 0x10041040UL,
    0x10000000UL, 0x10001040UL, 0x00000040UL, 0x10000000UL,
    0x00040040UL, 0x10040000UL, 0x10041040UL, 0x00041000UL,
    0x10041000UL, 0x00041040UL, 0x00001000UL, 0x00000040UL,
    0x10040000UL, 0x10000040UL, 0x10001000UL, 0x00001040UL,
    0x00041000UL, 0x00040040UL, 0x10040040UL, 0x10041000UL,
    0x00001040UL, 0x00000000UL, 0x00000000UL, 0x10040040UL,
    0x10000040UL, 0x10001000UL, 0x00041040UL, 0x00040000UL,
    0x00041040UL, 0x00040000UL, 0x10041000UL, 0x00001000UL,
    0x00000040UL, 0x10040040UL, 0x00001000UL, 0x00041040UL,
    0x10001000UL, 0x00000040UL, 0x10000040UL, 0x10040000UL,
    0x10040040UL, 0x10000000UL, 0x00040000UL, 0x10001040UL,
    0x00000000UL, 0x10041040UL, 0x00040040UL, 0x10000040UL,
    0x10040000UL, 0x10001000UL, 0x10001040UL, 0x00000000UL,
    0x10041040UL, 0x00041000UL, 0x00041000UL, 0x00001040UL,
    0x00001040UL, 0x00040040UL, 0x10000000UL, 0x10041000UL
};

static const uint32_t bytebit[8] =
{
    0200, 0100, 040, 020, 010, 04, 02, 01
};

static const uint32_t bigbyte[24] =
{
    0x800000UL,  0x400000UL,  0x200000UL,  0x100000UL,
    0x80000UL,   0x40000UL,   0x20000UL,   0x10000UL,
    0x8000UL,    0x4000UL,    0x2000UL,    0x1000UL,
    0x800UL,     0x400UL,     0x200UL,     0x100UL,
    0x80UL,      0x40UL,      0x20UL,      0x10UL,
    0x8UL,       0x4UL,       0x2UL,       0x1L
};

CC_INLINE
void cookey(const uint32_t *raw1, uint32_t *keyout) {
    uint32_t *cook;
    const uint32_t *raw0;
    uint32_t dough[32];
    int i;

    cook = dough;
    for(i=0; i < 16; i++, raw1++)
    {
        raw0 = raw1++;
        *cook    = (*raw0 & 0x00fc0000L) << 6;
        *cook   |= (*raw0 & 0x00000fc0L) << 10;
        *cook   |= (*raw1 & 0x00fc0000L) >> 10;
        *cook++ |= (*raw1 & 0x00000fc0L) >> 6;
        *cook    = (*raw0 & 0x0003f000L) << 12;
        *cook   |= (*raw0 & 0x0000003fL) << 16;
        *cook   |= (*raw1 & 0x0003f000L) >> 4;
        *cook++ |= (*raw1 & 0x0000003fL);
    }

    memcpy(keyout, dough, sizeof(dough));
}

CC_INLINE
void deskey(const uint8_t *key, short edf, uint32_t *keyout) {
    uint32_t i, j, l, m, n, kn[32];
    uint8_t pc1m[56], pcr[56];

    for (j=0; j < 56; j++) {
        l = (uint32_t)pc1[j];
        m = l & 7;
        pc1m[j] = (uint8_t)((key[l >> 3U] & bytebit[m]) == bytebit[m] ? 1 : 0);
    }

    for (i=0; i < 16; i++) {
        if (edf == DE1) {
           m = (15 - i) << 1;
        } else {
           m = i << 1;
        }
        n = m + 1;
        kn[m] = kn[n] = 0L;
        for (j=0; j < 28; j++) {
            l = j + (uint32_t)totrot[i];
            if (l < 28) {
               pcr[j] = pc1m[l];
            } else {
               pcr[j] = pc1m[l - 28];
            }
        }
        for (/*j = 28*/; j < 56; j++) {
            l = j + (uint32_t)totrot[i];
            if (l < 56) {
               pcr[j] = pc1m[l];
            } else {
               pcr[j] = pc1m[l - 28];
            }
        }
        for (j=0; j < 24; j++)  {
            if ((int)pcr[(int)pc2[j]] != 0) {
               kn[m] |= bigbyte[j];
            }
            if ((int)pcr[(int)pc2[j+24]] != 0) {
               kn[n] |= bigbyte[j];
            }
        }
    }

    cookey(kn, keyout);
}

CC_INLINE
void desfunc(uint32_t *block, const uint32_t *keys) {
    uint32_t work, right, leftt;
    int cur_round;

    leftt = block[0];
    right = block[1];

    work = ((leftt >> 4)  ^ right) & 0x0f0f0f0fL;
    right ^= work;
    leftt ^= (work << 4);

    work = ((leftt >> 16) ^ right) & 0x0000ffffL;
    right ^= work;
    leftt ^= (work << 16);

    work = ((right >> 2)  ^ leftt) & 0x33333333L;
    leftt ^= work;
    right ^= (work << 2);

    work = ((right >> 8)  ^ leftt) & 0x00ff00ffL;
    leftt ^= work;
    right ^= (work << 8);

    right = ROLc(right, 1);
    work = (leftt ^ right) & 0xaaaaaaaaL;

    leftt ^= work;
    right ^= work;
    leftt = ROLc(leftt, 1);

    for (cur_round = 0; cur_round < 8; cur_round++) {
        work  = RORc(right, 4) ^ *keys++;
        leftt ^= SP7[work        & 0x3fL]
              ^  SP5[(work >>  8) & 0x3fL]
              ^  SP3[(work >> 16) & 0x3fL]
              ^  SP1[(work >> 24) & 0x3fL];
        work  = right ^ *keys++;
        leftt ^= SP8[ work        & 0x3fL]
              ^  SP6[(work >>  8) & 0x3fL]
              ^  SP4[(work >> 16) & 0x3fL]
              ^  SP2[(work >> 24) & 0x3fL];

        work = RORc(leftt, 4) ^ *keys++;
        right ^= SP7[ work        & 0x3fL]
              ^  SP5[(work >>  8) & 0x3fL]
              ^  SP3[(work >> 16) & 0x3fL]
              ^  SP1[(work >> 24) & 0x3fL];
        work  = leftt ^ *keys++;
        right ^= SP8[ work        & 0x3fL]
              ^  SP6[(work >>  8) & 0x3fL]
              ^  SP4[(work >> 16) & 0x3fL]
              ^  SP2[(work >> 24) & 0x3fL];
    }

    right = RORc(right, 1);
    work = (leftt ^ right) & 0xaaaaaaaaL;
    leftt ^= work;
    right ^= work;
    leftt = RORc(leftt, 1);
    work = ((leftt >> 8) ^ right) & 0x00ff00ffL;
    right ^= work;
    leftt ^= (work << 8);
    /* -- */
    work = ((leftt >> 2) ^ right) & 0x33333333L;
    right ^= work;
    leftt ^= (work << 2);
    work = ((right >> 16) ^ leftt) & 0x0000ffffL;
    leftt ^= work;
    right ^= (work << 16);
    work = ((right >> 4) ^ leftt) & 0x0f0f0f0fL;
    leftt ^= work;
    right ^= (work << 4);

    block[0] = right;
    block[1] = leftt;
}

static int ccdes_ecb_init(const struct ccmode_ecb* info, ccecb_ctx* ctx, size_t key_size, const void* key) {
	if (ctx == NULL) {
		return CCERR_PARAMETER;
	}

	if (key == NULL) {
		return CCERR_PARAMETER;
	}

	if (key_size != 8) {
		return CCERR_PARAMETER;
	}

	struct the_real_ccecb_ctx* real_ctx = (struct the_real_ccecb_ctx*)ctx;

	deskey((const uint8_t*)key, EN0, real_ctx->ek);
	deskey((const uint8_t*)key, DE1, real_ctx->dk);

	return CCERR_OK;
};

static int ccdes_ecb_encrypt(const ccecb_ctx* ctx, size_t block_count, const void* in, void* out) {
	if (ctx == NULL) {
		return CCERR_PARAMETER;
	}

	if (in == NULL) {
		return CCERR_PARAMETER;
	}

	if (out == NULL) {
		return CCERR_PARAMETER;
	}

	struct the_real_ccecb_ctx* real_ctx = (struct the_real_ccecb_ctx*)ctx;

	while (block_count > 0) {
		const uint8_t* pt = (const uint8_t*)in;
		uint8_t* ct = (uint8_t*)out;
		uint32_t work[2];

		LOAD32H(work[0], pt+0);
		LOAD32H(work[1], pt+4);
		desfunc(work, real_ctx->ek);
		STORE32H(work[0],ct+0);
		STORE32H(work[1],ct+4);

		in += 8;
		out += 8;
		--block_count;
	}

    return CCERR_OK;
};

static int ccdes_ecb_decrypt(const ccecb_ctx* ctx, size_t block_count, const void* in, void* out) {
	if (ctx == NULL) {
		return CCERR_PARAMETER;
	}

	if (in == NULL) {
		return CCERR_PARAMETER;
	}

	if (out == NULL) {
		return CCERR_PARAMETER;
	}

	struct the_real_ccecb_ctx* real_ctx = (struct the_real_ccecb_ctx*)ctx;

	while (block_count > 0) {
		const uint8_t* ct = (const uint8_t*)in;
		uint8_t* pt = (uint8_t*)out;
		uint32_t work[2];

		LOAD32H(work[0], ct+0);
		LOAD32H(work[1], ct+4);
		desfunc(work, real_ctx->dk);
		STORE32H(work[0],pt+0);
		STORE32H(work[1],pt+4);

		in += 8;
		out += 8;
		--block_count;
	}

    return CCERR_OK;
};

CCMODE_GCM_FACTORY(des3, encrypt);
CCMODE_GCM_FACTORY(des3, decrypt);

CCMODE_CBC_FACTORY(des3, encrypt);
CCMODE_CBC_FACTORY(des3, decrypt);

CCMODE_CFB_FACTORY(des3, cfb, encrypt);
CCMODE_CFB_FACTORY(des3, cfb, decrypt);

CCMODE_CFB_FACTORY(des3, cfb8, encrypt);
CCMODE_CFB_FACTORY(des3, cfb8, decrypt);

CCMODE_XTS_FACTORY(des3, encrypt);
CCMODE_XTS_FACTORY(des3, decrypt);

CCMODE_CCM_FACTORY(des3, encrypt);
CCMODE_CCM_FACTORY(des3, decrypt);

CCMODE_CTR_FACTORY(des3);

CCMODE_OFB_FACTORY(des3);

static int ccdes3_ecb_init(const struct ccmode_ecb* info, ccecb_ctx* ctx, size_t key_size, const void* key);
static int ccdes3_ecb_encrypt(const ccecb_ctx* ctx, size_t block_count, const void* in, void* out);
static int ccdes3_ecb_decrypt(const ccecb_ctx* ctx, size_t block_count, const void* in, void* out);

struct the_real_ccecb_ctx_3 {
    uint32_t ek[3][32], dk[3][32];
};

const struct ccmode_ecb ccdes3_ltc_ecb_encrypt_mode = {
	.block_size = 8,
	.size = sizeof(struct the_real_ccecb_ctx_3),
	.init = ccdes3_ecb_init,
	.ecb = ccdes3_ecb_encrypt,
};

const struct ccmode_ecb ccdes3_ltc_ecb_decrypt_mode = {
	.block_size = 8,
	.size = sizeof(struct the_real_ccecb_ctx_3),
	.init = ccdes3_ecb_init,
	.ecb = ccdes3_ecb_decrypt,
};

const struct ccmode_ecb* ccdes3_ecb_encrypt_mode(void) {
	return &ccdes3_ltc_ecb_encrypt_mode;
};

const struct ccmode_ecb* ccdes3_ecb_decrypt_mode(void) {
	return &ccdes3_ltc_ecb_decrypt_mode;
};

static int ccdes3_ecb_init(const struct ccmode_ecb* info, ccecb_ctx* ctx, size_t key_size, const void* key) {
	if (ctx == NULL) {
		return CCERR_PARAMETER;
	}

	if (key == NULL) {
		return CCERR_PARAMETER;
	}

	if (key_size != 16 && key_size != 24) {
		return CCERR_PARAMETER;
	}

	struct the_real_ccecb_ctx_3* real_ctx = (struct the_real_ccecb_ctx_3*)ctx;

    deskey((const uint8_t*)key,    EN0, real_ctx->ek[0]);
    deskey((const uint8_t*)key+8,  DE1, real_ctx->ek[1]);
    if (key_size == 24) {
        deskey(key+16, EN0, real_ctx->ek[2]);
    } else {
        /* two-key 3DES: K3=K1 */
        deskey((const uint8_t*)key, EN0, real_ctx->ek[2]);
    }

    deskey((const uint8_t*)key,    DE1, real_ctx->dk[2]);
    deskey((const uint8_t*)key+8,  EN0, real_ctx->dk[1]);
    if (key_size == 24) {
        deskey((const uint8_t*)key+16, DE1, real_ctx->dk[0]);
    } else {
        /* two-key 3DES: K3=K1 */
        deskey((const uint8_t*)key, DE1, real_ctx->dk[0]);
    }

	return CCERR_OK;
};

static int ccdes3_ecb_encrypt(const ccecb_ctx* ctx, size_t block_count, const void* in, void* out) {
	if (ctx == NULL) {
		return CCERR_PARAMETER;
	}

	if (in == NULL) {
		return CCERR_PARAMETER;
	}

	if (out == NULL) {
		return CCERR_PARAMETER;
	}

	struct the_real_ccecb_ctx_3* real_ctx = (struct the_real_ccecb_ctx_3*)ctx;

	while (block_count > 0) {
		const uint8_t* pt = (const uint8_t*)in;
		uint8_t* ct = (uint8_t*)out;
		uint32_t work[2];

		LOAD32H(work[0], pt+0);
		LOAD32H(work[1], pt+4);
		desfunc(work, real_ctx->ek[0]);
		desfunc(work, real_ctx->ek[1]);
		desfunc(work, real_ctx->ek[2]);
		STORE32H(work[0],ct+0);
		STORE32H(work[1],ct+4);

		in += 8;
		out += 8;
		--block_count;
	}

    return CCERR_OK;
};

static int ccdes3_ecb_decrypt(const ccecb_ctx* ctx, size_t block_count, const void* in, void* out) {
	if (ctx == NULL) {
		return CCERR_PARAMETER;
	}

	if (in == NULL) {
		return CCERR_PARAMETER;
	}

	if (out == NULL) {
		return CCERR_PARAMETER;
	}

	struct the_real_ccecb_ctx_3* real_ctx = (struct the_real_ccecb_ctx_3*)ctx;

	while (block_count > 0) {
		const uint8_t* ct = (const uint8_t*)in;
		uint8_t* pt = (uint8_t*)out;
		uint32_t work[2];

		LOAD32H(work[0], ct+0);
		LOAD32H(work[1], ct+4);
		desfunc(work, real_ctx->dk[0]);
		desfunc(work, real_ctx->dk[1]);
		desfunc(work, real_ctx->dk[2]);
		STORE32H(work[0],pt+0);
		STORE32H(work[1],pt+4);

		in += 8;
		out += 8;
		--block_count;
	}

    return CCERR_OK;
};

int ccdes_key_is_weak(void *key, size_t  length) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccdes_key_set_odd_parity(void *key, size_t length) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

uint32_t
ccdes_cbc_cksum(void *in, void *out, size_t length,
                void *key, size_t keylen, void *ivec) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

