/* wld: surface.c
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

struct wld_surface *
default_create_surface(struct wld_context *context,
                       uint32_t width, uint32_t height,
                       uint32_t format, uint32_t flags)
{
	return buffered_surface_create(context, width, height, format, flags, NULL);
}

void
surface_initialize(struct wld_surface *surface, const struct wld_surface_impl *impl)
{
	*((const struct wld_surface_impl **)&surface->impl) = impl;
}

EXPORT
pixman_region32_t *
wld_surface_damage(struct wld_surface *surface, pixman_region32_t *new_damage)
{
	return surface->impl->damage(surface, new_damage);
}

EXPORT
struct wld_buffer *
wld_surface_take(struct wld_surface *surface)
{
	return &surface->impl->take(surface)->base;
}

EXPORT
void
wld_surface_release(struct wld_surface *surface, struct wld_buffer *buffer)
{
	surface->impl->release(surface, (struct buffer *)buffer);
}

EXPORT
bool
wld_swap(struct wld_surface *surface)
{
	return surface->impl->swap(surface);
}

EXPORT
void
wld_destroy_surface(struct wld_surface *surface)
{
	surface->impl->destroy(surface);
}
