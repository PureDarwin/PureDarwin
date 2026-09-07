/* swc: libswc/screen.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "screen.h"
#ifdef ENABLE_DRM
#include "drm.h"
#else
#include "fb.h"
#endif
#include "event.h"
#include "internal.h"
#include "mode.h"
#include "output.h"
#ifdef ENABLE_DRM
#include "plane.h"
#endif
#include "pointer.h"
#include "util.h"

#include "swc-server-protocol.h"
#include <stdlib.h>

#define INTERNAL(s) ((struct screen *)(s))

static struct screen *active_screen;
static const struct swc_screen_handler null_handler;

static bool
handle_motion(struct pointer_handler *handler,
              uint32_t time,
              wl_fixed_t x,
              wl_fixed_t y);

struct pointer_handler screens_pointer_handler = {
    .motion = handle_motion,
};

EXPORT void
swc_screen_set_handler(struct swc_screen *base,
                       const struct swc_screen_handler *handler,
                       void *data)
{
	struct screen *screen = INTERNAL(base);

	screen->handler = handler;
	screen->handler_data = data;
}

bool
screens_initialize(void)
{
	wl_list_init(&swc.screens);

	if (!
#ifdef ENABLE_DRM
	    drm_create_screens(&swc.screens)
#else
	    fb_create_screens(&swc.screens)
#endif
	) {
		return false;
	}

	if (wl_list_empty(&swc.screens)) {
		return false;
	}

	return true;
}

void
screens_finalize(void)
{
	struct screen *screen, *tmp;

	wl_list_for_each_safe(screen, tmp, &swc.screens, link)
	    screen_destroy(screen);
}

static void
bind_screen(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct screen *screen = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &swc_screen_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, NULL, screen, &remove_resource);
	wl_list_insert(&screen->resources, wl_resource_get_link(resource));
}

struct screen *
#ifdef ENABLE_DRM
screen_new(uint32_t crtc, struct output *output, struct plane *cursor_plane)
#else
screen_new(struct output *output)
#endif
{
	struct screen *screen;
	int32_t x = 0;

	/* Simple heuristic for initial screen positioning. */
	wl_list_for_each(screen, &swc.screens, link) x =
	    MAX(x, screen->base.geometry.x + screen->base.geometry.width);

	if (!(screen = malloc(sizeof(*screen)))) {
		goto error0;
	}

	screen->global = wl_global_create(
	    swc.display, &swc_screen_interface, 1, screen, &bind_screen);

	if (!screen->global) {
		ERROR("Failed to create screen global\n");
		goto error1;
	}

#ifdef ENABLE_DRM
	screen->crtc = crtc;
	if (!primary_plane_initialize(&screen->planes.primary,
	                              crtc,
	                              output->preferred_mode,
	                              &output->connector,
	                              1)) {
		ERROR("Failed to initialize primary plane\n");
		goto error2;
	}

	cursor_plane->screen = screen;
	screen->planes.cursor = cursor_plane;
#else
	if (!primary_plane_initialize(&screen->planes.primary,
	                              output->preferred_mode)) {
		ERROR("Failed to initialize primary plane\n");
		goto error2;
	}
	screen->planes.cursor = NULL;
#endif

	screen->handler = &null_handler;
	wl_signal_init(&screen->destroy_signal);
	wl_list_init(&screen->resources);
	wl_list_init(&screen->outputs);
	wl_list_insert(&screen->outputs, &output->link);
	wl_list_init(&screen->modifiers);

	view_move(&screen->planes.primary.view, x, 0);
	screen->base.geometry = screen->planes.primary.view.geometry;
	screen->base.usable_geometry = screen->base.geometry;

	swc.manager->new_screen(&screen->base);

	return screen;

error2:
	wl_global_destroy(screen->global);
error1:
	free(screen);
error0:
	return NULL;
}

void
screen_destroy(struct screen *screen)
{
	struct output *output, *next;

	if (active_screen == screen) {
		active_screen = NULL;
	}
	if (screen->handler->destroy) {
		screen->handler->destroy(screen->handler_data);
	}
	wl_signal_emit(&screen->destroy_signal, NULL);
	wl_list_for_each_safe(output, next, &screen->outputs, link)
	    output_destroy(output);
	primary_plane_finalize(&screen->planes.primary);
#ifdef ENABLE_DRM
	if (screen->planes.cursor) {
		plane_destroy(screen->planes.cursor);
	}
#endif
	free(screen);
}

void
screen_update_usable_geometry(struct screen *screen)
{
	pixman_region32_t total_usable, usable;
	pixman_box32_t *extents;
	struct screen_modifier *modifier;
	struct swc_rectangle *geom = &screen->base.geometry;

	DEBUG("Updating usable geometry\n");

	pixman_region32_init_rect(
	    &total_usable, geom->x, geom->y, geom->width, geom->height);
	pixman_region32_init(&usable);

	wl_list_for_each(modifier, &screen->modifiers, link)
	{
		modifier->modify(modifier, geom, &usable);
		pixman_region32_intersect(&total_usable, &total_usable, &usable);
	}

	extents = pixman_region32_extents(&total_usable);

	if (extents->x1 != screen->base.usable_geometry.x ||
	    extents->y1 != screen->base.usable_geometry.y ||
	    (extents->x2 - extents->x1) != screen->base.usable_geometry.width ||
	    (extents->y2 - extents->y1) != screen->base.usable_geometry.height) {
		screen->base.usable_geometry.x = extents->x1;
		screen->base.usable_geometry.y = extents->y1;
		screen->base.usable_geometry.width = extents->x2 - extents->x1;
		screen->base.usable_geometry.height = extents->y2 - extents->y1;

		if (screen->handler->usable_geometry_changed) {
			screen->handler->usable_geometry_changed(screen->handler_data);
		}
	}
}

bool
handle_motion(struct pointer_handler *handler,
              uint32_t time,
              wl_fixed_t fx,
              wl_fixed_t fy)
{
	struct screen *screen;
	int32_t x = wl_fixed_to_int(fx), y = wl_fixed_to_int(fy);

	wl_list_for_each(screen, &swc.screens, link)
	{
		if (rectangle_contains_point(&screen->base.geometry, x, y)) {
			if (screen != active_screen) {
				active_screen = screen;

				if (screen->handler->entered) {
					screen->handler->entered(screen->handler_data);
				}
			}
			break;
		}
	}

	return false;
}
