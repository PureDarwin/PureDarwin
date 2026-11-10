/* swc: libswc/pointer_constraints.c
 *
 * Copyright (c) 2026 The PureDarwin Project
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

#include "pointer_constraints.h"
#include "compositor.h"
#include "event.h"
#include "input.h"
#include "internal.h"
#include "pointer.h"
#include "seat.h"
#include "surface.h"
#include "util.h"

#include "pointer-constraints-unstable-v1-server-protocol.h"
#include "relative-pointer-unstable-v1-server-protocol.h"
#include <pixman.h>
#include <stdlib.h>

// one lock or confinement on a surface, semantics from wayland-protocols
// unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
struct constraint {
	struct wl_resource *resource;
	struct surface *surface;
	struct wl_listener surface_destroy;
	struct wl_listener surface_commit;
	bool lock;
	uint32_t lifetime;
	bool active, defunct;

	// a null region means the whole input region of the surface
	bool infinite, pending_infinite, region_pending;
	pixman_region32_t region, pending_region;

	// set_cursor_position_hint is double-buffered like the region
	bool hint_set, hint_pending;
	wl_fixed_t hint_x, hint_y, pending_hint_x, pending_hint_y;

	struct wl_list link;
};

static struct {
	struct wl_list constraints;
	struct wl_list relative_pointers;
	struct constraint *active;
	struct wl_listener focus_listener;
} state;

static struct surface *
focus_surface(struct pointer *pointer)
{
	return pointer->focus.view ? pointer->focus.view->surface : NULL;
}

static bool
surface_origin(struct surface *surface, int32_t *x, int32_t *y)
{
	struct compositor_view *view;

	if (!surface || !surface->view)
		return false;
	view = compositor_view(surface->view);
	if (!view)
		return false;
	*x = view->base.geometry.x - view->buffer_offset_x;
	*y = view->base.geometry.y - view->buffer_offset_y;
	return true;
}

// the request region intersected with the input region, in surface coordinates
static void
effective_region(struct constraint *c, pixman_region32_t *out)
{
	pixman_region32_init(out);
	pixman_region32_copy(out, &c->surface->state.input);
	if (!c->infinite)
		pixman_region32_intersect(out, out, &c->region);
}

static bool
pointer_inside(struct constraint *c, struct pointer *pointer)
{
	pixman_region32_t region;
	int32_t ox, oy;
	bool inside;

	if (!surface_origin(c->surface, &ox, &oy))
		return false;
	effective_region(c, &region);
	inside = pixman_region32_contains_point(&region, wl_fixed_to_int(pointer->x) - ox,
	                                        wl_fixed_to_int(pointer->y) - oy, NULL);
	pixman_region32_fini(&region);
	return inside;
}

static void
activate(struct constraint *c)
{
	c->active = true;
	state.active = c;
	if (c->lock)
		zwp_locked_pointer_v1_send_locked(c->resource);
	else
		zwp_confined_pointer_v1_send_confined(c->resource);
}

// the hint warp on unlock sends no relative motion, as the protocol asks
static void
deactivate(struct constraint *c, bool send)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;
	int32_t ox, oy;

	c->active = false;
	if (state.active == c)
		state.active = NULL;
	if (pointer && c->lock && c->hint_set && surface_origin(c->surface, &ox, &oy))
		pointer_warp(pointer, c->hint_x + wl_fixed_from_int(ox), c->hint_y + wl_fixed_from_int(oy));
	if (send) {
		if (c->lock)
			zwp_locked_pointer_v1_send_unlocked(c->resource);
		else
			zwp_confined_pointer_v1_send_unconfined(c->resource);
	}
	if (c->lifetime != ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT)
		c->defunct = true;
}

// a constraint is active only while its surface has pointer focus and the pointer is in its region
void
pointer_constraints_update(struct pointer *pointer)
{
	struct surface *surface = focus_surface(pointer);
	struct constraint *c = state.active;

	if (c) {
		if (c->surface == surface && pointer_inside(c, pointer))
			return;
		deactivate(c, true);
	}
	if (!surface)
		return;
	wl_list_for_each(c, &state.constraints, link) {
		if (c->surface == surface && !c->defunct && pointer_inside(c, pointer)) {
			activate(c);
			break;
		}
	}
}

static void
send_relative(struct pointer *pointer, uint32_t time, wl_fixed_t dx, wl_fixed_t dy)
{
	struct wl_resource *resource;
	uint64_t usec = (uint64_t)time * 1000;
	bool sent = false;

	if (!pointer->focus.client || (dx == 0 && dy == 0))
		return;
	wl_resource_for_each(resource, &state.relative_pointers) {
		if (wl_resource_get_client(resource) != pointer->focus.client)
			continue;
		// the input layer has no acceleration, so both pairs carry the device deltas
		zwp_relative_pointer_v1_send_relative_motion(resource, usec >> 32, usec & 0xffffffff,
		                                             dx, dy, dx, dy);
		sent = true;
	}
	// relative events are grouped by wl_pointer.frame like the rest
	if (sent)
		pointer->client_handler.pending = true;
}

bool
pointer_constraints_motion(struct pointer *pointer, uint32_t time, wl_fixed_t dx,
                           wl_fixed_t dy, wl_fixed_t *x, wl_fixed_t *y)
{
	struct constraint *c = state.active;
	pixman_region32_t region;
	pixman_box32_t box;
	int32_t ox, oy, tx, ty;

	send_relative(pointer, time, dx, dy);
	if (!c)
		return true;
	if (c->lock)
		return false;
	if (!surface_origin(c->surface, &ox, &oy))
		return true;

	effective_region(c, &region);
	tx = wl_fixed_to_int(*x) - ox;
	ty = wl_fixed_to_int(*y) - oy;
	if (!pixman_region32_contains_point(&region, tx, ty, NULL)) {
		// clamp into the box that holds the pointer now, the way clip_position keeps it on screen
		if (pixman_region32_contains_point(&region, wl_fixed_to_int(pointer->x) - ox,
		                                   wl_fixed_to_int(pointer->y) - oy, &box)) {
			*x = wl_fixed_from_int(MAX(MIN(tx, box.x2 - 1), box.x1) + ox);
			*y = wl_fixed_from_int(MAX(MIN(ty, box.y2 - 1), box.y1) + oy);
		} else {
			*x = pointer->x;
			*y = pointer->y;
		}
	}
	pixman_region32_fini(&region);
	return true;
}

static void
handle_focus_event(struct wl_listener *listener, void *data)
{
	struct event *event = data;

	if (event->type == INPUT_FOCUS_EVENT_CHANGED && swc.seat)
		pointer_constraints_update(swc.seat->pointer);
}

static void
handle_surface_commit(struct wl_listener *listener, void *data)
{
	struct constraint *c = wl_container_of(listener, c, surface_commit);

	if (c->region_pending) {
		c->infinite = c->pending_infinite;
		pixman_region32_copy(&c->region, &c->pending_region);
		c->region_pending = false;
	}
	if (c->hint_pending) {
		c->hint_x = c->pending_hint_x;
		c->hint_y = c->pending_hint_y;
		c->hint_set = true;
		c->hint_pending = false;
	}
	if (swc.seat)
		pointer_constraints_update(swc.seat->pointer);
}

static void
detach_surface(struct constraint *c)
{
	if (!c->surface)
		return;
	wl_list_remove(&c->surface_destroy.link);
	wl_list_remove(&c->surface_commit.link);
	c->surface = NULL;
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct constraint *c = wl_container_of(listener, c, surface_destroy);

	if (c->active)
		deactivate(c, true);
	detach_surface(c);
	c->defunct = true;
}

static void
constraint_destroyed(struct wl_resource *resource)
{
	struct constraint *c = wl_resource_get_user_data(resource);

	// destroying the object unlocks, a client exiting lands here too
	if (c->active)
		deactivate(c, false);
	detach_surface(c);
	wl_list_remove(&c->link);
	pixman_region32_fini(&c->region);
	pixman_region32_fini(&c->pending_region);
	free(c);
	if (swc.seat)
		pointer_constraints_update(swc.seat->pointer);
}

static void
set_region(struct wl_client *client, struct wl_resource *resource,
           struct wl_resource *region_resource)
{
	struct constraint *c = wl_resource_get_user_data(resource);

	c->region_pending = true;
	c->pending_infinite = region_resource == NULL;
	if (region_resource)
		pixman_region32_copy(&c->pending_region, wl_resource_get_user_data(region_resource));
	else
		pixman_region32_clear(&c->pending_region);
}

static void
set_cursor_position_hint(struct wl_client *client, struct wl_resource *resource,
                         wl_fixed_t x, wl_fixed_t y)
{
	struct constraint *c = wl_resource_get_user_data(resource);

	c->pending_hint_x = x;
	c->pending_hint_y = y;
	c->hint_pending = true;
}

static const struct zwp_locked_pointer_v1_interface locked_impl = {
	.destroy = destroy_resource,
	.set_cursor_position_hint = set_cursor_position_hint,
	.set_region = set_region,
};

static const struct zwp_confined_pointer_v1_interface confined_impl = {
	.destroy = destroy_resource,
	.set_region = set_region,
};

static void
constrain(struct wl_client *client, struct wl_resource *resource, uint32_t id,
          struct wl_resource *surface_resource, struct wl_resource *region_resource,
          uint32_t lifetime, bool lock)
{
	struct surface *surface = wl_resource_get_user_data(surface_resource);
	struct constraint *c;

	wl_list_for_each(c, &state.constraints, link) {
		if (c->surface == surface) {
			wl_resource_post_error(resource, ZWP_POINTER_CONSTRAINTS_V1_ERROR_ALREADY_CONSTRAINED,
			                       "the surface already has a pointer constraint");
			return;
		}
	}
	c = calloc(1, sizeof(*c));
	if (!c) {
		wl_resource_post_no_memory(resource);
		return;
	}
	c->resource = wl_resource_create(client, lock ? &zwp_locked_pointer_v1_interface : &zwp_confined_pointer_v1_interface,
	                                 wl_resource_get_version(resource), id);
	if (!c->resource) {
		free(c);
		wl_resource_post_no_memory(resource);
		return;
	}
	c->lock = lock;
	c->lifetime = lifetime;
	c->surface = surface;
	c->infinite = region_resource == NULL;
	pixman_region32_init(&c->region);
	pixman_region32_init(&c->pending_region);
	if (region_resource)
		pixman_region32_copy(&c->region, wl_resource_get_user_data(region_resource));
	c->surface_destroy.notify = handle_surface_destroy;
	wl_resource_add_destroy_listener(surface->resource, &c->surface_destroy);
	c->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&surface->signal.commit, &c->surface_commit);
	wl_list_insert(&state.constraints, &c->link);
	wl_resource_set_implementation(c->resource, lock ? (const void *)&locked_impl : (const void *)&confined_impl,
	                               c, constraint_destroyed);
	if (swc.seat)
		pointer_constraints_update(swc.seat->pointer);
}

static void
lock_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id,
             struct wl_resource *surface, struct wl_resource *pointer,
             struct wl_resource *region, uint32_t lifetime)
{
	constrain(client, resource, id, surface, region, lifetime, true);
}

static void
confine_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id,
                struct wl_resource *surface, struct wl_resource *pointer,
                struct wl_resource *region, uint32_t lifetime)
{
	constrain(client, resource, id, surface, region, lifetime, false);
}

static const struct zwp_pointer_constraints_v1_interface constraints_impl = {
	.destroy = destroy_resource,
	.lock_pointer = lock_pointer,
	.confine_pointer = confine_pointer,
};

static void
bind_constraints(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client, &zwp_pointer_constraints_v1_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &constraints_impl, NULL, NULL);
}

static void
relative_pointer_destroyed(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

static const struct zwp_relative_pointer_v1_interface relative_impl = {
	.destroy = destroy_resource,
};

static void
get_relative_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id,
                     struct wl_resource *pointer)
{
	struct wl_resource *relative = wl_resource_create(client, &zwp_relative_pointer_v1_interface,
	                                                  wl_resource_get_version(resource), id);

	if (!relative) {
		wl_resource_post_no_memory(resource);
		return;
	}
	wl_resource_set_implementation(relative, &relative_impl, NULL, relative_pointer_destroyed);
	wl_list_insert(&state.relative_pointers, wl_resource_get_link(relative));
}

static const struct zwp_relative_pointer_manager_v1_interface relative_manager_impl = {
	.destroy = destroy_resource,
	.get_relative_pointer = get_relative_pointer,
};

static void
bind_relative(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client, &zwp_relative_pointer_manager_v1_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &relative_manager_impl, NULL, NULL);
}

bool
pointer_constraints_create(struct wl_display *display)
{
	wl_list_init(&state.constraints);
	wl_list_init(&state.relative_pointers);
	if (swc.seat && swc.seat->pointer) {
		state.focus_listener.notify = handle_focus_event;
		wl_signal_add(&swc.seat->pointer->focus.event_signal, &state.focus_listener);
	}
	if (!wl_global_create(display, &zwp_pointer_constraints_v1_interface, 1, NULL, bind_constraints))
		return false;
	return wl_global_create(display, &zwp_relative_pointer_manager_v1_interface, 1, NULL, bind_relative) != NULL;
}
