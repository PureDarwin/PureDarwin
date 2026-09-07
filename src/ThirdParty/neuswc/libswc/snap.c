#include "snap.h"
#include "compositor.h"
#include "internal.h"
#include "pointer.h"
#include "screen.h"
#include "seat.h"
#include "shm.h"
#include "util.h"

#include "swc_snap-server-protocol.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <wayland-server.h>
#include <wld/wld.h>

/* get cursor */
static void
cursor(uint8_t *dst, uint32_t dst_width, uint32_t dst_height,
       uint32_t dst_pitch, struct screen *screen)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;
	struct wld_buffer *cursor_buf;
	const uint8_t *src;
	int32_t dst_x, dst_y;
	int32_t src_x = 0, src_y = 0;
	uint32_t copy_w, copy_h;

	if (!pointer || !pointer->cursor.buffer || !pointer->cursor.view.buffer) {
		return;
	}

	if (!(pointer->cursor.view.screens & screen_mask(screen))) {
		return;
	}

	cursor_buf = pointer->cursor.buffer;
	if (!wld_map(cursor_buf) || !cursor_buf->map) {
		return;
	}

	dst_x = pointer->cursor.view.geometry.x - screen->base.geometry.x;
	dst_y = pointer->cursor.view.geometry.y - screen->base.geometry.y;

	if (dst_x >= (int32_t)dst_width || dst_y >= (int32_t)dst_height ||
	    dst_x + (int32_t)cursor_buf->width <= 0 ||
	    dst_y + (int32_t)cursor_buf->height <= 0) {
		wld_unmap(cursor_buf);
		return;
	}

	if (dst_x < 0) {
		src_x = -dst_x;
		dst_x = 0;
	}
	if (dst_y < 0) {
		src_y = -dst_y;
		dst_y = 0;
	}

	copy_w = cursor_buf->width - (uint32_t)src_x;
	if (copy_w > dst_width - (uint32_t)dst_x) {
		copy_w = dst_width - (uint32_t)dst_x;
	}
	copy_h = cursor_buf->height - (uint32_t)src_y;
	if (copy_h > dst_height - (uint32_t)dst_y) {
		copy_h = dst_height - (uint32_t)dst_y;
	}

	src = cursor_buf->map;

	for (uint32_t y = 0; y < copy_h; y++) {
		const uint32_t *src_row =
		    (const uint32_t *)(src + ((size_t)(src_y + (int32_t)y) *
		                              cursor_buf->pitch)) +
		    src_x;
		uint32_t *dst_row =
		    (uint32_t *)(dst + ((size_t)(dst_y + (int32_t)y) * dst_pitch)) +
		    dst_x;

		for (uint32_t x = 0; x < copy_w; x++) {
			uint32_t src_px = src_row[x];
			uint32_t a = src_px >> 24;

			if (a == 0) {
				continue;
			}
			if (a == 255) {
				dst_row[x] = 0xFF000000 | (src_px & 0x00FFFFFF);
				continue;
			}

			uint32_t dst_px = dst_row[x];
			uint32_t inv = 255 - a;
			uint32_t r = ((src_px >> 16) & 0xFF) +
			             ((((dst_px >> 16) & 0xFF) * inv + 127) / 255);
			uint32_t g = ((src_px >> 8) & 0xFF) +
			             ((((dst_px >> 8) & 0xFF) * inv + 127) / 255);
			uint32_t b =
			    (src_px & 0xFF) + (((dst_px & 0xFF) * inv + 127) / 255);

			dst_row[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
		}
	}

	wld_unmap(cursor_buf);
}

static void
send_buffer_metadata(struct wl_resource *resource, uint32_t width,
                     uint32_t height)
{
	swc_snap_send_buffer(resource, width, height,
	                     SWC_SNAP_PIXEL_FORMAT_ARGB8888);
}

static bool
get_screen(struct screen **screen, uint32_t *width, uint32_t *height)
{
	struct screen *s;

	if (wl_list_empty(&swc.screens)) {
		return false;
	}

	s = wl_container_of(swc.screens.next, s, link);
	*screen = s;
	*width = s->base.geometry.width;
	*height = s->base.geometry.height;
	return true;
}

static void
capture(struct wl_client *client, struct wl_resource *resource,
        struct wl_resource *buffer_resource, uint32_t flags)
{
	struct screen *screen;
	struct swc_shm_buffer_info target;
	struct wld_buffer *shm_buffer;
	uint8_t *src_pixels;
	uint8_t *dst_pixels;
	uint32_t width, height;
	uint32_t target_width, target_height, target_stride, target_format;
	uint32_t row_bytes;
	struct timespec ts;
	(void)client;

	if (!get_screen(&screen, &width, &height)) {
		swc_snap_send_failed(resource, SWC_SNAP_FAILURE_REASON_NO_SCREEN);
		return;
	}

	send_buffer_metadata(resource, width, height);

	if (!shm_buffer_get_info(buffer_resource, &target)) {
		swc_snap_send_failed(resource,
		                     SWC_SNAP_FAILURE_REASON_UNSUPPORTED_BUFFER);
		return;
	}

	target_format = target.format;
	if (target_format != WL_SHM_FORMAT_XRGB8888 &&
	    target_format != WL_SHM_FORMAT_ARGB8888) {
		swc_snap_send_failed(resource,
		                     SWC_SNAP_FAILURE_REASON_UNSUPPORTED_BUFFER);
		return;
	}

	target_width = (uint32_t)target.width;
	target_height = (uint32_t)target.height;
	target_stride = (uint32_t)target.stride;
	row_bytes = width * 4;

	if (target_width < width || target_height < height ||
	    target_stride < row_bytes) {
		swc_snap_send_failed(resource, SWC_SNAP_FAILURE_REASON_SIZE_MISMATCH);
		return;
	}
	if (!target.writable) {
		swc_snap_send_failed(resource, SWC_SNAP_FAILURE_REASON_UNSUPPORTED_BUFFER);
		return;
	}

	shm_buffer = compositor_render_to_shm(screen);
	if (!shm_buffer) {
		swc_snap_send_failed(resource, SWC_SNAP_FAILURE_REASON_INTERNAL);
		return;
	}

	if (!wld_map(shm_buffer) || !shm_buffer->map) {
		wld_buffer_unreference(shm_buffer);
		swc_snap_send_failed(resource, SWC_SNAP_FAILURE_REASON_INTERNAL);
		return;
	}

	src_pixels = shm_buffer->map;

	if (flags & SWC_SNAP_FLAGS_OVERLAY_CURSOR) {
		cursor(src_pixels, width, height, shm_buffer->pitch, screen);
	}

	dst_pixels = target.data;

	for (uint32_t y = 0; y < height; y++) {
		memcpy(dst_pixels + (size_t)y * target_stride,
		       src_pixels + (size_t)y * shm_buffer->pitch, row_bytes);
	}

	wld_unmap(shm_buffer);
	wld_buffer_unreference(shm_buffer);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	swc_snap_send_ready(resource, (uint32_t)ts.tv_sec, (uint32_t)ts.tv_nsec);
}

static const struct swc_snap_interface snap_impl = {
    .destroy = destroy_resource,
    .capture = capture,
};

static void
bind_snap(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &swc_snap_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &snap_impl, NULL, NULL);

	if (!wl_list_empty(&swc.screens)) {
		struct screen *screen;

		screen = wl_container_of(swc.screens.next, screen, link);
		send_buffer_metadata(resource, screen->base.geometry.width,
		                     screen->base.geometry.height);
	}
}

struct wl_global *
snap_manager_create(struct wl_display *display)
{
	return wl_global_create(display, &swc_snap_interface, 2, NULL, &bind_snap);
}
