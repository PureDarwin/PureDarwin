/*
 * crypt_sha512.c - the "$6$" SHA-512 password hash, as specified by Ulrich Drepper's
 * public-domain "Unix crypt using SHA-256 and SHA-512" (the scheme glibc and FreeBSD use).
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CRYPT_SHA512_HOST_TEST
// built on a Linux host against OpenSSL to check the spec's test vectors
#include <openssl/sha.h>
typedef SHA512_CTX digest_ctx;
#define digest_init(c) SHA512_Init(c)
#define digest_update(c, p, n) SHA512_Update(c, p, n)
#define digest_final(c, out) SHA512_Final(out, c)
#else
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>
typedef struct {
    ccdigest_ctx_decl(CCSHA512_STATE_SIZE, CCSHA512_BLOCK_SIZE, ctx);
} digest_ctx;
#define digest_init(c) ccdigest_init(ccsha512_di(), (c)->ctx)
#define digest_update(c, p, n) ccdigest_update(ccsha512_di(), (c)->ctx, (n), (p))
#define digest_final(c, out) ccdigest_final(ccsha512_di(), (c)->ctx, (out))
#endif

#define SALT_MAX 16
#define ROUNDS_DEFAULT 5000
#define ROUNDS_MIN 1000
#define ROUNDS_MAX 999999999
#define HASH_LEN 64

static const char itoa64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// "$6$" + "rounds=999999999$" + 16 salt + "$" + 86 hash + NUL
static char result[3 + 17 + SALT_MAX + 1 + 86 + 1];

static char *b64_from_24bit(char *out, unsigned char b2, unsigned char b1, unsigned char b0, int n)
{
    unsigned int w = ((unsigned int)b2 << 16) | ((unsigned int)b1 << 8) | b0;

    while (n-- > 0) {
        *out++ = itoa64[w & 0x3f];
        w >>= 6;
    }
    return out;
}

// the hash of `len` bytes made by repeating `block` (HASH_LEN bytes) as many times as needed
static void repeat_into(digest_ctx *ctx, const unsigned char *block, size_t len)
{
    for (; len >= HASH_LEN; len -= HASH_LEN)
        digest_update(ctx, block, HASH_LEN);
    digest_update(ctx, block, len);
}

char *__crypt_sha512(const char *key, const char *setting)
{
    unsigned char a[HASH_LEN], b[HASH_LEN], dp[HASH_LEN], ds[HASH_LEN];
    size_t keylen = strlen(key), saltlen;
    unsigned long rounds = ROUNDS_DEFAULT;
    int custom_rounds = 0;
    const char *salt;
    digest_ctx ctx;
    char *out;

    if (strncmp(setting, "$6$", 3) != 0)
        return NULL;
    salt = setting + 3;
    if (strncmp(salt, "rounds=", 7) == 0) {
        char *end;
        unsigned long r = strtoul(salt + 7, &end, 10);

        if (*end != '$')
            return NULL;
        rounds = r < ROUNDS_MIN ? ROUNDS_MIN : r > ROUNDS_MAX ? ROUNDS_MAX : r;
        custom_rounds = 1;
        salt = end + 1;
    }
    saltlen = strcspn(salt, "$");
    if (saltlen > SALT_MAX)
        saltlen = SALT_MAX;

    // B = H(key salt key)
    digest_init(&ctx);
    digest_update(&ctx, key, keylen);
    digest_update(&ctx, salt, saltlen);
    digest_update(&ctx, key, keylen);
    digest_final(&ctx, b);

    // A = H(key salt B-repeated-to-keylen, then B or key for each bit of keylen)
    digest_init(&ctx);
    digest_update(&ctx, key, keylen);
    digest_update(&ctx, salt, saltlen);
    repeat_into(&ctx, b, keylen);
    for (size_t n = keylen; n > 0; n >>= 1) {
        if (n & 1)
            digest_update(&ctx, b, HASH_LEN);
        else
            digest_update(&ctx, key, keylen);
    }
    digest_final(&ctx, a);

    // DP = H(key repeated keylen times), DS = H(salt repeated 16 + A[0] times)
    digest_init(&ctx);
    for (size_t i = 0; i < keylen; i++)
        digest_update(&ctx, key, keylen);
    digest_final(&ctx, dp);
    digest_init(&ctx);
    for (size_t i = 0; i < 16u + a[0]; i++)
        digest_update(&ctx, salt, saltlen);
    digest_final(&ctx, ds);

    // P and S are DP and DS repeated out to the key and salt lengths
    unsigned char p[keylen ? keylen : 1], s[SALT_MAX];

    for (size_t i = 0; i < keylen; i++)
        p[i] = dp[i % HASH_LEN];
    for (size_t i = 0; i < saltlen; i++)
        s[i] = ds[i];

    for (unsigned long r = 0; r < rounds; r++) {
        digest_init(&ctx);
        if (r & 1)
            digest_update(&ctx, p, keylen);
        else
            digest_update(&ctx, a, HASH_LEN);
        if (r % 3)
            digest_update(&ctx, s, saltlen);
        if (r % 7)
            digest_update(&ctx, p, keylen);
        if (r & 1)
            digest_update(&ctx, a, HASH_LEN);
        else
            digest_update(&ctx, p, keylen);
        digest_final(&ctx, a);
    }

    out = result;
    out += sprintf(out, "$6$");
    if (custom_rounds)
        out += sprintf(out, "rounds=%lu$", rounds);
    memcpy(out, salt, saltlen);
    out += saltlen;
    *out++ = '$';

    // the bytes go out in the scheme's fixed order, three at a time
    static const unsigned char order[21][3] = {
        {0, 21, 42}, {22, 43, 1}, {44, 2, 23}, {3, 24, 45}, {25, 46, 4}, {47, 5, 26}, {6, 27, 48},
        {28, 49, 7}, {50, 8, 29}, {9, 30, 51}, {31, 52, 10}, {53, 11, 32}, {12, 33, 54}, {34, 55, 13},
        {56, 14, 35}, {15, 36, 57}, {37, 58, 16}, {59, 17, 38}, {18, 39, 60}, {40, 61, 19}, {62, 20, 41},
    };

    for (int i = 0; i < 21; i++)
        out = b64_from_24bit(out, a[order[i][0]], a[order[i][1]], a[order[i][2]], 4);
    out = b64_from_24bit(out, 0, 0, a[63], 2);
    *out = '\0';

    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));
    memset(dp, 0, sizeof(dp));
    memset(p, 0, keylen);
    return result;
}

#ifdef CRYPT_SHA512_HOST_TEST
// the test vectors from the specification
int main(void)
{
    static const char *v[][3] = {
        {"$6$saltstring", "Hello world!", "$6$saltstring$svn8UoSVapNtMuq1ukKS4tPQd8iKwSMHWjl/O817G3uBnIFNjnQJuesI68u4OTLiBFdcbYEdFCoEOfaS35inz1"},
        {"$6$rounds=10000$saltstringsaltstring", "Hello world!", "$6$rounds=10000$saltstringsaltst$OW1/O6BYHV6BcXZu8QVeXbDWra3Oeqh0sbHbbMCVNSnCM/UrjmM0Dp8vOuZeHBy/YTBmSK6H9qs/y3RnOaw5v."},
        {"$6$rounds=5000$toolongsaltstring", "This is just a test", "$6$rounds=5000$toolongsaltstrin$lQ8jolhgVRVhY4b5pZKaysCLi0QBxGoNeKQzQ3glMhwllF7oGDZxUhx1yxdYcz/e1JSbq3y6JMxxl8audkUEm0"},
        {"$6$rounds=1400$anotherlongsaltstring", "a very much longer text to encrypt.  This one even stretches over morethan one line.", "$6$rounds=1400$anotherlongsalts$POfYwTEok97VWcjxIiSOjiykti.o/pQs.wPvMxQ6Fm7I6IoYN3CmLs66x9t0oSwbtEW7o7UmJEiDwGqd8p4ur1"},
        {"$6$rounds=77777$short", "we have a short salt string but not a short password", "$6$rounds=77777$short$WuQyW2YR.hBNpjjRhpYD/ifIw05xdfeEyQoMxIXbkvr0gge1a1x3yRULJ5CCaUeOxFmtlcGZelFl5CxtgfiAc0"},
        {"$6$rounds=123456$asaltof16chars..", "a short string", "$6$rounds=123456$asaltof16chars..$BtCwjqMJGx5hrJhZywWvt0RLE8uZ4oPwcelCjmw2kSYu.Ec6ycULevoBK25fs2xXgMNrCzIMVcgEJAstJeonj1"},
        {"$6$rounds=10$roundstoolow", "the minimum number is still observed", "$6$rounds=1000$roundstoolow$kUMsbe306n21p9R.FRkW3IGn.S9NPN0x50YhH1xhLsPuWGsUSklZt58jaTfF4ZEQpyUNGc0dqbpBYYBaHHrsX."},
    };
    int bad = 0;

    for (size_t i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
        const char *got = __crypt_sha512(v[i][1], v[i][0]);

        if (strcmp(got, v[i][2]) != 0) {
            printf("FAIL %zu\n  got  %s\n  want %s\n", i, got, v[i][2]);
            bad++;
        }
    }
    printf("%d of %zu vectors match\n", (int)(sizeof(v) / sizeof(v[0])) - bad, sizeof(v) / sizeof(v[0]));
    return bad != 0;
}
#endif
