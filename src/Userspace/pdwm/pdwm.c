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

#include <signal.h>
#include <stdbool.h>
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
	bool hidden;
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

// a bar that reserves its space after windows are up pushes any window it covers down below it
static void
screen_usable_geometry_changed(void *data)
{
	struct screen *screen = data;
	struct swc_rectangle usable = screen->swc->usable_geometry, geom;
	struct window *window;

	wl_list_for_each(window, &screen->windows, link) {
		swc_window_get_geometry(window->swc, &geom);
		if (geom.y < usable.y)
			swc_window_set_position(window->swc, geom.x, usable.y);
	}
}

static const struct swc_screen_handler screen_handler = {
    .usable_geometry_changed = &screen_usable_geometry_changed,
};

// the most recently focused shown window other than this one, the list keeps focus order
static struct window *
next_focus(struct window *window)
{
	struct window *other;

	if (!window->screen)
		return NULL;
	wl_list_for_each(other, &window->screen->windows, link) {
		if (other != window && !other->hidden)
			return other;
	}
	return NULL;
}

static void
focus_front(struct window *window)
{
	// the list runs from the most recently focused, so the next window to hand focus to is found first
	if (window->screen) {
		wl_list_remove(&window->link);
		wl_list_insert(&window->screen->windows, &window->link);
	}
	focus(window);
}

static void
window_destroy(void *data)
{
	struct window *window = data;

	// hand focus on so the desktop never ends up with no key window while windows remain
	if (focused_window == window)
		focus(next_focus(window));
	wl_list_remove(&window->link);
	free(window);
}

// AppKit hides a window by dropping its buffer, the keyboard must not stay on it
static void
window_unmapped(void *data)
{
	struct window *window = data;

	window->hidden = true;
	if (focused_window == window)
		focus(next_focus(window));
}

static void
window_mapped(void *data)
{
	struct window *window = data;
	bool was_hidden = window->hidden;

	window->hidden = false;
	if (was_hidden)
		focus_front(window);
}

static void
window_entered(void *data)
{
	focus_front(data);
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
    .unmapped = &window_unmapped,
    .mapped = &window_mapped,
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
	window->hidden = false;

	swc_window_set_handler(swc, &window_handler, window);
	swc_window_set_stacked(swc);
	/* Zero width: the frame belongs to AppKit. */
	swc_window_set_border(swc, 0, 0, 0, 0);

	if (active_screen) {
		wl_list_insert(&active_screen->windows, &window->link);
	} else {
		wl_list_init(&window->link);
	}

	// start inside the usable area, below the menu bar's exclusive zone
	if (active_screen) {
		swc_window_set_position(swc, active_screen->swc->usable_geometry.x,
		                        active_screen->swc->usable_geometry.y);
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

	// a client that quits mid-write must not take the compositor with it
	signal(SIGPIPE, SIG_IGN);

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
