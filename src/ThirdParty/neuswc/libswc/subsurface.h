/* swc: libswc/subsurface.h
 *
 * Copyright (c) 2015 Michael Forney
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

#ifndef SWC_SUBSURFACE_H
#define SWC_SUBSURFACE_H

#include "view.h"

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server.h>

struct wl_client;
struct surface;

struct subsurface {
	struct wl_resource *resource;
	struct surface *surface;
	struct surface *parent;
	struct view_handler parent_view_handler;
	struct wl_listener surface_destroy_listener;
	struct wl_listener parent_destroy_listener;
	struct wl_list link;
	struct wl_list pending_link;
	struct wl_list current_link;
	int32_t x, y;
	int32_t pending_x, pending_y;
	bool pending_position;
	bool sync;
	bool pending;
	bool added;
};

bool
subsurface_is_synchronized(const struct subsurface *subsurface);
void
subsurface_update_visibility(struct subsurface *subsurface);
void
subsurface_parent_commit(struct surface *parent);

struct subsurface *
subsurface_new(struct wl_client *client, uint32_t version, uint32_t id,
               struct surface *surface, struct surface *parent);

#endif
