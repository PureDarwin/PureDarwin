#include "fb.h"
#include "util.h"

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>

enum {
	kIOFBGetPixelInformationSelector = 1,
	kIOFBGetCurrentDisplayModeSelector = 2,
	kIOFBSetDisplayModeSelector = 4,
	kIOFBGetDisplayModeCountSelector = 6,
	kIOFBGetDisplayModesSelector = 7,
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
	uint32_t bits_per_pixel;
	uint32_t *previous_pixels;
	bool previous_valid;
} darwin_fb;

// opens the first framebuffer that accepts, one left behind by a replaced driver refuses
static io_service_t
open_framebuffer(io_connect_t *connection)
{
	io_service_t service;
	const char *framebuffer_class = getenv("SWC_FRAMEBUFFER_CLASS");
	if (!framebuffer_class || !*framebuffer_class)
		framebuffer_class = "IOFramebuffer";

#ifdef _PD_IOKITLIB_H
	// a hardware driver's framebuffer wins over the firmware's boot one, which is only the fallback
	for (int pass = 0; pass < 2; pass++) {
		io_iterator_t iterator;
		char *matching = IOServiceMatching(framebuffer_class);

		if (!matching || IOServiceGetMatchingServices(darwin_fb.master_port, matching, &iterator) != KERN_SUCCESS)
			return IO_OBJECT_NULL;
		while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
			io_name_t name = "";
			IOObjectGetClass(service, name);
			bool boot = !strcmp(name, "IOGOPFramebuffer") || !strcmp(name, "IOBootFramebuffer");

			if (boot == (pass == 1) && IOServiceOpen(service, mach_task_self(), 0, connection) == KERN_SUCCESS)
				break;
			IOObjectRelease(service);
		}
		IOObjectRelease(iterator);
		if (service != IO_OBJECT_NULL)
			return service;
	}
	return IO_OBJECT_NULL;
#else
	io_iterator_t iterator;
	CFDictionaryRef matching = IOServiceMatching(framebuffer_class);

	if (!matching || IOServiceGetMatchingServices(darwin_fb.master_port, matching, &iterator) != KERN_SUCCESS)
		return IO_OBJECT_NULL;
	while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
		if (IOServiceOpen(service, mach_task_self(), 0, connection) == KERN_SUCCESS)
			break;
		IOObjectRelease(service);
	}
	IOObjectRelease(iterator);
	return service;
#endif
}

// the compositor now owns the screen, so the boot bar completes and the kernel stops drawing it
// turning it off repaints what was under it, which has to happen before the first frame
static void
boot_progress_done(void)
{
	int full = 1024, off = 0;

	(void)sysctlbyname("kern.progressmeter", NULL, NULL, &full, sizeof(full));
	(void)sysctlbyname("kern.progressmeterenable", NULL, NULL, &off, sizeof(off));
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
#ifdef _PD_IOKITLIB_H
	// a gpu driver brings its head up inside start(), so let matching settle before choosing, as WindowServer does
	mach_timespec_t settle = { 20, 0 };
	IOKitWaitQuiet(darwin_fb.master_port, &settle);
#endif
	darwin_fb.service = open_framebuffer(&darwin_fb.connection);
	if (darwin_fb.service != IO_OBJECT_NULL) {
		io_name_t name = "";
		IOObjectGetClass(darwin_fb.service, name);
		fprintf(stderr, "neuswc: drawing to %s\n", name);
	}
	if (darwin_fb.service == IO_OBJECT_NULL) {
		ERROR("Could not open an IOFramebuffer service\n");
		goto error;
	}
	// optional mode selection uses only modes advertised by the framebuffer.
	const char *requested = getenv("SWC_DISPLAY_MODE");
	const char *requested_depth = getenv("SWC_DISPLAY_DEPTH");
	uint64_t depth = 0;
	if (requested_depth && *requested_depth) {
		if (!strcmp(requested_depth, "16")) depth = 1;
		else if (strcmp(requested_depth, "32")) { ERROR("Unsupported requested display depth\n"); goto error; }
		if (!requested || !*requested) { ERROR("Display depth requires SWC_DISPLAY_MODE\n"); goto error; }
	}
	if (requested && *requested) {
		unsigned width, height; char tail;
		uint64_t count = 0; uint32_t output_count = 1;
		uint32_t modes[256];
		bool selected = false;
		if (sscanf(requested, "%ux%u%c", &width, &height, &tail) != 2 || !width || !height ||
		    IOConnectCallScalarMethod(darwin_fb.connection, kIOFBGetDisplayModeCountSelector,
		                              NULL, 0, &count, &output_count) != KERN_SUCCESS ||
		    output_count != 1 || !count || count > 256) {
			ERROR("Invalid or unavailable requested display mode\n");
			goto error;
		}
		size_t modes_size = count * sizeof(modes[0]);
		if (IOConnectCallStructMethod(darwin_fb.connection, kIOFBGetDisplayModesSelector,
		                             NULL, 0, modes, &modes_size) != KERN_SUCCESS ||
		    modes_size != count * sizeof(modes[0])) goto error;
		for (unsigned i = 0; i < count; i++) {
			query[0] = modes[i]; query[1] = depth; query[2] = kIOFBSystemAperture;
			info_size = sizeof(info); memset(&info, 0, sizeof(info));
			if (IOConnectCallMethod(darwin_fb.connection, kIOFBGetPixelInformationSelector,
			                        query, 3, NULL, 0, NULL, NULL, &info, &info_size) != KERN_SUCCESS ||
			    info.active_width != width || info.active_height != height) continue;
			uint64_t selected_mode[2] = {modes[i], depth};
			selected = IOConnectCallScalarMethod(darwin_fb.connection, kIOFBSetDisplayModeSelector,
			                                     selected_mode, 2, NULL, NULL) == KERN_SUCCESS;
			break;
		}
		if (!selected) { ERROR("Requested display mode is not supported\n"); goto error; }
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
	if (result != KERN_SUCCESS || (info.bits_per_pixel != 32 && info.bits_per_pixel != 16) ||
	    (info.bits_per_pixel == 16 && (info.component_masks[0] != 0xf800 ||
	     info.component_masks[1] != 0x07e0 || info.component_masks[2] != 0x001f)) ||
	    !info.active_width || !info.active_height ||
	    info.bytes_per_row < (size_t)info.active_width * (info.bits_per_pixel / 8)) {
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
	darwin_fb.bits_per_pixel = info.bits_per_pixel;
	if ((uint64_t)fb->width * fb->height > SIZE_MAX / sizeof(uint32_t)) goto error;
	darwin_fb.previous_pixels = malloc((size_t)fb->width * fb->height * sizeof(uint32_t));
	if (!darwin_fb.previous_pixels) goto error;
	fprintf(stderr, "IOFramebuffer: %ux%u %u bpp pitch %u\n", fb->width, fb->height, info.bits_per_pixel, info.bytes_per_row);
	boot_progress_done();
	return true;

error:
	framebuffer_finalize(fb);
	return false;
}

void
framebuffer_finalize(struct swc_fb *fb)
{
	(void)fb;
	free(darwin_fb.previous_pixels);
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

	bool changed = false;
	for (y = 0; y < fb->height; ++y) {
		const uint32_t *src = (const uint32_t *)((uint8_t *)fb->pixels + (size_t)y * fb->pitch);
		uint32_t *previous = darwin_fb.previous_pixels + (size_t)y * fb->width;
		uint32_t first = 0, end = fb->width;
		// compare cached RAM, never read the write-combined display aperture.
		if (darwin_fb.previous_valid) {
			if (!memcmp(previous, src, (size_t)fb->width * 4)) continue;
			while (first < end && previous[first] == src[first]) first++;
			while (end > first && previous[end - 1] == src[end - 1]) end--;
		}
		uint8_t *row = (uint8_t *)(uintptr_t)darwin_fb.memory + (size_t)y * darwin_fb.pitch;
		if (darwin_fb.bits_per_pixel == 16) {
			uint16_t *dst = (uint16_t *)row;
			for (uint32_t x = first; x < end; x++) {
				uint32_t c = src[x];
				dst[x] = ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
			}
		} else memcpy(row + (size_t)first * 4, src + first, (size_t)(end - first) * 4);
		memcpy(previous + first, src + first, (size_t)(end - first) * 4);
		changed = true;
	}
	darwin_fb.previous_valid = true;
	if (!changed) return true;
	// drain WC stores before publishing damage or returning to the event loop.
#if defined(__i386__) || defined(__x86_64__)
	__asm__ volatile("sfence" ::: "memory");
#else
	__sync_synchronize();
#endif

	/* Some Darwin framebuffer drivers expose an explicit damage operation.
	 * A directly mapped framebuffer does not require it, so unsupported is
	 * deliberately harmless. */
	(void)IOConnectCallStructMethod(darwin_fb.connection,
	                                kIOFBPresentSelector, &present,
	                                sizeof(present), NULL, NULL);
	return true;
}

// every mode the IOFramebuffer offers, sized from each mode's pixel information
int
framebuffer_modes(struct fb_mode *modes, int max)
{
	uint64_t count = 0, current[2] = {0, 0};
	uint32_t output_count = 1, current_count = 2;
	uint32_t ids[256];
	size_t ids_size;
	int found = 0;

	if (!darwin_fb.connection ||
	    IOConnectCallScalarMethod(darwin_fb.connection, kIOFBGetDisplayModeCountSelector,
	                              NULL, 0, &count, &output_count) != KERN_SUCCESS ||
	    !count || count > 256)
		return 0;
	ids_size = count * sizeof(ids[0]);
	if (IOConnectCallStructMethod(darwin_fb.connection, kIOFBGetDisplayModesSelector,
	                              NULL, 0, ids, &ids_size) != KERN_SUCCESS)
		return 0;
	IOConnectCallScalarMethod(darwin_fb.connection, kIOFBGetCurrentDisplayModeSelector,
	                          NULL, 0, current, &current_count);
	for (uint64_t i = 0; i < count && found < max; i++) {
		struct pixel_information info;
		size_t info_size = sizeof(info);
		uint64_t query[3] = {ids[i], 0, kIOFBSystemAperture};

		memset(&info, 0, sizeof(info));
		if (IOConnectCallMethod(darwin_fb.connection, kIOFBGetPixelInformationSelector,
		                        query, 3, NULL, 0, NULL, NULL, &info, &info_size) != KERN_SUCCESS ||
		    !info.active_width || !info.active_height)
			continue;
		modes[found].id = ids[i];
		modes[found].width = info.active_width;
		modes[found].height = info.active_height;
		modes[found].current = ids[i] == current[0];
		found++;
	}
	return found;
}

// the old mapping goes first, the driver may replace the memory behind it
bool
framebuffer_set_mode(struct swc_fb *fb, uint32_t mode_id)
{
	struct pixel_information info;
	size_t info_size = sizeof(info);
	uint64_t mode[2] = {mode_id, 0};
	uint64_t query[3] = {mode_id, 0, kIOFBSystemAperture};
	uint32_t *previous;

	if (!darwin_fb.connection)
		return false;
	if (darwin_fb.memory) {
		IOConnectUnmapMemory64(darwin_fb.connection, kIOFBVRAMMemory, mach_task_self(), darwin_fb.memory);
		darwin_fb.memory = 0;
	}
	if (IOConnectCallScalarMethod(darwin_fb.connection, kIOFBSetDisplayModeSelector,
	                              mode, 2, NULL, NULL) != KERN_SUCCESS)
		ERROR("IOFramebuffer refused display mode %u\n", mode_id);
	memset(&info, 0, sizeof(info));
	if (IOConnectCallMethod(darwin_fb.connection, kIOFBGetPixelInformationSelector,
	                        query, 3, NULL, 0, NULL, NULL, &info, &info_size) != KERN_SUCCESS ||
	    IOConnectMapMemory64(darwin_fb.connection, kIOFBVRAMMemory, mach_task_self(),
	                         &darwin_fb.memory, &darwin_fb.memory_length, kIOMapAnywhere) != KERN_SUCCESS ||
	    (size_t)info.active_height * info.bytes_per_row > darwin_fb.memory_length) {
		ERROR("Could not remap the IOFramebuffer after a mode change\n");
		return false;
	}
	previous = malloc((size_t)info.active_width * info.active_height * sizeof(uint32_t));
	if (!previous)
		return false;
	free(darwin_fb.previous_pixels);
	darwin_fb.previous_pixels = previous;
	darwin_fb.previous_valid = false;
	darwin_fb.pitch = info.bytes_per_row;
	darwin_fb.bits_per_pixel = info.bits_per_pixel;
	fb->width = info.active_width;
	fb->height = info.active_height;
	fprintf(stderr, "IOFramebuffer: now %ux%u %u bpp pitch %u\n", fb->width, fb->height, info.bits_per_pixel, info.bytes_per_row);
	return true;
}

const char *
framebuffer_name(void)
{
	return "ioframebuffer-0";
}
