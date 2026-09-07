/* wld: wayland.h
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

#ifndef WLD_WAYLAND_H
#define WLD_WAYLAND_H

#include <stdbool.h>
#include <stdint.h>

struct wl_display;
struct wl_surface;

#define WLD_WAYLAND_ID (0x3 << 24)

enum wld_wayland_interface_id {
	/**
	 * Give up on trying any new interfaces. This can be considered as a
	 * sentinel for wld_wayland_create_context.
	 */
	WLD_NONE = -2,

	/**
	 * Try any available interface.
	 */
	WLD_ANY = -1,
	WLD_DRM,
	WLD_SHM
};

enum wld_wayland_object_type {
	WLD_WAYLAND_OBJECT_BUFFER = WLD_WAYLAND_ID
};

/**
 * Create a new WLD context which uses various available Wayland interfaces
 * (such as wl_shm and wl_drm) to create wl_buffers backed by implementations
 * specific to the interface.
 *
 * You can specify the particular interface you want to use by specifying them
 * as arguments. Interfaces will be tried in the order they are given.
 *
 * The last argument must be either WLD_NONE or WLD_ANY.
 *
 * @see enum wld_wayland_interface_id
 */
struct wld_context *wld_wayland_create_context(struct wl_display *display, enum wld_wayland_interface_id id, ...);

struct wld_surface *wld_wayland_create_surface(struct wld_context *context,
                                               uint32_t width, uint32_t height,
                                               uint32_t format, uint32_t flags,
                                               struct wl_surface *surface);

/**
 * Check if the wayland implementation supports a particular pixel format.
 *
 * @see enum wld_format
 */
bool wld_wayland_has_format(struct wld_context *context, uint32_t format);

#endif
