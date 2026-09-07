#ifndef SWC_OUTPUT_H
#define SWC_OUTPUT_H

#include <pixman.h>
#include <stdint.h>
#include <wayland-util.h>
#ifdef ENABLE_DRM
#include <xf86drmMode.h>
#endif

struct wl_display;

struct output {
	struct wl_resource *resource;
	struct screen *screen;

	char name[24];
	/* The physical dimensions (in mm) of this output */
	uint32_t physical_width, physical_height;

	struct wl_array modes;
	struct mode *preferred_mode;

	pixman_region32_t current_damage, previous_damage;

#ifdef ENABLE_DRM
	/* The DRM connector corresponding to this output */
	uint32_t connector;
#endif

	struct wl_global *global;
	struct wl_list resources;
	struct wl_list link;
};

#ifdef ENABLE_DRM
struct output *
output_new(drmModeConnector *connector);
#endif
struct output *
output_new_fb(uint32_t width, uint32_t height, const char *name);
void
output_destroy(struct output *output);

#endif
