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

	// drag and drop: what the source allows, what the target took, and who to tell when it ends
	uint32_t source_actions;
	uint32_t offer_actions, preferred_action, action;
	bool accepted;
	bool dropped;
	void (*finished)(void *context);
	void *finished_context;
};

static uint32_t
choose_action(struct data *data)
{
	uint32_t both = data->source_actions & data->offer_actions;

	if (both & data->preferred_action)
		return data->preferred_action;
	if (both & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
		return WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
	if (both & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
		return WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
	if (both & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
		return WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
	return WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
}

// tells the source and the newest offer which action a drop would perform
static void
update_action(struct data *data, struct wl_resource *offer)
{
	uint32_t action = choose_action(data);

	if (action == data->action)
		return;
	data->action = action;
	if (wl_resource_get_version(data->source) >= WL_DATA_SOURCE_ACTION_SINCE_VERSION)
		wl_data_source_send_action(data->source, action);
	if (offer && wl_resource_get_version(offer) >= WL_DATA_OFFER_ACTION_SINCE_VERSION)
		wl_data_offer_send_action(offer, action);
}

static void
offer_accept(struct wl_client *client, struct wl_resource *offer,
             uint32_t serial, const char *mime_type)
{
	struct data *data = wl_resource_get_user_data(offer);

	/* Protect against expired data_offers being used. */
	if (!data) {
		return;
	}

	data->accepted = mime_type != NULL;
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

// the target read what it wanted from a drop, the source may now delete moved data
static void
offer_finish(struct wl_client *client, struct wl_resource *offer)
{
	struct data *data = wl_resource_get_user_data(offer);

	(void)client;
	if (!data || !data->dropped)
		return;
	if (wl_resource_get_version(data->source) >= WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION)
		wl_data_source_send_dnd_finished(data->source);
	data->dropped = false;
	if (data->finished)
		data->finished(data->finished_context);
}

static void
offer_set_actions(struct wl_client *client, struct wl_resource *offer, uint32_t dnd_actions, uint32_t preferred_action)
{
	struct data *data = wl_resource_get_user_data(offer);

	(void)client;
	if (!data)
		return;
	data->offer_actions = dnd_actions;
	data->preferred_action = preferred_action;
	update_action(data, offer);
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
	struct data *data = wl_resource_get_user_data(resource);

	(void)client;
	data->source_actions = dnd_actions;
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
	data->source_actions = 0;
	data->offer_actions = 0;
	data->preferred_action = 0;
	data->action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
	data->accepted = false;
	data->dropped = false;
	data->finished = NULL;
	data->finished_context = NULL;

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

// a drag offer starts with nothing accepted, and the source's actions announced to the target
void
data_offer_start_drag(struct wl_resource *offer)
{
	struct data *data = wl_resource_get_user_data(offer);

	data->accepted = false;
	data->offer_actions = 0;
	data->preferred_action = 0;
	data->action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
	if (wl_resource_get_version(offer) >= WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION)
		wl_data_offer_send_source_actions(offer, data->source_actions);
}

// whether a drop now would go through: a type taken and, from version 3, an action agreed
bool
data_source_drop_ok(struct wl_resource *source)
{
	struct data *data = wl_resource_get_user_data(source);

	if (!data->accepted)
		return false;
	if (wl_resource_get_version(source) >= WL_DATA_SOURCE_ACTION_SINCE_VERSION)
		return data->action != WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
	return true;
}

void
data_source_set_dropped(struct wl_resource *source, void (*finished)(void *context), void *context)
{
	struct data *data = wl_resource_get_user_data(source);

	data->dropped = true;
	data->finished = finished;
	data->finished_context = context;
}

void
data_source_clear_finished(struct wl_resource *source)
{
	struct data *data = wl_resource_get_user_data(source);

	data->finished = NULL;
	data->finished_context = NULL;
}
