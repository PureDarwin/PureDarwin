#include <OpenGL/OpenGL.h>

/* Two backends. GLX is the original one and needs an X server; the EGL one is
 * for images built without X11 at all, and uses Mesa's surfaceless platform so
 * it needs no window system either - which suits CGL, whose contexts are always
 * offscreen pbuffers. */
#ifdef PD_CGL_USE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#else
#include <GL/glx.h>
#endif

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* Size of the backing buffer for a context. Large enough that a caller which
 * renders without ever setting a viewport still has somewhere sensible to draw,
 * small enough to be cheap for the common case of merely querying strings. */
#define PD_CGL_DEFAULT_WIDTH  256
#define PD_CGL_DEFAULT_HEIGHT 256

struct _CGLPixelFormatObject {
	unsigned int  retain_count;
	int           profile;          /* kCGLPFAOpenGLProfile value, 0 if unset */
	int           color_size;
	int           alpha_size;
	int           depth_size;
	int           stencil_size;
	int           accum_size;
	int           double_buffer;
	int           stereo;
	int           samples;
};

struct _CGLContextObject {
	unsigned int      retain_count;
#ifdef PD_CGL_USE_EGL
	EGLContext        glx;
	EGLSurface        pbuffer;
#else
	GLXContext        glx;
	GLXPbuffer        pbuffer;
#endif
	int               width;
	int               height;
	CGLPixelFormatObj pixel_format;
	int               virtual_screen;
	int               swap_interval;
};

static _Thread_local CGLContextObj pd_cgl_current;

/*
 * CGL contexts are offscreen, so each one is backed by a GLX pbuffer rather
 * than a window. That needs a display connection, which every context shares:
 * closing it while contexts remain alive would invalidate them, and CGL has no
 * "shut down" entry point where a refcounted one could be dropped.
 */
#ifdef PD_CGL_USE_EGL
static EGLDisplay      pd_cgl_display = EGL_NO_DISPLAY;
#else
static Display        *pd_cgl_display;
#endif
static pthread_once_t  pd_cgl_display_once = PTHREAD_ONCE_INIT;

static void
pd_cgl_open_display(void)
{
#ifdef PD_CGL_USE_EGL
	/* Surfaceless first: it is the only platform that is guaranteed to exist
	 * with no display server running at all. eglGetDisplay is the fallback for
	 * an EGL that does not advertise the extension. */
	pd_cgl_display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
	    EGL_DEFAULT_DISPLAY, NULL);
	if (pd_cgl_display == EGL_NO_DISPLAY) {
		pd_cgl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	}
	if (pd_cgl_display != EGL_NO_DISPLAY &&
	    !eglInitialize(pd_cgl_display, NULL, NULL)) {
		pd_cgl_display = EGL_NO_DISPLAY;
	}
#else
	pd_cgl_display = XOpenDisplay(NULL);
#endif
}

#ifdef PD_CGL_USE_EGL
static EGLDisplay
#else
static Display *
#endif
pd_cgl_get_display(void)
{
	pthread_once(&pd_cgl_display_once, pd_cgl_open_display);
	return pd_cgl_display;
}

/*
 * Attribute lists are NULL/0-terminated and mix bare flags with value pairs.
 * Anything not understood is skipped rather than rejected: a pbuffer cannot
 * honour kCGLPFAAccelerated or a display mask, but refusing the whole request over an
 * attribute that only expresses a preference would leave callers with no
 * context at all.
 */
static void
pd_cgl_parse_attributes(const CGLPixelFormatAttribute *attribs,
    struct _CGLPixelFormatObject *pix)
{
	if (attribs == NULL) {
		return;
	}

	for (const CGLPixelFormatAttribute *a = attribs; *a != 0; a++) {
		switch (*a) {
		/* Value-taking attributes: consume the following word. */
		case kCGLPFAOpenGLProfile:
			pix->profile = (int)*(++a);
			break;
		case kCGLPFAColorSize:
			pix->color_size = (int)*(++a);
			break;
		case kCGLPFAAlphaSize:
			pix->alpha_size = (int)*(++a);
			break;
		case kCGLPFADepthSize:
			pix->depth_size = (int)*(++a);
			break;
		case kCGLPFAStencilSize:
			pix->stencil_size = (int)*(++a);
			break;
		case kCGLPFAAccumSize:
			pix->accum_size = (int)*(++a);
			break;
		case kCGLPFASamples:
		case kCGLPFASampleBuffers:
			pix->samples = (int)*(++a);
			break;
		case kCGLPFARendererID:
		case kCGLPFADisplayMask:
		case kCGLPFAVirtualScreenCount:
			(void)*(++a);
			break;

		/* Boolean flags: no following word. */
		case kCGLPFADoubleBuffer:
			pix->double_buffer = 1;
			break;
		case kCGLPFAStereo:
			pix->stereo = 1;
			break;
		default:
			break;
		}
	}
}

CGLError
CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs,
    CGLPixelFormatObj *pix, int *npix)
{
	if (pix == NULL) {
		return kCGLBadAddress;
	}

	struct _CGLPixelFormatObject *p = calloc(1, sizeof(*p));
	if (p == NULL) {
		return kCGLBadAlloc;
	}

	p->retain_count = 1;
	p->color_size   = 32;
	p->alpha_size   = 8;
	p->depth_size   = 24;
	p->stencil_size = 8;
	pd_cgl_parse_attributes(attribs, p);

	*pix = p;
	if (npix != NULL) {
		*npix = 1;
	}
	return kCGLNoError;
}

CGLPixelFormatObj
CGLRetainPixelFormat(CGLPixelFormatObj pix)
{
	if (pix != NULL) {
		pix->retain_count++;
	}
	return pix;
}

void
CGLReleasePixelFormat(CGLPixelFormatObj pix)
{
	if (pix != NULL && --pix->retain_count == 0) {
		free(pix);
	}
}

CGLError
CGLDestroyPixelFormat(CGLPixelFormatObj pix)
{
	CGLReleasePixelFormat(pix);
	return kCGLNoError;
}

unsigned int
CGLGetPixelFormatRetainCount(CGLPixelFormatObj pix)
{
	return pix != NULL ? pix->retain_count : 0;
}

CGLError
CGLDescribePixelFormat(CGLPixelFormatObj pix, int pix_num,
    CGLPixelFormatAttribute attrib, int *value)
{
	(void)pix_num;

	if (pix == NULL) {
		return kCGLBadPixelFormat;
	}
	if (value == NULL) {
		return kCGLBadAddress;
	}

	switch (attrib) {
	case kCGLPFAOpenGLProfile:   *value = pix->profile;       break;
	case kCGLPFAColorSize:       *value = pix->color_size;    break;
	case kCGLPFAAlphaSize:       *value = pix->alpha_size;    break;
	case kCGLPFADepthSize:       *value = pix->depth_size;    break;
	case kCGLPFAStencilSize:     *value = pix->stencil_size;  break;
	case kCGLPFAAccumSize:       *value = pix->accum_size;    break;
	case kCGLPFADoubleBuffer:    *value = pix->double_buffer; break;
	case kCGLPFAStereo:          *value = pix->stereo;        break;
	case kCGLPFASamples:
	case kCGLPFASampleBuffers:   *value = pix->samples;       break;
	/* Software rendering: not accelerated, exactly one virtual screen. */
	case kCGLPFAAccelerated:        *value = 0; break;
	case kCGLPFAVirtualScreenCount: *value = 1; break;
	default:
		*value = 0;
		return kCGLBadAttribute;
	}
	return kCGLNoError;
}

CGLError
CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share, CGLContextObj *ctx)
{
	if (ctx == NULL) {
		return kCGLBadAddress;
	}

	struct _CGLContextObject *c = calloc(1, sizeof(*c));
	if (c == NULL) {
		return kCGLBadAlloc;
	}

#ifdef PD_CGL_USE_EGL
	EGLDisplay dpy = pd_cgl_get_display();
	if (dpy == EGL_NO_DISPLAY) {
		free(c);
		return kCGLBadConnection;
	}
#else
	Display *dpy = pd_cgl_get_display();
	if (dpy == NULL) {
		free(c);
		return kCGLBadConnection;
	}
#endif

	c->width  = PD_CGL_DEFAULT_WIDTH;
	c->height = PD_CGL_DEFAULT_HEIGHT;

	int depth   = pix != NULL ? pix->depth_size   : 24;
	int stencil = pix != NULL ? pix->stencil_size : 8;
	int accum   = pix != NULL ? pix->accum_size   : 0;

#ifdef PD_CGL_USE_EGL
	(void)accum;    /* no accumulation buffer in EGL configs */

	/* Desktop GL, not GLES - this is what makes libGL's dispatch table the one
	 * that gets populated when the context is made current. */
	if (!eglBindAPI(EGL_OPENGL_API)) {
		free(c);
		return kCGLBadPixelFormat;
	}

	EGLint cfg_attribs[] = {
		EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_RED_SIZE,        8,
		EGL_GREEN_SIZE,      8,
		EGL_BLUE_SIZE,       8,
		EGL_ALPHA_SIZE,      pix != NULL && pix->alpha_size ? pix->alpha_size : 8,
		EGL_DEPTH_SIZE,      depth,
		EGL_STENCIL_SIZE,    stencil,
		EGL_NONE
	};

	EGLConfig config;
	EGLint nconfigs = 0;
	if (!eglChooseConfig(dpy, cfg_attribs, &config, 1, &nconfigs) ||
	    nconfigs == 0) {
		free(c);
		return kCGLBadPixelFormat;
	}

	EGLint pb_attribs[] = {
		EGL_WIDTH,  c->width,
		EGL_HEIGHT, c->height,
		EGL_NONE
	};
	c->pbuffer = eglCreatePbufferSurface(dpy, config, pb_attribs);
	if (c->pbuffer == EGL_NO_SURFACE) {
		free(c);
		return kCGLBadAlloc;
	}

	/* As in the GLX path, a core-profile request is honoured so the version
	 * string matches what was asked for; anything else takes the default. */
	if (pix != NULL && pix->profile >= kCGLOGLPVersion_3_2_Core) {
		EGLint ctx_attribs[] = {
			EGL_CONTEXT_MAJOR_VERSION, (pix->profile >> 12) & 0xf,
			EGL_CONTEXT_MINOR_VERSION, (pix->profile >> 8)  & 0xf,
			EGL_CONTEXT_OPENGL_PROFILE_MASK,
			    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
			EGL_NONE
		};
		c->glx = eglCreateContext(dpy, config,
		    share != NULL ? share->glx : EGL_NO_CONTEXT, ctx_attribs);
	}

	if (c->glx == EGL_NO_CONTEXT || c->glx == NULL) {
		c->glx = eglCreateContext(dpy, config,
		    share != NULL ? share->glx : EGL_NO_CONTEXT, NULL);
	}

	if (c->glx == EGL_NO_CONTEXT) {
		eglDestroySurface(dpy, c->pbuffer);
		free(c);
		return kCGLBadContext;
	}
#else
	int fb_attribs[] = {
		GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
		GLX_RENDER_TYPE,   GLX_RGBA_BIT,
		GLX_RED_SIZE,      8,
		GLX_GREEN_SIZE,    8,
		GLX_BLUE_SIZE,     8,
		GLX_ALPHA_SIZE,    pix != NULL && pix->alpha_size ? pix->alpha_size : 8,
		GLX_DEPTH_SIZE,    depth,
		GLX_STENCIL_SIZE,  stencil,
		GLX_ACCUM_RED_SIZE, accum,
		GLX_DOUBLEBUFFER,  False,
		None
	};

	int nconfigs = 0;
	GLXFBConfig *configs = glXChooseFBConfig(dpy, DefaultScreen(dpy),
	    fb_attribs, &nconfigs);
	if (configs == NULL || nconfigs == 0) {
		if (configs != NULL) {
			XFree(configs);
		}
		free(c);
		return kCGLBadPixelFormat;
	}

	int pb_attribs[] = {
		GLX_PBUFFER_WIDTH,  c->width,
		GLX_PBUFFER_HEIGHT, c->height,
		GLX_LARGEST_PBUFFER, False,
		None
	};
	c->pbuffer = glXCreatePbuffer(dpy, configs[0], pb_attribs);
	if (c->pbuffer == None) {
		XFree(configs);
		free(c);
		return kCGLBadAlloc;
	}

	/* A core-profile request goes through glXCreateContextAttribsARB so the
	 * version string a caller sees matches what it asked for; anything else
	 * gets the default (compatibility) context. The entry point is resolved at
	 * runtime because it is an extension, not part of the GLX ABI. */
	if (pix != NULL && pix->profile >= kCGLOGLPVersion_3_2_Core) {
		PFNGLXCREATECONTEXTATTRIBSARBPROC create_attribs =
		    (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress(
		        (const GLubyte *)"glXCreateContextAttribsARB");
		if (create_attribs != NULL) {
			int ctx_attribs[] = {
				GLX_CONTEXT_MAJOR_VERSION_ARB, (pix->profile >> 12) & 0xf,
				GLX_CONTEXT_MINOR_VERSION_ARB, (pix->profile >> 8)  & 0xf,
				GLX_CONTEXT_PROFILE_MASK_ARB,
				    GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
				None
			};
			c->glx = create_attribs(dpy, configs[0],
			    share != NULL ? share->glx : NULL, True, ctx_attribs);
		}
	}

	if (c->glx == NULL) {
		c->glx = glXCreateNewContext(dpy, configs[0], GLX_RGBA_TYPE,
		    share != NULL ? share->glx : NULL, True);
	}

	XFree(configs);

	if (c->glx == NULL) {
		glXDestroyPbuffer(dpy, c->pbuffer);
		free(c);
		return kCGLBadContext;
	}
#endif

	c->retain_count = 1;
	c->pixel_format = CGLRetainPixelFormat(pix);

	*ctx = c;
	return kCGLNoError;
}

CGLContextObj
CGLRetainContext(CGLContextObj ctx)
{
	if (ctx != NULL) {
		ctx->retain_count++;
	}
	return ctx;
}

void
CGLReleaseContext(CGLContextObj ctx)
{
	if (ctx == NULL || --ctx->retain_count > 0) {
		return;
	}

#ifdef PD_CGL_USE_EGL
	EGLDisplay dpy = pd_cgl_get_display();

	if (pd_cgl_current == ctx) {
		if (dpy != EGL_NO_DISPLAY) {
			eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
			    EGL_NO_CONTEXT);
		}
		pd_cgl_current = NULL;
	}
	if (dpy != EGL_NO_DISPLAY) {
		eglDestroyContext(dpy, ctx->glx);
		eglDestroySurface(dpy, ctx->pbuffer);
	}
#else
	Display *dpy = pd_cgl_get_display();

	if (pd_cgl_current == ctx) {
		if (dpy != NULL) {
			glXMakeContextCurrent(dpy, None, None, NULL);
		}
		pd_cgl_current = NULL;
	}
	if (dpy != NULL) {
		glXDestroyContext(dpy, ctx->glx);
		glXDestroyPbuffer(dpy, ctx->pbuffer);
	}
#endif
	CGLReleasePixelFormat(ctx->pixel_format);
	free(ctx);
}

CGLError
CGLDestroyContext(CGLContextObj ctx)
{
	CGLReleaseContext(ctx);
	return kCGLNoError;
}

unsigned int
CGLGetContextRetainCount(CGLContextObj ctx)
{
	return ctx != NULL ? ctx->retain_count : 0;
}

CGLError
CGLSetCurrentContext(CGLContextObj ctx)
{
#ifdef PD_CGL_USE_EGL
	EGLDisplay dpy = pd_cgl_get_display();
	if (dpy == EGL_NO_DISPLAY) {
		return kCGLBadConnection;
	}

	if (ctx == NULL) {
		/* CGL has no "unbind" entry point of its own; a NULL context here is
		 * how callers release the current one. */
		eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		pd_cgl_current = NULL;
		return kCGLNoError;
	}

	/* The API binding is per-thread, so it has to be re-asserted here and not
	 * only where the context was created. */
	eglBindAPI(EGL_OPENGL_API);
	if (!eglMakeCurrent(dpy, ctx->pbuffer, ctx->pbuffer, ctx->glx)) {
		return kCGLBadContext;
	}
#else
	Display *dpy = pd_cgl_get_display();
	if (dpy == NULL) {
		return kCGLBadConnection;
	}

	if (ctx == NULL) {
		/* CGL has no "unbind" entry point of its own; a NULL context here is
		 * how callers release the current one. */
		glXMakeContextCurrent(dpy, None, None, NULL);
		pd_cgl_current = NULL;
		return kCGLNoError;
	}

	if (!glXMakeContextCurrent(dpy, ctx->pbuffer, ctx->pbuffer, ctx->glx)) {
		return kCGLBadContext;
	}
#endif

	pd_cgl_current = ctx;
	return kCGLNoError;
}

CGLContextObj
CGLGetCurrentContext(void)
{
	return pd_cgl_current;
}

CGLError
CGLGetPixelFormat(CGLContextObj ctx, CGLPixelFormatObj *pix)
{
	if (ctx == NULL) {
		return kCGLBadContext;
	}
	if (pix == NULL) {
		return kCGLBadAddress;
	}
	*pix = ctx->pixel_format;
	return kCGLNoError;
}

CGLError
CGLFlushDrawable(CGLContextObj ctx)
{
	if (ctx == NULL) {
		return kCGLBadContext;
	}
	/* Nothing to present to. Still flush so that rendering issued before this
	 * call has actually landed in the backing buffer, which is what a caller
	 * reading the buffer back afterwards expects. */
	glFinish();
	return kCGLNoError;
}

CGLError
CGLSetParameter(CGLContextObj ctx, CGLContextParameter parameter,
    const int *params)
{
	if (ctx == NULL) {
		return kCGLBadContext;
	}
	if (params == NULL) {
		return kCGLBadAddress;
	}

	switch (parameter) {
	case kCGLCPSwapInterval:
		ctx->swap_interval = params[0];
		return kCGLNoError;
	default:
		return kCGLBadEnumeration;
	}
}

CGLError
CGLGetParameter(CGLContextObj ctx, CGLContextParameter parameter, int *params)
{
	if (ctx == NULL) {
		return kCGLBadContext;
	}
	if (params == NULL) {
		return kCGLBadAddress;
	}

	switch (parameter) {
	case kCGLCPSwapInterval:
		params[0] = ctx->swap_interval;
		return kCGLNoError;
	case kCGLCPHasDrawable:
		params[0] = 0;          /* offscreen only */
		return kCGLNoError;
	default:
		return kCGLBadEnumeration;
	}
}

CGLError
CGLSetVirtualScreen(CGLContextObj ctx, int screen)
{
	if (ctx == NULL) {
		return kCGLBadContext;
	}
	if (screen != 0) {
		return kCGLBadValue;    /* one software renderer, screen 0 */
	}
	ctx->virtual_screen = screen;
	return kCGLNoError;
}

CGLError
CGLGetVirtualScreen(CGLContextObj ctx, int *screen)
{
	if (ctx == NULL) {
		return kCGLBadContext;
	}
	if (screen == NULL) {
		return kCGLBadAddress;
	}
	*screen = ctx->virtual_screen;
	return kCGLNoError;
}

void
CGLGetVersion(int *majorvers, int *minorvers)
{
	/* The CGL API level implemented, which is independent of the GL version
	 * the underlying Mesa context reports. */
	if (majorvers != NULL) {
		*majorvers = 1;
	}
	if (minorvers != NULL) {
		*minorvers = 6;
	}
}

const char *
CGLErrorString(CGLError error)
{
	switch (error) {
	case kCGLNoError:         return "no error";
	case kCGLBadAttribute:    return "invalid pixel format attribute";
	case kCGLBadProperty:     return "invalid renderer property";
	case kCGLBadPixelFormat:  return "invalid pixel format";
	case kCGLBadRendererInfo: return "invalid renderer info";
	case kCGLBadContext:      return "invalid context";
	case kCGLBadDrawable:     return "invalid drawable";
	case kCGLBadDisplay:      return "invalid graphics device";
	case kCGLBadState:        return "invalid context state";
	case kCGLBadValue:        return "invalid numerical value";
	case kCGLBadMatch:        return "invalid share context";
	case kCGLBadEnumeration:  return "invalid enumerant";
	case kCGLBadOffScreen:    return "invalid offscreen drawable";
	case kCGLBadFullScreen:   return "invalid fullscreen drawable";
	case kCGLBadWindow:       return "invalid window";
	case kCGLBadAddress:      return "invalid pointer";
	case kCGLBadCodeModule:   return "invalid code module";
	case kCGLBadAlloc:        return "invalid memory allocation";
	case kCGLBadConnection:   return "invalid CoreGraphics connection";
	default:                  return "unknown error";
	}
}
