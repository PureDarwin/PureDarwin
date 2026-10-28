// pd_msm_bo.c - buffer objects: contiguous pieces joined by the smmu, gpu addresses and /dev/dri mmap
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/err.h>

#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>

#include "pd_fbsd_os.h"
#include "../../fbsdkpi/pd_dri_cdev.h"
#include "pd_msm.h"

extern int (*pd_drm_mmap_pages)(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg);

// cleans and invalidates a range of the cacheable view, so no line of it is ever written back
static void
dcache_civac(void *va, size_t size)
{
	uintptr_t p = (uintptr_t)va & ~(uintptr_t)63, end = (uintptr_t)va + size;

	for (; p < end; p += 64)
		__asm__ volatile("dc civac, %0" : : "r"(p) : "memory");
	__asm__ volatile("dsb sy" : : : "memory");
}

void *
pd_msm_dma_alloc(size_t size, u64 *phys, void **alloc)
{
	void *cpu;

	size = round_up(size, PAGE_SIZE);
	*alloc = pdos_malloc_contig(size, PAGE_SIZE, 0xffffffffull, phys);
	if (*alloc == NULL)
		return (NULL);
	// the gpu sees memory uncached, so the cpu works through an uncached view of its own
	dcache_civac(*alloc, size);
	cpu = pdos_map_phys(*phys, size, 1);
	if (cpu == NULL) {
		pdos_free_contig(*alloc, size);
		return (NULL);
	}
	memset(cpu, 0, size);
	return (cpu);
}

void
pd_msm_dma_free(void *cpu, void *alloc, size_t size)
{
	size = round_up(size, PAGE_SIZE);
	pdos_unmap_phys(cpu, size);
	pdos_free_contig(alloc, size);
}

static void
bo_free(struct drm_gem_object *obj)
{
	struct pd_msm_bo *bo = to_pd_msm_bo(obj);
	struct pd_msm_dev *mdev = to_pd_msm(obj->dev);

	if (bo->iova != 0)
		pd_msm_iommu_unmap(mdev, bo->iova, obj->size);
	drm_gem_free_mmap_offset(obj);
	drm_gem_object_release(obj);
	if (bo->cpu != NULL)
		pd_msm_dma_free(bo->cpu, bo->chunks[0].alloc, obj->size);
	else
		for (u32 i = 0; i < bo->nchunks; i++)
			pdos_free_contig(bo->chunks[i].alloc, bo->chunks[i].size);
	kvfree(bo->chunks);
	kfree(bo);
}

const struct drm_gem_object_funcs pd_msm_bo_funcs = {
	.free = bo_free,
};

// the largest piece tried, a fragmented memory gets halves down to single pages
#define CHUNK_MAX	(64 * 1024)

static int
bo_alloc_chunks(struct pd_msm_bo *bo, size_t size)
{
	size_t left = size, want = CHUNK_MAX;

	while (left > 0) {
		struct pd_msm_chunk *c = &bo->chunks[bo->nchunks];

		if (want > left)
			want = left;
		c->alloc = pdos_malloc_contig(want, PAGE_SIZE, 0xffffffffull, &c->phys);
		if (c->alloc == NULL) {
			if (want == PAGE_SIZE)
				return (-ENOMEM);
			want = (want / 2) & ~(size_t)(PAGE_SIZE - 1);
			if (want < PAGE_SIZE)
				want = PAGE_SIZE;
			continue;
		}
		// zeroed through the cacheable view, which then holds no line the gpu could lose
		memset(c->alloc, 0, want);
		dcache_civac(c->alloc, want);
		c->size = want;
		bo->nchunks++;
		left -= want;
	}
	return (0);
}

struct pd_msm_bo *
pd_msm_bo_new(struct drm_device *drm, size_t size, u32 flags)
{
	struct pd_msm_bo *bo;

	size = round_up(size, PAGE_SIZE);
	if (size == 0)
		return (ERR_PTR(-EINVAL));
	bo = kzalloc(sizeof(*bo), GFP_KERNEL);
	if (bo == NULL)
		return (ERR_PTR(-ENOMEM));
	bo->chunks = kvmalloc_array(size / PAGE_SIZE, sizeof(*bo->chunks), GFP_KERNEL);
	if (bo->chunks == NULL) {
		kfree(bo);
		return (ERR_PTR(-ENOMEM));
	}
	if (flags & PD_MSM_BO_KERNEL) {
		bo->cpu = pd_msm_dma_alloc(size, &bo->chunks[0].phys, &bo->chunks[0].alloc);
		bo->chunks[0].size = size;
		bo->nchunks = bo->cpu != NULL;
	} else if (bo_alloc_chunks(bo, size) != 0) {
		for (u32 i = 0; i < bo->nchunks; i++)
			pdos_free_contig(bo->chunks[i].alloc, bo->chunks[i].size);
		bo->nchunks = 0;
	}
	if (bo->nchunks == 0) {
		kvfree(bo->chunks);
		kfree(bo);
		return (ERR_PTR(-ENOMEM));
	}
	bo->flags = flags & ~PD_MSM_BO_KERNEL;
	bo->base.funcs = &pd_msm_bo_funcs;
	drm_gem_private_object_init(drm, &bo->base, size);
	if (drm_gem_create_mmap_offset(&bo->base) != 0) {
		drm_gem_object_put(&bo->base);
		return (ERR_PTR(-ENOMEM));
	}
	return (bo);
}

// a gpu address on first need: a simple rising allocator, freed space is not reused yet
int
pd_msm_bo_iova(struct pd_msm_dev *mdev, struct pd_msm_bo *bo)
{
	struct pd_msm_gpu *gpu = &mdev->gpu;
	size_t size = bo->base.size;
	int error = 0;

	mutex_lock(&gpu->lock);
	if (bo->iova == 0) {
		if (gpu->va_next + size > mdev->info->va_end || gpu->va_next + size < gpu->va_next) {
			error = -ENOSPC;
		} else {
			u64 off = 0;

			for (u32 i = 0; i < bo->nchunks && error == 0; i++) {
				error = pd_msm_iommu_map(mdev, gpu->va_next + off, bo->chunks[i].phys,
				    bo->chunks[i].size);
				off += bo->chunks[i].size;
			}
			if (error != 0)
				pd_msm_iommu_unmap(mdev, gpu->va_next, size);
			if (error == 0) {
				bo->iova = gpu->va_next;
				gpu->va_next += size;
			}
		}
	}
	mutex_unlock(&gpu->lock);
	return (error);
}

static void
mmap_release(void *arg)
{
	drm_gem_object_put(arg);
}

// the pages behind an mmap offset of /dev/dri: the bo's own, write-combined
static int
pd_msm_mmap_pages(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_vma_offset_node *node;
	struct drm_gem_object *obj = NULL;
	unsigned long npages = size >> PAGE_SHIFT;

	drm_vma_offset_lock_lookup(dev->vma_offset_manager);
	node = drm_vma_offset_exact_lookup_locked(dev->vma_offset_manager, offset >> PAGE_SHIFT, npages);
	if (node != NULL) {
		obj = container_of(node, struct drm_gem_object, vma_node);
		if (!kref_get_unless_zero(&obj->refcount))
			obj = NULL;
	}
	drm_vma_offset_unlock_lookup(dev->vma_offset_manager);
	if (obj == NULL)
		return (-EINVAL);
	if (!drm_vma_node_is_allowed(node, priv) || size > obj->size) {
		drm_gem_object_put(obj);
		return (-EACCES);
	}
	struct pd_msm_bo *bo = to_pd_msm_bo(obj);
	for (unsigned long i = 0, c = 0, in = 0; i < npages; i++, in++) {
		if (in == bo->chunks[c].size >> PAGE_SHIFT) {
			c++;
			in = 0;
		}
		pages[i] = (uint32_t)((bo->chunks[c].phys >> PAGE_SHIFT) + in);
	}
	*wimg = PD_DRI_WIMG_WCOMB;
	*release = mmap_release;
	*release_arg = obj;
	return (0);
}

void
pd_msm_mmap_register(void)
{
	pd_drm_mmap_pages = pd_msm_mmap_pages;
}
