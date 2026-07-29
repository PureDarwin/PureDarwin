/*
 * OpenGL.framework - CGL types.
 */

#ifndef _PUREDARWIN_OPENGL_CGLTYPES_H
#define _PUREDARWIN_OPENGL_CGLTYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CGLContextObject      *CGLContextObj;
typedef struct _CGLPixelFormatObject  *CGLPixelFormatObj;
typedef struct _CGLRendererInfoObject *CGLRendererInfoObj;
typedef struct _CGLPBufferObject      *CGLPBufferObj;
typedef struct _CGLShareGroupObject   *CGLShareGroupObj;

typedef enum _CGLPixelFormatAttribute {
	kCGLPFAAllRenderers            =   1,
	kCGLPFADoubleBuffer            =   5,
	kCGLPFAStereo                  =   6,
	kCGLPFAColorSize               =   8,
	kCGLPFAAlphaSize               =  11,
	kCGLPFADepthSize               =  12,
	kCGLPFAStencilSize             =  13,
	kCGLPFAAccumSize               =  14,
	kCGLPFAMinimumPolicy           =  51,
	kCGLPFAMaximumPolicy           =  52,
	kCGLPFASampleBuffers           =  55,
	kCGLPFASamples                 =  56,
	kCGLPFAColorFloat              =  58,
	kCGLPFAMultisample             =  59,
	kCGLPFASupersample             =  60,
	kCGLPFASampleAlpha             =  61,
	kCGLPFARendererID              =  70,
	kCGLPFANoRecovery              =  72,
	kCGLPFAAccelerated             =  73,
	kCGLPFAClosestPolicy           =  74,
	kCGLPFABackingStore            =  76,
	kCGLPFADisplayMask             =  84,
	kCGLPFAAllowOfflineRenderers   =  96,
	kCGLPFAAcceleratedCompute      =  97,
	kCGLPFAOpenGLProfile           =  99,
	kCGLPFAVirtualScreenCount      = 128,
} CGLPixelFormatAttribute;

/* Values for kCGLPFAOpenGLProfile. */
enum {
	kCGLOGLPVersion_Legacy   = 0x1000,
	kCGLOGLPVersion_3_2_Core = 0x3200,
	kCGLOGLPVersion_GL3_Core = 0x3200,
	kCGLOGLPVersion_GL4_Core = 0x4100,
};

typedef enum _CGLContextParameter {
	kCGLCPSwapInterval      = 222,
	kCGLCPSurfaceOpacity    = 236,
	kCGLCPHasDrawable       = 508,
	kCGLCPCurrentRendererID = 309,
	kCGLCPGPUVertexProcessing   = 310,
	kCGLCPGPUFragmentProcessing = 311,
} CGLContextParameter;

typedef enum _CGLError {
	kCGLNoError            = 0,
	kCGLBadAttribute       = 10000,
	kCGLBadProperty        = 10001,
	kCGLBadPixelFormat     = 10002,
	kCGLBadRendererInfo    = 10003,
	kCGLBadContext         = 10004,
	kCGLBadDrawable        = 10005,
	kCGLBadDisplay         = 10006,
	kCGLBadState           = 10007,
	kCGLBadValue           = 10008,
	kCGLBadMatch           = 10009,
	kCGLBadEnumeration     = 10010,
	kCGLBadOffScreen       = 10011,
	kCGLBadFullScreen      = 10012,
	kCGLBadWindow          = 10013,
	kCGLBadAddress         = 10014,
	kCGLBadCodeModule      = 10015,
	kCGLBadAlloc           = 10016,
	kCGLBadConnection      = 10017,
} CGLError;

#ifdef __cplusplus
}
#endif

#endif /* _PUREDARWIN_OPENGL_CGLTYPES_H */
