// SPDX-License-Identifier: MIT
// the display client behind PDDRMFramebuffer, one XRGB8888 buffer pinned in visible vram
// it drives the first connected display, switches modes and follows connector changes
#include <linux/iosys-map.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/dma-buf.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_client.h>
#include <drm/drm_drv.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_vblank.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modes.h>
#include <drm/drm_print.h>
#include <drm/drm_prime.h>
#include <drm/drm_gem.h>

#ifndef PD_DRM_FB_DRIVER_BUFFERS
#include "amdgpu.h"
#include "amdgpu_gem.h"
#include "amdgpu_object.h"
#endif

#include "pd_drm_fb.h"

#define PD_FB_MAX_MODES	64

struct pd_drm_fb_ops *pd_drm_fb;

static struct {
	struct mutex lock;
	struct drm_device *dev;
	struct drm_client_dev client;
	bool client_ready;
	// drm_client_register calls hotplug at once, setup has just probed
	bool registering;
	struct drm_client_buffer *buf;
	// a second scanout buffer for page flipping, made when a presenter asks
	struct drm_client_buffer *back;
	u64 back_phys;
	// which of buf (0) and back (1) the crtc scans out
	int front;
	// the active display's modes, entry 0 is the one in use after setup
	struct drm_display_mode *modes[PD_FB_MAX_MODES];
	int nmodes;
	struct pd_drm_fb_info info;
} pdfb;

long pd_drm_tunable_long(const char *path, long def);

#ifdef PD_DRM_FB_DRIVER_BUFFERS
// another driver (i915) places scanout buffers its own way, see pd_i915_fb.c
struct drm_client_buffer *pd_drm_fb_alloc_pinned(struct drm_client_dev *client,
    u32 width, u32 height, u64 *phys);
void pd_drm_fb_free_pinned(struct drm_client_buffer *buf);

static void
pd_fb_free_buffer(struct drm_client_buffer *buf)
{
	if (buf != NULL)
		pd_drm_fb_free_pinned(buf);
}

static struct drm_client_buffer *
pd_fb_alloc_pinned(u32 width, u32 height, u64 *phys)
{
	return pd_drm_fb_alloc_pinned(&pdfb.client, width, height, phys);
}
#else
static void
pd_fb_free_buffer(struct drm_client_buffer *buf)
{
	struct amdgpu_bo *abo;

	if (buf == NULL)
		return;
	abo = gem_to_amdgpu_bo(buf->gem);
	if (amdgpu_bo_reserve(abo, true) == 0) {
		amdgpu_bo_unpin(abo);
		amdgpu_bo_unreserve(abo);
	}
	drm_client_framebuffer_delete(buf);
}

// a cleared XRGB8888 client buffer pinned in cpu visible vram, and where it is
static struct drm_client_buffer *
pd_fb_alloc_pinned(u32 width, u32 height, u64 *phys)
{
	struct amdgpu_device *adev = drm_to_adev(pdfb.dev);
	struct drm_client_buffer *buf;
	struct amdgpu_bo *abo;
	struct iosys_map map;
	u64 offset;
	int ret;

	buf = drm_client_framebuffer_create(&pdfb.client, width, height, DRM_FORMAT_XRGB8888);
	if (IS_ERR(buf))
		return buf;

	// scanout and the cpu aperture both need it in visible vram, for good
	abo = gem_to_amdgpu_bo(buf->gem);
	ret = amdgpu_bo_reserve(abo, false);
	if (ret != 0) {
		drm_client_framebuffer_delete(buf);
		return ERR_PTR(ret);
	}
	abo->flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
	ret = amdgpu_bo_pin(abo, AMDGPU_GEM_DOMAIN_VRAM);
	offset = ret == 0 ? amdgpu_bo_gpu_offset(abo) - adev->gmc.vram_start : 0;
	amdgpu_bo_unreserve(abo);
	if (ret != 0) {
		drm_err(pdfb.dev, "pdfb: pin in vram failed %d\n", ret);
		drm_client_framebuffer_delete(buf);
		return ERR_PTR(ret);
	}
	if (offset + (u64)buf->fb->pitches[0] * height > adev->gmc.visible_vram_size) {
		drm_err(pdfb.dev, "pdfb: buffer at 0x%llx is past visible vram\n", offset);
		pd_fb_free_buffer(buf);
		return ERR_PTR(-ENOSPC);
	}
	if (drm_client_buffer_vmap(buf, &map) == 0)
		iosys_map_memset(&map, 0, 0, (size_t)buf->fb->pitches[0] * height);
	*phys = adev->gmc.aper_base + offset;
	return buf;
}
#endif

static void
pd_fb_drop_back(void)
{
	pd_fb_free_buffer(pdfb.back);
	pdfb.back = NULL;
	pdfb.front = 0;
}

static void
pd_fb_forget_modes(void)
{
	int i;

	for (i = 0; i < pdfb.nmodes; i++)
		drm_mode_destroy(pdfb.dev, pdfb.modes[i]);
	pdfb.nmodes = 0;
}

// the first modeset that lights a connector, the one this client drives
static struct drm_mode_set *
pd_fb_active_modeset(void)
{
	struct drm_mode_set *modeset;

	drm_client_for_each_modeset(modeset, &pdfb.client) {
		if (modeset->mode != NULL && modeset->num_connectors > 0)
			return modeset;
	}
	return NULL;
}

// snapshots the active connector's modes, the current one first
static void
pd_fb_collect_modes(struct drm_mode_set *modeset)
{
	struct drm_connector *connector = modeset->connectors[0];
	struct drm_display_mode *mode;

	pd_fb_forget_modes();
	pdfb.modes[pdfb.nmodes++] = drm_mode_duplicate(pdfb.dev, modeset->mode);
	mutex_lock(&pdfb.dev->mode_config.mutex);
	list_for_each_entry(mode, &connector->modes, head) {
		if (pdfb.nmodes == PD_FB_MAX_MODES)
			break;
		if (drm_mode_match(mode, modeset->mode, DRM_MODE_MATCH_TIMINGS))
			continue;
		pdfb.modes[pdfb.nmodes++] = drm_mode_duplicate(pdfb.dev, mode);
	}
	mutex_unlock(&pdfb.dev->mode_config.mutex);
}

// puts a fresh pinned buffer on the active modeset at its mode and commits,
// the old buffer goes once the new one is on screen
static int
pd_fb_light(struct drm_mode_set *modeset)
{
	const struct drm_display_mode *mode = modeset->mode;
	struct drm_client_buffer *buf, *old = pdfb.buf;
	u64 phys;
	int ret;

	// a new size means new buffers, the flip pair is made again on demand
	pd_fb_drop_back();
	buf = pd_fb_alloc_pinned(mode->hdisplay, mode->vdisplay, &phys);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	modeset->fb = buf->fb;
	ret = drm_client_modeset_commit(&pdfb.client);
	if (ret != 0) {
		drm_err(pdfb.dev, "pdfb: commit %dx%d failed %d\n", mode->hdisplay,
		    mode->vdisplay, ret);
		modeset->fb = old != NULL ? old->fb : NULL;
		pd_fb_free_buffer(buf);
		return ret;
	}

	pdfb.buf = buf;
	pd_fb_free_buffer(old);
	pdfb.info.phys = phys;
	pdfb.info.width = mode->hdisplay;
	pdfb.info.height = mode->vdisplay;
	pdfb.info.pitch = buf->fb->pitches[0];
	pdfb.info.refresh = drm_mode_vrefresh(mode);
	drm_info(pdfb.dev, "pdfb: %ux%u@%u pitch %u at phys 0x%llx\n", pdfb.info.width,
	    pdfb.info.height, pdfb.info.refresh, pdfb.info.pitch,
	    (unsigned long long)pdfb.info.phys);
	return 0;
}

// probes, picks the preferred mode (or pdfb_mode=WxH) and lights it
static int
pd_fb_probe_and_light(void)
{
	struct drm_mode_set *modeset;
	struct drm_display_mode *mode;
	long want = pd_drm_tunable_long("pdfb.mode", 0);
	int ret;

	ret = drm_client_modeset_probe(&pdfb.client, 0, 0);
	if (ret != 0)
		return ret;
	modeset = pd_fb_active_modeset();
	if (modeset == NULL) {
		struct drm_connector_list_iter iter;
		struct drm_connector *connector;

		drm_info(pdfb.dev, "pdfb: no connected display\n");
		drm_connector_list_iter_begin(pdfb.dev, &iter);
		drm_for_each_connector_iter(connector, &iter)
			drm_info(pdfb.dev, "pdfb: %s status %d, %d modes\n", connector->name,
			    connector->status, list_count_nodes(&connector->modes));
		drm_connector_list_iter_end(&iter);
		return -ENODEV;
	}

	// boot-arg pdfb_mode=WWWWHHHH picks a listed mode, 12800720 is 1280x720
	if (want > 0) {
		struct drm_connector *connector = modeset->connectors[0];
		int w = want / 10000, h = want % 10000;

		mutex_lock(&pdfb.dev->mode_config.mutex);
		list_for_each_entry(mode, &connector->modes, head) {
			if (mode->hdisplay == w && mode->vdisplay == h) {
				drm_mode_destroy(pdfb.dev, modeset->mode);
				modeset->mode = drm_mode_duplicate(pdfb.dev, mode);
				break;
			}
		}
		mutex_unlock(&pdfb.dev->mode_config.mutex);
	}

	ret = pd_fb_light(modeset);
	if (ret == 0)
		pd_fb_collect_modes(modeset);
	return ret;
}

static int
pd_fb_hotplug(struct drm_client_dev *client)
{
	int ret;

	mutex_lock(&pdfb.lock);
	if (pdfb.buf == NULL || pdfb.registering) {
		mutex_unlock(&pdfb.lock);
		return 0;
	}
	// the probe rebuilds the modesets, the old buffer stays until the new commit
	ret = pd_fb_probe_and_light();
	mutex_unlock(&pdfb.lock);
	drm_info(pdfb.dev, "pdfb: connector change handled (%d)\n", ret);
	if (ret == 0 && pd_drm_fb->changed != NULL)
		pd_drm_fb->changed(pd_drm_fb->changed_context);
	return 0;
}

static const struct drm_client_funcs pd_fb_client_funcs = {
	.owner = THIS_MODULE,
	.hotplug = pd_fb_hotplug,
};

static int
pd_amdgpu_fb_setup(struct pd_drm_fb_info *info)
{
	int ret;

	bool registering = false;

	mutex_lock(&pdfb.lock);
	if (!pdfb.client_ready) {
		ret = drm_client_init(pdfb.dev, &pdfb.client, "pdfb", &pd_fb_client_funcs);
		if (ret != 0) {
			mutex_unlock(&pdfb.lock);
			return ret;
		}
		pdfb.client_ready = true;
		registering = true;
	}
	ret = pd_fb_probe_and_light();
	if (ret == 0)
		*info = pdfb.info;
	mutex_unlock(&pdfb.lock);

	// registering calls the hotplug hook at once, which takes the lock itself
	if (registering) {
		pdfb.registering = true;
		drm_client_register(&pdfb.client);
		pdfb.registering = false;
	}
	return ret;
}

static int
pd_amdgpu_fb_on_screen(struct pd_drm_fb_info *info)
{
	mutex_lock(&pdfb.lock);
	*info = pdfb.info;
	mutex_unlock(&pdfb.lock);
	return pdfb.buf != NULL ? 0 : -ENODEV;
}

static int
pd_amdgpu_fb_mode_count(void)
{
	return pdfb.nmodes;
}

static int
pd_amdgpu_fb_get_mode(int index, struct pd_drm_fb_mode *out)
{
	const struct drm_display_mode *mode;

	mutex_lock(&pdfb.lock);
	if (index < 0 || index >= pdfb.nmodes) {
		mutex_unlock(&pdfb.lock);
		return -EINVAL;
	}
	mode = pdfb.modes[index];
	out->width = mode->hdisplay;
	out->height = mode->vdisplay;
	out->refresh = drm_mode_vrefresh(mode);
	out->preferred = (mode->type & DRM_MODE_TYPE_PREFERRED) != 0;
	mutex_unlock(&pdfb.lock);
	return 0;
}

static int
pd_amdgpu_fb_set_mode(int index, struct pd_drm_fb_info *info)
{
	struct drm_mode_set *modeset;
	int ret;

	mutex_lock(&pdfb.lock);
	modeset = pd_fb_active_modeset();
	if (modeset == NULL || index < 0 || index >= pdfb.nmodes) {
		mutex_unlock(&pdfb.lock);
		return -EINVAL;
	}
	mutex_lock(&pdfb.client.modeset_mutex);
	drm_mode_destroy(pdfb.dev, modeset->mode);
	modeset->mode = drm_mode_duplicate(pdfb.dev, pdfb.modes[index]);
	mutex_unlock(&pdfb.client.modeset_mutex);
	ret = pd_fb_light(modeset);
	if (ret == 0)
		*info = pdfb.info;
	mutex_unlock(&pdfb.lock);
	return ret;
}

static void
pd_amdgpu_fb_release(void)
{
	struct drm_mode_set *modeset;

	mutex_lock(&pdfb.lock);
	if (pdfb.buf != NULL) {
		// dark first, so nothing scans out of the buffer being freed
		drm_client_modeset_dpms(&pdfb.client, DRM_MODE_DPMS_OFF);
		drm_client_for_each_modeset(modeset, &pdfb.client)
			modeset->fb = NULL;
		pd_fb_drop_back();
		pd_fb_free_buffer(pdfb.buf);
		pdfb.buf = NULL;
		drm_info(pdfb.dev, "pdfb: scanout buffer released\n");
	}
	pd_fb_forget_modes();
	mutex_unlock(&pdfb.lock);
}

// the flip pair: buffer 0 is the one the client lit, buffer 1 is made here
static int
pd_amdgpu_fb_buffer(int index, uint64_t *phys)
{
	int ret = 0;

	mutex_lock(&pdfb.lock);
	if (pdfb.buf == NULL || index < 0 || index > 1) {
		ret = -EINVAL;
	} else if (index == 0) {
		*phys = pdfb.info.phys;
	} else {
		if (pdfb.back == NULL) {
			struct drm_client_buffer *back = pd_fb_alloc_pinned(pdfb.info.width,
			    pdfb.info.height, &pdfb.back_phys);
			if (IS_ERR(back))
				ret = PTR_ERR(back);
			else
				pdfb.back = back;
		}
		if (ret == 0)
			*phys = pdfb.back_phys;
	}
	mutex_unlock(&pdfb.lock);
	return ret;
}

// scans out buffer index from the next vblank on, returns once the flip
// happened with that vblank's count and time
static int
pd_amdgpu_fb_flip(int index, uint64_t *sequence, uint64_t *nanoseconds)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state;
	struct drm_plane_state *plane_state;
	struct drm_mode_set *modeset;
	struct drm_framebuffer *fb;
	struct drm_crtc *crtc;
	ktime_t when;
	int ret;

	mutex_lock(&pdfb.lock);
	modeset = pd_fb_active_modeset();
	fb = index == 0 ? (pdfb.buf != NULL ? pdfb.buf->fb : NULL) :
	    (pdfb.back != NULL ? pdfb.back->fb : NULL);
	if (modeset == NULL || fb == NULL) {
		mutex_unlock(&pdfb.lock);
		return -EINVAL;
	}
	crtc = modeset->crtc;

	DRM_MODESET_LOCK_ALL_BEGIN(pdfb.dev, ctx, 0, ret);
	state = drm_atomic_state_alloc(pdfb.dev);
	if (state == NULL) {
		ret = -ENOMEM;
		goto unlock;
	}
	state->acquire_ctx = &ctx;
	plane_state = drm_atomic_get_plane_state(state, crtc->primary);
	if (IS_ERR(plane_state)) {
		ret = PTR_ERR(plane_state);
		goto put;
	}
	drm_atomic_set_fb_for_plane(plane_state, fb);
	// a blocking commit returns after flip_done, at the vblank that took it
	ret = drm_atomic_commit(state);
put:
	drm_atomic_state_put(state);
unlock:
	DRM_MODESET_LOCK_ALL_END(pdfb.dev, ctx, ret);

	if (ret == 0) {
		pdfb.front = index;
		*sequence = drm_crtc_vblank_count_and_time(crtc, &when);
		*nanoseconds = ktime_to_ns(when);
	}
	mutex_unlock(&pdfb.lock);
	return ret;
}

// the flip buffer as a dma-buf fd of the caller, amdgpu imports it back as the same bo
static int
pd_amdgpu_fb_export_fd(int index, int *fd)
{
	struct drm_client_buffer *buf;
	struct dma_buf *dmabuf;
	uint64_t phys;
	int ret;

	ret = pd_amdgpu_fb_buffer(index, &phys);
	if (ret != 0)
		return ret;
	mutex_lock(&pdfb.lock);
	buf = index == 0 ? pdfb.buf : pdfb.back;
	if (buf == NULL) {
		mutex_unlock(&pdfb.lock);
		return -EINVAL;
	}
	dmabuf = buf->gem->funcs->export != NULL ? buf->gem->funcs->export(buf->gem, O_RDWR) :
	    drm_gem_prime_export(buf->gem, O_RDWR);
	mutex_unlock(&pdfb.lock);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);
	ret = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (ret < 0) {
		dma_buf_put(dmabuf);
		return ret;
	}
	*fd = ret;
	return 0;
}

static struct pd_drm_fb_ops pd_amdgpu_fb_ops = {
	.setup = pd_amdgpu_fb_setup,
	.on_screen = pd_amdgpu_fb_on_screen,
	.mode_count = pd_amdgpu_fb_mode_count,
	.get_mode = pd_amdgpu_fb_get_mode,
	.set_mode = pd_amdgpu_fb_set_mode,
	.release = pd_amdgpu_fb_release,
	.buffer = pd_amdgpu_fb_buffer,
	.flip = pd_amdgpu_fb_flip,
	.export_fd = pd_amdgpu_fb_export_fd,
};

// called from amdgpu_pci_probe once the drm device is registered
void pd_amdgpu_fb_register(struct drm_device *dev);
void pd_amdgpu_mmap_register(void);

void
pd_amdgpu_fb_register(struct drm_device *dev)
{
	mutex_init(&pdfb.lock);
	pdfb.dev = dev;
	pd_drm_fb = &pd_amdgpu_fb_ops;
#ifndef PD_DRM_FB_DRIVER_BUFFERS
	pd_amdgpu_mmap_register();
#endif
}
