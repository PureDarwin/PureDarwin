/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfs.h"
#include "apfsrw/apfsrw.h"

#include <IOKit/IOLocks.h>

#include <libkern/libkern.h>
#include <libkern/OSByteOrder.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/vnode_if.h>
#include <string.h>

extern errno_t VNOP_OPEN(vnode_t vp, int mode, vfs_context_t ctx);
extern errno_t VNOP_CLOSE(vnode_t vp, int fflag, vfs_context_t ctx);

static vfstable_t apfs_vfsconf;

struct apfs_mount_args {
	char *fspec;
};

static int apfs_mount(struct mount *mp, vnode_t devvp, user_addr_t data,
    vfs_context_t ctx);
static int apfs_start(struct mount *mp, int flags, vfs_context_t ctx);
static int apfs_unmount(struct mount *mp, int mntflags, vfs_context_t ctx);
static int apfs_root(struct mount *mp, vnode_t *vpp, vfs_context_t ctx);
static int apfs_getattr(struct mount *mp, struct vfs_attr *fsap,
    vfs_context_t ctx);
static int apfs_sync(struct mount *mp, int waitfor, vfs_context_t ctx);
static int apfs_vfs_vget(struct mount *mp, ino64_t ino, vnode_t *vpp,
    vfs_context_t ctx);
static int apfs_init(struct vfsconf *vfsp);
static int apfs_open_fspec(user_addr_t data, vnode_t *devvpp,
    vfs_context_t ctx);
static int apfs_probe_container(struct apfs_mount *amp, vfs_context_t ctx);

static struct vfsops apfs_vfsops = {
	.vfs_mount = apfs_mount,
	.vfs_start = apfs_start,
	.vfs_unmount = apfs_unmount,
	.vfs_root = apfs_root,
	.vfs_getattr = apfs_getattr,
	.vfs_sync = apfs_sync,
	.vfs_vget = apfs_vfs_vget,
	.vfs_init = apfs_init,
};

static int
apfs_init(__unused struct vfsconf *vfsp)
{
	return 0;
}

static int
apfs_mount(__unused struct mount *mp, vnode_t devvp, user_addr_t data,
    vfs_context_t ctx)
{
	struct apfs_mount *amp;
	struct vfsstatfs *sfs;
	int error;
	int own_devvp_ref = 0;

	if (vfs_isupdate(mp))
		return 0;

	if (devvp == NULLVP) {
		error = apfs_open_fspec(data, &devvp, ctx);
		if (error)
			return error;
		own_devvp_ref = 1;
	}

	amp = (struct apfs_mount *)_MALLOC(sizeof(*amp), M_TEMP, M_WAITOK | M_ZERO);
	if (amp == NULL) {
		if (own_devvp_ref)
			vnode_rele(devvp);
		return ENOMEM;
	}

	amp->mp = mp;
	amp->am_hash_lock = IOLockAlloc();
	amp->am_rw_lock = IOLockAlloc();
	if (amp->am_hash_lock == NULL || amp->am_rw_lock == NULL) {
		if (amp->am_hash_lock)
			IOLockFree(amp->am_hash_lock);
		if (amp->am_rw_lock)
			IOLockFree(amp->am_rw_lock);
		_FREE(amp, M_TEMP);
		if (own_devvp_ref)
			vnode_rele(devvp);
		return ENOMEM;
	}
	{
		int i;

		for (i = 0; i < APFS_NODE_HASH_SIZE; i++)
			LIST_INIT(&amp->am_node_hash[i]);
	}
	amp->devvp = devvp;
	amp->dev = vnode_specrdev(devvp);
	APFSLOG("mount requested devvp=%p", devvp);
	error = VNOP_OPEN(devvp, FREAD | FWRITE, ctx);
	if (error) {
		APFSLOG("device open for write failed: %d", error);
		goto fail;
	}
	amp->dev_opened = 1;

	error = apfs_probe_container(amp, ctx);
	if (error)
		goto fail;
	error = apfs_load_volume(amp, ctx);
	if (error)
		goto fail;

	/*
	 * Bind the shared read/write implementation to the same device. The kext
	 * keeps its own cached view for reads, so anything that writes through
	 * amp->rw has to re-probe the container afterwards - see
	 * apfs_reload_container().
	 */
	error = apfsrw_open_kernel(devvp, amp->block_count, 1, 0, &amp->rw);
	if (error != 0) {
		APFSLOG("apfsrw_open_kernel failed: %d", error);
		amp->rw = NULL;
	}

	if (!own_devvp_ref) {
		error = vnode_ref(devvp);
		if (error)
			goto fail;
	}

	error = apfs_vget(amp, APFS_ROOT_FILEID, NULLVP, &amp->root_vp);
	if (error) {
		if (!own_devvp_ref)
			vnode_rele(devvp);
		goto fail;
	}
	vnode_put(amp->root_vp);

	vfs_setfsprivate(mp, amp);
	vfs_setflags(mp, MNT_LOCAL);
	/* Advisory locks are handled in the VFS (lf_advlock). */
	vfs_setlocklocal(mp);
	vfs_clearflags(mp, MNT_RDONLY);

	sfs = vfs_statfs(mp);
	sfs->f_bsize = amp->block_size;
	sfs->f_iosize = amp->block_size;
	sfs->f_blocks = amp->block_count;
	sfs->f_bfree = 0;
	sfs->f_bavail = 0;
	sfs->f_files = OSSwapLittleToHostInt64(amp->apfs.apfs_num_files) +
	    OSSwapLittleToHostInt64(amp->apfs.apfs_num_directories) +
	    OSSwapLittleToHostInt64(amp->apfs.apfs_num_symlinks) +
	    OSSwapLittleToHostInt64(amp->apfs.apfs_num_other_fsobjects);
	sfs->f_ffree = 0;
	sfs->f_fsid.val[0] = (int32_t)amp->dev;
	sfs->f_fsid.val[1] = (int32_t)vfs_typenum(mp);
	strlcpy(sfs->f_fstypename, APFS_MODULE_NAME, sizeof(sfs->f_fstypename));

	APFSLOG("mounted scaffold read-write");
	return 0;

fail:
	if (amp && amp->dev_opened) {
		(void)VNOP_CLOSE(devvp, FREAD | FWRITE, ctx);
		amp->dev_opened = 0;
	}
	_FREE(amp, M_TEMP);
	if (own_devvp_ref)
		vnode_rele(devvp);
	return error;
}

static int
apfs_parse_disk_minor(const char *path, uint32_t *minor_out)
{
	const char *p = path;
	uint32_t disk = 0;
	uint32_t slice = 0;

	if (strncmp(p, "/dev/disk", 9) != 0)
		return EINVAL;

	p += 9;
	if (*p < '0' || *p > '9')
		return EINVAL;

	while (*p >= '0' && *p <= '9') {
		disk = disk * 10 + (uint32_t)(*p - '0');
		p++;
	}

	if (disk != 0)
		return ENOTSUP;

	if (*p == '\0') {
		*minor_out = 0;
		return 0;
	}

	if (*p != 's')
		return EINVAL;
	p++;
	if (*p < '0' || *p > '9')
		return EINVAL;

	while (*p >= '0' && *p <= '9') {
		slice = slice * 10 + (uint32_t)(*p - '0');
		p++;
	}

	if (*p != '\0')
		return EINVAL;

	*minor_out = slice;
	return 0;
}

static int
apfs_open_fspec(user_addr_t data, vnode_t *devvpp, vfs_context_t ctx)
{
	struct apfs_mount_args args;
	char fspec[MAXPATHLEN];
	size_t fspec_len;
	uint32_t minor_id;
	dev_t dev;
	int error;

	if (data == USER_ADDR_NULL)
		return EINVAL;

	error = copyin(data, &args, sizeof(args));
	if (error)
		return error;
	error = copyinstr((user_addr_t)args.fspec, fspec, sizeof(fspec), &fspec_len);
	if (error)
		return error;

	/* Look the path up to get the real dev_t for any device node. */
	{
		vnode_t dvp = NULLVP;

		error = vnode_lookup(fspec, 0, &dvp, ctx);
		if (error == 0) {
			if (vnode_vtype(dvp) != VBLK) {
				APFSLOG("fspec '%s' is not a block device", fspec);
				vnode_put(dvp);
				return ENOTBLK;
			}
			dev = vnode_specrdev(dvp);
			vnode_put(dvp);
			APFSLOG("fspec '%s' -> dev=0x%x (lookup)", fspec, dev);
			return bdevvp(dev, devvpp);
		}
		APFSLOG("vnode_lookup('%s') failed: %d, falling back", fspec,
		    error);
	}

	error = apfs_parse_disk_minor(fspec, &minor_id);
	if (error) {
		APFSLOG("unsupported fspec '%s': %d", fspec, error);
		return error;
	}
	if (rootdev == NODEV) {
		APFSLOG("cannot derive disk major before rootdev is set");
		return ENODEV;
	}

	dev = makedev(major(rootdev), minor_id);
	APFSLOG("fspec '%s' -> dev=0x%x (derived)", fspec, dev);
	return bdevvp(dev, devvpp);
}

int
apfs_reload_container(struct apfs_mount *amp, vfs_context_t ctx)
{
	int error;

	if (amp == NULL)
		return EINVAL;
	/* No buffer invalidation: libapfsrw shares this kext's buffer cache,
	 * and dropping it would discard the commit's delayed writes. */
	error = apfs_probe_container(amp, ctx);
	if (error)
		return error;
	return apfs_load_volume(amp, ctx);
}

static uint64_t
apfs_le64(uint64_t v)
{
	return OSSwapLittleToHostInt64(v);
}

static uint32_t
apfs_le32(uint32_t v)
{
	return OSSwapLittleToHostInt32(v);
}

static int64_t
apfs_le64s(int64_t v)
{
	return (int64_t)OSSwapLittleToHostInt64((uint64_t)v);
}

static uint64_t
apfs_fletcher64(const void *data, size_t size)
{
	const uint8_t *bytes = (const uint8_t *)data;
	uint64_t lo = 0;
	uint64_t hi = 0;
	uint64_t check1;
	uint64_t check2;
	size_t offset;

	if (data == NULL || size <= APFS_MAX_CKSUM_SIZE)
		return 0;

	for (offset = APFS_MAX_CKSUM_SIZE; offset + sizeof(uint32_t) <= size;
	    offset += sizeof(uint32_t)) {
		uint32_t word;

		memcpy(&word, bytes + offset, sizeof(word));
		lo = (lo + apfs_le32(word)) % 0xffffffffULL;
		hi = (hi + lo) % 0xffffffffULL;
	}

	check1 = 0xffffffffULL - ((lo + hi) % 0xffffffffULL);
	check2 = 0xffffffffULL - ((lo + check1) % 0xffffffffULL);
	return (check2 << 32) | check1;
}

static int
apfs_verify_object_checksum(const void *object, size_t size)
{
	const struct apfs_obj_phys *obj = (const struct apfs_obj_phys *)object;
	uint64_t actual;
	uint64_t expected;

	if (object == NULL || size <= sizeof(*obj))
		return EINVAL;

	memcpy(&expected, obj->o_cksum, sizeof(expected));
	actual = apfs_fletcher64(object, size);
	if (apfs_le64(expected) != actual)
		return EINVAL;
	return 0;
}

static uint32_t
apfs_object_type(uint32_t type)
{
	return apfs_le32(type) & APFS_OBJECT_TYPE_MASK;
}

static int
apfs_read_probe_block(vnode_t devvp, apfs_paddr_t paddr, uint32_t block_size,
    kauth_cred_t cred, void *out)
{
	buf_t bp = NULL;
	int error;

	if (devvp == NULLVP || paddr < 0 || block_size == 0 || out == NULL)
		return EINVAL;

	error = (int)buf_meta_bread(devvp, (daddr64_t)paddr, block_size, cred,
	    &bp);
	if (error) {
		if (bp)
			buf_brelse(bp);
		return error;
	}
	memcpy(out, (const void *)buf_dataptr(bp), block_size);
	buf_brelse(bp);
	return 0;
}

static int
apfs_select_checkpoint_nx(vnode_t devvp, struct apfs_nx_superblock *nx,
    uint32_t block_size, vfs_context_t ctx)
{
	void *block;
	uint32_t desc_blocks;
	apfs_paddr_t desc_base;
	apfs_xid_t best_xid;
	apfs_paddr_t best_paddr = 0;
	uint32_t i;
	int error = 0;

	desc_blocks = apfs_le32(nx->nx_xp_desc_blocks) &
	    APFS_CHECKPOINT_BLOCK_COUNT_MASK;
	desc_base = apfs_le64s(nx->nx_xp_desc_base);
	if (desc_blocks == 0 || desc_base < 0)
		return 0;

	block = _MALLOC(block_size, M_TEMP, M_WAITOK);
	if (block == NULL)
		return ENOMEM;

	best_xid = apfs_le64(nx->nx_o.o_xid);
	for (i = 0; i < desc_blocks; i++) {
		struct apfs_nx_superblock *candidate;
		apfs_paddr_t paddr = desc_base + i;

		error = apfs_read_probe_block(devvp, paddr, block_size,
		    vfs_context_ucred(ctx), block);
		if (error) {
			error = 0;
			continue;
		}
		if (apfs_verify_object_checksum(block, block_size))
			continue;

		candidate = (struct apfs_nx_superblock *)block;
		if (apfs_object_type(candidate->nx_o.o_type) !=
		    APFS_OBJECT_TYPE_NX_SUPERBLOCK ||
		    apfs_le32(candidate->nx_magic) != APFS_NX_MAGIC)
			continue;
		if (apfs_le32(candidate->nx_block_size) != block_size)
			continue;
		if (apfs_le64(candidate->nx_o.o_xid) < best_xid)
			continue;

		best_xid = apfs_le64(candidate->nx_o.o_xid);
		best_paddr = paddr;
		memcpy(nx, candidate, sizeof(*nx));
	}


	_FREE(block, M_TEMP);
	return 0;
}

static void
apfs_log_uuid(const uint8_t uuid[16])
{
	APFSLOG("  uuid=%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    uuid[0], uuid[1], uuid[2], uuid[3],
	    uuid[4], uuid[5], uuid[6], uuid[7],
	    uuid[8], uuid[9], uuid[10], uuid[11],
	    uuid[12], uuid[13], uuid[14], uuid[15]);
}

static void
apfs_log_volume_oids(const struct apfs_nx_superblock *nx, uint32_t max_fs)
{
	uint32_t shown = 0;
	uint32_t i;

	if (max_fs > APFS_NX_MAX_FILE_SYSTEMS)
		max_fs = APFS_NX_MAX_FILE_SYSTEMS;

	for (i = 0; i < max_fs; i++) {
		uint64_t oid = apfs_le64(nx->nx_fs_oid[i]);

		if (oid == 0)
			continue;
		APFSLOG("  fs_oid[%u]=0x%llx", i, (unsigned long long)oid);
		if (++shown == 8)
			break;
	}
}

static int
apfs_probe_container(struct apfs_mount *amp, vfs_context_t ctx)
{
	struct apfs_nx_superblock nx;
	struct apfs_nx_superblock *disk_nx;
	buf_t bp = NULL;
	uint32_t block_size;
	uint32_t max_fs;
	vnode_t devvp = amp->devvp;
	int error;

	if (devvp == NULLVP) {
		APFSLOG("mount probe has no device vnode");
		return EINVAL;
	}

	error = (int)buf_meta_bread(devvp, 0, APFS_BS_BYTES,
	    vfs_context_ucred(ctx), &bp);
	if (error) {
		if (bp)
			buf_brelse(bp);
		APFSLOG("block-zero read failed: %d", error);
		return error;
	}

	disk_nx = (struct apfs_nx_superblock *)buf_dataptr(bp);
	memcpy(&nx, disk_nx, sizeof(nx));
	buf_brelse(bp);

	if (apfs_le32(nx.nx_magic) != APFS_NX_MAGIC) {
		APFSLOG("bad container magic 0x%x", apfs_le32(nx.nx_magic));
		return EINVAL;
	}

	block_size = apfs_le32(nx.nx_block_size);
	if (block_size < APFS_BS_BYTES || (block_size & (block_size - 1)) != 0) {
		APFSLOG("invalid container block size %u", block_size);
		return EINVAL;
	}

	/* Keep future block reads aligned with the APFS container block size. */
	(void)VNOP_IOCTL(devvp, DKIOCSETBLOCKSIZE, (caddr_t)&block_size,
	    FWRITE, ctx);

	error = apfs_select_checkpoint_nx(devvp, &nx, block_size, ctx);
	if (error)
		return error;

	max_fs = apfs_le32(nx.nx_max_file_systems);
	/* Reloads after every commit would spam the console; dump once.
	 * apfs_load_volume() prints its own line and then sets the flag. */
	if (amp->am_probe_logged)
		goto adopt;
	APFSLOG("container dev=0x%x block_size=%u blocks=%llu",
	    amp->dev, block_size, (unsigned long long)apfs_le64(nx.nx_block_count));
	apfs_log_uuid(nx.nx_uuid);
	APFSLOG("  oid=0x%llx xid=%llu next_oid=0x%llx next_xid=%llu",
	    (unsigned long long)apfs_le64(nx.nx_o.o_oid),
	    (unsigned long long)apfs_le64(nx.nx_o.o_xid),
	    (unsigned long long)apfs_le64(nx.nx_next_oid),
	    (unsigned long long)apfs_le64(nx.nx_next_xid));
	APFSLOG("  features=0x%llx ro_compat=0x%llx incompat=0x%llx",
	    (unsigned long long)apfs_le64(nx.nx_features),
	    (unsigned long long)apfs_le64(nx.nx_readonly_compatible_features),
	    (unsigned long long)apfs_le64(nx.nx_incompatible_features));
	APFSLOG("  checkpoint desc blocks=%u base=%lld next=%u index=%u len=%u",
	    apfs_le32(nx.nx_xp_desc_blocks),
	    (long long)apfs_le64s(nx.nx_xp_desc_base),
	    apfs_le32(nx.nx_xp_desc_next),
	    apfs_le32(nx.nx_xp_desc_index),
	    apfs_le32(nx.nx_xp_desc_len));
	APFSLOG("  checkpoint data blocks=%u base=%lld next=%u index=%u len=%u",
	    apfs_le32(nx.nx_xp_data_blocks),
	    (long long)apfs_le64s(nx.nx_xp_data_base),
	    apfs_le32(nx.nx_xp_data_next),
	    apfs_le32(nx.nx_xp_data_index),
	    apfs_le32(nx.nx_xp_data_len));
	APFSLOG("  spaceman_oid=0x%llx omap_oid=0x%llx reaper_oid=0x%llx max_fs=%u",
	    (unsigned long long)apfs_le64(nx.nx_spaceman_oid),
	    (unsigned long long)apfs_le64(nx.nx_omap_oid),
	    (unsigned long long)apfs_le64(nx.nx_reaper_oid),
	    max_fs);
	apfs_log_volume_oids(&nx, max_fs);

adopt:
	amp->nx = nx;
	amp->block_size = block_size;
	amp->block_count = apfs_le64(nx.nx_block_count);
	amp->max_file_systems = max_fs;
	return 0;
}

static int
apfs_start(__unused struct mount *mp, __unused int flags,
    __unused vfs_context_t ctx)
{
	return 0;
}

static int
apfs_unmount(struct mount *mp, int mntflags, vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);
	int flags = 0;
	int error;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	error = vflush(mp, NULLVP, flags);
	if (error)
		return error;

	if (amp) {
		amp->root_vp = NULLVP;
		if (amp->devvp) {
			/* Land every delayed write before the device closes;
			 * spec_close may discard whatever is still dirty. */
			buf_flushdirtyblks(amp->devvp, 1, 0, "apfs_unmount");
			if (amp->dev_opened) {
				(void)VNOP_CLOSE(amp->devvp, FREAD | FWRITE, ctx);
				amp->dev_opened = 0;
			}
			vnode_rele(amp->devvp);
			amp->devvp = NULLVP;
		}
		vfs_setfsprivate(mp, NULL);
		_FREE(amp, M_TEMP);
	}
	return 0;
}

static int
apfs_root(struct mount *mp, vnode_t *vpp,
    __unused vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);
	int error;

	if (amp == NULL || amp->root_vp == NULLVP)
		return EINVAL;

	error = vnode_getwithref(amp->root_vp);
	if (error)
		return error;
	*vpp = amp->root_vp;
	return 0;
}

static int
apfs_getattr(__unused struct mount *mp, struct vfs_attr *fsap,
    __unused vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);
	uint32_t block_size = amp ? amp->block_size : APFS_BS_BYTES;

	VFSATTR_RETURN(fsap, f_objcount, 1);
	VFSATTR_RETURN(fsap, f_maxobjcount, 1);
	VFSATTR_RETURN(fsap, f_bsize, block_size);
	VFSATTR_RETURN(fsap, f_iosize, block_size);
	if (amp) {
		VFSATTR_RETURN(fsap, f_blocks, amp->block_count);
		VFSATTR_RETURN(fsap, f_bfree, 0);
		VFSATTR_RETURN(fsap, f_bavail, 0);
		VFSATTR_RETURN(fsap, f_bused, amp->block_count);
	}
	VFSATTR_RETURN(fsap, f_files, 1);
	VFSATTR_RETURN(fsap, f_ffree, 0);
	return 0;
}

static int
apfs_sync(struct mount *mp, int waitfor, __unused vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);

	if (amp && amp->devvp)
		buf_flushdirtyblks(amp->devvp, waitfor == MNT_WAIT, 0,
		    "apfs_sync");
	return 0;
}

static int
apfs_vfs_vget(struct mount *mp, ino64_t ino, vnode_t *vpp,
    __unused vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);

	if (amp == NULL)
		return EINVAL;
	return apfs_vget(amp, ino, NULLVP, vpp);
}

int
apfs_vfs_register(void)
{
	struct vfs_fsentry vfe;
	struct vnodeopv_desc *opv[1];
	int error;

	if (apfs_vfsconf != NULL)
		return 0;

	memset(&vfe, 0, sizeof(vfe));
	opv[0] = &apfs_vnodeop_opv_desc;

	vfe.vfe_vfsops = &apfs_vfsops;
	vfe.vfe_vopcnt = 1;
	vfe.vfe_opvdescs = opv;
	strncpy(vfe.vfe_fsname, APFS_MODULE_NAME, sizeof(vfe.vfe_fsname));
	vfe.vfe_flags = VFS_TBLTHREADSAFE | VFS_TBLFSNODELOCK |
	    VFS_TBL64BITREADY | VFS_TBLNOTYPENUM |
	    VFS_TBLLOCALVOL | VFS_TBLGENERICMNTARGS |
	    VFS_TBLCANMOUNTROOT;

	error = vfs_fsadd(&vfe, &apfs_vfsconf);
	if (error) {
		APFSLOG("vfs_fsadd failed: %d", error);
		return error;
	}

	APFSLOG("registered apfs filesystem scaffold");
	return 0;
}

int
apfs_vfs_unregister(void)
{
	if (apfs_vfsconf) {
		if (vfs_fsremove(apfs_vfsconf) != 0)
			return -1;
		apfs_vfsconf = NULL;
	}
	return 0;
}
