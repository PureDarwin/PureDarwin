/* swc: libswc/seat-ws.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 * Copyright (c) 2019 Nia Alarie
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

#include "compositor.h"
#include "data_device.h"
#include "event.h"
#include "internal.h"
#include "keyboard.h"
#include "launch.h"
#include "pointer.h"
#include "screen.h"
#include "seat.h"
#include "surface.h"
#include "util.h"
#include "wscons/atKeynames.h"
#include "wscons/bsd_KbdMap.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsksymdef.h>
#include <sys/ioctl.h>

/* Map wscons encodings to libxkbcommon layout names. */
struct ws_xkb_map {
	const int ws;
	const char *const xkb;
};

static const struct ws_xkb_map ws_xkb_encodings[] = {
    {KB_UK, "gb"}, {KB_BE, "be"},
#ifdef KB_CZ
    {KB_CZ, "cz"},
#endif
    {KB_DK, "dk"}, {KB_NL, "nl"}, {KB_DE, "de"},
#ifdef KB_GR
    {KB_GR, "gr"},
#endif
    {KB_HU, "hu"}, {KB_IT, "it"}, {KB_JP, "jp"}, {KB_NO, "no"}, {KB_PL, "pl"},
    {KB_PT, "pt"}, {KB_RU, "ru"}, {KB_ES, "es"}, {KB_SV, "sv"}, {KB_SG, "sg"},
    {KB_TR, "tr"}, {KB_UA, "ua"}, {-1, NULL}};

struct seat {
	struct swc_seat base;

	char *name;
	uint32_t capabilities;

	int mouse_fd;
	int kbd_fd;
	bool ignore;

	unsigned kbd_type;

	struct xkb_rule_names names;

	struct wl_event_source *mouse_source;
	struct wl_event_source *kbd_source;

	struct wl_listener swc_listener;

	struct wl_listener keyboard_focus_listener;
	struct pointer pointer;
	struct wl_listener data_device_listener;

	struct wl_global *global;
	struct wl_list resources;
};

static void
handle_keyboard_focus_event(struct wl_listener *listener, void *data)
{
	struct seat *seat =
	    wl_container_of(listener, seat, keyboard_focus_listener);
	struct event *ev = data;
	struct input_focus_event_data *event_data = ev->data;

	if (ev->type != INPUT_FOCUS_EVENT_CHANGED) {
		return;
	}

	if (event_data->new) {
		struct wl_client *client =
		    wl_resource_get_client(event_data->new->surface->resource);

		/* Offer the selection to the new focus. */
		data_device_offer_selection(seat->base.data_device, client);
	}
}

static void
handle_data_device_event(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, data_device_listener);
	struct event *ev = data;

	if (ev->type != DATA_DEVICE_EVENT_SELECTION_CHANGED) {
		return;
	}

	if (seat->base.keyboard->focus.client) {
		data_device_offer_selection(seat->base.data_device,
		                            seat->base.keyboard->focus.client);
	}
}

static void
handle_swc_event(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swc_listener);
	struct event *ev = data;

	switch (ev->type) {
	case SWC_EVENT_DEACTIVATED:
		seat->ignore = true;
		keyboard_reset(seat->base.keyboard);
		break;
	case SWC_EVENT_ACTIVATED:
		seat->ignore = false;
		break;
	}
}

/* Wayland Seat Interface */
static void
get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct seat *seat = wl_resource_get_user_data(resource);

	pointer_bind(&seat->pointer, client, wl_resource_get_version(resource), id);
}

static void
get_keyboard(struct wl_client *client, struct wl_resource *resource,
             uint32_t id)
{
	struct seat *seat = wl_resource_get_user_data(resource);

	keyboard_bind(seat->base.keyboard, client,
	              wl_resource_get_version(resource), id);
}

static void
get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	/* XXX: Implement */
}

static struct wl_seat_interface seat_impl = {
    .get_pointer = get_pointer,
    .get_keyboard = get_keyboard,
    .get_touch = get_touch,
    .release = destroy_resource,
};

static void
bind_seat(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct seat *seat = data;
	struct wl_resource *resource;

	if (version > 5) {
		version = 5;
	}

	resource = wl_resource_create(client, &wl_seat_interface, version, id);
	wl_resource_set_implementation(resource, &seat_impl, seat,
	                               &remove_resource);
	wl_list_insert(&seat->resources, wl_resource_get_link(resource));

	if (version >= 2) {
		wl_seat_send_name(resource, seat->name);
	}

	wl_seat_send_capabilities(resource, seat->capabilities);
}

static int
ws_to_xkb(unsigned type, int key)
{
	switch (type) {
	case WSKBD_TYPE_PC_XT:
	case WSKBD_TYPE_PC_AT:
		return wsXtMap[key];
	case WSKBD_TYPE_USB:
#ifdef WSKBD_TYPE_MAPLE
	case WSKBD_TYPE_MAPLE:
		return wsUsbMap[key];
#endif
		return wsUsbMap[key];
	default:
		fprintf(stderr, "Unknown wskbd type %d\n", type);
		return key;
	}
}

static int
wsmouse_to_evdev(int button)
{
	/* The right and middle mouse buttons must be swapped. */
	switch (button) {
	case 1: /* Middle */
		return 0x112;
	case 2: /* Right */
		return 0x111;
	default:
		return button + 0x110;
	}
}

static int
handle_ws_data(int fd, uint32_t mask, void *data)
{
	struct seat *seat = data;
	struct wscons_event ev;

	while (!seat->ignore && (read(fd, &ev, sizeof(ev))) != -1) {
		uint32_t state, time;
		int key;
		wl_fixed_t pos;

		time = ev.time.tv_sec + (ev.time.tv_nsec / 1000000L);
		switch (ev.type) {
		case WSCONS_EVENT_KEY_UP:
			state = WL_KEYBOARD_KEY_STATE_RELEASED;
			key = ws_to_xkb(seat->kbd_type, ev.value);
			keyboard_handle_key(seat->base.keyboard, time, key, state);
			break;
		case WSCONS_EVENT_KEY_DOWN:
			state = WL_KEYBOARD_KEY_STATE_PRESSED;
			key = ws_to_xkb(seat->kbd_type, ev.value);
			keyboard_handle_key(seat->base.keyboard, time, key, state);
			break;
		case WSCONS_EVENT_ALL_KEYS_UP:
			break;
		case WSCONS_EVENT_MOUSE_UP:
			state = WL_POINTER_BUTTON_STATE_RELEASED;
			key = wsmouse_to_evdev(ev.value);
			pointer_handle_button(seat->base.pointer, time, key, state);
			break;
		case WSCONS_EVENT_MOUSE_DOWN:
			state = WL_POINTER_BUTTON_STATE_PRESSED;
			key = wsmouse_to_evdev(ev.value);
			pointer_handle_button(seat->base.pointer, time, key, state);
			break;
		case WSCONS_EVENT_MOUSE_DELTA_X:
			pos = wl_fixed_from_int(ev.value);
			pointer_handle_relative_motion(seat->base.pointer, time, pos, 0);
			break;
		case WSCONS_EVENT_MOUSE_DELTA_Y:
			pos = wl_fixed_from_int(-ev.value);
			pointer_handle_relative_motion(seat->base.pointer, time, 0, pos);
			break;
		case WSCONS_EVENT_MOUSE_DELTA_Z:
			pos = wl_fixed_from_int(ev.value * 10);
			pointer_handle_axis(seat->base.pointer, time,
				WL_POINTER_AXIS_VERTICAL_SCROLL,
				WL_POINTER_AXIS_SOURCE_WHEEL, pos, pos * 12);
			break;
		case WSCONS_EVENT_MOUSE_DELTA_W:
			pos = wl_fixed_from_int(ev.value * 10);
			pointer_handle_axis(seat->base.pointer, time,
				WL_POINTER_AXIS_HORIZONTAL_SCROLL,
				WL_POINTER_AXIS_SOURCE_WHEEL, pos, pos * 12);
			break;
		case WSCONS_EVENT_MOUSE_ABSOLUTE_X:
			pos = wl_fixed_from_int(ev.value);
			pointer_handle_absolute_motion(seat->base.pointer, time, pos, 0);
			break;
		case WSCONS_EVENT_MOUSE_ABSOLUTE_Y:
			pos = wl_fixed_from_int(-ev.value);
			pointer_handle_absolute_motion(seat->base.pointer, time, 0, pos);
			break;
		}
	}
	return 0;
}

static bool
initialize_wscons(struct seat *seat)
{
#ifdef WSMOUSE_EVENT_VERSION
	int mouse_ver = WSMOUSE_EVENT_VERSION;
#endif
#ifdef WSKBDIO_EVENT_VERSION
	int kbd_ver = WSKBDIO_EVENT_VERSION;
#endif
	int encoding_layout;
	kbd_t encoding;
	unsigned i;

	if ((seat->mouse_fd =
	         launch_open_device("/dev/wsmouse", O_RDWR | O_NONBLOCK)) == -1) {
		ERROR("Could not open mouse device\n");
		goto error0;
	}
	if ((seat->kbd_fd =
	         launch_open_device("/dev/wskbd", O_RDWR | O_NONBLOCK)) == -1) {
		ERROR("Could not open keyboard device\n");
		goto error1;
	}

#ifdef WSMOUSEIO_SETVERSION
	(void)ioctl(seat->mouse_fd, WSMOUSEIO_SETVERSION, &mouse_ver);
#endif
#ifdef WSKBDIO_SETVERSION
	(void)ioctl(seat->kbd_fd, WSKBDIO_SETVERSION, &kbd_ver);
#endif

	/* set devices to nativemode to receive events */
#ifdef WSMOUSEIO_SETMODE
	{
		int mode = WSMOUSE_COMPAT; /* use compat mode; it sends events */
		if (ioctl(seat->mouse_fd, WSMOUSEIO_SETMODE, &mode) == -1) {
			fprintf(stderr, "wscons: WSMOUSEIO_SETMODE failed: %s\n",
			        strerror(errno));
		}
	}
#endif /* WSMOUSEIO_SETMODE */
#ifdef WSKBDIO_SETMODE
	{
		int mode = WSKBD_TRANSLATED; /* use translated mode for key events */
		if (ioctl(seat->kbd_fd, WSKBDIO_SETMODE, &mode) == -1) {
			fprintf(stderr, "wscons: WSKBDIO_SETMODE failed: %s\n",
			        strerror(errno));
		}
	}
#endif /* WSKBDIO_SETMODE */

	if (ioctl(seat->kbd_fd, WSKBDIO_GTYPE, &seat->kbd_type) == -1) {
		ERROR("Could not get keyboard type\n");
		goto error2;
	}

	if (ioctl(seat->kbd_fd, WSKBDIO_GETENCODING, &encoding) != -1) {
		encoding_layout = KB_ENCODING(encoding);
		for (i = 0; ws_xkb_encodings[i].xkb != NULL; ++i) {
			if (ws_xkb_encodings[i].ws == encoding_layout) {
				seat->names.layout = ws_xkb_encodings[i].xkb;
				break;
			}
		}
		switch (KB_VARIANT(encoding)) {
		case KB_NODEAD:
			seat->names.variant = "nodeadkeys";
			break;
		case KB_SWAPCTRLCAPS:
			seat->names.options = "ctrl:swapcaps";
			break;
		case KB_DVORAK:
			seat->names.variant = "dvorak";
			break;
		case KB_COLEMAK:
			seat->names.variant = "colemak";
			break;
		}
	}

	return true;
error2:
	close(seat->kbd_fd);
error1:
	close(seat->mouse_fd);
error0:
	return false;
}

struct swc_seat *
seat_create(struct wl_display *display, const char *seat_name)
{
	struct seat *seat;

	seat = malloc(sizeof(*seat));
	if (!seat) {
		goto error0;
	}

	seat->ignore = false;
	memset(&seat->names, 0, sizeof(seat->names));
	seat->names.rules = "base";
	seat->names.model = "pc105";
	seat->names.layout = "us";
	seat->names.variant = "basic";

	seat->name = strdup(seat_name);
	if (!seat->name) {
		ERROR("Could not allocate seat name string\n");
		goto error1;
	}

	if (!initialize_wscons(seat)) {
		goto error2;
	}

	seat->global =
	    wl_global_create(display, &wl_seat_interface, 5, seat, &bind_seat);
	if (!seat->global) {
		goto error2;
	}
	seat->capabilities =
	    WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER;
	wl_list_init(&seat->resources);

	seat->swc_listener.notify = &handle_swc_event;
	wl_signal_add(&swc.event_signal, &seat->swc_listener);

	seat->base.data_device = data_device_create();
	if (!seat->base.data_device) {
		ERROR("Could not initialize data device\n");
		goto error3;
	}
	seat->data_device_listener.notify = &handle_data_device_event;
	wl_signal_add(&seat->base.data_device->event_signal,
	              &seat->data_device_listener);

	seat->base.keyboard = keyboard_create(&seat->names);
	if (!seat->base.keyboard) {
		ERROR("Could not initialize keyboard\n");
		goto error4;
	}
	seat->keyboard_focus_listener.notify = handle_keyboard_focus_event;
	wl_signal_add(&seat->base.keyboard->focus.event_signal,
	              &seat->keyboard_focus_listener);

	if (!pointer_initialize(&seat->pointer)) {
		ERROR("Could not initialize pointer\n");
		goto error5;
	}
	seat->base.pointer = &seat->pointer;

	seat->kbd_source = wl_event_loop_add_fd(
	    swc.event_loop, seat->kbd_fd, WL_EVENT_READABLE, &handle_ws_data, seat);
	seat->mouse_source =
	    wl_event_loop_add_fd(swc.event_loop, seat->mouse_fd, WL_EVENT_READABLE,
	                         &handle_ws_data, seat);

	return &seat->base;

error5:
	keyboard_destroy(seat->base.keyboard);
error4:
	data_device_destroy(seat->base.data_device);
error3:
	wl_global_destroy(seat->global);
error2:
	free(seat->name);
error1:
	free(seat);
error0:
	return NULL;
}

void
seat_destroy(struct swc_seat *seat_base)
{
	struct seat *seat = wl_container_of(seat_base, seat, base);

	wl_event_source_remove(seat->mouse_source);
	wl_event_source_remove(seat->kbd_source);
	close(seat->mouse_fd);
	seat->mouse_fd = -1;
	close(seat->kbd_fd);
	seat->kbd_fd = -1;

	pointer_finalize(&seat->pointer);
	keyboard_destroy(seat->base.keyboard);
	data_device_destroy(seat->base.data_device);

	wl_global_destroy(seat->global);
	free(seat->name);
	free(seat);
}
