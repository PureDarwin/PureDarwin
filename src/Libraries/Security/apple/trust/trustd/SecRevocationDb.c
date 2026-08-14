/*
 * Copyright (c) 2016-2020 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */

/*
 *  SecRevocationDb.c
 */

#include "trust/trustd/SecRevocationDb.h"
#include "trust/trustd/OTATrustUtilities.h"
#include "trust/trustd/SecRevocationNetworking.h"
#include "trust/trustd/SecTrustLoggingServer.h"
#include "trust/trustd/trustdFileLocations.h"
#include <Security/SecCertificateInternal.h>
#include <Security/SecCMS.h>
#include <Security/CMSDecoder.h>
#include <Security/SecFramework.h>
#include <Security/SecInternal.h>
#include <Security/SecPolicyPriv.h>
#include <AssertMacros.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dispatch/dispatch.h>
#include <notify.h>
#include <asl.h>
#include <copyfile.h>
#include "utilities/debugging.h"
#include "utilities/sec_action.h"
#include "utilities/sqlutils.h"
#include "utilities/SecAppleAnchorPriv.h"
#include "utilities/iOSforOSX.h"
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <sqlite3.h>
#include <zlib.h>
#include <malloc/malloc.h>
#include <xpc/activity.h>
#include <xpc/private.h>
#include <os/transaction_private.h>
#include <os/variant_private.h>
#include <os/lock.h>

#include <CommonCrypto/CommonDigest.h>
#include <CFNetwork/CFHTTPMessage.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFUtilities.h>

/*
 ==============================================================================
   CoreFoundation utilities
 ==============================================================================
*/

static bool hashCFThing(CFTypeRef thing, CC_SHA256_CTX* hash_ctx);

/* comparison function for sorting dictionary keys alphabetically */
static int compareCFStrings(const void *p, const void *q) {
    CFStringRef str1 = *(CFStringRef *)p;
    CFStringRef str2 = *(CFStringRef *)q;
    if (!(isString(str1) && isString(str2))) {
        return -1; /* can't compare non-string types */
    }
    CFComparisonResult result = CFStringCompare(str1, str2, 0);
    if (result == kCFCompareLessThan) {
        return -1;
    } else if (result == kCFCompareGreaterThan) {
        return 1;
    }
    return 0; /* (result == kCFCompareEqualTo) */
}

static bool hashData(CFDataRef data, CC_SHA256_CTX* hash_ctx) {
    if (!isData(data)) { return false; }
    uint32_t length = (uint32_t)(CFDataGetLength(data) & 0xFFFFFFFF);
    uint32_t n = OSSwapInt32(length);
    CC_SHA256_Update(hash_ctx, &n, sizeof(uint32_t));
    const uint8_t *p = (uint8_t*)CFDataGetBytePtr(data);
    if (p) { CC_SHA256_Update(hash_ctx, p, length); }
    return (p != NULL);
}

static bool hashString(CFStringRef str, CC_SHA256_CTX* hash_ctx) {
    if (!isString(str)) { return false; }
    __block bool ok = false;
    CFStringPerformWithCString(str, ^(const char *strbuf) {
        // hash string length (in bytes, not counting null terminator)
        uint32_t c = (uint32_t)(strlen(strbuf) & 0xFFFFFFFF);
        uint32_t n = OSSwapInt32(c);
        CC_SHA256_Update(hash_ctx, &n, sizeof(uint32_t));
        // hash string bytes
        CC_SHA256_Update(hash_ctx, strbuf, c);
        ok = true;
    });
    return ok;
}

static bool hashNumber(CFNumberRef num, CC_SHA256_CTX* hash_ctx) {
    uint32_t n = 0;
    if (!isNumber(num) || !CFNumberGetValue(num, kCFNumberSInt32Type, &n)) {
        return false;
    }
    n = OSSwapInt32(n);
    CC_SHA256_Update(hash_ctx, &n, sizeof(uint32_t));
    return true;
}

static bool hashBoolean(CFBooleanRef value, CC_SHA256_CTX* hash_ctx) {
    if (!isBoolean(value)) { return false; }
    uint8_t c = CFBooleanGetValue(value) ? 1 : 0;
    CC_SHA256_Update(hash_ctx, &c, sizeof(uint8_t));
    return true;
}

static bool hashArray(CFArrayRef array, CC_SHA256_CTX* hash_ctx) {
    if (!isArray(array)) { return false; }
    CFIndex count = CFArrayGetCount(array);
    uint32_t n = OSSwapInt32(count & 0xFFFFFFFF);
    CC_SHA256_Update(hash_ctx, &n, sizeof(uint32_t));
    __block bool ok = true;
    CFArrayForEach(array, ^(const void *thing) {
        ok &= hashCFThing(thing, hash_ctx);
    });
    return ok;
}

static bool hashDictionary(CFDictionaryRef dictionary, CC_SHA256_CTX* hash_ctx) {
    if (!isDictionary(dictionary)) { return false; }
    CFIndex count = CFDictionaryGetCount(dictionary);
    const void **keys = (const void **)malloc(sizeof(void*) * count);
    const void **vals = (const void **)malloc(sizeof(void*) * count);
    bool ok = (keys && vals);
    if (ok) {
        CFDictionaryGetKeysAndValues(dictionary, keys, vals);
        qsort(keys, count, sizeof(CFStringRef), compareCFStrings);
        uint32_t n = OSSwapInt32(count & 0xFFFFFFFF);
        CC_SHA256_Update(hash_ctx, &n, sizeof(uint32_t));
    }
    for (CFIndex idx = 0; ok && idx < count; idx++) {
        CFStringRef key = (CFStringRef)keys[idx];
        CFTypeRef value = (CFTypeRef)CFDictionaryGetValue(dictionary, key);
        ok &= hashString(key, hash_ctx);
        ok &= hashCFThing(value, hash_ctx);
    }
    free(keys);
    free(vals);
    return ok;
}

static bool hashCFThing(CFTypeRef thing, CC_SHA256_CTX* hash_ctx) {
    if (isArray(thing)) {
        return hashArray(thing, hash_ctx);
    } else if (isDictionary(thing)) {
        return hashDictionary(thing, hash_ctx);
    } else if (isData(thing)) {
        return hashData(thing, hash_ctx);
    } else if (isString(thing)) {
        return hashString(thing, hash_ctx);
    } else if (isNumber(thing)) {
        return hashNumber(thing, hash_ctx);
    } else if (isBoolean(thing)) {
        return hashBoolean(thing, hash_ctx);
    }
    return false;
}

static double htond(double h) {
    /* no-op if big endian */
    if (OSHostByteOrder() == OSBigEndian) {
        return h;
    }
    double n;
    size_t i=0;
    char *hp = (char*)&h;
    char *np = (char*)&n;
    while (i < sizeof(h)) { np[i] = hp[(sizeof(h)-1)-i]; ++i; }
    return n;
}

/* Obtain swapped value of the 32-bit integer referenced by the given pointer.
   Since a generic void or char pointer may not be aligned on a 4-byte boundary,
   the UB sanitizer does not let us directly dereference it as *(uint32_t*).
*/
static uint32_t SecSwapInt32Ptr(const void* P) {
    uint32_t _i=0; memcpy(&_i,P,sizeof(_i)); return OSSwapInt32(_i);
}


// MARK: -
// MARK: Valid definitions
/*
 ==============================================================================
   Valid definitions
 ==============================================================================
*/

const CFStringRef kValidUpdateProdServer   = CFSTR("valid.apple.com");
const CFStringRef kValidUpdateSeedServer   = CFSTR("valid.apple.com/seed");
const CFStringRef kValidUpdateCarryServer  = CFSTR("valid.apple.com/carry");

static CFStringRef kSecPrefsDomain          = CFSTR("com.apple.security");
static CFStringRef kUpdateServerKey         = CFSTR("ValidUpdateServer");
static CFStringRef kUpdateEnabledKey        = CFSTR("ValidUpdateEnabled");
static CFStringRef kVerifyEnabledKey        = CFSTR("ValidVerifyEnabled");
static CFStringRef kUpdateIntervalKey       = CFSTR("ValidUpdateInterval");
static CFStringRef kBoolTrueKey             = CFSTR("1");
static CFStringRef kBoolFalseKey            = CFSTR("0");

/* constant length of boolean string keys */
#define BOOL_STRING_KEY_LENGTH          1

typedef CF_OPTIONS(CFOptionFlags, SecValidInfoFlags) {
    kSecValidInfoComplete               = 1u << 0,
    kSecValidInfoCheckOCSP              = 1u << 1,
    kSecValidInfoKnownOnly              = 1u << 2,
    kSecValidInfoRequireCT              = 1u << 3,
    kSecValidInfoAllowlist              = 1u << 4,
    kSecValidInfoNoCACheck              = 1u << 5,
    kSecValidInfoOverridable            = 1u << 6,
    kSecValidInfoDateConstraints        = 1u << 7,
    kSecValidInfoNameConstraints        = 1u << 8,
    kSecValidInfoPolicyConstraints      = 1u << 9,
    kSecValidInfoNoCAv2Check            = 1u << 10,
};

/* minimum update interval */
#define kSecMinUpdateInterval           (60.0 * 5)

/* standard update interval */
#define kSecStdUpdateInterval           (60.0 * 60 * 3)

/* maximum allowed interval */
#define kSecMaxUpdateInterval           (60.0 * 60 * 24 * 7)

/* filenames we use, relative to revocation info directory */
#define kSecRevocationCurUpdateFile     "update-current"
#define kSecRevocationDbFileName        "valid.sqlite3"
#define kSecRevocationDbReplaceFile     ".valid_replace"

#define isDbOwner SecOTAPKIIsSystemTrustd

#define kSecRevocationDbChanged         "com.apple.trustd.valid.db-changed"

/* database schema version
   v1 = initial version
   v2 = fix for group entry transitions
   v3 = handle optional entries in update dictionaries
   v4 = add db_format and db_source entries
   v5 = add date constraints table, with updated group flags
   v6 = explicitly set autovacuum and journal modes at db creation
   v7 = add policies column to groups table (policy constraints)

   Note: kSecRevocationDbMinSchemaVersion is the lowest version whose
   results can be used. This allows revocation results to be obtained
   from an existing db before the next update interval occurs, at which
   time we'll update to the current version (kSecRevocationDbSchemaVersion).
*/
#define kSecRevocationDbSchemaVersion       7  /* current version we support */
#define kSecRevocationDbMinSchemaVersion    7  /* minimum version we can use */

/* update file format
*/
CF_ENUM(CFIndex) {
    kSecValidUpdateFormatG1               = 1, /* initial version */
    kSecValidUpdateFormatG2               = 2, /* signed content, single plist */
    kSecValidUpdateFormatG3               = 3  /* signed content, multiple plists */
};

#define kSecRevocationDbUpdateFormat        3  /* current version we support */
#define kSecRevocationDbMinUpdateFormat     2  /* minimum version we can use */

#define kSecRevocationDbCacheSize           100

typedef struct __SecRevocationDb *SecRevocationDbRef;
struct __SecRevocationDb {
    SecDbRef db;
    dispatch_queue_t update_queue;
    bool updateInProgress;
    bool unsupportedVersion;
    bool changed;
    CFMutableArrayRef info_cache_list;
    CFMutableDictionaryRef info_cache;
    os_unfair_lock info_cache_lock;
};

typedef struct __SecRevocationDbConnection *SecRevocationDbConnectionRef;
struct __SecRevocationDbConnection {
    SecRevocationDbRef db;
    SecDbConnectionRef dbconn;
    CFIndex precommitVersion;
    CFIndex precommitDbVersion;
    CFIndex precommitInterval;
    bool fullUpdate;
};

bool SecRevocationDbVerifyUpdate(void *update, CFIndex length);
bool SecRevocationDbIngestUpdate(SecRevocationDbConnectionRef dbc, CFDictionaryRef update, CFIndex chunkVersion, CFIndex *outVersion, CFErrorRef *error);
bool _SecRevocationDbApplyUpdate(SecRevocationDbConnectionRef dbc, CFDictionaryRef update, CFIndex version, CFErrorRef *error);
CFAbsoluteTime SecRevocationDbComputeNextUpdateTime(CFIndex updateInterval);
bool SecRevocationDbUpdateSchema(SecRevocationDbRef rdb);
CFIndex SecRevocationDbGetUpdateFormat(void);
bool _SecRevocationDbSetUpdateSource(SecRevocationDbConnectionRef dbc, CFStringRef source, CFErrorRef *error);
bool SecRevocationDbSetUpdateSource(SecRevocationDbRef rdb, CFStringRef source);
CF_RETURNS_RETAINED CFStringRef SecRevocationDbCopyUpdateSource(void);
bool SecRevocationDbSetNextUpdateTime(CFAbsoluteTime nextUpdate, CFErrorRef *error);
CFAbsoluteTime SecRevocationDbGetNextUpdateTime(void);
dispatch_queue_t SecRevocationDbGetUpdateQueue(void);
bool _SecRevocationDbRemoveAllEntries(SecRevocationDbConnectionRef dbc, CFErrorRef *error);
void SecRevocationDbReleaseAllConnections(void);
static bool SecValidUpdateForceReplaceDatabase(void);
static void SecRevocationDbWith(void(^dbJob)(SecRevocationDbRef db));
static bool SecRevocationDbPerformWrite(SecRevocationDbRef rdb, CFErrorRef *error, bool(^writeJob)(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError));
static bool SecRevocationDbPerformRead(SecRevocationDbRef rdb, CFErrorRef *error, bool(^readJob)(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError));
static SecValidInfoFormat _SecRevocationDbGetGroupFormat(SecRevocationDbConnectionRef dbc, int64_t groupId, SecValidInfoFlags *flags, CFDataRef *data, CFDataRef *policies, CFErrorRef *error);
static bool _SecRevocationDbSetUpdateInterval(SecRevocationDbConnectionRef dbc, int64_t interval, CFErrorRef *error);
static int64_t _SecRevocationDbGetUpdateInterval(SecRevocationDbConnectionRef dbc, CFErrorRef *error);
static bool _SecRevocationDbSetVersion(SecRevocationDbConnectionRef dbc, CFIndex version, CFErrorRef *error);
static int64_t _SecRevocationDbGetVersion(SecRevocationDbConnectionRef dbc, CFErrorRef *error);
static int64_t _SecRevocationDbGetSchemaVersion(SecRevocationDbRef rdb, SecRevocationDbConnectionRef dbc, CFErrorRef *error);
static CFArrayRef _SecRevocationDbCopyHashes(SecRevocationDbConnectionRef dbc, CFErrorRef *error);
static bool _SecRevocationDbSetHashes(SecRevocationDbConnectionRef dbc, CFArrayRef hashes, CFErrorRef *error);
static void SecRevocationDbResetCaches(void);
static SecRevocationDbConnectionRef SecRevocationDbConnectionInit(SecRevocationDbRef db, SecDbConnectionRef dbconn, CFErrorRef *error);
static bool SecRevocationDbComputeDigests(SecRevocationDbConnectionRef dbc, CFErrorRef *error);


static CFDataRef copyInflatedData(CFDataRef data) {
    if (!data) {
        return NULL;
    }
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    /* 32 is a magic value which enables automatic header detection
       of gzip or zlib compressed data. */
    if (inflateInit2(&zs, 32+MAX_WBITS) != Z_OK) {
        return NULL;
    }
    zs.next_in = (UInt8 *)(CFDataGetBytePtr(data));
    zs.avail_in = (uInt)CFDataGetLength(data);

    CFMutableDataRef outData = CFDataCreateMutable(NULL, 0);
    if (!outData) {
        return NULL;
    }
    CFIndex buf_sz = malloc_good_size(zs.avail_in ? zs.avail_in : 1024 * 4);
    unsigned char *buf = malloc(buf_sz);
    int rc;
    do {
        zs.next_out = (Bytef*)buf;
        zs.avail_out = (uInt)buf_sz;
        rc = inflate(&zs, 0);
        CFIndex outLen = CFDataGetLength(outData);
        if (outLen < (CFIndex)zs.total_out) {
            CFDataAppendBytes(outData, (const UInt8*)buf, (CFIndex)zs.total_out - outLen);
        }
    } while (rc == Z_OK);

    inflateEnd(&zs);
    free(buf);
    if (rc != Z_STREAM_END) {
        CFReleaseSafe(outData);
        return NULL;
    }
    return (CFDataRef)outData;
}

static CFDataRef copyDeflatedData(CFDataRef data) {
    if (!data) {
        return NULL;
    }
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        return NULL;
    }
    zs.next_in = (UInt8 *)(CFDataGetBytePtr(data));
    zs.avail_in = (uInt)CFDataGetLength(data);

    CFMutableDataRef outData = CFDataCreateMutable(NULL, 0);
    if (!outData) {
        return NULL;
    }
    CFIndex buf_sz = malloc_good_size(zs.avail_in ? zs.avail_in : 1024 * 4);
    unsigned char *buf = malloc(buf_sz);
    int rc = Z_BUF_ERROR;
    do {
        zs.next_out = (Bytef*)buf;
        zs.avail_out = (uInt)buf_sz;
        rc = deflate(&zs, Z_FINISH);

        if (rc == Z_OK || rc == Z_STREAM_END) {
            CFIndex buf_used = buf_sz - zs.avail_out;
            CFDataAppendBytes(outData, (const UInt8*)buf, buf_used);
        }
        else if (rc == Z_BUF_ERROR) {
            free(buf);
            buf_sz = malloc_good_size(buf_sz * 2);
            buf = malloc(buf_sz);
            if (buf) {
                rc = Z_OK; /* try again with larger buffer */
            }
        }
    } while (rc == Z_OK && zs.avail_in);

    deflateEnd(&zs);
    free(buf);
    if (rc != Z_STREAM_END) {
        CFReleaseSafe(outData);
        return NULL;
    }
    return (CFDataRef)outData;
}

/* Read file opens the file, mmaps it and then closes the file. */
int readValidFile(const char *fileName,
                    CFDataRef  *bytes) {   // mmapped and returned -- must be munmapped!
    int rtn, fd;
    const uint8_t *buf = NULL;
    struct stat	sb;
    size_t size = 0;

    *bytes = NULL;
    fd = open(fileName, O_RDONLY);
    if (fd < 0) { return errno; }
    rtn = fstat(fd, &sb);
    if (rtn) { goto errOut; }
    if (sb.st_size > (off_t) ((UINT32_MAX >> 1)-1)) {
        rtn = EFBIG;
        goto errOut;
    }
    size = (size_t)sb.st_size;

    buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!buf || buf == MAP_FAILED) {
        rtn = errno;
        secerror("unable to map %s (errno %d)", fileName, rtn);
        goto errOut;
    }

    *bytes = CFDataCreateWithBytesNoCopy(NULL, buf, size, kCFAllocatorNull);

errOut:
    close(fd);
    if(rtn) {
        CFReleaseNull(*bytes);
        if (buf) {
            int unmap_err = munmap((void *)buf, size);
            if (unmap_err != 0) {
                secerror("unable to unmap %ld bytes at %p (error %d)", (long)size, buf, rtn);
            }
        }
    }
    return rtn;
}

static bool removeFileWithSuffix(const char *basepath, const char *suffix) {
    bool result = false;
    char *path = NULL;
    asprintf(&path, "%s%s", basepath, suffix);
    if (path) {
        if (remove(path) == -1) {
            int error = errno;
            if (error == ENOENT) {
                result = true; // not an error if the file did not exist
            } else {
                secnotice("validupdate", "remove (%s): %s", path, strerror(error));
            }
        } else {
            result = true;
        }
        free(path);
    }
    return result;
}

static CFDataRef CF_RETURNS_RETAINED createPoliciesData(CFArrayRef policies) {
    /*
     * Given an array of CFNumber values (in the range 0..127),
     * allocate and return a CFDataRef representation. Per Valid specification,
     * a zero-length array is allowed, meaning no policies are permitted.
     */
    CFIndex count = (policies) ? CFArrayGetCount(policies) : -1;
    if (count < 0 || count > 127) {
        return NULL; /* either no constraints, or far more than we expect. */
    }
    CFDataRef data = NULL;
    CFIndex length = 1 + (sizeof(int8_t) * count);
    int8_t *bytes = malloc(length);
    if (bytes) {
        int8_t *p = bytes;
        *p++ = (int8_t)(count & 0xFF);
        for (CFIndex idx = 0; idx < count; idx++) {
            int8_t pval = 0;
            CFNumberRef value = (CFNumberRef)CFArrayGetValueAtIndex(policies, idx);
            if (isNumber(value)) {
                (void)CFNumberGetValue(value, kCFNumberSInt8Type, &pval);
            }
            *p++ = pval;
        }
        data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)bytes, length);
    }
    free(bytes);
    return data;
}

static CFDataRef CF_RETURNS_RETAINED cfToHexData(CFDataRef data, bool prependWildcard) {
    if (!isData(data)) { return NULL; }
    CFIndex len = CFDataGetLength(data) * 2;
    CFMutableStringRef hex = CFStringCreateMutable(NULL, len+1);
    static const char* digits[]={
        "0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};
    if (prependWildcard) {
        CFStringAppendCString(hex, "%", 1);
    }
    const uint8_t* p = CFDataGetBytePtr(data);
    for (CFIndex i = 0; i < CFDataGetLength(data); i++) {
        CFStringAppendCString(hex, digits[p[i] >> 4], 1);
        CFStringAppendCString(hex, digits[p[i] & 0xf], 1);
    }
    CFDataRef result = CFStringCreateExternalRepresentation(NULL, hex, kCFStringEncodingUTF8, 0);
    CFReleaseSafe(hex);
    return result;
}

static bool copyFilterComponents(CFDataRef xmlData, CFDataRef * CF_RETURNS_RETAINED xor,
                                 CFArrayRef * CF_RETURNS_RETAINED params) {
    /*
       The 'xmlData' parameter is a flattened XML dictionary,
       containing 'xor' and 'params' keys. First order of
       business is to reconstitute the blob into components.
    */
    bool result = false;
    CFRetainSafe(xmlData);
    CFDataRef propListData = xmlData;
    /* Expand data blob if needed */
    CFDataRef inflatedData = copyInflatedData(propListData);
    if (inflatedData) {
        CFReleaseSafe(propListData);
        propListData = inflatedData;
    }
    CFDataRef xorData = NULL;
    CFArrayRef paramsArray = NULL;
    CFPropertyListRef nto1 = CFPropertyListCreateWithData(kCFAllocatorDefault, propListData, 0, NULL, NULL);
    CFReleaseSafe(propListData);
    if (nto1) {
        xorData = (CFDataRef)CFDictionaryGetValue((CFDictionaryRef)nto1, CFSTR("xor"));
        CFRetainSafe(xorData);
        paramsArray = (CFArrayRef)CFDictionaryGetValue((CFDictionaryRef)nto1, CFSTR("params"));
        CFRetainSafe(paramsArray);
        CFReleaseSafe(nto1);
    }
    result = (xorData && paramsArray);
    if (xor) {
        *xor = xorData;
    } else {
        CFReleaseSafe(xorData);
    }
    if (params) {
        *params = paramsArray;
    } else {
        CFReleaseSafe(paramsArray);
    }
    return result;
}

// MARK: -
// MARK: SecValidUpdate
/*
 ==============================================================================
   SecValidUpdate
 ==============================================================================
*/

CFAbsoluteTime gUpdateStarted = 0.0;
CFAbsoluteTime gNextUpdate = 0.0;
static CFIndex gUpdateInterval = 0;
static CFIndex gLastVersion = 0;

/* Update Format:
    1. The length of the signed data, as a 4-byte integer in network byte order.
    2. The signed data, which consists of:
        a. A 4-byte integer in network byte order, the count of plists to follow; and then for each plist:
            i. A 4-byte integer, the length of each plist
            ii. A plist, in binary form
        b. There may be other data after the plists in the signed data, described by a future version of this specification.
    3. The length of the following CMS blob, as a 4-byte integer in network byte order.
    4. A detached CMS signature of the signed data described above.
    5. There may be additional data after the CMS blob, described by a future version of this specification.

   Note: the difference between g2 and g3 format is the addition of the 4-byte count in (2a).
*/
static bool SecValidUpdateProcessData(SecRevocationDbConnectionRef dbc, CFIndex format, CFDataRef updateData, CFErrorRef *error) {
    bool result = false;
    if (!updateData || format < 2) {
        SecError(errSecParam, error, CFSTR("SecValidUpdateProcessData: invalid update format"));
        return result;
    }
    CFIndex version = 0;
    CFIndex interval = 0;
    const UInt8* p = CFDataGetBytePtr(updateData);
    size_t bytesRemaining = (p) ? (size_t)CFDataGetLength(updateData) : 0;
    /* make sure there is enough data to contain length and count */
    if (bytesRemaining < ((CFIndex)sizeof(uint32_t) * 2)) {
        secinfo("validupdate", "Skipping property list creation (length %ld is too short)", (long)bytesRemaining);
        SecError(errSecParam, error, CFSTR("SecValidUpdateProcessData: data length is too short"));
        return result;
    }
    uint32_t dataLength = SecSwapInt32Ptr(p);
    bytesRemaining -= sizeof(uint32_t);
    p += sizeof(uint32_t);

    /* get plist count (G3 format and later) */
    uint32_t plistCount = 1;
    uint32_t plistTotal = 1;
    if (format > kSecValidUpdateFormatG2) {
        plistCount = SecSwapInt32Ptr(p);
        plistTotal = plistCount;
        bytesRemaining -= sizeof(uint32_t);
        p += sizeof(uint32_t);
    }
    if (dataLength > bytesRemaining) {
        secinfo("validupdate", "Skipping property list creation (dataLength=%ld, bytesRemaining=%ld)",
                (long)dataLength, (long)bytesRemaining);
        SecError(errSecParam, error, CFSTR("SecValidUpdateProcessData: data longer than expected"));
        return result;
    }

    /* process each chunked plist */
    bool ok = true;
    CFErrorRef localError = NULL;
    uint32_t plistProcessed = 0;
    while (plistCount > 0 && bytesRemaining > 0) {
        CFPropertyListRef propertyList = NULL;
        uint32_t plistLength = dataLength;
        if (format > kSecValidUpdateFormatG2) {
            plistLength = SecSwapInt32Ptr(p);
            bytesRemaining -= sizeof(uint32_t);
            p += sizeof(uint32_t);
        }
        --plistCount;
        ++plistProcessed;

        if (plistLength <= bytesRemaining) {
            CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, p, plistLength, kCFAllocatorNull);
            propertyList = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
            CFReleaseNull(data);
        }
        if (isDictionary(propertyList)) {
            secdebug("validupdate", "Ingesting plist chunk %u of %u, length: %u",
                    plistProcessed, plistTotal, plistLength);
            CFIndex curVersion = -1;
            ok = ok && SecRevocationDbIngestUpdate(dbc, (CFDictionaryRef)propertyList, version, &curVersion, &localError);
            if (plistProcessed == 1) {
                dbc->precommitVersion = version = curVersion;
                // get server-provided interval
                CFTypeRef value = (CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)propertyList,
                                                                    CFSTR("check-again"));
                if (isNumber(value)) {
                    CFNumberGetValue((CFNumberRef)value, kCFNumberCFIndexType, &interval);
                }
                // get server-provided hash list
                value = (CFArrayRef)CFDictionaryGetValue((CFDictionaryRef)propertyList,
                                                         CFSTR("hash"));
                ok = _SecRevocationDbSetHashes(dbc, (CFArrayRef)value, &localError);
            }
            if (ok && curVersion < 0) {
                plistCount = 0; // we already had this version; skip remaining plists
                result = true;
            }
        } else {
            secinfo("validupdate", "Failed to deserialize update chunk %u of %u",
                    plistProcessed, plistTotal);
            SecError(errSecParam, error, CFSTR("SecValidUpdateProcessData: failed to get update chunk"));
            if (plistProcessed == 1) {
                gNextUpdate = SecRevocationDbComputeNextUpdateTime(0);
            }
        }
        /* All finished with this property list */
        CFReleaseSafe(propertyList);

        bytesRemaining -= plistLength;
        p += plistLength;
    }

    if (ok && version > 0) {
        secdebug("validupdate", "Update received: v%ld", (long)version);
        gLastVersion = version;
        gNextUpdate = SecRevocationDbComputeNextUpdateTime(interval);
        secdebug("validupdate", "Next update time: %f", gNextUpdate);
        result = true;
    }

    (void) CFErrorPropagate(localError, error);
    return result;
}

void SecValidUpdateVerifyAndIngest(CFDataRef updateData, CFStringRef updateServer, bool fullUpdate) {
    if (!updateData) {
        secnotice("validupdate", "invalid update data");
        return;
    }
    /* Verify CMS signature on signed data */
    if (!SecRevocationDbVerifyUpdate((void *)CFDataGetBytePtr(updateData), CFDataGetLength(updateData))) {
        secerror("failed to verify valid update");
        TrustdHealthAnalyticsLogErrorCode(TAEventValidUpdate, TAFatalError, errSecVerifyFailed);
        return;
    }
    /* Read current update source from database. */
    CFStringRef dbSource = SecRevocationDbCopyUpdateSource();
    if (dbSource && updateServer && (kCFCompareEqualTo != CFStringCompare(dbSource, updateServer,
        kCFCompareCaseInsensitive))) {
        secnotice("validupdate", "switching db source from \"%@\" to \"%@\"", dbSource, updateServer);
    }
    CFReleaseNull(dbSource);

    /* Ingest the update. This is now performed under a single immediate write transaction,
       so other writers are blocked (but not other readers), and the changes can be rolled back
       in their entirety if any error occurs. */
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    SecRevocationDbWith(^(SecRevocationDbRef rdb) {
        ok &= SecRevocationDbPerformWrite(rdb, &localError, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            if (fullUpdate) {
                /* Must completely replace existing database contents */
                secdebug("validupdate", "starting to process full update; clearing database");
                ok = ok && _SecRevocationDbRemoveAllEntries(dbc, blockError);
                ok = ok && _SecRevocationDbSetUpdateSource(dbc, updateServer, blockError);
                dbc->precommitVersion = 0;
                dbc->fullUpdate = true;
            }
            CFIndex startingVersion = dbc->precommitVersion;
            ok = ok && SecValidUpdateProcessData(dbc, kSecValidUpdateFormatG3, updateData, blockError);
            rdb->changed = ok && (startingVersion < dbc->precommitVersion);
            if (!ok) {
                secerror("failed to process valid update: %@", blockError ? *blockError : NULL);
                TrustdHealthAnalyticsLogErrorCode(TAEventValidUpdate, TAFatalError, errSecDecode);
            } else {
                TrustdHealthAnalyticsLogSuccess(TAEventValidUpdate);
            }
            return ok;
        });
        if (rdb->changed) {
            rdb->changed = false;
            bool verifyEnabled = false;
            CFBooleanRef value = (CFBooleanRef)CFPreferencesCopyValue(kVerifyEnabledKey,
                                 kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
            if (isBoolean(value)) {
                verifyEnabled = CFBooleanGetValue(value);
            }
            CFReleaseNull(value);
            if (verifyEnabled) {
                /* compute and verify database content hashes */
                ok = ok && SecRevocationDbPerformRead(rdb, &localError, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
                    ok = ok && SecRevocationDbComputeDigests(dbc, blockError);
                    if (!ok) {
                        /* digests failed to verify, so roll back to known-good snapshot */
                        (void) SecValidUpdateForceReplaceDatabase();
                    }
                    return ok;
                });
            }
            /* signal other trustd instances that the database has been updated */
            notify_post(kSecRevocationDbChanged);
        }
    });

    /* remember next update time in case of restart (separate write transaction) */
    (void) SecRevocationDbSetNextUpdateTime(gNextUpdate, NULL);

    CFReleaseSafe(localError);
}

static bool SecValidUpdateForceReplaceDatabase(void) {
    __block bool result = false;

    // write semaphore file that we will pick up when we next launch
    WithPathInRevocationInfoDirectory(CFSTR(kSecRevocationDbReplaceFile), ^(const char *utf8String) {
        struct stat sb;
        int fd = open(utf8String, O_WRONLY | O_CREAT, DEFFILEMODE);
        if (fd == -1 || fstat(fd, &sb)) {
            secnotice("validupdate", "unable to write %s (error %d)", utf8String, errno);
        } else {
            result = true;
        }
        if (fd >= 0) {
            CFErrorRef error = NULL;
            if (!TrustdChangeFileProtectionToClassD(utf8String, &error)) {
                secerror("failed to change replace valid db flag protection class: %@", error);
                CFReleaseNull(error);
            }
            close(fd);
        }
    });
    if (result) {
        // exit as gracefully as possible so we can replace the database
        secnotice("validupdate", "process exiting to replace db file");
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 3ull*NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            xpc_transaction_exit_clean();
        });
    }
    return result;
}

static bool SecValidUpdateSatisfiedLocally(CFStringRef server, CFIndex version, bool safeToReplace) {
    __block bool result = false;
    SecOTAPKIRef otapkiRef = NULL;
    bool relaunching = false;
    static int sNumLocalUpdates = 0;

    // if we've replaced the database with a local asset twice in a row,
    // something is wrong with it. Get this update from the server.
    if (sNumLocalUpdates > 1) {
        secdebug("validupdate", "%d consecutive db resets, ignoring local asset", sNumLocalUpdates);
        goto updateExit;
    }

    // if a non-production server is specified, we will not be able to use a
    // local production asset since its update sequence will be different.
    if (kCFCompareEqualTo != CFStringCompare(server, kValidUpdateProdServer,
        kCFCompareCaseInsensitive)) {
        secdebug("validupdate", "non-production server specified, ignoring local asset");
        goto updateExit;
    }

    // check static database asset(s)
    otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
    if (!otapkiRef) {
        goto updateExit;
    }
    CFIndex assetVersion = SecOTAPKIGetValidSnapshotVersion(otapkiRef);
    CFIndex assetFormat = SecOTAPKIGetValidSnapshotFormat(otapkiRef);
    // version <= 0 means the database is invalid or empty.
    // version > 0 means we have some version, but we need to see if a
    // newer version is available as a local asset.
    if (assetVersion <= version || assetFormat < kSecValidUpdateFormatG3) {
        // asset is not newer than ours, or its version is unknown
        goto updateExit;
    }

    // replace database only if safe to do so (i.e. called at startup)
    if (!safeToReplace) {
        relaunching = SecValidUpdateForceReplaceDatabase();
        goto updateExit;
    }

    // try to copy uncompressed database asset, if available
    const char *validDbPathBuf = SecOTAPKIGetValidDatabaseSnapshot(otapkiRef);
    if (validDbPathBuf) {
        WithPathInRevocationInfoDirectory(CFSTR(kSecRevocationDbFileName), ^(const char *path) {
            secdebug("validupdate", "will copy data from \"%s\"", validDbPathBuf);
            copyfile_state_t state = copyfile_state_alloc();
            int retval = copyfile(validDbPathBuf, path, state, COPYFILE_DATA);
            copyfile_state_free(state);
            if (retval < 0) {
                secnotice("validupdate", "copyfile error %d", retval);
            } else {
                CFErrorRef localError = NULL;
                result = TrustdChangeFileProtectionToClassD(path, &localError);
                if (!result) {
                    secerror("failed to change protection class of copied valid snapshot: %@", localError);
                    CFReleaseNull(localError);
                }
            }
        });
    }

updateExit:
    CFReleaseNull(otapkiRef);
    if (result) {
        sNumLocalUpdates++;
        gLastVersion = SecRevocationDbGetVersion();
        // note: snapshot should already have latest schema and production source,
        // but set it here anyway so we don't keep trying to replace the db.
        SecRevocationDbWith(^(SecRevocationDbRef db) {
            (void)SecRevocationDbSetUpdateSource(db, server);
            (void)SecRevocationDbUpdateSchema(db);
        });
        gUpdateStarted = 0;
        secdebug("validupdate", "local update to g%ld/v%ld complete at %f",
                 (long)SecRevocationDbGetUpdateFormat(), (long)gLastVersion,
                 (double)CFAbsoluteTimeGetCurrent());
    } else {
        sNumLocalUpdates = 0; // reset counter
    }
    if (relaunching) {
        // request is locally satisfied; don't schedule a network update
        result = true;
    }
    return result;
}

static bool SecValidUpdateSchedule(bool updateEnabled, CFStringRef server, CFIndex version) {
    /* Check if we have a later version available locally */
    if (SecValidUpdateSatisfiedLocally(server, version, false)) {
        return true;
    }

    /* If update not permitted return */
    if (!updateEnabled) {
        secnotice("validupdate", "skipping update");
        return false;
    }

#if !TARGET_OS_BRIDGE
    /* Schedule as a maintenance task */
    secdebug("validupdate", "will fetch v%lu from \"%@\"", (unsigned long)version, server);
    return SecValidUpdateRequest(SecRevocationDbGetUpdateQueue(), server, version);
#else
    return false;
#endif
}

static CFStringRef SecRevocationDbGetDefaultServer(void) {
#if !TARGET_OS_WATCH && !TARGET_OS_BRIDGE
    #if RC_SEED_BUILD
        CFStringRef defaultServer = kValidUpdateSeedServer;
    #else // !RC_SEED_BUILD
        CFStringRef defaultServer = kValidUpdateProdServer;
    #endif // !RC_SEED_BUILD
        if (os_variant_has_internal_diagnostics("com.apple.security")) {
            defaultServer = kValidUpdateCarryServer;
        }
        return defaultServer;
#else // TARGET_OS_WATCH || TARGET_OS_BRIDGE
    /* Because watchOS and bridgeOS can't update over the air, we should
     * always use the prod server so that the valid database built into the
     * image is used. */
    return kValidUpdateProdServer;
#endif
}

static CF_RETURNS_RETAINED CFStringRef SecRevocationDbCopyServer(void) {
    /* Prefer a in-process setting for the update server, as used in testing */
    CFTypeRef value = CFPreferencesCopyAppValue(kUpdateServerKey, kSecPrefsDomain);
    if (!value) {
        value = CFPreferencesCopyValue(kUpdateServerKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    }
    CFStringRef server = NULL;
    if (isString(value)) {
        server = (CFStringRef)value;
    } else {
        CFReleaseNull(value);
        server = CFRetainSafe(SecRevocationDbGetDefaultServer());
    }
    return server;
}

void SecRevocationDbInitialize() {
    if (!isDbOwner()) { return; }
    os_transaction_t transaction = os_transaction_create("com.apple.trustd.valid.initialize");
    __block bool initializeDb = false;

    /* create base path if it doesn't exist */
    WithPathInRevocationInfoDirectory(NULL, ^(const char *utf8String) {
        (void)mkpath_np(utf8String, 0755);
    });

    /* check semaphore file */
    WithPathInRevocationInfoDirectory(CFSTR(kSecRevocationDbReplaceFile), ^(const char *path) {
        struct stat sb;
        if (stat(path, &sb) == 0) {
            initializeDb = true; /* file was found, so we will replace the database */
            if (remove(path) == -1) {
                int error = errno;
                secnotice("validupdate", "remove (%s): %s", path, strerror(error));
            }
        }
    });

    /* check database */
    WithPathInRevocationInfoDirectory(CFSTR(kSecRevocationDbFileName), ^(const char *path) {
        if (initializeDb) {
            /* remove old database file(s) */
            (void)removeFileWithSuffix(path, "");
            (void)removeFileWithSuffix(path, "-journal");
            (void)removeFileWithSuffix(path, "-shm");
            (void)removeFileWithSuffix(path, "-wal");
        }
        else {
            struct stat sb;
            if (stat(path, &sb) == -1) {
                initializeDb = true; /* file not found, so we will create the database */
            }
        }
    });

    if (!initializeDb) {
        os_release(transaction);
        return; /* database exists and doesn't need replacing */
    }

    /* initialize database from local asset */
    CFStringRef server = SecRevocationDbCopyServer();
    CFIndex version = 0;
    secnotice("validupdate", "initializing database");
    if (!SecValidUpdateSatisfiedLocally(server, version, true)) {
#if !TARGET_OS_BRIDGE
        /* Schedule full update as a maintenance task */
        (void)SecValidUpdateRequest(SecRevocationDbGetUpdateQueue(), server, version);
#endif
    }
    CFReleaseSafe(server);
    os_release(transaction);
}


// MARK: -
// MARK: SecValidInfoRef
/*
 ==============================================================================
   SecValidInfoRef
 ==============================================================================
*/

CFGiblisWithCompareFor(SecValidInfo);

static SecValidInfoRef SecValidInfoCreate(SecValidInfoFormat format,
                                          CFOptionFlags flags,
                                          bool isOnList,
                                          CFDataRef certHash,
                                          CFDataRef issuerHash,
                                          CFDataRef anchorHash,
                                          CFDateRef notBeforeDate,
                                          CFDateRef notAfterDate,
                                          CFDataRef nameConstraints,
                                          CFDataRef policyConstraints) {
    SecValidInfoRef validInfo;
    validInfo = CFTypeAllocate(SecValidInfo, struct __SecValidInfo, kCFAllocatorDefault);
    if (!validInfo) { return NULL; }

    CFRetainSafe(certHash);
    CFRetainSafe(issuerHash);
    CFRetainSafe(anchorHash);
    CFRetainSafe(notBeforeDate);
    CFRetainSafe(notAfterDate);
    CFRetainSafe(nameConstraints);
    CFRetainSafe(policyConstraints);

    validInfo->format = format;
    validInfo->certHash = certHash;
    validInfo->issuerHash = issuerHash;
    validInfo->anchorHash = anchorHash;
    validInfo->isOnList = isOnList;
    validInfo->valid = (flags & kSecValidInfoAllowlist);
    validInfo->complete = (flags & kSecValidInfoComplete);
    validInfo->checkOCSP = (flags & kSecValidInfoCheckOCSP);
    validInfo->knownOnly = (flags & kSecValidInfoKnownOnly);
    validInfo->requireCT = (flags & kSecValidInfoRequireCT);
    validInfo->noCACheck = (flags & kSecValidInfoNoCAv2Check);
    validInfo->overridable = (flags & kSecValidInfoOverridable);
    validInfo->hasDateConstraints = (flags & kSecValidInfoDateConstraints);
    validInfo->hasNameConstraints = (flags & kSecValidInfoNameConstraints);
    validInfo->hasPolicyConstraints = (flags & kSecValidInfoPolicyConstraints);
    validInfo->notBeforeDate = notBeforeDate;
    validInfo->notAfterDate = notAfterDate;
    validInfo->nameConstraints = nameConstraints;
    validInfo->policyConstraints = policyConstraints;

    return validInfo;
}

static void SecValidInfoDestroy(CFTypeRef cf) {
    SecValidInfoRef validInfo = (SecValidInfoRef)cf;
    if (validInfo) {
        CFReleaseNull(validInfo->certHash);
        CFReleaseNull(validInfo->issuerHash);
        CFReleaseNull(validInfo->anchorHash);
        CFReleaseNull(validInfo->notBeforeDate);
        CFReleaseNull(validInfo->notAfterDate);
        CFReleaseNull(validInfo->nameConstraints);
        CFReleaseNull(validInfo->policyConstraints);
    }
}

void SecValidInfoSetAnchor(SecValidInfoRef validInfo, SecCertificateRef anchor) {
    if (!validInfo) {
        return;
    }
    CFDataRef anchorHash = NULL;
    if (anchor) {
        anchorHash = SecCertificateCopySHA256Digest(anchor);

        /* clear no-ca flag for anchors where we want OCSP checked [32523118] */
        if (SecIsAppleTrustAnchor(anchor, 0)) {
            validInfo->noCACheck = false;
        }
    }
    CFReleaseNull(validInfo->anchorHash);
    validInfo->anchorHash = anchorHash;
}

static Boolean SecValidInfoCompare(CFTypeRef a, CFTypeRef b) {
    SecValidInfoRef validInfoA = (SecValidInfoRef)a;
    SecValidInfoRef validInfoB = (SecValidInfoRef)b;
    if (validInfoA == validInfoB) {
        return true;
    }
    if (!validInfoA || !validInfoB ||
        (CFGetTypeID(a) != SecValidInfoGetTypeID()) ||
        (CFGetTypeID(b) != SecValidInfoGetTypeID())) {
        return false;
    }
    return CFEqualSafe(validInfoA->certHash, validInfoB->certHash) && CFEqualSafe(validInfoA->issuerHash, validInfoB->issuerHash);
}

static CFStringRef SecValidInfoCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecValidInfoRef validInfo = (SecValidInfoRef)cf;
    CFStringRef certHash = CFDataCopyHexString(validInfo->certHash);
    CFStringRef issuerHash = CFDataCopyHexString(validInfo->issuerHash);
    CFStringRef desc = CFStringCreateWithFormat(NULL, formatOptions, CFSTR("validInfo certHash: %@ issuerHash: %@"), certHash, issuerHash);
    CFReleaseNull(certHash);
    CFReleaseNull(issuerHash);
    return desc;
}


// MARK: -
// MARK: SecRevocationDb
/*
 ==============================================================================
   SecRevocationDb
 ==============================================================================
*/

static CFIndex _SecRevocationDbGetUpdateVersion(CFStringRef server) {
    // determine version of our current database
    CFIndex version = SecRevocationDbGetVersion();
    secdebug("validupdate", "got version %ld from db", (long)version);
    if (version <= 0) {
        if (gLastVersion > 0) {
            secdebug("validupdate", "error getting version; using last good version: %ld", (long)gLastVersion);
        }
        version = gLastVersion;
    }

    // determine source of our current database
    // (if this ever changes, we will need to reload the db)
    CFStringRef db_source = SecRevocationDbCopyUpdateSource();
    if (!db_source) {
        db_source = (CFStringRef) CFRetain(kValidUpdateProdServer);
    }

    // determine whether we need to recreate the database
    CFIndex db_version = SecRevocationDbGetSchemaVersion();
    CFIndex db_format = SecRevocationDbGetUpdateFormat();
    if (db_version < kSecRevocationDbSchemaVersion ||
        db_format < kSecRevocationDbUpdateFormat ||
        kCFCompareEqualTo != CFStringCompare(server, db_source, kCFCompareCaseInsensitive)) {
        // we need to fully rebuild the db contents, so we set our version to 0.
        version = gLastVersion = 0;
    }
    CFReleaseNull(db_source);
    return version;
}

static bool _SecRevocationDbIsUpdateEnabled(void) {
    CFTypeRef value = NULL;
    // determine whether update fetching is enabled
#if !TARGET_OS_WATCH && !TARGET_OS_BRIDGE
    // Valid update fetching was initially enabled on macOS 10.13 and iOS 11.0.
    // This conditional has been changed to include every platform and version
    // except for those where the db should not be updated over the air.
    bool updateEnabled = true;
#else
    bool updateEnabled = false;
#endif
    value = (CFBooleanRef)CFPreferencesCopyValue(kUpdateEnabledKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isBoolean(value)) {
        updateEnabled = CFBooleanGetValue((CFBooleanRef)value);
    }
    CFReleaseNull(value);
    return updateEnabled;
}

/* SecRevocationDbCheckNextUpdate returns true if we dispatched an
   update request, otherwise false.
*/
static bool _SecRevocationDbCheckNextUpdate(void) {
    // are we the db owner instance?
    if (!isDbOwner()) {
        return false;
    }
    CFTypeRef value = NULL;

    // is it time to check?
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime minNextUpdate = now + gUpdateInterval;
    gUpdateStarted = now;

    if (0 == gNextUpdate) {
        // first time we're called, check if we have a saved nextUpdate value
        gNextUpdate = SecRevocationDbGetNextUpdateTime();
        minNextUpdate = now;
        if (gNextUpdate < minNextUpdate) {
            gNextUpdate = minNextUpdate;
        }
        // allow pref to override update interval, if it exists
        CFIndex interval = -1;
        value = (CFNumberRef)CFPreferencesCopyValue(kUpdateIntervalKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if (isNumber(value)) {
            if (CFNumberGetValue((CFNumberRef)value, kCFNumberCFIndexType, &interval)) {
                if (interval < kSecMinUpdateInterval) {
                    interval = kSecMinUpdateInterval;
                } else if (interval > kSecMaxUpdateInterval) {
                    interval = kSecMaxUpdateInterval;
                }
            }
        }
        CFReleaseNull(value);
        gUpdateInterval = kSecStdUpdateInterval;
        if (interval > 0) {
            gUpdateInterval = interval;
        }
        // pin next update time to the preferred update interval
        if (gNextUpdate > (gUpdateStarted + gUpdateInterval)) {
            gNextUpdate = gUpdateStarted + gUpdateInterval;
        }
        secdebug("validupdate", "next update at %f (in %f seconds)",
                 (double)gUpdateStarted, (double)gNextUpdate-gUpdateStarted);
    }
    if (gNextUpdate > now) {
        gUpdateStarted = 0;
        return false;
    }
    secnotice("validupdate", "starting update");

    // set minimum next update time here in case we can't get an update
    gNextUpdate = minNextUpdate;

    // determine which server to query
    CFStringRef server = SecRevocationDbCopyServer();

    // determine version to update from
    CFIndex version = _SecRevocationDbGetUpdateVersion(server);

    // determine if update is enabled for this device
    bool updateEnabled = _SecRevocationDbIsUpdateEnabled();

    // Schedule maintenance work
    bool result = SecValidUpdateSchedule(updateEnabled, server, version);
    CFReleaseNull(server);
    return result;
}

void SecRevocationDbCheckNextUpdate(void) {
    static dispatch_once_t once;
    static sec_action_t action;

    dispatch_once(&once, ^{
        dispatch_queue_t update_queue = SecRevocationDbGetUpdateQueue();
        action = sec_action_create_with_queue(update_queue, "update_check", kSecMinUpdateInterval);
        sec_action_set_handler(action, ^{
            os_transaction_t transaction = os_transaction_create("com.apple.trustd.valid.checkNextUpdate");
            (void)_SecRevocationDbCheckNextUpdate();
            os_release(transaction);
        });
    });
    sec_action_perform(action);
}

bool SecRevocationDbUpdate(CFErrorRef *error)
{
    // are we the db owner instance?
    if (!isDbOwner()) {
        return SecError(errSecWrPerm, error, CFSTR("Unable to update Valid DB from user agent"));
    }

    if (!_SecRevocationDbIsUpdateEnabled()) {
        return SecError(errSecWrPerm, error, CFSTR("Valid updates not enabled on this device"));
    }

    CFStringRef server = SecRevocationDbCopyServer();
    CFIndex version = _SecRevocationDbGetUpdateVersion(server);

    secdebug("validupdate", "will fetch v%lu from \"%@\" now", (unsigned long)version, server);
    bool result = SecValidUpdateUpdateNow(SecRevocationDbGetUpdateQueue(), server, version);
    CFReleaseNull(server);
    return result;
}

/*  This function verifies an update, in this format:
    1) unsigned 32-bit network-byte-order length of binary plist
    2) binary plist data
    3) unsigned 32-bit network-byte-order length of CMS message
    4) CMS message (containing certificates and signature over binary plist)

    The length argument is the total size of the packed update data.
*/
bool SecRevocationDbVerifyUpdate(void *update, CFIndex length) {
    if (!update || length <= (CFIndex)sizeof(uint32_t)) {
        return false;
    }
    uint32_t plistLength = SecSwapInt32Ptr(update);
    if ((plistLength + (CFIndex)(sizeof(uint32_t)*2)) > (uint64_t) length) {
        secdebug("validupdate", "ERROR: reported plist length (%lu)+%lu exceeds total length (%lu)\n",
                (unsigned long)plistLength, (unsigned long)sizeof(uint32_t)*2, (unsigned long)length);
        return false;
    }
    uint8_t *plistData = (uint8_t *)update + sizeof(uint32_t);
    uint8_t *sigData = (uint8_t *)plistData + plistLength;
    uint32_t sigLength = SecSwapInt32Ptr(sigData);
    sigData += sizeof(uint32_t);
    if ((plistLength + sigLength + (CFIndex)(sizeof(uint32_t) * 2)) != (uint64_t) length) {
        secdebug("validupdate", "ERROR: reported lengths do not add up to total length\n");
        return false;
    }

    OSStatus status = 0;
    CMSSignerStatus signerStatus;
    CMSDecoderRef cms = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDataRef content = NULL;

    if ((content = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
        (const UInt8 *)plistData, (CFIndex)plistLength, kCFAllocatorNull)) == NULL) {
        secdebug("validupdate", "CFDataCreateWithBytesNoCopy failed (%ld bytes)\n", (long)plistLength);
        return false;
    }

    if ((status = CMSDecoderCreate(&cms)) != errSecSuccess) {
        secdebug("validupdate", "CMSDecoderCreate failed with error %d\n", (int)status);
        goto verifyExit;
    }
    if ((status = CMSDecoderUpdateMessage(cms, sigData, sigLength)) != errSecSuccess) {
        secdebug("validupdate", "CMSDecoderUpdateMessage failed with error %d\n", (int)status);
        goto verifyExit;
    }
    if ((status = CMSDecoderSetDetachedContent(cms, content)) != errSecSuccess) {
        secdebug("validupdate", "CMSDecoderSetDetachedContent failed with error %d\n", (int)status);
        goto verifyExit;
    }
    if ((status = CMSDecoderFinalizeMessage(cms)) != errSecSuccess) {
        secdebug("validupdate", "CMSDecoderFinalizeMessage failed with error %d\n", (int)status);
        goto verifyExit;
    }

    policy = SecPolicyCreateApplePinned(CFSTR("ValidUpdate"), // kSecPolicyNameAppleValidUpdate
                CFSTR("1.2.840.113635.100.6.2.10"), // System Integration 2 Intermediate Certificate
                CFSTR("1.2.840.113635.100.6.51"));  // Valid update signing OID

    // Check that the first signer actually signed this message.
    if ((status = CMSDecoderCopySignerStatus(cms, 0, policy,
            false, &signerStatus, &trust, NULL)) != errSecSuccess) {
        secdebug("validupdate", "CMSDecoderCopySignerStatus failed with error %d\n", (int)status);
        goto verifyExit;
    }
    // Make sure the signature verifies against the detached content
    if (signerStatus != kCMSSignerValid) {
        secdebug("validupdate", "ERROR: signature did not verify (signer status %d)\n", (int)signerStatus);
        status = errSecInvalidSignature;
        goto verifyExit;
    }
    // Make sure the signing certificate is valid for the specified policy
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    status = SecTrustEvaluate(trust, &trustResult);
    if (status != errSecSuccess) {
        secdebug("validupdate", "SecTrustEvaluate failed with error %d (trust=%p)\n", (int)status, (void *)trust);
    } else if (!(trustResult == kSecTrustResultUnspecified || trustResult == kSecTrustResultProceed)) {
        secdebug("validupdate", "SecTrustEvaluate failed with trust result %d\n", (int)trustResult);
        status = errSecVerificationFailure;
        goto verifyExit;
    }

verifyExit:
    CFReleaseSafe(content);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(cms);

    return (status == errSecSuccess);
}

CFAbsoluteTime SecRevocationDbComputeNextUpdateTime(CFIndex updateInterval) {
    CFIndex interval = updateInterval;
    // try to use interval preference if it exists
    CFTypeRef value = (CFNumberRef)CFPreferencesCopyValue(kUpdateIntervalKey, kSecPrefsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (isNumber(value)) {
        CFNumberGetValue((CFNumberRef)value, kCFNumberCFIndexType, &interval);
    }
    CFReleaseNull(value);

    if (interval <= 0) {
        interval = kSecStdUpdateInterval;
    }

    // sanity check
    if (interval < kSecMinUpdateInterval) {
        interval = kSecMinUpdateInterval;
    } else if (interval > kSecMaxUpdateInterval) {
        interval = kSecMaxUpdateInterval;
    }

    // compute randomization factor, between 0 and 50% of the interval
    CFIndex fuzz = arc4random() % (long)(interval/2.0);
    CFAbsoluteTime nextUpdate =  CFAbsoluteTimeGetCurrent() + interval + fuzz;
    secdebug("validupdate", "next update in %ld seconds", (long)(interval + fuzz));
    return nextUpdate;
}

void SecRevocationDbComputeAndSetNextUpdateTime(void) {
    gNextUpdate = SecRevocationDbComputeNextUpdateTime(0);
    (void) SecRevocationDbSetNextUpdateTime(gNextUpdate, NULL);
    gUpdateStarted = 0; /* no update is currently in progress */
}

bool SecRevocationDbIngestUpdate(SecRevocationDbConnectionRef dbc, CFDictionaryRef update, CFIndex chunkVersion, CFIndex *outVersion, CFErrorRef *error) {
    bool ok = false;
    CFIndex version = 0;
    CFErrorRef localError = NULL;
    if (!update) {
        SecError(errSecParam, &localError, CFSTR("SecRevocationDbIngestUpdate: invalid update parameter"));
        goto setVersionAndExit;
    }
    CFTypeRef value = (CFNumberRef)CFDictionaryGetValue(update, CFSTR("version"));
    if (isNumber(value)) {
        if (!CFNumberGetValue((CFNumberRef)value, kCFNumberCFIndexType, &version)) {
            version = 0;
        }
    }
    if (version == 0) {
        // only the first chunk will have a version, so the second and
        // subsequent chunks will need to pass it in chunkVersion.
        version = chunkVersion;
    }
    // check precommitted version since update hasn't been committed yet
    CFIndex curVersion = dbc->precommitVersion;
    if (version > curVersion || chunkVersion > 0) {
        ok = _SecRevocationDbApplyUpdate(dbc, update, version, &localError);
        secdebug("validupdate", "_SecRevocationDbApplyUpdate=%s, v%ld, precommit=%ld, full=%s",
                 (ok) ? "1" : "0", (long)version, (long)dbc->precommitVersion,
                 (dbc->fullUpdate) ? "1" : "0");
    } else {
        secdebug("validupdate", "we have v%ld, skipping update to v%ld",
                 (long)curVersion, (long)version);
        version = -1; // invalid, so we know to skip subsequent chunks
        ok = true; // this is not an error condition
    }
setVersionAndExit:
    if (outVersion) {
        *outVersion = version;
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}


/* Database schema */

/* admin table holds these key-value (or key-ival) pairs:
 'version' (integer)    // version of database content
 'check_again' (double) // CFAbsoluteTime of next check (optional)
 'db_version' (integer) // version of database schema
 'db_hash' (blob)       // SHA-256 database hash
 --> entries in admin table are unique by text key

 issuers table holds map of issuing CA hashes to group identifiers:
 groupid (integer)     // associated group identifier in group ID table
 issuer_hash (blob)    // SHA-256 hash of issuer certificate (primary key)
 --> entries in issuers table are unique by issuer_hash;
 multiple issuer entries may have the same groupid!

 groups table holds records with these attributes:
 groupid (integer)     // ordinal ID associated with this group entry
 flags (integer)       // a bitmask of the following values:
   kSecValidInfoComplete    (0x00000001) set if we have all revocation info for this issuer group
   kSecValidInfoCheckOCSP   (0x00000002) set if must check ocsp for certs from this issuer group
   kSecValidInfoKnownOnly   (0x00000004) set if any CA from this issuer group must be in database
   kSecValidInfoRequireCT   (0x00000008) set if all certs from this issuer group must have SCTs
   kSecValidInfoAllowlist   (0x00000010) set if this entry describes valid certs (i.e. is allowed)
   kSecValidInfoNoCACheck   (0x00000020) set if this entry does not require an OCSP check to accept (deprecated)
   kSecValidInfoOverridable (0x00000040) set if the trust status is recoverable and can be overridden
   kSecValidInfoDateConstraints (0x00000080) set if this group has not-before or not-after constraints
   kSecValidInfoNameConstraints (0x00000100) set if this group has name constraints in database
   kSecValidInfoPolicyConstraints (0x00000200) set if this group has policy constraints in database
   kSecValidInfoNoCAv2Check (0x00000400) set if this entry does not require an OCSP check to accept
 format (integer)      // an integer describing format of entries:
   kSecValidInfoFormatUnknown (0) unknown format
   kSecValidInfoFormatSerial  (1) serial number, not greater than 20 bytes in length
   kSecValidInfoFormatSHA256  (2) SHA-256 hash, 32 bytes in length
   kSecValidInfoFormatNto1    (3) filter data blob of arbitrary length
 data (blob)           // Bloom filter data if format is 'nto1', otherwise NULL
 policies (blob)       // NULL, or uint8_t count value followed by array of int8_t policy values
 --> entries in groups table are unique by groupid

 serials table holds serial number blobs with these attributes:
 groupid (integer)     // identifier for issuer group in the groups table
 serial (blob)         // serial number
 --> entries in serials table are unique by serial and groupid

 hashes table holds SHA-256 hashes of certificates with these attributes:
 groupid (integer)     // identifier for issuer group in the groups table
 sha256 (blob)         // SHA-256 hash of subject certificate
 --> entries in hashes table are unique by sha256 and groupid

 dates table holds notBefore and notAfter dates (as CFAbsoluteTime) with these attributes:
 groupid (integer)     // identifier for issuer group in the groups table (primary key)
 notbefore (real)      // issued certs are invalid if their notBefore is prior to this date
 notafter (real)       // issued certs are invalid after this date (or their notAfter, if earlier)
 --> entries in dates table are unique by groupid, and only exist if kSecValidInfoDateConstraints is true

 */
#define createTablesSQL   CFSTR("CREATE TABLE IF NOT EXISTS admin(" \
                                    "key TEXT PRIMARY KEY NOT NULL," \
                                    "ival INTEGER NOT NULL," \
                                    "value BLOB" \
                                ");" \
                                "CREATE TABLE IF NOT EXISTS issuers(" \
                                    "groupid INTEGER NOT NULL," \
                                    "issuer_hash BLOB PRIMARY KEY NOT NULL" \
                                ");" \
                                "CREATE INDEX IF NOT EXISTS issuer_idx ON issuers(issuer_hash);" \
                                "CREATE TABLE IF NOT EXISTS groups(" \
                                    "groupid INTEGER PRIMARY KEY AUTOINCREMENT," \
                                    "flags INTEGER," \
                                    "format INTEGER," \
                                    "data BLOB," \
                                    "policies BLOB" \
                                ");" \
                                "CREATE TABLE IF NOT EXISTS serials(" \
                                    "groupid INTEGER NOT NULL," \
                                    "serial BLOB NOT NULL," \
                                    "UNIQUE(groupid,serial)" \
                                ");" \
                                "CREATE TABLE IF NOT EXISTS hashes(" \
                                    "groupid INTEGER NOT NULL," \
                                    "sha256 BLOB NOT NULL," \
                                    "UNIQUE(groupid,sha256)" \
                                ");" \
                                "CREATE TABLE IF NOT EXISTS dates(" \
                                    "groupid INTEGER PRIMARY KEY NOT NULL," \
                                    "notbefore REAL," \
                                    "notafter REAL" \
                                ");" \
                                "CREATE TRIGGER IF NOT EXISTS group_del " \
                                    "BEFORE DELETE ON groups FOR EACH ROW " \
                                    "BEGIN " \
                                        "DELETE FROM serials WHERE groupid=OLD.groupid; " \
                                        "DELETE FROM hashes WHERE groupid=OLD.groupid; " \
                                        "DELETE FROM issuers WHERE groupid=OLD.groupid; " \
                                        "DELETE FROM dates WHERE groupid=OLD.groupid; " \
                                    "END;")

#define selectGroupIdSQL  CFSTR("SELECT DISTINCT groupid " \
    "FROM issuers WHERE issuer_hash=?")
#define selectVersionSQL CFSTR("SELECT ival FROM admin " \
    "WHERE key='version'")
#define selectDbVersionSQL CFSTR("SELECT ival FROM admin " \
    "WHERE key='db_version'")
#define selectDbFormatSQL CFSTR("SELECT ival FROM admin " \
    "WHERE key='db_format'")
#define selectDbHashSQL CFSTR("SELECT value FROM admin " \
    "WHERE key='db_hash'")
#define selectDbSourceSQL CFSTR("SELECT value FROM admin " \
    "WHERE key='db_source'")
#define selectNextUpdateSQL CFSTR("SELECT value FROM admin " \
    "WHERE key='check_again'")
#define selectUpdateIntervalSQL CFSTR("SELECT ival FROM admin " \
    "WHERE key='interval'")
#define selectGroupRecordSQL CFSTR("SELECT flags,format,data,policies " \
    "FROM groups WHERE groupid=?")
#define selectSerialRecordSQL CFSTR("SELECT rowid FROM serials " \
    "WHERE groupid=? AND serial=?")
#define selectDateRecordSQL CFSTR("SELECT notbefore,notafter FROM " \
    "dates WHERE groupid=?")
#define selectHashRecordSQL CFSTR("SELECT rowid FROM hashes " \
    "WHERE groupid=? AND sha256=?")
#define insertAdminRecordSQL CFSTR("INSERT OR REPLACE INTO admin " \
    "(key,ival,value) VALUES (?,?,?)")
#define insertIssuerRecordSQL CFSTR("INSERT OR REPLACE INTO issuers " \
    "(groupid,issuer_hash) VALUES (?,?)")
#define insertGroupRecordSQL CFSTR("INSERT OR REPLACE INTO groups " \
    "(groupid,flags,format,data,policies) VALUES (?,?,?,?,?)")
#define insertSerialRecordSQL CFSTR("INSERT OR REPLACE INTO serials " \
    "(groupid,serial) VALUES (?,?)")
#define deleteSerialRecordSQL CFSTR("DELETE FROM serials " \
    "WHERE groupid=? AND hex(serial) LIKE ?")
#define insertSha256RecordSQL CFSTR("INSERT OR REPLACE INTO hashes " \
    "(groupid,sha256) VALUES (?,?)")
#define deleteSha256RecordSQL CFSTR("DELETE FROM hashes " \
    "WHERE groupid=? AND hex(sha256) LIKE ?")
#define insertDateRecordSQL CFSTR("INSERT OR REPLACE INTO dates " \
    "(groupid,notbefore,notafter) VALUES (?,?,?)")
#define deleteGroupRecordSQL CFSTR("DELETE FROM groups " \
    "WHERE groupid=?")
#define deleteGroupIssuersSQL CFSTR("DELETE FROM issuers " \
    "WHERE groupid=?")
#define addPoliciesColumnSQL CFSTR("ALTER TABLE groups " \
    "ADD COLUMN policies BLOB")
#define updateGroupPoliciesSQL CFSTR("UPDATE OR IGNORE groups " \
    "SET policies=? WHERE groupid=?")

#define updateConstraintsTablesSQL CFSTR("" \
"CREATE TABLE IF NOT EXISTS dates(" \
    "groupid INTEGER PRIMARY KEY NOT NULL," \
    "notbefore REAL," \
    "notafter REAL" \
");")

#define updateGroupDeleteTriggerSQL CFSTR("" \
    "DROP TRIGGER IF EXISTS group_del;" \
    "CREATE TRIGGER group_del BEFORE DELETE ON groups FOR EACH ROW " \
    "BEGIN " \
        "DELETE FROM serials WHERE groupid=OLD.groupid; " \
        "DELETE FROM hashes WHERE groupid=OLD.groupid; " \
        "DELETE FROM issuers WHERE groupid=OLD.groupid; " \
        "DELETE FROM dates WHERE groupid=OLD.groupid; " \
    "END;")

#define deleteAllEntriesSQL CFSTR("" \
    "DELETE FROM groups; " \
    "DELETE FROM admin WHERE key='version'; " \
    "DELETE FROM sqlite_sequence")


/* Database management */

static SecDbRef SecRevocationDbCreate(CFStringRef path) {
    /* only the db owner should open a read-write connection. */
    __block bool readWrite = isDbOwner();
    mode_t mode = 0644;

    SecDbRef result = SecDbCreate(path, mode, readWrite, false, true, true, 1, ^bool (SecDbRef db, SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error) {
        __block bool ok = true;
        if (readWrite) {
            ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
                /* Create all database tables, indexes, and triggers.
                 * SecDbOpen will set auto_vacuum and journal_mode for us before we get called back.*/
                ok = ok && SecDbExec(dbconn, createTablesSQL, error);
                *commit = ok;
            });
        }
        if (!ok || (error && *error)) {
            CFIndex errCode = errSecInternalComponent;
            if (error && *error) {
                errCode = CFErrorGetCode(*error);
            }
            secerror("%s failed: %@", didCreate ? "Create" : "Open", error ? *error : NULL);
            TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationCreate, TAFatalError, errCode);
        }
        return ok;
    });

    return result;
}

static dispatch_once_t kSecRevocationDbOnce;
static SecRevocationDbRef kSecRevocationDb = NULL;

static SecRevocationDbRef SecRevocationDbInit(CFStringRef db_name) {
    SecRevocationDbRef rdb;
    dispatch_queue_attr_t attr;

    require(rdb = (SecRevocationDbRef)malloc(sizeof(struct __SecRevocationDb)), errOut);
    rdb->db = NULL;
    rdb->update_queue = NULL;
    rdb->updateInProgress = false;
    rdb->unsupportedVersion = false;
    rdb->changed = false;

    require(rdb->db = SecRevocationDbCreate(db_name), errOut);
    attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_BACKGROUND, 0);
    attr = dispatch_queue_attr_make_with_autorelease_frequency(attr, DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM);
    require(rdb->update_queue = dispatch_queue_create(NULL, attr), errOut);
    require(rdb->info_cache_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), errOut);
    require(rdb->info_cache = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);
    rdb->info_cache_lock = OS_UNFAIR_LOCK_INIT;

    if (!isDbOwner()) {
        /* register for changes signaled by the db owner instance */
        int out_token = 0;
        notify_register_dispatch(kSecRevocationDbChanged, &out_token, rdb->update_queue, ^(int __unused token) {
            secnotice("validupdate", "Got notification of database change");
            SecRevocationDbResetCaches();
        });
    }
    return rdb;

errOut:
    secdebug("validupdate", "Failed to create db at \"%@\"", db_name);
    if (rdb) {
        if (rdb->update_queue) {
            dispatch_release(rdb->update_queue);
        }
        CFReleaseSafe(rdb->db);
        free(rdb);
    }
    return NULL;
}

static CFStringRef SecRevocationDbCopyPath(void) {
    CFURLRef revDbURL = NULL;
    CFStringRef revInfoRelPath = NULL;
    if ((revInfoRelPath = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), kSecRevocationDbFileName)) != NULL) {
        revDbURL = SecCopyURLForFileInRevocationInfoDirectory(revInfoRelPath);
    }
    CFReleaseSafe(revInfoRelPath);

    CFStringRef revDbPath = NULL;
    if (revDbURL) {
        revDbPath = CFURLCopyFileSystemPath(revDbURL, kCFURLPOSIXPathStyle);
        CFRelease(revDbURL);
    }
    return revDbPath;
}

static void SecRevocationDbWith(void(^dbJob)(SecRevocationDbRef db)) {
    dispatch_once(&kSecRevocationDbOnce, ^{
        CFStringRef dbPath = SecRevocationDbCopyPath();
        if (dbPath) {
            kSecRevocationDb = SecRevocationDbInit(dbPath);
            CFRelease(dbPath);
            if (kSecRevocationDb && isDbOwner()) {
                /* check and update schema immediately after database is opened */
                SecRevocationDbUpdateSchema(kSecRevocationDb);
            }
        }
    });
    // Do pre job run work here (cancel idle timers etc.)
    if (kSecRevocationDb->updateInProgress) {
        return; // this would block since SecDb has an exclusive transaction lock
    }
    dbJob(kSecRevocationDb);
    // Do post job run work here (gc timer, etc.)
}

static bool SecRevocationDbPerformWrite(SecRevocationDbRef rdb, CFErrorRef *error,
                                        bool(^writeJob)(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError)) {
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok &= SecDbPerformWrite(rdb->db, &localError, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn, kSecDbImmediateTransactionType, &localError, ^(bool *commit) {
            SecRevocationDbConnectionRef dbc = SecRevocationDbConnectionInit(rdb, dbconn, &localError);
            ok = ok && writeJob(dbc, &localError);
            *commit = ok;
            free(dbc);
        });
    });
    ok &= CFErrorPropagate(localError, error);
    return ok;
}

static bool SecRevocationDbPerformRead(SecRevocationDbRef rdb, CFErrorRef *error,
                                       bool(^readJob)(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError)) {
    __block CFErrorRef localError = NULL;
    __block bool ok = true;

    ok &= SecDbPerformRead(rdb->db, &localError, ^(SecDbConnectionRef dbconn) {
        SecRevocationDbConnectionRef dbc = SecRevocationDbConnectionInit(rdb, dbconn, &localError);
        ok = ok && readJob(dbc, &localError);
        free(dbc);
    });
    ok &= CFErrorPropagate(localError, error);
    return ok;
}

static SecRevocationDbConnectionRef SecRevocationDbConnectionInit(SecRevocationDbRef db, SecDbConnectionRef dbconn, CFErrorRef *error) {
    SecRevocationDbConnectionRef dbc = NULL;
    CFErrorRef localError = NULL;

    dbc = (SecRevocationDbConnectionRef)malloc(sizeof(struct __SecRevocationDbConnection));
    if (dbc) {
        dbc->db = db;
        dbc->dbconn = dbconn;
        dbc->precommitVersion = (CFIndex)_SecRevocationDbGetVersion(dbc, &localError);
        dbc->precommitDbVersion = (CFIndex)_SecRevocationDbGetSchemaVersion(db, dbc, &localError);
        dbc->precommitInterval = 0; /* set only if we are explicitly given a new value */
        dbc->fullUpdate = false;
    }
    (void) CFErrorPropagate(localError, error);
    return dbc;
}

static CF_RETURNS_RETAINED CFDataRef createCacheKey(CFDataRef certHash, CFDataRef issuerHash) {
    CFMutableDataRef concat = CFDataCreateMutableCopy(NULL, 0, certHash);
    CFDataAppend(concat, issuerHash);
    CFDataRef result = SecSHA256DigestCreateFromData(NULL, concat);
    CFReleaseNull(concat);
    return result;
}

static CF_RETURNS_RETAINED SecValidInfoRef SecRevocationDbCacheRead(SecRevocationDbRef db,
                                                                     SecCertificateRef certificate,
                                                                     CFDataRef issuerHash) {
    if (!db) {
        return NULL;
    }
    SecValidInfoRef result = NULL;
    if (!db || !db->info_cache || !db->info_cache_list) {
        return result;
    }
    CFIndex ix = kCFNotFound;
    CFDataRef certHash = SecCertificateCopySHA256Digest(certificate);
    CFDataRef cacheKey = createCacheKey(certHash, issuerHash);

    os_unfair_lock_lock(&db->info_cache_lock); // grab the cache lock before using the cache
    if (0 <= (ix = CFArrayGetFirstIndexOfValue(db->info_cache_list,
                                               CFRangeMake(0, CFArrayGetCount(db->info_cache_list)),
                                               cacheKey))) {
        result = (SecValidInfoRef)CFDictionaryGetValue(db->info_cache, cacheKey);
        // Verify this really is the right result
        if (CFEqualSafe(result->certHash, certHash) && CFEqualSafe(result->issuerHash, issuerHash)) {
            // Cache hit. Move the entry to the bottom of the list.
            CFArrayRemoveValueAtIndex(db->info_cache_list, ix);
            CFArrayAppendValue(db->info_cache_list, cacheKey);
            secdebug("validcache", "cache hit: %@", cacheKey);
        } else {
            // Just remove this bad entry
            CFArrayRemoveValueAtIndex(db->info_cache_list, ix);
            CFDictionaryRemoveValue(db->info_cache, cacheKey);
            secdebug("validcache", "cache remove bad: %@", cacheKey);
            secnotice("validcache", "found a bad valid info cache entry at %ld", (long)ix);
        }
    }
    CFRetainSafe(result);
    os_unfair_lock_unlock(&db->info_cache_lock);
    CFReleaseSafe(certHash);
    CFReleaseSafe(cacheKey);
    return result;
}

static void SecRevocationDbCacheWrite(SecRevocationDbRef db,
                                       SecValidInfoRef validInfo) {
    if (!db || !validInfo || !db->info_cache || !db->info_cache_list) {
        return;
    }

    CFDataRef cacheKey = createCacheKey(validInfo->certHash, validInfo->issuerHash);

    os_unfair_lock_lock(&db->info_cache_lock); // grab the cache lock before using the cache
    // check to make sure another thread didn't add this entry to the cache already
    if (0 > CFArrayGetFirstIndexOfValue(db->info_cache_list,
                                        CFRangeMake(0, CFArrayGetCount(db->info_cache_list)),
                                        cacheKey)) {
        CFDictionaryAddValue(db->info_cache, cacheKey, validInfo);
        if (kSecRevocationDbCacheSize <= CFArrayGetCount(db->info_cache_list)) {
            // Remove least recently used cache entry.
            secdebug("validcache", "cache remove stale: %@", CFArrayGetValueAtIndex(db->info_cache_list, 0));
            CFDictionaryRemoveValue(db->info_cache, CFArrayGetValueAtIndex(db->info_cache_list, 0));
            CFArrayRemoveValueAtIndex(db->info_cache_list, 0);
        }
        CFArrayAppendValue(db->info_cache_list, cacheKey);
        secdebug("validcache", "cache add: %@", cacheKey);
    }
    os_unfair_lock_unlock(&db->info_cache_lock);
    CFReleaseNull(cacheKey);
}

static void SecRevocationDbCachePurge(SecRevocationDbRef db) {
    if (!db || !db->info_cache || !db->info_cache_list) {
        return;
    }

    /* grab the cache lock and clear all entries */
    os_unfair_lock_lock(&db->info_cache_lock);
    CFArrayRemoveAllValues(db->info_cache_list);
    CFDictionaryRemoveAllValues(db->info_cache);
    secdebug("validcache", "cache purge");
    os_unfair_lock_unlock(&db->info_cache_lock);
}

static int64_t _SecRevocationDbGetUpdateInterval(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    /* look up interval entry in admin table; returns -1 on error */
    __block int64_t interval = -1;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    ok = ok && SecDbWithSQL(dbc->dbconn, selectUpdateIntervalSQL, &localError, ^bool(sqlite3_stmt *selectInterval) {
        ok = ok && SecDbStep(dbc->dbconn, selectInterval, &localError, ^void(bool *stop) {
            interval = sqlite3_column_int64(selectInterval, 0);
            *stop = true;
        });
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbGetUpdateInterval failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return interval;
}

static bool _SecRevocationDbSetUpdateInterval(SecRevocationDbConnectionRef dbc, int64_t interval, CFErrorRef *error) {
    secdebug("validupdate", "setting interval to %lld", interval);

    __block CFErrorRef localError = NULL;
    __block bool ok = (dbc != NULL);
    ok = ok && SecDbWithSQL(dbc->dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertInterval) {
        const char *intervalKey = "interval";
        ok = ok && SecDbBindText(insertInterval, 1, intervalKey, strlen(intervalKey),
                            SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbBindInt64(insertInterval, 2,
                             (sqlite3_int64)interval, &localError);
        ok = ok && SecDbStep(dbc->dbconn, insertInterval, &localError, NULL);
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbSetUpdateInterval failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static CFArrayRef _SecRevocationDbCopyHashes(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    /* return a retained copy of the db_hash array stored in the admin table; or NULL on error */
    __block CFMutableArrayRef hashes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    __block bool ok = (dbc && hashes);
    __block CFErrorRef localError = NULL;

    ok = ok && SecDbWithSQL(dbc->dbconn, selectDbHashSQL, &localError, ^bool(sqlite3_stmt *selectDbHash) {
        ok = ok && SecDbStep(dbc->dbconn, selectDbHash, &localError, ^void(bool *stop) {
            uint8_t *p = (uint8_t *)sqlite3_column_blob(selectDbHash, 0);
            uint64_t len = sqlite3_column_bytes(selectDbHash, 0);
            CFIndex hashLen = CC_SHA256_DIGEST_LENGTH;
            while (p && len >= (uint64_t)hashLen) {
                CFDataRef hash = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)p, hashLen);
                if (hash) {
                    CFArrayAppendValue(hashes, hash);
                    CFReleaseNull(hash);
                }
                len -= hashLen;
                p += hashLen;
            }
            *stop = true;
        });
        return ok;
    });
    if (!ok || localError) {
        CFReleaseNull(hashes);
    }
    (void) CFErrorPropagate(localError, error);
    return hashes;
}

static bool _SecRevocationDbSetHashes(SecRevocationDbConnectionRef dbc, CFArrayRef hashes, CFErrorRef *error) {
    /* flatten and store db_hash array in the admin table */
    __block CFErrorRef localError = NULL;
    __block bool ok = (dbc && hashes);

    ok = ok && SecDbWithSQL(dbc->dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertHashes) {
        CFIndex count = CFArrayGetCount(hashes);
        CFIndex hashLen = CC_SHA256_DIGEST_LENGTH, dataLen = hashLen * count;
        uint8_t *dataPtr = (uint8_t *)calloc(dataLen, 1);
        uint8_t *p = dataPtr;
        for (CFIndex idx = 0; idx < count && p; idx++) {
            CFDataRef hash = CFArrayGetValueAtIndex(hashes, idx);
            uint8_t *h = (hash) ? (uint8_t *)CFDataGetBytePtr(hash) : NULL;
            if (h && CFDataGetLength(hash) == hashLen) { memcpy(p, h, hashLen); }
            p += hashLen;
        }
        const char *hashKey = "db_hash";
        ok = ok && SecDbBindText(insertHashes, 1, hashKey, strlen(hashKey),
                                 SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbBindInt64(insertHashes, 2,
                                  (sqlite3_int64)0, &localError);
        ok = ok && SecDbBindBlob(insertHashes, 3,
                                 dataPtr, dataLen,
                                 SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbStep(dbc->dbconn, insertHashes, &localError, NULL);
        free(dataPtr);
        return ok;
    });
    if (!ok || localError) {
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static int64_t _SecRevocationDbGetVersion(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    /* look up version entry in admin table; returns -1 on error */
    __block int64_t version = -1;
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;

    ok = ok && SecDbWithSQL(dbc->dbconn, selectVersionSQL, &localError, ^bool(sqlite3_stmt *selectVersion) {
        ok = ok && SecDbStep(dbc->dbconn, selectVersion, &localError, ^void(bool *stop) {
            version = sqlite3_column_int64(selectVersion, 0);
            *stop = true;
        });
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbGetVersion failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return version;
}

static bool _SecRevocationDbSetVersion(SecRevocationDbConnectionRef dbc, CFIndex version, CFErrorRef *error) {
    secdebug("validupdate", "setting version to %ld", (long)version);

    __block CFErrorRef localError = NULL;
    __block bool ok = (dbc != NULL);
    ok = ok && SecDbWithSQL(dbc->dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertVersion) {
        const char *versionKey = "version";
        ok = ok && SecDbBindText(insertVersion, 1, versionKey, strlen(versionKey),
                            SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbBindInt64(insertVersion, 2,
                             (sqlite3_int64)version, &localError);
        ok = ok && SecDbStep(dbc->dbconn, insertVersion, &localError, NULL);
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbSetVersion failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static int64_t _SecRevocationDbReadSchemaVersionFromDb(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    /* look up db_version entry in admin table; returns -1 on error */
    __block int64_t db_version = -1;
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;

    ok = ok && SecDbWithSQL(dbc->dbconn, selectDbVersionSQL, &localError, ^bool(sqlite3_stmt *selectDbVersion) {
        ok = ok && SecDbStep(dbc->dbconn, selectDbVersion, &localError, ^void(bool *stop) {
            db_version = sqlite3_column_int64(selectDbVersion, 0);
            *stop = true;
        });
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbReadSchemaVersionFromDb failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return db_version;
}

static _Atomic int64_t gSchemaVersion = -1;
static int64_t _SecRevocationDbGetSchemaVersion(SecRevocationDbRef rdb, SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (dbc) {
            atomic_init(&gSchemaVersion, _SecRevocationDbReadSchemaVersionFromDb(dbc, error));
        } else {
            (void) SecRevocationDbPerformRead(rdb, error, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
                atomic_init(&gSchemaVersion, _SecRevocationDbReadSchemaVersionFromDb(dbc, blockError));
                return true;
            });
        }
    });
    if (atomic_load(&gSchemaVersion) == -1) {
        /* Initial read(s) failed. Try to read the schema version again. */
        if (dbc) {
            atomic_store(&gSchemaVersion, _SecRevocationDbReadSchemaVersionFromDb(dbc, error));
        } else {
            (void) SecRevocationDbPerformRead(rdb, error, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
                atomic_store(&gSchemaVersion, _SecRevocationDbReadSchemaVersionFromDb(dbc, blockError));
                return true;
            });
        }
    }
    return atomic_load(&gSchemaVersion);
}

static void SecRevocationDbResetCaches(void) {
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        db->unsupportedVersion = false;
        db->changed = false;
        (void) SecRevocationDbPerformRead(db, NULL, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            atomic_store(&gSchemaVersion, _SecRevocationDbReadSchemaVersionFromDb(dbc, blockError));
            return true;
        });
        SecRevocationDbCachePurge(db);
    });
}

static bool _SecRevocationDbSetSchemaVersion(SecRevocationDbConnectionRef dbc, CFIndex dbversion, CFErrorRef *error) {
    if (dbversion > 0) {
        int64_t db_version = (dbc) ? dbc->precommitDbVersion : -1;
        if (db_version >= dbversion) {
            return true; /* requested schema is earlier than current schema */
        }
    }
    secdebug("validupdate", "setting db_version to %ld", (long)dbversion);

    __block CFErrorRef localError = NULL;
    __block bool ok = (dbc != NULL);
    ok = ok && SecDbWithSQL(dbc->dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertDbVersion) {
        const char *dbVersionKey = "db_version";
        ok = ok && SecDbBindText(insertDbVersion, 1, dbVersionKey, strlen(dbVersionKey),
                            SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbBindInt64(insertDbVersion, 2,
                             (sqlite3_int64)dbversion, &localError);
        ok = ok && SecDbStep(dbc->dbconn, insertDbVersion, &localError, NULL);
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbSetSchemaVersion failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    } else {
        dbc->db->changed = true; /* will notify clients of this change */
        dbc->db->unsupportedVersion = false;
        dbc->precommitDbVersion = dbversion;
        atomic_store(&gSchemaVersion, (int64_t)dbversion);
    }
    CFReleaseSafe(localError);
    return ok;
}

static bool _SecRevocationDbUpdateSchema(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    __block CFErrorRef localError = NULL;
    __block bool ok = (dbc != NULL);
    __block int64_t db_version = (dbc) ? dbc->precommitDbVersion : 0;
    if (db_version >= kSecRevocationDbSchemaVersion) {
        return ok; /* schema version already up to date */
    }
    secdebug("validupdate", "updating db schema from v%lld to v%lld",
            (long long)db_version, (long long)kSecRevocationDbSchemaVersion);

    if (ok && db_version < 5) {
        /* apply v5 changes (add dates table and replace trigger) */
        ok &= SecDbWithSQL(dbc->dbconn, updateConstraintsTablesSQL, &localError, ^bool(sqlite3_stmt *updateTables) {
            ok = SecDbStep(dbc->dbconn, updateTables, &localError, NULL);
            return ok;
        });
        ok &= SecDbWithSQL(dbc->dbconn, updateGroupDeleteTriggerSQL, &localError, ^bool(sqlite3_stmt *updateTrigger) {
            ok = SecDbStep(dbc->dbconn, updateTrigger, &localError, NULL);
            return ok;
        });
        secdebug("validupdate", "applied schema update to v5 (%s)", (ok) ? "ok" : "failed!");
    }
    if (ok && db_version < 6) {
        /* apply v6 changes (the SecDb layer will update autovacuum mode if needed, so we don't execute
           any SQL here, but we do want the database to be replaced in case transaction scope problems
           with earlier versions caused missing entries.) */
        secdebug("validupdate", "applied schema update to v6 (%s)", (ok) ? "ok" : "failed!");
        if (db_version > 0) {
            SecValidUpdateForceReplaceDatabase();
        }
    }
    if (ok && db_version < 7) {
        /* apply v7 changes (add policies column in groups table) */
        ok &= SecDbWithSQL(dbc->dbconn, addPoliciesColumnSQL, &localError, ^bool(sqlite3_stmt *addPoliciesColumn) {
            ok = SecDbStep(dbc->dbconn, addPoliciesColumn, &localError, NULL);
            return ok;
        });
        secdebug("validupdate", "applied schema update to v7 (%s)", (ok) ? "ok" : "failed!");
    }

    if (!ok) {
        secerror("_SecRevocationDbUpdateSchema failed: %@", localError);
    } else {
        ok = ok && _SecRevocationDbSetSchemaVersion(dbc, kSecRevocationDbSchemaVersion, &localError);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

bool SecRevocationDbUpdateSchema(SecRevocationDbRef rdb) {
    /* note: this function assumes it is called only by the database owner.
       non-owner (read-only) clients will fail if changes to the db are needed. */
    if (!rdb || !rdb->db) {
        return false;
    }
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    ok &= SecRevocationDbPerformWrite(rdb, &localError, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
        return _SecRevocationDbUpdateSchema(dbc, blockError);
    });
    CFReleaseSafe(localError);
    return ok;
}

static int64_t _SecRevocationDbGetUpdateFormat(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    /* look up db_format entry in admin table; returns -1 on error */
    __block int64_t db_format = -1;
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;

    ok = ok && SecDbWithSQL(dbc->dbconn, selectDbFormatSQL, &localError, ^bool(sqlite3_stmt *selectDbFormat) {
        ok &= SecDbStep(dbc->dbconn, selectDbFormat, &localError, ^void(bool *stop) {
            db_format = sqlite3_column_int64(selectDbFormat, 0);
            *stop = true;
        });
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbGetUpdateFormat failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return db_format;
}

static bool _SecRevocationDbSetUpdateFormat(SecRevocationDbConnectionRef dbc, CFIndex dbformat, CFErrorRef *error) {
    secdebug("validupdate", "setting db_format to %ld", (long)dbformat);

    __block CFErrorRef localError = NULL;
    __block bool ok = (dbc != NULL);
    ok = ok && SecDbWithSQL(dbc->dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertDbFormat) {
        const char *dbFormatKey = "db_format";
        ok = ok && SecDbBindText(insertDbFormat, 1, dbFormatKey, strlen(dbFormatKey),
                            SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbBindInt64(insertDbFormat, 2,
                             (sqlite3_int64)dbformat, &localError);
        ok = ok && SecDbStep(dbc->dbconn, insertDbFormat, &localError, NULL);
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbSetUpdateFormat failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    } else {
        dbc->db->changed = true; /* will notify clients of this change */
        dbc->db->unsupportedVersion = false;
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static CFStringRef _SecRevocationDbCopyUpdateSource(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    /* look up db_source entry in admin table; returns NULL on error */
    __block CFStringRef updateSource = NULL;
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;

    ok = ok && SecDbWithSQL(dbc->dbconn, selectDbSourceSQL, &localError, ^bool(sqlite3_stmt *selectDbSource) {
        ok &= SecDbStep(dbc->dbconn, selectDbSource, &localError, ^void(bool *stop) {
            const UInt8 *p = (const UInt8 *)sqlite3_column_blob(selectDbSource, 0);
            if (p != NULL) {
                CFIndex length = (CFIndex)sqlite3_column_bytes(selectDbSource, 0);
                if (length > 0) {
                    updateSource = CFStringCreateWithBytes(kCFAllocatorDefault, p, length, kCFStringEncodingUTF8, false);
                }
            }
            *stop = true;
        });
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbCopyUpdateSource failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return updateSource;
}

bool _SecRevocationDbSetUpdateSource(SecRevocationDbConnectionRef dbc, CFStringRef updateSource, CFErrorRef *error) {
    if (!updateSource) {
        secerror("_SecRevocationDbSetUpdateSource failed: %d", errSecParam);
        return false;
    }
    __block char buffer[256];
    __block const char *updateSourceCStr = CFStringGetCStringPtr(updateSource, kCFStringEncodingUTF8);
    if (!updateSourceCStr) {
        if (CFStringGetCString(updateSource, buffer, 256, kCFStringEncodingUTF8)) {
            updateSourceCStr = buffer;
        }
    }
    if (!updateSourceCStr) {
        secerror("_SecRevocationDbSetUpdateSource failed: unable to get UTF-8 encoding");
        return false;
    }
    secdebug("validupdate", "setting update source to \"%s\"", updateSourceCStr);

    __block CFErrorRef localError = NULL;
    __block bool ok = (dbc != NULL);
    ok = ok && SecDbWithSQL(dbc->dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertRecord) {
        const char *dbSourceKey = "db_source";
        ok = ok && SecDbBindText(insertRecord, 1, dbSourceKey, strlen(dbSourceKey),
                           SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbBindInt64(insertRecord, 2,
                            (sqlite3_int64)0, &localError);
        ok = ok && SecDbBindBlob(insertRecord, 3,
                           updateSourceCStr, strlen(updateSourceCStr),
                           SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbStep(dbc->dbconn, insertRecord, &localError, NULL);
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbSetUpdateSource failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    CFReleaseSafe(localError);
    return ok;
}

bool SecRevocationDbSetUpdateSource(SecRevocationDbRef rdb, CFStringRef updateSource) {
    /* note: this function assumes it is called only by the database owner.
       non-owner (read-only) clients will fail if changes to the db are needed. */
    if (!rdb || !rdb->db) {
        return false;
    }
    CFErrorRef localError = NULL;
    bool ok = true;
    ok &= SecRevocationDbPerformWrite(rdb, &localError, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
        return _SecRevocationDbSetUpdateSource(dbc, updateSource, error);
    });
    CFReleaseSafe(localError);
    return ok;
}

static CFAbsoluteTime _SecRevocationDbGetNextUpdateTime(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    /* look up check_again entry in admin table; returns 0 on error */
    __block CFAbsoluteTime nextUpdate = 0;
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;

    ok = ok && SecDbWithSQL(dbc->dbconn, selectNextUpdateSQL, &localError, ^bool(sqlite3_stmt *selectNextUpdate) {
        ok &= SecDbStep(dbc->dbconn, selectNextUpdate, &localError, ^void(bool *stop) {
            CFAbsoluteTime *p = (CFAbsoluteTime *)sqlite3_column_blob(selectNextUpdate, 0);
            if (p != NULL) {
                if (sizeof(CFAbsoluteTime) == sqlite3_column_bytes(selectNextUpdate, 0)) {
                    nextUpdate = *p;
                }
            }
            *stop = true;
        });
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbGetNextUpdateTime failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return nextUpdate;
}

static bool _SecRevocationDbSetNextUpdateTime(SecRevocationDbConnectionRef dbc, CFAbsoluteTime nextUpdate, CFErrorRef *error){
    secdebug("validupdate", "setting next update to %f", (double)nextUpdate);

    __block CFErrorRef localError = NULL;
    __block bool ok = (dbc != NULL);
    ok = ok && SecDbWithSQL(dbc->dbconn, insertAdminRecordSQL, &localError, ^bool(sqlite3_stmt *insertRecord) {
        const char *nextUpdateKey = "check_again";
        ok = ok && SecDbBindText(insertRecord, 1, nextUpdateKey, strlen(nextUpdateKey),
                            SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbBindInt64(insertRecord, 2,
                             (sqlite3_int64)0, &localError);
        ok = ok && SecDbBindBlob(insertRecord, 3,
                            &nextUpdate, sizeof(CFAbsoluteTime),
                            SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbStep(dbc->dbconn, insertRecord, &localError, NULL);
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbSetNextUpdate failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

bool _SecRevocationDbRemoveAllEntries(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    /* clear out the contents of the database and start fresh */
    bool ok = (dbc != NULL);
    CFErrorRef localError = NULL;

    /* _SecRevocationDbUpdateSchema was called when db was opened, so no need to do it again. */

    /* delete all entries */
    ok = ok && SecDbExec(dbc->dbconn, deleteAllEntriesSQL, &localError);
    secnotice("validupdate", "resetting database, result: %d (expected 1)", (ok) ? 1 : 0);

    /* one more thing: update the schema version and format to current */
    ok = ok && _SecRevocationDbSetSchemaVersion(dbc, kSecRevocationDbSchemaVersion, &localError);
    ok = ok && _SecRevocationDbSetUpdateFormat(dbc, kSecRevocationDbUpdateFormat, &localError);

    if (!ok || localError) {
        secerror("_SecRevocationDbRemoveAllEntries failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static bool _SecRevocationDbUpdateIssuers(SecRevocationDbConnectionRef dbc, int64_t groupId, CFArrayRef issuers, CFErrorRef *error) {
    /* insert or replace issuer records in issuers table */
    if (!issuers || groupId < 0) {
        return false; /* must have something to insert, and a group to associate with it */
    }
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;
    if (isArray(issuers)) {
        CFIndex issuerIX, issuerCount = CFArrayGetCount(issuers);
        for (issuerIX=0; issuerIX<issuerCount && ok; issuerIX++) {
            CFDataRef hash = (CFDataRef)CFArrayGetValueAtIndex(issuers, issuerIX);
            if (!hash) { continue; }
            ok = ok && SecDbWithSQL(dbc->dbconn, insertIssuerRecordSQL, &localError, ^bool(sqlite3_stmt *insertIssuer) {
                ok = ok && SecDbBindInt64(insertIssuer, 1,
                                     groupId, &localError);
                ok = ok && SecDbBindBlob(insertIssuer, 2,
                                    CFDataGetBytePtr(hash),
                                    CFDataGetLength(hash),
                                    SQLITE_TRANSIENT, &localError);
                /* Execute the insert statement for this issuer record. */
                ok = ok && SecDbStep(dbc->dbconn, insertIssuer, &localError, NULL);
                return ok;
            });
        }
    }
    if (!ok || localError) {
        secerror("_SecRevocationDbUpdateIssuers failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static SecValidInfoFormat _SecRevocationDbGetGroupFormatForData(SecRevocationDbConnectionRef dbc, int64_t groupId, CFDataRef data) {
    /* determine existing format if groupId is supplied,
       otherwise return the expected format for the given data. */
    SecValidInfoFormat format = kSecValidInfoFormatUnknown;
    if (groupId >= 0) {
        format = _SecRevocationDbGetGroupFormat(dbc, groupId, NULL, NULL, NULL, NULL);
    }
    if (format == kSecValidInfoFormatUnknown && data != NULL) {
        /* group doesn't exist, so determine format based on length of specified data.
           len <= 20 is a serial number (actually, <=37, but != 32.)
           len==32 is a sha256 hash. otherwise: nto1. */
        CFIndex length = CFDataGetLength(data);
        if (length == 32) {
            format = kSecValidInfoFormatSHA256;
        } else if (length <= 37) {
            format = kSecValidInfoFormatSerial;
        } else if (length > 0) {
            format = kSecValidInfoFormatNto1;
        }
    }
    return format;
}

static bool _SecRevocationDbUpdateIssuerData(SecRevocationDbConnectionRef dbc, int64_t groupId, CFDictionaryRef dict, CFErrorRef *error) {
    /* update/delete records in serials or hashes table. */
    if (!dict || groupId < 0) {
        return false; /* must have something to insert, and a group to associate with it */
    }
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;
    /* process deletions */
    CFArrayRef deleteArray = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("delete"));
    if (isArray(deleteArray)) {
        SecValidInfoFormat format = kSecValidInfoFormatUnknown;
        CFIndex processed=0, identifierIX, identifierCount = CFArrayGetCount(deleteArray);
        for (identifierIX=0; identifierIX<identifierCount; identifierIX++) {
            CFDataRef identifierData = (CFDataRef)CFArrayGetValueAtIndex(deleteArray, identifierIX);
            if (!identifierData) { continue; }
            if (format == kSecValidInfoFormatUnknown) {
                format = _SecRevocationDbGetGroupFormatForData(dbc, groupId, identifierData);
            }
            CFStringRef sql = NULL;
            if (format == kSecValidInfoFormatSerial) {
                sql = deleteSerialRecordSQL;
            } else if (format == kSecValidInfoFormatSHA256) {
                sql = deleteSha256RecordSQL;
            }
            if (!sql) { continue; }

            ok = ok && SecDbWithSQL(dbc->dbconn, sql, &localError, ^bool(sqlite3_stmt *deleteIdentifier) {
                /* (groupid,serial|sha256) */
                CFDataRef hexData = cfToHexData(identifierData, true);
                if (!hexData) { return false; }
                ok = ok && SecDbBindInt64(deleteIdentifier, 1,
                                     groupId, &localError);
                ok = ok && SecDbBindBlob(deleteIdentifier, 2,
                                    CFDataGetBytePtr(hexData),
                                    CFDataGetLength(hexData),
                                    SQLITE_TRANSIENT, &localError);
                /* Execute the delete statement for the identifier record. */
                ok = ok && SecDbStep(dbc->dbconn, deleteIdentifier, &localError, NULL);
                CFReleaseSafe(hexData);
                return ok;
            });
            if (ok) { ++processed; }
        }
#if VERBOSE_LOGGING
        secdebug("validupdate", "Processed %ld of %ld deletions for group %lld, result=%s",
                 processed, identifierCount, groupId, (ok) ? "true" : "false");
#endif
    }
    /* process additions */
    CFArrayRef addArray = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("add"));
    if (isArray(addArray)) {
        SecValidInfoFormat format = kSecValidInfoFormatUnknown;
        CFIndex processed=0, identifierIX, identifierCount = CFArrayGetCount(addArray);
        for (identifierIX=0; identifierIX<identifierCount; identifierIX++) {
            CFDataRef identifierData = (CFDataRef)CFArrayGetValueAtIndex(addArray, identifierIX);
            if (!identifierData) { continue; }
            if (format == kSecValidInfoFormatUnknown) {
                format = _SecRevocationDbGetGroupFormatForData(dbc, groupId, identifierData);
            }
            CFStringRef sql = NULL;
            if (format == kSecValidInfoFormatSerial) {
                sql = insertSerialRecordSQL;
            } else if (format == kSecValidInfoFormatSHA256) {
                sql = insertSha256RecordSQL;
            }
            if (!sql) { continue; }

            ok = ok && SecDbWithSQL(dbc->dbconn, sql, &localError, ^bool(sqlite3_stmt *insertIdentifier) {
                /* rowid,(groupid,serial|sha256) */
                /* rowid is autoincremented and we never set it directly */
                ok = ok && SecDbBindInt64(insertIdentifier, 1,
                                     groupId, &localError);
                ok = ok && SecDbBindBlob(insertIdentifier, 2,
                                    CFDataGetBytePtr(identifierData),
                                    CFDataGetLength(identifierData),
                                    SQLITE_TRANSIENT, &localError);
                /* Execute the insert statement for the identifier record. */
                ok = ok && SecDbStep(dbc->dbconn, insertIdentifier, &localError, NULL);
                return ok;
            });
            if (ok) { ++processed; }
        }
#if VERBOSE_LOGGING
        secdebug("validupdate", "Processed %ld of %ld additions for group %lld, result=%s",
                 processed, identifierCount, groupId, (ok) ? "true" : "false");
#endif
    }
    if (!ok || localError) {
        secerror("_SecRevocationDbUpdatePerIssuerData failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static bool _SecRevocationDbCopyDateConstraints(SecRevocationDbConnectionRef dbc,
    int64_t groupId, CFDateRef *notBeforeDate, CFDateRef *notAfterDate, CFErrorRef *error) {
    /* return true if one or both date constraints exist for a given groupId.
       the actual constraints are optionally returned in output CFDateRef parameters.
       caller is responsible for releasing date and error parameters, if provided.
    */
    __block bool ok = (dbc != NULL);
    __block CFDateRef localNotBefore = NULL;
    __block CFDateRef localNotAfter = NULL;
    __block CFErrorRef localError = NULL;

    ok = ok && SecDbWithSQL(dbc->dbconn, selectDateRecordSQL, &localError, ^bool(sqlite3_stmt *selectDates) {
        /* (groupid,notbefore,notafter) */
        ok &= SecDbBindInt64(selectDates, 1, groupId, &localError);
        ok = ok && SecDbStep(dbc->dbconn, selectDates, &localError, ^(bool *stop) {
            /* if column has no value, its type will be SQLITE_NULL */
            if (SQLITE_NULL != sqlite3_column_type(selectDates, 0)) {
                CFAbsoluteTime nb = (CFAbsoluteTime)sqlite3_column_double(selectDates, 0);
                localNotBefore = CFDateCreate(NULL, nb);
            }
            if (SQLITE_NULL != sqlite3_column_type(selectDates, 1)) {
                CFAbsoluteTime na = (CFAbsoluteTime)sqlite3_column_double(selectDates, 1);
                localNotAfter = CFDateCreate(NULL, na);
            }
        });
        return ok;
    });
    /* must have at least one date constraint to return true.
       since date constraints are optional, not finding any should not log an error. */
    ok = ok && !localError && (localNotBefore != NULL || localNotAfter != NULL);
    if (localError) {
        secerror("_SecRevocationDbCopyDateConstraints failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    if (!ok) {
        CFReleaseNull(localNotBefore);
        CFReleaseNull(localNotAfter);
    }
    if (notBeforeDate) {
        *notBeforeDate = localNotBefore;
    } else {
        CFReleaseSafe(localNotBefore);
    }
    if (notAfterDate) {
        *notAfterDate = localNotAfter;
    } else {
        CFReleaseSafe(localNotAfter);
    }

    (void) CFErrorPropagate(localError, error);
    return ok;
}

static bool _SecRevocationDbUpdateDateConstraints(SecRevocationDbConnectionRef dbc, int64_t groupId, CFDictionaryRef dict, CFErrorRef *error) {
    /* Called only from _SecRevocationDbUpdateIssuerConstraints.
       Function assumes that the caller has checked the input arguments.
    */
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    __block CFAbsoluteTime notBefore = -3155760000.0; /* default: 1901-01-01 00:00:00-0000 */
    __block CFAbsoluteTime notAfter = 31556908800.0;  /* default: 3001-01-01 00:00:00-0000 */

    CFDateRef notBeforeDate = (CFDateRef)CFDictionaryGetValue(dict, CFSTR("not-before"));
    CFDateRef notAfterDate = (CFDateRef)CFDictionaryGetValue(dict, CFSTR("not-after"));

    if (isDate(notBeforeDate)) {
        notBefore = CFDateGetAbsoluteTime(notBeforeDate);
    } else {
        notBeforeDate = NULL;
    }
    if (isDate(notAfterDate)) {
        notAfter = CFDateGetAbsoluteTime(notAfterDate);
    } else {
        notAfterDate = NULL;
    }
    if (!(notBeforeDate || notAfterDate)) {
        return ok; /* no dates supplied, so we have nothing to update for this issuer */
    }

    if (!(notBeforeDate && notAfterDate)) {
        /* only one date was supplied, so check for existing date constraints */
        CFDateRef curNotBeforeDate = NULL;
        CFDateRef curNotAfterDate = NULL;
        if (_SecRevocationDbCopyDateConstraints(dbc, groupId, &curNotBeforeDate,
                                                       &curNotAfterDate, &localError)) {
            if (!notBeforeDate) {
                notBeforeDate = curNotBeforeDate;
                notBefore = CFDateGetAbsoluteTime(notBeforeDate);
            } else {
                CFReleaseSafe(curNotBeforeDate);
            }
            if (!notAfterDate) {
                notAfterDate = curNotAfterDate;
                notAfter = CFDateGetAbsoluteTime(notAfterDate);
            } else {
                CFReleaseSafe(curNotAfterDate);
            }
        }
    }
    ok = ok && SecDbWithSQL(dbc->dbconn, insertDateRecordSQL, &localError, ^bool(sqlite3_stmt *insertDate) {
        /* (groupid,notbefore,notafter) */
        ok = ok && SecDbBindInt64(insertDate, 1, groupId, &localError);
        ok = ok && SecDbBindDouble(insertDate, 2, notBefore, &localError);
        ok = ok && SecDbBindDouble(insertDate, 3, notAfter, &localError);
        ok = ok && SecDbStep(dbc->dbconn, insertDate, &localError, NULL);
        return ok;
    });

    if (!ok || localError) {
        secinfo("validupdate", "_SecRevocationDbUpdateDateConstraints failed (ok=%s, localError=%@)",
                (ok) ? "1" : "0", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static bool _SecRevocationDbUpdatePolicyConstraints(SecRevocationDbConnectionRef dbc, int64_t groupId, CFDictionaryRef dict, CFErrorRef *error) {
    /* Called only from _SecRevocationDbUpdateIssuerConstraints.
       Function assumes that the caller has checked the input arguments.
    */
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    CFArrayRef policies = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("policies"));
    if (!isArray(policies)) {
        return ok; /* no policies supplied, so nothing to update for this issuer */
    }

    __block CFDataRef data = createPoliciesData(policies);
    ok = data && SecDbWithSQL(dbc->dbconn, updateGroupPoliciesSQL, &localError, ^bool(sqlite3_stmt *updatePolicies) {
        /* (policies,groupid) */
        ok = ok && SecDbBindBlob(updatePolicies, 1,
                                 CFDataGetBytePtr(data),
                                 CFDataGetLength(data),
                                 SQLITE_TRANSIENT, &localError);
        ok = ok && SecDbBindInt64(updatePolicies, 2, groupId, &localError);
        ok = ok && SecDbStep(dbc->dbconn, updatePolicies, &localError, NULL);
        return ok;
    });
    CFReleaseSafe(data);

    if (!ok || localError) {
        secinfo("validupdate", "_SecRevocationDbUpdatePolicyConstraints failed (ok=%s, localError=%@)",
                (ok) ? "1" : "0", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static bool _SecRevocationDbUpdateNameConstraints(SecRevocationDbConnectionRef dbc, int64_t groupId, CFDictionaryRef dict, CFErrorRef *error) {
    /* Called only from _SecRevocationDbUpdateIssuerConstraints.
       Function assumes that the caller has checked the input arguments.
    */

    /* %%% (TBI:9254570) update name constraint entries here */
    return true;
}

static bool _SecRevocationDbUpdateIssuerConstraints(SecRevocationDbConnectionRef dbc, int64_t groupId, CFDictionaryRef dict, CFErrorRef *error) {
    /* check input arguments */
    if (!dbc || !dict || groupId < 0) {
        return false;
    }
    bool ok = true;
    ok = ok && _SecRevocationDbUpdateDateConstraints(dbc, groupId, dict, error);
    ok = ok && _SecRevocationDbUpdateNameConstraints(dbc, groupId, dict, error);
    ok = ok && _SecRevocationDbUpdatePolicyConstraints(dbc, groupId, dict, error);
    return ok;
}

static SecValidInfoFormat _SecRevocationDbGetGroupFormat(SecRevocationDbConnectionRef dbc,
    int64_t groupId, SecValidInfoFlags *flags, CFDataRef *data, CFDataRef *policies, CFErrorRef *error) {
    /* return group record fields for a given groupId.
       on success, returns a non-zero format type, and other field values in optional output parameters.
       caller is responsible for releasing data, policies, and error parameters, if provided.
    */
    __block bool ok = (dbc != NULL);
    __block SecValidInfoFormat format = 0;
    __block CFErrorRef localError = NULL;

    /* Select the group record to determine flags and format. */
    ok = ok && SecDbWithSQL(dbc->dbconn, selectGroupRecordSQL, &localError, ^bool(sqlite3_stmt *selectGroup) {
        ok = ok && SecDbBindInt64(selectGroup, 1, groupId, &localError);
        ok = ok && SecDbStep(dbc->dbconn, selectGroup, &localError, ^(bool *stop) {
            if (flags) {
                *flags = (SecValidInfoFlags)sqlite3_column_int(selectGroup, 0);
            }
            format = (SecValidInfoFormat)sqlite3_column_int(selectGroup, 1);
            if (data) {
                //%%% stream the data from the db into a streamed decompression <rdar://32142637>
                uint8_t *p = (uint8_t *)sqlite3_column_blob(selectGroup, 2);
                if (p != NULL && format == kSecValidInfoFormatNto1) {
                    CFIndex length = (CFIndex)sqlite3_column_bytes(selectGroup, 2);
                    *data = CFDataCreate(kCFAllocatorDefault, p, length);
                }
            }
            if (policies) {
                uint8_t *p = (uint8_t *)sqlite3_column_blob(selectGroup, 3);
                if (p != NULL) {
                    CFIndex length = (CFIndex)sqlite3_column_bytes(selectGroup, 3);
                    *policies = CFDataCreate(kCFAllocatorDefault, p, length);
                }
            }
        });
        return ok;
    });
    if (!ok || localError) {
        secdebug("validupdate", "GetGroupFormat for groupId %lu failed", (unsigned long)groupId);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
        format = kSecValidInfoFormatUnknown;
    }
    (void) CFErrorPropagate(localError, error);
    if (!(format > kSecValidInfoFormatUnknown)) {
        secdebug("validupdate", "GetGroupFormat: got format %d for groupId %lld", format, (long long)groupId);
    }
    return format;
}

static bool _SecRevocationDbUpdateFlags(CFDictionaryRef dict, CFStringRef key, SecValidInfoFlags mask, SecValidInfoFlags *flags) {
    /* If a boolean value exists in the given dictionary for the given key,
       or an explicit "1" or "0" is specified as the key string,
       set or clear the corresponding bit(s) defined by the mask argument.
       Function returns true if the flags value was changed, false otherwise.
    */
    if (!isDictionary(dict) || !isString(key) || !flags) {
        return false;
    }
    bool hasValue = false, newValue = false, result = false;
    CFTypeRef value = (CFBooleanRef)CFDictionaryGetValue(dict, key);
    if (isBoolean(value)) {
        newValue = CFBooleanGetValue((CFBooleanRef)value);
        hasValue = true;
    } else if (BOOL_STRING_KEY_LENGTH == CFStringGetLength(key)) {
        if (CFStringCompare(key, kBoolTrueKey, 0) == kCFCompareEqualTo) {
            hasValue = newValue = true;
        } else if (CFStringCompare(key, kBoolFalseKey, 0) == kCFCompareEqualTo) {
            hasValue = true;
        }
    }
    if (hasValue) {
        SecValidInfoFlags oldFlags = *flags;
        if (newValue) {
            *flags |= mask;
        } else {
            *flags &= ~(mask);
        }
        result = (*flags != oldFlags);
    }
    return result;
}

static bool _SecRevocationDbUpdateFilter(CFDictionaryRef dict, CFDataRef oldData, CFDataRef * __nonnull CF_RETURNS_RETAINED xmlData) {
    /* If xor and/or params values exist in the given dictionary, create a new
       property list containing the updated values, and return as a flattened
       data blob in the xmlData output parameter (note: caller must release.)
       Function returns true if there is new xmlData to save, false otherwise.
    */
    bool result = false;
    bool xorProvided = false;
    bool paramsProvided = false;
    bool missingData = false;

    if (!dict || !xmlData) {
        return result; /* no-op if no dictionary is provided, or no way to update the data */
    }
    *xmlData = NULL;
    CFDataRef xorCurrent = NULL;
    CFDataRef xorUpdate = (CFDataRef)CFDictionaryGetValue(dict, CFSTR("xor"));
    if (isData(xorUpdate)) {
        xorProvided = true;
    }
    CFArrayRef paramsCurrent = NULL;
    CFArrayRef paramsUpdate = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("params"));
    if (isArray(paramsUpdate)) {
        paramsProvided = true;
    }
    if (!(xorProvided || paramsProvided)) {
        return result; /* nothing to update, so we can bail out here. */
    }

    CFPropertyListRef nto1Current = NULL;
    CFMutableDictionaryRef nto1Update = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                  &kCFTypeDictionaryKeyCallBacks,
                                                                  &kCFTypeDictionaryValueCallBacks);
    if (!nto1Update) {
        return result;
    }

    /* turn old data into property list */
    CFDataRef data = (CFDataRef)CFRetainSafe(oldData);
    CFDataRef inflatedData = copyInflatedData(data);
    if (inflatedData) {
        CFReleaseSafe(data);
        data = inflatedData;
    }
    if (data) {
        nto1Current = CFPropertyListCreateWithData(kCFAllocatorDefault, data, 0, NULL, NULL);
        CFReleaseSafe(data);
    }
    if (nto1Current) {
        xorCurrent = (CFDataRef)CFDictionaryGetValue((CFDictionaryRef)nto1Current, CFSTR("xor"));
        paramsCurrent = (CFArrayRef)CFDictionaryGetValue((CFDictionaryRef)nto1Current, CFSTR("params"));
    }

    /* set current or updated xor data in new property list */
    if (xorProvided) {
        CFDataRef xorNew = NULL;
        if (xorCurrent) {
            CFIndex xorUpdateLen = CFDataGetLength(xorUpdate);
            CFMutableDataRef xor = CFDataCreateMutableCopy(NULL, 0, xorCurrent);
            if (xor && xorUpdateLen > 0) {
                /* truncate or zero-extend data to match update size */
                CFDataSetLength(xor, xorUpdateLen);
                /* exclusive-or update bytes over the existing data */
                UInt8 *xorP = (UInt8 *)CFDataGetMutableBytePtr(xor);
                UInt8 *updP = (UInt8 *)CFDataGetBytePtr(xorUpdate);
                if (xorP && updP) {
                    for (int idx = 0; idx < xorUpdateLen; idx++) {
                        xorP[idx] = xorP[idx] ^ updP[idx];
                    }
                }
            }
            xorNew = (CFDataRef)xor;
        } else {
            xorNew = (CFDataRef)CFRetainSafe(xorUpdate);
        }
        if (xorNew) {
            CFDictionaryAddValue(nto1Update, CFSTR("xor"), xorNew);
            CFReleaseSafe(xorNew);
        } else {
            secdebug("validupdate", "Failed to get updated filter data");
            missingData = true;
        }
    } else if (xorCurrent) {
        /* not provided, so use existing xor value */
        CFDictionaryAddValue(nto1Update, CFSTR("xor"), xorCurrent);
    } else {
        secdebug("validupdate", "Failed to get current filter data");
        missingData = true;
    }

    /* set current or updated params in new property list */
    if (paramsProvided) {
        CFDictionaryAddValue(nto1Update, CFSTR("params"), paramsUpdate);
    } else if (paramsCurrent) {
        /* not provided, so use existing params value */
        CFDictionaryAddValue(nto1Update, CFSTR("params"), paramsCurrent);
    } else {
        /* missing params: neither provided nor existing */
        secdebug("validupdate", "Failed to get current filter params");
        missingData = true;
    }

    CFReleaseSafe(nto1Current);
    if (!missingData) {
        *xmlData = CFPropertyListCreateData(kCFAllocatorDefault, nto1Update,
                                            kCFPropertyListXMLFormat_v1_0,
                                            0, NULL);
        result = (*xmlData != NULL);
    }
    CFReleaseSafe(nto1Update);

    /* compress the xmlData blob, if possible */
    if (result) {
        CFDataRef deflatedData = copyDeflatedData(*xmlData);
        if (deflatedData) {
            if (CFDataGetLength(deflatedData) < CFDataGetLength(*xmlData)) {
                CFRelease(*xmlData);
                *xmlData = deflatedData;
            } else {
                CFRelease(deflatedData);
            }
        }
    }
    return result;
}


static int64_t _SecRevocationDbUpdateGroup(SecRevocationDbConnectionRef dbc, int64_t groupId, CFDictionaryRef dict, CFErrorRef *error) {
    /* insert group record for a given groupId.
       if the specified groupId is < 0, a new group entry is created.
       returns the groupId on success, or -1 on failure.
     */
    if (!dict) {
        return groupId; /* no-op if no dictionary is provided */
    }

    __block int64_t result = -1;
    __block bool ok = (dbc != NULL);
    __block bool isFormatChange = false;
    __block CFErrorRef localError = NULL;

    __block SecValidInfoFlags flags = 0;
    __block SecValidInfoFormat format = kSecValidInfoFormatUnknown;
    __block SecValidInfoFormat formatUpdate = kSecValidInfoFormatUnknown;
    __block CFDataRef data = NULL;
    __block CFDataRef policies = NULL;

    if (groupId >= 0) {
        /* fetch the flags and data for an existing group record, in case some are being changed. */
        if (ok) {
            format = _SecRevocationDbGetGroupFormat(dbc, groupId, &flags, &data, &policies, NULL);
        }
        if (format == kSecValidInfoFormatUnknown) {
            secdebug("validupdate", "existing group %lld has unknown format %d, flags=0x%lx",
                     (long long)groupId, format, flags);
            //%%% clean up by deleting all issuers with this groupId, then the group record,
            // or just force a full update? note: we can get here if we fail to bind the
            // format value in the prepared SQL statement below.
            return -1;
        }
    }
    CFTypeRef value = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("format"));
    if (isString(value)) {
        if (CFStringCompare((CFStringRef)value, CFSTR("serial"), 0) == kCFCompareEqualTo) {
            formatUpdate = kSecValidInfoFormatSerial;
        } else if (CFStringCompare((CFStringRef)value, CFSTR("sha256"), 0) == kCFCompareEqualTo) {
            formatUpdate = kSecValidInfoFormatSHA256;
        } else if (CFStringCompare((CFStringRef)value, CFSTR("nto1"), 0) == kCFCompareEqualTo) {
            formatUpdate = kSecValidInfoFormatNto1;
        }
    }
    /* if format value is explicitly supplied, then this is effectively a new group entry. */
    isFormatChange = (formatUpdate > kSecValidInfoFormatUnknown &&
                      formatUpdate != format &&
                      groupId >= 0);

    if (isFormatChange) {
        secdebug("validupdate", "group %lld format change from %d to %d",
                 (long long)groupId, format, formatUpdate);
        /* format of an existing group is changing; delete the group first.
           this should ensure that all entries referencing the old groupid are deleted.
        */
        ok = ok && SecDbWithSQL(dbc->dbconn, deleteGroupRecordSQL, &localError, ^bool(sqlite3_stmt *deleteResponse) {
            ok = ok && SecDbBindInt64(deleteResponse, 1, groupId, &localError);
            /* Execute the delete statement. */
            ok = ok && SecDbStep(dbc->dbconn, deleteResponse, &localError, NULL);
            return ok;
        });
    }
    ok = ok && SecDbWithSQL(dbc->dbconn, insertGroupRecordSQL, &localError, ^bool(sqlite3_stmt *insertGroup) {
        /* (groupid,flags,format,data,policies) */
        /* groups.groupid */
        if (ok && (!isFormatChange) && (groupId >= 0)) {
            /* bind to existing groupId row if known, otherwise will insert and autoincrement */
            ok = SecDbBindInt64(insertGroup, 1, groupId, &localError);
            if (!ok) {
                secdebug("validupdate", "failed to set groupId %lld", (long long)groupId);
            }
        }
        /* groups.flags */
        if (ok) {
            (void)_SecRevocationDbUpdateFlags(dict, CFSTR("complete"), kSecValidInfoComplete, &flags);
            (void)_SecRevocationDbUpdateFlags(dict, CFSTR("check-ocsp"), kSecValidInfoCheckOCSP, &flags);
            (void)_SecRevocationDbUpdateFlags(dict, CFSTR("known-intermediates-only"), kSecValidInfoKnownOnly, &flags);
            (void)_SecRevocationDbUpdateFlags(dict, CFSTR("require-ct"), kSecValidInfoRequireCT, &flags);
            (void)_SecRevocationDbUpdateFlags(dict, CFSTR("valid"), kSecValidInfoAllowlist, &flags);
            (void)_SecRevocationDbUpdateFlags(dict, CFSTR("no-ca"), kSecValidInfoNoCACheck, &flags);
            (void)_SecRevocationDbUpdateFlags(dict, CFSTR("no-ca-v2"), kSecValidInfoNoCAv2Check, &flags);
            (void)_SecRevocationDbUpdateFlags(dict, CFSTR("overridable"), kSecValidInfoOverridable, &flags);

            /* date constraints exist if either "not-before" or "not-after" keys are found */
            CFTypeRef notBeforeValue = (CFDateRef)CFDictionaryGetValue(dict, CFSTR("not-before"));
            CFTypeRef notAfterValue = (CFDateRef)CFDictionaryGetValue(dict, CFSTR("not-after"));
            if (isDate(notBeforeValue) || isDate(notAfterValue)) {
                (void)_SecRevocationDbUpdateFlags(dict, kBoolTrueKey, kSecValidInfoDateConstraints, &flags);
                /* Note that the spec defines not-before and not-after dates as optional, such that
                   not providing one does not change the database contents. Therefore, we can never clear
                   this flag; either a new date entry will be supplied, or a format change will cause
                   the entire group entry to be deleted. */
            }
            /* policy constraints exist if "policies" key is found */
            CFTypeRef policiesValue = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("policies"));
            if (isArray(policiesValue)) {
                (void)_SecRevocationDbUpdateFlags(dict, kBoolTrueKey, kSecValidInfoPolicyConstraints, &flags);
                /* As above, not providing this value in an update does not change the existing state,
                   so we never need to clear this flag once it is set. */
            }

            /* %%% (TBI:9254570) name constraints don't exist yet */
            (void)_SecRevocationDbUpdateFlags(dict, kBoolFalseKey, kSecValidInfoNameConstraints, &flags);

            ok = SecDbBindInt(insertGroup, 2, (int)flags, &localError);
            if (!ok) {
                secdebug("validupdate", "failed to set flags (%lu) for groupId %lld", flags, (long long)groupId);
            }
        }
        /* groups.format */
        if (ok) {
            SecValidInfoFormat formatValue = format;
            if (formatUpdate > kSecValidInfoFormatUnknown) {
                formatValue = formatUpdate;
            }
            ok = SecDbBindInt(insertGroup, 3, (int)formatValue, &localError);
            if (!ok) {
                secdebug("validupdate", "failed to set format (%d) for groupId %lld", formatValue, (long long)groupId);
            }
        }
        /* groups.data */
        CFDataRef xmlData = NULL;
        if (ok) {
            bool hasFilter = ((formatUpdate == kSecValidInfoFormatNto1) ||
                              (formatUpdate == kSecValidInfoFormatUnknown &&
                               format == kSecValidInfoFormatNto1));
            if (hasFilter) {
                CFDataRef dataValue = data; /* use existing data */
                if (_SecRevocationDbUpdateFilter(dict, data, &xmlData)) {
                    dataValue = xmlData; /* use updated data */
                }
                if (dataValue) {
                    ok = SecDbBindBlob(insertGroup, 4,
                                       CFDataGetBytePtr(dataValue),
                                       CFDataGetLength(dataValue),
                                       SQLITE_TRANSIENT, &localError);
                }
                if (!ok) {
                    secdebug("validupdate", "failed to set data for groupId %lld",
                             (long long)groupId);
                }
            }
            /* else there is no data, so NULL is implicitly bound to column 4 */
        }
        /* groups.policies */
        CFDataRef newPoliciesData = NULL;
        if (ok) {
            CFDataRef policiesValue = policies; /* use existing policies */
            newPoliciesData = createPoliciesData((CFArrayRef)CFDictionaryGetValue(dict, CFSTR("policies")));
            if (newPoliciesData) {
                policiesValue = newPoliciesData; /* use updated policies */
            }
            if (policiesValue) {
                ok = SecDbBindBlob(insertGroup, 5,
                                   CFDataGetBytePtr(policiesValue),
                                   CFDataGetLength(policiesValue),
                                   SQLITE_TRANSIENT, &localError);
            }
            /* else there is no policy data, so NULL is implicitly bound to column 5 */
            if (!ok) {
                secdebug("validupdate", "failed to set policies for groupId %lld",
                         (long long)groupId);
            }
        }

        /* Execute the insert statement for the group record. */
        if (ok) {
            ok = SecDbStep(dbc->dbconn, insertGroup, &localError, NULL);
            if (!ok) {
                secdebug("validupdate", "failed to execute insertGroup statement for groupId %lld",
                         (long long)groupId);
            }
            result = (int64_t)sqlite3_last_insert_rowid(SecDbHandle(dbc->dbconn));
        }
        if (!ok) {
            secdebug("validupdate", "failed to insert group %lld", (long long)result);
        }
        /* Clean up temporary allocations made in this block. */
        CFReleaseSafe(xmlData);
        CFReleaseSafe(newPoliciesData);
        return ok;
    });

    CFReleaseSafe(data);
    CFReleaseSafe(policies);

    if (!ok || localError) {
        secerror("_SecRevocationDbUpdateGroup failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return result;
}

static int64_t _SecRevocationDbGroupIdForIssuerHash(SecRevocationDbConnectionRef dbc, CFDataRef hash, CFErrorRef *error) {
    /* look up issuer hash in issuers table to get groupid, if it exists */
    __block int64_t groupId = -1;
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;

    if (!hash) {
        secdebug("validupdate", "failed to get hash (%@)", hash);
    }
    require(hash && dbc, errOut);

    /* This is the starting point for any lookup; find a group id for the given issuer hash.
       Before we do that, need to verify the current db_version. We cannot use results from a
       database created with a schema version older than the minimum supported version.
       However, we may be able to use results from a newer version. At the next database
       update interval, if the existing schema is old, we'll be removing and recreating
       the database contents with the current schema version.
    */
    int64_t db_version = _SecRevocationDbGetSchemaVersion(dbc->db, dbc, NULL);
    if (db_version < kSecRevocationDbMinSchemaVersion) {
        if (!dbc->db->unsupportedVersion) {
            secdebug("validupdate", "unsupported db_version: %lld", (long long)db_version);
            dbc->db->unsupportedVersion = true; /* only warn once for a given unsupported version */
        }
    }
    require_quiet(db_version >= kSecRevocationDbMinSchemaVersion, errOut);

    /* Look up provided issuer_hash in the issuers table.
    */
    ok = ok && SecDbWithSQL(dbc->dbconn, selectGroupIdSQL, &localError, ^bool(sqlite3_stmt *selectGroupId) {
        ok &= SecDbBindBlob(selectGroupId, 1, CFDataGetBytePtr(hash), CFDataGetLength(hash), SQLITE_TRANSIENT, &localError);
        ok &= SecDbStep(dbc->dbconn, selectGroupId, &localError, ^(bool *stopGroupId) {
            groupId = sqlite3_column_int64(selectGroupId, 0);
        });
        return ok;
    });

errOut:
    if (!ok || localError) {
        secerror("_SecRevocationDbGroupIdForIssuerHash failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return groupId;
}

static bool _SecRevocationDbApplyGroupDelete(SecRevocationDbConnectionRef dbc, CFDataRef issuerHash, CFErrorRef *error) {
    /* delete group associated with the given issuer;
       schema trigger will delete associated issuers, serials, and hashes. */
    __block int64_t groupId = -1;
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;

    if (ok) {
        groupId = _SecRevocationDbGroupIdForIssuerHash(dbc, issuerHash, &localError);
    }
    if (groupId < 0) {
        if (!localError) {
            SecError(errSecParam, &localError, CFSTR("group not found for issuer"));
        }
        ok = false;
    }
    ok = ok && SecDbWithSQL(dbc->dbconn, deleteGroupRecordSQL, &localError, ^bool(sqlite3_stmt *deleteResponse) {
        ok &= SecDbBindInt64(deleteResponse, 1, groupId, &localError);
        /* Execute the delete statement. */
        ok = ok && SecDbStep(dbc->dbconn, deleteResponse, &localError, NULL);
        return ok;
    });
    if (!ok || localError) {
        secerror("_SecRevocationDbApplyGroupDelete failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationWrite, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return ok;
}

static bool _SecRevocationDbApplyGroupUpdate(SecRevocationDbConnectionRef dbc, CFDictionaryRef dict, CFErrorRef *error) {
    /* process one issuer group's update dictionary */
    __block int64_t groupId = -1;
    __block bool ok = (dbc != NULL);
    __block CFErrorRef localError = NULL;

    CFArrayRef issuers = (dict) ? (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("issuer-hash")) : NULL;
    /* look for existing group id */
    if (ok && isArray(issuers)) {
        CFIndex issuerIX, issuerCount = CFArrayGetCount(issuers);
        /* while we have issuers and haven't found a matching group id */
        for (issuerIX=0; issuerIX<issuerCount && groupId < 0; issuerIX++) {
            CFDataRef hash = (CFDataRef)CFArrayGetValueAtIndex(issuers, issuerIX);
            if (!hash) { continue; }
            groupId = _SecRevocationDbGroupIdForIssuerHash(dbc, hash, &localError);
        }
        if (groupId >= 0) {
            /* according to the spec, we must replace all existing issuers with
               the new issuers list, so delete all issuers in the group first. */
            ok = ok && SecDbWithSQL(dbc->dbconn, deleteGroupIssuersSQL, &localError, ^bool(sqlite3_stmt *deleteIssuers) {
                ok = ok && SecDbBindInt64(deleteIssuers, 1, groupId, &localError);
                ok = ok && SecDbStep(dbc->dbconn, deleteIssuers, &localError, NULL);
                return ok;
            });
        }
    }
    /* create or update the group entry */
    if (ok) {
        groupId = _SecRevocationDbUpdateGroup(dbc, groupId, dict, &localError);
    }
    if (groupId < 0) {
        secdebug("validupdate", "failed to get groupId");
        ok = false;
    } else {
        /* create or update issuer entries, now that we know the group id */
        ok = ok && _SecRevocationDbUpdateIssuers(dbc, groupId, issuers, &localError);
        /* create or update entries in serials or hashes tables */
        ok = ok && _SecRevocationDbUpdateIssuerData(dbc, groupId, dict, &localError);
        /* create or update entries in dates/names/policies tables */
        ok = ok && _SecRevocationDbUpdateIssuerConstraints(dbc, groupId, dict, &localError);
    }

    (void) CFErrorPropagate(localError, error);
    return ok;
}

bool _SecRevocationDbApplyUpdate(SecRevocationDbConnectionRef dbc, CFDictionaryRef update, CFIndex version, CFErrorRef *error) {
    /* process entire update dictionary */
    if (!dbc || !dbc->db || !update) {
        secerror("_SecRevocationDbApplyUpdate failed: invalid args");
        SecError(errSecParam, error, CFSTR("_SecRevocationDbApplyUpdate: invalid db or update parameter"));
        return false;
    }

    CFDictionaryRef localUpdate = (CFDictionaryRef)CFRetainSafe(update);
    CFErrorRef localError = NULL;
    bool ok = true;

    CFTypeRef value = NULL;
    CFIndex deleteCount = 0;
    CFIndex updateCount = 0;

    dbc->db->updateInProgress = true;

    /* check whether this is a full update */
    value = (CFBooleanRef)CFDictionaryGetValue(update, CFSTR("full"));
    if (isBoolean(value) && CFBooleanGetValue((CFBooleanRef)value)) {
        /* clear the database before processing a full update */
        dbc->fullUpdate = true;
        secdebug("validupdate", "update has \"full\" attribute; clearing database");
        ok = ok && _SecRevocationDbRemoveAllEntries(dbc, &localError);
    }

    /* process 'delete' list */
    value = (CFArrayRef)CFDictionaryGetValue(localUpdate, CFSTR("delete"));
    if (isArray(value)) {
        deleteCount = CFArrayGetCount((CFArrayRef)value);
        secdebug("validupdate", "processing %ld deletes", (long)deleteCount);
        for (CFIndex deleteIX=0; deleteIX<deleteCount; deleteIX++) {
            CFDataRef issuerHash = (CFDataRef)CFArrayGetValueAtIndex((CFArrayRef)value, deleteIX);
            if (isData(issuerHash)) {
                ok = ok && _SecRevocationDbApplyGroupDelete(dbc, issuerHash, &localError);
            } else {
                secdebug("validupdate", "skipping delete %ld (hash is not a data value)", (long)deleteIX);
            }
        }
    }

    /* process 'update' list */
    value = (CFArrayRef)CFDictionaryGetValue(localUpdate, CFSTR("update"));
    if (isArray(value)) {
        updateCount = CFArrayGetCount((CFArrayRef)value);
        secdebug("validupdate", "processing %ld updates", (long)updateCount);
        for (CFIndex updateIX=0; updateIX<updateCount; updateIX++) {
            CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex((CFArrayRef)value, updateIX);
            if (isDictionary(dict)) {
                ok = ok && _SecRevocationDbApplyGroupUpdate(dbc, dict, &localError);
            } else {
                secdebug("validupdate", "skipping update %ld (not a dictionary)", (long)updateIX);
            }
        }
    }
    CFReleaseSafe(localUpdate);

    /* set version */
    ok = ok && _SecRevocationDbSetVersion(dbc, version, &localError);

    /* set interval if not already set, or changed */
    int64_t interval = _SecRevocationDbGetUpdateInterval(dbc, NULL);
    if (interval != dbc->precommitInterval) {
        interval = (dbc->precommitInterval > 0) ? dbc->precommitInterval : kSecStdUpdateInterval;
        ok = ok && _SecRevocationDbSetUpdateInterval(dbc, interval, &localError);
    }

    /* set db_version if not already set */
    int64_t db_version = _SecRevocationDbGetSchemaVersion(dbc->db, dbc, NULL);
    if (db_version <= 0) {
        ok = ok && _SecRevocationDbSetSchemaVersion(dbc, kSecRevocationDbSchemaVersion, &localError);
    }

    /* set db_format if not already set */
    int64_t db_format = _SecRevocationDbGetUpdateFormat(dbc, NULL);
    if (db_format <= 0) {
        ok = ok && _SecRevocationDbSetUpdateFormat(dbc, kSecRevocationDbUpdateFormat, &localError);
    }

    /* purge the in-memory cache */
    SecRevocationDbCachePurge(dbc->db);

    dbc->db->updateInProgress = false;

    (void) CFErrorPropagate(localError, error);
    return ok;
}

static bool _SecRevocationDbSerialInGroup(SecRevocationDbConnectionRef dbc,
                                          CFDataRef serial,
                                          int64_t groupId,
                                          CFErrorRef *error) {
    __block bool result = false;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    require(dbc && serial, errOut);
    ok &= SecDbWithSQL(dbc->dbconn, selectSerialRecordSQL, &localError, ^bool(sqlite3_stmt *selectSerial) {
        ok &= SecDbBindInt64(selectSerial, 1, groupId, &localError);
        ok &= SecDbBindBlob(selectSerial, 2, CFDataGetBytePtr(serial),
                            CFDataGetLength(serial), SQLITE_TRANSIENT, &localError);
        ok &= SecDbStep(dbc->dbconn, selectSerial, &localError, ^(bool *stop) {
            int64_t foundRowId = (int64_t)sqlite3_column_int64(selectSerial, 0);
            result = (foundRowId > 0);
        });
        return ok;
    });

errOut:
    if (!ok || localError) {
        secerror("_SecRevocationDbSerialInGroup failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return result;
}

static bool _SecRevocationDbCertHashInGroup(SecRevocationDbConnectionRef dbc,
                                            CFDataRef certHash,
                                            int64_t groupId,
                                            CFErrorRef *error) {
    __block bool result = false;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    require(dbc && certHash, errOut);
    ok &= SecDbWithSQL(dbc->dbconn, selectHashRecordSQL, &localError, ^bool(sqlite3_stmt *selectHash) {
        ok &= SecDbBindInt64(selectHash, 1, groupId, &localError);
        ok = SecDbBindBlob(selectHash, 2, CFDataGetBytePtr(certHash),
                           CFDataGetLength(certHash), SQLITE_TRANSIENT, &localError);
        ok &= SecDbStep(dbc->dbconn, selectHash, &localError, ^(bool *stop) {
            int64_t foundRowId = (int64_t)sqlite3_column_int64(selectHash, 0);
            result = (foundRowId > 0);
        });
        return ok;
    });

errOut:
    if (!ok || localError) {
        secerror("_SecRevocationDbCertHashInGroup failed: %@", localError);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TARevocationDb, TAOperationRead, TAFatalError,
                                                     localError ? CFErrorGetCode(localError) : errSecInternalComponent);
    }
    (void) CFErrorPropagate(localError, error);
    return result;
}

static bool _SecRevocationDbSerialInFilter(SecRevocationDbConnectionRef dbc,
                                           CFDataRef serialData,
                                           CFDataRef xmlData) {
    /* N-To-1 filter implementation.
       The 'xmlData' parameter is a flattened XML dictionary,
       containing 'xor' and 'params' keys. First order of
       business is to reconstitute the blob into components.
    */
    bool result = false;
    CFRetainSafe(xmlData);
    CFDataRef propListData = xmlData;
    /* Expand data blob if needed */
    CFDataRef inflatedData = copyInflatedData(propListData);
    if (inflatedData) {
        CFReleaseSafe(propListData);
        propListData = inflatedData;
    }
    CFDataRef xor = NULL;
    CFArrayRef params = NULL;
    CFPropertyListRef nto1 = CFPropertyListCreateWithData(kCFAllocatorDefault, propListData, 0, NULL, NULL);
    if (nto1) {
        xor = (CFDataRef)CFDictionaryGetValue((CFDictionaryRef)nto1, CFSTR("xor"));
        params = (CFArrayRef)CFDictionaryGetValue((CFDictionaryRef)nto1, CFSTR("params"));
    }
    uint8_t *hash = (xor) ? (uint8_t*)CFDataGetBytePtr(xor) : NULL;
    CFIndex hashLen = (hash) ? CFDataGetLength(xor) : 0;
    uint8_t *serial = (serialData) ? (uint8_t*)CFDataGetBytePtr(serialData) : NULL;
    CFIndex serialLen = (serial) ? CFDataGetLength(serialData) : 0;

    require(hash && serial && params, errOut);

    const uint32_t FNV_OFFSET_BASIS = 2166136261;
    const uint32_t FNV_PRIME = 16777619;
    bool notInHash = false;
    CFIndex ix, count = CFArrayGetCount(params);
    for (ix = 0; ix < count; ix++) {
        int32_t param;
        CFNumberRef cfnum = (CFNumberRef)CFArrayGetValueAtIndex(params, ix);
        if (!isNumber(cfnum) ||
            !CFNumberGetValue(cfnum, kCFNumberSInt32Type, &param)) {
            secinfo("validupdate", "error processing filter params at index %ld", (long)ix);
            continue;
        }
        /* process one param */
        uint32_t hval = FNV_OFFSET_BASIS ^ param;
        CFIndex i = serialLen;
        while (i > 0) {
            hval = ((hval ^ (serial[--i])) * FNV_PRIME) & 0xFFFFFFFF;
        }
        hval = hval % (hashLen * 8);
        if ((hash[hval/8] & (1 << (hval % 8))) == 0) {
            notInHash = true; /* definitely not in hash */
            break;
        }
    }
    if (!notInHash) {
        /* probabilistically might be in hash if we get here. */
        result = true;
    }

errOut:
    CFReleaseSafe(nto1);
    CFReleaseSafe(propListData);
    return result;
}

static SecValidInfoRef _SecRevocationDbValidInfoForCertificate(SecRevocationDbConnectionRef dbc,
                                                               SecCertificateRef certificate,
                                                               CFDataRef issuerHash,
                                                               CFErrorRef *error) {
    __block CFErrorRef localError = NULL;
    __block SecValidInfoFlags flags = 0;
    __block SecValidInfoFormat format = kSecValidInfoFormatUnknown;
    __block CFDataRef data = NULL;

    bool matched = false;
    bool isOnList = false;
    int64_t groupId = 0;
    CFDataRef serial = NULL;
    CFDataRef certHash = NULL;
    CFDateRef notBeforeDate = NULL;
    CFDateRef notAfterDate = NULL;
    CFDataRef names = NULL;
    CFDataRef policies = NULL;
    SecValidInfoRef result = NULL;

    require((serial = SecCertificateCopySerialNumberData(certificate, NULL)) != NULL, errOut);
    require((certHash = SecCertificateCopySHA256Digest(certificate)) != NULL, errOut);
    require_quiet((groupId = _SecRevocationDbGroupIdForIssuerHash(dbc, issuerHash, &localError)) > 0, errOut);

    /* Look up the group record to determine flags and format. */
    format = _SecRevocationDbGetGroupFormat(dbc, groupId, &flags, &data, &policies, &localError);

    if (format == kSecValidInfoFormatUnknown) {
        /* No group record found for this issuer. Don't return a SecValidInfoRef */
        goto errOut;
    }
    else if (format == kSecValidInfoFormatSerial) {
        /* Look up certificate's serial number in the serials table. */
        matched = _SecRevocationDbSerialInGroup(dbc, serial, groupId, &localError);
    }
    else if (format == kSecValidInfoFormatSHA256) {
        /* Look up certificate's SHA-256 hash in the hashes table. */
        matched = _SecRevocationDbCertHashInGroup(dbc, certHash, groupId, &localError);
    }
    else if (format == kSecValidInfoFormatNto1) {
        /* Perform a Bloom filter match against the serial. If matched is false,
           then the cert is definitely not in the list. But if matched is true,
           we don't know for certain, so we would need to check OCSP. */
        matched = _SecRevocationDbSerialInFilter(dbc, serial, data);
    }

    if (matched) {
        /* Found a specific match for this certificate. */
        secdebug("validupdate", "Valid db matched certificate: %@, format=%d, flags=0x%lx",
                 certHash, format, flags);
        isOnList = true;
    }

    /* If supplemental constraints are present for this issuer, then we always match. */
    if ((flags & kSecValidInfoDateConstraints) &&
        (_SecRevocationDbCopyDateConstraints(dbc, groupId, &notBeforeDate, &notAfterDate, &localError))) {
        secdebug("validupdate", "Valid db matched supplemental date constraints for groupId %lld: nb=%@, na=%@",
                 (long long)groupId, notBeforeDate, notAfterDate);
    }


    /* Return SecValidInfo for certificates for which an issuer entry is found. */
    result = SecValidInfoCreate(format, flags, isOnList,
                                certHash, issuerHash, /*anchorHash*/ NULL,
                                notBeforeDate, notAfterDate,
                                names, policies);

    if (result && SecIsAppleTrustAnchor(certificate, 0)) {
        /* Prevent a catch-22. */
        secdebug("validupdate", "Valid db match for Apple trust anchor: %@, format=%d, flags=0x%lx",
                 certHash, format, flags);
        CFReleaseNull(result);
    }

errOut:
    (void) CFErrorPropagate(localError, error);
    CFReleaseSafe(data);
    CFReleaseSafe(certHash);
    CFReleaseSafe(serial);
    CFReleaseSafe(notBeforeDate);
    CFReleaseSafe(notAfterDate);
    CFReleaseSafe(names);
    CFReleaseSafe(policies);
    return result;
}

static SecValidInfoRef _SecRevocationDbCopyMatching(SecRevocationDbConnectionRef dbc,
                                                    SecCertificateRef certificate,
                                                    SecCertificateRef issuer) {
    SecValidInfoRef result = NULL;
    CFErrorRef error = NULL;
    CFDataRef issuerHash = NULL;

    require_quiet(dbc && certificate && issuer, errOut);
    require(issuerHash = SecCertificateCopySHA256Digest(issuer), errOut);

    /* Check for the result in the cache. */
    result = SecRevocationDbCacheRead(dbc->db, certificate, issuerHash);

    /* Upon cache miss, get the result from the database and add it to the cache. */
    if (!result) {
        result = _SecRevocationDbValidInfoForCertificate(dbc, certificate, issuerHash, &error);
        SecRevocationDbCacheWrite(dbc->db, result);
    }

errOut:
    CFReleaseSafe(issuerHash);
    CFReleaseSafe(error);
    return result;
}

/* Return the update source as a retained CFStringRef.
   If the value cannot be obtained, NULL is returned.
*/
CF_RETURNS_RETAINED CFStringRef SecRevocationDbCopyUpdateSource(void) {
    __block CFStringRef result = NULL;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        (void) SecRevocationDbPerformRead(db, NULL, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            result = _SecRevocationDbCopyUpdateSource(dbc, blockError);
            return (bool)result;
        });
    });
    return result;
}

/* Set the next update value for the revocation database.
   (This function is expected to be called only by the database
   maintainer, normally the system instance of trustd. If the
   caller does not have write access, this is a no-op.)
*/
bool SecRevocationDbSetNextUpdateTime(CFAbsoluteTime nextUpdate, CFErrorRef *error) {
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    SecRevocationDbWith(^(SecRevocationDbRef rdb) {
        ok &= SecRevocationDbPerformWrite(rdb, &localError, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            return _SecRevocationDbSetNextUpdateTime(dbc, nextUpdate, blockError);
        });
    });
    (void) CFErrorPropagate(localError, error);
    return ok;
}

/* Return the next update value as a CFAbsoluteTime.
   If the value cannot be obtained, -1 is returned.
*/
CFAbsoluteTime SecRevocationDbGetNextUpdateTime(void) {
    __block CFAbsoluteTime result = -1;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        (void) SecRevocationDbPerformRead(db, NULL, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            result = _SecRevocationDbGetNextUpdateTime(dbc, blockError);
            return true;
        });
    });
    return result;
}

/* Return the serial background queue for database updates.
   If the queue cannot be obtained, NULL is returned.
*/
dispatch_queue_t SecRevocationDbGetUpdateQueue(void) {
    __block dispatch_queue_t result = NULL;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        result = (db) ? db->update_queue : NULL;
    });
    return result;
}

/* Release all connections to the revocation database.
*/
void SecRevocationDbReleaseAllConnections(void) {
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        SecDbReleaseAllConnections((db) ? db->db : NULL);
    });
}

/* === SecRevocationDb API === */

/* Given a certificate and its issuer, returns a SecValidInfoRef if the
   valid database contains matching info; otherwise returns NULL.
   Caller must release the returned SecValidInfoRef when finished.
*/
SecValidInfoRef SecRevocationDbCopyMatching(SecCertificateRef certificate,
                                            SecCertificateRef issuer) {
    __block SecValidInfoRef result = NULL;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        (void) SecRevocationDbPerformRead(db, NULL, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            result = _SecRevocationDbCopyMatching(dbc, certificate, issuer);
            return (bool)result;
        });
    });
    return result;
}

/* Given an issuer, returns true if an entry for this issuer exists in
   the database (i.e. a known CA). If the provided certificate is NULL,
   or its entry is not found, the function returns false.
*/
bool SecRevocationDbContainsIssuer(SecCertificateRef issuer) {
    if (!issuer) {
        return false;
    }
    __block bool result = false;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        (void) SecRevocationDbPerformRead(db, NULL, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            CFDataRef issuerHash = SecCertificateCopySHA256Digest(issuer);
            int64_t groupId = _SecRevocationDbGroupIdForIssuerHash(dbc, issuerHash, blockError);
            CFReleaseSafe(issuerHash);
            result = (groupId > 0);
            return result;
        });
    });
    return result;
}

/* Return the current version of the revocation database.
   A version of 0 indicates an empty database which must be populated.
   If the version cannot be obtained, -1 is returned.
*/
CFIndex SecRevocationDbGetVersion(void) {
    __block CFIndex result = -1;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        (void) SecRevocationDbPerformRead(db, NULL, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            result = (CFIndex)_SecRevocationDbGetVersion(dbc, blockError);
            return (result >= 0);
        });
    });
    return result;
}

/* Return the current schema version of the revocation database.
 A version of 0 indicates an empty database which must be populated.
 If the schema version cannot be obtained, -1 is returned.
 */
CFIndex SecRevocationDbGetSchemaVersion(void) {
    __block CFIndex result = -1;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        result = (CFIndex)_SecRevocationDbGetSchemaVersion(db, NULL, NULL);
    });
    return result;
}

/* Return the current update format of the revocation database.
 A version of 0 indicates the format was unknown.
 If the update format cannot be obtained, -1 is returned.
 */
CFIndex SecRevocationDbGetUpdateFormat(void) {
    __block CFIndex result = -1;
    SecRevocationDbWith(^(SecRevocationDbRef db) {
        (void) SecRevocationDbPerformRead(db, NULL, ^bool(SecRevocationDbConnectionRef dbc, CFErrorRef *blockError) {
            result = (CFIndex)_SecRevocationDbGetUpdateFormat(dbc, blockError);
            return (result >= 0);
        });
    });
    return result;
}

// MARK: -
// MARK: Digests
/*
 ==============================================================================
   Digest computation
 ==============================================================================
*/

/* Returns array of SHA-256 hashes computed over the contents of a valid.sqlite3
   database, in the order specified by the valid-server-api documentation. The
   resulting hashes can be compared against those in the update's 'hash' array.

   Hash 0: full database (all fields in initial Valid specification)
   Hash 1: all issuer_hash arrays, plus not-after and not-before dates for each
   Hash 2: subset of issuer_hash arrays where the no-ca-v2 flag is set
*/
static CF_RETURNS_RETAINED CFArrayRef SecRevocationDbComputeFullContentDigests(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    if (!dbc) { return NULL; }
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    __block uint32_t N[4]={0,0,0,0};
    __block CC_SHA256_CTX hash0_ctx, hash1_ctx, hash2_ctx;
    CC_SHA256_Init(&hash0_ctx);
    CC_SHA256_Init(&hash1_ctx);
    CC_SHA256_Init(&hash2_ctx);

    // Add version, check-again, and update (array count) fields as array of N.
    // (Note: 'N' is defined as "unsigned 32-bit integer in network byte order")
    int64_t version = _SecRevocationDbGetVersion(dbc, NULL);
    N[0] = OSSwapInt32(version & 0xffffffff);
    int64_t interval =  _SecRevocationDbGetUpdateInterval(dbc, NULL);
    if (interval < 0) {
        interval = kSecStdUpdateInterval; // if we didn't store it, assume default
    }
    N[1] = OSSwapInt32(interval & 0xffffffff);
    __block int64_t count = 0;
    ok = ok && SecDbWithSQL(dbc->dbconn, CFSTR("SELECT count(*) FROM groups"), &localError, ^bool(sqlite3_stmt *selectGroupsCount) {
        ok = ok && SecDbStep(dbc->dbconn, selectGroupsCount, &localError, ^void(bool *stop) {
            count = sqlite3_column_int64(selectGroupsCount, 0);
            *stop = true;
        });
        return ok;
    });
    N[2] = OSSwapInt32(count & 0xffffffff);
    CC_SHA256_Update(&hash0_ctx, N, sizeof(uint32_t) * 3);

    // Sort the update array in order of minimum 'issuer-hash' entry.
    // The issuer-hash array is first sorted to determine the lowest issuer-hash,
    // and that value is used to sort the update entries.
    //
    // For our sqlite database, recreating the update array order means fetching
    // the groupid column from the issuers table after sorting on issuer_hash,
    // using DISTINCT to remove duplicates. Then, for each returned groupid, we
    // obtain its list of issuers, its list of serials or hashes, and other data.

    ok = ok && SecDbWithSQL(dbc->dbconn, CFSTR("SELECT DISTINCT groupid FROM issuers ORDER BY issuer_hash ASC"), &localError, ^bool(sqlite3_stmt *selectGroups) {
        ok = ok && SecDbForEach(dbc->dbconn, selectGroups, &localError, ^bool(int row_index) {
            __block int64_t groupId = sqlite3_column_int64(selectGroups, 0);
            ok = ok && SecDbWithSQL(dbc->dbconn, CFSTR("SELECT flags,format,data FROM groups WHERE groupid=?"), &localError, ^bool(sqlite3_stmt *selectGroup) {
                ok = ok && SecDbBindInt64(selectGroup, 1, groupId, &localError);
                ok = ok && SecDbStep(dbc->dbconn, selectGroup, &localError, ^(bool *stop) {
                    // per-group info is hashed in the following order:
                    // - issuer_hash array data (sorted)
                    // - flag bytes, in order listed below
                    // - format string [serial|sha256|nto1]
                    // - add array data (sorted), if [serial|sha256]
                    // - params (if present)
                    // - xor data (if present)

                    int64_t flags = sqlite3_column_int64(selectGroup, 0);
                    bool noCAv2 = (flags & kSecValidInfoNoCAv2Check);

                    // instead of recreating the issuer_hash array in memory,
                    // hash its length (item count) followed by the data of each issuer_hash.
                    ok = ok && SecDbWithSQL(dbc->dbconn, CFSTR("SELECT count(*) FROM issuers WHERE groupid=?"), &localError, ^bool(sqlite3_stmt *selectIssuersCount) {
                        ok = ok && SecDbBindInt64(selectIssuersCount, 1, groupId, &localError);
                        ok = ok && SecDbStep(dbc->dbconn, selectIssuersCount, &localError, ^void(bool *stop) {
                            count = sqlite3_column_int64(selectIssuersCount, 0);
                            *stop = true;
                        });
                        return ok;
                    });
                    uint32_t n = OSSwapInt32(count & 0xffffffff);
                    CC_SHA256_Update(&hash0_ctx, &n, sizeof(uint32_t));
                    CC_SHA256_Update(&hash1_ctx, &n, sizeof(uint32_t));
                    if (noCAv2) {
                        CC_SHA256_Update(&hash2_ctx, &n, sizeof(uint32_t));
                    }

                    // process issuer_hash entries for this group
                    ok = ok && SecDbWithSQL(dbc->dbconn, CFSTR("SELECT issuer_hash FROM issuers WHERE groupid=? ORDER BY issuer_hash ASC"), &localError, ^bool(sqlite3_stmt *selectIssuerHash) {
                        ok = ok && SecDbBindInt64(selectIssuerHash, 1, groupId, &localError);
                        ok = ok && SecDbForEach(dbc->dbconn, selectIssuerHash, &localError, ^bool(int row_index) {
                            uint8_t *p = (uint8_t *)sqlite3_column_blob(selectIssuerHash, 0);
                            CFDataRef data = NULL;
                            if (p != NULL) {
                                CFIndex length = (CFIndex)sqlite3_column_bytes(selectIssuerHash, 0);
                                data = CFDataCreate(kCFAllocatorDefault, p, length);
                            }
                            if (data != NULL) {
                                hashData(data, &hash0_ctx);
                                hashData(data, &hash1_ctx);
                                if (noCAv2) {
                                    hashData(data, &hash2_ctx);
                                }
                                CFRelease(data);
                            } else {
                                ok = false;
                            }
                            return ok;
                        });
                        return ok;
                    });

                    // process flags, converting to array of unsigned 8-bit values, either 0 or 1:
                    // [ complete, check-ocsp, known-intermediates-only, no-ca, overridable, require-ct, valid ]
                    uint8_t C[8]={0,0,0,0,0,0,0,0};
                    C[0] = (flags & kSecValidInfoComplete) ? 1 : 0;
                    C[1] = (flags & kSecValidInfoCheckOCSP) ? 1 : 0;
                    C[2] = (flags & kSecValidInfoKnownOnly) ? 1 : 0;
                    C[3] = (flags & kSecValidInfoNoCACheck) ? 1 : 0;
                    C[4] = (flags & kSecValidInfoOverridable) ? 1 : 0;
                    C[5] = (flags & kSecValidInfoRequireCT) ? 1 : 0;
                    C[6] = (flags & kSecValidInfoAllowlist) ? 1 : 0;
                    CC_SHA256_Update(&hash0_ctx, C, sizeof(uint8_t) * 7);

                    // process format, converting integer to string value [serial|sha256|nto1]
                    SecValidInfoFormat format = (SecValidInfoFormat)sqlite3_column_int(selectGroup, 1);
                    switch (format) {
                        case kSecValidInfoFormatSerial:
                            hashString(CFSTR("serial"), &hash0_ctx);
                            break;
                        case kSecValidInfoFormatSHA256:
                            hashString(CFSTR("sha256"), &hash0_ctx);
                            break;
                        case kSecValidInfoFormatNto1:
                            hashString(CFSTR("nto1"), &hash0_ctx);
                            break;
                        case kSecValidInfoFormatUnknown:
                        default:
                            ok = false; // unexpected format values are not allowed
                            break;
                    }
                    // process 'add' array (serial or sha256 format).
                    // instead of recreating the 'add' array in memory,
                    // hash its length (item count) followed by the data of each entry.
                    CFStringRef arrayCountSql = NULL;
                    if (format == kSecValidInfoFormatSerial) {
                        arrayCountSql = CFSTR("SELECT count(*) FROM serials WHERE groupid=?");
                    } else if (format == kSecValidInfoFormatSHA256) {
                        arrayCountSql = CFSTR("SELECT count(*) FROM hashes WHERE groupid=?");
                    }
                    if (arrayCountSql) {
                        ok = ok && SecDbWithSQL(dbc->dbconn, arrayCountSql, &localError, ^bool(sqlite3_stmt *selectAddCount) {
                            ok = ok && SecDbBindInt64(selectAddCount, 1, groupId, &localError);
                            ok = ok && SecDbStep(dbc->dbconn, selectAddCount, &localError, ^void(bool *stop) {
                                count = sqlite3_column_int64(selectAddCount, 0);
                                *stop = true;
                            });
                            return ok;
                        });
                        n = OSSwapInt32(count & 0xffffffff);
                        CC_SHA256_Update(&hash0_ctx, &n, sizeof(uint32_t));
                    }
                    // process data entries for this group
                    CFStringRef arrayDataSql = NULL;
                    if (format == kSecValidInfoFormatSerial) {
                        arrayDataSql = CFSTR("SELECT serial FROM serials WHERE groupid=? ORDER BY serial ASC");
                    } else if (format == kSecValidInfoFormatSHA256) {
                        arrayDataSql = CFSTR("SELECT sha256 FROM hashes WHERE groupid=? ORDER by sha256 ASC");
                    }
                    if (arrayDataSql) {
                        ok = ok && SecDbWithSQL(dbc->dbconn, arrayDataSql, &localError, ^bool(sqlite3_stmt *selectAddData) {
                            ok = ok && SecDbBindInt64(selectAddData, 1, groupId, &localError);
                            ok = ok && SecDbForEach(dbc->dbconn, selectAddData, &localError, ^bool(int row_index) {
                                uint8_t *p = (uint8_t *)sqlite3_column_blob(selectAddData, 0);
                                CFDataRef data = NULL;
                                if (p != NULL) {
                                    CFIndex length = (CFIndex)sqlite3_column_bytes(selectAddData, 0);
                                    data = CFDataCreate(kCFAllocatorDefault, p, length);
                                }
                                if (data != NULL) {
                                    hashData(data, &hash0_ctx);
                                    CFRelease(data);
                                } else {
                                    ok = false;
                                }
                                return ok;
                            });
                            return ok;
                        });
                    }

                    // process params and xor data, if format is nto1
                    if (format == kSecValidInfoFormatNto1) {
                        uint8_t *p = (uint8_t *)sqlite3_column_blob(selectGroup, 2);
                        CFDataRef data = NULL;
                        if (p != NULL) {
                            CFIndex length = (CFIndex)sqlite3_column_bytes(selectGroup, 2);
                            data = CFDataCreate(kCFAllocatorDefault, p, length);
                        }
                        if (data != NULL) {
                            // unpack params and xor data
                            CFDataRef xor = NULL;
                            CFArrayRef params = NULL;
                            if (copyFilterComponents(data, &xor, &params)) {
                                hashArray(params, &hash0_ctx);
                                hashData(xor, &hash0_ctx);
                            } else {
                                ok = false;
                            }
                            CFReleaseSafe(xor);
                            CFReleaseSafe(params);
                        }
                        CFReleaseSafe(data);
                    }

                    // process date constraints [not-after, not-before]
                    CFAbsoluteTime notBefore = -3155760000.0; /* default: 1901-01-01 00:00:00-0000 */
                    CFAbsoluteTime notAfter = 31556908800.0;  /* default: 3001-01-01 00:00:00-0000 */
                    CFDateRef notBeforeDate = NULL;
                    CFDateRef notAfterDate = NULL;
                    if (_SecRevocationDbCopyDateConstraints(dbc, groupId, &notBeforeDate, &notAfterDate, &localError)) {
                        if (notBeforeDate) {
                            notBefore = CFDateGetAbsoluteTime(notBeforeDate);
                            CFReleaseNull(notBeforeDate);
                        }
                        if (notAfterDate) {
                            notAfter = CFDateGetAbsoluteTime(notAfterDate);
                            CFReleaseNull(notAfterDate);
                        }
                    }
                    double nb = htond(notBefore);
                    double na = htond(notAfter);
                    CC_SHA256_Update(&hash1_ctx, &na, sizeof(double));
                    CC_SHA256_Update(&hash1_ctx, &nb, sizeof(double));

                    *stop = true;
                }); // per-group step
                return ok;
            }); // per-group select
            return ok;
        }); // for each group in list
        return ok;
    }); // select full group list

    CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (result) {
        uint8_t digest[CC_SHA256_DIGEST_LENGTH];
        CFDataRef data = NULL;
        CC_SHA256_Final(digest, &hash0_ctx);
        if ((data = CFDataCreate(NULL, (const UInt8 *)digest, CC_SHA256_DIGEST_LENGTH)) != NULL) {
            CFArrayAppendValue(result, data);
            CFReleaseNull(data);
        }
        CC_SHA256_Final(digest, &hash1_ctx);
        if ((data = CFDataCreate(NULL, (const UInt8 *)digest, CC_SHA256_DIGEST_LENGTH)) != NULL) {
            CFArrayAppendValue(result, data);
            CFReleaseNull(data);
        }
        CC_SHA256_Final(digest, &hash2_ctx);
        if ((data = CFDataCreate(NULL, (const UInt8 *)digest, CC_SHA256_DIGEST_LENGTH)) != NULL) {
            CFArrayAppendValue(result, data);
            CFReleaseNull(data);
        }
    }
    (void) CFErrorPropagate(localError, error);
    return result;
}

static bool SecRevocationDbComputeDigests(SecRevocationDbConnectionRef dbc, CFErrorRef *error) {
    secinfo("validupdate", "Started verifying db content");
    bool result = true;
    CFArrayRef expectedList = _SecRevocationDbCopyHashes(dbc, error);
    CFIndex expectedCount = (expectedList) ? CFArrayGetCount(expectedList) : 0;
    if (expectedCount < 1) {
        secinfo("validupdate", "Unable to read db_hash values");
        CFReleaseNull(expectedList);
        return result; // %%%% this will happen on first update, when db_hash isn't there
    }
    CFArrayRef computedList = SecRevocationDbComputeFullContentDigests(dbc, error);
    CFIndex computedCount = (computedList) ? CFArrayGetCount(computedList) : 0;
    for (CFIndex idx = 0; idx < expectedCount; idx++) {
        if (idx >= computedCount) {
            continue; // server provided additional hash value that we don't yet compute
        }
        CFDataRef expectedHash = (CFDataRef)CFArrayGetValueAtIndex(expectedList, idx);
        CFDataRef computedHash = (CFDataRef)CFArrayGetValueAtIndex(computedList, idx);
        if (!CFEqualSafe(expectedHash, computedHash)) {
            result = false;
            break;
        }
    }
    if (!result) {
        secinfo("validupdate", "Expected: %@", expectedList);
        secinfo("validupdate", "Computed: %@", computedList);
    }
    secinfo("validupdate", "Finished verifying db content; result=%s",
            (result) ? "SUCCESS" : "FAIL");
    CFReleaseSafe(expectedList);
    CFReleaseSafe(computedList);
    return result;
}

