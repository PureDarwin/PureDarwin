/* swc: libswc/keyboard.h
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

#ifndef SWC_KEYBOARD_H
#define SWC_KEYBOARD_H

#include "input.h"

#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>

/* Keycodes are offset by 8 in XKB. */
#define XKB_KEY(key) ((key) + 8)

struct keyboard;
struct wl_client;

struct key {
	struct press press;
	struct keyboard_handler *handler;
};

struct keyboard_modifier_state {
	uint32_t depressed;
	uint32_t latched;
	uint32_t locked;
	uint32_t group;
};

struct keyboard_handler {
	bool (*key)(struct keyboard *keyboard, uint32_t time, struct key *key,
	            uint32_t state);
	bool (*modifiers)(struct keyboard *keyboard,
	                  const struct keyboard_modifier_state *state);

	struct wl_list link;
};

struct xkb {
	struct xkb_context *context;
	struct xkb_state *state;

	struct {
		struct xkb_keymap *map;
		int fd;
		uint32_t size;
		char *area;
	} keymap;

	struct {
		uint32_t ctrl, alt, super, shift;
	} indices;
};

struct keyboard {
	struct input_focus focus;
	struct input_focus_handler focus_handler;
	struct xkb xkb;

	struct wl_array keys;
	struct wl_list handlers;
	struct keyboard_handler client_handler;
	struct wl_array client_keys;

	struct keyboard_modifier_state modifier_state;
	uint32_t modifiers;
};

struct keyboard *
keyboard_create(struct xkb_rule_names *names);
void
keyboard_destroy(struct keyboard *keyboard);
bool
keyboard_reset(struct keyboard *keyboard);
void
keyboard_set_focus(struct keyboard *keyboard, struct compositor_view *view);
struct wl_resource *
keyboard_bind(struct keyboard *keyboard, struct wl_client *client,
              uint32_t version, uint32_t id);
void
keyboard_handle_key(struct keyboard *keyboard, uint32_t time, uint32_t key,
                    uint32_t state);

#endif
