// SPDX-License-Identifier: MIT
// file descriptors for objects the linuxkpi side owns (dma-bufs, sync_files, drm syncobjs)
// the descriptor holds one reference on the object, taken and dropped by the registered ops
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file_internal.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kauth.h>
#include <sys/proc_internal.h>
#include <sys/uio_internal.h>
#include <sys/vnode.h>

#define f_flag fp_glob->fg_flag
#define f_ops fp_glob->fg_ops

struct pd_lfile_ops {
	void (*hold)(void *obj);
	void (*release)(void *obj);
	// nonzero when ready, which is a direction from select
	int (*poll)(void *obj, int which);
	// data is the kernel copy of the argument, len its size
	int (*ioctl)(void *obj, unsigned int cmd, void *data, size_t len);
	// a negative errno or the bytes read
	long (*read)(void *obj, void *buffer, size_t size, int nonblock);
	// the new offset or a negative errno, like linux llseek
	long long (*seek)(void *obj, long long offset, int whence);
};

int pd_lfile_register(const struct pd_lfile_ops *ops);
int pd_lfile_reserve(int cloexec, int *fdp);
void pd_lfile_unreserve(int fd);
void pd_lfile_install(int fd, void *obj);
void *pd_lfile_get(int fd);

static const struct pd_lfile_ops *pd_lfile_ops;
static const struct fileops pd_lfileops;

// a descriptor still reserved by pd_lfile_reserve, with the proc fd lock held
static struct fileproc *
pd_lfile_reserved_locked(proc_t p, int fd)
{
	struct filedesc *fdp = &p->p_fd;
	struct fileproc *fp;

	if (fd < 0 || fd >= fdp->fd_nfiles ||
	    (fdp->fd_ofileflags[fd] & UF_RESERVED) == 0 ||
	    (fp = fdp->fd_ofiles[fd]) == NULL || fp->f_ops != &pd_lfileops) {
		return NULL;
	}
	return fp;
}

static int
pd_lfile_read(struct fileproc *fp, struct uio *uio, __unused int flags,
    __unused vfs_context_t ctx)
{
	char buffer[256];
	long got;

	if (pd_lfile_ops->read == NULL) {
		return ENXIO;
	}
	got = pd_lfile_ops->read(fp_get_data(fp), buffer,
	    MIN((size_t)uio_resid(uio), sizeof(buffer)), (fp->f_flag & FNONBLOCK) != 0);
	if (got < 0) {
		return (int)-got;
	}
	return uiomove(buffer, (int)got, uio);
}

static int
pd_lfile_ioctl(struct fileproc *fp, u_long cmd, caddr_t data, __unused vfs_context_t ctx)
{
	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return 0;
	}
	if (pd_lfile_ops->ioctl == NULL) {
		return ENOTTY;
	}
	return pd_lfile_ops->ioctl(fp_get_data(fp), (unsigned int)cmd, data, IOCPARM_LEN(cmd));
}

static int
pd_lfile_select(struct fileproc *fp, int which, __unused void *wql, __unused vfs_context_t ctx)
{
	if (pd_lfile_ops->poll == NULL) {
		return 1;
	}
	return pd_lfile_ops->poll(fp_get_data(fp), which);
}

static int
pd_lfile_close(struct fileglob *fg, __unused vfs_context_t ctx)
{
	void *obj = fg_get_data(fg);

	if (obj != NULL) {
		fg_set_data(fg, NULL);
		pd_lfile_ops->release(obj);
	}
	return 0;
}

static const struct fileops pd_lfileops = {
	.fo_type     = DTYPE_PDLFILE,
	.fo_read     = pd_lfile_read,
	.fo_write    = fo_no_write,
	.fo_ioctl    = pd_lfile_ioctl,
	.fo_select   = pd_lfile_select,
	.fo_close    = pd_lfile_close,
	.fo_drain    = fo_no_drain,
	.fo_kqfilter = fo_no_kqfilter,
};

int
pd_lfile_register(const struct pd_lfile_ops *ops)
{
	if (ops == NULL || ops->hold == NULL || ops->release == NULL) {
		return EINVAL;
	}
	pd_lfile_ops = ops;
	return 0;
}

// a descriptor reserved for the current process, filled in by pd_lfile_install
int
pd_lfile_reserve(int cloexec, int *fdp)
{
	proc_t p = current_proc();
	struct fileproc *fp;
	int error;

	if (pd_lfile_ops == NULL) {
		return ENXIO;
	}
	error = falloc_withinit(p, current_cached_proc_cred(p), vfs_context_current(),
	    &fp, fdp, NULL, NULL);
	if (error != 0) {
		return error;
	}
	if (cloexec) {
		fp->fp_flags |= FP_CLOEXEC;
	}
	fp->f_flag = FREAD | FWRITE;
	fp->f_ops = &pd_lfileops;
	fp_set_data(fp, NULL);
	return 0;
}

void
pd_lfile_unreserve(int fd)
{
	proc_t p = current_proc();
	struct fileproc *fp;

	proc_fdlock(p);
	fp = pd_lfile_reserved_locked(p, fd);
	proc_fdunlock(p);
	if (fp != NULL) {
		fp_free(p, fd, fp);
	}
}

// the descriptor takes over one reference the caller holds on obj
void
pd_lfile_install(int fd, void *obj)
{
	proc_t p = current_proc();
	struct fileproc *fp;

	proc_fdlock(p);
	fp = pd_lfile_reserved_locked(p, fd);
	if (fp == NULL) {
		proc_fdunlock(p);
		pd_lfile_ops->release(obj);
		return;
	}
	fp_set_data(fp, obj);
	procfdtbl_releasefd(p, fd, NULL);
	fp_drop(p, fd, fp, 1);
	proc_fdunlock(p);
}

// the object behind one of our descriptors, with a reference for the caller
void *
pd_lfile_get(int fd)
{
	proc_t p = current_proc();
	struct fileproc *fp;
	void *obj = NULL;

	if (fp_lookup(p, fd, &fp, 0) != 0) {
		return NULL;
	}
	if (fp->f_ops == &pd_lfileops && fp_get_data(fp) != NULL) {
		obj = fp_get_data(fp);
		pd_lfile_ops->hold(obj);
	}
	fp_drop(p, fd, fp, 0);
	return obj;
}

// lseek on a linux file, ESPIPE for any other descriptor
int
pd_lfile_lseek(proc_t p, int fd, off_t offset, int whence, off_t *retval)
{
	struct fileproc *fp;
	long long ret;
	int error;

	if ((error = fp_lookup(p, fd, &fp, 0)))
		return error;
	if (fp->f_ops != &pd_lfileops || pd_lfile_ops == NULL || pd_lfile_ops->seek == NULL) {
		fp_drop(p, fd, fp, 0);
		return ESPIPE;
	}
	ret = pd_lfile_ops->seek(fp_get_data(fp), offset, whence);
	fp_drop(p, fd, fp, 0);
	if (ret < 0)
		return (int)-ret;
	*retval = ret;
	return 0;
}
