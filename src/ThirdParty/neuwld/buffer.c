/* wld: buffer.c
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

void
buffer_initialize(struct buffer *buffer,
                  const struct wld_buffer_impl *impl,
                  uint32_t width, uint32_t height,
                  uint32_t format, uint32_t pitch)
{
	*((const struct wld_buffer_impl **)&buffer->base.impl) = impl;
	buffer->base.width = width;
	buffer->base.height = height;
	buffer->base.format = format;
	buffer->base.pitch = pitch;
	buffer->base.map = NULL;
	buffer->ref = 1;
	buffer->map_ref = 0;
	buffer->exporters = NULL;
	buffer->destructors = NULL;
	pixman_region32_init_rect(&buffer->base.damage, 0, 0, width, height);
}

EXPORT
bool
wld_map(struct wld_buffer *base)
{
	struct buffer *buffer = (void *)base;

	if (buffer->map_ref == 0 && !buffer->base.impl->map(buffer))
		return false;

	++buffer->map_ref;
	return true;
}

EXPORT
bool
wld_unmap(struct wld_buffer *base)
{
	struct buffer *buffer = (void *)base;

	if (buffer->map_ref == 0 || (buffer->map_ref == 1 && !buffer->base.impl->unmap(buffer)))
		return false;

	--buffer->map_ref;
	return true;
}

EXPORT
bool
wld_export(struct wld_buffer *base, uint32_t type, union wld_object *object)
{
	struct buffer *buffer = (void *)base;
	struct wld_exporter *exporter;

	for (exporter = buffer->exporters; exporter; exporter = exporter->next) {
		if (exporter->export(exporter, &buffer->base, type, object))
			return true;
	}

	return false;
}

EXPORT
void
wld_buffer_add_exporter(struct wld_buffer *base, struct wld_exporter *exporter)
{
	struct buffer *buffer = (void *)base;

	exporter->next = buffer->exporters;
	buffer->exporters = exporter;
}

EXPORT
void
wld_buffer_add_destructor(struct wld_buffer *base, struct wld_destructor *destructor)
{
	struct buffer *buffer = (void *)base;

	destructor->next = buffer->destructors;
	buffer->destructors = destructor;
}

EXPORT
void
wld_buffer_reference(struct wld_buffer *base)
{
	struct buffer *buffer = (void *)base;

	++buffer->ref;
}

EXPORT
void
wld_buffer_unreference(struct wld_buffer *base)
{
	struct buffer *buffer = (void *)base;
	struct wld_destructor *destructor, *next;

	if (--buffer->ref > 0)
		return;

	pixman_region32_fini(&buffer->base.damage);

	for (destructor = buffer->destructors; destructor; destructor = next) {
		next = destructor->next;
		destructor->destroy(destructor);
	}

	if (buffer->map_ref > 0)
		buffer->base.impl->unmap(buffer);

	buffer->base.impl->destroy(buffer);
}
