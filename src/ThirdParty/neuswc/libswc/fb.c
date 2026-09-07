#include "fb.h"
#include "internal.h"
#include "output.h"
#include "pointer.h"
#include "screen.h"
#include "seat.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <wld/pixman.h>
#include <wld/wld.h>

struct swc_fb swc_fb;

/* for the cursor */ 
static uint32_t
blend(uint32_t background, uint32_t foreground)
{
	uint32_t alpha = foreground >> 24;
	uint32_t inverse = 255 - alpha;
	uint32_t red = ((foreground >> 16) & 0xff) +
	               (((background >> 16) & 0xff) * inverse + 127) / 255;
	uint32_t green = ((foreground >> 8) & 0xff) +
	                 (((background >> 8) & 0xff) * inverse + 127) / 255;
	uint32_t blue = (foreground & 0xff) +
	                ((background & 0xff) * inverse + 127) / 255;
	return (MIN(red, 255) << 16) | (MIN(green, 255) << 8) | MIN(blue, 255);
}

bool
fb_initialize(void)
{
	if (!framebuffer_initialize(&swc_fb)) {
		return false;
	}
	swc_fb.pitch = (size_t)swc_fb.width * sizeof(*swc_fb.pixels);
	swc_fb.pixels = calloc(swc_fb.height, swc_fb.pitch);
	if (!swc_fb.pixels) {
		goto error0;
	}
	swc_fb.backend.context = wld_pixman_create_context();
	if (!swc_fb.backend.context) {
		goto error1;
	}
	swc_fb.backend.renderer = wld_create_renderer(swc_fb.backend.context);
	if (!swc_fb.backend.renderer) {
		goto error2;
	}
	swc_fb.backend.cursor_width = 64;
	swc_fb.backend.cursor_height = 64;
	swc.backend = &swc_fb.backend;
	return true;

error2:
	wld_destroy_context(swc_fb.backend.context);
error1:
	free(swc_fb.pixels);
error0:
	framebuffer_finalize(&swc_fb);
	return false;
}

void
fb_finalize(void)
{
	wld_destroy_renderer(swc_fb.backend.renderer);
	wld_destroy_context(swc_fb.backend.context);
	free(swc_fb.pixels);
	framebuffer_finalize(&swc_fb);
}

bool
fb_create_screens(struct wl_list *screens)
{
	struct output *output;
	struct screen *screen;

	output = output_new_fb(swc_fb.width, swc_fb.height, framebuffer_name());
	if (!output) {
		return false;
	}
	screen = screen_new(output);
	if (!screen) {
		output_destroy(output);
		return false;
	}
	screen->id = 0;
	output->screen = screen;
	wl_list_insert(screens, &screen->link);
	return true;
}

bool
fb_present(struct wld_buffer *buffer, int32_t origin_x, int32_t origin_y)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;
	struct wld_buffer *cursor = NULL;
	int32_t cursor_x = 0, cursor_y = 0;
	uint32_t width, height, x, y;
	bool cursor_mapped = false;

	if (!buffer || !wld_map(buffer)) {
		return false;
	}
	if (pointer && pointer->cursor.view.buffer) {
		cursor = pointer->cursor.buffer;
		cursor_x = pointer->cursor.view.geometry.x - origin_x;
		cursor_y = pointer->cursor.view.geometry.y - origin_y;
		cursor_mapped = wld_map(cursor);
	}

	width = buffer->width < swc_fb.width ? buffer->width : swc_fb.width;
	height = buffer->height < swc_fb.height ? buffer->height : swc_fb.height;
	if (width != swc_fb.width || height != swc_fb.height) {
		memset(swc_fb.pixels, 0, swc_fb.height * swc_fb.pitch);
	}
	for (y = 0; y < height; ++y) {
		uint32_t *source = (uint32_t *)((uint8_t *)buffer->map +
		                                (size_t)y * buffer->pitch);
		uint32_t *destination = (uint32_t *)((uint8_t *)swc_fb.pixels +
		                                     (size_t)y * swc_fb.pitch);
		for (x = 0; x < width; ++x) {
			uint32_t pixel = source[x];
			int32_t cx = (int32_t)x - cursor_x;
			int32_t cy = (int32_t)y - cursor_y;
			if (cursor_mapped && cx >= 0 && cy >= 0 &&
			    (uint32_t)cx < cursor->width && (uint32_t)cy < cursor->height) {
				uint32_t foreground = *(uint32_t *)((uint8_t *)cursor->map +
				    (size_t)cy * cursor->pitch + (size_t)cx * 4);
				pixel = blend(pixel, foreground);
			}
			destination[x] = pixel;
		}
	}
	if (cursor_mapped) {
		wld_unmap(cursor);
	}
	wld_unmap(buffer);
	return framebuffer_present(&swc_fb);
}
