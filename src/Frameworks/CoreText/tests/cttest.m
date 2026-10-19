/*
 * cttest.m - CoreText checks: fonts, metrics, shaping, bidi, fallback, breaking, drawing
 *
 * Usage: cttest [out.png]
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdio.h>

static int failures;

#define CHECK(cond, ...) do { if (cond) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static CFAttributedStringRef makeString(NSString *text, CTFontRef font) {
    NSDictionary *attrs = @{ (id)kCTFontAttributeName: (id)font };

    return CFAttributedStringCreate(NULL, (CFStringRef)text, (CFDictionaryRef)attrs);
}

static void dumpLine(const char *label, CTLineRef line) {
    CFArrayRef runs = CTLineGetGlyphRuns(line);
    CGFloat ascent, descent, leading;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

    printf("     %s: width %.2f ascent %.2f descent %.2f runs %ld\n", label, width, ascent, descent, (long)CFArrayGetCount(runs));
    for (CFIndex i = 0; i < CFArrayGetCount(runs); i++) {
        CTRunRef run = CFArrayGetValueAtIndex(runs, i);
        CTFontRef font = CFDictionaryGetValue(CTRunGetAttributes(run), kCTFontAttributeName);
        CFStringRef name = CTFontCopyPostScriptName(font);
        CFRange r = CTRunGetStringRange(run);

        printf("       run %ld: %s glyphs %ld range %ld+%ld status %u\n", (long)i,
               name ? [(NSString *)name UTF8String] : "?", (long)CTRunGetGlyphCount(run), (long)r.location, (long)r.length, CTRunGetStatus(run));
        if (name) CFRelease(name);
    }
}

int main(int argc, char **argv) {
    @autoreleasepool {
        // CTTEST_FONTS registers a directory of fonts first, to compare with the same faces elsewhere
        const char *fontDir = getenv("CTTEST_FONTS");

        if (fontDir) {
            NSArray *names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:[NSString stringWithUTF8String:fontDir] error:NULL];

            for (NSString *n in names) {
                NSURL *u = [NSURL fileURLWithPath:[[NSString stringWithUTF8String:fontDir] stringByAppendingPathComponent:n]];

                CTFontManagerRegisterFontsForURL((CFURLRef)u, kCTFontManagerScopeProcess, NULL);
            }
        }

        CFArrayRef families = CTFontManagerCopyAvailableFontFamilyNames();

        CHECK(families && CFArrayGetCount(families) > 0, "font families: %ld", families ? (long)CFArrayGetCount(families) : 0L);
        for (CFIndex i = 0; families && i < CFArrayGetCount(families) && i < 12; i++)
            printf("     %s\n", [(NSString *)CFArrayGetValueAtIndex(families, i) UTF8String]);

        const char *fontName = getenv("CTTEST_FONT");
        CTFontRef font = CTFontCreateWithName(fontName ? (CFStringRef)[NSString stringWithUTF8String:fontName] : CFSTR("Helvetica"), 24, NULL);
        CFStringRef ps = CTFontCopyPostScriptName(font);
        CFStringRef family = CTFontCopyFamilyName(font);

        CHECK(font != NULL, "CTFontCreateWithName Helvetica 24 -> %s (%s)", [(NSString *)ps UTF8String], [(NSString *)family UTF8String]);
        CHECK(CTFontGetSize(font) == 24, "size %.1f", CTFontGetSize(font));
        CHECK(CTFontGetAscent(font) > 15 && CTFontGetAscent(font) < 30, "ascent %.2f descent %.2f leading %.2f", CTFontGetAscent(font), CTFontGetDescent(font), CTFontGetLeading(font));
        CHECK(CTFontGetCapHeight(font) > 10 && CTFontGetXHeight(font) > 6 && CTFontGetXHeight(font) < CTFontGetCapHeight(font),
              "cap height %.2f x height %.2f", CTFontGetCapHeight(font), CTFontGetXHeight(font));
        CHECK(CTFontGetUnitsPerEm(font) >= 1000, "units per em %u glyphs %ld", CTFontGetUnitsPerEm(font), (long)CTFontGetGlyphCount(font));

        UniChar chars[] = { 'A', 'V', 0xD83D, 0xDE00 };
        CGGlyph glyphs[4];
        bool all = CTFontGetGlyphsForCharacters(font, chars, glyphs, 4);

        CHECK(glyphs[0] != 0 && glyphs[1] != 0, "glyphs A=%u V=%u emoji=%u,%u all=%d", glyphs[0], glyphs[1], glyphs[2], glyphs[3], all);

        CGSize advances[2];
        double total = CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, glyphs, advances, 2);

        CHECK(total > 20, "advances A %.2f V %.2f", advances[0].width, advances[1].width);

        CGPathRef path = CTFontCreatePathForGlyph(font, glyphs[0], NULL);
        CGRect pathBox = path ? CGPathGetBoundingBox(path) : CGRectZero;

        CHECK(path && pathBox.size.height > 10, "glyph path box %.1f,%.1f %.1fx%.1f", pathBox.origin.x, pathBox.origin.y, pathBox.size.width, pathBox.size.height);

        CFDataRef head = CTFontCopyTable(font, kCTFontTableHead, 0);

        CHECK(head && CFDataGetLength(head) >= 54, "head table %ld bytes", head ? (long)CFDataGetLength(head) : 0L);

        CTFontRef bold = CTFontCreateCopyWithSymbolicTraits(font, 0, NULL, kCTFontTraitBold, kCTFontTraitBold);
        CFStringRef boldName = bold ? CTFontCopyPostScriptName(bold) : NULL;

        CHECK(bold && (CTFontGetSymbolicTraits(bold) & kCTFontTraitBold), "bold copy -> %s", boldName ? [(NSString *)boldName UTF8String] : "NULL");

        CTFontRef ui = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 13, NULL);
        CFStringRef uiName = ui ? CTFontCopyPostScriptName(ui) : NULL;

        CHECK(ui != NULL, "system UI font -> %s", uiName ? [(NSString *)uiName UTF8String] : "NULL");

        CTFontDescriptorRef desc = CTFontDescriptorCreateWithNameAndSize(CFSTR("Courier"), 10);
        CTFontRef mono = CTFontCreateWithFontDescriptor(desc, 0, NULL);
        CFStringRef monoName = CTFontCopyPostScriptName(mono);

        CHECK(mono != NULL, "descriptor Courier -> %s, monospace %d", [(NSString *)monoName UTF8String], (CTFontGetSymbolicTraits(mono) & kCTFontTraitMonoSpace) != 0);

        // fallback for characters Helvetica does not have
        CTFontRef cjk = CTFontCreateForString(font, CFSTR("中"), CFRangeMake(0, 1));
        CFStringRef cjkName = CTFontCopyPostScriptName(cjk);

        CHECK(cjk && CTFontGetGlyphCount(cjk) > 0, "fallback for U+4E2D -> %s", [(NSString *)cjkName UTF8String]);

        CTLineRef latin = CTLineCreateWithAttributedString(makeString(@"office AVA", font));

        dumpLine("latin", latin);
        CHECK(CTLineGetTypographicBounds(latin, NULL, NULL, NULL) > 50, "latin line laid out");

        CTLineRef mixed = CTLineCreateWithAttributedString(makeString(@"abc שלום def مرحبا", font));

        dumpLine("bidi", mixed);
        CFArrayRef runs = CTLineGetGlyphRuns(mixed);
        bool sawRTL = false;

        for (CFIndex i = 0; i < CFArrayGetCount(runs); i++)
            if (CTRunGetStatus(CFArrayGetValueAtIndex(runs, i)) & kCTRunStatusRightToLeft) sawRTL = true;
        CHECK(sawRTL && CFArrayGetCount(runs) >= 3, "bidi runs present");

        CTLineRef emoji = CTLineCreateWithAttributedString(makeString(@"hi \U0001F600 你好", font));

        dumpLine("fallback", emoji);
        CHECK(CFArrayGetCount(CTLineGetGlyphRuns(emoji)) >= 2, "fallback runs split");

        NSDictionary *noKern = @{ (id)kCTFontAttributeName: (id)font, (id)kCTKernAttributeName: @0 };
        NSDictionary *wide = @{ (id)kCTFontAttributeName: (id)font, (id)kCTKernAttributeName: @2 };
        CTLineRef unkerned = CTLineCreateWithAttributedString(CFAttributedStringCreate(NULL, CFSTR("AVA"), (CFDictionaryRef)noKern));
        CTLineRef kerned = CTLineCreateWithAttributedString(makeString(@"AVA", font));
        CTLineRef spaced = CTLineCreateWithAttributedString(CFAttributedStringCreate(NULL, CFSTR("AVA"), (CFDictionaryRef)wide));

        printf("     AVA widths: default %.2f kern 0 %.2f kern 2 %.2f, underline position %.2f thickness %.2f\n",
               CTLineGetTypographicBounds(kerned, NULL, NULL, NULL), CTLineGetTypographicBounds(unkerned, NULL, NULL, NULL),
               CTLineGetTypographicBounds(spaced, NULL, NULL, NULL), CTFontGetUnderlinePosition(font), CTFontGetUnderlineThickness(font));

        CGFloat secondary;
        CGFloat offset = CTLineGetOffsetForStringIndex(latin, 3, &secondary);
        CFIndex index = CTLineGetStringIndexForPosition(latin, CGPointMake(offset + 1, 0));

        CHECK(offset > 0 && index == 3, "caret offset of index 3 = %.2f, position back -> %ld", offset, (long)index);

        CTLineRef token = CTLineCreateWithAttributedString(makeString(@"…", font));
        CTLineRef truncated = CTLineCreateTruncatedLine(latin, 60, kCTLineTruncationEnd, token);

        CHECK(truncated && CTLineGetTypographicBounds(truncated, NULL, NULL, NULL) <= 60.5, "truncated width %.2f",
              truncated ? CTLineGetTypographicBounds(truncated, NULL, NULL, NULL) : -1);

        NSString *para = @"The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs.";
        CTTypesetterRef ts = CTTypesetterCreateWithAttributedString(makeString(para, font));
        CFIndex start = 0, lines = 0;

        while (start < (CFIndex)[para length] && lines < 20) {
            CFIndex n = CTTypesetterSuggestLineBreak(ts, start, 200);

            printf("     line %ld: \"%s\"\n", (long)lines, [[para substringWithRange:NSMakeRange(start, n)] UTF8String]);
            start += n;
            lines++;
        }
        CHECK(lines >= 3 && lines < 10, "wrapped into %ld lines at width 200", (long)lines);

        CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(makeString(para, font));
        CFRange fit;
        CGSize size = CTFramesetterSuggestFrameSizeWithConstraints(fs, CFRangeMake(0, 0), NULL, CGSizeMake(300, CGFLOAT_MAX), &fit);

        CHECK(size.width <= 300 && size.height > 50 && fit.length == (CFIndex)[para length], "suggested frame %.0fx%.0f fit %ld", size.width, size.height, (long)fit.length);

        // draw into a bitmap and count inked pixels
        size_t w = 480, h = 240;
        CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(NULL, w, h, 8, w * 4, rgb, kCGImageAlphaPremultipliedLast);

        CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
        CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
        CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
        CGContextSetTextPosition(ctx, 10, 200);
        CTLineDraw(latin, ctx);
        CGContextSetTextPosition(ctx, 10, 160);
        CTLineDraw(mixed, ctx);
        CGContextSetTextPosition(ctx, 10, 120);
        CTLineDraw(emoji, ctx);

        CGMutablePathRef box = CGPathCreateMutable();

        CGPathAddRect(box, NULL, CGRectMake(10, 0, 460, 100));
        CTFrameRef frame = CTFramesetterCreateFrame(CTFramesetterCreateWithAttributedString(makeString(para, CTFontCreateWithName(CFSTR("Helvetica"), 14, NULL))), CFRangeMake(0, 0), box, NULL);

        CTFrameDraw(frame, ctx);

        const uint8_t *pixels = CGBitmapContextGetData(ctx);
        size_t inked = 0;

        for (size_t i = 0; i < w * h; i++)
            if (pixels[i * 4] < 128) inked++;
        CHECK(inked > 2000, "inked pixels %zu", inked);

        if (argc > 1) {
            FILE *f = fopen(argv[1], "wb");

            // binary PPM, bitmap rows are stored top row first
            fprintf(f, "P6 %zu %zu 255\n", w, h);
            for (size_t y = 0; y < h; y++) {
                const uint8_t *row = pixels + y * w * 4;

                for (size_t x = 0; x < w; x++) fwrite(row + x * 4, 1, 3, f);
            }
            fclose(f);
            printf("     wrote %s\n", argv[1]);
        }
        printf("%s, %d failures\n", failures ? "FAILED" : "PASSED", failures);
    }
    return failures != 0;
}
