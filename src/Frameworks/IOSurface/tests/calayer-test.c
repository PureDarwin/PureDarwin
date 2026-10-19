// SPDX-License-Identifier: MIT
// calayer-test: CARenderer draws layer backgrounds, rounded clipping, borders, hidden and flipped layers into an FBO
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { double x, y; } TPoint;
typedef struct { double x, y, w, h; } TRect;
typedef void *CGColorRef;

#define N 64

static void *(*msg)(void *, void *, ...);
static void *(*cls)(const char *);
static void *(*sel)(const char *);
static CGColorRef (*colorRGB)(double, double, double, double);
static int failures;

static void *makeLayer(TRect frame) {
    void *layer = msg(cls("CALayer"), sel("layer"));

    ((void (*)(void *, void *, TRect))msg)(layer, sel("setBounds:"), (TRect){ 0, 0, frame.w, frame.h });
    ((void (*)(void *, void *, TPoint))msg)(layer, sel("setPosition:"), (TPoint){ frame.x + frame.w / 2, frame.y + frame.h / 2 });
    return layer;
}

static void setColor(void *layer, const char *setter, double r, double g, double b) {
    ((void (*)(void *, void *, CGColorRef))msg)(layer, sel(setter), colorRGB(r, g, b, 1));
}

static void setFloat(void *layer, const char *setter, double value) {
    ((void (*)(void *, void *, double))msg)(layer, sel(setter), value);
}

static void setBool(void *layer, const char *setter, int value) {
    ((void (*)(void *, void *, signed char))msg)(layer, sel(setter), (signed char)value);
}

static void render(void *renderer) {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    ((void (*)(void *, void *, double, void *))msg)(renderer, sel("beginFrameAtTime:timeStamp:"), 0.0, NULL);
    msg(renderer, sel("render"));
    msg(renderer, sel("endFrame"));
}

static void expect(const char *what, int x, int y, int r, int g, int b, int a) {
    unsigned char px[4] = { 0 };

    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    int ok = abs(px[0] - r) <= 8 && abs(px[1] - g) <= 8 && abs(px[2] - b) <= 8 && abs(px[3] - a) <= 8;
    printf("%s %s at %d,%d: %d,%d,%d,%d (want %d,%d,%d,%d)\n", ok ? "ok  " : "FAIL", what, x, y,
           px[0], px[1], px[2], px[3], r, g, b, a);
    failures += !ok;
}

static int run(int withStencil) {
    CGLPixelFormatAttribute attrs[] = { kCGLPFAColorSize, 24, kCGLPFAAlphaSize, 8, 0 };
    CGLPixelFormatObj pix = NULL;
    CGLContextObj ctx = NULL;
    GLint npix = 0;
    GLuint fbo, color, depthStencil;

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
    if (withStencil) {
        glGenRenderbuffers(1, &depthStencil);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, N, N);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencil);
    }
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

    GLint bits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &bits);
    printf("-- %s, %d stencil bits\n", withStencil ? "stencil clipping" : "scissor clipping", bits);

    void *pool = msg(msg(cls("NSAutoreleasePool"), sel("alloc")), sel("init"));
    void *root = makeLayer((TRect){ 0, 0, N, N });
    void *renderer = msg(cls("CARenderer"), sel("rendererWithCGLContext:options:"), ctx, NULL);
    msg(renderer, sel("setLayer:"), root);

    // a background fills the layer
    setColor(root, "setBackgroundColor:", 0, 0, 1);
    render(renderer);
    expect("background", N / 2, N / 2, 0, 0, 255, 255);
    expect("background corner", 0, 0, 0, 0, 255, 255);

    // rounded corners cut the background, a sublayer covering everything is clipped to them
    setFloat(root, "setCornerRadius:", 16);
    render(renderer);
    expect("rounded background corner", 1, 1, 0, 0, 0, 0);
    void *child = makeLayer((TRect){ 0, 0, N, N });
    setColor(child, "setBackgroundColor:", 0, 1, 0);
    msg(root, sel("addSublayer:"), child);
    render(renderer);
    expect("unclipped sublayer corner", 1, 1, 0, 255, 0, 255);
    setBool(root, "setMasksToBounds:", 1);
    render(renderer);
    if (withStencil)
        expect("clipped sublayer corner", 1, 1, 0, 0, 0, 0);
    else
        expect("scissored sublayer corner", 1, 1, 0, 255, 0, 255);
    expect("sublayer centre", N / 2, N / 2, 0, 255, 0, 255);

    // the border sits on the edge above the sublayer
    setFloat(root, "setBorderWidth:", 4);
    setColor(root, "setBorderColor:", 1, 0, 0);
    render(renderer);
    expect("border bottom edge", N / 2, 1, 255, 0, 0, 255);
    expect("border left edge", 1, N / 2, 255, 0, 0, 255);
    expect("inside the border", N / 2, 8, 0, 255, 0, 255);

    // a hidden layer and its sublayers are not drawn
    setBool(child, "setHidden:", 1);
    render(renderer);
    expect("hidden sublayer", N / 2, N / 2, 0, 0, 255, 255);
    setBool(child, "setHidden:", 0);

    // a flipped layer puts its sublayers' origin at the top
    void *strip = makeLayer((TRect){ 0, 0, N, 16 });
    setColor(strip, "setBackgroundColor:", 1, 1, 0);
    msg(child, sel("addSublayer:"), strip);
    render(renderer);
    expect("strip at the bottom", N / 2, 8, 255, 255, 0, 255);
    setBool(child, "setGeometryFlipped:", 1);
    render(renderer);
    expect("flipped strip at the top", N / 2, N - 8, 255, 255, 0, 255);
    expect("flipped bottom shows the sublayer", N / 2, 8, 0, 255, 0, 255);

    // a fresh tree: a shadow below an opaque layer, then a mask cutting a layer down
    void *scene = makeLayer((TRect){ 0, 0, N, N });
    setColor(scene, "setBackgroundColor:", 0, 0, 1);
    msg(renderer, sel("setLayer:"), scene);
    void *card = makeLayer((TRect){ 16, 24, 32, 32 });
    setColor(card, "setBackgroundColor:", 1, 1, 1);
    setColor(card, "setShadowColor:", 0, 0, 0);
    ((void (*)(void *, void *, float))msg)(card, sel("setShadowOpacity:"), 1.0f);
    setFloat(card, "setShadowRadius:", 0);
    ((void (*)(void *, void *, TPoint))msg)(card, sel("setShadowOffset:"), (TPoint){ 0, -8 });
    msg(scene, sel("addSublayer:"), card);
    render(renderer);
    expect("shadow below the card", N / 2, 20, 0, 0, 0, 255);
    expect("card over its shadow", N / 2, 40, 255, 255, 255, 255);
    expect("no shadow beside the card", 8, 40, 0, 0, 255, 255);

    if (withStencil) {
        void *maskLayer = makeLayer((TRect){ 8, 8, 16, 16 });
        setColor(maskLayer, "setBackgroundColor:", 1, 1, 1);
        msg(card, sel("setMask:"), maskLayer);
        ((void (*)(void *, void *, float))msg)(card, sel("setShadowOpacity:"), 0.0f);
        render(renderer);
        expect("inside the mask", N / 2, 40, 255, 255, 255, 255);
        expect("outside the mask", 18, 26, 0, 0, 255, 255);
    }

    msg(pool, sel("drain"));
    CGLSetCurrentContext(NULL);
    return 0;
}

int main(void) {
    if (!dlopen("/System/Library/Frameworks/QuartzCore.framework/QuartzCore", RTLD_NOW | RTLD_GLOBAL)) {
        printf("FAIL no QuartzCore: %s\n", dlerror());
        return 1;
    }
    msg = dlsym(RTLD_DEFAULT, "objc_msgSend"), cls = dlsym(RTLD_DEFAULT, "objc_getClass");
    sel = dlsym(RTLD_DEFAULT, "sel_registerName");
    colorRGB = dlsym(RTLD_DEFAULT, "CGColorCreateSRGB");
    if (!colorRGB)
        colorRGB = dlsym(RTLD_DEFAULT, "CGColorCreateGenericRGB");
    if (!colorRGB) {
        printf("FAIL no CGColor constructor\n");
        return 1;
    }
    if (run(1) || run(0))
        return 1;
    printf("%d failures\n", failures);
    return failures != 0;
}
