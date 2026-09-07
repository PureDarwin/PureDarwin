/* swc: libswc/pointer.c
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

#include "pointer.h"
#include "backend.h"
#include "compositor.h"
#include "cursor/cursor_data.h"
#include "event.h"
#include "internal.h"
#ifdef ENABLE_DRM
#include "plane.h"
#endif
#include "screen.h"
#include "seat.h"
#include "shm.h"
#include "surface.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wld/wld.h>

static enum swc_cursor_kind cursor_override = SWC_CURSOR_DEFAULT;
static enum swc_cursor_mode cursor_mode = SWC_CURSOR_MODE_CLIENT;

static struct {
	const uint32_t *data;
	uint32_t width, height;
	int32_t hotspot_x, hotspot_y;
	bool active;
} cursor_images[6];

EXPORT void
swc_pointer_send_button(uint32_t time, uint32_t button, uint32_t state)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;
	struct wl_resource *resource;
	uint32_t serial;

	if (!pointer || wl_list_empty(&pointer->focus.active)) {
		return;
	}

	serial = wl_display_next_serial(swc.display);
	wl_resource_for_each(resource, &pointer->focus.active)
	    wl_pointer_send_button(resource, serial, time, button, state);
	wl_resource_for_each(resource, &pointer->focus.active)
	{
		if (wl_resource_get_version(resource) >=
		    WL_POINTER_FRAME_SINCE_VERSION) {
			wl_pointer_send_frame(resource);
		}
	}
	pointer->client_axis_source = -1;
}

EXPORT void
swc_pointer_send_axis(uint32_t time, uint32_t axis, int32_t value120)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;
	struct wl_resource *resource;
	wl_fixed_t value;

	if (!pointer || wl_list_empty(&pointer->focus.active)) {
		return;
	}

	value = wl_fixed_from_double((double)value120 / 120.0);

	wl_resource_for_each(resource, &pointer->focus.active)
	{
		int ver = wl_resource_get_version(resource);

		if (ver >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
			wl_pointer_send_axis_source(resource, WL_POINTER_AXIS_SOURCE_WHEEL);
		}
		if (value120) {
			if (ver >= WL_POINTER_AXIS_VALUE120_SINCE_VERSION) {
				wl_pointer_send_axis_value120(resource, axis, value120);
			} else if (ver >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION) {
				wl_pointer_send_axis_discrete(resource, axis, value120 / 120);
			}
		}

		if (value) {
			wl_pointer_send_axis(resource, time, axis, value);
		} else if (ver >= WL_POINTER_AXIS_STOP_SINCE_VERSION) {
			wl_pointer_send_axis_stop(resource, time, axis);
		}
	}

	wl_resource_for_each(resource, &pointer->focus.active)
	{
		if (wl_resource_get_version(resource) >=
		    WL_POINTER_FRAME_SINCE_VERSION) {
			wl_pointer_send_frame(resource);
		}
	}
	pointer->client_axis_source = -1;
}

static void
enter(struct input_focus_handler *handler, struct wl_list *resources,
      struct compositor_view *view)
{
	struct pointer *pointer = wl_container_of(handler, pointer, focus_handler);
	struct wl_resource *resource;
	uint32_t serial;
	wl_fixed_t surface_x, surface_y;
	int32_t origin_x, origin_y;

	if (wl_list_empty(resources)) {
		pointer_set_cursor(pointer, cursor_left_ptr);
		return;
	}
	serial = wl_display_next_serial(swc.display);
	/* do based on buffer origin, holy fuck */
	origin_x = view->base.geometry.x - view->buffer_offset_x;
	origin_y = view->base.geometry.y - view->buffer_offset_y;
	surface_x = pointer->x - wl_fixed_from_int(origin_x);
	surface_y = pointer->y - wl_fixed_from_int(origin_y);
	wl_resource_for_each(resource, resources) wl_pointer_send_enter(
	    resource, serial, view->surface->resource, surface_x, surface_y);
}

static void
leave(struct input_focus_handler *handler, struct wl_list *resources,
      struct compositor_view *view)
{
	struct wl_resource *resource;
	uint32_t serial;

	serial = wl_display_next_serial(swc.display);
	wl_resource_for_each(resource, resources)
	    wl_pointer_send_leave(resource, serial, view->surface->resource);
}

static void
handle_cursor_surface_destroy(struct wl_listener *listener, void *data)
{
	struct pointer *pointer =
	    wl_container_of(listener, pointer, cursor.destroy_listener);

	view_attach(&pointer->cursor.view, NULL);
	pointer->cursor.surface = NULL;
}

static bool
update(struct view *view)
{
	view_frame(view, get_time());
	return true;
}

static int
attach(struct view *view, struct wld_buffer *buffer)
{
	struct pointer *pointer = wl_container_of(view, pointer, cursor.view);
	struct surface *surface = pointer->cursor.surface;
#ifdef ENABLE_DRM
	struct screen *screen;
#endif

	if (surface && !pixman_region32_not_empty(&surface->state.damage)) {
		return 0;
	}

	wld_set_target_buffer(swc.shm->renderer, pointer->cursor.buffer);
	wld_fill_rectangle(swc.shm->renderer, 0x00000000, 0, 0,
	                   pointer->cursor.buffer->width,
	                   pointer->cursor.buffer->height);

	if (buffer) {
		wld_copy_rectangle(swc.shm->renderer, buffer, 0, 0, 0, 0, buffer->width,
		                   buffer->height);
	}

	wld_flush(swc.shm->renderer);

	if (surface) {
		pixman_region32_clear(&surface->state.damage);
	}

	/* TODO: Send an early release to the buffer */

	if (view_set_size_from_buffer(view, buffer)) {
		view_update_screens(view);
	}

#ifdef ENABLE_DRM
	wl_list_for_each(screen, &swc.screens, link) {
		view_attach(&screen->planes.cursor->view,
		            buffer ? pointer->cursor.buffer : NULL);
		view_update(&screen->planes.cursor->view);
	}
#else
	compositor_damage_all();
#endif

	return 0;
}

static bool
move(struct view *view, int32_t x, int32_t y)
{
#ifdef ENABLE_DRM
	struct screen *screen;
#endif

	if (view_set_position(view, x, y)) {
		view_update_screens(view);
	}

#ifdef ENABLE_DRM
	wl_list_for_each(screen, &swc.screens, link) {
		view_move(&screen->planes.cursor->view, view->geometry.x,
		          view->geometry.y);
		view_update(&screen->planes.cursor->view);
	}
#else
	compositor_damage_all();
#endif

	return true;
}

static const struct view_impl view_impl = {
    .update = update,
    .attach = attach,
    .move = move,
};

static inline void
update_cursor(struct pointer *pointer)
{
	int32_t x = wl_fixed_to_int(pointer->x) - pointer->cursor.hotspot.x,
	        y = wl_fixed_to_int(pointer->y) - pointer->cursor.hotspot.y;

	view_move(&pointer->cursor.view, x, y);
}

static void
drop_client_cursor_surface(struct pointer *pointer)
{
	if (!pointer || !pointer->cursor.surface) {
		return;
	}
	surface_set_view(pointer->cursor.surface, NULL);
	wl_list_remove(&pointer->cursor.destroy_listener.link);
	pointer->cursor.surface = NULL;
}

static void
apply_cursor_override(struct pointer *pointer)
{
	if (!pointer || pointer->cursor.surface) {
		return;
	}

	pointer_set_cursor(pointer, cursor_left_ptr);
}

EXPORT void
swc_set_cursor(enum swc_cursor_kind kind)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;

	cursor_override = kind;

	drop_client_cursor_surface(pointer);

	apply_cursor_override(pointer);
}

EXPORT void
swc_set_cursor_mode(enum swc_cursor_mode mode)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;

	cursor_mode = mode;
	if (cursor_mode == SWC_CURSOR_MODE_COMPOSITOR) {
		drop_client_cursor_surface(pointer);
	}
	apply_cursor_override(pointer);
}

EXPORT void
swc_set_cursor_image(enum swc_cursor_kind kind, const uint32_t *argb8888,
                     uint32_t width, uint32_t height, int32_t hotspot_x,
                     int32_t hotspot_y)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;

	if (kind < 0 || kind >= (int)ARRAY_LENGTH(cursor_images)) {
		return;
	}
	if (!argb8888 || width == 0 || height == 0) {
		return;
	}

	cursor_images[kind].data = argb8888;
	cursor_images[kind].width = width;
	cursor_images[kind].height = height;
	cursor_images[kind].hotspot_x = hotspot_x;
	cursor_images[kind].hotspot_y = hotspot_y;
	cursor_images[kind].active = true;

	if (cursor_mode == SWC_CURSOR_MODE_COMPOSITOR) {
		drop_client_cursor_surface(pointer);
	}
	apply_cursor_override(pointer);
}

EXPORT void
swc_clear_cursor_image(enum swc_cursor_kind kind)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;

	if (kind < 0 || kind >= (int)ARRAY_LENGTH(cursor_images)) {
		return;
	}

	cursor_images[kind].active = false;
	cursor_images[kind].data = NULL;

	apply_cursor_override(pointer);
}

void
pointer_set_cursor(struct pointer *pointer, uint32_t id)
{
	struct cursor *cursor = &cursor_metadata[id];
	const uint32_t *data = cursor_data;
	union wld_object object = {.ptr = &cursor_data[cursor->offset]};
	struct wld_buffer *buffer;

	if (id == cursor_left_ptr) {
		enum swc_cursor_kind kind = cursor_override;
		if (kind < 0 || kind >= (int)ARRAY_LENGTH(cursor_images)) {
			kind = SWC_CURSOR_DEFAULT;
		}

		if (cursor_images[kind].active) {
			static struct cursor custom_cursor;
			custom_cursor.width = (int)cursor_images[kind].width;
			custom_cursor.height = (int)cursor_images[kind].height;
			custom_cursor.hotspot_x = (int)cursor_images[kind].hotspot_x;
			custom_cursor.hotspot_y = (int)cursor_images[kind].hotspot_y;
			custom_cursor.offset = 0;

			cursor = &custom_cursor;
			data = cursor_images[kind].data;
			object.ptr = (void *)data;
		}
	}

	if (pointer->cursor.internal_buffer) {
		wld_buffer_unreference(pointer->cursor.internal_buffer);
	}
	if (pointer->cursor.surface) {
		surface_set_view(pointer->cursor.surface, NULL);
		wl_list_remove(&pointer->cursor.destroy_listener.link);
		pointer->cursor.surface = NULL;
	}

	buffer = wld_import_buffer(swc.shm->context, WLD_OBJECT_DATA, object,
	                           cursor->width, cursor->height,
	                           WLD_FORMAT_ARGB8888, cursor->width * 4);
	if (!buffer) {
		WARNING("Failed to create cursor buffer\n");
	}
	pointer->cursor.internal_buffer = buffer;
	pointer->cursor.hotspot.x = cursor->hotspot_x;
	pointer->cursor.hotspot.y = cursor->hotspot_y;
	update_cursor(pointer);
	view_attach(&pointer->cursor.view, buffer);
}

static bool
client_handle_motion(struct pointer_handler *handler, uint32_t time,
                     wl_fixed_t x, wl_fixed_t y)
{
	struct pointer *pointer = wl_container_of(handler, pointer, client_handler);
	struct wl_resource *resource;
	wl_fixed_t sx, sy;
	int32_t origin_x, origin_y;

	if (wl_list_empty(&pointer->focus.active)) {
		return false;
	}

	origin_x = pointer->focus.view->base.geometry.x -
	           pointer->focus.view->buffer_offset_x;
	origin_y = pointer->focus.view->base.geometry.y -
	           pointer->focus.view->buffer_offset_y;
	sx = x - wl_fixed_from_int(origin_x);
	sy = y - wl_fixed_from_int(origin_y);
	wl_resource_for_each(resource, &pointer->focus.active)
	    wl_pointer_send_motion(resource, time, sx, sy);
	return true;
}

static bool
client_handle_button(struct pointer_handler *handler, uint32_t time,
                     struct button *button, uint32_t state)
{
	struct pointer *pointer = wl_container_of(handler, pointer, client_handler);
	struct wl_resource *resource;

	if (wl_list_empty(&pointer->focus.active)) {
		return false;
	}

	wl_resource_for_each(resource, &pointer->focus.active)
	    wl_pointer_send_button(resource, button->press.serial, time,
	                           button->press.value, state);
	return true;
}

static bool
client_handle_axis(struct pointer_handler *handler, uint32_t time,
                   enum wl_pointer_axis axis,
                   enum wl_pointer_axis_source source, wl_fixed_t value,
                   int value120)
{
	struct pointer *pointer = wl_container_of(handler, pointer, client_handler);
	struct wl_resource *resource;
	int ver;

	if (wl_list_empty(&pointer->focus.active)) {
		return false;
	}

	if (pointer->client_axis_source != -1) {
		assert(pointer->client_axis_source == source);
		source = -1;
	} else {
		pointer->client_axis_source = source;
	}

	wl_resource_for_each(resource, &pointer->focus.active)
	{
		ver = wl_resource_get_version(resource);
		if (source != -1 && ver >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
			wl_pointer_send_axis_source(resource, source);
		}
		if (value120) {
			if (ver >= WL_POINTER_AXIS_VALUE120_SINCE_VERSION) {
				wl_pointer_send_axis_value120(resource, axis, value120);
			} else if (ver >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION) {
				wl_pointer_send_axis_discrete(resource, axis, value120 / 120);
			}
		}
		if (value) {
			wl_pointer_send_axis(resource, time, axis, value);
		} else if (ver >= WL_POINTER_AXIS_STOP_SINCE_VERSION) {
			wl_pointer_send_axis_stop(resource, time, axis);
		}
	}
	return true;
}

static void
client_handle_frame(struct pointer_handler *handler)
{
	struct pointer *pointer = wl_container_of(handler, pointer, client_handler);
	struct wl_resource *resource;

	wl_resource_for_each(resource, &pointer->focus.active)
	{
		if (wl_resource_get_version(resource) >=
		    WL_POINTER_FRAME_SINCE_VERSION) {
			wl_pointer_send_frame(resource);
		}
	}
	pointer->client_axis_source = -1;
}

bool
pointer_initialize(struct pointer *pointer)
{
	struct screen *screen = wl_container_of(swc.screens.next, screen, link);
	struct swc_rectangle *geom = &screen->base.geometry;

	/* Center cursor in the geometry of the first screen. */
	pointer->x = wl_fixed_from_int(geom->x + geom->width / 2);
	pointer->y = wl_fixed_from_int(geom->y + geom->height / 2);
	pointer->focus_handler.enter = enter;
	pointer->focus_handler.leave = leave;
	pointer->client_handler.motion = client_handle_motion;
	pointer->client_handler.button = client_handle_button;
	pointer->client_handler.axis = client_handle_axis;
	pointer->client_handler.frame = client_handle_frame;
	pointer->client_handler.pending = false;
	pointer->client_axis_source = -1;
	wl_list_init(&pointer->handlers);
	wl_list_insert(&pointer->handlers, &pointer->client_handler.link);
	wl_array_init(&pointer->buttons);

	view_initialize(&pointer->cursor.view, &view_impl);
	pointer->cursor.surface = NULL;
	pointer->cursor.destroy_listener.notify = &handle_cursor_surface_destroy;
	pointer->cursor.buffer = wld_create_buffer(
	    swc.backend->context, swc.backend->cursor_width,
	    swc.backend->cursor_height,
	    WLD_FORMAT_ARGB8888, WLD_FLAG_MAP | WLD_FLAG_CURSOR);
	pointer->cursor.internal_buffer = NULL;

	if (!pointer->cursor.buffer) {
		return false;
	}

	pointer_set_cursor(pointer, cursor_left_ptr);

#ifdef ENABLE_DRM
	wl_list_for_each(screen, &swc.screens, link)
		if (screen->planes.cursor)
			view_attach(&screen->planes.cursor->view, pointer->cursor.buffer);
#endif

	input_focus_initialize(&pointer->focus, &pointer->focus_handler);
	pixman_region32_init(&pointer->region);

	return true;
}

void
pointer_finalize(struct pointer *pointer)
{
	input_focus_finalize(&pointer->focus);
	pixman_region32_fini(&pointer->region);
}

void
pointer_set_focus(struct pointer *pointer, struct compositor_view *view)
{
	input_focus_set(&pointer->focus, view);
}

static void
clip_position(struct pointer *pointer, wl_fixed_t fx, wl_fixed_t fy)
{
	int32_t x, y, last_x, last_y;
	pixman_box32_t box;

	x = wl_fixed_to_int(fx);
	y = wl_fixed_to_int(fy);
	last_x = wl_fixed_to_int(pointer->x);
	last_y = wl_fixed_to_int(pointer->y);

	if (!pixman_region32_contains_point(&pointer->region, x, y, NULL)) {
		if (!pixman_region32_contains_point(&pointer->region, last_x, last_y,
		                                    &box)) {
			WARNING("cursor is not in the visible screen area\n");
			pointer->x = 0;
			pointer->y = 0;
			return;
		}

		/* Do some clipping. */
		fx = wl_fixed_from_int(MAX(MIN(x, box.x2 - 1), box.x1));
		fy = wl_fixed_from_int(MAX(MIN(y, box.y2 - 1), box.y1));
	}

	pointer->x = fx;
	pointer->y = fy;
}

void
pointer_set_region(struct pointer *pointer, pixman_region32_t *region)
{
	pixman_region32_copy(&pointer->region, region);
	clip_position(pointer, pointer->x, pointer->y);
}

static void
set_cursor(struct wl_client *client, struct wl_resource *resource,
           uint32_t serial, struct wl_resource *surface_resource,
           int32_t hotspot_x, int32_t hotspot_y)
{
	struct pointer *pointer = wl_resource_get_user_data(resource);
	struct surface *surface;

	(void)serial;

	if (client != pointer->focus.client) {
		return;
	}

	/* If forcing compositor cursor, ignore client cursor surfaces. */
	if (cursor_mode == SWC_CURSOR_MODE_COMPOSITOR ||
	    cursor_override != SWC_CURSOR_DEFAULT) {
		return;
	}

	if (pointer->cursor.surface) {
		surface_set_view(pointer->cursor.surface, NULL);
		wl_list_remove(&pointer->cursor.destroy_listener.link);
	}

	surface =
	    surface_resource ? wl_resource_get_user_data(surface_resource) : NULL;
	pointer->cursor.surface = surface;
	pointer->cursor.hotspot.x = hotspot_x;
	pointer->cursor.hotspot.y = hotspot_y;

	if (surface) {
		surface_set_view(surface, &pointer->cursor.view);
		wl_resource_add_destroy_listener(surface->resource,
		                                 &pointer->cursor.destroy_listener);
		update_cursor(pointer);
	}
}

static const struct wl_pointer_interface pointer_impl = {
    .set_cursor = set_cursor,
    .release = destroy_resource,
};

static void
unbind(struct wl_resource *resource)
{
	struct pointer *pointer = wl_resource_get_user_data(resource);
	input_focus_remove_resource(&pointer->focus, resource);
}

struct wl_resource *
pointer_bind(struct pointer *pointer, struct wl_client *client,
             uint32_t version, uint32_t id)
{
	struct wl_resource *client_resource;

	client_resource =
	    wl_resource_create(client, &wl_pointer_interface, version, id);
	if (!client_resource) {
		return NULL;
	}
	wl_resource_set_implementation(client_resource, &pointer_impl, pointer,
	                               &unbind);
	input_focus_add_resource(&pointer->focus, client_resource);

	return client_resource;
}

struct button *
pointer_get_button(struct pointer *pointer, uint32_t serial)
{
	struct button *button;

	wl_array_for_each(button, &pointer->buttons)
	{
		if (button->press.serial == serial) {
			return button;
		}
	}

	return NULL;
}

void
pointer_handle_button(struct pointer *pointer, uint32_t time, uint32_t value,
                      uint32_t state)
{
	struct pointer_handler *handler;
	struct button *button;
	uint32_t serial;

	serial = wl_display_next_serial(swc.display);

	if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
		wl_array_for_each(button, &pointer->buttons)
		{
			if (button->press.value == value) {
				if (button->handler) {
					button->press.serial = serial;
					button->handler->button(button->handler, time, button,
					                        state);
					button->handler->pending = true;
				}

				array_remove(&pointer->buttons, button, sizeof(*button));
				break;
			}
		}
	} else {
		button = wl_array_add(&pointer->buttons, sizeof(*button));

		if (!button) {
			return;
		}

		button->press.value = value;
		button->press.serial = serial;
		button->handler = NULL;

		wl_list_for_each(handler, &pointer->handlers, link)
		{
			if (handler->button &&
			    handler->button(handler, time, button, state)) {
				button->handler = handler;
				handler->pending = true;
				break;
			}
		}
	}
}

void
pointer_handle_axis(struct pointer *pointer, uint32_t time,
                    enum wl_pointer_axis axis,
                    enum wl_pointer_axis_source source, wl_fixed_t value,
                    int value120)
{
	struct pointer_handler *handler;

	wl_list_for_each(handler, &pointer->handlers, link)
	{
		if (handler->axis &&
		    handler->axis(handler, time, axis, source, value, value120)) {
			handler->pending = true;
			break;
		}
	}
}

void
pointer_handle_relative_motion(struct pointer *pointer, uint32_t time,
                               wl_fixed_t dx, wl_fixed_t dy)
{
	pointer_handle_absolute_motion(pointer, time, pointer->x + dx,
	                               pointer->y + dy);
}

void
pointer_handle_absolute_motion(struct pointer *pointer, uint32_t time,
                               wl_fixed_t x, wl_fixed_t y)
{
	struct pointer_handler *handler;

	clip_position(pointer, x, y);

	wl_list_for_each(handler, &pointer->handlers, link)
	{
		if (handler->motion &&
		    handler->motion(handler, time, pointer->x, pointer->y)) {
			handler->pending = true;
			break;
		}
	}

	update_cursor(pointer);
}

void
pointer_handle_frame(struct pointer *pointer)
{
	struct pointer_handler *handler;

	wl_list_for_each(handler, &pointer->handlers, link)
	{
		if (handler->pending && handler->frame) {
			handler->frame(handler);
			handler->pending = false;
		}
	}

	update_cursor(pointer);
}
