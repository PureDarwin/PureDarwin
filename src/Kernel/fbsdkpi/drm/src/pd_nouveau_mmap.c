// SPDX-License-Identifier: MIT
// the physical pages behind a nouveau gem mmap offset for /dev/dri, vram through BAR 1, gart as ttm pages

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_tt.h>

#include "nouveau_drv.h"
#include "nouveau_bo.h"
#include "nouveau_gem.h"

#include "../../pd_dri_cdev.h"

extern int (*pd_drm_mmap_pages)(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg);

// the pin and object reference one mapping holds until xnu drops its pager
struct pd_nouveau_mapping {
	struct work_struct work;
	struct nouveau_bo *nvbo;
};

// unpinning reserves the bo, which can sleep, so it runs off the vm teardown path
static void
pd_nouveau_unmap_work(struct work_struct *work)
{
	struct pd_nouveau_mapping *map = container_of(work, struct pd_nouveau_mapping, work);

	nouveau_bo_unpin(map->nvbo);
	drm_gem_object_put(&map->nvbo->bo.base);
	kfree(map);
}

static void
pd_nouveau_unmap(void *arg)
{
	struct pd_nouveau_mapping *map = arg;

	INIT_WORK(&map->work, pd_nouveau_unmap_work);
	schedule_work(&map->work);
}

static int
pd_nouveau_mmap_pages(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_vma_offset_node *node;
	struct drm_gem_object *obj = NULL;
	struct pd_nouveau_mapping *map;
	struct nouveau_bo *nvbo;
	struct ttm_resource *res;
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

	// xnu maps the pages once, so the buffer stays where it is while mapped
	nvbo = nouveau_gem_object(obj);
	domain = nvbo->bo.resource->mem_type == TTM_PL_VRAM ?
	    NOUVEAU_GEM_DOMAIN_VRAM : NOUVEAU_GEM_DOMAIN_GART;
	ret = nouveau_bo_pin(nvbo, domain, true);
	if (ret != 0)
		goto out;

	ret = ttm_bo_reserve(&nvbo->bo, false, false, NULL);
	if (ret != 0)
		goto unpin;
	res = nvbo->bo.resource;
	if (res->mem_type == TTM_PL_VRAM) {
		// pinned contiguous, ttm's io reservation puts it in BAR 1
		ret = ttm_mem_io_reserve(nvbo->bo.bdev, res);
		if (ret == 0 && !res->bus.is_iomem)
			ret = -EINVAL;
		for (i = 0; ret == 0 && i < npages; i++)
			pages[i] = (uint32_t)((res->bus.offset + (i << PAGE_SHIFT)) >> PAGE_SHIFT);
		*wimg = PD_DRI_WIMG_WCOMB;
	} else {
		struct ttm_tt *tt = nvbo->bo.ttm;

		if (tt == NULL || tt->num_pages < npages) {
			ret = -EINVAL;
		} else {
			for (i = 0; i < npages; i++)
				pages[i] = (uint32_t)(page_to_phys(tt->pages[i]) >> PAGE_SHIFT);
			*wimg = tt->caching == ttm_cached ? PD_DRI_WIMG_DEFAULT : PD_DRI_WIMG_WCOMB;
		}
	}
	ttm_bo_unreserve(&nvbo->bo);
	if (ret != 0)
		goto unpin;

	// the pin and the reference stay until the pager goes
	map->nvbo = nvbo;
	*release = pd_nouveau_unmap;
	*release_arg = map;
	return 0;
unpin:
	nouveau_bo_unpin(nvbo);
out:
	kfree(map);
	drm_gem_object_put(obj);
	return ret;
}

void pd_nouveau_mmap_register(void);

void
pd_nouveau_mmap_register(void)
{
	pd_drm_mmap_pages = pd_nouveau_mmap_pages;
}
