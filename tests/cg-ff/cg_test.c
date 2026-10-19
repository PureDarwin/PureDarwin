// cg_test.c - the CoreGraphics calls Firefox's XUL imports, checked in the guest
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/ucontext.h>

static int failures;

#define CHECK(cond, ...) do { \
    if (cond) { printf("ok   "); } else { printf("FAIL "); failures++; } \
    printf(__VA_ARGS__); printf("\n"); \
} while (0)

#define TAG(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

// test strings are created at run time, see the CFSTR isa check in main
static CFStringRef str(const char *text)
{
    return CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
}

static CFDataRef readFile(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    UInt8 *bytes = malloc(size);
    size_t got = fread(bytes, 1, size, f);

    fclose(f);
    CFDataRef data = CFDataCreate(NULL, bytes, (CFIndex)got);

    free(bytes);
    return data;
}

static int providerReleased;

static void releaseProviderBytes(void *info, const void *data, size_t size)
{
    providerReleased++;
    free((void *)data);
}

static CGFontRef fontFromFile(const char *path, bool withCallback)
{
    CFDataRef data = readFile(path);

    if (data == NULL)
        return NULL;

    CGDataProviderRef provider;

    if (withCallback) {
        size_t length = CFDataGetLength(data);
        void *copy = malloc(length);

        memcpy(copy, CFDataGetBytePtr(data), length);
        provider = CGDataProviderCreateWithData(NULL, copy, length, releaseProviderBytes);
    } else {
        CFIndex before = CFGetRetainCount(data);

        provider = CGDataProviderCreateWithCFData(data);
        CHECK(CFGetRetainCount(data) > before, "CGDataProviderCreateWithCFData retains its data (%ld -> %ld)",
            (long)before, (long)CFGetRetainCount(data));
    }
    CFRelease(data);

    CGFontRef font = CGFontCreateWithDataProvider(provider);

    CGDataProviderRelease(provider);
    return font;
}

static void testStaticFont(const char *path)
{
    CGFontRef font = fontFromFile(path, true);

    CHECK(font != NULL, "CGFontCreateWithDataProvider(%s)", path);
    if (font == NULL)
        return;

    CFStringRef ps = CGFontCopyPostScriptName(font);
    char name[128] = "";

    if (ps != NULL)
        CFStringGetCString(ps, name, sizeof(name), kCFStringEncodingUTF8);
    CHECK(strcmp(name, "DejaVuSans") == 0, "CGFontCopyPostScriptName = %s", name);
    if (ps != NULL)
        CFRelease(ps);

    CFArrayRef tags = CGFontCopyTableTags(font);
    bool head = false, cmap = false;
    CFIndex count = tags != NULL ? CFArrayGetCount(tags) : 0;

    for (CFIndex i = 0; i < count; i++) {
        uint32_t tag = (uint32_t)(uintptr_t)CFArrayGetValueAtIndex(tags, i);

        head |= (tag == TAG('h', 'e', 'a', 'd'));
        cmap |= (tag == TAG('c', 'm', 'a', 'p'));
    }
    CHECK(count > 5 && head && cmap, "CGFontCopyTableTags count %ld, head %d, cmap %d", (long)count, head, cmap);
    if (tags != NULL)
        CFRelease(tags);

    CFDataRef headTable = CGFontCopyTableForTag(font, TAG('h', 'e', 'a', 'd'));

    CHECK(headTable != NULL && CFDataGetLength(headTable) == 54, "CGFontCopyTableForTag(head) length %ld",
        headTable != NULL ? (long)CFDataGetLength(headTable) : -1L);
    if (headTable != NULL)
        CFRelease(headTable);

    // created strings, a constant one is only safe to message once its isa is a real class
    CFStringRef hName = CFStringCreateWithCString(NULL, "H", kCFStringEncodingASCII);
    CFStringRef periodName = CFStringCreateWithCString(NULL, "period", kCFStringEncodingASCII);
    CGGlyph glyphs[2] = { CGFontGetGlyphWithGlyphName(font, hName), CGFontGetGlyphWithGlyphName(font, periodName) };
    CGRect boxes[2];
    bool ok = CGFontGetGlyphBBoxes(font, glyphs, 2, boxes);

    CHECK(ok && boxes[0].size.width > 0 && boxes[0].size.height > boxes[1].size.height && boxes[1].origin.y >= 0,
        "CGFontGetGlyphBBoxes H %.0f,%.0f %.0fx%.0f period %.0f,%.0f %.0fx%.0f", boxes[0].origin.x, boxes[0].origin.y,
        boxes[0].size.width, boxes[0].size.height, boxes[1].origin.x, boxes[1].origin.y, boxes[1].size.width, boxes[1].size.height);

    CFRelease(hName);
    CFRelease(periodName);

    CFArrayRef axes = CGFontCopyVariationAxes(font);

    CHECK(axes == NULL, "CGFontCopyVariationAxes is NULL for a static font");
    if (axes != NULL)
        CFRelease(axes);

    CGFontRelease(font);
    CHECK(providerReleased == 1, "data provider release callback ran %d time(s)", providerReleased);
}

static void testVariableFont(const char *path)
{
    CGFontRef font = fontFromFile(path, false);

    CHECK(font != NULL, "CGFontCreateWithDataProvider(%s)", path);
    if (font == NULL)
        return;

    CFArrayRef axes = CGFontCopyVariationAxes(font);
    char axisName[64] = "";

    if (axes != NULL && CFArrayGetCount(axes) > 0) {
        CFDictionaryRef axis = CFArrayGetValueAtIndex(axes, 0);
        CFStringRef name = CFDictionaryGetValue(axis, kCGFontVariationAxisName);

        if (name != NULL)
            CFStringGetCString(name, axisName, sizeof(axisName), kCFStringEncodingUTF8);
    }
    CHECK(axes != NULL && CFArrayGetCount(axes) >= 1 && axisName[0] != 0, "CGFontCopyVariationAxes %ld, first %s",
        axes != NULL ? (long)CFArrayGetCount(axes) : -1L, axisName);
    if (axes != NULL)
        CFRelease(axes);

    uint32_t wght = TAG('w', 'g', 'h', 't');
    double bold = 700;
    CFNumberRef key = CFNumberCreate(NULL, kCFNumberSInt32Type, &wght);
    CFNumberRef value = CFNumberCreate(NULL, kCFNumberDoubleType, &bold);
    CFDictionaryRef request = CFDictionaryCreate(NULL, (const void **)&key, (const void **)&value, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CGFontRef boldFont = CGFontCreateCopyWithVariations(font, request);

    CFRelease(request);
    CFRelease(key);
    CFRelease(value);
    CHECK(boldFont != NULL && boldFont != font, "CGFontCreateCopyWithVariations by tag");

    CFDictionaryRef variations = boldFont != NULL ? CGFontCopyVariations(boldFont) : NULL;
    double weight = 0;

    if (variations != NULL) {
        CFNumberRef w = CFDictionaryGetValue(variations, str("Weight"));

        if (w != NULL)
            CFNumberGetValue(w, kCFNumberDoubleType, &weight);
        CFRelease(variations);
    }
    CHECK(weight == 700, "CGFontCopyVariations Weight = %.0f", weight);

    CFStringRef hName = CFStringCreateWithCString(NULL, "H", kCFStringEncodingASCII);
    CGGlyph h = CGFontGetGlyphWithGlyphName(font, hName);

    CFRelease(hName);
    int regularAdvance = 0, boldAdvance = 0;

    CGFontGetGlyphAdvances(font, &h, 1, &regularAdvance);
    if (boldFont != NULL)
        CGFontGetGlyphAdvances(boldFont, &h, 1, &boldAdvance);
    CHECK(boldAdvance > regularAdvance, "advance of H regular %d, bold %d", regularAdvance, boldAdvance);

    if (boldFont != NULL)
        CGFontRelease(boldFont);
    CGFontRelease(font);
}

static int bitmapReleased;

static void releaseBitmap(void *info, void *data)
{
    bitmapReleased++;
    free(data);
}

static uint32_t pixelAt(CGContextRef ctx, int x, int y)
{
    uint32_t *pixels = CGBitmapContextGetData(ctx);
    size_t row = CGBitmapContextGetBytesPerRow(ctx) / 4;
    size_t height = CGBitmapContextGetHeight(ctx);

    // bitmap rows run top down, CoreGraphics y runs bottom up
    return pixels[(height - 1 - y) * row + x];
}

static void testBitmap(void)
{
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    void *bytes = calloc(32 * 32, 4);
    CGContextRef ctx = CGBitmapContextCreateWithData(bytes, 32, 32, 8, 32 * 4, rgb,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little, releaseBitmap, NULL);

    CHECK(ctx != NULL && CGBitmapContextGetData(ctx) == bytes, "CGBitmapContextCreateWithData uses the given bytes");

    CGContextSetRGBFillColor(ctx, 1, 0, 0, 1);
    CGContextFillRect(ctx, CGRectMake(0, 0, 4, 4));
    CHECK(pixelAt(ctx, 1, 1) == 0xffff0000, "fill reaches the caller's buffer: %08x", pixelAt(ctx, 1, 1));

    CGContextSaveGState(ctx);
    CGContextSetBaseCTM(ctx, CGAffineTransformMakeTranslation(10, 10));
    CGContextSetRGBFillColor(ctx, 0, 0, 1, 1);
    CGContextFillRect(ctx, CGRectMake(0, 0, 2, 2));
    CGContextRestoreGState(ctx);
    int blueX = -1, blueY = -1;

    for (int y = 0; y < 32 && blueX < 0; y++) {
        for (int x = 0; x < 32; x++) {
            if (pixelAt(ctx, x, y) == 0xff0000ff) {
                blueX = x;
                blueY = y;
                break;
            }
        }
    }
    CHECK(pixelAt(ctx, 10, 10) == 0xff0000ff && pixelAt(ctx, 0, 0) == 0xffff0000, "CGContextSetBaseCTM moves drawing: %08x %08x, first blue at %d,%d",
        pixelAt(ctx, 10, 10), pixelAt(ctx, 0, 0), blueX, blueY);
    CGContextSetBaseCTM(ctx, CGAffineTransformIdentity);

    CGContextBeginTransparencyLayerWithRect(ctx, CGRectMake(20, 20, 4, 4), NULL);
    CGContextSetRGBFillColor(ctx, 0, 1, 0, 1);
    CGContextFillRect(ctx, CGRectMake(16, 16, 16, 16));
    CGContextEndTransparencyLayer(ctx);
    CHECK(pixelAt(ctx, 21, 21) == 0xff00ff00 && pixelAt(ctx, 17, 17) == 0, "transparency layer clipped to its rect: %08x %08x",
        pixelAt(ctx, 21, 21), pixelAt(ctx, 17, 17));

    CGContextSetShouldSubpixelPositionFonts(ctx, true);
    CGContextSetAllowsFontSubpixelPositioning(ctx, true);

    CGContextRelease(ctx);
    CGColorSpaceRelease(rgb);
    CHECK(bitmapReleased == 1, "bitmap release callback ran %d time(s)", bitmapReleased);
}

static void testColors(void)
{
    CGColorRef black = CGColorGetConstantColor(kCGColorBlack);
    const CGFloat *c = black != NULL ? CGColorGetComponents(black) : NULL;

    CHECK(black != NULL && CGColorGetNumberOfComponents(black) == 2 && c[0] == 0 && c[1] == 1, "kCGColorBlack is gray 0, alpha 1");
    CHECK(CGColorGetConstantColor(str("nonsense")) == NULL, "an unknown constant color is NULL");

    CGColorRef srgb = CGColorCreateSRGB(0.25, 0.5, 0.75, 1);

    c = CGColorGetComponents(srgb);
    CHECK(CGColorGetNumberOfComponents(srgb) == 4 && c[0] == 0.25 && c[2] == 0.75, "CGColorCreateSRGB components");
    CHECK(CFGetTypeID(srgb) == CGColorGetTypeID() && CGColorGetTypeID() != CFStringGetTypeID(),
        "CFGetTypeID(color) == CGColorGetTypeID() = %lu", (unsigned long)CGColorGetTypeID());
    CGColorRelease(srgb);

    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CHECK(CGColorSpaceCopyICCData(space) == NULL, "CGColorSpaceCopyICCData is NULL for a device space");
    CGColorSpaceRelease(space);
}

static void testGeometry(void)
{
    CGRect r = CGRectApplyAffineTransform(CGRectMake(0, 0, 10, 20), CGAffineTransformMakeRotation(M_PI / 2));

    CHECK(fabs(r.origin.x + 20) < 1e-9 && fabs(r.size.width - 20) < 1e-9 && fabs(r.size.height - 10) < 1e-9,
        "CGRectApplyAffineTransform rotated %.1f,%.1f %.1fx%.1f", r.origin.x, r.origin.y, r.size.width, r.size.height);

    CFDictionaryRef dict = CGRectCreateDictionaryRepresentation(CGRectMake(1, 2, 3, 4));
    CGRect back;

    CHECK(CGRectMakeWithDictionaryRepresentation(dict, &back) && back.origin.y == 2 && back.size.height == 4,
        "CGRect dictionary round trip");
    CFRelease(dict);
}

static void testPDF(void)
{
    CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
    CGDataConsumerRef consumer = CGDataConsumerCreateWithCFData(data);
    CGRect box = CGRectMake(0, 0, 200, 200);
    const void *keys[1] = { kCGPDFContextCreator };
    const void *values[1] = { str("cgtest") };
    CFDictionaryRef info = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CGContextRef pdf = CGPDFContextCreate(consumer, &box, info);
    CFURLRef url = CFURLCreateWithString(NULL, str("https://example.org/"), NULL);

    CGContextBeginPage(pdf, &box);
    CGPDFContextSetURLForRect(pdf, url, CGRectMake(10, 10, 50, 20));
    CGPDFContextAddDestinationAtPoint(pdf, str("top"), CGPointMake(0, 200));
    CGPDFContextSetDestinationForRect(pdf, str("top"), CGRectMake(10, 40, 50, 20));
    CGContextEndPage(pdf);
    CGPDFContextClose(pdf);

    CFIndex length = CFDataGetLength(data);
    char *text = calloc(length + 1, 1);

    memcpy(text, CFDataGetBytePtr(data), length);
    for (CFIndex i = 0; i < length; i++) {
        if (text[i] == 0)
            text[i] = ' ';
    }
    CHECK(strstr(text, "/URI") != NULL && strstr(text, "example.org") != NULL, "PDF has the URI link");
    CHECK(strstr(text, "/Dests") != NULL && strstr(text, "/XYZ") != NULL && strstr(text, "/Dest ") != NULL,
        "PDF has the named destination and its link");
    CHECK(strstr(text, "cgtest") != NULL, "PDF has kCGPDFContextCreator");
    free(text);
    CFRelease(url);
    CFRelease(info);
    CGContextRelease(pdf);
    CGDataConsumerRelease(consumer);
    CFRelease(data);
}

static void testWindowsAndDisplays(void)
{
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

    CHECK(list != NULL && CFArrayGetCount(list) == 0, "CGWindowListCopyWindowInfo is an empty array");
    if (list != NULL)
        CFRelease(list);
    CHECK(CGWindowListCreateImage(CGRectMake(0, 0, 100, 100), kCGWindowListOptionOnScreenOnly, kCGNullWindowID, 0) == NULL,
        "CGWindowListCreateImage is NULL");
    CHECK(CGWindowLevelForKey(kCGNormalWindowLevelKey) == 0 && CGWindowLevelForKey(kCGStatusWindowLevelKey) == 25,
        "CGWindowLevelForKey normal 0, status 25");

    CGSize mm = CGDisplayScreenSize(CGMainDisplayID());

    CHECK(mm.width > 0 && mm.height > 0, "CGDisplayScreenSize %.0f x %.0f mm", mm.width, mm.height);
    CHECK(!CGDisplayIsBuiltin(CGMainDisplayID()), "CGDisplayIsBuiltin is false");
    CHECK(CGDisplayCreateImage(CGMainDisplayID()) == NULL, "CGDisplayCreateImage is NULL");
    CHECK(CGDisplayStreamCreate(CGMainDisplayID(), 64, 64, 'BGRA', NULL, NULL) == NULL, "CGDisplayStreamCreate is NULL");
    CHECK(_CGSDefaultConnection() != 0, "_CGSDefaultConnection is nonzero");
    CHECK(CGSSetDenyWindowServerConnections(true) == kCGErrorSuccess, "CGSSetDenyWindowServerConnections");
    CGSShutdownServerConnections();
}

// a crash prints its backtrace, so a failing check names the frame it died in
static void crashed(int sig, siginfo_t *info, void *context)
{
    void *frames[64];
    int count = backtrace(frames, 64);

    fprintf(stderr, "CGTEST signal %d at %p\n", sig, info->si_addr);
#if defined(__x86_64__)
    // a call through a NULL pointer leaves the caller's return address on top of the stack
    ucontext_t *uc = context;
    void **sp = (void **)uc->uc_mcontext->__ss.__rsp;
    Dl_info where;


    void *rip = (void *)uc->uc_mcontext->__ss.__rip;

    if (dladdr(rip, &where) && where.dli_sname != NULL)
        fprintf(stderr, "CGTEST rip %p %s + %ld (%s)\n", rip, where.dli_sname,
            (long)((char *)rip - (char *)where.dli_saddr), where.dli_fname);
    else
        fprintf(stderr, "CGTEST rip %p\n", rip);
    for (int i = 0; i < 4; i++) {
        if (dladdr(sp[i], &where) && where.dli_sname != NULL)
            fprintf(stderr, "CGTEST sp[%d] %p %s + %ld (%s)\n", i, sp[i], where.dli_sname,
                (long)((char *)sp[i] - (char *)where.dli_saddr), where.dli_fname);
    }
#endif
    backtrace_symbols_fd(frames, count, 2);
    _exit(128 + sig);
}

int main(int argc, char **argv)
{
    struct sigaction action = { 0 };

    action.sa_sigaction = crashed;
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *dir = argc > 1 ? argv[1] : "/usr/local/libexec/cgtests";
    char path[512];

    // the isa of a CFSTR constant must be a class, its first word (the metaclass) is never zero
    void **isa = *(void ***)CFSTR("probe");

    CHECK(isa != NULL && isa[0] != NULL, "CFSTR constant isa %p is a class (first word %p)", (void *)isa, isa != NULL ? isa[0] : NULL);

    snprintf(path, sizeof(path), "%s/DejaVuSans.ttf", dir);
    testStaticFont(path);
    snprintf(path, sizeof(path), "%s/InterVariable.ttf", dir);
    testVariableFont(path);
    testBitmap();
    testColors();
    testGeometry();
    testPDF();
    testWindowsAndDisplays();

    printf("CGTEST %s, %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
