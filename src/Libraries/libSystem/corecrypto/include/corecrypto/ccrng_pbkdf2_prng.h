#ifndef _CCRNG_PBKDF2_PRNG_H_
#define _CCRNG_PBKDF2_PRNG_H_

#include <corecrypto/ccrng.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccpbkdf2.h>

#define CCRNG_PBKDF2_BUFFER 4096


struct ccrng_pbkdf2_prng_state {
    CCRNG_STATE_COMMON
    size_t random_buffer_size;
    uint8_t random_buffer[CCRNG_PBKDF2_BUFFER];
};

int ccrng_pbkdf2_prng_init(struct ccrng_pbkdf2_prng_state *rng, size_t maxbytes,
                           unsigned long passwordLen, const void *password,
                           unsigned long saltLen, const void *salt,
                           unsigned long iterations);

#endif
