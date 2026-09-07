/* swc: libswc/shell_surface.c
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

#include "shell_surface.h"
#include "compositor.h"
#include "internal.h"
#include "output.h"
#include "screen.h"
#include "seat.h"
#include "surface.h"
#include "swc.h"
#include "util.h"
#include "view.h"
#include "window.h"

#include <signal.h>
#include <stdlib.h>

struct shell_surface {
	struct window window;

	struct wl_resource *resource;
	struct wl_listener surface_destroy_listener;
};

static void
configure(struct window *window, uint32_t width, uint32_t height)
{
	struct shell_surface *shell_surface =
	    wl_container_of(window, shell_surface, window);

	wl_shell_surface_send_configure(
	    shell_surface->resource, WL_SHELL_SURFACE_RESIZE_NONE, width, height);

	/* wl_shell does not support acknowledging configures. */
	window->configure.acknowledged = true;
}

static void
close_(struct window *window)
{
	struct shell_surface *shell_surface =
	    wl_container_of(window, shell_surface, window);
	struct wl_client *client;
	pid_t pid;

	client = wl_resource_get_client(shell_surface->resource);
	wl_client_get_credentials(client, &pid, NULL, NULL);
	kill(pid, SIGTERM);
}

static const struct window_impl window_impl = {
    .configure = configure,
    .close = close_,
};

static void
pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial)
{
}

static void
move(struct wl_client *client, struct wl_resource *resource,
     struct wl_resource *seat_resource, uint32_t serial)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);
	struct button *button;

	if (!(button = pointer_get_button(swc.seat->pointer, serial))) {
		return;
	}

	window_begin_move(&shell_surface->window, button);
}

static void
resize(struct wl_client *client, struct wl_resource *resource,
       struct wl_resource *seat_resource, uint32_t serial, uint32_t edges)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);
	struct button *button;

	if (!(button = pointer_get_button(swc.seat->pointer, serial))) {
		return;
	}

	window_begin_resize(&shell_surface->window, edges, button);
}

static void
set_toplevel(struct wl_client *client, struct wl_resource *resource)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);

	window_manage(&shell_surface->window);
	window_set_parent(&shell_surface->window, NULL);
}

static void
set_transient(struct wl_client *client, struct wl_resource *resource,
              struct wl_resource *parent_resource, int32_t x, int32_t y,
              uint32_t flags)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);
	struct surface *parent_surface = wl_resource_get_user_data(parent_resource);
	struct compositor_view *parent_view = compositor_view(parent_surface->view);

	if (!parent_view || !parent_view->window) {
		return;
	}

	window_manage(&shell_surface->window);
	window_set_parent(&shell_surface->window, parent_view->window);
}

static void
set_fullscreen(struct wl_client *client, struct wl_resource *resource,
               uint32_t method, uint32_t framerate,
               struct wl_resource *output_resource)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);
	struct output *output =
	    output_resource ? wl_resource_get_user_data(output_resource) : NULL;
	struct screen *screen;

	screen = output ? output->screen
	                : wl_container_of(swc.screens.next, screen, link);

	/* TODO: Handle fullscreen windows. */

	window_manage(&shell_surface->window);
	window_set_parent(&shell_surface->window, NULL);
}

static void
set_popup(struct wl_client *client, struct wl_resource *resource,
          struct wl_resource *seat_resource, uint32_t serial,
          struct wl_resource *parent_resource, int32_t x, int32_t y,
          uint32_t flags)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);
	struct surface *parent_surface = wl_resource_get_user_data(parent_resource);
	struct compositor_view *parent_view = compositor_view(parent_surface->view);

	if (!parent_view || !parent_view->window) {
		return;
	}

	window_unmanage(&shell_surface->window);
	window_set_parent(&shell_surface->window, parent_view->window);
	view_move(&shell_surface->window.view->base,
	          parent_view->base.geometry.x + x,
	          parent_view->base.geometry.y + y);
}

static void
set_maximized(struct wl_client *client, struct wl_resource *resource,
              struct wl_resource *output_resource)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);

	/* TODO: Handle maximized windows. */

	window_manage(&shell_surface->window);
	window_set_parent(&shell_surface->window, NULL);
}

static void
set_title(struct wl_client *client, struct wl_resource *resource,
          const char *title)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);
	window_set_title(&shell_surface->window, title, -1);
}

static void
set_class(struct wl_client *client, struct wl_resource *resource,
          const char *class)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);
	window_set_app_id(&shell_surface->window, class);
}

static const struct wl_shell_surface_interface shell_surface_implementation = {
    .pong = pong,
    .move = move,
    .resize = resize,
    .set_toplevel = set_toplevel,
    .set_transient = set_transient,
    .set_fullscreen = set_fullscreen,
    .set_popup = set_popup,
    .set_maximized = set_maximized,
    .set_title = set_title,
    .set_class = set_class,
};

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct shell_surface *shell_surface =
	    wl_container_of(listener, shell_surface, surface_destroy_listener);
	wl_resource_destroy(shell_surface->resource);
}

static void
destroy_shell_surface(struct wl_resource *resource)
{
	struct shell_surface *shell_surface = wl_resource_get_user_data(resource);

	window_finalize(&shell_surface->window);
	free(shell_surface);
}

struct shell_surface *
shell_surface_new(struct wl_client *client, uint32_t version, uint32_t id,
                  struct surface *surface)
{
	struct shell_surface *shell_surface;

	shell_surface = malloc(sizeof(*shell_surface));

	if (!shell_surface) {
		goto error0;
	}

	shell_surface->resource =
	    wl_resource_create(client, &wl_shell_surface_interface, version, id);

	if (!shell_surface->resource) {
		goto error1;
	}
	if (!surface_set_role(surface, shell_surface->resource)) {
		goto error2;
	}

	if (!window_initialize(&shell_surface->window, &window_impl, surface)) {
		goto error2;
	}
	wl_resource_set_implementation(shell_surface->resource,
	                               &shell_surface_implementation, shell_surface,
	                               &destroy_shell_surface);
	shell_surface->surface_destroy_listener.notify = &handle_surface_destroy;
	wl_resource_add_destroy_listener(surface->resource,
	                                 &shell_surface->surface_destroy_listener);

	return shell_surface;

error2:
	wl_resource_destroy(shell_surface->resource);
	free(shell_surface);
error1:
error0:
	return NULL;
}
