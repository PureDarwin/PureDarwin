// SPDX-License-Identifier: MIT
// scanout buffers for the drm display client on radeon, pinned in cpu visible vram

#include <linux/err.h>
#include <linux/iosys-map.h>

#include <drm/drm_client.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_print.h>

#include "radeon.h"
#include "radeon_object.h"

struct drm_client_buffer *pd_drm_fb_alloc_pinned(struct drm_client_dev *client,
    u32 width, u32 height, u64 *phys);
void pd_drm_fb_free_pinned(struct drm_client_buffer *buf);
void pd_amdgpu_fb_register(struct drm_device *dev);
void pd_radeon_fb_register(struct drm_device *dev);

struct drm_client_buffer *
pd_drm_fb_alloc_pinned(struct drm_client_dev *client, u32 width, u32 height, u64 *phys)
{
	struct radeon_device *rdev = client->dev->dev_private;
	struct drm_client_buffer *buf;
	struct radeon_bo *rbo;
	struct iosys_map map;
	u64 gpu = 0, offset;
	int ret;

	buf = drm_client_framebuffer_create(client, width, height, DRM_FORMAT_XRGB8888);
	if (IS_ERR(buf))
		return buf;

	rbo = gem_to_radeon_bo(buf->gem);
	ret = radeon_bo_reserve(rbo, false);
	if (ret == 0) {
		ret = radeon_bo_pin(rbo, RADEON_GEM_DOMAIN_VRAM, &gpu);
		radeon_bo_unreserve(rbo);
	}
	if (ret != 0) {
		drm_err(client->dev, "pdfb: pin in vram failed %d\n", ret);
		drm_client_framebuffer_delete(buf);
		return ERR_PTR(ret);
	}
	offset = gpu - rdev->mc.vram_start;
	if (offset + (u64)buf->fb->pitches[0] * height > rdev->mc.visible_vram_size) {
		drm_err(client->dev, "pdfb: buffer at 0x%llx is past visible vram\n",
		    (unsigned long long)offset);
		pd_drm_fb_free_pinned(buf);
		return ERR_PTR(-ENOSPC);
	}
	*phys = rdev->mc.aper_base + offset;
	if (drm_client_buffer_vmap(buf, &map) == 0)
		iosys_map_memset(&map, 0, 0, (size_t)buf->fb->pitches[0] * height);

	drm_info(client->dev, "pdfb: %ux%u buffer at vram 0x%llx, cpu 0x%llx\n", width, height,
	    (unsigned long long)offset, (unsigned long long)*phys);
	return buf;
}

void
pd_drm_fb_free_pinned(struct drm_client_buffer *buf)
{
	struct radeon_bo *rbo = gem_to_radeon_bo(buf->gem);

	if (radeon_bo_reserve(rbo, true) == 0) {
		radeon_bo_unpin(rbo);
		radeon_bo_unreserve(rbo);
	}
	drm_client_framebuffer_delete(buf);
}

// called once radeon has registered its drm device, a gpu without heads gets no client
void
pd_radeon_fb_register(struct drm_device *dev)
{
	if (dev->mode_config.num_crtc == 0)
		return;
	pd_amdgpu_fb_register(dev);
}
