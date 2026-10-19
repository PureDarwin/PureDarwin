// SPDX-License-Identifier: MIT
// scanout buffers for the drm display client on nouveau, pinned contiguous in vram and reached through BAR 1

#include <linux/err.h>
#include <linux/iosys-map.h>
#include <linux/slab.h>

#include <drm/drm_client.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_print.h>
#include <drm/ttm/ttm_bo.h>

#include "nouveau_drv.h"
#include "nouveau_bo.h"
#include "nouveau_gem.h"

struct drm_client_buffer *pd_drm_fb_alloc_pinned(struct drm_client_dev *client,
    u32 width, u32 height, u64 *phys);
void pd_drm_fb_free_pinned(struct drm_client_buffer *buf);
void pd_amdgpu_fb_register(struct drm_device *dev);
void pd_nouveau_fb_register(struct drm_device *dev);

struct drm_client_buffer *
pd_drm_fb_alloc_pinned(struct drm_client_dev *client, u32 width, u32 height, u64 *phys)
{
	struct drm_client_buffer *buf;
	struct nouveau_bo *nvbo;
	struct ttm_resource *res;
	struct iosys_map map;
	int ret;

	buf = drm_client_framebuffer_create(client, width, height, DRM_FORMAT_XRGB8888);
	if (IS_ERR(buf))
		return buf;

	// dumb buffers are linear vram, the pin keeps them in one place for the display and the cpu
	nvbo = nouveau_gem_object(buf->gem);
	ret = nouveau_bo_pin(nvbo, NOUVEAU_GEM_DOMAIN_VRAM, true);
	if (ret != 0) {
		drm_err(client->dev, "pdfb: pin in vram failed %d\n", ret);
		drm_client_framebuffer_delete(buf);
		return ERR_PTR(ret);
	}
	ret = ttm_bo_reserve(&nvbo->bo, false, false, NULL);
	if (ret == 0) {
		res = nvbo->bo.resource;
		ret = ttm_mem_io_reserve(nvbo->bo.bdev, res);
		if (ret == 0 && !res->bus.is_iomem)
			ret = -EINVAL;
		if (ret == 0)
			*phys = res->bus.offset;
		ttm_bo_unreserve(&nvbo->bo);
	}
	if (ret != 0) {
		drm_err(client->dev, "pdfb: no BAR 1 mapping for the buffer %d\n", ret);
		nouveau_bo_unpin(nvbo);
		drm_client_framebuffer_delete(buf);
		return ERR_PTR(ret);
	}
	if (drm_client_buffer_vmap(buf, &map) == 0)
		iosys_map_memset(&map, 0, 0, (size_t)buf->fb->pitches[0] * height);

	drm_info(client->dev, "pdfb: %ux%u buffer at vram 0x%llx, cpu 0x%llx\n", width, height,
	    (unsigned long long)nvbo->offset, (unsigned long long)*phys);
	return buf;
}

void
pd_drm_fb_free_pinned(struct drm_client_buffer *buf)
{
	nouveau_bo_unpin(nouveau_gem_object(buf->gem));
	drm_client_framebuffer_delete(buf);
}

// called once nouveau has registered its drm device, a gpu without heads gets no client
void
pd_nouveau_fb_register(struct drm_device *dev)
{
	if (dev->mode_config.num_crtc == 0)
		return;
	pd_amdgpu_fb_register(dev);
}
