#ifdef __APPLE__

#include_next <CommonCrypto/CommonDigest.h>

#else

#include "md5.h"
#include "assert.h"

#define CC_MD5_DIGEST_LENGTH 16
#define CC_MD5_CTX           md5_state_t

static int CC_MD5_Init(CC_MD5_CTX *c) {
    md5_init(c);
    return 1;
}

static int CC_MD5_Update(CC_MD5_CTX *c, const void *data,
                         unsigned long nbytes) {
    assert(nbytes <= 0x7fffffff && "would overflow");
    md5_append(c, (const unsigned char*)data, nbytes);
    return 1;
}

static int CC_MD5_Final(unsigned char digest[CC_MD5_DIGEST_LENGTH],
                        CC_MD5_CTX *c) {
    md5_finish(c, digest);
    return 1;
}

static unsigned char *CC_MD5(const void *data, unsigned long nbytes,
                             unsigned char *md) {
    static unsigned char smd[CC_MD5_DIGEST_LENGTH];

    if (!md)
        md = smd;

    assert(nbytes <= 0x7fffffff && "would overflow");

    CC_MD5_CTX c;
    CC_MD5_Init(&c);
    CC_MD5_Update(&c, data, nbytes);
    CC_MD5_Final(md, &c);

    return md;
}

#endif /* __APPLE__ */
