/*
 * CTTypesetter.m - itemizing, shaping and line breaking
 *
 * Text is split into items by attribute run, bidi level (FriBidi), script and the font
 * that covers each character (fontconfig fallback), each item is shaped with HarfBuzz,
 * and line breaks come from ICU.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#import "CTLayout.h"
#import "KTFont.h"
#include <fribidi.h>
#include <unicode/ubrk.h>

static CFTypeID typesetterTypeID;

@implementation CTTypesetter

+ (void)initialize {
    if (self == [CTTypesetter class])
        typesetterTypeID = CTRegisterTypeID("CTTypesetter");
}

- (CFTypeID)_cfTypeID {
    return typesetterTypeID;
}

- (void)dealloc {
    if (_string) CFRelease(_string);
    if (_fallbacks) CFRelease(_fallbacks);
    free(_text);
    free(_levels);
    free(_charAdvances);
    [super dealloc];
}

@end

CFTypeID CTTypesetterGetTypeID(void) {
    [CTTypesetter class];
    return typesetterTypeID;
}

CTFontRef CTCopyDefaultFont(void) {
    static CTFontRef font;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        font = CTFontCreateWithName(CFSTR("Helvetica"), 12.0, NULL);
    });
    return font ? CFRetain(font) : NULL;
}

CTFontRef CTCopyFontFromAttributes(CFDictionaryRef attributes) {
    id value = attributes ? (id)CFDictionaryGetValue(attributes, kCTFontAttributeName) : nil;

    if (value == nil) return CTCopyDefaultFont();
    if ([value isKindOfClass:[KTFont class]]) return CFRetain(value);
    // AppKit's NSFont wraps a CTFont of its own
    if ([value respondsToSelector:@selector(fontName)] && [value respondsToSelector:@selector(pointSize)]) {
        CTFontRef font = CTFontCreateWithName((CFStringRef)[value fontName], [value pointSize], NULL);

        if (font) return font;
    }
    return CTCopyDefaultFont();
}

static bool isParagraphSeparator(UniChar c) {
    return c == '\n' || c == '\r' || c == 0x2029 || c == 0x0085;
}

// FriBidi levels per UTF-16 unit, one paragraph at a time
static void computeLevels(CTTypesetter *ts) {
    CFIndex start = 0;

    ts->_levels = calloc(ts->_length ? ts->_length : 1, 1);
    if (ts->_disableBidi) {
        memset(ts->_levels, ts->_forcedLevel > 0 ? ts->_forcedLevel : 0, ts->_length);
        return;
    }
    while (start < ts->_length) {
        CFIndex end = start;
        CFIndex count = 0;
        CTParagraphStyleValues style;
        FriBidiParType base;

        while (end < ts->_length && !isParagraphSeparator(ts->_text[end])) end++;
        if (end < ts->_length) end++;
        CTParagraphStyleValuesFromAttributes(CFAttributedStringGetAttributes(ts->_string, start, NULL), &style);
        base = ts->_forcedLevel >= 0 ? (ts->_forcedLevel & 1 ? FRIBIDI_PAR_RTL : FRIBIDI_PAR_LTR)
             : style.baseWritingDirection == kCTWritingDirectionRightToLeft ? FRIBIDI_PAR_RTL
             : style.baseWritingDirection == kCTWritingDirectionLeftToRight ? FRIBIDI_PAR_LTR : FRIBIDI_PAR_ON;

        FriBidiChar *u32 = malloc(sizeof(FriBidiChar) * (end - start));
        CFIndex *unitOf = malloc(sizeof(CFIndex) * (end - start));
        FriBidiCharType *types = malloc(sizeof(FriBidiCharType) * (end - start));
        FriBidiBracketType *brackets = malloc(sizeof(FriBidiBracketType) * (end - start));
        FriBidiLevel *levels = malloc(sizeof(FriBidiLevel) * (end - start));

        for (CFIndex i = start; i < end;) {
            CFIndex units;

            unitOf[count] = i;
            u32[count++] = CTCodePointAt(ts->_text, end, i, &units);
            i += units;
        }
        fribidi_get_bidi_types(u32, count, types);
        fribidi_get_bracket_types(u32, count, types, brackets);
        if (fribidi_get_par_embedding_levels_ex(types, brackets, count, &base, levels) == 0)
            memset(levels, 0, count);
        for (CFIndex k = 0; k < count; k++) {
            CFIndex next = k + 1 < count ? unitOf[k + 1] : end;

            for (CFIndex i = unitOf[k]; i < next; i++) ts->_levels[i] = levels[k];
        }
        free(u32);
        free(unitOf);
        free(types);
        free(brackets);
        free(levels);
        start = end;
    }
}

CTTypesetterRef CTTypesetterCreateWithAttributedStringAndOptions(CFAttributedStringRef string, CFDictionaryRef options) {
    CTTypesetter *ts;
    CFNumberRef forced = options ? CFDictionaryGetValue(options, kCTTypesetterOptionForcedEmbeddingLevel) : NULL;
    CFBooleanRef disable = options ? CFDictionaryGetValue(options, kCTTypesetterOptionDisableBidiProcessing) : NULL;

    if (string == NULL) return NULL;
    ts = [[CTTypesetter alloc] init];
    ts->_string = CFAttributedStringCreateCopy(NULL, string);
    ts->_length = CFAttributedStringGetLength(string);
    ts->_text = malloc(sizeof(UniChar) * (ts->_length ? ts->_length : 1));
    CFStringGetCharacters(CFAttributedStringGetString(string), CFRangeMake(0, ts->_length), ts->_text);
    ts->_forcedLevel = -1;
    if (forced) CFNumberGetValue(forced, kCFNumberIntType, &ts->_forcedLevel);
    ts->_disableBidi = disable && CFBooleanGetValue(disable);
    ts->_fallbacks = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    computeLevels(ts);
    return (CTTypesetterRef)ts;
}

CTTypesetterRef CTTypesetterCreateWithAttributedString(CFAttributedStringRef string) {
    return CTTypesetterCreateWithAttributedStringAndOptions(string, NULL);
}

static bool isDefaultIgnorable(UTF32Char c) {
    return c < 0x20 || c == 0x7F || c == 0x00AD || (c >= 0x200B && c <= 0x200F) || (c >= 0x2028 && c <= 0x202E) ||
           (c >= 0x2060 && c <= 0x206F) || (c >= 0xFE00 && c <= 0xFE0F) || c == 0xFEFF || (c >= 0xE0000 && c <= 0xE0FFF);
}

// the font for one character: the run's own font, else a fallback shared by every
// character that falls back to the same face and size
static CTFontRef fontForCharacter(CTTypesetter *ts, CTFontRef base, CFIndex i, UTF32Char c, CFStringRef language) {
    CFIndex units;
    CFStringRef s, name, key;
    CTFontRef found, cached;

    if (CTFontHasCharacter(base, c)) return base;
    CTCodePointAt(ts->_text, ts->_length, i, &units);
    s = CFStringCreateWithCharacters(NULL, ts->_text + i, units);
    found = CTFontCreateForStringWithLanguage(base, s, CFRangeMake(0, units), language);
    CFRelease(s);
    if (found == NULL) return base;
    name = CTFontCopyPostScriptName(found);
    key = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@/%g"), name ? name : CFSTR(""), (double)CTFontGetSize(found));
    if (name) CFRelease(name);
    cached = CFDictionaryGetValue(ts->_fallbacks, key);
    if (cached == NULL) {
        CFDictionarySetValue(ts->_fallbacks, key, found);
        cached = found;
    }
    CFRelease(key);
    CFRelease(found);
    // the base font itself came back, so keep using that object
    if (CFEqual(cached, base) || [(KTFont *)cached _ctFace] == [(KTFont *)base _ctFace])
        return base;
    return cached;
}

static bool numberAttribute(CFDictionaryRef attrs, CFStringRef key, double *value) {
    CFNumberRef n = attrs ? CFDictionaryGetValue(attrs, key) : NULL;

    if (n == NULL || CFGetTypeID(n) != CFNumberGetTypeID()) return false;
    CFNumberGetValue(n, kCFNumberDoubleType, value);
    return true;
}

static void setRunMetrics(CTRun *run) {
    run->_ascent = CTFontGetAscent(run->_font);
    run->_descent = CTFontGetDescent(run->_font);
    run->_leading = CTFontGetLeading(run->_font);
}

// a run delegate's text gets no glyphs drawn, only the delegate's width and height
static CTRun *delegateRun(CTTypesetter *ts, CFDictionaryRef attrs, CTFontRef font, CFRange range, uint8_t level) {
    CTRunDelegate *d = (CTRunDelegate *)CFDictionaryGetValue(attrs, kCTRunDelegateAttributeName);
    CTRun *run = CTRunCreate(attrs, font, range.length);
    CGFloat width = d->_callbacks.getWidth ? d->_callbacks.getWidth(d->_refCon) : 0;

    for (CFIndex i = 0; i < range.length; i++) {
        run->_glyphs[i] = 0;
        run->_indices[i] = range.location + i;
        run->_advances[i] = CGSizeMake(i == 0 ? width : 0, 0);
        run->_positions[i] = CGPointMake(i == 0 ? 0 : width, 0);
    }
    run->_range = range;
    run->_width = width;
    run->_level = level;
    run->_ascent = d->_callbacks.getAscent ? d->_callbacks.getAscent(d->_refCon) : 0;
    run->_descent = d->_callbacks.getDescent ? d->_callbacks.getDescent(d->_refCon) : 0;
    run->_leading = 0;
    if (level & 1) run->_status |= kCTRunStatusRightToLeft;
    return run;
}

static CTRun *shapeItem(CTTypesetter *ts, CFDictionaryRef attrs, CTFontRef font, CFRange range, uint8_t level, hb_script_t script) {
    hb_font_t *hb = [(KTFont *)font _hbFont];
    CTFace *face = [(KTFont *)font _ctFace];
    CFStringRef languageName = CFDictionaryGetValue(attrs, kCTLanguageAttributeName);
    double kern = 0, tracking = 0, ligatures = 1;
    bool hasKern = numberAttribute(attrs, kCTKernAttributeName, &kern);
    hb_feature_t features[4];
    unsigned int featureCount = 0;
    hb_buffer_t *buffer;
    hb_glyph_info_t *info;
    hb_glyph_position_t *pos;
    unsigned int count;
    CGFloat scale;
    CTRun *run;
    double pen = 0;

    if (CFDictionaryGetValue(attrs, kCTRunDelegateAttributeName))
        return delegateRun(ts, attrs, font, range, level);
    numberAttribute(attrs, kCTTrackingAttributeName, &tracking);
    numberAttribute(attrs, kCTLigatureAttributeName, &ligatures);

    // no HarfBuzz font means a graphics font without a file, map glyphs one by one
    if (hb == NULL || face == NULL) {
        run = CTRunCreate(attrs, font, range.length);
        CTFontGetGlyphsForCharacters(font, ts->_text + range.location, run->_glyphs, range.length);
        CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, run->_glyphs, run->_advances, range.length);
        for (CFIndex i = 0; i < range.length; i++) {
            CFIndex k = (level & 1) ? range.length - 1 - i : i;

            run->_indices[i] = range.location + k;
            run->_positions[i] = CGPointMake(pen, 0);
            pen += run->_advances[i].width + kern + tracking;
            run->_advances[i].width += kern + tracking;
        }
        goto done;
    }

    buffer = hb_buffer_create();
    hb_buffer_add_utf16(buffer, ts->_text, (int)ts->_length, (unsigned)range.location, (int)range.length);
    hb_buffer_set_direction(buffer, (level & 1) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, script);
    if (languageName && CFGetTypeID(languageName) == CFStringGetTypeID()) {
        char tag[64];

        if (CFStringGetCString(languageName, tag, sizeof(tag), kCFStringEncodingUTF8))
            hb_buffer_set_language(buffer, hb_language_from_string(tag, -1));
    }
    hb_buffer_guess_segment_properties(buffer);
    // kern 0 turns pair kerning off, ligature 0 keeps only the required ligatures
    if (hasKern && kern == 0) features[featureCount++] = (hb_feature_t){ HB_TAG('k', 'e', 'r', 'n'), 0, 0, (unsigned)-1 };
    if (ligatures == 0) {
        features[featureCount++] = (hb_feature_t){ HB_TAG('l', 'i', 'g', 'a'), 0, 0, (unsigned)-1 };
        features[featureCount++] = (hb_feature_t){ HB_TAG('c', 'l', 'i', 'g'), 0, 0, (unsigned)-1 };
    } else if (ligatures >= 2)
        features[featureCount++] = (hb_feature_t){ HB_TAG('d', 'l', 'i', 'g'), 1, 0, (unsigned)-1 };
    pthread_mutex_lock(&face->lock);
    hb_shape(hb, buffer, features, featureCount);
    pthread_mutex_unlock(&face->lock);

    info = hb_buffer_get_glyph_infos(buffer, &count);
    pos = hb_buffer_get_glyph_positions(buffer, &count);
    scale = CTFontGetSize(font) / face->unitsPerEm;
    run = CTRunCreate(attrs, font, count);
    for (unsigned int i = 0; i < count; i++) {
        double extra = kern + tracking;

        run->_glyphs[i] = info[i].codepoint;
        run->_indices[i] = info[i].cluster;
        run->_positions[i] = CGPointMake(pen + pos[i].x_offset * scale, pos[i].y_offset * scale);
        run->_advances[i] = CGSizeMake(pos[i].x_advance * scale + extra, pos[i].y_advance * scale);
        pen += run->_advances[i].width;
    }
    hb_buffer_destroy(buffer);

done:
    run->_range = range;
    run->_level = level;
    run->_width = pen;
    if (level & 1) run->_status |= kCTRunStatusRightToLeft;
    for (CFIndex i = 1; i < run->_count; i++)
        if ((level & 1) ? run->_indices[i] > run->_indices[i - 1] : run->_indices[i] < run->_indices[i - 1])
            run->_status |= kCTRunStatusNonMonotonic;
    setRunMetrics(run);
    return run;
}

// runs in logical order covering the range
static CFMutableArrayRef createRuns(CTTypesetter *ts, CFRange range) {
    CFMutableArrayRef runs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    hb_unicode_funcs_t *ucd = hb_unicode_funcs_get_default();
    CFIndex end = range.location + range.length;
    CFIndex i = range.location;

    while (i < end) {
        CFRange attrRange;
        CFDictionaryRef attrs = CFAttributedStringGetAttributesAndLongestEffectiveRange(ts->_string, i, CFRangeMake(i, end - i), &attrRange);
        CFIndex attrEnd = attrRange.location + attrRange.length;
        CTFontRef base = CTCopyFontFromAttributes(attrs);
        CFStringRef language = attrs ? CFDictionaryGetValue(attrs, kCTLanguageAttributeName) : NULL;
        bool delegate = attrs && CFDictionaryGetValue(attrs, kCTRunDelegateAttributeName);
        CFMutableDictionaryRef runAttrs = attrs ? CFDictionaryCreateMutableCopy(NULL, 0, attrs) : CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        if (language && CFGetTypeID(language) != CFStringGetTypeID()) language = NULL;
        while (i < attrEnd) {
            CFIndex start = i;
            uint8_t level = ts->_levels[i];
            hb_script_t script = HB_SCRIPT_COMMON;
            CTFontRef font = NULL;

            // grow the item while level, script and font agree
            while (i < attrEnd && ts->_levels[i] == level) {
                CFIndex units;
                UTF32Char c = CTCodePointAt(ts->_text, attrEnd, i, &units);
                hb_script_t s = hb_unicode_script(ucd, c);
                bool neutral = s == HB_SCRIPT_COMMON || s == HB_SCRIPT_INHERITED || s == HB_SCRIPT_UNKNOWN;
                bool mark = hb_unicode_general_category(ucd, c) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK;
                CTFontRef f;

                if (!neutral && script != HB_SCRIPT_COMMON && s != script) break;
                // marks stay with their base character's font, spaces go back to the run's own font as on Apple
                if (delegate || isDefaultIgnorable(c) || (mark && font != NULL && CTFontHasCharacter(font, c)))
                    f = font ? font : base;
                else
                    f = fontForCharacter(ts, base, i, c, language);
                if (font != NULL && f != font) break;
                font = f;
                if (!neutral) script = s;
                i += units;
            }
            if (font == NULL) font = base;
            if (font != base) CFDictionarySetValue(runAttrs, kCTFontAttributeName, font);
            else if (attrs == NULL || CFDictionaryGetValue(attrs, kCTFontAttributeName) == NULL) CFDictionarySetValue(runAttrs, kCTFontAttributeName, base);
            else CFDictionarySetValue(runAttrs, kCTFontAttributeName, CFDictionaryGetValue(attrs, kCTFontAttributeName));

            CTRun *run = shapeItem(ts, runAttrs, font, CFRangeMake(start, i - start), level, script);

            CFArrayAppendValue(runs, run);
            [run release];
        }
        CFRelease(runAttrs);
        if (base) CFRelease(base);
    }
    return runs;
}

// advances per UTF-16 unit for line breaking, each cluster's width on its first unit
static void computeCharAdvances(CTTypesetter *ts) {
    CFMutableArrayRef runs;

    if (ts->_charAdvances) return;
    ts->_charAdvances = calloc(ts->_length ? ts->_length : 1, sizeof(double));
    runs = createRuns(ts, CFRangeMake(0, ts->_length));
    for (CFIndex r = 0; r < CFArrayGetCount(runs); r++) {
        CTRun *run = (CTRun *)CFArrayGetValueAtIndex(runs, r);

        for (CFIndex g = 0; g < run->_count; g++)
            ts->_charAdvances[run->_indices[g]] += run->_advances[g].width;
    }
    CFRelease(runs);
}

static double tabWidth(CTTypesetter *ts, CFIndex i, double x) {
    CTParagraphStyleValues style;
    double interval;

    CTParagraphStyleValuesFromAttributes(CFAttributedStringGetAttributes(ts->_string, i, NULL), &style);
    for (CFIndex t = 0; style.tabStops && t < CFArrayGetCount(style.tabStops); t++) {
        double location = CTTextTabGetLocation(CFArrayGetValueAtIndex(style.tabStops, t));

        if (location > x + 0.001) return location - x;
    }
    interval = style.defaultTabInterval > 0 ? style.defaultTabInterval : 28.0;
    return (floor(x / interval) + 1) * interval - x;
}

static bool isWhitespace(UniChar c) {
    return c == ' ' || c == '\t' || c == 0x00A0 || c == 0x3000 || (c >= 0x2000 && c <= 0x200B) || isParagraphSeparator(c) || c == 0x2028;
}

// break opportunities from ICU's line iterator
static CFIndex lastLineBreak(CTTypesetter *ts, CFIndex start, CFIndex limit) {
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *it = ubrk_open(UBRK_LINE, NULL, (const UChar *)ts->_text, (int32_t)ts->_length, &status);
    CFIndex best = -1;

    if (U_FAILURE(status)) return -1;
    for (int32_t b = ubrk_following(it, (int32_t)start); b != UBRK_DONE && b <= limit; b = ubrk_next(it))
        best = b;
    ubrk_close(it);
    return best;
}

static CFIndex lastClusterBreak(CTTypesetter *ts, CFIndex start, CFIndex limit) {
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *it = ubrk_open(UBRK_CHARACTER, NULL, (const UChar *)ts->_text, (int32_t)ts->_length, &status);
    CFIndex best = -1, first = -1;

    if (U_FAILURE(status)) return start + 1;
    for (int32_t b = ubrk_following(it, (int32_t)start); b != UBRK_DONE; b = ubrk_next(it)) {
        if (first < 0) first = b;
        if (b > limit) break;
        best = b;
    }
    ubrk_close(it);
    // at least one cluster per line, so layout always advances
    return best > start ? best : (first > start ? first : start + 1);
}

static CFIndex suggestBreak(CTTypesetter *ts, CFIndex start, double width, double offset, bool cluster) {
    double x = offset;
    CFIndex i;

    if (start < 0 || start >= ts->_length) return 0;
    computeCharAdvances(ts);
    for (i = start; i < ts->_length; i++) {
        UniChar c = ts->_text[i];
        double advance = c == '\t' ? tabWidth(ts, i, x) : ts->_charAdvances[i];

        if (isParagraphSeparator(c) || c == 0x2028) {
            if (c == '\r' && i + 1 < ts->_length && ts->_text[i + 1] == '\n') i++;
            return i + 1 - start;
        }
        // trailing white space hangs past the edge
        if (x + advance > offset + width + 0.001 && !isWhitespace(c)) break;
        x += advance;
    }
    if (i >= ts->_length) return ts->_length - start;
    if (!cluster) {
        CFIndex b = lastLineBreak(ts, start, i);

        if (b > start) {
            // a hard break inside the fitting part ends the line there
            return b - start;
        }
    }
    return lastClusterBreak(ts, start, i) - start;
}

CFIndex CTTypesetterSuggestLineBreakWithOffset(CTTypesetterRef typesetter, CFIndex startIndex, double width, double offset) {
    return suggestBreak((CTTypesetter *)typesetter, startIndex, width, offset, false);
}

CFIndex CTTypesetterSuggestLineBreak(CTTypesetterRef typesetter, CFIndex startIndex, double width) {
    return suggestBreak((CTTypesetter *)typesetter, startIndex, width, 0, false);
}

CFIndex CTTypesetterSuggestClusterBreakWithOffset(CTTypesetterRef typesetter, CFIndex startIndex, double width, double offset) {
    return suggestBreak((CTTypesetter *)typesetter, startIndex, width, offset, true);
}

CFIndex CTTypesetterSuggestClusterBreak(CTTypesetterRef typesetter, CFIndex startIndex, double width) {
    return suggestBreak((CTTypesetter *)typesetter, startIndex, width, 0, true);
}

// UAX 9 rule L2: reverse every maximal sequence at or above each odd level, highest first
static void reorderVisually(CFMutableArrayRef runs) {
    CFIndex n = CFArrayGetCount(runs);
    uint8_t highest = 0, lowestOdd = 255;

    for (CFIndex i = 0; i < n; i++) {
        uint8_t l = ((CTRun *)CFArrayGetValueAtIndex(runs, i))->_level;

        if (l > highest) highest = l;
        if ((l & 1) && l < lowestOdd) lowestOdd = l;
    }
    if (lowestOdd == 255) return;
    for (int level = highest; level >= lowestOdd; level--) {
        for (CFIndex i = 0; i < n;) {
            CFIndex j = i;

            while (j < n && ((CTRun *)CFArrayGetValueAtIndex(runs, j))->_level >= level) j++;
            for (CFIndex a = i, b = j - 1; a < b; a++, b--)
                CFArrayExchangeValuesAtIndices(runs, a, b);
            i = j > i ? j : i + 1;
        }
    }
}

static CTLineRef createLine(CTTypesetter *ts, CFRange range, double offset) {
    CFMutableArrayRef runs;
    double x = 0;
    CTLineRef line;

    if (range.length == 0) range.length = ts->_length - range.location;
    if (range.location < 0 || range.location + range.length > ts->_length) return NULL;
    runs = createRuns(ts, range);
    reorderVisually(runs);

    // place runs left to right, tabs take their widths from the pen position
    for (CFIndex r = 0; r < CFArrayGetCount(runs); r++) {
        CTRun *run = (CTRun *)CFArrayGetValueAtIndex(runs, r);
        double pen = x;

        for (CFIndex g = 0; g < run->_count; g++) {
            double xoff = run->_positions[g].x;

            // positions held pen plus offset from the run start, keep only the offset
            for (CFIndex k = 0; k < g; k++) xoff -= run->_advances[k].width;
            if (ts->_text[run->_indices[g]] == '\t')
                run->_advances[g].width = tabWidth(ts, run->_indices[g], pen + offset);
            run->_positions[g].x = pen + xoff;
            pen += run->_advances[g].width;
        }
        run->_width = pen - x;
        x = pen;
    }
    line = CTLineCreateWithRuns(runs, range, ts->_string);
    CFRelease(runs);
    return line;
}

CTLineRef CTTypesetterCreateLineWithOffset(CTTypesetterRef typesetter, CFRange stringRange, double offset) {
    return createLine((CTTypesetter *)typesetter, stringRange, offset);
}

CTLineRef CTTypesetterCreateLine(CTTypesetterRef typesetter, CFRange stringRange) {
    return createLine((CTTypesetter *)typesetter, stringRange, 0);
}
