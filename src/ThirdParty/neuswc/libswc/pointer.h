/* swc: libswc/pointer.h
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

#ifndef SWC_POINTER_H
#define SWC_POINTER_H

#include "input.h"
#include "view.h"

#include <pixman.h>
#include <wayland-server.h>

struct button {
	struct press press;
	struct pointer_handler *handler;
};

struct pointer_handler {
	bool (*motion)(struct pointer_handler *handler, uint32_t time, wl_fixed_t x,
	               wl_fixed_t y);
	bool (*button)(struct pointer_handler *handler, uint32_t time,
	               struct button *button, uint32_t state);
	bool (*axis)(struct pointer_handler *handler, uint32_t time,
	             enum wl_pointer_axis axis, enum wl_pointer_axis_source source,
	             wl_fixed_t value, int value120);
	void (*frame)(struct pointer_handler *handler);

	int pending;
	struct wl_list link;
};

struct pointer {
	struct input_focus focus;
	struct input_focus_handler focus_handler;

	struct {
		struct view view;
		struct surface *surface;
		struct wl_listener destroy_listener;
		struct wld_buffer *buffer;

		/* Used for cursors set with pointer_set_cursor */
		struct wld_buffer *internal_buffer;

		struct {
			int32_t x, y;
		} hotspot;
	} cursor;

	struct wl_array buttons;
	struct wl_list handlers;
	struct pointer_handler client_handler;
	enum wl_pointer_axis_source client_axis_source;

	wl_fixed_t x, y;
	pixman_region32_t region;
};

bool
pointer_initialize(struct pointer *pointer);
void
pointer_finalize(struct pointer *pointer);
void
pointer_set_focus(struct pointer *pointer, struct compositor_view *view);
void
pointer_set_region(struct pointer *pointer, pixman_region32_t *region);
void
pointer_set_cursor(struct pointer *pointer, uint32_t id);

struct button *
pointer_get_button(struct pointer *pointer, uint32_t serial);

struct wl_resource *
pointer_bind(struct pointer *pointer, struct wl_client *client,
             uint32_t version, uint32_t id);
void
pointer_handle_button(struct pointer *pointer, uint32_t time, uint32_t button,
                      uint32_t state);
void
pointer_handle_axis(struct pointer *pointer, uint32_t time,
                    enum wl_pointer_axis axis,
                    enum wl_pointer_axis_source source, wl_fixed_t value,
                    int value120);
void
pointer_handle_relative_motion(struct pointer *pointer, uint32_t time,
                               wl_fixed_t dx, wl_fixed_t dy);
void
pointer_handle_absolute_motion(struct pointer *pointer, uint32_t time,
                               wl_fixed_t x, wl_fixed_t y);
void
pointer_handle_frame(struct pointer *pointer);

#endif
