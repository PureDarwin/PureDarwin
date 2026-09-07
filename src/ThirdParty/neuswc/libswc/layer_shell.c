#include "layer_shell.h"

#include "compositor.h"
#include "internal.h"
#include "keyboard.h"
#include "output.h"
#include "screen.h"
#include "seat.h"
#include "surface.h"
#include "util.h"
#include "view.h"

#include "wlr-layer-shell-unstable-v1-server-protocol.h"
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

struct layer_surface {
	struct wl_resource *resource;
	struct wl_listener surface_destroy_listener;
	struct wl_listener surface_commit_listener;
	struct compositor_view *view;
	struct view_handler view_handler;
	struct screen *screen;
	struct screen_modifier modifier;
	struct layer_surface_state {
		uint32_t layer;
		uint32_t anchor;
		int32_t exclusive_zone;
		uint32_t exclusive_edge;
		uint32_t keyboard_interactivity;
		struct {
			int32_t top, right, bottom, left;
		} margin;
		uint32_t desired_width, desired_height;
	} current, pending;
	uint32_t configure_serial;
	bool configured;
	bool mapped;
};

static bool
state_equal(const struct layer_surface_state *a,
            const struct layer_surface_state *b)
{
	return a->layer == b->layer && a->anchor == b->anchor &&
	       a->exclusive_zone == b->exclusive_zone &&
	       a->exclusive_edge == b->exclusive_edge &&
	       a->keyboard_interactivity == b->keyboard_interactivity &&
	       a->margin.top == b->margin.top &&
	       a->margin.right == b->margin.right &&
	       a->margin.bottom == b->margin.bottom &&
	       a->margin.left == b->margin.left &&
	       a->desired_width == b->desired_width &&
	       a->desired_height == b->desired_height;
}

static int32_t
available_size(uint32_t size, int32_t start_margin, int32_t end_margin)
{
	int32_t available = (int32_t)size - start_margin - end_margin;

	return MAX(available, 0);
}

static uint32_t
configure_size(uint32_t desired, uint32_t anchor, uint32_t first_anchor,
               uint32_t second_anchor, uint32_t total_size, int32_t start_margin,
               int32_t end_margin)
{
	if (desired != 0) {
		return desired;
	}

	if ((anchor & first_anchor) && (anchor & second_anchor)) {
		return available_size(total_size, start_margin, end_margin);
	}

	return 0;
}

static uint32_t
exclusive_edge(struct layer_surface *surface)
{
	if (surface->current.exclusive_edge != 0) {
		return surface->current.exclusive_edge;
	}

	switch (surface->current.anchor) {
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
	    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
	    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP:
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
	    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
	    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
	    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
	    ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT:
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
	    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
	    ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	default:
		return 0;
	}
}

static int32_t
exclusive_size(struct layer_surface *surface)
{
	switch (surface->current.exclusive_zone) {
	case 0:
		return 0;
	case -1:
		switch (exclusive_edge(surface)) {
		case ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP:
			return (int32_t)surface->view->base.geometry.height +
			       surface->current.margin.top;
		case ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
			return (int32_t)surface->view->base.geometry.height +
			       surface->current.margin.bottom;
		case ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT:
			return (int32_t)surface->view->base.geometry.width +
			       surface->current.margin.left;
		case ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
			return (int32_t)surface->view->base.geometry.width +
			       surface->current.margin.right;
		default:
			return 0;
		}
	default:
			return MAX(surface->current.exclusive_zone, 0);
	}
}

static void
update_usable_geometry(struct layer_surface *surface)
{
	if (!surface->screen) {
		return;
	}

	screen_update_usable_geometry(surface->screen);
}

static void
restack_layer(struct layer_surface *surface)
{
	uint32_t stack_layer = STACK_LAYER_NORMAL;
	bool always_top = false;

	switch (surface->current.layer) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		stack_layer = STACK_LAYER_BACKGROUND;
		break;
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		stack_layer = STACK_LAYER_BOTTOM;
		break;
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		stack_layer = STACK_LAYER_TOP;
		always_top = true;
		break;
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		stack_layer = STACK_LAYER_OVERLAY;
		always_top = true;
		break;
	default:
		break;
	}

	surface->view->always_top = always_top;
	compositor_view_set_stack_layer(surface->view, stack_layer, true);
}

static void
update_position(struct layer_surface *surface)
{
	struct swc_rectangle *screen = &surface->screen->base.geometry;
	struct swc_rectangle *view = &surface->view->base.geometry;
	int32_t x, y;

	if ((surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) &&
	    !(surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
		x = screen->x + surface->current.margin.left;
	} else if ((surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) &&
	           !(surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
		x = screen->x + (int32_t)screen->width - (int32_t)view->width -
		    surface->current.margin.right;
	} else {
		x = screen->x + ((int32_t)screen->width - (int32_t)view->width +
		                 surface->current.margin.left -
		                 surface->current.margin.right) /
		                2;
	}

	if ((surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) &&
	    !(surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
		y = screen->y + surface->current.margin.top;
	} else if ((surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) &&
	           !(surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
		y = screen->y + (int32_t)screen->height - (int32_t)view->height -
		    surface->current.margin.bottom;
	} else {
		y = screen->y + ((int32_t)screen->height - (int32_t)view->height +
		                 surface->current.margin.top -
		                 surface->current.margin.bottom) /
		                2;
	}

	view_move(&surface->view->base, x, y);
}

static void
send_configure(struct layer_surface *surface)
{
	uint32_t width, height;

	if (!surface->screen) {
		return;
	}

	width = configure_size(surface->current.desired_width, surface->current.anchor,
	                       ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT,
	                       ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
	                       surface->screen->base.geometry.width,
	                       surface->current.margin.left,
	                       surface->current.margin.right);
	height =
	    configure_size(surface->current.desired_height, surface->current.anchor,
	                   ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
	                   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
	                   surface->screen->base.geometry.height,
	                   surface->current.margin.top,
	                   surface->current.margin.bottom);
	surface->configure_serial = wl_display_next_serial(swc.display);
	zwlr_layer_surface_v1_send_configure(surface->resource,
	                                     surface->configure_serial, width,
	                                     height);
}

static void
set_size(struct wl_client *client, struct wl_resource *resource, uint32_t width,
         uint32_t height)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);

	(void)client;

	surface->pending.desired_width = width;
	surface->pending.desired_height = height;
}

static void
set_anchor(struct wl_client *client, struct wl_resource *resource,
           uint32_t anchor)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);

	(void)client;

	surface->pending.anchor = anchor;
}

static void
set_exclusive_zone(struct wl_client *client, struct wl_resource *resource,
                   int32_t zone)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);

	(void)client;

	surface->pending.exclusive_zone = zone;
}

static void
set_margin(struct wl_client *client, struct wl_resource *resource, int32_t top,
           int32_t right, int32_t bottom, int32_t left)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);

	(void)client;

	surface->pending.margin.top = top;
	surface->pending.margin.right = right;
	surface->pending.margin.bottom = bottom;
	surface->pending.margin.left = left;
}

static void
set_keyboard_interactivity(struct wl_client *client, struct wl_resource *resource,
                           uint32_t keyboard_interactivity)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);

	(void)client;

	surface->pending.keyboard_interactivity = keyboard_interactivity;
}

static void
get_popup(struct wl_client *client, struct wl_resource *resource,
          struct wl_resource *popup)
{
	(void)client;
	(void)resource;
	(void)popup;
}

static void
ack_configure(struct wl_client *client, struct wl_resource *resource,
              uint32_t serial)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);

	(void)client;

	if (serial == surface->configure_serial) {
		update_position(surface);
	}
}

static void
set_layer(struct wl_client *client, struct wl_resource *resource, uint32_t layer)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);

	(void)client;

	if (layer > ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
		wl_resource_post_error(resource,
		                       ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER,
		                       "invalid layer %" PRIu32, layer);
		return;
	}

	surface->pending.layer = layer;
}

static void
set_exclusive_edge(struct wl_client *client, struct wl_resource *resource,
                   uint32_t edge)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);

	(void)client;

	surface->pending.exclusive_edge = edge;
}

static const struct zwlr_layer_surface_v1_interface layer_surface_impl = {
    .set_size = set_size,
    .set_anchor = set_anchor,
    .set_exclusive_zone = set_exclusive_zone,
    .set_margin = set_margin,
    .set_keyboard_interactivity = set_keyboard_interactivity,
    .get_popup = get_popup,
    .ack_configure = ack_configure,
    .destroy = destroy_resource,
    .set_layer = set_layer,
    .set_exclusive_edge = set_exclusive_edge,
};

static void
handle_attach(struct view_handler *handler)
{
	struct layer_surface *surface = wl_container_of(handler, surface, view_handler);
	bool mapped = surface->view->base.buffer != NULL;

	if (mapped) {
		update_position(surface);
		restack_layer(surface);
		if (!surface->mapped) {
			compositor_view_show(surface->view);
			if (surface->current.keyboard_interactivity !=
			    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) {
				keyboard_set_focus(swc.seat->keyboard, surface->view);
			}
		}
	} else if (surface->mapped) {
		compositor_view_hide(surface->view);
	}

	surface->mapped = mapped;
	update_usable_geometry(surface);
}

static void
handle_resize(struct view_handler *handler, uint32_t old_width,
              uint32_t old_height)
{
	struct layer_surface *surface = wl_container_of(handler, surface, view_handler);

	(void)old_width;
	(void)old_height;

	update_position(surface);
	update_usable_geometry(surface);
}

static const struct view_handler_impl view_handler_impl = {
    .attach = handle_attach,
    .resize = handle_resize,
};

static void
handle_surface_commit(struct wl_listener *listener, void *data)
{
	struct layer_surface *surface =
	    wl_container_of(listener, surface, surface_commit_listener);
	bool state_changed = !state_equal(&surface->current, &surface->pending);

	(void)data;

	if (state_changed) {
		surface->current = surface->pending;
		restack_layer(surface);
		update_usable_geometry(surface);
	}

	/* Make sure that the initial commit and also any later state change gets a fresh
	 * configure  */
	if (!surface->configured || state_changed) {
		send_configure(surface);
		surface->configured = true;
	}
}

static void
modify(struct screen_modifier *modifier, const struct swc_rectangle *geom,
       pixman_region32_t *usable)
{
	struct layer_surface *surface = wl_container_of(modifier, surface, modifier);
	pixman_box32_t box = {.x1 = geom->x,
	                      .y1 = geom->y,
	                      .x2 = geom->x + geom->width,
	                      .y2 = geom->y + geom->height};
	int32_t size;

	if (!surface->mapped) {
		pixman_region32_reset(usable, &box);
		return;
	}

	size = exclusive_size(surface);
	if (size <= 0) {
		pixman_region32_reset(usable, &box);
		return;
	}

	/* shrink usable area */
	switch (exclusive_edge(surface)) {
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP:
		box.y1 = MAX(box.y1, geom->y + size);
		break;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
		box.y2 = MIN(box.y2, geom->y + geom->height - size);
		break;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT:
		box.x1 = MAX(box.x1, geom->x + size);
		break;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
		box.x2 = MIN(box.x2, geom->x + geom->width - size);
		break;
	default:
		break;
	}

	pixman_region32_reset(usable, &box);
}

static void
destroy_layer_surface(struct wl_resource *resource)
{
	struct layer_surface *surface = wl_resource_get_user_data(resource);
	bool had_screen = surface->screen != NULL;

	wl_list_remove(&surface->surface_destroy_listener.link);
	wl_list_remove(&surface->surface_commit_listener.link);
	wl_list_remove(&surface->modifier.link);
	compositor_view_destroy(surface->view);
	if (had_screen) {
		screen_update_usable_geometry(surface->screen);
	}
	free(surface);
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct layer_surface *surface =
	    wl_container_of(listener, surface, surface_destroy_listener);

	(void)data;

	wl_resource_destroy(surface->resource);
}

static struct layer_surface *
layer_surface_new(struct wl_client *client, uint32_t version, uint32_t id,
                  struct surface *surface, struct screen *screen, uint32_t layer)
{
	struct layer_surface *layer_surface;

	layer_surface = calloc(1, sizeof(*layer_surface));
	if (!layer_surface) {
		goto error0;
	}

	layer_surface->resource =
	    wl_resource_create(client, &zwlr_layer_surface_v1_interface, version, id);
	if (!layer_surface->resource) {
		goto error1;
	}

	if (!(layer_surface->view = compositor_create_view(surface))) {
		goto error2;
	}

	if (!surface_set_role(surface, layer_surface->resource)) {
		goto error3;
	}

	layer_surface->screen = screen;
	layer_surface->current.layer = layer;
	layer_surface->current.exclusive_zone = 0;
	layer_surface->current.exclusive_edge = 0;
	layer_surface->current.keyboard_interactivity =
	    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
	layer_surface->pending = layer_surface->current;
	layer_surface->surface_destroy_listener.notify = handle_surface_destroy;
	layer_surface->surface_commit_listener.notify = handle_surface_commit;
	layer_surface->view_handler.impl = &view_handler_impl;
	layer_surface->modifier.modify = modify;
	wl_list_init(&layer_surface->modifier.link);
	wl_resource_set_implementation(layer_surface->resource, &layer_surface_impl,
	                               layer_surface, destroy_layer_surface);
	wl_resource_add_destroy_listener(surface->resource,
	                                 &layer_surface->surface_destroy_listener);
	wl_signal_add(&surface->signal.commit, &layer_surface->surface_commit_listener);
	wl_list_insert(&layer_surface->view->base.handlers,
	               &layer_surface->view_handler.link);
	wl_list_insert(&screen->modifiers, &layer_surface->modifier.link);
	restack_layer(layer_surface);

	return layer_surface;

error3:
	compositor_view_destroy(layer_surface->view);
error2:
	wl_resource_destroy(layer_surface->resource);
error1:
	free(layer_surface);
error0:
	return NULL;
}

static void
get_layer_surface(struct wl_client *client, struct wl_resource *resource,
                  uint32_t id, struct wl_resource *surface_resource,
                  struct wl_resource *output_resource, uint32_t layer,
                  const char *namespace_)
{
	struct surface *surface = wl_resource_get_user_data(surface_resource);
	struct output *output;
	struct screen *screen;

	(void)namespace_;

	if (layer > ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
		wl_resource_post_error(resource,
		                       ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER,
		                       "invalid layer %" PRIu32, layer);
		return;
	}

	if (surface->role) {
		wl_resource_post_error(resource, ZWLR_LAYER_SHELL_V1_ERROR_ROLE,
		                       "surface already has a role");
		return;
	}

	if (surface_has_buffer(surface)) {
		wl_resource_post_error(resource,
		                       ZWLR_LAYER_SHELL_V1_ERROR_ALREADY_CONSTRUCTED,
		                       "surface already has a buffer");
		return;
	}

	if (output_resource) {
		output = wl_resource_get_user_data(output_resource);
		screen = output ? output->screen : NULL;
	} else if (!wl_list_empty(&swc.screens)) {
		screen = wl_container_of(swc.screens.next, screen, link);
	} else {
		screen = NULL;
	}

	if (!screen ||
	    !layer_surface_new(client, wl_resource_get_version(resource), id, surface,
	                       screen, layer)) {
		wl_client_post_no_memory(client);
	}
}

static const struct zwlr_layer_shell_v1_interface layer_shell_impl = {
    .get_layer_surface = get_layer_surface,
    .destroy = destroy_resource,
};

static void
bind_layer_shell(struct wl_client *client, void *data, uint32_t version,
                 uint32_t id)
{
	struct wl_resource *resource;

	(void)data;

	resource =
	    wl_resource_create(client, &zwlr_layer_shell_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &layer_shell_impl, NULL, NULL);
}

struct wl_global *
layer_shell_create(struct wl_display *display)
{
	return wl_global_create(display, &zwlr_layer_shell_v1_interface, 5, NULL,
	                        bind_layer_shell);
}
