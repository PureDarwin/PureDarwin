#include "background_effect.h"
#include "surface.h"
#include "util.h"

#include "ext-background-effect-v1-server-protocol.h"
#include <wayland-server.h>
#include <pixman.h>
#include <stdlib.h>

// one effect object per surface, set_blur_region is double buffered and lands on the next commit
struct background_effect {
	struct wl_resource *resource;
	struct surface *surface;
	struct wl_listener surface_destroy;
};

static void
destroy_effect(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
set_blur_region(struct wl_client *client, struct wl_resource *resource,
                struct wl_resource *region_resource)
{
	struct background_effect *effect = wl_resource_get_user_data(resource);

	if (!effect->surface) {
		wl_resource_post_error(resource, EXT_BACKGROUND_EFFECT_SURFACE_V1_ERROR_SURFACE_DESTROYED,
		                       "the surface is gone");
		return;
	}
	effect->surface->pending.commit |= SURFACE_COMMIT_BLUR;
	if (region_resource) {
		pixman_region32_t *region = wl_resource_get_user_data(region_resource);
		pixman_region32_copy(&effect->surface->pending.state.blur, region);
	} else {
		pixman_region32_clear(&effect->surface->pending.state.blur);
	}
}

static const struct ext_background_effect_surface_v1_interface effect_impl = {
	.destroy = destroy_effect,
	.set_blur_region = set_blur_region,
};

// destroying the effect object drops the blur on the next commit, as the protocol says
static void
effect_resource_destroyed(struct wl_resource *resource)
{
	struct background_effect *effect = wl_resource_get_user_data(resource);

	if (effect->surface) {
		effect->surface->pending.commit |= SURFACE_COMMIT_BLUR;
		pixman_region32_clear(&effect->surface->pending.state.blur);
		wl_list_remove(&effect->surface_destroy.link);
	}
	free(effect);
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct background_effect *effect = wl_container_of(listener, effect, surface_destroy);

	effect->surface = NULL;
}

static void
get_background_effect(struct wl_client *client, struct wl_resource *resource,
                      uint32_t id, struct wl_resource *surface_resource)
{
	struct surface *surface = wl_resource_get_user_data(surface_resource);
	struct background_effect *effect = calloc(1, sizeof(*effect));

	if (!effect) {
		wl_resource_post_no_memory(resource);
		return;
	}
	effect->resource = wl_resource_create(client, &ext_background_effect_surface_v1_interface,
	                                      wl_resource_get_version(resource), id);
	if (!effect->resource) {
		free(effect);
		wl_resource_post_no_memory(resource);
		return;
	}
	effect->surface = surface;
	effect->surface_destroy.notify = handle_surface_destroy;
	wl_resource_add_destroy_listener(surface->resource, &effect->surface_destroy);
	wl_resource_set_implementation(effect->resource, &effect_impl, effect, effect_resource_destroyed);
}

static void
destroy_manager(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct ext_background_effect_manager_v1_interface manager_impl = {
	.destroy = destroy_manager,
	.get_background_effect = get_background_effect,
};

static void
bind_manager(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client, &ext_background_effect_manager_v1_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
	ext_background_effect_manager_v1_send_capabilities(resource, EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR);
}

struct wl_global *
background_effect_create(struct wl_display *display)
{
	return wl_global_create(display, &ext_background_effect_manager_v1_interface, 1, NULL, bind_manager);
}
