// SPDX-License-Identifier: MIT
// the display client a drm kext offers its IOFramebuffers, one per lit display
#ifndef _PD_DRM_FB_H_
#define _PD_DRM_FB_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// the most displays one gpu shows at once, each is an IOFramebuffer
#define PD_DRM_FB_MAX_DISPLAYS 4

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

// set by the driver glue, ops is NULL when the kext has no display support. display is the index of
// a lit display, 0 is the first, an index keeps its display across connector changes
struct pd_drm_fb_ops {
	// probes and lights every connected display at its preferred mode, or at the boot-arg
	// pdfb_mode=WxH when the first one has that mode, and returns display 0
	int (*setup)(struct pd_drm_fb_info *info);
	// how many display indices are in use, some may be dark after a connector change
	int (*display_count)(void);
	// what the display shows now, after a mode change or a connector change
	int (*on_screen)(int display, struct pd_drm_fb_info *info);
	// the display's modes, index 0 is the one in use
	int (*mode_count)(int display);
	int (*get_mode)(int display, int index, struct pd_drm_fb_mode *mode);
	// switches the display to a listed mode with a new scanout buffer
	int (*set_mode)(int display, int index, struct pd_drm_fb_info *info);
	// darkens every display, frees their buffers and drops the client
	void (*release)(void);
	// page flipping for a presenter: buffer 0 is what lit the display, buffer 1 a second one of
	// the same size (made on first use), both cpu physical
	int (*buffer)(int display, int index, uint64_t *phys);
	// scans out a buffer from the next vblank, returns after the flip with that vblank's
	// sequence and time
	int (*flip)(int display, int index, uint64_t *sequence, uint64_t *nanoseconds);
	// a dma-buf fd in the calling process for buffer index, so a GPU renderer can draw straight
	// into the scanout buffer
	int (*export_fd)(int display, int index, int *fd);
	// called for each display after a connector change was handled, set by its framebuffer
	void (*changed)(void *context);
	void *changed_context[PD_DRM_FB_MAX_DISPLAYS];
};
extern struct pd_drm_fb_ops *pd_drm_fb;

#ifdef __cplusplus
}
#endif

#endif
