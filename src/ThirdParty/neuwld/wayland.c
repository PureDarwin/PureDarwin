/* wld: wayland.c
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

#include "wayland.h"
#include "wayland-private.h"
#include "wld-private.h"

#include <stdlib.h>
#include <wayland-client.h>

struct wayland_buffer {
	struct wld_exporter exporter;
	struct wld_destructor destructor;
	struct wl_buffer *wl;
};

struct wayland_buffer_socket {
	struct buffer_socket base;
	struct wl_buffer_listener listener;
	struct wld_surface *surface;
	struct wl_surface *wl;
	struct wl_display *display;
	struct wl_event_queue *queue;
};

static bool buffer_socket_attach(struct buffer_socket *socket,
                                 struct buffer *buffer);
static void buffer_socket_process(struct buffer_socket *socket);
static void buffer_socket_destroy(struct buffer_socket *socket);

static const struct buffer_socket_impl buffer_socket_impl = {
	.attach = &buffer_socket_attach,
	.process = &buffer_socket_process,
	.destroy = &buffer_socket_destroy
};

IMPL(wayland_buffer_socket, buffer_socket)

static void buffer_release(void *data, struct wl_buffer *buffer);

const static struct wayland_impl *impls[] = {
#if WITH_WAYLAND_DRM
	[WLD_DRM] = &drm_wayland_impl,
#endif

#if WITH_WAYLAND_SHM
	[WLD_SHM] = &shm_wayland_impl,
#endif
};

enum wld_wayland_interface_id
interface_id(const char *string)
{
	if (strcmp(string, "drm") == 0)
		return WLD_DRM;
	if (strcmp(string, "shm") == 0)
		return WLD_SHM;

	fprintf(stderr, "Unknown Wayland interface specified: '%s'\n", string);

	return WLD_NONE;
}

EXPORT
struct wld_context *
wld_wayland_create_context(struct wl_display *display, enum wld_wayland_interface_id id, ...)
{
	struct wayland_context *context = NULL;
	struct wl_event_queue *queue;
	va_list requested_impls;
	bool impls_tried[ARRAY_LENGTH(impls)] = { 0 };
	const char *interface_string;

	if (!(queue = wl_display_create_queue(display)))
		return NULL;

	if ((interface_string = getenv("WLD_WAYLAND_INTERFACE"))) {
		id = interface_id(interface_string);

		if ((context = impls[id]->create_context(display, queue)))
			return &context->base;

		fprintf(stderr, "Could not create context for Wayland interface '%s'\n",
		        interface_string);

		return NULL;
	}

	va_start(requested_impls, id);

	while (id >= 0) {
		if (impls_tried[id] || !impls[id])
			continue;

		if ((context = impls[id]->create_context(display, queue)))
			goto done;

		impls_tried[id] = true;
		id = va_arg(requested_impls, enum wld_wayland_interface_id);
	}

	va_end(requested_impls);

	/* If the user specified WLD_ANY, try any remaining implementations. */
	if (!context && id == WLD_ANY) {
		for (id = 0; id < ARRAY_LENGTH(impls); ++id) {
			if (impls_tried[id] || !impls[id])
				continue;

			if ((context = impls[id]->create_context(display, queue)))
				break;
		}
	}

	if (!context) {
		DEBUG("Could not initialize any of the specified implementations\n");
		return NULL;
	}

done:
	context->impl = impls[id];
	context->display = display;
	context->queue = queue;

	return &context->base;
}

EXPORT
struct wld_surface *
wld_wayland_create_surface(struct wld_context *context,
                           uint32_t width, uint32_t height,
                           uint32_t format, uint32_t flags,
                           struct wl_surface *wl)
{
	struct wayland_buffer_socket *socket;

	if (!(socket = malloc(sizeof *socket)))
		goto error0;

	socket->base.impl = &buffer_socket_impl;
	socket->listener.release = &buffer_release;
	socket->wl = wl;
	socket->queue = ((struct wayland_context *)context)->queue;
	socket->display = ((struct wayland_context *)context)->display;
	socket->surface = buffered_surface_create(context, width, height, format,
	                                          flags, &socket->base);

	if (!socket->surface)
		goto error1;

	return socket->surface;

error1:
	free(socket);
error0:
	return NULL;
}

EXPORT
bool
wld_wayland_has_format(struct wld_context *base, uint32_t format)
{
	struct wayland_context *context = (void *)base;

	return context->impl->has_format(base, format);
}

static bool
buffer_export(struct wld_exporter *exporter,
              struct wld_buffer *buffer,
              uint32_t type, union wld_object *object)
{
	struct wayland_buffer *wayland_buffer = CONTAINER_OF(exporter, struct wayland_buffer, exporter);

	switch (type) {
	case WLD_WAYLAND_OBJECT_BUFFER:
		object->ptr = wayland_buffer->wl;
		return true;
	default:
		return false;
	}
}

static void
buffer_destroy(struct wld_destructor *destructor)
{
	struct wayland_buffer *wayland_buffer = CONTAINER_OF(destructor, struct wayland_buffer, destructor);

	wl_buffer_destroy(wayland_buffer->wl);
	free(wayland_buffer);
}

bool
wayland_buffer_add_exporter(struct buffer *buffer, struct wl_buffer *wl)
{
	struct wayland_buffer *wayland_buffer;

	if (!(wayland_buffer = malloc(sizeof *wayland_buffer)))
		return false;

	wayland_buffer->wl = wl;
	wayland_buffer->exporter.export = &buffer_export;
	wld_buffer_add_exporter(&buffer->base, &wayland_buffer->exporter);
	wayland_buffer->destructor.destroy = &buffer_destroy;
	wld_buffer_add_destructor(&buffer->base, &wayland_buffer->destructor);

	return true;
}

bool
buffer_socket_attach(struct buffer_socket *base, struct buffer *buffer)
{
	struct wayland_buffer_socket *socket = wayland_buffer_socket(base);
	struct wl_buffer *wl;
	union wld_object object;

	if (!wld_export(&buffer->base, WLD_WAYLAND_OBJECT_BUFFER, &object))
		return false;

	wl = object.ptr;

	if (!wl_proxy_get_listener((struct wl_proxy *)wl))
		wl_buffer_add_listener(wl, &socket->listener, buffer);

	wl_surface_attach(socket->wl, wl, 0, 0);

	if (pixman_region32_not_empty(&buffer->base.damage)) {
		pixman_box32_t *box;
		int num_boxes;

		box = pixman_region32_rectangles(&buffer->base.damage, &num_boxes);

		while (num_boxes--) {
			wl_surface_damage(socket->wl, box->x1, box->y1,
			                  box->x2 - box->x1, box->y2 - box->y1);
		}
	}

	wl_surface_commit(socket->wl);

	return true;
}

void
buffer_socket_process(struct buffer_socket *base)
{
	struct wayland_buffer_socket *socket = wayland_buffer_socket(base);

	/* Since events for our wl_buffers lie in a special queue used by WLD, we
	 * must dispatch these events here so that we see any release events before
	 * the next back buffer is chosen. */
	wl_display_dispatch_queue_pending(socket->display, socket->queue);
}

void
buffer_socket_destroy(struct buffer_socket *socket)
{
	free(socket);
}

void
buffer_release(void *data, struct wl_buffer *wl)
{
	struct wld_buffer *buffer = data;
	const struct wl_buffer_listener *listener = wl_proxy_get_listener((struct wl_proxy *)wl);
	struct wayland_buffer_socket *socket = CONTAINER_OF(listener, struct wayland_buffer_socket, listener);

	wld_surface_release(socket->surface, buffer);
}
