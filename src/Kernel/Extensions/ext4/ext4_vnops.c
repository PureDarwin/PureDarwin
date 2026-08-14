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
#include <sys/fsctl.h>
#include <sys/disk.h>
#include <string.h>
#include <vm/vm_kern.h>
#include <IOKit/IOLocks.h>

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
	if (ep->e_lock == NULL) {
		IOLockUnlock(hlock);
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

	memset(&vfsp, 0, sizeof(vfsp));
	vfsp.vnfs_mp        = emp->em_mp;
	vfsp.vnfs_vtype     = vtype;
	vfsp.vnfs_str       = "ext4";
	vfsp.vnfs_dvp       = dvp;
	vfsp.vnfs_fsnode    = ep;
	vfsp.vnfs_vops      = ext4_vnodeop_p;
	vfsp.vnfs_markroot  = (ino == EXT4_ROOT_INO);
	vfsp.vnfs_marksystem = 0;
	vfsp.vnfs_rdev      = 0;
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

static int
ext4_vnop_getattr_impl(struct vnop_getattr_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct vnode_attr *vap = ap->a_vap;
	struct ext4_inode *ri = &ep->e_raw;
	struct ext4mount *emp = ep->e_mount;

	VATTR_RETURN(vap, va_rdev, 0);
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
	VATTR_RETURN(vap, va_flags, 0);
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

/* Copy [off, off+len) of the file into a kernel buffer `dst` (len<=bs). */
static int
ext4_read_range(struct ext4node *ep, uint64_t foff, void *dst, size_t len)
{
	struct ext4mount *emp = ep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint32_t lblk = (uint32_t)(foff / bs);
	uint32_t boff = (uint32_t)(foff % bs);
	uint64_t pblk = 0;
	buf_t bp = NULL;
	int error;
	struct ext4_inode raw_snapshot;

	if (boff + len > bs)
		len = bs - boff;

	/*
	 * Snapshot e_raw under the node lock before walking the extent tree:
	 * ext4_vget()'s cache-hit path can refresh e_raw from a concurrent
	 * thread (e.g. another exec of the same hot file) at any time, and
	 * ext4_bmap() otherwise reads it live, field-by-field, with no
	 * synchronization - a torn read of the extent header/entries here
	 * produced exactly this bug's symptoms (intermittent garbage reads of
	 * /usr/bin/xkbcomp depending on timing).
	 */
	IOLockLock((IOLock *)ep->e_lock);
	raw_snapshot = ep->e_raw;
	IOLockUnlock((IOLock *)ep->e_lock);

	error = ext4_bmap(emp, ep->e_ino, &raw_snapshot, lblk, &pblk);
	if (error)
		return error;
	if (pblk == 0) {
		memset(dst, 0, len);   /* hole */
		return 0;
	}
	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;
	memcpy(dst, (char *)buf_dataptr(bp) + boff, len);
	buf_brelse(bp);
	return 0;
}

static int
ext4_vnop_read_impl(struct vnop_read_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct ext4node *ep = VTOE(vp);
	uint32_t bs = ep->e_mount->em_blocksize;
	char *buf;
	size_t bufsz;
	uint64_t file_size;
	int error = 0;

	if (vnode_isdir(vp))
		return EISDIR;
	if (uio_offset(uio) < 0)
		return EINVAL;

	/* read(2) goes straight to the block layer below; any dirty mmap'd
	 * pages of this range must be pushed first or the read returns stale
	 * disk contents. Skip entirely when no pages are resident (the
	 * common case for plain open/read consumers). */
	if (vnode_isreg(vp) && ubc_pages_resident(vp))
		(void)ubc_msync(vp, uio_offset(uio),
		    uio_offset(uio) + uio_resid(uio), NULL,
		    UBC_PUSHDIRTY | UBC_SYNC);

	bufsz = (size_t)bs * EXT4_READ_BATCH_BLOCKS;
	buf = (char *)_MALLOC(bufsz, M_TEMP, M_WAITOK);
	if (buf == NULL)
		return ENOMEM;

	/* Snapshot e_size under the node lock, as ext4_read_range does: the
	 * fs lock is no longer held for the whole loop, so ext4_vget()'s
	 * cache-hit refresh can rewrite it underneath us. */
	IOLockLock((IOLock *)ep->e_lock);
	file_size = ep->e_size;
	IOLockUnlock((IOLock *)ep->e_lock);

	while (uio_resid(uio) > 0) {
		off_t foff = uio_offset(uio);
		size_t chunk, filled = 0;

		if (foff >= (off_t)file_size)
			break;
		/* Clamp so the batch spans at most EXT4_READ_BATCH_BLOCKS
		 * blocks: an unaligned start would otherwise reach one block
		 * past the mapping array resolved below. */
		chunk = bufsz - (size_t)(foff % bs);
		if (chunk > (size_t)uio_resid(uio))
			chunk = (size_t)uio_resid(uio);
		if (foff + (off_t)chunk > (off_t)file_size)
			chunk = (size_t)(file_size - foff);

		/*
		 * Gather a batch of blocks per acquisition rather than one.
		 * em_fs_lock covers the whole mount, so a block at a time meant
		 * every 4K of every read serialised against every other vnop -
		 * loading Wine's DLLs across dozens of processes starved the
		 * loader lock for a minute at a time. Copying out still happens
		 * unlocked below.
		 *
		 * The I/O deliberately stays inside the lock. Resolving the
		 * batch's mapping under it and reading outside was measurably
		 * faster but returned wrong bytes (three corpus files, and a
		 * guest that could no longer complete an ssh handshake): the
		 * mapping can change under a reader that no longer holds it.
		 */
		ext4_fs_lock(ep->e_mount);
		while (filled < chunk) {
			off_t at = foff + (off_t)filled;
			size_t part = bs - (size_t)(at % bs);

			if (part > chunk - filled)
				part = chunk - filled;
			error = ext4_read_range(ep, (uint64_t)at, buf + filled,
			    part);
			if (error)
				break;
			filled += part;
		}
		ext4_fs_unlock_nocommit(ep->e_mount);

		/*
		 * uiomove runs unlocked on purpose. The destination is a user
		 * buffer that can itself be a page of a file on this mount, so
		 * the copy can fault; holding em_fs_lock across it deadlocks
		 * the whole mount against the VNOP_PAGEIN servicing that fault.
		 */
		if (filled > 0) {
			int uerr = uiomove(buf, (int)filled, uio);
			if (uerr) {
				error = uerr;
				break;
			}
		}
		if (error)
			break;
	}

	_FREE(buf, M_TEMP);
	return error;
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
		ep->e_raw.i_dtime = ep->e_raw.i_ctime;
		error = ext4_write_inode(ep->e_mount, ep->e_ino, &ep->e_raw);
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

	error = ext4_bmap(emp, ep->e_ino, &ep->e_raw, lblk, &pblk);
	if (error)
		return error;
	if (pblk != 0) {
		*pblk_out = pblk;
		if (allocated != NULL)
			*allocated = false;
		return 0;
	}

	error = ext4_alloc_block(emp, 0, &pblk);
	if (error)
		return error;
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

/* Write [off, off+len) of the file from a kernel buffer `src` (len<=bs),
 * allocating the block if needed. Mirrors ext4_read_range for pagein.
 * `allocated` (may be NULL) reports whether a new block was mapped, so the
 * caller knows the extent tree changed and the inode has to be written out. */
static int
ext4_write_range(struct ext4node *ep, uint64_t foff, const void *src, size_t len,
    bool *allocated)
{
	struct ext4mount *emp = ep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint32_t boff = (uint32_t)(foff % bs);
	uint64_t pblk = 0;
	buf_t bp = NULL;
	int error;

	if (boff + len > bs)
		len = bs - boff;

	error = ext4_ensure_block_alloc(ep, (uint32_t)(foff / bs), &pblk, allocated);
	if (error)
		return error;
	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;
	memcpy((char *)buf_dataptr(bp) + boff, src, len);
	return buf_bawrite(bp);   /* async: file data, not journaled metadata */
}

static int
ext4_vnop_write_impl(struct vnop_write_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct ext4node *ep = VTOE(vp);
	struct ext4mount *emp = ep->e_mount;
	uint32_t bs = emp->em_blocksize;
	int error = 0;
	bool dirty = false;
	off_t write_start;
	char *bounce;

	if (vnode_isdir(vp))
		return EISDIR;

	ext4_fs_lock(emp);
	if (ap->a_ioflag & IO_APPEND)
		uio_setoffset(uio, (off_t)ep->e_size);
	ext4_fs_unlock_nocommit(emp);

	if (uio_offset(uio) < 0)
		return EINVAL;
	write_start = uio_offset(uio);

	bounce = (char *)_MALLOC(bs, M_TEMP, M_WAITOK);
	if (bounce == NULL)
		return ENOMEM;

	while (uio_resid(uio) > 0) {
		off_t foff = uio_offset(uio);
		uint32_t lblk = (uint32_t)(foff / bs);
		uint32_t boff = (uint32_t)(foff % bs);
		size_t want = bs - boff;
		uint64_t pblk = 0;
		buf_t bp = NULL;
		bool fresh = false;

		if (want > (size_t)uio_resid(uio))
			want = (size_t)uio_resid(uio);

		/*
		 * Copy out of the user buffer before taking anything. The source
		 * can be a page of a file on this same mount, so the copy can
		 * fault: doing it while holding em_fs_lock deadlocks the mount
		 * against the VNOP_PAGEIN servicing that fault, and doing it
		 * while holding a busy buf_t pins a device buffer across an
		 * unbounded wait. uiomove has already consumed these bytes if it
		 * succeeds - a later failure therefore surfaces as a short
		 * write, which is what vn_write() turns a partial transfer into
		 * anyway.
		 */
		error = uiomove(bounce, (int)want, uio);
		if (error)
			break;

		ext4_fs_lock(emp);
		error = ext4_ensure_block_alloc(ep, lblk, &pblk, &fresh);
		if (error == 0)
			error = ext4_blkread(emp, pblk, &bp);  /* read-modify-write */
		if (error == 0) {
			if (fresh)
				memset((char *)buf_dataptr(bp), 0, bs);
			memcpy((char *)buf_dataptr(bp) + boff, bounce, want);
			error = buf_bawrite(bp);   /* async: data block; releases bp */
		}
		if (error == 0) {
			if (uio_offset(uio) > (off_t)ep->e_size)
				ep->e_size = (uint64_t)uio_offset(uio);
			dirty = true;
		}
		ext4_fs_unlock(emp);
		if (error)
			break;
	}

	_FREE(bounce, M_TEMP);

	if (dirty) {
		struct timeval tv;

		microtime(&tv);
		ext4_fs_lock(emp);
		ep->e_raw.i_size_lo = le32((uint32_t)ep->e_size);
		ep->e_raw.i_size_high = le32((uint32_t)(ep->e_size >> 32));
		ep->e_raw.i_ctime = le32((uint32_t)tv.tv_sec);
		ep->e_raw.i_mtime = le32((uint32_t)tv.tv_sec);
		(void)ext4_write_inode(emp, ep->e_ino, &ep->e_raw);
		ext4_fs_unlock(emp);

		/* Both of these can re-enter the filesystem through the UBC, so
		 * they run outside the lock. write(2) went through the block
		 * layer below the UBC; any already-resident pages of this range
		 * are now stale and would be served to a later mmap/exec as-is. */
		ubc_setsize(vp, ep->e_size);
		(void)ubc_msync(vp, write_start, uio_offset(uio), NULL,
		    UBC_INVALIDATE);
	}
	return error;
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
		buf_t bp = NULL;

		if (chunk > end - off)
			chunk = (size_t)(end - off);

		error = ext4_ensure_block(ep, lblk, &pblk);
		if (error)
			return error;
		error = ext4_blkread(emp, pblk, &bp);
		if (error)
			return error;
		memset((char *)buf_dataptr(bp) + boff, 0, chunk);
		error = buf_bawrite(bp);
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
			buf_t bp = NULL;
			uint32_t tail = (uint32_t)(new_size % bs);

			error = ext4_bmap(emp, ep->e_ino, &ep->e_raw, (uint32_t)(new_blocks - 1), &pblk);
			if (error)
				return error;
			if (pblk != 0) {
				error = ext4_blkread(emp, pblk, &bp);
				if (error)
					return error;
				memset((char *)buf_dataptr(bp) + tail, 0, bs - tail);
				error = buf_bawrite(bp);
				if (error)
					return error;
			}
		}
		error = ext4_inode_truncate_extents(emp, &ep->e_raw, new_blocks);
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
	if (vtype != VREG && vtype != VSOCK)
		return ENOTSUP;
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
		uint16_t fmt = (vtype == VSOCK) ? EXT4_S_IFSOCK : EXT4_S_IFREG;
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
	if (size < 60 && le32(ep->e_raw.i_blocks_lo) == 0) {
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
	vnode_t vp = ap->a_vp;
	struct ext4node *ep = VTOE(vp);
	upl_t pl = ap->a_pl;
	vm_offset_t ioaddr = 0;
	off_t f_offset = ap->a_f_offset;
	size_t size = ap->a_size;
	kern_return_t kr;
	int error = 0;
	size_t done;
	uint64_t file_size;

	/* Snapshot e_size once, under the lock - see ext4_read_range for why
	 * (ext4_vget()'s cache-hit refresh can rewrite it concurrently). */
	IOLockLock((IOLock *)ep->e_lock);
	file_size = ep->e_size;
	IOLockUnlock((IOLock *)ep->e_lock);

	kr = ubc_upl_map(pl, &ioaddr);
	if (kr != KERN_SUCCESS)
		return EIO;
	ioaddr += ap->a_pl_offset;

	for (done = 0; done < size; ) {
		char *dst = (char *)ioaddr + done;
		off_t foff = f_offset + done;
		size_t chunk = ep->e_mount->em_blocksize;

		chunk -= (foff % ep->e_mount->em_blocksize);
		if (chunk > size - done)
			chunk = size - done;

		if (foff >= (off_t)file_size) {
			memset(dst, 0, size - done);   /* beyond EOF */
			break;
		}
		if (foff + (off_t)chunk > (off_t)file_size)
			chunk = (size_t)(file_size - foff);

		error = ext4_read_range(ep, foff, dst, chunk);
		if (error)
			break;
		done += chunk;
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

/*
 * No VNOP_STRATEGY is implemented for ext4 (block numbers returned by
 * ext4_vnop_blockmap are extent-tree physical blocks, not something a
 * generic buf_strategy/device dispatch can consume), so routing pageout
 * through cluster_pageout() (which relies on VNOP_STRATEGY doing the actual
 * I/O) silently no-ops or misdirects dirty-page writeback. Do direct block
 * I/O here instead, mirroring how ext4_vnop_pagein bypasses cluster_io.
 */
static int
ext4_vnop_pageout_impl(struct vnop_pageout_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct ext4node *ep = VTOE(vp);
	struct ext4mount *emp = ep->e_mount;
	upl_t pl = ap->a_pl;
	vm_offset_t ioaddr = 0;
	off_t f_offset = ap->a_f_offset;
	size_t size = ap->a_size;
	kern_return_t kr;
	int error = 0;
	size_t done;
	bool grew = false;

	kr = ubc_upl_map(pl, &ioaddr);
	if (kr != KERN_SUCCESS)
		return EIO;
	ioaddr += ap->a_pl_offset;

	for (done = 0; done < size; ) {
		char *src = (char *)ioaddr + done;
		off_t foff = f_offset + done;
		size_t chunk = emp->em_blocksize;

		chunk -= (foff % emp->em_blocksize);
		if (chunk > size - done)
			chunk = size - done;

		if (foff >= (off_t)ep->e_size) {
			done += chunk;
			continue;   /* beyond EOF: nothing to write back */
		}
		if (foff + (off_t)chunk > (off_t)ep->e_size)
			chunk = (size_t)(ep->e_size - foff);

		bool allocated = false;

		error = ext4_write_range(ep, foff, src, chunk, &allocated);
		if (error)
			break;
		if (allocated)
			grew = true;
		done += chunk;
	}

	ubc_upl_unmap(pl);

	/*
	 * Paging out into a hole allocates blocks and appends extents, but that
	 * only touched the in-core inode. Without writing it back the extent
	 * tree is lost on unmount, and the file reads back as zeroes at its
	 * full length - which is what a file extended by ftruncate(2) and then
	 * filled through mmap(2) did. Data written by write(2) survived only
	 * because it had already persisted the inode itself.
	 */
	if (grew && !error)
		error = ext4_write_inode(emp, ep->e_ino, &ep->e_raw);

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

static int
ext4_vnop_blockmap_impl(struct vnop_blockmap_args *ap)
{
	struct ext4node *ep = VTOE(ap->a_vp);
	struct ext4mount *emp = ep->e_mount;
	uint32_t bs = emp->em_blocksize;
	uint32_t lblk;
	uint32_t boff;
	uint64_t pblk = 0;
	uint64_t next_pblk;
	size_t run;
	int error;

	if (vnode_isdir(ap->a_vp))
		return ENOTSUP;
	if (ap->a_bpn == NULL)
		return 0;
	if (ap->a_foffset < 0)
		return EINVAL;

	lblk = (uint32_t)(ap->a_foffset / bs);
	boff = (uint32_t)(ap->a_foffset % bs);
	error = ext4_bmap(emp, ep->e_ino, &ep->e_raw, lblk, &pblk);
	if (error)
		return error;
	if (pblk == 0) {
		if (ap->a_flags & VNODE_WRITE)
			return EINVAL;
		*ap->a_bpn = (daddr64_t)-1;
		if (ap->a_run)
			*ap->a_run = bs - boff;
		return 0;
	}

	run = bs - boff;
	while (run < ap->a_size) {
		uint32_t next_lblk = lblk + 1 + (uint32_t)((run - (bs - boff)) / bs);
		uint64_t expect = pblk + 1 + ((run - (bs - boff)) / bs);

		error = ext4_bmap(emp, ep->e_ino, &ep->e_raw, next_lblk, &next_pblk);
		if (error || next_pblk != expect)
			break;
		run += bs;
	}
	*ap->a_bpn = (daddr64_t)pblk;
	if (ap->a_run)
		*ap->a_run = run;
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

	case FSIOC_KERNEL_ROOTAUTH:
		/*
		 * Apple Silicon macOS boots a Signed System Volume and bsd_init()
		 * panics if the root file system cannot vouch for its own seal.
		 */
		return 0;

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
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_setattr_impl(ap);
	ext4_fs_unlock(emp);
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
	int error;

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
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	emp->em_stats.pageins++;
	error = ext4_vnop_pagein_impl(ap);
	ext4_fs_unlock(emp);
	return error;
}

static int
ext4_vnop_pageout(struct vnop_pageout_args *ap)
{
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	emp->em_stats.pageouts++;
	error = ext4_vnop_pageout_impl(ap);
	ext4_fs_unlock(emp);
	return error;
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
	struct ext4mount *emp = VTOE(ap->a_vp)->e_mount;
	int error;

	ext4_fs_lock(emp);
	error = ext4_vnop_inactive_impl(ap);
	ext4_fs_unlock(emp);
	return error;
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
	{ &vnop_rmdir_desc,    (VOPFUNC)ext4_vnop_rmdir },
	{ &vnop_inactive_desc, (VOPFUNC)ext4_vnop_inactive },
	{ &vnop_fsync_desc,    (VOPFUNC)ext4_vnop_fsync },
	{ &vnop_ioctl_desc,    (VOPFUNC)ext4_vnop_ioctl },
	{ &vnop_reclaim_desc,  (VOPFUNC)ext4_vnop_reclaim },
	{ &vnop_pathconf_desc, (VOPFUNC)ext4_vnop_pathconf },
	{ NULL, NULL }
};

struct vnodeopv_desc ext4_vnodeop_opv_desc = {
	&ext4_vnodeop_p, ext4_vnodeop_entries
};
