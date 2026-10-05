// host tests for PDKeyStoreFV: DER shapes against a macOS 26 VM, key wrap and PBKDF2 against OpenSSL
// build: c++ -I shim -I .. fvtest.cpp ../PDKeyStoreFV.cpp -lcrypto -o fvtest
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include "PDKeyStoreFV.h"
#include "vectors.h"

static int sFail;

static void
check(bool ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        sFail++;
}

static uint32_t
unhex(const char *s, uint8_t *out)
{
    uint32_t n = 0;

    for (; s[0] && s[1]; s += 2)
        sscanf(s, "%2hhx", &out[n++]);
    return n;
}

int
main(void)
{
    uint8_t secret[32], kid[16], salt[16], blob[FV_BLOB_MAX], key[32], want[512], out[512];
    uint32_t n, wn;
    FVBlob b;
    FVBytes items[8];

    for (int i = 0; i < 32; i++)
        secret[i] = (uint8_t)(i * 7 + 1);
    for (int i = 0; i < 16; i++) {
        kid[i] = (uint8_t)(0xa0 + i);
        salt[i] = (uint8_t)(0x10 + i);
    }
    fvSetSecret(secret);

    // sel 85 replies, byte for byte, given the real kid and id
    memset(&b, 0, sizeof(b));
    b.kind = FV_KIND_TOKEN;
    unhex("6a5a70e926884a838fcfef411dabc6d9", b.kid);
    unhex("eb8c5ade7a204e078c9d2742beaa1a6b", b.id);
    n = fvInfo(&b, out);
    wn = unhex(kTokenInfo, want);
    check(n == FV_INFO_LEN && n == wn && memcmp(out, want, n) == 0, "sel 85 access token reply matches the VM");
    b.kind = FV_KIND_USER;
    unhex("ffffeeeeddddccccbbbbaaaa00000000", b.kid);
    n = fvInfo(&b, out);
    wn = unhex(kUserInfo, want);
    check(n == FV_INFO_LEN && n == wn && memcmp(out, want, n) == 0, "sel 85 user verifier reply matches the VM");

    // request shapes
    wn = unhex(kIn75, want);
    n = fvSequence(want, wn, items, 8);
    check(n == 5 && items[0].len == 0 && items[1].len == 4 && items[2].len == FV_VERIFIER_LEN &&
        items[3].len == FV_TOKEN_LEN && items[4].len == 0, "sel 75 request: domain, password, verifier, token, empty");
    wn = unhex(kIn77, want);
    n = fvSequence(want, wn, items, 8);
    check(n == 5 && items[2].len == FV_VERIFIER_LEN && items[3].len == 16, "sel 77 request: domain, old pw, verifier, uuid, new pw");
    n = fvWrapSequence(items, 5, out, sizeof(out));
    check(n == wn && memcmp(out, want, n) == 0, "sequence writer reproduces the sel 77 request");

    // wrap and PBKDF2 against OpenSSL
    {
        uint8_t k[32], data[32], w[40], ow[40], d2[32];
        int ol = 0;
        EVP_CIPHER_CTX *c = EVP_CIPHER_CTX_new();

        for (int i = 0; i < 32; i++) {
            k[i] = (uint8_t)(3 * i);
            data[i] = (uint8_t)(255 - i);
        }
        fvWrap(k, data, w);
        EVP_EncryptInit_ex(c, EVP_aes_128_wrap(), NULL, k, NULL);
        EVP_EncryptUpdate(c, ow, &ol, data, 32);
        EVP_CIPHER_CTX_free(c);
        check(ol == 40 && memcmp(w, ow, 40) == 0, "RFC 3394 wrap matches OpenSSL");
        check(fvUnwrap(k, w, d2) && memcmp(d2, data, 32) == 0, "unwrap round trip");
        w[3] ^= 1;
        check(!fvUnwrap(k, w, d2), "unwrap rejects a damaged blob");

        uint8_t p1[32], p2[32];
        fvPbkdf2((const uint8_t *)"password", 8, salt, 16, 1000, p1);
        PKCS5_PBKDF2_HMAC("password", 8, salt, 16, 1000, EVP_sha256(), 32, p2);
        check(memcmp(p1, p2, 32) == 0, "PBKDF2-HMAC-SHA256 matches OpenSSL");
    }

    // verifiers: sizes, parse, right and wrong password, tamper
    {
        uint8_t vk[32];

        fvVolumeKey(vk);
        n = fvBuild(FV_KIND_USER, kid, (const uint8_t *)"right", 5, salt, blob);
        check(n == FV_VERIFIER_LEN, "user verifier is 162 bytes");
        check(fvParse(blob, n, &b) && b.kind == FV_KIND_USER && memcmp(b.kid, kid, 16) == 0, "user verifier parses");
        check(fvOpen(&b, (const uint8_t *)"right", 5, key) && memcmp(key, vk, 32) == 0, "right password opens it");
        check(!fvOpen(&b, (const uint8_t *)"wrong", 5, key), "wrong password does not");
        blob[100] ^= 1;
        check(!fvParse(blob, n, &b), "a changed verifier fails its MAC");
        n = fvBuild(FV_KIND_TOKEN, kid, NULL, 0, salt, blob);
        check(n == FV_TOKEN_LEN && fvParse(blob, n, &b) && b.kind == FV_KIND_TOKEN, "access token is 139 bytes and parses");
        check(fvOpen(&b, NULL, 0, key) && memcmp(key, vk, 32) == 0, "access token opens with the device secret");
        wn = unhex(kIn75, want);
        n = fvSequence(want, wn, items, 8);
        check(!fvParse(items[2].p, items[2].len, &b), "a real Mac verifier is rejected, not misread");
    }

    printf("%d failed\n", sFail);
    return sFail != 0;
}
