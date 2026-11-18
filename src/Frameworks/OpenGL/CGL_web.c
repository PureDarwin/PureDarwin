// CGL_web.c - CGL for a web page: each context is a WebGL context on a hidden canvas of its own
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// PD's CGL (CGL.c) draws offscreen into pbuffers and AppKit reads each frame back into the view. Here the
// pbuffer is a canvas the page never shows, its size the context's backing size, and the frame is read back
// the same way. GL calls go to WebGL 1 through emscripten's legacy GL emulation, which covers fixed function.
#include <OpenGL/OpenGL.h>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _CGLPixelFormatObject {
    unsigned retain_count;
    int profile, color_size, alpha_size, depth_size, stencil_size, accum_size, samples, aux_buffers;
    int double_buffer, stereo, backing_store, pbuffer_only;
};

struct _CGLContextObject {
    unsigned retain_count;
    CGLPixelFormatObj pixel_format;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl;
    char canvas[32]; // the selector of its canvas, #pd-gl-N
    int width, height;
    int swap_interval, window_number, virtual_screen;
    int emulation_ready; // the fixed function emulation has its state in this context
};

static CGLContextObj current;
static unsigned next_canvas;

// a canvas for one context: in the page's DOM, as WebGL needs, but never shown
EM_JS(void, cgl_web_make_canvas, (const char *id, int width, int height), {
    var c = document.createElement('canvas');
    c.id = UTF8ToString(id);
    c.width = width;
    c.height = height;
    c.style.display = 'none';
    document.body.appendChild(c);
});

// emscripten sets up its fixed function emulation only for contexts it makes itself (EGL, SDL), so a CGL
// context runs that setup when it first becomes current, with GLctx its own
EM_JS(void, cgl_web_init_emulation, (void), {
    if (typeof GLImmediate === 'undefined')
        return;
    // the emulation skips its setup unless it believes it made a WebGL context
    if (typeof Browser !== 'undefined')
        Browser.useWebGL = true;
    GLImmediate.init();
});

EM_JS(void, cgl_web_remove_canvas, (const char *id), {
    var c = document.getElementById(UTF8ToString(id));
    if (c)
        c.remove();
});

CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs, CGLPixelFormatObj *pix, int *npix)
{
    if (pix == NULL)
        return kCGLBadAddress;
    CGLPixelFormatObj p = calloc(1, sizeof(*p));
    if (p == NULL)
        return kCGLBadAlloc;
    p->retain_count = 1;
    p->color_size = 32;
    p->alpha_size = 8;
    p->depth_size = 24;
    p->stencil_size = 8;
    for (const CGLPixelFormatAttribute *a = attribs; a != NULL && *a != 0; a++) {
        switch (*a) {
        // attributes that take the next word as their value
        case kCGLPFAOpenGLProfile: p->profile = (int)*(++a); break;
        case kCGLPFAColorSize: p->color_size = (int)*(++a); break;
        case kCGLPFAAlphaSize: p->alpha_size = (int)*(++a); break;
        case kCGLPFADepthSize: p->depth_size = (int)*(++a); break;
        case kCGLPFAStencilSize: p->stencil_size = (int)*(++a); break;
        case kCGLPFAAccumSize: p->accum_size = (int)*(++a); break;
        case kCGLPFASamples:
        case kCGLPFASampleBuffers: p->samples = (int)*(++a); break;
        case kCGLPFAAuxBuffers: p->aux_buffers = (int)*(++a); break;
        case kCGLPFARendererID:
        case kCGLPFADisplayMask:
        case kCGLPFAVirtualScreenCount: (void)*(++a); break;
        // flags
        case kCGLPFADoubleBuffer: p->double_buffer = 1; break;
        case kCGLPFAStereo: p->stereo = 1; break;
        case kCGLPFABackingStore: p->backing_store = 1; break;
        case kCGLPFAPBuffer: p->pbuffer_only = 1; break;
        case kCGLPFAWindow: p->pbuffer_only = 0; break;
        default: break;
        }
    }
    *pix = p;
    if (npix != NULL)
        *npix = 1;
    return kCGLNoError;
}

CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj pix)
{
    if (pix != NULL)
        pix->retain_count++;
    return pix;
}

void CGLReleasePixelFormat(CGLPixelFormatObj pix)
{
    if (pix != NULL && --pix->retain_count == 0)
        free(pix);
}

CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix)
{
    CGLReleasePixelFormat(pix);
    return kCGLNoError;
}

unsigned int CGLGetPixelFormatRetainCount(CGLPixelFormatObj pix)
{
    return pix != NULL ? pix->retain_count : 0;
}

CGLError CGLDescribePixelFormat(CGLPixelFormatObj pix, int pix_num, CGLPixelFormatAttribute attrib, int *value)
{
    (void)pix_num;
    if (pix == NULL)
        return kCGLBadPixelFormat;
    if (value == NULL)
        return kCGLBadAddress;
    switch (attrib) {
    case kCGLPFAOpenGLProfile: *value = pix->profile; break;
    case kCGLPFAColorSize: *value = pix->color_size; break;
    case kCGLPFAAlphaSize: *value = pix->alpha_size; break;
    case kCGLPFADepthSize: *value = pix->depth_size; break;
    case kCGLPFAStencilSize: *value = pix->stencil_size; break;
    case kCGLPFAAccumSize: *value = pix->accum_size; break;
    case kCGLPFASamples:
    case kCGLPFASampleBuffers: *value = pix->samples; break;
    case kCGLPFAAuxBuffers: *value = pix->aux_buffers; break;
    case kCGLPFADoubleBuffer: *value = pix->double_buffer; break;
    case kCGLPFAStereo: *value = pix->stereo; break;
    case kCGLPFABackingStore: *value = pix->backing_store; break;
    case kCGLPFAPBuffer: *value = 1; break;
    case kCGLPFAWindow: *value = !pix->pbuffer_only; break;
    case kCGLPFAAccelerated: *value = 1; break;
    case kCGLPFAVirtualScreenCount: *value = 1; break;
    default: *value = 0; break;
    }
    return kCGLNoError;
}

// one renderer, the browser's WebGL
struct _CGLRendererInfoObject {
    int unused;
};
static struct _CGLRendererInfoObject the_renderer;

CGLError CGLQueryRendererInfo(unsigned int display_mask, CGLRendererInfoObj *rend, int *nrend)
{
    (void)display_mask;
    if (rend == NULL || nrend == NULL)
        return kCGLBadAddress;
    *rend = &the_renderer;
    *nrend = 1;
    return kCGLNoError;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend)
{
    (void)rend;
    return kCGLNoError;
}

CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, int rend_num, CGLRendererProperty prop, int *value)
{
    if (rend == NULL || rend_num != 0)
        return kCGLBadRendererInfo;
    if (value == NULL)
        return kCGLBadAddress;
    switch (prop) {
    case kCGLRPAccelerated:
    case kCGLRPCompliant:
    case kCGLRPWindow:
    case kCGLRPOnline:
        *value = 1;
        break;
    case kCGLRPRendererCount:
        *value = 1;
        break;
    default:
        *value = 0;
        break;
    }
    return kCGLNoError;
}

CGLError CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share, CGLContextObj *ctx)
{
    // WebGL contexts cannot share objects, a share context is accepted and the new context stands alone
    (void)share;
    if (ctx == NULL)
        return kCGLBadAddress;
    *ctx = NULL;
    if (pix == NULL)
        return kCGLBadPixelFormat;

    CGLContextObj c = calloc(1, sizeof(*c));
    if (c == NULL)
        return kCGLBadAlloc;
    c->retain_count = 1;
    c->width = c->height = 1;
    snprintf(c->canvas, sizeof(c->canvas), "#pd-gl-%u", ++next_canvas);
    cgl_web_make_canvas(c->canvas + 1, 1, 1);

    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = pix->alpha_size > 0;
    attrs.depth = pix->depth_size > 0;
    attrs.stencil = pix->stencil_size > 0;
    attrs.antialias = pix->samples > 0;
    attrs.premultipliedAlpha = 0;
    // AppKit reads the frame back after the flush, the browser must not clear it first
    attrs.preserveDrawingBuffer = 1;
    attrs.majorVersion = 1;
    attrs.minorVersion = 0;
    attrs.enableExtensionsByDefault = 1;
    c->webgl = emscripten_webgl_create_context(c->canvas, &attrs);
    if (c->webgl <= 0) {
        cgl_web_remove_canvas(c->canvas + 1);
        free(c);
        return kCGLBadMatch;
    }
    c->pixel_format = CGLRetainPixelFormat(pix);
    *ctx = c;
    return kCGLNoError;
}

CGLContextObj CGLRetainContext(CGLContextObj ctx)
{
    if (ctx != NULL)
        ctx->retain_count++;
    return ctx;
}

void CGLReleaseContext(CGLContextObj ctx)
{
    if (ctx == NULL || --ctx->retain_count > 0)
        return;
    if (current == ctx) {
        emscripten_webgl_make_context_current(0);
        current = NULL;
    }
    emscripten_webgl_destroy_context(ctx->webgl);
    cgl_web_remove_canvas(ctx->canvas + 1);
    CGLReleasePixelFormat(ctx->pixel_format);
    free(ctx);
}

CGLError CGLDestroyContext(CGLContextObj ctx)
{
    CGLReleaseContext(ctx);
    return kCGLNoError;
}

unsigned int CGLGetContextRetainCount(CGLContextObj ctx)
{
    return ctx != NULL ? ctx->retain_count : 0;
}

CGLError CGLSetCurrentContext(CGLContextObj ctx)
{
    if (ctx == current)
        return kCGLNoError;
    if (emscripten_webgl_make_context_current(ctx != NULL ? ctx->webgl : 0) != EMSCRIPTEN_RESULT_SUCCESS)
        return kCGLBadContext;
    current = ctx;
    if (ctx != NULL && !ctx->emulation_ready) {
        cgl_web_init_emulation();
        ctx->emulation_ready = 1;
    }
    return kCGLNoError;
}

CGLContextObj CGLGetCurrentContext(void)
{
    return current;
}

CGLPixelFormatObj CGLGetPixelFormat(CGLContextObj ctx)
{
    return ctx != NULL ? ctx->pixel_format : NULL;
}

// the frame stays in the canvas (preserveDrawingBuffer) until AppKit reads it back
CGLError CGLFlushDrawable(CGLContextObj ctx)
{
    return ctx != NULL ? kCGLNoError : kCGLBadContext;
}

CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter parameter, const int *params)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (params == NULL)
        return kCGLBadAddress;
    switch (parameter) {
    case kCGLCPSwapInterval:
        ctx->swap_interval = params[0];
        return kCGLNoError;
    // the pbuffer is the canvas, its size is the drawable's
    case kCGLCPSurfaceBackingSize:
        if (params[0] <= 0 || params[1] <= 0)
            return kCGLBadValue;
        if (params[0] != ctx->width || params[1] != ctx->height) {
            emscripten_set_canvas_element_size(ctx->canvas, params[0], params[1]);
            ctx->width = params[0];
            ctx->height = params[1];
        }
        return kCGLNoError;
    case kCGLCPSurfaceWindowNumber:
        ctx->window_number = params[0];
        return kCGLNoError;
    // where the view sits and whether it shows are AppKit's business when it draws the frame
    case kCGLCPSurfaceBackingOrigin:
    case kCGLCPSurfaceHidden:
    case kCGLCPSurfaceOpacity:
        return kCGLNoError;
    default:
        return kCGLBadEnumeration;
    }
}

CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter parameter, int *params)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (params == NULL)
        return kCGLBadAddress;
    switch (parameter) {
    case kCGLCPSwapInterval:
        params[0] = ctx->swap_interval;
        return kCGLNoError;
    case kCGLCPHasDrawable:
        params[0] = ctx->window_number != 0;
        return kCGLNoError;
    case kCGLCPSurfaceBackingSize:
        params[0] = ctx->width;
        params[1] = ctx->height;
        return kCGLNoError;
    default:
        return kCGLBadEnumeration;
    }
}

CGLError CGLSetVirtualScreen(CGLContextObj ctx, int screen)
{
    if (ctx == NULL)
        return kCGLBadContext;
    return screen == 0 ? kCGLNoError : kCGLBadValue;
}

CGLError CGLGetVirtualScreen(CGLContextObj ctx, int *screen)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (screen == NULL)
        return kCGLBadAddress;
    *screen = ctx->virtual_screen;
    return kCGLNoError;
}

// the context enables CGL itself carries, none have a WebGL counterpart
CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname)
{
    (void)pname;
    return ctx != NULL ? kCGLNoError : kCGLBadContext;
}

CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname)
{
    (void)pname;
    return ctx != NULL ? kCGLNoError : kCGLBadContext;
}

CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable)
{
    (void)pname;
    if (ctx == NULL)
        return kCGLBadContext;
    if (enable != NULL)
        *enable = 0;
    return kCGLNoError;
}

void CGLGetVersion(int *majorvers, int *minorvers)
{
    if (majorvers != NULL)
        *majorvers = 1;
    if (minorvers != NULL)
        *minorvers = 6;
}

const char *CGLErrorString(CGLError error)
{
    switch (error) {
    case kCGLNoError: return "no error";
    case kCGLBadAttribute: return "invalid pixel format attribute";
    case kCGLBadProperty: return "invalid renderer property";
    case kCGLBadPixelFormat: return "invalid pixel format";
    case kCGLBadRendererInfo: return "invalid renderer info";
    case kCGLBadContext: return "invalid context";
    case kCGLBadDrawable: return "invalid drawable";
    case kCGLBadDisplay: return "invalid graphics device";
    case kCGLBadState: return "invalid context state";
    case kCGLBadValue: return "invalid numerical value";
    case kCGLBadMatch: return "invalid share context";
    case kCGLBadEnumeration: return "invalid enumerant";
    case kCGLBadOffScreen: return "invalid offscreen drawable";
    case kCGLBadFullScreen: return "invalid fullscreen drawable";
    case kCGLBadWindow: return "invalid window";
    case kCGLBadAddress: return "invalid pointer";
    case kCGLBadCodeModule: return "invalid code module";
    case kCGLBadAlloc: return "invalid memory allocation";
    case kCGLBadConnection: return "invalid CoreGraphics connection";
    default: return "unknown error";
    }
}

// one thread in a page, nothing to keep out
CGLError CGLLockContext(CGLContextObj ctx)
{
    return ctx != NULL ? kCGLNoError : kCGLBadContext;
}

CGLError CGLUnlockContext(CGLContextObj ctx)
{
    return ctx != NULL ? kCGLNoError : kCGLBadContext;
}

// no IOSurface to present: NSOpenGLContext reads the frame back and draws it into the view
void *CGLPDPresentDrawable(CGLContextObj context)
{
    (void)context;
    return NULL;
}
