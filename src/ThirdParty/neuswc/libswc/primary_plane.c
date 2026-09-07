/* swc: primary_plane.c
 *
 * Copyright (c) 2013, 2014, 2016 Michael Forney
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

#include "primary_plane.h"
#include "event.h"
#ifdef ENABLE_DRM
#include "drm.h"
#else
#include "compositor.h"
#include "fb.h"
#endif
#include "internal.h"
#include "launch.h"
#include "util.h"

#ifdef ENABLE_DRM
#include <errno.h>
#include <wld/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif
#include <wld/wld.h>

static bool
update(struct view *view)
{
	return true;
}

static void
send_frame(void *data)
{
	struct primary_plane *plane = data;

	view_frame(&plane->view, get_time());
}

static int
attach(struct view *view, struct wld_buffer *buffer)
{
	struct primary_plane *plane = wl_container_of(view, plane, view);
#ifdef ENABLE_DRM
	uint32_t fb;
	int ret;

	fb = drm_get_framebuffer(buffer);
	if (plane->need_modeset) {
		ret = drmModeSetCrtc(swc.drm->fd, plane->crtc, fb, 0, 0,
		                     plane->connectors.data, plane->connectors.size / 4,
		                     &plane->mode.info);

		if (ret == 0) {
			wl_event_loop_add_idle(swc.event_loop, &send_frame, plane);
			plane->need_modeset = false;
		} else {
			ERROR("Could not set CRTC to next framebuffer: %s\n",
			      strerror(-ret));
			return ret;
		}
	} else {
		ret = drmModePageFlip(swc.drm->fd, plane->crtc, fb,
		                      DRM_MODE_PAGE_FLIP_EVENT, &plane->drm_handler);

		if (ret < 0) {
			ERROR("Page flip failed: %s\n", strerror(errno));
			return ret;
		}
	}

	return 0;
#else
	if (!fb_present(buffer, view->geometry.x, view->geometry.y)) {
		return -1;
	}
	wl_event_loop_add_idle(swc.event_loop, &send_frame, plane);
	return 0;
#endif
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

#ifdef ENABLE_DRM
static void
handle_page_flip(struct drm_handler *handler, uint32_t time)
{
	struct primary_plane *plane = wl_container_of(handler, plane, drm_handler);
	view_frame(&plane->view, time);
}
#endif

static void
handle_swc_event(struct wl_listener *listener, void *data)
{
	struct event *event = data;
	struct primary_plane *plane =
	    wl_container_of(listener, plane, swc_listener);

	switch (event->type) {
	case SWC_EVENT_ACTIVATED:
#ifdef ENABLE_DRM
		plane->need_modeset = true;
#else
		compositor_damage_all();
#endif
		break;
	}
}

bool
#ifdef ENABLE_DRM
primary_plane_initialize(struct primary_plane *plane, uint32_t crtc,
                         struct mode *mode, uint32_t *connectors,
                         uint32_t num_connectors)
#else
primary_plane_initialize(struct primary_plane *plane, struct mode *mode)
#endif
{
#ifdef ENABLE_DRM
	uint32_t *plane_connectors;

	if (!(plane->original_crtc_state = drmModeGetCrtc(swc.drm->fd, crtc))) {
		ERROR("Failed to get CRTC state for CRTC %u: %s\n", crtc,
		      strerror(errno));
		goto error0;
	}

	wl_array_init(&plane->connectors);
	plane_connectors = wl_array_add(&plane->connectors,
	                                num_connectors * sizeof(connectors[0]));

	if (!plane_connectors) {
		ERROR("Failed to allocate connector array\n");
		goto error1;
	}

	memcpy(plane_connectors, connectors,
	       num_connectors * sizeof(connectors[0]));
	plane->crtc = crtc;
	plane->need_modeset = true;
	view_initialize(&plane->view, &view_impl);
	plane->view.geometry.width = mode->width;
	plane->view.geometry.height = mode->height;
	plane->drm_handler.page_flip = &handle_page_flip;
	plane->swc_listener.notify = &handle_swc_event;
	plane->mode = *mode;
#else
	view_initialize(&plane->view, &view_impl);
	plane->view.geometry.width = mode->width;
	plane->view.geometry.height = mode->height;
	plane->mode = *mode;
#endif
	plane->swc_listener.notify = &handle_swc_event;
	wl_signal_add(&swc.event_signal, &plane->swc_listener);

	return true;

#ifdef ENABLE_DRM
error1:
	drmModeFreeCrtc(plane->original_crtc_state);
error0:
	return false;
#endif
}

void
primary_plane_finalize(struct primary_plane *plane)
{
	wl_list_remove(&plane->swc_listener.link);
#ifdef ENABLE_DRM
	wl_array_release(&plane->connectors);
	drmModeCrtcPtr crtc = plane->original_crtc_state;
	drmModeSetCrtc(swc.drm->fd, crtc->crtc_id, crtc->buffer_id, crtc->x,
	               crtc->y, NULL, 0, &crtc->mode);
	drmModeFreeCrtc(crtc);
#else
	(void)plane;
#endif
}
