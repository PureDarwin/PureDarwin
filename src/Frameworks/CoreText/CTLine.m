// CTLine.m - glyph runs and lines: metrics, carets, truncation, justification, drawing
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "CTLayout.h"

static CFTypeID runTypeID, lineTypeID;

@implementation CTRun

+ (void)initialize {
    if (self == [CTRun class])
        runTypeID = CTRegisterTypeID("CTRun");
}

- (CFTypeID)_cfTypeID {
    return runTypeID;
}

- (void)dealloc {
    if (_attributes) CFRelease(_attributes);
    if (_font) CFRelease(_font);
    free(_glyphs);
    free(_positions);
    free(_advances);
    free(_indices);
    [super dealloc];
}

@end

@implementation CTLine

+ (void)initialize {
    if (self == [CTLine class])
        lineTypeID = CTRegisterTypeID("CTLine");
}

- (CFTypeID)_cfTypeID {
    return lineTypeID;
}

- (void)dealloc {
    if (_runs) CFRelease(_runs);
    if (_string) CFRelease(_string);
    [super dealloc];
}

@end

CFTypeID CTRunGetTypeID(void) {
    [CTRun class];
    return runTypeID;
}

CFTypeID CTLineGetTypeID(void) {
    [CTLine class];
    return lineTypeID;
}

CTRun *CTRunCreate(CFDictionaryRef attributes, CTFontRef font, CFIndex count) {
    CTRun *run = [[CTRun alloc] init];
    CFIndex n = count > 0 ? count : 1;

    run->_attributes = attributes ? CFDictionaryCreateCopy(NULL, attributes) : NULL;
    run->_font = font ? CFRetain(font) : NULL;
    run->_count = count;
    run->_glyphs = calloc(n, sizeof(CGGlyph));
    run->_positions = calloc(n, sizeof(CGPoint));
    run->_advances = calloc(n, sizeof(CGSize));
    run->_indices = calloc(n, sizeof(CFIndex));
    return run;
}

static CTRun *copyRun(CTRun *r) {
    CTRun *run = CTRunCreate(r->_attributes, r->_font, r->_count);

    memcpy(run->_glyphs, r->_glyphs, r->_count * sizeof(CGGlyph));
    memcpy(run->_positions, r->_positions, r->_count * sizeof(CGPoint));
    memcpy(run->_advances, r->_advances, r->_count * sizeof(CGSize));
    memcpy(run->_indices, r->_indices, r->_count * sizeof(CFIndex));
    run->_range = r->_range;
    run->_status = r->_status;
    run->_ascent = r->_ascent;
    run->_descent = r->_descent;
    run->_leading = r->_leading;
    run->_width = r->_width;
    run->_level = r->_level;
    return run;
}

#pragma mark runs

static CFRange clampRange(CTRun *run, CFRange range) {
    if (range.length == 0) range = CFRangeMake(range.location, run->_count - range.location);
    if (range.location < 0) range.location = 0;
    if (range.location + range.length > run->_count) range.length = run->_count - range.location;
    if (range.length < 0) range.length = 0;
    return range;
}

CFIndex CTRunGetGlyphCount(CTRunRef run) {
    return ((CTRun *)run)->_count;
}

CFDictionaryRef CTRunGetAttributes(CTRunRef run) {
    return ((CTRun *)run)->_attributes;
}

CTRunStatus CTRunGetStatus(CTRunRef run) {
    return ((CTRun *)run)->_status;
}

const CGGlyph *CTRunGetGlyphsPtr(CTRunRef run) {
    return ((CTRun *)run)->_glyphs;
}

void CTRunGetGlyphs(CTRunRef run, CFRange range, CGGlyph *buffer) {
    CTRun *r = (CTRun *)run;

    range = clampRange(r, range);
    memcpy(buffer, r->_glyphs + range.location, range.length * sizeof(CGGlyph));
}

const CGPoint *CTRunGetPositionsPtr(CTRunRef run) {
    return ((CTRun *)run)->_positions;
}

void CTRunGetPositions(CTRunRef run, CFRange range, CGPoint *buffer) {
    CTRun *r = (CTRun *)run;

    range = clampRange(r, range);
    memcpy(buffer, r->_positions + range.location, range.length * sizeof(CGPoint));
}

const CGSize *CTRunGetAdvancesPtr(CTRunRef run) {
    return ((CTRun *)run)->_advances;
}

void CTRunGetAdvances(CTRunRef run, CFRange range, CGSize *buffer) {
    CTRun *r = (CTRun *)run;

    range = clampRange(r, range);
    memcpy(buffer, r->_advances + range.location, range.length * sizeof(CGSize));
}

const CFIndex *CTRunGetStringIndicesPtr(CTRunRef run) {
    return ((CTRun *)run)->_indices;
}

void CTRunGetStringIndices(CTRunRef run, CFRange range, CFIndex *buffer) {
    CTRun *r = (CTRun *)run;

    range = clampRange(r, range);
    memcpy(buffer, r->_indices + range.location, range.length * sizeof(CFIndex));
}

CFRange CTRunGetStringRange(CTRunRef run) {
    return ((CTRun *)run)->_range;
}

double CTRunGetTypographicBounds(CTRunRef run, CFRange range, CGFloat *ascent, CGFloat *descent, CGFloat *leading) {
    CTRun *r = (CTRun *)run;
    double width = 0;

    range = clampRange(r, range);
    for (CFIndex i = range.location; i < range.location + range.length; i++)
        width += r->_advances[i].width;
    if (ascent) *ascent = r->_ascent;
    if (descent) *descent = r->_descent;
    if (leading) *leading = r->_leading;
    return width;
}

CGRect CTRunGetImageBounds(CTRunRef run, CGContextRef context, CFRange range) {
    CTRun *r = (CTRun *)run;
    CGRect total = CGRectNull;

    range = clampRange(r, range);
    if (range.length == 0 || r->_font == NULL) return CGRectNull;

    CGRect rects[range.length];

    CTFontGetBoundingRectsForGlyphs(r->_font, kCTFontOrientationHorizontal, r->_glyphs + range.location, rects, range.length);
    for (CFIndex i = 0; i < range.length; i++) {
        CGRect g = CGRectOffset(rects[i], r->_positions[range.location + i].x, r->_positions[range.location + i].y);

        if (rects[i].size.width > 0 || rects[i].size.height > 0)
            total = CGRectIsNull(total) ? g : CGRectUnion(total, g);
    }
    if (!CGRectIsNull(total) && context) {
        CGPoint p = CGContextGetTextPosition(context);

        total = CGRectOffset(total, p.x, p.y);
    }
    return total;
}

CGAffineTransform CTRunGetTextMatrix(CTRunRef run) {
    return CGAffineTransformIdentity;
}

void CTRunGetBaseAdvancesAndOrigins(CTRunRef run, CFRange range, CGSize *advancesBuffer, CGPoint *originsBuffer) {
    CTRun *r = (CTRun *)run;

    range = clampRange(r, range);
    for (CFIndex i = 0; i < range.length; i++) {
        CFIndex g = range.location + i;

        if (advancesBuffer) advancesBuffer[i] = r->_advances[g];
        if (originsBuffer) originsBuffer[i] = CGPointMake(r->_positions[g].x - (g > 0 ? r->_positions[g - 1].x + r->_advances[g - 1].width : r->_positions[0].x), r->_positions[g].y);
    }
}

static bool colorFromContext(CFDictionaryRef attrs) {
    CFBooleanRef b = attrs ? CFDictionaryGetValue(attrs, kCTForegroundColorFromContextAttributeName) : NULL;

    return b && CFGetTypeID(b) == CFBooleanGetTypeID() && CFBooleanGetValue(b);
}

// a CGColor, or an NSColor that can hand one over
static CGColorRef colorAttribute(CFDictionaryRef attrs, CFStringRef key) {
    id value = attrs ? (id)CFDictionaryGetValue(attrs, key) : nil;

    if (value == nil) return NULL;
    if ([value respondsToSelector:@selector(CGColor)]) return (CGColorRef)[value CGColor];
    return (CGColorRef)value;
}

static void drawRun(CTRun *r, CGContextRef context, CFRange range, CGPoint origin) {
    CGColorRef color = colorAttribute(r->_attributes, kCTForegroundColorAttributeName);
    CFNumberRef underline = r->_attributes ? CFDictionaryGetValue(r->_attributes, kCTUnderlineStyleAttributeName) : NULL;
    CFNumberRef strokeWidth = r->_attributes ? CFDictionaryGetValue(r->_attributes, kCTStrokeWidthAttributeName) : NULL;
    int32_t underlineStyle = 0;
    double stroke = 0;

    range = clampRange(r, range);
    if (range.length == 0 || r->_font == NULL || CFDictionaryGetValue(r->_attributes, kCTRunDelegateAttributeName)) return;

    CGPoint positions[range.length];

    for (CFIndex i = 0; i < range.length; i++)
        positions[i] = CGPointMake(origin.x + r->_positions[range.location + i].x, origin.y + r->_positions[range.location + i].y);
    CGContextSaveGState(context);
    if (color && !colorFromContext(r->_attributes)) CGContextSetFillColorWithColor(context, color);
    if (strokeWidth) CFNumberGetValue(strokeWidth, kCFNumberDoubleType, &stroke);
    // positive stroke widths outline only (mode 1), negative ones fill and outline (mode 2), in percent of the size
    if (stroke != 0) {
        CGColorRef strokeColor = colorAttribute(r->_attributes, kCTStrokeColorAttributeName);

        if (strokeColor) CGContextSetStrokeColorWithColor(context, strokeColor);
        else if (color) CGContextSetStrokeColorWithColor(context, color);
        CGContextSetLineWidth(context, fabs(stroke) * CTFontGetSize(r->_font) / 100);
        CGContextSetTextDrawingMode(context, stroke > 0 ? 1 : 2);
    }
    CTFontDrawGlyphs(r->_font, r->_glyphs + range.location, positions, range.length, context);
    if (underline) CFNumberGetValue(underline, kCFNumberSInt32Type, &underlineStyle);
    if (underlineStyle & 0xff) {
        CGColorRef ucolor = colorAttribute(r->_attributes, kCTUnderlineColorAttributeName);
        CGFloat thickness = CTFontGetUnderlineThickness(r->_font);
        CGFloat y = origin.y + CTFontGetUnderlinePosition(r->_font);
        double start = r->_positions[range.location].x, end = start;

        for (CFIndex i = range.location; i < range.location + range.length; i++)
            end = MAX(end, r->_positions[i].x + r->_advances[i].width);
        if (thickness <= 0) thickness = 1;
        if ((underlineStyle & 0xff) == 0x02) thickness *= 2;
        if (ucolor) CGContextSetFillColorWithColor(context, ucolor);
        CGContextFillRect(context, CGRectMake(origin.x + start, y - thickness / 2, end - start, thickness));
        if ((underlineStyle & 0xff) == 0x09)
            CGContextFillRect(context, CGRectMake(origin.x + start, y - thickness * 2.5, end - start, thickness));
    }
    CGContextRestoreGState(context);
}

void CTRunDraw(CTRunRef run, CGContextRef context, CFRange range) {
    drawRun((CTRun *)run, context, range, CGContextGetTextPosition(context));
}

#pragma mark lines

void CTLineFinish(CTLine *line) {
    CFIndex n = CFArrayGetCount(line->_runs);

    line->_width = 0;
    line->_ascent = line->_descent = line->_leading = 0;
    for (CFIndex i = 0; i < n; i++) {
        CTRun *run = (CTRun *)CFArrayGetValueAtIndex(line->_runs, i);

        line->_width += run->_width;
        line->_ascent = MAX(line->_ascent, run->_ascent);
        line->_descent = MAX(line->_descent, run->_descent);
        line->_leading = MAX(line->_leading, run->_leading);
    }
    // a line with no runs still has the default font's height
    if (n == 0) {
        CTFontRef font = CTCopyDefaultFont();

        if (font) {
            line->_ascent = CTFontGetAscent(font);
            line->_descent = CTFontGetDescent(font);
            line->_leading = CTFontGetLeading(font);
            CFRelease(font);
        }
    }

    // white space at the logical end of the line
    CFStringRef s = line->_string ? CFAttributedStringGetString(line->_string) : NULL;

    line->_trailingWhitespace = 0;
    for (CFIndex k = line->_range.location + line->_range.length - 1; s && k >= line->_range.location; k--) {
        UniChar c = CFStringGetCharacterAtIndex(s, k);
        bool found = false;

        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x00A0 || c == 0x2028 || c == 0x2029 || c == 0x3000)) break;
        for (CFIndex i = 0; i < n && !found; i++) {
            CTRun *run = (CTRun *)CFArrayGetValueAtIndex(line->_runs, i);

            for (CFIndex g = 0; g < run->_count; g++) {
                if (run->_indices[g] == k) {
                    line->_trailingWhitespace += run->_advances[g].width;
                    found = true;
                }
            }
        }
    }
}

CTLineRef CTLineCreateWithRuns(CFArrayRef runs, CFRange range, CFAttributedStringRef string) {
    CTLine *line = [[CTLine alloc] init];

    line->_runs = CFArrayCreateMutableCopy(NULL, 0, runs);
    line->_range = range;
    line->_string = string ? CFRetain(string) : NULL;
    CTLineFinish(line);
    return (CTLineRef)line;
}

CTLineRef CTLineCreateWithAttributedString(CFAttributedStringRef string) {
    CTTypesetterRef ts = CTTypesetterCreateWithAttributedString(string);
    CTLineRef line;

    if (ts == NULL) return NULL;
    line = CTTypesetterCreateLine(ts, CFRangeMake(0, CFAttributedStringGetLength(string)));
    CFRelease(ts);
    return line;
}

CFIndex CTLineGetGlyphCount(CTLineRef line) {
    CTLine *l = (CTLine *)line;
    CFIndex count = 0;

    for (CFIndex i = 0; i < CFArrayGetCount(l->_runs); i++)
        count += ((CTRun *)CFArrayGetValueAtIndex(l->_runs, i))->_count;
    return count;
}

CFArrayRef CTLineGetGlyphRuns(CTLineRef line) {
    return ((CTLine *)line)->_runs;
}

CFRange CTLineGetStringRange(CTLineRef line) {
    return ((CTLine *)line)->_range;
}

double CTLineGetPenOffsetForFlush(CTLineRef line, CGFloat flushFactor, double flushWidth) {
    CTLine *l = (CTLine *)line;
    double width = l->_width - l->_trailingWhitespace;

    if (flushFactor <= 0 || width >= flushWidth) return 0;
    if (flushFactor > 1) flushFactor = 1;
    return (flushWidth - width) * flushFactor;
}

void CTLineDraw(CTLineRef line, CGContextRef context) {
    CTLine *l = (CTLine *)line;
    CGPoint origin = CGContextGetTextPosition(context);

    for (CFIndex i = 0; i < CFArrayGetCount(l->_runs); i++) {
        CTRun *run = (CTRun *)CFArrayGetValueAtIndex(l->_runs, i);
        CGColorRef background = colorAttribute(run->_attributes, kCTBackgroundColorAttributeName);

        if (background) {
            CGFloat left = run->_count ? run->_positions[0].x : 0;

            CGContextSaveGState(context);
            CGContextSetFillColorWithColor(context, background);
            CGContextFillRect(context, CGRectMake(origin.x + left, origin.y - l->_descent, run->_width, l->_ascent + l->_descent));
            CGContextRestoreGState(context);
        }
        drawRun(run, context, CFRangeMake(0, 0), origin);
    }
    CGContextSetTextPosition(context, origin.x + l->_width, origin.y);
}

double CTLineGetTypographicBounds(CTLineRef line, CGFloat *ascent, CGFloat *descent, CGFloat *leading) {
    CTLine *l = (CTLine *)line;

    if (ascent) *ascent = l->_ascent;
    if (descent) *descent = l->_descent;
    if (leading) *leading = l->_leading;
    return l->_width;
}

CGRect CTLineGetBoundsWithOptions(CTLineRef line, CTLineBoundsOptions options) {
    CTLine *l = (CTLine *)line;
    CGRect bounds;

    if (options & kCTLineBoundsUseGlyphPathBounds) {
        bounds = CTLineGetImageBounds(line, NULL);
        return CGRectIsNull(bounds) ? CGRectZero : bounds;
    }
    bounds = CGRectMake(0, -l->_descent, l->_width, l->_ascent + l->_descent);
    if (!(options & kCTLineBoundsExcludeTypographicLeading))
        bounds = CGRectMake(0, -l->_descent - l->_leading, l->_width, l->_ascent + l->_descent + l->_leading);
    return bounds;
}

double CTLineGetTrailingWhitespaceWidth(CTLineRef line) {
    return ((CTLine *)line)->_trailingWhitespace;
}

CGRect CTLineGetImageBounds(CTLineRef line, CGContextRef context) {
    CTLine *l = (CTLine *)line;
    CGRect total = CGRectNull;

    for (CFIndex i = 0; i < CFArrayGetCount(l->_runs); i++) {
        CGRect r = CTRunGetImageBounds(CFArrayGetValueAtIndex(l->_runs, i), context, CFRangeMake(0, 0));

        if (!CGRectIsNull(r)) total = CGRectIsNull(total) ? r : CGRectUnion(total, r);
    }
    return total;
}

#pragma mark carets

// each glyph's caret span: x from left to right, string index and direction
typedef struct {
    double left, right;
    CFIndex index;
    bool rtl;
} Caret;

static CFIndex collectCarets(CTLine *l, Caret **out) {
    CFIndex count = CTLineGetGlyphCount((CTLineRef)l), k = 0;
    Caret *carets = malloc(sizeof(Caret) * (count ? count : 1));

    for (CFIndex i = 0; i < CFArrayGetCount(l->_runs); i++) {
        CTRun *run = (CTRun *)CFArrayGetValueAtIndex(l->_runs, i);
        double pen = run->_count ? run->_positions[0].x : 0;

        for (CFIndex g = 0; g < run->_count; g++) {
            carets[k].left = pen;
            carets[k].right = pen + run->_advances[g].width;
            carets[k].index = run->_indices[g];
            carets[k].rtl = (run->_status & kCTRunStatusRightToLeft) != 0;
            pen = carets[k].right;
            k++;
        }
    }
    *out = carets;
    return k;
}

// the characters a glyph stands for, up to the next glyph's index or the line end
static CFIndex spanEnd(CTLine *l, Caret *carets, CFIndex n, CFIndex i) {
    CFIndex end = l->_range.location + l->_range.length;

    for (CFIndex j = 0; j < n; j++)
        if (carets[j].index > carets[i].index && carets[j].index < end) end = carets[j].index;
    return end;
}

// a ligature's carets split its width evenly between the characters it covers
CFIndex CTLineGetStringIndexForPosition(CTLineRef line, CGPoint position) {
    CTLine *l = (CTLine *)line;
    Caret *carets;
    CFIndex n = collectCarets(l, &carets);
    CFIndex lineEnd = l->_range.location + l->_range.length, result = lineEnd;

    if (n == 0) {
        free(carets);
        return l->_range.location;
    }
    if (position.x < carets[0].left) {
        result = carets[0].rtl ? spanEnd(l, carets, n, 0) : carets[0].index;
    } else {
        for (CFIndex i = 0; i < n; i++) {
            double width = carets[i].right - carets[i].left;
            CFIndex end, count;
            double fraction;
            CFIndex step;

            if (!(position.x >= carets[i].left && position.x < carets[i].right) || width <= 0) continue;
            end = spanEnd(l, carets, n, i);
            count = end > carets[i].index ? end - carets[i].index : 1;
            fraction = (position.x - carets[i].left) / width;
            if (carets[i].rtl) fraction = 1 - fraction;
            step = (CFIndex)floor(fraction * count + 0.5);
            result = carets[i].index + step;
            break;
        }
        if (position.x >= carets[n - 1].right && carets[n - 1].rtl) result = carets[n - 1].index;
    }
    if (result > lineEnd) result = lineEnd;
    free(carets);
    return result;
}

CGFloat CTLineGetOffsetForStringIndex(CTLineRef line, CFIndex charIndex, CGFloat *secondaryOffset) {
    CTLine *l = (CTLine *)line;
    Caret *carets;
    CFIndex n = collectCarets(l, &carets);
    CFIndex lineEnd = l->_range.location + l->_range.length;
    CGFloat offset;
    CFIndex best = -1;

    for (CFIndex i = 0; i < n; i++) {
        if (carets[i].index > charIndex) continue;
        if (best < 0 || carets[i].index > carets[best].index) best = i;
    }
    if (charIndex >= lineEnd || best < 0) {
        bool rtlEnd = n > 0 && carets[n - 1].rtl;

        offset = charIndex >= lineEnd ? (rtlEnd ? 0 : l->_width) : (n > 0 && carets[0].rtl ? l->_width : 0);
    } else {
        CFIndex end = spanEnd(l, carets, n, best);
        CFIndex count = end > carets[best].index ? end - carets[best].index : 1;
        double part = (carets[best].right - carets[best].left) * (double)(charIndex - carets[best].index) / count;

        offset = carets[best].rtl ? carets[best].right - part : carets[best].left + part;
    }
    if (secondaryOffset) *secondaryOffset = offset;
    free(carets);
    return offset;
}

void CTLineEnumerateCaretOffsets(CTLineRef line, void (^block)(double offset, CFIndex charIndex, bool leadingEdge, bool *stop)) {
    CTLine *l = (CTLine *)line;
    Caret *carets;
    CFIndex n = collectCarets(l, &carets);
    bool stop = false;

    for (CFIndex i = 0; i < n && !stop; i++) {
        block(carets[i].rtl ? carets[i].right : carets[i].left, carets[i].index, true, &stop);
        if (!stop) block(carets[i].rtl ? carets[i].left : carets[i].right, carets[i].index, false, &stop);
    }
    free(carets);
}

#pragma mark truncation and justification

static CFAttributedStringRef tokenString(CTLine *token) {
    if (token && token->_string)
        return CFAttributedStringCreateWithSubstring(NULL, token->_string, token->_range);
    return NULL;
}

// the longest kept text plus the token that fits, keeping whole clusters
CTLineRef CTLineCreateTruncatedLine(CTLineRef line, double width, CTLineTruncationType truncationType, CTLineRef truncationToken) {
    CTLine *l = (CTLine *)line;
    CTLine *token = (CTLine *)truncationToken;
    double tokenWidth = token ? token->_width : 0;
    CFAttributedStringRef source = l->_string, tok;
    CFIndex length = l->_range.length, keepStart = 0, keepEnd = 0;
    CFMutableAttributedStringRef result;
    CTLineRef truncated;
    Caret *carets;
    CFIndex n;
    double *charWidth;

    if (l->_width - l->_trailingWhitespace <= width) return CFRetain(line);
    if (source == NULL || tokenWidth > width) return NULL;
    n = collectCarets(l, &carets);
    charWidth = calloc(length ? length : 1, sizeof(double));
    for (CFIndex i = 0; i < n; i++)
        charWidth[carets[i].index - l->_range.location] += carets[i].right - carets[i].left;
    free(carets);

    double budget = width - tokenWidth, used = 0;

    if (truncationType == kCTLineTruncationEnd) {
        while (keepEnd < length && used + charWidth[keepEnd] <= budget) used += charWidth[keepEnd++];
        keepStart = length;
    } else if (truncationType == kCTLineTruncationStart) {
        keepStart = length;
        while (keepStart > 0 && used + charWidth[keepStart - 1] <= budget) used += charWidth[--keepStart];
    } else {
        CFIndex head = 0, tail = length;

        while (head < tail) {
            if (head <= length - tail && used + charWidth[head] <= budget) used += charWidth[head++];
            else if (used + charWidth[tail - 1] <= budget) used += charWidth[--tail];
            else if (used + charWidth[head] <= budget) used += charWidth[head++];
            else break;
        }
        keepEnd = head;
        keepStart = tail;
    }
    free(charWidth);
    // never split a surrogate pair
    CFStringRef s = CFAttributedStringGetString(source);

    if (keepEnd > 0 && CTIsHighSurrogate(CFStringGetCharacterAtIndex(s, l->_range.location + keepEnd - 1))) keepEnd--;
    if (keepStart < length && CTIsLowSurrogate(CFStringGetCharacterAtIndex(s, l->_range.location + keepStart))) keepStart++;

    result = CFAttributedStringCreateMutable(NULL, 0);
    CFAttributedStringBeginEditing(result);
    if (keepEnd > 0) {
        CFAttributedStringRef head = CFAttributedStringCreateWithSubstring(NULL, source, CFRangeMake(l->_range.location, keepEnd));

        CFAttributedStringReplaceAttributedString(result, CFRangeMake(0, 0), head);
        CFRelease(head);
    }
    tok = tokenString(token);
    if (tok) {
        CFAttributedStringReplaceAttributedString(result, CFRangeMake(CFAttributedStringGetLength(result), 0), tok);
        CFRelease(tok);
    }
    if (keepStart < length) {
        CFAttributedStringRef tail = CFAttributedStringCreateWithSubstring(NULL, source, CFRangeMake(l->_range.location + keepStart, length - keepStart));

        CFAttributedStringReplaceAttributedString(result, CFRangeMake(CFAttributedStringGetLength(result), 0), tail);
        CFRelease(tail);
    }
    CFAttributedStringEndEditing(result);
    truncated = CTLineCreateWithAttributedString(result);
    CFRelease(result);
    return truncated;
}

// extra width goes to the spaces, or between clusters when the line has none
CTLineRef CTLineCreateJustifiedLine(CTLineRef line, CGFloat justificationFactor, double justificationWidth) {
    CTLine *l = (CTLine *)line;
    double content = l->_width - l->_trailingWhitespace;
    double extra;
    CFMutableArrayRef runs;
    CFStringRef s = l->_string ? CFAttributedStringGetString(l->_string) : NULL;
    CFIndex spaces = 0, glyphs = 0;
    CTLine *result;

    if (justificationFactor > 1) justificationFactor = 1;
    if (justificationFactor <= 0 || content <= 0 || content >= justificationWidth) return CFRetain(line);
    extra = (justificationWidth - content) * justificationFactor;
    runs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0; i < CFArrayGetCount(l->_runs); i++) {
        CTRun *run = copyRun((CTRun *)CFArrayGetValueAtIndex(l->_runs, i));

        CFArrayAppendValue(runs, run);
        [run release];
    }
    for (CFIndex i = 0; i < CFArrayGetCount(runs); i++) {
        CTRun *run = (CTRun *)CFArrayGetValueAtIndex(runs, i);

        for (CFIndex g = 0; g < run->_count; g++) {
            CFIndex idx = run->_indices[g];

            if (idx >= l->_range.location + l->_range.length - 0 || run->_advances[g].width == 0) continue;
            glyphs++;
            if (s && CFStringGetCharacterAtIndex(s, idx) == ' ') spaces++;
        }
    }

    double perSpace = spaces ? extra / spaces : 0;
    double perGlyph = spaces ? 0 : (glyphs > 1 ? extra / (glyphs - 1) : 0);
    double shift = 0;

    for (CFIndex i = 0; i < CFArrayGetCount(runs); i++) {
        CTRun *run = (CTRun *)CFArrayGetValueAtIndex(runs, i);
        double before = shift;

        for (CFIndex g = 0; g < run->_count; g++) {
            bool space = s && CFStringGetCharacterAtIndex(s, run->_indices[g]) == ' ';

            run->_positions[g].x += shift;
            if (run->_advances[g].width == 0) continue;
            double add = space ? perSpace : perGlyph;

            run->_advances[g].width += add;
            shift += add;
        }
        run->_width += shift - before;
    }
    result = (CTLine *)CTLineCreateWithRuns(runs, l->_range, l->_string);
    CFRelease(runs);
    return (CTLineRef)result;
}
