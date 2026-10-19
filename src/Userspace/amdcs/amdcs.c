// SPDX-License-Identifier: MIT
// amdcs: one SDMA command submission through libdrm_amdgpu, as libdrm's basic tests do
// a write-linear packet fills a GTT buffer, the fence is waited on and the cpu checks it

#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define WORDS 64

static int
alloc_map(amdgpu_device_handle dev, uint64_t size, uint32_t domain, amdgpu_bo_handle *bo,
    void **cpu, uint64_t *va, amdgpu_va_handle *vah)
{
	struct amdgpu_bo_alloc_request req = { .alloc_size = size, .phys_alignment = 4096,
	    .preferred_heap = domain };
	int r;

	if ((r = amdgpu_bo_alloc(dev, &req, bo)) != 0)
		return r;
	if ((r = amdgpu_va_range_alloc(dev, amdgpu_gpu_va_range_general, size, 4096, 0, va, vah, 0)) != 0)
		return r;
	if ((r = amdgpu_bo_va_op(*bo, 0, size, *va, 0, AMDGPU_VA_OP_MAP)) != 0)
		return r;
	return amdgpu_bo_cpu_map(*bo, cpu);
}

int
main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/dev/dri/renderD128";
	amdgpu_device_handle dev;
	amdgpu_context_handle ctx;
	amdgpu_bo_handle dst, ib;
	amdgpu_va_handle dst_vah, ib_vah;
	amdgpu_bo_list_handle list;
	uint32_t major, minor, *d, *p, expired = 0, bad = 0, i, n = 0;
	uint64_t dst_va, ib_va;
	void *dcpu, *icpu;
	struct amdgpu_cs_ib_info ib_info = { 0 };
	struct amdgpu_cs_request req = { 0 };
	struct amdgpu_cs_fence fence = { 0 };
	amdgpu_bo_handle handles[2];
	int fd, r;

	fd = open(path, O_RDWR);
	if (fd < 0 || amdgpu_device_initialize(fd, &major, &minor, &dev) != 0) {
		printf("amdcs: device open failed\n");
		return 1;
	}
	if ((r = amdgpu_cs_ctx_create(dev, &ctx)) != 0 ||
	    (r = alloc_map(dev, 4096, AMDGPU_GEM_DOMAIN_GTT, &dst, &dcpu, &dst_va, &dst_vah)) != 0 ||
	    (r = alloc_map(dev, 4096, AMDGPU_GEM_DOMAIN_GTT, &ib, &icpu, &ib_va, &ib_vah)) != 0) {
		printf("amdcs: setup failed: %d\n", r);
		return 1;
	}
	d = dcpu;
	memset(d, 0, 4096);

	// SDMA_PACKET(WRITE, LINEAR), destination, count - 1 (gfx9 and later), data
	p = icpu;
	p[n++] = 0x00000002;
	p[n++] = (uint32_t)dst_va;
	p[n++] = (uint32_t)(dst_va >> 32);
	p[n++] = WORDS - 1;
	for (i = 0; i < WORDS; i++)
		p[n++] = 0xdead0000 | i;
	while (n & 7)
		p[n++] = 0;	// SDMA nop

	handles[0] = dst;
	handles[1] = ib;
	if ((r = amdgpu_bo_list_create(dev, 2, handles, NULL, &list)) != 0) {
		printf("amdcs: bo list failed: %d\n", r);
		return 1;
	}
	ib_info.ib_mc_address = ib_va;
	ib_info.size = n;
	req.ip_type = AMDGPU_HW_IP_DMA;
	req.number_of_ibs = 1;
	req.ibs = &ib_info;
	req.resources = list;
	if ((r = amdgpu_cs_submit(ctx, 0, &req, 1)) != 0) {
		printf("amdcs: cs submit failed: %d\n", r);
		return 1;
	}
	fence.context = ctx;
	fence.ip_type = AMDGPU_HW_IP_DMA;
	fence.fence = req.seq_no;
	r = amdgpu_cs_query_fence_status(&fence, 2000000000ull, 0, &expired);
	for (i = 0; i < WORDS; i++)
		bad += d[i] != (0xdead0000 | i);
	printf("amdcs: sdma submit seq %llu, wait %d, signalled %u, first word %08x, %u of %u words wrong\n",
	    (unsigned long long)req.seq_no, r, expired, d[0], bad, WORDS);
	printf("amdcs: %s\n", r == 0 && expired && bad == 0 ? "sdma write reached the buffer" : "FAILED");
	return bad != 0;
}
