/* swc: libswc/view.h
 *
 * Copyright (c) 2013 Michael Forney
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

#ifndef SWC_VIEW_H
#define SWC_VIEW_H

#include "swc.h"

#include <wayland-util.h>

/**
 * A view represents a component that can display buffers to the user.
 *
 * For example, each output contains a view residing in its framebuffer plane
 * that is used to scan out buffers. Additionally, the compositor creates views
 * dynamically for each surface it displays, compositing them into a single
 * buffer, which it can then attach to the previously mentioned view.
 *
 * This abstraction allows various components of swc to connect in a general
 * way, allowing operations like setting the output view of a surface directly
 * to an output's framebuffer plane, bypassing the compositor.
 */
struct view {
	const struct view_impl *impl;
	struct wl_list handlers;

	struct swc_rectangle geometry;
	uint32_t screens;

	struct wld_buffer *buffer;
};

struct view_handler {
	const struct view_handler_impl *impl;
	struct wl_list link;
};

/**
 * Every view must have an implementation containing these functions.
 *
 * For descriptions, see the corresponding view_* function.
 */
struct view_impl {
	bool (*update)(struct view *view);
	int (*attach)(struct view *view, struct wld_buffer *buffer);
	bool (*move)(struct view *view, int32_t x, int32_t y);
};

struct view_handler_impl {
	/* Called when the view has displayed the next frame. */
	void (*frame)(struct view_handler *handler, uint32_t time);
	/* Called when a new buffer is attached to the view. */
	void (*attach)(struct view_handler *handler);
	/* Called after the view's position changes. */
	void (*move)(struct view_handler *handler);
	/* Called after the view's size changes. */
	void (*resize)(struct view_handler *handler, uint32_t old_width,
	               uint32_t old_height);
	/* Called when the set of screens the view is visible on changes. */
	void (*screens)(struct view_handler *handler, uint32_t left,
	                uint32_t entered);
};

/**
 * Attach a new buffer to the view.
 *
 * If buffer is NULL, the previous buffer is removed from the view.
 *
 * @return 0 on success, negative error code otherwise.
 */
int
view_attach(struct view *view, struct wld_buffer *buffer);

/**
 * Display a new frame consisting of the currently attached buffer.
 *
 * @return Whether or not the update succeeds.
 */
bool
view_update(struct view *view);

/**
 * Move the view to the specified coordinates, if supported.
 *
 * @return Whether or not the move succeeds.
 */
bool
view_move(struct view *view, int32_t x, int32_t y);

/**** For internal view use only ****/

/**
 * Initialize a new view with the specified implementation.
 */
void
view_initialize(struct view *view, const struct view_impl *impl);

/**
 * Release any resources associated with this view.
 */
void
view_finalize(struct view *view);

bool
view_set_position(struct view *view, int32_t x, int32_t y);
bool
view_set_size(struct view *view, uint32_t width, uint32_t height);
bool
view_set_size_from_buffer(struct view *view, struct wld_buffer *bufer);
void
view_set_screens(struct view *view, uint32_t screens);
void
view_update_screens(struct view *view);

/**
 * Send a new frame event through the view's event signal.
 *
 * This should be called by the view itself when the next frame is visible to
 * the user. If time information is not available, get_time() can be passed
 * instead.
 */
void
view_frame(struct view *view, uint32_t time);

#endif
