// surface_host.c - pd_surface_host_v1, a surface one client draws shown as a subsurface of another client's surface.
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include "surface_host.h"
#include "subsurface.h"
#include "surface.h"
#include "util.h"

#include "pd-surface-host-v1-server-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server.h>

bool
surface_host_trace_on(void)
{
	static int on = -1;

	if (on < 0)
		on = getenv("SWC_HOST_TRACE") != NULL;
	return on;
}

struct surface_export {
	struct wl_resource *resource;
	struct surface *surface;
	uint32_t handle;
	struct wl_listener surface_destroy;
	struct wl_list link;
};

static struct wl_list exports = { &exports, &exports };
static uint32_t next_handle;

static void
forget_surface(struct surface_export *export)
{
	if (!export->surface)
		return;
	wl_list_remove(&export->surface_destroy.link);
	export->surface = NULL;
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct surface_export *export = wl_container_of(listener, export, surface_destroy);

	forget_surface(export);
}

static void
export_destroyed(struct wl_resource *resource)
{
	struct surface_export *export = wl_resource_get_user_data(resource);

	forget_surface(export);
	wl_list_remove(&export->link);
	free(export);
}

static const struct pd_surface_export_v1_interface export_impl = {
	.destroy = destroy_resource,
};

// handles are hard to guess, as a process can only show what another one meant to hand it
static uint32_t
new_handle(void)
{
	struct surface_export *export;
	uint32_t handle;

	if (!next_handle)
		next_handle = arc4random();
	do {
		handle = (next_handle += 0x9e3779b9) ^ arc4random();
		wl_list_for_each (export, &exports, link) {
			if (export->handle == handle)
				handle = 0;
		}
	} while (!handle);
	return handle;
}

static void
export_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource)
{
	struct surface_export *export = calloc(1, sizeof(*export));

	if (!export) {
		wl_resource_post_no_memory(resource);
		return;
	}
	export->resource = wl_resource_create(client, &pd_surface_export_v1_interface, wl_resource_get_version(resource), id);
	if (!export->resource) {
		free(export);
		wl_resource_post_no_memory(resource);
		return;
	}
	wl_resource_set_implementation(export->resource, &export_impl, export, export_destroyed);
	export->surface = wl_resource_get_user_data(surface_resource);
	export->handle = new_handle();
	export->surface_destroy.notify = handle_surface_destroy;
	wl_resource_add_destroy_listener(surface_resource, &export->surface_destroy);
	wl_list_insert(&exports, &export->link);
	if (surface_host_trace_on())
		fprintf(stderr, "surface_host: export %u surface %p\n", export->handle, (void *)export->surface);
	pd_surface_export_v1_send_handle(export->resource, export->handle);
}

static struct surface *
exported_surface(uint32_t handle)
{
	struct surface_export *export;

	wl_list_for_each (export, &exports, link) {
		if (export->handle == handle)
			return export->surface;
	}
	return NULL;
}

static bool
is_descendant_of(struct surface *ancestor, struct surface *surface)
{
	while (surface && surface->subsurface) {
		surface = surface->subsurface->parent;
		if (surface == ancestor)
			return true;
	}
	return false;
}

static void
inert_set_position(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
}

static void
inert_place(struct wl_client *client, struct wl_resource *resource, struct wl_resource *sibling)
{
}

static void
inert_set_mode(struct wl_client *client, struct wl_resource *resource)
{
}

// what an import of a stale handle gets: a subsurface that takes every request and shows nothing
static const struct wl_subsurface_interface inert_subsurface_impl = {
	.destroy = destroy_resource,
	.set_position = inert_set_position,
	.place_above = inert_place,
	.place_below = inert_place,
	.set_sync = inert_set_mode,
	.set_desync = inert_set_mode,
};

static void
import_subsurface(struct wl_client *client, struct wl_resource *resource, uint32_t id, uint32_t handle,
                  struct wl_resource *parent_resource)
{
	struct surface *surface = exported_surface(handle);
	struct surface *parent = wl_resource_get_user_data(parent_resource);
	struct subsurface *subsurface;
	struct wl_resource *inert;

	if (!surface || !parent || surface == parent || surface->subsurface || is_descendant_of(surface, parent)) {
		if (surface_host_trace_on())
			fprintf(stderr, "surface_host: import %u into %p inert, surface %p subsurface %p\n", handle, (void *)parent,
			        (void *)surface, surface ? (void *)surface->subsurface : NULL);
		inert = wl_resource_create(client, &wl_subsurface_interface, 1, id);
		if (!inert) {
			wl_resource_post_no_memory(resource);
			return;
		}
		wl_resource_set_implementation(inert, &inert_subsurface_impl, NULL, NULL);
		return;
	}
	if (!(subsurface = subsurface_new(client, 1, id, surface, parent))) {
		wl_resource_post_no_memory(resource);
		return;
	}
	surface->subsurface = subsurface;
	if (surface_host_trace_on())
		fprintf(stderr, "surface_host: import %u surface %p into %p buffer %p\n", handle, (void *)surface, (void *)parent,
		        (void *)surface->state.buffer);
}

static const struct pd_surface_host_manager_v1_interface manager_impl = {
	.destroy = destroy_resource,
	.export_surface = export_surface,
	.import_subsurface = import_subsurface,
};

static void
bind_manager(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client, &pd_surface_host_manager_v1_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}

struct wl_global *
surface_host_create(struct wl_display *display)
{
	return wl_global_create(display, &pd_surface_host_manager_v1_interface, 1, NULL, bind_manager);
}
