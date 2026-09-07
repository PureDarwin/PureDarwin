#include "region.h"
#include "util.h"

#include <pixman.h>
#include <stdlib.h>
#include <wayland-server.h>

static void
add(struct wl_client *client, struct wl_resource *resource, int32_t x,
    int32_t y, int32_t width, int32_t height)
{
	pixman_region32_t *region = wl_resource_get_user_data(resource);

	pixman_region32_union_rect(region, region, x, y, width, height);
}

static void
subtract(struct wl_client *client, struct wl_resource *resource, int32_t x,
         int32_t y, int32_t width, int32_t height)
{
	pixman_region32_t *region = wl_resource_get_user_data(resource);
	pixman_region32_t operand;

	pixman_region32_init_rect(&operand, x, y, width, height);
	pixman_region32_subtract(region, region, &operand);
}

static const struct wl_region_interface region_impl = {
    .destroy = destroy_resource,
    .add = add,
    .subtract = subtract,
};

static void
region_destroy(struct wl_resource *resource)
{
	pixman_region32_t *region = wl_resource_get_user_data(resource);

	pixman_region32_fini(region);
	free(region);
}

struct wl_resource *
region_new(struct wl_client *client, uint32_t version, uint32_t id)
{
	pixman_region32_t *region;
	struct wl_resource *resource;

	region = malloc(sizeof(*region));
	if (!region) {
		goto error0;
	}

	resource = wl_resource_create(client, &wl_region_interface, version, id);
	if (!resource) {
		goto error1;
	}
	wl_resource_set_implementation(resource, &region_impl, region,
	                               &region_destroy);

	pixman_region32_init(region);

	return resource;

error1:
	free(region);
error0:
	return NULL;
}
