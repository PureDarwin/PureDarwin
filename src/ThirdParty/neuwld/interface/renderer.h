/* wld: interface/drawable.h
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

static uint32_t renderer_capabilities(struct wld_renderer *renderer,
                                      struct buffer *buffer);
static bool renderer_set_target(struct wld_renderer *renderer,
                                struct buffer *buffer);
static void renderer_fill_rectangle(struct wld_renderer *renderer,
                                    uint32_t color, int32_t x, int32_t y,
                                    uint32_t width, uint32_t height);
static void renderer_copy_rectangle(struct wld_renderer *renderer,
                                    struct buffer *buffer,
                                    int32_t dst_x, int32_t dst_y,
                                    int32_t src_x, int32_t src_y,
                                    uint32_t width, uint32_t height);

#ifdef RENDERER_IMPLEMENTS_REGION
static void renderer_fill_region(struct wld_renderer *base, uint32_t color,
                                 pixman_region32_t *region);
static void renderer_copy_region(struct wld_renderer *base,
                                 struct buffer *buffer,
                                 int32_t dst_x, int32_t dst_y,
                                 pixman_region32_t *region);
#endif
static void renderer_draw_text(struct wld_renderer *renderer,
                               struct font *font, uint32_t color,
                               int32_t x, int32_t y,
                               const char *text, uint32_t length,
                               struct wld_extents *extents);
static void renderer_flush(struct wld_renderer *renderer);
static void renderer_destroy(struct wld_renderer *renderer);

static const struct wld_renderer_impl wld_renderer_impl = {
	.capabilities = &renderer_capabilities,
	.set_target = &renderer_set_target,
	.fill_rectangle = &renderer_fill_rectangle,
	.copy_rectangle = &renderer_copy_rectangle,
#ifdef RENDERER_IMPLEMENTS_REGION
	.fill_region = &renderer_fill_region,
	.copy_region = &renderer_copy_region,
#else
	.fill_region = &default_fill_region,
	.copy_region = &default_copy_region,
#endif
	.draw_circle = &wld_draw_circle,
	.draw_line = &wld_draw_line,
	.draw_text = &renderer_draw_text,
	.flush = &renderer_flush,
	.destroy = &renderer_destroy
};
