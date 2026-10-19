// CTFrame.m - framesetters fill a rectangular path with lines, top to bottom
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "CTLayout.h"

@interface CTFramesetter : NSObject {
@public
    CTTypesetterRef _typesetter;
    CFAttributedStringRef _string;
}
@end

static CFTypeID framesetterTypeID, frameTypeID;

@implementation CTFramesetter

+ (void)initialize {
    if (self == [CTFramesetter class])
        framesetterTypeID = CTRegisterTypeID("CTFramesetter");
}

- (CFTypeID)_cfTypeID {
    return framesetterTypeID;
}

- (void)dealloc {
    if (_typesetter) CFRelease(_typesetter);
    if (_string) CFRelease(_string);
    [super dealloc];
}

@end

@implementation CTFrame

+ (void)initialize {
    if (self == [CTFrame class])
        frameTypeID = CTRegisterTypeID("CTFrame");
}

- (CFTypeID)_cfTypeID {
    return frameTypeID;
}

- (void)dealloc {
    if (_lines) CFRelease(_lines);
    if (_path) CGPathRelease(_path);
    if (_attributes) CFRelease(_attributes);
    free(_origins);
    [super dealloc];
}

@end

CFTypeID CTFramesetterGetTypeID(void) {
    [CTFramesetter class];
    return framesetterTypeID;
}

CFTypeID CTFrameGetTypeID(void) {
    [CTFrame class];
    return frameTypeID;
}

CTFramesetterRef CTFramesetterCreateWithTypesetter(CTTypesetterRef typesetter) {
    CTFramesetter *f;

    if (typesetter == NULL) return NULL;
    f = [[CTFramesetter alloc] init];
    f->_typesetter = CFRetain(typesetter);
    f->_string = CFRetain(((CTTypesetter *)typesetter)->_string);
    return (CTFramesetterRef)f;
}

CTFramesetterRef CTFramesetterCreateWithAttributedString(CFAttributedStringRef attrString) {
    CTTypesetterRef ts = CTTypesetterCreateWithAttributedString(attrString);
    CTFramesetterRef f = CTFramesetterCreateWithTypesetter(ts);

    if (ts) CFRelease(ts);
    return f;
}

CTTypesetterRef CTFramesetterGetTypesetter(CTFramesetterRef framesetter) {
    return ((CTFramesetter *)framesetter)->_typesetter;
}

static bool startsParagraph(CTTypesetter *ts, CFIndex i) {
    UniChar c;

    if (i == 0) return true;
    c = ts->_text[i - 1];
    return c == '\n' || c == '\r' || c == 0x2029 || c == 0x0085;
}

// lays lines into a box of the given size, origins relative to the box's bottom left
static CFIndex layout(CTFramesetter *f, CFRange range, CGSize box, CFMutableArrayRef lines, CFMutableDataRef origins, double *usedWidth, double *usedHeight) {
    CTTypesetter *ts = (CTTypesetter *)f->_typesetter;
    CFIndex end = range.length == 0 ? ts->_length : range.location + range.length;
    CFIndex i = range.location;
    double y = 0, maxWidth = 0, lastBottom = 0;

    while (i < end) {
        CTParagraphStyleValues style;
        bool first = startsParagraph(ts, i);
        double indent, avail, lineHeight, ascent, descent, leading, spacingBefore;
        CFIndex count;
        CTLineRef line;
        CTLine *l;
        bool truncate;

        CTParagraphStyleValuesFromAttributes(CFAttributedStringGetAttributes(ts->_string, i, NULL), &style);
        indent = first ? style.firstLineHeadIndent : style.headIndent;
        avail = (style.tailIndent > 0 ? style.tailIndent : box.width + style.tailIndent) - indent;
        truncate = style.lineBreakMode >= kCTLineBreakByClipping;
        count = truncate ? 0 : CTTypesetterSuggestLineBreakWithOffset((CTTypesetterRef)ts, i, avail, indent);
        if (truncate) {
            CFIndex k = i;

            // clipping and truncating modes keep each paragraph on one line
            while (k < end && !(ts->_text[k] == '\n' || ts->_text[k] == '\r' || ts->_text[k] == 0x2029)) k++;
            count = (k < end ? k + 1 : k) - i;
        }
        if (count <= 0) count = 1;
        if (i + count > end) count = end - i;
        line = CTTypesetterCreateLineWithOffset((CTTypesetterRef)ts, CFRangeMake(i, count), indent);
        if (line && style.lineBreakMode >= kCTLineBreakByTruncatingHead && ((CTLine *)line)->_width > avail) {
            CTLineTruncationType type = style.lineBreakMode == kCTLineBreakByTruncatingHead ? kCTLineTruncationStart
                                      : style.lineBreakMode == kCTLineBreakByTruncatingMiddle ? kCTLineTruncationMiddle : kCTLineTruncationEnd;
            CFDictionaryRef attrs = CFAttributedStringGetAttributes(ts->_string, i, NULL);
            CFAttributedStringRef ellipsis = CFAttributedStringCreate(NULL, CFSTR("…"), attrs);
            CTLineRef token = CTLineCreateWithAttributedString(ellipsis);
            CTLineRef t = CTLineCreateTruncatedLine(line, avail, type, token);

            if (t) {
                CFRelease(line);
                line = t;
            }
            CFRelease(token);
            CFRelease(ellipsis);
        }
        if (line == NULL) break;
        l = (CTLine *)line;
        ascent = l->_ascent;
        descent = l->_descent;
        leading = l->_leading;
        lineHeight = ascent + descent;
        if (style.lineHeightMultiple > 0) lineHeight *= style.lineHeightMultiple;
        if (style.minimumLineHeight > 0 && lineHeight < style.minimumLineHeight) lineHeight = style.minimumLineHeight;
        if (style.maximumLineHeight > 0 && lineHeight > style.maximumLineHeight) lineHeight = style.maximumLineHeight;
        spacingBefore = (first && i != range.location) ? style.paragraphSpacingBefore : 0;

        double top = y + spacingBefore;
        double baseline = top + lineHeight - descent;
        double bottom = top + lineHeight;

        if (box.height > 0 && bottom > box.height + 0.001 && CFArrayGetCount(lines) > 0) {
            CFRelease(line);
            break;
        }

        double pen = indent;
        double flush = style.alignment == kCTTextAlignmentRight ? 1 : style.alignment == kCTTextAlignmentCenter ? 0.5 : 0;

        // natural alignment follows the paragraph direction
        if (style.alignment == kCTTextAlignmentNatural && (ts->_levels[i] & 1)) flush = 1;
        pen += CTLineGetPenOffsetForFlush(line, flush, avail);
        if (style.alignment == kCTTextAlignmentJustified && i + count < end && !startsParagraph(ts, i + count)) {
            CTLineRef j = CTLineCreateJustifiedLine(line, 1, avail);

            CFRelease(line);
            line = j;
            l = (CTLine *)line;
        }

        CGPoint origin = CGPointMake(pen, box.height - baseline);

        CFArrayAppendValue(lines, line);
        CFDataAppendBytes(origins, (const UInt8 *)&origin, sizeof(origin));
        maxWidth = MAX(maxWidth, pen + l->_width - l->_trailingWhitespace);
        lastBottom = bottom;
        y = bottom + leading + style.lineSpacing;
        if (i + count >= end || startsParagraph(ts, i + count)) y += style.paragraphSpacing;
        CFRelease(line);
        i += count;
    }
    if (usedWidth) *usedWidth = maxWidth;
    if (usedHeight) *usedHeight = lastBottom;
    return i - range.location;
}

CTFrameRef CTFramesetterCreateFrame(CTFramesetterRef framesetter, CFRange stringRange, CGPathRef path, CFDictionaryRef frameAttributes) {
    CTFramesetter *f = (CTFramesetter *)framesetter;
    CGRect box = path ? CGPathGetBoundingBox(path) : CGRectZero;
    CFMutableArrayRef lines = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableDataRef origins = CFDataCreateMutable(NULL, 0);
    CTFrame *frame = [[CTFrame alloc] init];
    CFIndex used;
    CFIndex n;

    if (stringRange.length == 0) stringRange.length = ((CTTypesetter *)f->_typesetter)->_length - stringRange.location;
    used = layout(f, stringRange, box.size, lines, origins, NULL, NULL);
    n = CFArrayGetCount(lines);
    frame->_lines = lines;
    frame->_origins = malloc(sizeof(CGPoint) * (n ? n : 1));
    memcpy(frame->_origins, CFDataGetBytePtr(origins), sizeof(CGPoint) * n);
    // origins are in the path's coordinates
    for (CFIndex i = 0; i < n; i++) {
        frame->_origins[i].x += box.origin.x;
        frame->_origins[i].y += box.origin.y;
    }
    CFRelease(origins);
    frame->_range = stringRange;
    frame->_visible = CFRangeMake(stringRange.location, used);
    frame->_path = path ? CGPathRetain(path) : NULL;
    frame->_attributes = frameAttributes ? CFDictionaryCreateCopy(NULL, frameAttributes) : NULL;
    return (CTFrameRef)frame;
}

CGSize CTFramesetterSuggestFrameSizeWithConstraints(CTFramesetterRef framesetter, CFRange stringRange, CFDictionaryRef frameAttributes, CGSize constraints, CFRange *fitRange) {
    CTFramesetter *f = (CTFramesetter *)framesetter;
    CFMutableArrayRef lines = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableDataRef origins = CFDataCreateMutable(NULL, 0);
    CGSize box = CGSizeMake(constraints.width > 0 && constraints.width < CGFLOAT_MAX / 2 ? constraints.width : 1e7,
                            constraints.height > 0 && constraints.height < CGFLOAT_MAX / 2 ? constraints.height : 0);
    double width, height;
    CFIndex used;

    if (stringRange.length == 0) stringRange.length = ((CTTypesetter *)f->_typesetter)->_length - stringRange.location;
    used = layout(f, stringRange, box, lines, origins, &width, &height);
    CFRelease(lines);
    CFRelease(origins);
    if (fitRange) *fitRange = CFRangeMake(stringRange.location, used);
    return CGSizeMake(ceil(width), ceil(height));
}

CFRange CTFrameGetStringRange(CTFrameRef frame) {
    return ((CTFrame *)frame)->_range;
}

CFRange CTFrameGetVisibleStringRange(CTFrameRef frame) {
    return ((CTFrame *)frame)->_visible;
}

CGPathRef CTFrameGetPath(CTFrameRef frame) {
    return ((CTFrame *)frame)->_path;
}

CFDictionaryRef CTFrameGetFrameAttributes(CTFrameRef frame) {
    return ((CTFrame *)frame)->_attributes;
}

CFArrayRef CTFrameGetLines(CTFrameRef frame) {
    return ((CTFrame *)frame)->_lines;
}

void CTFrameGetLineOrigins(CTFrameRef frame, CFRange range, CGPoint *origins) {
    CTFrame *f = (CTFrame *)frame;
    CFIndex n = CFArrayGetCount(f->_lines);

    if (range.length == 0) range.length = n - range.location;
    if (range.location < 0 || range.location + range.length > n) return;
    memcpy(origins, f->_origins + range.location, sizeof(CGPoint) * range.length);
}

void CTFrameDraw(CTFrameRef frame, CGContextRef context) {
    CTFrame *f = (CTFrame *)frame;

    for (CFIndex i = 0; i < CFArrayGetCount(f->_lines); i++) {
        CGContextSetTextPosition(context, f->_origins[i].x, f->_origins[i].y);
        CTLineDraw(CFArrayGetValueAtIndex(f->_lines, i), context);
    }
}
