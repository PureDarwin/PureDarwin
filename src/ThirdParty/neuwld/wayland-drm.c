/* wld: wayland-drm.c
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

#include "drm-private.h"
#include "drm.h"
#include <wayland-client-protocol.h>
#include "wayland-drm-client-protocol.h"
#include "wayland-private.h"
#include "wayland.h"
#include "wld-private.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>

struct drm_context {
	struct wayland_context base;
	struct wld_context *driver_context;
	struct wl_drm *wl;
	struct wl_registry *registry;
	struct wl_array formats;
	uint32_t capabilities;
	int fd;
};

#define WAYLAND_IMPL_NAME drm
#include "interface/context.h"
#include "interface/wayland.h"
IMPL(drm_context, wld_context)

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version);
static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name);

static void drm_device(void *data, struct wl_drm *wl, const char *name);
static void drm_format(void *data, struct wl_drm *wl, uint32_t format);
static void drm_authenticated(void *data, struct wl_drm *wl);
static void drm_capabilities(void *data, struct wl_drm *wl,
                             uint32_t capabilities);

const static struct wl_registry_listener registry_listener = {
	.global = &registry_global,
	.global_remove = &registry_global_remove
};

const static struct wl_drm_listener drm_listener = {
	.device = &drm_device,
	.format = &drm_format,
	.authenticated = &drm_authenticated,
	.capabilities = &drm_capabilities
};

struct wayland_context *
wayland_create_context(struct wl_display *display,
                       struct wl_event_queue *queue)
{
	struct drm_context *context;

	if (!(context = malloc(sizeof *context)))
		goto error0;

	context_initialize(&context->base.base, &wld_context_impl);
	context->wl = NULL;
	context->fd = -1;
	context->capabilities = 0;
	wl_array_init(&context->formats);

	if (!(context->registry = wl_display_get_registry(display)))
		goto error1;

	wl_registry_add_listener(context->registry, &registry_listener, context);
	wl_proxy_set_queue((struct wl_proxy *)context->registry, queue);

	/* Wait for wl_drm global. */
	wl_display_roundtrip_queue(display, queue);

	if (!context->wl) {
		DEBUG("No wl_drm global\n");
		goto error2;
	}

	wl_drm_add_listener(context->wl, &drm_listener, context);

	/* Wait for DRM capabilities and device. */
	wl_display_roundtrip_queue(display, queue);

	if (!(context->capabilities & WL_DRM_CAPABILITY_PRIME)) {
		DEBUG("No PRIME support\n");
		goto error3;
	}

	if (context->fd == -1) {
		DEBUG("No DRM device\n");
		goto error3;
	}

	if (!(context->driver_context = wld_drm_create_context(context->fd))) {
		DEBUG("Couldn't initialize context for DRM device\n");
		goto error4;
	}

	return &context->base;

error4:
	close(context->fd);
error3:
	wl_drm_destroy(context->wl);
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
	struct drm_context *context = drm_context(base);
	uint32_t *supported_format;

	wl_array_for_each (supported_format, &context->formats) {
		if (*supported_format == format)
			return true;
	}

	return false;
}

EXPORT
int
wld_wayland_drm_get_fd(struct wld_context *base)
{
	struct drm_context *context = drm_context(base);

	return context->fd;
}

struct wld_renderer *
context_create_renderer(struct wld_context *base)
{
	struct drm_context *context = drm_context(base);

	return wld_create_renderer(context->driver_context);
}

struct buffer *
context_create_buffer(struct wld_context *base,
                      uint32_t width, uint32_t height,
                      uint32_t format, uint32_t flags)
{
	struct drm_context *context = drm_context(base);
	struct buffer *buffer;
	union wld_object object;
	struct wl_buffer *wl;

	if (!wayland_has_format(base, format))
		goto error0;

	buffer = context->driver_context->impl->create_buffer(context->driver_context, width, height, format, flags);

	if (!buffer)
		goto error0;

	if (!wld_export(&buffer->base, WLD_DRM_OBJECT_PRIME_FD, &object))
		goto error1;

	wl = wl_drm_create_prime_buffer(context->wl, object.i, width, height,
	                                format, 0, buffer->base.pitch, 0, 0, 0, 0);
	close(object.i);

	if (!wl)
		goto error1;

	if (!wayland_buffer_add_exporter(buffer, wl))
		goto error2;

	return buffer;

error2:
	wl_buffer_destroy(wl);
error1:
	wld_buffer_unreference(&buffer->base);
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
	struct drm_context *context = drm_context(base);

	wld_destroy_context(context->driver_context);
	close(context->fd);
	wl_drm_destroy(context->wl);
	wl_registry_destroy(context->registry);
	wl_array_release(&context->formats);
	wl_event_queue_destroy(context->base.queue);
	free(context);
}

void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
                const char *interface, uint32_t version)
{
	struct drm_context *context = data;

	if (strcmp(interface, "wl_drm") == 0 && version >= 2)
		context->wl = wl_registry_bind(registry, name, &wl_drm_interface, 2);
}

void
registry_global_remove(void *data, struct wl_registry *registry,
                       uint32_t name)
{
}

void
drm_device(void *data, struct wl_drm *wl, const char *name)
{
	struct drm_context *context = data;

	context->fd = open(name, O_RDWR);
	if (context->fd == -1) {
		DEBUG("Couldn't open DRM device '%s'\n", name);
		return;
	}
}

void
drm_format(void *data, struct wl_drm *wl, uint32_t format)
{
	struct drm_context *context = data;

	*((uint32_t *)wl_array_add(&context->formats, sizeof format)) = format;
}

void
drm_authenticated(void *data, struct wl_drm *wl)
{
}

void
drm_capabilities(void *data, struct wl_drm *wl, uint32_t capabilities)
{
	struct drm_context *context = data;

	context->capabilities = capabilities;
}
