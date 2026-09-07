/* swc: libswc/screen.h
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

#ifndef SWC_SCREEN_H
#define SWC_SCREEN_H

#include "primary_plane.h"
#include "swc.h"

#include <wayland-util.h>

struct output;
struct pixman_region32;

struct screen_modifier {
	/**
	 * Takes the screen geometry and sets 'usable' to the usable region of the
	 * screen. 'usable' is an already initialized pixman region.
	 */
	void (*modify)(struct screen_modifier *modifier,
	               const struct swc_rectangle *geometry,
	               struct pixman_region32 *usable);

	struct wl_list link;
};

struct screen {
	struct swc_screen base;
	const struct swc_screen_handler *handler;
	void *handler_data;

	struct wl_signal destroy_signal;
	uint8_t id;
#ifdef ENABLE_DRM
	uint32_t crtc;
#endif

	struct {
		struct primary_plane primary;
		struct plane *cursor;
	} planes;

	struct wl_global *global;
	struct wl_list resources;

	struct wl_list outputs;
	struct wl_list modifiers;
	struct wl_list link;
};

bool
screens_initialize(void);
void
screens_finalize(void);

#ifdef ENABLE_DRM
struct screen *
screen_new(uint32_t crtc, struct output *output, struct plane *cursor_plane);
#else
struct screen *
screen_new(struct output *output);
#endif
void
screen_destroy(struct screen *screen);

static inline uint32_t
screen_mask(struct screen *screen)
{
	return 1 << screen->id;
}

void
screen_update_usable_geometry(struct screen *screen);

#endif
