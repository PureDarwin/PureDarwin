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
#include "data.h"
#include "event.h"
#include "util.h"

static void
start_drag(struct wl_client *client, struct wl_resource *resource,
           struct wl_resource *source_resource,
           struct wl_resource *origin_resource,
           struct wl_resource *icon_resource, uint32_t serial)
{
	/* XXX: Implement */
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
