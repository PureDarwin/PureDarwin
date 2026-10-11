// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// pvr_gem.c's object lifetime on iokit memory: pages anywhere below 36 bits, the gpu sees them
// through a scatter list, the cpu only through write-combined mappings of their physical runs

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <kern/thread_call.h>
#include <pexpert/pexpert.h>
#include <sys/random.h>

#include "pvr_gem.h"

#define kGemPhysMask	0x0000000fffffc000ULL
#define kGemContigMask	0x00000000ffffc000ULL
// kIOMemoryBufferCacheShift, which iokit keeps out of the kext headers
#define kGemBufferCacheShift	28

struct pvr_gem_object *
pvr_gem_object_create(struct pvr_device *pvr_dev, size_t size, u64 flags)
{
	struct pvr_gem_object *obj;
	IOBufferMemoryDescriptor *bmd;
	IOAddressRange *ranges;
	IOMemoryDescriptor *md;
	uint8_t *cached;
	u32 n = 0, cap;

	if (size == 0 || (flags & PVR_BO_UNDEFINED_MASK))
		return (struct pvr_gem_object *)ERR_PTR(-EINVAL);
	size = PAGE_ALIGN(size);

	obj = (struct pvr_gem_object *)kzalloc(sizeof(*obj), GFP_KERNEL);
	if (obj == NULL)
		return (struct pvr_gem_object *)ERR_PTR(-ENOMEM);
	cap = (u32)(size / PAGE_SIZE);
	obj->sgt.sgl = (struct scatterlist *)kcalloc(cap, sizeof(struct scatterlist), GFP_KERNEL);
	ranges = (IOAddressRange *)kcalloc(cap, sizeof(IOAddressRange), GFP_KERNEL);
	// a scanout buffer is one run below 4 gib; when memory is too fragmented it stays usable by the gpu
	if (flags & PVR_BO_PD_CONTIGUOUS) {
		bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
		    kIODirectionInOut | kIOMemoryPhysicallyContiguous, size, kGemContigMask);
		if (bmd == NULL)
			flags &= ~PVR_BO_PD_CONTIGUOUS;
	} else {
		bmd = NULL;
	}
	if (bmd == NULL)
		bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIODirectionInOut, size,
		    kGemPhysMask);
	if (obj->sgt.sgl == NULL || ranges == NULL || bmd == NULL || bmd->prepare() != kIOReturnSuccess) {
		OSSafeReleaseNULL(bmd);
		kfree(ranges);
		kfree(obj->sgt.sgl);
		kfree(obj);
		return (struct pvr_gem_object *)ERR_PTR(-ENOMEM);
	}

	// zero through the cached alias, then push it out so it never lands on top of device writes
	cached = (uint8_t *)bmd->getBytesNoCopy();
	bzero(cached, size);
	for (size_t off = 0; off < size; off += 64) {
		__asm__ volatile ("dc civac, %0" :: "r"(cached + off) : "memory");
	}
	__asm__ volatile ("dsb sy" ::: "memory");

	for (size_t off = 0; off < size;) {
		IOByteCount len = 0;
		addr64_t pa = bmd->getPhysicalSegment(off, &len, kIOMemoryMapperNone);

		if (pa == 0 || len == 0)
			break;
		if (len > size - off)
			len = size - off;
		if (n > 0 && obj->sgt.sgl[n - 1].dma_address + obj->sgt.sgl[n - 1].length == pa &&
		    obj->sgt.sgl[n - 1].length + len <= 0x80000000u) {
			obj->sgt.sgl[n - 1].length += (u32)len;
			ranges[n - 1].length += len;
		} else {
			obj->sgt.sgl[n].dma_address = pa;
			obj->sgt.sgl[n].length = (u32)len;
			ranges[n].address = pa;
			ranges[n].length = len;
			n++;
		}
		off += len;
	}
	obj->sgt.nents = obj->sgt.orig_nents = n;

	// the cache mode lives in the descriptor: IOConnectMapMemory64 replaces a map's cache bits with
	// the caller's, so a mode passed only at map time leaves a user mapping cacheable
	md = IOMemoryDescriptor::withAddressRanges(ranges, n, kIODirectionInOut | kIOMemoryTypePhysical64 |
	    ((IOOptionBits)((flags & PVR_BO_PD_CPU_CACHED) ? kIOCopybackCache : kIOWriteCombineCache) <<
	    kGemBufferCacheShift), NULL);
	kfree(ranges);
	if (md == NULL) {
		bmd->complete();
		bmd->release();
		kfree(obj->sgt.sgl);
		kfree(obj);
		return (struct pvr_gem_object *)ERR_PTR(-ENOMEM);
	}

	kref_init(&obj->ref_count);
	obj->pvr_dev = pvr_dev;
	obj->flags = flags;
	obj->size = size;
	obj->bmd = bmd;
	obj->phys_md = md;
	return obj;
}

// a buffer the last handle dropped while a task still maps it: each IOMemoryMap of the physical
// descriptor holds a retain on it, so the pages are freed only once that count is back to ours
struct pvr_gem_grave {
	struct pvr_gem_grave *next;
	IOBufferMemoryDescriptor *bmd;
	IOMemoryDescriptor *phys_md;
};

static IOLock *gGraveLock;
static struct pvr_gem_grave *gGraves;
static thread_call_t gGraveCall;

static void
pvr_gem_pages_free(IOBufferMemoryDescriptor *bmd, IOMemoryDescriptor *phys_md)
{
	phys_md->release();
	bmd->complete();
	bmd->release();
}

static void
pvr_gem_reap(thread_call_param_t, thread_call_param_t)
{
	struct pvr_gem_grave *keep = NULL, *g, *next;
	uint64_t deadline;

	IOLockLock(gGraveLock);
	g = gGraves;
	gGraves = NULL;
	IOLockUnlock(gGraveLock);

	for (; g != NULL; g = next) {
		next = g->next;
		if (g->phys_md->getRetainCount() <= 1) {
			pvr_gem_pages_free(g->bmd, g->phys_md);
			IOFree(g, sizeof(*g));
		} else {
			g->next = keep;
			keep = g;
		}
	}

	IOLockLock(gGraveLock);
	while (keep != NULL) {
		g = keep;
		keep = keep->next;
		g->next = gGraves;
		gGraves = g;
	}
	if (gGraves != NULL) {
		clock_interval_to_deadline(200, kMillisecondScale, &deadline);
		thread_call_enter_delayed(gGraveCall, deadline);
	}
	IOLockUnlock(gGraveLock);
}

int
pvr_gem_init(void)
{
	if (gGraveLock == NULL)
		gGraveLock = IOLockAlloc();
	if (gGraveCall == NULL)
		gGraveCall = thread_call_allocate(pvr_gem_reap, NULL);
	return (gGraveLock != NULL && gGraveCall != NULL) ? 0 : -ENOMEM;
}

static void
pvr_gem_object_free(struct kref *ref)
{
	struct pvr_gem_object *obj = container_of(ref, struct pvr_gem_object, ref_count);
	IOBufferMemoryDescriptor *bmd = (IOBufferMemoryDescriptor *)obj->bmd;
	IOMemoryDescriptor *md = (IOMemoryDescriptor *)obj->phys_md;
	struct pvr_gem_grave *g;

	if (obj->kmap != NULL)
		((IOMemoryMap *)obj->kmap)->release();
	kfree(obj->sgt.sgl);
	kfree(obj);

	if (md->getRetainCount() <= 1 || gGraveCall == NULL) {
		pvr_gem_pages_free(bmd, md);
		return;
	}
	g = (struct pvr_gem_grave *)IOMalloc(sizeof(*g));
	if (g == NULL) {
		// leaking the pages beats freeing ones a task still maps
		IOLog("PDPowerVR: buffer pages leaked, still mapped and no memory to track them\n");
		return;
	}
	g->bmd = bmd;
	g->phys_md = md;
	IOLockLock(gGraveLock);
	g->next = gGraves;
	gGraves = g;
	thread_call_enter(gGraveCall);
	IOLockUnlock(gGraveLock);
}

void
pvr_gem_object_get(struct pvr_gem_object *pvr_obj)
{
	kref_get(&pvr_obj->ref_count);
}

void
pvr_gem_object_put(struct pvr_gem_object *pvr_obj)
{
	if (pvr_obj != NULL)
		kref_put(&pvr_obj->ref_count, pvr_gem_object_free);
}

void *
pvr_gem_object_vmap(struct pvr_gem_object *pvr_obj)
{
	if (pvr_obj->kmap == NULL) {
		IOMemoryMap *map = ((IOMemoryDescriptor *)pvr_obj->phys_md)->createMappingInTask(kernel_task, 0,
		    kIOMapAnywhere | kIOMapWriteCombineCache);

		if (map == NULL)
			return ERR_PTR(-ENOMEM);
		pvr_obj->kmap = map;
		pvr_obj->cpu = (u8 *)map->getVirtualAddress();
	}
	return pvr_obj->cpu;
}

void
pvr_gem_object_vunmap(struct pvr_gem_object *)
{
	// the mapping stays until the object goes away
}

int
pvr_gem_get_dma_addr(struct pvr_gem_object *pvr_obj, u32 offset, dma_addr_t *dma_addr_out)
{
	for (u32 i = 0; i < pvr_obj->sgt.nents; i++) {
		if (offset < pvr_obj->sgt.sgl[i].length) {
			*dma_addr_out = pvr_obj->sgt.sgl[i].dma_address + offset;
			return 0;
		}
		offset -= pvr_obj->sgt.sgl[i].length;
	}
	return -EINVAL;
}

int
pvr_gem_object_into_handle(struct pvr_gem_object *pvr_obj, struct pvr_file *pvr_file, u32 *handle)
{
	u32 h;
	int err = xa_alloc(&pvr_file->bo_handles, &h, pvr_obj, xa_limit_32b, GFP_KERNEL);

	if (err)
		return err;
	*handle = h;
	return 0;
}

struct pvr_gem_object *
pvr_gem_object_from_handle(struct pvr_file *pvr_file, u32 handle)
{
	struct pvr_gem_object *obj;

	xa_lock(&pvr_file->bo_handles);
	obj = (struct pvr_gem_object *)xa_load(&pvr_file->bo_handles, handle);
	if (obj != NULL)
		pvr_gem_object_get(obj);
	xa_unlock(&pvr_file->bo_handles);
	return obj;
}

int
pvr_gem_object_close_handle(struct pvr_file *pvr_file, u32 handle)
{
	struct pvr_gem_object *obj = (struct pvr_gem_object *)xa_erase(&pvr_file->bo_handles, handle);

	if (obj == NULL)
		return -EINVAL;
	pvr_gem_object_put(obj);
	return 0;
}

static bool gGemFlushChecked;
static uint32_t gGemFlush = 1;

// buffers mapped cacheable for their task (the gpu is not coherent) are cleaned to the point of coherency
// through their own kernel alias. pdpvrflush=2 does every mapped buffer (write-combined maps too), 0 none
u64
pvr_gem_file_cpu_flush(struct pvr_file *pvr_file)
{
	void *entry;
	unsigned long handle;
	u64 bytes = 0;

	if (!gGemFlushChecked) {
		PE_parse_boot_argn("pdpvrflush", &gGemFlush, sizeof(gGemFlush));
		gGemFlushChecked = true;
	}
	if (!gGemFlush)
		return 0;

	xa_lock(&pvr_file->bo_handles);
	__asm__ volatile ("dsb sy" ::: "memory");
	xa_for_each(&pvr_file->bo_handles, handle, entry) {
		struct pvr_gem_object *obj = (struct pvr_gem_object *)entry;
		uint8_t *va;

		if (!obj->user_mapped || (gGemFlush < 2 && !(obj->flags & PVR_BO_PD_CPU_CACHED)))
			continue;
		va = (uint8_t *)((IOBufferMemoryDescriptor *)obj->bmd)->getBytesNoCopy();
		for (size_t off = 0; off < obj->size; off += 64) {
			__asm__ volatile ("dc civac, %0" :: "r"(va + off) : "memory");
		}
		bytes += obj->size;
	}
	__asm__ volatile ("dsb sy" ::: "memory");
	xa_unlock(&pvr_file->bo_handles);
	return bytes;
}

u64
pvr_gem_contiguous_addr(struct pvr_gem_object *pvr_obj)
{
	if (pvr_obj->sgt.nents != 1 || pvr_obj->sgt.sgl[0].length < pvr_obj->size)
		return 0;
	return pvr_obj->sgt.sgl[0].dma_address;
}

#define kMaxExports	128

struct pvr_gem_export {
	u64 token;
	struct pvr_gem_object *obj;
	struct pvr_file *owner;
};

static struct pvr_gem_export gExports[kMaxExports];
static IOLock *gExportLock;

static IOLock *
export_lock(void)
{
	if (gExportLock == NULL) {
		IOLock *l = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, l, &gExportLock))
			IOLockFree(l);
	}
	return gExportLock;
}

// one token per buffer and exporting file, so re-exporting does not fill the table
int
pvr_gem_export(struct pvr_file *pvr_file, u32 handle, u64 *token)
{
	struct pvr_gem_object *obj = pvr_gem_object_from_handle(pvr_file, handle);
	struct pvr_gem_export *slot = NULL;
	IOLock *lock = export_lock();

	if (obj == NULL)
		return -ENOENT;
	if (lock == NULL) {
		pvr_gem_object_put(obj);
		return -ENOMEM;
	}
	IOLockLock(lock);
	for (int i = 0; i < kMaxExports; i++) {
		if (gExports[i].obj == obj && gExports[i].owner == pvr_file) {
			*token = gExports[i].token;
			IOLockUnlock(lock);
			pvr_gem_object_put(obj);
			return 0;
		}
		if (slot == NULL && gExports[i].obj == NULL)
			slot = &gExports[i];
	}
	if (slot == NULL) {
		IOLockUnlock(lock);
		pvr_gem_object_put(obj);
		return -ENOSPC;
	}
	do {
		read_random(&slot->token, sizeof(slot->token));
	} while (slot->token == 0);
	// the reference from_handle took is the export's
	slot->obj = obj;
	slot->owner = pvr_file;
	*token = slot->token;
	IOLockUnlock(lock);
	return 0;
}

// importing a buffer the file already holds gives back the same handle, as prime does
int
pvr_gem_import(struct pvr_file *pvr_file, u64 token, u32 *handle)
{
	struct pvr_gem_object *obj = NULL;
	IOLock *lock = export_lock();
	unsigned long h;
	void *held;
	int err;

	if (lock == NULL || token == 0)
		return -EINVAL;
	IOLockLock(lock);
	for (int i = 0; i < kMaxExports; i++) {
		if (gExports[i].obj != NULL && gExports[i].token == token) {
			obj = gExports[i].obj;
			pvr_gem_object_get(obj);
			break;
		}
	}
	IOLockUnlock(lock);
	if (obj == NULL)
		return -ENOENT;

	xa_lock(&pvr_file->bo_handles);
	xa_for_each(&pvr_file->bo_handles, h, held) {
		if (held == obj) {
			xa_unlock(&pvr_file->bo_handles);
			*handle = (u32)h;
			pvr_gem_object_put(obj);
			return 0;
		}
	}
	xa_unlock(&pvr_file->bo_handles);
	err = pvr_gem_object_into_handle(obj, pvr_file, handle);
	if (err)
		pvr_gem_object_put(obj);
	return err;
}

int
pvr_gem_export_release(struct pvr_file *pvr_file, u64 token)
{
	struct pvr_gem_object *obj = NULL;
	IOLock *lock = export_lock();

	if (lock == NULL)
		return -EINVAL;
	IOLockLock(lock);
	for (int i = 0; i < kMaxExports; i++) {
		if (gExports[i].obj != NULL && gExports[i].token == token && gExports[i].owner == pvr_file) {
			obj = gExports[i].obj;
			gExports[i].obj = NULL;
			gExports[i].owner = NULL;
			gExports[i].token = 0;
			break;
		}
	}
	IOLockUnlock(lock);
	if (obj == NULL)
		return -ENOENT;
	pvr_gem_object_put(obj);
	return 0;
}

void
pvr_gem_exports_release_file(struct pvr_file *pvr_file)
{
	struct pvr_gem_object *drop[kMaxExports];
	IOLock *lock = export_lock();
	int n = 0;

	if (lock == NULL)
		return;
	IOLockLock(lock);
	for (int i = 0; i < kMaxExports; i++) {
		if (gExports[i].obj != NULL && gExports[i].owner == pvr_file) {
			drop[n++] = gExports[i].obj;
			gExports[i].obj = NULL;
			gExports[i].owner = NULL;
			gExports[i].token = 0;
		}
	}
	IOLockUnlock(lock);
	for (int i = 0; i < n; i++) {
		pvr_gem_object_put(drop[i]);
	}
}
