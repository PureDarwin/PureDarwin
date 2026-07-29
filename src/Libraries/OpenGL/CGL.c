#include <OpenGL/OpenGL.h>

#include <GL/osmesa.h>

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
	OSMesaContext     osmesa;
	void             *buffer;
	int               width;
	int               height;
	CGLPixelFormatObj pixel_format;
	int               virtual_screen;
	int               swap_interval;
};

static _Thread_local CGLContextObj pd_cgl_current;

/*
 * Attribute lists are NULL/0-terminated and mix bare flags with value pairs.
 * Anything not understood is skipped rather than rejected: OSMesa cannot honour
 * kCGLPFAAccelerated or a display mask, but refusing the whole request over an
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

	c->width  = PD_CGL_DEFAULT_WIDTH;
	c->height = PD_CGL_DEFAULT_HEIGHT;
	c->buffer = calloc((size_t)c->width * (size_t)c->height, 4);
	if (c->buffer == NULL) {
		free(c);
		return kCGLBadAlloc;
	}

	int depth   = pix != NULL ? pix->depth_size   : 24;
	int stencil = pix != NULL ? pix->stencil_size : 8;
	int accum   = pix != NULL ? pix->accum_size   : 0;

	/* A core-profile request maps onto OSMesa's core profile so that the
	 * version string a caller sees matches what it asked for; anything else
	 * gets the default (compatibility) context. */
	if (pix != NULL && pix->profile >= kCGLOGLPVersion_3_2_Core) {
		int major = (pix->profile >> 12) & 0xf;
		int minor = (pix->profile >> 8) & 0xf;
		c->osmesa = OSMesaCreateContextAttribs((const int[]) {
			OSMESA_FORMAT,                 OSMESA_RGBA,
			OSMESA_DEPTH_BITS,             depth,
			OSMESA_STENCIL_BITS,           stencil,
			OSMESA_ACCUM_BITS,             accum,
			OSMESA_PROFILE,                OSMESA_CORE_PROFILE,
			OSMESA_CONTEXT_MAJOR_VERSION,  major,
			OSMESA_CONTEXT_MINOR_VERSION,  minor,
			0
		}, share != NULL ? share->osmesa : NULL);
	}

	if (c->osmesa == NULL) {
		c->osmesa = OSMesaCreateContextExt(OSMESA_RGBA, depth, stencil, accum,
		    share != NULL ? share->osmesa : NULL);
	}

	if (c->osmesa == NULL) {
		free(c->buffer);
		free(c);
		return kCGLBadContext;
	}

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

	if (pd_cgl_current == ctx) {
		pd_cgl_current = NULL;
	}
	OSMesaDestroyContext(ctx->osmesa);
	CGLReleasePixelFormat(ctx->pixel_format);
	free(ctx->buffer);
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
	if (ctx == NULL) {
		/* CGL has no "unbind" entry point of its own; a NULL context here is
		 * how callers release the current one. OSMesa keeps its binding until
		 * something else is made current, so just drop our own record. */
		pd_cgl_current = NULL;
		return kCGLNoError;
	}

	if (!OSMesaMakeCurrent(ctx->osmesa, ctx->buffer, GL_UNSIGNED_BYTE,
	        ctx->width, ctx->height)) {
		return kCGLBadContext;
	}

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
