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
#include <string.h>

struct gl_target {
	struct wld_buffer *buffer;
	GLuint texture, fbo;
};

struct gl_upload {
	struct wld_buffer *buffer;
	GLuint texture;
	uint32_t width, height;
	uint64_t hash;
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
	struct wld_buffer *last_uploaded;
	struct gl_upload uploads[12];
	uint64_t upload_serial;
	GLuint present_texture, present_fbo;
	uint32_t present_width, present_height;
	GLint use_texture, opaque, color;
	// blur: a gaussian program and two half-size ping-pong textures
	GLuint blur_program, blur_tex[2], blur_fbo[2];
	GLint blur_dir;
	int blur_w, blur_h;
	struct gl_target targets[4];
	struct gl_target *target;
};

static const char vertex_source[] =
    "#version 150\n"
    "in vec2 position; in vec2 texcoord; out vec2 uv;\n"
    "void main(){ uv=texcoord; gl_Position=vec4(position,0,1); }\n";
static const char fragment_source[] =
    "#version 150\n"
    "in vec2 uv; out vec4 pixel; uniform sampler2D image;\n"
    "uniform vec4 tint; uniform int textured; uniform int force_opaque;\n"
    "void main(){ pixel=textured!=0 ? texture(image,uv)*tint : tint;"
    " if(force_opaque!=0) pixel.a=1.0; }\n";

// 9 taps along dir, the weights of a gaussian with sigma about 2 texels
static const char blur_source[] =
    "#version 150\n"
    "in vec2 uv; out vec4 pixel; uniform sampler2D image; uniform vec2 dir;\n"
    "void main(){ vec4 c=texture(image,uv)*0.2270270270;\n"
    " c+=(texture(image,uv+dir*1.3846153846)+texture(image,uv-dir*1.3846153846))*0.3162162162;\n"
    " c+=(texture(image,uv+dir*3.2307692308)+texture(image,uv-dir*3.2307692308))*0.0702702703;\n"
    " pixel=c; }\n";

static inline struct gl_renderer *gl_renderer(struct wld_renderer *base)
{
	return (struct gl_renderer *)base;
}

static GLuint compile(GLenum kind, const char *source)
{
	GLuint shader = glCreateShader(kind);
	GLint ok = 0;
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

static bool initialize(struct gl_renderer *r)
{
	EGLConfig config;
	EGLint count;
	const EGLint config_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };
	const EGLint surface_attr[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	const EGLint context_attr[] = { EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 2, EGL_CONTEXT_OPENGL_PROFILE_MASK,
		EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
	r->display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
	                                 EGL_DEFAULT_DISPLAY, NULL);
	if (r->display == EGL_NO_DISPLAY || !eglInitialize(r->display, NULL, NULL) ||
	    !eglBindAPI(EGL_OPENGL_API) ||
	    !eglChooseConfig(r->display, config_attr, &config, 1, &count) || !count)
		return false;
	r->surface = eglCreatePbufferSurface(r->display, config, surface_attr);
	r->context = eglCreateContext(r->display, config, EGL_NO_CONTEXT, context_attr);
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
	glUseProgram(r->program);
	glUniform1i(glGetUniformLocation(r->program, "image"), 0);
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

static bool upload(struct gl_renderer *r, struct wld_buffer *src)
{
	if (src && src == r->last_uploaded) return true;
	if (!src || !src->width || !src->height || src->width > UINT32_MAX/4 ||
	    src->pitch < src->width*4 ||
	    !wld_map(src)) return false;
	uint64_t hash=1469598103934665603ULL;
	for(uint32_t y=0;y<src->height;y++) {
		const uint8_t *row=(const uint8_t *)src->map+(size_t)y*src->pitch;
		for(uint32_t x=0;x<src->width*4;x+=8) {
			uint64_t word=0;
			uint32_t count=src->width*4-x<8 ? src->width*4-x : 8;
			memcpy(&word,row+x,count);
			hash=(hash^word)*1099511628211ULL;
		}
	}
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
	if (slot->buffer != src) {
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
	if (slot->valid && slot->hash==hash) {
		wld_unmap(src);
		r->last_uploaded=src;
		return true;
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, src->pitch / 4);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src->width, src->height,
	                GL_BGRA, GL_UNSIGNED_BYTE, src->map);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	wld_unmap(src);
	if (glGetError() != GL_NO_ERROR) return false;
	slot->hash=hash; slot->valid=true;
	r->last_uploaded=src;
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
	if (!r->target || !upload(r,&buffer->base)) return;
	pixman_box32_t box={sx,sy,sx+(int32_t)w,sy+(int32_t)h};
	copy_box(r,&buffer->base,dx-sx,dy-sy,box,false);
}

static void copy_region(struct wld_renderer *base,struct buffer *buffer,
                        int32_t dx,int32_t dy,pixman_region32_t *region)
{
	struct gl_renderer *r=gl_renderer(base);
	if (!r->target || !upload(r,&buffer->base)) return;
	int count;
	pixman_box32_t *boxes=pixman_region32_rectangles(region,&count);
	for(int i=0;i<count;i++) copy_box(r,&buffer->base,dx,dy,boxes[i],false);
}

static void blend_region(struct wld_renderer *base,struct buffer *buffer,
                         int32_t dx,int32_t dy,pixman_region32_t *region)
{
	struct gl_renderer *r=gl_renderer(base);
	if (!r->target || !upload(r,&buffer->base)) return;
	int count;
	pixman_box32_t *boxes=pixman_region32_rectangles(region,&count);
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

static void flush(struct wld_renderer *base)
{
	gl_renderer(base)->last_uploaded=NULL;
	glFlush();
}

static void destroy(struct wld_renderer *base)
{
	struct gl_renderer *r=gl_renderer(base);
	if(r->context!=EGL_NO_CONTEXT && make_current(r)) {
		for(unsigned i=0;i<ARRAY_LENGTH(r->targets);i++) {
			struct gl_target *t=&r->targets[i];
			if(!t->buffer) continue;
			glDeleteFramebuffers(1,&t->fbo); glDeleteTextures(1,&t->texture);
			wld_buffer_unreference(t->buffer);
		}
		glDeleteTextures(1,&r->upload);
		for (unsigned i=0;i<ARRAY_LENGTH(r->uploads);i++) {
			if(r->uploads[i].texture) glDeleteTextures(1,&r->uploads[i].texture);
			if(r->uploads[i].buffer) wld_buffer_unreference(r->uploads[i].buffer);
		}
		if(r->present_fbo) glDeleteFramebuffers(1,&r->present_fbo);
		if(r->present_texture) glDeleteTextures(1,&r->present_texture);
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
};

EXPORT struct wld_renderer *wld_gl_create_renderer(void)
{
	struct gl_renderer *r=calloc(1,sizeof(*r));
	if(!r) return NULL;
	r->display=EGL_NO_DISPLAY; r->surface=EGL_NO_SURFACE;
	r->context=EGL_NO_CONTEXT;
	if(!initialize(r)) { destroy(&r->base); return NULL; }
	renderer_initialize(&r->base,&gl_impl);
	eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	return &r->base;
}

EXPORT bool wld_gl_present(struct wld_renderer *base,struct wld_buffer *buffer,
                           struct wld_buffer *cursor,int32_t cursor_x,int32_t cursor_y)
{
	struct gl_renderer *r=gl_renderer(base);
	if(!buffer || !make_current(r)) return false;
	for(unsigned i=0;i<ARRAY_LENGTH(r->targets);i++) {
		struct gl_target *t=&r->targets[i];
		if(t->buffer!=buffer) continue;
		if(!r->present_fbo || r->present_width!=buffer->width ||
		   r->present_height!=buffer->height) {
			if(r->present_fbo) glDeleteFramebuffers(1,&r->present_fbo);
			if(r->present_texture) glDeleteTextures(1,&r->present_texture);
			glGenTextures(1,&r->present_texture);
			glBindTexture(GL_TEXTURE_2D,r->present_texture);
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,buffer->width,buffer->height,
			             0,GL_BGRA,GL_UNSIGNED_BYTE,NULL);
			glGenFramebuffers(1,&r->present_fbo);
			glBindFramebuffer(GL_FRAMEBUFFER,r->present_fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,
			                       GL_TEXTURE_2D,r->present_texture,0);
			if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) break;
			r->present_width=buffer->width; r->present_height=buffer->height;
		}
		glBindFramebuffer(GL_READ_FRAMEBUFFER,t->fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER,r->present_fbo);
		glFinish();
		glBlitFramebuffer(0,0,buffer->width,buffer->height,
		                  0,0,buffer->width,buffer->height,
		                  GL_COLOR_BUFFER_BIT,GL_NEAREST);
		if(cursor && upload(r,cursor)) {
			struct wld_buffer dummy={.width=buffer->width,.height=buffer->height};
			struct gl_target overlay={.buffer=&dummy,.fbo=r->present_fbo};
			struct gl_target *saved=r->target;
			r->target=&overlay;
			draw(r,cursor_x,cursor_y,cursor->width,cursor->height,
			     0,0,1,1,r->active_upload,0xffffffffu,false,true);
			r->target=saved;
		}
		glBindFramebuffer(GL_READ_FRAMEBUFFER,r->present_fbo);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		int (*present)(void)=dlsym(RTLD_DEFAULT,"pd_radeon_present");
		bool ok=present && present()==0;
		eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
		return ok;
	}
	eglMakeCurrent(r->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	return false;
}
