// openbsd's sha256 names over libkern's
#ifndef PD80211_SHA2_H
#define PD80211_SHA2_H
#include <libkern/crypto/sha2.h>
typedef SHA256_CTX SHA2_CTX;
#define SHA256Init(c)		SHA256_Init(c)
#define SHA256Update(c, d, l)	SHA256_Update((c), (d), (l))
#define SHA256Final(d, c)	SHA256_Final((d), (c))
#endif
