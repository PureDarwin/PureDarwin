/* wld: intel.c
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

#include "drm-private.h"
#include "drm.h"
#include "intel/batch.h"
#include "intel/blt.h"
#include "wld-private.h"

#include <i915_drm.h>
#include <intel_bufmgr.h>
#include <string.h>
#include <unistd.h>

struct intel_context {
	struct wld_context base;
	drm_intel_bufmgr *bufmgr;
};

struct intel_renderer {
	struct wld_renderer base;
	struct intel_batch batch;
	struct intel_buffer *target;
};

struct intel_buffer {
	struct buffer base;
	struct wld_exporter exporter;
	drm_intel_bo *bo;
};

#include "interface/buffer.h"
#include "interface/context.h"
#include "interface/renderer.h"
#define DRM_DRIVER_NAME intel
#include "interface/drm.h"
IMPL(intel_context, wld_context)
IMPL(intel_renderer, wld_renderer)
IMPL(intel_buffer, wld_buffer)

static void
pack_gray_row_to_mono(uint8_t *dst, const uint8_t *src, uint32_t width)
{
	uint32_t x, bytes_per_row;

	bytes_per_row = (width + 7) / 8;
	memset(dst, 0, bytes_per_row);

	for (x = 0; x < width; ++x) {
		if (src[x] >= 128)
			dst[x / 8] |= 0x80 >> (x & 7);
	}
}

/**** DRM driver ****/
bool
driver_device_supported(uint32_t vendor_id, uint32_t device_id)
{
	return vendor_id == 0x8086;
}

struct wld_context *
driver_create_context(int drm_fd)
{
	struct intel_context *context;

	context = malloc(sizeof *context);

	if (!context)
		goto error0;

	context_initialize(&context->base, &wld_context_impl);
	context->bufmgr = drm_intel_bufmgr_gem_init(drm_fd, INTEL_BATCH_SIZE);

	if (!context->bufmgr)
		goto error1;

	return &context->base;

error1:
	free(context);
error0:
	return NULL;
}

/**** Context ****/
struct wld_renderer *
context_create_renderer(struct wld_context *base)
{
	struct intel_context *context = intel_context(base);
	struct intel_renderer *renderer;

	if (!(renderer = malloc(sizeof *renderer)))
		goto error0;

	if (!(intel_batch_initialize(&renderer->batch, context->bufmgr)))
		goto error1;

	renderer_initialize(&renderer->base, &wld_renderer_impl);

	return &renderer->base;

error1:
	free(renderer);
error0:
	return NULL;
}

static bool export(struct wld_exporter *exporter, struct wld_buffer *base,
                   uint32_t type, union wld_object *object)
{
	struct intel_buffer *buffer = intel_buffer(base);

	switch (type) {
	case WLD_DRM_OBJECT_HANDLE:
		object->u32 = buffer->bo->handle;
		return true;
	case WLD_DRM_OBJECT_PRIME_FD:
		if (drm_intel_bo_gem_export_to_prime(buffer->bo, &object->i) != 0)
			return false;
		return true;
	default:
		return false;
	}
}

static struct buffer *
new_buffer(uint32_t width, uint32_t height,
           uint32_t format, uint32_t pitch,
           drm_intel_bo *bo)
{
	struct intel_buffer *buffer;

	if (!(buffer = malloc(sizeof *buffer)))
		return NULL;

	buffer_initialize(&buffer->base, &wld_buffer_impl,
	                  width, height, format, pitch);
	buffer->bo = bo;
	buffer->exporter.export = &export;
	wld_buffer_add_exporter(&buffer->base.base, &buffer->exporter);

	return &buffer->base;
}

struct buffer *
context_create_buffer(struct wld_context *base,
                      uint32_t width, uint32_t height,
                      uint32_t format, uint32_t flags)
{
	struct intel_context *context = intel_context(base);
	struct buffer *buffer;
	drm_intel_bo *bo;
	uint32_t tiling_mode = width >= 128 && !(flags & WLD_FLAG_CURSOR) ? I915_TILING_X : I915_TILING_NONE;
	unsigned long pitch;

	bo = drm_intel_bo_alloc_tiled(context->bufmgr, "buffer", width, height, 4,
	                              &tiling_mode, &pitch, 0);

	if (!bo)
		goto error0;

	if (!(buffer = new_buffer(width, height, format, pitch, bo)))
		goto error1;

	return buffer;

error1:
	drm_intel_bo_unreference(bo);
error0:
	return NULL;
}

struct buffer *
context_import_buffer(struct wld_context *base,
                      uint32_t type, union wld_object object,
                      uint32_t width, uint32_t height,
                      uint32_t format, uint32_t pitch)
{
	struct intel_context *context = intel_context(base);
	struct buffer *buffer;
	drm_intel_bo *bo;

	switch (type) {
	case WLD_DRM_OBJECT_PRIME_FD: {
		uint32_t size = width * height * format_bytes_per_pixel(format);
		bo = drm_intel_bo_gem_create_from_prime(context->bufmgr,
		                                        object.i, size);
		break;
	}
	default:
		bo = NULL;
	};

	if (!bo)
		goto error0;

	if (!(buffer = new_buffer(width, height, format, pitch, bo)))
		goto error1;

	return buffer;

error1:
	drm_intel_bo_unreference(bo);
error0:
	return NULL;
}

void
context_destroy(struct wld_context *base)
{
	struct intel_context *context = intel_context(base);

	drm_intel_bufmgr_destroy(context->bufmgr);
	free(context);
}

/**** Renderer ****/
uint32_t
renderer_capabilities(struct wld_renderer *renderer, struct buffer *buffer)
{
	if (buffer->base.impl == &wld_buffer_impl)
		return WLD_CAPABILITY_READ | WLD_CAPABILITY_WRITE;

	return 0;
}

bool
renderer_set_target(struct wld_renderer *base, struct buffer *buffer)
{
	struct intel_renderer *renderer = intel_renderer(base);

	if (buffer && buffer->base.impl != &wld_buffer_impl)
		return false;

	renderer->target = buffer ? intel_buffer(&buffer->base) : NULL;

	return true;
}

void
renderer_fill_rectangle(struct wld_renderer *base, uint32_t color,
                        int32_t x, int32_t y,
                        uint32_t width, uint32_t height)
{
	struct intel_renderer *renderer = intel_renderer(base);
	struct intel_buffer *dst = renderer->target;

	xy_color_blt(&renderer->batch, dst->bo, dst->base.base.pitch,
	             x, y, x + width, y + height, color);
}

void
renderer_copy_rectangle(struct wld_renderer *base,
                        struct buffer *buffer_base,
                        int32_t dst_x, int32_t dst_y,
                        int32_t src_x, int32_t src_y,
                        uint32_t width, uint32_t height)
{
	struct intel_renderer *renderer = intel_renderer(base);

	if (buffer_base->base.impl != &wld_buffer_impl)
		return;

	struct intel_buffer *src = intel_buffer(&buffer_base->base),
	                    *dst = renderer->target;

	xy_src_copy_blt(&renderer->batch,
	                src->bo, src->base.base.pitch, src_x, src_y,
	                dst->bo, dst->base.base.pitch, dst_x, dst_y, width, height);
}

void
renderer_draw_text(struct wld_renderer *base,
                   struct font *font, uint32_t color,
                   int32_t x, int32_t y, const char *text,
                   uint32_t length, struct wld_extents *extents)
{
	struct intel_renderer *renderer = intel_renderer(base);
	struct intel_buffer *dst = renderer->target;
	int ret;
	struct glyph *glyph;
	uint32_t row;
	FT_UInt glyph_index;
	uint32_t c;
	uint8_t immediate[512];
	uint8_t *byte;
	int32_t origin_x = x;

	xy_setup_blt(&renderer->batch, true, BLT_RASTER_OPERATION_SRC,
	             0, color, dst->bo, dst->base.base.pitch);

	if (length == -1)
		length = strlen(text);

	while ((ret = FcUtf8ToUcs4((FcChar8 *)text, &c, length)) > 0 && c != '\0') {
		text += ret;
		length -= ret;
		glyph_index = FT_Get_Char_Index(font->face, c);

		if (!font_ensure_glyph(font, glyph_index))
			continue;

		glyph = font->glyphs[glyph_index];

		if (glyph->bitmap.width == 0 || glyph->bitmap.rows == 0)
			goto advance;

		byte = immediate;

		/* XY_TEXT_IMMEDIATE requires a pitch with no extra bytes */
		for (row = 0; row < glyph->bitmap.rows; ++row) {
			const uint8_t *src = glyph->bitmap.buffer +
			                     (row * glyph->bitmap.pitch);
			uint32_t bytes_per_row = (glyph->bitmap.width + 7) / 8;

			if (glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
				memcpy(byte, src, bytes_per_row);
			} else {
				pack_gray_row_to_mono(byte, src, glyph->bitmap.width);
			}
			byte += (glyph->bitmap.width + 7) / 8;
		}

	retry:
		ret = xy_text_immediate_blt(&renderer->batch, dst->bo,
		                            origin_x + glyph->x, y + glyph->y,
		                            origin_x + glyph->x + glyph->bitmap.width,
		                            y + glyph->y + glyph->bitmap.rows,
		                            (byte - immediate + 3) / 4,
		                            (uint32_t *)immediate);

		if (ret == INTEL_BATCH_NO_SPACE) {
			intel_batch_flush(&renderer->batch);
			xy_setup_blt(&renderer->batch, true, BLT_RASTER_OPERATION_SRC,
			             0, color, dst->bo, dst->base.base.pitch);
			goto retry;
		}

	advance:
		origin_x += glyph->advance;
	}

	if (extents)
		extents->advance = origin_x - x;
}

void
renderer_flush(struct wld_renderer *base)
{
	struct intel_renderer *renderer = intel_renderer(base);

	intel_batch_flush(&renderer->batch);
}

void
renderer_destroy(struct wld_renderer *base)
{
	struct intel_renderer *renderer = intel_renderer(base);

	intel_batch_finalize(&renderer->batch);
	free(renderer);
}

/**** Buffer ****/
bool
buffer_map(struct buffer *base)
{
	struct intel_buffer *buffer = intel_buffer(&base->base);

	if (drm_intel_gem_bo_map_gtt(buffer->bo) != 0)
		return false;

	buffer->base.base.map = buffer->bo->virtual;

	return true;
}

bool
buffer_unmap(struct buffer *base)
{
	struct intel_buffer *buffer = intel_buffer(&base->base);

	if (drm_intel_gem_bo_unmap_gtt(buffer->bo) != 0)
		return false;

	buffer->base.base.map = NULL;

	return true;
}

void
buffer_destroy(struct buffer *base)
{
	struct intel_buffer *buffer = intel_buffer(&base->base);

	drm_intel_bo_unreference(buffer->bo);
	free(buffer);
}
