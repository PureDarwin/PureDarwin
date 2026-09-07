/* swc: libswc/xdg_output.c
 *
 * Copyright (c) 2026 sewn <sewn@disroot.org>
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

#include "xdg_output.h"
#include "output.h"
#include "screen.h"
#include "util.h"

#include "xdg-output-unstable-v1-server-protocol.h"

static const struct zxdg_output_v1_interface output_impl = {
	.destroy = destroy_resource,
};

static void
get_output(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *output_resource)
{
	struct output *output =
	    wl_resource_get_user_data(output_resource);
	struct swc_rectangle *geom = &output->screen->base.geometry;
	struct wl_resource *wl_output_resource;

	resource = wl_resource_create(client, &zxdg_output_v1_interface, wl_resource_get_version(resource), id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &output_impl, NULL, NULL);
	zxdg_output_v1_send_logical_position(resource, geom->x, geom->y);
	zxdg_output_v1_send_logical_size(resource, geom->width, geom->height);
	if (wl_resource_get_version(resource) >= 2) {
		zxdg_output_v1_send_name(resource, output->name);
		zxdg_output_v1_send_description(resource, output->name);
	}
	zxdg_output_v1_send_done(resource);

	wl_output_resource =
	    wl_resource_find_for_client(&output->resources, client);
	if (wl_output_resource && wl_resource_get_version(wl_output_resource) >= 2) {
		wl_output_send_done(wl_output_resource);
	}
}

static const struct zxdg_output_manager_v1_interface output_manager_impl = {
	.destroy = destroy_resource,
	.get_xdg_output = get_output,
};

static void
bind_output_manager(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &zxdg_output_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &output_manager_impl, NULL, NULL);
}

struct wl_global *
xdg_output_manager_create(struct wl_display *display)
{
	return wl_global_create(display, &zxdg_output_manager_v1_interface, 3, NULL, &bind_output_manager);
}
