// pdnv_dev.c - /dev/nvidiactl and /dev/nvidiaN: opens, the interface layer's own ioctls, the rest to rm_ioctl
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <miscfs/devfs/devfs.h>

#include <nvtypes.h>
#include <nvstatus.h>
#include <os-interface.h>
#include <nv.h>
#include <nv-ioctl.h>
#include <nv-ioctl-numbers.h>

#include "pdnv_port.h"
#include "pdnv_pci.h"

int pdnv_stack_alloc(nvidia_stack_t **sp);
void pdnv_stack_free(nvidia_stack_t *sp);

typedef NV_STATUS (*rm_ioctl_fn)(nvidia_stack_t *, nv_state_t *, nv_file_private_t *, NvU32, void *, NvU32);
typedef void (*rm_cleanup_fn)(nvidia_stack_t *, nv_state_t *, nv_file_private_t *);
typedef NV_STATUS (*rm_version_fn)(nvidia_stack_t *, void *, NvU32);
typedef NvBool (*rm_locks_fn)(nvidia_stack_t *, nv_state_t *);

// a clone minor is (open slot << 8) | node, node 255 is the control device and 0-7 are GPUs, as on Linux
#define PDNV_CTL_NODE       255
#define PDNV_MAX_GPUS       8
#define PDNV_SLOTS          1024
#define SLOT(dev)           ((minor(dev) >> 8) & 0xffff)
#define NODE(dev)           (minor(dev) & 0xff)
#define PDNV_EVENTS         64

// bsd/sys/conf.h keeps the flag to the BSD kernel, cdevsw_setkqueueok itself is exported
#ifndef CDEVSW_SELECT_KQUEUE
#define CDEVSW_SELECT_KQUEUE 0x01
#endif

// what nv_linux_file_private_t is on Linux: the resource manager's part first, then ours
struct pdnv_file {
	nv_file_private_t nvfp;
	nv_state_t *nv;
	pid_t pid;                  // the opener, for nv_is_gpu_accessible
	NvU32 *attached_gpus;
	size_t num_attached_gpus;
	nv_alloc_mapping_context_t mmap;    // what the next mmap of this descriptor maps, set by the RM

	// RM events for this descriptor, as nvlfp's event list: a ring, posting may not sleep
	IOSimpleLock *ev_lock;
	nv_event_t ev[PDNV_EVENTS];
	unsigned ev_head, ev_count;
	NvBool dataless_pending;
	struct selinfo ev_sel;
};

static int sMajor = -1;
static IOLock *sLock;
static struct pdnv_file *sSlots[PDNV_SLOTS];
static unsigned char sSlotUsed[PDNV_SLOTS];

// the control device has a state of its own, as nv_ctl_device
static nv_state_t sCtl;
static nv_state_t *sGPUs[PDNV_MAX_GPUS];
static unsigned sNumGPUs;
static NvU64 sMemblockSize;

nv_state_t * NV_API_CALL
nv_get_ctl_state(void)
{
	return &sCtl;
}

static nv_state_t *
node_state(dev_t dev)
{
	if (NODE(dev) == PDNV_CTL_NODE)
		return &sCtl;
	return NODE(dev) < PDNV_MAX_GPUS ? sGPUs[NODE(dev)] : NULL;
}

static struct pdnv_file *
slot_file(dev_t dev)
{
	int slot = SLOT(dev);

	return slot > 0 && slot < PDNV_SLOTS ? sSlots[slot] : NULL;
}

static int
nv_clone(dev_t dev, int action)
{
	int slot;

	if (action == DEVFS_CLONE_ALLOC) {
		IOLockLock(sLock);
		for (slot = 1; slot < PDNV_SLOTS && sSlotUsed[slot]; slot++)
			;
		if (slot < PDNV_SLOTS)
			sSlotUsed[slot] = 1;
		IOLockUnlock(sLock);
		return slot < PDNV_SLOTS ? (slot << 8) | NODE(dev) : -1;
	}
	slot = SLOT(dev);
	if (slot > 0 && slot < PDNV_SLOTS) {
		IOLockLock(sLock);
		sSlotUsed[slot] = 0;
		sSlots[slot] = NULL;
		IOLockUnlock(sLock);
	}
	return 0;
}

static int
nv_open(dev_t dev, int flags, int devtype, proc_t p)
{
	nv_state_t *nv = node_state(dev);
	int slot = SLOT(dev);
	struct pdnv_file *f;

	(void)flags;
	(void)devtype;
	if (nv == NULL || slot <= 0 || slot >= PDNV_SLOTS)
		return ENXIO;
	// a GPU is started at probe and stays up, as with persistence mode on Linux
	if (nv != &sCtl && (nv->flags & NV_FLAG_OPEN) == 0)
		return EIO;
	f = IOMallocZero(sizeof(*f));
	if (f == NULL)
		return ENOMEM;
	f->nv = nv;
	f->pid = proc_pid(p);
	f->ev_lock = IOSimpleLockAlloc();
	if (f->ev_lock == NULL) {
		IOFree(f, sizeof(*f));
		return ENOMEM;
	}
	IOLockLock(sLock);
	sSlots[slot] = f;
	IOLockUnlock(sLock);
	return 0;
}

static int
nv_close(dev_t dev, int flags, int devtype, proc_t p)
{
	struct pdnv_file *f = slot_file(dev);
	nvidia_stack_t *sp = NULL;

	(void)flags;
	(void)devtype;
	(void)p;
	if (f == NULL)
		return 0;
	// posts still running against this descriptor finish first, as nvidia_close waits for them
	while (__atomic_load_n(&f->nvfp.event_posting_refcount, __ATOMIC_ACQUIRE) != 0)
		IOSleep(1);
	if (pdnv_rm.cleanup_file_private != NULL && pdnv_stack_alloc(&sp) == 0) {
		((rm_cleanup_fn)pdnv_rm.cleanup_file_private)(sp, f->nv, &f->nvfp);
		pdnv_stack_free(sp);
	}
	if (f->attached_gpus != NULL)
		IOFree(f->attached_gpus, f->num_attached_gpus * sizeof(NvU32));
	if (f->mmap.valid) {
		if (f->mmap.alloc != NULL)
			pdnv_alloc_release(f->mmap.alloc);
		if (f->mmap.memArea.pRanges != NULL)
			os_free_mem(f->mmap.memArea.pRanges);
	}
	IOLockLock(sLock);
	sSlots[SLOT(dev)] = NULL;
	IOLockUnlock(sLock);
	selthreadclear(&f->ev_sel);
	IOSimpleLockFree(f->ev_lock);
	IOFree(f, sizeof(*f));
	return 0;
}

static int
read_card_info(nv_ioctl_card_info_t *ci, size_t n)
{
	bzero(ci, n * sizeof(ci[0]));
	if (n < sNumGPUs)
		return EINVAL;
	for (unsigned i = 0; i < sNumGPUs; i++) {
		nv_state_t *nv = sGPUs[i];

		ci[i].valid = NV_TRUE;
		ci[i].pci_info.domain = nv->pci_info.domain;
		ci[i].pci_info.bus = nv->pci_info.bus;
		ci[i].pci_info.slot = nv->pci_info.slot;
		ci[i].pci_info.vendor_id = nv->pci_info.vendor_id;
		ci[i].pci_info.device_id = nv->pci_info.device_id;
		ci[i].gpu_id = nv->gpu_id;
		ci[i].interrupt_line = nv->interrupt_line;
		ci[i].reg_address = nv->regs->cpu_address;
		ci[i].reg_size = nv->regs->size;
		ci[i].fb_address = nv->fb->cpu_address;
		ci[i].fb_size = nv->fb->size;
		ci[i].minor_number = i;
	}
	return 0;
}

// one ioctl, as nvidia_ioctl: the argument is already in the kernel, unless it came through XFER
static int
dispatch(struct pdnv_file *f, NvU32 cmd, void *arg, NvU32 size)
{
	nv_state_t *nv = f->nv;
	nvidia_stack_t *sp = NULL;
	NV_STATUS st;
	int error = 0;

	if (cmd == NV_ESC_WAIT_OPEN_COMPLETE) {
		nv_ioctl_wait_open_complete_t *w = arg;

		if (size != sizeof(*w))
			return EINVAL;
		w->rc = 0;
		w->adapterStatus = NV_OK;
		return 0;
	}
	if (pdnv_stack_alloc(&sp) != 0)
		return ENOMEM;

	switch (cmd) {
	case NV_ESC_CARD_INFO:
		error = nv == &sCtl ? read_card_info(arg, size / sizeof(nv_ioctl_card_info_t)) : EINVAL;
		break;
	case NV_ESC_ATTACH_GPUS_TO_FD:
		// the GPUs stay started, attaching only records them
		if (nv != &sCtl || size == 0 || size % sizeof(NvU32) != 0 || f->attached_gpus != NULL) {
			error = EINVAL;
			break;
		}
		f->attached_gpus = IOMalloc(size);
		if (f->attached_gpus == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(arg, f->attached_gpus, size);
		f->num_attached_gpus = size / sizeof(NvU32);
		break;
	case NV_ESC_CHECK_VERSION_STR:
		if (nv != &sCtl) {
			error = EINVAL;
			break;
		}
		st = ((rm_version_fn)pdnv_rm.perform_version_check)(sp, arg, size);
		error = st == NV_OK ? 0 : EINVAL;
		break;
	case NV_ESC_SYS_PARAMS: {
		nv_ioctl_sys_params_t *api = arg;

		if (nv != &sCtl || size != sizeof(*api)) {
			error = EINVAL;
			break;
		}
		if (sMemblockSize == 0)
			sMemblockSize = api->memblock_size;
		else if (sMemblockSize != api->memblock_size)
			error = EBUSY;
		break;
	}
	default:
		st = ((rm_ioctl_fn)pdnv_rm.ioctl)(sp, nv, &f->nvfp, cmd, arg, size);
		error = st == NV_OK ? 0 : EINVAL;
		if (st != NV_OK)
			nv_printf(NV_DBG_ERRORS, "NVRM: ioctl 0x%x (0x%x bytes) on %s: 0x%x\n", cmd, size,
			    nv == &sCtl ? "nvidiactl" : "nvidia0", st);
		break;
	}
	pdnv_stack_free(sp);
	return error;
}

static int
nv_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, proc_t p)
{
	struct pdnv_file *f = slot_file(dev);
	NvU32 nr = (NvU32)(cmd & 0xff), size = (NvU32)IOCPARM_LEN(cmd);
	nv_ioctl_xfer_t *x;
	void *buf;
	int error;

	(void)fflag;
	(void)p;
	if (f == NULL)
		return ENOTTY;
	// fcntl's O_NONBLOCK and O_ASYNC reach a device as these, Linux never hands them to the driver
	if (cmd == FIONBIO || cmd == FIOASYNC)
		return 0;
	if (((cmd >> 8) & 0xff) != NV_IOCTL_MAGIC)
		return ENOTTY;
	if (nr != NV_ESC_IOCTL_XFER_CMD)
		return dispatch(f, nr, data, size);

	// a larger argument: the kernel copied only the descriptor, the rest comes from user space
	if (size != sizeof(*x))
		return EINVAL;
	x = (nv_ioctl_xfer_t *)data;
	if (x->size > NV_ABSOLUTE_MAX_IOCTL_SIZE)
		return EINVAL;
	buf = IOMallocZero(x->size);
	if (buf == NULL)
		return ENOMEM;
	error = copyin((user_addr_t)x->ptr, buf, x->size);
	if (error == 0)
		error = dispatch(f, x->cmd, buf, x->size);
	if (error == 0)
		error = copyout(buf, (user_addr_t)x->ptr, x->size);
	IOFree(buf, x->size);
	return error;
}

// x86 cache modes for pd_cdev_mmap, as VM_WIMG_*
#define PDNV_WIMG_DEFAULT   0x2
#define PDNV_WIMG_WCOMB     0x6
#define PDNV_WIMG_IO        0x7

NvBool pdnv_gpu_safe_to_mmap(nv_state_t *nv);
int pd_cdev_mmap_register(int major, int (*fn)(dev_t, off_t, vm_size_t, int, ppnum_t *, unsigned int *,
    void (**)(void *), void **));

// the pages behind an mmap of a descriptor, as nvidia_mmap_helper: BAR ranges on a GPU node, system
// memory on the control node. The RM validated the mapping first, offset 0 is the only one
static int
nv_mmap_pages(dev_t dev, off_t offset, vm_size_t size, int prot, ppnum_t *pages, unsigned int *wimg,
    void (**release)(void *), void **release_arg)
{
	struct pdnv_file *f = slot_file(dev);
	const nv_alloc_mapping_context_t *m;
	NvU64 n = size >> PAGE_SHIFT, done = 0;

	(void)prot;
	if (f == NULL || !f->mmap.valid || offset != 0)
		return EINVAL;
	m = &f->mmap;

	if (f->nv != &sCtl) {
		nv_state_t *nv = f->nv;
		NvU64 total = 0;

		for (NvU64 r = 0; r < m->memArea.numRanges; r++)
			total += m->memArea.pRanges[r].size;
		if (total != size || !pdnv_gpu_safe_to_mmap(nv))
			return ENXIO;
		if (m->access_start >= nv->regs->cpu_address &&
		    m->access_start + m->access_size <= nv->regs->cpu_address + nv->regs->size)
			*wimg = PDNV_WIMG_IO;
		else
			*wimg = m->caching == NV_MEMORY_WRITECOMBINED ? PDNV_WIMG_WCOMB : PDNV_WIMG_IO;
		for (NvU64 r = 0; r < m->memArea.numRanges; r++)
			for (NvU64 p = 0; p < m->memArea.pRanges[r].size >> PAGE_SHIFT; p++)
				pages[done++] = (ppnum_t)((m->memArea.pRanges[r].start >> PAGE_SHIFT) + p);
		return 0;
	}

	if (m->alloc == NULL || m->page_index + n > pdnv_alloc_pages(m->alloc))
		return ERANGE;
	for (NvU64 i = 0; i < n; i++)
		pages[i] = (ppnum_t)(pdnv_alloc_page_pa(m->alloc, m->page_index + i) >> PAGE_SHIFT);
	switch (pdnv_alloc_cache(m->alloc)) {
	case NV_MEMORY_CACHED:
		*wimg = PDNV_WIMG_DEFAULT;
		break;
	case NV_MEMORY_WRITECOMBINED:
		*wimg = PDNV_WIMG_WCOMB;
		break;
	default:
		*wimg = PDNV_WIMG_IO;
		break;
	}
	// the pages stay until the mapping goes
	pdnv_alloc_retain(m->alloc);
	*release = pdnv_alloc_release;
	*release_arg = m->alloc;
	return 0;
}

// the RM's validated mapping, kept on the descriptor the next mmap comes through, as on Linux
NV_STATUS NV_API_CALL
nv_add_mapping_context_to_file(nv_state_t *nv, nv_usermap_access_params_t *nvuap, NvU32 prot, void *pAllocPriv,
    NvU64 pageIndex, NvU32 fd)
{
	nv_alloc_mapping_context_t *m;
	nv_file_private_t *nvfp;
	struct pdnv_file *f;
	NV_STATUS st = NV_OK;
	void *priv = NULL;

	nvfp = nv_get_file_private(fd, NV_IS_CTL_DEVICE(nv) ? NV_TRUE : NV_FALSE, &priv);
	if (nvfp == NULL)
		return NV_ERR_INVALID_ARGUMENT;
	f = (struct pdnv_file *)nvfp;
	m = &f->mmap;
	if (m->valid) {
		st = NV_ERR_STATE_IN_USE;
		goto done;
	}
	bzero(m, sizeof(*m));
	if (NV_IS_CTL_DEVICE(nv)) {
		m->alloc = pAllocPriv;
		m->page_index = pageIndex;
		pdnv_alloc_retain(pAllocPriv);
	} else {
		if (f->nv != nv) {
			st = NV_ERR_INVALID_ARGUMENT;
			goto done;
		}
		st = os_alloc_mem((void **)&m->memArea.pRanges, sizeof(MemoryRange) * nvuap->memArea.numRanges);
		if (st != NV_OK) {
			m->memArea.pRanges = NULL;
			goto done;
		}
		m->memArea.numRanges = nvuap->memArea.numRanges;
		bcopy(nvuap->memArea.pRanges, m->memArea.pRanges, sizeof(MemoryRange) * nvuap->memArea.numRanges);
		m->access_start = nvuap->access_start;
		m->access_size = nvuap->access_size;
	}
	m->prot = prot;
	m->caching = nvuap->caching;
	__atomic_store_n(&m->valid, NV_TRUE, __ATOMIC_RELEASE);
done:
	nv_put_file_private(priv);
	return st;
}

// an event the RM raises for a descriptor, as nv_post_event: with data it is queued, without it only marks
// the descriptor readable
void NV_API_CALL
nv_post_event(nv_event_t *event, NvHandle handle, NvU32 index, NvU32 info32, NvU16 info16, NvBool data_valid)
{
	struct pdnv_file *f = (struct pdnv_file *)event->nvfp;
	IOInterruptState is;
	NvBool dropped = NV_FALSE;

	is = IOSimpleLockLockDisableInterrupt(f->ev_lock);
	if (data_valid) {
		if (f->ev_count == PDNV_EVENTS) {
			dropped = NV_TRUE;
		} else {
			nv_event_t *e = &f->ev[(f->ev_head + f->ev_count++) % PDNV_EVENTS];

			*e = *event;
			e->hObject = handle;
			e->index = index;
			e->info32 = info32;
			e->info16 = info16;
		}
	} else {
		f->dataless_pending = NV_TRUE;
	}
	IOSimpleLockUnlockEnableInterrupt(f->ev_lock, is);
	if (dropped)
		nv_printf(NV_DBG_ERRORS, "NVRM: event queue full, event for 0x%x dropped\n", handle);
	selwakeup(&f->ev_sel);
}

// the oldest queued event, as nv_get_event
NvS32 NV_API_CALL
nv_get_event(nv_file_private_t *nvfp, nv_event_t *event, NvU32 *pending)
{
	struct pdnv_file *f = (struct pdnv_file *)nvfp;
	IOInterruptState is;
	NvS32 st = NV_ERR_GENERIC;

	is = IOSimpleLockLockDisableInterrupt(f->ev_lock);
	if (f->ev_count != 0) {
		*event = f->ev[f->ev_head];
		f->ev_head = (f->ev_head + 1) % PDNV_EVENTS;
		f->ev_count--;
		*pending = f->ev_count != 0;
		st = NV_OK;
	}
	IOSimpleLockUnlockEnableInterrupt(f->ev_lock, is);
	return st;
}

// readable while an event is pending, as nvidia_poll's POLLIN | POLLPRI. Reporting a dataless one clears it
static int
nv_select(dev_t dev, int which, void *wql, proc_t p)
{
	struct pdnv_file *f = slot_file(dev);
	IOInterruptState is;
	int ready = 0;

	if (f == NULL)
		return 1;
	if (which != FREAD)
		return 0;
	// recorded before the check, so a post between the two still wakes the waiter
	selrecord(p, &f->ev_sel, wql);
	is = IOSimpleLockLockDisableInterrupt(f->ev_lock);
	if (f->ev_count != 0 || f->dataless_pending) {
		f->dataless_pending = NV_FALSE;
		ready = 1;
	}
	IOSimpleLockUnlockEnableInterrupt(f->ev_lock, is);
	return ready;
}

static struct cdevsw sCdevsw = {
	.d_open = nv_open,
	.d_close = nv_close,
	.d_read = eno_rdwrt,
	.d_write = eno_rdwrt,
	.d_ioctl = nv_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = NULL,
	.d_select = nv_select,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0,
};

// /dev/nvidia-caps/nvidia-cap1 and -cap2, the MIG config and monitor capabilities, as nv-caps makes them on
// Linux. NVIDIA's libraries expect the nodes, opening one grants nothing here: there is no MIG
static int sCapMajor = -1;

static int
cap_open(dev_t dev, int flags, int devtype, proc_t p)
{
	(void)flags;
	(void)devtype;
	(void)p;
	return minor(dev) == 1 || minor(dev) == 2 ? 0 : ENXIO;
}

static int
cap_close(dev_t dev, int flags, int devtype, proc_t p)
{
	(void)dev;
	(void)flags;
	(void)devtype;
	(void)p;
	return 0;
}

static struct cdevsw sCapCdevsw = {
	.d_open = cap_open,
	.d_close = cap_close,
	.d_read = eno_rdwrt,
	.d_write = eno_rdwrt,
	.d_ioctl = eno_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = NULL,
	.d_select = eno_select,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0,
};

static void
caps_init(void)
{
	sCapMajor = cdevsw_add(-1, &sCapCdevsw);
	if (sCapMajor < 0)
		return;
	// config is root's alone, monitor anyone's, the modes the capability files report
	devfs_make_node(makedev(sCapMajor, 1), DEVFS_CHAR, 0, 0, 0400, "nvidia-caps/nvidia-cap1");
	devfs_make_node(makedev(sCapMajor, 2), DEVFS_CHAR, 0, 0, 0444, "nvidia-caps/nvidia-cap2");
	IOLog("PDNVLoader: /dev/nvidia-caps, character major %d\n", sCapMajor);
}

// the control device, once the resource manager is up
int
pdnv_dev_init(void)
{
	nvidia_stack_t *sp = NULL;

	sLock = IOLockAlloc();
	sMajor = cdevsw_add(-1, &sCdevsw);
	if (sMajor < 0)
		return -1;
	pd_cdev_mmap_register(sMajor, nv_mmap_pages);
	// poll and kqueue go through nv_select, user space waits on its descriptors for RM events
	cdevsw_setkqueueok(sMajor, &sCdevsw, CDEVSW_SELECT_KQUEUE);

	sCtl.os_state = pdnv_gpu_record(&sCtl);
	if (sCtl.os_state == NULL)
		return -1;
	sCtl.flags = NV_FLAG_CONTROL | NV_FLAG_OPEN;
	if (pdnv_stack_alloc(&sp) != 0)
		return -1;
	if (!((rm_locks_fn)pdnv_rm.init_event_locks)(sp, &sCtl)) {
		pdnv_stack_free(sp);
		return -1;
	}
	pdnv_stack_free(sp);

	if (devfs_make_node_clone(makedev(sMajor, PDNV_CTL_NODE), DEVFS_CHAR, 0, 0, 0666, nv_clone,
	    "nvidiactl") == NULL)
		return -1;
	IOLog("PDNVLoader: /dev/nvidiactl, character major %d\n", sMajor);
	caps_init();
	return 0;
}

// a started GPU gets /dev/nvidiaN
int
pdnv_dev_add_gpu(void *state)
{
	nv_state_t *nv = state;
	unsigned n = sNumGPUs;

	if (sMajor < 0 || n >= PDNV_MAX_GPUS)
		return -1;
	nv->flags |= NV_FLAG_OPEN;
	sGPUs[n] = nv;
	sNumGPUs = n + 1;
	if (devfs_make_node_clone(makedev(sMajor, n), DEVFS_CHAR, 0, 0, 0666, nv_clone, "nvidia%u", n) == NULL)
		return -1;
	IOLog("PDNVLoader: /dev/nvidia%u\n", n);
	return 0;
}

// another descriptor of the calling process, as the resource manager checks the ones it is handed
nv_file_private_t * NV_API_CALL
nv_get_file_private(NvS32 fd, NvBool ctl, void **os_private)
{
	struct pdnv_file *f = NULL;
	vnode_t vp = NULL;
	dev_t rdev;

	*os_private = NULL;
	if (file_vnode(fd, &vp) != 0)
		return NULL;
	if (vnode_ischr(vp)) {
		rdev = vnode_specrdev(vp);
		if (major(rdev) == sMajor && (ctl ? NODE(rdev) == PDNV_CTL_NODE : NODE(rdev) < PDNV_MAX_GPUS))
			f = slot_file(rdev);
	}
	if (f == NULL) {
		file_drop(fd);
		return NULL;
	}
	// the descriptor stays held until nv_put_file_private
	*os_private = (void *)(uintptr_t)(fd + 1);
	return &f->nvfp;
}

void NV_API_CALL
nv_put_file_private(void *os_private)
{
	if (os_private != NULL)
		file_drop((int)(uintptr_t)os_private - 1);
}

// whether the calling process has this GPU's node open, as the Linux layer walks its descriptors.
// A descriptor passed to another process counts for the one that opened it
NvBool NV_API_CALL
nv_is_gpu_accessible(nv_state_t *nv)
{
	pid_t pid = proc_selfpid();
	NvBool found = NV_FALSE;

	IOLockLock(sLock);
	for (int i = 1; i < PDNV_SLOTS && !found; i++)
		found = sSlots[i] != NULL && sSlots[i]->nv == nv && sSlots[i]->pid == pid;
	IOLockUnlock(sLock);
	return found;
}

NV_STATUS NV_API_CALL
os_memcpy_from_user(void *to, const void *from, NvU32 n)
{
	return copyin((user_addr_t)(uintptr_t)from, to, n) == 0 ? NV_OK : NV_ERR_INVALID_ADDRESS;
}

NV_STATUS NV_API_CALL
os_memcpy_to_user(void *to, const void *from, NvU32 n)
{
	return copyout(from, (user_addr_t)(uintptr_t)to, n) == 0 ? NV_OK : NV_ERR_INVALID_ADDRESS;
}
