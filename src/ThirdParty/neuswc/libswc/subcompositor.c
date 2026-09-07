/* swc: libswc/subcompositor.c
 *
 * Copyright (c) 2015-2020 Michael Forney
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

#include "subcompositor.h"
#include "internal.h"
#include "subsurface.h"
#include "surface.h"
#include "swc.h"
#include "util.h"

static bool
is_descendant_of(struct surface *ancestor, struct surface *surface)
{
	while (surface && surface->subsurface) {
		surface = surface->subsurface->parent;
		if (surface == ancestor) {
			return true;
		}
	}

	return false;
}

static void
get_subsurface(struct wl_client *client, struct wl_resource *resource,
               uint32_t id, struct wl_resource *surface_resource,
               struct wl_resource *parent_resource)
{
	struct subsurface *subsurface;
	struct surface *surface = wl_resource_get_user_data(surface_resource);
	struct surface *parent = wl_resource_get_user_data(parent_resource);

	if (!surface || !parent) {
		wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
		                       "invalid surface");
		return;
	}

	if (surface == parent || is_descendant_of(surface, parent)) {
		wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_PARENT,
		                       "invalid parent surface");
		return;
	}

	if (surface->subsurface) {
		wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
		                       "surface already has a subsurface role");
		return;
	}

	subsurface = subsurface_new(client, wl_resource_get_version(resource), id,
	                            surface, parent);

	if (!subsurface) {
		wl_resource_post_no_memory(resource);
		return;
	}
	surface->subsurface = subsurface;
}

static const struct wl_subcompositor_interface subcompositor_impl = {
    .destroy = destroy_resource,
    .get_subsurface = get_subsurface,
};

static void
bind_subcompositor(struct wl_client *client, void *data, uint32_t version,
                   uint32_t id)
{
	struct wl_resource *resource;

	resource =
	    wl_resource_create(client, &wl_subcompositor_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &subcompositor_impl, NULL, NULL);
}

struct wl_global *
subcompositor_create(struct wl_display *display)
{
	return wl_global_create(display, &wl_subcompositor_interface, 1, NULL,
	                        &bind_subcompositor);
}
