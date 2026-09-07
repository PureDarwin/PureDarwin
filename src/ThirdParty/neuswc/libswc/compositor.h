/* swc: libswc/compositor.h
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

#ifndef SWC_COMPOSITOR_H
#define SWC_COMPOSITOR_H

#include "view.h"

#include <pixman.h>
#include <stdbool.h>
#include <wayland-server.h>

struct screen;
struct wld_buffer;
struct wld_font;

struct decor_part_buffer {
	void *data;
	struct wld_buffer *buffer;
	struct wld_buffer *tiled_buffer;
	uint32_t width, height, stride;
	uint32_t tiled_width, tiled_height;
	bool opaque;
};

struct swc_compositor {
	struct pointer_handler *const pointer_handler;
	struct {
		/**
		 * Emitted when a new surface is created.
		 *
		 * The data argument of the signal refers to the surface that has been
		 * created.
		 */
		struct wl_signal new_surface;
	} signal;
};

bool
compositor_initialize(void);
void
compositor_finalize(void);
void
compositor_damage_all(void);

struct compositor_view {
	struct view base;
	struct surface *surface;
	struct wld_buffer *buffer;
	bool buffer_opaque_valid;
	bool buffer_opaque;
	struct window *window;
	struct compositor_view *parent;
	int32_t buffer_offset_x;
	int32_t buffer_offset_y;

	/* Whether or not the view is visible (mapped). */
	bool visible;

	/* Whether or not to make it always be on top of other windows */
	bool always_top;

	/* Global stacking layer for this view */
	uint32_t stack_layer;

	/* The box that the surface covers (including it's border). */
	pixman_box32_t extents;

	/* The region that is covered by opaque regions of surfaces above this
	 * surface. */
	pixman_region32_t clip;

	struct {
		uint32_t outwidth;
		uint32_t outcolor;

		bool damaged_border1;

		/* sir, a second border has hit the compositor! */
		uint32_t inwidth;
		uint32_t incolor;

		bool damaged_border2;
	} border;

	struct {
		uint32_t color;
		uint32_t top, right, bottom, left;
		struct swc_decor_text text;
		const struct swc_decor_parts *parts_key;
		struct decor_part_buffer parts[8];
		char *string;
		char *font_name;
		struct wld_font *font;
		bool damaged;
	} decor;

	struct wl_list link;
	struct wl_signal destroy_signal;
};

struct compositor_view *
compositor_create_view(struct surface *surface);

void
compositor_view_destroy(struct compositor_view *view);

/**
 * Returns view as a compositor_view, or NULL if view is not a compositor_view.
 */
struct compositor_view *
compositor_view(struct view *view);

void
compositor_view_set_parent(struct compositor_view *view,
                           struct compositor_view *parent);
void
compositor_view_restack(struct compositor_view *view,
                        struct compositor_view *sibling, bool above);

void
compositor_view_show(struct compositor_view *view);
void
compositor_view_hide(struct compositor_view *view);

void
compositor_view_set_border_color(struct compositor_view *view,
                                 uint32_t outcolor, uint32_t incolor);
void
compositor_view_set_border_width(struct compositor_view *view,
                                 uint32_t outwidth, uint32_t inwidth);
void
compositor_view_set_decor(struct compositor_view *view,
                           const struct swc_decor *decor);
void
compositor_view_damage_decor(struct compositor_view *view);

/**
 * get the current composited buffer for a screen for screenshots.
 * returns null if no buffer
 */
struct wld_buffer *
compositor_get_buffer(struct screen *screen);

/**
 * render the compositor scene into a shm buffer
 * caller must free with wld_buffer_unreference()
 */
struct wld_buffer *
compositor_render_to_shm(struct screen *screen);

void
raise_window(struct compositor_view *view);
void
raise_window_top(struct compositor_view *view);

enum compositor_stack_layer {
	STACK_LAYER_BACKGROUND = 0,
	STACK_LAYER_BOTTOM = 1,
	STACK_LAYER_NORMAL = 2,
	STACK_LAYER_TOP = 3,
	STACK_LAYER_OVERLAY = 4,
};

void
compositor_view_set_stack_layer(struct compositor_view *view, uint32_t layer,
	                            bool raise);

#endif
