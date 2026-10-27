// SPDX-License-Identifier: MIT
// the display client behind PDDRMFramebuffer, XRGB8888 buffers pinned in visible vram
// every connected display is lit with its own buffer and mode, and follows connector changes
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
#include <drm/drm_crtc.h>
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
#define PD_FB_MAX_SETS	8

struct pd_drm_fb_ops *pd_drm_fb;

// every amdgpu device that registered, an apu and a discrete card can both be present
#define PD_FB_MAX_DEVS	4
static struct drm_device *pd_fb_devs[PD_FB_MAX_DEVS];
// one display client per device, made on its first probe and registered once it lights
static struct drm_client_dev pd_fb_clients[PD_FB_MAX_DEVS];
static bool pd_fb_client_inited[PD_FB_MAX_DEVS];
static int pd_fb_ndevs;

// one IOFramebuffer's display: the crtcs that scan its buffer (one, or all of them when mirrored),
// its buffers and the modes of its connector
struct pd_fb_display {
	// the connector it follows, so a monitor keeps its index across probes, 0 for a free slot
	int connector_id;
	bool lit;
	struct drm_mode_set *sets[PD_FB_MAX_SETS];
	int nsets;
	struct drm_client_buffer *buf;
	// a second scanout buffer for page flipping, made when a presenter asks
	struct drm_client_buffer *back;
	u64 back_phys;
	// which of buf (0) and back (1) the crtcs scan out
	int front;
	// the connector's modes, entry 0 is the one in use
	struct drm_display_mode *modes[PD_FB_MAX_MODES];
	int nmodes;
	struct pd_drm_fb_info info;
};

static struct {
	struct mutex lock;
	struct drm_device *dev;
	// the client of the device that drives the displays
	struct drm_client_dev *client;
	bool client_ready;
	// drm_client_register calls hotplug at once, setup has just probed
	bool registering;
	// the newest buffer's kernel mapping, a pdfb_console=1 boot draws the console there
	void *vaddr;
	struct pd_fb_display displays[PD_DRM_FB_MAX_DISPLAYS];
	// display indices handed out so far, a dark one keeps its index for when it comes back
	int ndisplays;
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
	return pd_drm_fb_alloc_pinned(pdfb.client, width, height, phys);
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

// eight colour bars across the buffer, to tell a scanout fault from a compositor one
static void
pd_fb_pattern(struct iosys_map *map, u32 width, u32 height, u32 pitch)
{
	static const u32 bars[8] = {
		0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0x000000,
	};
	u32 *row;
	u32 x, y;

	row = kmalloc_array(width, sizeof(*row), GFP_KERNEL);
	if (row == NULL)
		return;
	for (x = 0; x < width; x++)
		row[x] = bars[x * 8 / width];
	for (y = 0; y < height; y++)
		iosys_map_memcpy_to(map, (size_t)y * pitch, row, width * sizeof(*row));
	kfree(row);
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

	buf = drm_client_framebuffer_create(pdfb.client, width, height, DRM_FORMAT_XRGB8888);
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
	// boot-arg pdfb_clear=0 leaves the buffer as vram held it, the clear is the first cpu write this high
	// in a large bar
	drm_info(pdfb.dev, "pdfb: buffer at vram offset 0x%llx, clearing through the bar\n", offset);
	pdfb.vaddr = NULL;
	if (drm_client_buffer_vmap(buf, &map) == 0) {
		pdfb.vaddr = map.is_iomem ? (void *)map.vaddr_iomem : map.vaddr;
		if (pd_drm_tunable_long("pdfb.clear", 1) != 0)
			iosys_map_memset(&map, 0, 0, (size_t)buf->fb->pitches[0] * height);
		// boot-arg pdfb_pattern=1 draws colour bars where the clear left black
		if (pd_drm_tunable_long("pdfb.pattern", 0) != 0)
			pd_fb_pattern(&map, width, height, buf->fb->pitches[0]);
	}
	drm_info(pdfb.dev, "pdfb: buffer ready\n");
	*phys = adev->gmc.aper_base + offset;
	return buf;
}
#endif

static void
pd_fb_drop_back(struct pd_fb_display *d)
{
	pd_fb_free_buffer(d->back);
	d->back = NULL;
	d->front = 0;
}

static void
pd_fb_forget_modes(struct pd_fb_display *d)
{
	int i;

	for (i = 0; i < d->nmodes; i++)
		drm_mode_destroy(pdfb.dev, d->modes[i]);
	d->nmodes = 0;
}

static struct pd_fb_display *
pd_fb_display(int index)
{
	if (index < 0 || index >= pdfb.ndisplays)
		return NULL;
	return &pdfb.displays[index];
}

// snapshots the display's connector modes, the current one first
static void
pd_fb_collect_modes(struct pd_fb_display *d)
{
	struct drm_mode_set *modeset = d->sets[0];
	struct drm_connector *connector = modeset->connectors[0];
	struct drm_display_mode *mode;

	pd_fb_forget_modes(d);
	d->modes[d->nmodes++] = drm_mode_duplicate(pdfb.dev, modeset->mode);
	mutex_lock(&pdfb.dev->mode_config.mutex);
	list_for_each_entry(mode, &connector->modes, head) {
		if (d->nmodes == PD_FB_MAX_MODES)
			break;
		if (drm_mode_match(mode, modeset->mode, DRM_MODE_MATCH_TIMINGS))
			continue;
		d->modes[d->nmodes++] = drm_mode_duplicate(pdfb.dev, mode);
	}
	mutex_unlock(&pdfb.dev->mode_config.mutex);
}

// gives every lit modeset a display: its connector's slot when it had one, else a free slot, and with
// boot-arg pdfb_mirror=1 all of them share display 0 as before
static int
pd_fb_assign_displays(void)
{
	struct drm_mode_set *modeset;
	bool mirror = pd_drm_tunable_long("pdfb.mirror", 0) != 0;
	int lit = 0;

	for (int i = 0; i < PD_DRM_FB_MAX_DISPLAYS; i++) {
		pdfb.displays[i].lit = false;
		pdfb.displays[i].nsets = 0;
	}
	drm_client_for_each_modeset(modeset, pdfb.client) {
		struct pd_fb_display *d = NULL;
		int id;

		if (modeset->mode == NULL || modeset->num_connectors == 0)
			continue;
		id = modeset->connectors[0]->base.id;
		if (mirror) {
			d = &pdfb.displays[0];
			if (d->connector_id == 0)
				d->connector_id = id;
		} else {
			for (int i = 0; i < PD_DRM_FB_MAX_DISPLAYS && d == NULL; i++) {
				if (pdfb.displays[i].connector_id == id)
					d = &pdfb.displays[i];
			}
			for (int i = 0; i < PD_DRM_FB_MAX_DISPLAYS && d == NULL; i++) {
				if (pdfb.displays[i].connector_id == 0) {
					d = &pdfb.displays[i];
					d->connector_id = id;
				}
			}
		}
		if (d == NULL || d->nsets == PD_FB_MAX_SETS) {
			drm_info(pdfb.dev, "pdfb: no display slot left for %s\n", modeset->connectors[0]->name);
			continue;
		}
		d->sets[d->nsets++] = modeset;
		d->lit = true;
		lit++;
		if (d - pdfb.displays >= pdfb.ndisplays)
			pdfb.ndisplays = d - pdfb.displays + 1;
	}
	return lit;
}

// every crtc of a display keeps its own mode and scans the display's buffer from its top-left
// corner, so the buffer covers the largest and the drawing area the smallest
static void
pd_fb_bounds(struct pd_fb_display *d, u32 *maxw, u32 *maxh, u32 *minw, u32 *minh)
{
	*maxw = *maxh = 0;
	*minw = *minh = U32_MAX;
	for (int i = 0; i < d->nsets; i++) {
		*maxw = max_t(u32, *maxw, d->sets[i]->mode->hdisplay);
		*maxh = max_t(u32, *maxh, d->sets[i]->mode->vdisplay);
		*minw = min_t(u32, *minw, d->sets[i]->mode->hdisplay);
		*minh = min_t(u32, *minh, d->sets[i]->mode->vdisplay);
	}
}

static void
pd_fb_set_scanout(struct pd_fb_display *d, struct drm_client_buffer *buf)
{
	for (int i = 0; i < d->nsets; i++)
		d->sets[i]->fb = buf != NULL ? buf->fb : NULL;
}

// a fresh pinned buffer for every lit display whose size changed, then one commit for all of them,
// the old buffers go once the new ones are on screen
static int
pd_fb_light(void)
{
	struct drm_client_buffer *fresh[PD_DRM_FB_MAX_DISPLAYS] = { NULL };
	struct drm_client_buffer *stale[PD_DRM_FB_MAX_DISPLAYS] = { NULL };
	u64 phys[PD_DRM_FB_MAX_DISPLAYS] = { 0 };
	u32 maxw, maxh, minw, minh;
	int ret = 0;

	for (int i = 0; i < pdfb.ndisplays; i++) {
		struct pd_fb_display *d = &pdfb.displays[i];

		if (!d->lit)
			continue;
		pd_fb_bounds(d, &maxw, &maxh, &minw, &minh);
		if (d->buf != NULL && d->buf->fb->width == maxw && d->buf->fb->height == maxh)
			continue;
		fresh[i] = pd_fb_alloc_pinned(maxw, maxh, &phys[i]);
		if (IS_ERR(fresh[i])) {
			ret = PTR_ERR(fresh[i]);
			fresh[i] = NULL;
			goto undo;
		}
	}

	// boot-arg pdfb_commit=0 makes and clears the buffers but never scans them out
	if (pd_drm_tunable_long("pdfb.commit", 1) == 0) {
		drm_info(pdfb.dev, "pdfb: pdfb_commit=0, not committing\n");
		ret = -ENODEV;
		goto undo;
	}
	// boot-arg pdfb_console=1 moves the console onto display 0's buffer first, so the commit's own
	// messages show on the screen it lights, drm_debug=N sets drm's debug categories
	if (pd_drm_tunable_long("pdfb.console", 0) != 0 && pdfb.vaddr != NULL && fresh[0] != NULL) {
		extern void pdos_console_redirect(void *va, u32 width, u32 height, u32 pitch);
		extern unsigned long __drm_debug;

		__drm_debug = (unsigned long)pd_drm_tunable_long("drm.debug", (long)__drm_debug);
		pd_fb_bounds(&pdfb.displays[0], &maxw, &maxh, &minw, &minh);
		pdos_console_redirect(pdfb.vaddr, minw, minh, fresh[0]->fb->pitches[0]);
	}
	for (int i = 0; i < pdfb.ndisplays; i++) {
		struct pd_fb_display *d = &pdfb.displays[i];

		if (!d->lit)
			continue;
		pd_fb_bounds(d, &maxw, &maxh, &minw, &minh);
		drm_info(pdfb.dev, "pdfb: committing display %d %ux%u on %s\n", i, maxw, maxh,
		    d->sets[0]->connectors[0]->name);
		pd_fb_set_scanout(d, fresh[i] != NULL ? fresh[i] : d->buf);
	}
	ret = drm_client_modeset_commit(pdfb.client);
	drm_info(pdfb.dev, "pdfb: commit returned %d\n", ret);
	if (ret != 0) {
		drm_err(pdfb.dev, "pdfb: commit failed %d\n", ret);
		for (int i = 0; i < pdfb.ndisplays; i++) {
			if (pdfb.displays[i].lit)
				pd_fb_set_scanout(&pdfb.displays[i], pdfb.displays[i].buf);
		}
		goto undo;
	}

	for (int i = 0; i < pdfb.ndisplays; i++) {
		struct pd_fb_display *d = &pdfb.displays[i];

		if (!d->lit) {
			// a display that went dark keeps its index, its buffers go
			if (d->buf != NULL) {
				pd_fb_drop_back(d);
				stale[i] = d->buf;
				d->buf = NULL;
			}
			memset(&d->info, 0, sizeof(d->info));
			continue;
		}
		if (fresh[i] != NULL) {
			// a new size means new buffers, the flip pair is made again on demand
			pd_fb_drop_back(d);
			stale[i] = d->buf;
			d->buf = fresh[i];
			d->info.phys = phys[i];
		} else if (d->front != 0) {
			// a relight puts buffer 0 back on screen
			d->front = 0;
		}
		pd_fb_bounds(d, &maxw, &maxh, &minw, &minh);
		// the console and the compositor draw in the area every crtc of the display shows
		d->info.width = minw;
		d->info.height = minh;
		d->info.pitch = d->buf->fb->pitches[0];
		d->info.refresh = drm_mode_vrefresh(d->sets[0]->mode);
		if (minw != maxw || minh != maxh)
			drm_info(pdfb.dev, "pdfb: display %d mirrors unequal crtcs, buffer %ux%u, drawing in %ux%u\n",
			    i, maxw, maxh, minw, minh);
		drm_info(pdfb.dev, "pdfb: display %d is %ux%u@%u pitch %u at phys 0x%llx\n", i, d->info.width,
		    d->info.height, d->info.refresh, d->info.pitch, (unsigned long long)d->info.phys);
	}
	for (int i = 0; i < PD_DRM_FB_MAX_DISPLAYS; i++)
		pd_fb_free_buffer(stale[i]);
	return 0;

undo:
	for (int i = 0; i < PD_DRM_FB_MAX_DISPLAYS; i++)
		pd_fb_free_buffer(fresh[i]);
	return ret;
}

// probes, picks the preferred mode (or pdfb_mode=WxH) and lights it
// an output that reports a display but never answers on aux has no edid, training its link hangs the
// commit, so it stays dark as long as another output answered
static void
pd_fb_drop_silent_outputs(void)
{
	struct drm_mode_set *modeset;
	bool answered = false;

	mutex_lock(&pdfb.client->modeset_mutex);
	drm_client_for_each_modeset(modeset, pdfb.client) {
		if (modeset->mode != NULL && modeset->num_connectors > 0 &&
		    modeset->connectors[0]->edid_blob_ptr != NULL)
			answered = true;
	}
	drm_client_for_each_modeset(modeset, pdfb.client) {
		if (!answered || modeset->mode == NULL || modeset->num_connectors == 0 ||
		    modeset->connectors[0]->edid_blob_ptr != NULL)
			continue;
		drm_info(pdfb.dev, "pdfb: %s reports a display but gave no edid, leaving it dark\n",
		    modeset->connectors[0]->name);
		drm_mode_destroy(pdfb.dev, modeset->mode);
		modeset->mode = NULL;
		modeset->fb = NULL;
		for (unsigned int i = 0; i < modeset->num_connectors; i++)
			drm_connector_put(modeset->connectors[i]);
		modeset->num_connectors = 0;
	}
	mutex_unlock(&pdfb.client->modeset_mutex);
}

static int
pd_fb_probe_and_light(void)
{
	struct drm_mode_set *modeset, *lit;
	struct drm_display_mode *mode;
	long want = pd_drm_tunable_long("pdfb.mode", 0);
	int ret;

	struct drm_connector_list_iter iter;
	struct drm_connector *connector;

	ret = drm_client_modeset_probe(pdfb.client, 0, 0);
	if (ret != 0)
		return ret;
	pd_fb_drop_silent_outputs();
	// each gpu reports its connectors, so a multi-gpu log shows which card has the monitor
	char summary[160] = "";
	size_t used = 0;
	drm_connector_list_iter_begin(pdfb.dev, &iter);
	drm_for_each_connector_iter(connector, &iter) {
		const char *state = connector->status == connector_status_connected ? "connected" :
		    connector->status == connector_status_disconnected ? "disconnected" : "unknown";
		const char *edid = connector->edid_blob_ptr != NULL ? "EDID read" : "no EDID";

		drm_info(pdfb.dev, "pdfb: %s (%s) %s, %s, %d modes\n", connector->name,
		    drm_get_connector_type_name(connector->connector_type), state, edid,
		    list_count_nodes(&connector->modes));
		if (used < sizeof(summary))
			used += snprintf(summary + used, sizeof(summary) - used, "%s%s %s %s",
			    used != 0 ? ", " : "", connector->name, state, edid);
	}
	drm_connector_list_iter_end(&iter);
	modeset = NULL;
	drm_client_for_each_modeset(lit, pdfb.client) {
		if (modeset == NULL && lit->mode != NULL && lit->num_connectors > 0)
			modeset = lit;
	}
	if (modeset == NULL) {
		// this line becomes the handover reason IOGOPFramebuffer prints
		drm_info(pdfb.dev, "no connected display: %s\n", summary);
		return -ENODEV;
	}

	// boot-arg pdfb_mode=WWWWHHHH picks a listed mode for the first display, 12800720 is 1280x720
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

	// boot-arg pdfb_light=0 probes and logs the mode but leaves the displays alone, to tell a hang
	// in the modeset from one before it
	if (pd_drm_tunable_long("pdfb.light", 1) == 0) {
		drm_info(pdfb.dev, "pdfb: would light %ux%u@%d, pdfb_light=0 leaves it dark\n",
		    modeset->mode->hdisplay, modeset->mode->vdisplay, drm_mode_vrefresh(modeset->mode));
		return -ENODEV;
	}
	pd_fb_assign_displays();
	ret = pd_fb_light();
	if (ret == 0) {
		for (int i = 0; i < pdfb.ndisplays; i++) {
			if (pdfb.displays[i].lit)
				pd_fb_collect_modes(&pdfb.displays[i]);
			else
				pd_fb_forget_modes(&pdfb.displays[i]);
		}
	}
	// boot-arg pdfb_register=0 lights the displays but keeps them from IOKit, to tell a hang in the
	// modeset from one in the framebuffer handover
	if (ret == 0 && pd_drm_tunable_long("pdfb.register", 1) == 0) {
		drm_info(pdfb.dev, "pdfb: lit, pdfb_register=0 keeps it from IOKit\n");
		return -ENODEV;
	}
	return ret;
}

static bool
pd_fb_any_lit(void)
{
	for (int i = 0; i < pdfb.ndisplays; i++) {
		if (pdfb.displays[i].buf != NULL)
			return true;
	}
	return false;
}

static int
pd_fb_hotplug(struct drm_client_dev *client)
{
	int ret, count;

	mutex_lock(&pdfb.lock);
	if (!pd_fb_any_lit() || pdfb.registering) {
		mutex_unlock(&pdfb.lock);
		return 0;
	}
	// the probe rebuilds the modesets, the old buffers stay until the new commit
	ret = pd_fb_probe_and_light();
	count = pdfb.ndisplays;
	mutex_unlock(&pdfb.lock);
	drm_info(pdfb.dev, "pdfb: connector change handled (%d)\n", ret);
	// every display's framebuffer hears it, display 0 also makes framebuffers for new indices
	for (int i = 0; ret == 0 && i < count; i++) {
		if (pd_drm_fb->changed != NULL && pd_drm_fb->changed_context[i] != NULL)
			pd_drm_fb->changed(pd_drm_fb->changed_context[i]);
	}
	return 0;
}

static const struct drm_client_funcs pd_fb_client_funcs = {
	.owner = THIS_MODULE,
	.hotplug = pd_fb_hotplug,
};

static int
pd_amdgpu_fb_setup(struct pd_drm_fb_info *info)
{
	int ret = -ENODEV;
	bool registering = false;

	mutex_lock(&pdfb.lock);
	// one display client drives the screens, a second set of framebuffers would fight it
	if (pdfb.client_ready) {
		mutex_unlock(&pdfb.lock);
		return -EBUSY;
	}
	// the first device with a connected display wins, the monitors may sit on any of them
	for (int i = 0; i < pd_fb_ndevs; i++) {
		if (!pd_fb_client_inited[i]) {
			if (drm_client_init(pd_fb_devs[i], &pd_fb_clients[i], "pdfb", &pd_fb_client_funcs) != 0)
				continue;
			pd_fb_client_inited[i] = true;
		}
		pdfb.dev = pd_fb_devs[i];
		pdfb.client = &pd_fb_clients[i];
		ret = pd_fb_probe_and_light();
		if (ret == 0) {
			pdfb.client_ready = true;
			registering = true;
			break;
		}
	}
	if (ret == 0)
		*info = pdfb.displays[0].info;
	mutex_unlock(&pdfb.lock);

	// registering calls the hotplug hook at once, which takes the lock itself
	if (registering) {
		pdfb.registering = true;
		drm_client_register(pdfb.client);
		pdfb.registering = false;
	}
	return ret;
}

static int
pd_amdgpu_fb_display_count(void)
{
	return pdfb.ndisplays;
}

static int
pd_amdgpu_fb_on_screen(int display, struct pd_drm_fb_info *info)
{
	struct pd_fb_display *d;
	int ret = -ENODEV;

	mutex_lock(&pdfb.lock);
	d = pd_fb_display(display);
	if (d != NULL && d->buf != NULL) {
		*info = d->info;
		ret = 0;
	}
	mutex_unlock(&pdfb.lock);
	return ret;
}

static int
pd_amdgpu_fb_mode_count(int display)
{
	struct pd_fb_display *d = pd_fb_display(display);

	return d != NULL && d->lit ? d->nmodes : 0;
}

static int
pd_amdgpu_fb_get_mode(int display, int index, struct pd_drm_fb_mode *out)
{
	const struct drm_display_mode *mode;
	struct pd_fb_display *d;

	mutex_lock(&pdfb.lock);
	d = pd_fb_display(display);
	if (d == NULL || index < 0 || index >= d->nmodes) {
		mutex_unlock(&pdfb.lock);
		return -EINVAL;
	}
	mode = d->modes[index];
	out->width = mode->hdisplay;
	out->height = mode->vdisplay;
	out->refresh = drm_mode_vrefresh(mode);
	out->preferred = (mode->type & DRM_MODE_TYPE_PREFERRED) != 0;
	mutex_unlock(&pdfb.lock);
	return 0;
}

// the display's crtcs take the mode, the others keep theirs and their buffers
static int
pd_amdgpu_fb_set_mode(int display, int index, struct pd_drm_fb_info *info)
{
	struct pd_fb_display *d;
	int ret;

	mutex_lock(&pdfb.lock);
	d = pd_fb_display(display);
	if (d == NULL || !d->lit || index < 0 || index >= d->nmodes) {
		mutex_unlock(&pdfb.lock);
		return -EINVAL;
	}
	mutex_lock(&pdfb.client->modeset_mutex);
	for (int i = 0; i < d->nsets; i++) {
		drm_mode_destroy(pdfb.dev, d->sets[i]->mode);
		d->sets[i]->mode = drm_mode_duplicate(pdfb.dev, d->modes[index]);
	}
	mutex_unlock(&pdfb.client->modeset_mutex);
	ret = pd_fb_light();
	if (ret == 0)
		*info = d->info;
	mutex_unlock(&pdfb.lock);
	return ret;
}

static void
pd_amdgpu_fb_release(void)
{
	struct drm_mode_set *modeset;

	mutex_lock(&pdfb.lock);
	if (pd_fb_any_lit()) {
		// dark first, so nothing scans out of the buffers being freed
		drm_client_modeset_dpms(pdfb.client, DRM_MODE_DPMS_OFF);
		drm_client_for_each_modeset(modeset, pdfb.client)
			modeset->fb = NULL;
	}
	for (int i = 0; i < pdfb.ndisplays; i++) {
		struct pd_fb_display *d = &pdfb.displays[i];

		pd_fb_drop_back(d);
		if (d->buf != NULL) {
			pd_fb_free_buffer(d->buf);
			d->buf = NULL;
			drm_info(pdfb.dev, "pdfb: display %d scanout buffer released\n", i);
		}
		pd_fb_forget_modes(d);
	}
	mutex_unlock(&pdfb.lock);
}

// the flip pair: buffer 0 is the one the display was lit with, buffer 1 is made here
static int
pd_amdgpu_fb_buffer(int display, int index, uint64_t *phys)
{
	struct pd_fb_display *d;
	int ret = 0;

	mutex_lock(&pdfb.lock);
	d = pd_fb_display(display);
	if (d == NULL || d->buf == NULL || index < 0 || index > 1) {
		ret = -EINVAL;
	} else if (index == 0) {
		*phys = d->info.phys;
	} else {
		if (d->back == NULL) {
			// the same size as the scanout buffer, which can be larger than the drawing area
			struct drm_client_buffer *back = pd_fb_alloc_pinned(d->buf->fb->width,
			    d->buf->fb->height, &d->back_phys);
			if (IS_ERR(back))
				ret = PTR_ERR(back);
			else
				d->back = back;
		}
		if (ret == 0)
			*phys = d->back_phys;
	}
	mutex_unlock(&pdfb.lock);
	return ret;
}

// scans out buffer index on the display's crtcs from the next vblank on, returns once the flip
// happened with that vblank's count and time
static int
pd_amdgpu_fb_flip(int display, int index, uint64_t *sequence, uint64_t *nanoseconds)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state;
	struct drm_plane_state *plane_state;
	struct drm_framebuffer *fb;
	struct pd_fb_display *d;
	struct drm_crtc *crtc;
	ktime_t when;
	int ret;

	mutex_lock(&pdfb.lock);
	d = pd_fb_display(display);
	fb = d == NULL ? NULL : index == 0 ? (d->buf != NULL ? d->buf->fb : NULL) :
	    (d->back != NULL ? d->back->fb : NULL);
	if (d == NULL || !d->lit || d->nsets == 0 || fb == NULL) {
		mutex_unlock(&pdfb.lock);
		return -EINVAL;
	}
	crtc = d->sets[0]->crtc;

	DRM_MODESET_LOCK_ALL_BEGIN(pdfb.dev, ctx, 0, ret);
	if (!drm_drv_uses_atomic_modeset(pdfb.dev)) {
		// legacy display (nv04) has no atomic state, set_config swaps the base address only
		for (int i = 0; i < d->nsets; i++) {
			struct drm_mode_set set = *d->sets[i];

			set.fb = fb;
			ret = drm_mode_set_config_internal(&set);
			if (ret != 0)
				break;
		}
		goto unlock;
	}
	state = drm_atomic_state_alloc(pdfb.dev);
	if (state == NULL) {
		ret = -ENOMEM;
		goto unlock;
	}
	state->acquire_ctx = &ctx;
	// the display's crtcs flip in one commit, the other displays are left alone
	for (int i = 0; i < d->nsets; i++) {
		plane_state = drm_atomic_get_plane_state(state, d->sets[i]->crtc->primary);
		if (IS_ERR(plane_state)) {
			ret = PTR_ERR(plane_state);
			goto put;
		}
		drm_atomic_set_fb_for_plane(plane_state, fb);
	}
	// a blocking commit returns after flip_done, at the vblank that took it
	ret = drm_atomic_commit(state);
put:
	drm_atomic_state_put(state);
unlock:
	DRM_MODESET_LOCK_ALL_END(pdfb.dev, ctx, ret);

	if (ret == 0) {
		d->front = index;
		if (!drm_drv_uses_atomic_modeset(pdfb.dev))
			drm_crtc_wait_one_vblank(crtc);
		*sequence = drm_crtc_vblank_count_and_time(crtc, &when);
		*nanoseconds = ktime_to_ns(when);
	}
	mutex_unlock(&pdfb.lock);
	return ret;
}

// the flip buffer as a dma-buf fd of the caller, the gpu driver imports it back as the same bo
static int
pd_amdgpu_fb_export_fd(int display, int index, int *fd)
{
	struct drm_client_buffer *buf;
	struct pd_fb_display *d;
	struct dma_buf *dmabuf;
	uint64_t phys;
	int ret;

	ret = pd_amdgpu_fb_buffer(display, index, &phys);
	if (ret != 0)
		return ret;
	mutex_lock(&pdfb.lock);
	d = pd_fb_display(display);
	buf = d == NULL ? NULL : index == 0 ? d->buf : d->back;
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
	.display_count = pd_amdgpu_fb_display_count,
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
	if (pd_fb_ndevs == 0)
		mutex_init(&pdfb.lock);
	if (pd_fb_ndevs < PD_FB_MAX_DEVS)
		pd_fb_devs[pd_fb_ndevs++] = dev;
	if (pdfb.dev == NULL) {
		pdfb.dev = dev;
		pdfb.client = &pd_fb_clients[0];
	}
	pd_drm_fb = &pd_amdgpu_fb_ops;
#ifndef PD_DRM_FB_DRIVER_BUFFERS
	pd_amdgpu_mmap_register();
#endif
}
