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

#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ucontext.h>
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
	swc_window_raise(window->swc);
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

// the session log gets where the compositor died, the kernel alone only reports the signal
static void
crashed(int sig, siginfo_t *info, void *context)
{
	void *frames[48];
	int count = backtrace(frames, 48);
	ucontext_t *uc = context;
	Dl_info at;
	// glibc keeps the registers in an array, Darwin in the thread state
#if defined(__linux__) && defined(__x86_64__)
	void *pc = (void *)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__APPLE__) && defined(__x86_64__)
	void *pc = (void *)uc->uc_mcontext->__ss.__rip;
#elif defined(__APPLE__) && defined(__arm64__)
	void *pc = (void *)uc->uc_mcontext->__ss.__pc;
#else
	void *pc = NULL;
#endif

	fprintf(stderr, "neuswc: signal %d at %p, pc %p\n", sig, info->si_addr, pc);
	// the handler runs on its own stack, so walk the crashed thread's frame pointers
#if defined(__APPLE__) && defined(__x86_64__)
	void **fp = (void **)uc->uc_mcontext->__ss.__rbp;
#elif defined(__linux__) && defined(__x86_64__)
	void **fp = (void **)uc->uc_mcontext.gregs[REG_RBP];
#else
	void **fp = NULL;
#endif
	for (int i = 0; fp != NULL && i < 32; i++) {
		void *ret = fp[1];

		if (dladdr(ret, &at) && at.dli_sname != NULL)
			fprintf(stderr, "  fp%-2d %s + %ld\n", i, at.dli_sname, (long)((char *)ret - (char *)at.dli_saddr));
		else
			fprintf(stderr, "  fp%-2d %p\n", i, ret);
		if ((void **)fp[0] <= fp)
			break;
		fp = (void **)fp[0];
	}
	if (pc != NULL && dladdr(pc, &at) && at.dli_sname != NULL)
		fprintf(stderr, "  pc %s + %ld (%s)\n", at.dli_sname, (long)((char *)pc - (char *)at.dli_saddr), at.dli_fname);
	for (int i = 0; i < count; i++) {
		if (dladdr(frames[i], &at) && at.dli_sname != NULL)
			fprintf(stderr, "  %2d %s + %ld (%s)\n", i, at.dli_sname, (long)((char *)frames[i] - (char *)at.dli_saddr), at.dli_fname);
		else
			fprintf(stderr, "  %2d %p\n", i, frames[i]);
	}
	fflush(stderr);
	signal(sig, SIG_DFL);
	raise(sig);
}

int
main(int argc, char *argv[])
{
	const char *socket;

	(void)argc;
	(void)argv;

	// a client that quits mid-write must not take the compositor with it
	signal(SIGPIPE, SIG_IGN);

	struct sigaction action = { 0 };
	stack_t altstack = { .ss_sp = malloc(1 << 16), .ss_size = 1 << 16 };

	sigaltstack(&altstack, NULL);
	action.sa_sigaction = crashed;
	action.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGBUS, &action, NULL);
	sigaction(SIGILL, &action, NULL);
	sigaction(SIGABRT, &action, NULL);
	sigaction(SIGFPE, &action, NULL);

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
