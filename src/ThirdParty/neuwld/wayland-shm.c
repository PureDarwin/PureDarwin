/* wld: wayland-shm.c
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

#define _GNU_SOURCE /* Required for mkostemp */

#include "pixman.h"
#include "wayland-private.h"
#include "wayland.h"
#include "wld-private.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

static int
wldtmp(char *name)
{
	int fd;
	int flags;

	fd = mkstemp(name);
	if (fd < 0)
		return -1;

	flags = fcntl(fd, F_GETFD);
	if (flags < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

struct shm_context {
	struct wayland_context base;
	struct wl_registry *registry;
	struct wl_shm *wl;
	struct wl_array formats;
};

struct shm_buffer {
	struct buffer base;
	int fd;
};

#define WAYLAND_IMPL_NAME shm
#include "interface/buffer.h"
#include "interface/context.h"
#include "interface/wayland.h"
IMPL(shm_context, wld_context)
IMPL(shm_buffer, wld_buffer)

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version);
static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name);

static void shm_format(void *data, struct wl_shm *wl, uint32_t format);

const static struct wl_registry_listener registry_listener = {
	.global = &registry_global,
	.global_remove = &registry_global_remove
};

const static struct wl_shm_listener shm_listener = {
	.format = &shm_format,
};

static inline uint32_t
format_wld_to_shm(uint32_t format)
{
	switch (format) {
	case WLD_FORMAT_ARGB8888:
		return WL_SHM_FORMAT_ARGB8888;
	case WLD_FORMAT_XRGB8888:
		return WL_SHM_FORMAT_XRGB8888;
	default:
		return 0;
	}
}

struct wayland_context *
wayland_create_context(struct wl_display *display,
                       struct wl_event_queue *queue)
{
	struct shm_context *context;

	if (!(context = malloc(sizeof *context)))
		goto error0;

	context_initialize(&context->base.base, &wld_context_impl);
	context->wl = NULL;
	wl_array_init(&context->formats);

	if (!(context->registry = wl_display_get_registry(display))) {
		DEBUG("Couldn't get registry\n");
		goto error1;
	}

	wl_registry_add_listener(context->registry, &registry_listener, context);
	wl_proxy_set_queue((struct wl_proxy *)context->registry, queue);

	/* Wait for wl_shm global. */
	wl_display_roundtrip_queue(display, queue);

	if (!context->wl) {
		DEBUG("No wl_shm global\n");
		goto error2;
	}

	wl_shm_add_listener(context->wl, &shm_listener, context);

	/* Wait for SHM formats. */
	wl_display_roundtrip_queue(display, queue);

	return &context->base;

error2:
	wl_registry_destroy(context->registry);
error1:
	wl_array_release(&context->formats);
	free(context);
error0:
	return NULL;
}

bool
wayland_has_format(struct wld_context *base, uint32_t format)
{
	struct shm_context *context = shm_context(base);
	uint32_t *supported_format;
	uint32_t shm_format = format_wld_to_shm(format);

	wl_array_for_each (supported_format, &context->formats) {
		if (*supported_format == shm_format)
			return true;
	}

	return false;
}

struct wld_renderer *
context_create_renderer(struct wld_context *context)
{
	return wld_create_renderer(wld_pixman_context);
}

struct buffer *
context_create_buffer(struct wld_context *base,
                      uint32_t width, uint32_t height,
                      uint32_t format, uint32_t flags)
{
	struct shm_context *context = shm_context(base);
	struct shm_buffer *buffer;
	char name[] = "/tmp/wld-XXXXXX";
	uint32_t pitch = width * format_bytes_per_pixel(format);
	size_t size = pitch * height;
	int fd;
	struct wl_shm_pool *pool;
	struct wl_buffer *wl;

	if (!wayland_has_format(base, format))
		goto error0;

	if (!(buffer = malloc(sizeof *buffer)))
		goto error0;

	fd = wldtmp(name);

	if (fd < 0)
		goto error1;

	unlink(name);

#if defined(__linux__)
	/* try to preallocate */
	if (posix_fallocate(fd, 0, size) != 0 && ftruncate(fd, size) != 0)
		goto error2;
#else
	/* ftruncate() is enough otherwise */
	if (ftruncate(fd, size) != 0)
		goto error2;
#endif

	if (!(pool = wl_shm_create_pool(context->wl, fd, size)))
		goto error2;

	wl = wl_shm_pool_create_buffer(pool, 0, width, height, pitch,
	                               format_wld_to_shm(format));
	wl_shm_pool_destroy(pool);

	if (!wl)
		goto error2;

	buffer_initialize(&buffer->base, &wld_buffer_impl,
	                  width, height, format, pitch);
	buffer->fd = fd;

	if (!(wayland_buffer_add_exporter(&buffer->base, wl)))
		goto error3;

	return &buffer->base;

error3:
	wl_buffer_destroy(wl);
error2:
	close(fd);
error1:
	free(buffer);
error0:
	return NULL;
}

struct buffer *
context_import_buffer(struct wld_context *context,
                      uint32_t type, union wld_object object,
                      uint32_t width, uint32_t height,
                      uint32_t format, uint32_t pitch)
{
	return NULL;
}

void
context_destroy(struct wld_context *base)
{
	struct shm_context *context = shm_context(base);

	wl_shm_destroy(context->wl);
	wl_registry_destroy(context->registry);
	wl_array_release(&context->formats);
	wl_event_queue_destroy(context->base.queue);
	free(context);
}

/**** Buffer ****/

bool
buffer_map(struct buffer *base)
{
	struct shm_buffer *buffer = shm_buffer(&base->base);
	void *data;

	data = mmap(NULL, buffer->base.base.pitch * buffer->base.base.height,
	            PROT_READ | PROT_WRITE, MAP_SHARED, buffer->fd, 0);

	if (data == MAP_FAILED)
		return false;

	buffer->base.base.map = data;

	return true;
}

bool
buffer_unmap(struct buffer *buffer)
{
	if (munmap(buffer->base.map,
	           buffer->base.pitch * buffer->base.height)
	    == -1) {
		return false;
	}

	buffer->base.map = NULL;

	return true;
}

void
buffer_destroy(struct buffer *base)
{
	struct shm_buffer *buffer = shm_buffer(&base->base);

	close(buffer->fd);
	free(buffer);
}

void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
                const char *interface, uint32_t version)
{
	struct shm_context *context = data;

	if (strcmp(interface, "wl_shm") == 0)
		context->wl = wl_registry_bind(registry, name, &wl_shm_interface, 1);
}

void
registry_global_remove(void *data, struct wl_registry *registry,
                       uint32_t name)
{
}

void
shm_format(void *data, struct wl_shm *wl, uint32_t format)
{
	struct shm_context *context = data;
	uint32_t *added_format;

	if (!(added_format = wl_array_add(&context->formats, sizeof format)))
		return;
	*added_format = format;
}
