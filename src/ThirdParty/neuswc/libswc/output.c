#include "output.h"
#ifdef ENABLE_DRM
#include "drm.h"
#endif
#include "internal.h"
#include "mode.h"
#include "screen.h"
#include "util.h"

#ifdef ENABLE_DRM
#include <drm.h>
#include <xf86drm.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct wl_output_interface output_impl = {
    .release = destroy_resource,
};

static void
bind_output(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct output *output = data;
	struct screen *screen = output->screen;
	struct mode *mode = &screen->planes.primary.mode;
	struct wl_resource *resource;
	uint32_t flags;

	resource = wl_resource_create(client, &wl_output_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	output->resource = resource;
	wl_resource_set_implementation(resource, &output_impl, output,
	                               &remove_resource);
	wl_list_insert(&output->resources, wl_resource_get_link(resource));

	wl_output_send_geometry(resource, screen->base.geometry.x,
	                        screen->base.geometry.y, output->physical_width,
	                        output->physical_height, 0, "unknown", "unknown",
	                        WL_OUTPUT_TRANSFORM_NORMAL);

	wl_array_for_each(mode, &output->modes)
	{
		flags = 0;
		if (mode->preferred) {
			flags |= WL_OUTPUT_MODE_PREFERRED;
		}
		if (mode_equal(&screen->planes.primary.mode, mode)) {
			flags |= WL_OUTPUT_MODE_CURRENT;
		}

		wl_output_send_mode(resource, flags, mode->width, mode->height,
		                    mode->refresh);
	}

	if (version >= 4) {
		wl_output_send_name(resource, output->name);
		wl_output_send_description(resource, output->name);
	}

	if (version >= 2) {
		wl_output_send_done(resource);
	}
}

#ifdef ENABLE_DRM
struct output *
output_new(drmModeConnectorPtr connector)
{
	struct output *output;
	struct mode *modes;
	const char *name;
	uint32_t i;

	if (!(output = malloc(sizeof(*output)))) {
		ERROR("Failed to allocate output\n");
		goto error0;
	}

	output->global = wl_global_create(swc.display, &wl_output_interface, 4,
	                                  output, &bind_output);

	if (!output->global) {
		ERROR("Failed to create output global\n");
		goto error1;
	}

	output->physical_width = connector->mmWidth;
	output->physical_height = connector->mmHeight;
	output->preferred_mode = NULL;

	wl_list_init(&output->resources);
	wl_array_init(&output->modes);
	pixman_region32_init(&output->current_damage);
	pixman_region32_init(&output->previous_damage);

	output->connector = connector->connector_id;

	if (connector->count_modes == 0) {
		goto error2;
	}

	modes =
	    wl_array_add(&output->modes, connector->count_modes * sizeof(*modes));
	if (!modes) {
		goto error2;
	}

	for (i = 0; i < connector->count_modes; ++i) {
		mode_initialize(&modes[i], &connector->modes[i]);

		if (modes[i].preferred) {
			output->preferred_mode = &modes[i];
		}
	}

	if (!output->preferred_mode) {
		output->preferred_mode = &modes[0];
	}

	if (!(name = drmModeGetConnectorTypeName(connector->connector_type)))
		name = "UNKNOWN";
	snprintf(output->name, sizeof(output->name), "%s-%d", name, connector->connector_type_id);

	return output;

error2:
	wl_global_destroy(output->global);
error1:
	free(output);
error0:
	return NULL;
}
#endif

struct output *
output_new_fb(uint32_t width, uint32_t height, const char *name)
{
	struct output *output;
	struct mode *mode;

	output = calloc(1, sizeof(*output));
	if (!output) {
		return NULL;
	}
	output->global = wl_global_create(swc.display, &wl_output_interface, 4,
	                                  output, &bind_output);
	if (!output->global) {
		free(output);
		return NULL;
	}
	wl_list_init(&output->resources);
	wl_array_init(&output->modes);
	pixman_region32_init(&output->current_damage);
	pixman_region32_init(&output->previous_damage);
	mode = wl_array_add(&output->modes, sizeof(*mode));
	if (!mode) {
		output_destroy(output);
		return NULL;
	}
	mode_initialize_simple(mode, width, height, 60000);
	output->preferred_mode = mode;
	snprintf(output->name, sizeof(output->name), "%s", name);
	return output;
}

void
output_destroy(struct output *output)
{
	struct wl_resource *resource, *tmp;

	wl_list_for_each_safe(resource, tmp, &output->resources, link)
		wl_resource_destroy(resource);
	pixman_region32_fini(&output->current_damage);
	pixman_region32_fini(&output->previous_damage);
	wl_array_release(&output->modes);
	wl_global_destroy(output->global);
	free(output);
}
