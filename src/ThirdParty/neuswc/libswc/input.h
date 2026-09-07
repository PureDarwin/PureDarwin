/* swc: input.h
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

#ifndef SWC_INPUT_H
#define SWC_INPUT_H

#include <stdbool.h>
#include <wayland-server.h>

/* Focus {{{ */

enum { INPUT_FOCUS_EVENT_CHANGED };

struct input_focus_event_data {
	struct compositor_view *old, *new;
};

struct input_focus_handler {
	void (*enter)(struct input_focus_handler *handler,
	              struct wl_list *resources, struct compositor_view *view);
	void (*leave)(struct input_focus_handler *handler,
	              struct wl_list *resources, struct compositor_view *view);
};

struct input_focus {
	struct wl_client *client;
	struct compositor_view *view;
	struct wl_listener view_destroy_listener;

	struct input_focus_handler *handler;
	struct wl_list active, inactive;

	struct wl_signal event_signal;
};

bool
input_focus_initialize(struct input_focus *input_focus,
                       struct input_focus_handler *input_handler);
void
input_focus_finalize(struct input_focus *input_focus);
void
input_focus_add_resource(struct input_focus *input_focus,
                         struct wl_resource *resource);
void
input_focus_remove_resource(struct input_focus *input_focus,
                            struct wl_resource *resource);
void
input_focus_set(struct input_focus *input_focus, struct compositor_view *view);

/* }}} */

/* Key/button handling {{{ */

struct press {
	uint32_t value;
	uint32_t serial;
	void *data;
};

/* }}} */

#endif
