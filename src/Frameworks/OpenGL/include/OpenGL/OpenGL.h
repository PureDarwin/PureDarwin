/*
 * OpenGL.framework - CGL entry points.
 *
 * This is the header Apple's <OpenGL/OpenGL.h> occupies: it brings in CGL, not
 * GL. Include <OpenGL/gl.h> for the GL API itself.
 */

#ifndef _PUREDARWIN_OPENGL_OPENGL_H
#define _PUREDARWIN_OPENGL_OPENGL_H

#include <OpenGL/CGLTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pixel formats. Attributes are accepted and recorded; those the OSMesa
 * backend cannot honour are ignored rather than refused, so that callers
 * asking for e.g. an accelerated renderer still get a working context. */
CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs,
    CGLPixelFormatObj *pix, int *npix);
CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix);
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
CGLError CGLGetPixelFormat(CGLContextObj ctx, CGLPixelFormatObj *pix);
CGLError CGLFlushDrawable(CGLContextObj ctx);
CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter parameter,
    const int *params);
CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter parameter,
    int *params);
CGLError CGLSetVirtualScreen(CGLContextObj ctx, int screen);
CGLError CGLGetVirtualScreen(CGLContextObj ctx, int *screen);

/* Version and errors. */
void CGLGetVersion(int *majorvers, int *minorvers);
const char *CGLErrorString(CGLError error);

#ifdef __cplusplus
}
#endif

#endif /* _PUREDARWIN_OPENGL_OPENGL_H */
