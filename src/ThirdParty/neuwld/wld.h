/* wld: wld.h
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

#ifndef WLD_H
#define WLD_H

#include <fontconfig/fontconfig.h>
#include <pixman.h>
#include <stdbool.h>
#include <stdint.h>

#define WLD_USER_ID (0xff << 24)

#define __WLD_FOURCC(a, b, c, d) ((a)           \
	                          | ((b) << 8)  \
	                          | ((c) << 16) \
	                          | ((d) << 24))

/**
 * Supported pixel formats.
 *
 * These formats can safely be interchanged with GBM and wl_drm formats.
 */
enum wld_format {
	WLD_FORMAT_XRGB8888 = __WLD_FOURCC('X', 'R', '2', '4'),
	WLD_FORMAT_ARGB8888 = __WLD_FOURCC('A', 'R', '2', '4')
};

enum wld_flags {
	WLD_FLAG_MAP = 1 << 16,
	WLD_FLAG_CURSOR = 1 << 17,
};

bool wld_lookup_named_color(const char *name, uint32_t *color);

/**** WLD Context ****/

enum wld_object_type {
	WLD_OBJECT_DATA
};

union wld_object {
	void *ptr;
	uint32_t u32;
	int i;
};

struct wld_context {
	const struct wld_context_impl *const impl;
};

struct wld_renderer *wld_create_renderer(struct wld_context *context);

struct wld_buffer *wld_create_buffer(struct wld_context *context,
                                     uint32_t width, uint32_t height,
                                     uint32_t format, uint32_t flags);

struct wld_buffer *wld_import_buffer(struct wld_context *context,
                                     uint32_t type, union wld_object object,
                                     uint32_t width, uint32_t height,
                                     uint32_t format, uint32_t pitch);

struct wld_surface *wld_create_surface(struct wld_context *context,
                                       uint32_t width, uint32_t height,
                                       uint32_t format, uint32_t flags);

void wld_destroy_context(struct wld_context *context);

/**** Font Handling ****/

struct wld_extents {
	uint32_t advance;
};

struct wld_font {
	uint32_t ascent, descent;
	uint32_t height;
	uint32_t max_advance;
};

/**
 * Create a new font context.
 *
 * This sets up the underlying FreeType library.
 */
struct wld_font_context *wld_font_create_context();

/**
 * Destroy a font context.
 */
void wld_font_destroy_context(struct wld_font_context *context);

/**
 * Open a new font from the given fontconfig match.
 */
struct wld_font *wld_font_open_pattern(struct wld_font_context *context,
                                       FcPattern *match);

/**
 * Open a new font from a fontconfig pattern string.
 */
struct wld_font *wld_font_open_name(struct wld_font_context *context,
                                    const char *name);

/**
 * Close a font.
 */
void wld_font_close(struct wld_font *font);

/**
 * Check if the given font has a particular character (in UTF-32), and if so,
 * load the glyph.
 */
bool wld_font_ensure_char(struct wld_font *font, uint32_t character);

/**
 * Calculate the text extents of the given UTF-8 string.
 *
 * @param length The maximum number of bytes in the string to process
 */
void wld_font_text_extents_n(struct wld_font *font,
                             const char *text, int32_t length,
                             struct wld_extents *extents);

static inline void
wld_font_text_extents(struct wld_font *font,
                      const char *text,
                      struct wld_extents *extents)
{
	wld_font_text_extents_n(font, text, INT32_MAX, extents);
}

/**** Buffers ****/

struct wld_exporter {
	bool (*export)(struct wld_exporter *exporter, struct wld_buffer *buffer,
	               uint32_t type, union wld_object *object);
	struct wld_exporter *next;
};

struct wld_destructor {
	void (*destroy)(struct wld_destructor *destructor);
	struct wld_destructor *next;
};

struct wld_buffer {
	const struct wld_buffer_impl *const impl;

	uint32_t width, height, pitch;
	enum wld_format format;
	pixman_region32_t damage;
	void *map;
};

bool wld_map(struct wld_buffer *buffer);
bool wld_unmap(struct wld_buffer *buffer);

bool wld_export(struct wld_buffer *buffer,
                uint32_t type, union wld_object *object);

void wld_buffer_add_exporter(struct wld_buffer *buffer,
                             struct wld_exporter *exporter);

void wld_buffer_add_destructor(struct wld_buffer *buffer,
                               struct wld_destructor *destructor);

/**
 * Increase the reference count of a buffer.
 */
void wld_buffer_reference(struct wld_buffer *buffer);

/**
 * Decrease the reference count of a buffer.
 *
 * When the reference count drops to zero, the buffer will be destroyed.
 */
void wld_buffer_unreference(struct wld_buffer *buffer);

/**** Surfaces ****/

struct wld_surface {
	const struct wld_surface_impl *const impl;
};

pixman_region32_t *wld_surface_damage(struct wld_surface *surface,
                                      pixman_region32_t *new_damage);

struct wld_buffer *wld_surface_take(struct wld_surface *surface);

void wld_surface_release(struct wld_surface *surface,
                         struct wld_buffer *buffer);

bool wld_swap(struct wld_surface *surface);

void wld_destroy_surface(struct wld_surface *surface);

/**** Renderers ****/

struct wld_renderer {
	const struct wld_renderer_impl *const impl;
	struct wld_buffer *target;
};

enum wld_capability {
	WLD_CAPABILITY_READ = 1 << 0,
	WLD_CAPABILITY_WRITE = 1 << 1,
};

void wld_destroy_renderer(struct wld_renderer *renderer);

uint32_t wld_capabilities(struct wld_renderer *renderer,
                          struct wld_buffer *buffer);

bool wld_set_target_buffer(struct wld_renderer *renderer,
                           struct wld_buffer *buffer);

bool wld_set_target_surface(struct wld_renderer *renderer,
                            struct wld_surface *surface);

void wld_fill_rectangle(struct wld_renderer *renderer, uint32_t color,
                        int32_t x, int32_t y, uint32_t width, uint32_t height);

void wld_fill_region(struct wld_renderer *renderer, uint32_t color,
                     pixman_region32_t *region);

void wld_copy_rectangle(struct wld_renderer *renderer,
                        struct wld_buffer *buffer,
                        int32_t dst_x, int32_t dst_y,
                        int32_t src_x, int32_t src_y,
                        uint32_t width, uint32_t height);

void wld_copy_region(struct wld_renderer *renderer,
                     struct wld_buffer *buffer,
                     int32_t dst_x, int32_t dst_y, pixman_region32_t *region);

void wld_blend_region(struct wld_renderer *renderer,
                      struct wld_buffer *buffer,
                      int32_t dst_x, int32_t dst_y,
                      pixman_region32_t *region);

void wld_draw_circle(struct wld_renderer *renderer, uint32_t color,
				int32_t x, int32_t y, uint32_t r, bool fill);

void wld_draw_line(struct wld_renderer *renderer, uint32_t color,
			 int32_t x1, int32_t y1, int32_t x2, int32_t y2);

/**
 * Draw a UTF-8 text string to the given buffer.
 *
 * @param length    The maximum number of bytes in the string to process. If
 *                  length is -1, then draw until a NULL byte is found.
 * @param extents   If not NULL, will be initialized to the extents of the
 *                  drawn text
 */
void wld_draw_text(struct wld_renderer *renderer,
                   struct wld_font *font, uint32_t color,
                   int32_t x, int32_t y, const char *text, uint32_t length,
                   struct wld_extents *extents);

void wld_flush(struct wld_renderer *renderer);

#endif
