// CFURLEnumerator.h - enumerating directories and mounted volumes
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#if !defined(__COREFOUNDATION_CFURLENUMERATOR__)
#define __COREFOUNDATION_CFURLENUMERATOR__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFURL.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_EXTERN_C_BEGIN

typedef const struct __CFURLEnumerator *CFURLEnumeratorRef;

CF_EXPORT CFTypeID CFURLEnumeratorGetTypeID(void);

typedef CF_OPTIONS(CFOptionFlags, CFURLEnumeratorOptions) {
    kCFURLEnumeratorDefaultBehavior = 0,
    kCFURLEnumeratorDescendRecursively = 1UL << 0,
    kCFURLEnumeratorSkipInvisibles = 1UL << 1,
    kCFURLEnumeratorGenerateFileReferenceURLs = 1UL << 2,
    kCFURLEnumeratorSkipPackageContents = 1UL << 3,
    kCFURLEnumeratorIncludeDirectoriesPreOrder = 1UL << 4,
    kCFURLEnumeratorIncludeDirectoriesPostOrder = 1UL << 5,
    kCFURLEnumeratorGenerateRelativePathURLs = 1UL << 6
};

typedef CF_ENUM(CFIndex, CFURLEnumeratorResult) {
    kCFURLEnumeratorSuccess = 1,
    kCFURLEnumeratorEnd = 2,
    kCFURLEnumeratorError = 3,
    kCFURLEnumeratorDirectoryPostOrderSuccess = 4
};

CF_EXPORT CFURLEnumeratorRef CFURLEnumeratorCreateForDirectoryURL(CFAllocatorRef alloc, CFURLRef directoryURL, CFURLEnumeratorOptions option, CFArrayRef propertyKeys);
CF_EXPORT CFURLEnumeratorRef CFURLEnumeratorCreateForMountedVolumes(CFAllocatorRef alloc, CFURLEnumeratorOptions option, CFArrayRef propertyKeys);
CF_EXPORT CFURLEnumeratorResult CFURLEnumeratorGetNextURL(CFURLEnumeratorRef enumerator, CFURLRef *url, CFErrorRef *error);
CF_EXPORT void CFURLEnumeratorSkipDescendents(CFURLEnumeratorRef enumerator);
CF_EXPORT CFIndex CFURLEnumeratorGetDescendentLevel(CFURLEnumeratorRef enumerator);
CF_EXPORT Boolean CFURLEnumeratorGetSourceDidChange(CFURLEnumeratorRef enumerator);

CF_EXTERN_C_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif
