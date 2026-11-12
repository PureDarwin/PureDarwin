/*
 * ext4_vnops.c - vnode operations for read-only ext4
 */
#include "ext4.h"
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/ubc.h>
#include <sys/vnode_if.h>
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/unistd.h>
#include <sys/syslimits.h>
#include <sys/fcntl.h>
// macOS's Kernel.framework has no fsctl.h, the root-auth fsctl is PureDarwin's boot path only
#if __has_include(<sys/fsctl.h>)
#include <sys/fsctl.h>
#endif
#include <sys/disk.h>
#include <sys/xattr.h>
#include <string.h>
#include <vm/vm_kern.h>
#include <IOKit/IOLocks.h>
#include <miscfs/fifofs/fifo.h>
#include <sys/stat.h>
#include <miscfs/specfs/specdev.h>

/*
 * ext4's on-disk directory-entry file types are numbered independently of BSD's
 * d_type values - EXT4_FT_DIR is 2, which is DT_CHR; EXT4_FT_REG_FILE is 1,
 * which is DT_FIFO. Copying one into the other makes readdir() describe every
 * directory as a character device, which anything that trusts d_type then gets
 * wrong (CFBundle stops recognising bundle layouts, for one).
 */
static uint8_t
ext4_ft_to_dtype(uint8_t ft)
{
	switch (ft) {
	case EXT4_FT_REG_FILE:	return DT_REG;
	case EXT4_FT_DIR:	return DT_DIR;
	case EXT4_FT_CHRDEV:	return DT_CHR;
	case EXT4_FT_BLKDEV:	return DT_BLK;
	case EXT4_FT_FIFO:	return DT_FIFO;
	case EXT4_FT_SOCK:	return DT_SOCK;
	case EXT4_FT_SYMLINK:	return DT_LNK;
	default:		return DT_UNKNOWN;
	}
}

int (**ext4_vnodeop_p)(void *);

/* Blocks read per em_fs_lock acquisition. The lock is mount-wide, so a block
 * at a time makes every read contend with every other vnop on the mount. */
#define EXT4_READ_BATCH_BLOCKS  32

static int ext4_ensure_block(struct ext4node *ep, uint32_t lblk,
    uint64_t *pblk_out);
static int ext4_ensure_block_alloc(struct ext4node *ep, uint32_t lblk,
    uint64_t *pblk_out, bool *allocated);

/*
 * Create (or return) a vnode for inode `ino`.  Backed by a per-mount inode
 * hash so a given inode has exactly one in-core vnode: repeated lookups (after
 * name-cache misses, or from concurrent threads) return the existing vnode via
 * vnode_getwithvid() rather than minting duplicates whose cached inode state
 * would drift out of sync.
 */
int
ext4_vget(struct ext4mount *emp, ino_t ino, vnode_t dvp, vnode_t *vpp,
    struct componentname *cnp)
{
	IOLock *hlock = (IOLock *)emp->em_hash_lock;
	struct ext4_node_bucket *bucket = &emp->em_node_hash[EXT4_NODE_HASH(ino)];
	struct ext4node *ep;
	struct vnode_fsparam vfsp;
	struct ext4_inode raw;
	enum vtype vtype;
	uint64_t size;
	int error;

	/* Fast path: an existing (or being-created) vnode for this inode. */
	IOLockLock(hlock);
restart:
	LIST_FOREACH(ep, bucket, e_hash) {
		if (ep->e_ino != ino)
			continue;
		if (ep->e_alloc_wip) {
			/* Another thread is inside vnode_create(); wait for it. */
			IOLockSleep(hlock, &ep->e_vp, THREAD_UNINT);
			goto restart;
		}
		{
			vnode_t vp = ep->e_vp;
			uint32_t vid = vnode_vid(vp);
			IOLockUnlock(hlock);
			if (vnode_getwithvid(vp, vid) == 0) {
				/* The in-core e_raw is authoritative: every
				 * mutation happens under em_fs_lock and is
				 * written back through the journal. (This used
				 * to re-read the inode table on every cache
				 * hit as a defensive hack against the
				 * pre-locking races - one inode-table read per
				 * path-component lookup.) */
				*vpp = vp;
				return 0;
			}
			/* vnode was being reclaimed; retry from the top. */
			IOLockLock(hlock);
			goto restart;
		}
	}

	/* Not present: insert a placeholder so concurrent callers wait. */
	ep = (struct ext4node *)_MALLOC(sizeof(*ep), M_TEMP, M_WAITOK | M_ZERO);
	if (ep == NULL) {
		IOLockUnlock(hlock);
		return ENOMEM;
	}
	ep->e_lock = IOLockAlloc();
	ep->e_tlock = IORWLockAlloc();
	if (ep->e_lock == NULL || ep->e_tlock == NULL) {
		IOLockUnlock(hlock);
		if (ep->e_lock)
			IOLockFree((IOLock *)ep->e_lock);
		if (ep->e_tlock)
			IORWLockFree((IORWLock *)ep->e_tlock);
		_FREE(ep, M_TEMP);
		return ENOMEM;
	}
	ep->e_mount     = emp;
	ep->e_ino       = ino;
	ep->e_alloc_wip = 1;
	LIST_INSERT_HEAD(bucket, ep, e_hash);
	IOLockUnlock(hlock);

	/* Heavy lifting (disk read, vnode_create) done without the hash lock. */
	error = ext4_read_inode(emp, ino, &raw);
	if (error)
		goto fail;

	vtype = ext4_mode_to_vtype(le16(raw.i_mode));
	size  = le32(raw.i_size_lo) | ((uint64_t)le32(raw.i_size_high) << 32);
	if (vtype == VNON) {
		E4LOG("inode %llu has invalid mode 0%o",
		    (unsigned long long)ino, le16(raw.i_mode));
		error = EIO;
		goto fail;
	}
	ep->e_raw   = raw;
	ep->e_size  = size;
	ep->e_vtype = vtype;
	{
		uint32_t xv = 0;
		size_t xs = 0;
		if (ext4_xattr_get(emp, ino, EXT4_XATTR_INDEX_TRUSTED, "bsdflags", &xv, sizeof(xv), &xs) == 0 &&
		    xs == sizeof(xv))
			ep->e_xflags = le32(xv);
	}

	memset(&vfsp, 0, sizeof(vfsp));
	vfsp.vnfs_mp        = emp->em_mp;
	vfsp.vnfs_vtype     = vtype;
	vfsp.vnfs_str       = "ext4";
	vfsp.vnfs_dvp       = dvp;
	vfsp.vnfs_fsnode    = ep;
	vfsp.vnfs_vops      = vtype == VFIFO ? ext4_fifoop_p :
	    (vtype == VCHR || vtype == VBLK) ? ext4_specop_p : ext4_vnodeop_p;
	vfsp.vnfs_markroot  = (ino == EXT4_ROOT_INO);
	vfsp.vnfs_marksystem = 0;
	vfsp.vnfs_rdev      = (vtype == VCHR || vtype == VBLK) ? ext4_inode_rdev(&raw) : 0;
	vfsp.vnfs_filesize  = size;
	vfsp.vnfs_cnp       = cnp;
	vfsp.vnfs_flags     = VNFS_ADDFSREF;
	if (cnp == NULL || !(cnp->cn_flags & MAKEENTRY))
		vfsp.vnfs_flags |= VNFS_NOCACHE;

	error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, vpp);
	if (error)
		goto fail;

	vnode_settag(*vpp, VT_OTHER);

	IOLockLock(hlock);
	ep->e_vp        = *vpp;
	ep->e_alloc_wip = 0;
	IOLockWakeup(hlock, &ep->e_vp, false);   /* wake all waiters */
	IOLockUnlock(hlock);
	return 0;

fail:
	IOLockLock(hlock);
	LIST_REMOVE(ep, e_hash);
	ep->e_alloc_wip = 0;
	IOLockWakeup(hlock, &ep->e_vp, false);
	IOLockUnlock(hlock);
	IOLockFree((IOLock *)ep->e_lock);
	IORWLockFree((IORWLock *)ep->e_tlock);
	_FREE(ep, M_TEMP);
	return error;
}

/*
 * Iterate directory `dvp` looking for `name`/`namelen`.  On match returns the
 * child inode number in *ino_out.
 */
static int
ext4_dir_lookup(struct ext4node *dep, const char *name, size_t namelen,
    ino_t *ino_out)
{
	struct ext4mount *emp = dep->e_mount;
	uint64_t off, dsize = dep->e_size;
	uint32_t bs = emp->em_blocksize;
	char *blk;
	int error = ENOENT;

	blk = (char *)_MALLOC(bs, M_TEMP, M_WAITOK);
	if (blk == NULL)
		return ENOMEM;

	for (off = 0; off < dsize; off += bs) {
		uint64_t pblk = 0;
		buf_t bp = NULL;
		uint32_t p;

		if (ext4_bmap(emp, dep->e_ino, &dep->e_raw, (uint32_t)(off / bs), &pblk))
			continue;
		if (pblk == 0)
			continue;
		if (ext4_blkread(emp, pblk, &bp))
			continue;
		memcpy(blk, (char *)buf_dataptr(bp), bs);
		buf_brelse(bp);

		for (p = 0; p < bs; ) {
			struct ext4_dir_entry_2 *de =
			    (struct ext4_dir_entry_2 *)(blk + p);
			uint16_t reclen = le16(de->rec_len);

			if (reclen < 8 || p + reclen > bs)
				break;
			if (de->inode != 0 && de->name_len == namelen &&
			    memcmp(de->name, name, namelen) == 0) {
				*ino_out = le32(de->inode);
				error = 0;
				goto out;
			}
			p += reclen;
		}
	}
out:
	_FREE(blk, M_TEMP);
	return error;
}

static int
ext4_vnop_lookup_impl(struct vnop_lookup_args *ap)
{
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ext4node *dep = VTOE(dvp);
	ino_t ino;
	int error;

	*vpp = NULLVP;

	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		error = vnode_get(dvp);
		if (error)
			return error;
		*vpp = dvp;
		return 0;
	}

	error = ext4_dir_lookup(dep, cnp->cn_nameptr, cnp->cn_namelen, &ino);
	if (error) {
		if (error == ENOENT && (cnp->cn_flags & ISLASTCN) &&
		    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
			return EJUSTRETURN;
		return error;
	}

	return ext4_vget(dep->e_mount, ino, dvp, vpp, cnp);
}

// BSD file flags: the three Linux ext4 has bits for live in i_flags, the rest in trusted.bsdflags
#define EXT4_IMMUTABLE_FL 0x00000010u
#define EXT4_APPEND_FL    0x00000020u
#define EXT4_NODUMP_FL    0x00000040u
#define EXT4_NATIVE_BSD_FLAGS (SF_IMMUTABLE | SF_APPEND | UF_NODUMP)

static uint32_t
ext4_bsd_flags(const struct ext4node *ep)
{
	uint32_t f = le32(ep->e_raw.i_flags), out = ep->e_xflags;

	if (f & EXT4_IMMUTABLE_FL)
		out |= SF_IMMUTABLE;
	if (f & EXT4_APPEND_FL)
		out |= SF_APPEND;
	if (f & EXT4_NODUMP_FL)
		out |= UF_NODUMP;
	return out;
}

// updates the inode bits in e_raw (the caller writes the inode) and the attribute right away
static int
ext4_set_bsd_flags(struct ext4node *ep, uint32_t flags)
{
	uint32_t f = le32(ep->e_raw.i_flags) & ~(EXT4_IMMUTABLE_FL | EXT4_APPEND_FL | EXT4_NODUMP_FL);
	uint32_t extra = flags & ~EXT4_NATIVE_BSD_FLAGS;

	if (flags & SF_IMMUTABLE)
		f |= EXT4_IMMUTABLE_FL;
	if (flags & SF_APPEND)
		f |= EXT4_APPEND_FL;
	if (flags & UF_NODUMP)
		f |= EXT4_NODUMP_FL;
	if (extra != ep->e_xflags) {
		uint32_t v = le32(extra);
		int error = ext4_xattr_set(ep, EXT4_XATTR_INDEX_TRUSTED, "bsdflags", extra ? &v : NULL,
		    sizeof(v), 0);
		if (error && error != ENOATTR)
			return error;
		ep->e_xflags = extra;
	}
	ep->e_raw.i_flags = le32(f);
	return 0;
}

static int
ext4_vnop_getattr_impl(struct vnop_getattr_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct vnode_attr *vap = ap->a_vap;
	struct ext4_inode *ri = &ep->e_raw;
	struct ext4mount *emp = ep->e_mount;

	VATTR_RETURN(vap, va_rdev, (ep->e_vtype == VCHR || ep->e_vtype == VBLK) ? ext4_inode_rdev(ri) : 0);
	VATTR_RETURN(vap, va_nlink, le16(ri->i_links_count));
	VATTR_RETURN(vap, va_data_size, ep->e_size);
	VATTR_RETURN(vap, va_total_size, ep->e_size);
	VATTR_RETURN(vap, va_total_alloc, (uint64_t)le32(ri->i_blocks_lo) * 512);
	VATTR_RETURN(vap, va_iosize, emp->em_blocksize);
	VATTR_RETURN(vap, va_uid, le16(ri->i_uid));
	VATTR_RETURN(vap, va_gid, le16(ri->i_gid));
	VATTR_RETURN(vap, va_mode, le16(ri->i_mode) & 07777);
	VATTR_RETURN(vap, va_fileid, (uint64_t)ep->e_ino);
	VATTR_RETURN(vap, va_type, ep->e_vtype);
	VATTR_RETURN(vap, va_flags, ext4_bsd_flags(ep));
	VATTR_RETURN(vap, va_gen, le32(ri->i_generation));

	if (VATTR_IS_ACTIVE(vap, va_access_time)) {
		vap->va_access_time.tv_sec  = le32(ri->i_atime);
		vap->va_access_time.tv_nsec = 0;
		VATTR_SET_SUPPORTED(vap, va_access_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
		vap->va_modify_time.tv_sec  = le32(ri->i_mtime);
		vap->va_modify_time.tv_nsec = 0;
		VATTR_SET_SUPPORTED(vap, va_modify_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_change_time)) {
		vap->va_change_time.tv_sec  = le32(ri->i_ctime);
		vap->va_change_time.tv_nsec = 0;
		VATTR_SET_SUPPORTED(vap, va_change_time);
	}
	return 0;
}

// file size as readers see it, taken under the node lock
static uint64_t
ext4_size_snapshot(struct ext4node *ep)
{
	uint64_t size;

	IOLockLock((IOLock *)ep->e_lock);
	size = ep->e_size;
	IOLockUnlock((IOLock *)ep->e_lock);
	return size;
}

// size pageout may write up to: a write that is still extending the file counts
static uint64_t
ext4_pageout_limit(struct ext4node *ep)
{
	uint64_t size;

	IOLockLock((IOLock *)ep->e_lock);
	size = ep->e_size > ep->e_new_size ? ep->e_size : ep->e_new_size;
	IOLockUnlock((IOLock *)ep->e_lock);
	return size;
}

// drop a regular file's cached pages, dirty ones included, before its blocks are freed
// (call without em_fs_lock: waiting for a busy page can need a pageout, which takes it)
static void
ext4_discard_pages(vnode_t vp)
{
	if (vp != NULL && vnode_isreg(vp) && ubc_pages_resident(vp))
		(void)ubc_msync(vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
}

static int
ext4_vnop_read_impl(struct vnop_read_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct ext4node *ep = VTOE(vp);

	if (vnode_isdir(vp))
		return EISDIR;
	if (!vnode_isreg(vp))
		return EINVAL;
	if (uio_offset(uio) < 0)
		return EINVAL;
	if (uio_resid(uio) == 0)
		return 0;
	// file data lives in the UBC, the one cache mmap and read(2) share
	return cluster_read(vp, uio, (off_t)ext4_size_snapshot(ep), ap->a_ioflag);
}

static uint16_t
ext4_dir_rec_len(uint8_t namelen)
{
	return (uint16_t)((8 + namelen + 3) & ~3);
}

static uint32_t
ext4_dir_data_limit(struct ext4mount *emp, const void *block)
{
	if (ext4_dir_block_has_tail(emp, block))
		return emp->em_blocksize - EXT4_DIR_ENTRY_TAIL_SIZE;
	return emp->em_blocksize;
}

static uint8_t
ext4_vtype_to_ft(enum vtype type)
{
	switch (type) {
	case VREG: return EXT4_FT_REG_FILE;
	case VDIR: return EXT4_FT_DIR;
	case VLNK: return EXT4_FT_SYMLINK;
	case VCHR: return EXT4_FT_CHRDEV;
	case VBLK: return EXT4_FT_BLKDEV;
	case VFIFO: return EXT4_FT_FIFO;
	case VSOCK: return EXT4_FT_SOCK;
	default: return EXT4_FT_UNKNOWN;
	}
}

/*
 * Give up a directory's HTREE index before changing its contents.
 */
static void
ext4_dir_drop_htree(struct ext4node *dep)
{
	uint32_t flags = le32(dep->e_raw.i_flags);

	if ((flags & EXT4_INDEX_FL) == 0)
		return;
	dep->e_raw.i_flags = le32(flags & ~EXT4_INDEX_FL);
	(void)ext4_write_inode(dep->e_mount, dep->e_ino, &dep->e_raw);
}

static int
ext4_dir_add(struct ext4node *dep, const char *name, size_t namelen,
    ino_t ino, enum vtype type)
{
	struct ext4mount *emp = dep->e_mount;
	uint32_t bs = emp->em_blocksize;

	ext4_dir_drop_htree(dep);
	uint16_t need;
	uint64_t off;

	if (namelen == 0 || namelen > 255)
		return ENAMETOOLONG;
	need = ext4_dir_rec_len((uint8_t)namelen);

	for (off = 0; off < dep->e_size; off += bs) {
		uint64_t pblk = 0;
		buf_t bp = NULL;
		uint32_t p;
		int error;

		error = ext4_bmap(emp, dep->e_ino, &dep->e_raw, (uint32_t)(off / bs), &pblk);
		if (error)
			return error;
		if (pblk == 0)
			continue;
		error = ext4_blkread(emp, pblk, &bp);
		if (error)
			return error;

		for (p = 0; p < ext4_dir_data_limit(emp,
		    (const void *)buf_dataptr(bp)); ) {
			struct ext4_dir_entry_2 *de =
			    (struct ext4_dir_entry_2 *)((char *)buf_dataptr(bp) + p);
			uint16_t reclen = le16(de->rec_len);
			uint16_t used;

			if (reclen < 8 ||
			    p + reclen > ext4_dir_data_limit(emp,
			    (const void *)buf_dataptr(bp)))
				break;
			used = de->inode == 0 ? 8 : ext4_dir_rec_len(de->name_len);
			if (de->inode == 0 && reclen >= need) {
				used = 0;
			}
			if (reclen >= used + need) {
				struct ext4_dir_entry_2 *nde;
				uint16_t new_reclen = (uint16_t)(reclen - used);

				if (used != 0)
					de->rec_len = le16(used);
				nde = (struct ext4_dir_entry_2 *)((char *)de + used);
				nde->inode = le32((uint32_t)ino);
				nde->rec_len = le16(new_reclen);
				nde->name_len = (uint8_t)namelen;
				nde->file_type = ext4_vtype_to_ft(type);
				memcpy(nde->name, name, namelen);
				if (ext4_dir_block_check(emp,
				    (const void *)buf_dataptr(bp),
				    "dir_add", dep->e_ino)) {
					buf_brelse(bp);
					return EIO;
				}
				ext4_dir_block_csum_set(emp, dep->e_ino,
				    &dep->e_raw, (void *)buf_dataptr(bp));
				emp->em_stats.dir_adds++;
				return ext4_meta_bwrite(emp, bp);
			}
			p += reclen;
		}
		buf_brelse(bp);
	}

	{
		uint64_t pblk = 0;
		buf_t bp = NULL;
		struct ext4_dir_entry_2 *de;
		struct timeval tv;
		int error;

		error = ext4_ensure_block(dep, (uint32_t)(dep->e_size / bs), &pblk);
		if (error)
			return error;
		error = ext4_blkread(emp, pblk, &bp);
		if (error)
			return error;
		memset((void *)buf_dataptr(bp), 0, bs);
		de = (struct ext4_dir_entry_2 *)buf_dataptr(bp);
		de->inode = le32((uint32_t)ino);
		de->rec_len = le16((uint16_t)(emp->em_has_metadata_csum ?
		    bs - EXT4_DIR_ENTRY_TAIL_SIZE : bs));
		de->name_len = (uint8_t)namelen;
		de->file_type = ext4_vtype_to_ft(type);
		memcpy(de->name, name, namelen);
		ext4_dir_block_init_tail(emp, (void *)buf_dataptr(bp));
		ext4_dir_block_csum_set(emp, dep->e_ino, &dep->e_raw,
		    (void *)buf_dataptr(bp));
		emp->em_stats.dir_adds++;
		error = ext4_meta_bwrite(emp, bp);
		if (error)
			return error;

		dep->e_size += bs;
		dep->e_raw.i_size_lo = le32((uint32_t)dep->e_size);
		dep->e_raw.i_size_high = le32((uint32_t)(dep->e_size >> 32));
		microtime(&tv);
		dep->e_raw.i_ctime = le32((uint32_t)tv.tv_sec);
		dep->e_raw.i_mtime = le32((uint32_t)tv.tv_sec);
		ubc_setsize(dep->e_vp, dep->e_size);
		return ext4_write_inode(emp, dep->e_ino, &dep->e_raw);
	}
}

static int
ext4_dir_remove(struct ext4node *dep, const char *name, size_t namelen,
    ino_t *ino_out, uint8_t *ft_out)
{
	struct ext4mount *emp = dep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint64_t off;

	ext4_dir_drop_htree(dep);

	for (off = 0; off < dep->e_size; off += bs) {
		uint64_t pblk = 0;
		buf_t bp = NULL;
		uint32_t p;
		int error;

		error = ext4_bmap(emp, dep->e_ino, &dep->e_raw, (uint32_t)(off / bs), &pblk);
		if (error)
			return error;
		if (pblk == 0)
			continue;
		error = ext4_blkread(emp, pblk, &bp);
		if (error)
			return error;

		for (p = 0; p < ext4_dir_data_limit(emp,
		    (const void *)buf_dataptr(bp)); ) {
			struct ext4_dir_entry_2 *de =
			    (struct ext4_dir_entry_2 *)((char *)buf_dataptr(bp) + p);
			uint16_t reclen = le16(de->rec_len);

			if (reclen < 8 ||
			    p + reclen > ext4_dir_data_limit(emp,
			    (const void *)buf_dataptr(bp)))
				break;
			if (de->inode != 0 && de->name_len == namelen &&
			    memcmp(de->name, name, namelen) == 0) {
				if (ino_out)
					*ino_out = le32(de->inode);
				if (ft_out)
					*ft_out = de->file_type;
				de->inode = 0;
				if (ext4_dir_block_check(emp,
				    (const void *)buf_dataptr(bp),
				    "dir_remove", dep->e_ino)) {
					buf_brelse(bp);
					return EIO;
				}
				ext4_dir_block_csum_set(emp, dep->e_ino,
				    &dep->e_raw, (void *)buf_dataptr(bp));
				emp->em_stats.dir_removes++;
				return ext4_meta_bwrite(emp, bp);
			}
			p += reclen;
		}
		buf_brelse(bp);
	}
	return ENOENT;
}

static int
ext4_dir_replace(struct ext4node *dep, const char *name, size_t namelen,
    ino_t ino, enum vtype type, ino_t *old_ino, uint8_t *old_ft)
{
	struct ext4mount *emp = dep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint64_t off;

	ext4_dir_drop_htree(dep);

	for (off = 0; off < dep->e_size; off += bs) {
		uint64_t pblk = 0;
		buf_t bp = NULL;
		uint32_t p;
		int error;

		error = ext4_bmap(emp, dep->e_ino, &dep->e_raw, (uint32_t)(off / bs), &pblk);
		if (error)
			return error;
		if (pblk == 0)
			continue;
		error = ext4_blkread(emp, pblk, &bp);
		if (error)
			return error;

		for (p = 0; p < ext4_dir_data_limit(emp,
		    (const void *)buf_dataptr(bp)); ) {
			struct ext4_dir_entry_2 *de =
			    (struct ext4_dir_entry_2 *)((char *)buf_dataptr(bp) + p);
			uint16_t reclen = le16(de->rec_len);

			if (reclen < 8 ||
			    p + reclen > ext4_dir_data_limit(emp,
			    (const void *)buf_dataptr(bp)))
				break;
			if (de->inode != 0 && de->name_len == namelen &&
			    memcmp(de->name, name, namelen) == 0) {
				if (old_ino)
					*old_ino = le32(de->inode);
				if (old_ft)
					*old_ft = de->file_type;
				de->inode = le32((uint32_t)ino);
				de->file_type = ext4_vtype_to_ft(type);
				if (ext4_dir_block_check(emp,
				    (const void *)buf_dataptr(bp),
				    "dir_replace", dep->e_ino)) {
					buf_brelse(bp);
					return EIO;
				}
				ext4_dir_block_csum_set(emp, dep->e_ino,
				    &dep->e_raw, (void *)buf_dataptr(bp));
				return ext4_meta_bwrite(emp, bp);
			}
			p += reclen;
		}
		buf_brelse(bp);
	}
	return ENOENT;
}

static int
ext4_dir_update_dotdot(struct ext4node *dep, ino_t parent)
{
	struct ext4mount *emp = dep->e_mount;
	uint64_t pblk = 0;
	buf_t bp = NULL;
	struct ext4_dir_entry_2 *de;
	uint32_t p;
	int error;

	error = ext4_bmap(emp, dep->e_ino, &dep->e_raw, 0, &pblk);
	if (error)
		return error;
	if (pblk == 0)
		return EIO;
	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;
	for (p = 0; p < ext4_dir_data_limit(emp,
	    (const void *)buf_dataptr(bp)); ) {
		uint16_t reclen;
		de = (struct ext4_dir_entry_2 *)((char *)buf_dataptr(bp) + p);
		reclen = le16(de->rec_len);
		if (reclen < 8 ||
		    p + reclen > ext4_dir_data_limit(emp,
		    (const void *)buf_dataptr(bp)))
			break;
		if (de->inode != 0 && de->name_len == 2 &&
		    de->name[0] == '.' && de->name[1] == '.') {
			de->inode = le32((uint32_t)parent);
			ext4_dir_block_csum_set(emp, dep->e_ino, &dep->e_raw,
			    (void *)buf_dataptr(bp));
			return ext4_meta_bwrite(emp, bp);
		}
		p += reclen;
	}
	buf_brelse(bp);
	return EIO;
}

static int
ext4_dir_is_empty(struct ext4node *dep)
{
	struct ext4mount *emp = dep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint64_t off;

	for (off = 0; off < dep->e_size; off += bs) {
		uint64_t pblk = 0;
		buf_t bp = NULL;
		uint32_t p;
		int error;

		error = ext4_bmap(emp, dep->e_ino, &dep->e_raw, (uint32_t)(off / bs), &pblk);
		if (error)
			return error;
		if (pblk == 0)
			continue;
		error = ext4_blkread(emp, pblk, &bp);
		if (error)
			return error;
		for (p = 0; p < bs; ) {
			struct ext4_dir_entry_2 *de =
			    (struct ext4_dir_entry_2 *)((char *)buf_dataptr(bp) + p);
			uint16_t reclen = le16(de->rec_len);

			if (reclen < 8 || p + reclen > bs)
				break;
			if (de->inode != 0 &&
			    !(de->name_len == 1 && de->name[0] == '.') &&
			    !(de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.')) {
				buf_brelse(bp);
				return ENOTEMPTY;
			}
			p += reclen;
		}
		buf_brelse(bp);
	}
	return 0;
}

static void
ext4_touch_inode(struct ext4_inode *ri)
{
	struct timeval tv;
	microtime(&tv);
	ri->i_ctime = le32((uint32_t)tv.tv_sec);
	ri->i_mtime = le32((uint32_t)tv.tv_sec);
}

/*
 * A change to an inode's metadata updates ctime alone. mtime belongs to
 * changes of the file's data, and to whatever utimes(2) explicitly asks for.
 *
 * Keeping these apart matters more here than it looks: mmap(2) issues a
 * setattr carrying only va_access_time on every mapping, so bumping mtime
 * for any setattr made every mapped file appear freshly modified.
 */
static void
ext4_touch_ctime(struct ext4_inode *ri)
{
	struct timeval tv;
	microtime(&tv);
	ri->i_ctime = le32((uint32_t)tv.tv_sec);
}

// the cached node for ino, so an orphan list edit also reaches the copy an open vnode writes back
static struct ext4node *
ext4_cached_node(struct ext4mount *emp, ino_t ino)
{
	struct ext4_node_bucket *bucket = &emp->em_node_hash[EXT4_NODE_HASH(ino)];
	IOLock *hlock = (IOLock *)emp->em_hash_lock;
	struct ext4node *ep, *found = NULL;

	IOLockLock(hlock);
	LIST_FOREACH(ep, bucket, e_hash) {
		if (ep->e_ino == ino && !ep->e_unhashed) {
			found = ep;
			break;
		}
	}
	IOLockUnlock(hlock);
	return found;
}

// puts an inode with no names left but still open on the superblock's on-disk orphan list,
// s_last_orphan is the head and each orphan's i_dtime holds the next one
static int
ext4_orphan_add(struct ext4mount *emp, ino_t ino, struct ext4_inode *raw)
{
	int error;

	raw->i_dtime = emp->em_sb.s_last_orphan;
	error = ext4_write_inode(emp, ino, raw);
	if (error)
		return error;
	emp->em_sb.s_last_orphan = le32((uint32_t)ino);
	return ext4_write_super(emp);
}

// takes ino off the orphan list before it is freed, next is the ino it pointed at
static int
ext4_orphan_del(struct ext4mount *emp, ino_t ino, uint32_t next)
{
	uint32_t cur = le32(emp->em_sb.s_last_orphan);
	uint64_t steps = 0;

	if (cur == ino) {
		emp->em_sb.s_last_orphan = le32(next);
		return ext4_write_super(emp);
	}
	while (cur != 0 && steps++ < emp->em_inodes_count) {
		struct ext4node *pep = ext4_cached_node(emp, cur);
		struct ext4_inode raw;
		int error;

		if (pep != NULL) {
			if (le32(pep->e_raw.i_dtime) == ino) {
				pep->e_raw.i_dtime = le32(next);
				return ext4_write_inode(emp, cur, &pep->e_raw);
			}
			cur = le32(pep->e_raw.i_dtime);
			continue;
		}
		error = ext4_read_inode(emp, cur, &raw);
		if (error)
			return error;
		if (le32(raw.i_dtime) == ino) {
			raw.i_dtime = le32(next);
			return ext4_write_inode(emp, cur, &raw);
		}
		cur = le32(raw.i_dtime);
	}
	E4LOG("orphan inode %u was not on the orphan list", (unsigned)ino);
	return 0;
}

// frees what a crash or shutdown left on the orphan list, after journal replay at mount:
// inodes with no links are freed, inodes still linked are cut back to i_size
int
ext4_orphan_cleanup(struct ext4mount *emp)
{
	uint32_t ino = le32(emp->em_sb.s_last_orphan);
	uint32_t first_ino = le32(emp->em_sb.s_first_ino);
	uint64_t steps = 0;
	unsigned freed = 0, truncated = 0;
	int error = 0;

	if (ino == 0)
		return 0;
	while (ino != 0 && steps++ < emp->em_inodes_count) {
		struct ext4_inode raw;
		struct ext4node *ep;
		struct timeval tv;
		uint32_t next;

		if (ino < first_ino || ino > emp->em_inodes_count) {
			E4LOG("orphan list holds bad inode %u, stopping", ino);
			break;
		}
		error = ext4_read_inode(emp, ino, &raw);
		if (error)
			break;
		next = le32(raw.i_dtime);
		microtime(&tv);
		if (le16(raw.i_links_count) == 0) {
			enum vtype vtype = ext4_mode_to_vtype(le16(raw.i_mode));

			error = ext4_xattr_release_block(emp, &raw);
			if (!error)
				error = ext4_inode_free_extents(emp, &raw);
			if (error)
				break;
			raw.i_dtime = le32((uint32_t)tv.tv_sec);
			error = ext4_write_inode(emp, ino, &raw);
			if (!error)
				error = ext4_free_inode(emp, ino, vtype);
			if (error)
				break;
			freed++;
		} else {
			uint64_t size = le32(raw.i_size_lo) | ((uint64_t)le32(raw.i_size_high) << 32);

			error = ext4_inode_truncate_extents(emp, ino, &raw,
			    (size + emp->em_blocksize - 1) / emp->em_blocksize);
			if (error)
				break;
			raw.i_dtime = 0;
			error = ext4_write_inode(emp, ino, &raw);
			if (error)
				break;
			truncated++;
		}
		// nothing should be cached this early, drop a stale node if something was
		ep = ext4_cached_node(emp, ino);
		if (ep != NULL)
			ep->e_raw = raw;
		ino = next;
	}
	if (error)
		E4LOG("orphan cleanup stopped at inode %u: %d", ino, error);
	emp->em_sb.s_last_orphan = le32(error ? ino : 0);
	E4LOG("orphan cleanup: freed %u inodes, truncated %u", freed, truncated);
	{
		int werr = ext4_write_super(emp);
		return error ? error : werr;
	}
}

/*
 * Actually tear down an unlinked inode: free its extents/blocks and its
 * inode-bitmap entry, and unhash the in-memory node so a subsequent
 * ext4_alloc_inode() reusing this number can't hand back this stale,
 * zeroed-i_block ext4node via ext4_vget()'s hash-hit fast path.
 */
static int
ext4_finish_free_inode(struct ext4node *ep)
{
	int error;

	error = ext4_xattr_release_block(ep->e_mount, &ep->e_raw);
	if (!error)
		error = ext4_inode_free_extents(ep->e_mount, &ep->e_raw);
	if (error)
		return error;
	ep->e_raw.i_links_count = 0;
	ep->e_raw.i_dtime = ep->e_raw.i_ctime;
	error = ext4_write_inode(ep->e_mount, ep->e_ino, &ep->e_raw);
	if (error)
		return error;
	error = ext4_free_inode(ep->e_mount, ep->e_ino, ep->e_vtype);
	if (error)
		return error;
	// dirty pages of a deleted file can still be pushed out after this, they must land nowhere
	ep->e_freed = 1;
	ep->e_size = 0;
	if (ep->e_vp) {
		IOLock *hlock = (IOLock *)ep->e_mount->em_hash_lock;
		IOLockLock(hlock);
		if (!ep->e_unhashed) {
			LIST_REMOVE(ep, e_hash);
			ep->e_unhashed = 1;
		}
		IOLockUnlock(hlock);
	}
	return 0;
}

/*
 * i_extra_isize for a newly created inode.
 */
static uint16_t
ext4_new_extra_isize(const struct ext4mount *emp)
{
	if (emp->em_inode_size <= EXT4_GOOD_OLD_INODE_SIZE)
		return 0;
	if (emp->em_inode_size - EXT4_GOOD_OLD_INODE_SIZE < 32)
		return (uint16_t)(emp->em_inode_size - EXT4_GOOD_OLD_INODE_SIZE);
	return 32;
}

/*
 * Link count remaining after unlinking one name for this inode.
 */
static uint16_t
ext4_links_after_unlink(uint16_t links, enum vtype vtype)
{
	if (vtype == VDIR)
		return 0;
	return links > 0 ? (uint16_t)(links - 1) : 0;
}

static int
ext4_drop_inode(struct ext4node *ep)
{
	int error;
	uint16_t links = ext4_links_after_unlink(le16(ep->e_raw.i_links_count),
	    ep->e_vtype);

	ep->e_raw.i_links_count = le16(links);
	if (links > 0) {
		ext4_touch_inode(&ep->e_raw);
		return ext4_write_inode(ep->e_mount, ep->e_ino, &ep->e_raw);
	}

	/*
	 * POSIX "delete while open": if something still has this vnode
	 * open (e.g. Xorg holds a Popen fd across a child crash and retries
	 * against the same path), we must NOT free extents/blocks/inode-
	 * bitmap now. Doing so zeroed e_raw.i_block's extent header in
	 * place while the still-open reference kept using this exact
	 * ext4node, so every subsequent bmap/write through that fd hit
	 * "bad extent magic 0x0". Instead, just drop the link (as any
	 * unlink must) and defer the actual free to VNOP_RECLAIM/INACTIVE,
	 * once nothing references the vnode anymore.
	 */
	if (ep->e_vp && vnode_isinuse(ep->e_vp, 0)) {
		// on the orphan list until the last close, so a crash before it frees nothing twice and leaks nothing
		error = ext4_orphan_add(ep->e_mount, ep->e_ino, &ep->e_raw);
		if (error)
			return error;
		ep->e_pending_free = 1;
		return 0;
	}

	error = ext4_finish_free_inode(ep);
	if (error)
		return error;
	if (ep->e_vp)
		vnode_recycle(ep->e_vp);
	return 0;
}

static int
ext4_drop_inode_ino(struct ext4mount *emp, ino_t ino)
{
	struct ext4_inode raw;
	enum vtype vtype;
	struct timeval tv;
	uint16_t links;
	int error;

	error = ext4_read_inode(emp, ino, &raw);
	if (error)
		return error;

	/* Same last-link-only teardown as ext4_drop_inode(); see its comment. */
	vtype = ext4_mode_to_vtype(le16(raw.i_mode));
	links = ext4_links_after_unlink(le16(raw.i_links_count), vtype);
	raw.i_links_count = le16(links);
	if (links > 0) {
		microtime(&tv);
		raw.i_ctime = le32((uint32_t)tv.tv_sec);
		return ext4_write_inode(emp, ino, &raw);
	}

	error = ext4_xattr_release_block(emp, &raw);
	if (!error)
		error = ext4_inode_free_extents(emp, &raw);
	if (error)
		return error;
	microtime(&tv);
	raw.i_dtime = le32((uint32_t)tv.tv_sec);
	raw.i_ctime = le32((uint32_t)tv.tv_sec);
	error = ext4_write_inode(emp, ino, &raw);
	if (error)
		return error;
	error = ext4_free_inode(emp, ino, vtype);
	if (error)
		return error;

	{
		struct ext4_node_bucket *bucket =
		    &emp->em_node_hash[EXT4_NODE_HASH(ino)];
		IOLock *hlock = (IOLock *)emp->em_hash_lock;
		struct ext4node *ep;

		IOLockLock(hlock);
		LIST_FOREACH(ep, bucket, e_hash) {
			if (ep->e_ino == ino && !ep->e_unhashed) {
				LIST_REMOVE(ep, e_hash);
				ep->e_unhashed = 1;
				break;
			}
		}
		IOLockUnlock(hlock);
	}
	return 0;
}

static int
ext4_ensure_block_alloc(struct ext4node *ep, uint32_t lblk, uint64_t *pblk_out,
    bool *allocated)
{
	struct ext4mount *emp = ep->e_mount;
	uint64_t pblk = 0;
	int error;

	/* A file truncated to zero has its extent root zeroed by
	 * ext4_inode_free_extents(). Growing via ftruncate re-creates the header,
	 * but growing via a plain write() arrives straight here, and appending
	 * into a zeroed header (eh_max == 0) cannot store the entry - the block
	 * is allocated but never mapped, so the data reads back as zeroes.
	 * Re-create the header instead. A non-zero but wrong magic is real
	 * corruption and must stay loud. */
	{
		struct ext4_extent_header *eh =
		    (struct ext4_extent_header *)ep->e_raw.i_block;
		if ((le32(ep->e_raw.i_flags) & EXT4_EXTENTS_FL) &&
		    le16(eh->eh_magic) != EXT4_EXT_MAGIC) {
			if (le16(eh->eh_magic) == 0 && le16(eh->eh_entries) == 0) {
				ext4_inode_init_extent_header(&ep->e_raw);
			} else {
				E4LOG("ensure_block ino=%llu lblk=%u size=%llu "
				    "BAD magic=0x%x entries=%u",
				    (unsigned long long)ep->e_ino, lblk,
				    (unsigned long long)ep->e_size,
				    le16(eh->eh_magic), le16(eh->eh_entries));
				return EIO;
			}
		}
		ep->e_dbg_last_lblk = lblk;
	}

	if (ep->e_freed)
		return ENOENT;
	error = ext4_bmap(emp, ep->e_ino, &ep->e_raw, lblk, &pblk);
	if (error)
		return error;
	if (pblk != 0) {
		*pblk_out = pblk;
		if (allocated != NULL)
			*allocated = false;
		return 0;
	}

	// allocate next to the neighbouring block so the file stays in long extents
	{
		uint64_t goal = 0, nb = 0;

		if (lblk > 0 && ext4_bmap(emp, ep->e_ino, &ep->e_raw, lblk - 1, &nb) == 0 && nb != 0)
			goal = nb + 1;
		else if (ep->e_last_pblk != 0)
			goal = ep->e_last_pblk + 1;
		error = ext4_alloc_block(emp, goal, &pblk);
	}
	if (error)
		return error;
	ep->e_last_pblk = pblk;
	error = ext4_inode_append_extent(emp, ep->e_ino, &ep->e_raw, lblk, pblk);
	if (error) {
		(void)ext4_free_block(emp, pblk);
		return error;
	}
	ep->e_raw.i_blocks_lo = le32(le32(ep->e_raw.i_blocks_lo) +
	    (emp->em_blocksize / 512));
	*pblk_out = pblk;
	if (allocated != NULL)
		*allocated = true;
	return 0;
}

static int
ext4_ensure_block(struct ext4node *ep, uint32_t lblk, uint64_t *pblk_out)
{
	return ext4_ensure_block_alloc(ep, lblk, pblk_out, NULL);
}

// map every hole block of [lblk, lblk+count); the caller holds em_fs_lock
static int
ext4_alloc_range(struct ext4node *ep, uint32_t lblk, uint32_t count, bool *grew)
{
	struct ext4mount *emp = ep->e_mount;

	while (count > 0) {
		uint64_t pblk = 0;
		uint32_t run = 0, i;
		int unwritten = 0;
		int error;

		error = ext4_bmap_run(emp, ep->e_ino, &ep->e_raw, lblk, count, &pblk, &run, &unwritten);
		if (error)
			return error;
		if (run == 0 || run > count)
			run = count;
		if (unwritten) {
			E4LOG("write into unwritten extent ino=%llu lblk=%u unsupported",
			    (unsigned long long)ep->e_ino, lblk);
			return EIO;
		}
		if (pblk == 0) {
			for (i = 0; i < run; i++) {
				error = ext4_ensure_block_alloc(ep, lblk + i, &pblk, NULL);
				if (error)
					return error;
				*grew = true;
			}
		}
		lblk += run;
		count -= run;
	}
	return 0;
}

static int
ext4_vnop_write_impl(struct vnop_write_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct ext4node *ep = VTOE(vp);
	struct ext4mount *emp = ep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint64_t orig_size, new_eof, end, mapped_end;
	off_t offset, head_off;
	user_ssize_t resid, unwritten_resid = 0;
	int ioflag = ap->a_ioflag, lflag;
	uint32_t lblk, last_lblk;
	int error = 0;

	if (vnode_isdir(vp))
		return EISDIR;
	if (!vnode_isreg(vp))
		return EINVAL;

	IORWLockRead((IORWLock *)ep->e_tlock);

	IOLockLock((IOLock *)ep->e_lock);
	orig_size = ep->e_size;
	IOLockUnlock((IOLock *)ep->e_lock);
	if (ioflag & IO_APPEND)
		uio_setoffset(uio, (off_t)orig_size);
	offset = uio_offset(uio);
	resid = uio_resid(uio);
	if (offset < 0) {
		error = EINVAL;
		goto out;
	}
	if (resid == 0)
		goto out;
	end = (uint64_t)offset + (uint64_t)resid;
	if (end > ((uint64_t)1 << 32) * bs) {
		error = EFBIG;
		goto out;
	}

	// back every block of the range first, in small steps so metadata commits as it goes
	lblk = (uint32_t)((uint64_t)offset / bs);
	last_lblk = (uint32_t)((end - 1) / bs);
	mapped_end = (uint64_t)offset;
	while (lblk <= last_lblk) {
		uint32_t n = last_lblk - lblk + 1;
		bool grew = false;

		if (n > 64)
			n = 64;
		ext4_fs_lock(emp);
		error = ext4_alloc_range(ep, lblk, n, &grew);
		if (grew)
			(void)ext4_write_inode(emp, ep->e_ino, &ep->e_raw);
		ext4_fs_unlock(emp);
		if (error)
			break;
		lblk += n;
		mapped_end = (uint64_t)lblk * bs;
		if (lblk == 0)
			break;
	}
	if (error) {
		// out of space part way: write what is backed, report a short write
		if (mapped_end <= (uint64_t)offset)
			goto out;
		unwritten_resid = (user_ssize_t)(end - mapped_end);
		uio_setresid(uio, resid - unwritten_resid);
		end = mapped_end;
		error = 0;
	}

	new_eof = end > orig_size ? end : orig_size;
	// UBC must know the new size before cluster_write can page the new tail out
	if (new_eof > orig_size) {
		IOLockLock((IOLock *)ep->e_lock);
		if (new_eof > ep->e_new_size)
			ep->e_new_size = new_eof;
		IOLockUnlock((IOLock *)ep->e_lock);
		ubc_setsize(vp, (off_t)new_eof);
	}
	lflag = ioflag & ~(IO_TAILZEROFILL | IO_HEADZEROFILL | IO_NOZEROVALID | IO_NOZERODIRTY);
	head_off = trunc_page_64(offset);
	// blocks past the old EOF read back as zero, holes and fresh blocks alike
	error = cluster_write(vp, uio, (off_t)orig_size, (off_t)new_eof, head_off, 0,
	    lflag | IO_NOZERODIRTY);
	if (unwritten_resid)
		uio_setresid(uio, uio_resid(uio) + unwritten_resid);

	if (error && new_eof > orig_size)
		ubc_setsize(vp, (off_t)orig_size);

	IOLockLock((IOLock *)ep->e_lock);
	if (error == 0 && new_eof > ep->e_size)
		ep->e_size = new_eof;
	if (ep->e_new_size <= ep->e_size)
		ep->e_new_size = 0;
	IOLockUnlock((IOLock *)ep->e_lock);

	if (error == 0 || uio_offset(uio) != offset) {
		struct timeval tv;

		microtime(&tv);
		ext4_fs_lock(emp);
		ep->e_raw.i_size_lo = le32((uint32_t)ep->e_size);
		ep->e_raw.i_size_high = le32((uint32_t)(ep->e_size >> 32));
		ep->e_raw.i_ctime = le32((uint32_t)tv.tv_sec);
		ep->e_raw.i_mtime = le32((uint32_t)tv.tv_sec);
		(void)ext4_write_inode(emp, ep->e_ino, &ep->e_raw);
		ext4_fs_unlock(emp);
	}
out:
	IORWLockUnlock((IORWLock *)ep->e_tlock);
	return error;
}

// zero [boff, boff+len) of a data block on the device now, and leave nothing cached for it
static int
ext4_zero_block_part(struct ext4mount *emp, uint64_t pblk, uint32_t boff, uint32_t len)
{
	buf_t bp = NULL;
	int error = ext4_blkread(emp, pblk, &bp);

	if (error)
		return error;
	memset((char *)buf_dataptr(bp) + boff, 0, len);
	buf_markinvalid(bp);
	return buf_bwrite(bp);
}

static int
ext4_zero_range(struct ext4node *ep, uint64_t start, uint64_t end)
{
	struct ext4mount *emp = ep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint64_t off;
	int error;

	for (off = start; off < end; ) {
		uint32_t lblk = (uint32_t)(off / bs);
		uint32_t boff = (uint32_t)(off % bs);
		size_t chunk = bs - boff;
		uint64_t pblk = 0;

		if (chunk > end - off)
			chunk = (size_t)(end - off);

		error = ext4_ensure_block(ep, lblk, &pblk);
		if (error)
			return error;
		error = ext4_zero_block_part(emp, pblk, boff, (uint32_t)chunk);
		if (error)
			return error;
		off += chunk;
	}
	return 0;
}

static int
ext4_resize_file(struct ext4node *ep, uint64_t new_size)
{
	struct ext4mount *emp = ep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint64_t old_size = ep->e_size;
	uint64_t old_blocks = (ep->e_size + bs - 1) / bs;
	uint64_t new_blocks = (new_size + bs - 1) / bs;
	uint64_t i;
	int error;

	if (ep->e_vtype != VREG)
		return EISDIR;
	if (new_blocks < old_blocks) {
		if ((new_size % bs) != 0 && new_blocks != 0) {
			uint64_t pblk = 0;
			uint32_t tail = (uint32_t)(new_size % bs);

			error = ext4_bmap(emp, ep->e_ino, &ep->e_raw, (uint32_t)(new_blocks - 1), &pblk);
			if (error)
				return error;
			if (pblk != 0) {
				error = ext4_zero_block_part(emp, pblk, tail, bs - tail);
				if (error)
					return error;
			}
		}
		error = ext4_inode_truncate_extents(emp, ep->e_ino, &ep->e_raw, new_blocks);
		if (error)
			return error;
	} else if (new_blocks > old_blocks || new_size > old_size) {
		/* Growing allocates nothing, so nothing else will re-create the
		 * extent header that ext4_inode_free_extents() zeroes when a file
		 * is truncated to zero and then grown again. */
		ext4_inode_init_extent_header(&ep->e_raw);

		if ((old_size % bs) != 0) {
			uint64_t tail_end = ((old_size + bs - 1) / bs) * bs;
			if (tail_end > new_size)
				tail_end = new_size;
			if (tail_end > old_size) {
				error = ext4_zero_range(ep, old_size, tail_end);
				if (error)
					return error;
			}
		}
	}
	ep->e_size = new_size;
	ep->e_raw.i_size_lo = le32((uint32_t)new_size);
	ep->e_raw.i_size_high = le32((uint32_t)(new_size >> 32));
	/* Truncating to the length the file already has changes no data, so it
	 * is a metadata-only event. */
	if (new_size != old_size)
		ext4_touch_inode(&ep->e_raw);
	else
		ext4_touch_ctime(&ep->e_raw);
	ubc_setsize(ep->e_vp, ep->e_size);
	return ext4_write_inode(emp, ep->e_ino, &ep->e_raw);
}

static int
ext4_vnop_setattr_impl(struct vnop_setattr_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct vnode_attr *vap = ap->a_vap;
	int error = 0;

	if (VATTR_IS_ACTIVE(vap, va_data_size)) {
		error = ext4_resize_file(ep, vap->va_data_size);
		if (error)
			return error;
		VATTR_SET_SUPPORTED(vap, va_data_size);
	}
	if (VATTR_IS_ACTIVE(vap, va_mode)) {
		ep->e_raw.i_mode = le16((uint16_t)((le16(ep->e_raw.i_mode) & EXT4_S_IFMT) |
		    (vap->va_mode & 07777)));
		VATTR_SET_SUPPORTED(vap, va_mode);
	}
	if (VATTR_IS_ACTIVE(vap, va_uid)) {
		ep->e_raw.i_uid = le16((uint16_t)vap->va_uid);
		VATTR_SET_SUPPORTED(vap, va_uid);
	}
	if (VATTR_IS_ACTIVE(vap, va_gid)) {
		ep->e_raw.i_gid = le16((uint16_t)vap->va_gid);
		VATTR_SET_SUPPORTED(vap, va_gid);
	}
	if (VATTR_IS_ACTIVE(vap, va_access_time)) {
		ep->e_raw.i_atime = le32((uint32_t)vap->va_access_time.tv_sec);
		VATTR_SET_SUPPORTED(vap, va_access_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
		ep->e_raw.i_mtime = le32((uint32_t)vap->va_modify_time.tv_sec);
		VATTR_SET_SUPPORTED(vap, va_modify_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_flags)) {
		error = ext4_set_bsd_flags(ep, vap->va_flags);
		if (error)
			return error;
		VATTR_SET_SUPPORTED(vap, va_flags);
	}
	ext4_touch_ctime(&ep->e_raw);
	return ext4_write_inode(ep->e_mount, ep->e_ino, &ep->e_raw);
}

static int
ext4_vnop_create_impl(struct vnop_create_args *ap)
{
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct vnode_attr *vap = ap->a_vap;
	struct ext4node *dep = VTOE(dvp);
	struct ext4mount *emp = dep->e_mount;
	struct ext4_inode raw;
	struct timeval tv;
	ino_t ino;
	mode_t mode = 0644;
	uid_t uid = 0;
	gid_t gid = 0;
	int error;

	enum vtype vtype = VREG;

	*vpp = NULLVP;
	if (!vnode_isdir(dvp))
		return ENOTDIR;
	if (VATTR_IS_ACTIVE(vap, va_type))
		vtype = vap->va_type;
	if (vtype != VREG && vtype != VSOCK && vtype != VFIFO && vtype != VCHR && vtype != VBLK)
		return ENOTSUP;
	if ((vtype == VCHR || vtype == VBLK) && !VATTR_IS_ACTIVE(vap, va_rdev))
		return EINVAL;
	if (cnp->cn_namelen == 0 || cnp->cn_namelen > 255)
		return ENAMETOOLONG;

	error = ext4_dir_lookup(dep, cnp->cn_nameptr, cnp->cn_namelen, &ino);
	if (error == 0)
		return EEXIST;
	if (error != ENOENT)
		return error;

	error = ext4_alloc_inode(emp, vtype, &ino);
	if (error)
		return error;

	if (VATTR_IS_ACTIVE(vap, va_mode))
		mode = vap->va_mode & 07777;
	if (VATTR_IS_ACTIVE(vap, va_uid))
		uid = vap->va_uid;
	if (VATTR_IS_ACTIVE(vap, va_gid))
		gid = vap->va_gid;

	memset(&raw, 0, sizeof(raw));
	microtime(&tv);
	{
		uint16_t fmt = vtype == VSOCK ? EXT4_S_IFSOCK : vtype == VFIFO ? EXT4_S_IFIFO :
		    vtype == VCHR ? EXT4_S_IFCHR : vtype == VBLK ? EXT4_S_IFBLK : EXT4_S_IFREG;
		raw.i_mode = le16((uint16_t)(fmt | mode));
	}
	raw.i_uid = le16((uint16_t)uid);
	raw.i_gid = le16((uint16_t)gid);
	raw.i_atime = le32((uint32_t)tv.tv_sec);
	raw.i_ctime = le32((uint32_t)tv.tv_sec);
	raw.i_mtime = le32((uint32_t)tv.tv_sec);
	raw.i_links_count = le16(1);
	raw.i_extra_isize = le16(ext4_new_extra_isize(emp));
	/*
	 * Only a file that can hold data gets an extent tree. A socket has no
	 * blocks, and stamping an extent header into its i_block[] made e2fsck
	 * call it "an illegal socket" and then want the parent's dirent
	 * filetype cleared along with it.
	 */
	if (vtype == VCHR || vtype == VBLK)
		ext4_inode_set_rdev(&raw, vap->va_rdev);
	if (vtype == VREG) {
		raw.i_flags = le32(EXT4_EXTENTS_FL);
		(void)ext4_inode_append_extent(emp, ino, &raw, 0, 0);
		((struct ext4_extent_header *)raw.i_block)->eh_entries = 0;
	}

	error = ext4_write_inode(emp, ino, &raw);
	if (error) {
		(void)ext4_free_inode(emp, ino, vtype);
		return error;
	}
	error = ext4_dir_add(dep, cnp->cn_nameptr, cnp->cn_namelen, ino, vtype);
	if (error) {
		raw.i_links_count = 0;
		(void)ext4_write_inode(emp, ino, &raw);
		(void)ext4_free_inode(emp, ino, vtype);
		return error;
	}
	dep->e_raw.i_ctime = le32((uint32_t)tv.tv_sec);
	dep->e_raw.i_mtime = le32((uint32_t)tv.tv_sec);
	(void)ext4_write_inode(emp, dep->e_ino, &dep->e_raw);
	cache_purge(dvp);

	VATTR_SET_SUPPORTED(vap, va_type);
	VATTR_SET_SUPPORTED(vap, va_mode);
	VATTR_SET_SUPPORTED(vap, va_uid);
	VATTR_SET_SUPPORTED(vap, va_gid);
	if (vtype == VCHR || vtype == VBLK)
		VATTR_SET_SUPPORTED(vap, va_rdev);

	return ext4_vget(emp, ino, dvp, vpp, cnp);
}

static int
ext4_vnop_create_impl(struct vnop_create_args *ap);

static int
ext4_vnop_mknod_impl(struct vnop_mknod_args *ap)
{
	return ext4_vnop_create_impl((struct vnop_create_args *)ap);
}

static int
ext4_seed_dir_block(struct ext4mount *emp, uint64_t pblk, ino_t self, ino_t parent)
{
	buf_t bp = NULL;
	struct ext4_dir_entry_2 *de;
	char *data;
	int error;

	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;
	data = (char *)buf_dataptr(bp);
	memset(data, 0, emp->em_blocksize);

	de = (struct ext4_dir_entry_2 *)data;
	de->inode = le32((uint32_t)self);
	de->rec_len = le16(ext4_dir_rec_len(1));
	de->name_len = 1;
	de->file_type = EXT4_FT_DIR;
	de->name[0] = '.';

	de = (struct ext4_dir_entry_2 *)(data + ext4_dir_rec_len(1));
	de->inode = le32((uint32_t)parent);
	de->rec_len = le16((uint16_t)(emp->em_blocksize - ext4_dir_rec_len(1) -
	    (emp->em_has_metadata_csum ? EXT4_DIR_ENTRY_TAIL_SIZE : 0)));
	de->name_len = 2;
	de->file_type = EXT4_FT_DIR;
	de->name[0] = '.';
	de->name[1] = '.';

	ext4_dir_block_init_tail(emp, data);
	ext4_dir_block_csum_set(emp, self, NULL, data);
	return ext4_meta_bwrite(emp, bp);
}

static int
ext4_vnop_mkdir_impl(struct vnop_mkdir_args *ap)
{
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct vnode_attr *vap = ap->a_vap;
	struct ext4node *dep = VTOE(dvp);
	struct ext4mount *emp = dep->e_mount;
	struct ext4_inode raw;
	struct timeval tv;
	ino_t ino;
	uint64_t pblk;
	mode_t mode = 0755;
	uid_t uid = 0;
	gid_t gid = 0;
	int error;

	*vpp = NULLVP;
	if (!vnode_isdir(dvp))
		return ENOTDIR;
	if (cnp->cn_namelen == 0 || cnp->cn_namelen > 255)
		return ENAMETOOLONG;

	error = ext4_dir_lookup(dep, cnp->cn_nameptr, cnp->cn_namelen, &ino);
	if (error == 0)
		return EEXIST;
	if (error != ENOENT)
		return error;

	error = ext4_alloc_inode(emp, VDIR, &ino);
	if (error)
		return error;
	error = ext4_alloc_block(emp, 0, &pblk);
	if (error) {
		(void)ext4_free_inode(emp, ino, VDIR);
		return error;
	}

	if (VATTR_IS_ACTIVE(vap, va_mode))
		mode = vap->va_mode & 07777;
	if (VATTR_IS_ACTIVE(vap, va_uid))
		uid = vap->va_uid;
	if (VATTR_IS_ACTIVE(vap, va_gid))
		gid = vap->va_gid;

	memset(&raw, 0, sizeof(raw));
	microtime(&tv);
	raw.i_mode = le16((uint16_t)(EXT4_S_IFDIR | mode));
	raw.i_uid = le16((uint16_t)uid);
	raw.i_gid = le16((uint16_t)gid);
	raw.i_size_lo = le32(emp->em_blocksize);
	raw.i_atime = le32((uint32_t)tv.tv_sec);
	raw.i_ctime = le32((uint32_t)tv.tv_sec);
	raw.i_mtime = le32((uint32_t)tv.tv_sec);
	raw.i_links_count = le16(2);
	raw.i_blocks_lo = le32(emp->em_blocksize / 512);
	raw.i_flags = le32(EXT4_EXTENTS_FL);
	raw.i_extra_isize = le16(ext4_new_extra_isize(emp));
	error = ext4_inode_append_extent(emp, ino, &raw, 0, pblk);
	if (error) {
		(void)ext4_free_block(emp, pblk);
		(void)ext4_free_inode(emp, ino, VDIR);
		return error;
	}
	error = ext4_seed_dir_block(emp, pblk, ino, dep->e_ino);
	if (error) {
		(void)ext4_inode_free_extents(emp, &raw);
		(void)ext4_free_inode(emp, ino, VDIR);
		return error;
	}
	error = ext4_write_inode(emp, ino, &raw);
	if (error) {
		(void)ext4_inode_free_extents(emp, &raw);
		(void)ext4_free_inode(emp, ino, VDIR);
		return error;
	}
	error = ext4_dir_add(dep, cnp->cn_nameptr, cnp->cn_namelen, ino, VDIR);
	if (error) {
		(void)ext4_inode_free_extents(emp, &raw);
		(void)ext4_free_inode(emp, ino, VDIR);
		return error;
	}

	dep->e_raw.i_links_count = le16((uint16_t)(le16(dep->e_raw.i_links_count) + 1));
	dep->e_raw.i_ctime = le32((uint32_t)tv.tv_sec);
	dep->e_raw.i_mtime = le32((uint32_t)tv.tv_sec);
	(void)ext4_write_inode(emp, dep->e_ino, &dep->e_raw);
	cache_purge(dvp);

	VATTR_SET_SUPPORTED(vap, va_type);
	VATTR_SET_SUPPORTED(vap, va_mode);
	VATTR_SET_SUPPORTED(vap, va_uid);
	VATTR_SET_SUPPORTED(vap, va_gid);

	return ext4_vget(emp, ino, dvp, vpp, cnp);
}

static int
ext4_vnop_remove_impl(struct vnop_remove_args *ap)
{
	struct ext4node *dep = VTOE(ap->a_dvp);
	struct ext4node *ep = VTOE(ap->a_vp);
	ino_t removed;
	int error;

	if (vnode_isdir(ap->a_vp))
		return EISDIR;
	error = ext4_dir_remove(dep, ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen,
	    &removed, NULL);
	if (error)
		return error;
	ext4_touch_inode(&dep->e_raw);
	(void)ext4_write_inode(dep->e_mount, dep->e_ino, &dep->e_raw);
	cache_purge(ap->a_dvp);
	cache_purge(ap->a_vp);
	if (removed == ep->e_ino)
		return ext4_drop_inode(ep);
	E4LOG("remove: stale vnode for '%.*s' (vp ino %llu, dir ino %llu)",
	    (int)ap->a_cnp->cn_namelen, ap->a_cnp->cn_nameptr,
	    (unsigned long long)ep->e_ino, (unsigned long long)removed);
	/*
	 * `removed` is whatever the directory entry actually pointed to,
	 * which can diverge from the vnode VFS handed us (a lookup/unlink
	 * race, or the name got recreated under us). It must NOT be freed
	 * blindly: if it's still a live, open inode (e.g. an unrelated file
	 * that happens to share this number in some other, still-unexplained
	 * way), dropping its extents corrupts that file out from under
	 * whoever has it open. Only free it if nothing has it open.
	 */
	if (ext4_ino_is_live(dep->e_mount, removed)) {
		E4LOG("remove: refusing to free live ino %llu (dir said "
		    "removed, but it's still open)", (unsigned long long)removed);
		return 0;
	}
	return ext4_drop_inode_ino(dep->e_mount, removed);
}

static int
ext4_vnop_rmdir_impl(struct vnop_rmdir_args *ap)
{
	struct ext4node *dep = VTOE(ap->a_dvp);
	struct ext4node *ep = VTOE(ap->a_vp);
	ino_t removed;
	int error;

	if (!vnode_isdir(ap->a_vp))
		return ENOTDIR;
	if (ap->a_cnp->cn_namelen == 1 && ap->a_cnp->cn_nameptr[0] == '.')
		return EINVAL;
	error = ext4_dir_is_empty(ep);
	if (error)
		return error;
	error = ext4_dir_remove(dep, ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen,
	    &removed, NULL);
	if (error)
		return error;
	if (le16(dep->e_raw.i_links_count) > 0)
		dep->e_raw.i_links_count =
		    le16((uint16_t)(le16(dep->e_raw.i_links_count) - 1));
	ext4_touch_inode(&dep->e_raw);
	(void)ext4_write_inode(dep->e_mount, dep->e_ino, &dep->e_raw);
	cache_purge(ap->a_dvp);
	cache_purge(ap->a_vp);
	if (removed == ep->e_ino)
		return ext4_drop_inode(ep);
	E4LOG("rmdir: stale vnode for '%.*s' (vp ino %llu, dir ino %llu)",
	    (int)ap->a_cnp->cn_namelen, ap->a_cnp->cn_nameptr,
	    (unsigned long long)ep->e_ino, (unsigned long long)removed);
	if (ext4_ino_is_live(dep->e_mount, removed)) {
		E4LOG("rmdir: refusing to free live ino %llu (dir said "
		    "removed, but it's still open)", (unsigned long long)removed);
		return 0;
	}
	return ext4_drop_inode_ino(dep->e_mount, removed);
}

static int
ext4_vnop_rename_impl(struct vnop_rename_args *ap)
{
	struct ext4node *fdep = VTOE(ap->a_fdvp);
	struct ext4node *fep = VTOE(ap->a_fvp);
	struct ext4node *tdep = VTOE(ap->a_tdvp);
	struct ext4node *tep = ap->a_tvp ? VTOE(ap->a_tvp) : NULL;
	bool moving_dir = vnode_isdir(ap->a_fvp);
	bool cross_dir = ap->a_fdvp != ap->a_tdvp;
	int error;

	if (ap->a_fvp == ap->a_tvp)
		return 0;
	/*
	 * Different vnode pointers must never cause us to replace and then free
	 * the same underlying inode. This can happen if stale/duplicate vnodes
	 * survived earlier filesystem corruption.
	 */
	if (tep != NULL && fep->e_ino == tep->e_ino) {
		E4LOG("rename: source and target vnodes share ino %llu",
		    (unsigned long long)fep->e_ino);
		return 0;
	}
	if (moving_dir) {
		if (tep && !vnode_isdir(ap->a_tvp))
			return ENOTDIR;
		if (tep) {
			error = ext4_dir_is_empty(tep);
			if (error)
				return error;
		}
	} else if (tep && vnode_isdir(ap->a_tvp)) {
		return EISDIR;
	}

	if (tep) {
		ino_t old_ino = 0;
		uint8_t old_ft = EXT4_FT_UNKNOWN;

		error = ext4_dir_replace(tdep, ap->a_tcnp->cn_nameptr,
		    ap->a_tcnp->cn_namelen, fep->e_ino, fep->e_vtype,
		    &old_ino, &old_ft);
		if (error)
			return error;
		error = ext4_dir_remove(fdep, ap->a_fcnp->cn_nameptr,
		    ap->a_fcnp->cn_namelen, NULL, NULL);
		if (error) {
			(void)ext4_dir_replace(tdep, ap->a_tcnp->cn_nameptr,
			    ap->a_tcnp->cn_namelen, old_ino,
			    ext4_ft_to_vtype(old_ft), NULL, NULL);
			return error;
		}
		if (vnode_isdir(ap->a_tvp) && le16(tdep->e_raw.i_links_count) > 0)
			tdep->e_raw.i_links_count =
			    le16((uint16_t)(le16(tdep->e_raw.i_links_count) - 1));
		(void)ext4_drop_inode(tep);
	} else {
		error = ext4_dir_add(tdep, ap->a_tcnp->cn_nameptr,
		    ap->a_tcnp->cn_namelen, fep->e_ino, fep->e_vtype);
		if (error)
			return error;
		error = ext4_dir_remove(fdep, ap->a_fcnp->cn_nameptr,
		    ap->a_fcnp->cn_namelen, NULL, NULL);
		if (error) {
			(void)ext4_dir_remove(tdep, ap->a_tcnp->cn_nameptr,
			    ap->a_tcnp->cn_namelen, NULL, NULL);
			return error;
		}
	}
	if (moving_dir && cross_dir) {
		(void)ext4_dir_update_dotdot(fep, tdep->e_ino);
		if (le16(fdep->e_raw.i_links_count) > 0)
			fdep->e_raw.i_links_count =
			    le16((uint16_t)(le16(fdep->e_raw.i_links_count) - 1));
		tdep->e_raw.i_links_count =
		    le16((uint16_t)(le16(tdep->e_raw.i_links_count) + 1));
	}
	ext4_touch_inode(&fdep->e_raw);
	ext4_touch_inode(&tdep->e_raw);
	(void)ext4_write_inode(fdep->e_mount, fdep->e_ino, &fdep->e_raw);
	if (tdep != fdep)
		(void)ext4_write_inode(tdep->e_mount, tdep->e_ino, &tdep->e_raw);
	cache_purge(ap->a_fdvp);
	cache_purge(ap->a_tdvp);
	cache_purge(ap->a_fvp);
	if (ap->a_tvp)
		cache_purge(ap->a_tvp);
	return 0;
}

static int
ext4_vnop_symlink_impl(struct vnop_symlink_args *ap)
{
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct vnode_attr *vap = ap->a_vap;
	struct ext4node *dep = VTOE(dvp);
	struct ext4mount *emp = dep->e_mount;
	struct ext4_inode raw;
	struct timeval tv;
	ino_t ino;
	size_t len = strlen(ap->a_target);
	mode_t mode = 0777;
	uid_t uid = 0;
	gid_t gid = 0;
	int error;

	*vpp = NULLVP;
	if (!vnode_isdir(dvp))
		return ENOTDIR;
	if (cnp->cn_namelen == 0 || cnp->cn_namelen > 255)
		return ENAMETOOLONG;
	if (len > emp->em_blocksize)
		return ENAMETOOLONG;

	error = ext4_dir_lookup(dep, cnp->cn_nameptr, cnp->cn_namelen, &ino);
	if (error == 0)
		return EEXIST;
	if (error != ENOENT)
		return error;

	error = ext4_alloc_inode(emp, VLNK, &ino);
	if (error)
		return error;

	if (VATTR_IS_ACTIVE(vap, va_mode))
		mode = vap->va_mode & 07777;
	if (VATTR_IS_ACTIVE(vap, va_uid))
		uid = vap->va_uid;
	if (VATTR_IS_ACTIVE(vap, va_gid))
		gid = vap->va_gid;

	memset(&raw, 0, sizeof(raw));
	microtime(&tv);
	raw.i_mode = le16((uint16_t)(EXT4_S_IFLNK | mode));
	raw.i_uid = le16((uint16_t)uid);
	raw.i_gid = le16((uint16_t)gid);
	raw.i_size_lo = le32((uint32_t)len);
	raw.i_size_high = le32((uint32_t)((uint64_t)len >> 32));
	raw.i_atime = le32((uint32_t)tv.tv_sec);
	raw.i_ctime = le32((uint32_t)tv.tv_sec);
	raw.i_mtime = le32((uint32_t)tv.tv_sec);
	raw.i_links_count = le16(1);
	raw.i_extra_isize = le16(ext4_new_extra_isize(emp));

	if (len < sizeof(raw.i_block)) {
		memcpy(raw.i_block, ap->a_target, len);
	} else {
		uint64_t pblk = 0;
		buf_t bp = NULL;

		raw.i_flags = le32(EXT4_EXTENTS_FL);
		error = ext4_alloc_block(emp, 0, &pblk);
		if (error)
			goto fail_inode;
		error = ext4_inode_append_extent(emp, ino, &raw, 0, pblk);
		if (error) {
			(void)ext4_free_block(emp, pblk);
			goto fail_inode;
		}
		raw.i_blocks_lo = le32(emp->em_blocksize / 512);
		error = ext4_blkread(emp, pblk, &bp);
		if (error)
			goto fail_extents;
		memset((void *)buf_dataptr(bp), 0, emp->em_blocksize);
		memcpy((void *)buf_dataptr(bp), ap->a_target, len);
		error = buf_bawrite(bp);
		if (error)
			goto fail_extents;
	}

	error = ext4_write_inode(emp, ino, &raw);
	if (error)
		goto fail_extents;
	error = ext4_dir_add(dep, cnp->cn_nameptr, cnp->cn_namelen, ino, VLNK);
	if (error)
		goto fail_extents;
	ext4_touch_inode(&dep->e_raw);
	(void)ext4_write_inode(emp, dep->e_ino, &dep->e_raw);
	cache_purge(dvp);
	return ext4_vget(emp, ino, dvp, vpp, cnp);

fail_extents:
	(void)ext4_inode_free_extents(emp, &raw);
fail_inode:
	(void)ext4_free_inode(emp, ino, VLNK);
	return error;
}

static int
ext4_vnop_link_impl(struct vnop_link_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4node *tdp = VTOE(ap->a_tdvp);
	ino_t existing;
	uint16_t links;
	int error;

	if (vnode_isdir(ap->a_vp))
		return EPERM;
	if (!vnode_isdir(ap->a_tdvp))
		return ENOTDIR;
	if (ap->a_cnp->cn_namelen == 0 || ap->a_cnp->cn_namelen > 255)
		return ENAMETOOLONG;

	error = ext4_dir_lookup(tdp, ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen,
	    &existing);
	if (error == 0)
		return EEXIST;
	if (error != ENOENT)
		return error;

	links = le16(ep->e_raw.i_links_count);
	if (links == UINT16_MAX)
		return EMLINK;

	error = ext4_dir_add(tdp, ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen,
	    ep->e_ino, ep->e_vtype);
	if (error)
		return error;

	ep->e_raw.i_links_count = le16((uint16_t)(links + 1));
	ext4_touch_inode(&ep->e_raw);
	error = ext4_write_inode(ep->e_mount, ep->e_ino, &ep->e_raw);
	if (error) {
		(void)ext4_dir_remove(tdp, ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen,
		    NULL, NULL);
		ep->e_raw.i_links_count = le16(links);
		return error;
	}
	ext4_touch_inode(&tdp->e_raw);
	(void)ext4_write_inode(tdp->e_mount, tdp->e_ino, &tdp->e_raw);
	cache_purge(ap->a_tdvp);
	cache_purge(ap->a_vp);
	return 0;
}

static int
ext4_vnop_readdir_impl(struct vnop_readdir_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct ext4node *dep = VTOE(vp);
	struct ext4mount *emp = dep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint64_t off = (uint64_t)uio_offset(uio);
	char *blk;
	int error = 0;
	int numdir = 0;

	if (ap->a_flags & VNODE_READDIR_EXTENDED)
		return EINVAL;   /* only classic dirent supported */
	if (!vnode_isdir(vp))
		return ENOTDIR;

	blk = (char *)_MALLOC(bs, M_TEMP, M_WAITOK);
	if (blk == NULL)
		return ENOMEM;

	while (off < dep->e_size && uio_resid(uio) > 0) {
		uint64_t blkoff = (off / bs) * bs;
		uint32_t within = (uint32_t)(off - blkoff);
		uint64_t pblk = 0;
		buf_t bp = NULL;
		uint32_t p;

		if (ext4_bmap(emp, dep->e_ino, &dep->e_raw, (uint32_t)(blkoff / bs), &pblk) ||
		    pblk == 0) {
			off = blkoff + bs;
			continue;
		}
		if (ext4_blkread(emp, pblk, &bp)) {
			off = blkoff + bs;
			continue;
		}
		memcpy(blk, (char *)buf_dataptr(bp), bs);
		buf_brelse(bp);

		for (p = within; p < bs; ) {
			struct ext4_dir_entry_2 *de =
			    (struct ext4_dir_entry_2 *)(blk + p);
			uint16_t reclen = le16(de->rec_len);
			struct dirent dent;

			if (reclen < 8 || p + reclen > bs)
				break;
			if (de->inode != 0) {
				uint8_t nl = de->name_len;
				if (nl > NAME_MAX) nl = NAME_MAX;
				memset(&dent, 0, sizeof(dent));
				dent.d_ino    = le32(de->inode);
				dent.d_type   = ext4_ft_to_dtype(de->file_type);
				dent.d_namlen = nl;
				memcpy(dent.d_name, de->name, nl);
				dent.d_name[nl] = '\0';
				dent.d_reclen = (uint16_t)
				    (offsetof(struct dirent, d_name) + nl + 1 + 3) & ~3;

				if ((user_size_t)dent.d_reclen > uio_resid(uio))
					goto done;
				error = uiomove((caddr_t)&dent, dent.d_reclen, uio);
				if (error)
					goto done;
				numdir++;
			}
			p += reclen;
			off = blkoff + p;
		}
		off = blkoff + bs;
	}
done:
	_FREE(blk, M_TEMP);
	uio_setoffset(uio, off);
	if (ap->a_eofflag)
		*ap->a_eofflag = (off >= dep->e_size);
	if (ap->a_numdirent)
		*ap->a_numdirent = numdir;
	return error;
}

static int
ext4_vnop_readlink_impl(struct vnop_readlink_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct ext4node *ep = VTOE(vp);
	struct ext4mount *emp = ep->e_mount;
	uint64_t size = ep->e_size;
	int error;

	if (!vnode_islnk(vp))
		return EINVAL;

	/* fast symlink: target stored inline in i_block[] when size < 60 */
	// an xattr block counts in i_blocks without holding the target, as Linux reads it
	if (size < 60 && le32(ep->e_raw.i_blocks_lo) ==
	    (ext4_inode_xattr_block(emp, &ep->e_raw) ? emp->em_blocksize / 512 : 0)) {
		return uiomove((caddr_t)ep->e_raw.i_block, (int)size, uio);
	}

	/* slow symlink: target in the first data block */
	{
		uint64_t pblk = 0;
		buf_t bp = NULL;
		error = ext4_bmap(emp, ep->e_ino, &ep->e_raw, 0, &pblk);
		if (error)
			return error;
		if (pblk == 0)
			return EIO;
		error = ext4_blkread(emp, pblk, &bp);
		if (error)
			return error;
		error = uiomove((caddr_t)buf_dataptr(bp),
		    (int)(size > emp->em_blocksize ? emp->em_blocksize : size), uio);
		buf_brelse(bp);
		return error;
	}
}

static int
ext4_vnop_pagein_impl(struct vnop_pagein_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);

	return cluster_pagein(ap->a_vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
	    (size_t)ap->a_size, (off_t)ext4_size_snapshot(ep), ap->a_flags);
}

static int
ext4_vnop_pageout_impl(struct vnop_pageout_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);

	if (ep->e_freed) {
		// the file is gone, its dirty pages have nowhere to go
		if (ap->a_pl != NULL && !(ap->a_flags & UPL_NOCOMMIT))
			ubc_upl_commit_range(ap->a_pl, ap->a_pl_offset, (upl_size_t)ap->a_size,
			    UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY);
		return 0;
	}
	return cluster_pageout(ap->a_vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
	    (size_t)ap->a_size, (off_t)ext4_pageout_limit(ep), ap->a_flags);
}

// cluster_io finds its blocks here: runs of physically contiguous blocks, -1 for holes
// and unwritten extents (read as zeroes), and blocks are allocated for writes into holes
static int
ext4_vnop_blockmap_impl(struct vnop_blockmap_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4mount *emp = ep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint64_t scale = bs / emp->em_dev_bsize;
	uint64_t pblk = 0, first;
	uint32_t lblk, boff, want_blocks, run = 0, total;
	int unwritten = 0;
	size_t bytes;
	int error;

	if (vnode_isdir(ap->a_vp))
		return ENOTSUP;
	if (ap->a_bpn == NULL)
		return 0;
	if (ap->a_foffset < 0)
		return EINVAL;

	lblk = (uint32_t)(ap->a_foffset / bs);
	boff = (uint32_t)(ap->a_foffset % bs);
	bytes = ap->a_size ? ap->a_size : bs;
	want_blocks = (uint32_t)((boff + bytes + bs - 1) / bs);

	error = ext4_bmap_run(emp, ep->e_ino, &ep->e_raw, lblk, want_blocks, &pblk, &run, &unwritten);
	if (error)
		return error;
	if ((pblk == 0 || unwritten) && (ap->a_flags & VNODE_WRITE)) {
		bool grew = false;

		if (ep->e_freed)
			return ENOENT;
		error = ext4_alloc_range(ep, lblk, want_blocks, &grew);
		if (grew)
			(void)ext4_write_inode(emp, ep->e_ino, &ep->e_raw);
		if (error)
			return error;
		error = ext4_bmap_run(emp, ep->e_ino, &ep->e_raw, lblk, want_blocks, &pblk, &run, &unwritten);
		if (error)
			return error;
	}
	if (pblk == 0 || unwritten) {
		*ap->a_bpn = (daddr64_t)-1;
		if (ap->a_run)
			*ap->a_run = (size_t)run * bs - boff;
		return 0;
	}

	// extend over following extents that continue on the disk
	first = pblk;
	total = run;
	while (total < want_blocks) {
		uint64_t npblk = 0;
		uint32_t nrun = 0;
		int nunw = 0;

		if (ext4_bmap_run(emp, ep->e_ino, &ep->e_raw, lblk + total, want_blocks - total,
		    &npblk, &nrun, &nunw) != 0 || npblk != first + total || nunw || nrun == 0)
			break;
		total += nrun;
	}
	*ap->a_bpn = (daddr64_t)(pblk * scale + boff / emp->em_dev_bsize);
	if (ap->a_run)
		*ap->a_run = (size_t)total * bs - boff;
	return 0;
}

// file data goes to the device the same way for every regular file
static int
ext4_vnop_strategy(struct vnop_strategy_args *ap)
{
	struct ext4node *ep = VTOE(buf_vnode(ap->a_bp));

	return buf_strategy(ep->e_mount->em_devvp, ap);
}

static int
ext4_vnop_blktooff(struct vnop_blktooff_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);

	if (ep == NULL)
		return EINVAL;
	*ap->a_offset = (off_t)ap->a_lblkno * (off_t)ep->e_mount->em_blocksize;
	return 0;
}

static int
ext4_vnop_offtoblk(struct vnop_offtoblk_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);

	if (ep == NULL)
		return EINVAL;
	*ap->a_lblkno = (daddr64_t)(ap->a_offset / (off_t)ep->e_mount->em_blocksize);
	return 0;
}

static int
ext4_vnop_reclaim(struct vnop_reclaim_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct ext4node *ep = VTOE(vp);

	if (ep) {
		IOLock *hlock = (IOLock *)ep->e_mount->em_hash_lock;
		IOLockLock(hlock);
		if (!ep->e_unhashed) {
			LIST_REMOVE(ep, e_hash);
			ep->e_unhashed = 1;
		}
		IOLockUnlock(hlock);
	}
	vnode_removefsref(vp);
	vnode_clearfsnode(vp);
	if (ep) {
		if (ep->e_lock)
			IOLockFree((IOLock *)ep->e_lock);
		if (ep->e_tlock)
			IORWLockFree((IORWLock *)ep->e_tlock);
		_FREE(ep, M_TEMP);
	}
	return 0;
}

static int
ext4_vnop_open(__unused struct vnop_open_args *ap)
{
	return 0;
}

static int
ext4_vnop_close(__unused struct vnop_close_args *ap)
{
	return 0;
}

static int
ext4_vnop_inactive_impl(struct vnop_inactive_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct ext4node *ep = VTOE(vp);

	/*
	 * Last close of a file that was unlinked while still open (see
	 * ext4_drop_inode): now that nothing references it anymore, actually
	 * free its extents/blocks/inode-bitmap entry, and push it toward
	 * reclaim so the vnode doesn't linger holding a freed inode number.
	 */
	if (ep && ep->e_pending_free) {
		ep->e_pending_free = 0;
		(void)ext4_orphan_del(ep->e_mount, ep->e_ino, le32(ep->e_raw.i_dtime));
		(void)ext4_finish_free_inode(ep);
		vnode_recycle(vp);
	}
	return 0;
}

static int
ext4_vnop_fsync(struct vnop_fsync_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct ext4node *ep = VTOE(vp);

	if (ep == NULL)
		return 0;
	if (vnode_isreg(vp) && ubc_pages_resident(vp))
		(void)ubc_msync(vp, 0, ubc_getsize(vp), NULL,
		    UBC_PUSHDIRTY | UBC_SYNC);
	ext4_fs_lock(ep->e_mount);
	(void)ext4_jnl_commit(ep->e_mount);
	ep->e_mount->em_lock_depth--;
	IORecursiveLockUnlock((IORecursiveLock *)ep->e_mount->em_fs_lock);
	return 0;
}

/*
 * fcntl(F_FULLFSYNC) does NOT arrive as VNOP_FSYNC - XNU's fcntl handler sends
 * it, F_BARRIERFSYNC and F_CHKCLEAN straight to VNOP_IOCTL
 * (bsd/kern/kern_descrip.c). With no ioctl vnop at all they fell through to
 * ext4_vnop_default, which answers ENOTSUP, and userland saw errno 45
 * "Operation not supported". Rust's File::sync_data()/sync_all() are exactly
 * fcntl(F_FULLFSYNC) on Darwin, so anything written through Rust's std -
 * rustup writing settings.toml, cargo writing its caches - failed outright.
 */
static int
ext4_vnop_ioctl(struct vnop_ioctl_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct ext4node *ep = VTOE(vp);
	struct ext4mount *emp;

	if (ep == NULL)
		return ENOTTY;
	emp = ep->e_mount;

	switch (ap->a_command) {
	case F_FULLFSYNC:
	case F_BARRIERFSYNC:
		if (vnode_isreg(vp) && ubc_pages_resident(vp))
			(void)ubc_msync(vp, 0, ubc_getsize(vp), NULL,
			    UBC_PUSHDIRTY | UBC_SYNC);
		ext4_fs_lock(emp);
		(void)ext4_jnl_commit(emp);
		ext4_fs_unlock_nocommit(emp);
		/*
		 * What separates this from plain fsync: the caller is asking for
		 * the data to be on the media, and buf_bwrite only gets it as far
		 * as the device's own write-back cache. Flush that too, the way
		 * unmount already does.
		 */
		if (emp->em_devvp != NULL)
			(void)VNOP_IOCTL(emp->em_devvp, DKIOCSYNCHRONIZECACHE,
			    NULL, FWRITE, ap->a_context);
		return 0;

	case F_CHKCLEAN:
		/* "are this file's dirty pages written back" - after the above,
		 * and for anything we would have flushed, yes. */
		return 0;

#ifdef FSIOC_KERNEL_ROOTAUTH
	case FSIOC_KERNEL_ROOTAUTH:
		/*
		 * Apple Silicon macOS boots a Signed System Volume and bsd_init()
		 * panics if the root file system cannot vouch for its own seal.
		 */
		return 0;
#endif

	default:
		/* ENOTTY, not ENOTSUP: this is the POSIX answer for an ioctl a
		 * file system does not implement, and callers probe for it. */
		return ENOTTY;
	}
}

static int
ext4_vnop_pathconf(struct vnop_pathconf_args *ap)
{
	switch (ap->a_name) {
	case _PC_LINK_MAX: *ap->a_retval = 65000; return 0;
	case _PC_NAME_MAX: *ap->a_retval = 255; return 0;
	case _PC_PATH_MAX: *ap->a_retval = PATH_MAX; return 0;
	case _PC_CHOWN_RESTRICTED: *ap->a_retval = 1; return 0;
	case _PC_NO_TRUNC: *ap->a_retval = 1; return 0;
	case _PC_CASE_SENSITIVE: *ap->a_retval = 1; return 0;
	case _PC_CASE_PRESERVING: *ap->a_retval = 1; return 0;
	default: return EINVAL;
	}
}

static int
ext4_vnop_default(__unused struct vnop_generic_args *ap)
{
	return ENOTSUP;
}

/*
 * Locked vnop wrappers: every metadata-touching vnop runs under the
 * mount-wide recursive em_fs_lock; releasing the outermost hold commits
 * the vnop's journal transaction. VNOP_RECLAIM intentionally stays
 * unwrapped (see the em_fs_lock comment in ext4.h).
 */
static int
ext4_vnop_lookup(struct vnop_lookup_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_dvp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_lookup_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_mknod(struct vnop_mknod_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_dvp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_mknod_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_create(struct vnop_create_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_dvp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_create_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_mkdir(struct vnop_mkdir_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_dvp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_mkdir_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_symlink(struct vnop_symlink_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_dvp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_symlink_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_getattr(struct vnop_getattr_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_getattr_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_setattr(struct vnop_setattr_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4mount *emp = ep->e_mount;
	bool resize = vnode_isreg(ap->a_vp) && VATTR_IS_ACTIVE(ap->a_vap, va_data_size);
	int error;

	if (resize) {
		off_t new_size = (off_t)ap->a_vap->va_data_size;

		IORWLockWrite((IORWLock *)ep->e_tlock);
		if (new_size < (off_t)ext4_size_snapshot(ep)) {
			// push the page that will hold the new EOF so its disk tail can be zeroed
			if ((new_size & (off_t)PAGE_MASK) != 0 && ubc_pages_resident(ap->a_vp))
				(void)ubc_msync(ap->a_vp, trunc_page_64(new_size),
				    trunc_page_64(new_size) + PAGE_SIZE, NULL, UBC_PUSHDIRTY | UBC_SYNC);
			(void)ubc_setsize(ap->a_vp, new_size);
		}
	}
	ext4_fs_lock(emp);
	error = ext4_vnop_setattr_impl(ap);
	ext4_fs_unlock(emp);
	if (resize)
		IORWLockUnlock((IORWLock *)ep->e_tlock);
	return error;
}

/*
 * Read takes em_fs_lock itself, per block read, rather than across the whole
 * call: it must not hold it over uiomove. See ext4_vnop_read_impl.
 */
static int
ext4_vnop_read(struct vnop_read_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;

	ext4_fs_lock(emp);
	emp->em_stats.reads++;
	ext4_fs_unlock_nocommit(emp);
	return ext4_vnop_read_impl(ap);
}

static int
/*
 * Write takes em_fs_lock per block inside the impl rather than across the whole
 * call, for the same reason read does: it must not hold it over uiomove.
 * See ext4_vnop_write_impl.
 */
ext4_vnop_write(struct vnop_write_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;

	ext4_fs_lock(emp);
	emp->em_stats.writes++;
	ext4_fs_unlock_nocommit(emp);
	return ext4_vnop_write_impl(ap);
}

static int
ext4_vnop_remove(struct vnop_remove_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_dvp)->e_mount;
	struct ext4node *rep_ = VTOE(ap->a_vp);
	int error;

	if (le16(rep_->e_raw.i_links_count) == 1 && !vnode_isinuse(ap->a_vp, 0))
		ext4_discard_pages(ap->a_vp);
	ext4_fs_lock(emp);
	error = ext4_vnop_remove_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_rename(struct vnop_rename_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_fdvp)->e_mount;
	int error;

	// a file this rename replaces loses its blocks
	if (ap->a_tvp != NULL && vnode_isreg(ap->a_tvp) &&
	    le16(VTOE(ap->a_tvp)->e_raw.i_links_count) == 1 && !vnode_isinuse(ap->a_tvp, 0))
		ext4_discard_pages(ap->a_tvp);
	ext4_fs_lock(emp);
	error = ext4_vnop_rename_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_link(struct vnop_link_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_link_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_readdir(struct vnop_readdir_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_readdir_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_readlink(struct vnop_readlink_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_readlink_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_pagein(struct vnop_pagein_args *ap)
{
	// cluster_pagein calls back into blockmap, which takes em_fs_lock itself
	VTOE(ap->a_vp)->e_mount->em_stats.pageins++;
	return ext4_vnop_pagein_impl(ap);
}

static int
ext4_vnop_pageout(struct vnop_pageout_args *ap)
{
	VTOE(ap->a_vp)->e_mount->em_stats.pageouts++;
	return ext4_vnop_pageout_impl(ap);
}

static int
ext4_vnop_blockmap(struct vnop_blockmap_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_blockmap_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_rmdir(struct vnop_rmdir_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_dvp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_rmdir_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_inactive(struct vnop_inactive_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4mount *emp = ep->e_mount;
	int error;

	if (ep->e_pending_free)
		ext4_discard_pages(ap->a_vp);
	ext4_fs_lock(emp);
	error = ext4_vnop_inactive_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

/*
 * Native extended attributes in Linux's user namespace, so Linux sees name N
 * as user.N. A name that is not stored here answers ENOTSUP, which sends xnu
 * to the AppleDouble fallback for that one attribute.
 */
static int
ext4_xattr_native(vnode_t vp, const char *name)
{
	if (name != NULL && strcmp(name, XATTR_RESOURCEFORK_NAME) == 0)
		return 0;
	return vnode_isreg(vp) || vnode_isdir(vp) || vnode_islnk(vp);
}

static int
ext4_vnop_getxattr(struct vnop_getxattr_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4mount *emp = ep->e_mount;
	size_t size = 0, cap = ap->a_uio ? (size_t)uio_resid(ap->a_uio) : 0;
	void *buf = NULL;
	int error;

	if (!ext4_xattr_native(ap->a_vp, ap->a_name))
		return ENOTSUP;
	if (cap > emp->em_blocksize)
		cap = emp->em_blocksize;
	if (ap->a_uio != NULL) {
		buf = _MALLOC(cap ? cap : 1, M_TEMP, M_WAITOK);
		if (buf == NULL)
			return ENOMEM;
	}
	ext4_fs_lock(emp);
	error = ext4_xattr_get(emp, ep->e_ino, EXT4_XATTR_INDEX_USER, ap->a_name, buf, cap, &size);
	ext4_fs_unlock_nocommit(emp);
	if (error == 0 && ap->a_uio != NULL)
		error = uiomove(buf, (int)size, ap->a_uio);
	if (error == 0 && ap->a_size != NULL)
		*ap->a_size = size;
	if (buf)
		_FREE(buf, M_TEMP);
	return error == ENOATTR ? ENOTSUP : error;
}

static int
ext4_vnop_setxattr(struct vnop_setxattr_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4mount *emp = ep->e_mount;
	size_t len = (size_t)uio_resid(ap->a_uio);
	void *buf;
	int error;

	if (!ext4_xattr_native(ap->a_vp, ap->a_name) || len > emp->em_blocksize)
		return ENOTSUP;
	if (vfs_isrdonly(vnode_mount(ap->a_vp)))
		return EROFS;
	buf = _MALLOC(len ? len : 1, M_TEMP, M_WAITOK);
	if (buf == NULL)
		return ENOMEM;
	error = uiomove(buf, (int)len, ap->a_uio);
	if (error == 0) {
		ext4_fs_lock(emp);
		error = ext4_xattr_set(ep, EXT4_XATTR_INDEX_USER, ap->a_name, buf, len,
		    ap->a_options & (XATTR_CREATE | XATTR_REPLACE));
		ext4_fs_unlock(emp);
	}
	_FREE(buf, M_TEMP);
	// no room here, or a replace of an attribute that may live in AppleDouble
	if (error == ENOSPC || (error == ENOATTR && (ap->a_options & XATTR_REPLACE)))
		return ENOTSUP;
	return error;
}

static int
ext4_vnop_removexattr(struct vnop_removexattr_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4mount *emp = ep->e_mount;
	int error;

	if (!ext4_xattr_native(ap->a_vp, ap->a_name))
		return ENOTSUP;
	if (vfs_isrdonly(vnode_mount(ap->a_vp)))
		return EROFS;
	ext4_fs_lock(emp);
	error = ext4_xattr_set(ep, EXT4_XATTR_INDEX_USER, ap->a_name, NULL, 0, 0);
	ext4_fs_unlock(emp);
	return error == ENOATTR ? ENOTSUP : error;
}

static int
ext4_vnop_listxattr(struct vnop_listxattr_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4mount *emp = ep->e_mount;
	size_t len = 0, cap = ap->a_uio ? (size_t)uio_resid(ap->a_uio) : 0;
	char *buf = NULL;
	int error;

	if (!ext4_xattr_native(ap->a_vp, NULL))
		return ENOTSUP;
	// the names of one inode fit in its inode and one block
	if (cap > emp->em_blocksize + emp->em_inode_size)
		cap = emp->em_blocksize + emp->em_inode_size;
	if (ap->a_uio != NULL) {
		buf = _MALLOC(cap ? cap : 1, M_TEMP, M_WAITOK);
		if (buf == NULL)
			return ENOMEM;
	}
	ext4_fs_lock(emp);
	error = ext4_xattr_list(emp, ep->e_ino, EXT4_XATTR_INDEX_USER, buf, cap, &len);
	ext4_fs_unlock_nocommit(emp);
	if (error == 0 && ap->a_uio != NULL && len)
		error = uiomove(buf, (int)len, ap->a_uio);
	if (error == 0 && ap->a_size != NULL)
		*ap->a_size += len;
	if (buf)
		_FREE(buf, M_TEMP);
	// xnu then adds any names kept in an AppleDouble file
	return error ? error : ENOTSUP;
}

#define VOPFUNC int (*)(void *)

static const struct vnodeopv_entry_desc ext4_vnodeop_entries[] = {
	{ &vnop_default_desc,  (VOPFUNC)ext4_vnop_default },
	{ &vnop_lookup_desc,   (VOPFUNC)ext4_vnop_lookup },
	{ &vnop_create_desc,   (VOPFUNC)ext4_vnop_create },
	{ &vnop_mknod_desc,    (VOPFUNC)ext4_vnop_mknod },
	{ &vnop_mkdir_desc,    (VOPFUNC)ext4_vnop_mkdir },
	{ &vnop_open_desc,     (VOPFUNC)ext4_vnop_open },
	{ &vnop_close_desc,    (VOPFUNC)ext4_vnop_close },
	{ &vnop_getattr_desc,  (VOPFUNC)ext4_vnop_getattr },
	{ &vnop_setattr_desc,  (VOPFUNC)ext4_vnop_setattr },
	{ &vnop_read_desc,     (VOPFUNC)ext4_vnop_read },
	{ &vnop_write_desc,    (VOPFUNC)ext4_vnop_write },
	{ &vnop_remove_desc,   (VOPFUNC)ext4_vnop_remove },
	{ &vnop_rename_desc,   (VOPFUNC)ext4_vnop_rename },
	{ &vnop_symlink_desc,  (VOPFUNC)ext4_vnop_symlink },
	{ &vnop_link_desc,     (VOPFUNC)ext4_vnop_link },
	{ &vnop_readdir_desc,  (VOPFUNC)ext4_vnop_readdir },
	{ &vnop_readlink_desc, (VOPFUNC)ext4_vnop_readlink },
	{ &vnop_pagein_desc,   (VOPFUNC)ext4_vnop_pagein },
	{ &vnop_pageout_desc,  (VOPFUNC)ext4_vnop_pageout },
	{ &vnop_blockmap_desc, (VOPFUNC)ext4_vnop_blockmap },
	{ &vnop_strategy_desc, (VOPFUNC)ext4_vnop_strategy },
	{ &vnop_blktooff_desc, (VOPFUNC)ext4_vnop_blktooff },
	{ &vnop_offtoblk_desc, (VOPFUNC)ext4_vnop_offtoblk },
	{ &vnop_rmdir_desc,    (VOPFUNC)ext4_vnop_rmdir },
	{ &vnop_inactive_desc, (VOPFUNC)ext4_vnop_inactive },
	{ &vnop_fsync_desc,    (VOPFUNC)ext4_vnop_fsync },
	{ &vnop_ioctl_desc,    (VOPFUNC)ext4_vnop_ioctl },
	{ &vnop_reclaim_desc,  (VOPFUNC)ext4_vnop_reclaim },
	{ &vnop_pathconf_desc, (VOPFUNC)ext4_vnop_pathconf },
	{ &vnop_getxattr_desc,    (VOPFUNC)ext4_vnop_getxattr },
	{ &vnop_setxattr_desc,    (VOPFUNC)ext4_vnop_setxattr },
	{ &vnop_removexattr_desc, (VOPFUNC)ext4_vnop_removexattr },
	{ &vnop_listxattr_desc,   (VOPFUNC)ext4_vnop_listxattr },
	{ NULL, NULL }
};

struct vnodeopv_desc ext4_vnodeop_opv_desc = {
	&ext4_vnodeop_p, ext4_vnodeop_entries
};

// fifos and device nodes keep their inode here, their I/O goes to fifofs and specfs
int (**ext4_fifoop_p)(void *);

static const struct vnodeopv_entry_desc ext4_fifoop_entries[] = {
	{ &vnop_default_desc,  (VOPFUNC)vn_default_error },
	{ &vnop_lookup_desc,   (VOPFUNC)fifo_lookup },
	{ &vnop_create_desc,   (VOPFUNC)fifo_create },
	{ &vnop_mknod_desc,    (VOPFUNC)fifo_mknod },
	{ &vnop_open_desc,     (VOPFUNC)fifo_open },
	{ &vnop_close_desc,    (VOPFUNC)fifo_close },
	{ &vnop_getattr_desc,  (VOPFUNC)ext4_vnop_getattr },
	{ &vnop_setattr_desc,  (VOPFUNC)ext4_vnop_setattr },
	{ &vnop_read_desc,     (VOPFUNC)fifo_read },
	{ &vnop_write_desc,    (VOPFUNC)fifo_write },
	{ &vnop_ioctl_desc,    (VOPFUNC)fifo_ioctl },
	{ &vnop_select_desc,   (VOPFUNC)fifo_select },
	{ &vnop_revoke_desc,   (VOPFUNC)fifo_revoke },
	{ &vnop_mmap_desc,     (VOPFUNC)fifo_mmap },
	{ &vnop_fsync_desc,    (VOPFUNC)ext4_vnop_fsync },
	{ &vnop_remove_desc,   (VOPFUNC)fifo_remove },
	{ &vnop_link_desc,     (VOPFUNC)fifo_link },
	{ &vnop_rename_desc,   (VOPFUNC)fifo_rename },
	{ &vnop_mkdir_desc,    (VOPFUNC)fifo_mkdir },
	{ &vnop_rmdir_desc,    (VOPFUNC)fifo_rmdir },
	{ &vnop_symlink_desc,  (VOPFUNC)fifo_symlink },
	{ &vnop_readdir_desc,  (VOPFUNC)fifo_readdir },
	{ &vnop_readlink_desc, (VOPFUNC)fifo_readlink },
	{ &vnop_inactive_desc, (VOPFUNC)ext4_vnop_inactive },
	{ &vnop_reclaim_desc,  (VOPFUNC)ext4_vnop_reclaim },
	{ &vnop_strategy_desc, (VOPFUNC)fifo_strategy },
	{ &vnop_pathconf_desc, (VOPFUNC)fifo_pathconf },
	{ &vnop_advlock_desc,  (VOPFUNC)err_advlock },
	{ NULL, NULL }
};

struct vnodeopv_desc ext4_fifoop_opv_desc = {
	&ext4_fifoop_p, ext4_fifoop_entries
};

int (**ext4_specop_p)(void *);

static const struct vnodeopv_entry_desc ext4_specop_entries[] = {
	{ &vnop_default_desc,  (VOPFUNC)vn_default_error },
	{ &vnop_lookup_desc,   (VOPFUNC)spec_lookup },
	{ &vnop_create_desc,   (VOPFUNC)spec_create },
	{ &vnop_mknod_desc,    (VOPFUNC)spec_mknod },
	{ &vnop_open_desc,     (VOPFUNC)spec_open },
	{ &vnop_close_desc,    (VOPFUNC)spec_close },
	{ &vnop_getattr_desc,  (VOPFUNC)ext4_vnop_getattr },
	{ &vnop_setattr_desc,  (VOPFUNC)ext4_vnop_setattr },
	{ &vnop_read_desc,     (VOPFUNC)spec_read },
	{ &vnop_write_desc,    (VOPFUNC)spec_write },
	{ &vnop_ioctl_desc,    (VOPFUNC)spec_ioctl },
	{ &vnop_select_desc,   (VOPFUNC)spec_select },
	{ &vnop_revoke_desc,   (VOPFUNC)spec_revoke },
	{ &vnop_mmap_desc,     (VOPFUNC)spec_mmap },
	{ &vnop_fsync_desc,    (VOPFUNC)ext4_vnop_fsync },
	{ &vnop_remove_desc,   (VOPFUNC)spec_remove },
	{ &vnop_link_desc,     (VOPFUNC)spec_link },
	{ &vnop_rename_desc,   (VOPFUNC)spec_rename },
	{ &vnop_mkdir_desc,    (VOPFUNC)spec_mkdir },
	{ &vnop_rmdir_desc,    (VOPFUNC)spec_rmdir },
	{ &vnop_symlink_desc,  (VOPFUNC)spec_symlink },
	{ &vnop_readdir_desc,  (VOPFUNC)spec_readdir },
	{ &vnop_readlink_desc, (VOPFUNC)spec_readlink },
	{ &vnop_inactive_desc, (VOPFUNC)ext4_vnop_inactive },
	{ &vnop_reclaim_desc,  (VOPFUNC)ext4_vnop_reclaim },
	{ &vnop_strategy_desc, (VOPFUNC)spec_strategy },
	{ &vnop_pathconf_desc, (VOPFUNC)spec_pathconf },
	{ &vnop_advlock_desc,  (VOPFUNC)err_advlock },
	{ NULL, NULL }
};

struct vnodeopv_desc ext4_specop_opv_desc = {
	&ext4_specop_p, ext4_specop_entries
};
