/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */
// FileVault verifiers for the AppleKeyStore stand-in. The blob layout and the sel 85 reply follow
// a macOS 26 VM byte for byte; what is inside the blob is our own: the volume key wrapped
// (RFC 3394) under a PBKDF2 password key, with an HMAC so foreign blobs are rejected
#include "PDKeyStoreFV.h"

#include <string.h>
#include <libkern/crypto/aes.h>
#include <libkern/crypto/sha2.h>

#define FV_ITERATIONS   4096

static uint8_t sFVSecret[32];

// the 6 bytes in front of the volume id: flags then the kind, as the real blobs carry them
static const uint8_t kUserTag[6] = { 0x49, 0x00, 0x00, 0x00, 0x02, 0x00 };
static const uint8_t kTokenTag[6] = { 0x01, 0x00, 0x00, 0x00, 0x01, 0x00 };

void
fvSetSecret(const uint8_t secret[32])
{
    memcpy(sFVSecret, secret, sizeof(sFVSecret));
}

static void
derive(const char *label, uint8_t out[32])
{
    fvHmac(sFVSecret, sizeof(sFVSecret), (const uint8_t *)label, (uint32_t)strlen(label), out);
}

void
fvVolumeKey(uint8_t key[32])
{
    derive("PDKeyStore FV volume key", key);
}

void
fvDeviceId(uint8_t id[16])
{
    uint8_t h[32];

    derive("PDKeyStore FV device id", h);
    memcpy(id, h, 16);
}

void
fvHmac(const uint8_t *key, uint32_t klen, const uint8_t *d, uint32_t dlen, uint8_t out[32])
{
    uint8_t k[64] = { 0 }, pad[64], inner[32];
    SHA256_CTX c;

    if (klen > sizeof(k)) {
        SHA256_Init(&c);
        SHA256_Update(&c, key, klen);
        SHA256_Final(k, &c);
    } else {
        memcpy(k, key, klen);
    }
    for (int i = 0; i < 64; i++)
        pad[i] = k[i] ^ 0x36;
    SHA256_Init(&c);
    SHA256_Update(&c, pad, sizeof(pad));
    SHA256_Update(&c, d, dlen);
    SHA256_Final(inner, &c);
    for (int i = 0; i < 64; i++)
        pad[i] = k[i] ^ 0x5c;
    SHA256_Init(&c);
    SHA256_Update(&c, pad, sizeof(pad));
    SHA256_Update(&c, inner, sizeof(inner));
    SHA256_Final(out, &c);
}

// one PBKDF2-HMAC-SHA256 block, all we need for a 32-byte key
void
fvPbkdf2(const uint8_t *pw, uint32_t pwlen, const uint8_t *salt, uint32_t slen, uint32_t iters, uint8_t out[32])
{
    uint8_t s[64], u[32];

    if (slen > sizeof(s) - 4)
        slen = sizeof(s) - 4;
    memcpy(s, salt, slen);
    s[slen] = 0;
    s[slen + 1] = 0;
    s[slen + 2] = 0;
    s[slen + 3] = 1;
    fvHmac(pw, pwlen, s, slen + 4, u);
    memcpy(out, u, 32);
    for (uint32_t i = 1; i < iters; i++) {
        fvHmac(pw, pwlen, u, sizeof(u), u);
        for (int j = 0; j < 32; j++)
            out[j] ^= u[j];
    }
}

// RFC 3394 with the first half of the key: PD's corecrypto only does AES-128
void
fvWrap(const uint8_t key[32], const uint8_t *in, uint8_t *out)
{
    static const uint8_t zero[16] = { 0 };
    aes_encrypt_ctx ctx;
    uint8_t a[8], r[4][8], blk[16];

    aes_encrypt_key128(key, &ctx);
    memset(a, 0xa6, 8);
    memcpy(r, in, 32);
    for (int j = 0; j < 6; j++) {
        for (int i = 0; i < 4; i++) {
            memcpy(blk, a, 8);
            memcpy(blk + 8, r[i], 8);
            aes_encrypt_cbc(blk, zero, 1, blk, &ctx);
            memcpy(a, blk, 8);
            a[7] ^= (uint8_t)(4 * j + i + 1);
            memcpy(r[i], blk + 8, 8);
        }
    }
    memcpy(out, a, 8);
    memcpy(out + 8, r, 32);
}

bool
fvUnwrap(const uint8_t key[32], const uint8_t *in, uint8_t *out)
{
    static const uint8_t zero[16] = { 0 };
    aes_decrypt_ctx ctx;
    uint8_t a[8], r[4][8], blk[16];

    aes_decrypt_key128(key, &ctx);
    memcpy(a, in, 8);
    memcpy(r, in + 8, 32);
    for (int j = 5; j >= 0; j--) {
        for (int i = 3; i >= 0; i--) {
            a[7] ^= (uint8_t)(4 * j + i + 1);
            memcpy(blk, a, 8);
            memcpy(blk + 8, r[i], 8);
            aes_decrypt_cbc(blk, zero, 1, blk, &ctx);
            memcpy(a, blk, 8);
            memcpy(r[i], blk + 8, 8);
        }
    }
    for (int i = 0; i < 8; i++) {
        if (a[i] != 0xa6)
            return false;
    }
    memcpy(out, r, 32);
    return true;
}

static uint8_t *
put(uint8_t *p, uint8_t tag, const void *v, uint32_t len)
{
    *p++ = tag;
    *p++ = (uint8_t)len;
    memcpy(p, v, len);
    return p + len;
}

// the key that opens a blob: the password for user verifiers, the device secret for the token
static void
blobKey(const FVBlob *blob, const uint8_t *pw, uint32_t pwlen, uint8_t key[32])
{
    if (blob->kind == FV_KIND_USER)
        fvPbkdf2(pw, pwlen, blob->salt, sizeof(blob->salt), blob->iterations, key);
    else
        derive("PDKeyStore FV access token", key);
}

static void
blobMac(const uint8_t *body, uint32_t len, uint8_t mac[32])
{
    uint8_t k[32];

    derive("PDKeyStore FV mac", k);
    fvHmac(k, sizeof(k), body, len, mac);
}

uint32_t
fvBuild(uint8_t kind, const uint8_t kid[16], const uint8_t *pw, uint32_t pwlen,
        const uint8_t salt[16], uint8_t *out)
{
    FVBlob blob;
    uint8_t key[32], vk[32], inner[112], mac[32], *p;
    uint32_t ilen, total;

    memset(&blob, 0, sizeof(blob));
    blob.kind = kind;
    blob.iterations = FV_ITERATIONS;
    memcpy(blob.salt, salt, 16);
    blobKey(&blob, pw, pwlen, key);
    fvVolumeKey(vk);
    fvWrap(key, vk, blob.wrapped);

    // the a3 member: version, kid, flags+kind+id, wrapped key, then iterations and salt for users
    p = inner;
    *p++ = 0xa3;
    *p++ = 0;
    p = put(p, 0x80, "", 1);
    p = put(p, 0x81, kid, 16);
    *p++ = 0x82;
    *p++ = 22;
    memcpy(p, kind == FV_KIND_USER ? kUserTag : kTokenTag, 6);
    fvDeviceId(p + 6);
    p += 22;
    p = put(p, 0x83, blob.wrapped, 40);
    if (kind == FV_KIND_USER) {
        uint8_t it[3] = { (uint8_t)(FV_ITERATIONS >> 16), (uint8_t)(FV_ITERATIONS >> 8), (uint8_t)FV_ITERATIONS };

        p = put(p, 0x84, it, 3);
        p = put(p, 0x85, salt, 16);
    }
    ilen = (uint32_t)(p - inner);
    inner[1] = (uint8_t)(ilen - 2);
    blobMac(inner, ilen, mac);

    total = 3 + 3 + 34 + 10 + ilen;
    p = out;
    *p++ = 0x30;
    *p++ = 0x81;
    *p++ = (uint8_t)(total - 3);
    p = put(p, 0x80, "", 1);
    p = put(p, 0x81, mac, 32);
    p = put(p, 0x82, kid, 8);
    memcpy(p, inner, ilen);
    return total;
}

// one TLV with short or 0x81/0x82 long-form lengths
static bool
tlv(const uint8_t **pp, const uint8_t *end, uint8_t *tag, const uint8_t **v, uint32_t *len)
{
    const uint8_t *p = *pp;
    uint32_t l;

    if (end - p < 2)
        return false;
    *tag = *p++;
    l = *p++;
    if (l == 0x81) {
        if (end - p < 1)
            return false;
        l = *p++;
    } else if (l == 0x82) {
        if (end - p < 2)
            return false;
        l = ((uint32_t)p[0] << 8) | p[1];
        p += 2;
    } else if (l & 0x80) {
        return false;
    }
    if ((uint32_t)(end - p) < l)
        return false;
    *v = p;
    *len = l;
    *pp = p + l;
    return true;
}


bool
fvParse(const uint8_t *b, uint32_t len, FVBlob *blob)
{
    const uint8_t *p = b, *end = b + len, *v, *mac = NULL, *inner = NULL, *iv = NULL;
    const uint8_t *tag6 = NULL;
    uint32_t l, ilen = 0, il = 0;
    uint8_t tag, want[32];

    memset(blob, 0, sizeof(*blob));
    if (!tlv(&p, end, &tag, &v, &l) || tag != 0x30)
        return false;
    end = v + l;
    p = v;
    while (p < end) {
        const uint8_t *start = p;

        if (!tlv(&p, end, &tag, &v, &l))
            return false;
        if (tag == 0x81 && l == 32)
            mac = v;
        if (tag == 0xa3) {
            inner = start;
            ilen = (uint32_t)(p - start);
            iv = v;
            il = l;
        }
    }
    if (mac == NULL || inner == NULL)
        return false;
    blobMac(inner, ilen, want);
    for (int i = 0; i < 32; i++) {
        if (want[i] != mac[i])
            return false;
    }

    p = iv;
    end = iv + il;
    while (p < end) {
        if (!tlv(&p, end, &tag, &v, &l))
            return false;
        if (tag == 0x81 && l == 16)
            memcpy(blob->kid, v, 16);
        else if (tag == 0x82 && l == 22)
            tag6 = v;
        else if (tag == 0x83 && l == 40)
            memcpy(blob->wrapped, v, 40);
        else if (tag == 0x84 && l == 3)
            blob->iterations = ((uint32_t)v[0] << 16) | ((uint32_t)v[1] << 8) | v[2];
        else if (tag == 0x85 && l == 16)
            memcpy(blob->salt, v, 16);
    }
    if (tag6 == NULL)
        return false;
    blob->kind = tag6[4];
    memcpy(blob->id, tag6 + 6, 16);
    if (blob->kind == FV_KIND_USER && blob->iterations == 0)
        return false;
    return blob->kind == FV_KIND_USER || blob->kind == FV_KIND_TOKEN;
}

bool
fvOpen(const FVBlob *blob, const uint8_t *pw, uint32_t pwlen, uint8_t key[32])
{
    uint8_t k[32];

    blobKey(blob, pw, pwlen, k);
    return fvUnwrap(k, blob->wrapped, key);
}

static uint8_t *
pairStart(uint8_t *p, const char *name, uint32_t vlen)
{
    uint32_t n = (uint32_t)strlen(name);

    *p++ = 0x30;
    *p++ = (uint8_t)(2 + n + vlen);
    *p++ = 0x0c;
    *p++ = (uint8_t)n;
    memcpy(p, name, n);
    return p + n;
}

static uint8_t *
pairInt(uint8_t *p, const char *name, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    int n = 4;

    // minimal positive DER integer
    while (n > 1 && b[4 - n] == 0 && !(b[5 - n] & 0x80))
        n--;
    if (b[4 - n] & 0x80)
        n++;
    p = pairStart(p, name, 2 + n);
    *p++ = 0x02;
    *p++ = (uint8_t)n;
    for (int i = n; i > 0; i--)
        *p++ = i > 4 ? 0 : b[4 - i];
    return p;
}

static uint8_t *
pairBytes(uint8_t *p, const char *name, const uint8_t *v, uint32_t len)
{
    p = pairStart(p, name, 2 + len);
    return put(p, 0x04, v, len);
}

// the sel 85 reply as a macOS 26 VM gives it for its access token and user verifiers
uint32_t
fvInfo(const FVBlob *blob, uint8_t *out)
{
    static const uint8_t zero[8] = { 0 };
    bool user = blob->kind == FV_KIND_USER;
    uint8_t *p = out + 3;

    p = pairInt(p, "cap", 0);
    p = pairInt(p, "f", user ? 0x18a1 : 0x1001);
    p = pairBytes(p, "id", blob->id, 16);
    p = pairBytes(p, "kid", blob->kid, 16);
    p = pairInt(p, "kt", blob->kind);
    p = pairInt(p, "kv", 0);
    p = pairInt(p, "mua", user ? 10 : 0);
    p = pairInt(p, "refc", user ? 1 : 0);
    p = pairInt(p, "sb", 0);
    p = pairInt(p, "sbo", 0);
    p = pairInt(p, "sfa", 0);
    p = pairInt(p, "sfao", 0);
    p = pairBytes(p, "sr", zero, 8);
    p = pairInt(p, "vgsg", 0);
    out[0] = 0x31;
    out[1] = 0x81;
    out[2] = (uint8_t)(p - out - 3);
    return (uint32_t)(p - out);
}

uint32_t
fvSequence(const uint8_t *b, uint32_t len, FVBytes *items, uint32_t max)
{
    const uint8_t *p = b, *end = b + len, *v;
    uint32_t l, n = 0;
    uint8_t tag;

    if (!tlv(&p, end, &tag, &v, &l) || tag != 0x30)
        return 0;
    p = v;
    end = v + l;
    while (p < end && n < max) {
        if (!tlv(&p, end, &tag, &v, &l) || tag != 0x04)
            return n;
        items[n].p = v;
        items[n].len = l;
        n++;
    }
    return n;
}

static uint32_t
lenSize(uint32_t l)
{
    return l < 0x80 ? 1 : l < 0x100 ? 2 : 3;
}

static uint8_t *
putLen(uint8_t *p, uint32_t l)
{
    if (l >= 0x100) {
        *p++ = 0x82;
        *p++ = (uint8_t)(l >> 8);
    } else if (l >= 0x80) {
        *p++ = 0x81;
    }
    *p++ = (uint8_t)l;
    return p;
}

uint32_t
fvWrapSequence(const FVBytes *items, uint32_t n, uint8_t *out, uint32_t cap)
{
    uint32_t body = 0, total;
    uint8_t *p = out;

    for (uint32_t i = 0; i < n; i++)
        body += 1 + lenSize(items[i].len) + items[i].len;
    total = 1 + lenSize(body) + body;
    if (total > cap)
        return 0;
    *p++ = 0x30;
    p = putLen(p, body);
    for (uint32_t i = 0; i < n; i++) {
        *p++ = 0x04;
        p = putLen(p, items[i].len);
        memcpy(p, items[i].p, items[i].len);
        p += items[i].len;
    }
    return total;
}
