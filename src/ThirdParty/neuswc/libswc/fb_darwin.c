#include "fb.h"
#include "util.h"

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

// the display path, on stderr and the serial console, so a machine with no picture still says it
static void
fb_note(const char *format, ...)
{
	char line[256];
	va_list ap;

	va_start(ap, format);
	vsnprintf(line, sizeof(line), format, ap);
	va_end(ap);
	fputs(line, stderr);
	int fd = open("/dev/console", O_WRONLY | O_NOCTTY);
	if (fd >= 0) {
		write(fd, line, strlen(line));
		close(fd);
	}
}

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

// IOVirtIOGPU's 3D user client (IOVirtIOGPU3DShared.h), for scanning out a GPU resource directly
#define VIRTIO_GPU_3D_CONNECT_TYPE 0x76697267 /* 'virg' */
enum {
	kPDGPU_Present = 11,
	kPDGPU_SetScanoutResource = 14,
};

static struct {
	io_connect_t connection;
	bool opened, available;
	uint32_t resource;
} gpu_scanout;

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
	// a presenter (PDDRMFramebuffer): two scanout buffers flipped at vblank
	io_connect_t presenter;
	mach_vm_address_t buffers[2];
	bool last_damage_valid;
	uint32_t last_damage_y1, last_damage_y2;
	mach_vm_size_t buffer_lengths[2];
	uint32_t *previous_buffer[2];
	bool previous_buffer_valid[2];
	int front;
} darwin_fb;

enum {
	kPresenterType = 'pdpf',
	kPresenterGetInfo = 0,
	kPresenterFlip = 1,
	kPresenterExport = 2,
};

static void close_presenter(void);

// a framebuffer that can page flip hands out a second buffer, frames then go
// to the one not on screen and flip at vblank instead of tearing
static void
open_presenter(struct swc_fb *fb)
{
	uint64_t info[4];
	uint32_t count = 4;
	int i;

	if (darwin_fb.bits_per_pixel != 32 ||
	    IOServiceOpen(darwin_fb.service, mach_task_self(), kPresenterType,
	                  &darwin_fb.presenter) != KERN_SUCCESS) {
		darwin_fb.presenter = 0;
		return;
	}
	if (IOConnectCallScalarMethod(darwin_fb.presenter, kPresenterGetInfo, NULL, 0,
	                              info, &count) != KERN_SUCCESS || count < 4 ||
	    info[0] != fb->width || info[1] != fb->height || info[3] < 2)
		goto fail;
	for (i = 0; i < 2; i++) {
		if (IOConnectMapMemory64(darwin_fb.presenter, i, mach_task_self(),
		                         &darwin_fb.buffers[i], &darwin_fb.buffer_lengths[i],
		                         kIOMapAnywhere) != KERN_SUCCESS ||
		    darwin_fb.buffer_lengths[i] < (size_t)info[2] * fb->height)
			goto fail;
		darwin_fb.previous_buffer[i] = malloc((size_t)fb->width * fb->height * sizeof(uint32_t));
		if (!darwin_fb.previous_buffer[i]) goto fail;
		darwin_fb.previous_buffer_valid[i] = false;
	}
	darwin_fb.pitch = (uint32_t)info[2];
	darwin_fb.front = 0;
	darwin_fb.last_damage_valid = false;
	fb_note("neuswc: page flipping through the framebuffer's presenter\n");
	return;
fail:
	close_presenter();
}

static void
close_presenter(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (darwin_fb.buffers[i])
			IOConnectUnmapMemory64(darwin_fb.presenter, i, mach_task_self(), darwin_fb.buffers[i]);
		free(darwin_fb.previous_buffer[i]);
		darwin_fb.buffers[i] = 0;
		darwin_fb.previous_buffer[i] = NULL;
	}
	if (darwin_fb.presenter)
		IOServiceClose(darwin_fb.presenter);
	darwin_fb.presenter = 0;
}

// draws the frame into the buffer off screen, then flips it in at vblank. Each
// buffer remembers its own last contents, so only rows that differ are copied.
static bool
present_flipped(struct swc_fb *fb)
{
	int back = 1 - darwin_fb.front;
	uint32_t *previous = darwin_fb.previous_buffer[back];
	uint8_t *base = (uint8_t *)(uintptr_t)darwin_fb.buffers[back];
	bool changed = false;
	uint32_t y;
	// the back buffer is two frames old, so this frame's damage and the last one's cover it
	uint32_t first_row = 0, end_row = fb->height;
	bool damaged = fb->damage_y1 < fb->damage_y2;
	if (darwin_fb.previous_buffer_valid[back] && damaged && darwin_fb.last_damage_valid) {
		first_row = fb->damage_y1 < darwin_fb.last_damage_y1 ? fb->damage_y1 : darwin_fb.last_damage_y1;
		end_row = fb->damage_y2 > darwin_fb.last_damage_y2 ? fb->damage_y2 : darwin_fb.last_damage_y2;
		if (end_row > fb->height) end_row = fb->height;
	}
	darwin_fb.last_damage_valid = damaged;
	darwin_fb.last_damage_y1 = fb->damage_y1;
	darwin_fb.last_damage_y2 = fb->damage_y2;

	for (y = first_row; y < end_row; ++y) {
		const uint32_t *src = (const uint32_t *)((uint8_t *)fb->pixels + (size_t)y * fb->pitch);
		uint32_t *row_previous = previous + (size_t)y * fb->width;
		uint32_t first = 0, end = fb->width;
		if (darwin_fb.previous_buffer_valid[back]) {
			if (!memcmp(row_previous, src, (size_t)fb->width * 4)) continue;
			while (first < end && row_previous[first] == src[first]) first++;
			while (end > first && row_previous[end - 1] == src[end - 1]) end--;
		}
		memcpy(base + (size_t)y * darwin_fb.pitch + (size_t)first * 4, src + first,
		       (size_t)(end - first) * 4);
		memcpy(row_previous + first, src + first, (size_t)(end - first) * 4);
		changed = true;
	}
	darwin_fb.previous_buffer_valid[back] = true;
	if (!changed) return true;
#if defined(__i386__) || defined(__x86_64__)
	__asm__ volatile("sfence" ::: "memory");
#else
	__sync_synchronize();
#endif
	uint64_t in = (uint64_t)back, out[2];
	uint32_t count = 2;
	if (IOConnectCallScalarMethod(darwin_fb.presenter, kPresenterFlip, &in, 1, out, &count) != KERN_SUCCESS)
		return false;
	darwin_fb.front = back;
	return true;
}

// opens the first framebuffer that accepts, one left behind by a replaced driver refuses
static io_service_t
open_framebuffer(io_connect_t *connection)
{
	io_service_t service;
	const char *framebuffer_class = getenv("SWC_FRAMEBUFFER_CLASS");
	if (!framebuffer_class || !*framebuffer_class)
		framebuffer_class = "IOFramebuffer";

#ifdef _PD_IOKITLIB_H
	// a hardware driver's framebuffer wins over the firmware's boot one, which is only the fallback.
	// SWC_WAIT_HW_FB=N waits up to N seconds for one, a gpu driver in the aux KC comes up late
	const char *wait = getenv("SWC_WAIT_HW_FB");
	int wait_left = wait ? atoi(wait) : 0;
	for (int pass = 0; pass < 2; pass++) {
		io_iterator_t iterator;
		char *matching = IOServiceMatching(framebuffer_class);

		if (!matching || IOServiceGetMatchingServices(darwin_fb.master_port, matching, &iterator) != KERN_SUCCESS)
			return IO_OBJECT_NULL;
		while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
			io_name_t name = "";
			IOObjectGetClass(service, name);
			bool boot = !strcmp(name, "IOGOPFramebuffer") || !strcmp(name, "IOBootFramebuffer");

			if (boot == (pass == 1)) {
				kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, connection);
				fb_note("neuswc: %s %s (0x%x)\n", name, kr == KERN_SUCCESS ? "opened" : "refused", kr);
				if (kr == KERN_SUCCESS)
					break;
			}
			IOObjectRelease(service);
		}
		IOObjectRelease(iterator);
		if (service != IO_OBJECT_NULL)
			return service;
		if (pass == 0 && wait_left-- > 0) {
			if (wait_left % 5 == 4)
				fb_note("neuswc: waiting for a gpu driver's framebuffer\n");
			sleep(1);
			pass = -1;
		}
	}
	return IO_OBJECT_NULL;
#else
	// the same choice with Apple's IOKitLib: a gpu driver's framebuffer before the firmware's
	const char *wait = getenv("SWC_WAIT_HW_FB");
	int wait_left = wait ? atoi(wait) : 0;
	for (int pass = 0; pass < 2; pass++) {
		io_iterator_t iterator;
		CFMutableDictionaryRef matching = IOServiceMatching(framebuffer_class);

		if (!matching || IOServiceGetMatchingServices(darwin_fb.master_port, matching, &iterator) != KERN_SUCCESS)
			return IO_OBJECT_NULL;
		while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
			io_name_t name = "";
			IOObjectGetClass(service, name);
			bool boot = !strcmp(name, "IOGOPFramebuffer") || !strcmp(name, "IOBootFramebuffer");

			if (boot == (pass == 1)) {
				kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, connection);
				fb_note("neuswc: %s %s (0x%x)\n", name, kr == KERN_SUCCESS ? "opened" : "refused", kr);
				if (kr == KERN_SUCCESS)
					break;
			}
			IOObjectRelease(service);
		}
		IOObjectRelease(iterator);
		if (service != IO_OBJECT_NULL)
			return service;
		if (pass == 0 && wait_left-- > 0) {
			if (wait_left % 5 == 4)
				fb_note("neuswc: waiting for a gpu driver's framebuffer\n");
			sleep(1);
			pass = -1;
		}
	}
	return IO_OBJECT_NULL;
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
	// a gpu driver brings its head up inside start(), so let matching settle before choosing, as WindowServer does
	mach_timespec_t settle = { 20, 0 };
	IOKitWaitQuiet(darwin_fb.master_port, &settle);
	darwin_fb.service = open_framebuffer(&darwin_fb.connection);
	if (darwin_fb.service != IO_OBJECT_NULL) {
		io_name_t name = "";
		IOObjectGetClass(darwin_fb.service, name);
		fb_note("neuswc: drawing to %s\n", name);
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
	open_presenter(fb);
	if (!darwin_fb.presenter)
		fb_note("neuswc: no presenter, copying into the mapped framebuffer\n");
	fprintf(stderr, "IOFramebuffer: %ux%u %u bpp pitch %u%s\n", fb->width, fb->height,
	        info.bits_per_pixel, info.bytes_per_row,
	        darwin_fb.presenter ? ", page flipped at vblank" : "");
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
	close_presenter();
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

	if (darwin_fb.presenter)
		return present_flipped(fb);

	bool changed = false;
	// fb_present names the rows it touched, the rest still match the previous frame
	uint32_t first_row = 0, end_row = fb->height;
	if (darwin_fb.previous_valid && fb->damage_y1 < fb->damage_y2) {
		first_row = fb->damage_y1;
		end_row = fb->damage_y2 < fb->height ? fb->damage_y2 : fb->height;
	}
	for (y = first_row; y < end_row; ++y) {
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
	// the flip pair belongs to the old size, the presenter hands out new ones
	close_presenter();
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
	open_presenter(fb);
	fprintf(stderr, "IOFramebuffer: now %ux%u %u bpp pitch %u%s\n", fb->width, fb->height,
	        info.bits_per_pixel, info.bytes_per_row,
	        darwin_fb.presenter ? ", page flipped at vblank" : "");
	return true;
}

const char *
framebuffer_name(void)
{
	return "ioframebuffer-0";
}

// whether this framebuffer is a virtio-gpu with a 3D client, whose resources it can scan out
bool
framebuffer_gpu_scanout_available(void)
{
	io_name_t name = "";

	if (gpu_scanout.opened)
		return gpu_scanout.available;
	gpu_scanout.opened = true;
	if (darwin_fb.service == IO_OBJECT_NULL)
		return false;
	// a presenter's flip pair is scanned out by the GPU that renders into it
	if (darwin_fb.presenter) {
		gpu_scanout.available = true;
		return true;
	}
	IOObjectGetClass(darwin_fb.service, name);
	if (strcmp(name, "IOVirtIOGPU") != 0)
		return false;
	gpu_scanout.available = IOServiceOpen(darwin_fb.service, mach_task_self(), VIRTIO_GPU_3D_CONNECT_TYPE,
	                                      &gpu_scanout.connection) == KERN_SUCCESS;
	return gpu_scanout.available;
}

// points the display at a GPU resource, 0 gives it back to the framebuffer
bool
framebuffer_set_scanout_resource(uint32_t resource, uint32_t width, uint32_t height)
{
	uint64_t in[3] = { resource, width, height };

	if (!framebuffer_gpu_scanout_available())
		return false;
	if (IOConnectCallScalarMethod(gpu_scanout.connection, kPDGPU_SetScanoutResource, in, 3, NULL, NULL) != KERN_SUCCESS)
		return false;
	gpu_scanout.resource = resource;
	return true;
}

// the presenter's buffers as dma-buf fds, for a renderer to draw into directly
int
framebuffer_scanout_dmabufs(int *fds, uint32_t *width, uint32_t *height, uint32_t *pitch)
{
	uint64_t info[4];
	uint32_t count = 4;
	int i;

	if (!darwin_fb.presenter ||
	    IOConnectCallScalarMethod(darwin_fb.presenter, kPresenterGetInfo, NULL, 0, info, &count) != KERN_SUCCESS)
		return 0;
	for (i = 0; i < 2; i++) {
		uint64_t in = (uint64_t)i, out = 0;
		uint32_t n = 1;

		if (IOConnectCallScalarMethod(darwin_fb.presenter, kPresenterExport, &in, 1, &out, &n) != KERN_SUCCESS) {
			while (i-- > 0)
				close(fds[i]);
			return 0;
		}
		fds[i] = (int)out;
	}
	*width = (uint32_t)info[0];
	*height = (uint32_t)info[1];
	*pitch = (uint32_t)info[2];
	return 2;
}

int
framebuffer_back_index(void)
{
	return darwin_fb.presenter ? 1 - darwin_fb.front : -1;
}

// puts a buffer the GPU finished on screen at the next vblank
bool
framebuffer_flip(int index)
{
	uint64_t in = (uint64_t)index, out[2];
	uint32_t count = 2;

	if (!darwin_fb.presenter ||
	    IOConnectCallScalarMethod(darwin_fb.presenter, kPresenterFlip, &in, 1, out, &count) != KERN_SUCCESS)
		return false;
	darwin_fb.front = index;
	// the CPU path's copies of the buffers no longer match them
	darwin_fb.previous_buffer_valid[0] = darwin_fb.previous_buffer_valid[1] = false;
	return true;
}

// tells the host which part of the scanned out resource changed
bool
framebuffer_present_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	struct { uint32_t x, y, width, height; } present = { x, y, width, height };

	if (!gpu_scanout.available || !gpu_scanout.resource)
		return false;
	return IOConnectCallStructMethod(gpu_scanout.connection, kPDGPU_Present, &present, sizeof(present),
	                                 NULL, NULL) == KERN_SUCCESS;
}
