// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// pvr_vm.c without drm_gpuvm: a context keeps a plain list of whole mappings. a map may replace
// mappings it fully covers; partial overlaps, which gpuvm would split, are refused

#include "pvr_vm.h"

#include "pvr_device.h"
#include "pvr_drv.h"
#include "pvr_gem.h"
#include "pvr_mmu.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_heap_config.h"

#include <linux/err.h>
#include <linux/kref.h>
#include <linux/mutex.h>

struct pvr_vm_mapping {
	struct list_head node;
	u64 device_addr;
	u64 size;
	u64 offset;
	struct pvr_gem_object *pvr_obj;
};

struct pvr_vm_context {
	/**
	 * @pvr_dev: The PowerVR device to which this context is bound.
	 * This binding is immutable for the life of the context.
	 */
	struct pvr_device *pvr_dev;

	/** @mmu_ctx: The context for binding to physical memory. */
	struct pvr_mmu_context *mmu_ctx;

	/** @lock: Global lock on this VM. */
	struct mutex lock;

	/**
	 * @fw_mem_ctx_obj: Firmware object representing firmware memory
	 * context.
	 */
	struct pvr_fw_object *fw_mem_ctx_obj;

	/** @ref_count: Reference count of object. */
	struct kref ref_count;

	// the mappings, in no particular order
	struct list_head mappings;
};

struct pvr_vm_context *pvr_vm_context_get(struct pvr_vm_context *vm_ctx)
{
	if (vm_ctx)
		kref_get(&vm_ctx->ref_count);

	return vm_ctx;
}

dma_addr_t pvr_vm_get_page_table_root_addr(struct pvr_vm_context *vm_ctx)
{
	return pvr_mmu_get_root_table_dma_addr(vm_ctx->mmu_ctx);
}

bool
pvr_device_addr_is_valid(u64 device_addr)
{
	return (device_addr & ~PVR_PAGE_TABLE_ADDR_MASK) == 0 &&
	       (device_addr & ~PVR_DEVICE_PAGE_MASK) == 0;
}

bool
pvr_device_addr_and_size_are_valid(struct pvr_vm_context *vm_ctx,
				   u64 device_addr, u64 size)
{
	(void)vm_ctx;
	return pvr_device_addr_is_valid(device_addr) &&
	       size != 0 && (size & ~PVR_DEVICE_PAGE_MASK) == 0 &&
	       (device_addr + size <= PVR_PAGE_TABLE_ADDR_SPACE_SIZE);
}

static void
fw_mem_context_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_fwmemcontext *fw_mem_ctx = cpu_ptr;
	struct pvr_vm_context *vm_ctx = priv;

	fw_mem_ctx->pc_dev_paddr = pvr_vm_get_page_table_root_addr(vm_ctx);
	fw_mem_ctx->page_cat_base_reg_set = ROGUE_FW_BIF_INVALID_PCSET;
}

struct pvr_vm_context *
pvr_vm_create_context(struct pvr_device *pvr_dev, bool is_userspace_context)
{
	struct pvr_vm_context *vm_ctx;
	u16 device_addr_bits;

	int err;

	err = PVR_FEATURE_VALUE(pvr_dev, virtual_address_space_bits,
				&device_addr_bits);
	if (err) {
		drm_err(drm_dev,
			"Failed to get device virtual address space bits\n");
		return ERR_PTR(err);
	}

	if (device_addr_bits != PVR_PAGE_TABLE_ADDR_BITS) {
		drm_err(drm_dev,
			"Device has unsupported virtual address space size\n");
		return ERR_PTR(-EINVAL);
	}

	vm_ctx = kzalloc(sizeof(*vm_ctx), GFP_KERNEL);
	if (!vm_ctx)
		return ERR_PTR(-ENOMEM);

	vm_ctx->pvr_dev = pvr_dev;
	INIT_LIST_HEAD(&vm_ctx->mappings);

	vm_ctx->mmu_ctx = pvr_mmu_context_create(pvr_dev);
	err = PTR_ERR_OR_ZERO(vm_ctx->mmu_ctx);
	if (err)
		goto err_free;

	if (is_userspace_context) {
		err = pvr_fw_object_create(pvr_dev, sizeof(struct rogue_fwif_fwmemcontext),
					   PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
					   fw_mem_context_init, vm_ctx, &vm_ctx->fw_mem_ctx_obj);

		if (err)
			goto err_page_table_destroy;
	}

	mutex_init(&vm_ctx->lock);
	kref_init(&vm_ctx->ref_count);

	return vm_ctx;

err_page_table_destroy:
	pvr_mmu_context_destroy(vm_ctx->mmu_ctx);

err_free:
	kfree(vm_ctx);

	return ERR_PTR(err);
}

static void
pvr_vm_context_release(struct kref *ref_count)
{
	struct pvr_vm_context *vm_ctx =
		container_of(ref_count, struct pvr_vm_context, ref_count);

	if (vm_ctx->fw_mem_ctx_obj)
		pvr_fw_object_destroy(vm_ctx->fw_mem_ctx_obj);

	pvr_vm_unmap_all(vm_ctx);

	pvr_mmu_context_destroy(vm_ctx->mmu_ctx);
	mutex_destroy(&vm_ctx->lock);
	kfree(vm_ctx);
}

bool
pvr_vm_context_put(struct pvr_vm_context *vm_ctx)
{
	if (vm_ctx)
		return kref_put(&vm_ctx->ref_count, pvr_vm_context_release);

	return true;
}

// the page table side of an unmap, flushed before it returns
static int
pvr_vm_unmap_mapping_locked(struct pvr_vm_context *vm_ctx, struct pvr_vm_mapping *m)
{
	struct pvr_mmu_op_context *op_ctx = pvr_mmu_op_context_create(vm_ctx->mmu_ctx, NULL, 0, 0);
	int err;

	if (IS_ERR(op_ctx))
		return PTR_ERR(op_ctx);
	err = pvr_mmu_unmap(op_ctx, m->device_addr, m->size);
	pvr_mmu_op_context_destroy(op_ctx);
	if (err)
		return err;

	list_del(&m->node);
	pvr_gem_object_put(m->pvr_obj);
	kfree(m);
	return 0;
}

int
pvr_vm_map(struct pvr_vm_context *vm_ctx, struct pvr_gem_object *pvr_obj,
	   u64 pvr_obj_offset, u64 device_addr, u64 size)
{
	const bool is_user = vm_ctx->fw_mem_ctx_obj != NULL;
	const u64 pvr_obj_size = pvr_gem_object_size(pvr_obj);
	struct pvr_vm_mapping *m, *it, *next;
	struct pvr_mmu_op_context *op_ctx;
	u64 offset_plus_size;
	int err;

	if (check_add_overflow(pvr_obj_offset, size, &offset_plus_size))
		return -EINVAL;

	if (is_user &&
	    !pvr_find_heap_containing(vm_ctx->pvr_dev, device_addr, size)) {
		return -EINVAL;
	}

	if (!pvr_device_addr_and_size_are_valid(vm_ctx, device_addr, size) ||
	    pvr_obj_offset & ~PAGE_MASK || size & ~PAGE_MASK ||
	    pvr_obj_offset >= pvr_obj_size || offset_plus_size > pvr_obj_size)
		return -EINVAL;

	mutex_lock(&vm_ctx->lock);

	// what gpuvm would split is refused; what the new range covers whole is replaced
	list_for_each_entry_safe(it, next, &vm_ctx->mappings, node) {
		u64 end = device_addr + size, it_end = it->device_addr + it->size;

		if (it->device_addr >= end || it_end <= device_addr)
			continue;
		if (it->device_addr < device_addr || it_end > end) {
			err = -EINVAL;
			goto err_unlock;
		}
		err = pvr_vm_unmap_mapping_locked(vm_ctx, it);
		if (err)
			goto err_unlock;
	}

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m) {
		err = -ENOMEM;
		goto err_unlock;
	}

	op_ctx = pvr_mmu_op_context_create(vm_ctx->mmu_ctx, pvr_gem_object_get_pages_sgt(pvr_obj),
					   pvr_obj_offset, size);
	err = PTR_ERR_OR_ZERO(op_ctx);
	if (err)
		goto err_free;

	err = pvr_mmu_map(op_ctx, size, pvr_obj->flags, device_addr);
	pvr_mmu_op_context_destroy(op_ctx);
	if (err)
		goto err_free;

	pvr_gem_object_get(pvr_obj);
	m->pvr_obj = pvr_obj;
	m->device_addr = device_addr;
	m->size = size;
	m->offset = pvr_obj_offset;
	list_add_tail(&m->node, &vm_ctx->mappings);

	mutex_unlock(&vm_ctx->lock);

	return 0;

err_free:
	kfree(m);
err_unlock:
	mutex_unlock(&vm_ctx->lock);
	return err;
}

int
pvr_vm_unmap_obj(struct pvr_vm_context *vm_ctx, struct pvr_gem_object *pvr_obj,
		 u64 device_addr, u64 size)
{
	struct pvr_vm_mapping *it, *next;
	int err = -ENOENT;

	if (!pvr_device_addr_and_size_are_valid(vm_ctx, device_addr, size))
		return -EINVAL;

	mutex_lock(&vm_ctx->lock);
	list_for_each_entry_safe(it, next, &vm_ctx->mappings, node) {
		if (it->device_addr == device_addr && it->size == size &&
		    (pvr_obj == NULL || it->pvr_obj == pvr_obj)) {
			err = pvr_vm_unmap_mapping_locked(vm_ctx, it);
			break;
		}
	}
	mutex_unlock(&vm_ctx->lock);

	return err;
}

int
pvr_vm_unmap(struct pvr_vm_context *vm_ctx, u64 device_addr, u64 size)
{
	return pvr_vm_unmap_obj(vm_ctx, NULL, device_addr, size);
}

void
pvr_vm_unmap_all(struct pvr_vm_context *vm_ctx)
{
	mutex_lock(&vm_ctx->lock);

	while (!list_empty(&vm_ctx->mappings)) {
		struct pvr_vm_mapping *m = list_first_entry(&vm_ctx->mappings,
							    struct pvr_vm_mapping, node);

		if (WARN_ON(pvr_vm_unmap_mapping_locked(vm_ctx, m))) {
			list_del(&m->node);
			pvr_gem_object_put(m->pvr_obj);
			kfree(m);
		}
	}

	mutex_unlock(&vm_ctx->lock);
}

/* Static data areas are determined by firmware. */
static const struct drm_pvr_static_data_area static_data_areas[] = {
	{
		.area_usage = DRM_PVR_STATIC_DATA_AREA_FENCE,
		.location_heap_id = DRM_PVR_HEAP_GENERAL,
		.offset = 0,
		.size = 128,
	},
	{
		.area_usage = DRM_PVR_STATIC_DATA_AREA_YUV_CSC,
		.location_heap_id = DRM_PVR_HEAP_GENERAL,
		.offset = 128,
		.size = 1024,
	},
	{
		.area_usage = DRM_PVR_STATIC_DATA_AREA_VDM_SYNC,
		.location_heap_id = DRM_PVR_HEAP_PDS_CODE_DATA,
		.offset = 0,
		.size = 128,
	},
	{
		.area_usage = DRM_PVR_STATIC_DATA_AREA_EOT,
		.location_heap_id = DRM_PVR_HEAP_PDS_CODE_DATA,
		.offset = 128,
		.size = 128,
	},
	{
		.area_usage = DRM_PVR_STATIC_DATA_AREA_VDM_SYNC,
		.location_heap_id = DRM_PVR_HEAP_USC_CODE,
		.offset = 0,
		.size = 128,
	},
};

static const struct drm_pvr_heap pvr_heaps[] = {
	[DRM_PVR_HEAP_GENERAL] = {
		.base = ROGUE_GENERAL_HEAP_BASE,
		.size = ROGUE_GENERAL_HEAP_SIZE,
		.flags = 0,
		.page_size_log2 = PVR_DEVICE_PAGE_SHIFT,
	},
	[DRM_PVR_HEAP_PDS_CODE_DATA] = {
		.base = ROGUE_PDSCODEDATA_HEAP_BASE,
		.size = ROGUE_PDSCODEDATA_HEAP_SIZE,
		.flags = 0,
		.page_size_log2 = PVR_DEVICE_PAGE_SHIFT,
	},
	[DRM_PVR_HEAP_USC_CODE] = {
		.base = ROGUE_USCCODE_HEAP_BASE,
		.size = ROGUE_USCCODE_HEAP_SIZE,
		.flags = 0,
		.page_size_log2 = PVR_DEVICE_PAGE_SHIFT,
	},
	[DRM_PVR_HEAP_RGNHDR] = {
		.base = ROGUE_RGNHDR_HEAP_BASE,
		.size = ROGUE_RGNHDR_HEAP_SIZE,
		.flags = 0,
		.page_size_log2 = PVR_DEVICE_PAGE_SHIFT,
	},
	[DRM_PVR_HEAP_VIS_TEST] = {
		.base = ROGUE_VISTEST_HEAP_BASE,
		.size = ROGUE_VISTEST_HEAP_SIZE,
		.flags = 0,
		.page_size_log2 = PVR_DEVICE_PAGE_SHIFT,
	},
	[DRM_PVR_HEAP_TRANSFER_FRAG] = {
		.base = ROGUE_TRANSFER_FRAG_HEAP_BASE,
		.size = ROGUE_TRANSFER_FRAG_HEAP_SIZE,
		.flags = 0,
		.page_size_log2 = PVR_DEVICE_PAGE_SHIFT,
	},
};

int
pvr_static_data_areas_get(const struct pvr_device *pvr_dev,
			  struct drm_pvr_ioctl_dev_query_args *args)
{
	struct drm_pvr_dev_query_static_data_areas query = {0};
	int err;

	if (!args->pointer) {
		args->size = sizeof(struct drm_pvr_dev_query_static_data_areas);
		return 0;
	}

	err = PVR_UOBJ_GET(query, args->size, args->pointer);
	if (err < 0)
		return err;

	if (!query.static_data_areas.array) {
		query.static_data_areas.count = ARRAY_SIZE(static_data_areas);
		query.static_data_areas.stride = sizeof(struct drm_pvr_static_data_area);
		goto copy_out;
	}

	if (query.static_data_areas.count > ARRAY_SIZE(static_data_areas))
		query.static_data_areas.count = ARRAY_SIZE(static_data_areas);

	err = PVR_UOBJ_SET_ARRAY(&query.static_data_areas, static_data_areas);
	if (err < 0)
		return err;

copy_out:
	err = PVR_UOBJ_SET(args->pointer, args->size, query);
	if (err < 0)
		return err;

	args->size = sizeof(query);
	return 0;
}

int
pvr_heap_info_get(const struct pvr_device *pvr_dev,
		  struct drm_pvr_ioctl_dev_query_args *args)
{
	struct drm_pvr_dev_query_heap_info query = {0};
	u64 dest;
	int err;

	if (!args->pointer) {
		args->size = sizeof(struct drm_pvr_dev_query_heap_info);
		return 0;
	}

	err = PVR_UOBJ_GET(query, args->size, args->pointer);
	if (err < 0)
		return err;

	if (!query.heaps.array) {
		query.heaps.count = ARRAY_SIZE(pvr_heaps);
		query.heaps.stride = sizeof(struct drm_pvr_heap);
		goto copy_out;
	}

	if (query.heaps.count > ARRAY_SIZE(pvr_heaps))
		query.heaps.count = ARRAY_SIZE(pvr_heaps);

	/* Region header heap is only present if BRN63142 is present. */
	dest = query.heaps.array;
	for (size_t i = 0; i < query.heaps.count; i++) {
		struct drm_pvr_heap heap = pvr_heaps[i];

		if (i == DRM_PVR_HEAP_RGNHDR && !PVR_HAS_QUIRK(pvr_dev, 63142))
			heap.size = 0;

		err = PVR_UOBJ_SET(dest, query.heaps.stride, heap);
		if (err < 0)
			return err;

		dest += query.heaps.stride;
	}

copy_out:
	err = PVR_UOBJ_SET(args->pointer, args->size, query);
	if (err < 0)
		return err;

	args->size = sizeof(query);
	return 0;
}

const struct drm_pvr_static_data_area *
pvr_static_data_areas(u32 *count)
{
	*count = ARRAY_SIZE(static_data_areas);
	return static_data_areas;
}

// the heap table as the dev query returns it: the region header heap only with brn63142
u32
pvr_heaps_get(const struct pvr_device *pvr_dev, struct drm_pvr_heap *out, u32 max)
{
	u32 n = min_t(u32, max, ARRAY_SIZE(pvr_heaps));

	for (u32 i = 0; i < n; i++) {
		out[i] = pvr_heaps[i];
		if (i == DRM_PVR_HEAP_RGNHDR && !PVR_HAS_QUIRK(pvr_dev, 63142))
			out[i].size = 0;
	}
	return ARRAY_SIZE(pvr_heaps);
}

static __always_inline bool
pvr_heap_contains_range(const struct drm_pvr_heap *pvr_heap, u64 start, u64 end)
{
	return pvr_heap->base <= start && end < pvr_heap->base + pvr_heap->size;
}

const struct drm_pvr_heap *
pvr_find_heap_containing(struct pvr_device *pvr_dev, u64 start, u64 size)
{
	u64 end;

	if (check_add_overflow(start, size - 1, &end))
		return NULL;

	/*
	 * There are no guarantees about the order of address ranges in
	 * &pvr_heaps, so iterate over the entire array for a heap whose
	 * range completely encompasses the given range.
	 */
	for (u32 heap_id = 0; heap_id < ARRAY_SIZE(pvr_heaps); heap_id++) {
		/* Filter heaps that present only with an associated quirk */
		if (heap_id == DRM_PVR_HEAP_RGNHDR &&
		    !PVR_HAS_QUIRK(pvr_dev, 63142)) {
			continue;
		}

		if (pvr_heap_contains_range(&pvr_heaps[heap_id], start, end))
			return &pvr_heaps[heap_id];
	}

	return NULL;
}

struct pvr_gem_object *
pvr_vm_find_gem_object(struct pvr_vm_context *vm_ctx, u64 device_addr,
		       u64 *mapped_offset_out, u64 *mapped_size_out)
{
	struct pvr_vm_mapping *m;

	mutex_lock(&vm_ctx->lock);

	list_for_each_entry(m, &vm_ctx->mappings, node) {
		if (device_addr >= m->device_addr && device_addr < m->device_addr + m->size) {
			struct pvr_gem_object *pvr_obj = m->pvr_obj;

			pvr_gem_object_get(pvr_obj);
			if (mapped_offset_out)
				*mapped_offset_out = m->offset;
			if (mapped_size_out)
				*mapped_size_out = m->size;
			mutex_unlock(&vm_ctx->lock);
			return pvr_obj;
		}
	}

	mutex_unlock(&vm_ctx->lock);

	return NULL;
}

struct pvr_fw_object *
pvr_vm_get_fw_mem_context(struct pvr_vm_context *vm_ctx)
{
	return vm_ctx->fw_mem_ctx_obj;
}

struct pvr_vm_context *
pvr_vm_context_lookup(struct pvr_file *pvr_file, u32 handle)
{
	struct pvr_vm_context *vm_ctx;

	xa_lock(&pvr_file->vm_ctx_handles);
	vm_ctx = xa_load(&pvr_file->vm_ctx_handles, handle);
	pvr_vm_context_get(vm_ctx);
	xa_unlock(&pvr_file->vm_ctx_handles);

	return vm_ctx;
}

void pvr_destroy_vm_contexts_for_file(struct pvr_file *pvr_file)
{
	struct pvr_vm_context *vm_ctx;
	unsigned long handle;

	xa_for_each(&pvr_file->vm_ctx_handles, handle, vm_ctx) {
		/* vm_ctx is not used here because that would create a race with xa_erase */
		pvr_vm_context_put(xa_erase(&pvr_file->vm_ctx_handles, handle));
	}
}
