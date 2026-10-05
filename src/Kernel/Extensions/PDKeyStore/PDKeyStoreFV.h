/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */
// FileVault verifier blobs as opendirectoryd sees them: DER, the volume key wrapped under a
// password key. No IOKit here so the host tests can build it
#ifndef PDKEYSTOREFV_H
#define PDKEYSTOREFV_H

#include <stdint.h>
#include <stddef.h>

// verifiers are 162 bytes and the access token 139, like the real ones
#define FV_VERIFIER_LEN         162
#define FV_TOKEN_LEN            139
#define FV_BLOB_MAX             256
#define FV_KIND_TOKEN           1
#define FV_KIND_USER            2

// sel 85 reply, the DER set of { name, value } pairs, 177 bytes for both kinds
#define FV_INFO_LEN             177

// sel 75 reply: { key, status }
#define FV_UNWRAP_LEN           42

struct FVBlob {
    uint8_t  kind;
    uint8_t  kid[16];
    uint8_t  id[16];
    uint8_t  wrapped[40];
    uint32_t iterations;
    uint8_t  salt[16];
};

struct FVBytes {
    const uint8_t *p;
    uint32_t       len;
};

// the per-device secret everything is keyed from, set once by the kext
void     fvSetSecret(const uint8_t secret[32]);
void     fvVolumeKey(uint8_t key[32]);
void     fvDeviceId(uint8_t id[16]);

void     fvHmac(const uint8_t *key, uint32_t klen, const uint8_t *d, uint32_t dlen, uint8_t out[32]);
void     fvPbkdf2(const uint8_t *pw, uint32_t pwlen, const uint8_t *salt, uint32_t slen, uint32_t iters, uint8_t out[32]);
void     fvWrap(const uint8_t key[32], const uint8_t *in, uint8_t *out);
bool     fvUnwrap(const uint8_t key[32], const uint8_t *in, uint8_t *out);

// salt is caller-supplied so the tests are deterministic
uint32_t fvBuild(uint8_t kind, const uint8_t kid[16], const uint8_t *pw, uint32_t pwlen,
                 const uint8_t salt[16], uint8_t *out);
bool     fvParse(const uint8_t *b, uint32_t len, FVBlob *blob);
bool     fvOpen(const FVBlob *blob, const uint8_t *pw, uint32_t pwlen, uint8_t key[32]);
uint32_t fvInfo(const FVBlob *blob, uint8_t *out);

// reads the OCTET STRING members of a top-level SEQUENCE, returns how many
uint32_t fvSequence(const uint8_t *b, uint32_t len, FVBytes *items, uint32_t max);
// SEQUENCE { OCTET STRING ... } around the given parts
uint32_t fvWrapSequence(const FVBytes *items, uint32_t n, uint8_t *out, uint32_t cap);

#endif
