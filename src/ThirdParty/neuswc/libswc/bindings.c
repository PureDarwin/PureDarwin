/* swc: swc/bindings.c
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

#include "bindings.h"
#include "internal.h"
#include "keyboard.h"
#include "pointer.h"
#include "seat.h"
#include "swc.h"
#include "util.h"

#include <errno.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>

struct binding {
	uint32_t value;
	uint32_t modifiers;
	swc_binding_handler handler;
	void *data;
};

struct axis_binding {
	uint32_t axis;
	uint32_t modifiers;
	swc_axis_binding_handler handler;
	void *data;
};

static bool
handle_key(struct keyboard *keyboard, uint32_t time, struct key *key,
           uint32_t state);

static struct keyboard_handler key_binding_handler = {
    .key = handle_key,
};

static bool
handle_button(struct pointer_handler *handler, uint32_t time,
              struct button *button, uint32_t state);
static bool
handle_axis(struct pointer_handler *handler, uint32_t time,
            enum wl_pointer_axis axis, enum wl_pointer_axis_source source,
            wl_fixed_t value, int value120);

static struct pointer_handler button_binding_handler = {
    .button = handle_button,
    .axis = handle_axis,
};

static struct wl_array key_bindings, button_bindings, axis_bindings;

const struct swc_bindings swc_bindings = {
    .keyboard_handler = &key_binding_handler,
    .pointer_handler = &button_binding_handler,
};

static struct binding *
find_binding(struct wl_array *bindings, uint32_t modifiers, uint32_t value)
{
	struct binding *binding;

	wl_array_for_each(binding, bindings)
	{
		if (binding->value == value && (binding->modifiers == modifiers ||
		                                binding->modifiers == SWC_MOD_ANY)) {
			return binding;
		}
	}

	return NULL;
}

static struct binding *
find_key_binding(uint32_t modifiers, uint32_t key)
{
	struct binding *binding;
	struct xkb *xkb = &swc.seat->keyboard->xkb;
	xkb_keysym_t keysym;

	/* First try the keysym the keymap generates in it's current state. */
	keysym = xkb_state_key_get_one_sym(xkb->state, XKB_KEY(key));
	binding = find_binding(&key_bindings, modifiers, keysym);

	if (binding) {
		return binding;
	}

	xkb_layout_index_t layout;
	const xkb_keysym_t *keysyms;

	/* Then try the keysym associated with shift-level 0 for the key. */
	layout = xkb_state_key_get_layout(xkb->state, XKB_KEY(key));
	xkb_keymap_key_get_syms_by_level(xkb->keymap.map, XKB_KEY(key), layout, 0,
	                                 &keysyms);

	if (!keysyms) {
		return NULL;
	}

	binding = find_binding(&key_bindings, modifiers, keysyms[0]);

	return binding;
}

static struct binding *
find_button_binding(uint32_t modifiers, uint32_t value)
{
	return find_binding(&button_bindings, modifiers, value);
}

static struct axis_binding *
find_axis_binding(uint32_t modifiers, uint32_t axis)
{
	struct axis_binding *binding;

	wl_array_for_each(binding, &axis_bindings)
	{
		if (binding->axis == axis && (binding->modifiers == modifiers ||
		                              binding->modifiers == SWC_MOD_ANY)) {
			return binding;
		}
	}

	return NULL;
}

static bool
handle_binding(uint32_t time, struct press *press, uint32_t state,
               struct binding *(*find_binding)(uint32_t, uint32_t))
{
	struct binding *binding;
	uint32_t modifiers =
	    swc.seat && swc.seat->keyboard ? swc.seat->keyboard->modifiers : 0;

	if (state) {
		binding = find_binding(modifiers, press->value);

		if (!binding) {
			return false;
		}

		press->data = binding;
	} else {
		binding = press->data;
	}

	binding->handler(binding->data, time, binding->value, state);

	return true;
}

bool
handle_key(struct keyboard *keyboard, uint32_t time, struct key *key,
           uint32_t state)
{
	return handle_binding(time, &key->press, state, &find_key_binding);
}

bool
handle_button(struct pointer_handler *handler, uint32_t time,
              struct button *button, uint32_t state)
{
	return handle_binding(time, &button->press, state, &find_button_binding);
}

bool
handle_axis(struct pointer_handler *handler, uint32_t time,
            enum wl_pointer_axis axis, enum wl_pointer_axis_source source,
            wl_fixed_t value, int value120)
{
	(void)handler;
	(void)source;

	uint32_t modifiers =
	    swc.seat && swc.seat->keyboard ? swc.seat->keyboard->modifiers : 0;
	struct axis_binding *binding = find_axis_binding(modifiers, axis);
	int32_t delta120 = value120;

	if (!binding || !binding->handler) {
		return false;
	}

	if (!delta120 && value) {
		delta120 = (int32_t)(wl_fixed_to_double(value) * 120.0);
		if (!delta120) {
			delta120 = value > 0 ? 1 : -1;
		}
	}

	binding->handler(binding->data, time, axis, delta120);
	return true;
}

bool
bindings_initialize(void)
{
	wl_array_init(&key_bindings);
	wl_array_init(&button_bindings);
	wl_array_init(&axis_bindings);

	return true;
}

void
bindings_finalize(void)
{
	wl_array_release(&key_bindings);
	wl_array_release(&button_bindings);
	wl_array_release(&axis_bindings);
}

EXPORT int
swc_add_binding(enum swc_binding_type type, uint32_t modifiers, uint32_t value,
                swc_binding_handler handler, void *data)
{
	struct binding *binding;
	struct wl_array *bindings;

	switch (type) {
	case SWC_BINDING_KEY:
		bindings = &key_bindings;
		break;
	case SWC_BINDING_BUTTON:
		bindings = &button_bindings;
		break;
	default:
		return -EINVAL;
	}

	if (!(binding = wl_array_add(bindings, sizeof(*binding)))) {
		return -ENOMEM;
	}

	binding->value = value;
	binding->modifiers = modifiers;
	binding->handler = handler;
	binding->data = data;

	return 0;
}

EXPORT void
swc_remove_binding(enum swc_binding_type type, uint32_t modifiers,
                   uint32_t value)
{
	struct wl_array *bindings;
	switch (type) {
	case SWC_BINDING_KEY:
		bindings = &key_bindings;
		break;
	case SWC_BINDING_BUTTON:
		bindings = &button_bindings;
		break;
	default:
		return;
	}

	struct binding *b = find_binding(bindings, modifiers, value);
	if (!b) {
		return;
	}

	struct binding *last = (struct binding *)((char *)bindings->data +
	                                          bindings->size - sizeof(*b));
	if (b != last) {
		*b = *last;
	}
	bindings->size -= sizeof(*b);
}

EXPORT int
swc_add_axis_binding(uint32_t modifiers, uint32_t axis,
                     swc_axis_binding_handler handler, void *data)
{
	struct axis_binding *binding;

	if (!(binding = wl_array_add(&axis_bindings, sizeof(*binding)))) {
		return -ENOMEM;
	}

	binding->axis = axis;
	binding->modifiers = modifiers;
	binding->handler = handler;
	binding->data = data;

	return 0;
}
