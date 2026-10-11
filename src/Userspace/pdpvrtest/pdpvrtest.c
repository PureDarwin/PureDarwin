// pdpvrtest: drive the PDPowerVR user client the way the kext's own self-test drives the ioctl layer.
// a vm context, a buffer written from the cpu through IOConnectMapMemory64, a compute context and one compute job whose cdm control stream is a lone stream terminate

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pd_pvr_uapi.h"
#include "pvr_drm.h"

#define CTRL_ADDR_OFF		0x200000ULL
#define STATE_ADDR_OFF		0x300000ULL
#define CDM_STREAM_TERMINATE	0x80000000u
#define PATTERN(i)		(0xc0de0000u | (i))
#define BO_SIZE			0x4000

static io_connect_t conn;

static int
pvr_ioctl(uint32_t nr, void *args, size_t size, const char *what)
{
	size_t outsz = size;
	kern_return_t kr = IOConnectCallStructMethod(conn, nr, args, size, args, &outsz);

	if (kr != KERN_SUCCESS)
		printf("pdpvrtest: %s failed, kr 0x%x errno %d\n", what, kr, PD_PVR_ERRNO(kr));
	return kr == KERN_SUCCESS ? 0 : (PD_PVR_ERRNO(kr) > 0 ? PD_PVR_ERRNO(kr) : -1);
}

static int
dev_query(uint32_t type, void *p, uint32_t size, const char *what)
{
	struct drm_pvr_ioctl_dev_query_args q = { .type = type, .size = size,
	    .pointer = (uint64_t)(uintptr_t)p };

	return pvr_ioctl(DRM_IOCTL_PVR_DEV_QUERY, &q, sizeof(q), what);
}

static int
create_bo(uint64_t size, uint64_t flags, uint32_t *handle)
{
	struct drm_pvr_ioctl_create_bo_args a = { .size = size, .flags = flags };
	int err = pvr_ioctl(DRM_IOCTL_PVR_CREATE_BO, &a, sizeof(a), "create bo");

	*handle = a.handle;
	return err;
}

// the cmd stream layout depends on device features, the fields after the two addresses are all
// zero, so only the count of optional 32-bit words and the quirk extension header vary
static uint32_t
build_cmd_stream(uint8_t *buf, uint64_t ctrl, uint64_t state, int extra_words, int brn49927)
{
	uint32_t off = 8;
	uint32_t hdr[2] = { 0, 0 };

	memset(buf, 0, 128);
	off += 8;					// tpu_border_colour_table
	memcpy(buf + off, &ctrl, 8);			// cdm_ctrl_stream_base
	off += 8;
	memcpy(buf + off, &state, 8);			// cdm_context_state_base_addr
	off += 8;
	off += 4;					// cdm_resume_pds1
	off += 4 * extra_words;
	hdr[0] = off;
	memcpy(buf, hdr, sizeof(hdr));
	if (brn49927) {
		uint32_t ext = 1;			// header type 0, compute0 brn49927

		memcpy(buf + off, &ext, 4);
		off += 4;
		off += 4;				// regs.tpu
	}
	return off;
}

int
main(void)
{
	struct drm_pvr_dev_query_gpu_info gpu = { 0 };
	struct drm_pvr_heap heaps[8];
	struct drm_pvr_dev_query_heap_info hi = { 0 };
	struct drm_pvr_dev_query_quirks qk = { 0 };
	uint32_t quirks[32];
	struct drm_pvr_ioctl_create_vm_context_args vm = { 0 };
	struct drm_pvr_ioctl_get_bo_mmap_offset_args mo = { 0 };
	struct drm_pvr_ioctl_vm_map_args map = { 0 };
	struct drm_pvr_ioctl_create_context_args ctx = { 0 };
	struct drm_pvr_ioctl_submit_jobs_args submit = { 0 };
	struct drm_pvr_job job = { 0 };
	struct drm_pvr_sync_op sig = { 0 };
	struct pd_pvr_syncobj_wait w = { 0 };
	uint8_t static_state[64] = { 0 }, cmd[128];
	uint32_t bo_ctrl = 0, bo_state = 0, sync = 0, cmd_len = 0, bad = 0;
	uint64_t general, sync_out = 0, sync_flags = 0;
	uint32_t sync_out_cnt = 1;
	int brn49927 = 0;
	mach_vm_address_t va = 0;
	mach_vm_size_t vsize = 0;
	volatile uint32_t *ctrl;
	io_service_t svc;
	kern_return_t kr;
	int err = 0, submitted = 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching(PD_PVR_SERVICE_CLASS));
	if (svc == IO_OBJECT_NULL) {
		printf("pdpvrtest: no %s service\n", PD_PVR_SERVICE_CLASS);
		return 1;
	}
	kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
	IOObjectRelease(svc);
	if (kr != KERN_SUCCESS) {
		printf("pdpvrtest: IOServiceOpen failed, kr 0x%x\n", kr);
		return 1;
	}
	printf("pdpvrtest: user client open\n");

	if (dev_query(DRM_PVR_DEV_QUERY_GPU_INFO_GET, &gpu, sizeof(gpu), "gpu info") == 0)
		printf("pdpvrtest: gpu id 0x%016llx, num phantoms %u\n", (unsigned long long)gpu.gpu_id,
		    gpu.num_phantoms);
	hi.heaps.stride = sizeof(heaps[0]);
	hi.heaps.count = 8;
	hi.heaps.array = (uint64_t)(uintptr_t)heaps;
	memset(heaps, 0, sizeof(heaps));
	if (dev_query(DRM_PVR_DEV_QUERY_HEAP_INFO_GET, &hi, sizeof(hi), "heap info") != 0)
		goto out;
	general = heaps[DRM_PVR_HEAP_GENERAL].base;
	printf("pdpvrtest: %u heaps, general at 0x%llx size 0x%llx\n", hi.heaps.count,
	    (unsigned long long)general, (unsigned long long)heaps[DRM_PVR_HEAP_GENERAL].size);
	qk.quirks = (uint64_t)(uintptr_t)quirks;
	qk.count = 32;
	if (dev_query(DRM_PVR_DEV_QUERY_QUIRKS_GET, &qk, sizeof(qk), "quirks") == 0) {
		printf("pdpvrtest: %u quirks (%u must-have):", qk.count, qk.musthave_count);
		for (int i = 0; i < qk.count && i < 32; i++) {
			printf(" %u", quirks[i]);
			if (i < qk.musthave_count && quirks[i] == 49927)
				brn49927 = 1;
		}
		printf("\n");
	} else {
		goto out;
	}

	if ((err = pvr_ioctl(DRM_IOCTL_PVR_CREATE_VM_CONTEXT, &vm, sizeof(vm), "vm context")) != 0)
		goto out;
	printf("pdpvrtest: vm context %u\n", vm.handle);
	if ((err = create_bo(BO_SIZE, DRM_PVR_BO_ALLOW_CPU_USERSPACE_ACCESS, &bo_ctrl)) != 0 ||
	    (err = create_bo(BO_SIZE, 0, &bo_state)) != 0)
		goto out;
	mo.handle = bo_ctrl;
	if ((err = pvr_ioctl(DRM_IOCTL_PVR_GET_BO_MMAP_OFFSET, &mo, sizeof(mo), "mmap offset")) != 0)
		goto out;
	kr = IOConnectMapMemory64(conn, (uint32_t)mo.offset, mach_task_self(), &va, &vsize,
	    kIOMapAnywhere);
	if (kr != KERN_SUCCESS) {
		printf("pdpvrtest: IOConnectMapMemory64 failed, kr 0x%x\n", kr);
		err = -1;
		goto out;
	}
	printf("pdpvrtest: buffers %u and %u, control buffer offset 0x%llx mapped at 0x%llx size 0x%llx\n",
	    bo_ctrl, bo_state, (unsigned long long)mo.offset, (unsigned long long)va,
	    (unsigned long long)vsize);
	ctrl = (volatile uint32_t *)(uintptr_t)va;
	ctrl[0] = CDM_STREAM_TERMINATE;
	for (uint32_t i = 1; i < BO_SIZE / 4; i++) {
		ctrl[i] = PATTERN(i);
	}
	__asm__ volatile ("dsb sy" ::: "memory");

	map.vm_context_handle = vm.handle;
	map.handle = bo_ctrl;
	map.device_addr = general + CTRL_ADDR_OFF;
	map.size = BO_SIZE;
	if ((err = pvr_ioctl(DRM_IOCTL_PVR_VM_MAP, &map, sizeof(map), "vm map control")) != 0)
		goto out;
	map.handle = bo_state;
	map.device_addr = general + STATE_ADDR_OFF;
	if ((err = pvr_ioctl(DRM_IOCTL_PVR_VM_MAP, &map, sizeof(map), "vm map state")) != 0)
		goto out;
	printf("pdpvrtest: buffers mapped at gpu 0x%llx and 0x%llx\n",
	    (unsigned long long)(general + CTRL_ADDR_OFF), (unsigned long long)(general + STATE_ADDR_OFF));

	// static compute context state: seven zero 64-bit registers behind the stream header
	((uint32_t *)static_state)[0] = sizeof(static_state);
	ctx.type = DRM_PVR_CTX_TYPE_COMPUTE;
	ctx.priority = DRM_PVR_CTX_PRIORITY_NORMAL;
	ctx.static_context_state = (uint64_t)(uintptr_t)static_state;
	ctx.static_context_state_len = sizeof(static_state);
	ctx.vm_context_handle = vm.handle;
	if ((err = pvr_ioctl(DRM_IOCTL_PVR_CREATE_CONTEXT, &ctx, sizeof(ctx), "compute context")) != 0)
		goto out;
	printf("pdpvrtest: compute context %u\n", ctx.handle);

	kr = IOConnectCallScalarMethod(conn, PD_PVR_SEL_SYNCOBJ_CREATE, &sync_flags, 1, &sync_out,
	    &sync_out_cnt);
	if (kr != KERN_SUCCESS) {
		printf("pdpvrtest: syncobj create failed, kr 0x%x\n", kr);
		err = -1;
		goto out;
	}
	sync = (uint32_t)sync_out;
	sig.handle = sync;
	sig.flags = DRM_PVR_SYNC_OP_FLAG_HANDLE_TYPE_SYNCOBJ | DRM_PVR_SYNC_OP_FLAG_SIGNAL;
	job.type = DRM_PVR_JOB_TYPE_COMPUTE;
	job.context_handle = ctx.handle;
	job.sync_ops.stride = sizeof(sig);
	job.sync_ops.count = 1;
	job.sync_ops.array = (uint64_t)(uintptr_t)&sig;
	submit.jobs.stride = sizeof(job);
	submit.jobs.count = 1;
	submit.jobs.array = (uint64_t)(uintptr_t)&job;

	// one layout only: a rejected stream takes the kernel's create_job error path. the kext's
	// self-test sends 44 bytes on this gpu, the quirk header plus none or two optional words
	cmd_len = build_cmd_stream(cmd, general + CTRL_ADDR_OFF, general + STATE_ADDR_OFF, brn49927 ? 0 : 2,
	    brn49927);
	job.cmd_stream = (uint64_t)(uintptr_t)cmd;
	job.cmd_stream_len = cmd_len;
	err = pvr_ioctl(DRM_IOCTL_PVR_SUBMIT_JOBS, &submit, sizeof(submit), "submit");
	printf("pdpvrtest: submitted a %u-byte cmd stream (quirk 49927 header %d): %d\n", cmd_len, brn49927, err);
	submitted = (err == 0);
	if (!submitted)
		goto out;

	w.handles = (uint64_t)(uintptr_t)&sync;
	w.count = 1;
	w.flags = PD_PVR_SYNCOBJ_WAIT_ALL;
	w.timeout_us = 2000000;
	{
		size_t wsz = sizeof(w);

		kr = IOConnectCallStructMethod(conn, PD_PVR_SEL_SYNCOBJ_WAIT, &w, sizeof(w), &w, &wsz);
	}
	printf("pdpvrtest: null compute job %s (kr 0x%x errno %d)\n",
	    kr == KERN_SUCCESS ? "finished" : "did not finish", kr, PD_PVR_ERRNO(kr));
	err = kr == KERN_SUCCESS ? 0 : -1;

	for (uint32_t i = 1; i < BO_SIZE / 4; i++) {
		if (ctrl[i] != PATTERN(i))
			bad++;
	}
	printf("pdpvrtest: buffer read back: word0 0x%08x word1 0x%08x word2 0x%08x last 0x%08x, %u of %u "
	    "pattern words wrong\n", ctrl[0], ctrl[1], ctrl[2], ctrl[BO_SIZE / 4 - 1], bad, BO_SIZE / 4 - 1);
	if (bad)
		err = -1;

out:
	if (sync) {
		uint64_t h = sync;

		IOConnectCallScalarMethod(conn, PD_PVR_SEL_SYNCOBJ_DESTROY, &h, 1, NULL, NULL);
	}
	if (ctx.handle) {
		struct drm_pvr_ioctl_destroy_context_args d = { .handle = ctx.handle };

		pvr_ioctl(DRM_IOCTL_PVR_DESTROY_CONTEXT, &d, sizeof(d), "destroy context");
	}
	if (va)
		IOConnectUnmapMemory64(conn, (uint32_t)mo.offset, mach_task_self(), va);
	if (vm.handle) {
		struct drm_pvr_ioctl_vm_unmap_args u = { .vm_context_handle = vm.handle, .size = BO_SIZE };
		struct drm_pvr_ioctl_destroy_vm_context_args d = { .handle = vm.handle };

		u.device_addr = general + CTRL_ADDR_OFF;
		if (bo_ctrl)
			pvr_ioctl(DRM_IOCTL_PVR_VM_UNMAP, &u, sizeof(u), "vm unmap control");
		u.device_addr = general + STATE_ADDR_OFF;
		if (bo_state)
			pvr_ioctl(DRM_IOCTL_PVR_VM_UNMAP, &u, sizeof(u), "vm unmap state");
		pvr_ioctl(DRM_IOCTL_PVR_DESTROY_VM_CONTEXT, &d, sizeof(d), "destroy vm context");
	}
	for (int i = 0; i < 2; i++) {
		uint64_t h = i ? bo_state : bo_ctrl;

		if (h)
			IOConnectCallScalarMethod(conn, PD_PVR_SEL_GEM_CLOSE, &h, 1, NULL, NULL);
	}
	IOServiceClose(conn);
	printf("pdpvrtest: %s\n", err ? "FAIL" : "PASS");
	return err ? 1 : 0;
}
