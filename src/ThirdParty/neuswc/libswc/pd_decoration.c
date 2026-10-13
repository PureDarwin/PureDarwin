/*
 * pd_decoration.c - title strips for windows that asked for server side
 * decorations, drawn by a client (the decorator) with AppKit's own frame code.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include "pd_decoration.h"
#include "compositor.h"
#include "event.h"
#include "input.h"
#include "internal.h"
#include "keyboard.h"
#include "pointer.h"
#include "screen.h"
#include "seat.h"
#include "util.h"
#include "view.h"
#include "wayland_buffer.h"
#include "window.h"

#include "pd-decoration-v1-server-protocol.h"
#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <wld/wld.h>

static struct {
	struct wl_global *global;
	struct wl_resource *decorator;
	struct wl_list windows;
	uint32_t next_id;
	struct wl_listener focus_listener;
	bool focus_listening;
} deco;

static bool
mapped(struct window *window)
{
	return window->frame.wanted && window->has_buffer && window->view->visible;
}

static void
send_state(struct window *window)
{
	struct compositor_view *focus = swc.seat->keyboard->focus.view;

	if (!deco.decorator || !mapped(window))
		return;
	pd_decorator_v1_send_title(deco.decorator, window->frame.id, window->base.title ? window->base.title : "");
	pd_decorator_v1_send_width(deco.decorator, window->frame.id, window->view->base.geometry.width);
	pd_decorator_v1_send_active(deco.decorator, window->frame.id, focus == window->view);
	pd_decorator_v1_send_done(deco.decorator, window->frame.id);
}

static void
send_window(struct window *window)
{
	if (!deco.decorator || !mapped(window))
		return;
	pd_decorator_v1_send_window(deco.decorator, window->frame.id);
	send_state(window);
}

static struct window *
window_for_id(uint32_t id)
{
	struct window *window;

	wl_list_for_each(window, &deco.windows, frame.link) {
		if (window->frame.id == id)
			return window;
	}
	return NULL;
}

// the strip goes away with the decorator, the window keeps working without one
static void
drop_strip(struct window *window)
{
	window->frame.height = 0;
	compositor_view_set_decor(window->view, NULL);
}

static void
handle_focus(struct wl_listener *listener, void *data)
{
	struct event *event = data;
	struct input_focus_event_data *focus = event->data;

	if (event->type != INPUT_FOCUS_EVENT_CHANGED)
		return;
	if (focus->old && focus->old->window && focus->old->window->frame.wanted)
		send_state(focus->old->window);
	if (focus->new && focus->new->window && focus->new->window->frame.wanted)
		send_state(focus->new->window);
}

// a window that is not on screen yet is pushed down so its strip clears the bars
static void
keep_strip_visible(struct window *window)
{
	struct swc_rectangle *geom = &window->view->base.geometry;
	struct screen *screen;

	if (wl_list_empty(&swc.screens))
		return;
	screen = wl_container_of(swc.screens.next, screen, link);
	if (geom->y - (int32_t)window->frame.height < screen->base.usable_geometry.y)
		view_move(&window->view->base, geom->x, screen->base.usable_geometry.y + (int32_t)window->frame.height);
}

static void
set_frame(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *buffer_resource,
          uint32_t height, uint32_t button_y, uint32_t button_size, uint32_t close_x, uint32_t mini_x, uint32_t zoom_x)
{
	struct window *window = window_for_id(id);
	struct wld_buffer *buffer = wayland_buffer_get(buffer_resource);
	struct swc_decor_parts parts;
	struct swc_decor decor;

	if (resource != deco.decorator || !window || !buffer || !height || buffer->format != WLD_FORMAT_ARGB8888 ||
	    buffer->height < height || !wld_map(buffer)) {
		wl_buffer_send_release(buffer_resource);
		return;
	}

	// the strip is the top left part, as wide as the window, the decor copies the pixels
	memset(&parts, 0, sizeof(parts));
	memset(&decor, 0, sizeof(decor));
	parts.top_left.width = buffer->width;
	parts.top_left.height = height;
	parts.top_left.stride = buffer->pitch;
	parts.top_left.data = buffer->map;
	decor.top = height;
	decor.parts = &parts;
	compositor_view_set_decor(window->view, &decor);
	wld_unmap(buffer);
	wl_buffer_send_release(buffer_resource);

	window->frame.height = height;
	window->frame.button_y = button_y;
	window->frame.button_size = button_size;
	window->frame.close_x = close_x;
	window->frame.mini_x = mini_x;
	window->frame.zoom_x = zoom_x;
	keep_strip_visible(window);
}

static void
destroy_decorator(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct pd_decorator_v1_interface decorator_impl = {
    .destroy = destroy_decorator,
    .set_frame = set_frame,
};

static void
decorator_gone(struct wl_resource *resource)
{
	struct window *window;

	if (resource != deco.decorator)
		return;
	deco.decorator = NULL;
	wl_list_for_each(window, &deco.windows, frame.link)
		drop_strip(window);
}

static void
bind_decorator(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;
	struct window *window;

	resource = wl_resource_create(client, &pd_decorator_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &decorator_impl, NULL, &decorator_gone);
	// one decorator at a time, a second one is inert
	if (deco.decorator)
		return;
	deco.decorator = resource;
	if (!deco.focus_listening && swc.seat && swc.seat->keyboard) {
		deco.focus_listener.notify = &handle_focus;
		wl_signal_add(&swc.seat->keyboard->focus.event_signal, &deco.focus_listener);
		deco.focus_listening = true;
	}
	// windows that were up before the decorator started
	wl_list_for_each(window, &deco.windows, frame.link)
		send_window(window);
}

struct wl_global *
pd_decoration_create(struct wl_display *display)
{
	wl_list_init(&deco.windows);
	deco.global = wl_global_create(display, &pd_decorator_v1_interface, 1, NULL, &bind_decorator);
	return deco.global;
}

void
pd_decoration_window_wanted(struct window *window)
{
	if (window->frame.wanted)
		return;
	window->frame.wanted = true;
	window->frame.id = ++deco.next_id;
	wl_list_insert(&deco.windows, &window->frame.link);
	send_window(window);
}

void
pd_decoration_window_mapped(struct window *window)
{
	if (window->frame.wanted)
		send_window(window);
}

void
pd_decoration_window_unmapped(struct window *window)
{
	if (!window->frame.wanted || !deco.decorator)
		return;
	pd_decorator_v1_send_removed(deco.decorator, window->frame.id);
}

void
pd_decoration_window_changed(struct window *window)
{
	if (window->frame.wanted)
		send_state(window);
}

void
pd_decoration_window_destroyed(struct window *window)
{
	if (!window->frame.wanted)
		return;
	if (deco.decorator)
		pd_decorator_v1_send_removed(deco.decorator, window->frame.id);
	wl_list_remove(&window->frame.link);
	window->frame.wanted = false;
}

static bool
in_square(int32_t x, int32_t y, uint32_t left, uint32_t top, uint32_t size)
{
	return x >= (int32_t)left && x < (int32_t)(left + size) && y >= (int32_t)top && y < (int32_t)(top + size);
}

// zoom fills the usable area below the bars with the strip inside it, again goes back
static void
toggle_zoom(struct window *window)
{
	struct swc_rectangle *geom = &window->view->base.geometry, full;
	struct screen *screen;

	if (window->frame.zoomed) {
		window->frame.zoomed = false;
		swc_window_set_stacked(&window->base);
		swc_window_set_geometry(&window->base, &window->frame.unzoomed);
		return;
	}
	if (wl_list_empty(&swc.screens))
		return;
	screen = wl_container_of(swc.screens.next, screen, link);
	window->frame.unzoomed = *geom;
	window->frame.zoomed = true;
	full = screen->base.usable_geometry;
	full.y += window->frame.height;
	full.height -= window->frame.height;
	swc_window_set_tiled(&window->base);
	swc_window_set_geometry(&window->base, &full);
}

void
pd_decoration_button(struct window *window, struct button *button, int32_t x, int32_t y)
{
	const struct swc_rectangle *geom = &window->view->base.geometry;
	int32_t sx = x - geom->x, sy = y - (geom->y - (int32_t)window->frame.height);
	uint32_t size = window->frame.button_size, top = window->frame.button_y;

	if (size && in_square(sx, sy, window->frame.close_x, top, size))
		swc_window_close(&window->base);
	else if (size && in_square(sx, sy, window->frame.mini_x, top, size))
		swc_window_hide(&window->base);
	else if (size && in_square(sx, sy, window->frame.zoom_x, top, size))
		toggle_zoom(window);
	else
		window_begin_move(window, button);
}
