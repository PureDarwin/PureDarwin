// SPDX-License-Identifier: MIT
// the physical pages behind an i915 mmap offset for /dev/dri

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <drm/drm_cache.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>

#include "i915_drv.h"
#include "i915_gem.h"
#include "i915_vma.h"
#include "gem/i915_gem_object.h"
#include "gt/intel_gt.h"

#include "../../pd_dri_cdev.h"

extern int (*pd_drm_mmap_pages)(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg);
void pd_i915_mmap_register(void);

// what one mapping holds until xnu drops its pager: the pages or the ggtt pin, and the object
struct pd_i915_mapping {
	struct work_struct work;
	struct drm_i915_gem_object *obj;
	struct i915_vma *vma;
};

// unpinning takes the object lock, which can sleep, so it runs off the vm teardown path
static void
pd_i915_unmap_work(struct work_struct *work)
{
	struct pd_i915_mapping *map = container_of(work, struct pd_i915_mapping, work);

	if (map->vma != NULL)
		i915_vma_unpin(map->vma);
	else
		i915_gem_object_unpin_pages(map->obj);
	i915_gem_object_put(map->obj);
	kfree(map);
}

static void
pd_i915_unmap(void *arg)
{
	struct pd_i915_mapping *map = arg;

	INIT_WORK(&map->work, pd_i915_unmap_work);
	schedule_work(&map->work);
}

static int
pd_i915_mmap_pages(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_i915_private *i915 = to_i915(dev);
	struct drm_vma_offset_node *node;
	struct i915_mmap_offset *mmo = NULL;
	struct drm_i915_gem_object *obj = NULL;
	struct pd_i915_mapping *map;
	unsigned long npages = size >> PAGE_SHIFT, i;
	int ret = 0;

	drm_vma_offset_lock_lookup(dev->vma_offset_manager);
	node = drm_vma_offset_exact_lookup_locked(dev->vma_offset_manager,
	    offset >> PAGE_SHIFT, npages);
	if (node != NULL) {
		mmo = container_of(node, struct i915_mmap_offset, vma_node);
		obj = i915_gem_object_get_rcu(mmo->obj);
	}
	drm_vma_offset_unlock_lookup(dev->vma_offset_manager);
	if (obj == NULL)
		return -EINVAL;
	if (!drm_vma_node_is_allowed(node, priv)) {
		i915_gem_object_put(obj);
		return -EACCES;
	}
	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (map == NULL) {
		i915_gem_object_put(obj);
		return -ENOMEM;
	}
	map->obj = obj;

	// a linear object maps its own pages, a held ggtt pin would stop execbuffer moving it
	if (mmo->mmap_type == I915_MMAP_TYPE_GTT && i915_gem_object_is_tiled(obj)) {
		// through the aperture: the object is pinned in the mappable ggtt and the cpu
		// reaches it as one range, tiling and fences included
		struct i915_ggtt *ggtt = to_gt(i915)->ggtt;
		struct i915_vma *vma = i915_gem_object_ggtt_pin(obj, NULL, 0, 0, PIN_MAPPABLE);

		if (IS_ERR(vma)) {
			ret = PTR_ERR(vma);
			goto out;
		}
		for (i = 0; i < npages; i++)
			pages[i] = (uint32_t)((ggtt->gmadr.start + i915_ggtt_offset(vma) +
			    ((u64)i << PAGE_SHIFT)) >> PAGE_SHIFT);
		map->vma = vma;
		*wimg = PD_DRI_WIMG_WCOMB;
	} else if (mmo->mmap_type == I915_MMAP_TYPE_FIXED) {
		ret = -ENODEV;
		goto out;
	} else {
		// the object's own pages, cached for wb and write-combined otherwise
		struct sgt_iter iter;
		struct page *page;

		ret = i915_gem_object_pin_pages_unlocked(obj);
		if (ret != 0)
			goto out;
		i = 0;
		for_each_sgt_page(page, iter, obj->mm.pages) {
			if (i >= npages)
				break;
			pages[i++] = (uint32_t)(page_to_phys(page) >> PAGE_SHIFT);
		}
		if (i != npages) {
			i915_gem_object_unpin_pages(obj);
			ret = -EINVAL;
			goto out;
		}
		*wimg = mmo->mmap_type == I915_MMAP_TYPE_WB ? PD_DRI_WIMG_DEFAULT : PD_DRI_WIMG_WCOMB;
	}

	// the pin and the reference stay until the pager goes
	*release = pd_i915_unmap;
	*release_arg = map;
	return 0;
out:
	kfree(map);
	i915_gem_object_put(obj);
	return ret;
}

void
pd_i915_mmap_register(void)
{
	pd_drm_mmap_pages = pd_i915_mmap_pages;
}
