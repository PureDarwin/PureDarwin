#ifndef SWC_BACKEND_H
#define SWC_BACKEND_H

#include <stdint.h>

struct wld_context;
struct wld_renderer;

/* rendering state shared by the drm and fb backends */
struct swc_backend {
	struct wld_context *context;
	struct wld_renderer *renderer;
	uint32_t cursor_width;
	uint32_t cursor_height;
};

#endif
