#include "decor.h"
#include "backend.h"

#include "compositor.h"
#include "drm.h"
#include "internal.h"
#include "swc.h"
#include "util.h"
#include "window.h"

#include <pixman.h>
#include <stdlib.h>
#include <string.h>
#include <wld/wld.h>

#define DEFAULT_DECOR_FONT "sans-serif:size=10"

static struct wld_font_context *font_context;

enum decor_part_index {
	DECOR_PART_TOP_LEFT,
	DECOR_PART_TOP,
	DECOR_PART_TOP_RIGHT,
	DECOR_PART_LEFT,
	DECOR_PART_RIGHT,
	DECOR_PART_BOTTOM_LEFT,
	DECOR_PART_BOTTOM,
	DECOR_PART_BOTTOM_RIGHT,
	DECOR_PART_COUNT,
};

static uint32_t
decor_title_length(struct wld_font *font, const char *title, uint32_t max_width)
{
	uint32_t len = 0;
	struct wld_extents extents;

	if (!title || !max_width) {
		return 0;
	}

	while (title[len]) {
		uint32_t next = len + 1;

		/* we skip utf-8 continuation bytes */
		while ((title[next] & 0xc0) == 0x80) {
			next++;
		}

		wld_font_text_extents_n(font, title, (int32_t)next, &extents);
		if (extents.advance > max_width) {
			break;
		}
		len = next;
	}

	return len;
}

static uint32_t
utf8_next_len(const char *text, uint32_t offset)
{
	uint32_t next = offset + 1;

	while ((text[next] & 0xc0) == 0x80) {
		next++;
	}

	return next - offset;
}

static uint32_t
decor_title_stacked_length(struct wld_font *font, const char *title,
                           uint32_t max_height, uint32_t *glyph_count)
{
	uint32_t len = 0, count = 0;

	if (!title || !font->height) {
		*glyph_count = 0;
		return 0;
	}

	while (title[len] && (count + 1) * font->height <= max_height) {
		len += utf8_next_len(title, len);
		count++;
	}

	*glyph_count = count;
	return len;
}

static bool
streq(const char *a, const char *b)
{
	if (!a || !b) {
		return a == b;
	}
	return strcmp(a, b) == 0;
}

static const struct swc_decor_part *
decor_part_at(const struct swc_decor_parts *parts, enum decor_part_index index)
{
	if (!parts) {
		return NULL;
	}

	switch (index) {
	case DECOR_PART_TOP_LEFT:
		return &parts->top_left;
	case DECOR_PART_TOP:
		return &parts->top;
	case DECOR_PART_TOP_RIGHT:
		return &parts->top_right;
	case DECOR_PART_LEFT:
		return &parts->left;
	case DECOR_PART_RIGHT:
		return &parts->right;
	case DECOR_PART_BOTTOM_LEFT:
		return &parts->bottom_left;
	case DECOR_PART_BOTTOM:
		return &parts->bottom;
	case DECOR_PART_BOTTOM_RIGHT:
		return &parts->bottom_right;
	case DECOR_PART_COUNT:
		break;
	}

	return NULL;
}

static void
close_decor_parts(struct compositor_view *view)
{
	for (uint32_t i = 0; i < DECOR_PART_COUNT; ++i) {
		if (view->decor.parts[i].tiled_buffer) {
			wld_buffer_unreference(view->decor.parts[i].tiled_buffer);
			view->decor.parts[i].tiled_buffer = NULL;
		}
		if (view->decor.parts[i].buffer) {
			wld_buffer_unreference(view->decor.parts[i].buffer);
			view->decor.parts[i].buffer = NULL;
		}
		free(view->decor.parts[i].data);
		view->decor.parts[i].data = NULL;
		view->decor.parts[i].width = 0;
		view->decor.parts[i].height = 0;
		view->decor.parts[i].stride = 0;
		view->decor.parts[i].tiled_width = 0;
		view->decor.parts[i].tiled_height = 0;
		view->decor.parts[i].opaque = false;
	}
	view->decor.parts_key = NULL;
}

static bool
decor_part_is_empty(const struct swc_decor_part *part)
{
	return !part || !part->data || !part->width || !part->height ||
	       !part->stride;
}

static bool
decor_part_equal(const struct decor_part_buffer *owned,
                 const struct swc_decor_part *part)
{
	size_t size;

	if (decor_part_is_empty(part)) {
		return !owned->data && !owned->buffer && !owned->width && !owned->height &&
		       !owned->stride;
	}

	if (!owned->data || owned->width != part->width ||
	    owned->height != part->height || owned->stride != part->stride) {
		return false;
	}

	size = (size_t)part->stride * part->height;
	return memcmp(owned->data, part->data, size) == 0;
}

static bool
decor_parts_equal(struct compositor_view *view, const struct swc_decor_parts *parts)
{
	for (uint32_t i = 0; i < DECOR_PART_COUNT; ++i) {
		if (!decor_part_equal(&view->decor.parts[i], decor_part_at(parts, i))) {
			return false;
		}
	}

	return true;
}

static bool
copy_decor_part(struct decor_part_buffer *dst, const struct swc_decor_part *src)
{
	size_t row_size, size;

	if (decor_part_is_empty(src)) {
		memset(dst, 0, sizeof(*dst));
		return true;
	}

	row_size = (size_t)src->width * 4;
	if (src->stride < row_size ||
	    (src->height && src->stride > SIZE_MAX / src->height)) {
		return false;
	}
	size = (size_t)src->stride * src->height;
	dst->data = malloc(size);
	if (!dst->data) {
		return false;
	}
	memcpy(dst->data, src->data, size);

	dst->width = src->width;
	dst->height = src->height;
	dst->stride = src->stride;
	dst->opaque = true;
	for (uint32_t y = 0; y < src->height && dst->opaque; ++y) {
		const uint8_t *row = (const uint8_t *)src->data + (size_t)y * src->stride;

		for (uint32_t x = 0; x < src->width; ++x) {
			uint32_t pixel;

			memcpy(&pixel, row + (size_t)x * 4, sizeof(pixel));
			if ((pixel >> 24) != 0xff) {
				dst->opaque = false;
				break;
			}
		}
	}
	/* DRM renderers only allow read support for their native buffers not pixman/shmbuffers */
	dst->buffer = wld_create_buffer(swc.backend->context, src->width, src->height,
	                                WLD_FORMAT_ARGB8888, WLD_FLAG_MAP);
	if (!dst->buffer) {
		free(dst->data);
		memset(dst, 0, sizeof(*dst));
		return false;
	}

	if (dst->buffer->pitch < row_size || !wld_map(dst->buffer)) {
		wld_buffer_unreference(dst->buffer);
		free(dst->data);
		memset(dst, 0, sizeof(*dst));
		return false;
	}
	for (uint32_t y = 0; y < src->height; ++y) {
		memcpy((uint8_t *)dst->buffer->map + (size_t)y * dst->buffer->pitch,
		       (uint8_t *)dst->data + (size_t)y * src->stride, row_size);
	}
	wld_unmap(dst->buffer);

	return true;
}

static bool
copy_decor_parts(struct compositor_view *view, const struct swc_decor_parts *parts)
{
	struct decor_part_buffer copied[DECOR_PART_COUNT] = { 0 };
	uint32_t i;

	for (i = 0; i < DECOR_PART_COUNT; ++i) {
		if (!copy_decor_part(&copied[i], decor_part_at(parts, i))) {
			goto error;
		}
	}

	close_decor_parts(view);
	memcpy(view->decor.parts, copied, sizeof(copied));
	view->decor.parts_key = parts;
	return true;

error:
	for (i = 0; i < DECOR_PART_COUNT; ++i) {
		if (copied[i].buffer) {
			wld_buffer_unreference(copied[i].buffer);
		}
		free(copied[i].data);
	}
	return false;
}

static void
close_decor_font(struct compositor_view *view)
{
	if (view->decor.font) {
		wld_font_close(view->decor.font);
		view->decor.font = NULL;
	}
	free(view->decor.font_name);
	view->decor.font_name = NULL;
}

static void
close_decor_string(struct compositor_view *view)
{
	free(view->decor.string);
	view->decor.string = NULL;
}

static struct wld_buffer *
get_tiled_decor_buffer(struct decor_part_buffer *part, uint32_t width,
                       uint32_t height)
{
	struct wld_buffer *buffer;
	uint32_t buffer_width = width, buffer_height = height;
	size_t row_size;

	if (width == part->width && height == part->height) {
		return part->buffer;
	}
	if (part->tiled_buffer && part->tiled_width >= width &&
	    part->tiled_height >= height) {
		return part->tiled_buffer;
	}

	if (part->tiled_buffer) {
		wld_buffer_unreference(part->tiled_buffer);
		part->tiled_buffer = NULL;
		part->tiled_width = 0;
		part->tiled_height = 0;
	}

	if (width != part->width && width <= UINT32_MAX - 255) {
		buffer_width = (width + 255) & ~255U;
	}
	if (height != part->height && height <= UINT32_MAX - 255) {
		buffer_height = (height + 255) & ~255U;
	}

	if (buffer_width > SIZE_MAX / 4) {
		return NULL;
	}
	row_size = (size_t)buffer_width * 4;
	buffer = wld_create_buffer(swc.backend->context, buffer_width, buffer_height,
	                           WLD_FORMAT_ARGB8888, WLD_FLAG_MAP);
	if (!buffer || buffer->pitch < row_size || !wld_map(buffer)) {
		if (buffer) {
			wld_buffer_unreference(buffer);
		}
		return NULL;
	}

	for (uint32_t y = 0; y < buffer_height; ++y) {
		uint8_t *dst = (uint8_t *)buffer->map + (size_t)y * buffer->pitch;
		const uint8_t *src = (const uint8_t *)part->data +
		                     (size_t)(y % part->height) * part->stride;

		for (uint32_t x = 0; x < buffer_width; x += part->width) {
			uint32_t copy_width = MIN(part->width, buffer_width - x);

			memcpy(dst + (size_t)x * 4, src, (size_t)copy_width * 4);
		}
	}
	wld_unmap(buffer);

	part->tiled_buffer = buffer;
	part->tiled_width = buffer_width;
	part->tiled_height = buffer_height;
	return buffer;
}

/* draw the decor part by tiling it across the target reigon.  
 * the part buffer is repeated to fill the entirety of some width x height area
 * but then we cache the expanded version so it can be just one op
 * instead of one op per every time we repeat it */
static void
draw_decor_part(struct wld_renderer *renderer,
                const struct swc_rectangle *target_geom,
                struct compositor_view *view, pixman_region32_t *damage,
                struct decor_part_buffer *part, int32_t x, int32_t y,
                uint32_t width, uint32_t height)
{
	pixman_region32_t region;
	struct wld_buffer *buffer;

	if (!part->buffer || !part->width || !part->height || !width || !height) {
		return;
	}
	if (!(buffer = get_tiled_decor_buffer(part, width, height))) {
		return;
	}

	pixman_region32_init_rect(&region, x, y, width, height);
	pixman_region32_intersect(&region, &region, damage);
	pixman_region32_subtract(&region, &region, &view->clip);
	pixman_region32_translate(&region, -x, -y);
	if (part->opaque) {
		wld_copy_region(renderer, buffer, x - target_geom->x,
		                y - target_geom->y, &region);
	} else {
		wld_blend_region(renderer, buffer, x - target_geom->x,
		                 y - target_geom->y, &region);
	}

	pixman_region32_fini(&region);
}

static bool
decor_parts_complete(struct compositor_view *view)
{
	for (uint32_t i = 0; i < DECOR_PART_COUNT; ++i) {
		if (!view->decor.parts[i].buffer) {
			return false;
		}
	}

	return true;
}

bool
decor_initialize(void)
{
	font_context = wld_font_create_context();
	return font_context != NULL;
}

void
decor_finalize(void)
{
	if (font_context) {
		wld_font_destroy_context(font_context);
		font_context = NULL;
	}
}

void
decor_view_initialize(struct compositor_view *view)
{
	memset(&view->decor.text, 0, sizeof(view->decor.text));
	view->decor.parts_key = NULL;
	memset(view->decor.parts, 0, sizeof(view->decor.parts));
	view->decor.string = NULL;
	view->decor.font_name = NULL;
	view->decor.font = NULL;
}

void
decor_view_finalize(struct compositor_view *view)
{
	close_decor_parts(view);
	close_decor_string(view);
	close_decor_font(view);
}

void
decor_repaint(struct wld_renderer *renderer,
              const struct swc_rectangle *target_geom,
              struct compositor_view *view, pixman_region32_t *damage)
{
	const struct swc_rectangle *geom = &view->base.geometry;
	const struct swc_decor_text *text = &view->decor.text;
	struct wld_font *font = view->decor.font;
	const char *title = view->decor.string;
	pixman_region32_t decor_region, content_region;
	uint32_t title_len, max_width, decor_size;
	int32_t x, y, base_x, base_y, advance, available_width;
	struct wld_extents extents;
	pixman_region32_t title_region;
	int32_t outer_x = geom->x - (int32_t)view->decor.left;
	int32_t outer_y = geom->y - (int32_t)view->decor.top;
	uint32_t outer_width = geom->width + view->decor.left + view->decor.right;
	uint32_t outer_height = geom->height + view->decor.top + view->decor.bottom;
	uint32_t tl_width = view->decor.parts[DECOR_PART_TOP_LEFT].width;
	uint32_t tl_height = view->decor.parts[DECOR_PART_TOP_LEFT].height;
	uint32_t tr_width = view->decor.parts[DECOR_PART_TOP_RIGHT].width;
	uint32_t tr_height = view->decor.parts[DECOR_PART_TOP_RIGHT].height;
	uint32_t bl_width = view->decor.parts[DECOR_PART_BOTTOM_LEFT].width;
	uint32_t bl_height = view->decor.parts[DECOR_PART_BOTTOM_LEFT].height;
	uint32_t br_width = view->decor.parts[DECOR_PART_BOTTOM_RIGHT].width;
	uint32_t br_height = view->decor.parts[DECOR_PART_BOTTOM_RIGHT].height;

	if (!view->decor.top && !view->decor.right && !view->decor.bottom &&
	    !view->decor.left) {
		return;
	}

	pixman_region32_init_rect(&decor_region, outer_x, outer_y, outer_width,
	                          outer_height);
	pixman_region32_init_rect(&content_region, geom->x, geom->y, geom->width,
	                          geom->height);
	pixman_region32_subtract(&decor_region, &decor_region, &content_region);
	pixman_region32_intersect(&decor_region, &decor_region, damage);
	pixman_region32_subtract(&decor_region, &decor_region, &view->clip);
	if (!decor_parts_complete(view) &&
	    pixman_region32_not_empty(&decor_region)) {
		pixman_region32_translate(&decor_region, -target_geom->x, -target_geom->y);
		wld_fill_region(renderer, view->decor.color, &decor_region);
		pixman_region32_translate(&decor_region, target_geom->x, target_geom->y);
	}
	pixman_region32_fini(&decor_region);
	pixman_region32_fini(&content_region);

	draw_decor_part(renderer, target_geom, view, damage,
	                &view->decor.parts[DECOR_PART_TOP_LEFT], outer_x, outer_y,
	                tl_width, tl_height);
	draw_decor_part(renderer, target_geom, view, damage,
	                &view->decor.parts[DECOR_PART_TOP_RIGHT],
	                outer_x + (int32_t)outer_width - (int32_t)tr_width, outer_y,
	                tr_width, tr_height);
	draw_decor_part(renderer, target_geom, view, damage,
	                &view->decor.parts[DECOR_PART_BOTTOM_LEFT], outer_x,
	                outer_y + (int32_t)outer_height - (int32_t)bl_height,
	                bl_width, bl_height);
	draw_decor_part(renderer, target_geom, view, damage,
	                &view->decor.parts[DECOR_PART_BOTTOM_RIGHT],
	                outer_x + (int32_t)outer_width - (int32_t)br_width,
	                outer_y + (int32_t)outer_height - (int32_t)br_height,
	                br_width, br_height);

	if (outer_width > tl_width + tr_width) {
		draw_decor_part(renderer, target_geom, view, damage,
		                &view->decor.parts[DECOR_PART_TOP],
		                outer_x + (int32_t)tl_width, outer_y,
		                outer_width - tl_width - tr_width, view->decor.top);
	}
	if (outer_width > bl_width + br_width) {
		draw_decor_part(renderer, target_geom, view, damage,
		                &view->decor.parts[DECOR_PART_BOTTOM],
		                outer_x + (int32_t)bl_width,
		                outer_y + (int32_t)outer_height - (int32_t)view->decor.bottom,
		                outer_width - bl_width - br_width, view->decor.bottom);
	}

	if (outer_height > tl_height + bl_height) {
		draw_decor_part(renderer, target_geom, view, damage,
		                &view->decor.parts[DECOR_PART_LEFT], outer_x,
		                outer_y + (int32_t)tl_height, view->decor.left,
		                outer_height - tl_height - bl_height);
	}
	if (outer_height > tr_height + br_height) {
		draw_decor_part(renderer, target_geom, view, damage,
		                &view->decor.parts[DECOR_PART_RIGHT],
		                outer_x + (int32_t)outer_width - (int32_t)view->decor.right,
		                outer_y + (int32_t)tr_height, view->decor.right,
		                outer_height - tr_height - br_height);
	}

	if (!title || !font || !text->enabled) {
		return;
	}

	/* calculate text position based on which edge
	 * the decor is on. base_x base_y is the top-left corner,
	 * max_width is available space
	 * decor_size is perpendicular dimension of the edge where text is being drawn */
	switch (text->edge) {
	case SWC_DECOR_EDGE_TOP:
		if (!view->decor.top) {
			return;
		}
		base_x = geom->x - (int32_t)view->decor.left;
		base_y = geom->y - (int32_t)view->decor.top;
		max_width = geom->width + view->decor.left + view->decor.right;
		decor_size = view->decor.top;
		break;
	case SWC_DECOR_EDGE_BOTTOM:
		if (!view->decor.bottom) {
			return;
		}
		base_x = geom->x - (int32_t)view->decor.left;
		base_y = geom->y + (int32_t)geom->height;
		max_width = geom->width + view->decor.left + view->decor.right;
		decor_size = view->decor.bottom;
		break;
	case SWC_DECOR_EDGE_LEFT:
		if (!view->decor.left) {
			return;
		}
		base_x = geom->x - (int32_t)view->decor.left;
		base_y = geom->y - (int32_t)view->decor.top;
		max_width = view->decor.left;
		decor_size = geom->height + view->decor.top + view->decor.bottom;
		break;
	case SWC_DECOR_EDGE_RIGHT:
		if (!view->decor.right) {
			return;
		}
		base_x = geom->x + (int32_t)geom->width;
		base_y = geom->y - (int32_t)view->decor.top;
		max_width = view->decor.right;
		decor_size = geom->height + view->decor.top + view->decor.bottom;
		break;
	default:
		return;
	}

	x = base_x;
	y = base_y;

	pixman_region32_init_rect(&title_region, x, y, max_width, decor_size);
	pixman_region32_intersect(&title_region, &title_region, damage);
	pixman_region32_subtract(&title_region, &title_region, &view->clip);
	if (!pixman_region32_not_empty(&title_region)) {
		pixman_region32_fini(&title_region);
		return;
	}
	pixman_region32_fini(&title_region);

	if (max_width <= text->padding * 2 || decor_size < font->height) {
		return;
	}
	max_width -= text->padding * 2;

	if (text->edge == SWC_DECOR_EDGE_LEFT || text->edge == SWC_DECOR_EDGE_RIGHT) {
		uint32_t glyph_count, glyph_offset = 0, available_height, total_height;

		if (decor_size <= text->padding * 2) {
			return;
		}

		available_height = decor_size - text->padding * 2;
		title_len = decor_title_stacked_length(font, title, available_height,
		                                      &glyph_count);
		if (!title_len) {
			return;
		}

		total_height = glyph_count * font->height;
		y += (int32_t)text->padding;
		switch (text->align) {
		case SWC_DECOR_ALIGN_START:
			break;
		case SWC_DECOR_ALIGN_CENTER:
			y += (int32_t)((available_height - total_height) / 2);
			break;
		case SWC_DECOR_ALIGN_END:
			y += (int32_t)(available_height - total_height);
			break;
		}

		while (glyph_offset < title_len) {
			uint32_t glyph_len = utf8_next_len(title, glyph_offset);
			int32_t glyph_x;

			wld_font_text_extents_n(font, title + glyph_offset,
			                        (int32_t)glyph_len, &extents);
			advance = extents.advance > 0 ? extents.advance : 0;
			glyph_x = x + (int32_t)text->padding;
			if ((uint32_t)advance < max_width) {
				glyph_x += ((int32_t)max_width - advance) / 2;
			}

			wld_draw_text(renderer, font, text->color,
			              glyph_x + text->offset_x - target_geom->x,
			              y + (int32_t)font->ascent + text->offset_y -
			                  target_geom->y,
			              title + glyph_offset, glyph_len, NULL);

			glyph_offset += glyph_len;
			y += (int32_t)font->height;
		}

		return;
	}

	title_len = decor_title_length(font, title, max_width);
	if (!title_len) {
		return;
	}
	wld_font_text_extents_n(font, title, (int32_t)title_len, &extents);
	advance = extents.advance > 0 ? extents.advance : 0;
	available_width = (int32_t)max_width;

	switch (text->align) {
	case SWC_DECOR_ALIGN_START:
		x += text->padding;
		break;
	case SWC_DECOR_ALIGN_CENTER:
		x += (int32_t)text->padding + (available_width - advance) / 2;
		break;
	case SWC_DECOR_ALIGN_END:
		x += (int32_t)text->padding + (int32_t)max_width - advance;
		break;
	}

	y += (int32_t)((decor_size - font->height) / 2) + (int32_t)font->ascent;

	x += text->offset_x;
	y += text->offset_y;

	wld_draw_text(renderer, font, text->color, x - target_geom->x,
	              y - target_geom->y, title, title_len, NULL);
}

void
decor_view_set(struct compositor_view *view, const struct swc_decor *decor)
{
	const char *font_name = NULL;
	struct wld_font *font = NULL;
	struct swc_decor_text text = { 0 };
	char *owned_string = NULL;
	char *owned_font_name = NULL;
	const struct swc_decor_parts *parts = NULL;
	uint32_t color = 0, top = 0, right = 0, bottom = 0, left = 0;

	if (!decor) {
		goto apply;
	}

	color = decor->color;
	top = decor->top;
	right = decor->right;
	bottom = decor->bottom;
	left = decor->left;
	parts = decor->parts;
	text = decor->title;
	if (text.enabled) {
		font_name = text.font ? text.font : DEFAULT_DECOR_FONT;
	}

	if (view->decor.color == color && view->decor.top == top &&
	    view->decor.right == right && view->decor.bottom == bottom &&
	    view->decor.left == left &&
	    view->decor.text.enabled == text.enabled &&
	    view->decor.text.edge == text.edge &&
	    view->decor.text.align == text.align &&
	    streq(view->decor.string, text.string) &&
	    view->decor.text.color == text.color &&
	    view->decor.text.padding == text.padding &&
	    view->decor.text.offset_x == text.offset_x &&
	    view->decor.text.offset_y == text.offset_y &&
	    streq(view->decor.font_name, font_name) &&
	    decor_parts_equal(view, parts)) {
		return;
	}

	if (text.string) {
		owned_string = strdup(text.string);
	}

	if (font_name) {
		owned_font_name = strdup(font_name);
		if (owned_font_name && font_context) {
			font = wld_font_open_name(font_context, font_name);
		}
	}

apply:
	view->decor.color = color;
	view->decor.top = top;
	view->decor.right = right;
	view->decor.bottom = bottom;
	view->decor.left = left;
	view->decor.text = text;
	view->decor.text.string = NULL;
	view->decor.text.font = NULL;
	if (!copy_decor_parts(view, parts)) {
		close_decor_parts(view);
	}
	close_decor_string(view);
	view->decor.string = owned_string;
	close_decor_font(view);
	view->decor.font_name = owned_font_name;
	view->decor.font = font;
	view->decor.damaged = true;
}

void
decor_view_damage(struct compositor_view *view)
{
	view->decor.damaged = true;
}
