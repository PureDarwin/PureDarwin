// pd_nvidia_fb.c - nvidia-drm's device probe here, and the display client's scanout buffers in nvkms video memory
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <linux/err.h>
#include <linux/iosys-map.h>
#include <drm/drm_client.h>
#include <drm/drm_drv.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_print.h>

#include "nvidia-drm-priv.h"
#include "nvidia-drm-drv.h"
#include "nvidia-drm-gem.h"
#include "nvidia-drm-gem-nvkms-memory.h"

// the pre-include sends nvidia-drm's call here, this file reaches the real one
#undef drm_dev_register
int drm_dev_register(struct drm_device *dev, unsigned long flags);

struct drm_client_buffer *pd_drm_fb_alloc_pinned(struct drm_client_dev *client,
    u32 width, u32 height, u64 *phys);
void pd_drm_fb_free_pinned(struct drm_client_buffer *buf);
void pd_amdgpu_fb_register(struct drm_device *dev);
int pd_nvdrm_dev_register(struct drm_device *dev, unsigned long flags);

// as the linux build's: each GPU the modeset core knows gets its drm device
int
nv_drm_probe_devices(void)
{
	nv_drm_update_drm_driver_features();
	if (nvKms->enumerateGpus(nv_drm_register_drm_device) == 0)
		NV_DRM_LOG_INFO("No NVIDIA GPUs found");
	return 0;
}

// registered, then given to the display client behind PDDRMFramebuffer when it has heads
int
pd_nvdrm_dev_register(struct drm_device *dev, unsigned long flags)
{
	struct nv_drm_device *nv_dev = to_nv_device(dev);
	int ret = drm_dev_register(dev, flags);

	if (ret != 0 || !drm_core_check_feature(dev, DRIVER_MODESET) || dev->mode_config.num_crtc == 0)
		return ret;
	// the display client is no drm master, so it takes modeset ownership as linux's fbdev console does
	if (!nv_dev->hasFramebufferConsole) {
		if (!nvKms->grabOwnership(nv_dev->pDevice)) {
			NV_DRM_DEV_LOG_ERR(nv_dev, "pdfb: no modeset ownership, no display client");
			return ret;
		}
		nv_dev->hasFramebufferConsole = NV_TRUE;
	}
	nvKms->framebufferConsoleDisabled(nv_dev->pDevice);
	pd_amdgpu_fb_register(dev);
	return ret;
}

// a dumb buffer is linear nvkms video memory that never moves, its vmap maps it through BAR 1
struct drm_client_buffer *
pd_drm_fb_alloc_pinned(struct drm_client_dev *client, u32 width, u32 height, u64 *phys)
{
	struct drm_client_buffer *buf;
	struct nv_drm_gem_nvkms_memory *mem;
	struct iosys_map map;

	buf = drm_client_framebuffer_create(client, width, height, DRM_FORMAT_XRGB8888);
	if (IS_ERR(buf))
		return buf;
	if (drm_client_buffer_vmap(buf, &map) != 0) {
		drm_err(client->dev, "pdfb: the buffer does not map\n");
		drm_client_framebuffer_delete(buf);
		return ERR_PTR(-ENOMEM);
	}
	mem = to_nv_nvkms_memory(to_nv_gem_object(buf->gem));
	if (!mem->physically_mapped || mem->pPhysicalAddress == NULL) {
		drm_err(client->dev, "pdfb: the buffer is not in video memory\n");
		drm_client_buffer_vunmap(buf);
		drm_client_framebuffer_delete(buf);
		return ERR_PTR(-EINVAL);
	}
	*phys = (u64)(uintptr_t)mem->pPhysicalAddress;
	iosys_map_memset(&map, 0, 0, (size_t)buf->fb->pitches[0] * height);
	drm_info(client->dev, "pdfb: %ux%u buffer at cpu 0x%llx\n", width, height, (unsigned long long)*phys);
	return buf;
}

void
pd_drm_fb_free_pinned(struct drm_client_buffer *buf)
{
	drm_client_framebuffer_delete(buf);
}
