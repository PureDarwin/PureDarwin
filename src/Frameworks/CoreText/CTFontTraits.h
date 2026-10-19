// CTFontTraits.h - font trait keys and symbolic traits
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTFONTTRAITS__
#define __CORETEXT_CTFONTTRAITS__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreTextExport.h>

CORETEXT_EXPORT const CFStringRef kCTFontSymbolicTrait;
CORETEXT_EXPORT const CFStringRef kCTFontWeightTrait;
CORETEXT_EXPORT const CFStringRef kCTFontWidthTrait;
CORETEXT_EXPORT const CFStringRef kCTFontSlantTrait;

enum {
    kCTFontClassMaskShift = 28
};

typedef uint32_t CTFontSymbolicTraits;
enum {
    kCTFontTraitItalic = (1 << 0),
    kCTFontTraitBold = (1 << 1),
    kCTFontTraitExpanded = (1 << 5),
    kCTFontTraitCondensed = (1 << 6),
    kCTFontTraitMonoSpace = (1 << 10),
    kCTFontTraitVertical = (1 << 11),
    kCTFontTraitUIOptimized = (1 << 12),
    kCTFontTraitColorGlyphs = (1 << 13),
    kCTFontTraitComposite = (1 << 14),
    kCTFontTraitClassMask = (15U << kCTFontClassMaskShift),

    kCTFontItalicTrait = kCTFontTraitItalic,
    kCTFontBoldTrait = kCTFontTraitBold,
    kCTFontExpandedTrait = kCTFontTraitExpanded,
    kCTFontCondensedTrait = kCTFontTraitCondensed,
    kCTFontMonoSpaceTrait = kCTFontTraitMonoSpace,
    kCTFontVerticalTrait = kCTFontTraitVertical,
    kCTFontUIOptimizedTrait = kCTFontTraitUIOptimized,
    kCTFontColorGlyphsTrait = kCTFontTraitColorGlyphs,
    kCTFontCompositeTrait = kCTFontTraitComposite,
    kCTFontClassMaskTrait = kCTFontTraitClassMask
};

typedef uint32_t CTFontStylisticClass;
enum {
    kCTFontClassUnknown = (0 << kCTFontClassMaskShift),
    kCTFontClassOldStyleSerifs = (1 << kCTFontClassMaskShift),
    kCTFontClassTransitionalSerifs = (2 << kCTFontClassMaskShift),
    kCTFontClassModernSerifs = (3 << kCTFontClassMaskShift),
    kCTFontClassClarendonSerifs = (4 << kCTFontClassMaskShift),
    kCTFontClassSlabSerifs = (5 << kCTFontClassMaskShift),
    kCTFontClassFreeformSerifs = (7 << kCTFontClassMaskShift),
    kCTFontClassSansSerif = (8U << kCTFontClassMaskShift),
    kCTFontClassOrnamentals = (9U << kCTFontClassMaskShift),
    kCTFontClassScripts = (10U << kCTFontClassMaskShift),
    kCTFontClassSymbolic = (12U << kCTFontClassMaskShift),

    kCTFontUnknownClass = kCTFontClassUnknown,
    kCTFontOldStyleSerifsClass = kCTFontClassOldStyleSerifs,
    kCTFontTransitionalSerifsClass = kCTFontClassTransitionalSerifs,
    kCTFontModernSerifsClass = kCTFontClassModernSerifs,
    kCTFontClarendonSerifsClass = kCTFontClassClarendonSerifs,
    kCTFontSlabSerifsClass = kCTFontClassSlabSerifs,
    kCTFontFreeformSerifsClass = kCTFontClassFreeformSerifs,
    kCTFontSansSerifClass = kCTFontClassSansSerif,
    kCTFontOrnamentalsClass = kCTFontClassOrnamentals,
    kCTFontScriptsClass = kCTFontClassScripts,
    kCTFontSymbolicClass = kCTFontClassSymbolic
};

// the standard weights, in CoreText's -1.0 to 1.0 range
CORETEXT_EXPORT const CGFloat kCTFontWeightUltraLight;
CORETEXT_EXPORT const CGFloat kCTFontWeightThin;
CORETEXT_EXPORT const CGFloat kCTFontWeightLight;
CORETEXT_EXPORT const CGFloat kCTFontWeightRegular;
CORETEXT_EXPORT const CGFloat kCTFontWeightMedium;
CORETEXT_EXPORT const CGFloat kCTFontWeightSemibold;
CORETEXT_EXPORT const CGFloat kCTFontWeightBold;
CORETEXT_EXPORT const CGFloat kCTFontWeightHeavy;
CORETEXT_EXPORT const CGFloat kCTFontWeightBlack;

#endif
