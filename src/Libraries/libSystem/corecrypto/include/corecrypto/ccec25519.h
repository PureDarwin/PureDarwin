#ifndef _CORECRYPTO_CCEC25519_H_
#define _CORECRYPTO_CCEC25519_H_

#include <corecrypto/cc.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccdigest.h>

typedef uint8_t ccec25519key[32];
typedef ccec25519key ccec25519secretkey;
typedef ccec25519key ccec25519pubkey;
typedef ccec25519key ccec25519base;

typedef uint8_t ccec25519signature[64];

// these parameters are all pretty much just uint8_t arrays, but since the types are there, we might as well use them ¯\_(ツ)_/¯
//
// shared_secret: OUT parameter
// private_key: IN parameter
// public_key: IN parameter
int cccurve25519(ccec25519key shared_secret, ccec25519secretkey private_key, ccec25519pubkey public_key);

// rng: REGULAR parameter
// public_key: OUT parameter
// private_key: OUT parameter
int cccurve25519_make_key_pair(struct ccrng_state* rng, ccec25519pubkey public_key, ccec25519secretkey private_key);

#endif

