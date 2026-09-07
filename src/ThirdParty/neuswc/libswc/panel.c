/* swc: libswc/panel.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
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

#include "panel.h"
#include "compositor.h"
#include "internal.h"
#include "keyboard.h"
#include "output.h"
#include "screen.h"
#include "seat.h"
#include "surface.h"
#include "util.h"
#include "view.h"

#include "swc-server-protocol.h"
#include <assert.h>
#include <stdlib.h>

struct panel {
	struct wl_resource *resource;

	struct wl_listener surface_destroy_listener;
	struct compositor_view *view;
	struct view_handler view_handler;
	struct screen *screen;
	struct screen_modifier modifier;
	uint32_t edge;
	uint32_t offset, strut_size;
	uint32_t y_offset;
	bool docked;
};

static void
update_position(struct panel *panel)
{
	int32_t x, y;
	struct swc_rectangle *screen = &panel->screen->base.geometry,
	                     *view = &panel->view->base.geometry;

	switch (panel->edge) {
	case SWC_PANEL_EDGE_TOP:
		x = screen->x + panel->offset;
		y = screen->y + panel->y_offset;
		break;
	case SWC_PANEL_EDGE_BOTTOM:
		x = screen->x + panel->offset;
		y = screen->y + screen->height - view->height - panel->y_offset;
		break;
	case SWC_PANEL_EDGE_LEFT:
		x = screen->x;
		y = screen->y + screen->height - view->height - panel->offset;
		break;
	case SWC_PANEL_EDGE_RIGHT:
		x = screen->x + screen->width - view->width;
		y = screen->y + panel->offset;
		break;
	default:
		return;
	}

	view_move(&panel->view->base, x, y);
}

static void
dock(struct wl_client *client, struct wl_resource *resource, uint32_t edge,
     struct wl_resource *screen_resource, uint32_t focus)
{
	struct panel *panel = wl_resource_get_user_data(resource);
	struct screen *screen;
	uint32_t length;

	if (screen_resource) {
		screen = wl_resource_get_user_data(screen_resource);
	} else {
		screen = wl_container_of(swc.screens.next, screen, link);
	}

	switch (edge) {
	case SWC_PANEL_EDGE_TOP:
	case SWC_PANEL_EDGE_BOTTOM:
		length = screen->base.geometry.width;
		break;
	case SWC_PANEL_EDGE_LEFT:
	case SWC_PANEL_EDGE_RIGHT:
		length = screen->base.geometry.height;
		break;
	default:
		return;
	}

	if (panel->screen && screen != panel->screen) {
		wl_list_remove(&panel->modifier.link);
		screen_update_usable_geometry(panel->screen);
	}

	panel->screen = screen;
	panel->edge = edge;
	panel->docked = true;

	update_position(panel);
	compositor_view_show(panel->view);
	raise_window_top(panel->view);
	wl_list_insert(&screen->modifiers, &panel->modifier.link);

	if (focus) {
		keyboard_set_focus(swc.seat->keyboard, panel->view);
	}

	swc_panel_send_docked(resource, length);
}

static void
set_offset(struct wl_client *client, struct wl_resource *resource,
           uint32_t offset)
{
	struct panel *panel = wl_resource_get_user_data(resource);

	panel->offset = offset;
	if (panel->docked) {
		update_position(panel);
	}
}

static void
set_y_offset(struct wl_client *client, struct wl_resource *resource,
             uint32_t offset)
{
	struct panel *panel = wl_resource_get_user_data(resource);

	panel->y_offset = offset;
	if (panel->docked) {
		update_position(panel);
	}
}

static void
set_strut(struct wl_client *client, struct wl_resource *resource, uint32_t size,
          uint32_t begin, uint32_t end)
{
	struct panel *panel = wl_resource_get_user_data(resource);

	panel->strut_size = size;
	if (panel->docked) {
		screen_update_usable_geometry(panel->screen);
	}
}

static const struct swc_panel_interface panel_impl = {
    .dock = dock,
    .set_offset = set_offset,
    .set_y_offset = set_y_offset,
    .set_strut = set_strut,
};

static void
handle_resize(struct view_handler *handler, uint32_t old_width,
              uint32_t old_height)
{
	struct panel *panel = wl_container_of(handler, panel, view_handler);
	update_position(panel);
}

static const struct view_handler_impl view_handler_impl = {
    .resize = handle_resize,
};

static void
modify(struct screen_modifier *modifier, const struct swc_rectangle *geom,
       pixman_region32_t *usable)
{
	struct panel *panel = wl_container_of(modifier, panel, modifier);
	pixman_box32_t box = {.x1 = geom->x,
	                      .y1 = geom->y,
	                      .x2 = geom->x + geom->width,
	                      .y2 = geom->y + geom->height};

	assert(panel->docked);

	DEBUG("Original geometry { x1: %d, y1: %d, x2: %d, y2: %d }\n", box.x1,
	      box.y1, box.x2, box.y2);

	switch (panel->edge) {
	case SWC_PANEL_EDGE_TOP:
		box.y1 = MAX(box.y1, geom->y + panel->strut_size);
		break;
	case SWC_PANEL_EDGE_BOTTOM:
		box.y2 = MIN(box.y2, geom->y + geom->height - panel->strut_size);
		break;
	case SWC_PANEL_EDGE_LEFT:
		box.x1 = MAX(box.x1, geom->x + panel->strut_size);
		break;
	case SWC_PANEL_EDGE_RIGHT:
		box.x2 = MIN(box.x2, geom->x + geom->width - panel->strut_size);
		break;
	}

	DEBUG("Usable region { x1: %d, y1: %d, x2: %d, y2: %d }\n", box.x1, box.y1,
	      box.x2, box.y2);

	pixman_region32_reset(usable, &box);
}

static void
destroy_panel(struct wl_resource *resource)
{
	struct panel *panel = wl_resource_get_user_data(resource);

	if (panel->docked) {
		wl_list_remove(&panel->modifier.link);
		screen_update_usable_geometry(panel->screen);
	}

	compositor_view_destroy(panel->view);
	free(panel);
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct panel *panel =
	    wl_container_of(listener, panel, surface_destroy_listener);
	wl_resource_destroy(panel->resource);
}

struct panel *
panel_new(struct wl_client *client, uint32_t version, uint32_t id,
          struct surface *surface)
{
	struct panel *panel;

	panel = malloc(sizeof(*panel));

	if (!panel) {
		goto error0;
	}

	panel->resource =
	    wl_resource_create(client, &swc_panel_interface, version, id);

	if (!panel->resource) {
		goto error0;
	}

	if (!surface_set_role(surface, panel->resource)) {
		goto error2;
	}

	if (!(panel->view = compositor_create_view(surface))) {
		goto error3;
	}

	wl_resource_set_implementation(panel->resource, &panel_impl, panel,
	                               &destroy_panel);
	panel->surface_destroy_listener.notify = &handle_surface_destroy;
	panel->view_handler.impl = &view_handler_impl;
	panel->view->always_top = true;
	panel->modifier.modify = &modify;
	panel->screen = NULL;
	panel->offset = 0;
	panel->y_offset = 0;
	panel->strut_size = 0;
	panel->docked = false;
	wl_list_insert(&panel->view->base.handlers, &panel->view_handler.link);
	wl_resource_add_destroy_listener(surface->resource,
	                                 &panel->surface_destroy_listener);

	return panel;

error3:
	wl_resource_destroy(panel->resource);
error2:
	free(panel);
error0:
	return NULL;
}
