/* wld: context.c
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
context_initialize(struct wld_context *context, const struct wld_context_impl *impl)
{
	*((const struct wld_context_impl **)&context->impl) = impl;
}

EXPORT
struct wld_renderer *
wld_create_renderer(struct wld_context *context)
{
	return context->impl->create_renderer(context);
}

EXPORT
struct wld_buffer *
wld_create_buffer(struct wld_context *context,
                  uint32_t width, uint32_t height,
                  uint32_t format, uint32_t flags)
{
	return &context->impl->create_buffer(context, width, height,
	                                     format, flags)
	            ->base;
}

EXPORT
struct wld_buffer *
wld_import_buffer(struct wld_context *context,
                  uint32_t type, union wld_object object,
                  uint32_t width, uint32_t height,
                  uint32_t format, uint32_t pitch)
{
	return &context->impl->import_buffer(context, type, object,
	                                     width, height, format, pitch)
	            ->base;
}

EXPORT
struct wld_surface *
wld_create_surface(struct wld_context *context,
                   uint32_t width, uint32_t height,
                   uint32_t format, uint32_t flags)
{
	return context->impl->create_surface(context, width, height, format, flags);
}

EXPORT
void
wld_destroy_context(struct wld_context *context)
{
	context->impl->destroy(context);
}
