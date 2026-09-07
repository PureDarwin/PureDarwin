#include "fb.h"
#include "launch.h"
#include "launch/protocol.h"
#include "util.h"

#include <dev/wscons/wsconsio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct fb_channel {
	uint32_t offset;
	uint32_t length;
};

static struct {
	int fd;
	void *memory;
	size_t memory_length;
	size_t memory_offset;
	uint32_t pitch;
	uint32_t bits_per_pixel;
	struct fb_channel red;
	struct fb_channel green;
	struct fb_channel blue;
	u_int type;
	u_int original_mode;
	bool mode_saved;
} wsdisplay;

static bool
add_overflows_size(size_t a, size_t b, size_t *result)
{
	if (a > SIZE_MAX - b) {
		return true;
	}
	*result = a + b;
	return false;
}

static bool
multiply_overflows_size(size_t a, size_t b, size_t *result)
{
	if (a && b > SIZE_MAX / a) {
		return true;
	}
	*result = a * b;
	return false;
}

static const char *
device_path(void)
{
	const char *path;

	if (WSDISPLAY_DEVICE[0] != '\0') {
		return WSDISPLAY_DEVICE;
	}
	path = getenv(SWC_LAUNCH_TTY_ENV);
	if (path && path[0] != '\0') {
		return path;
	}
#ifdef __OpenBSD__
	return "/dev/ttyC0";
#else
	return "/dev/ttyE0";
#endif
}

static bool
default_channels(u_int type, uint32_t depth)
{
	bool blue_first = false;

	switch (depth) {
	case 15:
		wsdisplay.red = (struct fb_channel){10, 5};
		wsdisplay.green = (struct fb_channel){5, 5};
		wsdisplay.blue = (struct fb_channel){0, 5};
		return true;
	case 16:
		wsdisplay.red = (struct fb_channel){11, 5};
		wsdisplay.green = (struct fb_channel){5, 6};
		wsdisplay.blue = (struct fb_channel){0, 5};
		return true;
	case 24:
	case 32:
#ifdef WSDISPLAY_TYPE_SUN24
		blue_first |= type == WSDISPLAY_TYPE_SUN24;
#endif
#ifdef WSDISPLAY_TYPE_SUNCG12
		blue_first |= type == WSDISPLAY_TYPE_SUNCG12;
#endif
#ifdef WSDISPLAY_TYPE_SUNCG14
		blue_first |= type == WSDISPLAY_TYPE_SUNCG14;
#endif
#ifdef WSDISPLAY_TYPE_SUNTCX
		blue_first |= type == WSDISPLAY_TYPE_SUNTCX;
#endif
#ifdef WSDISPLAY_TYPE_SUNFFB
		blue_first |= type == WSDISPLAY_TYPE_SUNFFB;
#endif
		wsdisplay.red = (struct fb_channel){blue_first ? 0 : 16, 8};
		wsdisplay.green = (struct fb_channel){8, 8};
		wsdisplay.blue = (struct fb_channel){blue_first ? 16 : 0, 8};
		return true;
	default:
		return false;
	}
}

static bool
valid_channels(void)
{
	struct fb_channel channels[] = {
	    wsdisplay.red, wsdisplay.green, wsdisplay.blue,
	};
	size_t i;

	if (wsdisplay.bits_per_pixel != 15 && wsdisplay.bits_per_pixel != 16 &&
	    wsdisplay.bits_per_pixel != 24 && wsdisplay.bits_per_pixel != 32) {
		return false;
	}
	for (i = 0; i < sizeof(channels) / sizeof(channels[0]); ++i) {
		if (!channels[i].length || channels[i].length > 16 ||
		    channels[i].offset >= wsdisplay.bits_per_pixel ||
		    channels[i].length >
		        wsdisplay.bits_per_pixel - channels[i].offset) {
			return false;
		}
	}
	return true;
}

static bool
query_framebuffer(struct swc_fb *fb)
{
	size_t last_row_offset, row_bytes, visible_size, visible_end;

	wsdisplay.type = WSDISPLAY_TYPE_UNKNOWN;
	(void)ioctl(wsdisplay.fd, WSDISPLAYIO_GTYPE, &wsdisplay.type);

#ifdef __NetBSD__
	{
		struct wsdisplayio_fbinfo info;

		memset(&info, 0, sizeof(info));
		if (ioctl(wsdisplay.fd, WSDISPLAYIO_GET_FBINFO, &info) == 0) {
			if (info.fbi_pixeltype != WSFB_RGB ||
			    info.fbi_fbsize > SIZE_MAX || info.fbi_fboffset > SIZE_MAX) {
				ERROR("Unsupported wsdisplay framebuffer format\n");
				return false;
			}
			fb->width = info.fbi_width;
			fb->height = info.fbi_height;
			wsdisplay.pitch = info.fbi_stride;
			wsdisplay.bits_per_pixel = info.fbi_bitsperpixel;
			wsdisplay.memory_length = info.fbi_fbsize;
			wsdisplay.memory_offset = info.fbi_fboffset;
			wsdisplay.red = (struct fb_channel){
			    info.fbi_subtype.fbi_rgbmasks.red_offset,
			    info.fbi_subtype.fbi_rgbmasks.red_size,
			};
			wsdisplay.green = (struct fb_channel){
			    info.fbi_subtype.fbi_rgbmasks.green_offset,
			    info.fbi_subtype.fbi_rgbmasks.green_size,
			};
			wsdisplay.blue = (struct fb_channel){
			    info.fbi_subtype.fbi_rgbmasks.blue_offset,
			    info.fbi_subtype.fbi_rgbmasks.blue_size,
			};
			goto validate;
		}
	}
#endif

	{
		struct wsdisplay_fbinfo info;
		u_int linebytes = 0;

		memset(&info, 0, sizeof(info));
		if (ioctl(wsdisplay.fd, WSDISPLAYIO_GINFO, &info) < 0) {
			ERROR("WSDISPLAYIO_GINFO failed: %s\n", strerror(errno));
			return false;
		}
		fb->width = info.width;
		fb->height = info.height;
		wsdisplay.bits_per_pixel = info.depth;
#ifdef __OpenBSD__
		wsdisplay.pitch = info.stride;
		wsdisplay.memory_offset = info.offset;
#else
		wsdisplay.memory_offset = 0;
#endif
		if (!wsdisplay.pitch) {
			if (ioctl(wsdisplay.fd, WSDISPLAYIO_LINEBYTES, &linebytes) < 0) {
				ERROR("WSDISPLAYIO_LINEBYTES failed: %s\n", strerror(errno));
				return false;
			}
			wsdisplay.pitch = linebytes;
		}
		if (!default_channels(wsdisplay.type, info.depth)) {
			ERROR("Unsupported wsdisplay depth: %u bits per pixel\n", info.depth);
			return false;
		}
		if (multiply_overflows_size(wsdisplay.pitch, fb->height, &visible_size) ||
		    add_overflows_size(wsdisplay.memory_offset, visible_size,
		                       &wsdisplay.memory_length)) {
			ERROR("wsdisplay framebuffer size overflows\n");
			return false;
		}
	}

#ifdef __NetBSD__
validate:
#endif
	if (!fb->width || !fb->height || fb->width > UINT16_MAX ||
	    fb->height > UINT16_MAX || !wsdisplay.memory_length ||
	    !valid_channels() ||
	    multiply_overflows_size(fb->width,
	                            (wsdisplay.bits_per_pixel + 7) / 8,
	                            &row_bytes) ||
	    row_bytes > wsdisplay.pitch ||
	    multiply_overflows_size(fb->height - 1, wsdisplay.pitch,
	                            &last_row_offset) ||
	    add_overflows_size(last_row_offset, row_bytes, &visible_size) ||
	    add_overflows_size(wsdisplay.memory_offset, visible_size,
	                       &visible_end) ||
	    visible_end > wsdisplay.memory_length) {
		ERROR("Unsupported wsdisplay geometry or channel layout\n");
		return false;
	}
	return true;
}

bool
framebuffer_initialize(struct swc_fb *fb)
{
	const char *path = device_path();
	u_int mode = WSDISPLAYIO_MODE_DUMBFB;

	memset(&wsdisplay, 0, sizeof(wsdisplay));
	wsdisplay.fd = -1;
	wsdisplay.fd = launch_open_device(path, O_RDWR | O_CLOEXEC);
	if (wsdisplay.fd < 0) {
		ERROR("Could not open wsdisplay device %s\n", path);
		return false;
	}
	if (ioctl(wsdisplay.fd, WSDISPLAYIO_GMODE, &wsdisplay.original_mode) == 0) {
		wsdisplay.mode_saved = true;
	}
	if (ioctl(wsdisplay.fd, WSDISPLAYIO_SMODE, &mode) < 0) {
		ERROR("Could not put wsdisplay device %s in dumb framebuffer mode\n",
		      path);
		goto error;
	}
	if (!query_framebuffer(fb)) {
		goto error;
	}
	wsdisplay.memory = mmap(NULL, wsdisplay.memory_length,
	                        PROT_READ | PROT_WRITE, MAP_SHARED,
	                        wsdisplay.fd, 0);
	if (wsdisplay.memory == MAP_FAILED) {
		wsdisplay.memory = NULL;
		ERROR("Could not map wsdisplay device %s (type %u): %s\n", path,
		      wsdisplay.type, strerror(errno));
		goto error;
	}
	return true;

error:
	if (wsdisplay.mode_saved) {
		(void)ioctl(wsdisplay.fd, WSDISPLAYIO_SMODE,
		            &wsdisplay.original_mode);
	}
	close(wsdisplay.fd);
	wsdisplay.fd = -1;
	return false;
}

void
framebuffer_finalize(struct swc_fb *fb)
{
	(void)fb;
	munmap(wsdisplay.memory, wsdisplay.memory_length);
	if (wsdisplay.mode_saved) {
		(void)ioctl(wsdisplay.fd, WSDISPLAYIO_SMODE,
		            &wsdisplay.original_mode);
	}
	close(wsdisplay.fd);
}

static uint32_t
channel(uint32_t value, struct fb_channel field)
{
	uint32_t maximum = (1u << field.length) - 1;

	return (((uint32_t)value * maximum + 127) / 255) << field.offset;
}

static uint32_t
native_pixel(uint32_t pixel)
{
	return channel((pixel >> 16) & 0xff, wsdisplay.red) |
	       channel((pixel >> 8) & 0xff, wsdisplay.green) |
	       channel(pixel & 0xff, wsdisplay.blue);
}

static void
store_pixel(uint8_t *destination, uint32_t pixel)
{
	uint32_t native = native_pixel(pixel);

	if (wsdisplay.bits_per_pixel == 24) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		destination[0] = (uint8_t)(native >> 16);
		destination[1] = (uint8_t)(native >> 8);
		destination[2] = (uint8_t)native;
#else
		destination[0] = (uint8_t)native;
		destination[1] = (uint8_t)(native >> 8);
		destination[2] = (uint8_t)(native >> 16);
#endif
	} else {
		memcpy(destination, &native, (wsdisplay.bits_per_pixel + 7) / 8);
	}
}

bool
framebuffer_present(struct swc_fb *fb)
{
	uint32_t bytes = (wsdisplay.bits_per_pixel + 7) / 8;
	uint32_t x, y;

	for (y = 0; y < fb->height; ++y) {
		uint32_t *source = (uint32_t *)((uint8_t *)fb->pixels +
		                                (size_t)y * fb->pitch);
		uint8_t *destination = (uint8_t *)wsdisplay.memory +
		                       wsdisplay.memory_offset +
		                       (size_t)y * wsdisplay.pitch;
		for (x = 0; x < fb->width; ++x) {
			store_pixel(destination + (size_t)x * bytes, source[x]);
		}
	}
	return true;
}

const char *
framebuffer_name(void)
{
	return "wsdisplay-0";
}
