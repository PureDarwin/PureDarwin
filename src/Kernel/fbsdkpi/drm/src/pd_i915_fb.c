// SPDX-License-Identifier: MIT
// scanout buffers for the drm display client on i915, pinned in the mappable ggtt

#include <linux/err.h>
#include <linux/iosys-map.h>
#include <linux/sizes.h>
#include <linux/slab.h>

#include <drm/drm_client.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_print.h>

#include "i915_drv.h"
#include "i915_gem.h"
#include "i915_vma.h"
#include "gem/i915_gem_domain.h"
#include "gem/i915_gem_object.h"
#include "gt/intel_gt.h"

struct drm_client_buffer *pd_drm_fb_alloc_pinned(struct drm_client_dev *client,
    u32 width, u32 height, u64 *phys);
void pd_drm_fb_free_pinned(struct drm_client_buffer *buf);
void pd_amdgpu_fb_register(struct drm_device *dev);
void pd_i915_fb_register(struct drm_device *dev);

// a pinned buffer and the ggtt vma holding it in place
#define PD_I915_FB_MAX	4
static struct {
	struct drm_client_buffer *buf;
	struct i915_vma *vma;
} pd_i915_pins[PD_I915_FB_MAX];

struct drm_client_buffer *
pd_drm_fb_alloc_pinned(struct drm_client_dev *client, u32 width, u32 height, u64 *phys)
{
	struct drm_i915_private *i915 = to_i915(client->dev);
	struct i915_ggtt *ggtt = to_gt(i915)->ggtt;
	struct drm_client_buffer *buf;
	struct i915_vma *vma;
	struct iosys_map map;
	int slot;

	for (slot = 0; slot < PD_I915_FB_MAX && pd_i915_pins[slot].buf != NULL; slot++)
		;
	if (slot == PD_I915_FB_MAX)
		return ERR_PTR(-ENOSPC);

	buf = drm_client_framebuffer_create(client, width, height, DRM_FORMAT_XRGB8888);
	if (IS_ERR(buf))
		return buf;

	// the cache level scanout needs, set before the pin: changing it on a pinned vma
	// would need an unbind, and the display's own pin would wait on that forever
	{
		struct drm_i915_gem_object *obj = to_intel_bo(buf->gem);
		int err;

		i915_gem_object_lock(obj, NULL);
		err = i915_gem_object_set_cache_level(obj, HAS_WT(i915) ? I915_CACHE_WT : I915_CACHE_NONE);
		i915_gem_object_unlock(obj);
		if (err != 0) {
			drm_err(client->dev, "pdfb: scanout cache level failed %d\n", err);
			drm_client_framebuffer_delete(buf);
			return ERR_PTR(err);
		}
	}
	// 1 MB alignment covers what the display engine asks of a linear surface
	vma = i915_gem_object_ggtt_pin(to_intel_bo(buf->gem), NULL, 0, SZ_1M, PIN_MAPPABLE);
	if (IS_ERR(vma)) {
		drm_err(client->dev, "pdfb: pin in the mappable ggtt failed %ld\n", PTR_ERR(vma));
		drm_client_framebuffer_delete(buf);
		return ERR_CAST(vma);
	}
	if (drm_client_buffer_vmap(buf, &map) == 0)
		iosys_map_memset(&map, 0, 0, (size_t)buf->fb->pitches[0] * height);

	pd_i915_pins[slot].buf = buf;
	pd_i915_pins[slot].vma = vma;
	*phys = ggtt->gmadr.start + i915_ggtt_offset(vma);
	drm_info(client->dev, "pdfb: %ux%u buffer at ggtt 0x%x, cpu 0x%llx\n", width,
	    height, i915_ggtt_offset(vma), (unsigned long long)*phys);
	// the ptes the display fetches through, first and last page of the buffer
	{
		u64 __iomem *gsm = (u64 __iomem *)ggtt->gsm;
		u32 first = i915_ggtt_offset(vma) >> PAGE_SHIFT;
		u32 last = first + (u32)((buf->fb->pitches[0] * (u64)height - 1) >> PAGE_SHIFT);
		struct sg_table *pages = to_intel_bo(buf->gem)->mm.pages;
		drm_info(client->dev, "pdfb: ggtt pte %u = 0x%llx, pte %u = 0x%llx, first page phys 0x%llx dma 0x%llx\n",
		    first, (unsigned long long)readq(&gsm[first]), last,
		    (unsigned long long)readq(&gsm[last]),
		    pages != NULL ? (unsigned long long)page_to_phys(sg_page(pages->sgl)) : 0ULL,
		    pages != NULL ? (unsigned long long)sg_dma_address(pages->sgl) : 0ULL);
	}
	return buf;
}

void
pd_drm_fb_free_pinned(struct drm_client_buffer *buf)
{
	for (int slot = 0; slot < PD_I915_FB_MAX; slot++) {
		if (pd_i915_pins[slot].buf != buf)
			continue;
		i915_vma_unpin(pd_i915_pins[slot].vma);
		pd_i915_pins[slot].buf = NULL;
		pd_i915_pins[slot].vma = NULL;
	}
	drm_client_framebuffer_delete(buf);
}

// called once i915 has registered its drm device
void
pd_i915_fb_register(struct drm_device *dev)
{
	extern void pd_i915_mmap_register(void);
	extern void pd_i915_hangdump_test(struct drm_device *dev);

	pd_amdgpu_fb_register(dev);
	pd_i915_mmap_register();
	pd_i915_hangdump_test(dev);
}
