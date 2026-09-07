/* swc: libswc/kde_decoration.c
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

#include "kde_decoration.h"
#include "util.h"

#include "server-decoration-server-protocol.h"
#include <wayland-server.h>

static void
request_mode(struct wl_client *client, struct wl_resource *resource,
             uint32_t mode)
{
	/* Server is required to send back the mode requested by
	 * the client, we just don't plan to do anything with it. */
	org_kde_kwin_server_decoration_send_mode(resource, mode);
}

static const struct org_kde_kwin_server_decoration_interface decoration_impl = {
    .release = destroy_resource,
    .request_mode = request_mode,
};

static void
create(struct wl_client *client, struct wl_resource *resource, uint32_t id,
       struct wl_resource *toplevel_resource)
{
	struct wl_resource *decoration;

	decoration =
	    wl_resource_create(client, &org_kde_kwin_server_decoration_interface,
	                       wl_resource_get_version(resource), id);
	if (!decoration) {
		wl_resource_post_no_memory(resource);
		return;
	}
	wl_resource_set_implementation(decoration, &decoration_impl, NULL, NULL);
	org_kde_kwin_server_decoration_send_mode(
	    decoration, ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

static const struct org_kde_kwin_server_decoration_manager_interface
    decoration_manager_impl = {
        .create = create,
};

static void
bind_decoration_manager(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(
	    client, &org_kde_kwin_server_decoration_manager_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &decoration_manager_impl, NULL,
	                               NULL);
	org_kde_kwin_server_decoration_manager_send_default_mode(
	    resource, ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

struct wl_global *
kde_decoration_manager_create(struct wl_display *display)
{
	return wl_global_create(display,
	                        &org_kde_kwin_server_decoration_manager_interface,
	                        1, NULL, &bind_decoration_manager);
}
