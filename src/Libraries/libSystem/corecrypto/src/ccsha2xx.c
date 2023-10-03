#include <corecrypto/ccsha2.h>

const uint32_t ccsha224_initial_state[8] = {
	0xC1059ED8, 0x367CD507, 0x3070DD17, 0xF70E5939,
	0xFFC00B31, 0x68581511, 0x64F98FA7, 0xBEFA4FA4
};
const uint32_t ccsha256_initial_state[8] = {
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

extern void ccdigest_final_64be(const struct ccdigest_info *di, ccdigest_ctx_t ctx,
                 unsigned char *digest);
void ccsha2xx_compress(ccdigest_state_t state, size_t nblocks, const void* in);

const struct ccdigest_info ccsha256_ltc_di = {
		.initial_state = ccsha256_initial_state,
		.compress = ccsha2xx_compress,
		.final = ccdigest_final_64be,
		.output_size = CCSHA256_OUTPUT_SIZE,
		.state_size = CCSHA256_STATE_SIZE,
		.block_size = CCSHA256_BLOCK_SIZE,
		.oid = CC_DIGEST_OID_SHA256,
		.oid_size = 10,
};

const struct ccdigest_info ccsha224_ltc_di = {
		.initial_state = ccsha224_initial_state,
		.compress = ccsha2xx_compress,
		.final = ccdigest_final_64be,
		.output_size = CCSHA224_OUTPUT_SIZE,
		.state_size = CCSHA256_STATE_SIZE,
		.block_size = CCSHA256_BLOCK_SIZE,
		.oid = CC_DIGEST_OID_SHA224,
		.oid_size = 10,
};

const struct ccdigest_info *ccsha224_di(void) {
	return &ccsha224_ltc_di;
}

const struct ccdigest_info *ccsha256_di(void) {
	return &ccsha256_ltc_di;
}

#define SHA256_SHR(bits,word)      ((word) >> (bits))
#define SHA256_ROTL(bits,word)                         \
  (((word) << (bits)) | ((word) >> (32-(bits))))
#define SHA256_ROTR(bits,word)                         \
  (((word) >> (bits)) | ((word) << (32-(bits))))

/* Define the SHA SIGMA and sigma macros */
#define SHA256_SIGMA0(word)   \
  (SHA256_ROTR( 2,word) ^ SHA256_ROTR(13,word) ^ SHA256_ROTR(22,word))
#define SHA256_SIGMA1(word)   \
  (SHA256_ROTR( 6,word) ^ SHA256_ROTR(11,word) ^ SHA256_ROTR(25,word))
#define SHA256_sigma0(word)   \
  (SHA256_ROTR( 7,word) ^ SHA256_ROTR(18,word) ^ SHA256_SHR( 3,word))
#define SHA256_sigma1(word)   \
  (SHA256_ROTR(17,word) ^ SHA256_ROTR(19,word) ^ SHA256_SHR(10,word))

#define SHA_Ch(x, y, z)      (((x) & ((y) ^ (z))) ^ (z))
#define SHA_Maj(x, y, z)     (((x) & ((y) | (z))) | ((y) & (z)))

static void ccsha2xx_compress_one(ccdigest_state_t state, const uint8_t* input)
{
	 /* Constants defined in FIPS 180-3, section 4.2.2 */
  static const uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
      0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
      0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
      0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
      0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
      0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
      0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
      0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
      0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
      0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
  };
  int        t, t4;                   /* Loop counter */
  uint32_t   temp1, temp2;            /* Temporary word value */
  uint32_t   W[64];                   /* Word sequence */
  uint32_t   A, B, C, D, E, F, G, H;  /* Word buffers */

  /*
   * Initialize the first 16 words in the array W
   */
  for (t = t4 = 0; t < 16; t++, t4 += 4)
    W[t] = (((uint32_t)input[t4]) << 24) |
           (((uint32_t)input[t4 + 1]) << 16) |
           (((uint32_t)input[t4 + 2]) << 8) |
           (((uint32_t)input[t4 + 3]));

  for (t = 16; t < 64; t++)
    W[t] = SHA256_sigma1(W[t-2]) + W[t-7] +
        SHA256_sigma0(W[t-15]) + W[t-16];

  A = ccdigest_u32(state)[0];
  B = ccdigest_u32(state)[1];
  C = ccdigest_u32(state)[2];
  D = ccdigest_u32(state)[3];
  E = ccdigest_u32(state)[4];
  F = ccdigest_u32(state)[5];
  G = ccdigest_u32(state)[6];
  H = ccdigest_u32(state)[7];

  for (t = 0; t < 64; t++) {
    temp1 = H + SHA256_SIGMA1(E) + SHA_Ch(E,F,G) + K[t] + W[t];
    temp2 = SHA256_SIGMA0(A) + SHA_Maj(A,B,C);
    H = G;
    G = F;
    F = E;
    E = D + temp1;
    D = C;
    C = B;
    B = A;
    A = temp1 + temp2;
  }

  ccdigest_u32(state)[0] += A;
  ccdigest_u32(state)[1] += B;
  ccdigest_u32(state)[2] += C;
  ccdigest_u32(state)[3] += D;
  ccdigest_u32(state)[4] += E;
  ccdigest_u32(state)[5] += F;
  ccdigest_u32(state)[6] += G;
  ccdigest_u32(state)[7] += H;
}

void ccsha2xx_compress(ccdigest_state_t state, size_t nblocks, const void* in)
{
	const uint8_t* input = (const uint8_t*) in;
	for (size_t i = 0; i < nblocks; i++)
		ccsha2xx_compress_one(state, input + CCSHA256_BLOCK_SIZE*i);
}

