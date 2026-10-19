/* swc: data_device.c
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

#include "data_device.h"
#include "compositor.h"
#include "data.h"
#include "event.h"
#include "internal.h"
#include "pointer.h"
#include "seat.h"
#include "surface.h"
#include "util.h"

static struct wl_resource *
new_offer(struct wl_resource *resource, struct wl_client *client,
          struct wl_resource *source);

// one drag at a time: the pointer belongs to it from start_drag until the button is released
struct drag {
	struct data_device *data_device;
	struct wl_client *client;
	struct wl_resource *source;
	struct wl_listener source_destroy_listener;
	struct pointer_handler handler;
	uint32_t button;

	struct compositor_view *icon;
	struct surface *icon_surface;
	struct wl_listener icon_destroy_listener;
	int32_t icon_x, icon_y;

	struct compositor_view *focus;
	struct wl_listener focus_destroy_listener;
	struct wl_resource *focus_device;
	struct wl_resource *offer;
};

// static, the pointer code still touches the handler right after its release callback ends the drag
static struct drag drag_storage;
static struct drag *current_drag;

// the dropped source waits here for the target's finish, a new drag or its own end clears it
static struct wl_resource *dropped_source;
static struct wl_listener dropped_source_destroy_listener;

static void
forget_dropped_source(void)
{
	if (!dropped_source)
		return;
	data_source_clear_finished(dropped_source);
	wl_list_remove(&dropped_source_destroy_listener.link);
	dropped_source = NULL;
}

static void
handle_dropped_source_destroy(struct wl_listener *listener, void *data)
{
	(void)listener;
	(void)data;
	wl_list_remove(&dropped_source_destroy_listener.link);
	dropped_source = NULL;
}

static void
handle_drop_finished(void *context)
{
	(void)context;
	forget_dropped_source();
}

static void
drag_leave_focus(struct drag *drag)
{
	if (!drag->focus)
		return;
	if (drag->focus_device)
		wl_data_device_send_leave(drag->focus_device);
	wl_list_remove(&drag->focus_destroy_listener.link);
	drag->focus = NULL;
	drag->focus_device = NULL;
	drag->offer = NULL;
}

static void
handle_focus_destroy(struct wl_listener *listener, void *data)
{
	struct drag *drag = wl_container_of(listener, drag, focus_destroy_listener);

	(void)data;
	wl_list_remove(&drag->focus_destroy_listener.link);
	drag->focus = NULL;
	drag->focus_device = NULL;
	drag->offer = NULL;
}

static void
surface_point(struct compositor_view *view, wl_fixed_t x, wl_fixed_t y,
              wl_fixed_t *sx, wl_fixed_t *sy)
{
	*sx = x - wl_fixed_from_int(view->base.geometry.x - view->buffer_offset_x);
	*sy = y - wl_fixed_from_int(view->base.geometry.y - view->buffer_offset_y);
}

// a client drag without a source only goes to the client's own surfaces
static void
drag_set_focus(struct drag *drag, struct compositor_view *view, wl_fixed_t x, wl_fixed_t y)
{
	struct wl_client *client;
	struct wl_resource *device;
	wl_fixed_t sx, sy;

	if (view == drag->focus)
		return;
	drag_leave_focus(drag);
	if (!view || !view->surface)
		return;
	client = wl_resource_get_client(view->surface->resource);
	if (!drag->source && client != drag->client)
		return;
	device = wl_resource_find_for_client(&drag->data_device->resources, client);
	if (!device)
		return;

	drag->focus = view;
	drag->focus_device = device;
	wl_signal_add(&view->destroy_signal, &drag->focus_destroy_listener);
	if (drag->source) {
		drag->offer = new_offer(device, client, drag->source);
		if (drag->offer)
			data_offer_start_drag(drag->offer);
	}
	surface_point(view, x, y, &sx, &sy);
	wl_data_device_send_enter(device, wl_display_next_serial(swc.display),
	                          view->surface->resource, sx, sy, drag->offer);
}

static void
drag_move_icon(struct drag *drag, wl_fixed_t x, wl_fixed_t y)
{
	if (!drag->icon)
		return;
	view_move(&drag->icon->base, wl_fixed_to_int(x) + drag->icon_x,
	          wl_fixed_to_int(y) + drag->icon_y);
}

static void
refocus_pointer(void *data)
{
	(void)data;
	pointer_handle_absolute_motion(swc.seat->pointer, get_time(),
	                               swc.seat->pointer->x, swc.seat->pointer->y);
}

static void
drag_end(struct drag *drag)
{
	drag_leave_focus(drag);
	if (drag->icon) {
		wl_list_remove(&drag->icon_destroy_listener.link);
		compositor_view_destroy(drag->icon);
	}
	if (drag->source)
		wl_list_remove(&drag->source_destroy_listener.link);
	wl_list_remove(&drag->handler.link);
	current_drag = NULL;

	// once the release is done, the pointer goes back to whatever is under it
	wl_event_loop_add_idle(swc.event_loop, refocus_pointer, NULL);
}

static bool
drag_motion(struct pointer_handler *handler, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct drag *drag = wl_container_of(handler, drag, handler);
	struct compositor_view *view;
	wl_fixed_t sx, sy;

	view = compositor_view_at(wl_fixed_to_int(x), wl_fixed_to_int(y));
	drag_set_focus(drag, view, x, y);
	drag_move_icon(drag, x, y);
	if (drag->focus_device) {
		surface_point(drag->focus, x, y, &sx, &sy);
		wl_data_device_send_motion(drag->focus_device, time, sx, sy);
	}
	return true;
}

static bool
drag_button(struct pointer_handler *handler, uint32_t time, struct button *button, uint32_t state)
{
	struct drag *drag = wl_container_of(handler, drag, handler);
	struct wl_resource *source = drag->source;

	(void)time;
	// other buttons pressed during the drag are swallowed
	if (state != WL_POINTER_BUTTON_STATE_RELEASED || button->press.value != drag->button)
		return true;

	if (drag->focus_device && (!source || data_source_drop_ok(source))) {
		wl_data_device_send_drop(drag->focus_device);
		// the drop ends the target's part, it gets no leave and keeps the offer to read from
		drag->focus_device = NULL;
		if (source) {
			if (wl_resource_get_version(source) >= WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION)
				wl_data_source_send_dnd_drop_performed(source);
			forget_dropped_source();
			dropped_source = source;
			dropped_source_destroy_listener.notify = handle_dropped_source_destroy;
			wl_resource_add_destroy_listener(source, &dropped_source_destroy_listener);
			data_source_set_dropped(source, handle_drop_finished, NULL);
		}
	} else if (source) {
		wl_data_source_send_cancelled(source);
	}
	drag_end(drag);
	return true;
}

static void
handle_source_destroy(struct wl_listener *listener, void *data)
{
	struct drag *drag = wl_container_of(listener, drag, source_destroy_listener);
	struct button *button;

	(void)data;
	wl_list_remove(&drag->source_destroy_listener.link);
	drag->source = NULL;
	wl_array_for_each(button, &swc.seat->pointer->buttons)
	{
		if (button->handler == &drag->handler)
			button->handler = NULL;
	}
	drag_end(drag);
}

static void
handle_icon_destroy(struct wl_listener *listener, void *data)
{
	struct drag *drag = wl_container_of(listener, drag, icon_destroy_listener);

	(void)data;
	wl_list_remove(&drag->icon_destroy_listener.link);
	compositor_view_destroy(drag->icon);
	drag->icon = NULL;
}

static void
start_drag(struct wl_client *client, struct wl_resource *resource,
           struct wl_resource *source_resource,
           struct wl_resource *origin_resource,
           struct wl_resource *icon_resource, uint32_t serial)
{
	struct data_device *data_device = wl_resource_get_user_data(resource);
	struct pointer *pointer = swc.seat->pointer;
	struct button *button = pointer_get_button(pointer, serial);
	struct drag *drag;

	(void)origin_resource;
	// only the client holding the button that started it may drag, and only once at a time
	if (!button || current_drag || pointer->focus.client != client) {
		if (source_resource)
			wl_data_source_send_cancelled(source_resource);
		return;
	}
	drag = &drag_storage;
	*drag = (struct drag){0};
	forget_dropped_source();
	drag->data_device = data_device;
	drag->client = client;
	drag->source = source_resource;
	drag->button = button->press.value;
	drag->focus_destroy_listener.notify = handle_focus_destroy;
	if (source_resource) {
		drag->source_destroy_listener.notify = handle_source_destroy;
		wl_resource_add_destroy_listener(source_resource, &drag->source_destroy_listener);
	}
	if (icon_resource) {
		struct surface *icon = wl_resource_get_user_data(icon_resource);

		// the icon never takes input, so the pointer finds what lies under it
		pixman_region32_clear(&icon->state.input);
		pixman_region32_clear(&icon->pending.state.input);
		if (!icon->view && (drag->icon = compositor_create_view(icon))) {
			drag->icon_surface = icon;
			// the icon's attach offset places it relative to the pointer
			drag->icon_x = icon->pending.x;
			drag->icon_y = icon->pending.y;
			drag->icon_destroy_listener.notify = handle_icon_destroy;
			wl_resource_add_destroy_listener(icon_resource, &drag->icon_destroy_listener);
			compositor_view_set_stack_layer(drag->icon, STACK_LAYER_OVERLAY, true);
			compositor_view_show(drag->icon);
		}
	}

	// the drag owns the button now, the source client sees the pointer leave
	drag->handler.motion = drag_motion;
	drag->handler.button = drag_button;
	wl_list_insert(&pointer->handlers, &drag->handler.link);
	button->handler = &drag->handler;
	pointer_set_focus(pointer, NULL);
	current_drag = drag;
	drag_motion(&drag->handler, get_time(), pointer->x, pointer->y);
}

static void
set_selection(struct wl_client *client, struct wl_resource *resource,
              struct wl_resource *data_source, uint32_t serial)
{
	struct data_device *data_device = wl_resource_get_user_data(resource);

	/* Check if this data source is already the current selection. */
	if (data_source == data_device->selection) {
		return;
	}

	if (data_device->selection) {
		wl_data_source_send_cancelled(data_device->selection);
		wl_list_remove(&data_device->selection_destroy_listener.link);
	}

	data_device->selection = data_source;

	if (data_source) {
		wl_resource_add_destroy_listener(
		    data_source, &data_device->selection_destroy_listener);
	}

	send_event(&data_device->event_signal, DATA_DEVICE_EVENT_SELECTION_CHANGED,
	           NULL);
}

static const struct wl_data_device_interface data_device_impl = {
    .start_drag = start_drag,
    .set_selection = set_selection,
    .release = destroy_resource,
};

static void
handle_selection_destroy(struct wl_listener *listener, void *data)
{
	struct data_device *data_device =
	    wl_container_of(listener, data_device, selection_destroy_listener);

	data_device->selection = NULL;
	send_event(&data_device->event_signal, DATA_DEVICE_EVENT_SELECTION_CHANGED,
	           NULL);
}

struct data_device *
data_device_create(void)
{
	struct data_device *data_device;

	data_device = malloc(sizeof(*data_device));
	if (!data_device) {
		return NULL;
	}
	data_device->selection = NULL;
	data_device->selection_destroy_listener.notify = &handle_selection_destroy;
	wl_signal_init(&data_device->event_signal);
	wl_list_init(&data_device->resources);

	return data_device;
}

void
data_device_destroy(struct data_device *data_device)
{
	struct wl_resource *resource, *tmp;

	wl_list_for_each_safe(resource, tmp, &data_device->resources, link)
	    wl_resource_destroy(resource);
	free(data_device);
}

struct wl_resource *
data_device_bind(struct data_device *data_device, struct wl_client *client,
                 uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource =
	    wl_resource_create(client, &wl_data_device_interface, version, id);
	if (!resource) {
		return NULL;
	}
	wl_resource_set_implementation(resource, &data_device_impl, data_device,
	                               &remove_resource);
	wl_list_insert(&data_device->resources, &resource->link);

	return resource;
}

static struct wl_resource *
new_offer(struct wl_resource *resource, struct wl_client *client,
          struct wl_resource *source)
{
	struct wl_resource *offer;

	offer = data_offer_new(client, source, wl_resource_get_version(resource));
	if (!offer) {
		return NULL;
	}
	wl_data_device_send_data_offer(resource, offer);
	data_send_mime_types(source, offer);

	return offer;
}

void
data_device_offer_selection(struct data_device *data_device,
                            struct wl_client *client)
{
	struct wl_resource *resource;
	struct wl_resource *offer = NULL;

	/* Look for the client's data_device resource. */
	resource = wl_resource_find_for_client(&data_device->resources, client);

	/* If the client does not have a data device, there is nothing to do. */
	if (!resource) {
		return;
	}

	/* If we have a selection, create a new offer for the client. */
	if (data_device->selection) {
		offer = new_offer(resource, client, data_device->selection);
	}

	wl_data_device_send_selection(resource, offer);
}
