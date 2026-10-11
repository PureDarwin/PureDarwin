#include "window_position.h"
#include "surface.h"
#include "util.h"
#include "view.h"
#include "compositor.h"
#include "window.h"
#include "swc.h"

#include "pd-window-position-v1-server-protocol.h"
#include <stdlib.h>
#include <wayland-server.h>

// one per window a client asked about, it follows the surface's view and reports each move
struct window_position {
	struct wl_resource *resource;
	struct view *view;
	struct view_handler handler;
	struct wl_listener surface_destroy;
};

static void
send_position(struct window_position *position)
{
	if (position->view)
		pd_window_position_v1_send_position(position->resource, position->view->geometry.x, position->view->geometry.y);
}

static void
handle_move(struct view_handler *handler)
{
	struct window_position *position = wl_container_of(handler, position, handler);

	send_position(position);
}

static const struct view_handler_impl handler_impl = {
	.move = handle_move,
};

static void
detach(struct window_position *position)
{
	if (!position->view)
		return;
	wl_list_remove(&position->handler.link);
	wl_list_remove(&position->surface_destroy.link);
	position->view = NULL;
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct window_position *position = wl_container_of(listener, position, surface_destroy);

	detach(position);
}

// the client's own placement, only windows take it, a layer surface is anchored instead
static void
set_position(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
	struct window_position *position = wl_resource_get_user_data(resource);
	struct compositor_view *view = position->view ? compositor_view(position->view) : NULL;

	if (view && view->window)
		swc_window_set_position(&view->window->base, x, y);
}

static const struct pd_window_position_v1_interface position_impl = {
	.destroy = destroy_resource,
	.set_position = set_position,
};

static void
position_destroyed(struct wl_resource *resource)
{
	struct window_position *position = wl_resource_get_user_data(resource);

	detach(position);
	free(position);
}

static void
get_window_position(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource)
{
	struct surface *surface = wl_resource_get_user_data(surface_resource);
	struct window_position *position = calloc(1, sizeof(*position));

	if (!position) {
		wl_resource_post_no_memory(resource);
		return;
	}
	position->resource = wl_resource_create(client, &pd_window_position_v1_interface, wl_resource_get_version(resource), id);
	if (!position->resource) {
		free(position);
		wl_resource_post_no_memory(resource);
		return;
	}
	wl_resource_set_implementation(position->resource, &position_impl, position, position_destroyed);
	// a surface gets its view when its window role is made, asked for later than that it has one
	if (surface->view) {
		position->view = surface->view;
		position->handler.impl = &handler_impl;
		wl_list_insert(&position->view->handlers, &position->handler.link);
		position->surface_destroy.notify = handle_surface_destroy;
		wl_resource_add_destroy_listener(surface->resource, &position->surface_destroy);
		send_position(position);
	}
}

static const struct pd_window_position_manager_v1_interface manager_impl = {
	.destroy = destroy_resource,
	.get_window_position = get_window_position,
};

static void
bind_manager(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client, &pd_window_position_manager_v1_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}

struct wl_global *
window_position_create(struct wl_display *display)
{
	return wl_global_create(display, &pd_window_position_manager_v1_interface, 1, NULL, bind_manager);
}
