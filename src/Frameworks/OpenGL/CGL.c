#include <OpenGL/OpenGL.h>

/* Two backends. GLX is the original one and needs an X server; the EGL one is
 * for images built without X11 at all, and uses Mesa's surfaceless platform so
 * it needs no window system either - which suits CGL, whose contexts are always
 * offscreen pbuffers. */
#ifdef PD_CGL_USE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#else
#include <GL/glx.h>
#endif

#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <time.h>

#ifdef PD_CGL_USE_EGL
// libEGL is opened at run time, so a dlsym on OpenGL.framework finds no eglGetProcAddress, as on macOS,
// and loaders such as LWJGL look GL functions up with dlsym instead of through EGL
#define PD_EGL_FUNCS(X) X(eglBindAPI) X(eglChooseConfig) X(eglCreateContext) X(eglCreatePbufferSurface) \
	X(eglDestroyContext) X(eglDestroySurface) X(eglGetDisplay) X(eglGetError) X(eglGetPlatformDisplay) \
	X(eglGetProcAddress) X(eglInitialize) X(eglMakeCurrent) X(eglQuerySurface)
#define PD_EGL_POINTER(f) static __typeof__(&f) pd_##f;
PD_EGL_FUNCS(PD_EGL_POINTER)
static pthread_once_t pd_egl_once = PTHREAD_ONCE_INIT;

static void pd_egl_open(void)
{
	void *egl = dlopen("/usr/lib/libEGL.1.dylib", RTLD_NOW | RTLD_LOCAL);

	if (egl == NULL) {
		fprintf(stderr, "CGL: cannot open libEGL: %s\n", dlerror());
		return;
	}
#define PD_EGL_LOOKUP(f) pd_##f = (__typeof__(pd_##f))dlsym(egl, #f);
	PD_EGL_FUNCS(PD_EGL_LOOKUP)
#undef PD_EGL_LOOKUP
}

#define PD_EGL_CALL(f) (pthread_once(&pd_egl_once, pd_egl_open), pd_##f)
#define eglBindAPI PD_EGL_CALL(eglBindAPI)
#define eglChooseConfig PD_EGL_CALL(eglChooseConfig)
#define eglCreateContext PD_EGL_CALL(eglCreateContext)
#define eglCreatePbufferSurface PD_EGL_CALL(eglCreatePbufferSurface)
#define eglDestroyContext PD_EGL_CALL(eglDestroyContext)
#define eglDestroySurface PD_EGL_CALL(eglDestroySurface)
#define eglGetDisplay PD_EGL_CALL(eglGetDisplay)
#define eglGetError PD_EGL_CALL(eglGetError)
#define eglGetPlatformDisplay PD_EGL_CALL(eglGetPlatformDisplay)
#define eglGetProcAddress PD_EGL_CALL(eglGetProcAddress)
#define eglInitialize PD_EGL_CALL(eglInitialize)
#define eglMakeCurrent PD_EGL_CALL(eglMakeCurrent)
#define eglQuerySurface PD_EGL_CALL(eglQuerySurface)

// GLImaging.c looks its functions up through the same run-time EGL
__attribute__((visibility("hidden"))) void *pd_cgl_egl_proc(const char *name)
{
	pthread_once(&pd_egl_once, pd_egl_open);
	return pd_eglGetProcAddress != NULL ? (void *)pd_eglGetProcAddress(name) : NULL;
}
#endif

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
	int           aux_buffers;
	int           backing_store;
	int           pbuffer_only;
};

struct _CGLContextObject {
	unsigned int      retain_count;
	pthread_mutex_t   lock;
	int               lock_ready;
#ifdef PD_CGL_USE_EGL
	EGLContext        glx;
	EGLSurface        pbuffer;
	EGLConfig         config;
	// once a view presents: two textures the frame is copied into, each shared as an EGLImage
	// and named by an IOSurface, and which of them takes the next frame
	int               presenting;
	GLuint            present_fbo[2], present_tex[2];
	void             *present_image[2];
	IOSurfaceRef      present_surface[2];
	int               present_next;
#else
	GLXContext        glx;
	GLXPbuffer        pbuffer;
	GLXFBConfig       config;
#endif
	int               width;
	int               height;
	// the thread the context is current on, a size asked for from any other thread waits for it
	pthread_t         current_thread;
	int               bound;
	int               pending_width;
	int               pending_height;
	// the window an NSOpenGLContext draws for, 0 while offscreen only
	int               window_number;
	CGLPixelFormatObj pixel_format;
	int               virtual_screen;
	// CGLEnable switches, one bit each for the values that fit
	unsigned long long enabled;
	int               swap_interval;
	// the context whose share group this one joined, itself when it shares with none
	struct _CGLContextObject *share_root;
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

#ifdef PD_CGL_USE_EGL
// just enough of vulkan's ABI to list devices without its headers
struct pd_vk_instance_info {
	uint32_t sType;
	const void *pNext;
	uint32_t flags;
	const void *pApplicationInfo;
	uint32_t enabledLayerCount;
	const char *const *ppEnabledLayerNames;
	uint32_t enabledExtensionCount;
	const char *const *ppEnabledExtensionNames;
};

// whether the vulkan loader offers a device that isn't a CPU one, the condition for zink
static int
pd_cgl_have_gpu_vulkan(void)
{
	typedef int32_t (*create_fn)(const struct pd_vk_instance_info *, const void *, void **);
	typedef void (*destroy_fn)(void *, const void *);
	typedef int32_t (*enum_fn)(void *, uint32_t *, void **);
	typedef void (*props_fn)(void *, void *);
	typedef void *(*gipa_fn)(void *, const char *);
	struct pd_vk_instance_info info = { .sType = 1 };
	void *lib = dlopen("libvulkan.1.dylib", RTLD_LAZY | RTLD_LOCAL);
	void *instance = NULL, *devices[16];
	uint32_t count = 16, i;
	int found = 0;

	if (lib == NULL)
		return 0;
	gipa_fn gipa = (gipa_fn)dlsym(lib, "vkGetInstanceProcAddr");
	create_fn create = gipa != NULL ? (create_fn)gipa(NULL, "vkCreateInstance") : NULL;

	if (create == NULL || create(&info, NULL, &instance) != 0 || instance == NULL) {
		dlclose(lib);
		return 0;
	}
	enum_fn enumerate = (enum_fn)gipa(instance, "vkEnumeratePhysicalDevices");
	props_fn props = (props_fn)gipa(instance, "vkGetPhysicalDeviceProperties");
	destroy_fn destroy = (destroy_fn)gipa(instance, "vkDestroyInstance");

	if (enumerate != NULL && props != NULL && enumerate(instance, &count, devices) >= 0) {
		for (i = 0; i < count && !found; i++) {
			// VkPhysicalDeviceProperties: deviceType follows four uint32s, 4 is CPU
			uint32_t p[1024] = { 0 };

			props(devices[i], p);
			found = p[4] != 4;
		}
	}
	if (destroy != NULL)
		destroy(instance, NULL);
	dlclose(lib);
	return found;
}

// PD_CGL_RENDERER=zink, virgl, llvmpipe, default or auto picks mesa's driver, auto takes zink when a GPU vulkan device exists
// default leaves it to mesa and loads no vulkan driver
static int
pd_cgl_pick_renderer(void)
{
	const char *r = getenv("PD_CGL_RENDERER");

	if (getenv("GALLIUM_DRIVER") != NULL || getenv("LIBGL_ALWAYS_SOFTWARE") != NULL)
		return 0;
	if (r != NULL && strcmp(r, "default") == 0)
		return 0;
	if (r != NULL && strcmp(r, "llvmpipe") == 0) {
		setenv("GALLIUM_DRIVER", "llvmpipe", 1);
		return 0;
	}
	if (r != NULL && strcmp(r, "virgl") == 0) {
		setenv("GALLIUM_DRIVER", "virpipe", 1);
		return 0;
	}
	if ((r != NULL && strcmp(r, "zink") == 0) || ((r == NULL || strcmp(r, "auto") == 0) && pd_cgl_have_gpu_vulkan())) {
		setenv("GALLIUM_DRIVER", "zink", 1);
		return 1;
	}
	return 0;
}
#endif

static void
pd_cgl_open_display(void)
{
#ifdef PD_CGL_USE_EGL
	int zink = pd_cgl_pick_renderer();

	/* Surfaceless first: it is the only platform that is guaranteed to exist
	 * with no display server running at all. eglGetDisplay is the fallback for
	 * an EGL that does not advertise the extension. */
	pd_cgl_display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
	    EGL_DEFAULT_DISPLAY, NULL);
	if (pd_cgl_display == EGL_NO_DISPLAY) {
		pd_cgl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	}
	if (pd_cgl_display != EGL_NO_DISPLAY && !eglInitialize(pd_cgl_display, NULL, NULL)) {
		// mesa doesn't fall back from a named driver, so a zink that fails gets the default list again
		if (zink)
			unsetenv("GALLIUM_DRIVER");
		if (!zink || !eglInitialize(pd_cgl_display, NULL, NULL))
			pd_cgl_display = EGL_NO_DISPLAY;
	}
	if (getenv("PD_CGL_TRACE") != NULL && pd_cgl_display != EGL_NO_DISPLAY) {
		const char *driver = getenv("GALLIUM_DRIVER");

		fprintf(stderr, "CGL: renderer %s\n", driver != NULL ? driver : "default");
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
		case kCGLPFAAuxBuffers:
			pix->aux_buffers = (int)*(++a);
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
		case kCGLPFABackingStore:
			pix->backing_store = 1;
			break;
		// a pbuffer request without kCGLPFAWindow is an offscreen-only format
		case kCGLPFAPBuffer:
			pix->pbuffer_only = 1;
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
	for (const CGLPixelFormatAttribute *a = attribs; a != NULL && *a != 0; a++) {
		if (*a == kCGLPFAWindow)
			p->pbuffer_only = 0;
	}

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
	// every context renders through mesa's driver, winemac drops renderers that report no acceleration
	case kCGLPFAAccelerated:        *value = 1; break;
	case kCGLPFAVirtualScreenCount: *value = 1; break;
	case kCGLPFAAuxBuffers:         *value = pix->aux_buffers;   break;
	case kCGLPFABackingStore:       *value = pix->backing_store; break;
	case kCGLPFAWindow:             *value = !pix->pbuffer_only; break;
	case kCGLPFAPBuffer:            *value = 1; break;
	case kCGLPFAColorFloat:         *value = 0; break;
	default:
		*value = 0;
		return kCGLBadAttribute;
	}
	return kCGLNoError;
}

#ifdef PD_CGL_USE_EGL
static void pd_cgl_stop_presenting(CGLContextObj ctx);
#endif

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

	c->config = config;
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
	c->config = configs[0];
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
	c->share_root = share != NULL ? share->share_root : c;

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
	pd_cgl_stop_presenting(ctx);
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

static CGLError pd_cgl_resize(CGLContextObj ctx, int width, int height);

// the context this thread leaves is no longer drawn into here
static void
pd_cgl_unbind_current(void)
{
	if (pd_cgl_current != NULL)
		pd_cgl_current->bound = 0;
	pd_cgl_current = NULL;
}

static void
pd_cgl_apply_pending_size(CGLContextObj ctx)
{
	if (ctx->pending_width > 0)
		pd_cgl_resize(ctx, ctx->pending_width, ctx->pending_height);
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
		pd_cgl_unbind_current();
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
		pd_cgl_unbind_current();
		return kCGLNoError;
	}

	if (!glXMakeContextCurrent(dpy, ctx->pbuffer, ctx->pbuffer, ctx->glx)) {
		return kCGLBadContext;
	}
#endif

	pd_cgl_unbind_current();
	pd_cgl_current = ctx;
	ctx->current_thread = pthread_self();
	ctx->bound = 1;
	pd_cgl_apply_pending_size(ctx);
	return kCGLNoError;
}

CGLContextObj
CGLGetCurrentContext(void)
{
	return pd_cgl_current;
}

// Apple's signature returns the format itself
CGLPixelFormatObj
CGLGetPixelFormat(CGLContextObj ctx)
{
	return ctx != NULL ? ctx->pixel_format : NULL;
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
	if (pd_cgl_current == ctx)
		pd_cgl_apply_pending_size(ctx);
	return kCGLNoError;
}

// a view's size for the drawable: a new pbuffer of that size, bound again if the context is current
static CGLError
pd_cgl_resize(CGLContextObj ctx, int width, int height)
{
	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;
	// another thread may be drawing into the surface, it swaps to the new size when it next binds or flushes
	if (ctx->bound && !pthread_equal(ctx->current_thread, pthread_self())) {
		ctx->pending_width = width;
		ctx->pending_height = height;
		return kCGLNoError;
	}
	ctx->pending_width = 0;
	ctx->pending_height = 0;
	if (width == ctx->width && height == ctx->height)
		return kCGLNoError;

#ifdef PD_CGL_USE_EGL
	EGLDisplay dpy = pd_cgl_get_display();
	EGLint pb_attribs[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };
	EGLSurface pbuffer = eglCreatePbufferSurface(dpy, ctx->config, pb_attribs);

	if (pbuffer == EGL_NO_SURFACE)
		return kCGLBadAlloc;
	if (pd_cgl_current == ctx && !eglMakeCurrent(dpy, pbuffer, pbuffer, ctx->glx)) {
		eglDestroySurface(dpy, pbuffer);
		return kCGLBadContext;
	}
	// PD_CGL_TRACE=1 logs each drawable size change
	if (getenv("PD_CGL_TRACE") != NULL) {
		EGLint w = 0, h = 0;

		eglQuerySurface(dpy, pbuffer, EGL_WIDTH, &w);
		eglQuerySurface(dpy, pbuffer, EGL_HEIGHT, &h);
		fprintf(stderr, "CGL: drawable %dx%d (asked %dx%d) current %d error %x\n", w, h, width, height,
		    pd_cgl_current == ctx, eglGetError());
	}
	// the next flush makes textures of the new size
	pd_cgl_stop_presenting(ctx);
	eglDestroySurface(dpy, ctx->pbuffer);
#else
	Display *dpy = pd_cgl_get_display();
	int pb_attribs[] = {
		GLX_PBUFFER_WIDTH, width, GLX_PBUFFER_HEIGHT, height, GLX_LARGEST_PBUFFER, False, None
	};
	GLXPbuffer pbuffer = glXCreatePbuffer(dpy, ctx->config, pb_attribs);

	if (pbuffer == None)
		return kCGLBadAlloc;
	if (pd_cgl_current == ctx)
		glXMakeContextCurrent(dpy, pbuffer, pbuffer, ctx->glx);
	glXDestroyPbuffer(dpy, ctx->pbuffer);
#endif
	ctx->pbuffer = pbuffer;
	ctx->width = width;
	ctx->height = height;
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
	case kCGLCPSurfaceBackingSize:
		return pd_cgl_resize(ctx, params[0], params[1]);
	case kCGLCPSurfaceWindowNumber:
		ctx->window_number = params[0];
		return kCGLNoError;
	// where the view sits and whether it shows are AppKit's business when it presents
	case kCGLCPSurfaceBackingOrigin:
	case kCGLCPSurfaceHidden:
	case kCGLCPSurfaceOpacity:
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

// the switches are remembered for CGLIsEnabled, Mesa's pbuffer contexts have no behaviour to change
static int pd_cgl_enable_bit(CGLContextEnable pname)
{
	switch (pname) {
	case kCGLCESwapRectangle: return 0;
	case kCGLCESwapLimit: return 1;
	case kCGLCERasterization: return 2;
	case kCGLCEStateValidation: return 3;
	case kCGLCESurfaceBackingSize: return 4;
	case kCGLCEDisplayListOptimization: return 5;
	case kCGLCEMPEngine: return 6;
	case kCGLCECrashOnRemovedFunctions: return 7;
	default: return -1;
	}
}

CGLError
CGLEnable(CGLContextObj ctx, CGLContextEnable pname)
{
	int bit = pd_cgl_enable_bit(pname);

	if (ctx == NULL)
		return kCGLBadContext;
	if (bit < 0)
		return kCGLBadEnumeration;
	ctx->enabled |= 1ULL << bit;
	return kCGLNoError;
}

CGLError
CGLDisable(CGLContextObj ctx, CGLContextEnable pname)
{
	int bit = pd_cgl_enable_bit(pname);

	if (ctx == NULL)
		return kCGLBadContext;
	if (bit < 0)
		return kCGLBadEnumeration;
	ctx->enabled &= ~(1ULL << bit);
	return kCGLNoError;
}

CGLError
CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable)
{
	int bit = pd_cgl_enable_bit(pname);

	if (ctx == NULL)
		return kCGLBadContext;
	if (enable == NULL)
		return kCGLBadAddress;
	if (bit < 0)
		return kCGLBadEnumeration;
	*enable = (ctx->enabled >> bit) & 1;
	return kCGLNoError;
}

// IOSurface is found at run time, so OpenGL keeps no link dependency on it
static struct {
	int (*lock)(IOSurfaceRef, uint32_t, uint32_t *);
	int (*unlock)(IOSurfaceRef, uint32_t, uint32_t *);
	void *(*base)(IOSurfaceRef, size_t);
	size_t (*rowBytes)(IOSurfaceRef, size_t);
	size_t (*planeCount)(IOSurfaceRef);
	size_t (*elementBytes)(IOSurfaceRef, size_t);
} pd_iosurface;

static void pd_cgl_load_iosurface(void)
{
	void *lib = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_LAZY | RTLD_GLOBAL);

	if (lib == NULL)
		return;
	pd_iosurface.lock = dlsym(lib, "IOSurfaceLock");
	pd_iosurface.unlock = dlsym(lib, "IOSurfaceUnlock");
	pd_iosurface.base = dlsym(lib, "IOSurfaceGetBaseAddressOfPlane");
	pd_iosurface.rowBytes = dlsym(lib, "IOSurfaceGetBytesPerRowOfPlane");
	pd_iosurface.planeCount = dlsym(lib, "IOSurfaceGetPlaneCount");
	pd_iosurface.elementBytes = dlsym(lib, "IOSurfaceGetBytesPerElementOfPlane");
}

#ifdef PD_CGL_USE_EGL
// an IOSurface texture is a copy, so draws into it mark it and glFlush, glFinish and CGLFlushDrawable
// write it back. callers that dlsym GL from OpenGL.framework, as Gecko does, get these wrappers.
struct pd_cgl_surftex {
	void         *group;
	GLenum        target;
	GLuint        texture;
	IOSurfaceRef  surface;
	GLuint        plane;
	GLsizei       width, height;
	GLenum        format, type;
	size_t        row_bytes, element_bytes;
	int           dirty;
	// the pixel buffer a batched write-back copies through, 0 until first used
	GLuint        pbo;
};

struct pd_cgl_fbo {
	void   *group;
	GLuint  fbo;
	int     surftex;
};

static pthread_mutex_t pd_cgl_track_lock = PTHREAD_MUTEX_INITIALIZER;
static struct pd_cgl_surftex *pd_cgl_surftexs;
static int pd_cgl_surftex_count, pd_cgl_surftex_cap;
static struct pd_cgl_fbo *pd_cgl_fbos;
static int pd_cgl_fbo_count, pd_cgl_fbo_cap;
// PD_CGL_TRACE: write-back totals, printed every two seconds
static struct {
	double since, busy;
	long flushes, tiles, bytes;
} pd_cgl_stats;
static const void *(*pd_cf_retain)(const void *);
static void (*pd_cf_release)(const void *);

// a view's frame is copied on the GPU into one of two textures, shared as EGLImages, and an IOSurface
// names it: a renderer in this process samples the same storage, so nothing goes through the CPU
struct pd_cgl_drawable {
	IOSurfaceRef surface;
	EGLImage     image;
};

static pthread_mutex_t pd_cgl_drawable_lock = PTHREAD_MUTEX_INITIALIZER;
static struct pd_cgl_drawable pd_cgl_drawables[64];
static int pd_cgl_drawable_count;
static pthread_once_t pd_cgl_present_once = PTHREAD_ONCE_INIT;
static struct {
	IOSurfaceRef (*create)(const void *);
	const void **width, **height, **bytesPerElement, **pixelFormat;
	void *(*dictionary)(const void *, long, const void *, const void *);
	void (*set)(void *, const void *, const void *);
	const void *(*number)(const void *, long, const void *);
	const void *keyCallBacks, *valueCallBacks;
	PFNEGLCREATEIMAGEKHRPROC createImage;
	PFNEGLDESTROYIMAGEKHRPROC destroyImage;
	void (*targetTexture)(GLenum, void *);
} pd_present;

static void
pd_cgl_load_present(void)
{
	void *lib = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_LAZY | RTLD_GLOBAL);

	pd_cgl_load_iosurface();
	if (lib == NULL)
		return;
	pd_present.create = dlsym(lib, "IOSurfaceCreate");
	pd_present.width = dlsym(lib, "kIOSurfaceWidth");
	pd_present.height = dlsym(lib, "kIOSurfaceHeight");
	pd_present.bytesPerElement = dlsym(lib, "kIOSurfaceBytesPerElement");
	pd_present.pixelFormat = dlsym(lib, "kIOSurfacePixelFormat");
	pd_present.dictionary = dlsym(RTLD_DEFAULT, "CFDictionaryCreateMutable");
	pd_present.set = dlsym(RTLD_DEFAULT, "CFDictionarySetValue");
	pd_present.number = dlsym(RTLD_DEFAULT, "CFNumberCreate");
	pd_present.keyCallBacks = dlsym(RTLD_DEFAULT, "kCFTypeDictionaryKeyCallBacks");
	pd_present.valueCallBacks = dlsym(RTLD_DEFAULT, "kCFTypeDictionaryValueCallBacks");
	pd_present.createImage = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
	pd_present.destroyImage = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
	pd_present.targetTexture = (void (*)(GLenum, void *))eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if (pd_cf_retain == NULL) {
		pd_cf_retain = (const void *(*)(const void *))dlsym(RTLD_DEFAULT, "CFRetain");
		pd_cf_release = (void (*)(const void *))dlsym(RTLD_DEFAULT, "CFRelease");
	}
}

// a BGRA surface of the drawable's size, its memory stays untouched while the GL path works
static IOSurfaceRef
pd_cgl_create_surface(int width, int height)
{
	const void *keys[4] = { *pd_present.width, *pd_present.height, *pd_present.bytesPerElement,
		*pd_present.pixelFormat };
	int values[4] = { width, height, 4, 'BGRA' };
	IOSurfaceRef surface;
	void *properties;
	int i;

	properties = pd_present.dictionary(NULL, 4, pd_present.keyCallBacks, pd_present.valueCallBacks);
	if (properties == NULL)
		return NULL;
	for (i = 0; i < 4; i++) {
		// 9 is kCFNumberIntType
		const void *number = pd_present.number(NULL, 9, &values[i]);

		pd_present.set(properties, keys[i], number);
		pd_cf_release(number);
	}
	surface = pd_present.create(properties);
	pd_cf_release(properties);
	return surface;
}

static void
pd_cgl_forget_drawable(IOSurfaceRef surface)
{
	int i;

	for (i = 0; i < pd_cgl_drawable_count; i++) {
		if (pd_cgl_drawables[i].surface == surface) {
			pd_cgl_drawables[i] = pd_cgl_drawables[--pd_cgl_drawable_count];
			return;
		}
	}
}

// textures and framebuffers stay for the next size, images and surfaces go
static void
pd_cgl_stop_presenting(CGLContextObj ctx)
{
	int i;

	if (!ctx->presenting)
		return;
	pthread_mutex_lock(&pd_cgl_drawable_lock);
	for (i = 0; i < 2; i++)
		pd_cgl_forget_drawable(ctx->present_surface[i]);
	pthread_mutex_unlock(&pd_cgl_drawable_lock);
	for (i = 0; i < 2; i++) {
		pd_present.destroyImage(pd_cgl_get_display(), ctx->present_image[i]);
		pd_cf_release(ctx->present_surface[i]);
		ctx->present_image[i] = NULL;
		ctx->present_surface[i] = NULL;
	}
	ctx->presenting = 0;
}

static int
pd_cgl_start_presenting(CGLContextObj ctx)
{
	EGLDisplay dpy = pd_cgl_get_display();
	GLint texture = 0, fbo = 0;
	int i, ok = 1;

	pthread_once(&pd_cgl_present_once, pd_cgl_load_present);
	if (pd_present.create == NULL || pd_present.dictionary == NULL || pd_present.number == NULL ||
	    pd_cf_release == NULL || pd_iosurface.lock == NULL || pd_present.createImage == NULL ||
	    pd_present.destroyImage == NULL || pd_present.targetTexture == NULL || pd_cgl_drawable_count + 2 > 64)
		return 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
	if (ctx->present_tex[0] == 0) {
		glGenTextures(2, ctx->present_tex);
		glGenFramebuffers(2, ctx->present_fbo);
	}
	for (i = 0; i < 2; i++) {
		// RGB storage so the layer is opaque whatever alpha the app left in its drawable
		glBindTexture(GL_TEXTURE_2D, ctx->present_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, ctx->width, ctx->height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->present_fbo[i]);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->present_tex[i], 0);
		ok = ok && glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	}
	glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)fbo);
	for (i = 0; i < 2 && ok; i++) {
		ctx->present_image[i] = pd_present.createImage(dpy, ctx->glx, EGL_GL_TEXTURE_2D_KHR,
		    (EGLClientBuffer)(uintptr_t)ctx->present_tex[i], NULL);
		ctx->present_surface[i] = pd_cgl_create_surface(ctx->width, ctx->height);
		ok = ctx->present_image[i] != EGL_NO_IMAGE_KHR && ctx->present_surface[i] != NULL;
	}
	if (!ok) {
		if (getenv("PD_CGL_TRACE") != NULL)
			fprintf(stderr, "CGL: no shared present textures, error %x\n", eglGetError());
		for (i = 0; i < 2; i++) {
			if (ctx->present_image[i] != NULL)
				pd_present.destroyImage(dpy, ctx->present_image[i]);
			if (ctx->present_surface[i] != NULL)
				pd_cf_release(ctx->present_surface[i]);
			ctx->present_image[i] = NULL;
			ctx->present_surface[i] = NULL;
		}
		return 0;
	}
	pthread_mutex_lock(&pd_cgl_drawable_lock);
	for (i = 0; i < 2; i++)
		pd_cgl_drawables[pd_cgl_drawable_count++] = (struct pd_cgl_drawable){ ctx->present_surface[i],
			ctx->present_image[i] };
	pthread_mutex_unlock(&pd_cgl_drawable_lock);
	ctx->presenting = 1;
	return 1;
}

// called after CGLFlushDrawable with ctx current, returns the surface to show or NULL to read back instead.
// the blit turns GL's bottom-up rows into the top-down rows a layer expects
IOSurfaceRef
CGLPDPresentDrawable(CGLContextObj ctx)
{
	static int disabled = -1;
	GLint read_fbo = 0, draw_fbo = 0;
	GLboolean scissor;
	IOSurfaceRef surface;
	int i;

	if (disabled < 0)
		disabled = getenv("PD_CGL_NO_PRESENT") != NULL;
	if (disabled || ctx == NULL || ctx != pd_cgl_current)
		return NULL;
	if (!ctx->presenting && !pd_cgl_start_presenting(ctx)) {
		disabled = 1;
		return NULL;
	}
	i = ctx->present_next;
	ctx->present_next = !i;
	scissor = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->present_fbo[i]);
	glDisable(GL_SCISSOR_TEST);
	glBlitFramebuffer(0, 0, ctx->width, ctx->height, 0, ctx->height, ctx->width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	if (scissor)
		glEnable(GL_SCISSOR_TEST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fbo);
	glFinish();
	surface = ctx->present_surface[i];
	// a write unlock moves the seed, which tells a renderer the frame changed
	pd_iosurface.lock(surface, 0, NULL);
	pd_iosurface.unlock(surface, 0, NULL);
	return surface;
}

// with the renderer's context current and its texture bound to GL_TEXTURE_2D, points that texture at
// the frame the surface names. returns 0 for other surfaces
int
CGLPDBindSurfaceTexture(IOSurfaceRef surface, int bind)
{
	int i, found = 0;

	if (surface == NULL || pd_cgl_drawable_count == 0)
		return 0;
	pthread_mutex_lock(&pd_cgl_drawable_lock);
	for (i = 0; i < pd_cgl_drawable_count; i++) {
		if (pd_cgl_drawables[i].surface != surface)
			continue;
		found = 1;
		if (bind) {
			glGetError();
			pd_present.targetTexture(GL_TEXTURE_2D, pd_cgl_drawables[i].image);
			found = glGetError() == GL_NO_ERROR;
		}
		break;
	}
	pthread_mutex_unlock(&pd_cgl_drawable_lock);
	return found;
}

// the real entry point behind one of the wrappers below
#define PD_GL_REAL(name) \
	static __typeof__(name) *pd_real_##name; \
	if (pd_real_##name == NULL) \
		pd_real_##name = (__typeof__(name) *)eglGetProcAddress(#name)

static void *
pd_cgl_group(void)
{
	return pd_cgl_current != NULL ? (void *)pd_cgl_current->share_root : NULL;
}

static int
pd_cgl_find_surftex(void *group, GLuint texture)
{
	int i;

	for (i = 0; i < pd_cgl_surftex_count; i++) {
		if (pd_cgl_surftexs[i].group == group && pd_cgl_surftexs[i].texture == texture)
			return i;
	}
	return -1;
}

static void
pd_cgl_drop_surftex(int i)
{
	int j;

	if (pd_cf_release != NULL)
		pd_cf_release(pd_cgl_surftexs[i].surface);
	if (pd_cgl_surftexs[i].pbo != 0)
		glDeleteBuffers(1, &pd_cgl_surftexs[i].pbo);
	pd_cgl_surftexs[i] = pd_cgl_surftexs[--pd_cgl_surftex_count];
	// the moved entry keeps its framebuffers, the dropped one loses them
	for (j = 0; j < pd_cgl_fbo_count; j++) {
		if (pd_cgl_fbos[j].surftex == i)
			pd_cgl_fbos[j].surftex = -1;
		else if (pd_cgl_fbos[j].surftex == pd_cgl_surftex_count)
			pd_cgl_fbos[j].surftex = i;
	}
}

static void
pd_cgl_track_surface(CGLContextObj ctx, GLenum target, IOSurfaceRef surface, GLuint plane, GLsizei width,
    GLsizei height, GLenum format, GLenum type, size_t row_bytes, size_t element_bytes)
{
	GLint texture = 0;
	int i;

	if (target == GL_TEXTURE_RECTANGLE)
		glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &texture);
	else if (target == GL_TEXTURE_2D)
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
	if (texture == 0)
		return;
	if (pd_cf_retain == NULL) {
		pd_cf_retain = (const void *(*)(const void *))dlsym(RTLD_DEFAULT, "CFRetain");
		pd_cf_release = (void (*)(const void *))dlsym(RTLD_DEFAULT, "CFRelease");
	}
	pthread_mutex_lock(&pd_cgl_track_lock);
	i = pd_cgl_find_surftex(ctx->share_root, (GLuint)texture);
	GLuint pbo = i >= 0 ? pd_cgl_surftexs[i].pbo : 0;

	if (i >= 0 && pd_cf_release != NULL)
		pd_cf_release(pd_cgl_surftexs[i].surface);
	if (i < 0) {
		if (pd_cgl_surftex_count == pd_cgl_surftex_cap) {
			int cap = pd_cgl_surftex_cap ? pd_cgl_surftex_cap * 2 : 32;
			void *grown = realloc(pd_cgl_surftexs, cap * sizeof(*pd_cgl_surftexs));

			if (grown == NULL) {
				pthread_mutex_unlock(&pd_cgl_track_lock);
				return;
			}
			pd_cgl_surftexs = grown;
			pd_cgl_surftex_cap = cap;
		}
		i = pd_cgl_surftex_count++;
	}
	if (pd_cf_retain != NULL)
		pd_cf_retain(surface);
	pd_cgl_surftexs[i] = (struct pd_cgl_surftex){ ctx->share_root, target, (GLuint)texture, surface, plane,
		width, height, format, type, row_bytes, element_bytes, 0, pbo };
	if (getenv("PD_CGL_TRACE") != NULL && pd_cgl_surftex_count <= 8)
		fprintf(stderr, "CGL: surface texture %u %dx%d target %x format %x type %x\n", (unsigned)texture, width, height,
		    target, format, type);
	pthread_mutex_unlock(&pd_cgl_track_lock);
}

// marks the IOSurface texture the current draw framebuffer renders into
static void
pd_cgl_mark_drawn(void)
{
	GLint fbo = 0;
	void *group = pd_cgl_group();
	int i;

	if (pd_cgl_surftex_count == 0 || group == NULL)
		return;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
	if (fbo == 0)
		return;
	pthread_mutex_lock(&pd_cgl_track_lock);
	for (i = 0; i < pd_cgl_fbo_count; i++) {
		if (pd_cgl_fbos[i].group == group && pd_cgl_fbos[i].fbo == (GLuint)fbo) {
			if (pd_cgl_fbos[i].surftex >= 0)
				pd_cgl_surftexs[pd_cgl_fbos[i].surftex].dirty = 1;
			break;
		}
	}
	pthread_mutex_unlock(&pd_cgl_track_lock);
}

// every marked texture is copied into its own pixel buffer first, so the copies queue on the GPU together
// and one fence covers them all, then each buffer goes into its surface. returns how many were written
static int
pd_cgl_write_back_batched(void *group)
{
	static GLsync (*fence_sync)(GLenum, GLbitfield);
	GLint saved_texture = 0, pack_buffer = 0, row_length = 0, alignment = 4;
	int i, count = 0;

	if (fence_sync == NULL)
		fence_sync = (GLsync (*)(GLenum, GLbitfield))eglGetProcAddress("glFenceSync");
	for (i = 0; i < pd_cgl_surftex_count; i++) {
		struct pd_cgl_surftex *t = &pd_cgl_surftexs[i];
		GLenum binding = t->target == GL_TEXTURE_RECTANGLE ? GL_TEXTURE_BINDING_RECTANGLE : GL_TEXTURE_BINDING_2D;

		if (!t->dirty || t->group != group)
			continue;
		if (count == 0) {
			glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
			glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
			glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
		}
		if (t->pbo == 0) {
			glGenBuffers(1, &t->pbo);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, t->pbo);
			glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)(t->row_bytes * t->height), NULL, GL_STREAM_READ);
		} else {
			glBindBuffer(GL_PIXEL_PACK_BUFFER, t->pbo);
		}
		glGetIntegerv(binding, &saved_texture);
		glBindTexture(t->target, t->texture);
		glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(t->row_bytes / t->element_bytes));
		glGetTexImage(t->target, 0, t->format, t->type, NULL);
		glBindTexture(t->target, (GLuint)saved_texture);
		t->dirty = 2;
		count++;
	}
	if (count == 0)
		return 0;

	GLsync sync = fence_sync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 2000000000ull);
	glDeleteSync(sync);
	for (i = 0; i < pd_cgl_surftex_count; i++) {
		struct pd_cgl_surftex *t = &pd_cgl_surftexs[i];
		size_t size = t->row_bytes * t->height;
		void *pixels;

		if (t->dirty != 2 || t->group != group)
			continue;
		glBindBuffer(GL_PIXEL_PACK_BUFFER, t->pbo);
		pixels = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)size, GL_MAP_READ_BIT);
		if (pixels != NULL) {
			pd_iosurface.lock(t->surface, 0, NULL);
			memcpy(pd_iosurface.base(t->surface, t->plane), pixels, size);
			pd_iosurface.unlock(t->surface, 0, NULL);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		}
		t->dirty = 0;
		pd_cgl_stats.tiles++;
		pd_cgl_stats.bytes += (long)size;
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)pack_buffer);
	glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	return count;
}

// copies every marked texture of the current share group back to its surface
static double
pd_cgl_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void
pd_cgl_write_back(void)
{
	void *group = pd_cgl_group();
	GLint saved_texture = 0, pack_buffer = 0, row_length = 0, alignment = 4;
	int i, saved = 0, pending = 0;
	static int trace = -1, batch = -1;
	double start;

	if (pd_cgl_surftex_count == 0 || group == NULL)
		return;
	if (trace < 0)
		trace = getenv("PD_CGL_TRACE") != NULL;
	start = trace ? pd_cgl_now() : 0;
	pthread_mutex_lock(&pd_cgl_track_lock);
	if (batch < 0) {
		const char *mode = getenv("PD_CGL_WRITEBACK");

		batch = mode == NULL || strcmp(mode, "sync") != 0;
	}
	if (batch) {
		pending = pd_cgl_write_back_batched(group);
		goto done;
	}
	for (i = 0; i < pd_cgl_surftex_count; i++) {
		struct pd_cgl_surftex *t = &pd_cgl_surftexs[i];
		GLenum binding = t->target == GL_TEXTURE_RECTANGLE ? GL_TEXTURE_BINDING_RECTANGLE : GL_TEXTURE_BINDING_2D;

		if (!t->dirty || t->group != group)
			continue;
		if (!saved) {
			glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
			glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
			glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			saved = 1;
		}
		glGetIntegerv(binding, &saved_texture);
		glBindTexture(t->target, t->texture);
		glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(t->row_bytes / t->element_bytes));
		pd_iosurface.lock(t->surface, 0, NULL);
		glGetTexImage(t->target, 0, t->format, t->type, pd_iosurface.base(t->surface, t->plane));
		pd_iosurface.unlock(t->surface, 0, NULL);
		glBindTexture(t->target, (GLuint)saved_texture);
		t->dirty = 0;
		pd_cgl_stats.tiles++;
		pd_cgl_stats.bytes += (long)t->row_bytes * t->height;
	}
	if (saved) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)pack_buffer);
		glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
		glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	}
done:
	(void)pending;
	if (trace) {
		double now = pd_cgl_now();

		pd_cgl_stats.flushes++;
		pd_cgl_stats.busy += now - start;
		if (pd_cgl_stats.since == 0)
			pd_cgl_stats.since = now;
		if (now - pd_cgl_stats.since >= 2000) {
			fprintf(stderr, "CGL: %ld flushes, %ld tiles written back (%ld KB), %.1f ms in write-back, %d surface textures, %d framebuffers\n",
			    pd_cgl_stats.flushes, pd_cgl_stats.tiles, pd_cgl_stats.bytes / 1024, pd_cgl_stats.busy,
			    pd_cgl_surftex_count, pd_cgl_fbo_count);
			memset(&pd_cgl_stats, 0, sizeof(pd_cgl_stats));
			pd_cgl_stats.since = now;
		}
	}
	pthread_mutex_unlock(&pd_cgl_track_lock);
}

static void
pd_cgl_attach(GLenum target, GLenum attachment, GLuint texture)
{
	GLint fbo = 0;
	void *group = pd_cgl_group();
	int i, surftex;

	if (attachment != GL_COLOR_ATTACHMENT0 || target == GL_READ_FRAMEBUFFER || group == NULL)
		return;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
	if (fbo == 0)
		return;
	pthread_mutex_lock(&pd_cgl_track_lock);
	surftex = texture != 0 ? pd_cgl_find_surftex(group, texture) : -1;
	for (i = 0; i < pd_cgl_fbo_count; i++) {
		if (pd_cgl_fbos[i].group == group && pd_cgl_fbos[i].fbo == (GLuint)fbo)
			break;
	}
	if (i == pd_cgl_fbo_count) {
		if (surftex < 0) {
			pthread_mutex_unlock(&pd_cgl_track_lock);
			return;
		}
		if (pd_cgl_fbo_count == pd_cgl_fbo_cap) {
			int cap = pd_cgl_fbo_cap ? pd_cgl_fbo_cap * 2 : 32;
			void *grown = realloc(pd_cgl_fbos, cap * sizeof(*pd_cgl_fbos));

			if (grown == NULL) {
				pthread_mutex_unlock(&pd_cgl_track_lock);
				return;
			}
			pd_cgl_fbos = grown;
			pd_cgl_fbo_cap = cap;
		}
		pd_cgl_fbo_count++;
	}
	pd_cgl_fbos[i] = (struct pd_cgl_fbo){ group, (GLuint)fbo, surftex };
	pthread_mutex_unlock(&pd_cgl_track_lock);
}

void
glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	PD_GL_REAL(glFramebufferTexture2D);
	pd_real_glFramebufferTexture2D(target, attachment, textarget, texture, level);
	pd_cgl_attach(target, attachment, texture);
}

void
glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	PD_GL_REAL(glFramebufferTexture);
	pd_real_glFramebufferTexture(target, attachment, texture, level);
	pd_cgl_attach(target, attachment, texture);
}

void
glDeleteTextures(GLsizei n, const GLuint *textures)
{
	void *group = pd_cgl_group();
	int i, j;

	PD_GL_REAL(glDeleteTextures);
	if (pd_cgl_surftex_count != 0 && group != NULL && textures != NULL) {
		pthread_mutex_lock(&pd_cgl_track_lock);
		for (i = 0; i < n; i++) {
			if ((j = pd_cgl_find_surftex(group, textures[i])) >= 0)
				pd_cgl_drop_surftex(j);
		}
		pthread_mutex_unlock(&pd_cgl_track_lock);
	}
	pd_real_glDeleteTextures(n, textures);
}

void
glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	void *group = pd_cgl_group();
	int i, j;

	PD_GL_REAL(glDeleteFramebuffers);
	if (pd_cgl_fbo_count != 0 && group != NULL && framebuffers != NULL) {
		pthread_mutex_lock(&pd_cgl_track_lock);
		for (i = 0; i < n; i++) {
			for (j = 0; j < pd_cgl_fbo_count; j++) {
				if (pd_cgl_fbos[j].group == group && pd_cgl_fbos[j].fbo == framebuffers[i]) {
					pd_cgl_fbos[j] = pd_cgl_fbos[--pd_cgl_fbo_count];
					break;
				}
			}
		}
		pthread_mutex_unlock(&pd_cgl_track_lock);
	}
	pd_real_glDeleteFramebuffers(n, framebuffers);
}

void
glClear(GLbitfield mask)
{
	PD_GL_REAL(glClear);
	pd_real_glClear(mask);
	pd_cgl_mark_drawn();
}

void
glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	PD_GL_REAL(glClearBufferfv);
	pd_real_glClearBufferfv(buffer, drawbuffer, value);
	pd_cgl_mark_drawn();
}

void
glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	PD_GL_REAL(glDrawArrays);
	pd_real_glDrawArrays(mode, first, count);
	pd_cgl_mark_drawn();
}

void
glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instances)
{
	PD_GL_REAL(glDrawArraysInstanced);
	pd_real_glDrawArraysInstanced(mode, first, count, instances);
	pd_cgl_mark_drawn();
}

void
glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	PD_GL_REAL(glDrawElements);
	pd_real_glDrawElements(mode, count, type, indices);
	pd_cgl_mark_drawn();
}

void
glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instances)
{
	PD_GL_REAL(glDrawElementsInstanced);
	pd_real_glDrawElementsInstanced(mode, count, type, indices, instances);
	pd_cgl_mark_drawn();
}

void
glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
	PD_GL_REAL(glDrawRangeElements);
	pd_real_glDrawRangeElements(mode, start, end, count, type, indices);
	pd_cgl_mark_drawn();
}

void
glBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1, GLint dy1,
    GLbitfield mask, GLenum filter)
{
	PD_GL_REAL(glBlitFramebuffer);
	pd_real_glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
	pd_cgl_mark_drawn();
}

void
glFlush(void)
{
	PD_GL_REAL(glFlush);
	pd_cgl_write_back();
	pd_real_glFlush();
}

// a fence after a frame is the other way a caller hands it on
GLsync
glFenceSync(GLenum condition, GLbitfield flags)
{
	PD_GL_REAL(glFenceSync);
	pd_cgl_write_back();
	return pd_real_glFenceSync(condition, flags);
}

void
glFinish(void)
{
	PD_GL_REAL(glFinish);
	pd_cgl_write_back();
	pd_real_glFinish();
}
#endif

// Mesa cannot sample client memory in place, so the plane is uploaded each time it is bound,
// which matches how callers rebind a surface for every frame they composite
CGLError
CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target, GLenum internal_format, GLsizei width, GLsizei height,
                       GLenum format, GLenum type, IOSurfaceRef ioSurface, GLuint plane)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	GLint row_length = 0, alignment = 4;
	size_t rowBytes, elementBytes;
	void *pixels;

	if (ctx == NULL)
		return kCGLBadContext;
	if (ioSurface == NULL || width <= 0 || height <= 0)
		return kCGLBadValue;
	pthread_once(&once, pd_cgl_load_iosurface);
	if (pd_iosurface.base == NULL || pd_iosurface.rowBytes == NULL || pd_iosurface.elementBytes == NULL)
		return kCGLBadValue;
	if (pd_iosurface.planeCount(ioSurface) == 0 && plane != 0)
		return kCGLBadValue;
	if (pd_iosurface.planeCount(ioSurface) != 0 && plane >= pd_iosurface.planeCount(ioSurface))
		return kCGLBadValue;
	rowBytes = pd_iosurface.rowBytes(ioSurface, plane);
	elementBytes = pd_iosurface.elementBytes(ioSurface, plane);
	if (elementBytes == 0)
		elementBytes = 1;

	pd_iosurface.lock(ioSurface, 1, NULL);
	pixels = pd_iosurface.base(ioSurface, plane);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(rowBytes / elementBytes));
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(target, 0, (GLint)internal_format, width, height, 0, format, type, pixels);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
	pd_iosurface.unlock(ioSurface, 1, NULL);
	if (glGetError() != GL_NO_ERROR)
		return kCGLBadValue;
#ifdef PD_CGL_USE_EGL
	pd_cgl_track_surface(ctx, target, ioSurface, plane, width, height, format, type, rowBytes, elementBytes);
#endif
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

/* Recursive so a caller that locks around a block which itself locks - which
 * NSOpenGLView does via -lockFocus - does not deadlock. */
static void pd_cgl_init_lock(struct _CGLContextObject *c) {
	if (c->lock_ready) {
		return;
	}
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&c->lock, &attr);
	pthread_mutexattr_destroy(&attr);
	c->lock_ready = 1;
}

CGLError CGLLockContext(CGLContextObj ctx)
{
	if (ctx == NULL) {
		return kCGLBadContext;
	}
	pd_cgl_init_lock(ctx);
	pthread_mutex_lock(&ctx->lock);
	return kCGLNoError;
}

CGLError CGLUnlockContext(CGLContextObj ctx)
{
	if (ctx == NULL || !ctx->lock_ready) {
		return kCGLBadContext;
	}
	pthread_mutex_unlock(&ctx->lock);
	return kCGLNoError;
}

// one renderer, the Mesa one CGL's contexts use, on every display
// its memory is the machine's, since Mesa's rasterizer and the paravirtual GPUs draw from system RAM
struct _CGLRendererInfoObject {
	int count;
};

// kCGLRendererGenericFloatID
#define PD_CGL_RENDERER_ID 0x00020400

CGLError
CGLQueryRendererInfo(unsigned int display_mask, CGLRendererInfoObj *rend, int *nrend)
{
	if (rend == NULL || nrend == NULL)
		return kCGLBadAddress;

	struct _CGLRendererInfoObject *info = calloc(1, sizeof(*info));
	if (info == NULL)
		return kCGLBadAlloc;
	info->count = 1;
	*rend = info;
	*nrend = info->count;
	return kCGLNoError;
}

CGLError
CGLDestroyRendererInfo(CGLRendererInfoObj rend)
{
	free(rend);
	return kCGLNoError;
}

static int
pd_cgl_memory_megabytes(void)
{
	uint64_t bytes = 0;
	size_t size = sizeof(bytes);

	if (sysctlbyname("hw.memsize", &bytes, &size, NULL, 0) != 0)
		return 512;
	return (int)(bytes >> 20);
}

CGLError
CGLDescribeRenderer(CGLRendererInfoObj rend, int rend_num, CGLRendererProperty prop, int *value)
{
	if (rend == NULL || value == NULL)
		return kCGLBadAddress;
	if (rend_num < 0 || rend_num >= rend->count)
		return kCGLBadRendererInfo;

	switch (prop) {
	case kCGLRPRendererCount:
		*value = rend->count;
		break;
	case kCGLRPRendererID:
		*value = PD_CGL_RENDERER_ID;
		break;
	case kCGLRPDisplayMask:
		*value = (int)0xffffffff;
		break;
	case kCGLRPOffScreen: case kCGLRPWindow: case kCGLRPMPSafe: case kCGLRPCompliant:
	case kCGLRPOnline: case kCGLRPGPUVertProcCapable: case kCGLRPGPUFragProcCapable:
		*value = 1;
		break;
	// accelerated and a backing store as the pixel formats report, one aux buffer keeps winemac's aux loop finite
	case kCGLRPAccelerated: case kCGLRPBackingStore: case kCGLRPMaxAuxBuffers:
		*value = 1;
		break;
	case kCGLRPFullScreen: case kCGLRPRobust:
	case kCGLRPMultiScreen: case kCGLRPAcceleratedCompute:
	case kCGLRPMaxSampleBuffers: case kCGLRPMaxSamples:
		*value = 0;
		break;
	case kCGLRPVideoMemoryMegabytes: case kCGLRPTextureMemoryMegabytes:
		*value = pd_cgl_memory_megabytes();
		break;
	case kCGLRPVideoMemory: case kCGLRPTextureMemory: {
		int megabytes = pd_cgl_memory_megabytes();
		*value = megabytes >= 2048 ? 0x7fffffff : megabytes << 20;
		break;
	}
	case kCGLRPMajorGLVersion:
		*value = 4;
		break;
	// the buffer layouts the pbuffer configs offer, Wine's winemac builds its pixel formats from these
	case kCGLRPBufferModes:
		*value = kCGLSingleBufferBit | kCGLDoubleBufferBit | kCGLMonoscopicBit;
		break;
	case kCGLRPColorModes:
		*value = kCGLRGB888Bit | kCGLARGB8888Bit | kCGLRGB888A8Bit;
		break;
	case kCGLRPAccumModes:
		*value = kCGL0Bit;
		break;
	case kCGLRPDepthModes:
		*value = kCGL0Bit | kCGL16Bit | kCGL24Bit;
		break;
	case kCGLRPStencilModes:
		*value = kCGL0Bit | kCGL8Bit;
		break;
	default:
		return kCGLBadProperty;
	}
	return kCGLNoError;
}

// no pbuffer drawables yet, every call reports the module as unavailable so callers take their framebuffer object path
CGLError
CGLCreatePBuffer(GLsizei width, GLsizei height, GLenum target, GLenum internalFormat, GLint max_level, CGLPBufferObj *pbuffer)
{
    if (pbuffer)
        *pbuffer = NULL;
    return kCGLBadCodeModule;
}

CGLError
CGLDestroyPBuffer(CGLPBufferObj pbuffer)
{
    return pbuffer ? kCGLBadCodeModule : kCGLBadAddress;
}

CGLPBufferObj
CGLRetainPBuffer(CGLPBufferObj pbuffer)
{
    return pbuffer;
}

void
CGLReleasePBuffer(CGLPBufferObj pbuffer)
{
}

CGLError
CGLSetPBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum face, GLint level, GLint screen)
{
    return kCGLBadCodeModule;
}

CGLError
CGLGetPBuffer(CGLContextObj ctx, CGLPBufferObj *pbuffer, GLenum *face, GLint *level, GLint *screen)
{
    if (pbuffer)
        *pbuffer = NULL;
    return kCGLNoError;
}

CGLError
CGLTexImagePBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum source)
{
    return kCGLBadCodeModule;
}

// GL_EXT_memory_object's queries are advertised by the driver but not exported by libGL, and a Mac's
// OpenGL.framework exports every function of the extensions it lists: callers that dlsym them (Wine's
// wined3d reads the device UUID this way) would otherwise call through null
static void *
pd_cgl_driver_proc(const char *name)
{
#ifdef PD_CGL_USE_EGL
	return (void *)eglGetProcAddress(name);
#else
	return (void *)glXGetProcAddress((const GLubyte *)name);
#endif
}

void
glGetUnsignedBytevEXT(GLenum pname, GLubyte *data)
{
	static void (*fn)(GLenum, GLubyte *);

	if (!fn)
		fn = (void (*)(GLenum, GLubyte *))pd_cgl_driver_proc("glGetUnsignedBytevEXT");
	if (fn)
		fn(pname, data);
}

void
glGetUnsignedBytei_vEXT(GLenum target, GLuint index, GLubyte *data)
{
	static void (*fn)(GLenum, GLuint, GLubyte *);

	if (!fn)
		fn = (void (*)(GLenum, GLuint, GLubyte *))pd_cgl_driver_proc("glGetUnsignedBytei_vEXT");
	if (fn)
		fn(target, index, data);
}
