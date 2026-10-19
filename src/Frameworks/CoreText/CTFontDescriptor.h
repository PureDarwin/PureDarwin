// CTFontDescriptor.h - font descriptors and their attribute keys
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTFONTDESCRIPTOR__
#define __CORETEXT_CTFONTDESCRIPTOR__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreText/CoreTextExport.h>
#import <CoreText/CTFontTraits.h>

typedef const struct __CTFontDescriptor *CTFontDescriptorRef;

CORETEXT_EXPORT CFTypeID CTFontDescriptorGetTypeID(void);

CORETEXT_EXPORT const CFStringRef kCTFontURLAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontNameAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontDisplayNameAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFamilyNameAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontStyleNameAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontTraitsAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxesAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontSizeAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontMatrixAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontCascadeListAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontCharacterSetAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontLanguagesAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontBaselineAdjustAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontMacintoshEncodingsAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFeaturesAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureSettingsAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFixedAdvanceAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontOrientationAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFormatAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontRegistrationScopeAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontPriorityAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontEnabledAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontDownloadableAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontDownloadedAttribute;

typedef uint32_t CTFontOrientation;
enum {
    kCTFontOrientationDefault = 0,
    kCTFontOrientationHorizontal = 1,
    kCTFontOrientationVertical = 2,
    kCTFontDefaultOrientation = kCTFontOrientationDefault,
    kCTFontHorizontalOrientation = kCTFontOrientationHorizontal,
    kCTFontVerticalOrientation = kCTFontOrientationVertical
};

typedef uint32_t CTFontFormat;
enum {
    kCTFontFormatUnrecognized = 0,
    kCTFontFormatOpenTypePostScript = 1,
    kCTFontFormatOpenTypeTrueType = 2,
    kCTFontFormatTrueType = 3,
    kCTFontFormatPostScript = 4,
    kCTFontFormatBitmap = 5
};

enum {
    kCTFontPrioritySystem = 10000,
    kCTFontPriorityNetwork = 20000,
    kCTFontPriorityComputer = 30000,
    kCTFontPriorityUser = 40000,
    kCTFontPriorityDynamic = 50000,
    kCTFontPriorityProcess = 60000
};
typedef uint32_t CTFontPriority;

CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateWithNameAndSize(CFStringRef name, CGFloat size);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateWithAttributes(CFDictionaryRef attributes);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithAttributes(CTFontDescriptorRef original, CFDictionaryRef attributes);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithFamily(CTFontDescriptorRef original, CFStringRef family);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithSymbolicTraits(CTFontDescriptorRef original, CTFontSymbolicTraits symTraitValue, CTFontSymbolicTraits symTraitMask);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithVariation(CTFontDescriptorRef original, CFNumberRef variationIdentifier, CGFloat variationValue);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithFeature(CTFontDescriptorRef original, CFNumberRef featureTypeIdentifier, CFNumberRef featureSelectorIdentifier);
CORETEXT_EXPORT CFArrayRef CTFontDescriptorCreateMatchingFontDescriptors(CTFontDescriptorRef descriptor, CFSetRef mandatoryAttributes);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateMatchingFontDescriptor(CTFontDescriptorRef descriptor, CFSetRef mandatoryAttributes);
CORETEXT_EXPORT CFDictionaryRef CTFontDescriptorCopyAttributes(CTFontDescriptorRef descriptor);
CORETEXT_EXPORT CFTypeRef CTFontDescriptorCopyAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute);
CORETEXT_EXPORT CFTypeRef CTFontDescriptorCopyLocalizedAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute, CFStringRef *language);

#endif
