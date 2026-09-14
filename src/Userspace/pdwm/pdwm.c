/*
 * pdwm.c - the window manager half of PureDarwin's compositor.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

/*
 * swc splits a compositor in two: libswc drives hardware, input and surfaces,
 * and a manager decides policy. Policy lives here: AppKit owns window
 * geometry and draws its own frames, so windows float with no compositor
 * border, and no Super bindings are grabbed - Super is Command, and a
 * compositor Super+Q would eat every application's quit shortcut.
 */

#include <stdlib.h>
#include <wayland-server.h>
#include <swc.h>

struct screen {
	struct swc_screen *swc;
	struct wl_list windows;
};

struct window {
	struct swc_window *swc;
	struct screen *screen;
	struct wl_list link;
};

static struct wl_display *display;
static struct screen *active_screen;
static struct window *focused_window;

static void
focus(struct window *window)
{
	focused_window = window;
	swc_window_focus(window ? window->swc : NULL);
}

static void
screen_usable_geometry_changed(void *data)
{
	/* Clients place themselves against the screen and the layer-shell
	 * exclusive zones, so there is no layout here to recompute. */
	(void)data;
}

static const struct swc_screen_handler screen_handler = {
    .usable_geometry_changed = &screen_usable_geometry_changed,
};

static void
window_destroy(void *data)
{
	struct window *window = data, *next = NULL;

	if (focused_window == window) {
		/* Hand focus to a neighbour so the desktop never ends up with
		 * no key window while windows remain. */
		if (window->link.next != &window->screen->windows) {
			next = wl_container_of(window->link.next, next, link);
		} else if (window->link.prev != &window->screen->windows) {
			next = wl_container_of(window->link.prev, next, link);
		}
		focus(next);
	}

	wl_list_remove(&window->link);
	free(window);
}

static void
window_entered(void *data)
{
	focus(data);
}

static void
window_keep_floating(void *data)
{
	struct window *window = data;

	/* A move or resize must not promote the window back into a tiled
	 * layout: the application decides where its windows sit. */
	swc_window_set_stacked(window->swc);
}

static const struct swc_window_handler window_handler = {
    .destroy = &window_destroy,
    .entered = &window_entered,
    .move = &window_keep_floating,
    .resize = &window_keep_floating,
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
	window->screen = active_screen;

	swc_window_set_handler(swc, &window_handler, window);
	swc_window_set_stacked(swc);
	/* Zero width: the frame belongs to AppKit. */
	swc_window_set_border(swc, 0, 0, 0, 0);

	if (active_screen) {
		wl_list_insert(&active_screen->windows, &window->link);
	} else {
		wl_list_init(&window->link);
	}

	swc_window_show(swc);
	focus(window);
}

static const struct swc_manager manager = {&new_screen, &new_window};

int
main(int argc, char *argv[])
{
	const char *socket;

	(void)argc;
	(void)argv;

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

	/* Deliberately no swc_add_binding calls: every key belongs to the
	 * focused application. */

	wl_display_run(display);
	wl_display_destroy(display);

	return EXIT_SUCCESS;
}
