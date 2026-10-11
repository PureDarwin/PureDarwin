/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// pvr_gem.h's buffer object, backed by an iokit buffer instead of shmem (pvr_gem.cpp)

#ifndef _PDPOWERVR_PVR_GEM_H
#define _PDPOWERVR_PVR_GEM_H

#include "pvr_device.h"

#ifdef __cplusplus
extern "C" {
#endif

// pd: one physically contiguous run below 4 gib, the scanout engine has no iommu in use
#define PVR_BO_PD_CONTIGUOUS	(1ull << 62)
// pd: the task maps it write-back cached and cleans what it wrote itself (a cpu-drawn scanout)
#define PVR_BO_PD_CPU_CACHED	(1ull << 61)
#define PVR_BO_KERNEL_FLAGS_MASK (PVR_BO_FW_NO_CLEAR_ON_RESET | PVR_BO_PD_CONTIGUOUS | PVR_BO_PD_CPU_CACHED)
#define PVR_BO_UNDEFINED_MASK ~(PVR_BO_KERNEL_FLAGS_MASK | DRM_PVR_BO_FLAGS_MASK)

struct pvr_gem_object {
	struct kref ref_count;
	struct pvr_device *pvr_dev;
	u64 flags;
	size_t size;
	// the physically contiguous runs, coalesced
	struct sg_table sgt;
	// iokit: the buffer, a descriptor over its physical runs, and its write-combined kernel map
	void *bmd;
	void *phys_md;
	void *kmap;
	u8 *cpu;
	// handed to a task through clientMemoryForType
	bool user_mapped;
};

struct pvr_gem_object *pvr_gem_object_create(struct pvr_device *pvr_dev, size_t size, u64 flags);
// the deferred free of buffers still mapped into a task
int pvr_gem_init(void);
void pvr_gem_object_get(struct pvr_gem_object *pvr_obj);
void pvr_gem_object_put(struct pvr_gem_object *pvr_obj);
void *pvr_gem_object_vmap(struct pvr_gem_object *pvr_obj);
// the drm file's gem handle table
int pvr_gem_object_into_handle(struct pvr_gem_object *pvr_obj, struct pvr_file *pvr_file, u32 *handle);
struct pvr_gem_object *pvr_gem_object_from_handle(struct pvr_file *pvr_file, u32 handle);
int pvr_gem_object_close_handle(struct pvr_file *pvr_file, u32 handle);
// pd's dma-buf: an export is an unguessable token holding a reference, imported by any file
int pvr_gem_export(struct pvr_file *pvr_file, u32 handle, u64 *token);
int pvr_gem_import(struct pvr_file *pvr_file, u64 token, u32 *handle);
int pvr_gem_export_release(struct pvr_file *pvr_file, u64 token);
void pvr_gem_exports_release_file(struct pvr_file *pvr_file);
// the bus address of a contiguous buffer, 0 when it is not one run
u64 pvr_gem_contiguous_addr(struct pvr_gem_object *pvr_obj);
// clean and invalidate the cpu caches over every buffer the file's task maps, before the gpu reads
// them and after it writes them; returns the bytes covered
u64 pvr_gem_file_cpu_flush(struct pvr_file *pvr_file);
void pvr_gem_object_vunmap(struct pvr_gem_object *pvr_obj);
int pvr_gem_get_dma_addr(struct pvr_gem_object *pvr_obj, u32 offset, dma_addr_t *dma_addr_out);
static inline size_t pvr_gem_object_size(struct pvr_gem_object *pvr_obj) { return pvr_obj->size; }
static inline struct sg_table *pvr_gem_object_get_pages_sgt(struct pvr_gem_object *pvr_obj)
{
	return &pvr_obj->sgt;
}

#ifdef __cplusplus
}
#endif

#endif
