/*
 * pddecor-test - a stand-in decorator for testing pd-decoration-v1: plain
 * coloured title strips with three squares where AppKit puts the window buttons.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>

#include "pd-decoration-v1-client-protocol.h"

#define STRIP 28
#define BUTTON 12
#define BUTTON_Y 8

// state per window, enough for a handful of test windows
struct win {
	uint32_t id, width;
	int active, used;
};

static struct wl_shm *shm;
static struct pd_decorator_v1 *decorator;
static struct win wins[64];

static struct win *
win_for(uint32_t id, int create)
{
	for (int i = 0; i < 64; i++) {
		if (wins[i].used && wins[i].id == id)
			return &wins[i];
	}
	if (!create)
		return NULL;
	for (int i = 0; i < 64; i++) {
		if (!wins[i].used) {
			memset(&wins[i], 0, sizeof(wins[i]));
			wins[i].used = 1;
			wins[i].id = id;
			return &wins[i];
		}
	}
	return NULL;
}

static void
release(void *data, struct wl_buffer *buffer)
{
	wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener buffer_listener = {.release = release};

static void
square(uint32_t *px, uint32_t stride, uint32_t x0, uint32_t colour)
{
	for (uint32_t y = BUTTON_Y; y < BUTTON_Y + BUTTON; y++) {
		for (uint32_t x = x0; x < x0 + BUTTON; x++)
			px[y * stride + x] = colour;
	}
}

// an opaque strip, blue when active and grey when not, red yellow green squares for the buttons
static void
draw(struct win *w)
{
	uint32_t width = w->width ? w->width : 1, size = width * STRIP * 4;
	char path[] = "/tmp/pddecor-test-XXXXXX";
	int fd = mkstemp(path);
	uint32_t *px;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;

	if (fd < 0)
		return;
	unlink(path);
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return;
	}
	px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (px == MAP_FAILED) {
		close(fd);
		return;
	}
	for (uint32_t i = 0; i < width * STRIP; i++)
		px[i] = w->active ? 0xff3060c0 : 0xff909090;
	if (width >= 64) {
		square(px, width, 12, 0xffff5f57);
		square(px, width, 32, 0xfffebc2e);
		square(px, width, 52, 0xff28c840);
	}
	munmap(px, size);
	pool = wl_shm_create_pool(shm, fd, size);
	buffer = wl_shm_pool_create_buffer(pool, 0, width, STRIP, width * 4, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	wl_buffer_add_listener(buffer, &buffer_listener, NULL);
	pd_decorator_v1_set_frame(decorator, w->id, buffer, STRIP, BUTTON_Y, BUTTON, 12, 32, 52);
	printf("pddecor-test: frame for %u, %u wide, %s\n", w->id, width, w->active ? "active" : "inactive");
	fflush(stdout);
}

static void
on_window(void *data, struct pd_decorator_v1 *d, uint32_t id)
{
	win_for(id, 1);
}

static void
on_title(void *data, struct pd_decorator_v1 *d, uint32_t id, const char *title)
{
	printf("pddecor-test: %u is \"%s\"\n", id, title);
}

static void
on_width(void *data, struct pd_decorator_v1 *d, uint32_t id, uint32_t width)
{
	struct win *w = win_for(id, 1);
	if (w)
		w->width = width;
}

static void
on_active(void *data, struct pd_decorator_v1 *d, uint32_t id, uint32_t active)
{
	struct win *w = win_for(id, 1);
	if (w)
		w->active = active;
}

static void
on_done(void *data, struct pd_decorator_v1 *d, uint32_t id)
{
	struct win *w = win_for(id, 0);
	if (w)
		draw(w);
}

static void
on_removed(void *data, struct pd_decorator_v1 *d, uint32_t id)
{
	struct win *w = win_for(id, 0);
	if (w)
		w->used = 0;
	printf("pddecor-test: %u removed\n", id);
}

static const struct pd_decorator_v1_listener decorator_listener = {
    .window = on_window,
    .title = on_title,
    .width = on_width,
    .active = on_active,
    .done = on_done,
    .removed = on_removed,
};

static void
global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, wl_shm_interface.name) == 0)
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	else if (strcmp(interface, pd_decorator_v1_interface.name) == 0)
		decorator = wl_registry_bind(registry, name, &pd_decorator_v1_interface, 1);
}

static void
global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = global_remove};

int
main(void)
{
	struct wl_display *display = wl_display_connect(NULL);
	struct wl_registry *registry;

	if (!display) {
		fprintf(stderr, "pddecor-test: no wayland display\n");
		return 1;
	}
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	if (!shm || !decorator) {
		fprintf(stderr, "pddecor-test: the compositor has no pd_decorator_v1\n");
		return 1;
	}
	pd_decorator_v1_add_listener(decorator, &decorator_listener, NULL);
	printf("pddecor-test: bound\n");
	fflush(stdout);
	while (wl_display_dispatch(display) != -1)
		;
	return 0;
}
