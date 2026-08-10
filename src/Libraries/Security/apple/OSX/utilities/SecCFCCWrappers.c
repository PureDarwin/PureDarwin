/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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


#include <utilities/SecCFCCWrappers.h>

#include <utilities/simulatecrash_assert.h>

CFDataRef CFDataCreateDigestWithBytes(CFAllocatorRef allocator, const struct ccdigest_info *di, size_t len,
                                const void *data, CFErrorRef *error) {
    CFMutableDataRef digest = CFDataCreateMutable(allocator, di->output_size);
    CFDataSetLength(digest, di->output_size);
    ccdigest(di, len, data, CFDataGetMutableBytePtr(digest));
    return digest;
}

CFDataRef CFDataCreateSHA1DigestWithBytes(CFAllocatorRef allocator, size_t len, const void *data, CFErrorRef *error) {
    return CFDataCreateDigestWithBytes(allocator, ccsha1_di(), len, data, error);
}

CFDataRef CFDataCreateSHA256DigestWithBytes(CFAllocatorRef allocator, size_t len, const void *data, CFErrorRef *error) {
    return CFDataCreateDigestWithBytes(allocator, ccsha256_di(), len, data, error);
}


CFDataRef CFDataCopySHA1Digest(CFDataRef dataToDigest, CFErrorRef *error) {
    CFIndex length = CFDataGetLength(dataToDigest);
    assert((unsigned long)length < UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
    return CFDataCreateSHA1DigestWithBytes(CFGetAllocator(dataToDigest), length, CFDataGetBytePtr(dataToDigest), error);
}

CFDataRef CFDataCopySHA256Digest(CFDataRef dataToDigest, CFErrorRef *error) {
    CFIndex length = CFDataGetLength(dataToDigest);
    assert((unsigned long)length < UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
    return CFDataCreateSHA256DigestWithBytes(CFGetAllocator(dataToDigest), length, CFDataGetBytePtr(dataToDigest), error);
}
