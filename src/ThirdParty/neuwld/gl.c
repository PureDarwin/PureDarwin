/* SPDX-License-Identifier: MIT */
// GPU-backed WLD renderer for PureDarwin's native Radeon scanout.
#define _DARWIN_C_SOURCE 1
#define GL_GLEXT_PROTOTYPES 1
#include "wld-private.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/sysctl.h>

struct gl_target {
	struct wld_buffer *buffer;
	GLuint texture, fbo;
};

struct gl_upload {
	struct wld_buffer *buffer;
	GLuint texture;
	uint32_t width, height;
	uint64_t used;
	bool valid;
};

struct gl_renderer {
	struct wld_renderer base;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	GLuint program, vao, vbo, upload;
	GLuint active_upload;
	struct gl_upload uploads[12];
	uint64_t upload_serial;
	GLuint present_texture, present_fbo;
	uint32_t present_width, present_height;
	// reads back go through their own copy, so a second display never touches what the first scans out
	GLuint readback_texture, readback_fbo;
	uint32_t readback_width, readback_height;
	bool readback_new;
	// the pack buffer read backs go through, kept so its storage and cpu mapping outlive a frame
	GLuint pack_buffer;
	size_t pack_size;
	GLint use_texture, opaque, color;
	GLint u_masked, u_mask_rect, u_target_height;
	// the a8 mask in texture unit 1 while masked draws run, and which mask generation it holds
	GLuint mask_texture;
	unsigned mask_generation;
	bool mask_active;
	// blur: a gaussian program and two half-size ping-pong textures
	GLuint blur_program, blur_tex[2], blur_fbo[2];
	GLint blur_dir;
	int blur_w, blur_h;
	struct gl_target targets[4];
	struct gl_target *target;
	// the scanout copy changed size, so the next present is whole and its resource id is new
	bool present_new;
	uint32_t present_resource;
	// the display's own buffers imported from dma-bufs, the composite is blitted straight into one
	EGLImageKHR scanout_image[2];
	GLuint scanout_rb[2], scanout_fbo[2];
	int scanout_count;
	uint32_t scanout_width, scanout_height;
	bool failed;
	unsigned presents, fail_after;
	// the device the display was opened on, for the log
	char device[96];
};

static const char vertex_source[] =
    "#version 150\n"
    "in vec2 position; in vec2 texcoord; out vec2 uv;\n"
    "void main(){ uv=texcoord; gl_Position=vec4(position,0,1); }\n";
// mask_rect places an a8 mask on the target in top-left pixels, for shadows and rounded corners
static const char fragment_source[] =
    "#version 150\n"
    "in vec2 uv; out vec4 pixel; uniform sampler2D image; uniform sampler2D mask;\n"
    "uniform vec4 tint; uniform int textured; uniform int force_opaque;\n"
    "uniform int masked; uniform vec4 mask_rect; uniform float target_height;\n"
    "void main(){ pixel=textured!=0 ? texture(image,uv)*tint : tint;"
    " if(force_opaque!=0) pixel.a=1.0;"
    " if(masked!=0) pixel*=texture(mask,(vec2(gl_FragCoord.x,target_height-gl_FragCoord.y)-mask_rect.xy)/mask_rect.zw).r; }\n";

// 9 taps along dir, the weights of a gaussian with sigma about 2 texels
static const char blur_source[] =
    "#version 150\n"
    "in vec2 uv; out vec4 pixel; uniform sampler2D image; uniform vec2 dir;\n"
    "void main(){ vec4 c=texture(image,uv)*0.2270270270;\n"
    " c+=(texture(image,uv+dir*1.3846153846)+texture(image,uv-dir*1.3846153846))*0.3162162162;\n"
    " c+=(texture(image,uv+dir*3.2307692308)+texture(image,uv-dir*3.2307692308))*0.0702702703;\n"
    " pixel=c; }\n";

// ARGON_GFX_TRACE: each renderer call, to find one that stalls
static int gl_trace(void)
{
	// ARGON_GFX_TRACE=2 also traces each fill and blit, any other value is level 1
	static int on=-1;
	if(on<0) {
		const char *v=getenv("ARGON_GFX_TRACE");
		on=v ? (atoi(v)>1 ? atoi(v) : 1) : 0;
	}
	return on;
}

static inline struct gl_renderer *gl_renderer(struct wld_renderer *base)
{
	return (struct gl_renderer *)base;
}

// on a gl 3.1 context the same shaders compile as glsl 1.40, nothing in them needs 1.50
static bool glsl_140;
// a gl 2.1 context (nv30 and nv40 under nouveau) takes them rewritten as glsl 1.20
static bool glsl_120;

// copies src into out with every from replaced by to
static void replace_all(char *out, size_t size, const char *src, const char *from, const char *to)
{
	size_t n = 0, from_len = strlen(from), to_len = strlen(to);

	while (*src && n + 1 < size) {
		if (!strncmp(src, from, from_len) && n + to_len < size) {
			memcpy(out + n, to, to_len);
			n += to_len;
			src += from_len;
		} else
			out[n++] = *src++;
	}
	out[n] = 0;
}

// the 1.50 sources as glsl 1.20: attribute and varying, gl_FragColor and texture2D
static void glsl_120_source(GLenum kind, const char *source, char *out, size_t size)
{
	char a[2048], b[2048];
	const char *body = source + sizeof("#version 150\n") - 1;

	if (kind == GL_VERTEX_SHADER) {
		replace_all(a, sizeof(a), body, "in vec2", "attribute vec2");
		replace_all(b, sizeof(b), a, "out vec2", "varying vec2");
	} else {
		replace_all(a, sizeof(a), body, "in vec2", "varying vec2");
		replace_all(b, sizeof(b), a, " out vec4 pixel;", "");
	}
	replace_all(a, sizeof(a), b, "texture(", "texture2D(");
	snprintf(out, size, "#version 120\n%s%s", kind == GL_FRAGMENT_SHADER ? "#define pixel gl_FragColor\n" : "", a);
}

static GLuint compile(GLenum kind, const char *source)
{
	GLuint shader = glCreateShader(kind);
	GLint ok = 0;
	const char *sources[2] = { "#version 140\n", source + sizeof("#version 150\n") - 1 };
	char rewritten[2304];
	const char *source_120 = rewritten;

	if (glsl_120) {
		glsl_120_source(kind, source, rewritten, sizeof(rewritten));
		glShaderSource(shader, 1, &source_120, NULL);
	} else if (glsl_140)
		glShaderSource(shader, 2, sources, NULL);
	else
		glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) { glDeleteShader(shader); return 0; }
	return shader;
}

static bool make_current(struct gl_renderer *r)
{
	return eglMakeCurrent(r->display, r->surface, r->surface, r->context);
}

// the render node of a pci device from kern.pd_dri.devices, by "vvvv:dddd" or its bus id "pci:..."
static bool pci_render_node(const char *want, char *path, size_t size)
{
	char *text, *line, *next;
	size_t len = 0;
	bool found = false;

	if (sysctlbyname("kern.pd_dri.devices", NULL, &len, NULL, 0) != 0 || !len) return false;
	text = malloc(len + 1);
	if (!text) return false;
	if (sysctlbyname("kern.pd_dri.devices", text, &len, NULL, 0) != 0) { free(text); return false; }
	text[len] = 0;
	for (line = text; line && *line && !found; line = next) {
		unsigned minor, vendor, device;
		char id[64], pair[16];

		next = strchr(line, '\n');
		if (next) *next++ = 0;
		if (sscanf(line, "%u %63s %x %x", &minor, id, &vendor, &device) != 4 || minor < 128) continue;
		snprintf(pair, sizeof(pair), "%04x:%04x", vendor, device);
		if (strcasecmp(want, pair) && strcmp(want, id)) continue;
		snprintf(path, size, "/dev/dri/renderD%u", minor);
		found = true;
	}
	free(text);
	return found;
}

// an egl device by its render node or primary node path, or the software one
static EGLDeviceEXT find_device(const char *node, bool software)
{
	PFNEGLQUERYDEVICESEXTPROC query_devices = (void *)eglGetProcAddress("eglQueryDevicesEXT");
	PFNEGLQUERYDEVICESTRINGEXTPROC query_string = (void *)eglGetProcAddress("eglQueryDeviceStringEXT");
	EGLDeviceEXT devices[16];
	EGLint count = 0;

	if (!query_devices || !query_string || !query_devices(16, devices, &count)) return EGL_NO_DEVICE_EXT;
	for (EGLint i = 0; i < count; i++) {
		const char *ext = query_string(devices[i], EGL_EXTENSIONS);
		const char *file;

		if (software) {
			if (ext && strstr(ext, "EGL_MESA_device_software")) return devices[i];
			continue;
		}
		if (ext && strstr(ext, "EGL_EXT_device_drm_render_node")) {
			file = query_string(devices[i], EGL_DRM_RENDER_NODE_FILE_EXT);
			if (file && !strcmp(file, node)) return devices[i];
		}
		if (ext && strstr(ext, "EGL_EXT_device_drm")) {
			file = query_string(devices[i], EGL_DRM_DEVICE_FILE_EXT);
			if (file && !strcmp(file, node)) return devices[i];
		}
	}
	return EGL_NO_DEVICE_EXT;
}

// ARGON_GPU: auto (mesa's default), soft (llvmpipe), a render node, a pci id, or a gallium driver name
static EGLDisplay open_display(struct gl_renderer *r, const char *gpu)
{
	char node[64];
	EGLDeviceEXT device;
	EGLDisplay display;

	if (!gpu || !*gpu || !strcmp(gpu, "auto")) {
		snprintf(r->device, sizeof(r->device), "auto");
		return eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	}
	if (!strcmp(gpu, "soft") || !strcmp(gpu, "llvmpipe")) {
		// the device platform's software device, and llvmpipe ahead of virpipe in mesa's sw list
		setenv("GALLIUM_DRIVER", "llvmpipe", 1);
		setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
		device = find_device(NULL, true);
		snprintf(r->device, sizeof(r->device), "soft (%s)", device ? "egl software device" : "surfaceless");
		if (device) {
			display = eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, device, NULL);
			if (display != EGL_NO_DISPLAY) return display;
		}
		return eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	}
	if (!strncmp(gpu, "/dev/", 5))
		snprintf(node, sizeof(node), "%s", gpu);
	else if (!strncmp(gpu, "renderD", 7) || !strncmp(gpu, "card", 4))
		snprintf(node, sizeof(node), "/dev/dri/%s", gpu);
	else if (!pci_render_node(gpu, node, sizeof(node))) {
		if (strchr(gpu, ':')) {
			fprintf(stderr, "neuwld: ARGON_GPU %s: no such pci device in kern.pd_dri.devices\n", gpu);
			return EGL_NO_DISPLAY;
		}
		// virpipe, zink and the other sw-winsys drivers have no node
		setenv("GALLIUM_DRIVER", gpu, 1);
		snprintf(r->device, sizeof(r->device), "gallium %s", gpu);
		return eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	}
	device = find_device(node, false);
	if (!device) {
		fprintf(stderr, "neuwld: ARGON_GPU %s: no egl device for %s\n", gpu, node);
		return EGL_NO_DISPLAY;
	}
	snprintf(r->device, sizeof(r->device), "%s", node);
	return eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, device, NULL);
}

static bool initialize(struct gl_renderer *r, const char *gpu)
{
	EGLConfig config;
	EGLint count;
	const EGLint config_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };
	const EGLint surface_attr[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	const EGLint context_attr[] = { EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 2, EGL_CONTEXT_OPENGL_PROFILE_MASK,
		EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
	r->display = open_display(r, gpu);
	if (r->display == EGL_NO_DISPLAY || !eglInitialize(r->display, NULL, NULL) ||
	    !eglBindAPI(EGL_OPENGL_API) ||
	    !eglChooseConfig(r->display, config_attr, &config, 1, &count) || !count)
		return false;
	r->surface = eglCreatePbufferSurface(r->display, config, surface_attr);
	r->context = eglCreateContext(r->display, config, EGL_NO_CONTEXT, context_attr);
	// a gpu with core 3.1 but not 3.2 (freedreno's a3xx): the same renderer, glsl 1.40
	if (r->context == EGL_NO_CONTEXT) {
		const EGLint context_31[] = { EGL_CONTEXT_MAJOR_VERSION, 3,
			EGL_CONTEXT_MINOR_VERSION, 1, EGL_CONTEXT_OPENGL_PROFILE_MASK,
			EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
		r->context = eglCreateContext(r->display, config, EGL_NO_CONTEXT, context_31);
		glsl_140 = r->context != EGL_NO_CONTEXT;
	}
	// no core profile at all: mesa's compatibility context, 2.1 on nv30, with its fbo, blit and vao extensions
	if (r->context == EGL_NO_CONTEXT) {
		const EGLint context_21[] = { EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE };
		r->context = eglCreateContext(r->display, config, EGL_NO_CONTEXT, context_21);
		glsl_120 = r->context != EGL_NO_CONTEXT;
		if (glsl_120)
			fprintf(stderr, "neuwld: no gl 3.1, compositing on a gl 2.1 context with glsl 1.20\n");
	}
	if (r->surface == EGL_NO_SURFACE || r->context == EGL_NO_CONTEXT ||
	    !make_current(r)) return false;
	GLuint vs = compile(GL_VERTEX_SHADER, vertex_source);
	GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_source);
	if (!vs || !fs) return false;
	r->program = glCreateProgram();
	glAttachShader(r->program, vs);
	glAttachShader(r->program, fs);
	glBindAttribLocation(r->program, 0, "position");
	glBindAttribLocation(r->program, 1, "texcoord");
	glLinkProgram(r->program);
	glDeleteShader(vs); glDeleteShader(fs);
	GLint linked = 0;
	glGetProgramiv(r->program, GL_LINK_STATUS, &linked);
	if (!linked) return false;
	vs = compile(GL_VERTEX_SHADER, vertex_source);
	fs = compile(GL_FRAGMENT_SHADER, blur_source);
	if (!vs || !fs) return false;
	r->blur_program = glCreateProgram();
	glAttachShader(r->blur_program, vs);
	glAttachShader(r->blur_program, fs);
	glBindAttribLocation(r->blur_program, 0, "position");
	glBindAttribLocation(r->blur_program, 1, "texcoord");
	glLinkProgram(r->blur_program);
	glDeleteShader(vs); glDeleteShader(fs);
	glGetProgramiv(r->blur_program, GL_LINK_STATUS, &linked);
	if (!linked) return false;
	r->blur_dir = glGetUniformLocation(r->blur_program, "dir");
	glUseProgram(r->blur_program);
	glUniform1i(glGetUniformLocation(r->blur_program, "image"), 0);
	r->use_texture = glGetUniformLocation(r->program, "textured");
	r->opaque = glGetUniformLocation(r->program, "force_opaque");
	r->color = glGetUniformLocation(r->program, "tint");
	r->u_masked = glGetUniformLocation(r->program, "masked");
	r->u_mask_rect = glGetUniformLocation(r->program, "mask_rect");
	r->u_target_height = glGetUniformLocation(r->program, "target_height");
	glUseProgram(r->program);
	glUniform1i(glGetUniformLocation(r->program, "image"), 0);
	glUniform1i(glGetUniformLocation(r->program, "mask"), 1);
	glUniform1i(r->u_masked, 0);
	glGenVertexArrays(1, &r->vao);
	glBindVertexArray(r->vao);
	glGenBuffers(1, &r->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
	glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void *)0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void *)(2*sizeof(float)));
	glGenTextures(1, &r->upload);
	glBindTexture(GL_TEXTURE_2D, r->upload);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glDisable(GL_DEPTH_TEST);
	return glGetError() == GL_NO_ERROR;
}

static void draw(struct gl_renderer *r, int x, int y, int width, int height,
                 float u0, float v0, float u1, float v1, GLuint texture,
                 uint32_t color, bool opaque, bool blend)
{
	if (!r->target || width <= 0 || height <= 0) return;
	int tw = (int)r->target->buffer->width;
	int th = (int)r->target->buffer->height;
	float x0 = 2.f*x/tw - 1.f, x1 = 2.f*(x+width)/tw - 1.f;
	float y0 = 1.f - 2.f*y/th, y1 = 1.f - 2.f*(y+height)/th;
	float vertices[] = { x0,y0,u0,v0, x0,y1,u0,v1,
	                     x1,y0,u1,v0, x1,y1,u1,v1 };
	glBindFramebuffer(GL_FRAMEBUFFER, r->target->fbo);
	glViewport(0, 0, tw, th);
	glUseProgram(r->program);
	glUniform1i(r->use_texture, texture != 0);
	glUniform1i(r->opaque, opaque);
	glUniform1i(r->u_masked, r->mask_active);
	glUniform1f(r->u_target_height, (float)th);
	glUniform4f(r->color, ((color>>16)&255)/255.f, ((color>>8)&255)/255.f,
	            (color&255)/255.f, (color>>24)/255.f);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(r->vao);
	glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	if (blend) { glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); }
	else glDisable(GL_BLEND);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// uploads only the boxes about to be drawn (buffer coordinates, NULL for all of it): anything
// drawn from a client buffer is damage, so what the texture holds outside them is never shown
static bool upload_buffer(struct gl_renderer *r, struct wld_buffer *src, const pixman_box32_t *boxes, int count);

// ARGON_GFX_TRACE: each upload's size and time
static bool upload(struct gl_renderer *r, struct wld_buffer *src, const pixman_box32_t *boxes, int count)
{
	struct timespec t0,t1;
	bool ok;

	if(!gl_trace()) return upload_buffer(r,src,boxes,count);
	clock_gettime(CLOCK_MONOTONIC,&t0);
	ok=upload_buffer(r,src,boxes,count);
	clock_gettime(CLOCK_MONOTONIC,&t1);
	fprintf(stderr,"ARGON-GL upload %ux%u boxes %d, %ld us\n",src?src->width:0,src?src->height:0,boxes?count:-1,
	        (long)((t1.tv_sec-t0.tv_sec)*1000000+(t1.tv_nsec-t0.tv_nsec)/1000));
	return ok;
}

static bool upload_buffer(struct gl_renderer *r, struct wld_buffer *src, const pixman_box32_t *boxes, int count)
{
	if (!src || !src->width || !src->height || src->width > UINT32_MAX/4 ||
	    src->pitch < src->width*4 ||
	    !wld_map(src)) return false;
	struct gl_upload *slot = NULL;
	for (unsigned i=0; i<ARRAY_LENGTH(r->uploads); i++) {
		if (r->uploads[i].buffer == src) {
			slot=&r->uploads[i]; break;
		}
		if (!r->uploads[i].buffer && !slot) slot=&r->uploads[i];
	}
	if (!slot) {
		slot=&r->uploads[0];
		for (unsigned i=1; i<ARRAY_LENGTH(r->uploads); i++)
			if (r->uploads[i].used < slot->used) slot=&r->uploads[i];
	}
	if (slot->buffer != src || slot->width != src->width || slot->height != src->height) {
		if (slot->texture) glDeleteTextures(1,&slot->texture);
		if (slot->buffer) wld_buffer_unreference(slot->buffer);
		slot->buffer=src;
		slot->valid=false;
		wld_buffer_reference(src);
		glGenTextures(1,&slot->texture);
		glBindTexture(GL_TEXTURE_2D,slot->texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,src->width,src->height,
		             0,GL_BGRA,GL_UNSIGNED_BYTE,NULL);
		slot->width=src->width; slot->height=src->height;
	} else glBindTexture(GL_TEXTURE_2D,slot->texture);
	slot->used=++r->upload_serial;
	r->active_upload=slot->texture;
	glPixelStorei(GL_UNPACK_ROW_LENGTH, src->pitch / 4);
	if (!slot->valid || !boxes) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src->width, src->height,
		                GL_BGRA, GL_UNSIGNED_BYTE, src->map);
		slot->valid=true;
	} else {
		for (int i=0; i<count; i++) {
			int x1=boxes[i].x1<0?0:boxes[i].x1, y1=boxes[i].y1<0?0:boxes[i].y1;
			int x2=boxes[i].x2>(int)src->width?(int)src->width:boxes[i].x2;
			int y2=boxes[i].y2>(int)src->height?(int)src->height:boxes[i].y2;
			if (x2<=x1 || y2<=y1) continue;
			glTexSubImage2D(GL_TEXTURE_2D, 0, x1, y1, x2-x1, y2-y1, GL_BGRA, GL_UNSIGNED_BYTE,
			                (const uint8_t *)src->map + (size_t)y1*src->pitch + (size_t)x1*4);
		}
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	wld_unmap(src);
	GLenum err=glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr,"neuwld: upload failed, gl 0x%x\n",err);
		r->failed=true;
		return false;
	}
	return true;
}

static void copy_box(struct gl_renderer *r, struct wld_buffer *src,
                     int32_t dx, int32_t dy, pixman_box32_t b, bool blend)
{
	int w = b.x2-b.x1, h = b.y2-b.y1;
	if (w <= 0 || h <= 0) return;
	int tw = (int)r->target->buffer->width, th = (int)r->target->buffer->height;
	int x0 = b.x1+dx, y0=b.y1+dy;
	int x1 = b.x2+dx, y1=b.y2+dy;
	if (x1<=0 || y1<=0 || x0>=tw || y0>=th) return;
	int sx=x0<0?0:x0, sy=y0<0?0:y0;
	int ex=x1>tw?tw:x1, ey=y1>th?th:y1;
	glEnable(GL_SCISSOR_TEST);
	glScissor(sx, th-ey, ex-sx, ey-sy);
	draw(r, x0,y0,w,h, (float)b.x1/src->width, (float)b.y1/src->height,
	     (float)b.x2/src->width, (float)b.y2/src->height,
	     r->active_upload, 0xffffffffu, src->format==WLD_FORMAT_XRGB8888, blend);
	glDisable(GL_SCISSOR_TEST);
}

static uint32_t capabilities(struct wld_renderer *base, struct buffer *buffer)
{
	(void)base; (void)buffer;
	return WLD_CAPABILITY_READ;
}

static bool set_target(struct wld_renderer *base, struct buffer *buffer)
{
	struct gl_renderer *r=gl_renderer(base);
	if (!make_current(r)) return false;
	if (!buffer) { r->target=NULL; return true; }
	for (unsigned i=0;i<ARRAY_LENGTH(r->targets);i++) {
		struct gl_target *t=&r->targets[i];
		if (t->buffer == &buffer->base) { r->target=t; return true; }
		if (t->buffer) continue;
		t->buffer=&buffer->base;
		wld_buffer_reference(t->buffer);
		glGenTextures(1,&t->texture);
		glBindTexture(GL_TEXTURE_2D,t->texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,buffer->base.width,
		             buffer->base.height,0,GL_BGRA,GL_UNSIGNED_BYTE,NULL);
		glGenFramebuffers(1,&t->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER,t->fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t->texture,0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) return false;
		glViewport(0,0,buffer->base.width,buffer->base.height);
		glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
		r->target=t;
		return true;
	}
	return false;
}

static void fill_rectangle(struct wld_renderer *base, uint32_t color,
                           int32_t x,int32_t y,uint32_t w,uint32_t h)
{
	if(gl_trace()>1) fprintf(stderr,"ARGON-GL fill\n");
	draw(gl_renderer(base),x,y,w,h,0,0,0,0,0,color,false,false);
}

static void fill_region(struct wld_renderer *base,uint32_t color,pixman_region32_t *region)
{
	int count;
	pixman_box32_t *boxes=pixman_region32_rectangles(region,&count);
	for(int i=0;i<count;i++) fill_rectangle(base,color,boxes[i].x1,boxes[i].y1,
	                                     boxes[i].x2-boxes[i].x1,boxes[i].y2-boxes[i].y1);
}

static void copy_rectangle(struct wld_renderer *base,struct buffer *buffer,
                           int32_t dx,int32_t dy,int32_t sx,int32_t sy,uint32_t w,uint32_t h)
{
	struct gl_renderer *r=gl_renderer(base);
	pixman_box32_t box={sx,sy,sx+(int32_t)w,sy+(int32_t)h};
	if (!r->target || !upload(r,&buffer->base,&box,1)) return;
	copy_box(r,&buffer->base,dx-sx,dy-sy,box,false);
}

static void copy_region(struct wld_renderer *base,struct buffer *buffer,
                        int32_t dx,int32_t dy,pixman_region32_t *region)
{
	struct gl_renderer *r=gl_renderer(base);
	int count;
	pixman_box32_t *boxes=pixman_region32_rectangles(region,&count);
	if (!r->target || !upload(r,&buffer->base,boxes,count)) return;
	for(int i=0;i<count;i++) copy_box(r,&buffer->base,dx,dy,boxes[i],false);
}

static void blend_region(struct wld_renderer *base,struct buffer *buffer,
                         int32_t dx,int32_t dy,pixman_region32_t *region)
{
	struct gl_renderer *r=gl_renderer(base);
	int count;
	pixman_box32_t *boxes=pixman_region32_rectangles(region,&count);
	if (!r->target || !upload(r,&buffer->base,boxes,count)) return;
	for(int i=0;i<count;i++) copy_box(r,&buffer->base,dx,dy,boxes[i],true);
}

// draws a textured quad with the bound program into an fbo of the given size, top-left pixel coordinates
static void quad(struct gl_renderer *r, GLuint fbo, int fw, int fh, int x, int y, int w, int h,
                 float u0, float v0, float u1, float v1, GLuint texture)
{
	float x0 = 2.f*x/fw - 1.f, x1 = 2.f*(x+w)/fw - 1.f;
	float y0 = 1.f - 2.f*y/fh, y1 = 1.f - 2.f*(y+h)/fh;
	float vertices[] = { x0,y0,u0,v0, x0,y1,u0,v1, x1,y0,u1,v0, x1,y1,u1,v1 };

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, fw, fh);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(r->vao);
	glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glDisable(GL_BLEND);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// the ping-pong textures grow to the largest blurred area seen
static bool blur_scratch(struct gl_renderer *r, int w, int h)
{
	if (w <= r->blur_w && h <= r->blur_h)
		return true;
	w = w > r->blur_w ? w : r->blur_w, h = h > r->blur_h ? h : r->blur_h;
	for (int i = 0; i < 2; i++) {
		if (!r->blur_tex[i]) {
			glGenTextures(1, &r->blur_tex[i]);
			glGenFramebuffers(1, &r->blur_fbo[i]);
		}
		glBindTexture(GL_TEXTURE_2D, r->blur_tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
		glBindFramebuffer(GL_FRAMEBUFFER, r->blur_fbo[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->blur_tex[i], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			return false;
	}
	r->blur_w = w, r->blur_h = h;
	return true;
}

// downsample the padded area to half size, gaussian across then down, upsample back inside the region
static void blur_region(struct wld_renderer *base, pixman_region32_t *region, uint32_t radius)
{
	if(gl_trace()) fprintf(stderr,"ARGON-GL blur radius %u\n",radius);
	struct gl_renderer *r = gl_renderer(base);
	int tw, th, x0, y0, x1, y1, w, h, sw, sh, count;
	pixman_box32_t *e, *boxes;
	float spread;

	if (!r->target || !pixman_region32_not_empty(region))
		return;
	tw = (int)r->target->buffer->width, th = (int)r->target->buffer->height;
	e = pixman_region32_extents(region);
	x0 = e->x1 - (int)radius, y0 = e->y1 - (int)radius;
	x1 = e->x2 + (int)radius, y1 = e->y2 + (int)radius;
	x0 = x0 < 0 ? 0 : x0, y0 = y0 < 0 ? 0 : y0;
	x1 = x1 > tw ? tw : x1, y1 = y1 > th ? th : y1;
	w = x1 - x0, h = y1 - y0;
	sw = (w + 1) / 2, sh = (h + 1) / 2;
	if (w <= 0 || h <= 0 || !blur_scratch(r, sw, sh))
		return;

	// one tap step at half size covers radius/4 full-size pixels per side
	spread = radius / 8.f;
	glDisable(GL_SCISSOR_TEST);
	glUseProgram(r->blur_program);
	glUniform2f(r->blur_dir, 0, 0);
	quad(r, r->blur_fbo[0], r->blur_w, r->blur_h, 0, 0, sw, sh,
	     (float)x0/tw, 1.f - (float)y0/th, (float)x1/tw, 1.f - (float)y1/th, r->target->texture);
	for (int pass = 0; pass < 2; pass++) {
		glUniform2f(r->blur_dir, spread / r->blur_w, 0);
		quad(r, r->blur_fbo[1], r->blur_w, r->blur_h, 0, 0, sw, sh,
		     0, 1, (float)sw/r->blur_w, 1.f - (float)sh/r->blur_h, r->blur_tex[0]);
		glUniform2f(r->blur_dir, 0, spread / r->blur_h);
		quad(r, r->blur_fbo[0], r->blur_w, r->blur_h, 0, 0, sw, sh,
		     0, 1, (float)sw/r->blur_w, 1.f - (float)sh/r->blur_h, r->blur_tex[1]);
	}

	glUniform2f(r->blur_dir, 0, 0);
	glEnable(GL_SCISSOR_TEST);
	boxes = pixman_region32_rectangles(region, &count);
	for (int i = 0; i < count; i++) {
		glScissor(boxes[i].x1, th - boxes[i].y2, boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1);
		quad(r, r->target->fbo, tw, th, x0, y0, w, h,
		     0, 1, (float)sw/r->blur_w, 1.f - (float)sh/r->blur_h, r->blur_tex[0]);
	}
	glDisable(GL_SCISSOR_TEST);
	glUseProgram(r->program);
}

static void draw_text(struct wld_renderer *base,struct font *font,uint32_t color,
                      int32_t x,int32_t y,const char *text,uint32_t length,
                      struct wld_extents *extents)
{
	struct gl_renderer *r=gl_renderer(base);
	uint32_t advance=0;
	if (length==(uint32_t)-1) length=strlen(text);
	while(length) {
		uint32_t codepoint;
		int used=FcUtf8ToUcs4((const FcChar8 *)text,&codepoint,length);
		if(used<=0 || !codepoint) break;
		text+=used; length-=used;
		FT_UInt index=FT_Get_Char_Index(font->face,codepoint);
		if(!font_ensure_glyph(font,index)) continue;
		struct glyph *g=font->glyphs[index];
		FT_Bitmap *bitmap=&g->bitmap;
		if(bitmap->width && bitmap->rows && bitmap->width<=256 && bitmap->rows<=256) {
			size_t pixels=(size_t)bitmap->width*bitmap->rows;
			uint32_t *argb=malloc(pixels*4);
			if(argb) {
				for(unsigned yy=0;yy<bitmap->rows;yy++) for(unsigned xx=0;xx<bitmap->width;xx++) {
					uint8_t alpha=bitmap->pixel_mode==FT_PIXEL_MODE_GRAY ?
						bitmap->buffer[yy*bitmap->pitch+xx] :
						bitmap->pixel_mode==FT_PIXEL_MODE_MONO ?
						((bitmap->buffer[yy*bitmap->pitch+xx/8]>>(7-xx%8))&1)*255 : 0;
					uint32_t a=alpha*(color>>24)/255;
					argb[yy*bitmap->width+xx]=(a<<24)|
						((((color>>16)&255)*a/255)<<16)|
						((((color>>8)&255)*a/255)<<8)|((color&255)*a/255);
				}
				glBindTexture(GL_TEXTURE_2D,r->upload);
				glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,bitmap->width,bitmap->rows,
				             0,GL_BGRA,GL_UNSIGNED_BYTE,argb);
				draw(r,x+advance+g->x,y+g->y,bitmap->width,bitmap->rows,
				     0,0,1,1,r->upload,0xffffffffu,false,true);
				free(argb);
			}
		}
		advance+=g->advance;
	}
	if(extents) extents->advance=advance;
}

// the mask into texture unit 1, uploaded again only when its pixels changed
static bool bind_mask(struct gl_renderer *r, pixman_image_t *mask, unsigned generation, int32_t x, int32_t y)
{
	int w=pixman_image_get_width(mask), h=pixman_image_get_height(mask);
	if(!r->mask_texture) {
		glGenTextures(1,&r->mask_texture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D,r->mask_texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	} else {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D,r->mask_texture);
	}
	if(r->mask_generation!=generation) {
		glPixelStorei(GL_UNPACK_ROW_LENGTH,pixman_image_get_stride(mask));
		// gl 2.1 has no red-only textures, luminance samples as (l,l,l,1) so the shader's .r still reads it
		if(glsl_120)
			glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,w,h,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,pixman_image_get_data(mask));
		else
			glTexImage2D(GL_TEXTURE_2D,0,GL_R8,w,h,0,GL_RED,GL_UNSIGNED_BYTE,pixman_image_get_data(mask));
		glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
		r->mask_generation=generation;
	}
	glActiveTexture(GL_TEXTURE0);
	glUseProgram(r->program);
	glUniform4f(r->u_mask_rect,(float)x,(float)y,(float)w,(float)h);
	r->mask_active=true;
	return glGetError()==GL_NO_ERROR;
}

// a window's rounded corners, its buffer through the corner mask
static void blend_region_mask(struct wld_renderer *base, struct buffer *buffer, int32_t dst_x, int32_t dst_y,
                              pixman_region32_t *region, pixman_image_t *mask, unsigned generation)
{
	struct gl_renderer *r=gl_renderer(base);
	int count;
	pixman_box32_t *boxes=pixman_region32_rectangles(region,&count);
	if(!r->target || !upload(r,&buffer->base,boxes,count) || !bind_mask(r,mask,generation,dst_x,dst_y)) {
		r->mask_active=false;
		return;
	}
	for(int i=0;i<count;i++) copy_box(r,&buffer->base,dst_x,dst_y,boxes[i],true);
	r->mask_active=false;
}

// a window's shadow, black through the soft shadow mask
static void fill_region_mask(struct wld_renderer *base, uint32_t color, pixman_region32_t *region,
                             pixman_image_t *mask, unsigned generation, int32_t mask_x, int32_t mask_y)
{
	struct gl_renderer *r=gl_renderer(base);
	int count;
	pixman_box32_t *boxes=pixman_region32_rectangles(region,&count);
	if(!r->target || !bind_mask(r,mask,generation,mask_x,mask_y)) {
		r->mask_active=false;
		return;
	}
	for(int i=0;i<count;i++)
		draw(r,boxes[i].x1,boxes[i].y1,boxes[i].x2-boxes[i].x1,boxes[i].y2-boxes[i].y1,0,0,0,0,0,color,false,true);
	r->mask_active=false;
}

static void flush(struct wld_renderer *base)
{
	(void)base;
	glFlush();
}

static void drop_scanout(struct gl_renderer *r);

static void destroy(struct wld_renderer *base)
{
	struct gl_renderer *r=gl_renderer(base);
	if(r->context!=EGL_NO_CONTEXT && make_current(r)) {
		drop_scanout(r);
		for(unsigned i=0;i<ARRAY_LENGTH(r->targets);i++) {
			struct gl_target *t=&r->targets[i];
			if(!t->buffer) continue;
			glDeleteFramebuffers(1,&t->fbo); glDeleteTextures(1,&t->texture);
			wld_buffer_unreference(t->buffer);
		}
		glDeleteTextures(1,&r->upload);
		if(r->mask_texture) glDeleteTextures(1,&r->mask_texture);
		for (unsigned i=0;i<ARRAY_LENGTH(r->uploads);i++) {
			if(r->uploads[i].texture) glDeleteTextures(1,&r->uploads[i].texture);
			if(r->uploads[i].buffer) wld_buffer_unreference(r->uploads[i].buffer);
		}
		if(r->present_fbo) glDeleteFramebuffers(1,&r->present_fbo);
		if(r->present_texture) glDeleteTextures(1,&r->present_texture);
		if(r->readback_fbo) glDeleteFramebuffers(1,&r->readback_fbo);
		if(r->readback_texture) glDeleteTextures(1,&r->readback_texture);
		if(r->pack_buffer) glDeleteBuffers(1,&r->pack_buffer);
		for (int i=0;i<2;i++) {
			if(r->blur_fbo[i]) glDeleteFramebuffers(1,&r->blur_fbo[i]);
			if(r->blur_tex[i]) glDeleteTextures(1,&r->blur_tex[i]);
		}
		if(r->blur_program) glDeleteProgram(r->blur_program);
		glDeleteBuffers(1,&r->vbo);
		glDeleteVertexArrays(1,&r->vao); glDeleteProgram(r->program);
		eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	}
	if(r->context!=EGL_NO_CONTEXT) eglDestroyContext(r->display,r->context);
	if(r->surface!=EGL_NO_SURFACE) eglDestroySurface(r->display,r->surface);
	if(r->display!=EGL_NO_DISPLAY) eglTerminate(r->display);
	free(r);
}

static const struct wld_renderer_impl gl_impl={
	.capabilities=capabilities, .set_target=set_target,
	.fill_rectangle=fill_rectangle, .fill_region=fill_region,
	.copy_rectangle=copy_rectangle, .copy_region=copy_region,
	.blend_region=blend_region, .blur_region=blur_region, .draw_circle=wld_draw_circle,
	.draw_line=wld_draw_line, .draw_text=draw_text,
	.flush=flush, .destroy=destroy,
	.blend_region_mask=blend_region_mask, .fill_region_mask=fill_region_mask,
};

EXPORT struct wld_renderer *wld_gl_create_renderer(void)
{
	return wld_gl_create_renderer_on(getenv("ARGON_GPU"));
}

EXPORT struct wld_renderer *wld_gl_create_renderer_on(const char *gpu)
{
	struct gl_renderer *r=calloc(1,sizeof(*r));
	if(!r) return NULL;
	r->display=EGL_NO_DISPLAY; r->surface=EGL_NO_SURFACE;
	r->context=EGL_NO_CONTEXT;
	if(!initialize(r,gpu)) {
		fprintf(stderr,"neuwld: gpu %s: no GL context on %s\n",gpu&&*gpu?gpu:"auto",r->device[0]?r->device:"any device");
		destroy(&r->base);
		return NULL;
	}
	fprintf(stderr,"neuwld: gpu %s on %s, GL_RENDERER %s\n",gpu&&*gpu?gpu:"auto",r->device,(const char *)glGetString(GL_RENDERER));
	const char *fail_after=getenv("ARGON_GFX_FAIL_AFTER");
	r->fail_after=fail_after?(unsigned)atoi(fail_after):0;
	renderer_initialize(&r->base,&gl_impl);
	eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	return &r->base;
}

// composite one screen and its cursor into the scanout copy, inside damage only (NULL for all)
// with pixels the damaged rows are read back too, for a framebuffer the GPU can not scan out
static enum wld_gl_present_result present_to_scanout(struct wld_renderer *base, struct wld_buffer *buffer,
                                                     struct wld_buffer *cursor, int32_t cursor_x, int32_t cursor_y,
                                                     const pixman_region32_t *damage,
                                                     uint32_t *pixels, size_t pitch, bool top_down,
                                                     GLuint into)
{
	struct gl_renderer *r=gl_renderer(base);
	if(!buffer || r->failed || !make_current(r)) {
		fprintf(stderr,"neuwld: present failed, buffer %p failed %d egl 0x%x\n",(void *)buffer,r->failed,eglGetError());
		return WLD_GL_PRESENT_FAILED;
	}
	// ARGON_GFX_FAIL_AFTER=N: lose the context after N presents, to test the fallback
	if(++r->presents > r->fail_after && r->fail_after) r->failed=true;
	for(unsigned i=0;i<ARRAY_LENGTH(r->targets) && !r->failed;i++) {
		struct gl_target *t=&r->targets[i];
		if(t->buffer!=buffer) continue;
		// a read back lands in its own copy, the present copy is what direct scanout shows
		bool readback=pixels && !into;
		GLuint *copy_fbo=readback?&r->readback_fbo:&r->present_fbo;
		GLuint *copy_texture=readback?&r->readback_texture:&r->present_texture;
		uint32_t *copy_width=readback?&r->readback_width:&r->present_width;
		uint32_t *copy_height=readback?&r->readback_height:&r->present_height;
		bool *copy_new=readback?&r->readback_new:&r->present_new;
		if(!into && (!*copy_fbo || *copy_width!=buffer->width || *copy_height!=buffer->height)) {
			if(*copy_fbo) glDeleteFramebuffers(1,copy_fbo);
			if(*copy_texture) glDeleteTextures(1,copy_texture);
			glGenTextures(1,copy_texture);
			glBindTexture(GL_TEXTURE_2D,*copy_texture);
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,buffer->width,buffer->height,
			             0,GL_BGRA,GL_UNSIGNED_BYTE,NULL);
			glGenFramebuffers(1,copy_fbo);
			glBindFramebuffer(GL_FRAMEBUFFER,*copy_fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,
			                       GL_TEXTURE_2D,*copy_texture,0);
			if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) break;
			*copy_width=buffer->width; *copy_height=buffer->height;
			*copy_new=true;
			if(!readback) r->present_resource=0;
		}
		int w=(int)buffer->width, h=(int)buffer->height;
		int x1=0, y1=0, x2=w, y2=h;
		bool use_damage=damage && (into || !*copy_new);
		if(use_damage) {
			const pixman_box32_t *e=pixman_region32_extents((pixman_region32_t *)damage);
			if(!pixman_region32_not_empty((pixman_region32_t *)damage)) x2=y2=0;
			else {
				x1=e->x1<0?0:e->x1; y1=e->y1<0?0:e->y1;
				x2=e->x2>w?w:e->x2; y2=e->y2>h?h:e->y2;
			}
		}
		if(!into) *copy_new=false;
		GLuint dst=into?into:*copy_fbo;
		// ARGON_GFX_TRACE: the damage boxes, the box drawn and how long the blit and the read take
		struct timespec ts0,ts1,ts2;
		int nboxes=0;
		if(gl_trace()) {
			if(damage) pixman_region32_rectangles((pixman_region32_t *)damage,&nboxes);
			clock_gettime(CLOCK_MONOTONIC,&ts0);
		}
		if(x2>x1 && y2>y1) {
			// the composite's rows run bottom up like any GL framebuffer, the scanout copy's top down
			// as the display reads memory, so the blit flips them
			if(gl_trace()>1) fprintf(stderr,"ARGON-GL blit %d,%d-%d,%d from fbo %u to %u\n",x1,y1,x2,y2,t->fbo,dst);
			glBindFramebuffer(GL_READ_FRAMEBUFFER,t->fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER,dst);
			if(top_down)
				glBlitFramebuffer(x1,h-y2,x2,h-y1, x1,y2,x2,y1, GL_COLOR_BUFFER_BIT,GL_NEAREST);
			else
				glBlitFramebuffer(x1,h-y2,x2,h-y1, x1,h-y2,x2,h-y1, GL_COLOR_BUFFER_BIT,GL_NEAREST);
			GLenum blit_err=glGetError();
			if(blit_err!=GL_NO_ERROR) {
				fprintf(stderr,"neuwld: blit to %s failed, gl 0x%x\n",into?"scanout":"present copy",blit_err);
				r->failed=true;
			}
			if(cursor && upload(r,cursor,NULL,0)) {
				struct wld_buffer dummy={.width=buffer->width,.height=buffer->height};
				struct gl_target overlay={.buffer=&dummy,.fbo=dst};
				struct gl_target *saved=r->target;
				r->target=&overlay;
				// draw() places top-left coordinates for a bottom-up target, so mirror the cursor too
				if(top_down)
					draw(r,cursor_x,h-cursor_y-(int)cursor->height,cursor->width,cursor->height,
					     0,1,1,0,r->active_upload,0xffffffffu,false,true);
				else
					draw(r,cursor_x,cursor_y,cursor->width,cursor->height,
					     0,0,1,1,r->active_upload,0xffffffffu,false,true);
				r->target=saved;
			}
			if(gl_trace()) { glFinish(); clock_gettime(CLOCK_MONOTONIC,&ts1); }
			if(gl_trace()>1) fprintf(stderr,"ARGON-GL blitted, cursor done, reading %d\n",pixels!=NULL);
			if(pixels) {
				glBindFramebuffer(GL_READ_FRAMEBUFFER,*copy_fbo);
				glReadBuffer(GL_COLOR_ATTACHMENT0);
				// each damage box through one pack buffer that lives as long as the renderer: a fresh
				// staging buffer per read costs a new cpu mapping, which is slow on some drivers
				int n=1;
				pixman_box32_t whole={x1,y1,x2,y2};
				const pixman_box32_t *boxes=&whole;
				if(use_damage && pixman_region32_not_empty((pixman_region32_t *)damage))
					boxes=pixman_region32_rectangles((pixman_region32_t *)damage,&n);
				if(!r->pack_buffer) glGenBuffers(1,&r->pack_buffer);
				glBindBuffer(GL_PIXEL_PACK_BUFFER,r->pack_buffer);
				for(int b=0;b<n;b++) {
					int bx1=boxes[b].x1<x1?x1:boxes[b].x1, by1=boxes[b].y1<y1?y1:boxes[b].y1;
					int bx2=boxes[b].x2>x2?x2:boxes[b].x2, by2=boxes[b].y2>y2?y2:boxes[b].y2;
					if(bx2<=bx1 || by2<=by1) continue;
					size_t row=(size_t)(bx2-bx1)*4, size=row*(size_t)(by2-by1);
					if(size>r->pack_size) {
						glBufferData(GL_PIXEL_PACK_BUFFER,(GLsizeiptr)size,NULL,GL_STREAM_READ);
						r->pack_size=size;
					}
					// the present copy's rows run top down when top_down, bottom up otherwise
					int ry=top_down?by1:h-by2;
					glReadPixels(bx1,ry,bx2-bx1,by2-by1,GL_BGRA,GL_UNSIGNED_BYTE,NULL);
					const uint8_t *src=glMapBufferRange(GL_PIXEL_PACK_BUFFER,0,(GLsizeiptr)size,GL_MAP_READ_BIT);
					if(!src) {
						fprintf(stderr,"neuwld: pack buffer map failed, gl 0x%x\n",glGetError());
						r->failed=true;
						break;
					}
					for(int y=by1;y<by2;y++) {
						const uint8_t *line=src+(size_t)(top_down?y-by1:by2-1-y)*row;
						memcpy((uint8_t *)pixels+(size_t)y*pitch+(size_t)bx1*4,line,row);
					}
					glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
				}
				glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
				if(gl_trace()>1) fprintf(stderr,"ARGON-GL read back\n");
				if(gl_trace()) {
					clock_gettime(CLOCK_MONOTONIC,&ts2);
					fprintf(stderr,"ARGON-GL readback %d boxes, box %dx%d, blit %ld us, read %ld us\n",nboxes,x2-x1,y2-y1,
					        (long)((ts1.tv_sec-ts0.tv_sec)*1000000+(ts1.tv_nsec-ts0.tv_nsec)/1000),
					        (long)((ts2.tv_sec-ts1.tv_sec)*1000000+(ts2.tv_nsec-ts1.tv_nsec)/1000));
				}
			} else if(into) {
				struct timespec t0,t1;
				if(gl_trace()) clock_gettime(CLOCK_MONOTONIC,&t0);
				glFinish();
				if(gl_trace()) {
					clock_gettime(CLOCK_MONOTONIC,&t1);
					fprintf(stderr,"ARGON-GL scanout finish %ld us\n",
					        (long)((t1.tv_sec-t0.tv_sec)*1000000+(t1.tv_nsec-t0.tv_nsec)/1000));
				}
			} else glFlush();
		}
		GLenum err=glGetError();
		if(err!=GL_NO_ERROR) {
			fprintf(stderr,"neuwld: present failed, gl 0x%x\n",err);
			r->failed=true;
		}
		eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
		if(r->failed) return WLD_GL_PRESENT_FAILED;
		return pixels ? WLD_GL_PRESENT_READ_BACK : WLD_GL_PRESENT_ON_GPU;
	}
	fprintf(stderr,"neuwld: present failed, %s\n",r->failed?"context lost":"buffer is no render target");
	eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	return WLD_GL_PRESENT_FAILED;
}

EXPORT enum wld_gl_present_result wld_gl_present_damage(struct wld_renderer *base, struct wld_buffer *buffer,
                                                       struct wld_buffer *cursor, int32_t cursor_x, int32_t cursor_y,
                                                       const pixman_region32_t *damage,
                                                       uint32_t *pixels, size_t pitch)
{
	return present_to_scanout(base,buffer,cursor,cursor_x,cursor_y,damage,pixels,pitch,true,0);
}

static void drop_scanout(struct gl_renderer *r)
{
	PFNEGLDESTROYIMAGEKHRPROC destroy_image=(PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");

	for(int i=0;i<r->scanout_count;i++) {
		if(r->scanout_fbo[i]) glDeleteFramebuffers(1,&r->scanout_fbo[i]);
		if(r->scanout_rb[i]) glDeleteRenderbuffers(1,&r->scanout_rb[i]);
		if(r->scanout_image[i]!=EGL_NO_IMAGE_KHR && destroy_image) destroy_image(r->display,r->scanout_image[i]);
		r->scanout_fbo[i]=r->scanout_rb[i]=0;
		r->scanout_image[i]=EGL_NO_IMAGE_KHR;
	}
	r->scanout_count=0;
}

// the display's XRGB8888 linear buffers as render targets, count 0 drops them again
EXPORT bool wld_gl_set_scanout_dmabufs(struct wld_renderer *base, const int *fds, int count,
                                       uint32_t width, uint32_t height, uint32_t pitch)
{
	struct gl_renderer *r=gl_renderer(base);
	PFNEGLCREATEIMAGEKHRPROC create_image=(PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
	PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC target_storage=
		(PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC)eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	bool ok=true;
	GLenum status=0,glerr=GL_NO_ERROR;

	if(!make_current(r)) return false;
	drop_scanout(r);
	if(count>2 || (count && (!create_image || !target_storage))) ok=false;
	for(int i=0;i<count && ok;i++) {
		const EGLint attr[]={ EGL_WIDTH,(EGLint)width, EGL_HEIGHT,(EGLint)height,
			EGL_LINUX_DRM_FOURCC_EXT,0x34325258, // XR24
			EGL_DMA_BUF_PLANE0_FD_EXT,fds[i], EGL_DMA_BUF_PLANE0_OFFSET_EXT,0,
			EGL_DMA_BUF_PLANE0_PITCH_EXT,(EGLint)pitch,
			EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,0, EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,0, EGL_NONE };
		r->scanout_count=i+1;
		r->scanout_image[i]=create_image(r->display,EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT,NULL,attr);
		if(r->scanout_image[i]==EGL_NO_IMAGE_KHR) {
			// without a modifier the driver takes the buffer's own layout, linear here
			EGLint plain[ARRAY_LENGTH(attr)];
			memcpy(plain,attr,sizeof(attr));
			plain[12]=EGL_NONE;
			fprintf(stderr,"neuwld: dma-buf import with a linear modifier failed (egl 0x%x), trying without\n",eglGetError());
			r->scanout_image[i]=create_image(r->display,EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT,NULL,plain);
		}
		if(r->scanout_image[i]==EGL_NO_IMAGE_KHR) { ok=false; break; }
		glGenRenderbuffers(1,&r->scanout_rb[i]);
		glBindRenderbuffer(GL_RENDERBUFFER,r->scanout_rb[i]);
		target_storage(GL_RENDERBUFFER,r->scanout_image[i]);
		glGenFramebuffers(1,&r->scanout_fbo[i]);
		glBindFramebuffer(GL_FRAMEBUFFER,r->scanout_fbo[i]);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,r->scanout_rb[i]);
		status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glerr=glGetError();
		ok=status==GL_FRAMEBUFFER_COMPLETE && glerr==GL_NO_ERROR;
	}
	if(!ok) {
		fprintf(stderr,"neuwld: dma-buf import failed, egl 0x%x gl 0x%x fbo 0x%x\n",eglGetError(),glerr,status);
		drop_scanout(r);
	}
	r->scanout_width=width; r->scanout_height=height;
	eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	return ok;
}

// composites into scanout buffer index, finished on the GPU when this returns so it can be flipped
EXPORT enum wld_gl_present_result wld_gl_present_scanout(struct wld_renderer *base, struct wld_buffer *buffer,
                                                         struct wld_buffer *cursor, int32_t cursor_x, int32_t cursor_y,
                                                         const pixman_region32_t *damage, int index)
{
	struct gl_renderer *r=gl_renderer(base);

	if(index<0 || index>=r->scanout_count || buffer->width!=r->scanout_width || buffer->height!=r->scanout_height) {
		fprintf(stderr,"neuwld: no scanout target %d of %d for %ux%u (scanout %ux%u)\n",index,r->scanout_count,
		        buffer->width,buffer->height,r->scanout_width,r->scanout_height);
		return WLD_GL_PRESENT_FAILED;
	}
	return present_to_scanout(base,buffer,cursor,cursor_x,cursor_y,damage,NULL,0,true,r->scanout_fbo[index]);
}

// GL interop's view of a texture, version 2 of Mesa's mesa_glinterop.h, which Mesa does not install
struct gl_interop_in {
	uint32_t version;
	unsigned target, obj, miplevel;
	uint32_t access, out_driver_data_size;
	void *out_driver_data;
};
struct gl_interop_out {
	uint32_t version;
	int dmabuf_fd;
	unsigned internal_format;
	ptrdiff_t buf_offset, buf_size;
	unsigned view_minlevel, view_numlevels, view_minlayer, view_numlayers;
	uint32_t out_driver_data_written, width, height, depth, stride;
	uint64_t modifier;
};

// the virtio-gpu resource behind the scanout copy, 0 when the driver can not say (not virgl)
EXPORT uint32_t wld_gl_scanout_resource(struct wld_renderer *base)
{
	struct gl_renderer *r=gl_renderer(base);
	int (*export_object)(EGLDisplay, EGLContext, struct gl_interop_in *, struct gl_interop_out *);
	uint32_t resource=0;

	if(r->present_resource || !r->present_texture)
		return r->present_resource;
	// libEGL exports it, eglGetProcAddress may hand back a dispatch stub for an unknown name instead
	export_object=dlsym(RTLD_DEFAULT,"MesaGLInteropEGLExportObject");
	if(!export_object || !make_current(r))
		return 0;
	struct gl_interop_in in={ .version=2, .target=GL_TEXTURE_2D, .obj=r->present_texture,
	                          .access=0, .out_driver_data_size=sizeof(resource), .out_driver_data=&resource };
	struct gl_interop_out out={ .version=2, .dmabuf_fd=-1 };
	if(export_object(r->display,r->context,&in,&out)!=0 || out.out_driver_data_written<sizeof(resource))
		resource=0;
	eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	r->present_resource=resource;
	return resource;
}

// what the renderer runs on, for the log and to turn down software rasterisers
EXPORT const char *wld_gl_renderer_name(struct wld_renderer *base)
{
	static char name[128];
	struct gl_renderer *r=gl_renderer(base);

	if(!make_current(r)) return "";
	snprintf(name,sizeof(name),"%s",(const char *)glGetString(GL_RENDERER));
	eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	return name;
}

EXPORT bool wld_gl_present(struct wld_renderer *base,struct wld_buffer *buffer,
                           struct wld_buffer *cursor,int32_t cursor_x,int32_t cursor_y)
{
	struct gl_renderer *r=gl_renderer(base);
	if(present_to_scanout(base,buffer,cursor,cursor_x,cursor_y,NULL,NULL,0,false,0)==WLD_GL_PRESENT_FAILED)
		return false;
	if(!make_current(r)) return false;
	glBindFramebuffer(GL_READ_FRAMEBUFFER,r->present_fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	int (*present)(void)=dlsym(RTLD_DEFAULT,"pd_radeon_present");
	bool ok=present && present()==0;
	eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	return ok;
}
