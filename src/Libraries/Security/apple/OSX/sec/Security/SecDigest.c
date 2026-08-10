/*
 * Copyright (c) 2006-2010,2012-2015 Apple Inc. All Rights Reserved.
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
 */

#ifdef STANDALONE
/* Allows us to build genanchors against the BaseSDK. */
#undef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#undef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif

#include "SecFramework.h"
#include <dispatch/dispatch.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <Security/SecAsn1Coder.h>
#include <Security/oidsalg.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecBase.h>
#include <inttypes.h>

/* Return the SHA1 digest of a chunk of data as newly allocated CFDataRef. */
CFDataRef SecSHA1DigestCreate(CFAllocatorRef allocator,
                              const UInt8 *data, CFIndex length) {
    if (length < 0 || length > INT32_MAX || data == NULL) {
        return NULL; /* guard against passing bad values to digest function */
    }
    CFMutableDataRef digest = CFDataCreateMutable(allocator,
                                                  CC_SHA1_DIGEST_LENGTH);
    CFDataSetLength(digest, CC_SHA1_DIGEST_LENGTH);
    CCDigest(kCCDigestSHA1, data, length, CFDataGetMutableBytePtr(digest));
    return digest;
}

CFDataRef SecSHA256DigestCreate(CFAllocatorRef allocator,
                                const UInt8 *data, CFIndex length) {
    if (length < 0 || length > INT32_MAX || data == NULL) {
        return NULL; /* guard against passing bad values to digest function */
    }
    CFMutableDataRef digest = CFDataCreateMutable(allocator,
                                                  CC_SHA256_DIGEST_LENGTH);
    CFDataSetLength(digest, CC_SHA256_DIGEST_LENGTH);
    CCDigest(kCCDigestSHA256, data, length, CFDataGetMutableBytePtr(digest));
    return digest;
}

CFDataRef SecSHA256DigestCreateFromData(CFAllocatorRef allocator, CFDataRef data) {
    CFMutableDataRef digest = CFDataCreateMutable(allocator,
                                                  CC_SHA256_DIGEST_LENGTH);
    CFDataSetLength(digest, CC_SHA256_DIGEST_LENGTH);
    CCDigest(kCCDigestSHA256, CFDataGetBytePtr(data), CFDataGetLength(data), CFDataGetMutableBytePtr(digest));
    return digest;
}

CFDataRef SecDigestCreate(CFAllocatorRef allocator,
                          const SecAsn1Oid *algorithm, const SecAsn1Item *params,
                          const UInt8 *data, CFIndex length) {
    unsigned char *(*digestFcn)(const void *data, CC_LONG len, unsigned char *md);
    CFIndex digestLen;

    if (length < 0 || length > INT32_MAX || data == NULL)
        return NULL; /* guard against passing bad values to digest function */

    if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA1)) {
        digestFcn = CC_SHA1;
        digestLen = CC_SHA1_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA224)) {
        digestFcn = CC_SHA224;
        digestLen = CC_SHA224_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA256)) {
        digestFcn = CC_SHA256;
        digestLen = CC_SHA256_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA384)) {
        digestFcn = CC_SHA384;
        digestLen = CC_SHA384_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA512)) {
        digestFcn = CC_SHA512;
        digestLen = CC_SHA512_DIGEST_LENGTH;
    } else {
        return NULL;
    }

    CFMutableDataRef digest = CFDataCreateMutable(allocator, digestLen);
    CFDataSetLength(digest, digestLen);

    digestFcn(data, (CC_LONG)length, CFDataGetMutableBytePtr(digest));
    return digest;
}
