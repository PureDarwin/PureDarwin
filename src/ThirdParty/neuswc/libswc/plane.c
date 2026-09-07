/* swc: libswc/plane.c
 *
 * Copyright (c) 2019 Michael Forney
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

#include "plane.h"
#include "drm.h"
#include "event.h"
#include "internal.h"
#include "screen.h"
#include "util.h"

#include <errno.h>
#include <stdlib.h>
#include <wld/drm.h>
#include <wld/wld.h>
#include <xf86drmMode.h>

enum plane_property {
	PLANE_TYPE,
	PLANE_IN_FENCE_FD,
	PLANE_CRTC_ID,
	PLANE_CRTC_X,
	PLANE_CRTC_Y,
	PLANE_CRTC_W,
	PLANE_CRTC_H,
	PLANE_SRC_X,
	PLANE_SRC_Y,
	PLANE_SRC_W,
	PLANE_SRC_H,
};

static bool
update(struct view *view)
{
	struct plane *plane = wl_container_of(view, plane, view);
	uint32_t x, y, w, h;

	if (!plane->screen) {
		return false;
	}
	x = view->geometry.x - plane->screen->base.geometry.x;
	y = view->geometry.y - plane->screen->base.geometry.y;
	w = view->geometry.width;
	h = view->geometry.height;
	if (swc.active &&
	    drmModeSetPlane(swc.drm->fd, plane->id, plane->screen->crtc, plane->fb,
	                    0, x, y, w, h, 0, 0, w << 16, h << 16) < 0) {
		ERROR("Could not set cursor: %s\n", strerror(errno));
		return false;
	}

	return true;
}

static int
attach(struct view *view, struct wld_buffer *buffer)
{
	struct plane *plane = wl_container_of(view, plane, view);

	plane->fb = drm_get_framebuffer(buffer);
	view_set_size_from_buffer(view, buffer);
	return 0;
}

static bool
move(struct view *view, int32_t x, int32_t y)
{
	view_set_position(view, x, y);
	return true;
}

static const struct view_impl view_impl = {
    .update = update,
    .attach = attach,
    .move = move,
};

static enum plane_property
find_prop(const char *name)
{
	static const char property_names[][16] = {
	    [PLANE_TYPE] = "type",       [PLANE_IN_FENCE_FD] = "IN_FENCE_FD",
	    [PLANE_CRTC_ID] = "CRTC_ID", [PLANE_CRTC_X] = "CRTC_X",
	    [PLANE_CRTC_Y] = "CRTC_Y",   [PLANE_CRTC_W] = "CRTC_W",
	    [PLANE_CRTC_H] = "CRTC_H",   [PLANE_SRC_X] = "SRC_X",
	    [PLANE_SRC_Y] = "SRC_Y",     [PLANE_SRC_W] = "SRC_W",
	    [PLANE_SRC_H] = "SRC_H",
	};
	size_t i;

	for (i = 0; i < ARRAY_LENGTH(property_names); ++i) {
		if (strcmp(name, property_names[i]) == 0) {
			return i;
		}
	}
	return -1;
}

static void
handle_swc_event(struct wl_listener *listener, void *data)
{
	struct event *event = data;
	struct plane *plane = wl_container_of(listener, plane, swc_listener);

	switch (event->type) {
	case SWC_EVENT_ACTIVATED:
		update(&plane->view);
		break;
	}
}

struct plane *
plane_new(uint32_t id)
{
	struct plane *plane;
	uint32_t i;
	drmModeObjectProperties *props;
	drmModePropertyRes *prop;
	drmModePlane *drm_plane;

	plane = malloc(sizeof(*plane));
	if (!plane) {
		goto error0;
	}
	drm_plane = drmModeGetPlane(swc.drm->fd, id);
	if (!drm_plane) {
		goto error1;
	}
	plane->id = id;
	plane->fb = 0;
	plane->screen = NULL;
	plane->possible_crtcs = drm_plane->possible_crtcs;
	drmModeFreePlane(drm_plane);
	plane->type = -1;
	props = drmModeObjectGetProperties(swc.drm->fd, id, DRM_MODE_OBJECT_PLANE);
	for (i = 0; i < props->count_props; ++i, drmModeFreeProperty(prop)) {
		prop = drmModeGetProperty(swc.drm->fd, props->props[i]);
		if (prop && find_prop(prop->name) == PLANE_TYPE) {
			plane->type = props->prop_values[i];
		}
	}
	plane->swc_listener.notify = &handle_swc_event;
	wl_signal_add(&swc.event_signal, &plane->swc_listener);
	view_initialize(&plane->view, &view_impl);
	return plane;

error1:
	free(plane);
error0:
	return NULL;
}

void
plane_destroy(struct plane *plane)
{
	free(plane);
}
