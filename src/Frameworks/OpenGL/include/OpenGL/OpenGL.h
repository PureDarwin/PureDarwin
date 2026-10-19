/*
 * OpenGL.framework - CGL entry points.
 *
 * This is the header Apple's <OpenGL/OpenGL.h> occupies: it brings in CGL, not
 * GL. Include <OpenGL/gl.h> for the GL API itself.
 */

#ifndef _PUREDARWIN_OPENGL_OPENGL_H
#define _PUREDARWIN_OPENGL_OPENGL_H

#include <OpenGL/CGLTypes.h>

// the GL scalars these prototypes use, identical to gl.h's so either may come first
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;

#ifdef __cplusplus
extern "C" {
#endif

/* Pixel formats. Attributes are accepted and recorded; those the OSMesa
 * backend cannot honour are ignored rather than refused, so that callers
 * asking for e.g. an accelerated renderer still get a working context. */
CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs,
    CGLPixelFormatObj *pix, int *npix);
CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix);
CGLError CGLQueryRendererInfo(unsigned int display_mask, CGLRendererInfoObj *rend, int *nrend);
CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend);
CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, int rend_num, CGLRendererProperty prop, int *value);
CGLError CGLDescribePixelFormat(CGLPixelFormatObj pix, int pix_num,
    CGLPixelFormatAttribute attrib, int *value);
CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj pix);
void CGLReleasePixelFormat(CGLPixelFormatObj pix);
unsigned int CGLGetPixelFormatRetainCount(CGLPixelFormatObj pix);

/* Contexts. */
CGLError CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share,
    CGLContextObj *ctx);
CGLError CGLDestroyContext(CGLContextObj ctx);
CGLContextObj CGLRetainContext(CGLContextObj ctx);
void CGLReleaseContext(CGLContextObj ctx);
unsigned int CGLGetContextRetainCount(CGLContextObj ctx);
CGLError CGLSetCurrentContext(CGLContextObj ctx);
CGLContextObj CGLGetCurrentContext(void);
CGLPixelFormatObj CGLGetPixelFormat(CGLContextObj ctx);
CGLError CGLFlushDrawable(CGLContextObj ctx);
CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter parameter,
    const int *params);
CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter parameter,
    int *params);
CGLError CGLSetVirtualScreen(CGLContextObj ctx, int screen);
CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname);
CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname);
CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable);
// binds an IOSurface plane as the texture image of the context's current texture on target
typedef struct __IOSurface *IOSurfaceRef;
CGLError CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target, GLenum internal_format, GLsizei width,
                                GLsizei height, GLenum format, GLenum type, IOSurfaceRef ioSurface, GLuint plane);
CGLError CGLGetVirtualScreen(CGLContextObj ctx, int *screen);

// pixel buffers fail with kCGLBadCodeModule since contexts already render offscreen
// callers fall back to framebuffer objects
CGLError CGLCreatePBuffer(GLsizei width, GLsizei height, GLenum target, GLenum internalFormat, GLint max_level,
                          CGLPBufferObj *pbuffer);
CGLError CGLDestroyPBuffer(CGLPBufferObj pbuffer);
CGLPBufferObj CGLRetainPBuffer(CGLPBufferObj pbuffer);
void CGLReleasePBuffer(CGLPBufferObj pbuffer);
CGLError CGLSetPBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum face, GLint level, GLint screen);
CGLError CGLGetPBuffer(CGLContextObj ctx, CGLPBufferObj *pbuffer, GLenum *face, GLint *level, GLint *screen);
CGLError CGLTexImagePBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum source);

/* Version and errors. */
void CGLGetVersion(int *majorvers, int *minorvers);
const char *CGLErrorString(CGLError error);

#ifdef __cplusplus
}
#endif

/* Serialise access to a context shared between threads. AppKit's NSOpenGLView
 * brackets its drawing with these. */
extern CGLError CGLLockContext(CGLContextObj ctx);
extern CGLError CGLUnlockContext(CGLContextObj ctx);

#endif /* _PUREDARWIN_OPENGL_OPENGL_H */
