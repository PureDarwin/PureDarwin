// pdscanout: a cpu-cached, physically contiguous PDPowerVR dumb buffer flipped onto PDSun60iDisplay

#include "pdscanout.h"

// only the a733 has the engine, elsewhere the driver keeps its shadow copy
#if defined(__aarch64__) || defined(__arm64__)

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>

#include "pd_pvr_uapi.h"

#define LINE	64

int
pdscanout_open(struct pdscanout *s, uint32_t width, uint32_t height, uint32_t pitch, char *why, int whylen)
{
	struct pd_pvr_display_info info;
	size_t len = sizeof(info);
	io_service_t svc;
	io_connect_t conn = IO_OBJECT_NULL;
	mach_vm_address_t va = 0;
	mach_vm_size_t sz = 0;
	uint64_t in[3], out[2];
	uint32_t nout = 2;
	kern_return_t kr;

	memset(s, 0, sizeof(*s));
	kr = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching(PD_PVR_SERVICE_CLASS), &svc);
	if (kr != KERN_SUCCESS || svc == IO_OBJECT_NULL) {
		snprintf(why, whylen, "no %s service", PD_PVR_SERVICE_CLASS);
		return -1;
	}
	kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
	IOObjectRelease(svc);
	if (kr != KERN_SUCCESS) {
		snprintf(why, whylen, "cannot open the gpu: 0x%x", kr);
		return -1;
	}
	kr = IOConnectCallStructMethod(conn, PD_PVR_SEL_DISPLAY_INFO, NULL, 0, &info, &len);
	if (kr != KERN_SUCCESS || info.width != width || info.height != height) {
		snprintf(why, whylen, "no scanout for %ux%u (kr 0x%x, display %ux%u)", width, height, kr,
		    info.width, info.height);
		goto fail;
	}
	in[0] = (uint64_t)pitch * height;
	in[1] = PD_PVR_DUMB_CPU_CACHED;
	kr = IOConnectCallScalarMethod(conn, PD_PVR_SEL_DUMB_CREATE, in, 2, out, &nout);
	if (kr != KERN_SUCCESS || !out[1]) {
		snprintf(why, whylen, "no contiguous %llu-byte buffer (kr 0x%x)", (unsigned long long)in[0], kr);
		if (kr == KERN_SUCCESS)
			IOConnectCallScalarMethod(conn, PD_PVR_SEL_GEM_CLOSE, out, 1, NULL, NULL);
		goto fail;
	}
	s->handle = (uint32_t)out[0];
	kr = IOConnectMapMemory64(conn, s->handle, mach_task_self(), &va, &sz, kIOMapAnywhere);
	if (kr != KERN_SUCCESS || sz < in[0]) {
		snprintf(why, whylen, "cannot map the buffer: 0x%x", kr);
		IOConnectCallScalarMethod(conn, PD_PVR_SEL_GEM_CLOSE, out, 1, NULL, NULL);
		goto fail;
	}
	s->conn = conn;
	s->base = (uint8_t *)(uintptr_t)va;
	s->size = in[0];
	s->width = width;
	s->height = height;
	s->pitch = pitch;
	memset(s->base, 0, s->size);
	pdscanout_clean(s, 0, 0, width, height, 4);
	pdscanout_sync();

	in[0] = s->handle;
	in[1] = 0;
	in[2] = pitch;
	nout = 2;
	kr = IOConnectCallScalarMethod(conn, PD_PVR_SEL_FLIP, in, 3, out, &nout);
	if (kr != KERN_SUCCESS) {
		snprintf(why, whylen, "flip failed: 0x%x", kr);
		pdscanout_close(s);
		return -1;
	}
	return 0;

fail:
	IOServiceClose(conn);
	return -1;
}

void
pdscanout_clean(struct pdscanout *s, int x1, int y1, int x2, int y2, int cpp)
{
	uintptr_t a, end;

	// whole rows are one run of the buffer
	if (x1 == 0 && (uint32_t)x2 * cpp >= s->pitch - LINE) {
		a = ((uintptr_t)s->base + (size_t)y1 * s->pitch) & ~(uintptr_t)(LINE - 1);
		end = (uintptr_t)s->base + (size_t)y2 * s->pitch;
		for (; a < end; a += LINE) {
			__asm__ volatile ("dc cvac, %0" :: "r" (a) : "memory");
		}
		return;
	}
	for (int y = y1; y < y2; y++) {
		a = ((uintptr_t)s->base + (size_t)y * s->pitch + (size_t)x1 * cpp) & ~(uintptr_t)(LINE - 1);
		end = (uintptr_t)s->base + (size_t)y * s->pitch + (size_t)x2 * cpp;
		for (; a < end; a += LINE) {
			__asm__ volatile ("dc cvac, %0" :: "r" (a) : "memory");
		}
	}
}

void
pdscanout_sync(void)
{
	__asm__ volatile ("dsb sy" ::: "memory");
}

void
pdscanout_close(struct pdscanout *s)
{
	uint64_t in[3] = { 0, 0, 0 }, out[2];
	uint32_t nout = 2;

	if (s->conn == 0)
		return;
	// the console comes back before the buffer goes
	IOConnectCallScalarMethod(s->conn, PD_PVR_SEL_FLIP, in, 3, out, &nout);
	if (s->base != NULL)
		IOConnectUnmapMemory64(s->conn, s->handle, mach_task_self(), (mach_vm_address_t)(uintptr_t)s->base);
	in[0] = s->handle;
	IOConnectCallScalarMethod(s->conn, PD_PVR_SEL_GEM_CLOSE, in, 1, NULL, NULL);
	IOServiceClose(s->conn);
	memset(s, 0, sizeof(*s));
}

#else

int
pdscanout_open(struct pdscanout *s, uint32_t width, uint32_t height, uint32_t pitch, char *why, int whylen)
{
	(void)width;
	(void)height;
	(void)pitch;
	s->base = 0;
	s->conn = 0;
	if (whylen > 0)
		why[0] = 0;
	return -1;
}

void
pdscanout_clean(struct pdscanout *s, int x1, int y1, int x2, int y2, int cpp)
{
	(void)s;
	(void)x1;
	(void)y1;
	(void)x2;
	(void)y2;
	(void)cpp;
}

void
pdscanout_sync(void)
{
}

void
pdscanout_close(struct pdscanout *s)
{
	(void)s;
}

#endif
