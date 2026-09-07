#ifndef SWC_DRM_H
#define SWC_DRM_H

#include "backend.h"

#include <stdbool.h>
#include <stdint.h>

struct wl_list;
struct wld_buffer;

struct drm_handler {
	void (*page_flip)(struct drm_handler *handler, uint32_t time);
};

struct swc_drm {
	struct swc_backend backend;
	int fd;
	uint32_t cursor_w, cursor_h;
	struct wld_context *context;
	struct wld_renderer *renderer;
};

bool
drm_initialize(void);
void
drm_finalize(void);

bool
drm_create_screens(struct wl_list *screens);
uint32_t
drm_get_framebuffer(struct wld_buffer *buffer);

#endif
