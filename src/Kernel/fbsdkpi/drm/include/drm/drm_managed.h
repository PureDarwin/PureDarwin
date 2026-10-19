// SPDX-License-Identifier: MIT
// managed drm resources, released in reverse order when the drm_device goes away
#ifndef _PD_DRM_MANAGED_H_
#define _PD_DRM_MANAGED_H_

#include <linux/gfp.h>
#include <linux/overflow.h>
#include <linux/types.h>

struct drm_device;
struct mutex;

typedef void (*drmres_release_t)(struct drm_device *dev, void *res);

int __drmm_add_action(struct drm_device *dev, drmres_release_t action,
    void *data, const char *name);
int __drmm_add_action_or_reset(struct drm_device *dev, drmres_release_t action,
    void *data, const char *name);
void drmm_release_action(struct drm_device *dev, drmres_release_t action,
    void *data);

#define drmm_add_action(dev, action, data) \
	__drmm_add_action(dev, action, data, #action)
#define drmm_add_action_or_reset(dev, action, data) \
	__drmm_add_action_or_reset(dev, action, data, #action)

void drmm_add_final_kfree(struct drm_device *dev, void *container);
void drm_managed_release(struct drm_device *dev);

void *drmm_kmalloc(struct drm_device *dev, size_t size, gfp_t gfp);
void drmm_kfree(struct drm_device *dev, void *data);
char *drmm_kstrdup(struct drm_device *dev, const char *s, gfp_t gfp);

static inline void *
drmm_kzalloc(struct drm_device *dev, size_t size, gfp_t gfp)
{
	return drmm_kmalloc(dev, size, gfp | __GFP_ZERO);
}

static inline void *
drmm_kmalloc_array(struct drm_device *dev, size_t n, size_t size, gfp_t flags)
{
	size_t bytes;

	if (unlikely(check_mul_overflow(n, size, &bytes)))
		return NULL;
	return drmm_kmalloc(dev, bytes, flags);
}

static inline void *
drmm_kcalloc(struct drm_device *dev, size_t n, size_t size, gfp_t flags)
{
	return drmm_kmalloc_array(dev, n, size, flags | __GFP_ZERO);
}

int __drmm_mutex_release_add(struct drm_device *dev, struct mutex *lock);
#define drmm_mutex_init(dev, lock) ({ \
	mutex_init(lock); \
	__drmm_mutex_release_add(dev, lock); \
})

#endif
