/* swc: libswc/xwm.c
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

#include "xwm.h"
#include "compositor.h"
#include "internal.h"
#include "surface.h"
#include "swc.h"
#include "util.h"
#include "view.h"
#include "window.h"
#include "xserver.h"

#include <stdio.h>
#include <xcb/composite.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

struct xwl_window {
	xcb_window_t id;
	uint32_t surface_id;
	bool override_redirect, supports_delete;
	struct wl_list link;

	/* Only used for paired windows. */
	struct {
		struct window window;
		struct wl_listener surface_destroy_listener;
	};
};

enum atom {
	ATOM_WL_SURFACE_ID,
	ATOM_WM_DELETE_WINDOW,
	ATOM_WM_PROTOCOLS,
	ATOM_WM_S0,
};

static struct {
	xcb_connection_t *connection;
	xcb_ewmh_connection_t ewmh;
	xcb_screen_t *screen;
	xcb_window_t window;
	struct xwl_window *focus;
	struct wl_event_source *source;
	struct wl_list windows, unpaired_windows;
	union {
		const char *name;
		xcb_intern_atom_cookie_t cookie;
		xcb_atom_t value;
	} atoms[4];
} xwm = {.atoms = {
             [ATOM_WL_SURFACE_ID] = {"WL_SURFACE_ID"},
             [ATOM_WM_DELETE_WINDOW] = {"WM_DELETE_WINDOW"},
             [ATOM_WM_PROTOCOLS] = {"WM_PROTOCOLS"},
             [ATOM_WM_S0] = {"WM_S0"},
         }};

static void
update_name(struct xwl_window *xwl_window)
{
	xcb_get_property_cookie_t wm_name_cookie;
	xcb_ewmh_get_utf8_strings_reply_t wm_name_reply;

	wm_name_cookie = xcb_ewmh_get_wm_name(&xwm.ewmh, xwl_window->id);

	if (xcb_ewmh_get_wm_name_reply(&xwm.ewmh, wm_name_cookie, &wm_name_reply,
	                               NULL)) {
		window_set_title(&xwl_window->window, wm_name_reply.strings,
		                 wm_name_reply.strings_len);
		xcb_ewmh_get_utf8_strings_reply_wipe(&wm_name_reply);
	} else {
		window_set_title(&xwl_window->window, NULL, 0);
	}
}

static void
update_protocols(struct xwl_window *xwl_window)
{
	xcb_get_property_cookie_t cookie;
	xcb_icccm_get_wm_protocols_reply_t reply;
	unsigned index;

	cookie = xcb_icccm_get_wm_protocols(xwm.connection, xwl_window->id,
	                                    xwm.atoms[ATOM_WM_PROTOCOLS].value);
	xwl_window->supports_delete = true;

	if (!xcb_icccm_get_wm_protocols_reply(xwm.connection, cookie, &reply,
	                                      NULL)) {
		return;
	}

	for (index = 0; index < reply.atoms_len; ++index) {
		if (reply.atoms[index] == xwm.atoms[ATOM_WM_DELETE_WINDOW].value) {
			xwl_window->supports_delete = true;
		}
	}

	xcb_icccm_get_wm_protocols_reply_wipe(&reply);
}

static struct xwl_window *
find_window(struct wl_list *list, xcb_window_t id)
{
	struct xwl_window *window;

	wl_list_for_each(window, list, link)
	{
		if (window->id == id) {
			return window;
		}
	}

	return NULL;
}

static struct xwl_window *
find_window_by_surface_id(struct wl_list *list, uint32_t id)
{
	struct xwl_window *window;

	wl_list_for_each(window, list, link)
	{
		if (window->surface_id == id) {
			return window;
		}
	}

	return NULL;
}

static void
move(struct window *window, int32_t x, int32_t y)
{
	uint32_t mask, values[2];
	struct xwl_window *xwl_window = wl_container_of(window, xwl_window, window);

	mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
	values[0] = x;
	values[1] = y;

	xcb_configure_window(xwm.connection, xwl_window->id, mask, values);
	xcb_flush(xwm.connection);
}

static void
configure(struct window *window, uint32_t width, uint32_t height)
{
	uint32_t mask, values[2];
	struct xwl_window *xwl_window = wl_container_of(window, xwl_window, window);

	mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
	values[0] = width;
	values[1] = height;

	window->configure.acknowledged = true;
	xcb_configure_window(xwm.connection, xwl_window->id, mask, values);
	xcb_flush(xwm.connection);
}

static void
focus(struct window *window)
{
	struct xwl_window *xwl_window = wl_container_of(window, xwl_window, window);

	xcb_set_input_focus(xwm.connection, XCB_INPUT_FOCUS_NONE, xwl_window->id,
	                    XCB_CURRENT_TIME);
	xcb_flush(xwm.connection);
	xwm.focus = xwl_window;
}

static void
unfocus(struct window *window)
{
	struct xwl_window *xwl_window = wl_container_of(window, xwl_window, window);

	/* If the window we are unfocusing is the latest xwl_window to be focused,
	 * we know we have transitioned to some other window type, so the X11 focus
	 * can be set to XCB_NONE. Otherwise, we have transitioned to another X11
	 * window, and the X11 focus has already been updated. */
	if (xwl_window == xwm.focus) {
		xcb_set_input_focus(xwm.connection, XCB_INPUT_FOCUS_NONE, XCB_NONE,
		                    XCB_CURRENT_TIME);
		xcb_flush(xwm.connection);
	}
}

static void
close_(struct window *window)
{
	struct xwl_window *xwl_window = wl_container_of(window, xwl_window, window);

	if (xwl_window->supports_delete) {
		xcb_client_message_event_t event = {
		    .response_type = XCB_CLIENT_MESSAGE,
		    .format = 32,
		    .window = xwl_window->id,
		    .type = xwm.atoms[ATOM_WM_PROTOCOLS].value,
		    .data.data32 =
		        {
		            xwm.atoms[ATOM_WM_DELETE_WINDOW].value,
		            XCB_CURRENT_TIME,
		        },
		};

		xcb_send_event(xwm.connection, false, xwl_window->id,
		               XCB_EVENT_MASK_NO_EVENT, (const char *)&event);
	} else {
		xcb_kill_client(xwm.connection, xwl_window->id);
	}

	xcb_flush(xwm.connection);
}

static const struct window_impl xwl_window_handler = {
    .move = move,
    .configure = configure,
    .focus = focus,
    .unfocus = unfocus,
    .close = close_,
};

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct xwl_window *xwl_window =
	    wl_container_of(listener, xwl_window, surface_destroy_listener);

	if (xwm.focus == xwl_window) {
		xwm.focus = NULL;
	}

	window_finalize(&xwl_window->window);
	wl_list_remove(&xwl_window->link);
	wl_list_insert(&xwm.unpaired_windows, &xwl_window->link);
	xwl_window->surface_id = 0;
}

static bool
manage_window(struct xwl_window *xwl_window)
{
	struct wl_resource *resource;
	struct surface *surface;
	xcb_get_geometry_cookie_t geometry_cookie;
	xcb_get_geometry_reply_t *geometry_reply;

	resource =
	    wl_client_get_object(swc.xserver->client, xwl_window->surface_id);

	if (!resource) {
		return false;
	}

	surface = wl_resource_get_user_data(resource);
	geometry_cookie = xcb_get_geometry(xwm.connection, xwl_window->id);

	window_initialize(&xwl_window->window, &xwl_window_handler, surface);
	xwl_window->surface_destroy_listener.notify = &handle_surface_destroy;
	wl_resource_add_destroy_listener(surface->resource,
	                                 &xwl_window->surface_destroy_listener);

	if ((geometry_reply =
	         xcb_get_geometry_reply(xwm.connection, geometry_cookie, NULL))) {
		view_move(surface->view, geometry_reply->x, geometry_reply->y);
		free(geometry_reply);
	}

	if (xwl_window->override_redirect) {
		compositor_view_show(xwl_window->window.view);
	} else {
		uint32_t mask, values[1];

		mask = XCB_CW_EVENT_MASK;
		values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
		xcb_change_window_attributes(xwm.connection, xwl_window->id, mask,
		                             values);
		mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
		values[0] = 0;
		xcb_configure_window(xwm.connection, xwl_window->id, mask, values);
		update_name(xwl_window);
		update_protocols(xwl_window);
		window_manage(&xwl_window->window);
	}

	wl_list_remove(&xwl_window->link);
	wl_list_insert(&xwm.windows, &xwl_window->link);

	return true;
}

static void
handle_new_surface(struct wl_listener *listener, void *data)
{
	struct surface *surface = data;
	struct xwl_window *window;

	window = find_window_by_surface_id(&xwm.unpaired_windows,
	                                   wl_resource_get_id(surface->resource));

	if (!window) {
		return;
	}

	manage_window(window);
}

static struct wl_listener new_surface_listener = {.notify =
                                                      &handle_new_surface};

/* X event handlers */
static void
create_notify(xcb_create_notify_event_t *event)
{
	struct xwl_window *xwl_window;

	if (!(xwl_window = malloc(sizeof *xwl_window))) {
		return;
	}

	xwl_window->id = event->window;
	xwl_window->surface_id = 0;
	xwl_window->override_redirect = event->override_redirect;
	wl_list_insert(&xwm.unpaired_windows, &xwl_window->link);
}

static void
destroy_notify(xcb_destroy_notify_event_t *event)
{
	struct xwl_window *xwl_window;

	if ((xwl_window = find_window(&xwm.windows, event->window))) {
		wl_list_remove(&xwl_window->surface_destroy_listener.link);
		window_finalize(&xwl_window->window);
	} else if (!(xwl_window =
	                 find_window(&xwm.unpaired_windows, event->window))) {
		return;
	}

	wl_list_remove(&xwl_window->link);
	free(xwl_window);
}

static void
map_request(xcb_map_request_event_t *event)
{
	xcb_map_window(xwm.connection, event->window);
}

static void
configure_request(xcb_configure_request_event_t *event)
{
}

static void
property_notify(xcb_property_notify_event_t *event)
{
	struct xwl_window *xwl_window;

	if (!(xwl_window = find_window(&xwm.windows, event->window))) {
		return;
	}

	if (event->atom == xwm.ewmh._NET_WM_NAME &&
	    event->state == XCB_PROPERTY_NEW_VALUE) {
		update_name(xwl_window);
	} else if (event->atom == xwm.atoms[ATOM_WM_PROTOCOLS].value) {
		update_protocols(xwl_window);
	}
}

static void
client_message(xcb_client_message_event_t *event)
{
	if (event->type == xwm.atoms[ATOM_WL_SURFACE_ID].value) {
		struct xwl_window *xwl_window;

		if (!(xwl_window = find_window(&xwm.unpaired_windows, event->window))) {
			return;
		}

		xwl_window->surface_id = event->data.data32[0];
		manage_window(xwl_window);
	}
}

static int
connection_data(int fd, uint32_t mask, void *data)
{
	xcb_generic_event_t *event;
	uint32_t count = 0;

	while ((event = xcb_poll_for_event(xwm.connection))) {
		switch (event->response_type & ~0x80) {
		case XCB_CREATE_NOTIFY:
			create_notify((xcb_create_notify_event_t *)event);
			break;
		case XCB_DESTROY_NOTIFY:
			destroy_notify((xcb_destroy_notify_event_t *)event);
			break;
		case XCB_MAP_REQUEST:
			map_request((xcb_map_request_event_t *)event);
			break;
		case XCB_CONFIGURE_REQUEST:
			configure_request((xcb_configure_request_event_t *)event);
			break;
		case XCB_PROPERTY_NOTIFY:
			property_notify((xcb_property_notify_event_t *)event);
			break;
		case XCB_CLIENT_MESSAGE:
			client_message((xcb_client_message_event_t *)event);
			break;
		}

		free(event);
		++count;
	}

	xcb_flush(xwm.connection);

	return count;
}

bool
xwm_initialize(int fd)
{
	const xcb_setup_t *setup;
	xcb_screen_iterator_t screen_iterator;
	uint32_t mask;
	uint32_t values[1];
	xcb_void_cookie_t change_attributes_cookie, redirect_subwindows_cookie;
	xcb_generic_error_t *error;
	xcb_intern_atom_cookie_t *ewmh_cookies;
	xcb_intern_atom_reply_t *atom_reply;
	unsigned index;
	const char *name;
	const xcb_query_extension_reply_t *composite_extension;

	xwm.connection = xcb_connect_to_fd(fd, NULL);

	if (xcb_connection_has_error(xwm.connection)) {
		ERROR("xwm: Could not connect to X server\n");
		goto error0;
	}

	xcb_prefetch_extension_data(xwm.connection, &xcb_composite_id);
	ewmh_cookies = xcb_ewmh_init_atoms(xwm.connection, &xwm.ewmh);

	if (!ewmh_cookies) {
		ERROR("xwm: Failed to initialize EWMH atoms\n");
		goto error1;
	}

	for (index = 0; index < ARRAY_LENGTH(xwm.atoms); ++index) {
		name = xwm.atoms[index].name;
		xwm.atoms[index].cookie =
		    xcb_intern_atom(xwm.connection, 0, strlen(name), name);
	}

	setup = xcb_get_setup(xwm.connection);
	screen_iterator = xcb_setup_roots_iterator(setup);
	xwm.screen = screen_iterator.data;

	/* Try to select for substructure redirect. */
	mask = XCB_CW_EVENT_MASK;
	values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
	            XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
	change_attributes_cookie = xcb_change_window_attributes(
	    xwm.connection, xwm.screen->root, mask, values);

	xwm.source = wl_event_loop_add_fd(swc.event_loop, fd, WL_EVENT_READABLE,
	                                  &connection_data, NULL);
	wl_list_init(&xwm.windows);
	wl_list_init(&xwm.unpaired_windows);

	if (!xwm.source) {
		ERROR("xwm: Failed to create X connection event source\n");
		goto error2;
	}

	composite_extension =
	    xcb_get_extension_data(xwm.connection, &xcb_composite_id);

	if (!composite_extension->present) {
		ERROR("xwm: X server does not have composite extension\n");
		goto error3;
	}

	redirect_subwindows_cookie = xcb_composite_redirect_subwindows_checked(
	    xwm.connection, xwm.screen->root, XCB_COMPOSITE_REDIRECT_MANUAL);

	if ((error = xcb_request_check(xwm.connection, change_attributes_cookie))) {
		ERROR("xwm: Another window manager is running\n");
		free(error);
		goto error3;
	}

	if ((error =
	         xcb_request_check(xwm.connection, redirect_subwindows_cookie))) {
		ERROR("xwm: Could not redirect subwindows of root for compositing\n");
		free(error);
		goto error3;
	}

	xwm.window = xcb_generate_id(xwm.connection);
	xcb_create_window(xwm.connection, 0, xwm.window, xwm.screen->root, 0, 0, 1,
	                  1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT,
	                  0, NULL);

	xcb_ewmh_init_atoms_replies(&xwm.ewmh, ewmh_cookies, &error);

	if (error) {
		ERROR("xwm: Failed to get EWMH atom replies: %u\n", error->error_code);
		goto error3;
	}

	for (index = 0; index < ARRAY_LENGTH(xwm.atoms); ++index) {
		atom_reply = xcb_intern_atom_reply(xwm.connection,
		                                   xwm.atoms[index].cookie, &error);

		if (error) {
			ERROR("xwm: Failed to get atom reply: %u\n", error->error_code);
			return false;
		}

		xwm.atoms[index].value = atom_reply->atom;
		free(atom_reply);
	}

	xcb_set_selection_owner(xwm.connection, xwm.window,
	                        xwm.atoms[ATOM_WM_S0].value, XCB_CURRENT_TIME);
	xcb_flush(xwm.connection);

	wl_signal_add(&swc.compositor->signal.new_surface, &new_surface_listener);

	return true;

error3:
	wl_event_source_remove(xwm.source);
error2:
	xcb_ewmh_connection_wipe(&xwm.ewmh);
error1:
	xcb_disconnect(xwm.connection);
error0:
	return false;
}

void
xwm_finalize(void)
{
	wl_event_source_remove(xwm.source);
	xcb_ewmh_connection_wipe(&xwm.ewmh);
	xcb_disconnect(xwm.connection);
}
