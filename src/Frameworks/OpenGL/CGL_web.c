// CGL_web.c - CGL for a web page, which has no desktop GL renderer: every pixel format and context is refused
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// PD's CGL (CGL.c) makes offscreen pbuffer contexts on desktop GL, a page only has WebGL. With no renderer,
// AppKit and QuartzCore draw in software, as they do on a machine without GL.
#include <OpenGL/OpenGL.h>

#include <stddef.h>

CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs, CGLPixelFormatObj *pix, int *npix)
{
    (void)attribs;
    if (pix)
        *pix = NULL;
    if (npix)
        *npix = 0;
    return kCGLBadPixelFormat;
}

CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix)
{
    (void)pix;
    return kCGLNoError;
}

CGLError CGLQueryRendererInfo(unsigned int display_mask, CGLRendererInfoObj *rend, int *nrend)
{
    (void)display_mask;
    if (rend)
        *rend = NULL;
    if (nrend)
        *nrend = 0;
    return kCGLBadDisplay;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend)
{
    (void)rend;
    return kCGLNoError;
}

CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, int rend_num, CGLRendererProperty prop, int *value)
{
    (void)rend;
    (void)rend_num;
    (void)prop;
    if (value)
        *value = 0;
    return kCGLBadDisplay;
}

CGLError CGLDescribePixelFormat(CGLPixelFormatObj pix, int pix_num, CGLPixelFormatAttribute attrib, int *value)
{
    (void)pix;
    (void)pix_num;
    (void)attrib;
    if (value)
        *value = 0;
    return kCGLBadPixelFormat;
}

CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj pix)
{
    return pix;
}

void CGLReleasePixelFormat(CGLPixelFormatObj pix)
{
    (void)pix;
}

unsigned int CGLGetPixelFormatRetainCount(CGLPixelFormatObj pix)
{
    (void)pix;
    return 0;
}

CGLError CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share, CGLContextObj *ctx)
{
    (void)pix;
    (void)share;
    if (ctx)
        *ctx = NULL;
    return kCGLBadPixelFormat;
}

CGLError CGLDestroyContext(CGLContextObj ctx)
{
    (void)ctx;
    return kCGLNoError;
}

CGLContextObj CGLRetainContext(CGLContextObj ctx)
{
    return ctx;
}

void CGLReleaseContext(CGLContextObj ctx)
{
    (void)ctx;
}

// no context is ever made, so there is never a current one
CGLError CGLSetCurrentContext(CGLContextObj ctx)
{
    return ctx == NULL ? kCGLNoError : kCGLBadContext;
}

CGLContextObj CGLGetCurrentContext(void)
{
    return NULL;
}

CGLPixelFormatObj CGLGetPixelFormat(CGLContextObj ctx)
{
    (void)ctx;
    return NULL;
}

CGLError CGLFlushDrawable(CGLContextObj ctx)
{
    (void)ctx;
    return kCGLBadContext;
}

CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter parameter, const int *params)
{
    (void)ctx;
    (void)parameter;
    (void)params;
    return kCGLBadContext;
}

CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter parameter, int *params)
{
    (void)ctx;
    (void)parameter;
    if (params)
        *params = 0;
    return kCGLBadContext;
}

CGLError CGLSetVirtualScreen(CGLContextObj ctx, int screen)
{
    (void)ctx;
    (void)screen;
    return kCGLBadContext;
}

CGLError CGLLockContext(CGLContextObj ctx)
{
    (void)ctx;
    return kCGLNoError;
}

CGLError CGLUnlockContext(CGLContextObj ctx)
{
    (void)ctx;
    return kCGLNoError;
}

// NSOpenGLContext hands this frame to its view, there is none
void *CGLPDPresentDrawable(CGLContextObj context)
{
    (void)context;
    return NULL;
}
