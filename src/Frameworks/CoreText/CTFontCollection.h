// CTFontCollection.h - collections of font descriptors
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTFONTCOLLECTION__
#define __CORETEXT_CTFONTCOLLECTION__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreTextExport.h>
#import <CoreText/CTFontDescriptor.h>

typedef const struct __CTFontCollection *CTFontCollectionRef;
typedef struct __CTFontCollection *CTMutableFontCollectionRef;

CORETEXT_EXPORT CFTypeID CTFontCollectionGetTypeID(void);

typedef CFComparisonResult (*CTFontCollectionSortDescriptorsCallback)(CTFontDescriptorRef first, CTFontDescriptorRef second, void *refCon);

CORETEXT_EXPORT const CFStringRef kCTFontCollectionRemoveDuplicatesOption;
CORETEXT_EXPORT const CFStringRef kCTFontCollectionIncludeDisabledFontsOption;
CORETEXT_EXPORT const CFStringRef kCTFontCollectionDisallowAutoActivationOption;

typedef uint32_t CTFontCollectionCopyOptions;
enum {
    kCTFontCollectionCopyDefaultOptions = 0,
    kCTFontCollectionCopyUnique = (1 << 0),
    kCTFontCollectionCopyStandardSort = (1 << 1)
};

CORETEXT_EXPORT CTFontCollectionRef CTFontCollectionCreateFromAvailableFonts(CFDictionaryRef options);
CORETEXT_EXPORT CTFontCollectionRef CTFontCollectionCreateWithFontDescriptors(CFArrayRef queryDescriptors, CFDictionaryRef options);
CORETEXT_EXPORT CTFontCollectionRef CTFontCollectionCreateCopyWithFontDescriptors(CTFontCollectionRef original, CFArrayRef queryDescriptors, CFDictionaryRef options);
CORETEXT_EXPORT CTMutableFontCollectionRef CTFontCollectionCreateMutableCopy(CTFontCollectionRef original);
CORETEXT_EXPORT CFArrayRef CTFontCollectionCopyQueryDescriptors(CTFontCollectionRef collection);
CORETEXT_EXPORT void CTFontCollectionSetQueryDescriptors(CTMutableFontCollectionRef collection, CFArrayRef descriptors);
CORETEXT_EXPORT CFArrayRef CTFontCollectionCopyExclusionDescriptors(CTFontCollectionRef collection);
CORETEXT_EXPORT void CTFontCollectionSetExclusionDescriptors(CTMutableFontCollectionRef collection, CFArrayRef descriptors);
CORETEXT_EXPORT CFArrayRef CTFontCollectionCreateMatchingFontDescriptors(CTFontCollectionRef collection);
CORETEXT_EXPORT CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsSortedWithCallback(CTFontCollectionRef collection, CTFontCollectionSortDescriptorsCallback sortCallback, void *refCon);
CORETEXT_EXPORT CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsWithOptions(CTFontCollectionRef collection, CFDictionaryRef options);
CORETEXT_EXPORT CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsForFamily(CTFontCollectionRef collection, CFStringRef familyName, CFDictionaryRef options);
CORETEXT_EXPORT CFArrayRef CTFontCollectionCopyFontAttribute(CTFontCollectionRef collection, CFStringRef attributeName, CTFontCollectionCopyOptions options);
CORETEXT_EXPORT CFArrayRef CTFontCollectionCopyFontAttributes(CTFontCollectionRef collection, CFSetRef attributeNames, CTFontCollectionCopyOptions options);

#endif
