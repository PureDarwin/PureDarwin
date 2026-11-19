// pd_uvm_glue.c - PDNVUVM's linuxkpi side: the GPU's pci_dev, nv.c's globals, and UVM's character device for Darwin
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <sys/bus.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "nv-linux.h"
#include "nv-pci.h"
#include "nv-proto.h"
#include "uvm_ioctl.h"
#include "uvm_linux_ioctl.h"

void pd_lkpi_init_hardware(void);
// PDNVLoader's: where the GPU's interrupts reach nvidia-uvm
void pdnv_uvm_set_isr(void *fn);

HAIKU_FBSD_DRIVERS_GLUE(nvidia_uvm);
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

// the resource manager in PDNVLoader takes the GPU's interrupts, nothing here does
int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 0;
}

void
__haiku_init_hardware()
{
	pd_lkpi_init_hardware();
}

// nv.c's globals that nvidia-uvm and nv_uvm_interface.c read: the RM call stacks come from this cache, and PD has
// neither ATS nor confidential computing
void *nvidia_stack_t_cache;

// the one placeholder every nvidia-uvm procfs entry is (pd_uvm_linux.h)
char pd_uvm_proc_entry;
NvBool nv_ats_supported = NV_FALSE;
NvBool os_cc_enabled = NV_FALSE;

// the GPUs' linux pci_devs, one per GPU PDNVUVM has attached, found again by PCI location
#define PD_UVM_MAX_GPUS 32
static struct pci_dev *pd_uvm_pdevs[PD_UVM_MAX_GPUS];
static struct pci_dev *pd_uvm_claimed;

static int
pd_uvm_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int i;

	(void)id;
	for (i = 0; i < PD_UVM_MAX_GPUS; i++) {
		if (pd_uvm_pdevs[i] == NULL) {
			pd_uvm_pdevs[i] = pdev;
			break;
		}
	}
	pd_uvm_claimed = pdev;
	return 0;
}

static void
pd_uvm_remove(struct pci_dev *pdev)
{
	int i;

	for (i = 0; i < PD_UVM_MAX_GPUS; i++)
		if (pd_uvm_pdevs[i] == pdev)
			pd_uvm_pdevs[i] = NULL;
}

// any NVIDIA function: the attach is only ever pointed at a GPU PDNVLoader already drives
static const struct pci_device_id pd_uvm_ids[] = {
	{ .vendor = 0x10de, .device = PCI_ANY_ID, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID },
	{ 0 }
};

static struct pci_driver pd_uvm_driver = {
	.name = "nvidia-uvm-pd",
	.id_table = pd_uvm_ids,
	.probe = pd_uvm_probe,
	.remove = pd_uvm_remove,
};

// the stack cache comes first: nvidia-uvm's own init (a module_init, SI_SUB_OFED_MODINIT) already calls into the
// resource manager, so this runs in the subsystem before it
static int __init
pd_uvm_glue_init(void)
{
	int err;

	nvidia_stack_t_cache = kmem_cache_create("nvidia_stack_t", sizeof(nvidia_stack_t), 0, 0, NULL);
	if (nvidia_stack_t_cache == NULL)
		return -ENOMEM;
	// nv_uvm_interface.c's own setup, which nvidia.ko's module init runs on Linux: the lock its callbacks are kept under
	err = nv_uvm_init();
	if (err != 0)
		return err;
	// the GPU's interrupts reach nvidia-uvm through PDNVLoader's ISR, as nv.c calls it on Linux
	pdnv_uvm_set_isr((void *)nv_uvm_event_interrupt);
	err = pci_register_driver(&pd_uvm_driver);
	if (err != 0)
		printf("PDNVUVM: pci driver not registered (%d)\n", err);
	return err;
}
SYSINIT(pd_uvm_glue_init, SI_SUB_OFED_PREINIT, SI_ORDER_ANY, _module_run, pd_uvm_glue_init);

// whether the last attach found a GPU, for the IOKit side
int
pd_uvm_claimed_gpu(void)
{
	int found = pd_uvm_claimed != NULL;

	pd_uvm_claimed = NULL;
	return found;
}

int pdnv_uvm_gpu_location(const NvU8 *uuid, NvU32 *domain, NvU8 *bus, NvU8 *slot, NvU8 *function,
    NvU64 *dma_start, NvU64 *dma_limit);

// nv-pci.c's: the GPU with this UUID as a linux pci_dev, and the DMA range it can address
int
nvidia_dev_get_pci_info(const NvU8 *uuid, struct pci_dev **pci_dev_out, NvU64 *dma_start, NvU64 *dma_limit)
{
	NvU32 domain;
	NvU8 bus, slot, function;
	int i, err;

	err = pdnv_uvm_gpu_location(uuid, &domain, &bus, &slot, &function, dma_start, dma_limit);
	if (err != 0)
		return err;
	for (i = 0; i < PD_UVM_MAX_GPUS; i++) {
		struct pci_dev *pdev = pd_uvm_pdevs[i];

		if (pdev != NULL && pci_domain_nr(pdev->bus) == (int)domain && pdev->bus->number == bus &&
		    PCI_SLOT(pdev->devfn) == slot && PCI_FUNC(pdev->devfn) == function) {
			*pci_dev_out = pdev;
			return 0;
		}
	}
	return -ENODEV;
}

// nvidia-uvm's character device. cdev_add on PD has no devfs to make a node in, so the file operations are kept
// here and the IOKit side makes /dev/nvidia-uvm, whose opens and ioctls come through the calls below
// nvidia-uvm adds two: minor 0, /dev/nvidia-uvm, and minor 1, /dev/nvidia-uvm-tools, each with its own operations
#define PD_UVM_MINORS 2
static const struct file_operations *pd_uvm_fops[PD_UVM_MINORS];

int
pd_uvm_cdev_add(struct linux_cdev *cdev, dev_t dev, unsigned count)
{
	unsigned int minor = MINOR(dev);

	(void)count;
	if (minor >= PD_UVM_MINORS)
		return -EINVAL;
	cdev->dev = dev;
	pd_uvm_fops[minor] = cdev->ops;
	return 0;
}

void
pd_uvm_cdev_del(struct linux_cdev *cdev)
{
	unsigned int minor = MINOR(cdev->dev);

	if (minor < PD_UVM_MINORS && pd_uvm_fops[minor] == cdev->ops)
		pd_uvm_fops[minor] = NULL;
}

void *pd_uvm_file_for_fd(int fd);

// fget for nvidia-uvm: its own descriptors are found through the IOKit side, the others as the shared layer has them.
// The reference taken is the one the caller's fput gives back
struct linux_file *
pd_uvm_fget(unsigned int fd)
{
	struct linux_file *filp = pd_uvm_file_for_fd((int)fd);

	if (filp == NULL)
		return pd_lkpi_fget(fd);
	refcount_acquire(&filp->f_count);
	return filp;
}

// linux code asks an inode for its minor, and nvidia-uvm also for its page cache
struct pd_uvm_inode {
	struct vnode vn;
	unsigned int minor;
	struct address_space mapping;
};

int
pd_uvm_open(unsigned int minor, int flags, void **cookie)
{
	struct pd_uvm_inode *pi;
	struct linux_file *filp;
	int ret;

	const struct file_operations *fops = minor < PD_UVM_MINORS ? pd_uvm_fops[minor] : NULL;

	if (fops == NULL || fops->open == NULL)
		return ENXIO;
	pi = kzalloc(sizeof(*pi), GFP_KERNEL);
	filp = kzalloc(sizeof(*filp), GFP_KERNEL);
	if (pi == NULL || filp == NULL) {
		kfree(pi);
		kfree(filp);
		return ENOMEM;
	}
	pi->minor = minor;
	pi->vn.i_mapping = &pi->mapping;
	filp->f_op = fops;
	filp->f_flags = flags;
	filp->f_count = 1;
	filp->f_vnode = &pi->vn;

	linux_set_current(curthread);
	ret = fops->open((struct inode *)&pi->vn, filp);
	if (ret != 0) {
		kfree(filp);
		kfree(pi);
		return -ret;
	}
	*cookie = filp;
	return 0;
}

void
pd_uvm_close(void *cookie)
{
	struct linux_file *filp = cookie;
	struct vnode *vn = filp->f_vnode;

	linux_set_current(curthread);
	if (filp->f_op->release != NULL)
		filp->f_op->release((struct inode *)vn, filp);
	kfree(filp);
	kfree(vn);
}

// the result an nvidia-uvm request carries back in its parameters, rmStatus, which the ioctl's own return does not
// show: logged with pduvmdebug for the requests CUDA's start makes
// only the status field is read: the parameter structs are large (a per-GPU array in some), several on the stack at once
// overflow it
#define PD_UVM_STATUS(name) \
	case name: { \
		NV_STATUS st; \
		if (copy_from_user(&st, (void __user *)(uarg + offsetof(name##_PARAMS, rmStatus)), sizeof(st)) == 0) \
			printf("PDNVUVM: " #name " returned %ld, rmStatus 0x%x\n", ret, st); \
		return; \
	}

bool uvm_debug_prints_enabled(void);   // pd_uvm_common.c, the pduvmdebug boot-arg

static void
pd_uvm_log_status(unsigned int cmd, unsigned long uarg, long ret)
{
	switch (cmd) {
	PD_UVM_STATUS(UVM_INITIALIZE)
	PD_UVM_STATUS(UVM_MM_INITIALIZE)
	PD_UVM_STATUS(UVM_PAGEABLE_MEM_ACCESS)
	PD_UVM_STATUS(UVM_REGISTER_GPU)
	PD_UVM_STATUS(UVM_DISABLE_SYSTEM_WIDE_ATOMICS)
	PD_UVM_STATUS(UVM_CREATE_RANGE_GROUP)
	PD_UVM_STATUS(UVM_DESTROY_RANGE_GROUP)
	PD_UVM_STATUS(UVM_REGISTER_GPU_VASPACE)
	PD_UVM_STATUS(UVM_CREATE_EXTERNAL_RANGE)
	PD_UVM_STATUS(UVM_MAP_EXTERNAL_ALLOCATION)
	PD_UVM_STATUS(UVM_REGISTER_CHANNEL)
	PD_UVM_STATUS(UVM_ALLOC_SEMAPHORE_POOL)
	PD_UVM_STATUS(UVM_FREE)
	default:
		printf("PDNVUVM: uvm request 0x%x returned %ld\n", cmd, ret);
	}
}

// an ioctl with nvidia-uvm's own number and its argument's real address in the caller: copy_from_user and
// copy_to_user read and write the calling process directly
int
pd_uvm_ioctl(void *cookie, unsigned int cmd, unsigned long uarg)
{
	struct linux_file *filp = cookie;
	long ret;

	if (filp->f_op->unlocked_ioctl == NULL)
		return ENOTTY;
	linux_set_current(curthread);
	ret = filp->f_op->unlocked_ioctl(filp, cmd, uarg);
	if (uvm_debug_prints_enabled())
		pd_uvm_log_status(cmd, uarg, ret);
	if (ret > 0)
		return EINVAL;
	return (int)-ret;
}
