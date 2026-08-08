/*
 * ext4.h - ext2/3/4 on-disk format + in-core structures
 *
 * Read-write ext4 driver for PureDarwin/XNU. Handles extent-based inodes,
 * 32- and 64-bit block numbers, and now maintains metadata checksums
 * (metadata_csum / uninit_bg) and initializes uninitialized block/inode
 * groups on demand (see ext4_csum.c), so filesystems from a stock
 * `mkfs.ext4` (all features default-on except the journal) can be mounted
 * read-write without leaving the on-disk metadata inconsistent.
 *
 * Journaling (ext4_jbd.c): metadata writes are collected per-vnop under the
 * mount-wide em_fs_lock and committed as JBD2 transactions (write-ahead:
 * journal durable before in-place writes), with replay on mount - so a
 * journaled fs (stock mkfs.ext4) survives hard power-off mid-operation.
 */
#ifndef _EXT4_H_
#define _EXT4_H_

#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <libkern/OSByteOrder.h>

/*
 * In-core inode (ext4node) hash. Without it, ext4_vget() minted a fresh vnode
 * on every name-cache miss, so a single inode could end up with several vnodes
 * whose cached e_raw/block maps diverge - directory entries then intermittently
 * "disappeared" as lookups landed on a stale duplicate. Buckets are power-of-two
 * so the mask indexes directly; the table lives in ext4mount, guarded by
 * em_hash_lock (an IOLock, stored as void* to keep IOKit headers out of the
 * plain-C sources that include this).
 */
#define EXT4_NODE_HASH_SIZE 256u
#define EXT4_NODE_HASH(ino) ((uint32_t)(ino) & (EXT4_NODE_HASH_SIZE - 1u))

/* ext4 is little-endian on disk; XNU x86_64 is little-endian, so these are
 * pass-through, but keep them explicit for clarity/portability. */
#define le16(x) OSSwapLittleToHostInt16(x)
#define le32(x) OSSwapLittleToHostInt32(x)
#define le64(x) OSSwapLittleToHostInt64(x)

#define EXT4_SUPERBLOCK_OFFSET  1024
#define EXT4_SUPER_MAGIC        0xEF53
#define EXT4_ROOT_INO           2
#define EXT4_N_BLOCKS           15
#define EXT4_GOOD_OLD_INODE_SIZE 128

/* s_feature_incompat bits we care about */
#define EXT4_FEATURE_INCOMPAT_FILETYPE  0x0002
#define EXT4_FEATURE_INCOMPAT_RECOVER   0x0004  /* journal needs recovery */
#define EXT4_FEATURE_INCOMPAT_64BIT     0x0080
#define EXT4_FEATURE_INCOMPAT_EXTENTS   0x0040
#define EXT4_FEATURE_INCOMPAT_CSUM_SEED 0x2000  /* seed stored in s_checksum_seed */

/* s_feature_ro_compat bits we care about */
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM      0x0010 /* uninit_bg: crc16 gd csums */
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM 0x0400 /* crc32c metadata checksums */

/* bg_flags */
#define EXT4_BG_INODE_UNINIT    0x0001  /* inode bitmap not initialized on disk */
#define EXT4_BG_BLOCK_UNINIT    0x0002  /* block bitmap not initialized on disk */
#define EXT4_BG_INODE_ZEROED    0x0004  /* inode table zeroed */

/* i_flags */
#define EXT4_EXTENTS_FL         0x00080000  /* inode uses extents */
#define EXT4_INDEX_FL           0x00001000  /* directory has an HTREE index */

/* on-disk superblock (partial; fields we read) */
struct ext4_super_block {
	uint32_t s_inodes_count;
	uint32_t s_blocks_count_lo;
	uint32_t s_r_blocks_count_lo;
	uint32_t s_free_blocks_count_lo;
	uint32_t s_free_inodes_count;
	uint32_t s_first_data_block;
	uint32_t s_log_block_size;
	uint32_t s_log_cluster_size;
	uint32_t s_blocks_per_group;
	uint32_t s_clusters_per_group;
	uint32_t s_inodes_per_group;
	uint32_t s_mtime;
	uint32_t s_wtime;
	uint16_t s_mnt_count;
	uint16_t s_max_mnt_count;
	uint16_t s_magic;
	uint16_t s_state;
	uint16_t s_errors;
	uint16_t s_minor_rev_level;
	uint32_t s_lastcheck;
	uint32_t s_checkinterval;
	uint32_t s_creator_os;
	uint32_t s_rev_level;
	uint16_t s_def_resuid;
	uint16_t s_def_resgid;
	/* EXT4_DYNAMIC_REV */
	uint32_t s_first_ino;
	uint16_t s_inode_size;
	uint16_t s_block_group_nr;
	uint32_t s_feature_compat;
	uint32_t s_feature_incompat;
	uint32_t s_feature_ro_compat;
	uint8_t  s_uuid[16];
	char     s_volume_name[16];
	char     s_last_mounted[64];
	uint32_t s_algorithm_usage_bitmap;
	uint8_t  s_prealloc_blocks;
	uint8_t  s_prealloc_dir_blocks;
	uint16_t s_reserved_gdt_blocks;
	uint8_t  s_journal_uuid[16];
	uint32_t s_journal_inum;
	uint32_t s_journal_dev;
	uint32_t s_last_orphan;
	uint32_t s_hash_seed[4];
	uint8_t  s_def_hash_version;
	uint8_t  s_jnl_backup_type;
	uint16_t s_desc_size;
	uint32_t s_default_mount_opts;
	uint32_t s_first_meta_bg;
	uint32_t s_mkfs_time;
	uint32_t s_jnl_blocks[17];
	uint32_t s_blocks_count_hi;
	uint32_t s_r_blocks_count_hi;
	uint32_t s_free_blocks_count_hi;
	uint16_t s_min_extra_isize;
	uint16_t s_want_extra_isize;
	uint32_t s_flags;               /* 0x160 */
	uint8_t  s_pad_to_csum_seed[268]; /* 0x164 .. 0x270 (fields we don't use) */
	uint32_t s_checksum_seed;       /* 0x270: crc32c(~0,uuid) seed if csum_seed */
	/* ... rest ignored ... */
} __attribute__((packed));

/* on-disk block group descriptor (classic 32-byte form; 64-bit adds _hi) */
struct ext4_group_desc {
	uint32_t bg_block_bitmap_lo;
	uint32_t bg_inode_bitmap_lo;
	uint32_t bg_inode_table_lo;
	uint16_t bg_free_blocks_count_lo;
	uint16_t bg_free_inodes_count_lo;
	uint16_t bg_used_dirs_count_lo;
	uint16_t bg_flags;
	uint32_t bg_exclude_bitmap_lo;
	uint16_t bg_block_bitmap_csum_lo;
	uint16_t bg_inode_bitmap_csum_lo;
	uint16_t bg_itable_unused_lo;
	uint16_t bg_checksum;
	/* 64-bit hi halves follow when s_desc_size >= 64 */
	uint32_t bg_block_bitmap_hi;
	uint32_t bg_inode_bitmap_hi;
	uint32_t bg_inode_table_hi;
	uint16_t bg_free_blocks_count_hi;
	uint16_t bg_free_inodes_count_hi;
	uint16_t bg_used_dirs_count_hi;
	uint16_t bg_itable_unused_hi;
	uint32_t bg_exclude_bitmap_hi;
	uint16_t bg_block_bitmap_csum_hi;
	uint16_t bg_inode_bitmap_csum_hi;
	uint32_t bg_reserved;
} __attribute__((packed));

/* on-disk inode (first 128 bytes; large inodes have extra after) */
struct ext4_inode {
	uint16_t i_mode;
	uint16_t i_uid;
	uint32_t i_size_lo;
	uint32_t i_atime;
	uint32_t i_ctime;
	uint32_t i_mtime;
	uint32_t i_dtime;
	uint16_t i_gid;
	uint16_t i_links_count;
	uint32_t i_blocks_lo;
	uint32_t i_flags;
	uint32_t i_osd1;
	uint32_t i_block[EXT4_N_BLOCKS];  /* extent tree root OR block map */
	uint32_t i_generation;
	uint32_t i_file_acl_lo;
	uint32_t i_size_high;
	uint32_t i_obso_faddr;
	uint8_t  i_osd2[12];
	uint16_t i_extra_isize;
	uint16_t i_checksum_hi;
	/* ... */
} __attribute__((packed));

/* extent tree structures (in i_block[]) */
struct ext4_extent_header {
	uint16_t eh_magic;      /* 0xF30A */
	uint16_t eh_entries;
	uint16_t eh_max;
	uint16_t eh_depth;
	uint32_t eh_generation;
} __attribute__((packed));

#define EXT4_EXT_MAGIC  0xF30A

struct ext4_extent_idx {
	uint32_t ei_block;      /* first logical block covered */
	uint32_t ei_leaf_lo;    /* physical block of next-level node */
	uint16_t ei_leaf_hi;
	uint16_t ei_unused;
} __attribute__((packed));

struct ext4_extent {
	uint32_t ee_block;      /* first logical block */
	uint16_t ee_len;        /* number of blocks (>32768 => uninitialized) */
	uint16_t ee_start_hi;
	uint32_t ee_start_lo;   /* low 32 bits of physical block */
} __attribute__((packed));

/* on-disk directory entry (ext2 with filetype) */
struct ext4_dir_entry_2 {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t  name_len;
	uint8_t  file_type;
	char     name[];        /* name_len bytes */
} __attribute__((packed));

struct ext4_dir_entry_tail {
	uint32_t det_reserved_zero1;
	uint16_t det_rec_len;
	uint8_t  det_reserved_zero2;
	uint8_t  det_reserved_ft;
	uint32_t det_checksum;
} __attribute__((packed));

#define EXT4_DIR_ENTRY_TAIL_SIZE 12
#define EXT4_FT_DIR_CSUM        0xDE

/* file_type values */
#define EXT4_FT_UNKNOWN  0
#define EXT4_FT_REG_FILE 1
#define EXT4_FT_DIR      2
#define EXT4_FT_CHRDEV   3
#define EXT4_FT_BLKDEV   4
#define EXT4_FT_FIFO     5
#define EXT4_FT_SOCK     6
#define EXT4_FT_SYMLINK  7

/* i_mode format bits */
#define EXT4_S_IFMT   0xF000
#define EXT4_S_IFSOCK 0xC000
#define EXT4_S_IFLNK  0xA000
#define EXT4_S_IFREG  0x8000
#define EXT4_S_IFBLK  0x6000
#define EXT4_S_IFDIR  0x4000
#define EXT4_S_IFCHR  0x2000
#define EXT4_S_IFIFO  0x1000

/* ---- in-core structures ---- */

struct ext4mount {
	struct mount    *em_mp;
	vnode_t          em_devvp;      /* underlying block device vnode */
	dev_t            em_dev;
	uint32_t         em_blocksize;  /* fs block size (1024 << s_log_block_size) */
	uint32_t         em_dev_bsize;  /* underlying device's native sector size,
	                                   queried via DKIOCGETBLOCKSIZE; ext4_blkread()
	                                   converts fs-block numbers into this unit
	                                   itself rather than trusting a prior
	                                   DKIOCSETBLOCKSIZE to have re-tagged the
	                                   vnode - see ext4_blkread() for why. */
	uint32_t         em_log_blocksize;
	uint32_t         em_inodes_per_group;
	uint32_t         em_blocks_per_group;
	uint32_t         em_inode_size;
	uint32_t         em_first_data_block;
	uint32_t         em_groups_count;
	uint32_t         em_desc_size;  /* group descriptor size (32 or 64) */
	uint32_t         em_feature_incompat;
	uint32_t         em_feature_ro_compat;
	uint32_t         em_reserved_gdt_blocks;
	uint32_t         em_gdt_blocks;   /* blocks the GDT itself occupies */
	uint32_t         em_itable_blocks;/* inode-table blocks per group */
	uint32_t         em_csum_seed;    /* crc32c seed for metadata_csum */
	int              em_has_metadata_csum; /* crc32c metadata checksums */
	int              em_has_gdt_csum;      /* uninit_bg crc16 gd checksums */
	uint64_t         em_blocks_count;
	uint64_t         em_inodes_count;
	struct ext4_super_block em_sb;  /* cached copy */
	void            *em_hash_lock;  /* IOLock* guarding the ext4node hash */
	void            *em_alloc_lock;
	/*
	 * em_fs_lock (IORecursiveLock*): the mount-wide metadata lock. Every
	 * vnop that reads or mutates fs metadata (directory blocks, e_raw,
	 * extent trees, sizes) runs under it. Recursive because a vnop holding
	 * it can synchronously re-enter the driver on the same thread (a
	 * uiomove page fault paging in from this same fs, vnode_create
	 * draining into vnop_inactive, ubc_msync triggering pageout).
	 * VNOP_RECLAIM deliberately does NOT take it: a thread holding
	 * em_fs_lock can block in vnode_getwithvid() waiting for another
	 * thread's reclaim to finish, so reclaim taking this lock would
	 * deadlock. Lock order: em_fs_lock -> e_lock -> em_alloc_lock;
	 * em_hash_lock is a leaf.
	 */
	void            *em_fs_lock;
	/* op counters, logged at unmount (and by the PD-DIAG sysctl-less
	 * "stats" log in ext4_vfs_sync every ~64 syncs) */
	struct {
		uint64_t reads, writes, pageins, pageouts;
		uint64_t dir_adds, dir_removes;
		uint64_t alloc_blocks, free_blocks, alloc_inodes, free_inodes;
		uint64_t csum_fails, dir_check_fails, eio_returns;
		uint64_t jnl_commits, jnl_replays;
	} em_stats;
	LIST_HEAD(ext4_node_bucket, ext4node) em_node_hash[EXT4_NODE_HASH_SIZE];
	/* journaling state (ext4_jbd.c); NULL when the fs has no journal */
	void            *em_jnl;
	/* per-group metadata locations (block bitmap, inode bitmap, inode
	 * table start), cached on first use: ext4_block_is_metadata() used to
	 * re-read every group descriptor from disk for EVERY allocated block */
	struct e4_group_meta {
		uint64_t bbmp, ibmp, itbl;
	}               *em_group_meta;
	/* em_fs_lock recursion depth; only ever touched by the lock holder.
	 * ext4_fs_unlock commits the open journal transaction when the
	 * outermost hold is released, so a whole (possibly re-entrant) vnop
	 * is one atomic journal transaction. */
	int              em_lock_depth;
	/* Who holds em_fs_lock, so a thread that blocks acquiring it can name
	 * the holder. Written by the outermost holder, read racily by waiters -
	 * diagnostics only. */
	void            *em_lock_owner;
	const char      *em_lock_owner_tag;
};

/* in-core inode (attached to vnode fsnode) */
struct ext4node {
	struct ext4mount *e_mount;
	vnode_t           e_vp;
	ino_t             e_ino;
	struct ext4_inode e_raw;        /* cached on-disk inode */
	uint64_t          e_size;
	enum vtype        e_vtype;
	LIST_ENTRY(ext4node) e_hash;    /* em_node_hash linkage */
	int               e_alloc_wip;  /* vnode_create in progress; sleep on &e_vp */
	int               e_unhashed;   /* already LIST_REMOVE'd from em_node_hash */
	int               e_pending_free; /* unlinked while still open; free at reclaim */
	uint32_t          e_dbg_last_lblk; /* diagnostic: last logical block logged */
	void             *e_lock;       /* IOLock*: guards e_raw/e_size/e_vtype against
	                                    the cache-hit refresh in ext4_vget() racing
	                                    concurrent pagein/read/bmap on the same
	                                    vnode (multiple execs/opens of one file in
	                                    close succession - e.g. Xorg+i3+xkbcomp -
	                                    could otherwise observe a torn read of a
	                                    multi-field, unlocked update). */
};

#define VTOE(vp)  ((struct ext4node *)vnode_fsnode(vp))
#define VFSTOEXT4(mp) ((struct ext4mount *)vfs_fsprivate(mp))

/* ext4_subr.c */
int  ext4_read_super(struct ext4mount *emp);
int  ext4_write_super(struct ext4mount *emp);
uint64_t ext4_free_blocks_count(struct ext4mount *emp);
int  ext4_read_group_desc(struct ext4mount *emp, uint32_t grp,
               struct ext4_group_desc *gd);
int  ext4_write_group_desc(struct ext4mount *emp, uint32_t grp,
               const struct ext4_group_desc *gd);
int  ext4_read_inode(struct ext4mount *emp, ino_t ino, struct ext4_inode *out);
int  ext4_write_inode(struct ext4mount *emp, ino_t ino, const struct ext4_inode *in);
int  ext4_alloc_inode(struct ext4mount *emp, enum vtype type, ino_t *ino_out);
int  ext4_free_inode(struct ext4mount *emp, ino_t ino, enum vtype type);
int  ext4_alloc_block(struct ext4mount *emp, uint64_t goal, uint64_t *pblk_out);
int  ext4_free_block(struct ext4mount *emp, uint64_t pblk);
void ext4_inode_init_extent_header(struct ext4_inode *inode);
int  ext4_inode_append_extent(struct ext4mount *emp, ino_t ino,
               struct ext4_inode *inode, uint32_t lblk, uint64_t pblk);
int  ext4_inode_free_extents(struct ext4mount *emp, struct ext4_inode *inode);
int  ext4_inode_truncate_extents(struct ext4mount *emp, struct ext4_inode *inode,
               uint64_t keep_blocks);
int  ext4_bmap(struct ext4mount *emp, ino_t ino, struct ext4_inode *inode,
               uint32_t lblk, uint64_t *pblk_out);
int  ext4_ino_is_live(struct ext4mount *emp, ino_t ino);
int  ext4_indirect_lookup(struct ext4mount *emp, uint32_t blk, uint32_t lblk,
               int level, uint64_t *pblk_out);
int  ext4_blkread(struct ext4mount *emp, uint64_t pblk, buf_t *bpp);
uint64_t ext4_gd_block_bitmap(struct ext4mount *emp, const struct ext4_group_desc *gd);
uint64_t ext4_gd_inode_bitmap(struct ext4mount *emp, const struct ext4_group_desc *gd);
int  ext4_bitmap_test(const uint8_t *map, uint32_t bit);
void ext4_bitmap_set(uint8_t *map, uint32_t bit);
enum vtype ext4_ft_to_vtype(uint8_t ft);
enum vtype ext4_mode_to_vtype(uint16_t mode);

/* ext4_csum.c - metadata checksums (metadata_csum / uninit_bg) */
uint32_t ext4_crc32c(uint32_t crc, const void *buf, size_t len);
void ext4_csum_init(struct ext4mount *emp);   /* compute seed + feature flags */
void ext4_group_desc_csum_set(struct ext4mount *emp, uint32_t grp,
               struct ext4_group_desc *gd);
int  ext4_group_desc_csum_verify(struct ext4mount *emp, uint32_t grp,
               const struct ext4_group_desc *gd);
void ext4_block_bitmap_csum_set(struct ext4mount *emp, struct ext4_group_desc *gd,
               const void *bitmap);
void ext4_inode_bitmap_csum_set(struct ext4mount *emp, struct ext4_group_desc *gd,
               const void *bitmap);
void ext4_inode_csum_set(struct ext4mount *emp, ino_t ino, void *raw_full);
int  ext4_inode_csum_verify(struct ext4mount *emp, ino_t ino,
               const void *raw_full);
void ext4_superblock_csum_set(struct ext4mount *emp, void *sb_buf);
int  ext4_dir_block_has_tail(struct ext4mount *emp, const void *block);
void ext4_dir_block_init_tail(struct ext4mount *emp, void *block);
void ext4_dir_block_csum_set(struct ext4mount *emp, ino_t ino,
               const struct ext4_inode *inode, void *block);
void ext4_extent_block_csum_set(struct ext4mount *emp, ino_t ino,
               const struct ext4_inode *inode, void *block);
int  ext4_group_has_super(struct ext4mount *emp, uint32_t grp);
/* Synthesize an uninitialized block group's bitmap. Returns 0 and fills
 * `map`/`*free_out` on success, or nonzero if the computed free count
 * disagrees with the descriptor (caller must then not trust it). */
int  ext4_init_block_bitmap(struct ext4mount *emp, uint32_t grp,
               const struct ext4_group_desc *gd, uint8_t *map,
               uint32_t *free_out);

/* mount-wide metadata lock (ext4_vfsops.c) */
void ext4_fs_lock_tagged(struct ext4mount *emp, const char *who);
#define ext4_fs_lock(emp) ext4_fs_lock_tagged((emp), __func__)
void ext4_fs_unlock(struct ext4mount *emp);
void ext4_fs_unlock_nocommit(struct ext4mount *emp);

/* Validate a directory block's rec_len chain. Returns 0 if walkable. */
int  ext4_dir_block_check(struct ext4mount *emp, const void *block,
               const char *tag, uint64_t ino);

/* ext4_jbd.c - JBD2 journal (replay on mount, write-ahead commit per vnop) */
int  ext4_jnl_mount(struct ext4mount *emp);   /* locate + replay if needed */
void ext4_jnl_unmount(struct ext4mount *emp);
/* Record a metadata block write in the currently open transaction (no-op
 * without a journal). Called by ext4_meta_bwrite. */
int  ext4_jnl_log_block(struct ext4mount *emp, uint64_t pblk, const void *data);
/* Commit the open transaction: journal writes hit disk before the in-place
 * writes they describe are allowed to (WAL ordering). */
int  ext4_jnl_commit(struct ext4mount *emp);
/* 1 if the open transaction is due for commit (size/age policy). */
int  ext4_jnl_should_commit(struct ext4mount *emp);
/* Overlay pending (logged, not yet checkpointed) contents of pblk into out.
 * Returns 1 if an overlay was applied. */
int  ext4_jnl_read_overlay(struct ext4mount *emp, uint64_t pblk, void *out);

/* Journaled metadata write: logs the block, then writes it in place.
 * Falls back to plain buf_bwrite when the fs has no journal. */
int  ext4_meta_bwrite(struct ext4mount *emp, buf_t bp);

/* ext4_vnops.c */
extern int (**ext4_vnodeop_p)(void *);
extern struct vnodeopv_desc ext4_vnodeop_opv_desc;
int  ext4_vget(struct ext4mount *emp, ino_t ino, vnode_t dvp, vnode_t *vpp,
               struct componentname *cnp);

/* ext4_vfsops.c */
extern struct vfsops ext4_vfsops;

/* logging */
extern void kprintf(const char *fmt, ...);
#define E4LOG(fmt, ...) kprintf("ext4: " fmt "\n", ##__VA_ARGS__)

#endif /* _EXT4_H_ */
