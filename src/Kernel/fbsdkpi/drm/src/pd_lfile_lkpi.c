// SPDX-License-Identifier: MIT
// linux files behind xnu's pd_lfile descriptors (dma-buf, sync_file, drm syncobj)
// each descriptor holds one linux_file count, the last fput releases the file
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/refcount.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define LINUX_IOCTL_MIN_PTR	0x10000UL

struct pd_lfile_ops {
	void (*hold)(void *obj);
	void (*release)(void *obj);
	int (*poll)(void *obj, int which);
	int (*ioctl)(void *obj, unsigned int cmd, void *data, size_t len);
	long (*read)(void *obj, void *buffer, size_t size, int nonblock);
	// the new offset or a negative errno, like linux llseek
	long long (*seek)(void *obj, long long offset, int whence);
};

int pd_lfile_register(const struct pd_lfile_ops *ops);
int pd_lfile_reserve(int cloexec, int *fdp);
void pd_lfile_unreserve(int fd);
void pd_lfile_install(int fd, void *obj);
void *pd_lfile_get(int fd);

void
pd_lkpi_fput(struct linux_file *filp)
{
	if (filp == NULL || !refcount_release(&filp->f_count))
		return;
	linux_set_current(curthread);
	if (filp->f_op != NULL && filp->f_op->release != NULL)
		filp->f_op->release(NULL, filp);
	linux_file_free(filp);
}

struct linux_file *
pd_lkpi_fget(unsigned int fd)
{
	return pd_lfile_get((int)fd);
}

int
pd_lkpi_get_unused_fd_flags(int flags)
{
	int fd, error;

	error = pd_lfile_reserve((flags & O_CLOEXEC) != 0, &fd);
	return error != 0 ? -error : fd;
}

void
pd_lkpi_put_unused_fd(unsigned int fd)
{
	pd_lfile_unreserve((int)fd);
}

// the descriptor takes over the caller's count on filp
void
pd_lkpi_fd_install(unsigned int fd, struct linux_file *filp)
{
	pd_lfile_install((int)fd, filp);
}

static void
pd_lfile_hold(void *obj)
{
	struct linux_file *filp = obj;

	refcount_acquire(&filp->f_count);
}

static void
pd_lfile_release(void *obj)
{
	pd_lkpi_fput(obj);
}

// readiness only, as for the dri nodes: a select without events polls again
static int
pd_lfile_poll(void *obj, int which)
{
	struct linux_file *filp = obj;
	unsigned int mask;

	if (filp->f_op->poll == NULL)
		return 1;
	linux_set_current(curthread);
	mask = filp->f_op->poll(filp, NULL);
	return (mask & (POLLIN | POLLRDNORM | POLLOUT | POLLERR)) != 0;
}

static int
pd_lfile_ioctl(void *obj, unsigned int cmd, void *data, size_t len)
{
	struct linux_file *filp = obj;
	struct task_struct *task;
	long ret;

	if (filp->f_op->unlocked_ioctl == NULL)
		return ENOTTY;
	linux_set_current(curthread);
	task = current;
	task->bsd_ioctl_data = data;
	task->bsd_ioctl_len = len;
	ret = filp->f_op->unlocked_ioctl(filp, cmd, LINUX_IOCTL_MIN_PTR);
	task->bsd_ioctl_data = NULL;
	task->bsd_ioctl_len = 0;
	// linux ioctls return 0 or -errno, anything positive is a driver bug
	if (ret > 0) {
		printf("pd_lfile: ioctl 0x%08x returned %ld, failing it\n", cmd, ret);
		return EINVAL;
	}
	return (int)-ret;
}

static long long
pd_lfile_seek(void *obj, long long offset, int whence)
{
	struct linux_file *filp = obj;

	if (filp->f_op->llseek == NULL)
		return -ESPIPE;
	return filp->f_op->llseek(filp, offset, whence);
}

static const struct pd_lfile_ops pd_lfile_lkpi_ops = {
	.hold = pd_lfile_hold,
	.release = pd_lfile_release,
	.poll = pd_lfile_poll,
	.ioctl = pd_lfile_ioctl,
	.seek = pd_lfile_seek,
};

void pd_lfile_lkpi_init(void);

void
pd_lfile_lkpi_init(void)
{
	pd_lfile_register(&pd_lfile_lkpi_ops);
}
