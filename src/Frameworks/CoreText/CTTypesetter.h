// CTTypesetter.h - line breaking and line creation over an attributed string
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTTYPESETTER__
#define __CORETEXT_CTTYPESETTER__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreTextExport.h>
#import <CoreText/CTLine.h>

typedef const struct __CTTypesetter *CTTypesetterRef;

CORETEXT_EXPORT CFTypeID CTTypesetterGetTypeID(void);

CORETEXT_EXPORT const CFStringRef kCTTypesetterOptionAllowUnboundedLayout;
CORETEXT_EXPORT const CFStringRef kCTTypesetterOptionDisableBidiProcessing;
CORETEXT_EXPORT const CFStringRef kCTTypesetterOptionForcedEmbeddingLevel;

CORETEXT_EXPORT CTTypesetterRef CTTypesetterCreateWithAttributedString(CFAttributedStringRef string);
CORETEXT_EXPORT CTTypesetterRef CTTypesetterCreateWithAttributedStringAndOptions(CFAttributedStringRef string, CFDictionaryRef options);
CORETEXT_EXPORT CTLineRef CTTypesetterCreateLine(CTTypesetterRef typesetter, CFRange stringRange);
CORETEXT_EXPORT CTLineRef CTTypesetterCreateLineWithOffset(CTTypesetterRef typesetter, CFRange stringRange, double offset);
CORETEXT_EXPORT CFIndex CTTypesetterSuggestLineBreak(CTTypesetterRef typesetter, CFIndex startIndex, double width);
CORETEXT_EXPORT CFIndex CTTypesetterSuggestLineBreakWithOffset(CTTypesetterRef typesetter, CFIndex startIndex, double width, double offset);
CORETEXT_EXPORT CFIndex CTTypesetterSuggestClusterBreak(CTTypesetterRef typesetter, CFIndex startIndex, double width);
CORETEXT_EXPORT CFIndex CTTypesetterSuggestClusterBreakWithOffset(CTTypesetterRef typesetter, CFIndex startIndex, double width, double offset);

#endif
