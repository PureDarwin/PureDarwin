/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfs.h"
#include "apfsrw/apfsrw.h"

#include <IOKit/IOLocks.h>

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
static int apfs_container_attach(struct apfs_mount *amp);
static void apfs_container_detach(struct apfs_mount *amp);

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
	if (amp->am_hash_lock == NULL) {
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
	if (apfs_volume_slot_for_dev(amp->dev, &amp->vol_slot) != 0)
		amp->vol_slot = 0;
	error = apfs_container_attach(amp);
	if (error)
		goto fail;
	if (amp->io_devvp != devvp) {
		error = apfs_probe_container(amp, ctx);
		if (error)
			goto fail;
	}
	error = apfs_load_volume(amp, ctx);
	if (error)
		goto fail;

	error = apfsrw_open_kernel(&amp->cont->c_rw_dev, amp->block_count, 1, 0,
	    amp->vol_slot, &amp->rw);
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

	APFSLOG("mounted volume slot %u read-write", amp->vol_slot);
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
		c->c_lock = IORecursiveLockAlloc();
		if (c->c_lock == NULL) {
			_FREE(c, M_TEMP);
			IOLockUnlock(apfs_containers_lock);
			return ENOMEM;
		}
		memcpy(c->c_uuid, amp->nx.nx_uuid, sizeof(c->c_uuid));
		c->c_devvp = amp->devvp;
		vnode_ref(c->c_devvp);
		c->c_rw_dev.devvp = c->c_devvp;
		c->c_rw_dev.dev_bsize = amp->dev_bsize;
		c->c_rw_dev.block_size = amp->block_size;
		LIST_INSERT_HEAD(&apfs_containers, c, c_link);
	}
	c->c_refs++;
	IOLockUnlock(apfs_containers_lock);

	amp->cont = c;
	amp->io_devvp = c->c_devvp;
	amp->am_rw_lock = c->c_lock;
	amp->seen_generation = c->c_generation;
	return 0;
}

static void
apfs_container_detach(struct apfs_mount *amp)
{
	struct apfs_container *c = amp->cont;
	int last;

	if (c == NULL)
		return;
	IOLockLock(apfs_containers_lock);
	last = (--c->c_refs == 0);
	if (last)
		LIST_REMOVE(c, c_link);
	IOLockUnlock(apfs_containers_lock);
	amp->cont = NULL;
	amp->io_devvp = NULLVP;
	amp->am_rw_lock = NULL;
	if (last) {
		buf_flushdirtyblks(c->c_devvp, 1, 0, "apfs_container");
		vnode_rele(c->c_devvp);
		IORecursiveLockFree((IORecursiveLock *)c->c_lock);
		_FREE(c, M_TEMP);
	}
}

extern uint64_t apfsrw_kern_sync_ns, apfsrw_kern_sync_n, apfsrw_kern_bdwrites, apfsrw_kern_breads;
extern uint64_t apfsrw_wblocks[4];		// mutation, commit, publish, frees
extern uint64_t apfsrw_commits;
uint64_t apfs_ubc_ns, apfs_reload_ns, apfs_reload_n;
// setattr calls by attribute mask. Bit 0x80 marks the ones that committed
uint32_t apfs_setattr_masks[256];

// Hold time per call site, so the report names the vnop that serialises the container
static struct {
	const char *tag;
	uint64_t ns;
	uint64_t n;
	uint64_t bdw;
	uint64_t sync_ns;
} apfs_tags[16];
static uint64_t apfs_acq_bdw, apfs_acq_sync_ns;

static void
apfs_tag_add(const char *tag, uint64_t ns)
{
	for (unsigned i = 0; i < sizeof(apfs_tags) / sizeof(apfs_tags[0]); i++) {
		if (apfs_tags[i].tag == NULL || apfs_tags[i].tag == tag) {
			apfs_tags[i].tag = tag;
			apfs_tags[i].ns += ns;
			apfs_tags[i].n++;
			if (apfsrw_kern_bdwrites >= apfs_acq_bdw && apfsrw_kern_sync_ns >= apfs_acq_sync_ns) {
				apfs_tags[i].bdw += apfsrw_kern_bdwrites - apfs_acq_bdw;
				apfs_tags[i].sync_ns += apfsrw_kern_sync_ns - apfs_acq_sync_ns;
			}
			return;
		}
	}
}

// pdapfsbatch=1 holds one transaction open across mutations instead of committing each.
// Off for now, deferring a commit breaks the read path, which walks the tree itself
static int apfs_batch_on = -1;

static int
apfs_batch_enabled(void)
{
	if (apfs_batch_on < 0) {
		int v = 0;

		apfs_batch_on = PE_parse_boot_argn("pdapfsbatch", &v,
		    sizeof(v)) && v;
	}
	return apfs_batch_on;
}

int
apfs_batch_flush(struct apfs_container *c)
{
	struct apfs_mount *amp = c->c_batch_amp;
	int err;

	if (amp == NULL)
		return 0;
	c->c_batch_amp = NULL;
	c->c_batch_ops = 0;
	c->c_batch_first_abs = 0;
	err = apfsrw_batch_end(amp->rw);
	if (err != 0)
		APFSLOG("slot %u: batch commit failed: %d", amp->vol_slot, err);
	// Like any commit, this leaves the writer's own copy behind too.
	// The next apfs_rw_lock_tag() re-reads it
	++c->c_generation;
	return err;
}

// One mutation landed on amp and sits in the open batch.
// Commit once the policy thresholds are reached. Called with the lock held
void
apfs_batch_note(struct apfs_mount *amp)
{
	struct apfs_container *c = amp->cont;
	extern uint64_t mach_absolute_time(void);
	extern void absolutetime_to_nanoseconds(uint64_t abstime, uint64_t *result);
	uint64_t ns = 0;

	if (c->c_batch_amp != amp)
		return;			// committed on its own
	{
		struct apfsrw_volume_info vi;

		if (apfsrw_get_volume_info(amp->rw, &vi) == 0) {
			if (c->c_batch_ops < 8)
				APFSLOG("batch slot %u op %u: root %lld -> %llu, "
				    "xid %llu, deferred %u", amp->vol_slot,
				    c->c_batch_ops, (long long)amp->root_tree_paddr,
				    (unsigned long long)vi.root_tree_paddr,
				    (unsigned long long)vi.xid,
				    apfsrw_batch_pending(amp->rw));
			amp->root_tree_oid = (apfs_oid_t)vi.root_tree_oid;
			amp->root_tree_paddr = (apfs_paddr_t)vi.root_tree_paddr;
			amp->volume_omap_tree_paddr =
			    (apfs_paddr_t)vi.volume_omap_tree_paddr;
			amp->xid = (apfs_xid_t)vi.xid;
		}
	}
	c->c_batch_ops++;
	absolutetime_to_nanoseconds(mach_absolute_time() - c->c_batch_first_abs, &ns);
	if (c->c_batch_ops >= APFS_BATCH_MAX_OPS ||
	    apfsrw_batch_pending(amp->rw) >= APFS_BATCH_MAX_DEFERRED ||
	    ns >= APFS_BATCH_MAX_NS)
		(void)apfs_batch_flush(c);
}

void
apfs_rw_lock(struct apfs_mount *amp)
{
	apfs_rw_lock_tag(amp, "misc");
}

void
apfs_rw_lock_tag(struct apfs_mount *amp, const char *tag)
{
	struct apfs_container *c = amp->cont;
	extern uint64_t mach_absolute_time(void);
	extern void absolutetime_to_nanoseconds(uint64_t abstime, uint64_t *result);

	// One recursive lock serialises the whole container. Report who blocks whom
	if (!IORecursiveLockTryLock((IORecursiveLock *)c->c_lock)) {
		uint64_t t0 = mach_absolute_time(), ns = 0;
		int blocker = c->c_owner_pid;   // The holder clears this on unlock

		IORecursiveLockLock((IORecursiveLock *)c->c_lock);
		absolutetime_to_nanoseconds(mach_absolute_time() - t0, &ns);
		if (ns > 2000000000ull)
			printf("PD-apfslock: pid %d waited %llu ms; blocked by pid %d\n",
			    proc_selfpid(), (unsigned long long)(ns / 1000000), blocker);
	}
	c->c_owner_pid = proc_selfpid();
	c->c_acq_abs = mach_absolute_time();
	apfs_acq_bdw = apfsrw_kern_bdwrites;
	apfs_acq_sync_ns = apfsrw_kern_sync_ns;
	c->c_tag = tag;
	// One commit covers one volume, and a re-read would drop another
	// volume's uncommitted state, so hand the container over cleanly
	if (c->c_batch_amp != NULL && c->c_batch_amp != amp)
		(void)apfs_batch_flush(c);
	if (amp->seen_generation != c->c_generation) {
		vfs_context_t ctx = vfs_context_current();
		int error = 0;
		uint64_t r0 = mach_absolute_time(), rns = 0;

		if (amp->rw != NULL)
			error = apfsrw_refresh(amp->rw);
		if (error == 0 && apfs_probe_container(amp, ctx) == 0)
			error = apfs_load_volume(amp, ctx);
		if (error != 0)
			APFSLOG("slot %u: re-read after another volume's commit "
			    "failed: %d", amp->vol_slot, error);
		amp->seen_generation = c->c_generation;
		absolutetime_to_nanoseconds(mach_absolute_time() - r0, &rns);
		apfs_reload_ns += rns;
		apfs_reload_n++;
	}
	// Now that this mount is current, let its mutations accumulate
	if (apfs_batch_enabled() && c->c_batch_amp == NULL && amp->rw != NULL &&
	    apfsrw_batch_begin(amp->rw) == 0) {
		c->c_batch_amp = amp;
		c->c_batch_first_abs = mach_absolute_time();
		c->c_batch_ops = 0;
	}
}

void
apfs_rw_unlock(struct apfs_mount *amp)
{
	struct apfs_container *c = amp->cont;
	extern uint64_t mach_absolute_time(void);
	extern void absolutetime_to_nanoseconds(uint64_t abstime, uint64_t *result);
	uint64_t now = mach_absolute_time(), ns = 0;

	// Is the container lock held long, or only queued behind many short holds?
	if (c->c_acq_abs != 0) {
		absolutetime_to_nanoseconds(now - c->c_acq_abs, &ns);
		c->c_hold_ns += ns;
		c->c_hold_count++;
		apfs_tag_add(c->c_tag != NULL ? c->c_tag : "misc", ns);
		if (ns > c->c_hold_max_ns) {
			c->c_hold_max_ns = ns;
			c->c_hold_max_pid = c->c_owner_pid;
		}
	}
	if (c->c_report_abs == 0) {
		c->c_report_abs = now;
	} else {
		uint64_t since = 0;

		absolutetime_to_nanoseconds(now - c->c_report_abs, &since);
		if (since > 60000000000ull && c->c_hold_count != 0) {
			printf("PD-apfshold: %llu holds in %llu s, mean %llu us, max %llu ms by pid %d\n",
			    (unsigned long long)c->c_hold_count, (unsigned long long)(since / 1000000000ull),
			    (unsigned long long)(c->c_hold_ns / c->c_hold_count / 1000),
			    (unsigned long long)(c->c_hold_max_ns / 1000000), c->c_hold_max_pid);
			for (unsigned i = 0; i < sizeof(apfs_tags) / sizeof(apfs_tags[0]); i++) {
				if (apfs_tags[i].tag == NULL || apfs_tags[i].n == 0)
					continue;
				printf("PD-apfstag: %-22s %6llu holds %8llu ms total, %llu bdwrites, sync %llu ms\n",
				    apfs_tags[i].tag, (unsigned long long)apfs_tags[i].n,
				    (unsigned long long)(apfs_tags[i].ns / 1000000ull),
				    (unsigned long long)apfs_tags[i].bdw,
				    (unsigned long long)(apfs_tags[i].sync_ns / 1000000ull));
				apfs_tags[i].ns = apfs_tags[i].n = apfs_tags[i].bdw = apfs_tags[i].sync_ns = 0;
			}
			printf("PD-apfsio: %llu syncs %llu ms, %llu bdwrites, %llu breads, ubc_setsize %llu ms, %llu reloads %llu ms\n",
			    (unsigned long long)apfsrw_kern_sync_n, (unsigned long long)(apfsrw_kern_sync_ns / 1000000ull),
			    (unsigned long long)apfsrw_kern_bdwrites, (unsigned long long)apfsrw_kern_breads,
			    (unsigned long long)(apfs_ubc_ns / 1000000ull),
			    (unsigned long long)apfs_reload_n, (unsigned long long)(apfs_reload_ns / 1000000ull));
			printf("PD-apfscommit: %llu commits, blocks: mutation %llu, "
			    "commit %llu, publish %llu, frees %llu\n",
			    (unsigned long long)apfsrw_commits,
			    (unsigned long long)apfsrw_wblocks[0],
			    (unsigned long long)apfsrw_wblocks[1],
			    (unsigned long long)apfsrw_wblocks[2],
			    (unsigned long long)apfsrw_wblocks[3]);
			apfsrw_wblocks[0] = apfsrw_wblocks[1] = 0;
			apfsrw_wblocks[2] = apfsrw_wblocks[3] = 0;
			apfsrw_commits = 0;
			apfsrw_kern_sync_ns = apfsrw_kern_sync_n = apfsrw_kern_bdwrites = apfsrw_kern_breads = apfs_ubc_ns = 0;
			apfs_reload_ns = apfs_reload_n = 0;
			for (unsigned m = 0; m < 256; m++) {
				if (apfs_setattr_masks[m] != 0)
					printf("PD-apfsattr: mask 0x%x%s %u\n", m & 0x7f,
					    (m & 0x80) ? " committed" : "", apfs_setattr_masks[m]);
				apfs_setattr_masks[m] = 0;
			}
			c->c_report_abs = now;
			c->c_hold_ns = c->c_hold_count = c->c_hold_max_ns = 0;
			c->c_hold_max_pid = -1;
		}
	}
	// An idle batch would otherwise sit uncommitted until the next mutation:
	// bound how long a write can stay in memory
	if (c->c_batch_amp != NULL && apfsrw_batch_dirty(c->c_batch_amp->rw)) {
		uint64_t age = 0;

		absolutetime_to_nanoseconds(now - c->c_batch_first_abs, &age);
		if (age >= APFS_BATCH_MAX_NS)
			(void)apfs_batch_flush(c);
	}
	c->c_acq_abs = 0;
	c->c_owner_pid = -1;
	c->c_owner_thread = NULL;
	IORecursiveLockUnlock((IORecursiveLock *)c->c_lock);
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

int
apfs_reload_container(struct apfs_mount *amp, vfs_context_t ctx)
{
	(void)ctx;

	if (amp == NULL)
		return EINVAL;
	// Inside a batch nothing reached the disk, apfs_batch_flush() bumps the generation later.
	// Outside one, bump it and let the next lock re-read once. This mount stays stale on purpose
	if (amp->cont->c_batch_amp == amp) {
		apfs_batch_note(amp);
		return 0;
	}
	++amp->cont->c_generation;
	return 0;
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
		// Anything still batched has to land before the handle closes
		if (amp->cont != NULL) {
			apfs_rw_lock_tag(amp, "apfs_unmount");
			(void)apfs_batch_flush(amp->cont);
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

		apfs_rw_lock(amp);
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

static int
apfs_sync(struct mount *mp, int waitfor, __unused vfs_context_t ctx)
{
	struct apfs_mount *amp = VFSTOAPFS(mp);

	// A batched mutation is only in memory until it is committed
	if (amp != NULL && amp->cont != NULL) {
		apfs_rw_lock_tag(amp, "apfs_sync");
		(void)apfs_batch_flush(amp->cont);
		apfs_rw_unlock(amp);
	}
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
