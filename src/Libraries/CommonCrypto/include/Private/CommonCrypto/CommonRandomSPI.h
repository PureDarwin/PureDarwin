// CommonRandomSPI.h - private CommonCrypto random number interfaces
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef _CC_RANDOM_SPI_H_
#define _CC_RANDOM_SPI_H_

#include <stdint.h>
#include <sys/types.h>
#include <CommonCrypto/CommonRandom.h>

#ifdef __cplusplus
extern "C" {
#endif

// opaque generator handle, every value selects the system ccrng
typedef const void *CCRandomRef;

extern const CCRandomRef kCCRandomDefault;
extern const CCRandomRef kCCRandomDevRandom;

// fills bytes from the system generator, 0 on success
int CCRandomCopyBytes(CCRandomRef rnd, void *bytes, size_t count);

// uniform value in [0, bound)
CCRNGStatus CCRandomUniform(uint64_t bound, uint64_t *rand);

struct ccrng_state;

// the shared corecrypto generator used across CommonCrypto
struct ccrng_state *ccDRBGGetRngState(void);
struct ccrng_state *ccDevRandomGetRngState(void);

#ifdef __cplusplus
}
#endif

#endif
