/* swc: libswc/subsurface.c
 *
 * Copyright (c) 2015-2019 Michael Forney
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

#include "subsurface.h"
#include "compositor.h"
#include "surface.h"
#include "util.h"
#include "view.h"

#include <stdlib.h>
#include <wayland-server.h>

bool
subsurface_is_synchronized(const struct subsurface *subsurface)
{
	while (subsurface) {
		if (subsurface->sync) {
			return true;
		}
		if (!subsurface->parent) {
			return false;
		}
		subsurface = subsurface->parent->subsurface;
	}

	return false;
}

static void
subsurface_update_position(struct subsurface *subsurface)
{
	struct compositor_view *parent_view;
	struct compositor_view *view;

	if (!subsurface->surface || !subsurface->parent) {
		return;
	}

	view = compositor_view(subsurface->surface->view);
	parent_view = compositor_view(subsurface->parent->view);
	if (!view || !parent_view) {
		return;
	}

	view_move(&view->base,
	          parent_view->base.geometry.x + subsurface->x -
	              parent_view->buffer_offset_x,
	          parent_view->base.geometry.y + subsurface->y -
	              parent_view->buffer_offset_y);
}

static void
list_remove_if_linked(struct wl_list *link)
{
	if (!wl_list_empty(link)) {
		wl_list_remove(link);
		wl_list_init(link);
	}
}

void
subsurface_update_visibility(struct subsurface *subsurface)
{
	struct compositor_view *view;
	struct compositor_view *parent_view;

	if (!subsurface || !subsurface->surface || !subsurface->parent) {
		return;
	}

	view = compositor_view(subsurface->surface->view);
	parent_view = compositor_view(subsurface->parent->view);
	if (!view || !parent_view) {
		return;
	}

	if (subsurface->added && parent_view->visible &&
	    subsurface->surface->state.buffer) {
		compositor_view_show(view);
	} else {
		compositor_view_hide(view);
	}
}

static void
handle_parent_view_change(struct view_handler *handler)
{
	struct subsurface *subsurface =
	    wl_container_of(handler, subsurface, parent_view_handler);
	subsurface_update_position(subsurface);
}

static void
handle_parent_view_resize(struct view_handler *handler, uint32_t old_width,
                          uint32_t old_height)
{
	(void)old_width;
	(void)old_height;
	handle_parent_view_change(handler);
}

static const struct view_handler_impl parent_view_handler_impl = {
    .attach = handle_parent_view_change,
    .move = handle_parent_view_change,
    .resize = handle_parent_view_resize,
};

static struct subsurface *
subsurface_find_sibling(struct subsurface *subsurface, struct surface *surface)
{
	struct surface *parent = subsurface->parent;
	struct subsurface *sibling;

	if (!parent) {
		return NULL;
	}

	wl_list_for_each(sibling, &parent->pending.state.subsurfaces_below,
	                 pending_link)
	{
		if (sibling->surface == surface && sibling != subsurface) {
			return sibling;
		}
	}

	wl_list_for_each(sibling, &parent->pending.state.subsurfaces_above,
	                 pending_link)
	{
		if (sibling->surface == surface && sibling != subsurface) {
			return sibling;
		}
	}

	return NULL;
}

static bool
is_valid_sibling(struct subsurface *subsurface, struct surface *sibling_surface,
                 struct subsurface **sibling_subsurface)
{
	struct subsurface *sibling;

	if (!subsurface->parent || !sibling_surface ||
	    sibling_surface == subsurface->surface) {
		return false;
	}

	if (sibling_surface == subsurface->parent) {
		*sibling_subsurface = NULL;
		return true;
	}

	sibling = subsurface_find_sibling(subsurface, sibling_surface);
	if (!sibling) {
		return false;
	}

	*sibling_subsurface = sibling;
	return true;
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct subsurface *subsurface =
	    wl_container_of(listener, subsurface, surface_destroy_listener);
	if (subsurface->resource) {
		wl_resource_destroy(subsurface->resource);
	}
}

static void
handle_parent_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct subsurface *subsurface =
	    wl_container_of(listener, subsurface, parent_destroy_listener);
	struct compositor_view *view = NULL;

	if (subsurface->surface && subsurface->surface->view) {
		view = compositor_view(subsurface->surface->view);
	}

	if (view) {
		view->parent = NULL;
		compositor_view_hide(view);
	}

	if (!wl_list_empty(&subsurface->parent_view_handler.link)) {
		wl_list_remove(&subsurface->parent_view_handler.link);
		wl_list_init(&subsurface->parent_view_handler.link);
	}

	if (!wl_list_empty(&subsurface->link)) {
		wl_list_remove(&subsurface->link);
		wl_list_init(&subsurface->link);
	}

	list_remove_if_linked(&subsurface->pending_link);
	list_remove_if_linked(&subsurface->current_link);

	subsurface->parent = NULL;
}

static void
set_position(struct wl_client *client, struct wl_resource *resource, int32_t x,
             int32_t y)
{
	(void)client;
	struct subsurface *subsurface = wl_resource_get_user_data(resource);

	subsurface->pending_x = x;
	subsurface->pending_y = y;
	subsurface->pending_position = true;
}

static void
place_above(struct wl_client *client, struct wl_resource *resource,
            struct wl_resource *sibling_resource)
{
	(void)client;
	struct subsurface *subsurface = wl_resource_get_user_data(resource);
	struct surface *sibling_surface =
	    wl_resource_get_user_data(sibling_resource);
	struct subsurface *sibling_subsurface;

	if (!is_valid_sibling(subsurface, sibling_surface, &sibling_subsurface)) {
		wl_resource_post_error(resource, WL_SUBSURFACE_ERROR_BAD_SURFACE,
		                       "invalid sibling surface");
		return;
	}

	if (!sibling_subsurface) {
		wl_list_remove(&subsurface->pending_link);
		wl_list_insert(&subsurface->parent->pending.state.subsurfaces_above,
		               &subsurface->pending_link);
	} else {
		wl_list_remove(&subsurface->pending_link);
		wl_list_insert(&sibling_subsurface->pending_link,
		               &subsurface->pending_link);
	}
}

static void
place_below(struct wl_client *client, struct wl_resource *resource,
            struct wl_resource *sibling_resource)
{
	(void)client;
	struct subsurface *subsurface = wl_resource_get_user_data(resource);
	struct surface *sibling_surface =
	    wl_resource_get_user_data(sibling_resource);
	struct subsurface *sibling_subsurface;

	if (!is_valid_sibling(subsurface, sibling_surface, &sibling_subsurface)) {
		wl_resource_post_error(resource, WL_SUBSURFACE_ERROR_BAD_SURFACE,
		                       "invalid sibling surface");
		return;
	}

	if (!sibling_subsurface) {
		wl_list_remove(&subsurface->pending_link);
		wl_list_insert(subsurface->parent->pending.state.subsurfaces_below.prev,
		               &subsurface->pending_link);
	} else {
		wl_list_remove(&subsurface->pending_link);
		wl_list_insert(sibling_subsurface->pending_link.prev,
		               &subsurface->pending_link);
	}
}

static void
set_sync(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	struct subsurface *subsurface = wl_resource_get_user_data(resource);
	subsurface->sync = true;
}

static void
set_desync(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	struct subsurface *subsurface = wl_resource_get_user_data(resource);
	bool synchronized = subsurface_is_synchronized(subsurface);

	subsurface->sync = false;

	if (synchronized && !subsurface_is_synchronized(subsurface) &&
	    subsurface->pending && subsurface->surface) {
		surface_commit_pending(subsurface->surface);
	}
}

void
subsurface_parent_commit(struct surface *parent)
{
	struct subsurface *child;
	struct compositor_view *parent_view;
	struct compositor_view *reference;
	struct compositor_view *child_view;

	if (!parent) {
		return;
	}

	wl_list_for_each(child, &parent->subsurfaces, link)
	    list_remove_if_linked(&child->current_link);

	wl_list_init(&parent->state.subsurfaces_below);
	wl_list_init(&parent->state.subsurfaces_above);

	wl_list_for_each(child, &parent->pending.state.subsurfaces_below,
	                 pending_link)
	    wl_list_insert(parent->state.subsurfaces_below.prev,
	                   &child->current_link);

	wl_list_for_each(child, &parent->pending.state.subsurfaces_above,
	                 pending_link)
	    wl_list_insert(parent->state.subsurfaces_above.prev,
	                   &child->current_link);

	parent_view = parent->view ? compositor_view(parent->view) : NULL;
	if (parent_view) {
		reference = parent_view;
		wl_list_for_each_reverse(child, &parent->state.subsurfaces_below,
		                         current_link)
		{
			if (!child->surface || !child->surface->view) {
				continue;
			}

			child_view = compositor_view(child->surface->view);
			if (!child_view) {
				continue;
			}

			compositor_view_restack(child_view, reference, false);
			reference = child_view;
		}

		reference = parent_view;
		wl_list_for_each(child, &parent->state.subsurfaces_above, current_link)
		{
			if (!child->surface || !child->surface->view) {
				continue;
			}

			child_view = compositor_view(child->surface->view);
			if (!child_view) {
				continue;
			}

			compositor_view_restack(child_view, reference, true);
			reference = child_view;
		}
	}

	wl_list_for_each(child, &parent->subsurfaces, link)
	{
		if (!child->pending_position) {
			continue;
		}

		child->x = child->pending_x;
		child->y = child->pending_y;
		child->pending_position = false;
		subsurface_update_position(child);
	}

	wl_list_for_each(child, &parent->state.subsurfaces_below, current_link)
	{
		if (!child->added) {
			child->added = true;
		}
		subsurface_update_visibility(child);
	}
	wl_list_for_each(child, &parent->state.subsurfaces_above, current_link)
	{
		if (!child->added) {
			child->added = true;
		}
		subsurface_update_visibility(child);
	}
}

static const struct wl_subsurface_interface subsurface_impl = {
    .destroy = destroy_resource,
    .set_position = set_position,
    .place_above = place_above,
    .place_below = place_below,
    .set_sync = set_sync,
    .set_desync = set_desync,
};

static void
subsurface_destroy(struct wl_resource *resource)
{
	struct subsurface *subsurface = wl_resource_get_user_data(resource);

	if (subsurface->surface) {
		if (subsurface->surface->subsurface == subsurface) {
			subsurface->surface->subsurface = NULL;
		}
	}

	if (!wl_list_empty(&subsurface->parent_destroy_listener.link)) {
		wl_list_remove(&subsurface->parent_destroy_listener.link);
		wl_list_init(&subsurface->parent_destroy_listener.link);
	}
	if (!wl_list_empty(&subsurface->surface_destroy_listener.link)) {
		wl_list_remove(&subsurface->surface_destroy_listener.link);
		wl_list_init(&subsurface->surface_destroy_listener.link);
	}

	if (!wl_list_empty(&subsurface->parent_view_handler.link)) {
		wl_list_remove(&subsurface->parent_view_handler.link);
		wl_list_init(&subsurface->parent_view_handler.link);
	}

	if (!wl_list_empty(&subsurface->link)) {
		wl_list_remove(&subsurface->link);
		wl_list_init(&subsurface->link);
	}

	list_remove_if_linked(&subsurface->pending_link);
	list_remove_if_linked(&subsurface->current_link);

	if (subsurface->surface && subsurface->surface->view) {
		struct compositor_view *view =
		    compositor_view(subsurface->surface->view);
		if (view && !view->window) {
			compositor_view_destroy(view);
		}
	}

	free(subsurface);
}

struct subsurface *
subsurface_new(struct wl_client *client, uint32_t version, uint32_t id,
               struct surface *surface, struct surface *parent)
{
	struct subsurface *subsurface;
	struct compositor_view *parent_view;
	struct compositor_view *view;

	if (!(subsurface = malloc(sizeof(*subsurface)))) {
		goto error0;
	}

	subsurface->resource =
	    wl_resource_create(client, &wl_subsurface_interface, version, id);

	if (!subsurface->resource) {
		goto error1;
	}

	wl_resource_set_implementation(subsurface->resource, &subsurface_impl,
	                               subsurface, &subsurface_destroy);

	subsurface->surface = surface;
	subsurface->parent = parent;
	subsurface->x = 0;
	subsurface->y = 0;
	subsurface->pending_x = 0;
	subsurface->pending_y = 0;
	subsurface->pending_position = false;
	subsurface->sync = true;
	subsurface->pending = false;
	subsurface->added = false;

	subsurface->parent_view_handler.impl = &parent_view_handler_impl;
	wl_list_init(&subsurface->parent_view_handler.link);
	wl_list_init(&subsurface->surface_destroy_listener.link);
	wl_list_init(&subsurface->parent_destroy_listener.link);
	wl_list_init(&subsurface->link);
	wl_list_init(&subsurface->pending_link);
	wl_list_init(&subsurface->current_link);

	if (!surface->view) {
		compositor_create_view(surface);
	}
	if (!parent->view) {
		compositor_create_view(parent);
	}

	parent_view = compositor_view(parent->view);
	view = compositor_view(surface->view);
	if (!parent_view || !view) {
		goto error2;
	}

	compositor_view_set_parent(view, parent_view);
	wl_list_remove(&view->link);
	wl_list_insert(parent_view->link.prev, &view->link);

	wl_list_insert(&parent_view->base.handlers,
	               &subsurface->parent_view_handler.link);
	subsurface_update_position(subsurface);
	wl_list_insert(&parent->subsurfaces, &subsurface->link);
	wl_list_insert(parent->pending.state.subsurfaces_above.prev,
	               &subsurface->pending_link);
	subsurface_update_visibility(subsurface);

	subsurface->surface_destroy_listener.notify = handle_surface_destroy;
	wl_resource_add_destroy_listener(surface->resource,
	                                 &subsurface->surface_destroy_listener);
	subsurface->parent_destroy_listener.notify = handle_parent_destroy;
	wl_resource_add_destroy_listener(parent->resource,
	                                 &subsurface->parent_destroy_listener);

	return subsurface;

error2:
	wl_resource_destroy(subsurface->resource);
	return NULL;
error1:
	free(subsurface);
error0:
	return NULL;
}
