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
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
	if (renderer->impl->blend_region) {
		renderer->impl->blend_region(renderer, (struct buffer *)buffer,
		                             dst_x, dst_y, region);
		return;
	}

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


/* one horizontal box pass of the given radius over a w x h block of 32-bit pixels, in place via tmp */
static void
box_pass_h(uint32_t *px, uint32_t *tmp, int w, int h, int stride, int r)
{
	for (int y = 0; y < h; y++) {
		uint32_t *row = px + (size_t)y * stride;
		uint32_t sum[4] = { 0, 0, 0, 0 };
		int n = 2 * r + 1;

		for (int i = -r; i <= r; i++) {
			uint32_t p = row[i < 0 ? 0 : i >= w ? w - 1 : i];

			for (int c = 0; c < 4; c++)
				sum[c] += (p >> (8 * c)) & 0xff;
		}
		for (int x = 0; x < w; x++) {
			uint32_t in = row[x + r + 1 >= w ? w - 1 : x + r + 1];
			uint32_t out = row[x - r < 0 ? 0 : x - r];

			tmp[x] = (sum[0] / n) | (sum[1] / n) << 8 | (sum[2] / n) << 16 | (sum[3] / n) << 24;
			for (int c = 0; c < 4; c++)
				sum[c] += ((in >> (8 * c)) & 0xff) - ((out >> (8 * c)) & 0xff);
		}
		memcpy(row, tmp, (size_t)w * 4);
	}
}

/* vertical pass, done as a horizontal pass on a transposed copy */
static void
box_pass_v(uint32_t *px, uint32_t *tmp, uint32_t *col, int w, int h, int stride, int r)
{
	for (int x = 0; x < w; x++) {
		for (int y = 0; y < h; y++)
			col[y] = px[(size_t)y * stride + x];
		box_pass_h(col, tmp, h, 1, h, r);
		for (int y = 0; y < h; y++)
			px[(size_t)y * stride + x] = col[y];
	}
}

/* the CPU fallback: three box passes each way approximate a gaussian, on the region's extents
 * padded by the radius, then only pixels inside the region are written back */
EXPORT
void
wld_blur_region(struct wld_renderer *renderer, pixman_region32_t *region, uint32_t radius)
{
	struct wld_buffer *t = renderer->target;
	pixman_box32_t *e;
	int x0, y0, x1, y1, w, h, r;
	uint32_t *block, *tmp, *col;

	if (!t || !pixman_region32_not_empty(region) || radius == 0)
		return;
	if (renderer->impl->blur_region) {
		renderer->impl->blur_region(renderer, region, radius);
		return;
	}

	renderer->impl->flush(renderer);
	if (!wld_map(t))
		return;
	r = (int)radius / 3 > 0 ? (int)radius / 3 : 1;
	e = pixman_region32_extents(region);
	x0 = e->x1 - (int)radius, y0 = e->y1 - (int)radius;
	x1 = e->x2 + (int)radius, y1 = e->y2 + (int)radius;
	x0 = x0 < 0 ? 0 : x0, y0 = y0 < 0 ? 0 : y0;
	x1 = x1 > (int)t->width ? (int)t->width : x1, y1 = y1 > (int)t->height ? (int)t->height : y1;
	w = x1 - x0, h = y1 - y0;
	block = w > 0 && h > 0 ? malloc((size_t)w * h * 4) : NULL;
	tmp = malloc((size_t)(w > h ? w : h) * 4);
	col = malloc((size_t)h * 4);
	if (block && tmp && col) {
		for (int y = 0; y < h; y++)
			memcpy(block + (size_t)y * w, (uint8_t *)t->map + (size_t)(y0 + y) * t->pitch + (size_t)x0 * 4, (size_t)w * 4);
		for (int k = 0; k < 3; k++) {
			box_pass_h(block, tmp, w, h, w, r);
			box_pass_v(block, tmp, col, w, h, w, r);
		}
		int n;
		pixman_box32_t *b = pixman_region32_rectangles(region, &n);

		for (int i = 0; i < n; i++) {
			int bx0 = b[i].x1 < x0 ? x0 : b[i].x1, bx1 = b[i].x2 > x1 ? x1 : b[i].x2;
			int by0 = b[i].y1 < y0 ? y0 : b[i].y1, by1 = b[i].y2 > y1 ? y1 : b[i].y2;

			for (int y = by0; y < by1 && bx1 > bx0; y++)
				memcpy((uint8_t *)t->map + (size_t)y * t->pitch + (size_t)bx0 * 4,
				       block + (size_t)(y - y0) * w + (bx0 - x0), (size_t)(bx1 - bx0) * 4);
		}
	}
	free(block);
	free(tmp);
	free(col);
	wld_unmap(t);
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

// distance from a point to a rounded rectangle, negative inside
static float
rounded_rect_distance(float px, float py, float w, float h, float r)
{
	float qx = fabsf(px - w / 2) - (w / 2 - r), qy = fabsf(py - h / 2) - (h / 2 - r);
	float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
	float inside = qx > qy ? qx : qy;

	return sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - r;
}

// one alpha mask per size, windows keep their size for many frames
struct mask_cache {
	pixman_image_t *image;
	uint32_t w, h, radius, blur;
	unsigned generation;
};

static unsigned mask_generation;

#define MASK_CACHE_SIZE 8

static pixman_image_t *
cached_mask(struct mask_cache *caches, uint32_t w, uint32_t h, uint32_t radius, uint32_t blur, uint8_t alpha,
            unsigned *generation)
{
	static unsigned next;
	struct mask_cache *cache = NULL;
	uint32_t margin = blur, mw = w + 2 * margin, mh = h + 2 * margin;
	uint8_t *bits;
	int stride;

	// a few windows of different sizes share the cache, the oldest entry makes way
	for (int i = 0; i < MASK_CACHE_SIZE; i++) {
		if (caches[i].image && caches[i].w == w && caches[i].h == h && caches[i].radius == radius && caches[i].blur == blur) {
			*generation = caches[i].generation;
			return caches[i].image;
		}
	}
	cache = &caches[next++ % MASK_CACHE_SIZE];
	if (cache->image)
		pixman_image_unref(cache->image);
	cache->image = pixman_image_create_bits(PIXMAN_a8, mw, mh, NULL, 0);
	if (!cache->image)
		return NULL;
	bits = (uint8_t *)pixman_image_get_data(cache->image);
	stride = pixman_image_get_stride(cache->image);
	for (uint32_t y = 0; y < mh; y++) {
		for (uint32_t x = 0; x < mw; x++) {
			float d = rounded_rect_distance(x + 0.5f - margin, y + 0.5f - margin, w, h, radius);
			float a;

			// a hard edge antialiased over a pixel, or a soft falloff across the blur width
			if (blur == 0)
				a = d <= -0.5f ? 1 : d >= 0.5f ? 0 : 0.5f - d;
			else {
				float t = (d + blur / 2.0f) / blur;

				t = t < 0 ? 0 : t > 1 ? 1 : t;
				a = 1 - t * t * (3 - 2 * t);
			}
			bits[y * stride + x] = (uint8_t)(a * alpha + 0.5f);
		}
	}
	cache->w = w, cache->h = h, cache->radius = radius, cache->blur = blur;
	cache->generation = *generation = ++mask_generation;
	return cache->image;
}

static pixman_image_t *
target_image(struct wld_renderer *renderer)
{
	return pixman_image_create_bits(format_wld_to_pixman(renderer->target->format),
	                                renderer->target->width, renderer->target->height,
	                                renderer->target->map, renderer->target->pitch);
}

void
wld_blend_region_rounded(struct wld_renderer *renderer, struct wld_buffer *buffer,
                         int32_t dst_x, int32_t dst_y, pixman_region32_t *region, uint32_t radius)
{
	static struct mask_cache cache[MASK_CACHE_SIZE];
	pixman_image_t *src, *dst, *mask;
	pixman_region32_t clip;
	pixman_box32_t *extents;

	unsigned generation;

	if (!renderer->target || !pixman_region32_not_empty(region))
		return;
	if (renderer->impl->blend_region_mask) {
		mask = cached_mask(cache, buffer->width, buffer->height, radius, 0, 255, &generation);
		if (mask)
			renderer->impl->blend_region_mask(renderer, (struct buffer *)buffer, dst_x, dst_y, region, mask, generation);
		return;
	}
	renderer->impl->flush(renderer);
	if (!wld_map(buffer) || !wld_map(renderer->target))
		return;
	mask = cached_mask(cache, buffer->width, buffer->height, radius, 0, 255, &generation);
	src = pixman_image_create_bits(format_wld_to_pixman(buffer->format), buffer->width, buffer->height,
	                               buffer->map, buffer->pitch);
	dst = target_image(renderer);
	if (mask && src && dst) {
		pixman_region32_init(&clip);
		pixman_region32_copy(&clip, region);
		pixman_region32_translate(&clip, dst_x, dst_y);
		pixman_image_set_clip_region32(dst, &clip);
		extents = pixman_region32_extents(region);
		pixman_image_composite32(PIXMAN_OP_OVER, src, mask, dst, extents->x1, extents->y1, extents->x1, extents->y1,
		                         extents->x1 + dst_x, extents->y1 + dst_y,
		                         extents->x2 - extents->x1, extents->y2 - extents->y1);
		pixman_region32_fini(&clip);
	}
	if (src)
		pixman_image_unref(src);
	if (dst)
		pixman_image_unref(dst);
	wld_unmap(renderer->target);
	wld_unmap(buffer);
}

void
wld_draw_shadow(struct wld_renderer *renderer, pixman_region32_t *region, int32_t x, int32_t y,
                uint32_t w, uint32_t h, uint32_t radius, uint32_t blur, int32_t offset_y, uint8_t alpha)
{
	static struct mask_cache cache[MASK_CACHE_SIZE];
	pixman_color_t black = { 0, 0, 0, 0xffff };
	pixman_image_t *solid, *dst, *mask;

	unsigned generation;

	if (!renderer->target || !pixman_region32_not_empty(region) || w == 0 || h == 0)
		return;
	if (renderer->impl->fill_region_mask) {
		mask = cached_mask(cache, w, h, radius, blur, alpha, &generation);
		if (mask)
			renderer->impl->fill_region_mask(renderer, 0xff000000, region, mask, generation,
			                                 x - (int32_t)blur, y - (int32_t)blur + offset_y);
		return;
	}
	renderer->impl->flush(renderer);
	if (!wld_map(renderer->target))
		return;
	mask = cached_mask(cache, w, h, radius, blur, alpha, &generation);
	solid = pixman_image_create_solid_fill(&black);
	dst = target_image(renderer);
	if (mask && solid && dst) {
		pixman_image_set_clip_region32(dst, region);
		pixman_image_composite32(PIXMAN_OP_OVER, solid, mask, dst, 0, 0, 0, 0,
		                         x - (int32_t)blur, y - (int32_t)blur + offset_y, w + 2 * blur, h + 2 * blur);
	}
	if (solid)
		pixman_image_unref(solid);
	if (dst)
		pixman_image_unref(dst);
	wld_unmap(renderer->target);
}
