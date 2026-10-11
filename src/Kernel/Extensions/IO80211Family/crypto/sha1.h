// openbsd's sha1 names over libkern's
#ifndef PD80211_SHA1_H
#define PD80211_SHA1_H
#include <libkern/crypto/sha1.h>
#define SHA1_BLOCK_LENGTH	64
#define SHA1_DIGEST_LENGTH	SHA_DIGEST_LENGTH
#endif
