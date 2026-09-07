/* swc: libswc/compositor.c
 *
 * Copyright (c) 2013-2020 Michael Forney
 *
 * Based in part upon compositor.c from weston, which is:
 *
 *     Copyright © 2010-2011 Intel Corporation
 *     Copyright © 2008-2011 Kristian Høgsberg
 *     Copyright © 2012 Collabora, Ltd.
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

#include "compositor.h"
#include "backend.h"
#include "data_device_manager.h"
#include "decor.h"
#ifdef ENABLE_DRM
#include "drm.h"
#endif
#include "event.h"
#include "internal.h"
#include "launch.h"
#include "output.h"
#include "pointer.h"
#include "region.h"
#include "screen.h"
#include "seat.h"
#include "shm.h"
#include "subsurface.h"
#include "surface.h"
#include "swc.h"
#include "util.h"
#include "view.h"
#include "window.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef ENABLE_DRM
#include <wld/drm.h>
#endif
#include <wld/wld.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#define DEFAULT_BG 0xff000000u

static inline int32_t
clamp_i32(int64_t v)
{
	if (v > INT32_MAX) {
		return INT32_MAX;
	}
	if (v < INT32_MIN) {
		return INT32_MIN;
	}
	return (int32_t)v;
}

static inline uint32_t
span_u32(int32_t a, int32_t b)
{
	int64_t d = (int64_t)b - (int64_t)a;

	if (d <= 0) {
		return 0;
	}
	if (d > UINT32_MAX) {
		return UINT32_MAX;
	}
	return (uint32_t)d;
}

struct target {
	struct wld_surface *surface;
	struct wld_buffer *next_buffer, *current_buffer;
	struct view *view;
	struct view_handler view_handler;
	uint32_t mask;

	struct wl_listener screen_destroy_listener;
};

static bool
handle_motion(struct pointer_handler *handler, uint32_t time, wl_fixed_t x,
              wl_fixed_t y);
static bool
handle_button(struct pointer_handler *handler, uint32_t time,
              struct button *button, uint32_t state);
static void
perform_update(void *data);

static struct pointer_handler pointer_handler = {
    .motion = handle_motion,
    .button = handle_button,
};

static struct {
	struct wl_list views;
	pixman_region32_t damage, opaque;
	struct wl_listener swc_listener;

	/* A mask of screens that have been repainted but are waiting on a page
	 * flip. */
	uint32_t pending_flips;

	/* A mask of screens that are scheduled to be repainted on the next idle. */
	uint32_t scheduled_updates;

	bool updating;
	struct wl_global *global;
	bool initialized;

	/* zoom level (1.0 = normal, >1 = zoomed in, <1 = zoomed out) */
	float zoom;
	struct wld_buffer *zoom_buffer;
} compositor;

static struct {
	bool active;
	int32_t x, y;
	uint32_t width, height;
	uint32_t color;
	uint32_t border_width;
} overlay;

struct swc_compositor swc_compositor = {
    .pointer_handler = &pointer_handler,
};

static void
handle_screen_destroy(struct wl_listener *listener, void *data)
{
	struct target *target =
	    wl_container_of(listener, target, screen_destroy_listener);

	wld_destroy_surface(target->surface);
	free(target);
}

static struct target *
target_get(struct screen *screen)
{
	struct wl_listener *listener =
	    wl_signal_get(&screen->destroy_signal, &handle_screen_destroy);
	struct target *target;

	return listener ? wl_container_of(listener, target, screen_destroy_listener)
	                : NULL;
}

static void
handle_screen_frame(struct view_handler *handler, uint32_t time)
{
	struct target *target = wl_container_of(handler, target, view_handler);
	struct compositor_view *view;

	compositor.pending_flips &= ~target->mask;

	wl_list_for_each(view, &compositor.views, link)
	{
		if (view->visible && view->base.screens & target->mask) {
			view_frame(&view->base, time);
		}
	}

	if (target->current_buffer) {
		wld_surface_release(target->surface, target->current_buffer);
	}

	target->current_buffer = target->next_buffer;

	/* If we had scheduled updates that couldn't run because we were waiting on
	 * a page flip, run them now. If the compositor is currently updating, then
	 * the frame finished immediately, and we can be sure that there are no
	 * pending updates. */
	if (compositor.scheduled_updates && !compositor.updating) {
		perform_update(NULL);
	}
}

static const struct view_handler_impl screen_view_handler = {
    .frame = handle_screen_frame,
};

static int
target_swap_buffers(struct target *target)
{
	target->next_buffer = wld_surface_take(target->surface);
	return view_attach(target->view, target->next_buffer);
}

static struct target *
target_new(struct screen *screen)
{
	struct target *target;
	struct swc_rectangle *geom = &screen->base.geometry;

	if (!(target = malloc(sizeof(*target)))) {
		goto error0;
	}

	target->surface =
	    wld_create_surface(swc.backend->context, geom->width, geom->height,
	                       WLD_FORMAT_XRGB8888,
#ifdef ENABLE_DRM
	                       WLD_DRM_FLAG_SCANOUT
#else
	                       WLD_FLAG_MAP
#endif
	    );

	if (!target->surface) {
		goto error1;
	}

	target->view = &screen->planes.primary.view;
	target->view_handler.impl = &screen_view_handler;
	wl_list_insert(&target->view->handlers, &target->view_handler.link);
	target->current_buffer = NULL;
	target->mask = screen_mask(screen);

	target->screen_destroy_listener.notify = &handle_screen_destroy;
	wl_signal_add(&screen->destroy_signal, &target->screen_destroy_listener);

	return target;

error1:
	free(target);
error0:
	return NULL;
}

/* Rendering {{{ */

/* check the client's window buffer is opaque */
static bool
view_buffer_is_opaque(struct compositor_view *view)
{
	const struct swc_rectangle *geom = &view->base.geometry;
	struct wld_buffer *buffer = view->base.buffer;
	uint32_t x1, y1, width, height;

	if (view->buffer_opaque_valid) {
		return view->buffer_opaque;
	}

	view->buffer_opaque_valid = true;
	view->buffer_opaque = false;
	if (!buffer || buffer->format != WLD_FORMAT_ARGB8888) {
		return false;
	}

	if (view->window) {
		if (view->buffer_offset_x < 0 || view->buffer_offset_y < 0) {
			return false;
		}
		x1 = (uint32_t)view->buffer_offset_x;
		y1 = (uint32_t)view->buffer_offset_y;
		width = geom->width;
		height = geom->height;
	} else {
		x1 = 0;
		y1 = 0;
		width = buffer->width;
		height = buffer->height;
	}

	if (x1 > buffer->width || y1 > buffer->height ||
	    width > buffer->width - x1 || height > buffer->height - y1 ||
	    !wld_map(buffer)) {
		return false;
	}

	view->buffer_opaque = true;
	for (uint32_t y = 0; y < height && view->buffer_opaque; ++y) {
		const uint8_t *row = (const uint8_t *)buffer->map +
		                     (size_t)(y1 + y) * buffer->pitch +
		                     (size_t)x1 * 4;

		for (uint32_t x = 0; x < width; ++x) {
			uint32_t pixel;

			memcpy(&pixel, row + (size_t)x * 4, sizeof(pixel));
			if ((pixel >> 24) != 0xff) {
				view->buffer_opaque = false;
				break;
			}
		}
	}
	wld_unmap(buffer);

	return view->buffer_opaque;
}

static void
repaint_view(struct target *target, struct compositor_view *view,
             pixman_region32_t *damage)
{
	pixman_region32_t geom_region, buffer_region, border_region, view_damage,
	    buffer_damage, border_damage;
	const struct swc_rectangle *geom = &view->base.geometry,
	                           *target_geom = &target->view->geometry;
	int32_t buf_x, buf_y;
	uint32_t buf_w, buf_h;
	int64_t total_border;

	if (!view->base.buffer) {
		return;
	}

	buf_w = view->base.buffer->width;
	buf_h = view->base.buffer->height;
	buf_x = geom->x - view->buffer_offset_x;
	buf_y = geom->y - view->buffer_offset_y;

	total_border =
	    (int64_t)view->border.outwidth + (int64_t)view->border.inwidth;
	pixman_region32_init_rect(&geom_region, geom->x, geom->y, geom->width,
	                          geom->height);
	if (view->window) {
		pixman_region32_init_rect(&buffer_region, geom->x, geom->y, geom->width,
		                          geom->height);
	} else {
		pixman_region32_init_rect(&buffer_region, buf_x, buf_y, buf_w, buf_h);
	}
	pixman_region32_init_rect(&border_region,
	                          geom->x - (int32_t)total_border,
	                          geom->y - (int32_t)total_border,
	                          geom->width + (uint32_t)(2 * total_border),
	                          geom->height + (uint32_t)(2 * total_border));
	pixman_region32_subtract(&border_region, &border_region, &geom_region);
	pixman_region32_init_with_extents(&view_damage, &view->extents);
	pixman_region32_init(&buffer_damage);
	pixman_region32_init(&border_damage);

	pixman_region32_intersect(&view_damage, &view_damage, damage);
	pixman_region32_subtract(&view_damage, &view_damage, &view->clip);
	pixman_region32_intersect(&border_damage, &view_damage, &border_region);
	pixman_region32_intersect(&buffer_damage, &view_damage, &buffer_region);

	if (pixman_region32_not_empty(&buffer_damage)) {
		pixman_region32_translate(&buffer_damage,
		                          -geom->x + view->buffer_offset_x,
		                          -geom->y + view->buffer_offset_y);
		if (view->buffer->format == WLD_FORMAT_ARGB8888) {
			pixman_region32_t opaque_damage, blend_damage;

			/* keep pixels that we know are opaque on the accelerated path; only
			 * pixels which might have alpha need blending. */
			pixman_region32_init(&opaque_damage);
			pixman_region32_intersect(&opaque_damage, &buffer_damage,
			                          &view->surface->state.opaque);
			pixman_region32_init(&blend_damage);
			pixman_region32_subtract(&blend_damage, &buffer_damage,
			                         &opaque_damage);

			if (!pixman_region32_not_empty(&blend_damage) ||
			    view_buffer_is_opaque(view)) {
				wld_copy_region(swc.backend->renderer, view->buffer,
				                buf_x - target_geom->x,
				                buf_y - target_geom->y, &buffer_damage);
			} else {
				wld_copy_region(swc.backend->renderer, view->buffer,
				                buf_x - target_geom->x,
				                buf_y - target_geom->y, &opaque_damage);
				wld_blend_region(swc.backend->renderer, view->buffer,
				                 buf_x - target_geom->x,
				                 buf_y - target_geom->y, &blend_damage);
			}

			pixman_region32_fini(&blend_damage);
			pixman_region32_fini(&opaque_damage);
		} else {
			wld_copy_region(swc.backend->renderer, view->buffer,
			                buf_x - target_geom->x, buf_y - target_geom->y,
			                &buffer_damage);
		}
	}

	pixman_region32_fini(&view_damage);
	pixman_region32_fini(&buffer_damage);

	pixman_region32_t in_rect;
	pixman_region32_init_rect(&in_rect,
	                          geom->x - view->border.inwidth,
	                          geom->y - view->border.inwidth,
	                          geom->width + (2 * view->border.inwidth),
	                          geom->height + (2 * view->border.inwidth));

	pixman_region32_t out_border;
	pixman_region32_init(&out_border);
	pixman_region32_subtract(&out_border, &border_damage, &in_rect);

	pixman_region32_t in_border;
	pixman_region32_init(&in_border);
	pixman_region32_subtract(&in_border, &in_rect, &geom_region);
	pixman_region32_intersect(&in_border, &in_border, &border_damage);

	pixman_region32_fini(&geom_region);
	pixman_region32_fini(&buffer_region);
	pixman_region32_fini(&border_region);

	/* Draw border */
	if (view->border.outwidth > 0 && pixman_region32_not_empty(&out_border)) {
		pixman_region32_translate(&out_border, -target_geom->x,
		                          -target_geom->y);
		wld_fill_region(swc.backend->renderer, view->border.outcolor, &out_border);
	}

	if (view->border.inwidth > 0 && pixman_region32_not_empty(&in_border)) {
		pixman_region32_translate(&in_border, -target_geom->x, -target_geom->y);
		wld_fill_region(swc.backend->renderer, view->border.incolor, &in_border);
	}

	pixman_region32_fini(&border_damage);
	pixman_region32_fini(&in_rect);
	pixman_region32_fini(&out_border);
	pixman_region32_fini(&in_border);

	if ((view->decor.top || view->decor.right || view->decor.bottom ||
	     view->decor.left)) {
		decor_repaint(swc.backend->renderer, target_geom, view, damage);
	}
}

static void
draw_overlays(struct wld_renderer *renderer, const struct swc_rectangle *target_geom)
{
	int32_t tx = (int32_t)target_geom->width;
	int32_t ty = (int32_t)target_geom->height;

/* draw box as 4 rectangles with wld */
#define CLAMP_LOW(v, lo) ((v) < (lo) ? (lo) : (v))
#define CLAMP_HIGH(v, hi) ((v) > (hi) ? (hi) : (v))
#define DRAW_CLIPPED(rx, ry, rw, rh, clr)                                     \
	do {                                                                       \
		int32_t _x1 = CLAMP_LOW((rx), 0);                                      \
		int32_t _y1 = CLAMP_LOW((ry), 0);                                      \
		int32_t _x2 = CLAMP_HIGH((rx) + (int32_t)(rw), tx);                    \
		int32_t _y2 = CLAMP_HIGH((ry) + (int32_t)(rh), ty);                    \
		if (_x2 > _x1 && _y2 > _y1)                                            \
			wld_fill_rectangle(renderer, (clr), _x1, _y1,                      \
			                   (uint32_t)(_x2 - _x1), (uint32_t)(_y2 - _y1)); \
	} while (0)

	if (overlay.active && overlay.border_width > 0) {
		int32_t x = overlay.x - target_geom->x;
		int32_t y = overlay.y - target_geom->y;
		uint32_t w = overlay.width, h = overlay.height,
		         bw = overlay.border_width;

		if (w > 0 && h > 0) {
			if (bw > w) {
				bw = w;
			}
			if (bw > h) {
				bw = h;
			}

			DRAW_CLIPPED(x, y, (int32_t)w, (int32_t)bw, overlay.color);
			DRAW_CLIPPED(x, y + (int32_t)h - (int32_t)bw, (int32_t)w,
			             (int32_t)bw, overlay.color);
			DRAW_CLIPPED(x, y, (int32_t)bw, (int32_t)h, overlay.color);
			DRAW_CLIPPED(x + (int32_t)w - (int32_t)bw, y, (int32_t)bw,
			             (int32_t)h, overlay.color);
		}
	}

#undef DRAW_CLIPPED
#undef CLAMP_HIGH
#undef CLAMP_LOW
}

static void
renderer_repaint(struct target *target, pixman_region32_t *damage,
                 pixman_region32_t *base_damage, struct wl_list *views,
                 struct screen *screen)
{
	struct compositor_view *view;
	const struct swc_rectangle *target_geom = &target->view->geometry;

	DEBUG("Rendering to target { x: %d, y: %d, w: %u, h: %u }\n",
	      target->view->geometry.x, target->view->geometry.y,
	      target->view->geometry.width, target->view->geometry.height);

	wld_set_target_surface(swc.backend->renderer, target->surface);

	if (pixman_region32_not_empty(base_damage)) {
		pixman_region32_translate(base_damage, -target->view->geometry.x,
		                          -target->view->geometry.y);
		wld_fill_region(swc.backend->renderer, DEFAULT_BG,
		                base_damage);
	}

	wl_list_for_each_reverse(view, views, link)
	{
		if (view->visible && view->base.screens & target->mask) {
			repaint_view(target, view, damage);
		}
	}

	draw_overlays(swc.backend->renderer, target_geom);

	wld_flush(swc.backend->renderer);
}

static int
renderer_attach(struct compositor_view *view, struct wld_buffer *client_buffer)
{
	struct wld_buffer *buffer;
	uint32_t proxy_width, proxy_height;
	bool was_proxy = view->buffer != view->base.buffer;
	bool needs_proxy =
	    client_buffer && !(wld_capabilities(swc.backend->renderer, client_buffer) &
	                       WLD_CAPABILITY_READ);
	bool proxy_incompatible =
	    view->buffer && client_buffer &&
	    (view->buffer->format != client_buffer->format ||
	     view->buffer->width < client_buffer->width ||
	     view->buffer->height < client_buffer->height);

	if (client_buffer) {
		/* Create a proxy buffer if necessary (for example a hardware buffer
		 * backing a SHM buffer). */
		if (needs_proxy) {
			if (!was_proxy || proxy_incompatible) {
				DEBUG("Creating a proxy buffer\n");
				proxy_width = client_buffer->width;
				proxy_height = client_buffer->height;
				if (proxy_width <= UINT32_MAX - 255) {
					proxy_width = (proxy_width + 255) & ~255U;
				}
				if (proxy_height <= UINT32_MAX - 255) {
					proxy_height = (proxy_height + 255) & ~255U;
				}
				buffer = wld_create_buffer(
				    swc.backend->context, proxy_width, proxy_height,
				    client_buffer->format, WLD_FLAG_MAP);

				if (!buffer) {
					return -ENOMEM;
				}
			} else {
				/* Otherwise we can keep the original proxy buffer. */
				buffer = view->buffer;
			}
		} else {
			buffer = client_buffer;
		}
	} else {
		buffer = NULL;
	}

	/* If we no longer need a proxy buffer, or the original buffer is of a
	 * different size, destroy the old proxy image. */
	if (view->buffer &&
	    ((!needs_proxy && was_proxy) ||
	     (needs_proxy && was_proxy && proxy_incompatible))) {
		wld_buffer_unreference(view->buffer);
	}

	view->buffer = buffer;
	view->buffer_opaque_valid = false;

	return 0;
}

static void
renderer_flush_view(struct compositor_view *view)
{
	if (view->buffer == view->base.buffer) {
		return;
	}

	wld_set_target_buffer(swc.shm->renderer, view->buffer);
	wld_copy_region(swc.shm->renderer, view->base.buffer, 0, 0,
	                &view->surface->state.damage);
	wld_flush(swc.shm->renderer);
}

/* }}} */

/* Surface Views {{{ */

/**
 * Adds the region below a view to the compositor's damaged region.
 */
static void
damage_below_view(struct compositor_view *view)
{
	pixman_region32_t damage_below;

	pixman_region32_init_with_extents(&damage_below, &view->extents);
	pixman_region32_union(&compositor.damage, &compositor.damage,
	                      &damage_below);
	pixman_region32_fini(&damage_below);
}

/**
 * Completely damages the surface and its border.
 */
static void
damage_view(struct compositor_view *view)
{
	damage_below_view(view);
	view->border.damaged_border1 = true;
	view->border.damaged_border2 = true;
}

static void
update_extents(struct compositor_view *view)
{
	int64_t total_border =
	    (int64_t)view->border.outwidth + (int64_t)view->border.inwidth;
	int64_t geom_x = view->base.geometry.x;
	int64_t geom_y = view->base.geometry.y;
	int64_t geom_w = view->base.geometry.width;
	int64_t geom_h = view->base.geometry.height;

	int64_t border_x1 = geom_x - total_border;
	int64_t border_y1 = geom_y - total_border;
	int64_t border_x2 = geom_x + geom_w + total_border;
	int64_t border_y2 = geom_y + geom_h + total_border;
	int64_t decor_x1 = geom_x - (int64_t)view->decor.left;
	int64_t decor_y1 = geom_y - (int64_t)view->decor.top;
	int64_t decor_x2 = geom_x + geom_w + (int64_t)view->decor.right;
	int64_t decor_y2 = geom_y + geom_h + (int64_t)view->decor.bottom;

	int64_t buffer_x1 = geom_x - view->buffer_offset_x;
	int64_t buffer_y1 = geom_y - view->buffer_offset_y;
	int64_t buffer_x2 =
	    buffer_x1 +
	    (view->base.buffer ? view->base.buffer->width : (uint32_t)geom_w);
	int64_t buffer_y2 =
	    buffer_y1 +
	    (view->base.buffer ? view->base.buffer->height : (uint32_t)geom_h);

	view->extents.x1 = clamp_i32(MIN(MIN(border_x1, decor_x1), buffer_x1));
	view->extents.y1 = clamp_i32(MIN(MIN(border_y1, decor_y1), buffer_y1));
	view->extents.x2 = clamp_i32(MAX(MAX(border_x2, decor_x2), buffer_x2));
	view->extents.y2 = clamp_i32(MAX(MAX(border_y2, decor_y2), buffer_y2));

	if (view->extents.x2 < view->extents.x1) {
		view->extents.x2 = view->extents.x1;
	}
	if (view->extents.y2 < view->extents.y1) {
		view->extents.y2 = view->extents.y1;
	}

	/* Damage border. */
	view->border.damaged_border1 = true;
	view->border.damaged_border2 = true;
	view->decor.damaged = true;
}

static void
schedule_updates(uint32_t screens)
{
	if (compositor.scheduled_updates == 0) {
		wl_event_loop_add_idle(swc.event_loop, &perform_update, NULL);
	}

	if (screens == -1) {
		struct screen *screen;

		screens = 0;
		wl_list_for_each(screen, &swc.screens, link) screens |=
		    screen_mask(screen);
	}

	/* when zoomed, force full screen damage since actual area differs from
	 * world coords */
	if (compositor.zoom != 1.0f) {
		struct screen *screen;
		wl_list_for_each(screen, &swc.screens, link)
		{
			pixman_region32_union_rect(
			    &compositor.damage, &compositor.damage, screen->base.geometry.x,
			    screen->base.geometry.y, screen->base.geometry.width,
			    screen->base.geometry.height);
			screens |= screen_mask(screen);
		}
	}

	compositor.scheduled_updates |= screens;
}

void
compositor_damage_all(void)
{
	struct screen *screen;

	if (!compositor.initialized) {
		return;
	}

	wl_list_for_each(screen, &swc.screens, link)
	{
		pixman_region32_union_rect(
		    &compositor.damage, &compositor.damage, screen->base.geometry.x,
		    screen->base.geometry.y, screen->base.geometry.width,
		    screen->base.geometry.height);
	}

	schedule_updates(-1);
}

static void
overlay_damage_region(int32_t x, int32_t y, uint32_t width, uint32_t height,
                      uint32_t border_width)
{
	(void)border_width;
	pixman_region32_union_rect(&compositor.damage, &compositor.damage, x, y,
	                           width, height);
}

EXPORT void
swc_overlay_set_box(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                    uint32_t color, uint32_t border_width)
{
	int32_t x = x1 < x2 ? x1 : x2;
	int32_t y = y1 < y2 ? y1 : y2;
	uint32_t width = (uint32_t)abs(x2 - x1);
	uint32_t height = (uint32_t)abs(y2 - y1);

	if (border_width == 0) {
		border_width = 1;
	}

	if (overlay.active) {
		overlay_damage_region(overlay.x, overlay.y, overlay.width,
		                      overlay.height, overlay.border_width);
	}

	overlay.active = true;
	overlay.x = x;
	overlay.y = y;
	overlay.width = width;
	overlay.height = height;
	overlay.color = color;
	overlay.border_width = border_width;

	overlay_damage_region(overlay.x, overlay.y, overlay.width, overlay.height,
	                      overlay.border_width);
	schedule_updates(-1);
}

EXPORT void
swc_overlay_clear(void)
{
	if (!overlay.active) {
		return;
	}

	overlay_damage_region(overlay.x, overlay.y, overlay.width, overlay.height,
	                      overlay.border_width);
	overlay.active = false;
	schedule_updates(-1);
}

EXPORT void
swc_set_zoom(float level)
{
	if (level < 0.1f) {
		level = 0.1f;
	}
	if (level > 10.0f) {
		level = 10.0f;
	}

	if (compositor.zoom != level) {
		compositor.zoom = level;
		if (level == 1.0f) {
			if (compositor.zoom_buffer) {
				wld_buffer_unreference(compositor.zoom_buffer);
				compositor.zoom_buffer = NULL;
			}
		}
		/* damage entire screen to force full repaint */
		schedule_updates(-1);
	}
}

EXPORT float
swc_get_zoom(void)
{
	return compositor.zoom;
}

static pixman_format_code_t
wld_to_pixman_format(enum wld_format format)
{
	switch (format) {
	case WLD_FORMAT_XRGB8888:
		return PIXMAN_x8r8g8b8;
	case WLD_FORMAT_ARGB8888:
		return PIXMAN_a8r8g8b8;
	default:
		return PIXMAN_x8r8g8b8;
	}
}

static struct wld_buffer *
zoom_buffer_for_screen(struct screen *screen)
{
	uint32_t width = screen->base.geometry.width;
	uint32_t height = screen->base.geometry.height;

	if (compositor.zoom_buffer &&
	    (compositor.zoom_buffer->width != width ||
	     compositor.zoom_buffer->height != height ||
	     compositor.zoom_buffer->format != WLD_FORMAT_ARGB8888)) {
		wld_buffer_unreference(compositor.zoom_buffer);
		compositor.zoom_buffer = NULL;
	}

	if (!compositor.zoom_buffer) {
		compositor.zoom_buffer =
		    wld_create_buffer(swc.shm->context, width, height,
		                      WLD_FORMAT_ARGB8888, WLD_FLAG_MAP);
		if (!compositor.zoom_buffer) {
			return NULL;
		}
	}

	wld_buffer_reference(compositor.zoom_buffer);
	return compositor.zoom_buffer;
}

/* render zoomed view to shm      wallpaper unscaled, windows scaled */
static struct wld_buffer *
render_zoomed_to_shm(struct screen *screen, float zoom)
{
	uint32_t width = screen->base.geometry.width;
	uint32_t height = screen->base.geometry.height;
	int32_t screen_x = screen->base.geometry.x;
	int32_t screen_y = screen->base.geometry.y;
	int32_t cx = screen_x + width / 2;
	int32_t cy = screen_y + height / 2;
	struct compositor_view *view;
	struct wld_buffer *buffer = zoom_buffer_for_screen(screen);
	if (!buffer) {
		return NULL;
	}

	if (!wld_set_target_buffer(swc.shm->renderer, buffer)) {
		wld_buffer_unreference(buffer);
		return NULL;
	}

	pixman_region32_t full;
	pixman_region32_init_rect(&full, 0, 0, width, height);
	wld_fill_region(swc.shm->renderer, DEFAULT_BG, &full);
	pixman_region32_fini(&full);
	wld_flush(swc.shm->renderer);

	if (!wld_map(buffer)) {
		wld_buffer_unreference(buffer);
		return NULL;
	}

	pixman_image_t *dst_img = pixman_image_create_bits(
	    wld_to_pixman_format(buffer->format), buffer->width, buffer->height,
	    buffer->map, buffer->pitch);

	if (!dst_img) {
		wld_unmap(buffer);
		wld_buffer_unreference(buffer);
		return NULL;
	}

	/* render each view with scaling */
	wl_list_for_each_reverse(view, &compositor.views, link)
	{
		struct wld_buffer *src = view->buffer;
		const struct swc_rectangle *geom = &view->base.geometry;

		if (!src) {
			continue;
		}

		if (!(wld_capabilities(swc.shm->renderer, src) & WLD_CAPABILITY_READ)) {
			src = view->base.buffer;
		}
		if (!src) {
			continue;
		}

		/* maths     zoom position and size */
		float zoomed_x, zoomed_y, zoomed_w, zoomed_h;
		float border_out, border_in, total_border;

		if (view->always_top) {
			zoomed_x = geom->x - screen_x;
			zoomed_y = geom->y - screen_y;
			zoomed_w = geom->width;
			zoomed_h = geom->height;

			border_out = view->border.outwidth;
			border_in = view->border.inwidth;
		} else {
			zoomed_x = (geom->x - cx) * zoom + width / 2.0f;
			zoomed_y = (geom->y - cy) * zoom + height / 2.0f;
			zoomed_w = geom->width * zoom;
			zoomed_h = geom->height * zoom;

			border_out = view->border.outwidth * zoom;
			border_in = view->border.inwidth * zoom;
		}

		total_border = border_out + border_in;

		if (zoomed_x + zoomed_w + total_border < 0 ||
		    zoomed_x - total_border >= (int32_t)width ||
		    zoomed_y + zoomed_h + total_border < 0 ||
		    zoomed_y - total_border >= (int32_t)height) {
			continue;
		}

		if (view->border.outwidth > 0 && border_out >= 1) {
			int32_t bx = (int32_t)(zoomed_x - total_border);
			int32_t by = (int32_t)(zoomed_y - total_border);
			int32_t bw = (int32_t)(zoomed_w + 2 * total_border);
			int32_t bh = (int32_t)(zoomed_h + 2 * total_border);
			int32_t bo = (int32_t)border_out;

			pixman_color_t color = {
			    .red = ((view->border.outcolor >> 16) & 0xff) * 257,
			    .green = ((view->border.outcolor >> 8) & 0xff) * 257,
			    .blue = (view->border.outcolor & 0xff) * 257,
			    .alpha = 0xffff};
			pixman_image_t *fill = pixman_image_create_solid_fill(&color);
			if (fill) {
				pixman_image_composite32(PIXMAN_OP_OVER, fill, NULL, dst_img, 0,
				                         0, 0, 0, bx, by, bw, bo);
				pixman_image_composite32(PIXMAN_OP_OVER, fill, NULL, dst_img, 0,
				                         0, 0, 0, bx, by + bh - bo, bw, bo);
				pixman_image_composite32(PIXMAN_OP_OVER, fill, NULL, dst_img, 0,
				                         0, 0, 0, bx, by + bo, bo, bh - 2 * bo);
				pixman_image_composite32(PIXMAN_OP_OVER, fill, NULL, dst_img, 0,
				                         0, 0, 0, bx + bw - bo, by + bo, bo,
				                         bh - 2 * bo);
				pixman_image_unref(fill);
			}
		}

		if (view->border.inwidth > 0 && border_in >= 1) {
			int32_t bx = (int32_t)(zoomed_x - border_in);
			int32_t by = (int32_t)(zoomed_y - border_in);
			int32_t bw = (int32_t)(zoomed_w + 2 * border_in);
			int32_t bh = (int32_t)(zoomed_h + 2 * border_in);
			int32_t bi = (int32_t)border_in;

			pixman_color_t color = {
			    .red = ((view->border.incolor >> 16) & 0xff) * 257,
			    .green = ((view->border.incolor >> 8) & 0xff) * 257,
			    .blue = (view->border.incolor & 0xff) * 257,
			    .alpha = 0xffff};
			pixman_image_t *fill = pixman_image_create_solid_fill(&color);
			if (fill) {
				pixman_image_composite32(PIXMAN_OP_OVER, fill, NULL, dst_img, 0,
				                         0, 0, 0, bx, by, bw, bi);
				pixman_image_composite32(PIXMAN_OP_OVER, fill, NULL, dst_img, 0,
				                         0, 0, 0, bx, by + bh - bi, bw, bi);
				pixman_image_composite32(PIXMAN_OP_OVER, fill, NULL, dst_img, 0,
				                         0, 0, 0, bx, by + bi, bi, bh - 2 * bi);
				pixman_image_composite32(PIXMAN_OP_OVER, fill, NULL, dst_img, 0,
				                         0, 0, 0, bx + bw - bi, by + bi, bi,
				                         bh - 2 * bi);
				pixman_image_unref(fill);
			}
		}

		if (!wld_map(src)) {
			continue;
		}

		pixman_image_t *src_img = pixman_image_create_bits(
		    wld_to_pixman_format(src->format), src->width, src->height,
		    src->map, src->pitch);

		if (src_img) {
			if (!view->always_top) {
				pixman_transform_t transform;
				pixman_transform_init_identity(&transform);
				pixman_fixed_t scale = pixman_double_to_fixed(1.0 / zoom);
				pixman_transform_scale(&transform, NULL, scale, scale);
				pixman_image_set_transform(src_img, &transform);
				pixman_image_set_filter(src_img, PIXMAN_FILTER_BILINEAR, NULL,
				                        0);
			}

			pixman_image_composite32(PIXMAN_OP_OVER, src_img, NULL, dst_img, 0,
			                         0, 0, 0, (int32_t)zoomed_x,
			                         (int32_t)zoomed_y, (int32_t)(zoomed_w + 1),
			                         (int32_t)(zoomed_h + 1));

			pixman_image_unref(src_img);
		}

		wld_unmap(src);
	}

	pixman_image_unref(dst_img);
	wld_unmap(buffer);

	return buffer;
}

static bool
update(struct view *base)
{
	struct compositor_view *view = (void *)base;

	if (!swc.active || !view->visible) {
		return false;
	}

	schedule_updates(view->base.screens);

	return true;
}

static int
attach(struct view *base, struct wld_buffer *buffer)
{
	struct compositor_view *view = (void *)base;
	struct surface *surface = view->surface;
	pixman_box32_t old_extents;
	pixman_region32_t old, new, both;
	uint32_t new_width = buffer ? buffer->width : 0;
	uint32_t new_height = buffer ? buffer->height : 0;
	int ret;

	if ((ret = renderer_attach(view, buffer)) < 0) {
		return ret;
	}

	/* Schedule updates on the screens the view was previously
	 * visible on. */
	update(&view->base);

	view->buffer_offset_x = 0;
	view->buffer_offset_y = 0;
	if (surface && surface->has_window_geometry && buffer) {
		if (surface->window_width > 0 && surface->window_height > 0) {
			new_width = (uint32_t)surface->window_width;
			new_height = (uint32_t)surface->window_height;
			view->buffer_offset_x = surface->window_x;
			view->buffer_offset_y = surface->window_y;
		}
	}

	if (view_set_size(&view->base, new_width, new_height)) {
		/* The view was resized. */
		old_extents = view->extents;
		update_extents(view);

		if (view->visible) {
			/* Damage the region that was newly uncovered or covered. */
			pixman_region32_init_with_extents(&old, &old_extents);
			pixman_region32_init_with_extents(&new, &view->extents);
			pixman_region32_init(&both);
			pixman_region32_intersect(&both, &old, &new);
			pixman_region32_union(&new, &old, &new);
			pixman_region32_subtract(&new, &new, &both);
			pixman_region32_union(&compositor.damage, &compositor.damage, &new);
			pixman_region32_fini(&old);
			pixman_region32_fini(&new);
			pixman_region32_fini(&both);

			view_update_screens(&view->base);
			update(&view->base);
		}
	}

	return 0;
}

static bool
move(struct view *base, int32_t x, int32_t y)
{
	struct compositor_view *view = (void *)base;

	if (view->visible) {
		damage_below_view(view);
		update(&view->base);
	}

	if (view_set_position(&view->base, x, y)) {
		update_extents(view);

		if (view->visible) {
			/* Assume worst-case no clipping until we draw the next frame (in
			 * case the surface gets moved again before that). */
			pixman_region32_clear(&view->clip);

			view_update_screens(&view->base);
			damage_below_view(view);
			update(&view->base);
		}
	}

	return true;
}

static const struct view_impl view_impl = {
    .update = update,
    .attach = attach,
    .move = move,
};

static void
restack_view_for_layer(struct compositor_view *view, bool raise)
{
	struct compositor_view *other;
	struct wl_list *insert_after = &compositor.views;
	bool found_same = false;

	wl_list_for_each(other, &compositor.views, link)
	{
		if (other == view) {
			continue;
		}

		if (other->stack_layer > view->stack_layer) {
			insert_after = &other->link;
			continue;
		}

		if (other->stack_layer == view->stack_layer) {
			found_same = true;
			if (raise) {
				insert_after = other->link.prev;
			} else {
				insert_after = &other->link;
			}
			break;
		}

		insert_after = other->link.prev;
		break;
	}

	if (!found_same && !raise && insert_after == &compositor.views) {
		insert_after = compositor.views.prev;
		if (insert_after == &view->link) {
			insert_after = insert_after->prev;
		}
	}

	if (insert_after == &view->link) {
		insert_after = insert_after->prev;
	}

	wl_list_remove(&view->link);
	wl_list_insert(insert_after, &view->link);
}

static struct compositor_view *
view_at(int32_t x, int32_t y)
{
	struct compositor_view *view;
	struct swc_rectangle *geom;
	struct swc_rectangle buffer_geom;

	wl_list_for_each(view, &compositor.views, link)
	{
		if (!view->visible) {
			continue;
		}

		geom = &view->base.geometry;
		if (view->window) {
			if (!rectangle_contains_point(geom, x, y)) {
				continue;
			}
		} else if (view->base.buffer) {
			buffer_geom.x = geom->x - view->buffer_offset_x;
			buffer_geom.y = geom->y - view->buffer_offset_y;
			buffer_geom.width = view->base.buffer->width;
			buffer_geom.height = view->base.buffer->height;
			if (!rectangle_contains_point(&buffer_geom, x, y)) {
				continue;
			}
		} else if (!rectangle_contains_point(geom, x, y)) {
			continue;
		}

		if (pixman_region32_contains_point(&view->surface->state.input,
		                                   x - geom->x + view->buffer_offset_x,
		                                   y - geom->y + view->buffer_offset_y,
		                                   NULL)) {
			return view;
		}
	}

	return NULL;
}

static struct compositor_view *
window_view(struct compositor_view *view)
{
	while (view && !view->window && view->parent && view->parent != view) {
		view = view->parent;
	}
	return (view && view->window) ? view : NULL;
}

void
raise_window(struct compositor_view *view)
{
	struct compositor_view *other, *top_window;
	struct wl_list *insert_after;
	uint32_t screens;

	view = window_view(view);
	if (!view || !view->visible) {
		return;
	}

	top_window = NULL;
	insert_after = &compositor.views;
	wl_list_for_each(other, &compositor.views, link)
	{
		if (other == view) {
			continue;
		}

		if (!other->visible) {
			continue;
		}

		if (other->stack_layer > STACK_LAYER_NORMAL ||
		    other->always_top) {
			insert_after = &other->link;
			continue;
		}

		if (other->stack_layer < STACK_LAYER_NORMAL) {
			break;
		}

		if (other->window) {
			top_window = other;
			break;
		}
		insert_after = &other->link;
	}

	if (view == top_window) {
		return;
	}

	screens = view->base.screens;

	wl_list_remove(&view->link);
	wl_list_insert(insert_after, &view->link);

	view->border.damaged_border1 = true;
	pixman_region32_union_rect(&compositor.damage, &compositor.damage,
	                           view->extents.x1, view->extents.y1,
	                           span_u32(view->extents.x1, view->extents.x2),
	                           span_u32(view->extents.y1, view->extents.y2));
	schedule_updates(screens);
}

void
raise_window_top(struct compositor_view *view)
{
	view->stack_layer = STACK_LAYER_OVERLAY;
	restack_view_for_layer(view, true);
	damage_view(view);
	schedule_updates(view->base.screens);
}

void
compositor_view_set_stack_layer(struct compositor_view *view, uint32_t layer,
	                            bool raise)
{
	if (view->stack_layer == layer) {
		if (raise) {
			restack_view_for_layer(view, true);
			damage_view(view);
			schedule_updates(view->base.screens);
		}
		return;
	}

	damage_view(view);
	view->stack_layer = layer;
	restack_view_for_layer(view, raise);
	damage_view(view);
	schedule_updates(view->base.screens);
}

EXPORT struct swc_window *
swc_window_at(int32_t x, int32_t y)
{
	struct compositor_view *view = window_view(view_at(x, y));

	return view ? &view->window->base : NULL;
}

static struct compositor_view *
view_for_window(struct swc_window *base)
{
	struct window *window;

	if (!base) {
		return NULL;
	}

	window = (struct window *)base;
	return window->view;
}

static struct compositor_view *
prev_window_view(struct compositor_view *view)
{
	struct wl_list *link;
	struct compositor_view *other;

	for (link = view->link.prev; link != &compositor.views; link = link->prev) {
		other = wl_container_of(link, other, link);

		if (other->visible && other->window) {
			return other;
		}
	}

	return NULL;
}

static struct compositor_view *
next_window_view(struct compositor_view *view)
{
	struct wl_list *link;
	struct compositor_view *other;

	for (link = view->link.next; link != &compositor.views; link = link->next) {
		other = wl_container_of(link, other, link);

		if (other->visible && other->window) {
			return other;
		}
	}

	return NULL;
}

static void
damage_views(struct compositor_view *a, struct compositor_view *b)
{
	uint32_t screens = a->base.screens | (b ? b->base.screens : 0);

	a->border.damaged_border1 = true;
	a->border.damaged_border2 = true;
	pixman_region32_union_rect(&compositor.damage, &compositor.damage,
	                           a->extents.x1, a->extents.y1,
	                           span_u32(a->extents.x1, a->extents.x2),
	                           span_u32(a->extents.y1, a->extents.y2));

	if (b) {
		b->border.damaged_border1 = true;
		b->border.damaged_border2 = true;
		pixman_region32_union_rect(&compositor.damage, &compositor.damage,
		                           b->extents.x1, b->extents.y1,
		                           span_u32(b->extents.x1, b->extents.x2),
		                           span_u32(b->extents.y1, b->extents.y2));
	}

	schedule_updates(screens);
}

EXPORT void
swc_window_stack(struct swc_window *window, int32_t direction)
{
	struct compositor_view *view = view_for_window(window);
	struct compositor_view *other = NULL;

	if (!view || !view->visible || direction == 0) {
		return;
	}

	if (direction < 0) {
		other = prev_window_view(view);
		if (!other) {
			return;
		}
		wl_list_remove(&view->link);
		wl_list_insert(other->link.prev, &view->link);
	} else {
		other = next_window_view(view);
		if (!other) {
			return;
		}
		wl_list_remove(&view->link);
		wl_list_insert(&other->link, &view->link);
	}

	damage_views(view, other);
}

struct compositor_view *
compositor_create_view(struct surface *surface)
{
	struct compositor_view *view;

	view = malloc(sizeof(*view));

	if (!view) {
		return NULL;
	}

	view_initialize(&view->base, &view_impl);
	view->surface = surface;
	view->buffer = NULL;
	view->buffer_opaque_valid = false;
	view->buffer_opaque = false;
	view->window = NULL;
	view->parent = NULL;
	view->buffer_offset_x = 0;
	view->buffer_offset_y = 0;
	view->visible = false;
	view->always_top = false;
	view->stack_layer = STACK_LAYER_NORMAL;
	view->extents.x1 = 0;
	view->extents.y1 = 0;
	view->extents.x2 = 0;
	view->extents.y2 = 0;
	view->border.outwidth = 0;
	view->border.outcolor = 0x000000;
	view->border.damaged_border1 = false;
	view->border.inwidth = 0;
	view->border.incolor = 0x000000;
	view->border.damaged_border2 = false;
	view->decor.color = 0x000000;
	view->decor.top = 0;
	view->decor.right = 0;
	view->decor.bottom = 0;
	view->decor.left = 0;
	decor_view_initialize(view);
	view->decor.damaged = false;
	pixman_region32_init(&view->clip);
	wl_signal_init(&view->destroy_signal);
	surface_set_view(surface, &view->base);
	wl_list_insert(&compositor.views, &view->link);

	return view;
}

void
compositor_view_destroy(struct compositor_view *view)
{
	wl_signal_emit(&view->destroy_signal, NULL);
	compositor_view_hide(view);
	surface_set_view(view->surface, NULL);
	view_finalize(&view->base);
	decor_view_finalize(view);
	pixman_region32_fini(&view->clip);
	wl_list_remove(&view->link);
	free(view);
}

struct compositor_view *
compositor_view(struct view *view)
{
	return view->impl == &view_impl ? (struct compositor_view *)view : NULL;
}

void
compositor_view_set_parent(struct compositor_view *view,
                           struct compositor_view *parent)
{
	view->parent = parent;

	if (!parent) {
		return;
	}

	if (parent->visible) {
		compositor_view_show(view);
	} else {
		compositor_view_hide(view);
	}
}

void
compositor_view_restack(struct compositor_view *view,
                        struct compositor_view *sibling, bool above)
{
	if (!view || !sibling || view == sibling) {
		return;
	}

	if (above) {
		if (view->link.next == &sibling->link) {
			return;
		}
		wl_list_remove(&view->link);
		wl_list_insert(sibling->link.prev, &view->link);
	} else {
		if (view->link.prev == &sibling->link) {
			return;
		}
		wl_list_remove(&view->link);
		wl_list_insert(&sibling->link, &view->link);
	}

	damage_views(view, sibling);
}

void
compositor_view_show(struct compositor_view *view)
{
	struct compositor_view *other;
	struct subsurface *subsurface;

	if (view->visible) {
		return;
	}

	subsurface = view->surface ? view->surface->subsurface : NULL;
	if (subsurface) {
		if (!subsurface->added || !view->surface->state.buffer) {
			return;
		}
	}

	view->visible = true;
	view_update_screens(&view->base);

	if (view->window) {
		raise_window(view);
	}

	/* Assume worst-case no clipping until we draw the next frame (in case the
	 * surface gets moved before that. */
	pixman_region32_clear(&view->clip);
	damage_view(view);
	update(&view->base);

	wl_list_for_each(other, &compositor.views, link)
	{
		if (other->parent == view) {
			compositor_view_show(other);
		}
	}
}

void
compositor_view_hide(struct compositor_view *view)
{
	struct compositor_view *other;

	if (!view->visible) {
		return;
	}

	/* Update all the screens the view was on. */
	update(&view->base);
	damage_below_view(view);

	view_set_screens(&view->base, 0);
	view->visible = false;

	wl_list_for_each(other, &compositor.views, link)
	{
		if (other->parent == view) {
			compositor_view_hide(other);
		}
	}
}

void
compositor_view_set_border_width(struct compositor_view *view,
                                 uint32_t outwidth, uint32_t inwidth)
{
	if (view->border.outwidth == outwidth && view->border.inwidth == inwidth) {
		return;
	}

	view->border.outwidth = outwidth;
	view->border.damaged_border1 = true;

	view->border.inwidth = inwidth;
	view->border.damaged_border2 = true;

	/* XXX: Damage above surface for transparent surfaces? */

	update_extents(view);
	update(&view->base);
}

void
compositor_view_set_border_color(struct compositor_view *view,
                                 uint32_t outcolor, uint32_t incolor)
{
	if (view->border.outcolor == outcolor && view->border.incolor == incolor) {
		return;
	}

	view->border.outcolor = outcolor;
	view->border.damaged_border1 = true;

	view->border.incolor = incolor;
	view->border.damaged_border2 = true;

	/* XXX: Damage above surface for transparent surfaces? */

	update(&view->base);
}

void
compositor_view_set_decor(struct compositor_view *view,
                            const struct swc_decor *decor)
{
	/* decor_view_set() could shrink or remove the decoration, so wedamage the old
	 * extents before they are replaced */
	if (view->visible) {
		damage_below_view(view);
	}

	decor_view_set(view, decor);
	update_extents(view);
	update(&view->base);
}

void
compositor_view_damage_decor(struct compositor_view *view)
{
	if (!view->decor.top && !view->decor.right && !view->decor.bottom &&
	    !view->decor.left) {
		return;
	}

	decor_view_damage(view);
	update(&view->base);
}

/* }}} */

static void
calculate_damage(void)
{
	struct compositor_view *view;
	struct swc_rectangle *geom;
	pixman_region32_t surface_opaque, *surface_damage;

	pixman_region32_clear(&compositor.opaque);
	pixman_region32_init(&surface_opaque);

	/* Go through views top-down to calculate clipping regions. */
	wl_list_for_each(view, &compositor.views, link)
	{
		if (!view->visible) {
			continue;
		}

		geom = &view->base.geometry;
		pixman_region32_t view_region;

		pixman_region32_init_rect(&view_region, geom->x, geom->y, geom->width,
		                          geom->height);

		/* Clip the surface by the opaque region covering it. */
		pixman_region32_copy(&view->clip, &compositor.opaque);

		/* Translate the opaque region to global coordinates. */
		pixman_region32_copy(&surface_opaque, &view->surface->state.opaque);
		pixman_region32_translate(&surface_opaque,
		                          geom->x - view->buffer_offset_x,
		                          geom->y - view->buffer_offset_y);
		pixman_region32_intersect(&surface_opaque, &surface_opaque,
		                          &view_region);

		/* Add the surface's opaque region to the accumulated opaque region. */
		pixman_region32_union(&compositor.opaque, &compositor.opaque,
		                      &surface_opaque);

		surface_damage = &view->surface->state.damage;

		if (pixman_region32_not_empty(surface_damage)) {
			view->buffer_opaque_valid = false;
			renderer_flush_view(view);

			/* Translate surface damage to global coordinates. */
			pixman_region32_translate(surface_damage,
			                          geom->x - view->buffer_offset_x,
			                          geom->y - view->buffer_offset_y);

			/* Add the surface damage to the compositor damage. */
			pixman_region32_union(&compositor.damage, &compositor.damage,
			                      surface_damage);
			pixman_region32_clear(surface_damage);
		}

		/* redraw entire thingy if border or decor changed */
		if (view->border.damaged_border1 || view->border.damaged_border2 ||
		    view->decor.damaged) {
			pixman_region32_t border_region;

			pixman_region32_init_with_extents(&border_region, &view->extents);

			pixman_region32_subtract(&border_region, &border_region,
			                         &view_region);

			pixman_region32_union(&compositor.damage, &compositor.damage,
			                      &border_region);

			pixman_region32_fini(&border_region);

			view->border.damaged_border1 = false;
			view->border.damaged_border2 = false;
			view->decor.damaged = false;
		}

		pixman_region32_fini(&view_region);
	}

	pixman_region32_fini(&surface_opaque);
}

static void
update_screen(struct screen *screen)
{
	struct target *target;
	const struct swc_rectangle *geom = &screen->base.geometry;
	pixman_region32_t damage, *total_damage;

	if (!(compositor.scheduled_updates & screen_mask(screen))) {
		return;
	}

	if (!(target = target_get(screen))) {
		return;
	}

	pixman_region32_init(&damage);
	pixman_region32_intersect_rect(&damage, &compositor.damage, geom->x,
	                               geom->y, geom->width, geom->height);
	pixman_region32_translate(&damage, -geom->x, -geom->y);
	total_damage = wld_surface_damage(target->surface, &damage);

	/* Don't repaint the screen if it is waiting for a page flip. */
	if (compositor.pending_flips & screen_mask(screen)) {
		pixman_region32_fini(&damage);
		return;
	}

	/* check if zoom */
	if (compositor.zoom != 1.0f) {
		pixman_region32_fini(&damage);

		struct wld_buffer *zoomed =
		    render_zoomed_to_shm(screen, compositor.zoom);
		if (!zoomed) {
			return;
		}

		pixman_region32_t full;
		pixman_region32_init_rect(&full, 0, 0, geom->width, geom->height);
		wld_set_target_surface(swc.backend->renderer, target->surface);
		wld_copy_region(swc.backend->renderer, zoomed, 0, 0, &full);
		wld_flush(swc.backend->renderer);
		pixman_region32_fini(&full);

		wld_buffer_unreference(zoomed);
	} else {
		pixman_region32_t base_damage;
		pixman_region32_copy(&damage, total_damage);
		pixman_region32_translate(&damage, geom->x, geom->y);
		pixman_region32_init(&base_damage);
		pixman_region32_subtract(&base_damage, &damage, &compositor.opaque);
		renderer_repaint(target, &damage, &base_damage, &compositor.views,
		                 screen);
		pixman_region32_fini(&damage);
		pixman_region32_fini(&base_damage);
	}

	switch (target_swap_buffers(target)) {
	case -EACCES:
		/* If we get an EACCES, it is because this session is being deactivated,
		 * but we haven't yet received the deactivate signal from swc-launch. */
		swc_deactivate();
		break;
	case 0:
		compositor.pending_flips |= screen_mask(screen);
		break;
	}
}

static void
perform_update(void *data)
{
	struct screen *screen;
	uint32_t updates = compositor.scheduled_updates & ~compositor.pending_flips;

	if (!swc.active || !updates) {
		return;
	}

	DEBUG("Performing update\n");

	compositor.updating = true;
	calculate_damage();

	wl_list_for_each(screen, &swc.screens, link) update_screen(screen);

	/* XXX: Should assert that all damage was covered by some output */
	pixman_region32_clear(&compositor.damage);
	compositor.scheduled_updates &= ~updates;
	compositor.updating = false;
}

bool
handle_motion(struct pointer_handler *handler, uint32_t time, wl_fixed_t fx,
              wl_fixed_t fy)
{
	int32_t x = wl_fixed_to_int(fx), y = wl_fixed_to_int(fy);

	/* If buttons are pressed, don't change pointer focus. */
	if (swc.seat->pointer->buttons.size > 0) {
		return false;
	}

	struct compositor_view *view = view_at(x, y);

	pointer_set_focus(swc.seat->pointer, view);

	return false;
}

static bool
handle_button(struct pointer_handler *handler, uint32_t time,
              struct button *button, uint32_t state)
{
	(void)handler;
	(void)time;
	(void)button;

	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return false;
	}

	int32_t x = wl_fixed_to_int(swc.seat->pointer->x);
	int32_t y = wl_fixed_to_int(swc.seat->pointer->y);
	struct compositor_view *view = view_at(x, y);

	pointer_set_focus(swc.seat->pointer, view);
	raise_window(view);

	return false;
}

static void
handle_terminate(void *data, uint32_t time, uint32_t value, uint32_t state)
{
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		wl_display_terminate(swc.display);
	}
}

static void
handle_switch_vt(void *data, uint32_t time, uint32_t value, uint32_t state)
{
	uint8_t vt = value - XKB_KEY_XF86Switch_VT_1 + 1;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		launch_activate_vt(vt);
	}
}

static void
handle_swc_event(struct wl_listener *listener, void *data)
{
	struct event *event = data;

	switch (event->type) {
	case SWC_EVENT_ACTIVATED:
		schedule_updates(-1);
		break;
	case SWC_EVENT_DEACTIVATED:
		compositor.scheduled_updates = 0;
		break;
	}
}

static void
create_surface(struct wl_client *client, struct wl_resource *resource,
               uint32_t id)
{
	struct surface *surface;

	/* Initialize surface. */
	surface = surface_new(client, wl_resource_get_version(resource), id);

	if (!surface) {
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_signal_emit(&swc_compositor.signal.new_surface, surface);
}

static void
create_region(struct wl_client *client, struct wl_resource *resource,
              uint32_t id)
{
	if (!region_new(client, wl_resource_get_version(resource), id)) {
		wl_resource_post_no_memory(resource);
	}
}

static const struct wl_compositor_interface compositor_impl = {
    .create_surface = create_surface,
    .create_region = create_region,
};

static void
bind_compositor(struct wl_client *client, void *data, uint32_t version,
                uint32_t id)
{
	struct wl_resource *resource;

	resource =
	    wl_resource_create(client, &wl_compositor_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &compositor_impl, NULL, NULL);
}

bool
compositor_initialize(void)
{
	struct screen *screen;
	uint32_t keysym;

	compositor.global = wl_global_create(swc.display, &wl_compositor_interface,
	                                     4, NULL, &bind_compositor);

	if (!compositor.global) {
		return false;
	}

	compositor.scheduled_updates = 0;
	compositor.pending_flips = 0;
	compositor.updating = false;
	compositor.zoom = 1.0f;
	compositor.zoom_buffer = NULL;
	if (!decor_initialize()) {
		return false;
	}
	pixman_region32_init(&compositor.damage);
	pixman_region32_init(&compositor.opaque);
	wl_list_init(&compositor.views);
	wl_signal_init(&swc_compositor.signal.new_surface);
	compositor.swc_listener.notify = &handle_swc_event;
	wl_signal_add(&swc.event_signal, &compositor.swc_listener);

	wl_list_for_each(screen, &swc.screens, link) target_new(screen);
	if (swc.active) {
		schedule_updates(-1);
	}

	swc_add_binding(SWC_BINDING_KEY, SWC_MOD_CTRL | SWC_MOD_ALT,
	                XKB_KEY_BackSpace, &handle_terminate, NULL);

	for (keysym = XKB_KEY_XF86Switch_VT_1; keysym <= XKB_KEY_XF86Switch_VT_12;
	     ++keysym) {
		swc_add_binding(SWC_BINDING_KEY, SWC_MOD_ANY, keysym, &handle_switch_vt,
		                NULL);
	}

	compositor.initialized = true;

	return true;
}

void
compositor_finalize(void)
{
	compositor.initialized = false;

	if (compositor.zoom_buffer) {
		wld_buffer_unreference(compositor.zoom_buffer);
		compositor.zoom_buffer = NULL;
	}
	decor_finalize();
	pixman_region32_fini(&compositor.damage);
	pixman_region32_fini(&compositor.opaque);
	wl_global_destroy(compositor.global);
}

struct wld_buffer *
compositor_get_buffer(struct screen *screen)
{
	struct target *target = target_get(screen);
	if (!target) {
		return NULL;
	}
	return target->current_buffer;
}

struct wld_buffer *
compositor_render_to_shm(struct screen *screen)
{
	if (compositor.zoom != 1.0f) {
		return render_zoomed_to_shm(screen, compositor.zoom);
	}

	uint32_t width = screen->base.geometry.width;
	uint32_t height = screen->base.geometry.height;
	struct wld_buffer *buffer;
	struct compositor_view *view;
	pixman_region32_t region;
	pixman_region32_t damage;
	uint32_t caps;

	/* create shm buf */
	buffer = wld_create_buffer(swc.shm->context, width, height,
	                           WLD_FORMAT_ARGB8888, WLD_FLAG_MAP);
	if (!buffer) {
		return NULL;
	}

	caps = wld_capabilities(swc.shm->renderer, buffer);
	if (!(caps & WLD_CAPABILITY_WRITE) ||
	    !wld_set_target_buffer(swc.shm->renderer, buffer)) {
		wld_buffer_unreference(buffer);
		return NULL;
	}

	/* set region */
	pixman_region32_init_rect(&region, 0, 0, width, height);
	pixman_region32_init_rect(&damage, screen->base.geometry.x,
	                          screen->base.geometry.y, width, height);

	/* background */
	wld_fill_region(swc.shm->renderer, DEFAULT_BG, &region);

	wl_list_for_each_reverse(view, &compositor.views, link)
	{
		struct wld_buffer *src = view->buffer;

		if (!view->visible) {
			continue;
		}

		if (src &&
		    !(wld_capabilities(swc.shm->renderer, src) & WLD_CAPABILITY_READ)) {
			src = view->base.buffer;
		}

		if (src &&
		    (wld_capabilities(swc.shm->renderer, src) & WLD_CAPABILITY_READ)) {
			const struct swc_rectangle *geom = &view->base.geometry;
			int32_t src_x = view->window ? view->buffer_offset_x : 0;
			int32_t src_y = view->window ? view->buffer_offset_y : 0;
			int32_t dst_x = geom->x - src_x - screen->base.geometry.x;
			int32_t dst_y = geom->y - src_y - screen->base.geometry.y;
			pixman_region32_t source_region;

			pixman_region32_init_rect(&source_region, src_x, src_y, geom->width,
			                          geom->height);
			pixman_region32_intersect_rect(&source_region, &source_region, 0, 0,
			                               src->width, src->height);
			if (src->format == WLD_FORMAT_ARGB8888) {
				wld_blend_region(swc.shm->renderer, src, dst_x, dst_y,
				                 &source_region);
			} else {
				wld_copy_region(swc.shm->renderer, src, dst_x, dst_y,
				                &source_region);
			}
			pixman_region32_fini(&source_region);
		}

		if ((view->border.outwidth > 0 || view->border.inwidth > 0) &&
		    view->base.buffer) {
			pixman_region32_t view_region, view_damage, border_damage;
			const struct swc_rectangle *geom = &view->base.geometry;
			const struct swc_rectangle *target_geom = &screen->base.geometry;

			pixman_region32_init_rect(&view_region, geom->x, geom->y,
			                          geom->width, geom->height);
			pixman_region32_init_with_extents(&view_damage, &view->extents);
			pixman_region32_init(&border_damage);

			pixman_region32_intersect(&view_damage, &view_damage, &damage);
			pixman_region32_subtract(&view_damage, &view_damage, &view->clip);
			pixman_region32_subtract(&border_damage, &view_damage,
			                         &view_region);

			pixman_region32_t in_rect;
			pixman_region32_init_rect(&in_rect,
			                          geom->x - view->border.inwidth,
			                          geom->y - view->border.inwidth,
			                          geom->width + (2 * view->border.inwidth),
			                          geom->height +
			                              (2 * view->border.inwidth));

			pixman_region32_t out_border;
			pixman_region32_init(&out_border);
			pixman_region32_subtract(&out_border, &border_damage, &in_rect);

			pixman_region32_t in_border;
			pixman_region32_init(&in_border);
			pixman_region32_subtract(&in_border, &in_rect, &view_region);
			pixman_region32_intersect(&in_border, &in_border, &border_damage);

			if (view->border.outwidth > 0 &&
			    pixman_region32_not_empty(&out_border)) {
				pixman_region32_translate(&out_border, -target_geom->x,
				                          -target_geom->y);
				wld_fill_region(swc.shm->renderer, view->border.outcolor,
				                &out_border);
			}

			if (view->border.inwidth > 0 &&
			    pixman_region32_not_empty(&in_border)) {
				pixman_region32_translate(&in_border, -target_geom->x,
				                          -target_geom->y);
				wld_fill_region(swc.shm->renderer, view->border.incolor,
				                &in_border);
			}

			pixman_region32_fini(&border_damage);
			pixman_region32_fini(&view_region);
			pixman_region32_fini(&view_damage);
			pixman_region32_fini(&in_rect);
			pixman_region32_fini(&out_border);
			pixman_region32_fini(&in_border);
		}

		if (view->decor.top || view->decor.right || view->decor.bottom ||
		    view->decor.left) {
			const struct swc_rectangle *target_geom = &screen->base.geometry;
			decor_repaint(swc.shm->renderer, target_geom, view, &damage);
		}
	}

	draw_overlays(swc.shm->renderer, &screen->base.geometry);

	wld_flush(swc.shm->renderer);
	pixman_region32_fini(&region);
	pixman_region32_fini(&damage);

	return buffer;
}
