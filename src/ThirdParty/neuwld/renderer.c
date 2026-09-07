/* wld: renderer.c
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
default_fill_region(struct wld_renderer *renderer, uint32_t color, pixman_region32_t *region)
{
	pixman_box32_t *box;
	int num_boxes;

	box = pixman_region32_rectangles(region, &num_boxes);

	while (num_boxes--) {
		renderer->impl->fill_rectangle(renderer, color, box->x1, box->y1,
		                               box->x2 - box->x1, box->y2 - box->y1);
		++box;
	}
}

void
default_copy_region(struct wld_renderer *renderer, struct buffer *buffer,
                    int32_t dst_x, int32_t dst_y,
                    pixman_region32_t *region)
{
	pixman_box32_t *box;
	int num_boxes;

	box = pixman_region32_rectangles(region, &num_boxes);

	while (num_boxes--) {
		renderer->impl->copy_rectangle(renderer, buffer,
		                               dst_x + box->x1, dst_y + box->y1,
		                               box->x1, box->y1,
		                               box->x2 - box->x1, box->y2 - box->y1);
		++box;
	}
}

void
renderer_initialize(struct wld_renderer *renderer, const struct wld_renderer_impl *impl)
{
	*((const struct wld_renderer_impl **)&renderer->impl) = impl;
	renderer->target = NULL;
}

EXPORT
void
wld_destroy_renderer(struct wld_renderer *renderer)
{
	renderer->impl->destroy(renderer);
}

EXPORT
uint32_t
wld_capabilities(struct wld_renderer *renderer, struct wld_buffer *buffer)
{
	return renderer->impl->capabilities(renderer, (struct buffer *)buffer);
}

EXPORT
bool
wld_set_target_buffer(struct wld_renderer *renderer, struct wld_buffer *buffer)
{
	if (!renderer->impl->set_target(renderer, (struct buffer *)buffer))
		return false;

	renderer->target = buffer;

	return true;
}

EXPORT
bool
wld_set_target_surface(struct wld_renderer *renderer, struct wld_surface *surface)
{
	struct buffer *back_buffer;

	if (!(back_buffer = surface->impl->back(surface)))
		return false;

	if (!renderer->impl->set_target(renderer, back_buffer))
		return false;

	renderer->target = &back_buffer->base;
	return true;
}

EXPORT
void
wld_fill_rectangle(struct wld_renderer *renderer, uint32_t color,
                   int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	renderer->impl->fill_rectangle(renderer, color, x, y, width, height);
}

EXPORT
void
wld_fill_region(struct wld_renderer *renderer, uint32_t color, pixman_region32_t *region)
{
	renderer->impl->fill_region(renderer, color, region);
}

EXPORT
void
wld_copy_rectangle(struct wld_renderer *renderer,
                   struct wld_buffer *buffer,
                   int32_t dst_x, int32_t dst_y,
                   int32_t src_x, int32_t src_y,
                   uint32_t width, uint32_t height)
{
	renderer->impl->copy_rectangle(renderer, (struct buffer *)buffer,
	                               dst_x, dst_y, src_x, src_y, width, height);
}

EXPORT
void
wld_copy_region(struct wld_renderer *renderer,
                struct wld_buffer *buffer,
                int32_t dst_x, int32_t dst_y, pixman_region32_t *region)
{
	renderer->impl->copy_region(renderer, (struct buffer *)buffer,
	                            dst_x, dst_y, region);
}

EXPORT
void
wld_blend_region(struct wld_renderer *renderer, struct wld_buffer *buffer,
                 int32_t dst_x, int32_t dst_y, pixman_region32_t *region)
{
	pixman_image_t *src = NULL, *dst = NULL;
	pixman_region32_t clip;
	pixman_box32_t *extents;

	if (!renderer->target || !pixman_region32_not_empty(region))
		return;

	/* Complete accelerator writes before accessing the buffers on the CPU. */
	renderer->impl->flush(renderer);
	if (!wld_map(buffer))
		return;
	if (!wld_map(renderer->target))
		goto unmap_src;

	src = pixman_image_create_bits(format_wld_to_pixman(buffer->format),
	                               buffer->width, buffer->height,
	                               buffer->map, buffer->pitch);
	dst = pixman_image_create_bits(format_wld_to_pixman(renderer->target->format),
	                               renderer->target->width,
	                               renderer->target->height,
	                               renderer->target->map,
	                               renderer->target->pitch);
	if (!src || !dst)
		goto destroy;

	pixman_region32_init(&clip);
	pixman_region32_copy(&clip, region);
	pixman_region32_translate(&clip, dst_x, dst_y);
	extents = pixman_region32_extents(region);
	pixman_image_set_clip_region32(dst, &clip);
	pixman_image_composite32(PIXMAN_OP_OVER, src, NULL, dst,
	                         extents->x1, extents->y1, 0, 0,
	                         extents->x1 + dst_x, extents->y1 + dst_y,
	                         extents->x2 - extents->x1,
	                         extents->y2 - extents->y1);
	pixman_region32_fini(&clip);

destroy:
	if (src)
		pixman_image_unref(src);
	if (dst)
		pixman_image_unref(dst);
	wld_unmap(renderer->target);
unmap_src:
	wld_unmap(buffer);
}


/* https://en.wikipedia.org/wiki/Midpoint_circle_algorithm */
static void
circle_points(struct wld_renderer *renderer, uint32_t color,
			 int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool fill)
{
	/* hacky */
	if (fill) {
		renderer->impl->fill_rectangle(renderer, color, x1-x2, y1+y2, 2*x2 + 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1-x2, y1-y2, 2*x2 + 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1-y2, y1+x2, 2*y2 + 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1-y2, y1-x2, 2*y2 + 1, 1);
	}
	
	else {
		renderer->impl->fill_rectangle(renderer, color, x1+x2, y1+y2, 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1-x2, y1+y2, 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1+x2, y1-y2, 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1-x2, y1-y2, 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1+y2, y1+x2, 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1-y2, y1+x2, 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1+y2, y1-x2, 1, 1);
		renderer->impl->fill_rectangle(renderer, color, x1-y2, y1-x2, 1, 1);
	}
}

EXPORT
void
wld_draw_circle(struct wld_renderer *renderer, uint32_t color, 
				int32_t x, int32_t y, uint32_t r, bool fill)
{
	int32_t x1 = 0, y1 = r;
	int32_t d = 3 - 2 * r;
	circle_points(renderer, color, x, y, x1, y1, fill);

	while (y1 >= x1) {
		if (d > 0) {
			y1--;
			d = d + 4 * (x1 - y1) + 10;
		}
		else
			d = d + 4 * x1 + 6;

		x1++;

		circle_points(renderer, color, x, y, x1, y1, fill);
	}
}

EXPORT
void
wld_draw_line(struct wld_renderer *renderer, uint32_t color,
			 int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
	int32_t dx = abs(x2-x1),  sx = x1<x2 ? 1 : -1;
	int32_t dy = -abs(y2-y1), sy = y1<y2 ? 1 : -1;
	int32_t err = dx+dy, e2;

	while(true) {
		renderer->impl->fill_rectangle(renderer, color, x1, y1, 1, 1);

		if (x1==x2 && y1==y2)
			break;
		
		e2 = 2*err;
		
		if (e2 >= dy) {
			err += dy;
			x1 += sx;
		}

		if (e2 <= dx) {
			err += dx;
			y1 += sy;
		}
	}
}

EXPORT
void
wld_draw_text(struct wld_renderer *renderer,
              struct wld_font *font_base, uint32_t color,
              int32_t x, int32_t y, const char *text, uint32_t length,
              struct wld_extents *extents)
{
	struct font *font = (void *)font_base;

	renderer->impl->draw_text(renderer, font, color, x, y, text, length,
	                          extents);
}

EXPORT
void
wld_flush(struct wld_renderer *renderer)
{
	renderer->impl->flush(renderer);
	if (renderer->target && ((struct buffer *)renderer->target)->base.impl->flush)
		((struct buffer *)renderer->target)->base.impl->flush(
		    (struct buffer *)renderer->target);
	renderer->impl->set_target(renderer, NULL);
	renderer->target = NULL;
}
