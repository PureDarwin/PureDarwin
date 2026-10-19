#ifndef SWC_FB_H
#define SWC_FB_H

#include "backend.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wl_list;
struct wld_buffer;
struct pixman_region32;

struct swc_fb {
	struct swc_backend backend;
	uint32_t width;
	uint32_t height;
	uint32_t *pixels;
	size_t pitch;
	// rows the last fb_present changed, every row when damage_y1 >= damage_y2
	uint32_t damage_y1, damage_y2;
};

extern struct swc_fb swc_fb;

bool fb_initialize(void);
void fb_finalize(void);
bool fb_create_screens(struct wl_list *screens);
bool fb_present(struct wld_buffer *buffer, int32_t x, int32_t y);
bool fb_set_mode(uint32_t mode_id);
// what changed on screen in the frame about to be presented, NULL for everything
void fb_set_damage(const struct pixman_region32 *damage);
// marks where a frame's work starts, for ARGON_GFX_STATS
void fb_frame_begin(void);

// a framebuffer mode: its id for the driver and its size
struct fb_mode {
	uint32_t id;
	uint32_t width, height;
	bool current;
};

bool framebuffer_initialize(struct swc_fb *fb);
void framebuffer_finalize(struct swc_fb *fb);
bool framebuffer_present(struct swc_fb *fb);
const char *framebuffer_name(void);
// direct scanout of a GPU resource, only where the framebuffer driver offers it
bool framebuffer_gpu_scanout_available(void);
bool framebuffer_set_scanout_resource(uint32_t resource, uint32_t width, uint32_t height);
bool framebuffer_present_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
// a flip pair the GPU can render into: dma-buf fds of both buffers, then flips by index
int framebuffer_scanout_dmabufs(int *fds, uint32_t *width, uint32_t *height, uint32_t *pitch);
int framebuffer_back_index(void);
bool framebuffer_flip(int index);
int framebuffer_modes(struct fb_mode *modes, int max);
bool framebuffer_set_mode(struct swc_fb *fb, uint32_t mode_id);

#endif
