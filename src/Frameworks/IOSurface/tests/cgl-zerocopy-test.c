// cgl-zerocopy-test.c - a 1024x512 BGRA surface in GPU memory as a zink render target with no copies
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// checks its pixels here and in a second process, compares them with the copy path (PD_CGL_ZEROCOPY=0)
// and times draw plus glFlush both ways
#include <OpenGL/OpenGL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <CoreFoundation/CoreFoundation.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;
extern const CFStringRef kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement, kIOSurfacePixelFormat, kIOSurfaceIsGlobal;
IOSurfaceRef IOSurfaceCreate(CFDictionaryRef);
IOSurfaceRef IOSurfaceLookup(uint32_t);
uint32_t IOSurfaceGetID(IOSurfaceRef);
void *IOSurfaceGetBaseAddress(IOSurfaceRef);
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef);
uint32_t IOSurfaceGetSeed(IOSurfaceRef);
int IOSurfacePDCopyGPUFD(IOSurfaceRef, uint64_t *);

#define W 1024
#define H 512

static double now(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void put(CFMutableDictionaryRef d, CFStringRef k, long long v) {
	CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongLongType, &v);
	CFDictionarySetValue(d, k, n);
	CFRelease(n);
}

static GLuint program(void) {
	static const char *vs = "#version 150\nin vec2 p;\nvoid main(){gl_Position=vec4(p,0,1);}\n";
	// a pattern with position in it, so a wrong pitch or swapped channels show
	static const char *fs = "#version 150\nuniform float t;\nout vec4 c;\n"
	                        "void main(){c=vec4(fract(gl_FragCoord.x/256.0),fract(gl_FragCoord.y/128.0),fract(t),1);}\n";
	GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER), p = glCreateProgram();
	glShaderSource(v, 1, &vs, NULL), glCompileShader(v);
	glShaderSource(f, 1, &fs, NULL), glCompileShader(f);
	glAttachShader(p, v), glAttachShader(p, f), glBindAttribLocation(p, 0, "p"), glLinkProgram(p);
	return p;
}

// the byte sum of the surface, a cheap stand-in for comparing whole images between processes
static unsigned long long checksum(IOSurfaceRef s) {
	const uint8_t *px = IOSurfaceGetBaseAddress(s);
	size_t stride = IOSurfaceGetBytesPerRow(s);
	unsigned long long sum = 0;

	for (int y = 0; y < H; y++)
		for (int x = 0; x < W * 4; x++)
			sum = sum * 31 + px[y * stride + x];
	return sum;
}

static int child(const char *mode, uint32_t id) {
	char ids[32];
	pid_t pid;
	int status;
	char *argv[] = { "cgl-zerocopy-test", (char *)mode, ids, NULL };

	snprintf(ids, sizeof(ids), "%u", id);
	if (posix_spawn(&pid, "/usr/local/bin/cgl-zerocopy-test", NULL, NULL, argv, environ) != 0)
		return -1;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int main(int argc, char **argv) {
	// a second process: print the surface's checksum
	if (argc == 3 && strcmp(argv[1], "look") == 0) {
		IOSurfaceRef s = IOSurfaceLookup((uint32_t)atoi(argv[2]));
		if (s == NULL) {
			printf("FAIL lookup in a second process\n");
			return 1;
		}
		printf("lookup %016llx\n", checksum(s));
		return 0;
	}

	CGLPixelFormatAttribute attrs[] = { kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core, 0 };
	CGLPixelFormatObj pix;
	CGLContextObj ctx;
	GLint npix;
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	int failures = 0;
	const char *mode = getenv("PD_CGL_ZEROCOPY") && strcmp(getenv("PD_CGL_ZEROCOPY"), "0") == 0 ? "copy" : "zero-copy";

	CGLChoosePixelFormat(attrs, &pix, &npix);
	CGLCreateContext(pix, NULL, &ctx);
	CGLSetCurrentContext(ctx);
	put(d, kIOSurfaceWidth, W), put(d, kIOSurfaceHeight, H), put(d, kIOSurfaceBytesPerElement, 4);
	put(d, kIOSurfacePixelFormat, 'BGRA');
	CFDictionarySetValue(d, kIOSurfaceIsGlobal, kCFBooleanTrue);
	IOSurfaceRef s = IOSurfaceCreate(d);
	uint64_t size = 0;
	int fd = IOSurfacePDCopyGPUFD(s, &size);
	printf("%s: renderer %s, surface in %s memory, pitch %zu\n", mode, glGetString(GL_RENDERER),
	    fd >= 0 ? "GPU" : "ordinary", IOSurfaceGetBytesPerRow(s));
	if (fd >= 0)
		close(fd);

	GLuint tex, fbo, vao, vbo, prog = program();
	static const float q[] = { -1, -1, 1, -1, -1, 1, 1, -1, 1, 1, -1, 1 };
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_RECTANGLE, tex);
	CGLError err = CGLTexImageIOSurface2D(ctx, GL_TEXTURE_RECTANGLE, GL_RGBA, W, H, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, s, 0);
	printf("%s CGLTexImageIOSurface2D (%d)\n", err == kCGLNoError ? "ok  " : "FAIL", err), failures += err != kCGLNoError;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, tex, 0);
	glUseProgram(prog);
	glGenVertexArrays(1, &vao), glBindVertexArray(vao);
	glGenBuffers(1, &vbo), glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0), glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glViewport(0, 0, W, H);

	// one frame with a known value, then its pixels here and in a second process
	glUniform1f(glGetUniformLocation(prog, "t"), 0.5f);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glFlush();
	const uint32_t *px = IOSurfaceGetBaseAddress(s);
	size_t stride = IOSurfaceGetBytesPerRow(s) / 4;
	// at x=64,y=32: r=64/256, g=32/128, b=0.5, so BGRA bytes 7f 40 40 ff, the word ff40407f
	uint32_t p = px[32 * stride + 64];
	int near = ((p >> 16 & 0xff) >= 0x3f && (p >> 16 & 0xff) <= 0x41) && ((p >> 8 & 0xff) >= 0x3f && (p >> 8 & 0xff) <= 0x41) &&
	           ((p & 0xff) >= 0x7f && (p & 0xff) <= 0x80) && (p >> 24) == 0xff;
	printf("%s pixel at 64,32 is %08x (want about ff40407f)\n", near ? "ok  " : "FAIL", p), failures += !near;
	unsigned long long mine = checksum(s);
	printf("here %016llx\n", mine);
	fflush(stdout);
	failures += child("look", IOSurfaceGetID(s)) != 0;

	// frames: a draw and the flush that hands the surface on
	uint32_t seed = IOSurfaceGetSeed(s);
	double t0 = now(), worst = 0;
	for (int i = 0; i < 300; i++) {
		double f = now();
		glUniform1f(glGetUniformLocation(prog, "t"), i / 300.0f);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glFlush();
		if (now() - f > worst)
			worst = now() - f;
	}
	double per = (now() - t0) / 300;
	clock_t cpu = clock();
	printf("frames: %.3f ms each, worst %.3f ms, seed moved %u times, cpu %.0f ms total\n", per, worst,
	    IOSurfaceGetSeed(s) - seed, cpu * 1000.0 / CLOCKS_PER_SEC);
	printf("%s cgl-zerocopy-test (%s), %d failures\n", failures ? "FAIL" : "PASS", mode, failures);
	return failures != 0;
}
