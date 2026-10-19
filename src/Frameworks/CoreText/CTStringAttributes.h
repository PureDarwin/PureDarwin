// CTStringAttributes.h - attributed string keys CoreText reads
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTSTRINGATTRIBUTES__
#define __CORETEXT_CTSTRINGATTRIBUTES__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreTextExport.h>

CORETEXT_EXPORT const CFStringRef kCTFontAttributeName;
CORETEXT_EXPORT const CFStringRef kCTForegroundColorFromContextAttributeName;
CORETEXT_EXPORT const CFStringRef kCTKernAttributeName;
CORETEXT_EXPORT const CFStringRef kCTTrackingAttributeName;
CORETEXT_EXPORT const CFStringRef kCTLigatureAttributeName;
CORETEXT_EXPORT const CFStringRef kCTForegroundColorAttributeName;
CORETEXT_EXPORT const CFStringRef kCTBackgroundColorAttributeName;
CORETEXT_EXPORT const CFStringRef kCTParagraphStyleAttributeName;
CORETEXT_EXPORT const CFStringRef kCTStrokeWidthAttributeName;
CORETEXT_EXPORT const CFStringRef kCTStrokeColorAttributeName;
CORETEXT_EXPORT const CFStringRef kCTUnderlineStyleAttributeName;
CORETEXT_EXPORT const CFStringRef kCTSuperscriptAttributeName;
CORETEXT_EXPORT const CFStringRef kCTUnderlineColorAttributeName;
CORETEXT_EXPORT const CFStringRef kCTVerticalFormsAttributeName;
CORETEXT_EXPORT const CFStringRef kCTHorizontalInVerticalFormsAttributeName;
CORETEXT_EXPORT const CFStringRef kCTGlyphInfoAttributeName;
CORETEXT_EXPORT const CFStringRef kCTCharacterShapeAttributeName;
CORETEXT_EXPORT const CFStringRef kCTLanguageAttributeName;
CORETEXT_EXPORT const CFStringRef kCTRunDelegateAttributeName;
CORETEXT_EXPORT const CFStringRef kCTBaselineClassAttributeName;
CORETEXT_EXPORT const CFStringRef kCTBaselineInfoAttributeName;
CORETEXT_EXPORT const CFStringRef kCTBaselineReferenceInfoAttributeName;
CORETEXT_EXPORT const CFStringRef kCTBaselineOffsetAttributeName;
CORETEXT_EXPORT const CFStringRef kCTWritingDirectionAttributeName;
CORETEXT_EXPORT const CFStringRef kCTRubyAnnotationAttributeName;

typedef int32_t CTUnderlineStyle;
enum {
    kCTUnderlineStyleNone = 0x00,
    kCTUnderlineStyleSingle = 0x01,
    kCTUnderlineStyleThick = 0x02,
    kCTUnderlineStyleDouble = 0x09
};

typedef int32_t CTUnderlineStyleModifiers;
enum {
    kCTUnderlinePatternSolid = 0x0000,
    kCTUnderlinePatternDot = 0x0100,
    kCTUnderlinePatternDash = 0x0200,
    kCTUnderlinePatternDashDot = 0x0300,
    kCTUnderlinePatternDashDotDot = 0x0400
};

enum {
    kCTWritingDirectionEmbedding = (0 << 1),
    kCTWritingDirectionOverride = (1 << 1)
};

#endif
