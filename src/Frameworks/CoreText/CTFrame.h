// CTFrame.h - a laid out frame of lines
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTFRAME__
#define __CORETEXT_CTFRAME__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGContext.h>
#import <CoreGraphics/CGPath.h>
#import <CoreText/CoreTextExport.h>

typedef const struct __CTFrame *CTFrameRef;

CORETEXT_EXPORT CFTypeID CTFrameGetTypeID(void);

typedef uint32_t CTFrameProgression;
enum {
    kCTFrameProgressionTopToBottom = 0,
    kCTFrameProgressionRightToLeft = 1,
    kCTFrameProgressionLeftToRight = 2
};

typedef uint32_t CTFramePathFillRule;
enum {
    kCTFramePathFillEvenOdd = 0,
    kCTFramePathFillWindingNumber = 1
};

CORETEXT_EXPORT const CFStringRef kCTFrameProgressionAttributeName;
CORETEXT_EXPORT const CFStringRef kCTFramePathFillRuleAttributeName;
CORETEXT_EXPORT const CFStringRef kCTFramePathWidthAttributeName;
CORETEXT_EXPORT const CFStringRef kCTFrameClippingPathsAttributeName;
CORETEXT_EXPORT const CFStringRef kCTFramePathClippingPathAttributeName;

CORETEXT_EXPORT CFRange CTFrameGetStringRange(CTFrameRef frame);
CORETEXT_EXPORT CFRange CTFrameGetVisibleStringRange(CTFrameRef frame);
CORETEXT_EXPORT CGPathRef CTFrameGetPath(CTFrameRef frame);
CORETEXT_EXPORT CFDictionaryRef CTFrameGetFrameAttributes(CTFrameRef frame);
CORETEXT_EXPORT CFArrayRef CTFrameGetLines(CTFrameRef frame);
CORETEXT_EXPORT void CTFrameGetLineOrigins(CTFrameRef frame, CFRange range, CGPoint *origins);
CORETEXT_EXPORT void CTFrameDraw(CTFrameRef frame, CGContextRef context);

#endif
