// SPDX-License-Identifier: MIT
// cgl-zink-test: a core 3.2 CGL context, its renderer, and WebRender's needs from it
// rendering into an IOSurface texture, a fence, and readback through a pixel buffer
#include <OpenGL/OpenGL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>

extern const CFStringRef kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement, kIOSurfacePixelFormat;
IOSurfaceRef IOSurfaceCreate(CFDictionaryRef);
void *IOSurfaceGetBaseAddress(IOSurfaceRef);
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef);
uint32_t IOSurfaceGetSeed(IOSurfaceRef);

static const char *vs_src =
	"#version 150\n"
	"in vec2 pos;\n"
	"void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
static const char *fs_src =
	"#version 150\n"
	"out vec4 color;\n"
	"void main() { color = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static void put(CFMutableDictionaryRef d, CFStringRef k, long long v) {
	CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongLongType, &v);
	CFDictionarySetValue(d, k, n);
	CFRelease(n);
}

static GLuint shader(GLenum type, const char *src) {
	GLuint s = glCreateShader(type);
	GLint ok = 0;
	char log[512];

	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		printf("shader: %s\n", log);
	}
	return s;
}

int main(void) {
	CGLPixelFormatAttribute attrs[] = {
		kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core, kCGLPFAColorSize, 24,
		kCGLPFAAlphaSize, 8, kCGLPFAAccelerated, 0
	};
	CGLPixelFormatObj pix = NULL;
	CGLContextObj ctx = NULL;
	GLint npix = 0;
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	IOSurfaceRef s;
	uint32_t *px, seed;
	size_t stride;
	GLuint tex, fbo, vao, vbo, prog, pbo;
	int failures = 0;
	const float tri[] = { -1, -1, 0, -1, -1, 1, 0, -1, 0, 1, -1, 1 };

	if (CGLChoosePixelFormat(attrs, &pix, &npix) != kCGLNoError || CGLCreateContext(pix, NULL, &ctx) != kCGLNoError ||
	    CGLSetCurrentContext(ctx) != kCGLNoError) {
		printf("FAIL no CGL context\n");
		return 1;
	}
	printf("renderer %s\nversion %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
	printf("%s core 3.2 or newer\n", strncmp((const char *)glGetString(GL_VERSION), "3.", 2) >= 0 ? "ok  " : "FAIL");

	// a 64x64 BGRA surface rendered into through a rectangle texture, the way NativeLayerCA does it
	put(d, kIOSurfaceWidth, 64), put(d, kIOSurfaceHeight, 64), put(d, kIOSurfaceBytesPerElement, 4);
	put(d, kIOSurfacePixelFormat, 'BGRA');
	s = IOSurfaceCreate(d);
	px = IOSurfaceGetBaseAddress(s);
	stride = IOSurfaceGetBytesPerRow(s) / 4;
	memset(px, 0, IOSurfaceGetBytesPerRow(s) * 64);
	seed = IOSurfaceGetSeed(s);

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_RECTANGLE, tex);
	CGLError err = CGLTexImageIOSurface2D(ctx, GL_TEXTURE_RECTANGLE, GL_RGBA, 64, 64, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, s, 0);
	printf("%s CGLTexImageIOSurface2D returned %d\n", err == kCGLNoError ? "ok  " : "FAIL", err), failures += err != kCGLNoError;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, tex, 0);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	printf("%s framebuffer complete (%x)\n", status == GL_FRAMEBUFFER_COMPLETE ? "ok  " : "FAIL", status);
	failures += status != GL_FRAMEBUFFER_COMPLETE;

	// red everywhere, then green over the left half
	glViewport(0, 0, 64, 64);
	glClearColor(1, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	prog = glCreateProgram();
	glAttachShader(prog, shader(GL_VERTEX_SHADER, vs_src));
	glAttachShader(prog, shader(GL_FRAGMENT_SHADER, fs_src));
	glBindAttribLocation(prog, 0, "pos");
	glLinkProgram(prog);
	glUseProgram(prog);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// a fence, as WebRender waits on frames
	GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	GLenum wait = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
	printf("%s fence signalled (%x)\n", wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED ? "ok  " : "FAIL", wait);
	failures += !(wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED);
	glDeleteSync(sync);

	// readback through a pixel buffer object
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
	glBufferData(GL_PIXEL_PACK_BUFFER, 64 * 64 * 4, NULL, GL_STREAM_READ);
	glReadPixels(0, 0, 64, 64, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);
	uint32_t *mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 64 * 64 * 4, GL_MAP_READ_BIT);
	int pbo_ok = mapped != NULL && mapped[32 * 64 + 8] == 0xff00ff00u && mapped[32 * 64 + 56] == 0xffff0000u;
	printf("%s pixel buffer readback (%08x %08x)\n", pbo_ok ? "ok  " : "FAIL", mapped ? mapped[32 * 64 + 8] : 0,
	    mapped ? mapped[32 * 64 + 56] : 0);
	failures += !pbo_ok;
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	// glFlush hands the frame on: the surface must hold it now and its seed must have moved
	glFlush();
	int surf_ok = px[32 * stride + 8] == 0xff00ff00u && px[32 * stride + 56] == 0xffff0000u;
	printf("%s rendering reached the surface (%08x %08x)\n", surf_ok ? "ok  " : "FAIL", px[32 * stride + 8], px[32 * stride + 56]);
	failures += !surf_ok;
	printf("%s surface seed moved (%u -> %u)\n", IOSurfaceGetSeed(s) != seed ? "ok  " : "FAIL", seed, IOSurfaceGetSeed(s));
	failures += IOSurfaceGetSeed(s) == seed;

	printf("%s cgl-zink-test, %d failures\n", failures ? "FAIL" : "PASS", failures);
	return failures != 0;
}
