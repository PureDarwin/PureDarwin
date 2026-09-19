/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfs.h"
#include "apfsrw/apfsrw.h"

#include <IOKit/IOLocks.h>

#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/fsctl.h>
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
#include <sys/xattr.h>
#include <mach/mach_time.h>
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
			// Another thread is inside vnode_create(). Wait
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
			// Being reclaimed. Start over
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
	node->bsd_flags = info.bsd_flags;
	node->atime_ns = info.atime_ns;
	node->mtime_ns = info.mtime_ns;
	node->ctime_ns = info.ctime_ns;
	node->crtime_ns = info.crtime_ns;

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

// Local filesystems need no monitor hook. kevent ignores it, but ENOTSUP spams the console
static int
apfs_vnop_monitor(__unused struct vnop_monitor_args *ap)
{
	return 0;
}

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

static int
apfs_vnop_inactive(__unused struct vnop_inactive_args *ap)
{
	return 0;
}

static int
apfs_vnop_ioctl(struct vnop_ioctl_args *ap)
{
	switch (ap->a_command) {
	case F_FULLFSYNC:
		return 0;
	case FSIOC_KERNEL_ROOTAUTH:
		// arm64 bsd_init() panics unless the root volume vouches for its seal.
		// There is nothing to check here
		return 0;
	case 0xc0104a66:
		// mobileassetd retries this ~4000 times a boot on ENOTTY. Answer with zeroes
		if (ap->a_data != NULL)
			bzero(ap->a_data, 16);
		return 0;
	case 0xc1044a50: {
		// libignition's volume-by-role lookup: u16 at [2] is the role (0x10 Preboot).
		// Reply layout is being learned: answer with the sibling volume's BSD name at [4]
		struct apfs_node *node = VTOAPFS(ap->a_vp);
		struct apfs_mount *amp = node ? node->amp : NULL;
		uint8_t *d = (uint8_t *)ap->a_data;
		uint16_t role = (uint16_t)(d[2] | (d[3] << 8));
		struct apfsrw_volume_entry *vols;
		uint32_t count = 0, i;
		int found = -1;
		char base[MAXPATHLEN];
		char *s;

		if (amp == NULL || amp->rw == NULL)
			return ENOTTY;
		vols = _MALLOC(sizeof(*vols) * 16, M_TEMP, M_WAITOK | M_ZERO);
		if (vols == NULL)
			return ENOMEM;
		apfs_rw_lock(amp);
		if (apfsrw_list_volumes(amp->rw, vols, 16, &count) == 0) {
			for (i = 0; i < count; i++) {
				if (vols[i].role == role) {
					found = (int)vols[i].slot;
					break;
				}
			}
		}
		apfs_rw_unlock(amp);
		_FREE(vols, M_TEMP);
		if (found < 0) {
			printf("apfs: fsctl J 0x50 role 0x%x: no such volume\n", role);
			return ENOENT;
		}
		// The volume's device is diskNsM (the root mount says "root_device",
		// so ask the device vnode). Swap M for the sibling's slot
		base[0] = '\0';
		{
			extern int apfs_bsd_name_for_dev(dev_t dev, char *buf, size_t len);

			(void)apfs_bsd_name_for_dev(amp->dev, base, sizeof(base));
		}
		if (base[0] == '\0' && amp->devvp != NULLVP) {
			const char *dn = vnode_getname(amp->devvp);

			if (dn != NULL) {
				strlcpy(base, dn, sizeof(base));
				vnode_putname(dn);
			}
		}
		if (base[0] == '\0')
			strlcpy(base, vfs_statfs(vnode_mount(ap->a_vp))->f_mntfromname, sizeof(base));
		s = NULL;
		for (char *c = base; *c != '\0'; c++) {
			if (*c == 's')
				s = c;
		}
		if (s == NULL || s == base) {
			printf("apfs: fsctl J 0x50 role 0x%x: cannot derive name from %s\n", role, base);
			return ENOTTY;
		}
		snprintf(s, sizeof(base) - (size_t)(s - base), "s%d", found + 1);
		bzero(d + 4, 256);
		strlcpy((char *)d + 4, (base[0] == '/' && base[1] == 'd' && base[2] == 'e' &&
		    base[3] == 'v' && base[4] == '/') ? base + 5 : base, 256);
		printf("apfs: fsctl J 0x50 role 0x%x -> slot %d name %s\n", role, found, (char *)d + 4);
		return 0;
	}
	default:
		// Black-box APFS fsctls (group 'J'): log command and argument bytes
		if (((ap->a_command >> 8) & 0xff) == 'J') {
			uint32_t len = (uint32_t)((ap->a_command >> 16) & 0x1fff);
			const uint8_t *d = (const uint8_t *)ap->a_data;
			char hex[3 * 32 + 1];
			uint32_t n = len < 32 ? len : 32, i;

			hex[0] = '\0';
			for (i = 0; d != NULL && i < n; i++)
				snprintf(hex + 3 * i, 4, "%02x ", d[i]);
			printf("apfs: fsctl J 0x%lx len %u pid %d data %s\n",
			    (unsigned long)ap->a_command, len, proc_selfpid(), hex);
		}
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
		// Remember misses: dyld stats every cached dylib path
		if (error == ENOENT && (cnp->cn_flags & MAKEENTRY))
			cache_enter(ap->a_dvp, NULLVP, cnp);
		return error;
	}
	error = apfs_vget(dnode->amp, fileid, ap->a_dvp, ap->a_vpp);
	if (error == 0 && (cnp->cn_flags & MAKEENTRY))
		cache_enter(ap->a_dvp, *ap->a_vpp, cnp);
	return error;
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

// APFS stores times as nanoseconds since the epoch (spec p.99)
static struct timespec
apfs_ns_to_ts(uint64_t ns)
{
	struct timespec ts;

	ts.tv_sec = (time_t)(ns / 1000000000ull);
	ts.tv_nsec = (long)(ns % 1000000000ull);
	return ts;
}

static uint64_t
apfs_ts_to_ns(struct timespec ts)
{
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
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
	VATTR_RETURN(vap, va_flags, node->bsd_flags);
	VATTR_RETURN(vap, va_access_time, apfs_ns_to_ts(node->atime_ns));
	VATTR_RETURN(vap, va_modify_time, apfs_ns_to_ts(node->mtime_ns));
	VATTR_RETURN(vap, va_change_time, apfs_ns_to_ts(node->ctime_ns));
	VATTR_RETURN(vap, va_create_time, apfs_ns_to_ts(node->crtime_ns));
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
	// The stored target is NUL-terminated. readlink must not return the NUL
	if (target[len - 1] == '\0')
		len--;
	if (len == 0)
		return EIO;

	return uiomove(target, (int)len, uio);
}

static int
apfs_vnop_pagein_impl(struct vnop_pagein_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	upl_t pl = ap->a_pl;
	vm_offset_t ioaddr = 0;
	off_t f_offset = ap->a_f_offset;
	size_t size = ap->a_size;
	uint64_t filesize;
	kern_return_t kr;
	int error = 0;

	if (pl == NULL)
		return EINVAL;
	// Unless UPL_NOCOMMIT, every path must commit or abort the UPL:
	// a leaked one leaves its pages busy and vm_object_paging_wait() never returns
	if (node == NULL) {
		error = EINVAL;
		goto out;
	}
	filesize = node->size;

	kr = ubc_upl_map(pl, &ioaddr);
	if (kr != KERN_SUCCESS) {
		error = EIO;
		goto out;
	}
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
		// The tail of the last page past EOF has to read as zero
		if (error == 0 && want < size)
			memset((char *)ioaddr + want, 0, size - want);
	}

	ubc_upl_unmap(pl);
out:
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

// pd_fault_trace=1:
// running cost of page-ins, split into extent lookups and block copies (apfs_btree.c accumulators)
static int
apfs_vnop_pagein(struct vnop_pagein_args *ap)
{
	uint64_t t0 = mach_absolute_time();
	int error = apfs_vnop_pagein_impl(ap);

	if (apfs_trace_enabled()) {
		apfs_trace_add(&apfs_trace_pagein_ns, t0);
		if ((++apfs_trace_pageins & 255) == 0)
			APFSLOG("PD-pagein: pid %d n=%llu total=%llu ms walk=%llu ms "
			    "copy=%llu ms", proc_selfpid(),
			    (unsigned long long)apfs_trace_pageins,
			    (unsigned long long)(apfs_trace_pagein_ns / 1000000ull),
			    (unsigned long long)(apfs_trace_walk_ns / 1000000ull),
			    (unsigned long long)(apfs_trace_copy_ns / 1000000ull));
	}
	return error;
}

// Bytes handed to libapfsrw per transaction. Larger writes loop
#define APFS_WRITE_CHUNK	(1u << 20)

// apfsrw's transaction state is single-threaded:
// one writer at a time, and the reload happens under the same lock
extern uint64_t apfs_ubc_ns;
extern uint32_t apfs_setattr_masks[256];
#define APFS_RW_LOCK(amp)	apfs_rw_lock_tag(amp, __func__)
#define APFS_RW_UNLOCK(amp)	apfs_rw_unlock(amp)

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
	case APFSRW_EPERM:	return EPERM;
	case APFSRW_EINVAL:	return EINVAL;
	case APFSRW_EOVERFLOW:	return EFBIG;
	default:		return EIO;
	}
}

// Rewritten blocks are fresh ones. Pages mapped from the old blocks are stale.
// Called without am_rw_lock: a busy page's pagein needs that lock
static void
apfs_drop_cached_pages(struct apfs_node *node, off_t start, off_t end)
{
	if (end > start)
		(void)ubc_msync(node->vp, start, end, NULL, UBC_INVALIDATE);
}

// Write [off, off+len) from a kernel buffer, one libapfsrw transaction
static int
apfs_write_range(struct apfs_node *node, const char *path, uint64_t off,
    const void *buf, size_t len, vfs_context_t ctx)
{
	int error;
	static unsigned apfs_writes_logged;

	// Which processes drive per-write commits?
	if (apfs_writes_logged < 400 && (apfs_writes_logged++ & 3) == 0) {
		char pname[MAXCOMLEN + 1];

		proc_selfname(pname, (int)sizeof(pname));
		printf("PD-apfswr: pid %d %s off %llu len %zu %s\n", proc_selfpid(), pname,
		    (unsigned long long)off, len, path);
	}

	APFS_RW_LOCK(node->amp);
	error = apfsrw_write_range(node->amp->rw, path, off, buf, len);
	if (error != 0) {
		APFS_RW_UNLOCK(node->amp);
		return apfsrw_to_errno(error);
	}
	error = apfs_reload_container(node->amp, ctx);
	if (error == 0) {
		if (off + len > node->size)
			node->size = off + len;
		node->mtime_ns = node->ctime_ns = apfsrw_now_ns();
		{
			uint64_t t0 = apfsrw_now_ns();

			ubc_setsize(node->vp, (off_t)node->size);
			apfs_ubc_ns += apfsrw_now_ns() - t0;
		}
	}
	APFS_RW_UNLOCK(node->amp);
	return error;
}

// Dirty mapped pages had no way back to disk. Writes only what lies inside the file,
// and skips apfs_write_range's ubc_setsize while these pages are busy
static int
apfs_vnop_pageout(struct vnop_pageout_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	upl_t pl = ap->a_pl;
	vm_offset_t ioaddr = 0;
	off_t f_offset = ap->a_f_offset;
	size_t size = ap->a_size;
	char path[MAXPATHLEN];
	int plen = sizeof(path);
	int error = 0;

	if (pl == NULL)
		return EINVAL;
	if (node == NULL) {
		error = EINVAL;
		goto out;
	}
	if (node->type != VREG || node->amp->rw == NULL) {
		error = ENOTSUP;
		goto out;
	}
	if (f_offset < 0 || (uint64_t)f_offset >= node->size)
		goto out;
	error = vn_getpath(ap->a_vp, path, &plen);
	if (error)
		goto out;
	if (ubc_upl_map(pl, &ioaddr) != KERN_SUCCESS) {
		error = EIO;
		goto out;
	}
	{
		size_t want = size;

		if ((uint64_t)f_offset + want > node->size)
			want = (size_t)(node->size - (uint64_t)f_offset);
		APFS_RW_LOCK(node->amp);
		error = apfsrw_write_range(node->amp->rw, path, (uint64_t)f_offset,
		    (const void *)(ioaddr + ap->a_pl_offset), want);
		if (error != 0) {
			error = apfsrw_to_errno(error);
		} else {
			error = apfs_reload_container(node->amp, ap->a_context);
			if (error == 0)
				node->mtime_ns = node->ctime_ns = apfsrw_now_ns();
		}
		APFS_RW_UNLOCK(node->amp);
	}
	ubc_upl_unmap(pl);
out:
	if (!(ap->a_flags & UPL_NOCOMMIT)) {
		// A failed write stays dirty for a later retry
		if (error)
			ubc_upl_abort_range(pl, ap->a_pl_offset, size, UPL_ABORT_FREE_ON_EMPTY);
		else
			ubc_upl_commit_range(pl, ap->a_pl_offset, size,
			    UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY);
	}
	return error;
}

static int
apfs_vnop_write(struct vnop_write_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	struct uio *uio = ap->a_uio;
	char path[MAXPATHLEN];
	int plen = sizeof(path);
	uint8_t *buf;
	uint64_t off, start;
	size_t bufsz;
	int error = 0;

	if (node == NULL || uio == NULL)
		return EINVAL;
	if (node->type == VDIR)
		return EISDIR;
	if (node->type != VREG)
		return ENOTSUP;
	if (node->amp->rw == NULL)
		return ENOTSUP;
	if (ap->a_ioflag & IO_APPEND)
		uio_setoffset(uio, (off_t)node->size);
	if (uio_offset(uio) < 0)
		return EINVAL;
	if (uio_resid(uio) == 0)
		return 0;
	error = vn_getpath(ap->a_vp, path, &plen);
	if (error)
		return error;

	bufsz = APFS_WRITE_CHUNK;
	if ((uint64_t)uio_resid(uio) < bufsz)
		bufsz = (size_t)uio_resid(uio);
	buf = (uint8_t *)_MALLOC(bufsz, M_TEMP, M_WAITOK);
	if (buf == NULL)
		return ENOMEM;

	start = (uint64_t)uio_offset(uio);
	while (uio_resid(uio) > 0) {
		size_t n = bufsz;

		if ((uint64_t)uio_resid(uio) < n)
			n = (size_t)uio_resid(uio);
		off = (uint64_t)uio_offset(uio);
		error = uiomove((caddr_t)buf, (int)n, uio);
		if (error)
			break;
		error = apfs_write_range(node, path, off, buf, n, ap->a_context);
		if (error)
			break;
	}
	_FREE(buf, M_TEMP);
	apfs_drop_cached_pages(node, (off_t)(start & ~(uint64_t)PAGE_MASK),
	    (off_t)node->size);
	return error;
}

static int
apfs_truncate(struct apfs_node *node, uint64_t size, vfs_context_t ctx)
{
	char path[MAXPATHLEN];
	int plen = sizeof(path);
	uint64_t old;
	int error;

	if (node->type != VREG)
		return EISDIR;
	if (size == node->size)
		return 0;
	if (node->amp->rw == NULL)
		return ENOTSUP;
	error = vn_getpath(node->vp, path, &plen);
	if (error)
		return error;

	APFS_RW_LOCK(node->amp);
	error = apfsrw_truncate(node->amp->rw, path, size);
	if (error != 0) {
		APFS_RW_UNLOCK(node->amp);
		return apfsrw_to_errno(error);
	}
	error = apfs_reload_container(node->amp, ctx);
	old = node->size;
	if (error == 0) {
		node->size = size;
		node->mtime_ns = node->ctime_ns = apfsrw_now_ns();
		{
			uint64_t t0 = apfsrw_now_ns();

			ubc_setsize(node->vp, (off_t)size);
			apfs_ubc_ns += apfsrw_now_ns() - t0;
		}
	}
	APFS_RW_UNLOCK(node->amp);
	if (error == 0 && size < old)
		apfs_drop_cached_pages(node,
		    (off_t)(size & ~(uint64_t)PAGE_MASK), (off_t)old);
	return error;
}

static int
apfs_vnop_setattr(struct vnop_setattr_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	struct vnode_attr *vap = ap->a_vap;
	struct apfsrw_attr a;
	char path[MAXPATHLEN];
	int len = sizeof(path);
	int error;

	if (node == NULL || vap == NULL)
		return EINVAL;
	if (VATTR_IS_ACTIVE(vap, va_data_size)) {
		error = apfs_truncate(node, vap->va_data_size, ap->a_context);
		if (error)
			return error;
		VATTR_SET_SUPPORTED(vap, va_data_size);
	}

	memset(&a, 0, sizeof(a));
	if (VATTR_IS_ACTIVE(vap, va_mode)) {
		a.mask |= APFSRW_ATTR_MODE;
		a.mode = vap->va_mode & 07777;
	}
	if (VATTR_IS_ACTIVE(vap, va_uid)) {
		a.mask |= APFSRW_ATTR_UID;
		a.uid = vap->va_uid;
	}
	if (VATTR_IS_ACTIVE(vap, va_gid)) {
		a.mask |= APFSRW_ATTR_GID;
		a.gid = vap->va_gid;
	}
	if (VATTR_IS_ACTIVE(vap, va_flags)) {
		a.mask |= APFSRW_ATTR_FLAGS;
		a.bsd_flags = vap->va_flags;
	}
	if (VATTR_IS_ACTIVE(vap, va_access_time)) {
		a.mask |= APFSRW_ATTR_ATIME;
		a.atime_ns = apfs_ts_to_ns(vap->va_access_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
		a.mask |= APFSRW_ATTR_MTIME;
		a.mtime_ns = apfs_ts_to_ns(vap->va_modify_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_create_time)) {
		a.mask |= APFSRW_ATTR_CRTIME;
		a.crtime_ns = apfs_ts_to_ns(vap->va_create_time);
	}
	if (a.mask == 0)
		return 0;
	apfs_setattr_masks[a.mask & 0x7f]++;
	// Access-time-only updates come from mmap, and each commit costs ~25 ms under
	// the container lock. Keep them in memory with relatime rules. Never persist
	if (a.mask == APFSRW_ATTR_ATIME) {
		if (node->atime_ns <= node->mtime_ns || node->atime_ns <= node->ctime_ns ||
		    a.atime_ns >= node->atime_ns + 86400ull * 1000000000ull)
			node->atime_ns = a.atime_ns;
		VATTR_SET_SUPPORTED(vap, va_access_time);
		return 0;
	}
	if (node->amp->rw == NULL)
		return ENOTSUP;
	error = vn_getpath(ap->a_vp, path, &len);
	if (error)
		return error;
	apfs_setattr_masks[0x80 | (a.mask & 0x7f)]++;

	APFS_RW_LOCK(node->amp);
	error = apfsrw_setattr(node->amp->rw, path, &a);
	if (error != 0) {
		APFS_RW_UNLOCK(node->amp);
		return apfsrw_to_errno(error);
	}
	error = apfs_reload_container(node->amp, ap->a_context);
	APFS_RW_UNLOCK(node->amp);
	if (error)
		return error;

	if (a.mask & APFSRW_ATTR_MODE) {
		node->mode = a.mode;
		VATTR_SET_SUPPORTED(vap, va_mode);
	}
	if (a.mask & APFSRW_ATTR_UID) {
		node->uid = a.uid;
		VATTR_SET_SUPPORTED(vap, va_uid);
	}
	if (a.mask & APFSRW_ATTR_GID) {
		node->gid = a.gid;
		VATTR_SET_SUPPORTED(vap, va_gid);
	}
	if (a.mask & APFSRW_ATTR_FLAGS) {
		node->bsd_flags = a.bsd_flags;
		VATTR_SET_SUPPORTED(vap, va_flags);
	}
	if (a.mask & APFSRW_ATTR_ATIME) {
		node->atime_ns = a.atime_ns;
		VATTR_SET_SUPPORTED(vap, va_access_time);
	}
	if (a.mask & APFSRW_ATTR_MTIME) {
		node->mtime_ns = a.mtime_ns;
		VATTR_SET_SUPPORTED(vap, va_modify_time);
	}
	if (a.mask & APFSRW_ATTR_CRTIME) {
		node->crtime_ns = a.crtime_ns;
		VATTR_SET_SUPPORTED(vap, va_create_time);
	}
	if (a.mask != APFSRW_ATTR_ATIME)
		node->ctime_ns = apfsrw_now_ns();
	return 0;
}

// libapfsrw addresses entries by path,
// so rebuild the child's absolute path from the parent vnode plus the component name
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
	// vn_getpath gives "/" for the volume root. Avoid ending up with "//name"
	if (used > 0 && buf[used - 1] == '/')
		buf[--used] = '\0';
	if (used + 1 + cnp->cn_namelen + 1 > bufsz)
		return ENAMETOOLONG;
	buf[used++] = '/';
	memcpy(buf + used, cnp->cn_nameptr, cnp->cn_namelen);
	buf[used + cnp->cn_namelen] = '\0';
	return 0;
}

// apfsrw is path-based, so reconstruct the parent's path with vn_getpath()
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
		return apfsrw_to_errno(error);
	}

	// The write moved the volume on. The kext's cached paddrs are now stale
	error = apfs_reload_container(dnode->amp, ap->a_context);
	APFS_RW_UNLOCK(dnode->amp);
	if (error)
		return error;

	error = apfs_lookup_dirent(dnode->amp, dnode->fileid,
	    ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen, &fileid, NULL);
	if (error)
		return error;
	cache_purge_negatives(ap->a_dvp);
	return apfs_vget(dnode->amp, fileid, ap->a_dvp, ap->a_vpp);
}

// remove and rmdir both delete one directory entry plus its inode, so they share a helper.
// libapfsrw rejects a non-empty directory and frees a file's extents with its inode
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
		return apfsrw_to_errno(error);
	}

	// The write moved the volume on. Drop the stale cached view
	error = apfs_reload_container(dnode->amp, ctx);
	APFS_RW_UNLOCK(dnode->amp);
	if (error)
		return error;

	// Other hard links keep the inode (and this vnode) alive
	if (node->type != VDIR && node->nlink > 1) {
		node->nlink--;
		node->ctime_ns = apfsrw_now_ns();
	} else {
		vnode_recycle(vp);
	}
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

// launchd's IPC socket is created with VNOP_MKNOD (XNU routes non-regular vn_create types here),
// so without this pid 1 never publishes its socket and no other job can start
static int
apfs_vnop_link(struct vnop_link_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	struct apfs_node *dnode = VTOAPFS(ap->a_tdvp);
	char from[MAXPATHLEN], to[MAXPATHLEN];
	int flen = sizeof(from);
	int error;

	if (node == NULL || dnode == NULL || dnode->amp == NULL)
		return EINVAL;
	if (node->type == VDIR)
		return EPERM;
	if (dnode->type != VDIR)
		return ENOTDIR;
	if (dnode->amp->rw == NULL)
		return ENOTSUP;
	if (ap->a_cnp->cn_namelen == 0 || ap->a_cnp->cn_namelen > NAME_MAX)
		return ENAMETOOLONG;
	error = vn_getpath(ap->a_vp, from, &flen);
	if (error)
		return error;
	error = apfs_child_path(ap->a_tdvp, ap->a_cnp, to, sizeof(to));
	if (error)
		return error;

	APFS_RW_LOCK(dnode->amp);
	error = apfsrw_link(dnode->amp->rw, from, to);
	if (error != 0) {
		APFS_RW_UNLOCK(dnode->amp);
		return apfsrw_to_errno(error);
	}
	error = apfs_reload_container(dnode->amp, ap->a_context);
	APFS_RW_UNLOCK(dnode->amp);
	if (error)
		return error;
	node->nlink++;
	node->ctime_ns = apfsrw_now_ns();
	cache_purge_negatives(ap->a_tdvp);
	return 0;
}

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
	cache_purge_negatives(ap->a_dvp);
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
		return apfsrw_to_errno(error);
	}
	error = apfs_reload_container(fdnode->amp, ap->a_context);
	APFS_RW_UNLOCK(fdnode->amp);
	if (error)
		return error;

	cache_purge(ap->a_fvp);
	cache_purge_negatives(ap->a_tdvp);
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
	cache_purge_negatives(ap->a_dvp);
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
	cache_purge_negatives(ap->a_dvp);
	return apfs_vget(dnode->amp, fileid, ap->a_dvp, ap->a_vpp);
}

static int
apfs_vnop_fsync(struct vnop_fsync_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);

	// The data sits in an open batch until this commits
	if (node != NULL && node->amp != NULL && node->amp->cont != NULL) {
		APFS_RW_LOCK(node->amp);
		(void)apfs_batch_flush(node->amp->cont);
		APFS_RW_UNLOCK(node->amp);
	}
	return 0;
}

static int
apfs_vnop_reclaim(struct vnop_reclaim_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);

	vnode_removefsref(ap->a_vp);
	vnode_clearfsnode(ap->a_vp);
	if (node) {
		// Leave the hash before the memory goes away,
		// or apfs_vget() hands out a freed node on the next lookup
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
	// init_featureflags aborts when _PC_FILESIZEBITS on / fails
	case _PC_FILESIZEBITS:
		*ap->a_retval = 64;
		return 0;
	case _PC_CASE_SENSITIVE: {
		struct apfs_node *node = VTOAPFS(ap->a_vp);

		*ap->a_retval = (node != NULL && node->amp != NULL &&
		    (node->amp->apfs.apfs_incompatible_features & APFS_INCOMPAT_CASE_INSENSITIVE)) ? 0 : 1;
		return 0;
	}
	case _PC_CASE_PRESERVING:
		*ap->a_retval = 1;
		return 0;
	default:
		return EINVAL;
	}
}

#define APFS_XATTR_MAX_INLINE	3804

static int
apfs_vnop_getxattr(struct vnop_getxattr_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	char *buf;
	size_t len = 0;
	int error;

	if (node == NULL || ap->a_name == NULL)
		return EINVAL;
	if (strcmp(ap->a_name, APFS_XATTR_SYMLINK_NAME) == 0)
		return ENOATTR;
	buf = _MALLOC(APFS_XATTR_MAX_INLINE, M_TEMP, M_WAITOK);
	if (buf == NULL)
		return ENOMEM;
	apfs_rw_lock_tag(node->amp, __func__);
	error = apfs_lookup_xattr(node->amp, node->fileid, ap->a_name, buf,
	    APFS_XATTR_MAX_INLINE, &len);
	apfs_rw_unlock(node->amp);
	if (error == 0) {
		if (ap->a_uio == NULL)
			*ap->a_size = len;
		else if ((size_t)uio_resid(ap->a_uio) < len)
			error = ERANGE;
		else
			error = uiomove(buf, (int)len, ap->a_uio);
	}
	_FREE(buf, M_TEMP);
	return error;
}

static int
apfs_vnop_listxattr(struct vnop_listxattr_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	char *buf;
	size_t len = 0;
	int error;

	if (node == NULL)
		return EINVAL;
	buf = _MALLOC(APFS_XATTR_MAX_INLINE, M_TEMP, M_WAITOK);
	if (buf == NULL)
		return ENOMEM;
	apfs_rw_lock_tag(node->amp, __func__);
	error = apfs_list_xattrs(node->amp, node->fileid, buf,
	    APFS_XATTR_MAX_INLINE, &len);
	apfs_rw_unlock(node->amp);
	if (error == 0) {
		if (ap->a_uio == NULL)
			*ap->a_size = len;
		else if ((size_t)uio_resid(ap->a_uio) < len)
			error = ERANGE;
		else if (len > 0)
			error = uiomove(buf, (int)len, ap->a_uio);
	}
	_FREE(buf, M_TEMP);
	return error;
}

static int
apfs_vnop_setxattr(struct vnop_setxattr_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	char *path = NULL, *buf = NULL;
	int len = MAXPATHLEN;
	size_t n;
	int mode, error;

	if (node == NULL || ap->a_name == NULL || ap->a_uio == NULL)
		return EINVAL;
	if (node->amp->rw == NULL)
		return ENOTSUP;
	if (strcmp(ap->a_name, APFS_XATTR_SYMLINK_NAME) == 0)
		return EPERM;
	n = (size_t)uio_resid(ap->a_uio);
	if (n > APFSRW_XATTR_MAX_EMBEDDED)
		return E2BIG;
	path = _MALLOC(MAXPATHLEN, M_TEMP, M_WAITOK);
	buf = _MALLOC(APFSRW_XATTR_MAX_EMBEDDED + 1, M_TEMP, M_WAITOK);
	if (path == NULL || buf == NULL) {
		error = ENOMEM;
		goto out;
	}
	error = vn_getpath(ap->a_vp, path, &len);
	if (error)
		goto out;
	if (n > 0) {
		error = uiomove(buf, (int)n, ap->a_uio);
		if (error)
			goto out;
	}
	mode = (ap->a_options & XATTR_CREATE) ? 1 :
	    (ap->a_options & XATTR_REPLACE) ? 2 : 0;
	APFS_RW_LOCK(node->amp);
	error = apfsrw_set_xattr(node->amp->rw, path, ap->a_name, buf, n, mode);
	if (error == 0)
		error = apfs_reload_container(node->amp, ap->a_context);
	else
		error = apfsrw_to_errno(error);
	APFS_RW_UNLOCK(node->amp);
out:
	if (path != NULL)
		_FREE(path, M_TEMP);
	if (buf != NULL)
		_FREE(buf, M_TEMP);
	return error;
}

static int
apfs_vnop_removexattr(struct vnop_removexattr_args *ap)
{
	struct apfs_node *node = VTOAPFS(ap->a_vp);
	char *path;
	int len = MAXPATHLEN;
	int error;

	if (node == NULL || ap->a_name == NULL)
		return EINVAL;
	if (node->amp->rw == NULL)
		return ENOTSUP;
	if (strcmp(ap->a_name, APFS_XATTR_SYMLINK_NAME) == 0)
		return EPERM;
	// rootless-init strips its xattr from every file on the volume.
	// Most have none, and that answer must not cost a path resolution
	{
		size_t have = 0;
		uint8_t probe[1];

		APFS_RW_LOCK(node->amp);
		error = apfs_lookup_xattr(node->amp, node->fileid, ap->a_name,
		    probe, sizeof(probe), &have);
		APFS_RW_UNLOCK(node->amp);
		if (error == ENOATTR)
			return ENOATTR;
	}
	path = _MALLOC(MAXPATHLEN, M_TEMP, M_WAITOK);
	if (path == NULL)
		return ENOMEM;
	error = vn_getpath(ap->a_vp, path, &len);
	if (error == 0) {
		APFS_RW_LOCK(node->amp);
		error = apfsrw_remove_xattr(node->amp->rw, path, ap->a_name);
		if (error == 0)
			error = apfs_reload_container(node->amp, ap->a_context);
		else
			error = error == APFSRW_ENOENT ? ENOATTR :
			    apfsrw_to_errno(error);
		APFS_RW_UNLOCK(node->amp);
	}
	_FREE(path, M_TEMP);
	return error;
}

// ENOTSUP makes the VFS fall back to readdir + getattr (vfs_attrlist.c).
// Only worth a handler so it does not log once per directory
static int
apfs_vnop_getattrlistbulk(__unused struct vnop_getattrlistbulk_args *ap)
{
	return ENOTSUP;
}

#define VOPFUNC int (*)(void *)

static const struct vnodeopv_entry_desc apfs_vnodeop_entries[] = {
	{ &vnop_default_desc, (VOPFUNC)apfs_vnop_default },
	{ &vnop_getattrlistbulk_desc, (VOPFUNC)apfs_vnop_getattrlistbulk },
	{ &vnop_getxattr_desc, (VOPFUNC)apfs_vnop_getxattr },
	{ &vnop_listxattr_desc, (VOPFUNC)apfs_vnop_listxattr },
	{ &vnop_setxattr_desc, (VOPFUNC)apfs_vnop_setxattr },
	{ &vnop_removexattr_desc, (VOPFUNC)apfs_vnop_removexattr },
	{ &vnop_lookup_desc, (VOPFUNC)apfs_vnop_lookup },
	{ &vnop_create_desc, (VOPFUNC)apfs_vnop_create },
	{ &vnop_mkdir_desc, (VOPFUNC)apfs_vnop_mkdir },
	{ &vnop_mknod_desc, (VOPFUNC)apfs_vnop_mknod },
	{ &vnop_remove_desc, (VOPFUNC)apfs_vnop_remove },
	{ &vnop_rmdir_desc, (VOPFUNC)apfs_vnop_rmdir },
	{ &vnop_rename_desc, (VOPFUNC)apfs_vnop_rename },
	{ &vnop_symlink_desc, (VOPFUNC)apfs_vnop_symlink },
	{ &vnop_link_desc, (VOPFUNC)apfs_vnop_link },
	{ &vnop_open_desc, (VOPFUNC)apfs_vnop_open },
	{ &vnop_close_desc, (VOPFUNC)apfs_vnop_close },
	{ &vnop_getattr_desc, (VOPFUNC)apfs_vnop_getattr },
	{ &vnop_setattr_desc, (VOPFUNC)apfs_vnop_setattr },
	{ &vnop_read_desc, (VOPFUNC)apfs_vnop_read },
	{ &vnop_pagein_desc, (VOPFUNC)apfs_vnop_pagein },
	{ &vnop_pageout_desc, (VOPFUNC)apfs_vnop_pageout },
	{ &vnop_mmap_desc, (VOPFUNC)apfs_vnop_mmap },
	{ &vnop_monitor_desc, (VOPFUNC)apfs_vnop_monitor },
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
