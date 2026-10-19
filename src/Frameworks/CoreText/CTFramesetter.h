// CTFramesetter.h - lines laid into a path
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTFRAMESETTER__
#define __CORETEXT_CTFRAMESETTER__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGPath.h>
#import <CoreText/CoreTextExport.h>
#import <CoreText/CTFrame.h>
#import <CoreText/CTTypesetter.h>

typedef const struct __CTFramesetter *CTFramesetterRef;

CORETEXT_EXPORT CFTypeID CTFramesetterGetTypeID(void);

CORETEXT_EXPORT CTFramesetterRef CTFramesetterCreateWithTypesetter(CTTypesetterRef typesetter);
CORETEXT_EXPORT CTFramesetterRef CTFramesetterCreateWithAttributedString(CFAttributedStringRef attrString);
CORETEXT_EXPORT CTFrameRef CTFramesetterCreateFrame(CTFramesetterRef framesetter, CFRange stringRange, CGPathRef path, CFDictionaryRef frameAttributes);
CORETEXT_EXPORT CTTypesetterRef CTFramesetterGetTypesetter(CTFramesetterRef framesetter);
CORETEXT_EXPORT CGSize CTFramesetterSuggestFrameSizeWithConstraints(CTFramesetterRef framesetter, CFRange stringRange, CFDictionaryRef frameAttributes, CGSize constraints, CFRange *fitRange);

#endif
