// SPDX-License-Identifier: MIT
// iosurface-gl-test: a BGRA surface bound to a texture with CGLTexImageIOSurface2D on Mesa, read back
#include <OpenGL/OpenGL.h>
#include <GL/gl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

extern const CFStringRef kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement, kIOSurfacePixelFormat;
IOSurfaceRef IOSurfaceCreate(CFDictionaryRef);
void *IOSurfaceGetBaseAddress(IOSurfaceRef);
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef);

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif

static void put(CFMutableDictionaryRef d, CFStringRef k, long long v) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongLongType, &v);
    CFDictionarySetValue(d, k, n);
    CFRelease(n);
}

int main(void) {
    CGLPixelFormatAttribute attrs[] = { kCGLPFAColorSize, 24, kCGLPFAAlphaSize, 8, 0 };
    CGLPixelFormatObj pix = NULL;
    CGLContextObj ctx = NULL;
    GLint npix = 0, enabled = -1;
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IOSurfaceRef s;
    uint32_t *px, out[30 * 20];
    size_t stride;
    GLuint tex;
    int failures = 0;

    if (CGLChoosePixelFormat(attrs, &pix, &npix) != kCGLNoError || CGLCreateContext(pix, NULL, &ctx) != kCGLNoError ||
        CGLSetCurrentContext(ctx) != kCGLNoError) {
        printf("FAIL no CGL context\n");
        return 1;
    }
    CGLEnable(ctx, kCGLCESurfaceBackingSize);
    CGLIsEnabled(ctx, kCGLCESurfaceBackingSize, &enabled);
    printf("%s CGLEnable is remembered\n", enabled == 1 ? "ok  " : "FAIL"), failures += enabled != 1;

    // a 30 pixel wide surface pads its rows to 128 bytes, the upload must honour the stride
    put(d, kIOSurfaceWidth, 30), put(d, kIOSurfaceHeight, 20), put(d, kIOSurfaceBytesPerElement, 4);
    put(d, kIOSurfacePixelFormat, 'BGRA');
    s = IOSurfaceCreate(d);
    px = IOSurfaceGetBaseAddress(s);
    stride = IOSurfaceGetBytesPerRow(s) / 4;
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 30; x++)
            px[y * stride + x] = 0xff000000u | (uint32_t)(x * 8) << 16 | (uint32_t)(y * 12) << 8 | 0x40;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    CGLError err = CGLTexImageIOSurface2D(ctx, GL_TEXTURE_2D, GL_RGBA8, 30, 20, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, s, 0);
    printf("%s CGLTexImageIOSurface2D returned %d\n", err == kCGLNoError ? "ok  " : "FAIL", err), failures += err != kCGLNoError;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, out);
    int same = 1;
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 30; x++)
            same &= out[y * 30 + x] == px[y * stride + x];
    printf("%s the texture holds the surface's pixels (%08x at 29,19)\n", same ? "ok  " : "FAIL", out[19 * 30 + 29]);
    failures += !same;
    printf("iosurface-gl-test: %d failure%s\n", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
