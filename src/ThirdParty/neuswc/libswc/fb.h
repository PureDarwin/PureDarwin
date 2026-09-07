#ifndef SWC_FB_H
#define SWC_FB_H

#include "backend.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wl_list;
struct wld_buffer;

struct swc_fb {
	struct swc_backend backend;
	uint32_t width;
	uint32_t height;
	uint32_t *pixels;
	size_t pitch;
};

extern struct swc_fb swc_fb;

bool fb_initialize(void);
void fb_finalize(void);
bool fb_create_screens(struct wl_list *screens);
bool fb_present(struct wld_buffer *buffer, int32_t x, int32_t y);

bool framebuffer_initialize(struct swc_fb *fb);
void framebuffer_finalize(struct swc_fb *fb);
bool framebuffer_present(struct swc_fb *fb);
const char *framebuffer_name(void);

#endif
