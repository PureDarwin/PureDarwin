/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/util/log.h>
#include "util/time.h"

#include "backend.h"

struct pd_hid_keyboard_event {
	uint32_t sequence;
	uint8_t usage;
	uint8_t down;
	uint8_t reserved[2];
};

struct pd_hid_mouse_event {
	uint32_t sequence;
	uint8_t mouse_index;
	uint8_t buttons;
	int8_t dx, dy, wheel;
	uint8_t flags;
	uint16_t abs_y;
};

// an absolute record carries 0..0xffff screen fractions, abs_x in the dx/dy bytes
#define PD_HID_MOUSE_ABSOLUTE 0x01

static const uint16_t usb_to_evdev[256] = {
	[0x04] = 30, [0x05] = 48, [0x06] = 46, [0x07] = 32,
	[0x08] = 18, [0x09] = 33, [0x0a] = 34, [0x0b] = 35,
	[0x0c] = 23, [0x0d] = 36, [0x0e] = 37, [0x0f] = 38,
	[0x10] = 50, [0x11] = 49, [0x12] = 24, [0x13] = 25,
	[0x14] = 16, [0x15] = 19, [0x16] = 31, [0x17] = 20,
	[0x18] = 22, [0x19] = 47, [0x1a] = 17, [0x1b] = 45,
	[0x1c] = 21, [0x1d] = 44, [0x1e] = 2,  [0x1f] = 3,
	[0x20] = 4,  [0x21] = 5,  [0x22] = 6,  [0x23] = 7,
	[0x24] = 8,  [0x25] = 9,  [0x26] = 10, [0x27] = 11,
	[0x28] = 28, [0x29] = 1, [0x2a] = 14, [0x2b] = 15,
	[0x2c] = 57, [0x2d] = 12, [0x2e] = 13, [0x2f] = 26,
	[0x30] = 27, [0x31] = 43, [0x32] = 43, [0x33] = 39,
	[0x34] = 40, [0x35] = 41, [0x36] = 51, [0x37] = 52,
	[0x38] = 53, [0x39] = 58, [0x3a] = 59, [0x3b] = 60,
	[0x3c] = 61, [0x3d] = 62, [0x3e] = 63, [0x3f] = 64,
	[0x40] = 65, [0x41] = 66, [0x42] = 67, [0x43] = 68,
	[0x44] = 87, [0x45] = 88, [0x4f] = 106, [0x50] = 105,
	[0x51] = 108, [0x52] = 103,
	[0xe0] = 29, [0xe1] = 42, [0xe2] = 56, [0xe3] = 125,
	[0xe4] = 97, [0xe5] = 54, [0xe6] = 100, [0xe7] = 126,
};

static const struct wlr_keyboard_impl keyboard_impl = { .name = "puredarwin-hid" };
static const struct wlr_pointer_impl pointer_impl = { .name = "puredarwin-hid" };

static int keyboard_readable(int fd, uint32_t mask, void *data) {
	struct wlr_puredarwin_backend *backend = data;
	struct pd_hid_keyboard_event event;
	(void)fd;
	(void)mask;
	while (read(backend->keyboard_fd, &event, sizeof(event)) == sizeof(event)) {
		uint32_t keycode = usb_to_evdev[event.usage];
		if (keycode == 0) {
			continue;
		}
		struct wlr_keyboard_key_event key = {
			.time_msec = get_current_time_msec(),
			.keycode = keycode,
			.update_state = true,
			.state = event.down ? WL_KEYBOARD_KEY_STATE_PRESSED :
				WL_KEYBOARD_KEY_STATE_RELEASED,
		};
		wlr_keyboard_notify_key(&backend->keyboard, &key);
	}
	return 0;
}

static int mouse_readable(int fd, uint32_t mask, void *data) {
	struct wlr_puredarwin_backend *backend = data;
	struct pd_hid_mouse_event event;
	(void)fd;
	(void)mask;
	while (read(backend->mouse_fd, &event, sizeof(event)) == sizeof(event)) {
		uint32_t time = get_current_time_msec();
		if (event.flags & PD_HID_MOUSE_ABSOLUTE) {
			uint16_t abs_x;
			memcpy(&abs_x, &event.dx, sizeof(abs_x));
			struct wlr_pointer_motion_absolute_event motion = {
				.pointer = &backend->pointer,
				.time_msec = time,
				.x = abs_x / 65535.0,
				.y = event.abs_y / 65535.0,
			};
			wl_signal_emit_mutable(&backend->pointer.events.motion_absolute, &motion);
		} else if (event.dx || event.dy) {
			struct wlr_pointer_motion_event motion = {
				.pointer = &backend->pointer,
				.time_msec = time,
				.delta_x = event.dx,
				.delta_y = event.dy,
				.unaccel_dx = event.dx,
				.unaccel_dy = event.dy,
			};
			wl_signal_emit_mutable(&backend->pointer.events.motion, &motion);
		}
		uint8_t changed = event.buttons ^ backend->mouse_buttons;
		for (uint32_t bit = 0; bit < 3; bit++) {
			if (!(changed & (1u << bit))) {
				continue;
			}
			struct wlr_pointer_button_event button = {
				.pointer = &backend->pointer,
				.time_msec = time,
				.button = 0x110 + bit,
				.state = (event.buttons & (1u << bit)) ?
					WL_POINTER_BUTTON_STATE_PRESSED :
					WL_POINTER_BUTTON_STATE_RELEASED,
			};
			wlr_pointer_notify_button(&backend->pointer, &button);
		}
		backend->mouse_buttons = event.buttons;
		if (event.wheel) {
			struct wlr_pointer_axis_event axis = {
				.pointer = &backend->pointer,
				.time_msec = time,
				.source = WL_POINTER_AXIS_SOURCE_WHEEL,
				.orientation = WL_POINTER_AXIS_VERTICAL_SCROLL,
				.delta = -event.wheel * 10.0,
				.delta_discrete = -event.wheel * 120,
			};
			wl_signal_emit_mutable(&backend->pointer.events.axis, &axis);
		}
		wl_signal_emit_mutable(&backend->pointer.events.frame, &backend->pointer);
	}
	return 0;
}

static void input_emit_ready(struct wlr_puredarwin_backend *backend) {
	if (backend->keyboard_fd >= 0 && !backend->keyboard_emitted) {
		/* PureDarwin HID character devices are readable but are not
		 * kqueue-filterable. They are drained by the timer below. */
		backend->keyboard_emitted = true;
		wl_signal_emit_mutable(&backend->backend.events.new_input,
			&backend->keyboard.base);
	}
	if (backend->mouse_fd >= 0 && !backend->pointer_emitted) {
		backend->pointer_emitted = true;
		wl_signal_emit_mutable(&backend->backend.events.new_input,
			&backend->pointer.base);
	}
}

static int input_retry(void *data) {
	struct wlr_puredarwin_backend *backend = data;
	if (backend->keyboard_fd < 0) {
		backend->keyboard_fd = open("/dev/usb_hid_kbd", O_RDONLY | O_NONBLOCK);
	}
	if (backend->mouse_fd < 0) {
		backend->mouse_fd = open("/dev/usb_hid_mouse", O_RDONLY | O_NONBLOCK);
	}
	input_emit_ready(backend);
	if (backend->keyboard_fd >= 0)
		keyboard_readable(backend->keyboard_fd, WL_EVENT_READABLE, backend);
	if (backend->mouse_fd >= 0)
		mouse_readable(backend->mouse_fd, WL_EVENT_READABLE, backend);
	if (backend->input_retry_timer != NULL) {
		/* The HID devices do not participate in kqueue. Poll often enough
		 * to keep pointer motion and key repeat responsive. */
		wl_event_source_timer_update(backend->input_retry_timer, 8);
	}
	return 0;
}

bool puredarwin_input_init(struct wlr_puredarwin_backend *backend) {
	backend->keyboard_fd = -1;
	backend->mouse_fd = -1;
	wlr_keyboard_init(&backend->keyboard, &keyboard_impl, "PureDarwin HID Keyboard");
	wlr_pointer_init(&backend->pointer, &pointer_impl, "PureDarwin HID Pointer");
	backend->input_ready = true;
	backend->input_retry_timer = wl_event_loop_add_timer(backend->event_loop,
		input_retry, backend);
	if (backend->input_retry_timer == NULL) {
		wlr_log(WLR_ERROR, "PureDarwin HID retry timer failed");
	}
	if (!backend->keyboard_emitted || !backend->pointer_emitted) {
		wlr_log(WLR_INFO, "PureDarwin HID waiting for devices (keyboard=%s mouse=%s)",
			backend->keyboard_emitted ? "ready" : "waiting",
			backend->pointer_emitted ? "ready" : "waiting");
		if (backend->input_retry_timer != NULL)
			wl_event_source_timer_update(backend->input_retry_timer, 8);
	} else {
		wlr_log(WLR_INFO, "PureDarwin HID input ready (keyboard=yes mouse=yes)");
	}
	/* Run once immediately, then continue from the timer. */
	input_retry(backend);
	return true;
}

void puredarwin_input_finish(struct wlr_puredarwin_backend *backend) {
	if (!backend->input_ready) {
		return;
	}
	if (backend->input_retry_timer) wl_event_source_remove(backend->input_retry_timer);
	if (backend->keyboard_source) wl_event_source_remove(backend->keyboard_source);
	if (backend->mouse_source) wl_event_source_remove(backend->mouse_source);
	if (backend->keyboard_fd >= 0) close(backend->keyboard_fd);
	if (backend->mouse_fd >= 0) close(backend->mouse_fd);
	wlr_keyboard_finish(&backend->keyboard);
	wlr_pointer_finish(&backend->pointer);
	backend->input_ready = false;
}
