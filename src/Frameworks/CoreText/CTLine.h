// CTLine.h - one line of laid out text
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTLINE__
#define __CORETEXT_CTLINE__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGContext.h>
#import <CoreText/CoreTextExport.h>
#import <CoreText/CTRun.h>

typedef const struct __CTLine *CTLineRef;

CORETEXT_EXPORT CFTypeID CTLineGetTypeID(void);

typedef CFOptionFlags CTLineBoundsOptions;
enum {
    kCTLineBoundsExcludeTypographicLeading = 1 << 0,
    kCTLineBoundsExcludeTypographicShifts = 1 << 1,
    kCTLineBoundsUseHangingPunctuation = 1 << 2,
    kCTLineBoundsUseGlyphPathBounds = 1 << 3,
    kCTLineBoundsUseOpticalBounds = 1 << 4,
    kCTLineBoundsIncludeLanguageExtents = 1 << 5
};

typedef uint32_t CTLineTruncationType;
enum {
    kCTLineTruncationStart = 0,
    kCTLineTruncationEnd = 1,
    kCTLineTruncationMiddle = 2
};

CORETEXT_EXPORT CTLineRef CTLineCreateWithAttributedString(CFAttributedStringRef attrString);
CORETEXT_EXPORT CTLineRef CTLineCreateTruncatedLine(CTLineRef line, double width, CTLineTruncationType truncationType, CTLineRef truncationToken);
CORETEXT_EXPORT CTLineRef CTLineCreateJustifiedLine(CTLineRef line, CGFloat justificationFactor, double justificationWidth);
CORETEXT_EXPORT CFIndex CTLineGetGlyphCount(CTLineRef line);
CORETEXT_EXPORT CFArrayRef CTLineGetGlyphRuns(CTLineRef line);
CORETEXT_EXPORT CFRange CTLineGetStringRange(CTLineRef line);
CORETEXT_EXPORT double CTLineGetPenOffsetForFlush(CTLineRef line, CGFloat flushFactor, double flushWidth);
CORETEXT_EXPORT void CTLineDraw(CTLineRef line, CGContextRef context);
CORETEXT_EXPORT double CTLineGetTypographicBounds(CTLineRef line, CGFloat *ascent, CGFloat *descent, CGFloat *leading);
CORETEXT_EXPORT CGRect CTLineGetBoundsWithOptions(CTLineRef line, CTLineBoundsOptions options);
CORETEXT_EXPORT double CTLineGetTrailingWhitespaceWidth(CTLineRef line);
CORETEXT_EXPORT CGRect CTLineGetImageBounds(CTLineRef line, CGContextRef context);
CORETEXT_EXPORT CFIndex CTLineGetStringIndexForPosition(CTLineRef line, CGPoint position);
CORETEXT_EXPORT CGFloat CTLineGetOffsetForStringIndex(CTLineRef line, CFIndex charIndex, CGFloat *secondaryOffset);
#if __BLOCKS__
CORETEXT_EXPORT void CTLineEnumerateCaretOffsets(CTLineRef line, void (^block)(double offset, CFIndex charIndex, bool leadingEdge, bool *stop));
#endif

#endif
