// pdfb: the framebuffer through IOFramebuffer's server connection, the same path as the puredarwingop ddx:
// GetCurrentDisplayMode (2), GetPixelInformation (1), then clientMemoryForType(kIOFBVRAMMemory) mapped write-combined

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfb.h"

// PDFB_CLEAN=1 pushes each written row out with dc cvac, on the a733 the mapping is not cached and it is not needed
static void
clean(uint8_t *p, size_t len)
{
#ifdef __aarch64__
	uintptr_t a = (uintptr_t)p & ~(uintptr_t)63, end = (uintptr_t)p + len;

	for (; a < end; a += 64) {
		__asm__ volatile ("dc cvac, %0" :: "r" (a) : "memory");
	}
#else
	(void)p;
	(void)len;
#endif
}

#ifdef __APPLE__
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>

#define FB_SEL_PIXEL_INFO	1
#define FB_SEL_CURRENT_MODE	2
#define FB_VRAM_MEMORY		110
#define FB_SERVER_CONNECT	0
#define FB_SYSTEM_APERTURE	0

struct pixel_info {
	uint32_t bytesPerRow;
	uint32_t bytesPerPlane;
	uint32_t bitsPerPixel;
	uint32_t pixelType;
	uint32_t componentCount;
	uint32_t bitsPerComponent;
	uint32_t componentMasks[16];
	char pixelFormat[64];
	uint32_t flags;
	uint32_t activeWidth;
	uint32_t activeHeight;
	uint32_t reserved[2];
};

int
pdfb_open(struct pdfb *fb)
{
	static const char *classes[] = { "IOGOPFramebuffer", "IOFramebuffer" };
	struct pixel_info pi;
	size_t pisz = sizeof(pi);
	uint64_t mode[2] = { 0, 0 }, in[3];
	uint32_t nmode = 2;
	mach_vm_address_t va = 0;
	mach_vm_size_t sz = 0;
	kern_return_t kr;

	memset(fb, 0, sizeof(*fb));
	for (size_t i = 0; i < sizeof(classes) / sizeof(classes[0]) && fb->service == 0; i++) {
		fb->service = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching(classes[i]));
	}
	if (fb->service == 0) {
		printf("pdfb: no framebuffer service\n");
		return -1;
	}

	kr = IOServiceOpen(fb->service, mach_task_self(), FB_SERVER_CONNECT, &fb->connect);
	if (kr != KERN_SUCCESS) {
		printf("pdfb: IOServiceOpen 0x%x\n", kr);
		return -1;
	}

	kr = IOConnectCallScalarMethod(fb->connect, FB_SEL_CURRENT_MODE, NULL, 0, mode, &nmode);
	if (kr != KERN_SUCCESS) {
		printf("pdfb: GetCurrentDisplayMode 0x%x\n", kr);
		return -1;
	}

	in[0] = (uint32_t)mode[0];
	in[1] = (uint32_t)mode[1];
	in[2] = FB_SYSTEM_APERTURE;
	memset(&pi, 0, sizeof(pi));
	kr = IOConnectCallMethod(fb->connect, FB_SEL_PIXEL_INFO, in, 3, NULL, 0, NULL, NULL, &pi, &pisz);
	if (kr != KERN_SUCCESS) {
		printf("pdfb: GetPixelInformation 0x%x\n", kr);
		return -1;
	}

	// write-combined: the scanout is not snooped, and a device mapping would fault on unaligned copies
	kr = IOConnectMapMemory64(fb->connect, FB_VRAM_MEMORY, mach_task_self(), &va, &sz,
	    kIOMapAnywhere | kIOMapWriteCombineCache);
	if (kr != KERN_SUCCESS) {
		printf("pdfb: map vram 0x%x\n", kr);
		return -1;
	}

	fb->base = (uint8_t *)va;
	fb->size = sz;
	fb->width = pi.activeWidth;
	fb->height = pi.activeHeight;
	fb->stride = pi.bytesPerRow;
	fb->bpp = pi.bitsPerPixel;
	fb->row = calloc(fb->width, 4);
	fb->clean = getenv("PDFB_CLEAN") != NULL && atoi(getenv("PDFB_CLEAN")) != 0;
	printf("pdfb: %ux%u stride %u bpp %u, %llu bytes mapped at %p\n", fb->width, fb->height, fb->stride,
	    fb->bpp, (unsigned long long)sz, fb->base);
	if (fb->bpp != 32 || (uint64_t)fb->stride * fb->height > sz) {
		printf("pdfb: unsupported layout\n");
		return -1;
	}

	for (uint32_t y = 0; y < fb->height; y++) {
		memcpy(fb->base + (size_t)y * fb->stride, fb->row, (size_t)fb->width * 4);
		if (fb->clean)
			clean(fb->base + (size_t)y * fb->stride, (size_t)fb->width * 4);
	}
	printf("pdfb: row clean %s\n", fb->clean ? "on" : "off");
	return 0;
}

void
pdfb_close(struct pdfb *fb)
{
	if (fb->base)
		IOConnectUnmapMemory64(fb->connect, FB_VRAM_MEMORY, mach_task_self(), (mach_vm_address_t)fb->base);
	if (fb->connect)
		IOServiceClose(fb->connect);
	if (fb->service)
		IOObjectRelease(fb->service);
	free(fb->row);
	memset(fb, 0, sizeof(*fb));
}

#else

// host build: a 1920x1080 buffer in memory, written to $PDFB_PPM on close
int
pdfb_open(struct pdfb *fb)
{
	memset(fb, 0, sizeof(*fb));
	fb->width = 1920;
	fb->height = 1080;
	fb->stride = 1920 * 4;
	fb->bpp = 32;
	fb->size = (uint64_t)fb->stride * fb->height;
	fb->base = calloc(1, fb->size);
	fb->row = calloc(fb->width, 4);
	return 0;
}

void
pdfb_close(struct pdfb *fb)
{
	const char *path = getenv("PDFB_PPM");
	FILE *f = path ? fopen(path, "wb") : NULL;

	if (f != NULL) {
		fprintf(f, "P6 %u %u 255\n", fb->width, fb->height);
		for (uint32_t i = 0; i < fb->width * fb->height; i++) {
			const uint8_t *p = fb->base + i * 4;
			uint8_t rgb[3] = { p[2], p[1], p[0] };

			fwrite(rgb, 1, 3, f);
		}
		fclose(f);
	}
	free(fb->base);
	free(fb->row);
	memset(fb, 0, sizeof(*fb));
}

#endif

void
pdfb_blit(struct pdfb *fb, const uint8_t *src, uint32_t w, uint32_t h, size_t pitch, uint32_t scale, int swap)
{
	uint32_t ow = w * scale, oh = h * scale;
	uint32_t ox, oy;

	if (ow > fb->width || oh > fb->height)
		return;
	ox = (fb->width - ow) / 2 & ~1u;
	oy = (fb->height - oh) / 2;

	for (uint32_t y = 0; y < h; y++) {
		const uint32_t *s = (const uint32_t *)(src + y * pitch);
		uint32_t *d = fb->row;

		for (uint32_t x = 0; x < w; x++) {
			uint32_t v = s[x];

			if (swap)
				v = (v & 0xff00ff00u) | (v & 0xffu) << 16 | (v >> 16 & 0xffu);
			for (uint32_t k = 0; k < scale; k++) {
				*d++ = v;
			}
		}
		for (uint32_t k = 0; k < scale; k++) {
			uint8_t *d = fb->base + (size_t)(oy + y * scale + k) * fb->stride + (size_t)ox * 4;

			memcpy(d, fb->row, (size_t)ow * 4);
			if (fb->clean)
				clean(d, (size_t)ow * 4);
		}
	}
#ifdef __aarch64__
	if (fb->clean)
		__asm__ volatile ("dsb sy" ::: "memory");
#endif
}
