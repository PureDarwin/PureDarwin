/* swc: libswc/xdg_decoration.c
 *
 * Copyright (c) 2020 Michael Forney
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

#include "xdg_decoration.h"
#include "util.h"

#include "xdg-decoration-unstable-v1-server-protocol.h"
#include <wayland-server.h>

struct xdg_toplevel_decoration {
	struct wl_resource *resource;
	struct wl_listener toplevel_destroy_listener;
};

static void
set_mode(struct wl_client *client, struct wl_resource *resource, uint32_t mode)
{
}

static void
unset_mode(struct wl_client *client, struct wl_resource *resource)
{
}

static const struct zxdg_toplevel_decoration_v1_interface decoration_impl = {
    .destroy = destroy_resource,
    .set_mode = set_mode,
    .unset_mode = unset_mode,
};

static void
handle_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_toplevel_decoration *decoration =
	    wl_container_of(listener, decoration, toplevel_destroy_listener);

	wl_resource_destroy(decoration->resource);
}

static void
decoration_destroy(struct wl_resource *resource)
{
	struct xdg_toplevel_decoration *decoration =
	    wl_resource_get_user_data(resource);

	wl_list_remove(&decoration->toplevel_destroy_listener.link);
	free(decoration);
}

static void
get_toplevel_decoration(struct wl_client *client, struct wl_resource *resource,
                        uint32_t id, struct wl_resource *toplevel_resource)
{
	struct xdg_toplevel_decoration *decoration;

	decoration = malloc(sizeof(*decoration));
	if (!decoration) {
		goto error0;
	}
	decoration->resource =
	    wl_resource_create(client, &zxdg_toplevel_decoration_v1_interface,
	                       wl_resource_get_version(resource), id);
	if (!decoration->resource) {
		goto error1;
	}
	decoration->toplevel_destroy_listener.notify = &handle_toplevel_destroy;
	wl_resource_add_destroy_listener(toplevel_resource,
	                                 &decoration->toplevel_destroy_listener);
	wl_resource_set_implementation(decoration->resource, &decoration_impl,
	                               decoration, decoration_destroy);
	zxdg_toplevel_decoration_v1_send_configure(
	    decoration->resource, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	return;

error1:
	free(decoration);
error0:
	wl_resource_post_no_memory(resource);
}

static const struct zxdg_decoration_manager_v1_interface
    decoration_manager_impl = {
        .destroy = destroy_resource,
        .get_toplevel_decoration = get_toplevel_decoration,
};

static void
bind_decoration_manager(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &zxdg_decoration_manager_v1_interface,
	                              version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &decoration_manager_impl, NULL,
	                               NULL);
}

struct wl_global *
xdg_decoration_manager_create(struct wl_display *display)
{
	return wl_global_create(display, &zxdg_decoration_manager_v1_interface, 1,
	                        NULL, &bind_decoration_manager);
}
