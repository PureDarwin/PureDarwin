// SPDX-License-Identifier: MIT
// the linuxkpi side of /dev/dri, each open is a linux file on the drm minor
// ioctl and read arguments reach linux code through the fake user address window
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <drm/drm_device.h>

#include "../../pd_dri_cdev.h"

#define LINUX_IOCTL_MIN_PTR	0x10000UL

// linux code only asks the inode for its minor
struct pd_dri_inode {
	uint32_t magic;
	unsigned int minor;
};
#define PD_DRI_INODE_MAGIC	0x70646969

static const struct file_operations *pd_dri_fops;

void pd_lfile_lkpi_init(void);

// the gem mmap resolver, set by the driver glue
int (*pd_drm_mmap_pages)(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg);

unsigned int
linux_iminor(struct inode *inode)
{
	struct pd_dri_inode *pi = (struct pd_dri_inode *)inode;

	return pi != NULL && pi->magic == PD_DRI_INODE_MAGIC ? pi->minor : 0;
}

static int
pd_dri_open(int drm_minor, int flags, void **cookie)
{
	struct pd_dri_inode *pi;
	struct linux_file *filp;
	int ret;

	if (pd_dri_fops == NULL || pd_dri_fops->open == NULL)
		return ENXIO;
	pi = kzalloc(sizeof(*pi), GFP_KERNEL);
	filp = kzalloc(sizeof(*filp), GFP_KERNEL);
	if (pi == NULL || filp == NULL) {
		kfree(pi);
		kfree(filp);
		return ENOMEM;
	}
	pi->magic = PD_DRI_INODE_MAGIC;
	pi->minor = drm_minor;
	filp->f_op = pd_dri_fops;
	filp->f_flags = flags;
	filp->f_count = 1;
	filp->f_vnode = (struct vnode *)pi;

	linux_set_current(curthread);
	ret = pd_dri_fops->open((struct inode *)pi, filp);
	if (ret != 0) {
		kfree(filp);
		kfree(pi);
		return -ret;
	}
	*cookie = filp;
	return 0;
}

static void
pd_dri_close(void *cookie)
{
	struct linux_file *filp = cookie;
	struct pd_dri_inode *pi = (struct pd_dri_inode *)filp->f_vnode;

	linux_set_current(curthread);
	if (filp->f_op->release != NULL)
		filp->f_op->release((struct inode *)pi, filp);
	kfree(filp);
	kfree(pi);
}

static long
pd_dri_read(void *cookie, void *buffer, size_t size, int nonblock)
{
	struct linux_file *filp = cookie;
	struct task_struct *task;
	off_t pos = 0;
	long ret;

	if (filp->f_op->read == NULL)
		return -EINVAL;
	linux_set_current(curthread);
	task = current;
	if (nonblock)
		filp->f_flags |= O_NONBLOCK;
	else
		filp->f_flags &= ~O_NONBLOCK;
	task->bsd_ioctl_data = buffer;
	task->bsd_ioctl_len = size;
	ret = filp->f_op->read(filp, (char *)LINUX_IOCTL_MIN_PTR, size, &pos);
	task->bsd_ioctl_data = NULL;
	task->bsd_ioctl_len = 0;
	return ret;
}

long pd_drm_tunable_long(const char *path, long def);

static int
pd_dri_ioctl(void *cookie, unsigned int cmd, void *data, size_t len)
{
	// pd_dri_trace=1 logs every ioctl before and after, to find the one a client hangs in
	static int trace = -1;
	struct linux_file *filp = cookie;
	struct task_struct *task;
	long ret;

	if (trace < 0)
		trace = (int)pd_drm_tunable_long("pd_dri.trace", 0);

	if (filp->f_op->unlocked_ioctl == NULL)
		return ENOTTY;
	linux_set_current(curthread);
	task = current;
	task->bsd_ioctl_data = data;
	task->bsd_ioctl_len = len;
	if (trace)
		printf("pd_dri: ioctl 0x%08x len %zu\n", cmd, len);
	ret = filp->f_op->unlocked_ioctl(filp, cmd, LINUX_IOCTL_MIN_PTR);
	task->bsd_ioctl_data = NULL;
	task->bsd_ioctl_len = 0;
	if (trace)
		printf("pd_dri: ioctl 0x%08x -> %ld\n", cmd, ret);
	// linux ioctls return 0 or -errno, anything positive is a driver bug
	if (ret > 0) {
		printf("pd_dri: ioctl 0x%08x returned %ld, failing it\n", cmd, ret);
		return EINVAL;
	}
	return (int)-ret;
}

// readiness only, a select without events polls again
static int
pd_dri_poll(void *cookie, int which, void *wql)
{
	struct linux_file *filp = cookie;
	unsigned int mask;

	if (filp->f_op->poll == NULL)
		return 1;
	linux_set_current(curthread);
	mask = filp->f_op->poll(filp, NULL);
	return (mask & (POLLIN | POLLRDNORM | POLLOUT)) != 0;
}

static int
pd_dri_mmap_pages(void *cookie, off_t offset, size_t size, int prot, uint32_t *pages,
    unsigned int *wimg, void (**release)(void *), void **release_arg)
{
	int ret;

	if (pd_drm_mmap_pages == NULL)
		return ENODEV;
	linux_set_current(curthread);
	ret = pd_drm_mmap_pages(cookie, offset, size, prot, pages, wimg, release, release_arg);
	return ret < 0 ? -ret : ret;
}

static const struct pd_dri_ops pd_dri_ops = {
	.open = pd_dri_open,
	.close = pd_dri_close,
	.read = pd_dri_read,
	.ioctl = pd_dri_ioctl,
	.poll = pd_dri_poll,
	.mmap_pages = pd_dri_mmap_pages,
};

// drm_core_init registers the drm major, here it becomes the dri cdev
int
__register_chrdev_p(unsigned int major, unsigned int baseminor,
    unsigned int count, const char *name, const struct file_operations *fops,
    uid_t uid, gid_t gid, int mode)
{
	pd_dri_fops = fops;
	pd_lfile_lkpi_init();
	return -pd_dri_register(&pd_dri_ops);
}

// called for each drm minor drm_dev_alias names, card0 or renderD128
int pd_drm_dev_node(const char *minor_fmt, int index, struct drm_device *dev);

int
pd_drm_dev_node(const char *minor_fmt, int index, struct drm_device *dev)
{
	struct pd_dri_pci info, *pci = NULL;
	struct pci_dev *pdev;
	char name[32];

	snprintf(name, sizeof(name), minor_fmt, index);
	// a device tree device: its node name and compatible, never read as a pci device
	if (dev != NULL && dev->dev != NULL && pd_dev_is_platform(dev->dev)) {
		struct pd_dri_platform plat = { { 0 }, { 0 } };
		const char *compat = NULL;

		if (dev->dev->of_node != NULL) {
			strlcpy(plat.fullname, dev->dev->of_node->full_name, sizeof(plat.fullname));
			if (of_property_read_string(dev->dev->of_node, "compatible", &compat) == 0)
				strlcpy(plat.compatible, compat, sizeof(plat.compatible));
		}
		return pd_dri_make_platform_node(&pd_dri_ops, index, name, &plat);
	}
	// otherwise a pci device, and dev_is_pci asks the freebsd device class, which the
	// haiku layer does not set
	pdev = dev != NULL && dev->dev != NULL ? to_pci_dev(dev->dev) : NULL;
	if (pdev != NULL && pdev->vendor != 0 && pdev->vendor != 0xffff) {
		info.domain = pci_domain_nr(pdev->bus);
		info.bus = pdev->bus->number;
		info.dev = PCI_SLOT(pdev->devfn);
		info.func = PCI_FUNC(pdev->devfn);
		info.vendor = pdev->vendor;
		info.device = pdev->device;
		info.subvendor = pdev->subsystem_vendor;
		info.subdevice = pdev->subsystem_device;
		info.revision = pdev->revision;
		pci = &info;
	}
	return pd_dri_make_node(&pd_dri_ops, index, name, pci);
}
