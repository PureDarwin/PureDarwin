/* wld: buffered_surface.c
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

#include "wld-private.h"

#include "interface/surface.h"
IMPL(buffered_surface, wld_surface)

struct buffer_entry {
	struct buffer *buffer;
	bool busy;
};

struct buffered_surface {
	struct wld_surface base;

	struct wld_context *context;
	struct buffer_entry *entries, *back;
	unsigned entries_size, entries_capacity;

	struct buffer_socket *buffer_socket;

	uint32_t width, height;
	enum wld_format format;
	uint32_t flags;
};

struct wld_surface *
buffered_surface_create(struct wld_context *context, uint32_t width, uint32_t height,
                        uint32_t format, uint32_t flags, struct buffer_socket *buffer_socket)
{
	struct buffered_surface *surface;

	if (!(surface = malloc(sizeof *surface)))
		return NULL;

	surface_initialize(&surface->base, &wld_surface_impl);
	surface->context = context;
	surface->entries = NULL;
	surface->back = NULL;
	surface->entries_size = 0;
	surface->entries_capacity = 0;
	surface->buffer_socket = buffer_socket;
	surface->width = width;
	surface->height = height;
	surface->format = format;
	surface->flags = flags;

	return &surface->base;
}

pixman_region32_t *
surface_damage(struct wld_surface *base, pixman_region32_t *new_damage)
{
	struct buffered_surface *surface = buffered_surface(base);
	struct buffer *back_buffer;
	unsigned index;

	if (pixman_region32_not_empty(new_damage)) {
		for (index = 0; index < surface->entries_size; ++index) {
			pixman_region32_union(&surface->entries[index].buffer->base.damage,
			                      &surface->entries[index].buffer->base.damage,
			                      new_damage);
		}
	}

	if (!(back_buffer = surface_back(base)))
		return NULL;

	return &back_buffer->base.damage;
}

struct buffer *
surface_back(struct wld_surface *base)
{
	struct buffered_surface *surface = buffered_surface(base);
	unsigned index;

	if (surface->back)
		return surface->back->buffer;

	/* The buffer socket may need to process any incoming buffer releases. */
	if (surface->buffer_socket)
		surface->buffer_socket->impl->process(surface->buffer_socket);

	for (index = 0; index < surface->entries_size; ++index) {
		if (!surface->entries[index].busy) {
			surface->back = &surface->entries[index];
			return surface->back->buffer;
		}
	}

	/* If there are no free buffers, we need to allocate another one. */
	struct buffer *buffer;

	buffer = surface->context->impl->create_buffer(surface->context, surface->width, surface->height,
	                                               surface->format, surface->flags);

	if (!buffer)
		goto error0;

	if (surface->entries_size == surface->entries_capacity) {
		struct buffer_entry *new_entries;
		size_t new_capacity = surface->entries_capacity * 2 + 1;

		new_entries = realloc(surface->entries,
		                      new_capacity * sizeof surface->entries[0]);

		if (!new_entries)
			goto error1;

		surface->entries = new_entries;
		surface->entries_capacity = new_capacity;
	}

	surface->back = &surface->entries[surface->entries_size++];
	*surface->back = (struct buffer_entry){
		.buffer = buffer,
		.busy = false
	};

	return buffer;

error1:
	wld_buffer_unreference(&buffer->base);
error0:
	return NULL;
}

struct buffer *
surface_take(struct wld_surface *base)
{
	struct buffered_surface *surface = buffered_surface(base);
	struct buffer *buffer;

	if (!(buffer = surface_back(base)))
		return NULL;

	surface->back->busy = true;
	surface->back = NULL;
	pixman_region32_clear(&buffer->base.damage);

	return buffer;
}

bool
surface_release(struct wld_surface *base, struct buffer *buffer)
{
	struct buffered_surface *surface = buffered_surface(base);
	unsigned index;

	for (index = 0; index < surface->entries_size; ++index) {
		if (surface->entries[index].buffer == buffer) {
			surface->entries[index].busy = false;
			return true;
		}
	}

	return false;
}

bool
surface_swap(struct wld_surface *base)
{
	struct buffered_surface *surface = buffered_surface(base);
	struct buffer *buffer;

	if (!surface->buffer_socket)
		return false;

	if (!(buffer = surface_back(base)))
		return false;

	if (!surface->buffer_socket->impl->attach(surface->buffer_socket, buffer))
		return false;

	surface->back->busy = true;
	surface->back = NULL;
	pixman_region32_clear(&buffer->base.damage);

	return true;
}

void
surface_destroy(struct wld_surface *base)
{
	struct buffered_surface *surface = buffered_surface(base);
	unsigned index;

	if (surface->buffer_socket)
		surface->buffer_socket->impl->destroy(surface->buffer_socket);

	for (index = 0; index < surface->entries_size; ++index)
		wld_buffer_unreference(&surface->entries[index].buffer->base);

	free(surface->entries);
	free(surface);
}
