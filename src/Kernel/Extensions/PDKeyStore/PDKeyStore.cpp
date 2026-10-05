/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */
// Stand-in for the AppleKeyStore service. Keeps keybags in memory and answers the
// selectors the way a macOS 26 guest does during Setup Assistant and login
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOKitKeys.h>
#include <libkern/crypto/aes.h>
#include <libkern/crypto/sha2.h>
#include <libkern/crypto/rand.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <pexpert/pexpert.h>
#include <sys/systm.h>
#include "PDKeyStoreFV.h"

// handles clients pass for "my session" and for the device bag
#define AKS_HANDLE_SESSION      (-3)
#define AKS_HANDLE_DEVICE       (-6)
#define AKS_HANDLE_DEVICE_ALT   (-1)

#define AKS_ERR_NO_SESSION      ((IOReturn)0xe00002c2)
#define AKS_ERR_NOT_FOUND       ((IOReturn)0xe00002f0)
#define AKS_ERR_NOT_PERMITTED   ((IOReturn)0xe00002bc)
#define AKS_ERR_SEP_POLICY      ((IOReturn)0xe007c01e)
#define AKS_ERR_BAD_PASSWORD    ((IOReturn)0xe00002ce)

// sel 84 is the FileVault command: scalars { op, 0, DER params address, DER length }
#define AKS_FV_OWNERS_EXIST     0x11
#define AKS_FV_SIDP_STATUS      0x1a

#define AKS_STATE_DEVICE        0x4
#define AKS_STATE_SESSION       0x6000004
#define AKS_MAX_BAGS            16

struct PDKeyBag {
    bool     used;
    int64_t  handle;    // creation handle, 0 once released
    int64_t  uid;       // -1 until bound to a user session
    uint8_t  uuid[16];
};

class AppleKeyStore : public IOService
{
    OSDeclareDefaultStructors(AppleKeyStore)

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

class AppleKeyStoreUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(AppleKeyStoreUserClient)

public:
    virtual IOReturn clientClose() APPLE_KEXT_OVERRIDE;
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                    IOExternalMethodDispatch *dispatch, OSObject *target,
                                    void *reference) APPLE_KEXT_OVERRIDE;
};

static IOLock  *sLock;
static PDKeyBag sBags[AKS_MAX_BAGS];
static PDKeyBag sDeviceBag = { true, 0, -1, { 0 } };
static int64_t  sLastSessionUid = -1;
static uint8_t  sSecret[32];
static bool     sSecretReady;

// fv op 0x11 answers an empty owner list; boot-arg pdaksowners=<IOReturn> returns that code instead
// (0xe00002c7 only makes opendirectoryd log "Unable to determine ownership state", it still adds a verifier)
static IOReturn sOwnersReply = kIOReturnSuccess;

// the wrapping secret comes from the platform UUID, computed once so it can't change under a bag
static void
loadSecret()
{
    SHA256_CTX c;
    IOService *pe;
    OSString *uuid = NULL;

    if (sSecretReady)
        return;
    pe = IOService::getPlatform();
    if (pe != NULL)
        uuid = OSDynamicCast(OSString, pe->getProvider() ? pe->getProvider()->getProperty(kIOPlatformUUIDKey) : NULL);

    SHA256_Init(&c);
    SHA256_Update(&c, "PDKeyStore", 10);
    if (uuid != NULL)
        SHA256_Update(&c, uuid->getCStringNoCopy(), uuid->getLength());
    SHA256_Final(sSecret, &c);
    fvSetSecret(sSecret);
    sSecretReady = true;
}

static PDKeyBag *
bagForUid(int64_t uid)
{
    for (int i = 0; i < AKS_MAX_BAGS; i++) {
        if (sBags[i].used && sBags[i].uid == uid)
            return &sBags[i];
    }
    return NULL;
}

// -3 is the caller's session; root callers such as loginwindow get the newest session
static PDKeyBag *
resolve(int64_t h)
{
    PDKeyBag *b;

    if (h == AKS_HANDLE_DEVICE || h == AKS_HANDLE_DEVICE_ALT)
        return &sDeviceBag;
    if (h == AKS_HANDLE_SESSION) {
        b = bagForUid(kauth_cred_getuid(kauth_cred_get()));
        if (b == NULL && sLastSessionUid >= 0)
            b = bagForUid(sLastSessionUid);
        return b;
    }
    if (h < 0)
        return bagForUid(-h);

    for (int i = 0; i < AKS_MAX_BAGS; i++) {
        if (sBags[i].used && sBags[i].handle == h)
            return &sBags[i];
    }
    return NULL;
}

static int64_t
publicHandle(PDKeyBag *b)
{
    if (b == &sDeviceBag)
        return AKS_HANDLE_DEVICE;
    return b->uid >= 0 ? -b->uid : b->handle;
}

// DER set of { UTF8String name, value } pairs, the shape of the state replies
struct DerOut {
    uint8_t  buf[512];
    uint32_t len;
};

static void
derPair(DerOut *o, const char *name, uint8_t tag, const uint8_t *v, uint32_t vlen)
{
    uint32_t nlen = (uint32_t)strlen(name);

    o->buf[o->len++] = 0x30;
    o->buf[o->len++] = (uint8_t)(2 + nlen + 2 + vlen);
    o->buf[o->len++] = 0x0c;
    o->buf[o->len++] = (uint8_t)nlen;
    memcpy(o->buf + o->len, name, nlen);
    o->len += nlen;
    o->buf[o->len++] = tag;
    o->buf[o->len++] = (uint8_t)vlen;
    memcpy(o->buf + o->len, v, vlen);
    o->len += vlen;
}

static void
derInt(DerOut *o, const char *name, int64_t v)
{
    uint8_t b[8];
    int n = 8;

    for (int i = 0; i < 8; i++)
        b[i] = (uint8_t)(v >> (56 - 8 * i));
    // minimal two's complement
    while (n > 1 && ((b[8 - n] == 0x00 && !(b[9 - n] & 0x80)) || (b[8 - n] == 0xff && (b[9 - n] & 0x80))))
        n--;
    derPair(o, name, 0x02, b + 8 - n, n);
}

static void
derSet(DerOut *o, const DerOut *body)
{
    o->len = 0;
    o->buf[o->len++] = 0x31;
    if (body->len > 0x7f)
        o->buf[o->len++] = 0x81;
    o->buf[o->len++] = (uint8_t)body->len;
    memcpy(o->buf + o->len, body->buf, body->len);
    o->len += body->len;
}

// session bags report the user's uid in the uuid field, the device bag reports zeros
static void
sessionUuid(PDKeyBag *b, uint8_t out[16])
{
    static const uint8_t prefix[12] = { 0xff, 0xff, 0xee, 0xee, 0xdd, 0xdd, 0xcc, 0xcc, 0xbb, 0xbb, 0xaa, 0xaa };

    memset(out, 0, 16);
    if (b == &sDeviceBag || b->uid < 0)
        return;
    memcpy(out, prefix, sizeof(prefix));
    out[12] = (uint8_t)(b->uid >> 24);
    out[13] = (uint8_t)(b->uid >> 16);
    out[14] = (uint8_t)(b->uid >> 8);
    out[15] = (uint8_t)b->uid;
}

static void
bagState(PDKeyBag *b, DerOut *o)
{
    DerOut body = { { 0 }, 0 };
    uint8_t uuid[16];
    bool device = b == &sDeviceBag;

    sessionUuid(b, uuid);
    derInt(&body, "bh", publicHandle(b));
    derInt(&body, "mua", 11);
    derInt(&body, "sb", 0);
    derInt(&body, "sfa", 0);
    derInt(&body, "sgs", 0);
    derInt(&body, "sls", 0);
    derInt(&body, "sms", 0);
    derInt(&body, "srcd", 0);
    derInt(&body, "ss", device ? AKS_STATE_DEVICE : AKS_STATE_SESSION);
    derPair(&body, "uuuid", 0x04, uuid, 16);
    derSet(o, &body);
}

static IOReturn
output(IOExternalMethodArguments *args, const void *data, uint32_t len)
{
    if (args->structureOutput != NULL && args->structureOutputSize >= len) {
        memcpy(args->structureOutput, data, len);
        args->structureOutputSize = len;
        return kIOReturnSuccess;
    }
    if (args->structureVariableOutputData != NULL) {
        *args->structureVariableOutputData = OSData::withBytes(data, len);
        return kIOReturnSuccess;
    }
    return kIOReturnNoSpace;
}

// keyed by uid, not the bag uuid: bags are rebuilt every boot but keychains persist
static void
classKey(PDKeyBag *b, uint64_t keyclass, uint8_t key[32])
{
    SHA256_CTX c;
    uint32_t k = (uint32_t)keyclass;
    int64_t owner = b == &sDeviceBag ? -1 : b->uid;

    loadSecret();
    SHA256_Init(&c);
    SHA256_Update(&c, sSecret, sizeof(sSecret));
    SHA256_Update(&c, &owner, sizeof(owner));
    SHA256_Update(&c, &k, sizeof(k));
    SHA256_Final(key, &c);
}

// RFC 3394 key wrap with the first half of the class key: PD's corecrypto only does AES-128
static void
wrapKey(const uint8_t key[32], const uint8_t *in, uint8_t *out)
{
    static const uint8_t zero[16] = { 0 };
    aes_encrypt_ctx ctx;
    uint8_t a[8], r[4][8], blk[16];

    aes_encrypt_key128(key, &ctx);
    memset(a, 0xa6, 8);
    memcpy(r, in, 32);
    for (int j = 0; j < 6; j++) {
        for (int i = 0; i < 4; i++) {
            uint64_t t = (uint64_t)(4 * j + i + 1);

            memcpy(blk, a, 8);
            memcpy(blk + 8, r[i], 8);
            aes_encrypt_cbc(blk, zero, 1, blk, &ctx);
            memcpy(a, blk, 8);
            a[7] ^= (uint8_t)t;
            memcpy(r[i], blk + 8, 8);
        }
    }
    memcpy(out, a, 8);
    memcpy(out + 8, r, 32);
}

static bool
unwrapKey(const uint8_t key[32], const uint8_t *in, uint8_t *out)
{
    static const uint8_t zero[16] = { 0 };
    aes_decrypt_ctx ctx;
    uint8_t a[8], r[4][8], blk[16];

    aes_decrypt_key128(key, &ctx);
    memcpy(a, in, 8);
    memcpy(r, in + 8, 32);
    for (int j = 5; j >= 0; j--) {
        for (int i = 3; i >= 0; i--) {
            uint64_t t = (uint64_t)(4 * j + i + 1);

            a[7] ^= (uint8_t)t;
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

static IOReturn
createBag(int64_t *handle)
{
    int64_t h = 1;

    // creation handles are small and reused once released, as on macOS
    for (bool taken = true; taken; ) {
        taken = false;
        for (int i = 0; i < AKS_MAX_BAGS; i++) {
            if (sBags[i].used && sBags[i].handle == h) {
                taken = true;
                h++;
            }
        }
    }
    for (int i = 0; i < AKS_MAX_BAGS; i++) {
        if (sBags[i].used)
            continue;
        sBags[i].used = true;
        sBags[i].handle = h;
        sBags[i].uid = -1;
        random_buf(sBags[i].uuid, sizeof(sBags[i].uuid));
        *handle = h;
        return kIOReturnSuccess;
    }
    return kIOReturnNoResources;
}

// SIDP is not supported (kIOReturnUnsupportedMode, as in a macOS 26 VM); owners per sOwnersReply
static IOReturn
fvCommand(IOExternalMethodArguments *args)
{
    static const uint8_t noOwners[] = { 0x30, 0x03, 0x04, 0x01, 0x00 };

    if (args->scalarInputCount < 1)
        return kIOReturnBadArgument;
    switch (args->scalarInput[0]) {
    case AKS_FV_OWNERS_EXIST:
        if (sOwnersReply != kIOReturnSuccess)
            return sOwnersReply;
        return output(args, noOwners, sizeof(noOwners));
    case AKS_FV_SIDP_STATUS:
        return kIOReturnUnsupportedMode;
    }
    return kIOReturnUnsupported;
}

// FileVault verifier requests pass their DER by address; one buffer, used under sLock
static uint8_t sFVIn[1024];

// item lengths only, never contents: the requests carry passwords
static void
fvLogShape(uint32_t selector, const FVBytes *it, uint32_t n)
{
    IOLog("PDKeyStore: fv sel %u request %u items: %u %u %u %u %u %u\n", selector, n, n > 0 ? it[0].len : 0,
        n > 1 ? it[1].len : 0, n > 2 ? it[2].len : 0, n > 3 ? it[3].len : 0, n > 4 ? it[4].len : 0, n > 5 ? it[5].len : 0);
}

static bool
fvRequest(uint32_t selector, uint64_t addr, uint64_t len, FVBytes *items, uint32_t want)
{
    FVBytes all[8];
    uint32_t n;

    if (len == 0 || len > sizeof(sFVIn) || copyin((user_addr_t)addr, sFVIn, (size_t)len) != 0)
        return false;
    n = fvSequence(sFVIn, (uint32_t)len, all, 8);
    fvLogShape(selector, all, n);
    if (n < want)
        return false;
    memcpy(items, all, want * sizeof(*items));
    return true;
}

static IOReturn
fvVerifierOut(IOExternalMethodArguments *args, const uint8_t kid[16], const FVBytes *pw)
{
    uint8_t salt[16], blob[FV_BLOB_MAX], out[FV_BLOB_MAX + 8];
    FVBytes v;
    uint32_t n;

    if (pw->len == 0)
        return kIOReturnBadArgument;
    random_buf(salt, sizeof(salt));
    v.p = blob;
    v.len = fvBuild(FV_KIND_USER, kid, pw->p, pw->len, salt, blob);
    n = fvWrapSequence(&v, 1, out, sizeof(out));
    return n == 0 ? kIOReturnNoSpace : output(args, out, n);
}

// sel 76, first verifier: scalars { flags, address, length } of { domain, password, uuid }
static IOReturn
fvNewVerifier(IOExternalMethodArguments *args)
{
    FVBytes it[3];

    if (args->scalarInputCount < 3 || !fvRequest(76, args->scalarInput[1], args->scalarInput[2], it, 3) || it[2].len != 16)
        return kIOReturnBadArgument;
    return fvVerifierOut(args, it[2].p, &it[1]);
}

// sel 77, verifier for another user: { domain, password, verifier, new uuid, new password }
static IOReturn
fvRewrap(IOExternalMethodArguments *args)
{
    FVBytes it[5];
    FVBlob b;
    uint8_t key[32];

    if (args->scalarInputCount < 2 || !fvRequest(77, args->scalarInput[0], args->scalarInput[1], it, 5) || it[3].len != 16)
        return kIOReturnBadArgument;
    if (!fvParse(it[2].p, it[2].len, &b) || b.kind != FV_KIND_USER)
        return kIOReturnBadArgument;
    if (!fvOpen(&b, it[1].p, it[1].len, key))
        return AKS_ERR_BAD_PASSWORD;
    return fvVerifierOut(args, it[3].p, &it[4]);
}

// sel 75, unwrap with a password: { domain, password, verifier, access token, empty } -> { key, status }
static IOReturn
fvUnwrapVolumeKey(IOExternalMethodArguments *args)
{
    static const uint8_t status[4] = { 0 };
    FVBytes it[5], o[2];
    FVBlob b;
    uint8_t key[32], out[64];
    uint32_t n;

    if (args->scalarInputCount < 2 || !fvRequest(75, args->scalarInput[0], args->scalarInput[1], it, 3))
        return kIOReturnBadArgument;
    if (!fvParse(it[2].p, it[2].len, &b) || b.kind != FV_KIND_USER)
        return kIOReturnBadArgument;
    if (!fvOpen(&b, it[1].p, it[1].len, key))
        return AKS_ERR_BAD_PASSWORD;
    o[0].p = key;
    o[0].len = sizeof(key);
    o[1].p = status;
    o[1].len = sizeof(status);
    n = fvWrapSequence(o, 2, out, sizeof(out));
    return output(args, out, n);
}

// sel 74, new access token: scalars { address, length } of { 24 zero bytes, 7 bytes, verifier, uuid };
// the reply has the sel 76 shape around a 139-byte token
static IOReturn
fvNewAccessToken(IOExternalMethodArguments *args)
{
    FVBytes it[8], v;
    uint8_t kid[16], salt[16], blob[FV_BLOB_MAX], out[FV_BLOB_MAX + 8];
    uint32_t n;

    if (args->scalarInputCount < 2 || args->scalarInput[1] == 0 || args->scalarInput[1] > sizeof(sFVIn) ||
        copyin((user_addr_t)args->scalarInput[0], sFVIn, (size_t)args->scalarInput[1]) != 0)
        return kIOReturnBadArgument;
    n = fvSequence(sFVIn, (uint32_t)args->scalarInput[1], it, 8);
    fvLogShape(74, it, n);
    // only opendirectoryd's request carries a verifier; applekeystored's boot request stays unsupported
    if (n < 3 || it[2].len != FV_VERIFIER_LEN)
        return kIOReturnUnsupported;
    random_buf(kid, sizeof(kid));
    random_buf(salt, sizeof(salt));
    v.p = blob;
    v.len = fvBuild(FV_KIND_TOKEN, kid, NULL, 0, salt, blob);
    n = fvWrapSequence(&v, 1, out, sizeof(out));
    return n == 0 ? kIOReturnNoSpace : output(args, out, n);
}

// sel 85, describe a blob: struct { 24 zero bytes, blob }
static IOReturn
fvDescribe(IOExternalMethodArguments *args)
{
    FVBytes it[2];
    FVBlob b;
    uint8_t out[FV_INFO_LEN];

    if (args->structureInput == NULL ||
        fvSequence((const uint8_t *)args->structureInput, args->structureInputSize, it, 2) != 2)
        return kIOReturnBadArgument;
    if (!fvParse(it[1].p, it[1].len, &b))
        return kIOReturnBadArgument;
    return output(args, out, fvInfo(&b, out));
}

static IOReturn
dispatch(uint32_t selector, IOExternalMethodArguments *args)
{
    const uint64_t *in = args->scalarInput;
    uint32_t n = args->scalarInputCount;
    PDKeyBag *b = n > 0 ? resolve((int64_t)in[0]) : NULL;
    DerOut o;
    uint8_t key[32], buf[40];
    int64_t h;
    IOReturn ret;

    switch (selector) {
    case 0:
        return kIOReturnSuccess;
    case 2:
        ret = createBag(&h);
        if (ret == kIOReturnSuccess && args->scalarOutputCount > 0)
            args->scalarOutput[0] = (uint64_t)h;
        return ret;
    case 3: {
        // the blob applekeystored saves: magic, uuid, uid
        uint8_t blob[32] = { 'P', 'D', 'K', 'B', 1 };

        if (b == NULL)
            return AKS_ERR_NOT_FOUND;
        memcpy(blob + 8, b->uuid, 16);
        memcpy(blob + 24, &b->uid, 8);
        // right after kb_create applekeystored asks with no room for the blob, which succeeds on a mac
        if ((args->structureOutput == NULL || args->structureOutputSize < sizeof(blob)) &&
            args->structureVariableOutputData == NULL) {
            args->structureOutputSize = 0;
            return kIOReturnSuccess;
        }
        return output(args, blob, sizeof(blob));
    }
    case 4:
        if (b == NULL)
            return AKS_ERR_NOT_FOUND;
        b->handle = 0;
        return kIOReturnSuccess;
    case 5:
        if (b == NULL || n < 2)
            return AKS_ERR_NOT_FOUND;
        b->uid = (int64_t)in[1];
        sLastSessionUid = b->uid;
        return kIOReturnSuccess;
    case 7:
        if (n > 0 && in[0] == 0)
            return AKS_ERR_NOT_FOUND;
        if (b == NULL)
            return AKS_ERR_NO_SESSION;
        if (args->scalarOutputCount > 0)
            args->scalarOutput[0] = b == &sDeviceBag ? AKS_STATE_DEVICE : AKS_STATE_SESSION;
        return kIOReturnSuccess;
    case 9:
        if (b == NULL)
            return AKS_ERR_NO_SESSION;
        if (b != &sDeviceBag)
            sLastSessionUid = b->uid;
        return kIOReturnSuccess;
    case 10:
        if (b == NULL)
            return AKS_ERR_NO_SESSION;
        if (n < 2 || args->structureInputSize != 32)
            return kIOReturnBadArgument;
        classKey(b, in[1], key);
        wrapKey(key, (const uint8_t *)args->structureInput, buf);
        if (args->scalarOutputCount > 0)
            args->scalarOutput[0] = in[1];
        return output(args, buf, 40);
    case 11:
        if (b == NULL)
            return AKS_ERR_NO_SESSION;
        if (n < 2 || args->structureInputSize != 40)
            return kIOReturnBadArgument;
        classKey(b, in[1], key);
        if (!unwrapKey(key, (const uint8_t *)args->structureInput, buf))
            return kIOReturnNotPermitted;
        return output(args, buf, 32);
    case 12:
        return AKS_ERR_SEP_POLICY;
    case 14:
        // by uid, or -3 for the caller's own session
        if (n > 0 && (int64_t)in[0] != AKS_HANDLE_SESSION)
            b = (int64_t)in[0] >= 0 ? bagForUid((int64_t)in[0]) : NULL;
        if (b == NULL)
            return AKS_ERR_NOT_FOUND;
        if (args->scalarOutputCount > 0)
            args->scalarOutput[0] = (uint64_t)publicHandle(b);
        return kIOReturnSuccess;
    case 17:
    case 35:
        if (b == NULL)
            return AKS_ERR_NO_SESSION;
        bagState(b, &o);
        return output(args, o.buf, o.len);
    case 23:
        if (b == NULL)
            return AKS_ERR_NO_SESSION;
        return output(args, b->uuid, 16);
    case 30:
    case 31:
        return b == NULL ? AKS_ERR_NO_SESSION : kIOReturnSuccess;
    case 34:
        return AKS_ERR_NOT_FOUND;
    case 42:
        return AKS_ERR_NOT_PERMITTED;
    case 78:
        // Size queries observed from the entitled opendirectoryd client in
        // the macOS 26 reference VM. These are scalar replies, not keybags.
        if (n != 1 || args->scalarOutputCount != 1)
            return kIOReturnBadArgument;
        if (in[0] == 1)
            args->scalarOutput[0] = 438; // access token
        else if (in[0] == 2)
            args->scalarOutput[0] = 179; // verifier
        else
            return kIOReturnUnsupported;
        return kIOReturnSuccess;
    case 84:
        return fvCommand(args);
    case 74:
        loadSecret();
        return fvNewAccessToken(args);
    case 75:
        loadSecret();
        return fvUnwrapVolumeKey(args);
    case 76:
        loadSecret();
        return fvNewVerifier(args);
    case 77:
        loadSecret();
        return fvRewrap(args);
    case 85:
        loadSecret();
        return fvDescribe(args);
    }
    return kIOReturnUnsupported;
}

#undef super
#define super IOService
OSDefineMetaClassAndStructors(AppleKeyStore, IOService)

bool
AppleKeyStore::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    sLock = IOLockAlloc();
    PE_parse_boot_argn("pdaksowners", &sOwnersReply, sizeof(sOwnersReply));
    registerService();
    IOLog("PDKeyStore: AppleKeyStore published\n");
    return true;
}

#undef super
#define super IOUserClient
OSDefineMetaClassAndStructors(AppleKeyStoreUserClient, IOUserClient)

IOReturn
AppleKeyStoreUserClient::clientClose()
{
    if (!isInactive())
        terminate();
    return kIOReturnSuccess;
}

IOReturn
AppleKeyStoreUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                        IOExternalMethodDispatch *dispatch_, OSObject *target,
                                        void *reference)
{
    char name[32] = "?";
    IOReturn ret;

    IOLockLock(sLock);
    ret = dispatch(selector, args);
    IOLockUnlock(sLock);

    // every FileVault command with its reply, it decides Setup Assistant's security check
    if (selector == 84) {
        const uint8_t *o = (const uint8_t *)args->structureOutput;
        uint32_t olen = ret == kIOReturnSuccess && o != NULL ? args->structureOutputSize : 0;

        proc_name(proc_selfpid(), name, sizeof(name));
        IOLog("PDKeyStore: %s fv op 0x%llx der %llu -> 0x%x out %u [%02x %02x %02x %02x %02x]\n", name,
            args->scalarInputCount > 0 ? args->scalarInput[0] : 0,
            args->scalarInputCount > 3 ? args->scalarInput[3] : 0, ret, olen,
            olen > 0 ? o[0] : 0, olen > 1 ? o[1] : 0, olen > 2 ? o[2] : 0, olen > 3 ? o[3] : 0, olen > 4 ? o[4] : 0);
        return ret;
    }
    // FileVault verifier calls with their result, never their contents
    if (selector >= 74 && selector <= 77) {
        proc_name(proc_selfpid(), name, sizeof(name));
        IOLog("PDKeyStore: %s fv sel %u -> 0x%x out %u\n", name, selector, ret,
            ret == kIOReturnSuccess ? args->structureOutputSize : 0);
        return ret;
    }
    // failures only, and the frequent state queries stay quiet even then
    if (ret != kIOReturnSuccess && selector != 17 && selector != 35) {
        proc_name(proc_selfpid(), name, sizeof(name));
        IOLog("PDKeyStore: %s sel %u [%llx %llx] -> 0x%x\n", name, selector,
            args->scalarInputCount > 0 ? args->scalarInput[0] : 0,
            args->scalarInputCount > 1 ? args->scalarInput[1] : 0, ret);
    }
    return ret;
}
