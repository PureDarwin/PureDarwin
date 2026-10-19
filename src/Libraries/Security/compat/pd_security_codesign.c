/*
 * pd_security_codesign.c - static code signing checks over the Mach-O signature blob
 *
 * libsecurity_codesigning's C++ engine is not vendored. This reads the embedded
 * signature directly: the CodeDirectory's page hashes are checked against the file,
 * the Info.plist and resource seal hashes against their files, and the signing
 * certificates come out of the CMS blob. Requirements are not evaluated.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include "../apple/OSX/libsecurity_codesigning/lib/CSCommon.h"
#include <CommonCrypto/CommonDigest.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach/machine.h>
#include <libkern/OSByteOrder.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const CFStringRef kSecCodeInfoCertificates = CFSTR("certificates");
const CFStringRef kSecCodeInfoFlags = CFSTR("flags");
const CFStringRef kSecCodeInfoIdentifier = CFSTR("identifier");
const CFStringRef kSecCodeInfoMainExecutable = CFSTR("main-executable");
const CFStringRef kSecCodeInfoTeamIdentifier = CFSTR("teamid");
const CFStringRef kSecCodeInfoUnique = CFSTR("unique");
const CFStringRef kSecCodeInfoFormat = CFSTR("format");
const CFStringRef kSecCodeInfoSource = CFSTR("source");
const CFStringRef kSecCodeInfoDigestAlgorithm = CFSTR("digest-algorithm");
const CFStringRef kSecCodeInfoCMS = CFSTR("cms");
const CFStringRef kSecCodeInfoPList = CFSTR("info-plist");

#define CSMAGIC_EMBEDDED_SIGNATURE 0xfade0cc0
#define CSMAGIC_CODEDIRECTORY 0xfade0c02
#define CSMAGIC_BLOBWRAPPER 0xfade0b01
#define CSSLOT_CODEDIRECTORY 0
#define CSSLOT_ALTERNATE_CODEDIRECTORIES 0x1000
#define CSSLOT_SIGNATURESLOT 0x10000
#define CS_HASHTYPE_SHA1 1
#define CS_HASHTYPE_SHA256 2
#define CS_HASHTYPE_SHA256_TRUNCATED 3
#define CS_ADHOC 0x2

#define kSecCSSigningInformationFlag (1 << 1)

typedef struct {
    CFURLRef bundle;
    CFURLRef executable;
} PDStaticCode;

// the static code object is a CFData holding the two URLs, so CFRelease just works
static CFDataRef createCode(CFURLRef bundle, CFURLRef executable)
{
    PDStaticCode code = { (CFURLRef)CFRetain(bundle), (CFURLRef)CFRetain(executable) };

    return CFDataCreate(NULL, (const UInt8 *)&code, sizeof(code));
}

static const PDStaticCode *codeOf(SecStaticCodeRef ref)
{
    if (ref == NULL || CFGetTypeID(ref) != CFDataGetTypeID() || CFDataGetLength((CFDataRef)ref) != sizeof(PDStaticCode))
        return NULL;
    return (const PDStaticCode *)CFDataGetBytePtr((CFDataRef)ref);
}

OSStatus SecStaticCodeCreateWithPath(CFURLRef path, SecCSFlags flags, SecStaticCodeRef *staticCode)
{
    char p[PATH_MAX];
    struct stat st;
    CFURLRef executable = NULL;

    if (staticCode == NULL || path == NULL) return errSecParam;
    *staticCode = NULL;
    if (!CFURLGetFileSystemRepresentation(path, true, (UInt8 *)p, sizeof(p)) || stat(p, &st) != 0)
        return errSecCSStaticCodeNotFound;
    if (S_ISDIR(st.st_mode)) {
        CFBundleRef bundle = CFBundleCreate(NULL, path);

        if (bundle) {
            executable = CFBundleCopyExecutableURL(bundle);
            CFRelease(bundle);
        }
        if (executable == NULL) return errSecCSStaticCodeNotFound;
    } else
        executable = (CFURLRef)CFRetain(path);
    *staticCode = (SecStaticCodeRef)createCode(path, executable);
    CFRelease(executable);
    return errSecSuccess;
}

// the signature blob of the slice for this machine
static UInt8 *copySignature(const PDStaticCode *code, size_t *length, UInt8 **slice, size_t *sliceLength)
{
    char p[PATH_MAX];
    int fd;
    struct stat st;
    UInt8 *file, *base, *result = NULL;
    size_t baseLength;
    uint32_t magic;

    if (!CFURLGetFileSystemRepresentation(code->executable, true, (UInt8 *)p, sizeof(p))) return NULL;
    if ((fd = open(p, O_RDONLY)) < 0) return NULL;
    if (fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(struct mach_header_64)) {
        close(fd);
        return NULL;
    }
    file = malloc(st.st_size);
    if (pread(fd, file, st.st_size, 0) != st.st_size) {
        close(fd);
        free(file);
        return NULL;
    }
    close(fd);
    base = file;
    baseLength = st.st_size;
    magic = OSReadBigInt32(file, 0);
    if (magic == FAT_MAGIC) {
        uint32_t n = OSReadBigInt32(file, 4);
#if defined(__arm64__)
        cpu_type_t want = CPU_TYPE_ARM64;
#else
        cpu_type_t want = CPU_TYPE_X86_64;
#endif

        base = NULL;
        for (uint32_t i = 0; i < n && 8 + (i + 1) * sizeof(struct fat_arch) <= (size_t)st.st_size; i++) {
            const UInt8 *a = file + 8 + i * sizeof(struct fat_arch);

            if ((cpu_type_t)OSReadBigInt32(a, 0) == want) {
                uint32_t off = OSReadBigInt32(a, 8), size = OSReadBigInt32(a, 12);

                if ((off_t)off + size <= st.st_size) {
                    base = file + off;
                    baseLength = size;
                }
            }
        }
        if (base == NULL) {
            free(file);
            return NULL;
        }
    }
    if (*(uint32_t *)base == MH_MAGIC_64) {
        const struct mach_header_64 *mh = (const struct mach_header_64 *)base;
        const UInt8 *lc = base + sizeof(*mh);

        for (uint32_t i = 0; i < mh->ncmds && lc + sizeof(struct load_command) <= base + baseLength; i++) {
            const struct load_command *cmd = (const struct load_command *)lc;

            if (cmd->cmd == LC_CODE_SIGNATURE) {
                const struct linkedit_data_command *sig = (const struct linkedit_data_command *)lc;

                if ((size_t)sig->dataoff + sig->datasize <= baseLength) {
                    result = malloc(sig->datasize);
                    memcpy(result, base + sig->dataoff, sig->datasize);
                    *length = sig->datasize;
                }
                break;
            }
            lc += cmd->cmdsize;
        }
    }
    if (result && slice) {
        *slice = malloc(baseLength);
        memcpy(*slice, base, baseLength);
        *sliceLength = baseLength;
    }
    free(file);
    return result;
}

// a blob of the super blob by slot, bounds checked
static const UInt8 *findBlob(const UInt8 *sig, size_t length, uint32_t slot, size_t *blobLength)
{
    uint32_t count;

    if (length < 12 || OSReadBigInt32(sig, 0) != CSMAGIC_EMBEDDED_SIGNATURE) return NULL;
    count = OSReadBigInt32(sig, 8);
    for (uint32_t i = 0; i < count && 12 + (i + 1) * 8 <= length; i++) {
        uint32_t type = OSReadBigInt32(sig, 12 + i * 8), offset = OSReadBigInt32(sig, 16 + i * 8);

        if (type == slot && offset + 8 <= length) {
            uint32_t l = OSReadBigInt32(sig, offset + 4);

            if (offset + l > length) return NULL;
            *blobLength = l;
            return sig + offset;
        }
    }
    return NULL;
}

// the strongest code directory, SHA-256 when the signature has one
static const UInt8 *bestCodeDirectory(const UInt8 *sig, size_t length, size_t *cdLength)
{
    const UInt8 *best = findBlob(sig, length, CSSLOT_CODEDIRECTORY, cdLength);

    if (best && best[37] == CS_HASHTYPE_SHA256) return best;
    for (uint32_t s = CSSLOT_ALTERNATE_CODEDIRECTORIES; s < CSSLOT_ALTERNATE_CODEDIRECTORIES + 5; s++) {
        size_t l;
        const UInt8 *cd = findBlob(sig, length, s, &l);

        if (cd && OSReadBigInt32(cd, 0) == CSMAGIC_CODEDIRECTORY && cd[37] == CS_HASHTYPE_SHA256) {
            *cdLength = l;
            return cd;
        }
    }
    return best;
}

static void hashBytes(int type, const void *bytes, size_t length, UInt8 *out)
{
    if (type == CS_HASHTYPE_SHA1)
        CC_SHA1(bytes, (CC_LONG)length, out);
    else
        CC_SHA256(bytes, (CC_LONG)length, out);
}

static bool hashFile(int type, CFURLRef url, UInt8 *out)
{
    char p[PATH_MAX];
    struct stat st;
    int fd;
    bool ok = false;

    if (!CFURLGetFileSystemRepresentation(url, true, (UInt8 *)p, sizeof(p)) || (fd = open(p, O_RDONLY)) < 0) return false;
    if (fstat(fd, &st) == 0) {
        void *bytes = malloc(st.st_size ? st.st_size : 1);

        if (pread(fd, bytes, st.st_size, 0) == st.st_size) {
            hashBytes(type, bytes, st.st_size, out);
            ok = true;
        }
        free(bytes);
    }
    close(fd);
    return ok;
}

static CFURLRef copyBundleFile(const PDStaticCode *code, CFStringRef relative)
{
    return CFURLCreateCopyAppendingPathComponent(NULL, code->bundle, relative, false);
}

static bool checkSpecialSlot(const PDStaticCode *code, const UInt8 *cd, uint32_t hashOffset, uint8_t hashSize, int type, uint32_t nSpecial, int slot, CFStringRef relative)
{
    UInt8 expected[CC_SHA256_DIGEST_LENGTH], actual[CC_SHA256_DIGEST_LENGTH];
    static const UInt8 zero[CC_SHA256_DIGEST_LENGTH];
    CFURLRef url;
    bool ok;

    if ((uint32_t)slot > nSpecial) return true;
    memcpy(expected, cd + hashOffset - slot * hashSize, hashSize);
    // an all-zero slot means nothing was sealed there
    if (memcmp(expected, zero, hashSize) == 0) return true;
    url = copyBundleFile(code, relative);
    ok = hashFile(type, url, actual) && memcmp(actual, expected, hashSize) == 0;
    CFRelease(url);
    return ok;
}

OSStatus SecStaticCodeCheckValidity(SecStaticCodeRef staticCode, SecCSFlags flags, SecRequirementRef requirement)
{
    const PDStaticCode *code = codeOf(staticCode);
    size_t length = 0, sliceLength = 0, cdLength = 0;
    UInt8 *slice = NULL;
    UInt8 *sig;
    const UInt8 *cd;
    OSStatus status = errSecSuccess;

    if (code == NULL) return errSecParam;
    sig = copySignature(code, &length, &slice, &sliceLength);
    if (sig == NULL) return errSecCSUnsigned;
    cd = bestCodeDirectory(sig, length, &cdLength);
    if (cd == NULL || cdLength < 44 || OSReadBigInt32(cd, 0) != CSMAGIC_CODEDIRECTORY) {
        status = errSecCSSignatureFailed;
        goto out;
    }

    uint32_t hashOffset = OSReadBigInt32(cd, 16);
    uint32_t nSpecial = OSReadBigInt32(cd, 24);
    uint32_t nCode = OSReadBigInt32(cd, 28);
    uint32_t codeLimit = OSReadBigInt32(cd, 32);
    uint8_t hashSize = cd[36], hashType = cd[37], pageShift = cd[39];
    size_t page = pageShift ? (size_t)1 << pageShift : codeLimit;
    UInt8 digest[CC_SHA256_DIGEST_LENGTH];

    if (hashSize == 0 || hashSize > CC_SHA256_DIGEST_LENGTH || hashOffset + (size_t)nCode * hashSize > cdLength ||
        (size_t)nSpecial * hashSize > hashOffset || codeLimit > sliceLength) {
        status = errSecCSSignatureFailed;
        goto out;
    }
    for (uint32_t i = 0; i < nCode; i++) {
        size_t start = (size_t)i * page, end = start + page;

        if (end > codeLimit) end = codeLimit;
        hashBytes(hashType == CS_HASHTYPE_SHA1 ? CS_HASHTYPE_SHA1 : CS_HASHTYPE_SHA256, slice + start, end - start, digest);
        if (memcmp(digest, cd + hashOffset + (size_t)i * hashSize, hashSize) != 0) {
            status = errSecCSSignatureFailed;
            goto out;
        }
    }
    // slot 1 seals Info.plist, slot 3 the resource directory
    if (!CFEqual(code->bundle, code->executable)) {
        int type = hashType == CS_HASHTYPE_SHA1 ? CS_HASHTYPE_SHA1 : CS_HASHTYPE_SHA256;

        if (!checkSpecialSlot(code, cd, hashOffset, hashSize, type, nSpecial, 1, CFSTR("Contents/Info.plist")) ||
            !checkSpecialSlot(code, cd, hashOffset, hashSize, type, nSpecial, 3, CFSTR("Contents/_CodeSignature/CodeResources")))
            status = errSecCSBadResource;
    }
out:
    free(slice);
    free(sig);
    return status;
}

// DER length at p, advancing past the tag and length bytes
static bool derHeader(const UInt8 **p, const UInt8 *end, UInt8 *tag, size_t *length)
{
    const UInt8 *q = *p;
    size_t l;

    if (q + 2 > end) return false;
    *tag = *q++;
    l = *q++;
    if (l & 0x80) {
        int n = l & 0x7f;

        if (n == 0 || n > 4 || q + n > end) return false;
        l = 0;
        while (n--) l = (l << 8) | *q++;
    }
    if (q + l > end) return false;
    *p = q;
    *length = l;
    return true;
}

// ContentInfo { oid, [0] SignedData { version, digests, content, [0] certificates ... } }
static CFArrayRef copyCertificatesFromCMS(const UInt8 *cms, size_t length)
{
    const UInt8 *p = cms, *end = cms + length;
    UInt8 tag;
    size_t l;
    CFMutableArrayRef certs;

    if (!derHeader(&p, end, &tag, &l) || tag != 0x30) return NULL;
    end = p + l;
    if (!derHeader(&p, end, &tag, &l) || tag != 0x06) return NULL;
    p += l;
    if (!derHeader(&p, end, &tag, &l) || tag != 0xA0) return NULL;
    if (!derHeader(&p, end, &tag, &l) || tag != 0x30) return NULL;
    end = p + l;
    for (int skip = 0; skip < 3; skip++) {
        if (!derHeader(&p, end, &tag, &l)) return NULL;
        p += l;
    }
    if (!derHeader(&p, end, &tag, &l) || tag != 0xA0) return NULL;
    certs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    end = p + l;
    while (p < end) {
        const UInt8 *start = p;
        CFDataRef der;
        SecCertificateRef cert;

        if (!derHeader(&p, end, &tag, &l)) break;
        p += l;
        der = CFDataCreate(NULL, start, p - start);
        cert = SecCertificateCreateWithData(NULL, der);
        if (cert) {
            CFArrayAppendValue(certs, cert);
            CFRelease(cert);
        }
        CFRelease(der);
    }
    return certs;
}

OSStatus SecCodeCopySigningInformation(SecStaticCodeRef code, SecCSFlags flags, CFDictionaryRef *information)
{
    const PDStaticCode *c = codeOf(code);
    CFMutableDictionaryRef info;
    size_t length = 0, cdLength = 0;
    UInt8 *sig;
    const UInt8 *cd;

    if (c == NULL || information == NULL) return errSecParam;
    info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(info, kSecCodeInfoMainExecutable, c->executable);
    CFDictionarySetValue(info, kSecCodeInfoSource, CFSTR("embedded"));
    CFDictionarySetValue(info, kSecCodeInfoFormat, CFSTR("Mach-O thin"));
    sig = copySignature(c, &length, NULL, NULL);
    cd = sig ? bestCodeDirectory(sig, length, &cdLength) : NULL;
    if (cd && cdLength >= 44 && OSReadBigInt32(cd, 0) == CSMAGIC_CODEDIRECTORY) {
        uint32_t version = OSReadBigInt32(cd, 8);
        uint32_t csFlags = OSReadBigInt32(cd, 12);
        uint32_t identOffset = OSReadBigInt32(cd, 20);
        CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &csFlags);
        UInt8 unique[CC_SHA256_DIGEST_LENGTH];
        CFDataRef uniqueData;

        CFDictionarySetValue(info, kSecCodeInfoFlags, n);
        CFRelease(n);
        if (identOffset < cdLength && memchr(cd + identOffset, 0, cdLength - identOffset)) {
            CFStringRef ident = CFStringCreateWithCString(NULL, (const char *)cd + identOffset, kCFStringEncodingUTF8);

            if (ident) {
                CFDictionarySetValue(info, kSecCodeInfoIdentifier, ident);
                CFRelease(ident);
            }
        }
        if (version >= 0x20200 && cdLength >= 52) {
            uint32_t teamOffset = OSReadBigInt32(cd, 48);

            if (teamOffset && teamOffset < cdLength && memchr(cd + teamOffset, 0, cdLength - teamOffset)) {
                CFStringRef team = CFStringCreateWithCString(NULL, (const char *)cd + teamOffset, kCFStringEncodingUTF8);

                if (team) {
                    CFDictionarySetValue(info, kSecCodeInfoTeamIdentifier, team);
                    CFRelease(team);
                }
            }
        }
        // the cdhash, a hash of the code directory itself
        hashBytes(cd[37] == CS_HASHTYPE_SHA1 ? CS_HASHTYPE_SHA1 : CS_HASHTYPE_SHA256, cd, cdLength, unique);
        uniqueData = CFDataCreate(NULL, unique, 20);
        CFDictionarySetValue(info, kSecCodeInfoUnique, uniqueData);
        CFRelease(uniqueData);

        if ((flags & kSecCSSigningInformationFlag) && !(csFlags & CS_ADHOC)) {
            size_t cmsLength = 0;
            const UInt8 *wrapper = findBlob(sig, length, CSSLOT_SIGNATURESLOT, &cmsLength);

            if (wrapper && cmsLength > 8 && OSReadBigInt32(wrapper, 0) == CSMAGIC_BLOBWRAPPER) {
                CFArrayRef certs = copyCertificatesFromCMS(wrapper + 8, cmsLength - 8);
                CFDataRef cms = CFDataCreate(NULL, wrapper + 8, cmsLength - 8);

                if (certs && CFArrayGetCount(certs) > 0) CFDictionarySetValue(info, kSecCodeInfoCertificates, certs);
                CFDictionarySetValue(info, kSecCodeInfoCMS, cms);
                CFRelease(cms);
                if (certs) CFRelease(certs);
            }
        }
    }
    free(sig);
    *information = info;
    return errSecSuccess;
}

// no identity preferences are kept, so there is never a preferred one
SecIdentityRef SecIdentityCopyPreferred(CFStringRef name, CFArrayRef keyUsage, CFArrayRef validIssuers)
{
    return NULL;
}

// no trust settings are stored in any domain yet
OSStatus SecTrustSettingsCopyTrustSettings(SecCertificateRef certRef, SecTrustSettingsDomain domain, CFArrayRef *trustSettings)
{
    if (trustSettings) *trustSettings = NULL;
    return errSecItemNotFound;
}
