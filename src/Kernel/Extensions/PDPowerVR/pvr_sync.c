// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// the syncobj half of pvr_sync.c, with the drm_syncobj table kept per client

#include "pvr_device.h"
#include "pvr_queue.h"
#include "pvr_sync.h"

struct pvr_syncobj {
	struct kref ref_count;
	struct mutex lock;
	struct pvr_fence *fence;
};

static void
pvr_syncobj_release(struct kref *ref)
{
	struct pvr_syncobj *s = container_of(ref, struct pvr_syncobj, ref_count);

	pvr_fence_put(s->fence);
	mutex_destroy(&s->lock);
	kfree(s);
}

static struct pvr_syncobj *
pvr_syncobj_lookup(struct pvr_file *pvr_file, u32 handle)
{
	struct pvr_syncobj *s;

	xa_lock(&pvr_file->syncobj_handles);
	s = xa_load(&pvr_file->syncobj_handles, handle);
	if (s)
		kref_get(&s->ref_count);
	xa_unlock(&pvr_file->syncobj_handles);
	return s;
}

static void
pvr_syncobj_put(struct pvr_syncobj *s)
{
	if (s)
		kref_put(&s->ref_count, pvr_syncobj_release);
}

int
pvr_syncobj_create(struct pvr_file *pvr_file, u32 flags, u32 *handle_out)
{
	struct pvr_syncobj *s = kzalloc(sizeof(*s), GFP_KERNEL);
	int err;

	if (!s)
		return -ENOMEM;
	kref_init(&s->ref_count);
	mutex_init(&s->lock);
	if (flags & PVR_SYNCOBJ_CREATE_SIGNALED) {
		s->fence = pvr_fence_create_signaled();
		if (!s->fence) {
			pvr_syncobj_put(s);
			return -ENOMEM;
		}
	}
	err = xa_alloc(&pvr_file->syncobj_handles, handle_out, s, xa_limit_32b, GFP_KERNEL);
	if (err)
		pvr_syncobj_put(s);
	return err;
}

int
pvr_syncobj_destroy(struct pvr_file *pvr_file, u32 handle)
{
	struct pvr_syncobj *s = xa_erase(&pvr_file->syncobj_handles, handle);

	if (!s)
		return -EINVAL;
	pvr_syncobj_put(s);
	return 0;
}

// a binary syncobj with nothing in it cannot be waited on, as with drm_syncobj
struct pvr_fence *
pvr_syncobj_fence_get(struct pvr_file *pvr_file, u32 handle)
{
	struct pvr_syncobj *s = pvr_syncobj_lookup(pvr_file, handle);
	struct pvr_fence *f;

	if (!s)
		return ERR_PTR(-ENOENT);
	mutex_lock(&s->lock);
	f = pvr_fence_get(s->fence);
	mutex_unlock(&s->lock);
	pvr_syncobj_put(s);
	return f ? f : ERR_PTR(-EINVAL);
}

int
pvr_syncobj_replace_fence(struct pvr_file *pvr_file, u32 handle, struct pvr_fence *fence)
{
	struct pvr_syncobj *s = pvr_syncobj_lookup(pvr_file, handle);
	struct pvr_fence *old;

	if (!s)
		return -ENOENT;
	mutex_lock(&s->lock);
	old = s->fence;
	s->fence = pvr_fence_get(fence);
	mutex_unlock(&s->lock);
	pvr_fence_put(old);
	pvr_syncobj_put(s);
	return 0;
}

/* Transfer fence ownership without waiting for the GPU. Nonzero destination
 * points append to Mesa's temporary conjunction; binary points replace it. */
int
pvr_syncobj_transfer(struct pvr_file *pvr_file, u32 src, u32 dst, bool join)
{
	struct pvr_fence *f = pvr_syncobj_fence_get(pvr_file, src);
	struct pvr_syncobj *s;
	struct pvr_fence *old, *replacement;

	if (IS_ERR(f))
		return PTR_ERR(f);
	s = pvr_syncobj_lookup(pvr_file, dst);
	if (!s) {
		pvr_fence_put(f);
		return -ENOENT;
	}
	mutex_lock(&s->lock);
	old = s->fence;
	replacement = join ? pvr_fence_join(old, f) : pvr_fence_get(f);
	if (replacement)
		s->fence = replacement;
	mutex_unlock(&s->lock);
	if (replacement)
		pvr_fence_put(old);
	pvr_syncobj_put(s);
	pvr_fence_put(f);
	return replacement ? 0 : -ENOMEM;
}

int
pvr_syncobj_reset(struct pvr_file *pvr_file, u32 handle)
{
	return pvr_syncobj_replace_fence(pvr_file, handle, NULL);
}

int
pvr_syncobj_signal(struct pvr_file *pvr_file, u32 handle)
{
	struct pvr_fence *f = pvr_fence_create_signaled();
	int err;

	if (!f)
		return -ENOMEM;
	err = pvr_syncobj_replace_fence(pvr_file, handle, f);
	pvr_fence_put(f);
	return err;
}

// all, or the first of them, signalled within the timeout
int
pvr_syncobj_wait(struct pvr_device *pvr_dev, struct pvr_file *pvr_file, const u32 *handles,
		 u32 count, u32 flags, u64 timeout_us, u32 *first_signaled)
{
	u64 deadline = pd_pvr_now_us() + timeout_us;

	for (;;) {
		u32 done = 0, gen = pd_pvr_wait_gen();

		for (u32 i = 0; i < count; i++) {
			struct pvr_fence *f = pvr_syncobj_fence_get(pvr_file, handles[i]);
			bool sig;
			int err;

			if (IS_ERR(f)) {
				if (PTR_ERR(f) == -ENOENT)
					return -ENOENT;
				// not submitted yet: drm reports that as an error unless asked to wait for it
				return -EINVAL;
			}
			sig = pvr_fence_is_signaled(f);
			err = sig ? READ_ONCE(f->error) : 0;
			pvr_fence_put(f);
			if (err)
				return err;
			if (sig) {
				if (!(flags & PVR_SYNCOBJ_WAIT_ALL)) {
					if (first_signaled)
						*first_signaled = i;
					return 0;
				}
				done++;
			}
		}
		if (count == 0 || done == count)
			return 0;
		if (pd_pvr_now_us() >= deadline)
			return -ETIMEDOUT;
		pd_pvr_wait_since(pvr_dev, gen, 10);
	}
}

void
pvr_destroy_syncobjs_for_file(struct pvr_file *pvr_file)
{
	struct pvr_syncobj *s;
	unsigned long handle;

	xa_for_each(&pvr_file->syncobj_handles, handle, s)
		pvr_syncobj_put(xa_erase(&pvr_file->syncobj_handles, handle));
}
