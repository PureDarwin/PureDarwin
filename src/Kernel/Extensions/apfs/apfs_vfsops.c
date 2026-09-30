/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfs.h"
#include "apfs_kpi.h"
#include "apfsrw/apfsrw.h"

#include <IOKit/IOLocks.h>
#include <kern/locks.h>
#include <kern/thread.h>
#include <kern/thread_call.h>
#include <mach/mach_time.h>

#include <pexpert/pexpert.h>

#include <libkern/libkern.h>
#include <libkern/OSByteOrder.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vnode_if.h>
#include <string.h>

extern errno_t VNOP_OPEN(vnode_t vp, int mode, vfs_context_t ctx);
extern errno_t VNOP_CLOSE(vnode_t vp, int fflag, vfs_context_t ctx);

static vfstable_t apfs_vfsconf;
static lck_grp_t *apfs_lck_grp;

struct apfs_mount_args {
	char *fspec;
};

static int apfs_mount(struct mount *mp, vnode_t devvp, user_addr_t data,
    vfs_context_t ctx);
static int apfs_mount_dev(struct mount *mp, vnode_t devvp, int own_devvp_ref,
    vfs_context_t ctx);
static int apfs_vfs_ioctl(struct mount *mp, u_long command, caddr_t data,
    int flags, vfs_context_t ctx);
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
static int apfs_container_attach(struct apfs_mount *amp);
static void apfs_container_detach(struct apfs_mount *amp);
static void apfs_lock_enter_at(struct apfs_container *c, void *site);
// the site is who took the lock: the stat line charges each exclusive hold to it
#define apfs_lock_enter(c) apfs_lock_enter_at((c), __builtin_return_address(0))
static void apfs_lock_exit(struct apfs_container *c);
static void apfs_batch_timer_fire(thread_call_param_t p0, thread_call_param_t p1);

// batch limits: whichever comes first commits the container's open transaction
#define APFS_BATCH_MAX_OPS	1024
#define APFS_BATCH_MAX_BLOCKS	8192		// allocated plus superseded, 32 MB at 4K
#define APFS_BATCH_MAX_MS	1000
// one stat line per this many commits
#define APFS_BATCH_STAT_EVERY	256

static struct vfsops apfs_vfsops = {
	.vfs_mount = apfs_mount,
	.vfs_start = apfs_start,
	.vfs_unmount = apfs_unmount,
	.vfs_root = apfs_root,
	.vfs_getattr = apfs_getattr,
	.vfs_sync = apfs_sync,
	.vfs_vget = apfs_vfs_vget,
	.vfs_init = apfs_init,
	.vfs_ioctl = apfs_vfs_ioctl,
};

static int
apfs_init(__unused struct vfsconf *vfsp)
{
	return 0;
}

static int
apfs_mount(struct mount *mp, vnode_t devvp, user_addr_t data,
    vfs_context_t ctx)
{
	int error;

	if (vfs_isupdate(mp))
		return 0;
	if (devvp != NULLVP)
		return apfs_mount_dev(mp, devvp, 0, ctx);
	error = apfs_open_fspec(data, &devvp, ctx);
	if (error)
		return error;
	return apfs_mount_dev(mp, devvp, 1, ctx);
}

// own_devvp_ref: devvp came from bdevvp here, so its usecount is ours to drop on failure
static int
apfs_mount_dev(struct mount *mp, vnode_t devvp, int own_devvp_ref,
    vfs_context_t ctx)
{
	struct apfs_mount *amp;
	struct vfsstatfs *sfs;
	int error;

	amp = (struct apfs_mount *)_MALLOC(sizeof(*amp), M_TEMP, M_WAITOK | M_ZERO);
	if (amp == NULL) {
		if (own_devvp_ref)
			vnode_rele(devvp);
		return ENOMEM;
	}

	amp->mp = mp;
	amp->am_hash_lock = IOLockAlloc();
	amp->am_cache_lock = IOLockAlloc();
	if (amp->am_hash_lock == NULL || amp->am_cache_lock == NULL) {
		if (amp->am_hash_lock != NULL)
			IOLockFree(amp->am_hash_lock);
		if (amp->am_cache_lock != NULL)
			IOLockFree(amp->am_cache_lock);
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
	amp->io_devvp = devvp;
	amp->dev = vnode_specrdev(devvp);
	error = VNOP_OPEN(devvp, FREAD | FWRITE, ctx);
	if (error) {
		APFSLOG("device open for write failed: %d", error);
		goto fail;
	}
	amp->dev_opened = 1;

	// Every volume's device spans the whole container, so block zero says which container this is.
	// The registry says which volume
	error = apfs_probe_container(amp, ctx);
	if (error)
		goto fail;
	apfs_caches_alloc(amp);
	if (apfs_volume_slot_for_dev(amp->dev, &amp->vol_slot) != 0)
		amp->vol_slot = 0;
	// a System volume with a Data sibling, or that Data volume, makes up a volume group
	{
		uint16_t role = apfs_role_for_dev(amp->dev);

		amp->vol_group = role == APFS_VOL_ROLE_DATA ||
		    (role == APFS_VOL_ROLE_SYSTEM &&
		    apfs_role_dev(amp->dev, APFS_VOL_ROLE_DATA, NULL, NULL, 0) == 0);
	}
	error = apfs_container_attach(amp);
	if (error)
		goto fail;
	// the view is read under exclusive, so no commit lands halfway through it
	apfs_lock_enter(amp->cont);
	if (amp->io_devvp != devvp)
		error = apfs_probe_container(amp, ctx);
	if (error == 0)
		error = apfs_load_volume(amp, ctx);
	if (error == 0 && apfsrw_open_kernel(&amp->cont->c_rw_dev, amp->block_count, 1, 0,
	    amp->vol_slot, &amp->rw) != 0) {
		APFSLOG("apfsrw_open_kernel failed");
		amp->rw = NULL;
	}
	amp->seen_generation = amp->cont->c_generation;
	apfs_lock_exit(amp->cont);
	if (error)
		goto fail;
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
	// Advisory locks are handled in the VFS (lf_advlock)
	vfs_setlocklocal(mp);
	vfs_clearflags(mp, MNT_RDONLY);

	sfs = vfs_statfs(mp);
	sfs->f_bsize = amp->block_size;
	sfs->f_iosize = amp->block_size;
	sfs->f_blocks = amp->block_count;
	{
		struct apfsrw_space_info si;

		sfs->f_bfree = 0;
		if (amp->rw != NULL && apfsrw_get_space_info(amp->rw, &si) == 0 &&
		    si.free_count <= amp->block_count)
			sfs->f_bfree = si.free_count;
		sfs->f_bavail = sfs->f_bfree;
	}
	sfs->f_files = OSSwapLittleToHostInt64(amp->apfs.apfs_num_files) +
	    OSSwapLittleToHostInt64(amp->apfs.apfs_num_directories) +
	    OSSwapLittleToHostInt64(amp->apfs.apfs_num_symlinks) +
	    OSSwapLittleToHostInt64(amp->apfs.apfs_num_other_fsobjects);
	sfs->f_ffree = 1ull << 32;
	// User mounts arrive with f_mntfromname empty, so name the device node. Ask IOKit first,
	// libignition's Preboot devvp is nameless and apfs_boot_util wants /dev/diskNsM
	if (sfs->f_mntfromname[0] == '\0') {
		extern int apfs_bsd_name_for_dev(dev_t dev, char *buf, size_t len);
		char bsd[64];

		if (apfs_bsd_name_for_dev(amp->dev, bsd, sizeof(bsd)) == 0 && bsd[0] != '\0')
			snprintf(sfs->f_mntfromname, sizeof(sfs->f_mntfromname), "/dev/%s", bsd);
	}
	if (sfs->f_mntfromname[0] == '\0') {
		const char *dn = vnode_getname(devvp);

		if (dn != NULL) {
			snprintf(sfs->f_mntfromname, sizeof(sfs->f_mntfromname),
			    "/dev/%s", dn);
			vnode_putname(dn);
		}
	} else if (sfs->f_mntfromname[0] != '/') {
		// libignition mounts Preboot as bare "diskNsM".
		// mount_by_role then compares /dev/ paths and calls the mount "another volume"
		char bare[MAXPATHLEN];

		strlcpy(bare, sfs->f_mntfromname, sizeof(bare));
		snprintf(sfs->f_mntfromname, sizeof(sfs->f_mntfromname), "/dev/%s", bare);
	}
	sfs->f_fsid.val[0] = (int32_t)amp->dev;
	sfs->f_fsid.val[1] = (int32_t)vfs_typenum(mp);
	strlcpy(sfs->f_fstypename, APFS_MODULE_NAME, sizeof(sfs->f_fstypename));

	APFSLOG("mounted volume slot %u read-write%s", amp->vol_slot,
	    amp->vol_group ? ", volume group" : "");
	return 0;

fail:
	if (amp->rw != NULL)
		apfsrw_close(amp->rw);
	if (amp->cont != NULL)
		apfs_container_detach(amp);
	if (amp->dev_opened) {
		(void)VNOP_CLOSE(devvp, FREAD | FWRITE, ctx);
		amp->dev_opened = 0;
	}
	IOLockFree(amp->am_hash_lock);
	IOLockFree(amp->am_cache_lock);
	apfs_caches_free(amp);
	_FREE(amp, M_TEMP);
	if (own_devvp_ref)
		vnode_rele(devvp);
	return error;
}

// Containers are shared between the mounts of their volumes, keyed by the uuid in block zero
static LIST_HEAD(, apfs_container) apfs_containers =
    LIST_HEAD_INITIALIZER(apfs_containers);
static IOLock *apfs_containers_lock;

static int
apfs_container_attach(struct apfs_mount *amp)
{
	struct apfs_container *c;

	IOLockLock(apfs_containers_lock);
	LIST_FOREACH(c, &apfs_containers, c_link) {
		if (memcmp(c->c_uuid, amp->nx.nx_uuid, sizeof(c->c_uuid)) == 0)
			break;
	}
	if (c == NULL) {
		c = (struct apfs_container *)_MALLOC(sizeof(*c), M_TEMP,
		    M_WAITOK | M_ZERO);
		if (c == NULL) {
			IOLockUnlock(apfs_containers_lock);
			return ENOMEM;
		}
		c->c_lock = lck_rw_alloc_init(apfs_lck_grp, LCK_ATTR_NULL);
		c->c_batch_timer = thread_call_allocate(apfs_batch_timer_fire, c);
		if (c->c_lock == NULL || c->c_batch_timer == NULL) {
			if (c->c_lock != NULL)
				lck_rw_free((lck_rw_t *)c->c_lock, apfs_lck_grp);
			if (c->c_batch_timer != NULL)
				thread_call_free((thread_call_t)c->c_batch_timer);
			_FREE(c, M_TEMP);
			IOLockUnlock(apfs_containers_lock);
			return ENOMEM;
		}
		c->c_st_attach_abs = mach_absolute_time();
		memcpy(c->c_uuid, amp->nx.nx_uuid, sizeof(c->c_uuid));
		c->c_devvp = amp->devvp;
		vnode_ref(c->c_devvp);
		c->c_rw_dev.devvp = c->c_devvp;
		c->c_rw_dev.dev_bsize = amp->dev_bsize;
		c->c_rw_dev.block_size = amp->block_size;
		c->c_block_count = amp->block_count;
		LIST_INSERT_HEAD(&apfs_containers, c, c_link);
	}
	c->c_refs++;
	IOLockUnlock(apfs_containers_lock);

	amp->cont = c;
	amp->io_devvp = c->c_devvp;
	return 0;
}

static void
apfs_container_rele(struct apfs_container *c)
{
	int last;

	IOLockLock(apfs_containers_lock);
	last = (--c->c_refs == 0);
	if (last)
		LIST_REMOVE(c, c_link);
	IOLockUnlock(apfs_containers_lock);
	if (last) {
		// every mount committed on its way out, so the timer has nothing left to re-arm for
		thread_call_cancel_wait((thread_call_t)c->c_batch_timer);
		thread_call_free((thread_call_t)c->c_batch_timer);
		buf_flushdirtyblks(c->c_devvp, 1, 0, "apfs_container");
		vnode_rele(c->c_devvp);
		lck_rw_free((lck_rw_t *)c->c_lock, apfs_lck_grp);
		_FREE(c, M_TEMP);
	}
}

static void
apfs_container_detach(struct apfs_mount *amp)
{
	struct apfs_container *c = amp->cont;

	if (c == NULL)
		return;
	amp->cont = NULL;
	amp->io_devvp = NULLVP;
	apfs_container_rele(c);
}

// exclusive. The owner re-enters by depth alone, lck_rw_t itself is not recursive
static void
apfs_lock_enter_at(struct apfs_container *c, void *site)
{
	uint64_t t0;

	if (c->c_lock_owner == (void *)current_thread()) {
		c->c_lock_depth++;
		return;
	}
	t0 = mach_absolute_time();
	lck_rw_lock_exclusive((lck_rw_t *)c->c_lock);
	c->c_lock_owner = (void *)current_thread();
	c->c_lock_depth = 1;
	c->c_lock_abs = mach_absolute_time();
	c->c_lock_site = site;
	c->c_st_xwait_abs += c->c_lock_abs - t0;
}

static void
apfs_note_site(struct apfs_container *c, void *site, uint64_t abs)
{
	int slot = 0;

	for (int i = 0; i < 12; i++) {
		if (c->c_st_sites[i].site == site) {
			slot = i;
			goto found;
		}
		if (c->c_st_sites[i].abs < c->c_st_sites[slot].abs)
			slot = i;
	}
	c->c_st_sites[slot].site = site;
	c->c_st_sites[slot].abs = 0;
	c->c_st_sites[slot].n = 0;
found:
	c->c_st_sites[slot].abs += abs;
	c->c_st_sites[slot].n++;
}

static void
apfs_lock_exit(struct apfs_container *c)
{
	uint64_t held;

	if (--c->c_lock_depth > 0)
		return;
	held = mach_absolute_time() - c->c_lock_abs;
	c->c_st_hold_abs += held;
	apfs_note_site(c, c->c_lock_site, held);
	c->c_lock_owner = NULL;
	lck_rw_unlock_exclusive((lck_rw_t *)c->c_lock);
}

// an outermost exclusive hold becomes shared, no writer gets in between
static void
apfs_lock_downgrade(struct apfs_container *c)
{
	uint64_t held = mach_absolute_time() - c->c_lock_abs;

	c->c_st_hold_abs += held;
	apfs_note_site(c, c->c_lock_site, held);
	c->c_lock_depth = 0;
	c->c_lock_owner = NULL;
	lck_rw_lock_exclusive_to_shared((lck_rw_t *)c->c_lock);
}

static uint64_t
apfs_abs_ms(uint64_t abs)
{
	uint64_t ns = 0;

	absolutetime_to_nanoseconds(abs, &ns);
	return ns / 1000000ull;
}

static void
apfs_batch_stat(struct apfs_container *c, const char *why)
{
	uint64_t now = mach_absolute_time();
	uint64_t hold = c->c_st_hold_abs;

	// the running hold counts too
	if (c->c_lock_depth > 0)
		hold += now - c->c_lock_abs;
	APFSLOG("batch: %llu commits, %llu ops, lock held %llu ms of %llu ms, %llu shared holds, "
	    "exclusive wait %llu ms (%s)",
	    (unsigned long long)c->c_st_commits, (unsigned long long)c->c_st_ops,
	    (unsigned long long)apfs_abs_ms(hold),
	    (unsigned long long)apfs_abs_ms(now - c->c_st_attach_abs),
	    (unsigned long long)c->c_st_shared,
	    (unsigned long long)apfs_abs_ms(c->c_st_xwait_abs), why);
	{
		uint64_t io[5];

		apfsrw_kern_iostat(io);
		APFSLOG("batch: io %llu reads %llu ms, %llu writes, %llu syncs %llu ms, commit %llu ms, "
		    "%llu reloads %llu ms",
		    (unsigned long long)io[0], (unsigned long long)io[1], (unsigned long long)io[2],
		    (unsigned long long)io[3], (unsigned long long)io[4],
		    (unsigned long long)apfs_abs_ms(c->c_st_commit_abs),
		    (unsigned long long)c->c_st_reloads,
		    (unsigned long long)apfs_abs_ms(c->c_st_reload_abs));
	}
	for (int n = 0; n < 4; n++) {
		int best = -1;

		for (int i = 0; i < 8; i++) {
			if (c->c_st_writers[i].ops > 0 && (best < 0 || c->c_st_writers[i].ops > c->c_st_writers[best].ops))
				best = i;
		}
		if (best < 0)
			break;
		APFSLOG("batch: writer %s[%d] %u ops", c->c_st_writers[best].name,
		    c->c_st_writers[best].pid, c->c_st_writers[best].ops);
		c->c_st_writers[best].ops = 0;
	}
	bzero(c->c_st_writers, sizeof(c->c_st_writers));
	for (int n = 0; n < 4; n++) {
		int best = -1;

		for (int i = 0; i < 12; i++) {
			if (c->c_st_sites[i].n > 0 && (best < 0 || c->c_st_sites[i].abs > c->c_st_sites[best].abs))
				best = i;
		}
		if (best < 0)
			break;
		APFSLOG("batch: site %p %llu holds %llu ms", c->c_st_sites[best].site,
		    (unsigned long long)c->c_st_sites[best].n,
		    (unsigned long long)apfs_abs_ms(c->c_st_sites[best].abs));
		c->c_st_sites[best].n = 0;
	}
	bzero(c->c_st_sites, sizeof(c->c_st_sites));
}

// counts the caller against its slot, or takes the quietest one
static void
apfs_note_writer(struct apfs_container *c)
{
	int pid = proc_selfpid(), slot = 0;

	for (int i = 0; i < 8; i++) {
		if (c->c_st_writers[i].pid == pid && c->c_st_writers[i].ops > 0) {
			c->c_st_writers[i].ops++;
			return;
		}
		if (c->c_st_writers[i].ops < c->c_st_writers[slot].ops)
			slot = i;
	}
	c->c_st_writers[slot].pid = pid;
	c->c_st_writers[slot].ops = 1;
	proc_selfname(c->c_st_writers[slot].name, sizeof(c->c_st_writers[slot].name));
}

int
apfs_batch_commit(struct apfs_container *c)
{
	struct apfs_mount *amp = c->c_batch_amp;
	int dirty, err;

	if (amp == NULL)
		return 0;
	uint64_t t0 = mach_absolute_time();

	dirty = apfsrw_batch_dirty(amp->rw);
	err = apfsrw_batch_end(amp->rw);
	c->c_st_commit_abs += mach_absolute_time() - t0;
	c->c_batch_amp = NULL;
	c->c_batch_ops = 0;
	c->c_batch_abs = 0;
	if (!dirty)
		return 0;
	if (err != APFSRW_OK)
		APFSLOG("slot %u: batch commit failed: %s, back at the last checkpoint",
		    amp->vol_slot, apfsrw_strerror(err));
	// every mount re-reads. The writer's handle keeps its cache unless it fell back
	++c->c_generation;
	c->c_last_writer = err == APFSRW_OK ? amp : NULL;
	if (err == APFSRW_OK && ++c->c_st_commits % APFS_BATCH_STAT_EVERY == 0)
		apfs_batch_stat(c, "periodic");
	return err == APFSRW_OK ? 0 : EIO;
}

static void
apfs_batch_arm(struct apfs_container *c)
{
	uint64_t deadline;

	clock_interval_to_deadline(APFS_BATCH_MAX_MS, NSEC_PER_MSEC, &deadline);
	thread_call_enter_delayed((thread_call_t)c->c_batch_timer, deadline);
}

// nothing stays uncommitted longer than APFS_BATCH_MAX_MS after writes stop
static void
apfs_batch_timer_fire(thread_call_param_t p0, __unused thread_call_param_t p1)
{
	struct apfs_container *c = (struct apfs_container *)p0;

	apfs_lock_enter(c);
	(void)apfs_batch_commit(c);
	apfs_lock_exit(c);
}

// volume add for ApfsFileSystemDriver (apfs_kpi.h): a live container changes only here,
// under its lock and through its device's buffer cache, in the mounts' own transaction stream
int
pd_apfs_volume_add(const uint8_t container_uuid[16],
    struct apfsrw_kern_dev *fallback, uint64_t fallback_blocks,
    const char *name, uint16_t role, const uint8_t volume_uuid[16],
    uint32_t *slot)
{
	struct apfs_container *c = NULL;
	struct apfsrw *fs = NULL;
	void *dev = fallback;
	uint64_t blocks = fallback_blocks;
	int err;

	if (container_uuid == NULL || name == NULL || volume_uuid == NULL || slot == NULL)
		return EINVAL;
	// the filesystem registers (and makes the lock) before anything can mount
	if (apfs_containers_lock == NULL)
		return ENXIO;
	// on the fallback path the list lock stays held, so no first mount attaches mid-commit
	IOLockLock(apfs_containers_lock);
	LIST_FOREACH(c, &apfs_containers, c_link) {
		if (memcmp(c->c_uuid, container_uuid, sizeof(c->c_uuid)) == 0)
			break;
	}
	if (c != NULL) {
		c->c_refs++;
		IOLockUnlock(apfs_containers_lock);
		apfs_lock_enter(c);
		// the add commits on a handle of its own, which must start from all the mounts wrote
		(void)apfs_batch_commit(c);
		dev = &c->c_rw_dev;
		blocks = c->c_block_count;
	} else if (fallback == NULL) {
		IOLockUnlock(apfs_containers_lock);
		return ENOENT;
	}

	// a handle without a volume makes the new trees fresh instead of copying a mounted
	// volume's, which a sealed System would pass its features on through
	err = apfsrw_open_kernel(dev, blocks, 1, 0, APFSRW_SLOT_CONTAINER, &fs);
	if (err == APFSRW_OK) {
		err = apfsrw_create_volume(fs, name, role, volume_uuid, slot);
		apfsrw_close(fs);
	}
	APFSLOG("volume add '%s' role 0x%x, %s container: %s, slot %u", name, role,
	    c != NULL ? "mounted" : "unmounted", apfsrw_strerror(err),
	    err == APFSRW_OK ? *slot : 0);

	if (c == NULL) {
		IOLockUnlock(apfs_containers_lock);
	} else {
		// no mount made this commit: each drops its caches and re-reads on its next lock
		c->c_generation++;
		c->c_last_writer = NULL;
		buf_flushdirtyblks(c->c_devvp, 1, 0, "apfs_volume_add");
		apfs_lock_exit(c);
		apfs_container_rele(c);
	}
	switch (err) {
	case APFSRW_OK:		return 0;
	case APFSRW_ENOSPC:	return ENOSPC;
	case APFSRW_EINVAL:	return EINVAL;
	case APFSRW_EPERM:	return EPERM;
	case APFSRW_ENOMEM:	return ENOMEM;
	default:		return EIO;
	}
}

// whether this mount must commit or reload before it reads. Stable under any hold
static int
apfs_view_stale(struct apfs_mount *amp)
{
	struct apfs_container *c = amp->cont;

	return amp->seen_generation != c->c_generation ||
	    (c->c_batch_amp != NULL && c->c_batch_amp != amp &&
	    c->c_batch_amp->vol_slot == amp->vol_slot);
}

// brings the mount's view up to date. Exclusive held: the reload rewrites what readers walk
static void
apfs_view_sync(struct apfs_mount *amp, int write)
{
	struct apfs_container *c = amp->cont;

	// one commit covers one volume's tree. Another volume's write, or a second mount of the
	// batch's own volume reading around it, commits the open transaction first
	if (c->c_batch_amp != NULL && c->c_batch_amp != amp &&
	    (write || c->c_batch_amp->vol_slot == amp->vol_slot))
		(void)apfs_batch_commit(c);
	if (amp->seen_generation != c->c_generation) {
		vfs_context_t ctx = vfs_context_current();
		uint64_t t0 = mach_absolute_time();
		int error = 0;

		// Another volume's commit changed blocks this handle may hold. Its own did not
		if (amp->rw != NULL && c->c_last_writer != amp)
			apfsrw_cache_drop(amp->rw);
		if (amp->rw != NULL)
			error = apfsrw_refresh(amp->rw);
		if (error == 0 && apfs_probe_container(amp, ctx) == 0)
			error = apfs_load_volume(amp, ctx);
		if (error != 0)
			APFSLOG("slot %u: re-read after another volume's commit "
			    "failed: %d", amp->vol_slot, error);
		amp->seen_generation = c->c_generation;
		c->c_st_reload_abs += mach_absolute_time() - t0;
		c->c_st_reloads++;
	}
	if (write && c->c_batch_amp == NULL && amp->rw != NULL &&
	    apfsrw_batch_begin(amp->rw) == APFSRW_OK)
		c->c_batch_amp = amp;
}

void
apfs_rw_lock(struct apfs_mount *amp)
{
	struct apfs_container *c = amp->cont;

	// the exclusive owner reads under what it holds
	if (c->c_lock_owner == (void *)current_thread()) {
		apfs_rw_lock_excl(amp);
		return;
	}
	lck_rw_lock_shared((lck_rw_t *)c->c_lock);
	__atomic_fetch_add(&c->c_st_shared, 1, __ATOMIC_RELAXED);
	if (!apfs_view_stale(amp))
		return;
	// no upgrade: drop, sync under exclusive, then downgrade so the synced view stays put
	lck_rw_unlock_shared((lck_rw_t *)c->c_lock);
	apfs_lock_enter(c);
	apfs_view_sync(amp, 0);
	apfs_lock_downgrade(c);
}

void
apfs_rw_lock_excl(struct apfs_mount *amp)
{
	apfs_lock_enter(amp->cont);
	apfs_view_sync(amp, 0);
}

void
apfs_rw_lock_write(struct apfs_mount *amp)
{
	apfs_lock_enter(amp->cont);
	apfs_view_sync(amp, 1);
}

void
apfs_rw_unlock(struct apfs_mount *amp)
{
	struct apfs_container *c = amp->cont;

	if (c->c_lock_owner != (void *)current_thread()) {
		lck_rw_unlock_shared((lck_rw_t *)c->c_lock);
		return;
	}
	// a busy container commits on age here, an idle one from the timer
	if (c->c_lock_depth == 1 && c->c_batch_abs != 0 &&
	    apfs_abs_ms(mach_absolute_time() - c->c_batch_abs) >= APFS_BATCH_MAX_MS)
		(void)apfs_batch_commit(c);
	apfs_lock_exit(c);
}

int
apfs_write_done(struct apfs_mount *amp)
{
	struct apfs_container *c = amp->cont;
	struct apfsrw_volume_info vi;

	c->c_st_ops++;
	apfs_note_writer(c);
	if (c->c_batch_amp != amp) {
		// no batch could open, so the op committed on its own
		++c->c_generation;
		c->c_last_writer = amp;
		c->c_st_commits++;
		return 0;
	}
	// reads walk the batch's trees, whose new nodes are mapped at the next xid
	if (apfsrw_get_volume_info(amp->rw, &vi) == APFSRW_OK) {
		amp->xid = vi.xid + 1;
		amp->root_tree_paddr = (apfs_paddr_t)vi.root_tree_paddr;
		amp->volume_omap_tree_paddr = (apfs_paddr_t)vi.volume_omap_tree_paddr;
	}
	amp->rgen++;
	if (c->c_batch_abs == 0) {
		c->c_batch_abs = mach_absolute_time();
		apfs_batch_arm(c);
	}
	if (++c->c_batch_ops >= APFS_BATCH_MAX_OPS ||
	    apfsrw_batch_blocks(amp->rw) + apfsrw_batch_pending(amp->rw) >= APFS_BATCH_MAX_BLOCKS)
		(void)apfs_batch_commit(c);
	return 0;
}

int
apfs_batch_sync(struct apfs_mount *amp)
{
	int error;

	if (amp == NULL || amp->cont == NULL)
		return 0;
	apfs_lock_enter(amp->cont);
	error = apfs_batch_commit(amp->cont);
	apfs_lock_exit(amp->cont);
	return error;
}

int
apfs_batch_owns(struct apfs_mount *amp, apfs_paddr_t paddr, uint64_t n)
{
	return amp->cont->c_batch_amp == amp &&
	    apfsrw_batch_owns(amp->rw, (uint64_t)paddr, n);
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
	// libignition mounts Preboot with a bare "diskNsM". Treat it as /dev/diskNsM
	if (fspec[0] != '/' && fspec_len + 5 <= sizeof(fspec)) {
		size_t k;

		for (k = fspec_len; k > 0; k--)
			fspec[k - 1 + 5] = fspec[k - 1];
		memcpy(fspec, "/dev/", 5);
	}

	// Look the path up to get the real dev_t for any device node
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
			return bdevvp(dev, devvpp);
		}
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
	return bdevvp(dev, devvpp);
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
    uint32_t dev_bsize, kauth_cred_t cred, void *out)
{
	buf_t bp = NULL;
	int error;

	if (devvp == NULLVP || paddr < 0 || block_size == 0 || out == NULL)
		return EINVAL;

	error = (int)buf_meta_bread(devvp,
	    (daddr64_t)((uint64_t)paddr * (block_size / dev_bsize)), block_size,
	    cred, &bp);
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
    uint32_t block_size, uint32_t dev_bsize, apfs_xid_t min_xid, vfs_context_t ctx)
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
	// On reload, block zero mirrors our last commit.
	// Check the slot it names instead of the whole ring, and scan only if they disagree
	if (min_xid != 0 && best_xid >= min_xid && apfs_le32(nx->nx_xp_desc_len) != 0) {
		apfs_paddr_t paddr = desc_base + (apfs_le32(nx->nx_xp_desc_index) +
		    apfs_le32(nx->nx_xp_desc_len) - 1) % desc_blocks;
		struct apfs_nx_superblock *candidate = (struct apfs_nx_superblock *)block;

		if (apfs_read_probe_block(devvp, paddr, block_size, dev_bsize,
		    vfs_context_ucred(ctx), block) == 0 &&
		    !apfs_verify_object_checksum(block, block_size) &&
		    apfs_le32(candidate->nx_magic) == APFS_NX_MAGIC &&
		    apfs_le64(candidate->nx_o.o_xid) == best_xid) {
			memcpy(nx, candidate, sizeof(*nx));
			desc_blocks = 0;
		}
	}
	for (i = 0; i < desc_blocks; i++) {
		struct apfs_nx_superblock *candidate;
		apfs_paddr_t paddr = desc_base + i;

		error = apfs_read_probe_block(devvp, paddr, block_size,
		    dev_bsize, vfs_context_ucred(ctx), block);
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

static int
apfs_probe_container(struct apfs_mount *amp, vfs_context_t ctx)
{
	struct apfs_nx_superblock nx;
	struct apfs_nx_superblock *disk_nx;
	buf_t bp = NULL;
	uint32_t block_size;
	uint32_t max_fs;
	vnode_t devvp = amp->io_devvp;
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

	// Block numbers handed to the buffer cache are in device sectors
	if (amp->dev_bsize == 0) {
		uint32_t dbs = 0;

		if (VNOP_IOCTL(devvp, DKIOCGETBLOCKSIZE, (caddr_t)&dbs, FREAD,
		    ctx) != 0 || dbs == 0 || dbs > block_size ||
		    (block_size % dbs) != 0)
			dbs = 512;
		amp->dev_bsize = dbs;
	}

	error = apfs_select_checkpoint_nx(devvp, &nx, block_size,
	    amp->dev_bsize, amp->am_probe_logged ? amp->xid : 0, ctx);
	if (error)
		return error;

	max_fs = apfs_le32(nx.nx_max_file_systems);
	// Log the container once. apfs_load_volume() sets the flag after its own line,
	// and reloads after each commit stay silent
	if (amp->am_probe_logged)
		goto adopt;
	APFSLOG("container xid=%llu block_size=%u blocks=%llu next_oid=0x%llx",
	    (unsigned long long)apfs_le64(nx.nx_o.o_xid), block_size,
	    (unsigned long long)apfs_le64(nx.nx_block_count),
	    (unsigned long long)apfs_le64(nx.nx_next_oid));
	apfs_log_uuid(nx.nx_uuid);

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
		// A departing mount must not stay the container's last writer
		if (amp->cont != NULL) {
			apfs_rw_lock_excl(amp);
			if (amp->cont->c_batch_amp == amp)
				(void)apfs_batch_commit(amp->cont);
			apfs_batch_stat(amp->cont, "unmount");
			if (amp->cont->c_last_writer == amp)
				amp->cont->c_last_writer = NULL;
			apfs_rw_unlock(amp);
		}
		if (amp->rw != NULL) {
			apfsrw_close(amp->rw);
			amp->rw = NULL;
		}
		if (amp->io_devvp) {
			// Land every delayed write before the device closes.
			// spec_close may discard whatever is still dirty
			buf_flushdirtyblks(amp->io_devvp, 1, 0, "apfs_unmount");
		}
		apfs_container_detach(amp);
		if (amp->devvp) {
			if (amp->dev_opened) {
				(void)VNOP_CLOSE(amp->devvp, FREAD | FWRITE, ctx);
				amp->dev_opened = 0;
			}
			vnode_rele(amp->devvp);
			amp->devvp = NULLVP;
		}
		vfs_setfsprivate(mp, NULL);
		IOLockFree(amp->am_hash_lock);
		IOLockFree(amp->am_cache_lock);
		apfs_caches_free(amp);
		_FREE(amp, M_TEMP);
	}
	return 0;
}

static int
apfs_root(struct mount *mp, vnode_t *vpp,
    __unused vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);

	if (amp == NULL)
		return EINVAL;
	// Nothing holds a usecount on the root vnode, so it can be recycled like any other.
	// a cached pointer would then name some other file
	return apfs_vget(amp, APFS_ROOT_FILEID, NULLVP, vpp);
}

static int
apfs_getattr(__unused struct mount *mp, struct vfs_attr *fsap,
    __unused vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);
	uint32_t block_size = amp ? amp->block_size : APFS_BS_BYTES;
	uint64_t freeb = 0, files = 0;

	VFSATTR_RETURN(fsap, f_objcount, 1);
	VFSATTR_RETURN(fsap, f_maxobjcount, 1);
	VFSATTR_RETURN(fsap, f_bsize, block_size);
	VFSATTR_RETURN(fsap, f_iosize, block_size);
	if (amp) {
		struct apfsrw_space_info si;

		// the spaceman walk goes through libapfsrw's own block cache
		apfs_rw_lock_excl(amp);
		if (amp->rw != NULL &&
		    apfsrw_get_space_info(amp->rw, &si) == 0)
			freeb = si.free_count;
		files = OSSwapLittleToHostInt64(amp->apfs.apfs_num_files) +
		    OSSwapLittleToHostInt64(amp->apfs.apfs_num_directories) +
		    OSSwapLittleToHostInt64(amp->apfs.apfs_num_symlinks) +
		    OSSwapLittleToHostInt64(amp->apfs.apfs_num_other_fsobjects);
		apfs_rw_unlock(amp);
		if (freeb > amp->block_count)
			freeb = amp->block_count;
		VFSATTR_RETURN(fsap, f_blocks, amp->block_count);
		VFSATTR_RETURN(fsap, f_bfree, freeb);
		VFSATTR_RETURN(fsap, f_bavail, freeb);
		VFSATTR_RETURN(fsap, f_bused, amp->block_count - freeb);
		// VOL_GROUPS on root is what makes xnu mount the Data volume by role
		if (VFSATTR_IS_ACTIVE(fsap, f_capabilities)) {
			vol_capabilities_attr_t *cap = &fsap->f_capabilities;

			bzero(cap, sizeof(*cap));
			if (amp->vol_group)
				cap->capabilities[VOL_CAPABILITIES_FORMAT] = VOL_CAP_FMT_VOL_GROUPS;
			cap->valid[VOL_CAPABILITIES_FORMAT] = VOL_CAP_FMT_VOL_GROUPS;
			VFSATTR_SET_SUPPORTED(fsap, f_capabilities);
		}
		// ATTR_VOL_UUID: init_featureflags aborts when getattrlist on / fails for it
		if (VFSATTR_IS_ACTIVE(fsap, f_uuid)) {
			memcpy(fsap->f_uuid, amp->apfs.apfs_vol_uuid, sizeof(fsap->f_uuid));
			VFSATTR_SET_SUPPORTED(fsap, f_uuid);
		}
	}
	// Free inodes are not a resource in APFS. Report plenty
	VFSATTR_RETURN(fsap, f_files, files + (1ull << 32));
	VFSATTR_RETURN(fsap, f_ffree, 1ull << 32);
	return 0;
}

// VFSIOC_MOUNT_BYROLE: mount the root container's volume with this role into mp
static int
apfs_vfs_ioctl(struct mount *mp, u_long command, caddr_t data,
    __unused int flags, vfs_context_t ctx)
{
	fs_role_mount_args_t *frma = (fs_role_mount_args_t *)data;
	struct apfs_mount *origin;
	struct vfsstatfs *sfs;
	vnode_t devvp = NULLVP;
	char bsd[64];
	dev_t dev;
	int error;

	if (command != VFSIOC_MOUNT_BYROLE)
		return ENOTSUP;
	if (frma == NULL || frma->root_mp == NULL ||
	    strcmp(vfs_statfs(frma->root_mp)->f_fstypename, APFS_MODULE_NAME) != 0)
		return EINVAL;
	origin = VFSTOAPFS(frma->root_mp);
	if (origin == NULL)
		return EINVAL;
	if (frma->mount_role > 0xffff ||
	    apfs_role_dev(origin->dev, (uint16_t)frma->mount_role, &dev, bsd, sizeof(bsd)) != 0) {
		APFSLOG("mount by role 0x%x: no such volume in the container", frma->mount_role);
		return ENOENT;
	}
	error = bdevvp(dev, &devvp);
	if (error) {
		APFSLOG("mount by role 0x%x: bdevvp(%s) failed: %d", frma->mount_role, bsd, error);
		return error;
	}
	// xnu looks f_mntfromname up afterwards for mnt_devvp
	sfs = vfs_statfs(mp);
	snprintf(sfs->f_mntfromname, sizeof(sfs->f_mntfromname), "/dev/%s", bsd);
	error = apfs_mount_dev(mp, devvp, 1, ctx);
	if (error) {
		sfs->f_mntfromname[0] = '\0';
		APFSLOG("mount by role 0x%x: %s failed: %d", frma->mount_role, bsd, error);
		return error;
	}
	APFSLOG("mounted role 0x%x volume /dev/%s on %s", frma->mount_role, bsd,
	    sfs->f_mntonname);
	return 0;
}

static int
apfs_sync(struct mount *mp, int waitfor, __unused vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);

	if (amp != NULL)
		(void)apfs_batch_sync(amp);
	if (amp && amp->io_devvp)
		buf_flushdirtyblks(amp->io_devvp, waitfor == MNT_WAIT, 0,
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
	if (apfs_containers_lock == NULL) {
		apfs_containers_lock = IOLockAlloc();
		if (apfs_containers_lock == NULL)
			return ENOMEM;
	}
	if (apfs_lck_grp == NULL) {
		apfs_lck_grp = lck_grp_alloc_init("apfs", LCK_GRP_ATTR_NULL);
		if (apfs_lck_grp == NULL)
			return ENOMEM;
	}

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

	APFSLOG("registered apfs filesystem");
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
