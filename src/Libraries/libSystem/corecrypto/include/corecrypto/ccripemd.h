#ifndef _CCRIPEMD_H_
#define _CCRIPEMD_H_

#include <corecrypto/ccdigest.h>

#define CCRMD_BLOCK_SIZE   64

#define CCRMD128_OUTPUT_SIZE  16
#define CCRMD160_OUTPUT_SIZE  20
#define CCRMD256_OUTPUT_SIZE  32
#define CCRMD320_OUTPUT_SIZE  40

#define CCRMD128_STATE_SIZE  16
#define CCRMD160_STATE_SIZE  20
#define CCRMD256_STATE_SIZE  32
#define CCRMD320_STATE_SIZE  40

extern const uint32_t ccrmd_initial_state[4];

extern const struct ccdigest_info ccrmd128_ltc_di;
extern const struct ccdigest_info ccrmd160_ltc_di;
extern const struct ccdigest_info ccrmd256_ltc_di;
extern const struct ccdigest_info ccrmd320_ltc_di;

#define ccrmd128_di ccrmd128_ltc_di
#define ccrmd160_di ccrmd160_ltc_di
#define ccrmd256_di ccrmd256_ltc_di
#define ccrmd320_di ccrmd320_ltc_di

#endif
