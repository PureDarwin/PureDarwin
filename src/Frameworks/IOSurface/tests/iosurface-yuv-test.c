// SPDX-License-Identifier: MIT
// iosurface-yuv-test: CARenderer draws a layer whose contents are a biplanar 4:2:0 surface, read back from an FBO
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const CFStringRef kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement, kIOSurfacePixelFormat,
    kIOSurfacePlaneInfo, kIOSurfacePlaneWidth, kIOSurfacePlaneHeight, kIOSurfacePlaneBytesPerElement;
IOSurfaceRef IOSurfaceCreate(CFDictionaryRef);
void *IOSurfaceGetBaseAddress(IOSurfaceRef);
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef);
void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef, size_t);
size_t IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef, size_t);
int IOSurfaceLock(IOSurfaceRef, uint32_t, uint32_t *);
int IOSurfaceUnlock(IOSurfaceRef, uint32_t, uint32_t *);
void IOSurfaceSetValue(IOSurfaceRef, CFStringRef, CFTypeRef);

typedef struct { double x, y; } TPoint;
typedef struct { double x, y, w, h; } TRect;

#define N 32

static void *(*msg)(void *, void *, ...);
static void *(*cls)(const char *);
static void *(*sel)(const char *);
static int failures;

static void put(CFMutableDictionaryRef d, CFStringRef k, long long v) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongLongType, &v);
    CFDictionarySetValue(d, k, n);
    CFRelease(n);
}

static IOSurfaceRef biplanar(uint32_t format, int sampleBytes) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef y = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef uv = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableArrayRef planes = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);

    put(y, kIOSurfacePlaneWidth, N), put(y, kIOSurfacePlaneHeight, N), put(y, kIOSurfacePlaneBytesPerElement, sampleBytes);
    put(uv, kIOSurfacePlaneWidth, N / 2), put(uv, kIOSurfacePlaneHeight, N / 2);
    put(uv, kIOSurfacePlaneBytesPerElement, 2 * sampleBytes);
    CFArrayAppendValue(planes, y), CFArrayAppendValue(planes, uv);
    put(d, kIOSurfaceWidth, N), put(d, kIOSurfaceHeight, N), put(d, kIOSurfacePixelFormat, format);
    CFDictionarySetValue(d, kIOSurfacePlaneInfo, planes);
    return IOSurfaceCreate(d);
}

// one colour over the whole surface, 16-bit samples carry it in their high byte
static void fill(IOSurfaceRef s, int sampleBytes, int Y, int Cb, int Cr) {
    IOSurfaceLock(s, 0, NULL);
    for (int r = 0; r < N; r++) {
        uint8_t *row = (uint8_t *)IOSurfaceGetBaseAddressOfPlane(s, 0) + r * IOSurfaceGetBytesPerRowOfPlane(s, 0);
        for (int x = 0; x < N; x++) {
            if (sampleBytes == 1) row[x] = Y;
            else ((uint16_t *)row)[x] = Y << 8;
        }
    }
    for (int r = 0; r < N / 2; r++) {
        uint8_t *row = (uint8_t *)IOSurfaceGetBaseAddressOfPlane(s, 1) + r * IOSurfaceGetBytesPerRowOfPlane(s, 1);
        for (int x = 0; x < N / 2; x++) {
            if (sampleBytes == 1) row[2 * x] = Cb, row[2 * x + 1] = Cr;
            else ((uint16_t *)row)[2 * x] = Cb << 8, ((uint16_t *)row)[2 * x + 1] = Cr << 8;
        }
    }
    IOSurfaceUnlock(s, 0, NULL);
}

static void render(void *renderer) {
    ((void (*)(void *, void *, double, void *))msg)(renderer, sel("beginFrameAtTime:timeStamp:"), 0.0, NULL);
    msg(renderer, sel("render"));
    msg(renderer, sel("endFrame"));
}

static void expect(const char *what, int r, int g, int b) {
    unsigned char px[4] = { 0 };

    glReadPixels(N / 2, N / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    int ok = abs(px[0] - r) <= 4 && abs(px[1] - g) <= 4 && abs(px[2] - b) <= 4;
    printf("%s %s: %d,%d,%d (want %d,%d,%d)\n", ok ? "ok  " : "FAIL", what, px[0], px[1], px[2], r, g, b);
    failures += !ok;
}

int main(void) {
    CGLPixelFormatAttribute attrs[] = { kCGLPFAColorSize, 24, kCGLPFAAlphaSize, 8, 0 };
    CGLPixelFormatObj pix = NULL;
    CGLContextObj ctx = NULL;
    GLint npix = 0;
    GLuint fbo, color;

    if (!dlopen("/System/Library/Frameworks/QuartzCore.framework/QuartzCore", RTLD_NOW | RTLD_GLOBAL)) {
        printf("FAIL no QuartzCore: %s\n", dlerror());
        return 1;
    }
    msg = dlsym(RTLD_DEFAULT, "objc_msgSend"), cls = dlsym(RTLD_DEFAULT, "objc_getClass");
    sel = dlsym(RTLD_DEFAULT, "sel_registerName");
    if (CGLChoosePixelFormat(attrs, &pix, &npix) != kCGLNoError || CGLCreateContext(pix, NULL, &ctx) != kCGLNoError ||
        CGLSetCurrentContext(ctx) != kCGLNoError) {
        printf("FAIL no CGL context\n");
        return 1;
    }

    // an N x N target and the state a CARenderer caller sets up
    glGenFramebuffers(1, &fbo), glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("FAIL incomplete framebuffer\n");
        return 1;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glViewport(0, 0, N, N);
    glMatrixMode(GL_PROJECTION), glLoadIdentity(), glOrtho(0, N, 0, N, -100, 100);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY), glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    void *pool = msg(msg(cls("NSAutoreleasePool"), sel("alloc")), sel("init"));
    void *layer = msg(cls("CALayer"), sel("layer"));
    ((void (*)(void *, void *, TRect))msg)(layer, sel("setBounds:"), (TRect){ 0, 0, N, N });
    ((void (*)(void *, void *, TPoint))msg)(layer, sel("setPosition:"), (TPoint){ N / 2, N / 2 });
    void *renderer = msg(cls("CARenderer"), sel("rendererWithCGLContext:options:"), ctx, NULL);
    msg(renderer, sel("setLayer:"), layer);

    // BT.601 video range: Y 81 Cb 90 Cr 240 is red
    IOSurfaceRef v = biplanar('420v', 1);
    fill(v, 1, 81, 90, 240);
    msg(layer, sel("setContents:"), v);
    render(renderer);
    expect("420v red", 255, 0, 0);

    // the same surface written again: its seed moves and the layer shows the new colour (Y 145 Cb 54 Cr 34 is green)
    fill(v, 1, 145, 54, 34);
    render(renderer);
    expect("420v after a write", 0, 255, 0);

    // full range grey and BT.709 named on the surface (Y 128 Cb 128 Cr 128 is mid grey either way)
    IOSurfaceRef f = biplanar('420f', 1);
    fill(f, 1, 128, 128, 128);
    IOSurfaceSetValue(f, CFSTR("IOSurfaceYCbCrMatrix"), CFSTR("ITU_R_709_2"));
    msg(layer, sel("setContents:"), f);
    render(renderer);
    expect("420f grey", 128, 128, 128);

    // full range 709 blue: Y 18 Cb 255 Cr 116
    fill(f, 1, 18, 255, 116);
    render(renderer);
    expect("420f BT.709 blue", 0, 0, 255);

    // 10-bit in 16-bit samples, video range red again
    IOSurfaceRef x = biplanar('x420', 2);
    fill(x, 2, 81, 90, 240);
    msg(layer, sel("setContents:"), x);
    render(renderer);
    expect("x420 red", 255, 0, 0);

    msg(pool, sel("drain"));
    printf("%d failures\n", failures);
    return failures != 0;
}
