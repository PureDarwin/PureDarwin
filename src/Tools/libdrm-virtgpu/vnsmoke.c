// vnsmoke: the IOVirtIOGPU venus client end to end without mesa: capset, context, a mapped shmem blob the host
// answers vkEnumerateInstanceVersion into, then export and import of that blob by a second process

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>

#include "IOVirtIOGPU3DShared.h"

#define CAPSET_VENUS		4
#define BLOB_MEM_HOST3D		2
#define BLOB_FLAG_MAPPABLE	1
#define SHMEM_SIZE		(128 * 1024)
#define SHARE_MARK		0x5e5e0000u
#define CHILD_MARK		0xc0c0c0c0u
#define BENCH_FENCES		2000

// venus protocol command types and flags (venus-protocol vn_protocol_driver_defines.h)
#define CMD_vkEnumerateInstanceVersion		137
#define CMD_vkSetReplyCommandStreamMESA		178
#define CMD_vkSeekReplyCommandStreamMESA	179
#define CMD_GENERATE_REPLY			0x1

static int failed;

static void
check(int ok, const char *what)
{
	printf("vnsmoke: %-40s %s\n", what, ok ? "ok" : "FAILED");
	if (!ok)
		failed = 1;
}

static size_t
put32(uint8_t *p, size_t at, uint32_t v)
{
	memcpy(p + at, &v, 4);
	return at + 4;
}

static size_t
put64(uint8_t *p, size_t at, uint64_t v)
{
	memcpy(p + at, &v, 8);
	return at + 8;
}

static kern_return_t
submit(io_connect_t c, const void *cs, size_t len, uint32_t ring, uint32_t flags, uint64_t *fence)
{
	uint64_t in[2] = { ring, flags };
	uint32_t nout = 1;

	return IOConnectCallMethod(c, kPDVenus_Submit, in, 2, cs, len, fence, &nout, NULL, NULL);
}

static io_connect_t
open_venus(void)
{
	io_service_t svc = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOVirtIOGPU"));
	io_connect_t c = IO_OBJECT_NULL;

	if (!svc)
		return IO_OBJECT_NULL;
	if (IOServiceOpen(svc, mach_task_self(), kIOVirtIOGPUVenusConnectType, &c) != KERN_SUCCESS)
		c = IO_OBJECT_NULL;
	IOObjectRelease(svc);
	return c;
}

// the importing side, run as a separate process: import before having a context (the context
// attaches it), map, check the exporter's pattern and leave a mark
static int
import_child(uint64_t token)
{
	io_connect_t c = open_venus();
	uint64_t out[4] = { 0 }, ctxIn = CAPSET_VENUS, ctx = 0, mapOut[2];
	uint32_t n = 4;
	mach_vm_address_t addr = 0;
	mach_vm_size_t size = 0;

	if (!c)
		return 10;
	if (IOConnectCallScalarMethod(c, kPDVenus_Import, &token, 1, out, &n) != KERN_SUCCESS)
		return 11;
	if (out[1] != SHMEM_SIZE || out[2] != BLOB_MEM_HOST3D)
		return 12;
	n = 1;
	if (IOConnectCallScalarMethod(c, kPDVenus_ContextInit, &ctxIn, 1, &ctx, &n) != KERN_SUCCESS)
		return 13;
	n = 2;
	if (IOConnectCallScalarMethod(c, kPDVenus_Map, &out[0], 1, mapOut, &n) != KERN_SUCCESS)
		return 14;
	if (IOConnectMapMemory64(c, (uint32_t)out[0], mach_task_self(), &addr, &size, kIOMapAnywhere) != KERN_SUCCESS)
		return 15;

	volatile uint32_t *shm = (volatile uint32_t *)(uintptr_t)addr;
	for (uint32_t i = 1; i < SHMEM_SIZE / 4; i++) {
		if (shm[i] != (SHARE_MARK | (i & 0xffff)))
			return 16;
	}
	shm[0] = CHILD_MARK;
	IOConnectUnmapMemory64(c, (uint32_t)out[0], mach_task_self(), addr);
	IOServiceClose(c);
	return 0;
}

static int
spawn_import(const char *self, uint64_t token)
{
	char arg[32];
	char *argv[] = { (char *)self, "--import", arg, NULL };
	pid_t pid;
	int status = 0;

	snprintf(arg, sizeof(arg), "%llu", (unsigned long long)token);
	if (posix_spawn(&pid, self, NULL, NULL, argv, NULL) != 0 || waitpid(pid, &status, 0) != pid)
		return -1;
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int
main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	if (argc == 3 && strcmp(argv[1], "--import") == 0)
		return import_child(strtoull(argv[2], NULL, 10));

	io_service_t svc = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOVirtIOGPU"));
	if (!svc) {
		printf("vnsmoke: no IOVirtIOGPU\nFAIL\n");
		return 1;
	}
	io_connect_t c = IO_OBJECT_NULL;
	kern_return_t kr = IOServiceOpen(svc, mach_task_self(), kIOVirtIOGPUVenusConnectType, &c);
	IOObjectRelease(svc);
	if (kr != KERN_SUCCESS) {
		printf("vnsmoke: open venus client: 0x%x (no venus on this device?)\nFAIL\n", kr);
		return 1;
	}

	struct PDVenusInfo info;
	size_t infoLen = sizeof(info);
	kr = IOConnectCallStructMethod(c, kPDVenus_GetInfo, NULL, 0, &info, &infoLen);
	check(kr == KERN_SUCCESS, "get info");
	printf("vnsmoke: features 0x%x capsets 0x%x hostmem %llu MB\n", info.features, info.capset_mask,
	    (unsigned long long)(info.hostmem_size >> 20));

	uint32_t caps[64] = { 0 };
	size_t capsLen = sizeof(caps);
	uint64_t capIn[2] = { CAPSET_VENUS, 0 };
	kr = IOConnectCallMethod(c, kPDVenus_GetCaps, capIn, 2, NULL, 0, NULL, NULL, caps, &capsLen);
	check(kr == KERN_SUCCESS && capsLen >= 16, "venus capset");
	printf("vnsmoke: capset %zu bytes, wire format %u, vk.xml 0x%x, ext serial %u, venus proto %u\n",
	    capsLen, caps[0], caps[1], caps[2], caps[3]);

	uint64_t ctxIn = CAPSET_VENUS, ctx = 0;
	uint32_t n = 1;
	kr = IOConnectCallScalarMethod(c, kPDVenus_ContextInit, &ctxIn, 1, &ctx, &n);
	check(kr == KERN_SUCCESS, "context init");

	struct PDVenusBlobCreate bc = { BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, 0, SHMEM_SIZE };
	uint64_t blobOut[2] = { 0 };
	n = 2;
	kr = IOConnectCallMethod(c, kPDVenus_CreateBlob, NULL, 0, &bc, sizeof(bc), blobOut, &n, NULL, NULL);
	check(kr == KERN_SUCCESS, "create shmem blob");
	uint32_t res = (uint32_t)blobOut[0];

	uint64_t mapIn = res, mapOut[2] = { 0 };
	n = 2;
	kr = IOConnectCallScalarMethod(c, kPDVenus_Map, &mapIn, 1, mapOut, &n);
	check(kr == KERN_SUCCESS, "map blob into window");
	printf("vnsmoke: res %u map info 0x%llx size %llu\n", res, (unsigned long long)mapOut[0],
	    (unsigned long long)mapOut[1]);

	mach_vm_address_t addr = 0;
	mach_vm_size_t size = 0;
	kr = IOConnectMapMemory64(c, res, mach_task_self(), &addr, &size, kIOMapAnywhere);
	check(kr == KERN_SUCCESS && addr && size >= SHMEM_SIZE, "map blob into task");
	if (failed) {
		printf("FAIL\n");
		return 1;
	}

	volatile uint32_t *shm = (volatile uint32_t *)(uintptr_t)addr;
	for (uint32_t i = 0; i < SHMEM_SIZE / 4; i++)
		shm[i] = 0xa5a50000 | i;
	int same = 1;
	for (uint32_t i = 0; i < SHMEM_SIZE / 4; i++)
		same &= shm[i] == (0xa5a50000 | i);
	check(same, "cpu write/read through the window");
	memset((void *)shm, 0, 64);

	// reply stream = this blob at offset 0, then one command that generates a reply
	uint8_t cs[128];
	size_t at = 0;
	at = put32(cs, at, CMD_vkSetReplyCommandStreamMESA);
	at = put32(cs, at, 0);
	at = put64(cs, at, 1);
	at = put32(cs, at, res);
	at = put64(cs, at, 0);
	at = put64(cs, at, 4096);
	at = put32(cs, at, CMD_vkSeekReplyCommandStreamMESA);
	at = put32(cs, at, 0);
	at = put64(cs, at, 0);
	at = put32(cs, at, CMD_vkEnumerateInstanceVersion);
	at = put32(cs, at, CMD_GENERATE_REPLY);
	at = put64(cs, at, 1);

	uint64_t fence = 0;
	kr = submit(c, cs, at, 0, PD_VENUS_SUBMIT_RING | PD_VENUS_SUBMIT_FENCE, &fence);
	check(kr == KERN_SUCCESS && fence, "fenced submit on ring 0");

	uint64_t waitIn[2] = { fence, 2000000 };
	kr = IOConnectCallScalarMethod(c, kPDVenus_WaitFence, waitIn, 2, NULL, NULL);
	check(kr == KERN_SUCCESS, "fence retired");

	uint8_t retired = 0;
	size_t rlen = 1;
	kr = IOConnectCallStructMethod(c, kPDVenus_FenceStatus, &fence, sizeof(fence), &retired, &rlen);
	check(kr == KERN_SUCCESS && retired == 1, "fence status says retired");

	uint32_t type = shm[0], version = shm[4];
	int32_t result = (int32_t)shm[1];
	printf("vnsmoke: reply type %u result %d version %u.%u.%u\n", type, result, version >> 22,
	    (version >> 12) & 0x3ff, version & 0xfff);
	check(type == CMD_vkEnumerateInstanceVersion && result == 0 && (version >> 22) >= 1,
	    "host answered vkEnumerateInstanceVersion");

	// an empty fenced submit is how mesa gets an already signalled sync fd
	kr = submit(c, NULL, 0, 0, PD_VENUS_SUBMIT_RING | PD_VENUS_SUBMIT_FENCE, &fence);
	check(kr == KERN_SUCCESS && fence, "empty fenced submit");
	waitIn[0] = fence;
	kr = IOConnectCallScalarMethod(c, kPDVenus_WaitFence, waitIn, 2, NULL, NULL);
	check(kr == KERN_SUCCESS, "empty fence retired");

	uint64_t genIn[2] = { 0, 1000 }, gen = 0;
	n = 1;
	kr = IOConnectCallScalarMethod(c, kPDVenus_WaitProgress, genIn, 2, &gen, &n);
	check(kr == KERN_SUCCESS && gen != 0, "completion generation advances");

	// round trips of one reply command, wall and cpu time per fence
	struct rusage ru0, ru1;
	struct timespec t0, t1;
	int benchOk = 1;
	getrusage(RUSAGE_SELF, &ru0);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (int i = 0; i < BENCH_FENCES && benchOk; i++) {
		benchOk = submit(c, cs, at, 0, PD_VENUS_SUBMIT_RING | PD_VENUS_SUBMIT_FENCE, &fence) == KERN_SUCCESS;
		waitIn[0] = fence;
		benchOk = benchOk && IOConnectCallScalarMethod(c, kPDVenus_WaitFence, waitIn, 2, NULL, NULL) == KERN_SUCCESS;
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	getrusage(RUSAGE_SELF, &ru1);
	check(benchOk, "fence round trips");
	double wall = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;
	double cpu = (ru1.ru_utime.tv_sec - ru0.ru_utime.tv_sec + ru1.ru_stime.tv_sec - ru0.ru_stime.tv_sec) * 1e6 +
	    (ru1.ru_utime.tv_usec - ru0.ru_utime.tv_usec + ru1.ru_stime.tv_usec - ru0.ru_stime.tv_usec);
	printf("vnsmoke: %d fences, %.1f us wall, %.1f us cpu each\n", BENCH_FENCES, wall / BENCH_FENCES,
	    cpu / BENCH_FENCES);

	uint64_t exportIn = res, token = 0, token2 = 0;
	n = 1;
	kr = IOConnectCallScalarMethod(c, kPDVenus_Export, &exportIn, 1, &token, &n);
	check(kr == KERN_SUCCESS && token, "export blob");
	n = 1;
	kr = IOConnectCallScalarMethod(c, kPDVenus_Export, &exportIn, 1, &token2, &n);
	check(kr == KERN_SUCCESS && token2 == token, "export again gives the same token");
	uint64_t impOut[4] = { 0 };
	n = 4;
	kr = IOConnectCallScalarMethod(c, kPDVenus_Import, &token, 1, impOut, &n);
	check(kr == KERN_SUCCESS && impOut[0] == res, "import into the exporter gives its handle");

	for (uint32_t i = 0; i < SHMEM_SIZE / 4; i++)
		shm[i] = SHARE_MARK | (i & 0xffff);
	int rc = spawn_import(argv[0], token);
	if (rc)
		printf("vnsmoke: importer exited %d\n", rc);
	check(rc == 0 && shm[0] == CHILD_MARK, "another process imports, maps and writes it");

	IOConnectUnmapMemory64(c, res, mach_task_self(), addr);
	uint64_t unrefIn = res;
	kr = IOConnectCallScalarMethod(c, kPDVenus_Unref, &unrefIn, 1, NULL, NULL);
	check(kr == KERN_SUCCESS, "unref blob");
	// the export still holds it
	shm = NULL;
	rc = spawn_import(argv[0], token);
	check(rc == 0, "export keeps the blob alive after the unref");
	IOServiceClose(c);
	rc = spawn_import(argv[0], token);
	check(rc == 11, "the token dies with the exporting connection");

	printf(failed ? "FAIL\n" : "PASS\n");
	return failed;
}
