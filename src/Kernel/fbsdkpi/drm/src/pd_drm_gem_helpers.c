// SPDX-License-Identifier: MIT
// GEM framebuffer, atomic and TTM helpers for the calls amdgpu makes
// each is a thin step over the MIT drm and ttm cores
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-resv.h>
#include <linux/slab.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_ttm_helper.h>
#include <drm/drm_plane.h>
#include <drm/ttm/ttm_bo.h>

struct drm_gem_object *
drm_gem_fb_get_obj(struct drm_framebuffer *fb, unsigned int plane)
{
	if (plane >= ARRAY_SIZE(fb->obj))
		return NULL;
	return fb->obj[plane];
}

void
drm_gem_fb_destroy(struct drm_framebuffer *fb)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(fb->obj); i++)
		drm_gem_object_put(fb->obj[i]);
	drm_framebuffer_cleanup(fb);
	kfree(fb);
}

// an explicit fence from userspace stays, the implicit one is dropped then
static void
pd_plane_set_fence(struct drm_plane_state *state, struct dma_fence *fence)
{
	if (state->fence != NULL) {
		dma_fence_put(fence);
		return;
	}
	state->fence = fence;
}

int
drm_gem_fb_create_handle(struct drm_framebuffer *fb, struct drm_file *file,
    unsigned int *handle)
{
	return drm_gem_handle_create(file, fb->obj[0], handle);
}

int
drm_gem_plane_helper_prepare_fb(struct drm_plane *plane,
    struct drm_plane_state *state)
{
	struct dma_fence *fences[ARRAY_SIZE(state->fb->obj)];
	struct dma_fence_array *array;
	struct dma_fence *fence;
	unsigned int i, n = 0;
	int ret;

	if (state->fb == NULL)
		return 0;

	// the plane waits for every pending write to the buffers it scans out
	for (i = 0; i < ARRAY_SIZE(state->fb->obj); i++) {
		struct drm_gem_object *obj = state->fb->obj[i];

		if (obj == NULL)
			continue;
		ret = dma_resv_get_singleton(obj->resv, DMA_RESV_USAGE_WRITE,
		    &fence);
		if (ret != 0)
			goto fail;
		if (fence != NULL)
			fences[n++] = fence;
	}
	if (n == 0)
		return 0;
	if (n == 1) {
		pd_plane_set_fence(state, fences[0]);
		return 0;
	}
	array = dma_fence_array_create(n, kmemdup(fences, n * sizeof(fences[0]),
	    GFP_KERNEL), dma_fence_context_alloc(1), 1, false);
	if (array == NULL) {
		ret = -ENOMEM;
		goto fail;
	}
	pd_plane_set_fence(state, &array->base);
	return 0;
fail:
	while (n > 0)
		dma_fence_put(fences[--n]);
	return ret;
}

int
drm_gem_ttm_vmap(struct drm_gem_object *gem, struct iosys_map *map)
{
	return ttm_bo_vmap(drm_gem_ttm_of_gem(gem), map);
}

void
drm_gem_ttm_vunmap(struct drm_gem_object *gem, struct iosys_map *map)
{
	ttm_bo_vunmap(drm_gem_ttm_of_gem(gem), map);
}

int
drm_gem_ttm_mmap(struct drm_gem_object *gem, struct vm_area_struct *vma)
{
	int ret;

	ret = ttm_bo_mmap_obj(vma, drm_gem_ttm_of_gem(gem));
	if (ret < 0)
		return ret;
	// ttm_bo_mmap_obj took its own reference, drop the one drm_gem_mmap took
	drm_gem_object_put(gem);
	return 0;
}

// the fake mmap offset of a ttm object, for drivers whose dumb buffers are ttm objects
int
drm_gem_ttm_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
    uint32_t handle, uint64_t *offset)
{
	struct drm_gem_object *gem;

	gem = drm_gem_object_lookup(file, handle);
	if (gem == NULL)
		return -ENOENT;
	*offset = drm_vma_node_offset_addr(&gem->vma_node);
	drm_gem_object_put(gem);
	return 0;
}
