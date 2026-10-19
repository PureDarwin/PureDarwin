// SPDX-License-Identifier: MIT
// pdgltri: a windowless GLES2 triangle on mesa's surfaceless EGL platform
// drawn into a framebuffer object and read back to check the gpu result

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W 64
#define H 64

static const char *vs_src =
	"attribute vec2 pos;\n"
	"void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
static const char *fs_src =
	"precision mediump float;\n"
	"void main() { gl_FragColor = vec4(1.0, 0.5, 0.0, 1.0); }\n";

static GLuint
shader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	GLint ok = 0;
	char log[512];

	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		printf("pdgltri: shader compile failed: %s\n", log);
	}
	return s;
}

static double
now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

// bring-up: draw and full-screen readback timings, to compare drivers
static void
bench(void)
{
	enum { BW = 1920, BH = 1080 };
	unsigned char *buf = malloc((size_t)BW * BH * 4);
	GLuint tex, fbo;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, BW, BH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	glViewport(0, 0, BW, BH);
	// rgba whole, bgra whole, then bgra rows 0-603 with a row length, as neuswc reads its damage
	static const char *names[] = { "rgba", "bgra", "bgra-rowlen" };
	for (int v = 0; v < 3; v++) {
		for (int i = 0; i < 4; i++) {
			double t0 = now_ms(), t1, t2;

			glClearColor(0, 0, 1, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glFinish();
			t1 = now_ms();
			if (v == 0)
				glReadPixels(0, 0, BW, BH, GL_RGBA, GL_UNSIGNED_BYTE, buf);
			else if (v == 1)
				glReadPixels(0, 0, BW, BH, 0x80E1, GL_UNSIGNED_BYTE, buf);
			else {
				glPixelStorei(0x0D02, BW);
				glReadPixels(0, 0, BW, 603, 0x80E1, GL_UNSIGNED_BYTE, buf);
				glPixelStorei(0x0D02, 0);
			}
			t2 = now_ms();
			printf("pdgltri: bench %s %d draw+finish %.1f ms, readback %.1f ms, glGetError 0x%x\n",
			    names[v], i, t1 - t0, t2 - t1, glGetError());
		}
	}
	free(buf);
}

int
main(int argc, char **argv)
{
	PFNEGLGETPLATFORMDISPLAYEXTPROC get_display =
	    (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	static const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	static const GLfloat tri[] = { -1, -1, 1, -1, 0, 1 };
	static unsigned char px[W * H * 4];
	EGLDisplay dpy;
	EGLContext ctx;
	EGLConfig cfg;
	EGLint major, minor, n = 0;
	GLuint fbo, rb, prog;
	int inside, outside;

	if (get_display == NULL) {
		printf("pdgltri: no eglGetPlatformDisplayEXT\n");
		return 1;
	}
	dpy = get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
		printf("pdgltri: eglInitialize failed: 0x%x\n", eglGetError());
		return 1;
	}
	printf("pdgltri: EGL %d.%d, %s\n", major, minor, eglQueryString(dpy, EGL_VENDOR));
	eglBindAPI(EGL_OPENGL_ES_API);
	eglChooseConfig(dpy, (EGLint[]){ EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE },
	    &cfg, 1, &n);
	ctx = eglCreateContext(dpy, n > 0 ? cfg : EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ctx_attribs);
	if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
		printf("pdgltri: context failed: 0x%x\n", eglGetError());
		return 1;
	}
	printf("pdgltri: GL_VENDOR %s\n", glGetString(GL_VENDOR));
	printf("pdgltri: GL_RENDERER %s\n", glGetString(GL_RENDERER));
	printf("pdgltri: GL_VERSION %s\n", glGetString(GL_VERSION));

	glGenRenderbuffers(1, &rb);
	glBindRenderbuffer(GL_RENDERBUFFER, rb);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, W, H);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("pdgltri: framebuffer incomplete\n");
		return 1;
	}

	prog = glCreateProgram();
	glAttachShader(prog, shader(GL_VERTEX_SHADER, vs_src));
	glAttachShader(prog, shader(GL_FRAGMENT_SHADER, fs_src));
	glBindAttribLocation(prog, 0, "pos");
	glLinkProgram(prog);
	glUseProgram(prog);

	glViewport(0, 0, W, H);
	glClearColor(0, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, tri);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);

	// the centre is inside the triangle (orange), a bottom corner is not (blue)
	inside = (H / 2 * W + W / 2) * 4;
	outside = (H - 1) * W * 4;
	printf("pdgltri: centre %02x%02x%02x%02x, corner %02x%02x%02x%02x, glGetError 0x%x\n",
	    px[inside], px[inside + 1], px[inside + 2], px[inside + 3],
	    px[outside], px[outside + 1], px[outside + 2], px[outside + 3], glGetError());
	if (px[inside] > 0xe0 && px[inside + 1] > 0x60 && px[inside + 1] < 0xa0 && px[inside + 2] < 0x20 &&
	    px[outside + 2] > 0xe0 && px[outside] < 0x20) {
		printf("pdgltri: triangle drawn and read back\n");
		if (argc > 1 && strcmp(argv[1], "bench") == 0)
			bench();
		return 0;
	}
	printf("pdgltri: FAILED, unexpected pixels\n");
	return 1;
}
