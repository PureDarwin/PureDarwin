/* swc: example/wm.c
 *
 * Copyright (c) 2014 Michael Forney
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

#include <stdlib.h>
#include <swc.h>
#include <unistd.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>

struct screen {
	struct swc_screen *swc;
	struct wl_list windows;
	unsigned num_windows;
};

struct window {
	struct swc_window *swc;
	struct screen *screen;
	struct wl_list link;
};

static const char *terminal_command[] = {"st-wl", NULL};
static const char *dmenu_command[] = {"dmenu_run-wl", NULL};
static const uint32_t border_width = 1;
static const uint32_t border_color_active = 0xff333388;
static const uint32_t border_color_normal = 0xff888888;

static struct screen *active_screen;
static struct window *focused_window;
static struct wl_display *display;
static struct wl_event_loop *event_loop;

/* This is a basic grid arrange function that tries to give each window an
 * equal space. */
static void
arrange(struct screen *screen)
{
	struct window *window = NULL;
	unsigned num_columns, num_rows, column_index, row_index;
	struct swc_rectangle geometry;
	struct swc_rectangle *screen_geometry = &screen->swc->usable_geometry;

	if (screen->num_windows == 0) {
		return;
	}

	num_columns = ceil(sqrt(screen->num_windows));
	num_rows = screen->num_windows / num_columns + 1;
	window = wl_container_of(screen->windows.next, window, link);

	for (column_index = 0; &window->link != &screen->windows; ++column_index) {
		geometry.x = screen_geometry->x + border_width +
		             screen_geometry->width * column_index / num_columns;
		geometry.width =
		    screen_geometry->width / num_columns - 2 * border_width;

		if (column_index == screen->num_windows % num_columns) {
			--num_rows;
		}

		for (row_index = 0; row_index < num_rows; ++row_index) {
			geometry.y = screen_geometry->y + border_width +
			             screen_geometry->height * row_index / num_rows;
			geometry.height =
			    screen_geometry->height / num_rows - 2 * border_width;

			swc_window_set_geometry(window->swc, &geometry);
			window = wl_container_of(window->link.next, window, link);
		}
	}
}

static void
screen_add_window(struct screen *screen, struct window *window)
{
	window->screen = screen;
	wl_list_insert(&screen->windows, &window->link);
	++screen->num_windows;
	swc_window_show(window->swc);
	arrange(screen);
}

static void
screen_remove_window(struct screen *screen, struct window *window)
{
	window->screen = NULL;
	wl_list_remove(&window->link);
	--screen->num_windows;
	swc_window_hide(window->swc);
	arrange(screen);
}

static void
focus(struct window *window)
{
	if (focused_window) {
		swc_window_set_border(focused_window->swc, border_color_normal,
		                      border_width, 0, 0);
	}

	if (window) {
		swc_window_set_border(window->swc, border_color_active, border_width, 0, 0);
		swc_window_focus(window->swc);
	} else {
		swc_window_focus(NULL);
	}

	focused_window = window;
}

static void
screen_usable_geometry_changed(void *data)
{
	struct screen *screen = data;

	/* If the usable geometry of the screen changes, for example when a panel is
	 * docked to the edge of the screen, we need to rearrange the windows to
	 * ensure they are all within the new usable geometry. */
	arrange(screen);
}

static void
screen_entered(void *data)
{
	struct screen *screen = data;

	active_screen = screen;
}

static const struct swc_screen_handler screen_handler = {
    .usable_geometry_changed = &screen_usable_geometry_changed,
    .entered = &screen_entered,
};

static void
window_destroy(void *data)
{
	struct window *window = data, *next_focus;

	if (focused_window == window) {
		/* Try to find a new focus nearby the old one. */
		next_focus = wl_container_of(window->link.next, window, link);

		if (&next_focus->link == &window->screen->windows) {
			next_focus = wl_container_of(window->link.prev, window, link);

			if (&next_focus->link == &window->screen->windows) {
				next_focus = NULL;
			}
		}

		focus(next_focus);
	}

	screen_remove_window(window->screen, window);
	free(window);
}

static void
window_entered(void *data)
{
	struct window *window = data;

	focus(window);
}

static const struct swc_window_handler window_handler = {
    .destroy = &window_destroy,
    .entered = &window_entered,
};

static void
new_screen(struct swc_screen *swc)
{
	struct screen *screen;

	screen = malloc(sizeof(*screen));

	if (!screen) {
		return;
	}

	screen->swc = swc;
	screen->num_windows = 0;
	wl_list_init(&screen->windows);
	swc_screen_set_handler(swc, &screen_handler, screen);
	active_screen = screen;
}

static void
new_window(struct swc_window *swc)
{
	struct window *window;

	window = malloc(sizeof(*window));

	if (!window) {
		return;
	}

	window->swc = swc;
	window->screen = NULL;
	swc_window_set_handler(swc, &window_handler, window);
	swc_window_set_tiled(swc);
	screen_add_window(active_screen, window);
	focus(window);
}

const struct swc_manager manager = {&new_screen, &new_window};

static void
spawn(void *data, uint32_t time, uint32_t value, uint32_t state)
{
	char *const *command = data;

	if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		return;
	}

	if (fork() == 0) {
		execvp(command[0], command);
		exit(EXIT_FAILURE);
	}
}

static void
quit(void *data, uint32_t time, uint32_t value, uint32_t state)
{
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		return;
	}

	wl_display_terminate(display);
}

int
main(int argc, char *argv[])
{
	const char *socket;

	display = wl_display_create();
	if (!display) {
		return EXIT_FAILURE;
	}

	socket = wl_display_add_socket_auto(display);
	if (!socket) {
		return EXIT_FAILURE;
	}
	setenv("WAYLAND_DISPLAY", socket, 1);

	if (!swc_initialize(display, NULL, &manager)) {
		return EXIT_FAILURE;
	}

	swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO, XKB_KEY_Return, &spawn,
	                terminal_command);
	swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO, XKB_KEY_r, &spawn,
	                dmenu_command);
	swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO, XKB_KEY_q, &quit, NULL);

	event_loop = wl_display_get_event_loop(display);
	wl_display_run(display);
	wl_display_destroy(display);

	return EXIT_SUCCESS;
}
