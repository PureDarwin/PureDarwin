// SPDX-License-Identifier: MIT
// drm leases are unsupported as in drm-kmod
// every object counts as held and the lease ioctls fail
#ifndef _PD_DRM_LEASE_H_
#define _PD_DRM_LEASE_H_

#include <linux/types.h>
#include <linux/errno.h>

struct drm_device;
struct drm_file;
struct drm_master;

static inline struct drm_master *
drm_lease_owner(struct drm_master *master)
{
	return master;
}

static inline void drm_lease_destroy(struct drm_master *lessee) { }
static inline void drm_lease_revoke(struct drm_master *master) { }

static inline bool
drm_lease_held(struct drm_file *file_priv, int id)
{
	return true;
}

#define _drm_lease_held(f, id) drm_lease_held(f, id)

static inline uint32_t
drm_lease_filter_crtcs(struct drm_file *file_priv, uint32_t crtcs)
{
	return crtcs;
}

int drm_mode_create_lease_ioctl(struct drm_device *dev, void *data,
    struct drm_file *lessor_priv);
int drm_mode_list_lessees_ioctl(struct drm_device *dev, void *data,
    struct drm_file *lessor_priv);
int drm_mode_get_lease_ioctl(struct drm_device *dev, void *data,
    struct drm_file *lessee_priv);
int drm_mode_revoke_lease_ioctl(struct drm_device *dev, void *data,
    struct drm_file *lessor_priv);

#endif
