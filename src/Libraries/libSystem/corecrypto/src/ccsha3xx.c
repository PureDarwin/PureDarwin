#include <corecrypto/ccsha2.h>

/* The implementation below is based on the sample code from RFC 6234 */

/* Initial Hash Values: FIPS 180-3 sections 5.3.4 and 5.3.5 */
static uint64_t ccsha384_initial_state[8] = {
    0xCBBB9D5DC1059ED8ll, 0x629A292A367CD507ll, 0x9159015A3070DD17ll,
    0x152FECD8F70E5939ll, 0x67332667FFC00B31ll, 0x8EB44A8768581511ll,
    0xDB0C2E0D64F98FA7ll, 0x47B5481DBEFA4FA4ll
};
static uint64_t ccsha512_initial_state[8] = {
    0x6A09E667F3BCC908ll, 0xBB67AE8584CAA73Bll, 0x3C6EF372FE94F82Bll,
    0xA54FF53A5F1D36F1ll, 0x510E527FADE682D1ll, 0x9B05688C2B3E6C1Fll,
    0x1F83D9ABFB41BD6Bll, 0x5BE0CD19137E2179ll
};

static void ccdigest_final_128be(const struct ccdigest_info* di, ccdigest_ctx_t ctx, unsigned char* digest) {
	// Add what we have left in to buffer to the total bits processed
	uint64_t nbits = ccdigest_nbits(di, ctx) += ccdigest_num(di, ctx) * 8;

	// terminating byte; equivalent to 0b10000000
	ccdigest_data(di, ctx)[ccdigest_num(di, ctx)++] = 128;

	// Push in zeroes until there are exactly 56 bytes in the internal buffer
	if (ccdigest_num(di, ctx) != 112) {
		uint8_t zeroes[128] = {0};
		uint64_t count = (112 - ccdigest_num(di, ctx) + 128) % 128;
		ccdigest_update(di, ctx, count, zeroes);
	}

	uint8_t len[16] = {0};

	for (size_t i = 0; i < sizeof(uint64_t); ++i)
		len[15 - i] = (nbits >> (i * 8)) & 0xff;

	ccdigest_update(di, ctx, sizeof len, len);

	for (size_t i = 0; i < di->output_size / sizeof(uint64_t); ++i) {
		uint64_t item = ccdigest_state_u64(di, ctx)[i];
		for (size_t j = 0; j < sizeof(uint64_t); ++j)
			digest[(i * sizeof(uint64_t)) + (7 - j)] = (item >> (j * 8)) & 0xff;
	}
};

static void ccsha3xx_compress(ccdigest_state_t state, size_t nblocks, const void *input);

const struct ccdigest_info ccsha384_ltc_di = {
		.initial_state = ccsha384_initial_state,
		.compress = ccsha3xx_compress,
		.final = ccdigest_final_128be,
		.output_size = CCSHA384_OUTPUT_SIZE,
		.state_size = CCSHA512_STATE_SIZE,
		.block_size = CCSHA512_BLOCK_SIZE,
		.oid = CC_DIGEST_OID_SHA384,
		.oid_size = 10,
};

const struct ccdigest_info ccsha512_ltc_di = {
		.initial_state = ccsha512_initial_state,
		.compress = ccsha3xx_compress,
		.final = ccdigest_final_128be,
		.output_size = CCSHA512_OUTPUT_SIZE,
		.state_size = CCSHA512_STATE_SIZE,
		.block_size = CCSHA512_BLOCK_SIZE,
		.oid = CC_DIGEST_OID_SHA512,
		.oid_size = 10,
};

const struct ccdigest_info *ccsha384_di(void) {
    return &ccsha384_ltc_di;
}

const struct ccdigest_info *ccsha512_di(void) {
    return &ccsha512_ltc_di;
}

/* Define the SHA shift, rotate left and rotate right macros */
#define SHA512_SHR(bits,word)  (((uint64_t)(word)) >> (bits))
#define SHA512_ROTR(bits,word) ((((uint64_t)(word)) >> (bits)) | \
                                (((uint64_t)(word)) << (64-(bits))))

/*
 * Define the SHA SIGMA and sigma macros
 *
 *  SHA512_ROTR(28,word) ^ SHA512_ROTR(34,word) ^ SHA512_ROTR(39,word)
 */
#define SHA512_SIGMA0(word)   \
 (SHA512_ROTR(28,word) ^ SHA512_ROTR(34,word) ^ SHA512_ROTR(39,word))
#define SHA512_SIGMA1(word)   \
 (SHA512_ROTR(14,word) ^ SHA512_ROTR(18,word) ^ SHA512_ROTR(41,word))
#define SHA512_sigma0(word)   \
 (SHA512_ROTR( 1,word) ^ SHA512_ROTR( 8,word) ^ SHA512_SHR( 7,word))
#define SHA512_sigma1(word)   \
 (SHA512_ROTR(19,word) ^ SHA512_ROTR(61,word) ^ SHA512_SHR( 6,word))

#define SHA_Ch(x, y, z)      (((x) & ((y) ^ (z))) ^ (z))
#define SHA_Maj(x, y, z)     (((x) & ((y) | (z))) | ((y) & (z)))


static void ccsha3xx_compress_one(ccdigest_state_t state, const uint8_t *input)
{
  /* Constants defined in FIPS 180-3, section 4.2.3 */
  static const uint64_t K[80] = {
      0x428A2F98D728AE22ll, 0x7137449123EF65CDll, 0xB5C0FBCFEC4D3B2Fll,
      0xE9B5DBA58189DBBCll, 0x3956C25BF348B538ll, 0x59F111F1B605D019ll,
      0x923F82A4AF194F9Bll, 0xAB1C5ED5DA6D8118ll, 0xD807AA98A3030242ll,
      0x12835B0145706FBEll, 0x243185BE4EE4B28Cll, 0x550C7DC3D5FFB4E2ll,
      0x72BE5D74F27B896Fll, 0x80DEB1FE3B1696B1ll, 0x9BDC06A725C71235ll,
      0xC19BF174CF692694ll, 0xE49B69C19EF14AD2ll, 0xEFBE4786384F25E3ll,
      0x0FC19DC68B8CD5B5ll, 0x240CA1CC77AC9C65ll, 0x2DE92C6F592B0275ll,
      0x4A7484AA6EA6E483ll, 0x5CB0A9DCBD41FBD4ll, 0x76F988DA831153B5ll,
      0x983E5152EE66DFABll, 0xA831C66D2DB43210ll, 0xB00327C898FB213Fll,
      0xBF597FC7BEEF0EE4ll, 0xC6E00BF33DA88FC2ll, 0xD5A79147930AA725ll,
      0x06CA6351E003826Fll, 0x142929670A0E6E70ll, 0x27B70A8546D22FFCll,
      0x2E1B21385C26C926ll, 0x4D2C6DFC5AC42AEDll, 0x53380D139D95B3DFll,
      0x650A73548BAF63DEll, 0x766A0ABB3C77B2A8ll, 0x81C2C92E47EDAEE6ll,
      0x92722C851482353Bll, 0xA2BFE8A14CF10364ll, 0xA81A664BBC423001ll,
      0xC24B8B70D0F89791ll, 0xC76C51A30654BE30ll, 0xD192E819D6EF5218ll,
      0xD69906245565A910ll, 0xF40E35855771202All, 0x106AA07032BBD1B8ll,
      0x19A4C116B8D2D0C8ll, 0x1E376C085141AB53ll, 0x2748774CDF8EEB99ll,
      0x34B0BCB5E19B48A8ll, 0x391C0CB3C5C95A63ll, 0x4ED8AA4AE3418ACBll,
      0x5B9CCA4F7763E373ll, 0x682E6FF3D6B2B8A3ll, 0x748F82EE5DEFB2FCll,
      0x78A5636F43172F60ll, 0x84C87814A1F0AB72ll, 0x8CC702081A6439ECll,
      0x90BEFFFA23631E28ll, 0xA4506CEBDE82BDE9ll, 0xBEF9A3F7B2C67915ll,
      0xC67178F2E372532Bll, 0xCA273ECEEA26619Cll, 0xD186B8C721C0C207ll,
      0xEADA7DD6CDE0EB1Ell, 0xF57D4F7FEE6ED178ll, 0x06F067AA72176FBAll,
      0x0A637DC5A2C898A6ll, 0x113F9804BEF90DAEll, 0x1B710B35131C471Bll,
      0x28DB77F523047D84ll, 0x32CAAB7B40C72493ll, 0x3C9EBE0A15C9BEBCll,
      0x431D67C49C100D4Cll, 0x4CC5D4BECB3E42B6ll, 0x597F299CFC657E2All,
      0x5FCB6FAB3AD6FAECll, 0x6C44198C4A475817ll
  };
  int        t, t8;                   /* Loop counter */
  uint64_t   temp1, temp2;            /* Temporary word value */
  uint64_t   W[80];                   /* Word sequence */
  uint64_t   A, B, C, D, E, F, G, H;  /* Word buffers */

  /*
   * Initialize the first 16 words in the array W
   */
  for (t = t8 = 0; t < 16; t++, t8 += 8)
    W[t] = ((uint64_t)(input[t8    ]) << 56) |
           ((uint64_t)(input[t8 + 1]) << 48) |
           ((uint64_t)(input[t8 + 2]) << 40) |
           ((uint64_t)(input[t8 + 3]) << 32) |
           ((uint64_t)(input[t8 + 4]) << 24) |
           ((uint64_t)(input[t8 + 5]) << 16) |
           ((uint64_t)(input[t8 + 6]) << 8 ) |
           ((uint64_t)(input[t8 + 7]));

  for (t = 16; t < 80; t++)
    W[t] = SHA512_sigma1(W[t-2]) + W[t-7] +
        SHA512_sigma0(W[t-15]) + W[t-16];
  A = ccdigest_u64(state)[0];
  B = ccdigest_u64(state)[1];
  C = ccdigest_u64(state)[2];
  D = ccdigest_u64(state)[3];
  E = ccdigest_u64(state)[4];
  F = ccdigest_u64(state)[5];
  G = ccdigest_u64(state)[6];
  H = ccdigest_u64(state)[7];

  for (t = 0; t < 80; t++) {
    temp1 = H + SHA512_SIGMA1(E) + SHA_Ch(E,F,G) + K[t] + W[t];
    temp2 = SHA512_SIGMA0(A) + SHA_Maj(A,B,C);
    H = G;
    G = F;
    F = E;
    E = D + temp1;
    D = C;
    C = B;
    B = A;
    A = temp1 + temp2;
  }

  ccdigest_u64(state)[0] += A;
  ccdigest_u64(state)[1] += B;
  ccdigest_u64(state)[2] += C;
  ccdigest_u64(state)[3] += D;
  ccdigest_u64(state)[4] += E;
  ccdigest_u64(state)[5] += F;
  ccdigest_u64(state)[6] += G;
  ccdigest_u64(state)[7] += H;
}

void ccsha3xx_compress(ccdigest_state_t state, size_t nblocks, const void *in)
{
	const uint8_t *input = (const uint8_t *) in;
	for (size_t i = 0; i < nblocks; i++)
		ccsha3xx_compress_one(state, input + CCSHA512_BLOCK_SIZE*i);
}
