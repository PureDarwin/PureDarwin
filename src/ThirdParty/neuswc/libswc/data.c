/* swc: data.c
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

#include "data.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>

struct data {
	struct wl_array mime_types;
	struct wl_resource *source;
	struct wl_list offers;
};

static void
offer_accept(struct wl_client *client, struct wl_resource *offer,
             uint32_t serial, const char *mime_type)
{
	struct data *data = wl_resource_get_user_data(offer);

	/* Protect against expired data_offers being used. */
	if (!data) {
		return;
	}

	wl_data_source_send_target(data->source, mime_type);
}

static void
offer_receive(struct wl_client *client, struct wl_resource *offer,
              const char *mime_type, int fd)
{
	struct data *data = wl_resource_get_user_data(offer);

	/* Protect against expired data_offers being used. */
	if (!data) {
		return;
	}

	wl_data_source_send_send(data->source, mime_type, fd);
	close(fd);
}

static void
offer_finish(struct wl_client *client, struct wl_resource *offer)
{
	(void)client;
	(void)offer;
	/* TODO: Implement */
}

static void
offer_set_actions(struct wl_client *client, struct wl_resource *offer, uint32_t dnd_actions, uint32_t preferred_action)
{
	(void)client;
	(void)offer;
	(void)dnd_actions;
	(void)preferred_action;
	/* TODO: Implement */
}

static const struct wl_data_offer_interface data_offer_impl = {
	.accept = offer_accept,
	.receive = offer_receive,
	.destroy = destroy_resource,
	.finish = offer_finish,
	.set_actions = offer_set_actions,
};

static void
source_offer(struct wl_client *client, struct wl_resource *source,
             const char *mime_type)
{
	struct data *data = wl_resource_get_user_data(source);
	char *s, **dst;

	s = strdup(mime_type);
	if (!s) {
		goto error0;
	}
	dst = wl_array_add(&data->mime_types, sizeof(*dst));
	if (!dst) {
		goto error1;
	}
	*dst = s;
	return;

error1:
	free(s);
error0:
	wl_resource_post_no_memory(source);
}

static void
source_set_actions(struct wl_client *client, struct wl_resource *resource, uint32_t dnd_actions)
{
	(void)client;
	(void)resource;
	(void)dnd_actions;
	/* TODO: Implement */
}

static const struct wl_data_source_interface data_source_impl = {
	.offer = source_offer,
	.destroy = destroy_resource,
	.set_actions = source_set_actions,
};

static void
data_destroy(struct wl_resource *source)
{
	struct data *data = wl_resource_get_user_data(source);
	struct wl_resource *offer;
	char **mime_type;

	wl_array_for_each(mime_type, &data->mime_types) free(*mime_type);
	wl_array_release(&data->mime_types);

	/* After this data_source is destroyed, each of the data_offer objects
	 * associated with the data_source has a pointer to a free'd struct. We
	 * can't destroy the resources because this results in a segfault on the
	 * client when it correctly tries to call data_source.destroy. However, a
	 * misbehaving client could still attempt to call accept or receive on the
	 * data_offer, which would crash the server.
	 *
	 * So, we clear the user data on each of the offers to protect us. */
	wl_resource_for_each(offer, &data->offers)
	{
		wl_resource_set_user_data(offer, NULL);
		wl_resource_set_destructor(offer, NULL);
	}

	free(data);
}

struct wl_resource *
data_source_new(struct wl_client *client, uint32_t version, uint32_t id)
{
	struct data *data;

	data = malloc(sizeof(*data));
	if (!data) {
		goto error0;
	}
	wl_array_init(&data->mime_types);
	wl_list_init(&data->offers);

	data->source =
	    wl_resource_create(client, &wl_data_source_interface, version, id);
	if (!data->source) {
		goto error1;
	}
	wl_resource_set_implementation(data->source, &data_source_impl, data,
	                               &data_destroy);

	return data->source;

error1:
	free(data);
error0:
	return NULL;
}

struct wl_resource *
data_offer_new(struct wl_client *client, struct wl_resource *source,
               uint32_t version)
{
	struct data *data = wl_resource_get_user_data(source);
	struct wl_resource *offer;

	offer = wl_resource_create(client, &wl_data_offer_interface, version, 0);
	if (!offer) {
		return NULL;
	}
	wl_resource_set_implementation(offer, &data_offer_impl, data,
	                               &remove_resource);
	wl_list_insert(&data->offers, wl_resource_get_link(offer));

	return offer;
}

void
data_send_mime_types(struct wl_resource *source, struct wl_resource *offer)
{
	struct data *data = wl_resource_get_user_data(source);
	char **mime_type;

	wl_array_for_each(mime_type, &data->mime_types)
	    wl_data_offer_send_offer(offer, *mime_type);
}
