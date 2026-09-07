#include "fb.h"
#include "util.h"

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdint.h>
#include <string.h>

enum {
	kIOFBGetPixelInformationSelector = 1,
	kIOFBGetCurrentDisplayModeSelector = 2,
	kIOFBVRAMMemory = 110,
	kIOFBSystemAperture = 0,
	kIOFBPresentSelector = 11,
};

struct pixel_information {
	uint32_t bytes_per_row;
	uint32_t bytes_per_plane;
	uint32_t bits_per_pixel;
	uint32_t pixel_type;
	uint32_t component_count;
	uint32_t bits_per_component;
	uint32_t component_masks[16];
	char pixel_format[64];
	uint32_t flags;
	uint32_t active_width;
	uint32_t active_height;
	uint32_t reserved[2];
};

static struct {
	mach_port_t master_port;
	io_service_t service;
	io_connect_t connection;
	mach_vm_address_t memory;
	mach_vm_size_t memory_length;
	uint32_t pitch;
} darwin_fb;

static io_service_t
find_framebuffer(void)
{
	io_service_t service;

#ifdef _PD_IOKITLIB_H
	char *matching = IOServiceMatching("IOFramebuffer");
	if (!matching || IOServiceGetMatchingService(darwin_fb.master_port,
	                                             matching, &service) !=
	                     KERN_SUCCESS) {
		return IO_OBJECT_NULL;
	}
#else
	CFDictionaryRef matching = IOServiceMatching("IOFramebuffer");
	if (!matching) {
		return IO_OBJECT_NULL;
	}
	service = IOServiceGetMatchingService(darwin_fb.master_port, matching);
#endif
	return service;
}

bool
framebuffer_initialize(struct swc_fb *fb)
{
	uint64_t mode[2] = {0, 0};
	uint32_t mode_count = 2;
	uint64_t query[3];
	struct pixel_information info;
	size_t info_size = sizeof(info);
	kern_return_t result;

	memset(&darwin_fb, 0, sizeof(darwin_fb));
	result = IOMasterPort(MACH_PORT_NULL, &darwin_fb.master_port);
	if (result != KERN_SUCCESS) {
		ERROR("Could not obtain the IOKit master port\n");
		return false;
	}
	darwin_fb.service = find_framebuffer();
	if (darwin_fb.service == IO_OBJECT_NULL) {
		ERROR("Could not find an IOFramebuffer service\n");
		goto error;
	}
	result = IOServiceOpen(darwin_fb.service, mach_task_self(), 0,
	                       &darwin_fb.connection);
	if (result != KERN_SUCCESS) {
		ERROR("Could not open the IOFramebuffer service\n");
		goto error;
	}
	result = IOConnectCallScalarMethod(darwin_fb.connection,
	                                   kIOFBGetCurrentDisplayModeSelector,
	                                   NULL, 0, mode, &mode_count);
	if (result != KERN_SUCCESS || mode_count < 2) {
		ERROR("Could not query the current display mode\n");
		goto error;
	}
	query[0] = mode[0];
	query[1] = mode[1];
	query[2] = kIOFBSystemAperture;
	memset(&info, 0, sizeof(info));
	result = IOConnectCallMethod(darwin_fb.connection,
	                             kIOFBGetPixelInformationSelector, query, 3,
	                             NULL, 0, NULL, NULL, &info, &info_size);
	if (result != KERN_SUCCESS || info.bits_per_pixel != 32 ||
	    !info.active_width || !info.active_height ||
	    info.bytes_per_row < (size_t)info.active_width * 4) {
		ERROR("Unsupported IOFramebuffer pixel layout\n");
		goto error;
	}
	result = IOConnectMapMemory64(darwin_fb.connection, kIOFBVRAMMemory,
	                              mach_task_self(), &darwin_fb.memory,
	                              &darwin_fb.memory_length, kIOMapAnywhere);
	if (result != KERN_SUCCESS ||
	    (size_t)info.active_height * info.bytes_per_row >
	        darwin_fb.memory_length) {
		ERROR("Could not map IOFramebuffer memory\n");
		goto error;
	}

	fb->width = info.active_width;
	fb->height = info.active_height;
	darwin_fb.pitch = info.bytes_per_row;
	return true;

error:
	framebuffer_finalize(fb);
	return false;
}

void
framebuffer_finalize(struct swc_fb *fb)
{
	(void)fb;
	if (darwin_fb.memory && darwin_fb.connection) {
		IOConnectUnmapMemory64(darwin_fb.connection, kIOFBVRAMMemory,
		                       mach_task_self(), darwin_fb.memory);
	}
	if (darwin_fb.connection) {
		IOServiceClose(darwin_fb.connection);
	}
	if (darwin_fb.service) {
		IOObjectRelease(darwin_fb.service);
	}
	if (darwin_fb.master_port) {
		mach_port_deallocate(mach_task_self(), darwin_fb.master_port);
	}
	memset(&darwin_fb, 0, sizeof(darwin_fb));
}

bool
framebuffer_present(struct swc_fb *fb)
{
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t width;
		uint32_t height;
	} present = {0, 0, fb->width, fb->height};
	uint32_t y;

	for (y = 0; y < fb->height; ++y) {
		memcpy((uint8_t *)(uintptr_t)darwin_fb.memory +
		           (size_t)y * darwin_fb.pitch,
		       (uint8_t *)fb->pixels + (size_t)y * fb->pitch,
		       (size_t)fb->width * 4);
	}

	/* Some Darwin framebuffer drivers expose an explicit damage operation.
	 * A directly mapped framebuffer does not require it, so unsupported is
	 * deliberately harmless. */
	(void)IOConnectCallStructMethod(darwin_fb.connection,
	                                kIOFBPresentSelector, &present,
	                                sizeof(present), NULL, NULL);
	return true;
}

const char *
framebuffer_name(void)
{
	return "ioframebuffer-0";
}
