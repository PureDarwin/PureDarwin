// wlr-output-management-unstable-v1 for neuswc: each screen's output is a head with the modes its framebuffer
// offers, and a configuration that picks one of them switches the display mode

#include "output_management.h"
#include "internal.h"
#include "mode.h"
#include "output.h"
#include "screen.h"
#include "util.h"

#include "wlr-output-management-unstable-v1-server-protocol.h"
#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>

struct om_client {
	struct wl_resource *resource;
	struct wl_list heads;
	struct wl_list link;
};

struct om_head {
	struct wl_resource *resource;
	struct output *output;
	struct wl_array modes; // wl_resource *, one per output mode in order
	struct om_client *client;
	struct wl_list link;
};

struct om_config_head {
	struct wl_resource *resource;
	struct output *output;
	struct mode *mode;
	uint32_t custom_width, custom_height;
	struct wl_list link;
};

struct om_config {
	struct wl_resource *resource;
	uint32_t serial;
	bool used;
	bool failed;
	struct wl_list heads;
};

static struct {
	struct wl_global *global;
	struct wl_list clients;
	uint32_t serial;
} om;

static struct output *
screen_output(struct screen *screen)
{
	return wl_list_empty(&screen->outputs) ? NULL : wl_container_of(screen->outputs.next, (struct output *)NULL, link);
}

static void
resource_destroy(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	wl_resource_destroy(resource);
}

static const struct zwlr_output_mode_v1_interface mode_impl = {
    .release = resource_destroy,
};

static void
head_destroyed(struct wl_resource *resource)
{
	struct om_head *head = wl_resource_get_user_data(resource);
	struct wl_resource **mode;

	if (!head)
		return;
	wl_array_for_each(mode, &head->modes)
	{
		if (*mode)
			wl_resource_set_user_data(*mode, NULL);
	}
	wl_array_release(&head->modes);
	wl_list_remove(&head->link);
	free(head);
}

static const struct zwlr_output_head_v1_interface head_impl = {
    .release = resource_destroy,
};

// the mode resource a client holds for one of the output's modes
static struct wl_resource *
head_mode_resource(struct om_head *head, const struct mode *wanted)
{
	struct wl_resource **resource = head->modes.data;
	struct mode *mode;
	size_t i = 0;

	wl_array_for_each(mode, &head->output->modes)
	{
		if (mode_equal(mode, wanted))
			return resource[i];
		i++;
	}
	return NULL;
}

static void
send_head_state(struct om_head *head)
{
	struct screen *screen = head->output->screen;
	struct wl_resource *current = head_mode_resource(head, &screen->planes.primary.mode);

	zwlr_output_head_v1_send_enabled(head->resource, 1);
	if (current)
		zwlr_output_head_v1_send_current_mode(head->resource, current);
	zwlr_output_head_v1_send_position(head->resource, screen->base.geometry.x, screen->base.geometry.y);
	zwlr_output_head_v1_send_transform(head->resource, WL_OUTPUT_TRANSFORM_NORMAL);
	zwlr_output_head_v1_send_scale(head->resource, wl_fixed_from_int(1));
}

static void
send_head(struct om_client *client, struct output *output)
{
	struct wl_client *wl = wl_resource_get_client(client->resource);
	uint32_t version = wl_resource_get_version(client->resource);
	struct om_head *head = calloc(1, sizeof(*head));
	struct mode *mode;

	if (!head)
		return;
	head->resource = wl_resource_create(wl, &zwlr_output_head_v1_interface, version, 0);
	if (!head->resource) {
		free(head);
		return;
	}
	head->output = output;
	head->client = client;
	wl_array_init(&head->modes);
	wl_list_insert(&client->heads, &head->link);
	wl_resource_set_implementation(head->resource, &head_impl, head, &head_destroyed);
	zwlr_output_manager_v1_send_head(client->resource, head->resource);
	zwlr_output_head_v1_send_name(head->resource, output->name);
	zwlr_output_head_v1_send_description(head->resource, output->name);
	if (output->physical_width && output->physical_height)
		zwlr_output_head_v1_send_physical_size(head->resource, output->physical_width, output->physical_height);
	wl_array_for_each(mode, &output->modes)
	{
		struct wl_resource **slot = wl_array_add(&head->modes, sizeof(*slot));
		struct wl_resource *resource = wl_resource_create(wl, &zwlr_output_mode_v1_interface, version < 3 ? version : 3, 0);

		if (!slot)
			break;
		*slot = resource;
		if (!resource)
			continue;
		wl_resource_set_implementation(resource, &mode_impl, mode, NULL);
		zwlr_output_head_v1_send_mode(head->resource, resource);
		zwlr_output_mode_v1_send_size(resource, mode->width, mode->height);
		zwlr_output_mode_v1_send_refresh(resource, mode->refresh);
		if (mode->preferred)
			zwlr_output_mode_v1_send_preferred(resource);
	}
	if (version >= 2) {
		zwlr_output_head_v1_send_make(head->resource, "PureDarwin");
		zwlr_output_head_v1_send_model(head->resource, output->name);
	}
	send_head_state(head);
}

#pragma mark configurations

static void
config_head_destroyed(struct wl_resource *resource)
{
	struct om_config_head *head = wl_resource_get_user_data(resource);

	if (!head)
		return;
	wl_list_remove(&head->link);
	free(head);
}

static void
config_head_set_mode(struct wl_client *client, struct wl_resource *resource, struct wl_resource *mode)
{
	struct om_config_head *head = wl_resource_get_user_data(resource);

	(void)client;
	head->mode = mode ? wl_resource_get_user_data(mode) : NULL;
	head->custom_width = head->custom_height = 0;
}

static void
config_head_set_custom_mode(struct wl_client *client, struct wl_resource *resource, int32_t width,
                            int32_t height, int32_t refresh)
{
	struct om_config_head *head = wl_resource_get_user_data(resource);

	(void)client;
	(void)refresh;
	head->mode = NULL;
	head->custom_width = width > 0 ? (uint32_t)width : 0;
	head->custom_height = height > 0 ? (uint32_t)height : 0;
}

static void
config_head_ignore_position(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
	(void)client;
	(void)resource;
	(void)x;
	(void)y;
}

static void
config_head_ignore_transform(struct wl_client *client, struct wl_resource *resource, int32_t transform)
{
	(void)client;
	(void)resource;
	(void)transform;
}

static void
config_head_ignore_scale(struct wl_client *client, struct wl_resource *resource, wl_fixed_t scale)
{
	(void)client;
	(void)resource;
	(void)scale;
}

static void
config_head_ignore_adaptive_sync(struct wl_client *client, struct wl_resource *resource, uint32_t state)
{
	(void)client;
	(void)resource;
	(void)state;
}

static const struct zwlr_output_configuration_head_v1_interface config_head_impl = {
    .set_mode = config_head_set_mode,
    .set_custom_mode = config_head_set_custom_mode,
    .set_position = config_head_ignore_position,
    .set_transform = config_head_ignore_transform,
    .set_scale = config_head_ignore_scale,
    .set_adaptive_sync = config_head_ignore_adaptive_sync,
};

static void
config_destroyed(struct wl_resource *resource)
{
	struct om_config *config = wl_resource_get_user_data(resource);
	struct om_config_head *head, *next;

	wl_list_for_each_safe(head, next, &config->heads, link)
	{
		wl_resource_set_user_data(head->resource, NULL);
		wl_list_remove(&head->link);
		free(head);
	}
	free(config);
}

static void
config_enable_head(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *head_resource)
{
	struct om_config *config = wl_resource_get_user_data(resource);
	struct om_head *head = wl_resource_get_user_data(head_resource);
	struct om_config_head *config_head = calloc(1, sizeof(*config_head));

	if (!config_head) {
		wl_client_post_no_memory(client);
		return;
	}
	config_head->resource = wl_resource_create(client, &zwlr_output_configuration_head_v1_interface,
	                                           wl_resource_get_version(resource), id);
	if (!config_head->resource) {
		free(config_head);
		wl_client_post_no_memory(client);
		return;
	}
	config_head->output = head ? head->output : NULL;
	wl_list_insert(&config->heads, &config_head->link);
	wl_resource_set_implementation(config_head->resource, &config_head_impl, config_head, &config_head_destroyed);
	if (!head)
		config->failed = true;
}

// the only screen there is cannot be switched off
static void
config_disable_head(struct wl_client *client, struct wl_resource *resource, struct wl_resource *head)
{
	struct om_config *config = wl_resource_get_user_data(resource);

	(void)client;
	(void)head;
	config->failed = true;
}

// the mode a head asks for, a custom size has to be one the framebuffer already offers
static struct mode *
wanted_mode(struct om_config_head *head)
{
	struct mode *mode;

	if (head->mode || !head->custom_width)
		return head->mode;
	wl_array_for_each(mode, &head->output->modes)
	{
		if (mode->width == head->custom_width && mode->height == head->custom_height)
			return mode;
	}
	return NULL;
}

static void
config_finish(struct wl_resource *resource, bool apply)
{
	struct om_config *config = wl_resource_get_user_data(resource);
	struct om_config_head *head;
	bool ok = !config->failed;

	if (config->used) {
		wl_resource_post_error(resource, ZWLR_OUTPUT_CONFIGURATION_V1_ERROR_ALREADY_USED, "configuration already used");
		return;
	}
	config->used = true;
	if (config->serial != om.serial) {
		zwlr_output_configuration_v1_send_cancelled(resource);
		return;
	}
	wl_list_for_each(head, &config->heads, link)
	{
		if (!head->output || ((head->mode || head->custom_width) && !wanted_mode(head)))
			ok = false;
	}
	if (ok && apply) {
		wl_list_for_each(head, &config->heads, link)
		{
			struct mode *mode = wanted_mode(head);

			if (mode && !mode_equal(mode, &head->output->screen->planes.primary.mode) &&
			    !screen_set_mode(head->output->screen, mode))
				ok = false;
		}
	}
	if (ok)
		zwlr_output_configuration_v1_send_succeeded(resource);
	else
		zwlr_output_configuration_v1_send_failed(resource);
}

static void
config_apply(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	config_finish(resource, true);
}

static void
config_test(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	config_finish(resource, false);
}

static const struct zwlr_output_configuration_v1_interface config_impl = {
    .enable_head = config_enable_head,
    .disable_head = config_disable_head,
    .apply = config_apply,
    .test = config_test,
    .destroy = resource_destroy,
};

#pragma mark manager

static void
manager_create_configuration(struct wl_client *client, struct wl_resource *resource, uint32_t id, uint32_t serial)
{
	struct om_config *config = calloc(1, sizeof(*config));

	if (!config) {
		wl_client_post_no_memory(client);
		return;
	}
	config->resource = wl_resource_create(client, &zwlr_output_configuration_v1_interface,
	                                      wl_resource_get_version(resource), id);
	if (!config->resource) {
		free(config);
		wl_client_post_no_memory(client);
		return;
	}
	config->serial = serial;
	wl_list_init(&config->heads);
	wl_resource_set_implementation(config->resource, &config_impl, config, &config_destroyed);
}

static void
manager_stop(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	zwlr_output_manager_v1_send_finished(resource);
	wl_resource_destroy(resource);
}

static const struct zwlr_output_manager_v1_interface manager_impl = {
    .create_configuration = manager_create_configuration,
    .stop = manager_stop,
};

static void
manager_destroyed(struct wl_resource *resource)
{
	struct om_client *client = wl_resource_get_user_data(resource);
	struct om_head *head, *next;

	wl_list_for_each_safe(head, next, &client->heads, link)
	{
		wl_resource_set_user_data(head->resource, NULL);
		wl_array_release(&head->modes);
		wl_list_remove(&head->link);
		free(head);
	}
	wl_list_remove(&client->link);
	free(client);
}

static void
bind_manager(struct wl_client *wl, void *data, uint32_t version, uint32_t id)
{
	struct om_client *client = calloc(1, sizeof(*client));
	struct screen *screen;

	(void)data;
	if (!client) {
		wl_client_post_no_memory(wl);
		return;
	}
	client->resource = wl_resource_create(wl, &zwlr_output_manager_v1_interface, version, id);
	if (!client->resource) {
		free(client);
		wl_client_post_no_memory(wl);
		return;
	}
	wl_list_init(&client->heads);
	wl_list_insert(&om.clients, &client->link);
	wl_resource_set_implementation(client->resource, &manager_impl, client, &manager_destroyed);
	wl_list_for_each(screen, &swc.screens, link)
	{
		struct output *output = screen_output(screen);

		if (output)
			send_head(client, output);
	}
	zwlr_output_manager_v1_send_done(client->resource, om.serial);
}

bool
output_management_create(struct wl_display *display)
{
	wl_list_init(&om.clients);
	om.serial = 1;
	om.global = wl_global_create(display, &zwlr_output_manager_v1_interface, 4, NULL, &bind_manager);
	return om.global != NULL;
}

void
output_management_changed(void)
{
	struct om_client *client;
	struct om_head *head;

	if (!om.global)
		return;
	om.serial++;
	wl_list_for_each(client, &om.clients, link)
	{
		wl_list_for_each(head, &client->heads, link)
			send_head_state(head);
		zwlr_output_manager_v1_send_done(client->resource, om.serial);
	}
}
