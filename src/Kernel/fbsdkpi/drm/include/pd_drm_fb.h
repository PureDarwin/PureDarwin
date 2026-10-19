// SPDX-License-Identifier: MIT
// the display client a drm kext offers its IOFramebuffer
#ifndef _PD_DRM_FB_H_
#define _PD_DRM_FB_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pd_drm_fb_info {
	uint64_t phys;		// cpu physical address of the pixels, contiguous
	uint32_t width;
	uint32_t height;
	uint32_t pitch;		// bytes per row, XRGB8888
	uint32_t refresh;	// Hz
};

struct pd_drm_fb_mode {
	uint32_t width;
	uint32_t height;
	uint32_t refresh;	// Hz
	uint32_t preferred;
};

// set by the driver glue, ops is NULL when the kext has no display support
struct pd_drm_fb_ops {
	// probes and lights the first connected display at its preferred mode,
	// or at the boot-arg pdfb_mode=WxH when the display has that mode
	int (*setup)(struct pd_drm_fb_info *info);
	// what is on screen now, after a mode change or a connector change
	int (*on_screen)(struct pd_drm_fb_info *info);
	// the connected display's modes, index 0 is the one setup chose
	int (*mode_count)(void);
	int (*get_mode)(int index, struct pd_drm_fb_mode *mode);
	// switches to a listed mode with a new scanout buffer
	int (*set_mode)(int index, struct pd_drm_fb_info *info);
	// unpins and frees the scanout buffer and drops the client
	void (*release)(void);
	// page flipping for a presenter: buffer 0 is what setup lit, buffer 1 a
	// second one of the same size (made on first use), both cpu physical
	int (*buffer)(int index, uint64_t *phys);
	// scans out a buffer from the next vblank, returns after the flip with
	// that vblank's sequence and time
	int (*flip)(int index, uint64_t *sequence, uint64_t *nanoseconds);
	// a dma-buf fd in the calling process for buffer index, so a GPU renderer
	// can draw straight into the scanout buffer
	int (*export_fd)(int index, int *fd);
	// called after a connector change was handled, set by the framebuffer
	void (*changed)(void *context);
	void *changed_context;
};
extern struct pd_drm_fb_ops *pd_drm_fb;

#ifdef __cplusplus
}
#endif

#endif
