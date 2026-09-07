/* wld: pixman.c
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

#include "pixman.h"
#include "wld-private.h"

#include <string.h>

#define PIXMAN_COLOR(c)                              \
	{                                            \
		.alpha = ((c >> 24) & 0xff) * 0x101, \
		.red = ((c >> 16) & 0xff) * 0x101,   \
		.green = ((c >> 8) & 0xff) * 0x101,  \
		.blue = ((c >> 0) & 0xff) * 0x101,   \
	}

struct pixman_renderer {
	struct wld_renderer base;
	pixman_image_t *target;
	pixman_glyph_cache_t *glyph_cache;
};

struct pixman_buffer {
	struct buffer base;
	pixman_image_t *image;
};

struct pixman_map {
	struct wld_exporter exporter;
	struct wld_destructor destructor;
	pixman_image_t *image;
};

#include "interface/context.h"
#define RENDERER_IMPLEMENTS_REGION
#include "interface/buffer.h"
#include "interface/renderer.h"
IMPL(pixman_renderer, wld_renderer)
IMPL(pixman_buffer, wld_buffer)

static struct wld_context context = { .impl = &wld_context_impl };

EXPORT
struct wld_context *wld_pixman_context = &context;

struct wld_renderer *
context_create_renderer(struct wld_context *context)
{
	struct pixman_renderer *renderer;

	if (!(renderer = malloc(sizeof *renderer)))
		goto error0;

	if (!(renderer->glyph_cache = pixman_glyph_cache_create()))
		goto error1;

	renderer_initialize(&renderer->base, &wld_renderer_impl);
	renderer->target = NULL;

	return &renderer->base;

error1:
	free(renderer);
error0:
	return NULL;
}

static struct buffer *
new_buffer(pixman_image_t *image)
{
	struct pixman_buffer *buffer;

	if (!(buffer = malloc(sizeof *buffer)))
		return NULL;

	buffer_initialize(&buffer->base, &wld_buffer_impl,
	                  pixman_image_get_width(image),
	                  pixman_image_get_height(image),
	                  format_pixman_to_wld(pixman_image_get_format(image)),
	                  pixman_image_get_stride(image));
	buffer->base.base.map = pixman_image_get_data(image);
	buffer->image = image;

	return &buffer->base;
}

struct buffer *
context_create_buffer(struct wld_context *context,
                      uint32_t width, uint32_t height,
                      uint32_t format, uint32_t flags)
{
	struct buffer *buffer;
	pixman_image_t *image;

	image = pixman_image_create_bits(format_wld_to_pixman(format),
	                                 width, height, NULL, 0);

	if (!image)
		goto error0;

	if (!(buffer = new_buffer(image)))
		goto error1;

	return buffer;

error1:
	pixman_image_unref(image);
error0:
	return NULL;
}

struct buffer *
context_import_buffer(struct wld_context *context,
                      uint32_t type, union wld_object object,
                      uint32_t width, uint32_t height,
                      uint32_t format, uint32_t pitch)
{
	struct buffer *buffer;
	pixman_image_t *image;

	switch (type) {
	case WLD_OBJECT_DATA:
		image = pixman_image_create_bits(format_wld_to_pixman(format),
		                                 width, height, object.ptr, pitch);
		break;
	default:
		image = NULL;
	}

	if (!image)
		goto error0;

	if (!(buffer = new_buffer(image)))
		goto error1;

	return buffer;

error1:
	pixman_image_unref(image);
error0:
	return NULL;
}

void
context_destroy(struct wld_context *context)
{
}

uint32_t
renderer_capabilities(struct wld_renderer *renderer, struct buffer *buffer)
{
	/* The pixman renderer can read and write to any buffer using it's map
	 * implementation. */
	return WLD_CAPABILITY_READ | WLD_CAPABILITY_WRITE;
}

static void
destroy_image(pixman_image_t *image, void *data)
{
	struct buffer *buffer = data;

	wld_unmap(&buffer->base);
}

bool
map_export(struct wld_exporter *exporter, struct wld_buffer *buffer,
           uint32_t type, union wld_object *object)
{
	struct pixman_map *map = CONTAINER_OF(exporter, struct pixman_map, exporter);

	switch (type) {
	case WLD_PIXMAN_OBJECT_IMAGE:
		object->ptr = pixman_image_ref(map->image);
		return true;
	default:
		return false;
	}
}

void
map_destroy(struct wld_destructor *destructor)
{
	struct pixman_map *map = CONTAINER_OF(destructor, struct pixman_map, destructor);

	pixman_image_unref(map->image);
	free(map);
}

static pixman_image_t *
pixman_image(struct buffer *buffer)
{
	if (buffer->base.impl == &wld_buffer_impl)
		return pixman_image_ref(pixman_buffer(&buffer->base)->image);

	union wld_object object;

	if (wld_export(&buffer->base, WLD_PIXMAN_OBJECT_IMAGE, &object))
		return object.ptr;

	struct pixman_map *map;
	pixman_image_t *image;

	if (!wld_map(&buffer->base))
		goto error0;

	image = pixman_image_create_bits(format_wld_to_pixman(buffer->base.format),
	                                 buffer->base.width, buffer->base.height,
	                                 buffer->base.map, buffer->base.pitch);

	if (!image)
		goto error1;

	if (!(map = malloc(sizeof *map)))
		goto error2;

	map->image = image;
	map->exporter.export = &map_export;
	wld_buffer_add_exporter(&buffer->base, &map->exporter);
	map->destructor.destroy = &map_destroy;
	wld_buffer_add_destructor(&buffer->base, &map->destructor);
	pixman_image_set_destroy_function(image, &destroy_image, buffer);

	return pixman_image_ref(image);

error2:
	pixman_image_unref(image);
error1:
	wld_unmap(&buffer->base);
error0:
	return NULL;
}

bool
renderer_set_target(struct wld_renderer *base, struct buffer *buffer)
{
	struct pixman_renderer *renderer = pixman_renderer(base);

	if (renderer->target)
		pixman_image_unref(renderer->target);

	if (buffer)
		return (renderer->target = pixman_image(buffer));

	renderer->target = NULL;
	return true;
}

void
renderer_fill_rectangle(struct wld_renderer *base, uint32_t color,
                        int32_t x, int32_t y,
                        uint32_t width, uint32_t height)
{
	struct pixman_renderer *renderer = pixman_renderer(base);
	pixman_color_t pixman_color = PIXMAN_COLOR(color);
	pixman_box32_t box = { x, y, x + width, y + height };

	pixman_image_fill_boxes(PIXMAN_OP_SRC, renderer->target,
	                        &pixman_color, 1, &box);
}

void
renderer_fill_region(struct wld_renderer *base, uint32_t color,
                     pixman_region32_t *region)
{
	struct pixman_renderer *renderer = pixman_renderer(base);
	pixman_color_t pixman_color = PIXMAN_COLOR(color);
	pixman_box32_t *boxes;
	int num_boxes;

	boxes = pixman_region32_rectangles(region, &num_boxes);
	pixman_image_fill_boxes(PIXMAN_OP_SRC, renderer->target,
	                        &pixman_color, num_boxes, boxes);
}

void
renderer_copy_rectangle(struct wld_renderer *base, struct buffer *buffer,
                        int32_t dst_x, int32_t dst_y,
                        int32_t src_x, int32_t src_y,
                        uint32_t width, uint32_t height)
{
	struct pixman_renderer *renderer = pixman_renderer(base);
	pixman_image_t *src = pixman_image(buffer), *dst = renderer->target;

	if (!src)
		return;

	pixman_image_composite32(PIXMAN_OP_SRC, src, NULL, dst,
	                         src_x, src_y, 0, 0, dst_x, dst_y, width, height);
	pixman_image_unref(src);
}

void
renderer_copy_region(struct wld_renderer *base, struct buffer *buffer,
                     int32_t dst_x, int32_t dst_y,
                     pixman_region32_t *region)
{
	struct pixman_renderer *renderer = pixman_renderer(base);
	pixman_image_t *src = pixman_image(buffer), *dst = renderer->target;
	pixman_region32_t clip;

	if (!src)
		return;

	pixman_region32_init(&clip);
	pixman_region32_copy(&clip, region);
	pixman_region32_translate(&clip, dst_x, dst_y);

	pixman_image_set_clip_region32(dst, &clip);
	pixman_image_composite32(PIXMAN_OP_SRC, src, NULL, dst,
	                         region->extents.x1, region->extents.y1, 0, 0,
	                         region->extents.x1 + dst_x,
	                         region->extents.y1 + dst_y,
	                         region->extents.x2 - region->extents.x1,
	                         region->extents.y2 - region->extents.y1);
	pixman_image_unref(src);
	pixman_image_set_clip_region32(dst, NULL);

	pixman_region32_fini(&clip);
}

static inline uint8_t
reverse(uint8_t byte)
{
	byte = ((byte << 1) & 0xaa) | ((byte >> 1) & 0x55);
	byte = ((byte << 2) & 0xcc) | ((byte >> 2) & 0x33);
	byte = ((byte << 4) & 0xf0) | ((byte >> 4) & 0x0f);

	return byte;
}

static pixman_image_t *
glyph_bitmap_to_pixman_image(struct glyph *glyph)
{
	FT_Bitmap *bitmap = &glyph->bitmap;
	pixman_image_t *image;
	uint8_t *src, *dst;
	uint32_t row, byte_index, bytes_per_row, pitch;

	switch (bitmap->pixel_mode) {
	case FT_PIXEL_MODE_MONO:
		image = pixman_image_create_bits(PIXMAN_a1, bitmap->width,
		                                 bitmap->rows, NULL, bitmap->pitch);
		if (!image)
			return NULL;

		pitch = pixman_image_get_stride(image);
		bytes_per_row = (bitmap->width + 7) / 8;
		src = bitmap->buffer;
		dst = (uint8_t *)pixman_image_get_data(image);

		for (row = 0; row < bitmap->rows; ++row) {
			/* Pixman's A1 format expects the bits in the opposite order
			 * that Freetype gives us. */
			for (byte_index = 0; byte_index < bytes_per_row; ++byte_index)
				dst[byte_index] = reverse(src[byte_index]);

			dst += pitch;
			src += bitmap->pitch;
		}
		return image;
	case FT_PIXEL_MODE_GRAY:
		image = pixman_image_create_bits(PIXMAN_a8, bitmap->width,
		                                 bitmap->rows, NULL, 0);
		if (!image)
			return NULL;

		pitch = pixman_image_get_stride(image);
		src = bitmap->buffer;
		dst = (uint8_t *)pixman_image_get_data(image);

		for (row = 0; row < bitmap->rows; ++row) {
			memset(dst, 0, pitch);
			memcpy(dst, src, bitmap->width);
			dst += pitch;
			src += bitmap->pitch;
		}
		return image;
	default:
		return NULL;
	}
}

void
renderer_draw_text(struct wld_renderer *base,
                   struct font *font, uint32_t color,
                   int32_t x, int32_t y, const char *text,
                   uint32_t length, struct wld_extents *extents)
{
	struct pixman_renderer *renderer = pixman_renderer(base);
	int ret;
	uint32_t c;
	struct glyph *glyph;
	FT_UInt glyph_index;
	pixman_glyph_t *glyphs;
	uint32_t index = 0, origin_x = 0;
	pixman_color_t pixman_color = PIXMAN_COLOR(color);
	pixman_image_t *solid;

	if (length == -1)
		length = strlen(text);
	glyphs = malloc(length * sizeof(glyphs[0]));
	if (!glyphs)
		return;
	solid = pixman_image_create_solid_fill(&pixman_color);

	while ((ret = FcUtf8ToUcs4((FcChar8 *)text, &c, length)) > 0 && c != '\0') {
		text += ret;
		length -= ret;
		glyph_index = FT_Get_Char_Index(font->face, c);

		if (!font_ensure_glyph(font, glyph_index))
			continue;

		glyph = font->glyphs[glyph_index];

		glyphs[index].x = origin_x;
		glyphs[index].y = 0;
		glyphs[index].glyph = pixman_glyph_cache_lookup(renderer->glyph_cache,
		                                                font, glyph);

		/* If we don't have the glyph in our cache, do some conversions to make
		 * pixman happy, and then insert it. */
		if (!glyphs[index].glyph) {
			pixman_image_t *image;
			image = glyph_bitmap_to_pixman_image(glyph);

			if (!image)
				goto advance;

			/* Insert the glyph into the cache. */
			pixman_glyph_cache_freeze(renderer->glyph_cache);
			glyphs[index].glyph = pixman_glyph_cache_insert(renderer->glyph_cache, font, glyph,
			                                                -glyph->x, -glyph->y, image);
			pixman_glyph_cache_thaw(renderer->glyph_cache);

			/* The glyph cache copies the contents of the glyph bitmap. */
			pixman_image_unref(image);
		}

		++index;

	advance:
		origin_x += glyph->advance;
	}

	pixman_composite_glyphs_no_mask(PIXMAN_OP_OVER, solid, renderer->target,
	                                0, 0, x, y, renderer->glyph_cache,
	                                index, glyphs);

	free(glyphs);
	pixman_image_unref(solid);

	if (extents)
		extents->advance = origin_x;
}

void
renderer_flush(struct wld_renderer *renderer)
{
}

void
renderer_destroy(struct wld_renderer *base)
{
	struct pixman_renderer *renderer = pixman_renderer(base);

	pixman_glyph_cache_destroy(renderer->glyph_cache);
	free(renderer);
}

bool
buffer_map(struct buffer *buffer)
{
	return true;
}

bool
buffer_unmap(struct buffer *buffer)
{
	return true;
}

void
buffer_destroy(struct buffer *base)
{
	struct pixman_buffer *buffer = pixman_buffer(&base->base);

	pixman_image_unref(buffer->image);
	free(buffer);
}
