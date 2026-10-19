// SPDX-License-Identifier: MIT
// drm_device, written from the fields drm core, ttm and amdgpu use
// drm-kmod's own copy has no licence notice
#ifndef _PD_DRM_DEVICE_H_
#define _PD_DRM_DEVICE_H_

#include <linux/list.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/idr.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include <drm/drm_mode_config.h>

struct drm_driver;
struct drm_minor;
struct drm_master;
struct drm_vblank_crtc;
struct drm_vma_offset_manager;
struct drm_vram_mm;
struct drm_fb_helper;
struct inode;
struct device;

// the vga switcheroo state of a device, which drm_file checks on open
enum switch_power_state {
	DRM_SWITCH_POWER_ON = 0,
	DRM_SWITCH_POWER_OFF = 1,
	DRM_SWITCH_POWER_CHANGING = 2,
	DRM_SWITCH_POWER_DYNAMIC_OFF = 3,
};

struct drm_device {
	int if_version;
	struct kref ref;
	struct device *dev;

	// drm_managed.c keeps its release actions here
	struct {
		struct list_head resources;
		void *final_kfree;
		spinlock_t lock;
	} managed;

	const struct drm_driver *driver;
	void *dev_private;

	struct drm_minor *primary;
	struct drm_minor *render;
	struct drm_minor *accel;

	bool registered;
	bool unplugged;
	struct drm_master *master;
	u32 driver_features;
	char *unique;

	struct mutex struct_mutex;
	struct mutex master_mutex;
	atomic_t open_count;

	struct mutex filelist_mutex;
	struct list_head filelist;
	struct list_head filelist_internal;

	struct mutex clientlist_mutex;
	struct list_head clientlist;

	bool vblank_disable_immediate;
	struct drm_vblank_crtc *vblank;
	spinlock_t vblank_time_lock;
	spinlock_t vbl_lock;
	u32 max_vblank_count;
	struct list_head vblank_event_list;
	spinlock_t event_lock;
	unsigned int num_crtcs;

	struct drm_mode_config mode_config;

	struct mutex object_name_lock;
	struct idr object_name_idr;
	struct drm_vma_offset_manager *vma_offset_manager;
	struct drm_vram_mm *vram_mm;

	enum switch_power_state switch_power_state;
	struct drm_fb_helper *fb_helper;

	// FreeBSD's drm_os_freebsd.c registers the aperture's fake pages once
	bool fictitious_range_registered;
};

#endif
