/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfs.h"
#include "apfsrw/apfsrw.h"

#include <IOKit/IOLocks.h>

#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/ubc.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode_if.h>
#include <string.h>

int (**apfs_vnodeop_p)(void *);

#define APFS_DIRENT_SZ(dp) \
	((sizeof(struct dirent) - NAME_MAX) + (((dp)->d_namlen + 1 + 3) & ~3))

static int
apfs_emit_dirent(ino_t ino, const char *name, struct uio *uio)
{
	struct dirent dent;
	size_t namelen = strlen(name);

	if (namelen > NAME_MAX)
		return EINVAL;

	memset(&dent, 0, sizeof(dent));
	dent.d_ino = ino;
	dent.d_type = DT_DIR;
	dent.d_namlen = (uint8_t)namelen;
	dent.d_reclen = APFS_DIRENT_SZ(&dent);
	strlcpy(dent.d_name, name, sizeof(dent.d_name));

	if (uio_resid(uio) < dent.d_reclen)
		return EMSGSIZE;
	return uiomove((caddr_t)&dent, dent.d_reclen, uio);
}

/*
 * Create, or return, the one in-core vnode for `fileid`. Backed by a per-mount
 * hash: minting a fresh vnode per lookup (which is what this did) breaks
 * anything that stores state on the vnode. The case that caught it was
 * launchd's IPC socket - unp_bind() puts the listener in vp->v_socket and
 * unp_connect() reads it back off the vnode the path lookup returns, so a
 * second vnode for the same node made every connect() find v_socket == NULL.
 */
int
apfs_vget(struct apfs_mount *amp, uint64_t fileid, vnode_t dvp, vnode_t *vpp)
{
	IOLock *hlock;
	struct apfs_node_bucket *bucket;
	struct vnode_fsparam vfsp;
	struct apfs_node *node;
	struct apfs_inode_info info;
	int error;

	if (amp == NULL || vpp == NULL)
		return EINVAL;

	hlock = (IOLock *)amp->am_hash_lock;
	bucket = &amp->am_node_hash[APFS_NODE_HASH(fileid)];

	IOLockLock(hlock);
restart:
	LIST_FOREACH(node, bucket, a_hash) {
		if (node->fileid != fileid)
			continue;
		if (node->a_alloc_wip) {
			/* Another thread is inside vnode_create(); wait. */
			IOLockSleep(hlock, &node->vp, THREAD_UNINT);
			goto restart;
		}
		{
			vnode_t vp = node->vp;
			uint32_t vid = vnode_vid(vp);

			IOLockUnlock(hlock);
			if (vnode_getwithvid(vp, vid) == 0) {
				*vpp = vp;
				return 0;
			}
			/* Being reclaimed; start over. */
			IOLockLock(hlock);
			goto restart;
		}
	}

	error = 0;
	node = (struct apfs_node *)_MALLOC(sizeof(*node), M_TEMP,
	    M_WAITOK | M_ZERO);
	if (node == NULL) {
		IOLockUnlock(hlock);
		return ENOMEM;
	}
	node->amp = amp;
	node->fileid = fileid;
	node->a_alloc_wip = 1;
	LIST_INSERT_HEAD(bucket, node, a_hash);
	IOLockUnlock(hlock);

	error = apfs_lookup_inode(amp, fileid, &info);
	if (error)
		goto fail;

	node->type = info.type;
	node->mode = info.mode;
	node->uid = info.uid;
	node->gid = info.gid;
	node->size = info.size;
	node->nlink = info.nlink;
	node->parent_id = info.parent_id;

	memset(&vfsp, 0, sizeof(vfsp));
	vfsp.vnfs_mp = amp->mp;
	vfsp.vnfs_vtype = node->type;
	vfsp.vnfs_str = APFS_MODULE_NAME;
	vfsp.vnfs_dvp = dvp;
	vfsp.vnfs_fsnode = node;
	vfsp.vnfs_vops = apfs_vnodeop_p;
	vfsp.vnfs_markroot = (fileid == APFS_ROOT_FILEID);
	vfsp.vnfs_marksystem = 0;
	vfsp.vnfs_rdev = 0;
	vfsp.vnfs_filesize = (off_t)node->size;
	vfsp.vnfs_cnp = NULL;
	vfsp.vnfs_flags = VNFS_ADDFSREF | VNFS_NOCACHE;

	error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, vpp);
	if (error)
		goto fail;

	node->vp = *vpp;
	vnode_settag(*vpp, VT_OTHER);

	IOLockLock(hlock);
	node->a_alloc_wip = 0;
	IOLockWakeup(hlock, &node->vp, FALSE);
	IOLockUnlock(hlock);
	return 0;

fail:
	IOLockLock(hlock);
	if (!node->a_unhashed) {
		LIST_REMOVE(node, a_hash);
		node->a_unhashed = 1;
	}
	node->a_alloc_wip = 0;
	IOLockWakeup(hlock, &node->vp, FALSE);
	IOLockUnlock(hlock);
	_FREE(node, M_TEMP);
	return error;
}

/*
 * Mirror of the first two fields of struct vnodeop_desc (xnu
 * bsd/sys/vnode_internal.h), which is not in the kext KPI. Only used to name
 * the unimplemented operation in the log below.
 */
struct apfs_vnodeop_desc_head {
	int vdesc_offset;
	const char *vdesc_name;
};

static int
apfs_vnop_default(struct vnop_generic_args *ap)
{
	const struct apfs_vnodeop_desc_head *d =
	    (const struct apfs_vnodeop_desc_head *)ap->a_desc;

	char pname[MAXCOMLEN + 1];

	pname[0] = '\0';
	proc_selfname(pname, (int)sizeof(pname));
	APFSLOG("unimplemented vnop: %s (pid %d %s)",
	    (d && d->vdesc_name) ? d->vdesc_name : "?", proc_selfpid(), pname);
	return ENOTSUP;
}

/*
 * VNOP_MMAP's ENOTSUP is NOT swallowed by the KPI the way VNOP_MMAP_CHECK's is
 * (kpi_vfs.c), so leaving these unimplemented failed every mmap() on an APFS
 * root - which is most of what dyld and launchd do. There is no per-mapping
 * state to track here; VNOP_PAGEIN does the work.
 */
static int
apfs_vnop_mmap(__unused struct vnop_mmap_args *ap)
{
	return 0;
}

static int
apfs_vnop_mmap_check(__unused struct vnop_mmap_check_args *ap)
{
	return 0;
}

static int
apfs_vnop_mnomap(__unused struct vnop_mnomap_args *ap)
{
	return 0;
}

/*
 * Nothing is cached per-vnode that has to be flushed when the last reference
 * goes away; apfs_vnop_reclaim frees the node. Returning ENOTSUP here just
 * made every vnode teardown log an error.
 */
static int
apfs_vnop_inactive(__unused struct vnop_inactive_args *ap)
{
	return 0;
}

/*
 * Unknown ioctls must be ENOTTY, not ENOTSUP: callers treat ENOTTY as "this
 * file system does not have that control" and carry on, while ENOTSUP has
 * been read as a hard failure. F_FULLFSYNC is the one that matters in
 * practice - the same gap made Rust's file writes fail on ext4.
 */
static int
apfs_vnop_ioctl(struct vnop_ioctl_args *ap)
{
	switch (ap->a_command) {
	case F_FULLFSYNC:
		return 0;
	default:
		return ENOTTY;
	}
}

static int
apfs_vnop_lookup(struct vnop_lookup_args *ap)
{
	struct componentname *cnp = ap->a_cnp;
	struct apfs_node *dnode = VTOAPFS(ap->a_dvp);
	uint64_t fileid;
	int error;

	*ap->a_vpp = NULLVP;
	if (dnode == NULL)
		return EINVAL;
	if (dnode->type != VDIR)
		return ENOTDIR;

	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		error = vnode_get(ap->a_dvp);
		if (error)
			return error;
		*ap->a_vpp = ap->a_dvp;
		return 0;
	}
	if (cnp->cn_namelen == 2 && cnp->cn_nameptr[0] == '.' &&
	    cnp->cn_nameptr[1] == '.') {
		fileid = dnode->parent_id ? dnode->parent_id : APFS_ROOT_FILEID;
		if (fileid == dnode->fileid) {
			error = vnode_get(ap->a_dvp);
			if (error)
				return error;
			*ap->a_vpp = ap->a_dvp;
			return 0;
		}
		return apfs_vget(dnode->amp, fileid, NULLVP, ap->a_vpp);
	}

	error = apfs_lookup_dirent(dnode->amp, dnode->fileid,
	    cnp->cn_nameptr, cnp->cn_namelen, &fileid, NULL);
	if (error) {
		if (error == ENOENT && (cnp->cn_flags & ISLASTCN) &&
		    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
			return EJUSTRETURN;
		return error;
	}
	return apfs_vget(dnode->amp, fileid, ap->a_dvp, ap->a_vpp);
}

static int
apfs_vnop_open(__unused struct vnop_open_args *ap)
{
	return 0;
}

static int
apfs_vnop_close(__unused struct vnop_close_args *ap)
{
	return 0;
}

static int
apfs_vnop_getattr(struct vnop_getattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct vnode_attr *vap = ap->a_vap;
	struct apfs_node *node = VTOAPFS(vp);
	struct apfs_mount *amp = node ? node->amp : VFSTOAPFS(vnode_mount(vp));
	uint32_t iosize = (amp && amp->block_size) ? amp->block_size : APFS_BS_BYTES;

	if (node == NULL)
		return EINVAL;

	VATTR_RETURN(vap, va_type, node->type);
	VATTR_RETURN(vap, va_rdev, 0);
	VATTR_RETURN(vap, va_nlink, node->nlink ? node->nlink : 1);
	VATTR_RETURN(vap, va_total_size, node->size);
	VATTR_RETURN(vap, va_data_size, node->size);
	VATTR_RETURN(vap, va_total_alloc, node->size);
	VATTR_RETURN(vap, va_data_alloc, node->size);
	VATTR_RETURN(vap, va_iosize, iosize);
	VATTR_RETURN(vap, va_uid, node->uid);
	VATTR_RETURN(vap, va_gid, node->gid);
	VATTR_RETURN(vap, va_mode, node->mode);
	VATTR_RETURN(vap, va_fileid, node->fileid);
	VATTR_RETURN(vap, va_linkid, node->fileid);
	VATTR_RETURN(vap, va_parentid,
	    node->parent_id ? node->parent_id : APFS_ROOT_FILEID);
	if (amp) {
		VATTR_RETURN(vap, va_fsid, vfs_statfs(amp->mp)->f_fsid.val[0]);
		VATTR_RETURN(vap, va_fsid64, vfs_statfs(amp->mp)->f_fsid);
	}
	VATTR_RETURN(vap, va_filerev, 0);
	VATTR_RETURN(vap, va_gen, 0);
	VATTR_RETURN(vap, va_flags, 0);
	return 0;
}

static int
apfs_vnop_readdir(struct vnop_readdir_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct apfs_node *node = VTOAPFS(vp);
	struct uio *uio = ap->a_uio;
	off_t offset = uio_offset(uio);
	int error = 0;
	int entries = 0;
	int real_entries = 0;
	int real_eof = 1;

	if (ap->a_flags & (VNODE_READDIR_EXTENDED | VNODE_READDIR_REQSEEKOFF))
		return EINVAL;
	if (node == NULL)
		return EINVAL;
	if (node->type != VDIR)
		return ENOTDIR;

	if (offset == 0) {
		error = apfs_emit_dirent((ino_t)node->fileid, ".", uio);
		if (error)
			goto out;
		offset++;
		entries++;
	}
	if (offset == 1) {
		error = apfs_emit_dirent((ino_t)APFS_ROOT_FILEID, "..", uio);
		if (error)
			goto out;
		offset++;
		entries++;
	}
	if (offset >= 2) {
		error = apfs_iterate_dir(node->amp, node->fileid, offset - 2,
		    uio, &real_entries, &real_eof);
		if (error)
			goto out;
		offset += real_entries;
		entries += real_entries;
	}

out:
	if (error == EMSGSIZE)
		error = 0;
	uio_setoffset(uio, offset);
	if (ap->a_eofflag)
		*ap->a_eofflag = (offset >= 2 && real_eof);
	if (ap->a_numdirent)
		*ap->a_numdirent = entries;
	return error;
}

static int
apfs_vnop_read(struct vnop_read_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	struct uio *uio = ap->a_uio;

	if (node == NULL || uio == NULL)
		return EINVAL;
	if (node->type == VDIR)
		return EISDIR;
	if (node->type != VREG)
		return ENOTSUP;
	if (uio_offset(uio) < 0)
		return EINVAL;
	if ((uint64_t)uio_offset(uio) >= node->size)
		return 0;
	return apfs_read_file(node, uio);
}

/*
 * APFS keeps a symlink's target in the com.apple.fs.symlink extended attribute
 * rather than in a data stream. Without this vnop every versioned dylib symlink
 * (/usr/lib/libicudata.76.dylib and friends) resolved to the default vnop, so
 * namei() reported ENOTSUP and dyld could not start launchd.
 */
static int
apfs_vnop_readlink(struct vnop_readlink_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	struct uio *uio = ap->a_uio;
	char target[PATH_MAX];
	size_t len = 0;
	int error;

	if (node == NULL || uio == NULL)
		return EINVAL;
	if (node->type != VLNK)
		return EINVAL;

	error = apfs_lookup_xattr(node->amp, node->fileid,
	    APFS_XATTR_SYMLINK_NAME, target, sizeof(target), &len);
	if (error)
		return error;
	if (len == 0)
		return EIO;
	/* The stored target is NUL-terminated; readlink must not return the NUL. */
	if (target[len - 1] == '\0')
		len--;
	if (len == 0)
		return EIO;

	return uiomove(target, (int)len, uio);
}

/*
 * Without this, execve() could read a Mach-O header through VNOP_READ but every
 * fault on the mapped image went to the default vnop, so /sbin/launchd never
 * started. apfs_read_file() already works off uio_offset, so map the page list
 * and let it fill the pages directly; there is no VNOP_STRATEGY to route
 * cluster_pagein() through (see the same reasoning in ext4_vnop_pagein).
 */
static int
apfs_vnop_pagein(struct vnop_pagein_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	upl_t pl = ap->a_pl;
	vm_offset_t ioaddr = 0;
	off_t f_offset = ap->a_f_offset;
	size_t size = ap->a_size;
	uint64_t filesize;
	kern_return_t kr;
	int error = 0;

	if (node == NULL || pl == NULL)
		return EINVAL;
	filesize = node->size;

	kr = ubc_upl_map(pl, &ioaddr);
	if (kr != KERN_SUCCESS)
		return EIO;
	ioaddr += ap->a_pl_offset;

	if (f_offset < 0 || (uint64_t)f_offset >= filesize) {
		memset((void *)ioaddr, 0, size);
	} else {
		size_t want = size;
		uio_t uio;

		if ((uint64_t)f_offset + want > filesize)
			want = (size_t)(filesize - (uint64_t)f_offset);

		uio = uio_create(1, f_offset, UIO_SYSSPACE, UIO_READ);
		if (uio == NULL) {
			error = ENOMEM;
		} else {
			uio_addiov(uio, CAST_USER_ADDR_T(ioaddr), want);
			error = apfs_read_file(node, uio);
			uio_free(uio);
		}
		/* The tail of the last page past EOF has to read as zero. */
		if (error == 0 && want < size)
			memset((char *)ioaddr + want, 0, size - want);
	}

	ubc_upl_unmap(pl);

	if (!(ap->a_flags & UPL_NOCOMMIT)) {
		if (error)
			ubc_upl_abort_range(pl, ap->a_pl_offset, size,
			    UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
		else
			ubc_upl_commit_range(pl, ap->a_pl_offset, size,
			    UPL_COMMIT_FREE_ON_EMPTY);
	}
	return error;
}

/* Whole-file rewrites are buffered in kernel memory; cap the exposure. */
#define APFS_WRITE_MAX	(64ull << 20)

/* apfsrw's transaction state is single-threaded: one writer at a time, and
 * the reload happens under the same lock. */
#define APFS_RW_LOCK(amp)	IOLockLock((IOLock *)(amp)->am_rw_lock)
#define APFS_RW_UNLOCK(amp)	IOLockUnlock((IOLock *)(amp)->am_rw_lock)

static int apfsrw_to_errno(int err)
{
	switch (err) {
	case 0:			return 0;
	case APFSRW_ENOENT:	return ENOENT;
	case APFSRW_ENOTEMPTY:	return ENOTEMPTY;
	case APFSRW_ENOTSUP:	return ENOTSUP;
	case APFSRW_ENOMEM:	return ENOMEM;
	case APFSRW_ENOTDIR:	return ENOTDIR;
	case APFSRW_EEXIST:	return EEXIST;
	case APFSRW_ENOSPC:	return ENOSPC;
	default:		return EIO;
	}
}

/* Replace the file's entire content through libapfsrw, keeping the fileid. */
static int
apfs_set_content(struct apfs_node *node, const void *buf, uint64_t size,
    vfs_context_t ctx)
{
	char path[MAXPATHLEN];
	int len = sizeof(path);
	int error;

	if (node->amp->rw == NULL)
		return ENOTSUP;
	error = vn_getpath(node->vp, path, &len);
	if (error)
		return error;
	error = apfsrw_set_file_content(node->amp->rw, path, buf,
	    (size_t)size);
	if (error != 0) {
		APFSLOG("set_content('%s', %llu) failed: %d", path,
		    (unsigned long long)size, error);
		return apfsrw_to_errno(error);
	}
	error = apfs_reload_container(node->amp, ctx);
	if (error)
		return error;
	node->size = size;
	ubc_setsize(node->vp, (off_t)size);
	return 0;
}

/* Read current content, splice the new bytes in, write the whole file back. */
static int
apfs_vnop_write(struct vnop_write_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	struct uio *uio = ap->a_uio;
	uint8_t *buf;
	uint64_t off, resid, newsize;
	int error;

	if (node == NULL || uio == NULL)
		return EINVAL;
	if (node->type == VDIR)
		return EISDIR;
	if (node->type != VREG)
		return ENOTSUP;
	if (ap->a_ioflag & IO_APPEND)
		uio_setoffset(uio, (off_t)node->size);
	if (uio_offset(uio) < 0)
		return EINVAL;
	if (uio_resid(uio) == 0)
		return 0;

	off = (uint64_t)uio_offset(uio);
	resid = (uint64_t)uio_resid(uio);
	newsize = off + resid;
	if (newsize < node->size)
		newsize = node->size;
	if (newsize > APFS_WRITE_MAX)
		return EFBIG;

	buf = (uint8_t *)_MALLOC((size_t)newsize, M_TEMP, M_WAITOK | M_ZERO);
	if (buf == NULL)
		return ENOMEM;

	/* Lock covers the read-modify-write, or a concurrent append is lost. */
	APFS_RW_LOCK(node->amp);
	if (node->size > 0) {
		uio_t ruio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);

		if (ruio == NULL) {
			APFS_RW_UNLOCK(node->amp);
			_FREE(buf, M_TEMP);
			return ENOMEM;
		}
		uio_addiov(ruio, CAST_USER_ADDR_T(buf), (user_size_t)node->size);
		error = apfs_read_file(node, ruio);
		uio_free(ruio);
		if (error) {
			APFS_RW_UNLOCK(node->amp);
			_FREE(buf, M_TEMP);
			return error;
		}
	}

	error = uiomove((caddr_t)(buf + off), (int)resid, uio);
	if (error == 0)
		error = apfs_set_content(node, buf, newsize, ap->a_context);
	APFS_RW_UNLOCK(node->amp);
	_FREE(buf, M_TEMP);
	return error;
}

static int
apfs_truncate(struct apfs_node *node, uint64_t size, vfs_context_t ctx)
{
	uint8_t *buf = NULL;
	uint64_t keep;
	int error;

	if (node->type != VREG)
		return EISDIR;
	if (size == node->size)
		return 0;
	if (size > APFS_WRITE_MAX)
		return EFBIG;

	if (size > 0) {
		buf = (uint8_t *)_MALLOC((size_t)size, M_TEMP,
		    M_WAITOK | M_ZERO);
		if (buf == NULL)
			return ENOMEM;
	}
	APFS_RW_LOCK(node->amp);
	if (size > 0) {
		keep = node->size < size ? node->size : size;
		if (keep > 0) {
			uio_t ruio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);

			if (ruio == NULL) {
				APFS_RW_UNLOCK(node->amp);
				_FREE(buf, M_TEMP);
				return ENOMEM;
			}
			uio_addiov(ruio, CAST_USER_ADDR_T(buf),
			    (user_size_t)keep);
			error = apfs_read_file(node, ruio);
			uio_free(ruio);
			if (error) {
				APFS_RW_UNLOCK(node->amp);
				_FREE(buf, M_TEMP);
				return error;
			}
		}
	}
	error = apfs_set_content(node, buf, size, ctx);
	APFS_RW_UNLOCK(node->amp);
	if (buf != NULL)
		_FREE(buf, M_TEMP);
	return error;
}

static int
apfs_vnop_setattr(struct vnop_setattr_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	struct vnode_attr *vap = ap->a_vap;
	int error;

	if (node == NULL || vap == NULL)
		return EINVAL;
	if (VATTR_IS_ACTIVE(vap, va_data_size)) {
		error = apfs_truncate(node, vap->va_data_size, ap->a_context);
		if (error)
			return error;
		VATTR_SET_SUPPORTED(vap, va_data_size);
	}
	if (VATTR_IS_ACTIVE(vap, va_mode)) {
		node->mode = vap->va_mode & 07777;
		VATTR_SET_SUPPORTED(vap, va_mode);
	}
	return 0;
}

/*
 * libapfsrw addresses entries by path, so rebuild the child's absolute path
 * from the parent vnode plus the component name.
 */
static int
apfs_child_path(vnode_t dvp, struct componentname *cnp, char *buf, size_t bufsz)
{
	int len = (int)bufsz;
	size_t used;
	int error;

	error = vn_getpath(dvp, buf, &len);
	if (error)
		return error;
	used = strlen(buf);
	/* vn_getpath gives "/" for the volume root; avoid ending up with "//name". */
	if (used > 0 && buf[used - 1] == '/')
		buf[--used] = '\0';
	if (used + 1 + cnp->cn_namelen + 1 > bufsz)
		return ENAMETOOLONG;
	buf[used++] = '/';
	memcpy(buf + used, cnp->cn_nameptr, cnp->cn_namelen);
	buf[used + cnp->cn_namelen] = '\0';
	return 0;
}

/* apfsrw is path-based, so reconstruct the parent's path with vn_getpath(). */
static int
apfs_vnop_mkdir(struct vnop_mkdir_args *ap)
{
	struct apfs_node *dnode = VTOAPFS(ap->a_dvp);
	struct vnode_attr *vap = ap->a_vap;
	char path[MAXPATHLEN];
	mode_t mode = 0755;
	uid_t uid = 0;
	gid_t gid = 0;
	uint64_t fileid;
	int error;

	*ap->a_vpp = NULLVP;
	if (dnode == NULL || dnode->amp == NULL)
		return EINVAL;
	if (dnode->type != VDIR)
		return ENOTDIR;
	if (dnode->amp->rw == NULL)
		return ENOTSUP;
	if (ap->a_cnp->cn_namelen == 0 || ap->a_cnp->cn_namelen > NAME_MAX)
		return ENAMETOOLONG;
	if (VATTR_IS_ACTIVE(vap, va_mode))
		mode = vap->va_mode & 07777;
	if (VATTR_IS_ACTIVE(vap, va_uid))
		uid = vap->va_uid;
	if (VATTR_IS_ACTIVE(vap, va_gid))
		gid = vap->va_gid;

	error = apfs_child_path(ap->a_dvp, ap->a_cnp, path, sizeof(path));
	if (error)
		return error;

	APFS_RW_LOCK(dnode->amp);
	error = apfsrw_mkdir(dnode->amp->rw, path, (uint16_t)mode, uid, gid);
	if (error != 0) {
		APFS_RW_UNLOCK(dnode->amp);
		APFSLOG("mkdir('%s') failed: %d", path, error);
		return apfsrw_to_errno(error);
	}

	/* The write moved the volume on; the kext's cached paddrs are now stale. */
	error = apfs_reload_container(dnode->amp, ap->a_context);
	APFS_RW_UNLOCK(dnode->amp);
	if (error)
		return error;

	error = apfs_lookup_dirent(dnode->amp, dnode->fileid,
	    ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen, &fileid, NULL);
	if (error)
		return error;
	return apfs_vget(dnode->amp, fileid, ap->a_dvp, ap->a_vpp);
}

/*
 * remove/rmdir both come down to deleting one directory entry plus its inode,
 * so they share a helper. libapfsrw rejects a non-empty directory itself
 * (APFSRW_ENOTEMPTY) and frees a file's extents along with its inode.
 */
static int
apfs_delete_entry(vnode_t dvp, vnode_t vp, struct componentname *cnp,
    vfs_context_t ctx, int want_dir)
{
	struct apfs_node *dnode = VTOAPFS(dvp);
	struct apfs_node *node = VTOAPFS(vp);
	char path[MAXPATHLEN];
	int error;

	if (dnode == NULL || node == NULL || dnode->amp == NULL)
		return EINVAL;
	if (dnode->type != VDIR)
		return ENOTDIR;
	if (want_dir && node->type != VDIR)
		return ENOTDIR;
	if (!want_dir && node->type == VDIR)
		return EISDIR;
	if (dnode->amp->rw == NULL)
		return ENOTSUP;

	error = apfs_child_path(dvp, cnp, path, sizeof(path));
	if (error)
		return error;

	APFS_RW_LOCK(dnode->amp);
	error = apfsrw_unlink(dnode->amp->rw, path);
	if (error != 0) {
		APFS_RW_UNLOCK(dnode->amp);
		APFSLOG("unlink('%s') failed: %d", path, error);
		return apfsrw_to_errno(error);
	}

	/* The write moved the volume on; drop the stale cached view. */
	error = apfs_reload_container(dnode->amp, ctx);
	APFS_RW_UNLOCK(dnode->amp);
	if (error)
		return error;

	vnode_recycle(vp);
	return 0;
}

static int
apfs_vnop_remove(struct vnop_remove_args *ap)
{
	return apfs_delete_entry(ap->a_dvp, ap->a_vp, ap->a_cnp, ap->a_context,
	    0);
}

static int
apfs_vnop_rmdir(struct vnop_rmdir_args *ap)
{
	return apfs_delete_entry(ap->a_dvp, ap->a_vp, ap->a_cnp, ap->a_context,
	    1);
}

/*
 * launchd's IPC socket is created with VNOP_MKNOD (XNU routes non-regular
 * vn_create types here), so without this pid 1 never publishes its socket and
 * no other job can start.
 */
static int
apfs_vnop_symlink(struct vnop_symlink_args *ap)
{
	struct apfs_node *dnode = VTOAPFS(ap->a_dvp);
	struct vnode_attr *vap = ap->a_vap;
	char path[MAXPATHLEN];
	uint64_t fileid;
	int error;

	*ap->a_vpp = NULLVP;
	if (dnode == NULL || dnode->amp == NULL)
		return EINVAL;
	if (dnode->type != VDIR)
		return ENOTDIR;
	if (dnode->amp->rw == NULL)
		return ENOTSUP;
	if (ap->a_cnp->cn_namelen == 0 || ap->a_cnp->cn_namelen > NAME_MAX)
		return ENAMETOOLONG;
	if (ap->a_target == NULL || ap->a_target[0] == '\0')
		return EINVAL;

	error = apfs_child_path(ap->a_dvp, ap->a_cnp, path, sizeof(path));
	if (error)
		return error;

	APFS_RW_LOCK(dnode->amp);
	error = apfsrw_symlink(dnode->amp->rw, path, ap->a_target,
	    VATTR_IS_ACTIVE(vap, va_uid) ? vap->va_uid : 0,
	    VATTR_IS_ACTIVE(vap, va_gid) ? vap->va_gid : 0);
	if (error != 0) {
		APFS_RW_UNLOCK(dnode->amp);
		APFSLOG("symlink('%s') failed: %d", path, error);
		return apfsrw_to_errno(error);
	}
	error = apfs_reload_container(dnode->amp, ap->a_context);
	APFS_RW_UNLOCK(dnode->amp);
	if (error)
		return error;

	error = apfs_lookup_dirent(dnode->amp, dnode->fileid,
	    ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen, &fileid, NULL);
	if (error)
		return error;
	return apfs_vget(dnode->amp, fileid, ap->a_dvp, ap->a_vpp);
}

static int
apfs_vnop_rename(struct vnop_rename_args *ap)
{
	struct apfs_node *fdnode = VTOAPFS(ap->a_fdvp);
	char from[MAXPATHLEN];
	char to[MAXPATHLEN];
	int error;

	if (fdnode == NULL || fdnode->amp == NULL)
		return EINVAL;
	if (fdnode->amp->rw == NULL)
		return ENOTSUP;

	error = apfs_child_path(ap->a_fdvp, ap->a_fcnp, from, sizeof(from));
	if (error)
		return error;
	error = apfs_child_path(ap->a_tdvp, ap->a_tcnp, to, sizeof(to));
	if (error)
		return error;

	APFS_RW_LOCK(fdnode->amp);
	error = apfsrw_rename(fdnode->amp->rw, from, to);
	if (error != 0) {
		APFS_RW_UNLOCK(fdnode->amp);
		APFSLOG("rename('%s' -> '%s') failed: %d", from, to, error);
		return apfsrw_to_errno(error);
	}
	error = apfs_reload_container(fdnode->amp, ap->a_context);
	APFS_RW_UNLOCK(fdnode->amp);
	if (error)
		return error;

	cache_purge(ap->a_fvp);
	if (ap->a_tvp != NULLVP)
		vnode_recycle(ap->a_tvp);
	return 0;
}

static int
apfs_vnop_mknod(struct vnop_mknod_args *ap)
{
	struct apfs_node *dnode = VTOAPFS(ap->a_dvp);
	struct vnode_attr *vap = ap->a_vap;
	char path[MAXPATHLEN];
	uint16_t ftype;
	uint16_t mode;
	uint64_t fileid;
	int error;

	*ap->a_vpp = NULLVP;
	if (dnode == NULL || dnode->amp == NULL)
		return EINVAL;
	if (dnode->type != VDIR)
		return ENOTDIR;
	if (dnode->amp->rw == NULL)
		return ENOTSUP;

	switch (VATTR_IS_ACTIVE(vap, va_type) ? vap->va_type : VSOCK) {
	case VSOCK:	ftype = APFSRW_DT_SOCK; mode = 0140000; break;
	case VFIFO:	ftype = APFSRW_DT_FIFO; mode = 0010000; break;
	case VCHR:	ftype = APFSRW_DT_CHR;  mode = 0020000; break;
	case VBLK:	ftype = APFSRW_DT_BLK;  mode = 0060000; break;
	default:	return ENOTSUP;
	}
	if (VATTR_IS_ACTIVE(vap, va_mode))
		mode |= (uint16_t)(vap->va_mode & 07777);
	else
		mode |= 0666;

	error = apfs_child_path(ap->a_dvp, ap->a_cnp, path, sizeof(path));
	if (error)
		return error;

	APFS_RW_LOCK(dnode->amp);
	error = apfsrw_mknod(dnode->amp->rw, path, ftype, mode,
	    VATTR_IS_ACTIVE(vap, va_uid) ? vap->va_uid : 0,
	    VATTR_IS_ACTIVE(vap, va_gid) ? vap->va_gid : 0);
	if (error != 0) {
		APFS_RW_UNLOCK(dnode->amp);
		APFSLOG("mknod('%s') failed: %d", path, error);
		return apfsrw_to_errno(error);
	}

	error = apfs_reload_container(dnode->amp, ap->a_context);
	APFS_RW_UNLOCK(dnode->amp);
	if (error)
		return error;

	error = apfs_lookup_dirent(dnode->amp, dnode->fileid,
	    ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen, &fileid, NULL);
	if (error)
		return error;
	return apfs_vget(dnode->amp, fileid, ap->a_dvp, ap->a_vpp);
}

static int
apfs_vnop_create(struct vnop_create_args *ap)
{
	struct apfs_node *dnode = VTOAPFS(ap->a_dvp);
	struct vnode_attr *vap = ap->a_vap;
	char path[MAXPATHLEN];
	mode_t mode = 0644;
	uid_t uid = 0;
	gid_t gid = 0;
	uint64_t fileid;
	int error;

	*ap->a_vpp = NULLVP;
	if (dnode == NULL || dnode->amp == NULL)
		return EINVAL;
	if (dnode->type != VDIR)
		return ENOTDIR;
	if (dnode->amp->rw == NULL)
		return ENOTSUP;
	if (VATTR_IS_ACTIVE(vap, va_type) && vap->va_type != VREG)
		return ENOTSUP;
	if (ap->a_cnp->cn_namelen == 0 || ap->a_cnp->cn_namelen > NAME_MAX)
		return ENAMETOOLONG;
	if (VATTR_IS_ACTIVE(vap, va_mode))
		mode = vap->va_mode & 07777;
	if (VATTR_IS_ACTIVE(vap, va_uid))
		uid = vap->va_uid;
	if (VATTR_IS_ACTIVE(vap, va_gid))
		gid = vap->va_gid;

	error = apfs_child_path(ap->a_dvp, ap->a_cnp, path, sizeof(path));
	if (error)
		return error;

	APFS_RW_LOCK(dnode->amp);
	error = apfsrw_create_file(dnode->amp->rw, path, NULL, 0,
	    (uint16_t)mode, uid, gid);
	if (error != 0) {
		APFS_RW_UNLOCK(dnode->amp);
		APFSLOG("create('%s') failed: %d", path, error);
		return apfsrw_to_errno(error);
	}

	error = apfs_reload_container(dnode->amp, ap->a_context);
	APFS_RW_UNLOCK(dnode->amp);
	if (error)
		return error;

	error = apfs_lookup_dirent(dnode->amp, dnode->fileid,
	    ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen, &fileid, NULL);
	if (error)
		return error;

	VATTR_SET_SUPPORTED(vap, va_type);
	VATTR_SET_SUPPORTED(vap, va_mode);
	VATTR_SET_SUPPORTED(vap, va_uid);
	VATTR_SET_SUPPORTED(vap, va_gid);
	return apfs_vget(dnode->amp, fileid, ap->a_dvp, ap->a_vpp);
}

static int
apfs_vnop_fsync(__unused struct vnop_fsync_args *ap)
{
	return 0;
}

static int
apfs_vnop_reclaim(struct vnop_reclaim_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);

	vnode_removefsref(ap->a_vp);
	vnode_clearfsnode(ap->a_vp);
	if (node) {
		/* Leave the hash before the memory goes away, or apfs_vget()
		 * hands out a freed node on the next lookup. */
		if (node->amp != NULL && node->amp->am_hash_lock != NULL) {
			IOLock *hlock = (IOLock *)node->amp->am_hash_lock;

			IOLockLock(hlock);
			if (!node->a_unhashed) {
				LIST_REMOVE(node, a_hash);
				node->a_unhashed = 1;
			}
			IOLockUnlock(hlock);
		}
		_FREE(node, M_TEMP);
	}
	return 0;
}

static int
apfs_vnop_pathconf(struct vnop_pathconf_args *ap)
{
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return 0;
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		return 0;
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return 0;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return 0;
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return 0;
	default:
		return EINVAL;
	}
}

#define VOPFUNC int (*)(void *)

static const struct vnodeopv_entry_desc apfs_vnodeop_entries[] = {
	{ &vnop_default_desc, (VOPFUNC)apfs_vnop_default },
	{ &vnop_lookup_desc, (VOPFUNC)apfs_vnop_lookup },
	{ &vnop_create_desc, (VOPFUNC)apfs_vnop_create },
	{ &vnop_mkdir_desc, (VOPFUNC)apfs_vnop_mkdir },
	{ &vnop_mknod_desc, (VOPFUNC)apfs_vnop_mknod },
	{ &vnop_remove_desc, (VOPFUNC)apfs_vnop_remove },
	{ &vnop_rmdir_desc, (VOPFUNC)apfs_vnop_rmdir },
	{ &vnop_rename_desc, (VOPFUNC)apfs_vnop_rename },
	{ &vnop_symlink_desc, (VOPFUNC)apfs_vnop_symlink },
	{ &vnop_open_desc, (VOPFUNC)apfs_vnop_open },
	{ &vnop_close_desc, (VOPFUNC)apfs_vnop_close },
	{ &vnop_getattr_desc, (VOPFUNC)apfs_vnop_getattr },
	{ &vnop_setattr_desc, (VOPFUNC)apfs_vnop_setattr },
	{ &vnop_read_desc, (VOPFUNC)apfs_vnop_read },
	{ &vnop_pagein_desc, (VOPFUNC)apfs_vnop_pagein },
	{ &vnop_mmap_desc, (VOPFUNC)apfs_vnop_mmap },
	{ &vnop_mmap_check_desc, (VOPFUNC)apfs_vnop_mmap_check },
	{ &vnop_mnomap_desc, (VOPFUNC)apfs_vnop_mnomap },
	{ &vnop_readlink_desc, (VOPFUNC)apfs_vnop_readlink },
	{ &vnop_write_desc, (VOPFUNC)apfs_vnop_write },
	{ &vnop_readdir_desc, (VOPFUNC)apfs_vnop_readdir },
	{ &vnop_fsync_desc, (VOPFUNC)apfs_vnop_fsync },
	{ &vnop_inactive_desc, (VOPFUNC)apfs_vnop_inactive },
	{ &vnop_ioctl_desc, (VOPFUNC)apfs_vnop_ioctl },
	{ &vnop_reclaim_desc, (VOPFUNC)apfs_vnop_reclaim },
	{ &vnop_pathconf_desc, (VOPFUNC)apfs_vnop_pathconf },
	{ NULL, NULL }
};

struct vnodeopv_desc apfs_vnodeop_opv_desc = {
	&apfs_vnodeop_p, apfs_vnodeop_entries
};
