#include "fb.h"
#include "launch.h"
#include "util.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct fb_channel {
	uint8_t offset;
	uint8_t length;
};

static struct {
	int fd;
	void *memory;
	size_t memory_length;
	size_t memory_offset;
	uint32_t pitch;
	uint8_t bits_per_pixel;
	struct fb_channel red;
	struct fb_channel green;
	struct fb_channel blue;
} fbdev;

static struct fb_channel
channel_from_fbdev(struct fb_bitfield field)
{
	return (struct fb_channel) {
	    .offset = field.offset,
	    .length = field.length,
	};
}

bool
framebuffer_initialize(struct swc_fb *fb)
{
	struct fb_fix_screeninfo fixed;
	struct fb_var_screeninfo variable;
	size_t visible_offset;

	memset(&fbdev, 0, sizeof(fbdev));
	fbdev.fd = launch_open_device(FBDEV_DEVICE, O_RDWR | O_CLOEXEC);
	if (fbdev.fd < 0) {
		ERROR("Could not open framebuffer device %s\n", FBDEV_DEVICE);
		return false;
	}
	if (ioctl(fbdev.fd, FBIOGET_FSCREENINFO, &fixed) < 0 ||
	    ioctl(fbdev.fd, FBIOGET_VSCREENINFO, &variable) < 0) {
		ERROR("Could not query framebuffer device %s\n", FBDEV_DEVICE);
		goto error;
	}
	if (variable.bits_per_pixel != 16 && variable.bits_per_pixel != 24 &&
	    variable.bits_per_pixel != 32) {
		ERROR("Unsupported fbdev depth: %u bits per pixel\n",
		      variable.bits_per_pixel);
		goto error;
	}
	if (fixed.type != FB_TYPE_PACKED_PIXELS ||
	    (fixed.visual != FB_VISUAL_TRUECOLOR &&
	     fixed.visual != FB_VISUAL_DIRECTCOLOR)) {
		ERROR("Unsupported fbdev framebuffer type or visual\n");
		goto error;
	}

	if (!variable.xres || !variable.yres || variable.xres > UINT16_MAX ||
	    variable.yres > UINT16_MAX ||
	    variable.red.msb_right || variable.green.msb_right ||
	    variable.blue.msb_right ||
	    variable.red.length > 16 || variable.green.length > 16 ||
	    variable.blue.length > 16 ||
	    variable.red.offset + variable.red.length > variable.bits_per_pixel ||
	    variable.green.offset + variable.green.length > variable.bits_per_pixel ||
	    variable.blue.offset + variable.blue.length > variable.bits_per_pixel) {
		ERROR("Unsupported fbdev geometry or channel layout\n");
		goto error;
	}
	fbdev.memory_length = fixed.smem_len;
	fbdev.memory = mmap(NULL, fbdev.memory_length, PROT_READ | PROT_WRITE,
	                  MAP_SHARED, fbdev.fd, 0);
	if (fbdev.memory == MAP_FAILED) {
		fbdev.memory = NULL;
		ERROR("Could not map framebuffer device %s\n", FBDEV_DEVICE);
		goto error;
	}

	fb->width = variable.xres;
	fb->height = variable.yres;
	fbdev.pitch = fixed.line_length;
	fbdev.bits_per_pixel = variable.bits_per_pixel;
	fbdev.red = channel_from_fbdev(variable.red);
	fbdev.green = channel_from_fbdev(variable.green);
	fbdev.blue = channel_from_fbdev(variable.blue);
	visible_offset = (size_t)variable.yoffset * fbdev.pitch +
	                 (size_t)variable.xoffset * variable.bits_per_pixel / 8;
	if (fixed.line_length < (size_t)variable.xres * variable.bits_per_pixel / 8 ||
	    visible_offset >= fbdev.memory_length ||
	    (size_t)(variable.yres - 1) * fixed.line_length >
	        fbdev.memory_length - visible_offset ||
	    (size_t)variable.xres * variable.bits_per_pixel / 8 >
	        fbdev.memory_length - visible_offset -
	            (size_t)(variable.yres - 1) * fixed.line_length) {
		ERROR("fbdev visible buffer lies outside its mapping\n");
		goto error;
	}
	fbdev.memory_offset = visible_offset;
	return true;

error:
	if (fbdev.memory) {
		munmap(fbdev.memory, fbdev.memory_length);
	}
	close(fbdev.fd);
	fbdev.fd = -1;
	return false;
}

void
framebuffer_finalize(struct swc_fb *fb)
{
	(void)fb;
	munmap(fbdev.memory, fbdev.memory_length);
	close(fbdev.fd);
}

static uint32_t
channel(uint8_t value, struct fb_channel field)
{
	uint32_t maximum;

	if (!field.length) {
		return 0;
	}
	maximum = field.length >= 32 ? UINT32_MAX : (1u << field.length) - 1;
	return (((uint32_t)value * maximum + 127) / 255) << field.offset;
}

static uint32_t
native_pixel(uint32_t pixel)
{
	return channel(pixel >> 16, fbdev.red) |
	       channel(pixel >> 8, fbdev.green) |
	       channel(pixel, fbdev.blue);
}

static void
store_pixel(uint8_t *destination, uint32_t pixel)
{
	uint32_t native = native_pixel(pixel);

	if (fbdev.bits_per_pixel == 24) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		destination[0] = native >> 16;
		destination[1] = native >> 8;
		destination[2] = native;
#else
		destination[0] = native;
		destination[1] = native >> 8;
		destination[2] = native >> 16;
#endif
	} else {
		memcpy(destination, &native, fbdev.bits_per_pixel / 8);
	}
}

bool
framebuffer_present(struct swc_fb *fb)
{
	uint32_t bytes = fbdev.bits_per_pixel / 8;
	uint32_t x, y;

	for (y = 0; y < fb->height; ++y) {
		uint32_t *source = (uint32_t *)((uint8_t *)fb->pixels + y * fb->pitch);
		uint8_t *destination = (uint8_t *)fbdev.memory + fbdev.memory_offset +
		                       (size_t)y * fbdev.pitch;
		for (x = 0; x < fb->width; ++x) {
			store_pixel(destination + (size_t)x * bytes, source[x]);
		}
	}
	return true;
}

const char *
framebuffer_name(void)
{
	return "fbdev-0";
}
