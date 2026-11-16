// PDEventFD.c - /dev/eventfd, a cloning device with Linux eventfd semantics, one counter per open
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <mach/mach_types.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/ioccom.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <kern/locks.h>
#include <libkern/libkern.h>
#include <miscfs/devfs/devfs.h>

#include "PDEventFD.h"

#define EFD_MAX      0xfffffffffffffffeULL
#define EFD_MINORS   4096

extern int cdevsw_setkqueueok(int, const struct cdevsw *, int);
extern void *IOMalloc(vm_size_t size);
extern void IOFree(void *address, vm_size_t size);
#ifndef CDEVSW_SELECT_KQUEUE
#define CDEVSW_SELECT_KQUEUE 0x01
#endif

typedef struct {
	int used, open, semaphore;
	uint64_t count;
	struct selinfo rsel, wsel;
} EventFD;

static EventFD *efds;
static lck_grp_t *efd_grp;
static lck_mtx_t *efd_lock;
static int efd_major = -1;
static void *efd_node;

static EventFD *
efd_get(dev_t dev)
{
	int m = minor(dev);

	return m >= 0 && m < EFD_MINORS && efds[m].used ? &efds[m] : NULL;
}

static int
efd_clone(dev_t dev, int action)
{
	int m;

	if (action == DEVFS_CLONE_FREE)
		return 0;
	lck_mtx_lock(efd_lock);
	for (m = 0; m < EFD_MINORS && efds[m].used; m++) {
	}
	if (m < EFD_MINORS) {
		bzero(&efds[m], sizeof(efds[m]));
		efds[m].used = 1;
	}
	lck_mtx_unlock(efd_lock);
	(void)dev;
	return m < EFD_MINORS ? m : -1;
}

static int
efd_open(dev_t dev, int flags, int devtype, struct proc *p)
{
	EventFD *e;

	(void)flags, (void)devtype, (void)p;
	lck_mtx_lock(efd_lock);
	e = efd_get(dev);
	if (e)
		e->open = 1;
	lck_mtx_unlock(efd_lock);
	return e ? 0 : ENXIO;
}

// the last close of the clone: wake anyone still waiting, then the slot is free for the next open
static int
efd_close(dev_t dev, int flags, int devtype, struct proc *p)
{
	EventFD *e;

	(void)flags, (void)devtype, (void)p;
	lck_mtx_lock(efd_lock);
	e = efd_get(dev);
	if (e) {
		selwakeup(&e->rsel);
		selwakeup(&e->wsel);
		wakeup(e);
		bzero(e, sizeof(*e));
	}
	lck_mtx_unlock(efd_lock);
	return 0;
}

static int
efd_read(dev_t dev, struct uio *uio, int ioflag)
{
	EventFD *e;
	uint64_t value;
	int err = 0;

	if (uio_resid(uio) < 8)
		return EINVAL;
	lck_mtx_lock(efd_lock);
	while ((e = efd_get(dev)) && e->count == 0) {
		if (ioflag & IO_NDELAY) {
			err = EAGAIN;
			break;
		}
		err = msleep(e, efd_lock, PCATCH | PSOCK, "eventfd", NULL);
		if (err)
			break;
	}
	if (!e && !err)
		err = ENXIO;
	if (!err) {
		value = e->semaphore ? 1 : e->count;
		e->count -= value;
		selwakeup(&e->wsel);
		wakeup(e);
	}
	lck_mtx_unlock(efd_lock);
	return err ? err : uiomove((caddr_t)&value, 8, uio);
}

static int
efd_write(dev_t dev, struct uio *uio, int ioflag)
{
	EventFD *e;
	uint64_t add;
	int err;

	if (uio_resid(uio) < 8)
		return EINVAL;
	if ((err = uiomove((caddr_t)&add, 8, uio)))
		return err;
	if (add == UINT64_MAX)
		return EINVAL;
	lck_mtx_lock(efd_lock);
	while ((e = efd_get(dev)) && add > EFD_MAX - e->count) {
		if (ioflag & IO_NDELAY) {
			err = EAGAIN;
			break;
		}
		err = msleep(e, efd_lock, PCATCH | PSOCK, "eventfd", NULL);
		if (err)
			break;
	}
	if (!e && !err)
		err = ENXIO;
	if (!err && add) {
		e->count += add;
		selwakeup(&e->rsel);
		wakeup(e);
	}
	lck_mtx_unlock(efd_lock);
	return err;
}

static int
efd_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	EventFD *e;
	int err = 0;

	(void)flag, (void)p;
	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return 0;
	case PD_EVENTFD_SETUP:
		lck_mtx_lock(efd_lock);
		if ((e = efd_get(dev)) && ((PDEventFDSetup *)data)->initial <= EFD_MAX) {
			e->count = ((PDEventFDSetup *)data)->initial;
			e->semaphore = !!(((PDEventFDSetup *)data)->flags & PD_EVENTFD_SEMAPHORE);
		} else {
			err = e ? EINVAL : ENXIO;
		}
		lck_mtx_unlock(efd_lock);
		return err;
	default:
		return ENOTTY;
	}
}

// select, poll and kqueue: with CDEVSW_SELECT_KQUEUE the return is the byte count ready
static int
efd_select(dev_t dev, int which, void *wql, struct proc *p)
{
	EventFD *e;
	int ready = 0;

	lck_mtx_lock(efd_lock);
	if ((e = efd_get(dev))) {
		if (which == FREAD) {
			if (e->count)
				ready = 8;
			else
				selrecord(p, &e->rsel, wql);
		} else if (which == FWRITE) {
			if (e->count < EFD_MAX)
				ready = 8;
			else
				selrecord(p, &e->wsel, wql);
		}
	}
	lck_mtx_unlock(efd_lock);
	return ready;
}

static struct cdevsw efd_cdevsw = {
	.d_open = efd_open,
	.d_close = efd_close,
	.d_read = efd_read,
	.d_write = efd_write,
	.d_ioctl = efd_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = NULL,
	.d_select = efd_select,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0,
};

kern_return_t pd_eventfd_start(kmod_info_t *ki, void *data);
kern_return_t pd_eventfd_stop(kmod_info_t *ki, void *data);

kern_return_t
pd_eventfd_start(kmod_info_t *ki, void *data)
{
	(void)ki, (void)data;
	efds = IOMalloc(sizeof(EventFD) * EFD_MINORS);
	if (!efds)
		return KERN_FAILURE;
	bzero(efds, sizeof(EventFD) * EFD_MINORS);
	efd_grp = lck_grp_alloc_init("eventfd", LCK_GRP_ATTR_NULL);
	efd_lock = lck_mtx_alloc_init(efd_grp, LCK_ATTR_NULL);
	efd_major = cdevsw_add(-1, &efd_cdevsw);
	if (efd_major < 0)
		goto fail;
	cdevsw_setkqueueok(efd_major, &efd_cdevsw, CDEVSW_SELECT_KQUEUE);
	efd_node = devfs_make_node_clone(makedev(efd_major, 0), DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666,
	    efd_clone, "eventfd");
	if (!efd_node)
		goto fail;
	printf("PDEventFD: /dev/eventfd, major %d\n", efd_major);
	return KERN_SUCCESS;
fail:
	if (efd_major >= 0)
		cdevsw_remove(efd_major, &efd_cdevsw);
	lck_mtx_free(efd_lock, efd_grp);
	lck_grp_free(efd_grp);
	IOFree(efds, sizeof(EventFD) * EFD_MINORS);
	return KERN_FAILURE;
}

// never unloaded while a counter is open, since a stale minor would land on freed state
kern_return_t
pd_eventfd_stop(kmod_info_t *ki, void *data)
{
	(void)ki, (void)data;
	for (int m = 0; m < EFD_MINORS; m++)
		if (efds[m].used)
			return KERN_FAILURE;
	devfs_remove(efd_node);
	cdevsw_remove(efd_major, &efd_cdevsw);
	lck_mtx_free(efd_lock, efd_grp);
	lck_grp_free(efd_grp);
	IOFree(efds, sizeof(EventFD) * EFD_MINORS);
	return KERN_SUCCESS;
}
