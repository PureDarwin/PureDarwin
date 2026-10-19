// CTRun.h - glyph runs, one font and one direction each
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTRUN__
#define __CORETEXT_CTRUN__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGContext.h>
#import <CoreGraphics/CGFont.h>
#import <CoreText/CoreTextExport.h>

typedef const struct __CTRun *CTRunRef;

CORETEXT_EXPORT CFTypeID CTRunGetTypeID(void);

typedef uint32_t CTRunStatus;
enum {
    kCTRunStatusNoStatus = 0,
    kCTRunStatusRightToLeft = (1 << 0),
    kCTRunStatusNonMonotonic = (1 << 1),
    kCTRunStatusHasNonIdentityMatrix = (1 << 2)
};

CORETEXT_EXPORT CFIndex CTRunGetGlyphCount(CTRunRef run);
CORETEXT_EXPORT CFDictionaryRef CTRunGetAttributes(CTRunRef run);
CORETEXT_EXPORT CTRunStatus CTRunGetStatus(CTRunRef run);
CORETEXT_EXPORT const CGGlyph *CTRunGetGlyphsPtr(CTRunRef run);
CORETEXT_EXPORT void CTRunGetGlyphs(CTRunRef run, CFRange range, CGGlyph *buffer);
CORETEXT_EXPORT const CGPoint *CTRunGetPositionsPtr(CTRunRef run);
CORETEXT_EXPORT void CTRunGetPositions(CTRunRef run, CFRange range, CGPoint *buffer);
CORETEXT_EXPORT const CGSize *CTRunGetAdvancesPtr(CTRunRef run);
CORETEXT_EXPORT void CTRunGetAdvances(CTRunRef run, CFRange range, CGSize *buffer);
CORETEXT_EXPORT const CFIndex *CTRunGetStringIndicesPtr(CTRunRef run);
CORETEXT_EXPORT void CTRunGetStringIndices(CTRunRef run, CFRange range, CFIndex *buffer);
CORETEXT_EXPORT CFRange CTRunGetStringRange(CTRunRef run);
CORETEXT_EXPORT double CTRunGetTypographicBounds(CTRunRef run, CFRange range, CGFloat *ascent, CGFloat *descent, CGFloat *leading);
CORETEXT_EXPORT CGRect CTRunGetImageBounds(CTRunRef run, CGContextRef context, CFRange range);
CORETEXT_EXPORT CGAffineTransform CTRunGetTextMatrix(CTRunRef run);
CORETEXT_EXPORT void CTRunGetBaseAdvancesAndOrigins(CTRunRef run, CFRange range, CGSize *advancesBuffer, CGPoint *originsBuffer);
CORETEXT_EXPORT void CTRunDraw(CTRunRef run, CGContextRef context, CFRange range);

#endif
