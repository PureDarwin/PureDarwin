// SPDX-License-Identifier: MIT
// sync_file, a fence wrapped in a file descriptor with the merge and info ioctls
// written from the sync_file uapi and the way drm_syncobj.c and amdgpu use it
#include <linux/dma-fence.h>
#include <linux/dma-fence-unwrap.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sync_file.h>
#include <linux/uaccess.h>

// the uapi layouts from linux's include/uapi/linux/sync_file.h
struct pd_sync_merge_data {
	char name[32];
	int32_t fd2;
	int32_t fence;
	uint32_t flags;
	uint32_t pad;
};

struct pd_sync_fence_info {
	char obj_name[32];
	char driver_name[32];
	int32_t status;
	uint32_t flags;
	uint64_t timestamp_ns;
};

struct pd_sync_file_info {
	char name[32];
	int32_t status;
	uint32_t flags;
	uint32_t num_fences;
	uint32_t pad;
	uint64_t sync_fence_info;
};

#define PD_SYNC_IOC_MERGE	_IOWR('>', 3, struct pd_sync_merge_data)
#define PD_SYNC_IOC_FILE_INFO	_IOWR('>', 4, struct pd_sync_file_info)

static const struct file_operations pd_sync_file_fops;

static int
pd_sync_file_release(struct inode *inode, struct linux_file *filp)
{
	struct sync_file *sf = filp->private_data;

	dma_fence_put(sf->fence);
	kfree(sf);
	return 0;
}

static unsigned int
pd_sync_file_poll(struct linux_file *filp, poll_table *wait)
{
	struct sync_file *sf = filp->private_data;

	return dma_fence_is_signaled(sf->fence) ? POLLIN : 0;
}

// the descriptor's file and its fence reference, or NULL
struct sync_file *
sync_file_create(struct dma_fence *fence)
{
	struct sync_file *sf = kzalloc(sizeof(*sf), GFP_KERNEL);

	if (sf == NULL)
		return NULL;
	sf->file = alloc_file(FMODE_READ, &pd_sync_file_fops);
	if (sf->file == NULL) {
		kfree(sf);
		return NULL;
	}
	sf->file->private_data = sf;
	sf->fence = dma_fence_get(fence);
	return sf;
}

static struct sync_file *
pd_sync_file_fdget(int fd, struct linux_file **filpp)
{
	struct linux_file *filp = fget(fd);

	if (filp == NULL)
		return NULL;
	if (filp->f_op != &pd_sync_file_fops) {
		fput(filp);
		return NULL;
	}
	*filpp = filp;
	return filp->private_data;
}

struct dma_fence *
sync_file_get_fence(int fd)
{
	struct linux_file *filp;
	struct sync_file *sf = pd_sync_file_fdget(fd, &filp);
	struct dma_fence *fence;

	if (sf == NULL)
		return NULL;
	fence = dma_fence_get(sf->fence);
	fput(filp);
	return fence;
}

static long
pd_sync_file_merge(struct sync_file *sf, unsigned long arg)
{
	struct pd_sync_merge_data data;
	struct linux_file *filp2;
	struct sync_file *sf2, *merged;
	struct dma_fence *fence;
	int fd, ret = 0;

	if (copy_from_user(&data, (void *)arg, sizeof(data)) != 0)
		return -EFAULT;
	if (data.flags != 0 || data.pad != 0)
		return -EINVAL;
	sf2 = pd_sync_file_fdget(data.fd2, &filp2);
	if (sf2 == NULL)
		return -ENOENT;
	fence = dma_fence_unwrap_merge(sf->fence, sf2->fence);
	fput(filp2);
	if (fence == NULL)
		return -ENOMEM;
	merged = sync_file_create(fence);
	dma_fence_put(fence);
	if (merged == NULL)
		return -ENOMEM;
	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		fput(merged->file);
		return fd;
	}
	data.fence = fd;
	if (copy_to_user((void *)arg, &data, sizeof(data)) != 0) {
		put_unused_fd(fd);
		fput(merged->file);
		return -EFAULT;
	}
	fd_install(fd, merged->file);
	return ret;
}

static int
pd_sync_status(struct dma_fence *fence)
{
	if (!dma_fence_is_signaled(fence))
		return 0;
	return fence->error != 0 ? fence->error : 1;
}

static long
pd_sync_file_info(struct sync_file *sf, unsigned long arg)
{
	struct pd_sync_file_info info;
	struct dma_fence_unwrap cursor;
	struct dma_fence *fence;
	uint32_t count = 0;

	if (copy_from_user(&info, (void *)arg, sizeof(info)) != 0)
		return -EFAULT;
	if (info.flags != 0 || info.pad != 0)
		return -EINVAL;
	dma_fence_unwrap_for_each(fence, &cursor, sf->fence)
		count++;
	// asked for the count only, or with room for every fence
	if (info.num_fences != 0) {
		struct pd_sync_fence_info fi;
		uint64_t at = info.sync_fence_info;

		if (info.num_fences < count)
			return -EINVAL;
		dma_fence_unwrap_for_each(fence, &cursor, sf->fence) {
			memset(&fi, 0, sizeof(fi));
			strlcpy(fi.obj_name, fence->ops->get_timeline_name(fence), sizeof(fi.obj_name));
			strlcpy(fi.driver_name, fence->ops->get_driver_name(fence), sizeof(fi.driver_name));
			fi.status = pd_sync_status(fence);
			if (copy_to_user((void *)(uintptr_t)at, &fi, sizeof(fi)) != 0)
				return -EFAULT;
			at += sizeof(fi);
		}
	}
	strlcpy(info.name, "dma_fence", sizeof(info.name));
	info.status = pd_sync_status(sf->fence);
	info.num_fences = count;
	return copy_to_user((void *)arg, &info, sizeof(info)) != 0 ? -EFAULT : 0;
}

static long
pd_sync_file_ioctl(struct linux_file *filp, unsigned int cmd, unsigned long arg)
{
	struct sync_file *sf = filp->private_data;

	switch (cmd) {
	case PD_SYNC_IOC_MERGE:
		return pd_sync_file_merge(sf, arg);
	case PD_SYNC_IOC_FILE_INFO:
		return pd_sync_file_info(sf, arg);
	}
	return -ENOTTY;
}

static const struct file_operations pd_sync_file_fops = {
	.release = pd_sync_file_release,
	.poll = pd_sync_file_poll,
	.unlocked_ioctl = pd_sync_file_ioctl,
};
