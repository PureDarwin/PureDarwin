// SPDX-License-Identifier: MIT
// managed drm resources from the drm_managed.h contract
// releases run newest first at device release
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/mutex.h>

#include <drm/drm_device.h>
#include <drm/drm_managed.h>
#include <drm/drm_print.h>

struct pd_drmres {
	struct list_head entry;
	drmres_release_t release;
	void *data;
	const char *name;
	bool is_alloc;
	// allocations live right after the header
	char payload[] __aligned(16);
};

static void
pd_drmres_add(struct drm_device *dev, struct pd_drmres *dr)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->managed.lock, flags);
	list_add(&dr->entry, &dev->managed.resources);
	spin_unlock_irqrestore(&dev->managed.lock, flags);
}

int
__drmm_add_action(struct drm_device *dev, drmres_release_t action, void *data,
    const char *name)
{
	struct pd_drmres *dr;

	dr = kzalloc(sizeof(*dr), GFP_KERNEL);
	if (dr == NULL)
		return -ENOMEM;
	dr->release = action;
	dr->data = data;
	dr->name = name;
	pd_drmres_add(dev, dr);
	return 0;
}

int
__drmm_add_action_or_reset(struct drm_device *dev, drmres_release_t action,
    void *data, const char *name)
{
	int ret;

	ret = __drmm_add_action(dev, action, data, name);
	if (ret != 0)
		action(dev, data);
	return ret;
}

void
drmm_release_action(struct drm_device *dev, drmres_release_t action, void *data)
{
	struct pd_drmres *dr, *found = NULL;
	unsigned long flags;

	spin_lock_irqsave(&dev->managed.lock, flags);
	list_for_each_entry(dr, &dev->managed.resources, entry) {
		if (!dr->is_alloc && dr->release == action && dr->data == data) {
			list_del(&dr->entry);
			found = dr;
			break;
		}
	}
	spin_unlock_irqrestore(&dev->managed.lock, flags);
	if (found == NULL)
		return;
	found->release(dev, found->data);
	kfree(found);
}

void *
drmm_kmalloc(struct drm_device *dev, size_t size, gfp_t gfp)
{
	struct pd_drmres *dr;

	dr = kmalloc(sizeof(*dr) + size, gfp);
	if (dr == NULL)
		return NULL;
	memset(dr, 0, sizeof(*dr));
	dr->is_alloc = true;
	dr->name = "kmalloc";
	pd_drmres_add(dev, dr);
	return dr->payload;
}

void
drmm_kfree(struct drm_device *dev, void *data)
{
	struct pd_drmres *dr, *found = NULL;
	unsigned long flags;

	if (data == NULL)
		return;
	spin_lock_irqsave(&dev->managed.lock, flags);
	list_for_each_entry(dr, &dev->managed.resources, entry) {
		if (dr->is_alloc && dr->payload == data) {
			list_del(&dr->entry);
			found = dr;
			break;
		}
	}
	spin_unlock_irqrestore(&dev->managed.lock, flags);
	WARN_ON(found == NULL);
	kfree(found);
}

char *
drmm_kstrdup(struct drm_device *dev, const char *s, gfp_t gfp)
{
	size_t len;
	char *p;

	if (s == NULL)
		return NULL;
	len = strlen(s) + 1;
	p = drmm_kmalloc(dev, len, gfp);
	if (p != NULL)
		memcpy(p, s, len);
	return p;
}

static void
pd_drmm_mutex_release(struct drm_device *dev, void *res)
{
	mutex_destroy((struct mutex *)res);
}

int
__drmm_mutex_release_add(struct drm_device *dev, struct mutex *lock)
{
	return drmm_add_action_or_reset(dev, pd_drmm_mutex_release, lock);
}

void
drmm_add_final_kfree(struct drm_device *dev, void *container)
{
	WARN_ON(dev->managed.final_kfree != NULL);
	// the device must sit inside the container being freed
	WARN_ON((void *)dev < container);
	dev->managed.final_kfree = container;
}

void
drm_managed_release(struct drm_device *dev)
{
	struct pd_drmres *dr, *next;

	list_for_each_entry_safe(dr, next, &dev->managed.resources, entry) {
		list_del(&dr->entry);
		if (dr->release != NULL)
			dr->release(dev, dr->data);
		kfree(dr);
	}
}
