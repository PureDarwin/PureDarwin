/* swc: libswc/panel_manager.c
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

#include "panel_manager.h"
#include "internal.h"
#include "panel.h"

#include "swc-server-protocol.h"
#include <wayland-server.h>

static void
create_panel(struct wl_client *client, struct wl_resource *resource,
             uint32_t id, struct wl_resource *surface_resource)
{
	struct surface *surface = wl_resource_get_user_data(surface_resource);

	if (!panel_new(client, wl_resource_get_version(resource), id, surface)) {
		wl_client_post_no_memory(client);
	}
}

static const struct swc_panel_manager_interface panel_manager_impl = {
    .create_panel = create_panel,
};

static void
bind_panel_manager(struct wl_client *client, void *data, uint32_t version,
                   uint32_t id)
{
	struct wl_resource *resource;

	resource =
	    wl_resource_create(client, &swc_panel_manager_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &panel_manager_impl, NULL, NULL);
}

struct wl_global *
panel_manager_create(struct wl_display *display)
{
	return wl_global_create(display, &swc_panel_manager_interface, 1, NULL,
	                        &bind_panel_manager);
}
