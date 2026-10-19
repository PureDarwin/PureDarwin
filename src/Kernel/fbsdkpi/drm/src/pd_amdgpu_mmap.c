// SPDX-License-Identifier: MIT
// the physical pages behind a gem mmap offset for /dev/dri, mapped once at mmap time
// vram in the cpu visible window (BAR 0), gtt as its ttm pages
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>
#include <drm/ttm/ttm_tt.h>

#include "amdgpu.h"
#include "amdgpu_object.h"
#include "amdgpu_res_cursor.h"

#include "../../pd_dri_cdev.h"

extern int (*pd_drm_mmap_pages)(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg);

// the pin and object reference one mapping holds until xnu drops its pager
struct pd_amdgpu_mapping {
	struct work_struct work;
	struct amdgpu_bo *abo;
};

// unpinning reserves the bo, which can sleep, so it runs off the vm teardown path
static void
pd_amdgpu_unmap_work(struct work_struct *work)
{
	struct pd_amdgpu_mapping *map = container_of(work, struct pd_amdgpu_mapping, work);
	struct amdgpu_bo *abo = map->abo;

	if (amdgpu_bo_reserve(abo, true) == 0) {
		amdgpu_bo_unpin(abo);
		amdgpu_bo_unreserve(abo);
	}
	drm_gem_object_put(&abo->tbo.base);
	kfree(map);
}

static void
pd_amdgpu_unmap(void *arg)
{
	struct pd_amdgpu_mapping *map = arg;

	INIT_WORK(&map->work, pd_amdgpu_unmap_work);
	schedule_work(&map->work);
}

static int
pd_amdgpu_mmap_pages(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct drm_vma_offset_node *node;
	struct drm_gem_object *obj = NULL;
	struct pd_amdgpu_mapping *map;
	struct amdgpu_bo *abo;
	unsigned long npages = size >> PAGE_SHIFT, i;
	u32 domain;
	int ret;

	drm_vma_offset_lock_lookup(dev->vma_offset_manager);
	node = drm_vma_offset_exact_lookup_locked(dev->vma_offset_manager,
	    offset >> PAGE_SHIFT, npages);
	if (node != NULL) {
		obj = container_of(node, struct drm_gem_object, vma_node);
		if (!kref_get_unless_zero(&obj->refcount))
			obj = NULL;
	}
	drm_vma_offset_unlock_lookup(dev->vma_offset_manager);
	if (obj == NULL)
		return -EINVAL;
	if (!drm_vma_node_is_allowed(node, priv)) {
		drm_gem_object_put(obj);
		return -EACCES;
	}
	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (map == NULL) {
		drm_gem_object_put(obj);
		return -ENOMEM;
	}

	abo = gem_to_amdgpu_bo(obj);
	ret = amdgpu_bo_reserve(abo, false);
	if (ret != 0)
		goto out;
	// a vram buffer the cpu maps has to sit in the BAR window
	domain = abo->tbo.resource->mem_type == TTM_PL_VRAM ?
	    AMDGPU_GEM_DOMAIN_VRAM : AMDGPU_GEM_DOMAIN_GTT;
	if (domain == AMDGPU_GEM_DOMAIN_VRAM)
		abo->flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
	ret = amdgpu_bo_pin(abo, domain);
	if (ret != 0) {
		amdgpu_bo_unreserve(abo);
		goto out;
	}

	if (abo->tbo.resource->mem_type == TTM_PL_VRAM) {
		struct amdgpu_res_cursor cursor;

		// vram need not be contiguous, so walk its blocks
		amdgpu_res_first(abo->tbo.resource, 0, size, &cursor);
		for (i = 0; i < npages && cursor.remaining != 0; i++) {
			pages[i] = (uint32_t)((adev->gmc.aper_base + cursor.start) >> PAGE_SHIFT);
			amdgpu_res_next(&cursor, PAGE_SIZE);
		}
		if (i != npages)
			ret = -EINVAL;
		*wimg = PD_DRI_WIMG_WCOMB;
	} else {
		struct ttm_tt *tt = abo->tbo.ttm;

		if (tt == NULL || tt->num_pages < npages) {
			ret = -EINVAL;
		} else {
			for (i = 0; i < npages; i++)
				pages[i] = (uint32_t)(page_to_phys(tt->pages[i]) >> PAGE_SHIFT);
			*wimg = tt->caching == ttm_cached ? PD_DRI_WIMG_DEFAULT : PD_DRI_WIMG_WCOMB;
		}
	}
	if (ret != 0)
		amdgpu_bo_unpin(abo);
	amdgpu_bo_unreserve(abo);
	if (ret != 0)
		goto out;

	// the pin and the reference stay until the pager goes
	map->abo = abo;
	*release = pd_amdgpu_unmap;
	*release_arg = map;
	return 0;
out:
	kfree(map);
	drm_gem_object_put(obj);
	return ret;
}

void pd_amdgpu_mmap_register(void);

void
pd_amdgpu_mmap_register(void)
{
	pd_drm_mmap_pages = pd_amdgpu_mmap_pages;
}
