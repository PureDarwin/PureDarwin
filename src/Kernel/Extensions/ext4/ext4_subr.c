/*
 * ext4_subr.c - superblock, inode, extent, and block I/O helpers
 */
#include "ext4.h"
#include <sys/buf.h>
#include <sys/ubc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <string.h>
#include <IOKit/IOLocks.h>

/*
 * True if `ino` has a live (open) ext4node in the in-core hash. The inode
 * bitmap is the authoritative allocation record, but a read of a stale
 * cached bitmap block can make an in-use inode look free (observed: Xorg's
 * own log file's inode got handed back out to a short-lived /tmp file while
 * still open, and freeing the second file's extents zeroed the first file's
 * still-live extent header out from under it). Cross-checking the hash is a
 * cheap belt-and-suspenders guard against ext4_alloc_inode() ever handing out
 * a number that's still actively open, regardless of what the bitmap says.
 */
int
ext4_ino_is_live(struct ext4mount *emp, ino_t ino)
{
	IOLock *hlock = (IOLock *)emp->em_hash_lock;
	struct ext4_node_bucket *bucket = &emp->em_node_hash[EXT4_NODE_HASH(ino)];
	struct ext4node *ep;
	int live = 0;

	IOLockLock(hlock);
	LIST_FOREACH(ep, bucket, e_hash) {
		if (ep->e_ino == ino) {
			live = 1;
			break;
		}
	}
	IOLockUnlock(hlock);
	return live;
}

enum vtype
ext4_ft_to_vtype(uint8_t ft)
{
	switch (ft) {
	case EXT4_FT_REG_FILE: return VREG;
	case EXT4_FT_DIR:      return VDIR;
	case EXT4_FT_CHRDEV:   return VCHR;
	case EXT4_FT_BLKDEV:   return VBLK;
	case EXT4_FT_FIFO:     return VFIFO;
	case EXT4_FT_SOCK:     return VSOCK;
	case EXT4_FT_SYMLINK:  return VLNK;
	default:              return VNON;
	}
}

enum vtype
ext4_mode_to_vtype(uint16_t mode)
{
	switch (mode & EXT4_S_IFMT) {
	case EXT4_S_IFREG:  return VREG;
	case EXT4_S_IFDIR:  return VDIR;
	case EXT4_S_IFCHR:  return VCHR;
	case EXT4_S_IFBLK:  return VBLK;
	case EXT4_S_IFIFO:  return VFIFO;
	case EXT4_S_IFSOCK: return VSOCK;
	case EXT4_S_IFLNK:  return VLNK;
	default:           return VNON;
	}
}

/*
 * Read one fs block (em_blocksize bytes) at physical block pblk from the
 * underlying device.  buf_meta_bread()'s blkno argument is interpreted in
 * units of the device vnode's tagged block size (bdevBlockSize, set only by
 * a prior DKIOCSETBLOCKSIZE), NOT in bytes and NOT implicitly in em_blocksize
 * units just because that's what we pass as the size argument. Rather than
 * depend on a DKIOCSETBLOCKSIZE retag having actually taken effect on this
 * vnode (unverifiable from here, and silently wrong if it didn't - see
 * ext4_mount() in ext4_vfsops.c), convert pblk into the device's real native
 * sector units ourselves using em_dev_bsize queried at mount time.
 */
int
ext4_blkread(struct ext4mount *emp, uint64_t pblk, buf_t *bpp)
{
	buf_t bp = NULL;
	int error;
	daddr64_t devblk;

	devblk = (daddr64_t)(pblk * ((uint64_t)emp->em_blocksize / emp->em_dev_bsize));

	error = buf_meta_bread(emp->em_devvp, devblk,
	    (int)emp->em_blocksize, NOCRED, &bp);
	if (error) {
		if (bp)
			buf_brelse(bp);
		*bpp = NULL;
		return error;
	}
	/* A block sitting in the open journal transaction is newer than
	 * whatever the buf cache (or disk) just returned - the in-place write
	 * is deferred until commit. Overlay it so reads inside a transaction
	 * always see their own writes even if the clean buffer got recycled. */
	(void)ext4_jnl_read_overlay(emp, pblk, (void *)buf_dataptr(bp));
	*bpp = bp;
	return 0;
}

/*
 * Journaled metadata write. With a journal: log the buffer's contents into
 * the open transaction and release the buffer CLEAN - the in-place write
 * happens at commit (WAL ordering: journal blocks must be durable first).
 * The buf cache keeps the new contents for subsequent reads; if it drops
 * them, ext4_blkread()'s overlay (above) restores them from the txn.
 * Without a journal: plain synchronous write-through.
 */
int
ext4_meta_bwrite(struct ext4mount *emp, buf_t bp)
{
	uint64_t scale = (uint64_t)emp->em_blocksize / emp->em_dev_bsize;
	uint64_t pblk = (uint64_t)buf_blkno(bp) / (scale ? scale : 1);
	int error;

	if (emp->em_jnl != NULL) {
		error = ext4_jnl_log_block(emp, pblk, (void *)buf_dataptr(bp));
		if (error == 0) {
			buf_brelse(bp);
			return 0;
		}
		/* txn overflow / no-journal race: fall through to direct write */
	}
	return buf_bwrite(bp);
}

/*
 * Validate a directory block's rec_len chain before writing it back: every
 * entry must be >= 8 bytes, 4-aligned, contain its name, and the chain must
 * land exactly on the block's data limit (ext2 dir blocks are fully covered
 * by design). A block failing this was corrupted in memory - writing it out
 * would destroy the directory, so callers must refuse.
 */
int
ext4_dir_block_check(struct ext4mount *emp, const void *block,
    const char *tag, uint64_t ino)
{
	uint32_t limit = emp->em_blocksize;
	uint32_t p = 0;

	if (ext4_dir_block_has_tail(emp, block))
		limit -= EXT4_DIR_ENTRY_TAIL_SIZE;

	while (p < limit) {
		const struct ext4_dir_entry_2 *de =
		    (const struct ext4_dir_entry_2 *)((const char *)block + p);
		uint16_t reclen = le16(de->rec_len);

		if (reclen < 8 || (reclen & 3) != 0 || p + reclen > limit ||
		    (uint32_t)(8 + de->name_len) > reclen) {
			E4LOG("%s: corrupt dir block (dir ino=%llu off=%u "
			    "reclen=%u namelen=%u inode=%u)", tag,
			    (unsigned long long)ino, p, reclen, de->name_len,
			    le32(de->inode));
			emp->em_stats.dir_check_fails++;
			return EIO;
		}
		p += reclen;
	}
	if (p != limit) {
		E4LOG("%s: dir block chain ends at %u != %u (dir ino=%llu)",
		    tag, p, limit, (unsigned long long)ino);
		emp->em_stats.dir_check_fails++;
		return EIO;
	}
	return 0;
}

int
ext4_read_super(struct ext4mount *emp)
{
	struct ext4_super_block *sb = &emp->em_sb;
	buf_t bp = NULL;
	int error;

	/*
	 * The superblock lives at byte offset 1024.  Read it via a 2048-byte
	 * meta read from device block 0 using a temporary 512-byte view so we
	 * don't depend on the fs blocksize (which we don't know yet).  We read
	 * 4 sectors (2KB) at DEV_BSIZE granularity.
	 */
	error = buf_meta_bread(emp->em_devvp, (daddr64_t)0, 2048, NOCRED, &bp);
	if (error) {
		if (bp) buf_brelse(bp);
		E4LOG("superblock read failed: %d", error);
		return error;
	}

	memcpy(sb, (char *)buf_dataptr(bp) + EXT4_SUPERBLOCK_OFFSET, sizeof(*sb));
	buf_brelse(bp);

	if (le16(sb->s_magic) != EXT4_SUPER_MAGIC) {
		E4LOG("bad magic 0x%x", le16(sb->s_magic));
		return EINVAL;
	}

	emp->em_log_blocksize = le32(sb->s_log_block_size);
	emp->em_blocksize     = 1024u << emp->em_log_blocksize;
	emp->em_inodes_per_group = le32(sb->s_inodes_per_group);
	emp->em_blocks_per_group = le32(sb->s_blocks_per_group);
	emp->em_first_data_block = le32(sb->s_first_data_block);
	emp->em_feature_incompat = le32(sb->s_feature_incompat);

	if (le32(sb->s_rev_level) == 0) {
		emp->em_inode_size = EXT4_GOOD_OLD_INODE_SIZE;
		emp->em_desc_size  = 32;
	} else {
		emp->em_inode_size = le16(sb->s_inode_size);
		if (emp->em_inode_size == 0)
			emp->em_inode_size = EXT4_GOOD_OLD_INODE_SIZE;
		emp->em_desc_size = le16(sb->s_desc_size);
		if ((emp->em_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) == 0 ||
		    emp->em_desc_size < 32)
			emp->em_desc_size = 32;
	}

	emp->em_blocks_count = le32(sb->s_blocks_count_lo);
	if (emp->em_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		emp->em_blocks_count |= ((uint64_t)le32(sb->s_blocks_count_hi)) << 32;
	emp->em_inodes_count = le32(sb->s_inodes_count);

	if (emp->em_blocks_per_group == 0) {
		E4LOG("zero blocks_per_group");
		return EINVAL;
	}
	emp->em_groups_count = (uint32_t)
	    ((emp->em_blocks_count - emp->em_first_data_block +
	     emp->em_blocks_per_group - 1) / emp->em_blocks_per_group);

	ext4_csum_init(emp);

	E4LOG("mounted: bs=%u ipg=%u bpg=%u isize=%u groups=%u incompat=0x%x "
	    "ro_compat=0x%x csum=%d blocks=%llu",
	    emp->em_blocksize, emp->em_inodes_per_group, emp->em_blocks_per_group,
	    emp->em_inode_size, emp->em_groups_count, emp->em_feature_incompat,
	    emp->em_feature_ro_compat, emp->em_has_metadata_csum,
	    emp->em_blocks_count);
	return 0;
}

int
ext4_write_super(struct ext4mount *emp)
{
	uint64_t pblk = emp->em_first_data_block == 0 ? 0 : emp->em_first_data_block;
	uint32_t off = emp->em_first_data_block == 0 ? EXT4_SUPERBLOCK_OFFSET : 0;
	buf_t bp = NULL;
	int error;

	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;
	memcpy((char *)buf_dataptr(bp) + off, &emp->em_sb, sizeof(emp->em_sb));
	/* The on-disk superblock checksum spans the full 1024-byte block from
	 * `off`; compute it in place on the buffer (which now holds our updated
	 * fields plus the untouched tail read from disk). */
	ext4_superblock_csum_set(emp, (char *)buf_dataptr(bp) + off);
	return ext4_meta_bwrite(emp, bp);
}

uint64_t
ext4_free_blocks_count(struct ext4mount *emp)
{
	uint64_t free_blocks = le32(emp->em_sb.s_free_blocks_count_lo);

	if (emp->em_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		free_blocks |= ((uint64_t)le32(emp->em_sb.s_free_blocks_count_hi)) << 32;
	return free_blocks;
}

/* Read the group descriptor for group `grp` into `gd`. */
int
ext4_read_group_desc(struct ext4mount *emp, uint32_t grp,
    struct ext4_group_desc *gd)
{
	uint64_t gdt_block;
	uint64_t byteoff;
	uint64_t pblk;
	uint32_t off_in_block;
	buf_t bp = NULL;
	int error;

	/* GDT starts at first_data_block + 1 */
	gdt_block = emp->em_first_data_block + 1;
	byteoff   = (uint64_t)grp * emp->em_desc_size;
	pblk      = gdt_block + (byteoff / emp->em_blocksize);
	off_in_block = (uint32_t)(byteoff % emp->em_blocksize);

	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;

	memset(gd, 0, sizeof(*gd));
	memcpy(gd, (char *)buf_dataptr(bp) + off_in_block,
	    emp->em_desc_size < sizeof(*gd) ? emp->em_desc_size : sizeof(*gd));
	buf_brelse(bp);
	return 0;
}

int
ext4_write_group_desc(struct ext4mount *emp, uint32_t grp,
    const struct ext4_group_desc *gd)
{
	uint64_t gdt_block;
	uint64_t byteoff;
	uint64_t pblk;
	uint32_t off_in_block;
	buf_t bp = NULL;
	int error;

	if (grp >= emp->em_groups_count)
		return EINVAL;

	gdt_block = emp->em_first_data_block + 1;
	byteoff   = (uint64_t)grp * emp->em_desc_size;
	pblk      = gdt_block + (byteoff / emp->em_blocksize);
	off_in_block = (uint32_t)(byteoff % emp->em_blocksize);

	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;
	{
		/* recompute bg_checksum over the (updated) descriptor before write */
		struct ext4_group_desc gcopy = *gd;
		uint32_t n = emp->em_desc_size < sizeof(gcopy) ?
		    emp->em_desc_size : (uint32_t)sizeof(gcopy);
		ext4_group_desc_csum_set(emp, grp, &gcopy);
		memcpy((char *)buf_dataptr(bp) + off_in_block, &gcopy, n);
	}
	return ext4_meta_bwrite(emp, bp);
}

uint64_t
ext4_gd_block_bitmap(struct ext4mount *emp, const struct ext4_group_desc *gd)
{
	uint64_t blk = le32(gd->bg_block_bitmap_lo);
	if (emp->em_desc_size >= 64)
		blk |= ((uint64_t)le32(gd->bg_block_bitmap_hi)) << 32;
	return blk;
}

uint64_t
ext4_gd_inode_bitmap(struct ext4mount *emp, const struct ext4_group_desc *gd)
{
	uint64_t blk = le32(gd->bg_inode_bitmap_lo);
	if (emp->em_desc_size >= 64)
		blk |= ((uint64_t)le32(gd->bg_inode_bitmap_hi)) << 32;
	return blk;
}

int
ext4_bitmap_test(const uint8_t *map, uint32_t bit)
{
	return (map[bit >> 3] & (uint8_t)(1u << (bit & 7))) != 0;
}

void
ext4_bitmap_set(uint8_t *map, uint32_t bit)
{
	map[bit >> 3] |= (uint8_t)(1u << (bit & 7));
}

static void
ext4_bitmap_clear(uint8_t *map, uint32_t bit)
{
	map[bit >> 3] &= (uint8_t)~(1u << (bit & 7));
}

static void
ext4_sb_add_free_blocks(struct ext4mount *emp, int64_t delta)
{
	uint64_t free_blocks = le32(emp->em_sb.s_free_blocks_count_lo);
	if (emp->em_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		free_blocks |= ((uint64_t)le32(emp->em_sb.s_free_blocks_count_hi)) << 32;
	free_blocks = (uint64_t)((int64_t)free_blocks + delta);
	emp->em_sb.s_free_blocks_count_lo = le32((uint32_t)free_blocks);
	if (emp->em_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		emp->em_sb.s_free_blocks_count_hi = le32((uint32_t)(free_blocks >> 32));
}

static void
ext4_gd_add_free_blocks(struct ext4mount *emp, struct ext4_group_desc *gd,
    int32_t delta)
{
	uint32_t free_blocks = le16(gd->bg_free_blocks_count_lo);
	if (emp->em_desc_size >= 64)
		free_blocks |= ((uint32_t)le16(gd->bg_free_blocks_count_hi)) << 16;
	free_blocks = (uint32_t)((int32_t)free_blocks + delta);
	gd->bg_free_blocks_count_lo = le16((uint16_t)free_blocks);
	if (emp->em_desc_size >= 64)
		gd->bg_free_blocks_count_hi = le16((uint16_t)(free_blocks >> 16));
}

static void
ext4_gd_add_free_inodes(struct ext4mount *emp, struct ext4_group_desc *gd,
    int32_t delta)
{
	uint32_t free_inodes = le16(gd->bg_free_inodes_count_lo);
	if (emp->em_desc_size >= 64)
		free_inodes |= ((uint32_t)le16(gd->bg_free_inodes_count_hi)) << 16;
	free_inodes = (uint32_t)((int32_t)free_inodes + delta);
	gd->bg_free_inodes_count_lo = le16((uint16_t)free_inodes);
	if (emp->em_desc_size >= 64)
		gd->bg_free_inodes_count_hi = le16((uint16_t)(free_inodes >> 16));
}

static void
ext4_gd_add_used_dirs(struct ext4mount *emp, struct ext4_group_desc *gd,
    int32_t delta)
{
	uint32_t used_dirs = le16(gd->bg_used_dirs_count_lo);
	if (emp->em_desc_size >= 64)
		used_dirs |= ((uint32_t)le16(gd->bg_used_dirs_count_hi)) << 16;
	used_dirs = (uint32_t)((int32_t)used_dirs + delta);
	gd->bg_used_dirs_count_lo = le16((uint16_t)used_dirs);
	if (emp->em_desc_size >= 64)
		gd->bg_used_dirs_count_hi = le16((uint16_t)(used_dirs >> 16));
}

/* Clear a bg_flags bit (e.g. after materializing an uninitialized bitmap). */
static void
ext4_gd_clear_flag(struct ext4_group_desc *gd, uint16_t flag)
{
	gd->bg_flags = le16((uint16_t)(le16(gd->bg_flags) & ~flag));
}

static uint32_t
ext4_gd_itable_unused(struct ext4mount *emp, const struct ext4_group_desc *gd)
{
	uint32_t v = le16(gd->bg_itable_unused_lo);

	if (emp->em_desc_size >= 64)
		v |= ((uint32_t)le16(gd->bg_itable_unused_hi)) << 16;
	return v;
}

static void
ext4_gd_set_itable_unused(struct ext4mount *emp, struct ext4_group_desc *gd,
    uint32_t tail)
{
	gd->bg_itable_unused_lo = le16((uint16_t)(tail & 0xffff));
	if (emp->em_desc_size >= 64)
		gd->bg_itable_unused_hi = le16((uint16_t)(tail >> 16));
}

/* Count the run of never-allocated inodes at the END of a group's table. */
static uint32_t
ext4_itable_unused_from_bitmap(const uint8_t *map, uint32_t limit)
{
	uint32_t tail = 0;

	while (tail < limit && !ext4_bitmap_test(map, limit - tail - 1))
		tail++;
	return tail;
}

/*
 * bg_itable_unused is how many inodes at the END of this group's table have
 * never been written, and fsck skips them.
 */
static void
ext4_gd_note_inode_used(struct ext4mount *emp, struct ext4_group_desc *gd,
    const uint8_t *map)
{
	ext4_gd_set_itable_unused(emp, gd,
	    ext4_itable_unused_from_bitmap(map, emp->em_inodes_per_group));
}

static int
ext4_alloc_inode_locked(struct ext4mount *emp, enum vtype type,
    ino_t *ino_out)
{
	uint32_t first_ino = le32(emp->em_sb.s_first_ino);
	uint32_t grp;

	if (first_ino == 0)
		first_ino = 11;

	for (grp = 0; grp < emp->em_groups_count; grp++) {
		struct ext4_group_desc gd;
		uint64_t bitmap_block;
		buf_t bp = NULL;
		uint8_t *map;
		uint32_t i, limit;
		int error;

		error = ext4_read_group_desc(emp, grp, &gd);
		if (error)
			return error;
		if (le16(gd.bg_free_inodes_count_lo) == 0)
			continue;

		bitmap_block = ext4_gd_inode_bitmap(emp, &gd);
		error = ext4_blkread(emp, bitmap_block, &bp);
		if (error)
			return error;
		map = (uint8_t *)buf_dataptr(bp);
		limit = emp->em_inodes_per_group;
		if ((uint64_t)(grp + 1) * emp->em_inodes_per_group > emp->em_inodes_count)
			limit = (uint32_t)(emp->em_inodes_count -
			    (uint64_t)grp * emp->em_inodes_per_group);

		/* INODE_UNINIT is supposed to mean "this group has never had any
		 * inode allocated, on-disk bitmap bytes are stale/undefined" - by
		 * construction that means bg_free_inodes_count must equal the
		 * group's full inode capacity (limit). If a group is flagged
		 * INODE_UNINIT but its free-inode count says otherwise (some
		 * inodes ARE accounted used - e.g. mke2fs -d populated real files
		 * into this group at image-build time), synthesizing an all-free
		 * bitmap here would hand out inode numbers that are genuinely
		 * still in use, letting a brand new file's ext4_write_inode()
		 * silently overwrite - byte for byte - an existing file's inode
		 * (observed in practice: xkbcomp's inode got clobbered by a fresh
		 * ~35-byte temp file this way). In that inconsistent case, trust
		 * the on-disk bitmap bytes as-is instead of synthesizing - mke2fs
		 * writes real bitmaps for any group it actually populates, so this
		 * is the safe fallback, not a guess. */
		if ((le16(gd.bg_flags) & EXT4_BG_INODE_UNINIT) &&
		    le16(gd.bg_free_inodes_count_lo) == limit) {
			uint32_t nbytes = emp->em_inodes_per_group / 8;
			memset(map, 0, nbytes);
			memset(map + nbytes, 0xff, emp->em_blocksize - nbytes);
		}

		for (i = 0; i < limit; i++) {
			ino_t ino = (ino_t)((uint64_t)grp * emp->em_inodes_per_group + i + 1);
			if (ino < first_ino || ext4_bitmap_test(map, i))
				continue;
			if (ext4_ino_is_live(emp, ino))
				continue;
			ext4_bitmap_set(map, i);
			ext4_inode_bitmap_csum_set(emp, &gd, map);
			error = ext4_meta_bwrite(emp, bp);
			if (error)
				return error;
			ext4_gd_add_free_inodes(emp, &gd, -1);
			if (type == VDIR)
				ext4_gd_add_used_dirs(emp, &gd, 1);
			ext4_gd_clear_flag(&gd, EXT4_BG_INODE_UNINIT);
			ext4_gd_note_inode_used(emp, &gd, map);
			error = ext4_write_group_desc(emp, grp, &gd);
			if (error)
				return error;
			emp->em_sb.s_free_inodes_count =
			    le32(le32(emp->em_sb.s_free_inodes_count) - 1);
			error = ext4_write_super(emp);
			if (error)
				return error;
			emp->em_stats.alloc_inodes++;
			*ino_out = ino;
			return 0;
		}
		buf_brelse(bp);
	}
	return ENOSPC;
}

int
ext4_repair_itable_unused(struct ext4mount *emp)
{
	uint32_t grp;
	uint32_t repaired = 0;

	for (grp = 0; grp < emp->em_groups_count; grp++) {
		struct ext4_group_desc gd;
		buf_t bp = NULL;
		uint32_t limit, tail, cur;
		int error;

		error = ext4_read_group_desc(emp, grp, &gd);
		if (error)
			return error;

		limit = emp->em_inodes_per_group;
		if ((uint64_t)(grp + 1) * emp->em_inodes_per_group >
		    emp->em_inodes_count)
			limit = (uint32_t)(emp->em_inodes_count -
			    (uint64_t)grp * emp->em_inodes_per_group);

		/* A group flagged INODE_UNINIT with every inode still free has
		 * never been allocated into, so its on-disk bitmap bytes are
		 * undefined and must not be read - the whole table is unused. */
		if ((le16(gd.bg_flags) & EXT4_BG_INODE_UNINIT) &&
		    le16(gd.bg_free_inodes_count_lo) == limit) {
			tail = limit;
		} else {
			error = ext4_blkread(emp, ext4_gd_inode_bitmap(emp, &gd),
			    &bp);
			if (error)
				return error;
			tail = ext4_itable_unused_from_bitmap(
			    (const uint8_t *)buf_dataptr(bp), limit);
			buf_brelse(bp);
		}

		cur = ext4_gd_itable_unused(emp, &gd);
		if (tail == cur)
			continue;

		ext4_gd_set_itable_unused(emp, &gd, tail);
		error = ext4_write_group_desc(emp, grp, &gd);
		if (error)
			return error;
		repaired++;
	}

	if (repaired != 0)
		E4LOG("repaired bg_itable_unused in %u group(s)", repaired);

	return 0;
}

int
ext4_alloc_inode(struct ext4mount *emp, enum vtype type, ino_t *ino_out)
{
	IOLock *lock;
	int error;

	if (emp == NULL || ino_out == NULL)
		return EINVAL;

	lock = (IOLock *)emp->em_alloc_lock;
	if (lock == NULL)
		return EIO;

	IOLockLock(lock);
	error = ext4_alloc_inode_locked(emp, type, ino_out);
	IOLockUnlock(lock);

	return error;
}

static int
ext4_free_inode_locked(struct ext4mount *emp, ino_t ino, enum vtype type)
{
	struct ext4_group_desc gd;
	uint32_t grp, index;
	uint64_t bitmap_block;
	buf_t bp = NULL;
	uint8_t *map;
	int error;

	if (ino == 0 || ino > emp->em_inodes_count)
		return EINVAL;
	grp = (uint32_t)((ino - 1) / emp->em_inodes_per_group);
	index = (uint32_t)((ino - 1) % emp->em_inodes_per_group);
	error = ext4_read_group_desc(emp, grp, &gd);
	if (error)
		return error;
	bitmap_block = ext4_gd_inode_bitmap(emp, &gd);
	error = ext4_blkread(emp, bitmap_block, &bp);
	if (error)
		return error;
	map = (uint8_t *)buf_dataptr(bp);
	if (!ext4_bitmap_test(map, index)) {
		buf_brelse(bp);
		return 0;
	}
	ext4_bitmap_clear(map, index);
	ext4_inode_bitmap_csum_set(emp, &gd, map);
	error = ext4_meta_bwrite(emp, bp);
	if (error)
		return error;
	ext4_gd_add_free_inodes(emp, &gd, 1);
	if (type == VDIR && le16(gd.bg_used_dirs_count_lo) > 0)
		ext4_gd_add_used_dirs(emp, &gd, -1);
	error = ext4_write_group_desc(emp, grp, &gd);
	if (error)
		return error;
	emp->em_sb.s_free_inodes_count =
	    le32(le32(emp->em_sb.s_free_inodes_count) + 1);
	emp->em_stats.free_inodes++;
	return ext4_write_super(emp);
}

int
ext4_free_inode(struct ext4mount *emp, ino_t ino, enum vtype type)
{
	IOLock *lock;
	int error;

	if (emp == NULL)
		return EINVAL;

	lock = (IOLock *)emp->em_alloc_lock;
	if (lock == NULL)
		return EIO;

	IOLockLock(lock);
	error = ext4_free_inode_locked(emp, ino, type);
	IOLockUnlock(lock);

	return error;
}

static int
ext4_group_meta_init(struct ext4mount *emp)
{
	struct e4_group_meta *gm;
	uint32_t grp;

	if (emp->em_group_meta != NULL)
		return 0;
	gm = (struct e4_group_meta *)_MALLOC(
	    sizeof(*gm) * emp->em_groups_count, M_TEMP, M_WAITOK | M_ZERO);
	if (gm == NULL)
		return ENOMEM;
	for (grp = 0; grp < emp->em_groups_count; grp++) {
		struct ext4_group_desc gd;
		if (ext4_read_group_desc(emp, grp, &gd) != 0) {
			_FREE(gm, M_TEMP);
			return EIO;
		}
		gm[grp].bbmp = ext4_gd_block_bitmap(emp, &gd);
		gm[grp].ibmp = ext4_gd_inode_bitmap(emp, &gd);
		gm[grp].itbl = le32(gd.bg_inode_table_lo);
		if (emp->em_desc_size >= 64)
			gm[grp].itbl |=
			    ((uint64_t)le32(gd.bg_inode_table_hi)) << 32;
	}
	emp->em_group_meta = gm;
	return 0;
}

static int
ext4_block_is_metadata(struct ext4mount *emp, uint64_t pblk)
{
	uint32_t grp;

	if (pblk < emp->em_first_data_block || pblk >= emp->em_blocks_count)
		return 1;
	if (ext4_group_meta_init(emp) != 0)
		return 1;   /* can't tell: refuse, caller skips the block */

	for (grp = 0; grp < emp->em_groups_count; grp++) {
		const struct e4_group_meta *gm = &emp->em_group_meta[grp];
		uint64_t group_first = emp->em_first_data_block +
		    (uint64_t)grp * emp->em_blocks_per_group;

		if (ext4_group_has_super(emp, grp)) {
			uint32_t n = 1 + emp->em_gdt_blocks +
			    emp->em_reserved_gdt_blocks;
			if (pblk >= group_first && pblk < group_first + n)
				return 1;
		}
		if (pblk == gm->bbmp || pblk == gm->ibmp)
			return 1;
		if (pblk >= gm->itbl && pblk < gm->itbl + emp->em_itable_blocks)
			return 1;
	}
	return 0;
}

static int
ext4_alloc_block_locked(struct ext4mount *emp, uint64_t goal,
    uint64_t *pblk_out)
{
	uint32_t start_grp = 0;
	uint32_t pass;

	if (goal >= emp->em_first_data_block && goal < emp->em_blocks_count) {
		start_grp = (uint32_t)((goal - emp->em_first_data_block) /
		    emp->em_blocks_per_group);
		if (start_grp >= emp->em_groups_count)
			start_grp = 0;
	}

	for (pass = 0; pass < emp->em_groups_count; pass++) {
		uint32_t grp = (start_grp + pass) % emp->em_groups_count;
		uint64_t group_first = emp->em_first_data_block +
		    (uint64_t)grp * emp->em_blocks_per_group;
		uint32_t limit = emp->em_blocks_per_group;
		struct ext4_group_desc gd;
		uint64_t bitmap_block;
		buf_t bp = NULL;
		uint8_t *map;
		uint32_t i;
		int error;

		if (group_first >= emp->em_blocks_count)
			continue;
		if (group_first + limit > emp->em_blocks_count)
			limit = (uint32_t)(emp->em_blocks_count - group_first);

		error = ext4_read_group_desc(emp, grp, &gd);
		if (error)
			return error;
		if (le16(gd.bg_free_blocks_count_lo) == 0)
			continue;

		bitmap_block = ext4_gd_block_bitmap(emp, &gd);
		error = ext4_blkread(emp, bitmap_block, &bp);
		if (error)
			return error;
		map = (uint8_t *)buf_dataptr(bp);

		if (le16(gd.bg_flags) & EXT4_BG_BLOCK_UNINIT) {
			uint32_t synth_free = 0;
			if (ext4_init_block_bitmap(emp, grp, &gd, map, &synth_free) != 0) {
				buf_brelse(bp);
				continue;
			}
		}

		for (i = 0; i < limit; i++) {
			uint64_t pblk = group_first + i;
			buf_t zbp = NULL;

			if (pblk < emp->em_first_data_block || ext4_bitmap_test(map, i))
				continue;
			if (ext4_block_is_metadata(emp, pblk)) {
				E4LOG("alloc_block: refusing metadata pblk=%llu "
				    "grp=%u bit=%u", (unsigned long long)pblk,
				    grp, i);
				continue;
			}
			ext4_bitmap_set(map, i);
			ext4_block_bitmap_csum_set(emp, &gd, map);
			error = ext4_meta_bwrite(emp, bp);
			if (error)
				return error;
			ext4_gd_add_free_blocks(emp, &gd, -1);
			ext4_gd_clear_flag(&gd, EXT4_BG_BLOCK_UNINIT);
			error = ext4_write_group_desc(emp, grp, &gd);
			if (error)
				return error;
			ext4_sb_add_free_blocks(emp, -1);
			error = ext4_write_super(emp);
			if (error)
				return error;
			error = ext4_blkread(emp, pblk, &zbp);
			if (error)
				return error;
			memset((void *)buf_dataptr(zbp), 0, emp->em_blocksize);
			/* MUST be write-through, not journaled: freshly allocated
			 * blocks are often filled with file DATA right after via
			 * plain (unjournaled) writes. If this zero-fill sat in
			 * the open transaction, commit's checkpoint would replay
			 * the stale zeroed copy over the just-written data -
			 * observed as /tmp/server-0.xkm reading back as zeros.
			 * A block that becomes metadata gets its real contents
			 * journaled by its own later ext4_meta_bwrite. */
			error = buf_bwrite(zbp);
			if (error)
				return error;
			emp->em_stats.alloc_blocks++;
			*pblk_out = pblk;
			return 0;
		}
		buf_brelse(bp);
	}
	return ENOSPC;
}

int
ext4_alloc_block(struct ext4mount *emp, uint64_t goal, uint64_t *pblk_out)
{
	IOLock *lock;
	int error;

	if (emp == NULL || pblk_out == NULL)
		return EINVAL;

	lock = (IOLock *)emp->em_alloc_lock;
	if (lock == NULL)
		return EIO;

	IOLockLock(lock);
	error = ext4_alloc_block_locked(emp, goal, pblk_out);
	IOLockUnlock(lock);

	return error;
}

static int
ext4_free_block_locked(struct ext4mount *emp, uint64_t pblk)
{
	struct ext4_group_desc gd;
	uint32_t grp, index;
	uint64_t group_first, bitmap_block;
	buf_t bp = NULL;
	uint8_t *map;
	int error;

	if (pblk < emp->em_first_data_block || pblk >= emp->em_blocks_count)
		return EINVAL;
	grp = (uint32_t)((pblk - emp->em_first_data_block) / emp->em_blocks_per_group);
	if (grp >= emp->em_groups_count)
		return EINVAL;
	group_first = emp->em_first_data_block + (uint64_t)grp * emp->em_blocks_per_group;
	index = (uint32_t)(pblk - group_first);

	error = ext4_read_group_desc(emp, grp, &gd);
	if (error)
		return error;
	bitmap_block = ext4_gd_block_bitmap(emp, &gd);
	error = ext4_blkread(emp, bitmap_block, &bp);
	if (error)
		return error;
	map = (uint8_t *)buf_dataptr(bp);
	if (!ext4_bitmap_test(map, index)) {
		buf_brelse(bp);
		return 0;
	}
	ext4_bitmap_clear(map, index);
	ext4_block_bitmap_csum_set(emp, &gd, map);
	error = ext4_meta_bwrite(emp, bp);
	if (error)
		return error;
	ext4_gd_add_free_blocks(emp, &gd, 1);
	error = ext4_write_group_desc(emp, grp, &gd);
	if (error)
		return error;
	ext4_sb_add_free_blocks(emp, 1);
	emp->em_stats.free_blocks++;
	return ext4_write_super(emp);
}

int
ext4_free_block(struct ext4mount *emp, uint64_t pblk)
{
	IOLock *lock;
	int error;

	if (emp == NULL)
		return EINVAL;

	lock = (IOLock *)emp->em_alloc_lock;
	if (lock == NULL)
		return EIO;

	IOLockLock(lock);
	error = ext4_free_block_locked(emp, pblk);
	IOLockUnlock(lock);

	return error;
}

/* Entry capacity of a full-block extent node (extent and extent_idx are both
 * 12 bytes, so leaves and index nodes hold the same count). */
static uint16_t
ext4_ext_block_max(struct ext4mount *emp)
{
	return (uint16_t)((emp->em_blocksize - sizeof(struct ext4_extent_header)) /
	    sizeof(struct ext4_extent));
}

/* Entry capacity of the inline root that lives in i_block[] (60 bytes). */
static uint16_t
ext4_ext_root_max(void)
{
	return (uint16_t)((sizeof(((struct ext4_inode *)0)->i_block) -
	    sizeof(struct ext4_extent_header)) / sizeof(struct ext4_extent));
}

/* Count one freshly-allocated tree metadata block against the inode. */
static void
ext4_ext_account_meta(struct ext4mount *emp, struct ext4_inode *inode)
{
	inode->i_blocks_lo = le32(le32(inode->i_blocks_lo) +
	    (emp->em_blocksize / 512));
}

static int
ext4_ext_read_block(struct ext4mount *emp, uint64_t blk, char *out)
{
	buf_t bp = NULL;
	int error = ext4_blkread(emp, blk, &bp);
	if (error)
		return error;
	memcpy(out, (char *)buf_dataptr(bp), emp->em_blocksize);
	buf_brelse(bp);
	return 0;
}

static int
ext4_ext_write_block(struct ext4mount *emp, ino_t ino,
    const struct ext4_inode *inode, uint64_t blk, const char *data)
{
	buf_t bp = NULL;
	int error = ext4_blkread(emp, blk, &bp);
	if (error)
		return error;
	memcpy((char *)buf_dataptr(bp), data, emp->em_blocksize);
	ext4_extent_block_csum_set(emp, ino, inode, (void *)buf_dataptr(bp));
	return ext4_meta_bwrite(emp, bp);
}

/* Allocate a fresh right-sibling leaf holding the single extent lblk->pblk. */
static int
ext4_ext_new_leaf(struct ext4mount *emp, ino_t ino, struct ext4_inode *inode,
    uint32_t lblk, uint64_t pblk, uint64_t *out_blk, uint32_t *out_lblk)
{
	struct ext4_extent_header *nh;
	struct ext4_extent *ex;
	uint64_t nb = 0;
	char *buf;
	int error;

	error = ext4_alloc_block(emp, 0, &nb);
	if (error)
		return error;
	buf = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
	if (buf == NULL) {
		(void)ext4_free_block(emp, nb);
		return ENOMEM;
	}
	memset(buf, 0, emp->em_blocksize);
	nh = (struct ext4_extent_header *)buf;
	nh->eh_magic = le16(EXT4_EXT_MAGIC);
	nh->eh_entries = le16(1);
	nh->eh_max = le16(ext4_ext_block_max(emp));
	nh->eh_depth = 0;
	ex = (struct ext4_extent *)(nh + 1);
	ex[0].ee_block = le32(lblk);
	ex[0].ee_len = le16(1);
	ex[0].ee_start_hi = le16((uint16_t)(pblk >> 32));
	ex[0].ee_start_lo = le32((uint32_t)pblk);
	error = ext4_ext_write_block(emp, ino, inode, nb, buf);
	_FREE(buf, M_TEMP);
	if (error) {
		(void)ext4_free_block(emp, nb);
		return error;
	}
	ext4_ext_account_meta(emp, inode);
	*out_blk = nb;
	*out_lblk = lblk;
	return 0;
}

/* Allocate a fresh right-sibling index node (at `depth`) holding one entry
 * covering child_lblk via child_blk. */
static int
ext4_ext_new_index(struct ext4mount *emp, ino_t ino, struct ext4_inode *inode,
    uint16_t depth, uint32_t child_lblk, uint64_t child_blk,
    uint64_t *out_blk, uint32_t *out_lblk)
{
	struct ext4_extent_header *nh;
	struct ext4_extent_idx *ix;
	uint64_t nb = 0;
	char *buf;
	int error;

	error = ext4_alloc_block(emp, 0, &nb);
	if (error)
		return error;
	buf = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
	if (buf == NULL) {
		(void)ext4_free_block(emp, nb);
		return ENOMEM;
	}
	memset(buf, 0, emp->em_blocksize);
	nh = (struct ext4_extent_header *)buf;
	nh->eh_magic = le16(EXT4_EXT_MAGIC);
	nh->eh_entries = le16(1);
	nh->eh_max = le16(ext4_ext_block_max(emp));
	nh->eh_depth = le16(depth);
	ix = (struct ext4_extent_idx *)(nh + 1);
	ix[0].ei_block = le32(child_lblk);
	ix[0].ei_leaf_lo = le32((uint32_t)child_blk);
	ix[0].ei_leaf_hi = le16((uint16_t)(child_blk >> 32));
	ix[0].ei_unused = 0;
	error = ext4_ext_write_block(emp, ino, inode, nb, buf);
	_FREE(buf, M_TEMP);
	if (error) {
		(void)ext4_free_block(emp, nb);
		return error;
	}
	ext4_ext_account_meta(emp, inode);
	*out_blk = nb;
	*out_lblk = child_lblk;
	return 0;
}

/*
 * Insert (lblk -> pblk, one block) at the rightmost position of the extent
 * (sub)tree node `node` (capacity `node_max` entries). Files only ever grow at
 * the tail here, so we always descend the rightmost path and never split in the
 * middle.
 *   *did_split == 0: entry absorbed (caller writes the node back if on disk).
 *   *did_split == 1: node was full; a fresh right sibling was allocated at
 *                    *split_blk covering *split_lblk, for the caller to index.
 */
static int
ext4_ext_insert(struct ext4mount *emp, ino_t ino, struct ext4_inode *inode,
    char *node, uint16_t node_max, uint32_t lblk, uint64_t pblk,
    int *did_split, uint64_t *split_blk, uint32_t *split_lblk)
{
	struct ext4_extent_header *eh = (struct ext4_extent_header *)node;
	uint16_t entries = le16(eh->eh_entries);
	uint16_t depth = le16(eh->eh_depth);

	*did_split = 0;

	if (depth == 0) {
		struct ext4_extent *ex = (struct ext4_extent *)(eh + 1);
		uint16_t i;

		for (i = 0; i < entries; i++) {
			if (le32(ex[i].ee_block) > lblk)
				break;
		}

		/* Grow the extent this block continues, if any. */
		if (i > 0) {
			struct ext4_extent *prev = &ex[i - 1];
			uint32_t pfirst = le32(prev->ee_block);
			uint16_t plen = le16(prev->ee_len);
			uint64_t pstart = le32(prev->ee_start_lo) |
			    ((uint64_t)le16(prev->ee_start_hi) << 32);

			if (lblk < pfirst + plen)
				return 0;   /* already mapped by prev */
			if (plen < 32768 && lblk == pfirst + plen &&
			    pblk == pstart + plen) {
				prev->ee_len = le16((uint16_t)(plen + 1));
				return 0;
			}
		}

		/* Or prepend to the one it runs into. */
		if (i < entries) {
			struct ext4_extent *next = &ex[i];
			uint32_t nfirst = le32(next->ee_block);
			uint16_t nlen = le16(next->ee_len);
			uint64_t nstart = le32(next->ee_start_lo) |
			    ((uint64_t)le16(next->ee_start_hi) << 32);

			if (nlen < 32768 && lblk + 1 == nfirst &&
			    pblk + 1 == nstart) {
				next->ee_block = le32(lblk);
				next->ee_start_hi = le16((uint16_t)(pblk >> 32));
				next->ee_start_lo = le32((uint32_t)pblk);
				next->ee_len = le16((uint16_t)(nlen + 1));
				return 0;
			}
		}

		if (entries < node_max) {
			if (i < entries)
				memmove(&ex[i + 1], &ex[i],
				    (size_t)(entries - i) * sizeof(*ex));
			ex[i].ee_block = le32(lblk);
			ex[i].ee_len = le16(1);
			ex[i].ee_start_hi = le16((uint16_t)(pblk >> 32));
			ex[i].ee_start_lo = le32((uint32_t)pblk);
			eh->eh_entries = le16((uint16_t)(entries + 1));
			return 0;
		}
		/* A full node can only be split on its right edge here. */
		if (i < entries)
			return ENOSPC;
		*did_split = 1;
		return ext4_ext_new_leaf(emp, ino, inode, lblk, pblk,
		    split_blk, split_lblk);
	} else {
		struct ext4_extent_idx *ix = (struct ext4_extent_idx *)(eh + 1);
		uint64_t child_blk;
		char *child;
		int cs = 0;
		uint64_t cs_blk = 0;
		uint32_t cs_lblk = 0;
		int error;

		uint16_t ci;

		if (entries == 0)
			return EIO;
		for (ci = entries - 1; ci > 0; ci--) {
			if (le32(ix[ci].ei_block) <= lblk)
				break;
		}
		child_blk = le32(ix[ci].ei_leaf_lo) |
		    ((uint64_t)le16(ix[ci].ei_leaf_hi) << 32);

		child = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
		if (child == NULL)
			return ENOMEM;
		error = ext4_ext_read_block(emp, child_blk, child);
		if (error) {
			_FREE(child, M_TEMP);
			return error;
		}
		error = ext4_ext_insert(emp, ino, inode, child,
		    ext4_ext_block_max(emp), lblk, pblk, &cs, &cs_blk, &cs_lblk);
		if (error) {
			_FREE(child, M_TEMP);
			return error;
		}
		error = ext4_ext_write_block(emp, ino, inode, child_blk, child);
		_FREE(child, M_TEMP);
		if (error)
			return error;

		if (!cs)
			return 0;

		if (entries < node_max) {
			uint16_t ii;

			/* Keep indices sorted for the same reason the leaves are. */
			for (ii = 0; ii < entries; ii++) {
				if (le32(ix[ii].ei_block) > cs_lblk)
					break;
			}
			if (ii < entries)
				memmove(&ix[ii + 1], &ix[ii],
				    (size_t)(entries - ii) * sizeof(*ix));
			ix[ii].ei_block = le32(cs_lblk);
			ix[ii].ei_leaf_lo = le32((uint32_t)cs_blk);
			ix[ii].ei_leaf_hi = le16((uint16_t)(cs_blk >> 32));
			ix[ii].ei_unused = 0;
			eh->eh_entries = le16((uint16_t)(entries + 1));
			return 0;
		}
		*did_split = 1;
		return ext4_ext_new_index(emp, ino, inode, depth, cs_lblk, cs_blk,
		    split_blk, split_lblk);
	}
}

/* The inline root filled up: push its contents into a new block and turn the
 * root into a one-deeper index node with two children (old contents + the new
 * right sibling). */
static int
ext4_ext_grow_root(struct ext4mount *emp, ino_t ino, struct ext4_inode *inode,
    uint64_t sib_blk, uint32_t sib_lblk)
{
	struct ext4_extent_header *reh =
	    (struct ext4_extent_header *)inode->i_block;
	uint16_t root_entries = le16(reh->eh_entries);
	uint16_t root_depth = le16(reh->eh_depth);
	struct ext4_extent_idx *rix;
	uint32_t old_first;
	uint64_t ob = 0;
	char *buf;
	int error;

	if (root_depth == 0)
		old_first = le32(((struct ext4_extent *)(reh + 1))[0].ee_block);
	else
		old_first = le32(((struct ext4_extent_idx *)(reh + 1))[0].ei_block);

	error = ext4_alloc_block(emp, 0, &ob);
	if (error)
		return error;
	buf = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
	if (buf == NULL) {
		(void)ext4_free_block(emp, ob);
		return ENOMEM;
	}
	memset(buf, 0, emp->em_blocksize);
	memcpy(buf, inode->i_block, sizeof(struct ext4_extent_header) +
	    (size_t)root_entries * sizeof(struct ext4_extent));
	((struct ext4_extent_header *)buf)->eh_max = le16(ext4_ext_block_max(emp));
	error = ext4_ext_write_block(emp, ino, inode, ob, buf);
	_FREE(buf, M_TEMP);
	if (error) {
		(void)ext4_free_block(emp, ob);
		return error;
	}
	ext4_ext_account_meta(emp, inode);

	memset(inode->i_block, 0, sizeof(inode->i_block));
	reh = (struct ext4_extent_header *)inode->i_block;
	reh->eh_magic = le16(EXT4_EXT_MAGIC);
	reh->eh_entries = le16(2);
	reh->eh_max = le16(ext4_ext_root_max());
	reh->eh_depth = le16((uint16_t)(root_depth + 1));
	reh->eh_generation = 0;
	rix = (struct ext4_extent_idx *)(reh + 1);
	rix[0].ei_block = le32(old_first);
	rix[0].ei_leaf_lo = le32((uint32_t)ob);
	rix[0].ei_leaf_hi = le16((uint16_t)(ob >> 32));
	rix[0].ei_unused = 0;
	rix[1].ei_block = le32(sib_lblk);
	rix[1].ei_leaf_lo = le32((uint32_t)sib_blk);
	rix[1].ei_leaf_hi = le16((uint16_t)(sib_blk >> 32));
	rix[1].ei_unused = 0;
	return 0;
}

/*
 * Give an extents inode a valid, empty extent tree.
 *
 * ext4_inode_free_extents() deliberately zeroes i_block when it releases the
 * last extent, and ext4_inode_append_extent() re-initialises the header on its
 * next call - so any path that grows a file *without* allocating a block (a
 * sparse ftruncate) has to do it itself, or the inode is left with a zeroed
 * header that ext4_bmap() rightly refuses ("bad extent magic 0x0").
 */
void
ext4_inode_init_extent_header(struct ext4_inode *inode)
{
	struct ext4_extent_header *eh =
	    (struct ext4_extent_header *)inode->i_block;

	if ((le32(inode->i_flags) & EXT4_EXTENTS_FL) != 0 &&
	    le16(eh->eh_magic) == EXT4_EXT_MAGIC)
		return;

	memset(inode->i_block, 0, sizeof(inode->i_block));
	inode->i_flags = le32(le32(inode->i_flags) | EXT4_EXTENTS_FL);
	eh->eh_magic = le16(EXT4_EXT_MAGIC);
	eh->eh_entries = 0;
	eh->eh_max = le16(ext4_ext_root_max());
	eh->eh_depth = 0;
	eh->eh_generation = 0;
}

static int
ext4_ext_deepen_root(struct ext4mount *emp, ino_t ino, struct ext4_inode *inode)
{
	struct ext4_extent_header *reh =
	    (struct ext4_extent_header *)inode->i_block;
	uint16_t root_entries = le16(reh->eh_entries);
	uint16_t root_depth = le16(reh->eh_depth);
	struct ext4_extent_idx *rix;
	uint32_t old_first;
	uint64_t ob = 0;
	char *buf;
	int error;

	if (root_entries == 0)
		return EINVAL;
	if (root_depth == 0)
		old_first = le32(((struct ext4_extent *)(reh + 1))[0].ee_block);
	else
		old_first = le32(((struct ext4_extent_idx *)(reh + 1))[0].ei_block);

	error = ext4_alloc_block(emp, 0, &ob);
	if (error)
		return error;
	buf = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
	if (buf == NULL) {
		(void)ext4_free_block(emp, ob);
		return ENOMEM;
	}
	memset(buf, 0, emp->em_blocksize);
	memcpy(buf, inode->i_block, sizeof(struct ext4_extent_header) +
	    (size_t)root_entries * sizeof(struct ext4_extent));
	((struct ext4_extent_header *)buf)->eh_max = le16(ext4_ext_block_max(emp));
	error = ext4_ext_write_block(emp, ino, inode, ob, buf);
	_FREE(buf, M_TEMP);
	if (error) {
		(void)ext4_free_block(emp, ob);
		return error;
	}
	ext4_ext_account_meta(emp, inode);

	memset(inode->i_block, 0, sizeof(inode->i_block));
	reh = (struct ext4_extent_header *)inode->i_block;
	reh->eh_magic = le16(EXT4_EXT_MAGIC);
	reh->eh_entries = le16(1);
	reh->eh_max = le16(ext4_ext_root_max());
	reh->eh_depth = le16((uint16_t)(root_depth + 1));
	reh->eh_generation = 0;
	rix = (struct ext4_extent_idx *)(reh + 1);
	rix[0].ei_block = le32(old_first);
	rix[0].ei_leaf_lo = le32((uint32_t)ob);
	rix[0].ei_leaf_hi = le16((uint16_t)(ob >> 32));
	rix[0].ei_unused = 0;
	return 0;
}

int
ext4_inode_append_extent(struct ext4mount *emp, ino_t ino,
    struct ext4_inode *inode, uint32_t lblk, uint64_t pblk)
{
	struct ext4_extent_header *eh =
	    (struct ext4_extent_header *)inode->i_block;
	int did_split = 0;
	uint64_t split_blk = 0;
	uint32_t split_lblk = 0;
	int error;

	if ((le32(inode->i_flags) & EXT4_EXTENTS_FL) == 0 ||
	    le16(eh->eh_magic) != EXT4_EXT_MAGIC) {
		memset(inode->i_block, 0, sizeof(inode->i_block));
		inode->i_flags = le32(le32(inode->i_flags) | EXT4_EXTENTS_FL);
		eh->eh_magic = le16(EXT4_EXT_MAGIC);
		eh->eh_entries = 0;
		eh->eh_max = le16(ext4_ext_root_max());
		eh->eh_depth = 0;
		eh->eh_generation = 0;
	}

	error = ext4_ext_insert(emp, ino, inode, (char *)inode->i_block,
	    ext4_ext_root_max(), lblk, pblk, &did_split, &split_blk, &split_lblk);
	if (error == ENOSPC) {
		error = ext4_ext_deepen_root(emp, ino, inode);
		if (error)
			return error;
		error = ext4_ext_insert(emp, ino, inode, (char *)inode->i_block,
		    ext4_ext_root_max(), lblk, pblk, &did_split, &split_blk,
		    &split_lblk);
	}
	if (error)
		return error;
	if (!did_split)
		return 0;

	/* Rightmost path filled all the way up to the inline root: deepen it. */
	return ext4_ext_grow_root(emp, ino, inode, split_blk, split_lblk);
}

/*
 * Free every data block referenced by the extent (sub)tree node `node`, plus,
 * for index nodes, the child node blocks themselves (depth-first). Does NOT
 * free the block holding `node` - that is the caller's job (the inline root has
 * no block of its own).
 */
static int
ext4_ext_free_subtree(struct ext4mount *emp, char *node)
{
	struct ext4_extent_header *eh = (struct ext4_extent_header *)node;
	uint16_t entries, depth, i;
	int error;

	if (le16(eh->eh_magic) != EXT4_EXT_MAGIC)
		return EIO;
	entries = le16(eh->eh_entries);
	depth = le16(eh->eh_depth);

	if (depth == 0) {
		struct ext4_extent *ex = (struct ext4_extent *)(eh + 1);
		for (i = 0; i < entries; i++) {
			uint16_t j, len = le16(ex[i].ee_len);
			uint64_t start = le32(ex[i].ee_start_lo) |
			    ((uint64_t)le16(ex[i].ee_start_hi) << 32);
			if (len > 32768)
				len -= 32768;
			for (j = 0; j < len; j++) {
				error = ext4_free_block(emp, start + j);
				if (error)
					return error;
			}
		}
		return 0;
	}

	{
		struct ext4_extent_idx *ix = (struct ext4_extent_idx *)(eh + 1);
		for (i = 0; i < entries; i++) {
			uint64_t child_blk = le32(ix[i].ei_leaf_lo) |
			    ((uint64_t)le16(ix[i].ei_leaf_hi) << 32);
			char *child = (char *)_MALLOC(emp->em_blocksize, M_TEMP,
			    M_WAITOK);
			if (child == NULL)
				return ENOMEM;
			error = ext4_ext_read_block(emp, child_blk, child);
			if (error == 0)
				error = ext4_ext_free_subtree(emp, child);
			_FREE(child, M_TEMP);
			if (error)
				return error;
			error = ext4_free_block(emp, child_blk);
			if (error)
				return error;
		}
		return 0;
	}
}

int
ext4_inode_free_extents(struct ext4mount *emp, struct ext4_inode *inode)
{
	struct ext4_extent_header *eh =
	    (struct ext4_extent_header *)inode->i_block;
	int error;

	if ((le32(inode->i_flags) & EXT4_EXTENTS_FL) == 0)
		return 0;
	/*
	 * magic == 0 means this inode's extents were already freed (this
	 * function itself zeroes i_block on the way out, below). Callers can
	 * legitimately reach here twice for the same inode - e.g.
	 * ext4_vnop_remove()'s stale-vnode fallback racing the primary
	 * ext4_drop_inode() path - so treat "already empty" as success
	 * instead of EIO; only a genuinely non-zero, non-magic header is
	 * real corruption.
	 */
	if (le16(eh->eh_magic) == 0)
		return 0;
	if (le16(eh->eh_magic) != EXT4_EXT_MAGIC)
		return EIO;

	error = ext4_ext_free_subtree(emp, (char *)inode->i_block);
	if (error)
		return error;

	memset(inode->i_block, 0, sizeof(inode->i_block));
	inode->i_blocks_lo = 0;
	inode->i_size_lo = 0;
	inode->i_size_high = 0;
	return 0;
}

int
ext4_inode_truncate_extents(struct ext4mount *emp, struct ext4_inode *inode,
    uint64_t keep_blocks)
{
	struct ext4_extent_header *eh =
	    (struct ext4_extent_header *)inode->i_block;
	struct ext4_extent *ex = (struct ext4_extent *)(eh + 1);
	uint16_t entries, depth;
	int error = 0;

	if ((le32(inode->i_flags) & EXT4_EXTENTS_FL) == 0)
		return keep_blocks == 0 ? 0 : EFBIG;
	if (le16(eh->eh_magic) != EXT4_EXT_MAGIC)
		return EIO;
	depth = le16(eh->eh_depth);
	if (depth != 0) {
		/* Truncating a multi-level tree to empty is just a full free +
		 * reset; partial shrink of a deep tree isn't supported yet. */
		if (keep_blocks != 0)
			return EFBIG;
		error = ext4_inode_free_extents(emp, inode);
		if (error)
			return error;
		eh = (struct ext4_extent_header *)inode->i_block;
		eh->eh_magic = le16(EXT4_EXT_MAGIC);
		eh->eh_entries = 0;
		eh->eh_max = le16(ext4_ext_root_max());
		eh->eh_depth = 0;
		eh->eh_generation = 0;
		inode->i_flags = le32(le32(inode->i_flags) | EXT4_EXTENTS_FL);
		return 0;
	}

	entries = le16(eh->eh_entries);
	while (entries > 0) {
		struct ext4_extent *last = &ex[entries - 1];
		uint32_t first = le32(last->ee_block);
		uint16_t raw_len = le16(last->ee_len);
		uint16_t len = raw_len;
		uint64_t start = le32(last->ee_start_lo) |
		    ((uint64_t)le16(last->ee_start_hi) << 32);
		uint64_t end;

		if (len > 32768)
			len -= 32768;
		end = (uint64_t)first + len;
		if (end <= keep_blocks)
			break;

		if ((uint64_t)first >= keep_blocks) {
			uint16_t j;
			for (j = 0; j < len; j++) {
				error = ext4_free_block(emp, start + j);
				if (error)
					return error;
			}
			entries--;
			eh->eh_entries = le16(entries);
			continue;
		}

		{
			uint16_t new_len = (uint16_t)(keep_blocks - first);
			uint16_t j;
			for (j = new_len; j < len; j++) {
				error = ext4_free_block(emp, start + j);
				if (error)
					return error;
			}
			last->ee_len = le16(new_len);
		}
		break;
	}

	if (keep_blocks == 0) {
		uint16_t max = le16(eh->eh_max);
		memset(inode->i_block, 0, sizeof(inode->i_block));
		eh = (struct ext4_extent_header *)inode->i_block;
		eh->eh_magic = le16(EXT4_EXT_MAGIC);
		eh->eh_entries = 0;
		eh->eh_max = le16(max ? max : (uint16_t)((sizeof(inode->i_block) -
		    sizeof(*eh)) / sizeof(struct ext4_extent)));
		eh->eh_depth = 0;
		eh->eh_generation = 0;
	}
	inode->i_blocks_lo = le32((uint32_t)(keep_blocks * (emp->em_blocksize / 512)));
	return 0;
}

int
ext4_read_inode(struct ext4mount *emp, ino_t ino, struct ext4_inode *out)
{
	struct ext4_group_desc gd;
	uint32_t grp, index;
	uint64_t itable_block;
	uint64_t byteoff, pblk;
	uint32_t off_in_block;
	buf_t bp = NULL;
	int error;
	IOLock *lock;

	if (ino == 0 || ino > emp->em_inodes_count) {
		E4LOG("inode %llu out of range", (uint64_t)ino);
		return EINVAL;
	}

	grp   = (uint32_t)((ino - 1) / emp->em_inodes_per_group);
	index = (uint32_t)((ino - 1) % emp->em_inodes_per_group);

	lock = (IOLock *)emp->em_alloc_lock;
	if (lock == NULL)
		return EIO;
	IOLockLock(lock);

	error = ext4_read_group_desc(emp, grp, &gd);
	if (error) {
		IOLockUnlock(lock);
		return error;
	}

	itable_block = le32(gd.bg_inode_table_lo);
	if (emp->em_desc_size >= 64)
		itable_block |= ((uint64_t)le32(gd.bg_inode_table_hi)) << 32;

	byteoff = (uint64_t)index * emp->em_inode_size;
	pblk    = itable_block + (byteoff / emp->em_blocksize);
	off_in_block = (uint32_t)(byteoff % emp->em_blocksize);

	error = ext4_blkread(emp, pblk, &bp);
	if (error) {
		IOLockUnlock(lock);
		return error;
	}

	if (!ext4_inode_csum_verify(emp, ino,
	    (const char *)buf_dataptr(bp) + off_in_block)) {
		const uint8_t *raw = (const uint8_t *)buf_dataptr(bp) + off_in_block;
		E4LOG("inode checksum mismatch ino=%llu pblk=%llu off=%u "
		    "dev_bsize=%u blocksize=%u seed=0x%x isize=%u mode=0x%x "
		    "gen=0x%08x first16=%02x%02x%02x%02x%02x%02x%02x%02x"
		    "%02x%02x%02x%02x%02x%02x%02x%02x",
		    (unsigned long long)ino, (unsigned long long)pblk,
		    off_in_block, emp->em_dev_bsize, emp->em_blocksize,
		    emp->em_csum_seed, emp->em_inode_size,
		    *(const uint16_t *)raw, *(const uint32_t *)(raw + 0x64),
		    raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7],
		    raw[8], raw[9], raw[10], raw[11], raw[12], raw[13], raw[14], raw[15]);
		emp->em_stats.csum_fails++;
		buf_brelse(bp);
		IOLockUnlock(lock);
		return EIO;
	}

	memset(out, 0, sizeof(*out));
	memcpy(out, (char *)buf_dataptr(bp) + off_in_block,
	    sizeof(*out) < emp->em_inode_size ? sizeof(*out) : emp->em_inode_size);
	buf_brelse(bp);
	IOLockUnlock(lock);
	return 0;
}

/*
 * Write a (possibly modified) on-disk inode back to the inode table.
 * Read-modify-write the containing block so bytes beyond our struct
 * (extra_isize region, etc.) are preserved. Synchronous.
 */
int
ext4_write_inode(struct ext4mount *emp, ino_t ino, const struct ext4_inode *in)
{
	struct ext4_group_desc gd;
	uint32_t grp, index;
	uint64_t itable_block, byteoff, pblk;
	uint32_t off_in_block;
	buf_t bp = NULL;
	int error;
	IOLock *lock;

	if (ino == 0 || ino > emp->em_inodes_count)
		return EINVAL;

	grp   = (uint32_t)((ino - 1) / emp->em_inodes_per_group);
	index = (uint32_t)((ino - 1) % emp->em_inodes_per_group);

	lock = (IOLock *)emp->em_alloc_lock;
	if (lock == NULL)
		return EIO;
	IOLockLock(lock);

	error = ext4_read_group_desc(emp, grp, &gd);
	if (error) {
		IOLockUnlock(lock);
		return error;
	}

	itable_block = le32(gd.bg_inode_table_lo);
	if (emp->em_desc_size >= 64)
		itable_block |= ((uint64_t)le32(gd.bg_inode_table_hi)) << 32;

	byteoff = (uint64_t)index * emp->em_inode_size;
	pblk    = itable_block + (byteoff / emp->em_blocksize);
	off_in_block = (uint32_t)(byteoff % emp->em_blocksize);

	error = ext4_blkread(emp, pblk, &bp);
	if (error) {
		IOLockUnlock(lock);
		return error;
	}

	memcpy((char *)buf_dataptr(bp) + off_in_block, in,
	    sizeof(*in) < emp->em_inode_size ? sizeof(*in) : emp->em_inode_size);
	/* checksum spans the full on-disk inode (our fields + preserved tail) */
	ext4_inode_csum_set(emp, ino, (char *)buf_dataptr(bp) + off_in_block);
	error = ext4_meta_bwrite(emp, bp);   /* journaled; releases bp */
	IOLockUnlock(lock);
	return error;
}

/* Walk an extent tree node to resolve a logical block. buf holds the node. */
static int
ext4_extent_lookup(struct ext4mount *emp, ino_t ino, char *node, uint32_t lblk,
    uint64_t *pblk_out)
{
	struct ext4_extent_header *eh = (struct ext4_extent_header *)node;
	uint16_t entries, depth, i;

	/*
	 * A fully zeroed header is an empty tree, not corruption:
	 * ext4_inode_free_extents() leaves i_block that way after releasing the
	 * last extent, so a file truncated to zero legitimately looks like this
	 * until something allocates again. Every block is a hole - which is the
	 * right answer for a file that has no extents. Only a header that is
	 * non-zero but not ours is real damage.
	 */
	if (le16(eh->eh_magic) == 0 && le16(eh->eh_entries) == 0) {
		*pblk_out = 0;
		return 0;
	}
	if (le16(eh->eh_magic) != EXT4_EXT_MAGIC) {
		E4LOG("bad extent magic 0x%x ino=%llu lblk=%u",
		    le16(eh->eh_magic), (unsigned long long)ino, lblk);
		return EIO;
	}
	entries = le16(eh->eh_entries);
	depth   = le16(eh->eh_depth);

	if (depth == 0) {
		struct ext4_extent *ex = (struct ext4_extent *)(eh + 1);
		for (i = 0; i < entries; i++) {
			uint32_t first = le32(ex[i].ee_block);
			uint16_t len   = le16(ex[i].ee_len);
			if (len > 32768) len -= 32768;  /* uninitialized extent */
			if (lblk >= first && lblk < first + len) {
				uint64_t start = le32(ex[i].ee_start_lo) |
				    ((uint64_t)le16(ex[i].ee_start_hi) << 32);
				*pblk_out = start + (lblk - first);
				return 0;
			}
		}
		*pblk_out = 0;   /* hole */
		return 0;
	} else {
		struct ext4_extent_idx *ix = (struct ext4_extent_idx *)(eh + 1);
		uint64_t child = 0;
		int found = 0;
		for (i = 0; i < entries; i++) {
			uint32_t first = le32(ix[i].ei_block);
			if (lblk >= first) {
				child = le32(ix[i].ei_leaf_lo) |
				    ((uint64_t)le16(ix[i].ei_leaf_hi) << 32);
				found = 1;
			} else {
				break;
			}
		}
		if (!found) {
			*pblk_out = 0;
			return 0;
		}
		buf_t bp = NULL;
		int error = ext4_blkread(emp, child, &bp);
		if (error) {
			E4LOG("extent idx depth=%u lblk=%u -> child blk %llu: blkread error %d",
			    depth, lblk, (unsigned long long)child, error);
			return error;
		}
		/* copy node out so we can release the buffer before recursing */
		char *copy = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
		if (!copy) { buf_brelse(bp); return ENOMEM; }
		memcpy(copy, (char *)buf_dataptr(bp), emp->em_blocksize);
		buf_brelse(bp);
		{
			struct ext4_extent_header *child_eh = (struct ext4_extent_header *)copy;
			if (le16(child_eh->eh_magic) != EXT4_EXT_MAGIC) {
				E4LOG("extent idx depth=%u lblk=%u -> child blk %llu: "
				    "bad child magic 0x%x (entries[0].block=%u leaf=%llu)",
				    depth, lblk, (unsigned long long)child,
				    le16(child_eh->eh_magic), le32(ix[0].ei_block),
				    (unsigned long long)child);
			}
		}
		error = ext4_extent_lookup(emp, ino, copy, lblk, pblk_out);
		_FREE(copy, M_TEMP);
		return error;
	}
}

/*
 * Map a logical block of an inode to a physical block.  Returns 0 and sets
 * *pblk_out (0 => hole).  Handles both extent-mapped and classic
 * (direct/indirect) inodes.
 */
int
ext4_bmap(struct ext4mount *emp, ino_t ino, struct ext4_inode *inode, uint32_t lblk,
    uint64_t *pblk_out)
{
	uint32_t flags = le32(inode->i_flags);

	if (flags & EXT4_EXTENTS_FL) {
		/* extent root lives inline in i_block[] (60 bytes) */
		return ext4_extent_lookup(emp, ino, (char *)inode->i_block, lblk,
		    pblk_out);
	}

	/* classic block map: 12 direct, then single/double/triple indirect */
	uint32_t ptrs = emp->em_blocksize / 4;   /* pointers per indirect block */
	if (lblk < 12) {
		*pblk_out = le32(inode->i_block[lblk]);
		return 0;
	}
	lblk -= 12;
	if (lblk < ptrs) {
		return ext4_indirect_lookup(emp, le32(inode->i_block[12]), lblk,
		    1, pblk_out);
	}
	lblk -= ptrs;
	if (lblk < ptrs * ptrs) {
		return ext4_indirect_lookup(emp, le32(inode->i_block[13]), lblk,
		    2, pblk_out);
	}
	lblk -= ptrs * ptrs;
	return ext4_indirect_lookup(emp, le32(inode->i_block[14]), lblk,
	    3, pblk_out);
}

/* Resolve `lblk` within an N-level indirect block tree rooted at `blk`. */
int
ext4_indirect_lookup(struct ext4mount *emp, uint32_t blk, uint32_t lblk,
    int level, uint64_t *pblk_out)
{
	uint32_t ptrs = emp->em_blocksize / 4;
	uint32_t idx, sub;
	buf_t bp = NULL;
	int error;
	uint32_t next;

	if (blk == 0) { *pblk_out = 0; return 0; }

	if (level == 1) {
		idx = lblk;
	} else if (level == 2) {
		idx = lblk / ptrs;
		sub = lblk % ptrs;
	} else {
		idx = lblk / (ptrs * ptrs);
		sub = lblk % (ptrs * ptrs);
	}

	error = ext4_blkread(emp, blk, &bp);
	if (error)
		return error;
	next = le32(((uint32_t *)buf_dataptr(bp))[idx]);
	buf_brelse(bp);

	if (level == 1) {
		*pblk_out = next;
		return 0;
	}
	return ext4_indirect_lookup(emp, next, sub, level - 1, pblk_out);
}
