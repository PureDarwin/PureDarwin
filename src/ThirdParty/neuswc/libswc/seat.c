/* swc: libswc/seat.c
 *
 * Copyright (c) 2013-2020 Michael Forney
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

#include "seat.h"
#include "compositor.h"
#include "data_device.h"
#include "event.h"
#include "internal.h"
#include "keyboard.h"
#include "launch.h"
#include "pointer.h"
#include "screen.h"
#include "surface.h"
#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libinput.h>
#if defined(__linux__)
#include <linux/input.h>
#elif defined(__FreeBSD__)
#include <dev/evdev/input.h> /* mirrors linux/input.h functionality */
#endif
#ifdef ENABLE_LIBUDEV
#include <libudev.h>
#endif

#ifndef NETLINK_MASK
#define NETLINK_MASK 4
#endif

struct seat {
	struct swc_seat base;

	char *name;
	uint32_t capabilities;

	struct libinput *libinput;
	struct wl_event_source *libinput_source;

#ifdef ENABLE_LIBUDEV
	struct udev *udev;
#endif

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
		libinput_suspend(seat->libinput);
		keyboard_reset(seat->base.keyboard);
		break;
	case SWC_EVENT_ACTIVATED:
		if (libinput_resume(seat->libinput) != 0) {
			WARNING("Failed to resume libinput context\n");
		}
		break;
	}
}

/* Wayland Seat Interface */
static void
get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct seat *seat = wl_resource_get_user_data(resource);

	if (!pointer_bind(&seat->pointer, client, wl_resource_get_version(resource),
	                  id)) {
		wl_resource_post_no_memory(resource);
	}
}

static void
get_keyboard(struct wl_client *client, struct wl_resource *resource,
             uint32_t id)
{
	struct seat *seat = wl_resource_get_user_data(resource);

	if (!keyboard_bind(seat->base.keyboard, client,
	                   wl_resource_get_version(resource), id)) {
		wl_resource_post_no_memory(resource);
	}
}

static void
get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	/* XXX: Implement */
}

static const struct wl_seat_interface seat_impl = {
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

	resource = wl_resource_create(client, &wl_seat_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &seat_impl, seat,
	                               &remove_resource);
	wl_list_insert(&seat->resources, wl_resource_get_link(resource));

	if (version >= 2) {
		wl_seat_send_name(resource, seat->name);
	}

	wl_seat_send_capabilities(resource, seat->capabilities);
}

static void
update_capabilities(struct seat *seat, uint32_t capabilities)
{
	struct wl_resource *resource;

	if (!(~seat->capabilities & capabilities)) {
		return;
	}

	seat->capabilities |= capabilities;
	wl_list_for_each(resource, &seat->resources, link)
	    wl_seat_send_capabilities(resource, seat->capabilities);
}

static int
open_restricted(const char *path, int flags, void *user_data)
{
	return launch_open_device(path, flags);
}

static void
close_restricted(int fd, void *user_data)
{
	close(fd);
}

const struct libinput_interface libinput_interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static uint32_t
device_capabilities(struct libinput_device *device)
{
	uint32_t capabilities = 0;

	if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
		capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER)) {
		capabilities |= WL_SEAT_CAPABILITY_POINTER;
	}
	/* TODO: Add touch device support
	 * if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TOUCH))
	 * 	capabilities |= WL_SEAT_CAPABILITY_TOUCH;
	 */

	return capabilities;
}

static int
handle_libinput_data(int fd, uint32_t mask, void *data)
{
	struct seat *seat = data;
	struct screen *screen;
	struct swc_rectangle *rect;
	struct libinput_event *generic_event;
	struct libinput_device *device;
	union {
		struct libinput_event_keyboard *k;
		struct libinput_event_pointer *p;
	} event;
	wl_fixed_t x, y, value;
	uint32_t time, key, state;
	enum wl_pointer_axis_source source;
	int value120;

	if (libinput_dispatch(seat->libinput) != 0) {
		WARNING("libinput_dispatch failed: %s\n", strerror(errno));
		return 0;
	}

	while ((generic_event = libinput_get_event(seat->libinput))) {
		switch (libinput_event_get_type(generic_event)) {
		case LIBINPUT_EVENT_DEVICE_ADDED:
			device = libinput_event_get_device(generic_event);
			update_capabilities(seat, device_capabilities(device));
			if (swc.manager->new_device) {
				swc.manager->new_device(device);
			}
			break;
		case LIBINPUT_EVENT_KEYBOARD_KEY:
			event.k = libinput_event_get_keyboard_event(generic_event);
			time = libinput_event_keyboard_get_time(event.k);
			key = libinput_event_keyboard_get_key(event.k);
			state = libinput_event_keyboard_get_key_state(event.k);
			keyboard_handle_key(seat->base.keyboard, time, key, state);
			break;
		case LIBINPUT_EVENT_POINTER_MOTION:
			event.p = libinput_event_get_pointer_event(generic_event);
			time = libinput_event_pointer_get_time(event.p);
			x = wl_fixed_from_double(libinput_event_pointer_get_dx(event.p));
			y = wl_fixed_from_double(libinput_event_pointer_get_dy(event.p));
			pointer_handle_relative_motion(&seat->pointer, time, x, y);
			pointer_handle_frame(&seat->pointer);
			break;
		case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
			screen = wl_container_of(swc.screens.next, screen, link);
			rect = &screen->base.geometry;
			event.p = libinput_event_get_pointer_event(generic_event);
			time = libinput_event_pointer_get_time(event.p);
			x = wl_fixed_from_double(
			    libinput_event_pointer_get_absolute_x_transformed(event.p,
			                                                      rect->width));
			y = wl_fixed_from_double(
			    libinput_event_pointer_get_absolute_y_transformed(
			        event.p, rect->height));
			pointer_handle_absolute_motion(&seat->pointer, time, x, y);
			pointer_handle_frame(&seat->pointer);
			break;
		case LIBINPUT_EVENT_POINTER_BUTTON:
			event.p = libinput_event_get_pointer_event(generic_event);
			time = libinput_event_pointer_get_time(event.p);
			key = libinput_event_pointer_get_button(event.p);
			state = libinput_event_pointer_get_button_state(event.p);
			pointer_handle_button(&seat->pointer, time, key, state);
			if (state == LIBINPUT_BUTTON_STATE_PRESSED) {
				/* qemu generates GEAR_UP/GEAR_DOWN events on scroll, so pass
				 * those through as axis events. */
				source = WL_POINTER_AXIS_SOURCE_WHEEL;
				switch (key) {
				case BTN_GEAR_DOWN:
					pointer_handle_axis(&seat->pointer, time,
					                    WL_POINTER_AXIS_VERTICAL_SCROLL, source,
					                    wl_fixed_from_int(10), 120);
					break;
				case BTN_GEAR_UP:
					pointer_handle_axis(&seat->pointer, time,
					                    WL_POINTER_AXIS_VERTICAL_SCROLL, source,
					                    wl_fixed_from_int(-10), -120);
					break;
				}
			}
			pointer_handle_frame(&seat->pointer);
			break;
		case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
			source = WL_POINTER_AXIS_SOURCE_WHEEL;
			goto scroll;
		case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
			source = WL_POINTER_AXIS_SOURCE_FINGER;
			goto scroll;
		case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
			source = WL_POINTER_AXIS_SOURCE_CONTINUOUS;
			goto scroll;
		scroll:
			event.p = libinput_event_get_pointer_event(generic_event);
			time = libinput_event_pointer_get_time(event.p);
			value120 = 0;
			if (libinput_event_pointer_has_axis(
			        event.p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
				value = wl_fixed_from_double(
				    libinput_event_pointer_get_scroll_value(
				        event.p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL));
				if (source == WL_POINTER_AXIS_SOURCE_WHEEL) {
					value120 = libinput_event_pointer_get_scroll_value_v120(
					    event.p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
				}
				pointer_handle_axis(&seat->pointer, time,
				                    WL_POINTER_AXIS_VERTICAL_SCROLL, source,
				                    value, value120);
			}
			if (libinput_event_pointer_has_axis(
			        event.p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
				value = wl_fixed_from_double(
				    libinput_event_pointer_get_scroll_value(
				        event.p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL));
				if (source == WL_POINTER_AXIS_SOURCE_WHEEL) {
					value120 = libinput_event_pointer_get_scroll_value_v120(
					    event.p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
				}
				pointer_handle_axis(&seat->pointer, time,
				                    WL_POINTER_AXIS_HORIZONTAL_SCROLL, source,
				                    value, value120);
			}
			pointer_handle_frame(&seat->pointer);
			break;
		default:
			break;
		}

		libinput_event_destroy(generic_event);
	}

	return 0;
}

bool
initialize_libinput(struct seat *seat)
{
#ifdef ENABLE_LIBUDEV
	if (!(seat->udev = udev_new())) {
		ERROR("Could not create udev context\n");
		goto error0;
	}

	seat->libinput =
	    libinput_udev_create_context(&libinput_interface, NULL, seat->udev);
#else
	seat->libinput = libinput_netlink_create_context(&libinput_interface, NULL,
	                                                 NETLINK_MASK);
#endif

	if (!seat->libinput) {
		ERROR("Could not create libinput context\n");
		goto error1;
	}

#ifdef ENABLE_LIBUDEV
	if (libinput_udev_assign_seat(seat->libinput, seat->name) != 0) {
		ERROR("Failed to assign seat to libinput context\n");
		goto error2;
	}
#else
	if (libinput_netlink_assign_seat(seat->libinput, seat->name) != 0) {
		ERROR("Failed to assign seat to libinput context\n");
		goto error2;
	}
#endif

	seat->libinput_source =
	    wl_event_loop_add_fd(swc.event_loop, libinput_get_fd(seat->libinput),
	                         WL_EVENT_READABLE, &handle_libinput_data, seat);
	if (!seat->libinput_source) {
		ERROR("Could not create event source for libinput\n");
		goto error2;
	}

	if (!swc.active) {
		libinput_suspend(seat->libinput);
	}

	return true;

error2:
	libinput_unref(seat->libinput);
error1:
#ifdef ENABLE_LIBUDEV
	udev_unref(seat->udev);
error0:
#endif
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
	seat->name = strdup(seat_name);
	if (!seat->name) {
		ERROR("Could not allocate seat name string\n");
		goto error1;
	}
	seat->global =
	    wl_global_create(display, &wl_seat_interface, 8, seat, &bind_seat);
	if (!seat->global) {
		goto error2;
	}
	seat->capabilities = 0;
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

	seat->base.keyboard = keyboard_create(NULL);
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

	if (!initialize_libinput(seat)) {
		goto error6;
	}

	return &seat->base;

error6:
	pointer_finalize(&seat->pointer);
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

	wl_event_source_remove(seat->libinput_source);
	libinput_unref(seat->libinput);
#ifdef ENABLE_LIBUDEV
	udev_unref(seat->udev);
#endif

	pointer_finalize(&seat->pointer);
	keyboard_destroy(seat->base.keyboard);
	data_device_destroy(seat->base.data_device);

	wl_global_destroy(seat->global);
	free(seat->name);
	free(seat);
}
