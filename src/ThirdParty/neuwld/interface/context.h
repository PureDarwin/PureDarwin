/* wld: interface/context.h
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

static struct wld_renderer *context_create_renderer(struct wld_context *context);
static struct buffer *context_create_buffer(struct wld_context *context,
                                            uint32_t width, uint32_t height, uint32_t format, uint32_t flags);
static struct buffer *context_import_buffer(struct wld_context *context, uint32_t type, union wld_object object,
                                            uint32_t width, uint32_t height, uint32_t format, uint32_t pitch);
#ifdef CONTEXT_IMPLEMENTS_CREATE_SURFACE
static struct wld_surface *context_create_surface(struct wld_context *context,
                                                  uint32_t width, uint32_t height, uint32_t format, uint32_t flags);
#endif
static void context_destroy(struct wld_context *context);

static const struct wld_context_impl wld_context_impl = {
	.create_renderer = &context_create_renderer,
	.create_buffer = &context_create_buffer,
	.import_buffer = &context_import_buffer,
#ifdef CONTEXT_IMPLEMENTS_CREATE_SURFACE
	.create_surface = &context_create_surface,
#else
	.create_surface = &default_create_surface,
#endif
	.destroy = &context_destroy
};
