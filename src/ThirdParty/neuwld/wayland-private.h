/* wld: wayland-private.h
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

#ifndef WLD_WAYLAND_PRIVATE_H
#define WLD_WAYLAND_PRIVATE_H

#include "wld.h"

struct buffer;
struct wl_display;
struct wl_event_queue;
struct wl_buffer;

struct wayland_context {
	struct wld_context base;
	const struct wayland_impl *impl;
	struct wl_display *display;
	struct wl_event_queue *queue;
};

struct wayland_impl {
	struct wayland_context *(*create_context)(struct wl_display *display,
	                                          struct wl_event_queue *queue);
	bool (*has_format)(struct wld_context *context, uint32_t format);
};

#if WITH_WAYLAND_DRM
extern const struct wayland_impl drm_wayland_impl;
#endif

#if WITH_WAYLAND_SHM
extern const struct wayland_impl shm_wayland_impl;
#endif

bool wayland_buffer_add_exporter(struct buffer *buffer, struct wl_buffer *wl);

#endif
