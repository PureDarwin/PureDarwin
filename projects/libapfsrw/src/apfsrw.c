/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

/*
 * Userspace-only feature tests. _XOPEN_SOURCE in particular puts Apple's
 * headers into strict-POSIX mode, which hides the KERNEL-guarded typedefs
 * (user_time_t, user64_long_t, ...) and makes every kernel header fail.
 */
#ifndef APFSRW_KERNEL
#define _FILE_OFFSET_BITS 64
#define _XOPEN_SOURCE 700
#endif

#include "apfsrw/apfsrw.h"
#ifndef APFSRW_KERNEL
/*
 * The LZVN/LZBITMAP decoders pull in <stdlib.h>, <assert.h> and <limits.h>,
 * none of which exist in the kernel. The kext only needs the B-tree paths, so
 * decmpfs decompression is userspace-only; a compressed file simply reports
 * APFSRW_ECOMPRESSED there.
 */
#include "apfsrw_lzvn.h"
#include "libzbitmap.h"
#endif

#include "apfsrw/apfsrw_port.h"

#define APFS_NX_MAGIC 0x4253584eU
#define APFS_APSB_MAGIC 0x42535041U
#define APFS_OBJECT_TYPE_MASK 0x0000ffffU
#define APFS_OBJECT_TYPE_BTREE 0x00000002U
#define APFS_OBJECT_TYPE_BTREE_NODE 0x00000003U
#define APFS_OBJECT_TYPE_OMAP 0x0000000bU
#define APFS_OBJECT_TYPE_FS 0x0000000dU
#define APFS_OBJECT_TYPE_FSTREE 0x0000000eU
#define APFS_OBJECT_TYPE_SPACEMAN 0x00000005U
#define APFS_OBJECT_TYPE_CHECKPOINT_MAP 0x0000000cU
#define APFS_OBJECT_TYPE_SPACEMAN_CIB 0x00000007U
/* spec p.177 "Object Types and Flags": OBJ_EPHEMERAL 0x80000000 */
#define APFS_OBJ_EPHEMERAL 0x80000000U
#define APFS_BTNODE_ROOT 0x0001U
#define APFS_BTNODE_LEAF 0x0002U
#define APFS_BTNODE_FIXED_KV_SIZE 0x0004U
#define APFS_BTNODE_HASHED 0x0008U
/* spec p.132 BTNODE_NOHEADER: the node's obj_phys_t is not populated, so it
 * carries no checksum to verify. The header SLOT still exists - btn_flags is
 * at the usual offset - it is simply zero-filled. */
#define APFS_BTNODE_NOHEADER 0x0010U
/* btree_info_t.bt_flags; spec p.132 "B-Tree Flags". Neither PHYSICAL nor
 * EPHEMERAL set means child links are virtual oids. */
#define APFS_BTREE_EPHEMERAL 0x00000008U
#define APFS_BTREE_PHYSICAL 0x00000010U
/* spec p.132 "B-Tree Flags": BTREE_HASHED - nonleaf children store a hash of
 * the child node alongside its oid (btn_index_node_val_t, spec p.127). */
#define APFS_BTREE_HASHED 0x00000080U
/* omap_val_t.ov_flags; spec p.48 "Object Map Value Flags". NOHEADER means the
 * object is stored without an obj_phys_t header - so there is no checksum to
 * verify and node fields are NOT offset by sizeof(obj_phys_t). */
#define APFS_OMAP_VAL_NOHEADER 0x00000008U
#define APFS_OBJ_ID_MASK 0x0fffffffffffffffULL
#define APFS_OBJ_TYPE_MASK 0xf000000000000000ULL
#define APFS_OBJ_TYPE_SHIFT 60U
#define APFS_TYPE_INODE 3U
#define APFS_TYPE_FILE_EXTENT 8U
#define APFS_TYPE_EXTENT 2U
#define APFS_KIND_NEW 1U
#define APFS_TYPE_DSTREAM_ID 6U
#define APFS_TYPE_XATTR 4U
#define APFS_TYPE_DIR_REC 9U
/* spec p.96 APFS_INCOMPAT_CASE_INSENSITIVE */
#define APFS_INCOMPAT_CASE_INSENSITIVE 0x00000001ULL
/* spec p.94-95 "Extended-Attribute Flags" */
#define APFS_XATTR_DATA_STREAM 0x0001U
#define APFS_XATTR_DATA_EMBEDDED 0x0002U
/* spec p.94-95: the symlink target xattr must be marked system-owned. */
#define APFS_XATTR_FILE_SYSTEM_OWNED 0x0004U
/* decmpfs_disk_header, from src/Kernel/xnu/bsd/sys/decmpfs.h (APSL) - NOT the
 * APFS spec, which never mentions decmpfs. DECMPFS_MAGIC is 'cmpf'. */
#define APFS_DECMPFS_MAGIC 0x636d7066U
#define APFS_DECMPFS_TYPE_UNCOMPRESSED 1U
/* Measured on 25G83, not spec: type 8 = LZVN in the ResourceFork stream,
 * type 9 = raw content inline in the decmpfs xattr. See apfs-notes.txt. */
#define APFS_DECMPFS_TYPE_LZVN_RSRC 8U
#define APFS_DECMPFS_TYPE_LZBITMAP_RSRC 14U
#define APFS_DECMPFS_TYPE_RAW_XATTR 9U
#define APFS_DECMPFS_RAW_MARKER 0xccU
#define APFS_DECMPFS_CHUNK 65536U
#define APFS_DECMPFS_XATTR "com.apple.decmpfs\0"
#define APFS_RSRCFORK_XATTR "com.apple.ResourceFork\0" 
/* spec p.111 "Extended-Field Types": INO_EXT_TYPE_DSTREAM 8 */
/* j_inode_flags: INODE_NO_RSRC_FORK (fsck_apfs asks for it on files with no
 * resource fork) */
#define APFS_INODE_NO_RSRC_FORK 0x8000ULL
#define APFS_INO_EXT_TYPE_NAME 4U
#define APFS_INO_EXT_TYPE_DSTREAM 8U
#define APFS_FILE_EXTENT_LEN_MASK 0x00ffffffffffffffULL
#define APFS_NX_MAX_FILE_SYSTEMS 100
#define APFS_MAX_CKSUM_SIZE 8

typedef int64_t apfs_paddr_t;
typedef uint64_t apfs_oid_t;
typedef uint64_t apfs_xid_t;

struct apfs_obj_phys {
    uint8_t o_cksum[APFS_MAX_CKSUM_SIZE];
    apfs_oid_t o_oid;
    apfs_xid_t o_xid;
    uint32_t o_type;
    uint32_t o_subtype;
} __attribute__((packed));

struct apfs_nx_superblock {
    struct apfs_obj_phys nx_o;
    uint32_t nx_magic;
    uint32_t nx_block_size;
    uint64_t nx_block_count;
    uint64_t nx_features;
    uint64_t nx_readonly_compatible_features;
    uint64_t nx_incompatible_features;
    uint8_t nx_uuid[16];
    apfs_oid_t nx_next_oid;
    apfs_xid_t nx_next_xid;
    uint32_t nx_xp_desc_blocks;
    uint32_t nx_xp_data_blocks;
    apfs_paddr_t nx_xp_desc_base;
    apfs_paddr_t nx_xp_data_base;
    uint32_t nx_xp_desc_next;
    uint32_t nx_xp_data_next;
    uint32_t nx_xp_desc_index;
    uint32_t nx_xp_desc_len;
    uint32_t nx_xp_data_index;
    uint32_t nx_xp_data_len;
    apfs_oid_t nx_spaceman_oid;
    apfs_oid_t nx_omap_oid;
    apfs_oid_t nx_reaper_oid;
    uint32_t nx_test_type;
    uint32_t nx_max_file_systems;
    apfs_oid_t nx_fs_oid[APFS_NX_MAX_FILE_SYSTEMS];
} __attribute__((packed));

struct apfs_crypto_state {
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t cpflags;
    uint32_t persistent_class;
    uint32_t key_os_version;
    uint16_t key_revision;
    uint16_t unused;
} __attribute__((packed));

struct apfs_superblock {
    struct apfs_obj_phys apfs_o;
    uint32_t apfs_magic;
    uint32_t apfs_fs_index;
    uint64_t apfs_features;
    uint64_t apfs_readonly_compatible_features;
    uint64_t apfs_incompatible_features;
    uint64_t apfs_unmount_time;
    uint64_t apfs_fs_reserve_block_count;
    uint64_t apfs_fs_quota_block_count;
    uint64_t apfs_fs_alloc_count;
    struct apfs_crypto_state apfs_meta_crypto;
    uint32_t apfs_root_tree_type;
    uint32_t apfs_extentref_tree_type;
    uint32_t apfs_snap_meta_tree_type;
    apfs_oid_t apfs_omap_oid;
    apfs_oid_t apfs_root_tree_oid;
    apfs_oid_t apfs_extentref_tree_oid;
    apfs_oid_t apfs_snap_meta_tree_oid;
    apfs_xid_t apfs_revert_to_xid;
    apfs_oid_t apfs_revert_to_sblock_oid;
    uint64_t apfs_next_obj_id;
    uint64_t apfs_num_files;
    uint64_t apfs_num_directories;
    uint64_t apfs_num_symlinks;
    uint64_t apfs_num_other_fsobjects;
    uint64_t apfs_num_snapshots;
    uint64_t apfs_total_blocks_alloced;
    uint64_t apfs_total_blocks_freed;
    uint8_t apfs_vol_uuid[16];
    uint64_t apfs_last_mod_time;
    uint64_t apfs_fs_flags;
} __attribute__((packed));

struct apfs_omap_phys {
    struct apfs_obj_phys om_o;
    uint32_t om_flags;
    uint32_t om_snap_count;
    uint32_t om_tree_type;
    uint32_t om_snapshot_tree_type;
    apfs_oid_t om_tree_oid;
    apfs_oid_t om_snapshot_tree_oid;
    apfs_xid_t om_most_recent_snap;
    apfs_xid_t om_pending_revert_min;
    apfs_xid_t om_pending_revert_max;
} __attribute__((packed));

struct apfs_omap_key {
    apfs_oid_t ok_oid;
    apfs_xid_t ok_xid;
} __attribute__((packed));

struct apfs_omap_val {
    uint32_t ov_flags;
    uint32_t ov_size;
    apfs_paddr_t ov_paddr;
} __attribute__((packed));

struct apfs_nloc {
    uint16_t off;
    uint16_t len;
} __attribute__((packed));

struct apfs_kvloc {
    struct apfs_nloc k;
    struct apfs_nloc v;
} __attribute__((packed));

struct apfs_btree_node_phys {
    struct apfs_obj_phys btn_o;
    uint16_t btn_flags;
    uint16_t btn_level;
    uint32_t btn_nkeys;
    struct apfs_nloc btn_table_space;
    struct apfs_nloc btn_free_space;
    struct apfs_nloc btn_key_free_list;
    struct apfs_nloc btn_val_free_list;
    uint8_t btn_data[];
} __attribute__((packed));

struct apfs_btree_info {
    uint32_t bt_flags;
    uint32_t bt_node_size;
    uint32_t bt_key_size;
    uint32_t bt_val_size;
    uint32_t bt_longest_key;
    uint32_t bt_longest_val;
    uint64_t bt_key_count;
    uint64_t bt_node_count;
} __attribute__((packed));

struct apfs_j_key {
    uint64_t obj_id_and_type;
} __attribute__((packed));

struct apfs_j_inode_val {
    uint64_t parent_id;
    uint64_t private_id;
    uint64_t create_time;
    uint64_t mod_time;
    uint64_t change_time;
    uint64_t access_time;
    uint64_t internal_flags;
    union {
        int32_t nchildren;
        int32_t nlink;
    } u;
    uint32_t default_protection_class;
    uint32_t write_generation_counter;
    uint32_t bsd_flags;
    uint32_t owner;
    uint32_t group;
    uint16_t mode;
    uint16_t pad1;
    uint64_t uncompressed_size;
    uint8_t xfields[];
} __attribute__((packed));

struct apfs_j_drec_val {
    uint64_t file_id;
    uint64_t date_added;
    uint16_t flags;
    uint8_t xfields[];
} __attribute__((packed));

/* spec p.40-41 checkpoint_mapping_t / checkpoint_map_phys_t */
struct apfs_checkpoint_mapping {
    uint32_t cpm_type;
    uint32_t cpm_subtype;
    uint32_t cpm_size;
    uint32_t cpm_pad;
    apfs_oid_t cpm_fs_oid;
    apfs_oid_t cpm_oid;
    apfs_oid_t cpm_paddr;
} __attribute__((packed));

struct apfs_checkpoint_map_phys {
    struct apfs_obj_phys cpm_o;
    uint32_t cpm_flags;
    uint32_t cpm_count;
    struct apfs_checkpoint_mapping cpm_map[];
} __attribute__((packed));

/* spec p.159 chunk_info_t / chunk_info_block */
struct apfs_chunk_info {
    uint64_t ci_xid;
    uint64_t ci_addr;
    uint32_t ci_block_count;
    uint32_t ci_free_count;
    apfs_paddr_t ci_bitmap_addr;
} __attribute__((packed));

struct apfs_chunk_info_block {
    struct apfs_obj_phys cib_o;
    uint32_t cib_index;
    uint32_t cib_chunk_info_count;
    struct apfs_chunk_info cib_chunk_info[];
} __attribute__((packed));

/* spec p.160-161 spaceman_device_t / spaceman_phys_t. Only the leading fields
 * are needed: sm_dev[] is at a fixed offset and the CIB address array lives at
 * sm_dev[].sm_addr_offset from the start of the spaceman block. */
struct apfs_spaceman_device {
    uint64_t sm_block_count;
    uint64_t sm_chunk_count;
    uint32_t sm_cib_count;
    uint32_t sm_cab_count;
    uint64_t sm_free_count;
    uint32_t sm_addr_offset;
    uint32_t sm_reserved;
    uint64_t sm_reserved2;
} __attribute__((packed));

struct apfs_spaceman_phys {
    struct apfs_obj_phys sm_o;
    uint32_t sm_block_size;
    uint32_t sm_blocks_per_chunk;
    uint32_t sm_chunks_per_cib;
    uint32_t sm_cibs_per_cab;
    struct apfs_spaceman_device sm_dev[2];
    uint32_t sm_flags;
    uint32_t sm_ip_bm_tx_multiplier;
    uint64_t sm_ip_block_count;
    uint32_t sm_ip_bm_size_in_blocks;
    uint32_t sm_ip_bm_block_count;
    apfs_paddr_t sm_ip_bm_base;
    apfs_paddr_t sm_ip_base;
    uint64_t sm_fs_reserve_block_count;
    uint64_t sm_fs_reserve_alloc_count;
    uint8_t sm_fq[120];                  /* spaceman_free_queue_t sm_fq[3] */
    uint16_t sm_ip_bm_free_head;
    uint16_t sm_ip_bm_free_tail;
    uint32_t sm_ip_bm_xid_offset;
    uint32_t sm_ip_bitmap_offset;
    uint32_t sm_ip_bm_free_next_offset;
} __attribute__((packed));

struct apfs_j_xattr_val {
    uint16_t flags;
    uint16_t xdata_len;
    uint8_t xdata[];
} __attribute__((packed));

/* spec p.106 j_xattr_dstream_t: an xattr whose value is a data stream. */
struct apfs_j_xattr_dstream {
    uint64_t xattr_obj_id;
    uint64_t size;
    uint64_t alloced_size;
    uint64_t default_crypto_id;
    uint64_t total_bytes_written;
    uint64_t total_bytes_read;
} __attribute__((packed));

/* decmpfs_disk_header (xnu bsd/sys/decmpfs.h), little-endian on disk. */
struct apfs_decmpfs_header {
    uint32_t compression_magic;
    uint32_t compression_type;
    uint64_t uncompressed_size;
    uint8_t attr_bytes[];
} __attribute__((packed));

struct apfs_j_file_extent_key {
    struct apfs_j_key hdr;
    uint64_t logical_addr;
} __attribute__((packed));

struct apfs_j_file_extent_val {
    uint64_t len_and_flags;
    uint64_t phys_block_num;
    uint64_t crypto_id;
} __attribute__((packed));

struct apfsrw {
    int fd;
    /* Kernel builds have no descriptor; this carries the device vnode. */
    void *io_ctx;
    int writable;
    uint64_t image_blocks;
    uint32_t block_size;
    uint64_t block_count;
    struct apfs_nx_superblock nx;
    struct apfs_superblock apfs;
    apfs_xid_t xid;
    apfs_oid_t fs_oid;
    apfs_paddr_t fs_paddr;
    apfs_paddr_t container_omap_paddr;
    apfs_paddr_t container_omap_tree_paddr;
    apfs_oid_t volume_omap_oid;
    apfs_paddr_t volume_omap_paddr;
    apfs_paddr_t volume_omap_tree_paddr;
    apfs_xid_t want_xid;
    apfs_paddr_t nx_paddr;
    apfs_oid_t root_tree_oid;
    apfs_oid_t next_oid;
    /*
     * Net change in blocks OWNED BY THE VOLUME for the transaction in progress.
     * apfs_fs_alloc_count counts volume metadata nodes as well as file extents
     * (apfsck sums btree nodes, volume objects and extents into it), and
     * copy-on-write is net zero - the old block is freed as the new one is
     * allocated - so only genuinely new nodes and new data blocks move it.
     */
    int64_t alloc_delta;
    /*
     * Batch mode. Every create is otherwise its own transaction, ending in a
     * full copy-on-write commit plus two fsyncs - about 19 ms per file on a
     * real disk against 1 ms on tmpfs, so ~94% of bulk population is
     * durability barriers rather than work. Batching defers the commit so a
     * whole tree lands as ONE transaction.
     */
    int batch;
    uint64_t batch_files;
    uint64_t batch_dirs;
    uint64_t batch_links;
    uint64_t batch_next_oid;
    /* Current extent reference tree root. Must be tracked here: inside a batch
     * fs->apfs is not re-read, so the copy there goes stale after the first
     * insert. */
    apfs_paddr_t extref_paddr;
    /*
     * Blocks superseded during the transaction in progress. They must NOT go
     * back to the allocator until the new checkpoint is on disk: releasing one
     * mid-transaction lets it be handed straight back out and overwritten while
     * the structure that still points at it has not been rewritten yet. Real
     * APFS defers reuse through the space manager's free queue for the same
     * reason.
     */
    uint64_t *deferred;
    uint32_t deferred_count;
    uint32_t deferred_cap;
    /* Blocks allocated by the transaction in progress. If it fails before the
     * checkpoint lands, nothing on disk references them, so they are handed
     * back rather than leaked - otherwise apfsck reports a bad allocation
     * bitmap. */
    uint64_t *alloced;
    uint32_t alloced_count;
    uint32_t alloced_cap;
    apfs_paddr_t root_tree_paddr;
};

static uint16_t rd16(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t rd32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
        ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint64_t rd64(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    uint64_t v = 0;
    int i;

    for (i = 7; i >= 0; i--)
        v = (v << 8) | b[i];
    return v;
}

static uint32_t object_type(uint32_t type)
{
    return rd32(&type) & APFS_OBJECT_TYPE_MASK;
}

static uint64_t key_id(uint64_t obj_id_and_type)
{
    return rd64(&obj_id_and_type) & APFS_OBJ_ID_MASK;
}

static uint8_t key_type(uint64_t obj_id_and_type)
{
    return (uint8_t)((rd64(&obj_id_and_type) & APFS_OBJ_TYPE_MASK) >>
        APFS_OBJ_TYPE_SHIFT);
}

static uint64_t fletcher64(const uint8_t *block, size_t size)
{
    uint64_t lo = 0;
    uint64_t hi = 0;
    uint64_t check1;
    uint64_t check2;
    size_t off;

    for (off = APFS_MAX_CKSUM_SIZE; off + sizeof(uint32_t) <= size;
        off += sizeof(uint32_t)) {
        lo = (lo + rd32(block + off)) % 0xffffffffULL;
        hi = (hi + lo) % 0xffffffffULL;
    }
    check1 = 0xffffffffULL - ((lo + hi) % 0xffffffffULL);
    check2 = 0xffffffffULL - ((lo + check1) % 0xffffffffULL);
    return (check2 << 32) | check1;
}

/*
 * All block I/O funnels through these two. Userspace talks to a file
 * descriptor; the kernel build backs them with the mounted device vnode
 * (see apfsrw_kern.c).
 */
#ifndef APFSRW_KERNEL
uint64_t apfsrw_now_ns(void)
{
    struct timespec ts;

    return (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        ? (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec : 0;
}

static ssize_t apfsrw_pread(struct apfsrw *fs, void *buf, size_t n, off_t off)
{
    return pread(fs->fd, buf, n, off);
}

static ssize_t apfsrw_pwrite(struct apfsrw *fs, const void *buf, size_t n,
    off_t off)
{
    return pwrite(fs->fd, buf, n, off);
}
#else
ssize_t apfsrw_pread(struct apfsrw *fs, void *buf, size_t n, off_t off);
ssize_t apfsrw_pwrite(struct apfsrw *fs, const void *buf, size_t n, off_t off);
#endif

static int read_block(struct apfsrw *fs, apfs_paddr_t paddr, void *out)
{
    ssize_t n;

    if (fs == NULL || out == NULL || paddr < 0 ||
        (uint64_t)paddr >= fs->image_blocks)
        return APFSRW_EINVAL;
    n = apfsrw_pread(fs, out, fs->block_size,
        (off_t)((uint64_t)paddr * fs->block_size));
    if (n < 0)
        return APFSRW_EIO;
    if ((size_t)n != fs->block_size)
        return APFSRW_EIO;
    return APFSRW_OK;
}

/* Blocks with no obj_phys_t header (allocation bitmaps, file data) carry no
 * checksum, so they are read and written without verification or sealing. */
static int read_raw(struct apfsrw *fs, apfs_paddr_t paddr, void *out)
{
    ssize_t n;

    if (paddr < 0 || (uint64_t)paddr >= fs->block_count)
        return APFSRW_EINVAL;
    n = apfsrw_pread(fs, out, fs->block_size,
        (off_t)((uint64_t)paddr * fs->block_size));
    if (n < 0 || (size_t)n != fs->block_size)
        return APFSRW_EIO;
    return APFSRW_OK;
}

static uint64_t make_jkey(uint64_t id, uint8_t type)
{
    return (id & APFS_OBJ_ID_MASK) |
        ((uint64_t)type << APFS_OBJ_TYPE_SHIFT);
}

static int read_object(struct apfsrw *fs, apfs_paddr_t paddr, void *out)
{
    uint64_t expected;
    uint64_t actual;
    int err;

    err = read_block(fs, paddr, out);
    if (err != APFSRW_OK)
        return err;
    /*
     * A B-tree node flagged BTNODE_NOHEADER has an unpopulated obj_phys_t and
     * therefore no checksum to check (spec p.132 "B-Tree Node Flags"); the
     * header slot is present but zero-filled, so btn_flags still reads from
     * its usual offset. Sealed volumes use this for their file-system tree.
     */
    {
        const struct apfs_btree_node_phys *n =
            (const struct apfs_btree_node_phys *)out;

        if (rd16(&n->btn_flags) & APFS_BTNODE_NOHEADER)
            return APFSRW_OK;
    }
    expected = rd64(out);
    actual = fletcher64((const uint8_t *)out, fs->block_size);
    if (expected != actual)
        return APFSRW_EIO;
    return APFSRW_OK;
}

static const struct apfs_btree_info *
btree_info_for_node(struct apfsrw *fs, const struct apfs_btree_node_phys *node)
{
    if ((rd16(&node->btn_flags) & APFS_BTNODE_ROOT) == 0)
        return NULL;
    return (const struct apfs_btree_info *)((const uint8_t *)node +
        fs->block_size - sizeof(struct apfs_btree_info));
}

static int btree_entry(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node, const struct apfs_btree_info *info,
    uint32_t index, const void **key, uint16_t *key_len, const void **val,
    uint16_t *val_len)
{
    const uint8_t *base = (const uint8_t *)node;
    uint32_t data_off = (uint32_t)offsetof(struct apfs_btree_node_phys,
        btn_data);
    uint16_t table_off = rd16(&node->btn_table_space.off);
    uint16_t table_len = rd16(&node->btn_table_space.len);
    uint16_t val_end = (uint16_t)fs->block_size;
    uint32_t nkeys = rd32(&node->btn_nkeys);
    uint32_t key_base;
    uint16_t ko;
    uint16_t kl;
    uint16_t vo;
    uint16_t vl;

    (void)info;
    if (index >= nkeys)
        return APFSRW_EINVAL;
    if (data_off + table_off + table_len > fs->block_size)
        return APFSRW_EINVAL;
    /*
     * Values are addressed backwards from the end of the value area. Only a
     * ROOT node carries a trailing btree_info_t, so only there does the value
     * area stop short of the end of the block (spec p.126 btree_info_t:
     * "appears only in a root node, stored at the end of the node"). This must
     * test the node's own flag - an index node inherits its root's
     * btree_info_t for the key/value SIZES, but not for its own layout.
     */
    if (rd16(&node->btn_flags) & APFS_BTNODE_ROOT)
        val_end = (uint16_t)(val_end - sizeof(struct apfs_btree_info));
    key_base = data_off + table_off + table_len;

    uint16_t node_flags_disk;

    memcpy(&node_flags_disk, &node->btn_flags, sizeof(node_flags_disk));
    if (rd16(&node_flags_disk) & APFS_BTNODE_FIXED_KV_SIZE) {
        const struct apfs_kvoff {
            uint16_t k;
            uint16_t v;
        } __attribute__((packed)) *toc;
        uint32_t fixed_key = info != NULL ? rd32(&info->bt_key_size) : 0;
        uint32_t fixed_val = info != NULL ? rd32(&info->bt_val_size) : 0;

        /*
         * A nonleaf node needs only bt_key_size; its values are oid_t.
         * bt_val_size may legitimately be unavailable here when the caller
         * could not supply the root's btree_info_t.
         */
        if (fixed_key == 0)
            return APFSRW_EINVAL;
        if (fixed_val == 0 && rd16(&node->btn_level) == 0)
            return APFSRW_EINVAL;
        toc = (const struct apfs_kvoff *)(base + data_off + table_off +
            index * sizeof(*toc));
        ko = rd16(&toc->k);
        kl = (uint16_t)fixed_key;
        vo = rd16(&toc->v);
        /*
         * bt_val_size describes LEAF values. In a nonleaf node the value is
         * the child link instead: a bare oid_t, or btn_index_node_val_t
         * (oid first, then a hash) on a BTREE_HASHED tree - either way the
         * leading 8 bytes are the child's oid (spec p.127
         * "B-Trees / btn_index_node_val_t").
         */
        if (rd16(&node->btn_level) != 0)
            vl = (uint16_t)sizeof(apfs_oid_t);
        else
            vl = (uint16_t)fixed_val;
    } else {
        const struct apfs_kvloc *toc;

        toc = (const struct apfs_kvloc *)(base + data_off + table_off +
            index * sizeof(*toc));
        ko = rd16(&toc->k.off);
        kl = rd16(&toc->k.len);
        vo = rd16(&toc->v.off);
        vl = rd16(&toc->v.len);
    }

    if (kl == 0 || key_base + ko + kl > fs->block_size)
        return APFSRW_EINVAL;
    /*
     * Values are addressed backwards from the end of the value area, so the
     * value occupies [val_end - vo, val_end - vo + vl). Require that it fits
     * inside the node and does not run past val_end.
     */
    if (vo > val_end || vl > vo)
        return APFSRW_EINVAL;

    *key = base + key_base + ko;
    *key_len = kl;
    *val = base + val_end - vo;
    *val_len = vl;
    return APFSRW_OK;
}

static int omap_lookup_tree(struct apfsrw *fs, apfs_paddr_t tree_paddr,
    apfs_oid_t oid, apfs_xid_t xid, struct apfs_omap_val *out);

/*
 * Walk every leaf of a B-tree, calling cb() once per leaf node.
 *
 * A root node is only a leaf on trivially small trees; real volumes have index
 * nodes above the leaves, so anything that wants all the records has to descend.
 * Nonleaf values start with the child's oid_t - true for both plain and hashed
 * trees, since btn_index_node_val_t puts binv_child_oid first (spec p.127
 * "B-Trees / btn_index_node_val_t"). Whether that oid is a physical address or
 * needs an object-map lookup is decided by BTREE_PHYSICAL in bt_flags (spec
 * p.132 "B-Tree Flags": if neither BTREE_PHYSICAL nor BTREE_EPHEMERAL is set,
 * child links are virtual).
 *
 * cb() returning non-zero stops the walk and that value is returned.
 */
typedef int (*leaf_cb)(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx);

#define APFSRW_BTREE_MAX_DEPTH 16

static int btree_walk_node(struct apfsrw *fs, apfs_paddr_t paddr,
    const struct apfs_btree_info *root_info, uint32_t depth,
    uint64_t oid_min, uint64_t oid_max, leaf_cb cb, void *ctx)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *info;
    struct apfs_btree_info info_storage;
    uint32_t nkeys;
    uint32_t i;
    int err;

    if (depth > APFSRW_BTREE_MAX_DEPTH)
        return APFSRW_EINVAL;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, paddr, node);
    if (err != APFSRW_OK)
        goto out;
    /*
     * A BTNODE_NOHEADER node has a zero-filled obj_phys_t, so o_type reads as
     * 0 and cannot be validated (spec p.132 "B-Tree Node Flags").
     */
    if ((rd16(&node->btn_flags) & APFS_BTNODE_NOHEADER) == 0 &&
        object_type(node->btn_o.o_type) != APFS_OBJECT_TYPE_BTREE &&
        object_type(node->btn_o.o_type) != APFS_OBJECT_TYPE_BTREE_NODE) {
        err = APFSRW_EINVAL;
        goto out;
    }

    /*
     * btree_info_t lives only in the root node (spec p.126 btree_info_t), and
     * it must be COPIED: recursing re-reads into a fresh buffer, but the
     * caller's node is freed on the way out, so a borrowed pointer would
     * dangle for the children that inherit it.
     */
    if (btree_info_for_node(fs, node) != NULL) {
        memcpy(&info_storage, btree_info_for_node(fs, node),
            sizeof(info_storage));
        info = &info_storage;
    } else {
        info = root_info;
    }

    if (rd16(&node->btn_level) == 0) {
        err = cb(fs, node, info, ctx);
        goto out;
    }

    nkeys = rd32(&node->btn_nkeys);
    for (i = 0; i < nkeys; i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        apfs_oid_t child;
        apfs_paddr_t child_paddr;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            goto out;
        /* Hashed trees store oid + hash, so the value is larger than an
         * oid_t; only the leading oid is needed. */
        if (val_len < sizeof(apfs_oid_t))
            continue;

        /*
         * Prune by object id: child i holds the keys in [key(i), key(i+1)),
         * so once a separator passes oid_max nothing later can match, and if
         * the NEXT separator is still below oid_min this child cannot either.
         */
        if (key_len >= sizeof(uint64_t) && key_id(rd64(keyp)) > oid_max)
            break;
        if (i + 1 < nkeys) {
            const void *nk;
            const void *nv;
            uint16_t nkl, nvl;

            if (btree_entry(fs, node, info, i + 1, &nk, &nkl, &nv, &nvl) ==
                APFSRW_OK && nkl >= sizeof(uint64_t) &&
                key_id(rd64(nk)) < oid_min)
                continue;
        }
        child = rd64(valp);

        if (info != NULL &&
            (rd32(&info->bt_flags) & APFS_BTREE_PHYSICAL) != 0) {
            child_paddr = (apfs_paddr_t)child;
        } else {
            struct apfs_omap_val ov;

            /*
             * Sealed (hashed) volumes store binv_child_oid as an offset from
             * the tree's own root oid, not as an absolute oid: the tree's
             * nodes occupy a contiguous oid range starting at
             * apfs_root_tree_oid (bt_node_count entries of it). Absolute oids
             * resolve to nothing at all on such a volume, and the offset form
             * verifies against binv_child_hash at every level. Not stated in
             * the spec - see apfs-notes.txt.
             */
            if (info != NULL &&
                (rd32(&info->bt_flags) & APFS_BTREE_HASHED) != 0)
                child += fs->root_tree_oid;

            /* xid + 1, not xid: nodes rewritten earlier in the transaction
             * in progress carry the NEXT transaction id. */
            err = omap_lookup_tree(fs, fs->volume_omap_tree_paddr, child,
                fs->xid + 1, &ov);
            if (err != APFSRW_OK)
                goto out;
            child_paddr = (apfs_paddr_t)rd64(&ov.ov_paddr);
        }

        err = btree_walk_node(fs, child_paddr, info, depth + 1, oid_min,
            oid_max, cb, ctx);
        if (err != APFSRW_OK)
            goto out;
    }
    err = APFSRW_OK;
out:
    free(node);
    return err;
}

static int btree_walk_leaves_oid(struct apfsrw *fs, apfs_paddr_t root_paddr,
    uint64_t oid_min, uint64_t oid_max, leaf_cb cb, void *ctx)
{
    return btree_walk_node(fs, root_paddr, NULL, 0, oid_min, oid_max, cb, ctx);
}

static int btree_walk_leaves(struct apfsrw *fs, apfs_paddr_t root_paddr,
    leaf_cb cb, void *ctx)
{
    return btree_walk_leaves_oid(fs, root_paddr, 0, UINT64_MAX, cb, ctx);
}

static int omap_lookup_tree(struct apfsrw *fs, apfs_paddr_t tree_paddr,
    apfs_oid_t oid, apfs_xid_t xid, struct apfs_omap_val *out)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *info;
    struct apfs_btree_info info_copy;
    struct apfs_omap_val best;
    apfs_xid_t best_xid = 0;
    int found = 0;
    uint32_t i;
    int err;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, tree_paddr, node);
    if (err != APFSRW_OK)
        goto out;
    /*
     * The root's btree_info_t must be COPIED: descending re-reads into the
     * same buffer, which would leave a pointer into overwritten memory. Only
     * the root carries one (spec p.126 btree_info_t), so the sizes it holds
     * have to survive the walk down.
     */
    {
        const struct apfs_btree_info *root_info = btree_info_for_node(fs, node);

        if (root_info == NULL) {
            err = APFSRW_EINVAL;
            goto out;
        }
        memcpy(&info_copy, root_info, sizeof(info_copy));
        info = &info_copy;
    }

    /*
     * Descend to the leaf that would hold this oid. Object-map trees are sorted
     * by oid then xid (spec p.123 "Key Comparison"), and their child links are
     * physical - an omap cannot need an omap to read itself - so no recursion
     * through omap_lookup_tree() is possible here.
     */
    uint32_t depth_guard = 0;

    while (rd16(&node->btn_level) != 0) {
        uint32_t nkeys = rd32(&node->btn_nkeys);
        apfs_paddr_t child_paddr = -1;

        for (i = 0; i < nkeys; i++) {
            const void *keyp;
            const void *valp;
            uint16_t key_len;
            uint16_t val_len;
            const struct apfs_omap_key *key;

            err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
                &val_len);
            if (err != APFSRW_OK)
                goto out;
            if (key_len < sizeof(*key) || val_len < sizeof(apfs_oid_t))
                continue;
            key = (const struct apfs_omap_key *)keyp;
            /*
             * Take the last child whose key does not exceed (oid, xid). The
             * first child also covers everything below its own key, so seed
             * with entry 0 and only advance past it.
             */
            if (i == 0 ||
                rd64(&key->ok_oid) < oid ||
                (rd64(&key->ok_oid) == oid && rd64(&key->ok_xid) <= xid))
                child_paddr = (apfs_paddr_t)rd64(valp);
            else
                break;
        }
        if (child_paddr < 0) {
            err = APFSRW_ENOENT;
            goto out;
        }
        if (++depth_guard > APFSRW_BTREE_MAX_DEPTH) {
            err = APFSRW_EINVAL;
            goto out;
        }
        err = read_object(fs, child_paddr, node);
        if (err != APFSRW_OK)
            goto out;
        /* Non-root nodes carry no btree_info_t, so the root's stays in use
         * (spec p.126 btree_info_t). */
    }

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        const struct apfs_omap_key *key;
        const struct apfs_omap_val *val;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            goto out;
        if (key_len < sizeof(*key) || val_len < sizeof(*val))
            continue;
        key = (const struct apfs_omap_key *)keyp;
        val = (const struct apfs_omap_val *)valp;
        if (rd64(&key->ok_oid) == oid && rd64(&key->ok_xid) <= xid &&
            (!found || rd64(&key->ok_xid) > best_xid)) {
            memcpy(&best, val, sizeof(best));
            best_xid = rd64(&key->ok_xid);
            found = 1;
        }
    }
    if (!found) {
        err = APFSRW_ENOENT;
        goto out;
    }
    memcpy(out, &best, sizeof(*out));
    err = APFSRW_OK;
out:
    free(node);
    return err;
}

static int read_omap(struct apfsrw *fs, apfs_paddr_t paddr,
    struct apfs_omap_phys *omap)
{
    uint8_t *block;
    int err;

    block = calloc(1, fs->block_size);
    if (block == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, paddr, block);
    if (err == APFSRW_OK) {
        memcpy(omap, block, sizeof(*omap));
        if (object_type(omap->om_o.o_type) != APFS_OBJECT_TYPE_OMAP)
            err = APFSRW_EINVAL;
    }
    free(block);
    return err;
}

static int omap_lookup(struct apfsrw *fs, apfs_paddr_t omap_paddr,
    apfs_oid_t oid, apfs_xid_t xid, struct apfs_omap_val *out,
    apfs_paddr_t *tree_paddr)
{
    struct apfs_omap_phys omap;
    int err;

    err = read_omap(fs, omap_paddr, &omap);
    if (err != APFSRW_OK)
        return err;
    if (tree_paddr != NULL)
        *tree_paddr = (apfs_paddr_t)rd64(&omap.om_tree_oid);
    return omap_lookup_tree(fs, (apfs_paddr_t)rd64(&omap.om_tree_oid), oid,
        xid, out);
}

static int load_volume(struct apfsrw *fs)
{
#ifndef APFSRW_KERNEL
    struct stat st;
#endif
    struct apfs_omap_phys omap;
    struct apfs_omap_val ov;
    uint8_t *block;
    uint32_t max_fs;
    uint32_t i;
    int err;

#ifndef APFSRW_KERNEL
    if (fstat(fs->fd, &st) != 0)
        return APFSRW_EIO;
    fs->image_blocks = (uint64_t)st.st_size / APFSRW_BLOCK_SIZE;
#else
    /* apfsrw_open_kernel() already knows the device size. */
    if (fs->image_blocks == 0)
        return APFSRW_EIO;
#endif
    fs->block_size = APFSRW_BLOCK_SIZE;

    block = calloc(1, fs->block_size);
    if (block == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, 0, block);
    if (err != APFSRW_OK)
        goto out;
    memcpy(&fs->nx, block, sizeof(fs->nx));
    if (rd32(&fs->nx.nx_magic) != APFS_NX_MAGIC ||
        rd32(&fs->nx.nx_block_size) != APFSRW_BLOCK_SIZE) {
        err = APFSRW_EINVAL;
        goto out;
    }

    /*
     * Block zero is only a COPY of the container superblock; the authoritative
     * one is the newest valid copy in the checkpoint descriptor area, which is
     * a ring buffer (spec p.26 "Mounting an Apple File System Partition",
     * steps 2-4). Walk the ring and take the largest xid that does not exceed
     * fs->want_xid, so an earlier checkpoint can be mounted deliberately.
     */
    {
        uint64_t desc_base = rd64(&fs->nx.nx_xp_desc_base);
        uint32_t desc_blocks = rd32(&fs->nx.nx_xp_desc_blocks) & 0x7fffffffU;
        apfs_xid_t best = rd64(&fs->nx.nx_o.o_xid);
        uint32_t j;

        if (fs->want_xid != 0 && best > fs->want_xid)
            best = 0;
        for (j = 0; j < desc_blocks && desc_blocks < 65536U; j++) {
            struct apfs_nx_superblock cand;

            if (read_object(fs, (apfs_paddr_t)(desc_base + j), block) !=
                APFSRW_OK)
                continue;
            memcpy(&cand, block, sizeof(cand));
            if (rd32(&cand.nx_magic) != APFS_NX_MAGIC)
                continue;
            if (fs->want_xid != 0 && rd64(&cand.nx_o.o_xid) > fs->want_xid)
                continue;
            if (rd64(&cand.nx_o.o_xid) <= best)
                continue;
            best = rd64(&cand.nx_o.o_xid);
            memcpy(&fs->nx, &cand, sizeof(fs->nx));
            fs->nx_paddr = (apfs_paddr_t)(desc_base + j);
        }
        if (best == 0) {
            err = APFSRW_ENOENT;
            goto out;
        }
    }

    fs->block_count = rd64(&fs->nx.nx_block_count);
    fs->xid = rd64(&fs->nx.nx_o.o_xid);
    fs->container_omap_paddr = (apfs_paddr_t)rd64(&fs->nx.nx_omap_oid);
    err = read_omap(fs, fs->container_omap_paddr, &omap);
    if (err != APFSRW_OK)
        goto out;
    fs->container_omap_tree_paddr = (apfs_paddr_t)rd64(&omap.om_tree_oid);

    max_fs = rd32(&fs->nx.nx_max_file_systems);
    if (max_fs > APFS_NX_MAX_FILE_SYSTEMS)
        max_fs = APFS_NX_MAX_FILE_SYSTEMS;
    for (i = 0; i < max_fs; i++) {
        apfs_oid_t fs_oid = rd64(&fs->nx.nx_fs_oid[i]);

        if (fs_oid == 0)
            continue;
        err = omap_lookup_tree(fs, fs->container_omap_tree_paddr, fs_oid,
            fs->xid, &ov);
        if (err == APFSRW_OK) {
            fs->fs_oid = fs_oid;
            fs->fs_paddr = (apfs_paddr_t)rd64(&ov.ov_paddr);
            break;
        }
    }
    if (fs->fs_oid == 0) {
        err = APFSRW_ENOENT;
        goto out;
    }

    err = read_object(fs, fs->fs_paddr, block);
    if (err != APFSRW_OK)
        goto out;
    memcpy(&fs->apfs, block, sizeof(fs->apfs));
    if (rd32(&fs->apfs.apfs_magic) != APFS_APSB_MAGIC) {
        err = APFSRW_EINVAL;
        goto out;
    }

    fs->volume_omap_oid = rd64(&fs->apfs.apfs_omap_oid);
    fs->volume_omap_paddr = (apfs_paddr_t)fs->volume_omap_oid;
    fs->root_tree_oid = rd64(&fs->apfs.apfs_root_tree_oid);
    err = omap_lookup(fs, fs->volume_omap_paddr, fs->root_tree_oid, fs->xid,
        &ov, &fs->volume_omap_tree_paddr);
    if (err != APFSRW_OK)
        goto out;
    fs->root_tree_paddr = (apfs_paddr_t)rd64(&ov.ov_paddr);
out:
    free(block);
    return err;
}

/*
 * Extended fields follow the fixed part of j_inode_val_t as: an xf_blob_t
 * header, then xf_num_exts x_field_t descriptors, then the field data in the
 * SAME order, each datum aligned to an 8-byte boundary (spec p.108 "Extended
 * Fields"; xf_blob_t p.108, x_field_t p.109). INO_EXT_TYPE_DSTREAM is 8 and
 * its data is a j_dstream_t whose first field is the stream size (spec p.111
 * INO_EXT_TYPE_DSTREAM; j_dstream_t p.106).
 */
static int inode_dstream_size(const void *val, uint16_t val_len, uint64_t *out)
{
    const uint8_t *p = (const uint8_t *)val;
    uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
    uint32_t num, i, desc, data;

    if (val_len < fixed + 4U)
        return APFSRW_ENOENT;
    num = rd16(p + fixed);
    desc = fixed + 4U;
    data = desc + num * 4U;
    if (data > val_len)
        return APFSRW_EINVAL;
    for (i = 0; i < num; i++) {
        uint8_t type = p[desc + i * 4U];
        uint16_t size = rd16(p + desc + i * 4U + 2U);

        if (data + size > val_len)
            return APFSRW_EINVAL;
        if (type == APFS_INO_EXT_TYPE_DSTREAM) {
            if (size < sizeof(uint64_t))
                return APFSRW_EINVAL;
            *out = rd64(p + data);
            return APFSRW_OK;
        }
        data += ((uint32_t)size + 7U) & ~7U;
    }
    return APFSRW_ENOENT;
}

struct inode_ctx {
    uint64_t file_id;
    uint64_t size;
    uint64_t private_id;
    int found;
};

static int inode_leaf_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct inode_ctx *c = (struct inode_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        uint64_t key;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            return err;
        if (key_len < sizeof(struct apfs_j_key) ||
            val_len < sizeof(struct apfs_j_inode_val))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_id(key) == c->file_id && key_type(key) == APFS_TYPE_INODE) {
            const struct apfs_j_inode_val *inode =
                (const struct apfs_j_inode_val *)valp;
            uint64_t dsize;

            /*
             * uncompressed_size is only meaningful for a compressed file; the
             * real length of the data stream lives in the DSTREAM extended
             * field, so prefer that and fall back otherwise.
             */
            if (inode_dstream_size(valp, val_len, &dsize) == APFSRW_OK)
                c->size = dsize;
            else
                c->size = rd64(&inode->uncompressed_size);
            /*
             * File extents are keyed by the data stream's id, which is the
             * inode's private_id - not the inode's own object id (spec p.100
             * j_inode_val_t private_id: "the unique identifier used by this
             * file's data stream").
             */
            c->private_id = rd64(&inode->private_id);
            c->found = 1;
            return 1;                    /* stop the walk */
        }
    }
    return APFSRW_OK;
}

static int lookup_inode(struct apfsrw *fs, uint64_t file_id, uint64_t *size,
    uint64_t *private_id)
{
    struct inode_ctx c;
    int err;

    c.file_id = file_id;
    c.size = 0;
    c.private_id = file_id;
    c.found = 0;
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, file_id, file_id,
        inode_leaf_cb, &c);
    if (err != APFSRW_OK && err != 1)
        return err;
    if (!c.found)
        return APFSRW_ENOENT;
    *size = c.size;
    if (private_id != NULL)
        *private_id = c.private_id;
    return APFSRW_OK;
}

struct dirent_lookup_ctx {
    uint64_t parent_id;
    const char *name;
    size_t want_len;
    uint64_t file_id;
    uint8_t type;
    int found;
};

static int dirent_lookup_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct dirent_lookup_ctx *c = (struct dirent_lookup_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        uint64_t key;
        uint32_t name_len;
        const char *entry_name;
        const struct apfs_j_drec_val *drec;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            return err;
        if (key_len < 13 || val_len < sizeof(*drec))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_id(key) != c->parent_id ||
            key_type(key) != APFS_TYPE_DIR_REC)
            continue;
        name_len = rd32((const uint8_t *)keyp + 8) & 0x3ffU;
        if (name_len == 0 || 12U + name_len > key_len)
            continue;
        entry_name = (const char *)keyp + 12;
        if (name_len - 1 == c->want_len &&
            memcmp(entry_name, c->name, c->want_len) == 0) {
            drec = (const struct apfs_j_drec_val *)valp;
            c->file_id = rd64(&drec->file_id);
            c->type = (uint8_t)rd16(&drec->flags);
            c->found = 1;
            return 1;
        }
    }
    return APFSRW_OK;
}

static int lookup_dirent(struct apfsrw *fs, uint64_t parent_id,
    const char *name, size_t name_len, uint64_t *file_id, uint8_t *type)
{
    struct dirent_lookup_ctx c;
    int err;

    c.parent_id = parent_id;
    c.name = name;
    c.want_len = name_len;
    c.file_id = 0;
    c.type = APFSRW_DT_UNKNOWN;
    c.found = 0;
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, parent_id, parent_id,
        dirent_lookup_cb, &c);
    if (err != APFSRW_OK && err != 1)
        return err;
    if (!c.found)
        return APFSRW_ENOENT;
    *file_id = c.file_id;
    if (type != NULL)
        *type = c.type;
    return APFSRW_OK;
}

/*
 * Resolve a '/'-separated path to a file id, starting from the root directory
 * (APFSRW_ROOT_FILEID, spec p.71 "File-System Objects": the root is always
 * object id 2). Each component is one DIR_REC lookup against the previous
 * component's id. Symlinks are not followed.
 */
static int resolve_path(struct apfsrw *fs, const char *path, uint64_t *file_id,
    uint8_t *type)
{
    uint64_t id = APFSRW_ROOT_FILEID;
    uint8_t t = APFSRW_DT_DIR;

    while (*path == '/')
        path++;
    while (*path != '\0') {
        const char *slash = strchr(path, '/');
        size_t len = slash != NULL ? (size_t)(slash - path) : strlen(path);
        int err;

        if (len == 0 || len > 255)
            return APFSRW_EINVAL;
        err = lookup_dirent(fs, id, path, len, &id, &t);
        if (err != APFSRW_OK)
            return err;
        path += len;
        while (*path == '/')
            path++;
    }
    *file_id = id;
    if (type != NULL)
        *type = t;
    return APFSRW_OK;
}

void *apfsrw_io_context(struct apfsrw *fs)
{
    return fs == NULL ? (void *)0 : fs->io_ctx;
}

#ifdef APFSRW_KERNEL
/*
 * Kernel entry point: the caller has already opened the device, so bind to it
 * and read the container exactly as the userspace path does.
 */
int apfsrw_open_kernel(void *io_ctx, uint64_t image_blocks, int writable,
    uint64_t xid, struct apfsrw **out)
{
    struct apfsrw *fs;
    int err;

    if (io_ctx == (void *)0 || out == (void *)0)
        return APFSRW_EINVAL;
    fs = calloc(1, sizeof(*fs));
    if (fs == NULL)
        return APFSRW_ENOMEM;
    fs->fd = -1;
    fs->io_ctx = io_ctx;
    fs->image_blocks = image_blocks;
    fs->writable = writable;
    fs->want_xid = xid;
    err = load_volume(fs);
    if (err != APFSRW_OK) {
        apfsrw_close(fs);
        return err;
    }
    *out = fs;
    return APFSRW_OK;
}
#endif

#ifndef APFSRW_KERNEL
int apfsrw_open(const char *path, int writable, struct apfsrw **out)
{
    return apfsrw_open_xid(path, writable, 0, out);
}

int apfsrw_open_xid(const char *path, int writable, uint64_t xid,
    struct apfsrw **out)
{
    struct apfsrw *fs;
    int flags = writable ? O_RDWR : O_RDONLY;
    int err;

    if (path == NULL || out == NULL)
        return APFSRW_EINVAL;
    fs = calloc(1, sizeof(*fs));
    if (fs == NULL)
        return APFSRW_ENOMEM;
    fs->fd = open(path, flags);
    if (fs->fd < 0) {
        free(fs);
        return APFSRW_EIO;
    }
    fs->writable = writable;
    fs->want_xid = xid;
    err = load_volume(fs);
    if (err != APFSRW_OK) {
        apfsrw_close(fs);
        return err;
    }
    *out = fs;
    return APFSRW_OK;
}

#endif /* !APFSRW_KERNEL */

void apfsrw_close(struct apfsrw *fs)
{
    if (fs == NULL)
        return;
#ifndef APFSRW_KERNEL
    if (fs->fd >= 0)
        close(fs->fd);
#endif
    free(fs->deferred);
    free(fs->alloced);
    free(fs);
}

const char *apfsrw_strerror(int error)
{
    switch (error) {
    case APFSRW_OK:
        return "ok";
    case APFSRW_EINVAL:
        return "invalid APFS image or argument";
    case APFSRW_EIO:
        return "I/O or checksum error";
    case APFSRW_ENOMEM:
        return "out of memory";
    case APFSRW_ENOENT:
        return "not found";
    case APFSRW_ENOTSUP:
        return "APFS feature not supported yet";
    case APFSRW_EOVERFLOW:
        return "APFS object too large";
    case APFSRW_ENOTDIR:
        return "not a directory";
    case APFSRW_ECOMPRESSED:
        return "file is decmpfs-compressed (not supported yet)";
    case APFSRW_EPERM:
        return "image not opened for writing";
    case APFSRW_EEXIST:
        return "already exists";
    case APFSRW_ENOSPC:
        return "no space left in container";
    default:
        return "unknown APFS error";
    }
}

int apfsrw_get_volume_info(struct apfsrw *fs, struct apfsrw_volume_info *info)
{
    if (fs == NULL || info == NULL)
        return APFSRW_EINVAL;
    memset(info, 0, sizeof(*info));
    info->block_size = fs->block_size;
    info->block_count = fs->block_count;
    info->xid = fs->xid;
    info->fs_oid = fs->fs_oid;
    info->fs_paddr = (uint64_t)fs->fs_paddr;
    info->root_tree_oid = fs->root_tree_oid;
    info->root_tree_paddr = (uint64_t)fs->root_tree_paddr;
    info->next_obj_id = rd64(&fs->apfs.apfs_next_obj_id);
    info->num_files = rd64(&fs->apfs.apfs_num_files);
    info->num_directories = rd64(&fs->apfs.apfs_num_directories);
    return APFSRW_OK;
}

struct list_ctx {
    uint64_t parent_id;
    apfsrw_dirent_cb cb;
    void *user;
};

static int list_leaf_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct list_ctx *c = (struct list_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        uint64_t key;
        uint32_t name_len;
        struct apfsrw_dirent entry;
        const struct apfs_j_drec_val *drec;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            return err;
        if (key_len < 13 || val_len < sizeof(*drec))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_id(key) != c->parent_id ||
            key_type(key) != APFS_TYPE_DIR_REC)
            continue;
        name_len = rd32((const uint8_t *)keyp + 8) & 0x3ffU;
        if (name_len == 0 || name_len > sizeof(entry.name) ||
            12U + name_len > key_len)
            continue;
        drec = (const struct apfs_j_drec_val *)valp;
        memset(&entry, 0, sizeof(entry));
        entry.file_id = rd64(&drec->file_id);
        entry.type = (uint8_t)rd16(&drec->flags);
        memcpy(entry.name, (const uint8_t *)keyp + 12, name_len);
        entry.name[sizeof(entry.name) - 1] = '\0';
        err = c->cb(&entry, c->user);
        if (err != 0)
            return err;
    }
    return APFSRW_OK;
}

struct read_extents_ctx {
    uint64_t stream_id;
    uint8_t *buf;
    uint64_t size;
    uint64_t nextents;
    int err;
};

/*
 * A data stream is described by one FILE_EXTENT record per extent, keyed by
 * (stream id, logical address within the file) - so a file is assembled by
 * copying each extent to its own logical offset rather than assuming a single
 * contiguous run (spec p.102 j_file_extent_key_t / j_file_extent_val_t).
 * The low 56 bits of len_and_flags are the length in BYTES (spec p.103
 * J_FILE_EXTENT_LEN_MASK). A phys_block_num of 0 marks a hole.
 */
static int read_extents_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct read_extents_ctx *c = (struct read_extents_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        const struct apfs_j_file_extent_key *key;
        const struct apfs_j_file_extent_val *val;
        uint64_t logical, len, phys;
        ssize_t n;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            return err;
        if (key_len < sizeof(*key) || val_len < sizeof(*val))
            continue;
        key = (const struct apfs_j_file_extent_key *)keyp;
        if (key_id(rd64(&key->hdr.obj_id_and_type)) != c->stream_id ||
            key_type(rd64(&key->hdr.obj_id_and_type)) != APFS_TYPE_FILE_EXTENT)
            continue;

        c->nextents++;
        val = (const struct apfs_j_file_extent_val *)valp;
        logical = rd64(&key->logical_addr);
        len = rd64(&val->len_and_flags) & APFS_FILE_EXTENT_LEN_MASK;
        phys = rd64(&val->phys_block_num);
        if (logical >= c->size)
            continue;
        if (len > c->size - logical)
            len = c->size - logical;
        if (phys == 0)
            continue;                    /* sparse hole: already zeroed */
        n = apfsrw_pread(fs, c->buf + logical, (size_t)len,
            (off_t)(phys * fs->block_size));
        if (n < 0 || (uint64_t)n != len) {
            c->err = APFSRW_EIO;
            return 1;
        }
    }
    return APFSRW_OK;
}

static int read_extents(struct apfsrw *fs, uint64_t stream_id, uint8_t *buf,
    uint64_t size)
{
    struct read_extents_ctx c;
    int err;

    c.stream_id = stream_id;
    c.buf = buf;
    c.size = size;
    c.nextents = 0;
    c.err = APFSRW_OK;
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, stream_id, stream_id,
        read_extents_cb, &c);
    if (err != APFSRW_OK && err != 1)
        return err;
    if (c.err == APFSRW_OK && c.nextents == 0)
        /*
         * A non-empty stream with no FILE_EXTENT records at all is stored
         * inline in a decmpfs extended attribute rather than in extents.
         * Reporting that is far better than handing back a zero-filled buffer.
         */
        return APFSRW_ECOMPRESSED;
    return c.err;
}

struct stat_ctx {
    uint64_t stream_id;
    uint64_t nextents;
    uint64_t nholes;
    uint64_t bytes;
    uint64_t first_phys;
    uint64_t first_len;
};

static int stat_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct stat_ctx *c = (struct stat_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        const struct apfs_j_file_extent_key *key;
        const struct apfs_j_file_extent_val *val;
        uint64_t len, phys;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp, &val_len);
        if (err != APFSRW_OK)
            return err;
        if (key_len < sizeof(*key) || val_len < sizeof(*val))
            continue;
        key = (const struct apfs_j_file_extent_key *)keyp;
        if (key_id(rd64(&key->hdr.obj_id_and_type)) != c->stream_id ||
            key_type(rd64(&key->hdr.obj_id_and_type)) != APFS_TYPE_FILE_EXTENT)
            continue;
        val = (const struct apfs_j_file_extent_val *)valp;
        len = rd64(&val->len_and_flags) & APFS_FILE_EXTENT_LEN_MASK;
        phys = rd64(&val->phys_block_num);
        if (c->nextents == 0) {
            c->first_phys = phys;
            c->first_len = len;
        }
        c->nextents++;
        c->bytes += len;
        if (phys == 0)
            c->nholes++;
    }
    return APFSRW_OK;
}

/*
 * Extended attributes are j_xattr_key_t { j_key_t hdr; uint16 name_len;
 * char name[] } / j_xattr_val_t { uint16 flags; uint16 xdata_len; uint8
 * xdata[] } (spec p.82). name_len INCLUDES the trailing NUL.
 */
struct xattr_ctx {
    uint64_t file_id;
    const char *want;               /* NULL = enumerate */
    apfsrw_xattr_cb cb;
    void *user;
    uint16_t flags;
    uint16_t len;
    uint8_t data[512];
    int found;
};

static int xattr_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct xattr_ctx *c = (struct xattr_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        const struct apfs_j_xattr_val *xv;
        uint64_t key;
        uint16_t name_len;
        const char *name;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp, &val_len);
        if (err != APFSRW_OK)
            return err;
        if (key_len < 10 || val_len < sizeof(*xv))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_id(key) != c->file_id || key_type(key) != APFS_TYPE_XATTR)
            continue;
        name_len = rd16((const uint8_t *)keyp + 8);
        if (name_len == 0 || 10U + name_len > key_len)
            continue;
        name = (const char *)keyp + 10;
        xv = (const struct apfs_j_xattr_val *)valp;

        if (c->want == NULL) {
            struct apfsrw_xattr e;

            memset(&e, 0, sizeof(e));
            snprintf(e.name, sizeof(e.name), "%s", name);
            e.flags = rd16(&xv->flags);
            e.size = rd16(&xv->xdata_len);
            err = c->cb(&e, c->user);
            if (err != 0)
                return err;
            continue;
        }
        if (strcmp(name, c->want) != 0)
            continue;
        c->flags = rd16(&xv->flags);
        c->len = rd16(&xv->xdata_len);
        if (c->len > sizeof(c->data))
            c->len = (uint16_t)sizeof(c->data);
        if (4U + c->len > val_len)
            continue;
        memcpy(c->data, xv->xdata, c->len);
        c->found = 1;
        return 1;
    }
    return APFSRW_OK;
}

static int lookup_xattr(struct apfsrw *fs, uint64_t file_id, const char *name,
    struct xattr_ctx *c)
{
    int err;

    memset(c, 0, sizeof(*c));
    c->file_id = file_id;
    c->want = name;
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, file_id, file_id,
        xattr_cb, c);
    if (err != APFSRW_OK && err != 1)
        return err;
    return c->found ? APFSRW_OK : APFSRW_ENOENT;
}

/*
 * Histogram decmpfs compression types across the whole volume, so the codecs
 * worth implementing are chosen from measurement rather than assumption.
 */
struct cmpstat_ctx {
    struct apfsrw_compression_stats *st;
};

static int cmpstat_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct cmpstat_ctx *c = (struct cmpstat_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len, val_len, name_len;
        const struct apfs_j_xattr_val *xv;
        const struct apfs_decmpfs_header *h;
        uint64_t key;
        uint32_t type;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp, &val_len);
        if (err != APFSRW_OK)
            return err;
        if (key_len < 10 || val_len < sizeof(*xv))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_type(key) != APFS_TYPE_XATTR)
            continue;
        name_len = rd16((const uint8_t *)keyp + 8);
        if (name_len == 0 || 10U + name_len > key_len)
            continue;
        if (strcmp((const char *)keyp + 10, "com.apple.decmpfs") != 0)
            continue;
        xv = (const struct apfs_j_xattr_val *)valp;
        if (rd16(&xv->xdata_len) < sizeof(*h) ||
            4U + sizeof(*h) > val_len)
            continue;
        h = (const struct apfs_decmpfs_header *)xv->xdata;
        if (rd32(&h->compression_magic) != APFS_DECMPFS_MAGIC)
            continue;
        type = rd32(&h->compression_type);
        c->st->total++;
        c->st->bytes += rd64(&h->uncompressed_size);
        if (type < APFSRW_MAX_COMPRESSION_TYPES) {
            uint64_t usz = rd64(&h->uncompressed_size);
            uint32_t ilen = (uint32_t)rd16(&xv->xdata_len) -
                (uint32_t)sizeof(*h);

            if (c->st->count[type] == 0) {
                c->st->sample_id[type] = key_id(key);
                c->st->sample_size[type] = usz;
                c->st->sample_xdata[type] = rd16(&xv->xdata_len);
            }
            if (ilen == usz + 1U) {
                c->st->inline_plus1[type]++;
                if (4U + sizeof(*h) < val_len)
                    c->st->head_hist[type][h->attr_bytes[0]]++;
            }
            c->st->count[type]++;
        } else {
            c->st->other++;
        }
    }
    return APFSRW_OK;
}

int apfsrw_compression_stats(struct apfsrw *fs,
    struct apfsrw_compression_stats *out)
{
    struct cmpstat_ctx c;

    if (fs == NULL || out == NULL)
        return APFSRW_EINVAL;
    memset(out, 0, sizeof(*out));
    c.st = out;
    return btree_walk_leaves(fs, fs->root_tree_paddr, cmpstat_cb, &c);
}

int apfsrw_list_xattrs(struct apfsrw *fs, const char *path,
    apfsrw_xattr_cb cb, void *ctx)
{
    struct xattr_ctx c;
    uint64_t file_id;
    int err;

    if (fs == NULL || path == NULL || cb == NULL)
        return APFSRW_EINVAL;
    err = resolve_path(fs, path, &file_id, NULL);
    if (err != APFSRW_OK)
        return err;
    memset(&c, 0, sizeof(c));
    c.file_id = file_id;
    c.want = NULL;
    c.cb = cb;
    c.user = ctx;
    err = btree_walk_leaves(fs, fs->root_tree_paddr, xattr_cb, &c);
    return (err == 1) ? APFSRW_OK : err;
}

int apfsrw_stat_file(struct apfsrw *fs, const char *path,
    struct apfsrw_file_stat *out)
{
    struct stat_ctx c;
    uint64_t file_id, size, stream_id;
    int err;

    if (fs == NULL || path == NULL || out == NULL)
        return APFSRW_EINVAL;
    err = resolve_path(fs, path, &file_id, NULL);
    if (err != APFSRW_OK)
        return err;
    err = lookup_inode(fs, file_id, &size, &stream_id);
    if (err != APFSRW_OK)
        return err;
    memset(&c, 0, sizeof(c));
    c.stream_id = stream_id;
    err = btree_walk_leaves(fs, fs->root_tree_paddr, stat_cb, &c);
    if (err != APFSRW_OK && err != 1)
        return err;
    out->file_id = file_id;
    out->stream_id = stream_id;
    out->size = size;
    out->num_extents = c.nextents;
    out->num_holes = c.nholes;
    out->extent_bytes = c.bytes;
    out->first_phys_block = c.first_phys;
    out->first_extent_len = c.first_len;
    {
        struct xattr_ctx xc;
        const struct apfs_decmpfs_header *h;

        out->compression_type = 0;
        out->compressed_size = 0;
        if (lookup_xattr(fs, file_id, "com.apple.decmpfs", &xc) == APFSRW_OK &&
            xc.len >= sizeof(*h)) {
            h = (const struct apfs_decmpfs_header *)xc.data;
            if (rd32(&h->compression_magic) == APFS_DECMPFS_MAGIC) {
                out->compression_type = rd32(&h->compression_type);
                out->size = rd64(&h->uncompressed_size);
                out->inline_bytes = (uint32_t)(xc.len - sizeof(*h));
            }
        }
        if (lookup_xattr(fs, file_id, "com.apple.ResourceFork", &xc) ==
            APFSRW_OK && xc.len >= sizeof(struct apfs_j_xattr_dstream)) {
            const struct apfs_j_xattr_dstream *ds =
                (const struct apfs_j_xattr_dstream *)xc.data;

            out->compressed_size = rd64(&ds->size);
        }
    }
    return APFSRW_OK;
}

/*
 * Ephemeral objects (the space manager, the reaper) are not in any object map;
 * they live in the checkpoint DATA area and are found through the checkpoint
 * MAPS stored in the descriptor area (spec p.26 mount step 5; p.40-41
 * checkpoint_map_phys_t / checkpoint_mapping_t). The descriptor area is a ring,
 * so the current checkpoint occupies nx_xp_desc_len blocks starting at
 * nx_xp_desc_index, wrapping modulo nx_xp_desc_blocks.
 *
 * Resolving through the map beats scanning the device: the kext PoC finds the
 * space manager by reading every block in the container, which is 3.2M reads on
 * a 13 GB image.
 */
static int resolve_ephemeral(struct apfsrw *fs, apfs_oid_t oid,
    apfs_paddr_t *out)
{
    uint64_t desc_base = rd64(&fs->nx.nx_xp_desc_base);
    uint32_t desc_blocks = rd32(&fs->nx.nx_xp_desc_blocks) & 0x7fffffffU;
    uint32_t index = rd32(&fs->nx.nx_xp_desc_index);
    uint32_t len = rd32(&fs->nx.nx_xp_desc_len);
    uint8_t *block;
    uint32_t i;
    int err = APFSRW_ENOENT;

    if (desc_blocks == 0 || len == 0)
        return APFSRW_ENOENT;
    block = calloc(1, fs->block_size);
    if (block == NULL)
        return APFSRW_ENOMEM;

    for (i = 0; i < len; i++) {
        const struct apfs_checkpoint_map_phys *cpm;
        uint32_t count, j;
        uint32_t slot = (index + i) % desc_blocks;

        if (read_object(fs, (apfs_paddr_t)(desc_base + slot), block) !=
            APFSRW_OK)
            continue;
        cpm = (const struct apfs_checkpoint_map_phys *)block;
        if (object_type(cpm->cpm_o.o_type) != APFS_OBJECT_TYPE_CHECKPOINT_MAP)
            continue;                    /* the superblock itself */
        count = rd32(&cpm->cpm_count);
        if (count > (fs->block_size - sizeof(*cpm)) /
            sizeof(struct apfs_checkpoint_mapping))
            continue;
        for (j = 0; j < count; j++) {
            if (rd64(&cpm->cpm_map[j].cpm_oid) != oid)
                continue;
            *out = (apfs_paddr_t)rd64(&cpm->cpm_map[j].cpm_paddr);
            err = APFSRW_OK;
            goto out;
        }
    }
out:
    free(block);
    return err;
}

/*
 * Walk the space manager's allocation bitmaps. sm_dev[SD_MAIN] describes the
 * main device; the CIB addresses are an array of paddr_t at sm_addr_offset from
 * the start of the spaceman block, each pointing at a chunk_info_block whose
 * chunk_info entries each own one bitmap block (spec p.159-161).
 */
static int spaceman_walk(struct apfsrw *fs, struct apfsrw_space_info *out,
    int (*chunk_cb)(struct apfsrw *, const struct apfs_chunk_info *, void *),
    void *ctx)
{
    struct apfs_spaceman_phys *sm = NULL;
    struct apfs_chunk_info_block *cib = NULL;
    apfs_paddr_t sm_paddr = 0;
    uint32_t cib_count, addr_offset, i;
    int err;

    err = resolve_ephemeral(fs, rd64(&fs->nx.nx_spaceman_oid), &sm_paddr);
    if (err != APFSRW_OK)
        return err;

    sm = calloc(1, fs->block_size);
    cib = calloc(1, fs->block_size);
    if (sm == NULL || cib == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }
    err = read_object(fs, sm_paddr, sm);
    if (err != APFSRW_OK)
        goto out;
    if (object_type(sm->sm_o.o_type) != APFS_OBJECT_TYPE_SPACEMAN ||
        rd32(&sm->sm_block_size) != fs->block_size) {
        err = APFSRW_EINVAL;
        goto out;
    }
    if (rd32(&sm->sm_dev[0].sm_cab_count) != 0) {
        err = APFSRW_ENOTSUP;            /* CAB indirection not handled */
        goto out;
    }

    cib_count = rd32(&sm->sm_dev[0].sm_cib_count);
    addr_offset = rd32(&sm->sm_dev[0].sm_addr_offset);
    if ((uint64_t)addr_offset + (uint64_t)cib_count * sizeof(apfs_paddr_t) >
        fs->block_size) {
        err = APFSRW_EINVAL;
        goto out;
    }

    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->spaceman_paddr = (uint64_t)sm_paddr;
        out->block_count = rd64(&sm->sm_dev[0].sm_block_count);
        out->chunk_count = rd64(&sm->sm_dev[0].sm_chunk_count);
        out->cib_count = cib_count;
        out->free_count = rd64(&sm->sm_dev[0].sm_free_count);
        out->blocks_per_chunk = rd32(&sm->sm_blocks_per_chunk);
    }

    for (i = 0; i < cib_count; i++) {
        apfs_paddr_t cib_paddr;
        uint32_t chunks, c;

        memcpy(&cib_paddr, (const uint8_t *)sm + addr_offset +
            i * sizeof(cib_paddr), sizeof(cib_paddr));
        cib_paddr = (apfs_paddr_t)rd64(&cib_paddr);
        if (cib_paddr <= 0)
            continue;
        err = read_object(fs, cib_paddr, cib);
        if (err != APFSRW_OK)
            goto out;
        if (object_type(cib->cib_o.o_type) != APFS_OBJECT_TYPE_SPACEMAN_CIB) {
            err = APFSRW_EINVAL;
            goto out;
        }
        chunks = rd32(&cib->cib_chunk_info_count);
        if (chunks > (fs->block_size - sizeof(*cib)) /
            sizeof(struct apfs_chunk_info)) {
            err = APFSRW_EINVAL;
            goto out;
        }
        for (c = 0; c < chunks; c++) {
            const struct apfs_chunk_info *ci = &cib->cib_chunk_info[c];

            if (out != NULL) {
                out->chunks_seen++;
                out->blocks_seen += rd32(&ci->ci_block_count);
                out->free_seen += rd32(&ci->ci_free_count);
            }
            if (chunk_cb != NULL) {
                err = chunk_cb(fs, ci, ctx);
                if (err != APFSRW_OK)
                    goto out;
            }
        }
    }
    err = APFSRW_OK;
out:
    free(cib);
    free(sm);
    return err;
}

int apfsrw_get_space_info(struct apfsrw *fs, struct apfsrw_space_info *out)
{
    if (fs == NULL || out == NULL)
        return APFSRW_EINVAL;
    return spaceman_walk(fs, out, NULL, NULL);
}

int apfsrw_list_dir(struct apfsrw *fs, const char *path, apfsrw_dirent_cb cb,
    void *ctx)
{
    struct list_ctx c;
    uint64_t id;
    uint8_t type;
    int err;

    if (fs == NULL || cb == NULL)
        return APFSRW_EINVAL;
    if (path == NULL)
        path = "/";
    err = resolve_path(fs, path, &id, &type);
    if (err != APFSRW_OK)
        return err;
    if (type != APFSRW_DT_DIR)
        return APFSRW_ENOTDIR;
    c.parent_id = id;
    c.cb = cb;
    c.user = ctx;
    return btree_walk_leaves_oid(fs, fs->root_tree_paddr, id, id,
        list_leaf_cb, &c);
}

int apfsrw_list_root(struct apfsrw *fs, apfsrw_dirent_cb cb, void *ctx)
{
    return apfsrw_list_dir(fs, "/", cb, ctx);
}

/*
 * A decmpfs-compressed file keeps a 16-byte decmpfs_disk_header in the
 * 'com.apple.decmpfs' xattr. Type 1 means the payload is stored UNCOMPRESSED
 * in that same xattr, so it can be returned directly (xnu decmpfs.h,
 * CMP_Type1). Every other type needs a codec Apple documents nowhere; for
 * those, report the type rather than inventing data.
 */
/*
 * The compressed payload of a large decmpfs file lives in the
 * 'com.apple.ResourceFork' xattr, which carries XATTR_DATA_STREAM and holds a
 * j_xattr_dstream_t (spec p.106): an xattr_obj_id plus a j_dstream_t. The
 * bytes are therefore an ordinary data stream - read its FILE_EXTENT records
 * keyed by xattr_obj_id, exactly like a file's.
 */
static int read_resource_fork(struct apfsrw *fs, uint64_t file_id,
    uint8_t **data_out, uint64_t *size_out)
{
    struct xattr_ctx c;
    const struct apfs_j_xattr_dstream *ds;
    uint64_t stream_id, size;
    uint8_t *data;
    int err;

    err = lookup_xattr(fs, file_id, "com.apple.ResourceFork", &c);
    if (err != APFSRW_OK)
        return err;
    if ((c.flags & APFS_XATTR_DATA_STREAM) == 0 || c.len < sizeof(*ds)) {
        return APFSRW_ENOTSUP;
    }
    ds = (const struct apfs_j_xattr_dstream *)c.data;
    stream_id = rd64(&ds->xattr_obj_id);
    size = rd64(&ds->size);
    if (size == 0 || size >= SIZE_MAX)
        return APFSRW_EINVAL;
    data = calloc(1, (size_t)size + 1);
    if (data == NULL)
        return APFSRW_ENOMEM;
    err = read_extents(fs, stream_id, data, size);
    if (err != APFSRW_OK) {
        free(data);
        return err;
    }
    *data_out = data;
    *size_out = size;
    return APFSRW_OK;
}

int apfsrw_read_resource_fork(struct apfsrw *fs, const char *path,
    uint8_t **data_out, size_t *size_out)
{
    uint64_t file_id, size;
    uint8_t *data;
    int err;

    if (fs == NULL || path == NULL || data_out == NULL || size_out == NULL)
        return APFSRW_EINVAL;
    err = resolve_path(fs, path, &file_id, NULL);
    if (err != APFSRW_OK)
        return err;
    err = read_resource_fork(fs, file_id, &data, &size);
    if (err != APFSRW_OK)
        return err;
    *data_out = data;
    *size_out = (size_t)size;
    return APFSRW_OK;
}

/*
 * Type 8: the ResourceFork stream holds (nchunks + 1) little-endian uint32
 * offsets from the start of the stream, followed by the chunk data. Chunk i is
 * [off[i], off[i+1]) and expands to at most APFS_DECMPFS_CHUNK bytes. A chunk
 * whose first byte is 0x06 is stored raw rather than LZVN-compressed.
 * Measured on 25G83 - this layout is not in the APFS spec. See apfs-notes.txt.
 */
static int read_decmpfs_rsrc(struct apfsrw *fs, uint64_t file_id,
    uint64_t size, uint32_t type, uint8_t **data_out, size_t *size_out)
{
    uint8_t *rf = NULL;
    uint8_t *out;
    uint64_t rf_size = 0;
    uint64_t nchunks, i, done = 0;
    int err;

    err = read_resource_fork(fs, file_id, &rf, &rf_size);
    if (err != APFSRW_OK)
        return err;

    nchunks = (size + APFS_DECMPFS_CHUNK - 1) / APFS_DECMPFS_CHUNK;
    if (nchunks == 0 || (nchunks + 1) * 4U > rf_size) {
        free(rf);
        return APFSRW_ECOMPRESSED;
    }
    /* The first offset must point past the table, or this is not the layout. */
    if (rd32(rf) != (nchunks + 1) * 4U) {
        free(rf);
        return APFSRW_ECOMPRESSED;
    }

    out = malloc((size_t)size + 1);
    if (out == NULL) {
        free(rf);
        return APFSRW_ENOMEM;
    }

    for (i = 0; i < nchunks; i++) {
        uint32_t start = rd32(rf + i * 4U);
        uint32_t end = rd32(rf + (i + 1) * 4U);
        uint64_t want = size - done;
        size_t got;

        if (want > APFS_DECMPFS_CHUNK)
            want = APFS_DECMPFS_CHUNK;
        if (end < start || end > rf_size) {
            err = APFSRW_ECOMPRESSED;
            goto fail;
        }
        if (end - start == want + 1U) {
            /*
             * Raw chunk: one marker byte then the bytes verbatim. Detect it by
             * LENGTH, not by the marker value - LZVN resource forks use 0x06
             * and LZBITMAP ones use 0xff, so keying on a single byte wrongly
             * rejects the other. A compressed chunk that happened to be
             * exactly want+1 bytes would fail to decode anyway.
             */
            memcpy(out + done, rf + start + 1, (size_t)want);
            got = (size_t)want;
#ifdef APFSRW_KERNEL
        } else {
            err = APFSRW_ECOMPRESSED;
            goto fail;
#else
        } else if (type == APFS_DECMPFS_TYPE_LZBITMAP_RSRC) {
            size_t n = 0;
            int zerr = zbm_decompress(out + done, (size_t)want, rf + start,
                end - start, &n);

            if (zerr != 0) {
                if (apfsrw_getenv("APFSRW_DEBUG") != NULL)
                    fprintf(stderr, "zbm chunk %llu/%llu rc=%d start=%u "
                        "clen=%u want=%llu head=%02x %02x %02x %02x\n",
                        (unsigned long long)i, (unsigned long long)nchunks,
                        zerr, start, end - start,
                        (unsigned long long)want, rf[start], rf[start + 1],
                        rf[start + 2], rf[start + 3]);
                err = APFSRW_ECOMPRESSED;
                goto fail;
            }
            got = n;
        } else {
            got = apfsrw_lzvn_decode(out + done, (size_t)want, rf + start,
                end - start);
#endif
        }
        if (got != want) {
            err = APFSRW_ECOMPRESSED;
            goto fail;
        }
        done += got;
    }

    free(rf);
    out[size] = '\0';
    *data_out = out;
    *size_out = (size_t)size;
    return APFSRW_OK;

fail:
    free(out);
    free(rf);
    return err;
}

static int read_decmpfs(struct apfsrw *fs, uint64_t file_id, uint64_t size,
    uint8_t **data_out, size_t *size_out)
{
    struct xattr_ctx c;
    const struct apfs_decmpfs_header *h;
    uint32_t type;
    uint32_t inline_len;
    uint8_t *data;
    int err;

    err = lookup_xattr(fs, file_id, "com.apple.decmpfs", &c);
    if (err != APFSRW_OK)
        return APFSRW_ECOMPRESSED;
    if (c.len < sizeof(*h))
        return APFSRW_ECOMPRESSED;
    h = (const struct apfs_decmpfs_header *)c.data;
    if (rd32(&h->compression_magic) != APFS_DECMPFS_MAGIC)
        return APFSRW_ECOMPRESSED;
    type = rd32(&h->compression_type);
    inline_len = (uint32_t)(c.len - sizeof(*h));

    if (apfsrw_getenv("APFSRW_DEBUG") != NULL) {
        uint32_t n = inline_len < 16 ? inline_len : 16;
        uint32_t j;

        fprintf(stderr, "decmpfs: type=%u size=%llu inline_len=%u head=",
            type, (unsigned long long)size, inline_len);
        for (j = 0; j < n; j++)
            fprintf(stderr, "%02x ", h->attr_bytes[j]);
        fprintf(stderr, "\n");
    }

    if (type == APFS_DECMPFS_TYPE_UNCOMPRESSED) {
        if (size > inline_len)
            return APFSRW_ECOMPRESSED;
        data = malloc((size_t)size + 1);
        if (data == NULL)
            return APFSRW_ENOMEM;
        memcpy(data, h->attr_bytes, (size_t)size);
        data[size] = '\0';
        *data_out = data;
        *size_out = (size_t)size;
        return APFSRW_OK;
    }

    if (size >= SIZE_MAX)
        return APFSRW_EOVERFLOW;

    if (type == APFS_DECMPFS_TYPE_RAW_XATTR) {
        /*
         * One marker byte then the content verbatim. Measured, not spec: all
         * 28913 type-9 files on the 25G83 System volume have
         * inline_len == size + 1 and a first byte of 0xcc, with no other value
         * occurring. It is NOT an LZVN stream - 0xcc decodes as sml_d, a match
         * opcode, which cannot begin a stream with empty history.
         */
        if (inline_len != size + 1U ||
            h->attr_bytes[0] != APFS_DECMPFS_RAW_MARKER)
            return APFSRW_ECOMPRESSED;
        data = malloc((size_t)size + 1);
        if (data == NULL)
            return APFSRW_ENOMEM;
        memcpy(data, h->attr_bytes + 1, (size_t)size);
        data[size] = '\0';
        *data_out = data;
        *size_out = (size_t)size;
        return APFSRW_OK;
    }

    if (type == APFS_DECMPFS_TYPE_LZVN_RSRC ||
        type == APFS_DECMPFS_TYPE_LZBITMAP_RSRC)
        return read_decmpfs_rsrc(fs, file_id, size, type, data_out, size_out);

    return APFSRW_ECOMPRESSED;
}

int apfsrw_read_root_file(struct apfsrw *fs, const char *path,
    uint8_t **data_out, size_t *size_out)
{
    return apfsrw_read_file(fs, path, data_out, size_out);
}

int apfsrw_read_file(struct apfsrw *fs, const char *path,
    uint8_t **data_out, size_t *size_out)
{
    uint64_t file_id;
    int err;

    if (fs == NULL || path == NULL || data_out == NULL || size_out == NULL)
        return APFSRW_EINVAL;
    err = resolve_path(fs, path, &file_id, NULL);
    if (err != APFSRW_OK)
        return err;
    return apfsrw_read_file_by_id(fs, file_id, data_out, size_out);
}

int apfsrw_read_file_by_id(struct apfsrw *fs, uint64_t file_id,
    uint8_t **data_out, size_t *size_out)
{
    uint64_t file_size;
    uint64_t stream_id;
    uint8_t *data;
    int err;

    if (fs == NULL || data_out == NULL || size_out == NULL)
        return APFSRW_EINVAL;
    err = lookup_inode(fs, file_id, &file_size, &stream_id);
    if (err != APFSRW_OK)
        return err;
    if (file_size == 0) {
        /* An empty file has no file-extent record to find. */
        data = malloc(1);
        if (data == NULL)
            return APFSRW_ENOMEM;
        data[0] = '\0';
        *data_out = data;
        *size_out = 0;
        return APFSRW_OK;
    }
    if (file_size >= SIZE_MAX)
        return APFSRW_EOVERFLOW;
    data = calloc(1, (size_t)file_size + 1);
    if (data == NULL)
        return APFSRW_ENOMEM;
    err = read_extents(fs, stream_id, data, file_size);
    if (err == APFSRW_ECOMPRESSED) {
        free(data);
        return read_decmpfs(fs, file_id, file_size, data_out, size_out);
    }
    if (err != APFSRW_OK) {
        free(data);
        return err;
    }
    data[file_size] = '\0';
    *data_out = data;
    *size_out = (size_t)file_size;
    return APFSRW_OK;
}

/* -- WRITE -- */

static void wr16(void *p, uint16_t v)
{
    uint8_t *b = (uint8_t *)p;

    b[0] = (uint8_t)(v & 0xff);
    b[1] = (uint8_t)(v >> 8);
}

static void wr32(void *p, uint32_t v)
{
    uint8_t *b = (uint8_t *)p;
    int i;

    for (i = 0; i < 4; i++)
        b[i] = (uint8_t)(v >> (8 * i));
}

static void wr64(void *p, uint64_t v)
{
    uint8_t *b = (uint8_t *)p;
    int i;

    for (i = 0; i < 8; i++)
        b[i] = (uint8_t)(v >> (8 * i));
}

static int write_raw(struct apfsrw *fs, apfs_paddr_t paddr, const void *buf)
{
    ssize_t n;

    if (!fs->writable)
        return APFSRW_EPERM;
    if (paddr < 0 || (uint64_t)paddr >= fs->block_count)
        return APFSRW_EINVAL;
    n = apfsrw_pwrite(fs, buf, fs->block_size,
        (off_t)((uint64_t)paddr * fs->block_size));
    if (n < 0 || (size_t)n != fs->block_size)
        return APFSRW_EIO;
    return APFSRW_OK;
}

static int write_block(struct apfsrw *fs, apfs_paddr_t paddr, const void *buf)
{
    ssize_t n;

    if (!fs->writable)
        return APFSRW_EPERM;
    if (paddr < 0 || (uint64_t)paddr >= fs->block_count)
        return APFSRW_EINVAL;
    n = apfsrw_pwrite(fs, buf, fs->block_size,
        (off_t)((uint64_t)paddr * fs->block_size));
    if (n < 0 || (size_t)n != fs->block_size)
        return APFSRW_EIO;
    return APFSRW_OK;
}

/* Recompute o_cksum over the object, excluding the checksum field itself
 * (spec p.10 obj_phys_t o_cksum; algorithm from the PureDarwin kext). */
static void seal_object(struct apfsrw *fs, void *block)
{
    wr64(block, fletcher64((const uint8_t *)block, fs->block_size));
}

/*
 * CRC-32C (Castagnoli), reflected form. Used only for the dirent name hash.
 */
static uint32_t crc32c(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xffffffffU;
    size_t i;
    int k;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0x82f63b78U & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return crc ^ 0xffffffffU;
}

/*
 * j_drec_hashed_key_t.name_len_and_hash (spec p.78-79): the low 10 bits are the
 * name length INCLUDING its NUL, and bits 31:10 are a 22-bit hash computed by
 * normalizing the name to NFD, representing it as UTF-32, taking CRC-32C,
 * complementing, and keeping the low 22 bits.
 *
 * Two details were settled against real Apple-formatted volumes rather than the
 * text, which is ambiguous or silent on both:
 *   - the trailing NUL is NOT included in the CRC input (verified 4/4 on one
 *     volume, 6/6 on another);
 *   - on a volume with APFS_INCOMPAT_CASE_INSENSITIVE the name is LOWERCASED
 *     before hashing (proved by MiXeDcAsE / UPPER / Hello.TXT, which match only
 *     the lowercased hash).
 * NFD normalization and Unicode case folding need tables we do not have, so
 * non-ASCII names are refused rather than hashed wrongly - a wrong hash makes
 * the file invisible to macOS instead of failing loudly.
 */
static int drec_name_hash(struct apfsrw *fs, const char *name, size_t len,
    uint32_t *out)
{
    uint8_t utf32[4 * 256];
    size_t i;
    int fold = (rd64(&fs->apfs.apfs_incompatible_features) &
        APFS_INCOMPAT_CASE_INSENSITIVE) != 0;

    if (len == 0 || len > 255)
        return APFSRW_EINVAL;
    for (i = 0; i < len; i++) {
        uint8_t c = (uint8_t)name[i];

        if (c >= 0x80)
            return APFSRW_ENOTSUP;       /* needs NFD + Unicode case folding */
        if (fold && c >= 'A' && c <= 'Z')
            c = (uint8_t)(c - 'A' + 'a');
        wr32(utf32 + i * 4, c);
    }
    *out = (~crc32c(utf32, len * 4)) & 0x003fffffU;
    return APFSRW_OK;
}

/*
 * Allocate a run of n contiguous blocks from the space manager.
 *
 * Walks sm_dev[SD_MAIN]'s chunk_info_block array, finds a run of clear bits in
 * a chunk's bitmap, sets them, and decrements ci_free_count and sm_free_count
 * to match (spec p.159-161). The space manager is ephemeral, so the updated
 * copy is written back where it already lives in the checkpoint data area.
 */
static int spaceman_set_range(struct apfsrw *fs, uint64_t first, uint32_t n,
    int set);

/* Release blocks back to the allocator. Real APFS defers reuse through the
 * space manager's free queue so an older checkpoint stays readable; this frees
 * immediately, which is why rolling back to a previous checkpoint after a write
 * is NOT safe here. Without it fsck_apfs reports the superseded metadata blocks
 * as "overallocation". */
static int free_blocks(struct apfsrw *fs, uint64_t first, uint32_t n)
{
    return spaceman_set_range(fs, first, n, 0);
}

/*
 * Take a block from the space manager's INTERNAL POOL, which is where chunk
 * allocation bitmaps have to live - apfsck rejects one anywhere else with
 * "Out-of-range ip block number".
 *
 * The pool keeps a ring of 16 bitmap versions; sm_ip_bitmap_offset points at an
 * array of u16 indices into that ring, and entry 0 names the CURRENT bitmap.
 * Only that bitmap's contents are touched here, so the ring structure and its
 * free list stay as they are. The pool's blocks are already marked used in the
 * main allocation bitmap, so nothing else needs updating.
 */
static int alloc_ip_block(struct apfsrw *fs, struct apfs_spaceman_phys *sm,
    apfs_paddr_t *out)
{
    uint8_t *bm;
    apfs_paddr_t bm_base = (apfs_paddr_t)rd64(&sm->sm_ip_bm_base);
    apfs_paddr_t ip_base = (apfs_paddr_t)rd64(&sm->sm_ip_base);
    uint64_t ip_count = rd64(&sm->sm_ip_block_count);
    uint32_t bmoff = rd32(&sm->sm_ip_bitmap_offset);
    uint16_t idx;
    uint64_t i;
    int err = APFSRW_ENOSPC;

    if (bmoff + sizeof(uint16_t) > fs->block_size || ip_count == 0)
        return APFSRW_EINVAL;
    idx = rd16((const uint8_t *)sm + bmoff);
    if (idx >= rd32(&sm->sm_ip_bm_block_count))
        return APFSRW_EINVAL;

    bm = calloc(1, fs->block_size);
    if (bm == NULL)
        return APFSRW_ENOMEM;
    if (read_raw(fs, bm_base + idx, bm) != APFSRW_OK) {
        free(bm);
        return APFSRW_EIO;
    }
    for (i = 0; i < ip_count; i++) {
        if (bm[i >> 3] & (uint8_t)(1U << (i & 7)))
            continue;
        bm[i >> 3] |= (uint8_t)(1U << (i & 7));
        err = write_raw(fs, bm_base + idx, bm);
        if (err == APFSRW_OK)
            *out = ip_base + (apfs_paddr_t)i;
        break;
    }
    free(bm);
    return err;
}

static int defer_free(struct apfsrw *fs, uint64_t paddr, uint32_t n)
{
    uint32_t i;

    if (paddr == 0)
        return APFSRW_OK;
    for (i = 0; i < n; i++) {
        if (fs->deferred_count == fs->deferred_cap) {
            uint32_t cap = fs->deferred_cap ? fs->deferred_cap * 2 : 256;
            uint64_t *nb = realloc(fs->deferred, cap * sizeof(*nb));

            if (nb == NULL)
                return APFSRW_ENOMEM;
            fs->deferred = nb;
            fs->deferred_cap = cap;
        }
        fs->deferred[fs->deferred_count++] = paddr + i;
    }
    return APFSRW_OK;
}

/* Give back everything this transaction allocated; the checkpoint that would
 * have referenced it never landed. */
static void txn_rollback(struct apfsrw *fs)
{
    uint32_t i;

    for (i = 0; i < fs->alloced_count; i++)
        free_blocks(fs, fs->alloced[i], 1);
    fs->alloced_count = 0;
    fs->deferred_count = 0;
    fs->alloc_delta = 0;
    (void)apfsrw_sync(fs);
    /* The failed op may have re-pointed cached tree/omap paddrs at blocks
     * just freed; re-derive everything from the last committed checkpoint. */
    fs->fs_oid = 0;
    (void)load_volume(fs);
}

static void flush_deferred(struct apfsrw *fs)
{
    uint32_t i;

    for (i = 0; i < fs->deferred_count; i++)
        free_blocks(fs, fs->deferred[i], 1);
    fs->deferred_count = 0;
}

static int alloc_blocks(struct apfsrw *fs, uint32_t n, uint64_t *out)
{
    struct apfs_spaceman_phys *sm = NULL;
    struct apfs_chunk_info_block *cib = NULL;
    uint8_t *bitmap = NULL;
    apfs_paddr_t sm_paddr = 0;
    uint32_t cib_count, addr_offset, i;
    int err;

    if (n == 0)
        return APFSRW_EINVAL;
    err = resolve_ephemeral(fs, rd64(&fs->nx.nx_spaceman_oid), &sm_paddr);
    if (err != APFSRW_OK)
        return err;

    sm = calloc(1, fs->block_size);
    cib = calloc(1, fs->block_size);
    bitmap = calloc(1, fs->block_size);
    if (sm == NULL || cib == NULL || bitmap == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }
    err = read_object(fs, sm_paddr, sm);
    if (err != APFSRW_OK)
        goto out;
    if (rd32(&sm->sm_dev[0].sm_cab_count) != 0) {
        err = APFSRW_ENOTSUP;
        goto out;
    }
    cib_count = rd32(&sm->sm_dev[0].sm_cib_count);
    addr_offset = rd32(&sm->sm_dev[0].sm_addr_offset);
    if ((uint64_t)addr_offset + (uint64_t)cib_count * 8U > fs->block_size) {
        err = APFSRW_EINVAL;
        goto out;
    }

    for (i = 0; i < cib_count; i++) {
        apfs_paddr_t cib_paddr;
        uint32_t chunks, c;

        memcpy(&cib_paddr, (const uint8_t *)sm + addr_offset + i * 8U, 8);
        cib_paddr = (apfs_paddr_t)rd64(&cib_paddr);
        if (cib_paddr <= 0)
            continue;
        err = read_object(fs, cib_paddr, cib);
        if (err != APFSRW_OK)
            goto out;
        chunks = rd32(&cib->cib_chunk_info_count);
        if (chunks > (fs->block_size - sizeof(*cib)) /
            sizeof(struct apfs_chunk_info)) {
            err = APFSRW_EINVAL;
            goto out;
        }
        for (c = 0; c < chunks; c++) {
            struct apfs_chunk_info *ci = &cib->cib_chunk_info[c];
            apfs_paddr_t bm_paddr = (apfs_paddr_t)rd64(&ci->ci_bitmap_addr);
            uint64_t chunk_addr = rd64(&ci->ci_addr);
            uint32_t nblk = rd32(&ci->ci_block_count);
            uint32_t free_count = rd32(&ci->ci_free_count);
            uint32_t start, run, k;

            if (free_count < n || nblk == 0)
                continue;
            if (bm_paddr == 0) {
                /* ci_bitmap_addr == 0: the chunk is entirely free with no
                 * bitmap block yet; give it one from the internal pool. */
                if (alloc_ip_block(fs, sm, &bm_paddr) != APFSRW_OK)
                    continue;
                memset(bitmap, 0, fs->block_size);
                wr64(&ci->ci_bitmap_addr, (uint64_t)bm_paddr);
            } else if (read_raw(fs, bm_paddr, bitmap) != APFSRW_OK) {
                continue;
            }

            run = 0;
            start = 0;
            for (k = 0; k < nblk; k++) {
                if (chunk_addr + k >= fs->block_count)
                    break;
                if (bitmap[k >> 3] & (uint8_t)(1U << (k & 7))) {
                    run = 0;
                    continue;
                }
                if (run == 0)
                    start = k;
                if (++run < n)
                    continue;

                for (k = start; k < start + n; k++)
                    bitmap[k >> 3] |= (uint8_t)(1U << (k & 7));
                wr32(&ci->ci_free_count, free_count - n);
                wr64(&sm->sm_dev[0].sm_free_count,
                    rd64(&sm->sm_dev[0].sm_free_count) - n);

                err = write_raw(fs, bm_paddr, bitmap);
                if (err != APFSRW_OK)
                    goto out;
                seal_object(fs, cib);
                err = write_block(fs, cib_paddr, cib);
                if (err != APFSRW_OK)
                    goto out;
                seal_object(fs, sm);
                err = write_block(fs, sm_paddr, sm);
                if (err != APFSRW_OK)
                    goto out;
                *out = chunk_addr + start;
                if (fs->alloced_count + n > fs->alloced_cap) {
                    uint32_t cap = fs->alloced_cap ? fs->alloced_cap * 2 : 256;
                    uint64_t *nb;

                    while (cap < fs->alloced_count + n)
                        cap *= 2;
                    nb = realloc(fs->alloced, cap * sizeof(*nb));
                    if (nb != NULL) {
                        fs->alloced = nb;
                        fs->alloced_cap = cap;
                    }
                }
                if (fs->alloced != NULL &&
                    fs->alloced_count + n <= fs->alloced_cap) {
                    uint32_t j;

                    for (j = 0; j < n; j++)
                        fs->alloced[fs->alloced_count++] =
                            chunk_addr + start + j;
                }
                err = APFSRW_OK;
                goto out;
            }
        }
    }
    err = APFSRW_ENOSPC;
out:
    free(bitmap);
    free(cib);
    free(sm);
    return err;
}

static int spaceman_set_range(struct apfsrw *fs, uint64_t first, uint32_t n,
    int set)
{
    struct apfs_spaceman_phys *sm = NULL;
    struct apfs_chunk_info_block *cib = NULL;
    uint8_t *bitmap = NULL;
    apfs_paddr_t sm_paddr = 0;
    uint32_t cib_count, addr_offset, i;
    int err;

    err = resolve_ephemeral(fs, rd64(&fs->nx.nx_spaceman_oid), &sm_paddr);
    if (err != APFSRW_OK)
        return err;
    sm = calloc(1, fs->block_size);
    cib = calloc(1, fs->block_size);
    bitmap = calloc(1, fs->block_size);
    if (sm == NULL || cib == NULL || bitmap == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }
    err = read_object(fs, sm_paddr, sm);
    if (err != APFSRW_OK)
        goto out;
    cib_count = rd32(&sm->sm_dev[0].sm_cib_count);
    addr_offset = rd32(&sm->sm_dev[0].sm_addr_offset);
    for (i = 0; i < cib_count; i++) {
        apfs_paddr_t cib_paddr;
        uint32_t chunks, c;

        memcpy(&cib_paddr, (const uint8_t *)sm + addr_offset + i * 8U, 8);
        cib_paddr = (apfs_paddr_t)rd64(&cib_paddr);
        if (cib_paddr <= 0)
            continue;
        err = read_object(fs, cib_paddr, cib);
        if (err != APFSRW_OK)
            goto out;
        chunks = rd32(&cib->cib_chunk_info_count);
        for (c = 0; c < chunks; c++) {
            struct apfs_chunk_info *ci = &cib->cib_chunk_info[c];
            uint64_t base = rd64(&ci->ci_addr);
            uint32_t nblk = rd32(&ci->ci_block_count);
            apfs_paddr_t bm = (apfs_paddr_t)rd64(&ci->ci_bitmap_addr);
            uint32_t changed = 0, k;

            if (first < base || first + n > base + nblk || bm <= 0)
                continue;
            err = read_raw(fs, bm, bitmap);
            if (err != APFSRW_OK)
                goto out;
            for (k = 0; k < n; k++) {
                uint32_t bit = (uint32_t)(first - base) + k;
                uint8_t mask = (uint8_t)(1U << (bit & 7));
                int cur = (bitmap[bit >> 3] & mask) != 0;

                if (cur == (set != 0))
                    continue;
                if (set)
                    bitmap[bit >> 3] |= mask;
                else
                    bitmap[bit >> 3] &= (uint8_t)~mask;
                changed++;
            }
            if (changed == 0) {
                err = APFSRW_OK;
                goto out;
            }
            wr32(&ci->ci_free_count, set ?
                rd32(&ci->ci_free_count) - changed :
                rd32(&ci->ci_free_count) + changed);
            wr64(&sm->sm_dev[0].sm_free_count, set ?
                rd64(&sm->sm_dev[0].sm_free_count) - changed :
                rd64(&sm->sm_dev[0].sm_free_count) + changed);
            err = write_raw(fs, bm, bitmap);
            if (err != APFSRW_OK)
                goto out;
            seal_object(fs, cib);
            err = write_block(fs, cib_paddr, cib);
            if (err != APFSRW_OK)
                goto out;
            seal_object(fs, sm);
            err = write_block(fs, sm_paddr, sm);
            goto out;
        }
    }
    err = APFSRW_ENOENT;
out:
    free(bitmap);
    free(cib);
    free(sm);
    return err;
}

#define APFSRW_MAX_RECORDS 512
/* Largest legal fs-tree key is a hashed drec: 12 + 255-byte name + NUL = 268
 * (spec p.98 j_drec_hashed_key_t). Kept tight; these sit on the kernel stack. */
#define APFSRW_MAX_KEY 300
/* Deferred-free blocks tolerated before a batch checkpoints mid-flight. */
#define APFSRW_BATCH_FLUSH 16384u
/* spec p.128 BTREE_TOC_ENTRY_INCREMENT */
#define APFS_BTREE_TOC_ENTRY_INCREMENT 8u

struct rw_rec {
    uint8_t *key;
    uint8_t *val;
    uint16_t klen;
    uint16_t vlen;
};

static void free_records(struct rw_rec *r, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        free(r[i].key);
        free(r[i].val);
    }
}

/*
 * Order of records in a file-system tree: by object id, then by record type,
 * then type-specific (spec p.123 "Key Comparison", p.71 "File-System Objects").
 * For directory entries the tiebreak is the name hash - verified against real
 * volumes, where the dirents of one directory appear in ascending hash order.
 */
static int rec_cmp(const uint8_t *ka, uint16_t la, const uint8_t *kb,
    uint16_t lb)
{
    uint64_t a, b;

    if (la < 8 || lb < 8)
        return la < lb ? -1 : (la > lb);
    a = rd64(ka);
    b = rd64(kb);
    if (key_id(a) != key_id(b))
        return key_id(a) < key_id(b) ? -1 : 1;
    if (key_type(a) != key_type(b))
        return key_type(a) < key_type(b) ? -1 : 1;

    if (key_type(a) == APFS_TYPE_DIR_REC && la >= 12 && lb >= 12) {
        uint32_t ha = rd32(ka + 8) >> 10;
        uint32_t hb = rd32(kb + 8) >> 10;
        uint32_t na, nb, n;
        int c;

        if (ha != hb)
            return ha < hb ? -1 : 1;
        /*
         * The hash is only 22 bits, so distinct names in one directory collide
         * routinely - with a couple of thousand entries it is more likely than
         * not. Comparing hashes alone makes two different files compare EQUAL,
         * and the insert then reports EEXIST for a file that does not exist.
         * Fall back to the name to get a total order.
         */
        na = (uint32_t)(la - 12);
        nb = (uint32_t)(lb - 12);
        n = na < nb ? na : nb;
        c = memcmp(ka + 12, kb + 12, n);
        if (c != 0)
            return c < 0 ? -1 : 1;
        if (na != nb)
            return na < nb ? -1 : 1;
    } else if (key_type(a) == APFS_TYPE_FILE_EXTENT && la >= 16 && lb >= 16) {
        uint64_t la2 = rd64(ka + 8);
        uint64_t lb2 = rd64(kb + 8);

        if (la2 != lb2)
            return la2 < lb2 ? -1 : 1;
    }
    return 0;
}

static int load_leaf_records(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, struct rw_rec *out, uint32_t *count)
{
    uint32_t n = rd32(&node->btn_nkeys);
    uint32_t i;

    if (n > APFSRW_MAX_RECORDS) {
        return APFSRW_ENOTSUP;
    }
    for (i = 0; i < n; i++) {
        const void *kp, *vp;
        uint16_t kl, vl;
        int err = btree_entry(fs, node, info, i, &kp, &kl, &vp, &vl);

        if (err != APFSRW_OK)
            return err;
        out[i].key = malloc(kl ? kl : 1);
        out[i].val = malloc(vl ? vl : 1);
        if (out[i].key == NULL || out[i].val == NULL)
            return APFSRW_ENOMEM;
        memcpy(out[i].key, kp, kl);
        memcpy(out[i].val, vp, vl);
        out[i].klen = kl;
        out[i].vlen = vl;
    }
    *count = n;
    return APFSRW_OK;
}

static int insert_record(struct rw_rec *r, uint32_t *count, const void *key,
    uint16_t klen, const void *val, uint16_t vlen)
{
    uint32_t pos = 0;
    uint32_t i;

    if (*count + 1 > APFSRW_MAX_RECORDS)
        return APFSRW_ENOSPC;
    while (pos < *count && rec_cmp(r[pos].key, r[pos].klen, key, klen) < 0)
        pos++;
    if (pos < *count && rec_cmp(r[pos].key, r[pos].klen, key, klen) == 0)
        return APFSRW_EEXIST;
    for (i = *count; i > pos; i--)
        r[i] = r[i - 1];
    r[pos].key = malloc(klen);
    r[pos].val = malloc(vlen ? vlen : 1);
    if (r[pos].key == NULL || r[pos].val == NULL)
        return APFSRW_ENOMEM;
    memcpy(r[pos].key, key, klen);
    memcpy(r[pos].val, val, vlen);
    r[pos].klen = klen;
    r[pos].vlen = vlen;
    (*count)++;
    return APFSRW_OK;
}




/* -- NODE BUILDING -- */

/*
 * Build a b-tree node from a record array.
 *
 * Generalises repack_leaf to any level and either table-of-contents form:
 *  - level 0 is a leaf, above that an index node whose values are oid_t;
 *  - BTNODE_FIXED_KV_SIZE nodes use kvoff_t (offsets only, no lengths), which
 *    is what object map nodes use (spec p.128-129);
 *  - only a ROOT node carries the trailing btree_info_t (spec p.126), so only
 *    there does the value area stop short of the end of the block.
 *
 * Returns APFSRW_ENOSPC when the records do not fit, which is the caller's cue
 * to split.
 */
static int build_node(struct apfsrw *fs, const struct apfs_btree_node_phys *tmpl,
    int is_root, uint16_t level, const struct apfs_btree_info *info,
    struct rw_rec *r, uint32_t count, uint8_t *node)
{
    uint32_t data_off = (uint32_t)offsetof(struct apfs_btree_node_phys,
        btn_data);
    int fixed = (rd16(&tmpl->btn_flags) & APFS_BTNODE_FIXED_KV_SIZE) != 0;
    uint16_t ent = (uint16_t)(fixed ? 4 : sizeof(struct apfs_kvloc));
    /*
     * Table of contents sizing. A tree with FIXED key/value sizes preallocates
     * the table for the whole node - every slot the node could ever hold - and
     * the size must match exactly. A variable-size tree only needs room for the
     * records present, and keeps spare entries at the end, growing in
     * BTREE_TOC_ENTRY_INCREMENT steps (spec p.122 "Table of Contents",
     * p.128 BTREE_TOC_ENTRY_INCREMENT = 8).
     *
     * Getting this wrong is what apfsck reports as "block ... is not sane":
     * for a fixed node it compares the table size against
     * (blocksize - sizeof(btree_node_phys)) / (key + value + toc entry).
     */
    uint32_t slots;
    uint16_t table_len;

    if (fixed) {
        uint32_t ksz = info != NULL ? rd32(&info->bt_key_size) : 16;
        uint32_t vsz = (level == 0)
            ? (info != NULL ? rd32(&info->bt_val_size) : 16)
            : (uint32_t)sizeof(apfs_oid_t);
        uint32_t space = fs->block_size -
            (uint32_t)sizeof(struct apfs_btree_node_phys);

        if (ksz + vsz + ent == 0)
            return APFSRW_EINVAL;
        slots = space / (ksz + vsz + ent);
    } else {
        slots = ((count + APFS_BTREE_TOC_ENTRY_INCREMENT - 1) /
            APFS_BTREE_TOC_ENTRY_INCREMENT) * APFS_BTREE_TOC_ENTRY_INCREMENT;
        if (slots == 0)
            slots = APFS_BTREE_TOC_ENTRY_INCREMENT;
    }
    table_len = (uint16_t)(slots * ent);
    uint16_t key_base = (uint16_t)(data_off + table_len);
    uint16_t val_end = (uint16_t)(fs->block_size -
        (is_root ? sizeof(struct apfs_btree_info) : 0));
    uint16_t key_off = 0, val_off = 0, max_k = 0, max_v = 0;
    uint32_t i;

    memset(node, 0, fs->block_size);
    memcpy(node, tmpl, data_off);
    /*
     * A root node is OBJECT_TYPE_BTREE, everything below it is
     * OBJECT_TYPE_BTREE_NODE (spec p.11 "Objects"). A node split off from a
     * root inherits the root's header, so the type has to be restamped or
     * apfsck reports "wrong object type for nonroot". The high half (storage
     * flags such as OBJ_PHYSICAL) and the subtype are preserved.
     */
    wr32(node + offsetof(struct apfs_obj_phys, o_type),
        (rd32(&tmpl->btn_o.o_type) & 0xffff0000u) |
        (is_root ? APFS_OBJECT_TYPE_BTREE : APFS_OBJECT_TYPE_BTREE_NODE));
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_flags),
        (uint16_t)((is_root ? APFS_BTNODE_ROOT : 0) |
        (level == 0 ? APFS_BTNODE_LEAF : 0) |
        (fixed ? APFS_BTNODE_FIXED_KV_SIZE : 0)));
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_level), level);
    wr32(node + offsetof(struct apfs_btree_node_phys, btn_nkeys), count);
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_table_space), 0);
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_table_space) + 2,
        table_len);

    for (i = 0; i < count; i++) {
        uint8_t *toc = node + data_off + i * ent;

        val_off = (uint16_t)(val_off + r[i].vlen);
        if (val_off > val_end ||
            (uint32_t)key_base + key_off + r[i].klen > (uint32_t)val_end -
            val_off)
            return APFSRW_ENOSPC;
        if (fixed) {
            wr16(toc, key_off);
            wr16(toc + 2, val_off);
        } else {
            wr16(toc, key_off);
            wr16(toc + 2, r[i].klen);
            wr16(toc + 4, val_off);
            wr16(toc + 6, r[i].vlen);
        }
        memcpy(node + key_base + key_off, r[i].key, r[i].klen);
        memcpy(node + val_end - val_off, r[i].val, r[i].vlen);
        key_off = (uint16_t)(key_off + r[i].klen);
        if (r[i].klen > max_k)
            max_k = r[i].klen;
        if (r[i].vlen > max_v)
            max_v = r[i].vlen;
    }

    wr16(node + offsetof(struct apfs_btree_node_phys, btn_free_space), key_off);
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_free_space) + 2,
        (uint16_t)(val_end - val_off - key_base - key_off));
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_key_free_list),
        0xffff);
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_key_free_list) + 2, 0);
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_val_free_list),
        0xffff);
    wr16(node + offsetof(struct apfs_btree_node_phys, btn_val_free_list) + 2, 0);

    if (is_root) {
        struct apfs_btree_info *bi = (struct apfs_btree_info *)
            (node + fs->block_size - sizeof(*bi));

        memset(bi, 0, sizeof(*bi));
        if (info != NULL)
            memcpy(bi, info, sizeof(*bi));
        wr32(&bi->bt_node_size, fs->block_size);
        if (max_k > rd32(&bi->bt_longest_key))
            wr32(&bi->bt_longest_key, max_k);
        if (max_v > rd32(&bi->bt_longest_val))
            wr32(&bi->bt_longest_val, max_v);
        if (level == 0) {
            /* Root is the only node, so the tree-wide counts are this node's.
             * Above level 0 they are maintained by the caller as records are
             * added, since the root cannot see the rest of the tree. */
            wr64(&bi->bt_key_count, count);
            wr64(&bi->bt_node_count, 1);
        }
    }
    return APFSRW_OK;
}

/*
 * Choose where to split a record array: the largest prefix whose keys, values
 * and table entries still fit in a node, capped at half the records so both
 * halves make progress.
 */
static uint32_t split_point(struct apfsrw *fs, struct rw_rec *r, uint32_t count,
    int fixed)
{
    uint32_t ent = fixed ? 4u : (uint32_t)sizeof(struct apfs_kvloc);
    uint32_t avail = fs->block_size -
        (uint32_t)offsetof(struct apfs_btree_node_phys, btn_data) -
        (uint32_t)sizeof(struct apfs_btree_info);
    uint32_t used = 0, i;

    for (i = 0; i < count; i++) {
        uint32_t need = ent + r[i].klen + r[i].vlen;

        if (used + need > avail / 2 && i > 0)
            break;
        used += need;
    }
    if (i == 0)
        i = 1;
    if (i >= count)
        i = count - 1;
    return i;
}

/* Virtual oids for new b-tree nodes come from the CONTAINER's counter, not the
 * volume's file-id counter: on a freshly created volume root_tree_oid is 1028
 * while apfs_next_obj_id is 16. */
static apfs_oid_t alloc_oid(struct apfsrw *fs)
{
    if (fs->next_oid == 0)
        fs->next_oid = rd64(&fs->nx.nx_next_oid);
    return fs->next_oid++;
}

static int fstree_bump_counts(struct apfsrw *fs, int64_t key_delta,
    int64_t node_delta, uint32_t klen, uint32_t vlen);

/* -- MULTI-LEVEL TREE UPDATE -- */

struct bpath {
    apfs_paddr_t paddr[APFSRW_BTREE_MAX_DEPTH];
    uint32_t index[APFSRW_BTREE_MAX_DEPTH];
    uint32_t n;                          /* paddr[n-1] is the leaf */
    struct apfs_btree_info info;         /* the root's, copied */
};

enum bkey_kind { BKEY_OMAP, BKEY_JKEY };

/* Order two keys of the given flavour. */
static int bkey_cmp(enum bkey_kind kind, const void *a, uint16_t alen,
    const void *b, uint16_t blen, apfs_xid_t xid)
{
    if (kind == BKEY_OMAP) {
        const struct apfs_omap_key *ka = a;
        const struct apfs_omap_key *kb = b;

        if (alen < sizeof(*ka) || blen < sizeof(*kb))
            return 0;
        if (rd64(&ka->ok_oid) != rd64(&kb->ok_oid))
            return rd64(&ka->ok_oid) < rd64(&kb->ok_oid) ? -1 : 1;
        (void)xid;
        if (rd64(&ka->ok_xid) != rd64(&kb->ok_xid))
            return rd64(&ka->ok_xid) < rd64(&kb->ok_xid) ? -1 : 1;
        return 0;
    }
    return rec_cmp(a, alen, b, blen);
}

/*
 * Descend a tree whose child links are PHYSICAL (object maps, the extent
 * reference tree), recording the node and chosen child index at every level so
 * the path can be copied back up afterwards.
 */
static int descend_phys(struct apfsrw *fs, apfs_paddr_t root,
    enum bkey_kind kind, const void *key, uint16_t klen, struct bpath *p)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *ip;
    apfs_paddr_t cur = root;
    int err = APFSRW_OK;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    p->n = 0;

    for (;;) {
        uint32_t nkeys, i, chosen = 0;
        apfs_paddr_t child = -1;

        if (p->n >= APFSRW_BTREE_MAX_DEPTH) {
            err = APFSRW_EINVAL;
            goto out;
        }
        err = read_object(fs, cur, node);
        if (err != APFSRW_OK)
            goto out;
        if (p->n == 0) {
            ip = btree_info_for_node(fs, node);
            if (ip == NULL) {
                err = APFSRW_EINVAL;
                goto out;
            }
            memcpy(&p->info, ip, sizeof(p->info));
        }
        p->paddr[p->n] = cur;

        if (rd16(&node->btn_level) == 0) {
            p->index[p->n] = 0;
            p->n++;
            break;
        }

        nkeys = rd32(&node->btn_nkeys);
        for (i = 0; i < nkeys; i++) {
            const void *kp, *vp;
            uint16_t kl, vl;

            err = btree_entry(fs, node, &p->info, i, &kp, &kl, &vp, &vl);
            if (err != APFSRW_OK)
                goto out;
            if (vl < sizeof(apfs_oid_t))
                continue;
            /* Entry 0 also covers everything below its own key. */
            if (i == 0 || bkey_cmp(kind, kp, kl, key, klen, fs->xid) <= 0) {
                child = (apfs_paddr_t)rd64(vp);
                chosen = i;
            } else {
                break;
            }
        }
        if (child <= 0) {
            err = APFSRW_ENOENT;
            goto out;
        }
        p->index[p->n] = chosen;
        p->n++;
        cur = child;
    }
out:
    free(node);
    return err;
}

/*
 * Publish a rewritten leaf back up a physical path: the leaf goes to a fresh
 * block, then each parent is copied with its child pointer repointed, all the
 * way to a new root. Physical nodes own their address, so every copy is
 * restamped (spec p.11 "Objects").
 */
/*
 * `new_minkey`, when given, is the leaf's new smallest key. A parent stores the
 * minimum key of each child, so if that changed the parent's separator must be
 * rewritten too - and if the child sat at index 0, the parent's own minimum
 * changed as well, so it keeps propagating. Object map keys are (oid, xid), so
 * simply moving an entry to a new transaction can shift a leaf's minimum;
 * fsck_apfs catches it as "Checking if the parent's minkey can be updated".
 */
static int phys_path_publish_key(struct apfsrw *fs, struct bpath *p,
    uint8_t *leaf, const void *new_minkey, uint16_t minkey_len,
    int64_t key_delta, apfs_paddr_t *new_root)
{
    struct apfs_btree_node_phys *node = NULL;
    uint64_t child_new = 0;
    uint32_t level;
    int err;

    err = alloc_blocks(fs, 1, &child_new);
    if (err != APFSRW_OK)
        return err;
    wr64(leaf + offsetof(struct apfs_obj_phys, o_oid), child_new);
    wr64(leaf + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    seal_object(fs, leaf);
    err = write_block(fs, (apfs_paddr_t)child_new, leaf);
    if (err != APFSRW_OK)
        return err;
    defer_free(fs, (uint64_t)p->paddr[p->n - 1], 1);

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;

    for (level = p->n - 1; level > 0; level--) {
        uint32_t pi = level - 1;
        const void *kp, *vp;
        uint16_t kl, vl;
        uint64_t parent_new = 0;

        err = read_object(fs, p->paddr[pi], node);
        if (err != APFSRW_OK)
            goto out;
        err = btree_entry(fs, node, &p->info, p->index[pi], &kp, &kl, &vp,
            &vl);
        if (err != APFSRW_OK)
            goto out;
        if (vl < sizeof(apfs_oid_t)) {
            err = APFSRW_EINVAL;
            goto out;
        }
        wr64((void *)(uintptr_t)vp, child_new);
        if (new_minkey != NULL && kl == minkey_len) {
            memcpy((void *)(uintptr_t)kp, new_minkey, minkey_len);
            if (p->index[pi] != 0)
                new_minkey = NULL;       /* parent's own minimum unchanged */
        } else {
            new_minkey = NULL;
        }

        /* bt_key_count on the ROOT counts the whole tree (spec p.126-127). */
        if (pi == 0 && key_delta != 0 &&
            (rd16(&node->btn_flags) & APFS_BTNODE_ROOT) != 0) {
            struct apfs_btree_info *bi = (struct apfs_btree_info *)
                ((uint8_t *)node + fs->block_size - sizeof(*bi));

            wr64(&bi->bt_key_count,
                (uint64_t)((int64_t)rd64(&bi->bt_key_count) + key_delta));
        }

        err = alloc_blocks(fs, 1, &parent_new);
        if (err != APFSRW_OK)
            goto out;
        wr64(&node->btn_o.o_oid, parent_new);
        wr64(&node->btn_o.o_xid, fs->xid + 1);
        seal_object(fs, node);
        err = write_block(fs, (apfs_paddr_t)parent_new, node);
        if (err != APFSRW_OK)
            goto out;
        defer_free(fs, (uint64_t)p->paddr[pi], 1);
        child_new = parent_new;
    }
    *new_root = (apfs_paddr_t)child_new;
    err = APFSRW_OK;
out:
    free(node);
    return err;
}

static int phys_path_publish(struct apfsrw *fs, struct bpath *p, uint8_t *leaf,
    int64_t key_delta, apfs_paddr_t *new_root)
{
    return phys_path_publish_key(fs, p, leaf, NULL, 0, key_delta, new_root);
}

/*
 * Point an object map entry at a new physical address, moving it into the new
 * transaction. Returns the new object map tree root, since the whole path is
 * copied.
 */
static int omap_set(struct apfsrw *fs, apfs_paddr_t tree_root, apfs_oid_t oid,
    apfs_paddr_t new_paddr, apfs_paddr_t *new_tree_root)
{
    struct bpath p;
    struct apfs_btree_node_phys *leaf = NULL;
    struct apfs_omap_key want;
    uint32_t i, best = UINT32_MAX;
    apfs_xid_t best_xid = 0;
    int err;

    memset(&want, 0, sizeof(want));
    wr64(&want.ok_oid, oid);
    wr64(&want.ok_xid, fs->xid + 1);

    err = descend_phys(fs, tree_root, BKEY_OMAP, &want, sizeof(want), &p);
    if (err != APFSRW_OK)
        return err;

    leaf = calloc(1, fs->block_size);
    if (leaf == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, p.paddr[p.n - 1], leaf);
    if (err != APFSRW_OK)
        goto out;

    for (i = 0; i < rd32(&leaf->btn_nkeys); i++) {
        const void *kp, *vp;
        uint16_t kl, vl;
        const struct apfs_omap_key *k;

        err = btree_entry(fs, leaf, &p.info, i, &kp, &kl, &vp, &vl);
        if (err != APFSRW_OK)
            goto out;
        if (kl < sizeof(*k) || vl < sizeof(struct apfs_omap_val))
            continue;
        k = kp;
        if (rd64(&k->ok_oid) != oid || rd64(&k->ok_xid) > fs->xid + 1)
            continue;
        if (best == UINT32_MAX || rd64(&k->ok_xid) >= best_xid) {
            best_xid = rd64(&k->ok_xid);
            best = i;
        }
    }
    if (best == UINT32_MAX) {
        err = APFSRW_ENOENT;
        goto out;
    }
    {
        const void *kp, *vp;
        uint16_t kl, vl;

        err = btree_entry(fs, leaf, &p.info, best, &kp, &kl, &vp, &vl);
        if (err != APFSRW_OK)
            goto out;
        wr64((void *)(uintptr_t)&((struct apfs_omap_key *)(uintptr_t)kp)->
            ok_xid, (uint64_t)(fs->xid + 1));
        wr64((void *)(uintptr_t)&((struct apfs_omap_val *)(uintptr_t)vp)->
            ov_paddr, (uint64_t)new_paddr);
    }
    {
        struct apfs_omap_key newmin;
        const void *kp;
        const void *vp;
        uint16_t kl, vl;

        if (best == 0 &&
            btree_entry(fs, leaf, &p.info, 0, &kp, &kl, &vp, &vl) ==
            APFSRW_OK && kl == sizeof(newmin)) {
            memcpy(&newmin, kp, sizeof(newmin));
            err = phys_path_publish_key(fs, &p, (uint8_t *)leaf, &newmin,
                (uint16_t)sizeof(newmin), 0, new_tree_root);
        } else {
            err = phys_path_publish(fs, &p, (uint8_t *)leaf, 0, new_tree_root);
        }
    }
out:
    free(leaf);
    return err;
}


/*
 * Descend the file-system tree to the leaf that should hold `key`.
 *
 * Its child links are VIRTUAL oids, so each step costs an object map lookup -
 * and, usefully, a modified leaf keeps its oid, so only the map entry has to
 * move and no parent needs rewriting.
 */
static int descend_fs(struct apfsrw *fs, const void *key, uint16_t klen,
    apfs_paddr_t *leaf_paddr, apfs_oid_t *leaf_oid,
    struct apfs_btree_info *info_out)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *ip;
    apfs_paddr_t cur = fs->root_tree_paddr;
    uint32_t depth = 0;
    int err;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;

    for (;;) {
        uint32_t nkeys, i;
        apfs_oid_t child_oid = 0;
        int have_child = 0;

        if (depth++ > APFSRW_BTREE_MAX_DEPTH) {
            err = APFSRW_EINVAL;
            goto out;
        }
        err = read_object(fs, cur, node);
        if (err != APFSRW_OK)
            goto out;
        if (depth == 1) {
            ip = btree_info_for_node(fs, node);
            if (ip == NULL) {
                err = APFSRW_EINVAL;
                goto out;
            }
            memcpy(info_out, ip, sizeof(*info_out));
        }
        if (rd16(&node->btn_level) == 0) {
            *leaf_paddr = cur;
            *leaf_oid = rd64(&node->btn_o.o_oid);
            err = APFSRW_OK;
            goto out;
        }

        nkeys = rd32(&node->btn_nkeys);
        for (i = 0; i < nkeys; i++) {
            const void *kp, *vp;
            uint16_t kl, vl;

            err = btree_entry(fs, node, info_out, i, &kp, &kl, &vp, &vl);
            if (err != APFSRW_OK)
                goto out;
            if (vl < sizeof(apfs_oid_t))
                continue;
            if (i == 0 || rec_cmp(kp, kl, key, klen) <= 0) {
                child_oid = rd64(vp);
                have_child = 1;
            } else {
                break;
            }
        }
        if (!have_child) {
            err = APFSRW_ENOENT;
            goto out;
        }
        {
            struct apfs_omap_val ov;

            /*
             * Entries rewritten earlier in this same transaction carry
             * xid + 1, so the in-progress transaction must look up with that
             * ceiling or it cannot see its own work.
             */
            err = omap_lookup_tree(fs, fs->volume_omap_tree_paddr, child_oid,
                fs->xid + 1, &ov);
            if (err != APFSRW_OK)
                goto out;
            cur = (apfs_paddr_t)rd64(&ov.ov_paddr);
        }
    }
out:
    free(node);
    return err;
}


/*
 * Insert a record into a tree whose nodes are PHYSICAL (object maps, the
 * extent reference tree), splitting nodes as needed.
 *
 * The mirror of fstree_put, but every node here owns its address, so a
 * rewritten node forces its parent to be rewritten too - the pointer changed.
 * That makes the whole path copy-on-write on every insert, and a root split
 * produces a root at a NEW address, which is why this returns the new root for
 * the caller to store (om_tree_oid, apfs_extentref_tree_oid).
 */
static int phys_write_node(struct apfsrw *fs, uint8_t *node,
    apfs_paddr_t old_paddr, apfs_paddr_t *out)
{
    uint64_t blk = 0;
    int err = alloc_blocks(fs, 1, &blk);

    if (err != APFSRW_OK)
        return err;
    wr64(node + offsetof(struct apfs_obj_phys, o_oid), blk);
    wr64(node + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    seal_object(fs, node);
    err = write_block(fs, (apfs_paddr_t)blk, node);
    if (err != APFSRW_OK)
        return err;
    if (old_paddr > 0)
        defer_free(fs, (uint64_t)old_paddr, 1);
    *out = (apfs_paddr_t)blk;
    return APFSRW_OK;
}

static int phys_insert(struct apfsrw *fs, apfs_paddr_t tree_root,
    enum bkey_kind kind, const void *key, uint16_t klen, const void *val,
    uint16_t vlen, int upsert, apfs_paddr_t *new_root)
{
    struct bpath p;
    struct apfs_btree_node_phys *cur = NULL;
    struct rw_rec *recs = NULL;
    uint8_t *b1 = NULL, *b2 = NULL;
    int replaced = 0;
    uint32_t count = 0;
    int64_t added_nodes = 0;
    uint32_t lvl;
    uint8_t newmin[APFSRW_MAX_KEY];
    uint16_t newmin_len = 0;
    int err;

    err = descend_phys(fs, tree_root, kind, key, klen, &p);
    if (err != APFSRW_OK)
        return err;

    cur = calloc(1, fs->block_size);
    b1 = calloc(1, fs->block_size);
    b2 = calloc(1, fs->block_size);
    recs = calloc(APFSRW_MAX_RECORDS, sizeof(*recs));
    if (cur == NULL || b1 == NULL || b2 == NULL || recs == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }

    lvl = p.n - 1;
    err = read_object(fs, p.paddr[lvl], cur);
    if (err != APFSRW_OK)
        goto out;
    err = load_leaf_records(fs, cur, &p.info, recs, &count);
    if (err != APFSRW_OK)
        goto out;
    if (upsert) {
        uint32_t r;

        for (r = 0; r < count; r++) {
            if (rec_cmp(recs[r].key, recs[r].klen, key, klen) != 0)
                continue;
            if (vlen > recs[r].vlen) {
                uint8_t *nv = realloc(recs[r].val, vlen);

                if (nv == NULL) {
                    err = APFSRW_ENOMEM;
                    goto out;
                }
                recs[r].val = nv;
            }
            memcpy(recs[r].val, val, vlen);
            recs[r].vlen = vlen;
            replaced = 1;
            break;
        }
    }
    if (!replaced) {
        err = insert_record(recs, &count, key, klen, val, vlen);
        if (err != APFSRW_OK)
            goto out;
    }

    for (;;) {
        int is_root = (lvl == 0);
        uint16_t tree_level = (uint16_t)(p.n - 1 - lvl);
        apfs_paddr_t child_new = 0, right_pa = 0;
        uint32_t sp, rcount, l;

        err = build_node(fs, cur, is_root, tree_level, &p.info, recs, count,
            b1);
        if (err == APFSRW_OK) {
            if (!is_root && rec_cmp(recs[0].key, recs[0].klen, key, klen) == 0 &&
                recs[0].klen <= sizeof(newmin)) {
                memcpy(newmin, recs[0].key, recs[0].klen);
                newmin_len = recs[0].klen;
            }
            if (is_root && tree_level > 0) {
                struct apfs_btree_info *bi = (struct apfs_btree_info *)
                    (b1 + fs->block_size - sizeof(*bi));

                wr64(&bi->bt_key_count,
                    rd64(&bi->bt_key_count) + (replaced ? 0 : 1));
                wr64(&bi->bt_node_count,
                    (uint64_t)((int64_t)rd64(&bi->bt_node_count) +
                    added_nodes));
                seal_object(fs, b1);
            }
            err = phys_write_node(fs, b1, p.paddr[lvl], &child_new);
            if (err != APFSRW_OK)
                goto out;
            /*
             * Repoint the rest of the path at the rewritten child - and, if
             * the record landed at index 0, carry its new MINIMUM KEY up too.
             * A parent stores each child's smallest key, so inserting below
             * that minimum silently leaves the parent claiming a larger one
             * and the tree reads back out of order.
             */
            for (l = lvl; l > 0; l--) {
                const void *kp, *vp;
                uint16_t kl, vl;
                uint32_t pi = l - 1;
                int min_moved = (l == lvl) && (newmin_len != 0);

                err = read_object(fs, p.paddr[pi], cur);
                if (err != APFSRW_OK)
                    goto out;
                err = btree_entry(fs, cur, &p.info, p.index[pi], &kp, &kl, &vp,
                    &vl);
                if (err != APFSRW_OK)
                    goto out;
                wr64((void *)(uintptr_t)vp, (uint64_t)child_new);
                if (min_moved && kl == newmin_len) {
                    memcpy((void *)(uintptr_t)kp, newmin, newmin_len);
                    /* Only index 0 changes the parent's own minimum. */
                    if (p.index[pi] != 0)
                        newmin_len = 0;
                } else {
                    newmin_len = 0;
                }
                if (pi == 0) {
                    struct apfs_btree_info *bi = (struct apfs_btree_info *)
                        ((uint8_t *)cur + fs->block_size - sizeof(*bi));

                    wr64(&bi->bt_key_count,
                        rd64(&bi->bt_key_count) + (replaced ? 0 : 1));
                    wr64(&bi->bt_node_count,
                        (uint64_t)((int64_t)rd64(&bi->bt_node_count) +
                        added_nodes));
                }
                err = phys_write_node(fs, (uint8_t *)cur, p.paddr[pi],
                    &child_new);
                if (err != APFSRW_OK)
                    goto out;
            }
            *new_root = child_new;
            /* Both trees reached through phys_insert - the volume object map
             * and the extent reference tree - are owned by the VOLUME, so new
             * nodes count towards apfs_fs_alloc_count. */
            fs->alloc_delta += added_nodes;
            err = APFSRW_OK;
            goto out;
        }
        if (err != APFSRW_ENOSPC)
            goto out;

        sp = split_point(fs, recs, count, 1);
        rcount = count - sp;

        err = build_node(fs, cur, 0, tree_level, &p.info, recs, sp, b1);
        if (err != APFSRW_OK)
            goto out;
        err = phys_write_node(fs, b1, is_root ? 0 : p.paddr[lvl], &child_new);
        if (err != APFSRW_OK)
            goto out;
        err = build_node(fs, cur, 0, tree_level, &p.info, recs + sp, rcount,
            b2);
        if (err != APFSRW_OK)
            goto out;
        err = phys_write_node(fs, b2, 0, &right_pa);
        if (err != APFSRW_OK)
            goto out;
        added_nodes += is_root ? 2 : 1;

        if (is_root) {
            struct rw_rec sep[2];
            uint8_t k0[APFSRW_MAX_KEY], k1[APFSRW_MAX_KEY];
            uint8_t v0[8], v1[8];
            struct apfs_btree_info *bi;

            memcpy(k0, recs[0].key, recs[0].klen);
            memcpy(k1, recs[sp].key, recs[sp].klen);
            wr64(v0, (uint64_t)child_new);
            wr64(v1, (uint64_t)right_pa);
            sep[0].key = k0; sep[0].klen = recs[0].klen;
            sep[0].val = v0; sep[0].vlen = 8;
            sep[1].key = k1; sep[1].klen = recs[sp].klen;
            sep[1].val = v1; sep[1].vlen = 8;
            err = build_node(fs, cur, 1, (uint16_t)(tree_level + 1), &p.info,
                sep, 2, b1);
            if (err != APFSRW_OK)
                goto out;
            bi = (struct apfs_btree_info *)(b1 + fs->block_size - sizeof(*bi));
            wr64(&bi->bt_key_count, rd64(&p.info.bt_key_count) + 1);
            wr64(&bi->bt_node_count,
                (uint64_t)((int64_t)rd64(&p.info.bt_node_count) + added_nodes));
            err = phys_write_node(fs, b1, p.paddr[0], new_root);
            fs->alloc_delta += added_nodes;
            goto out;
        }

        /* Point the parent at the left half and give it the new sibling. */
        {
            uint8_t sepkey[APFSRW_MAX_KEY];
            uint8_t sepval[8];
            uint16_t sepklen = recs[sp].klen;
            const void *kp, *vp;
            uint16_t kl, vl;
            uint32_t pi = lvl - 1;

            memcpy(sepkey, recs[sp].key, sepklen);
            wr64(sepval, (uint64_t)right_pa);

            free_records(recs, count);
            memset(recs, 0, APFSRW_MAX_RECORDS * sizeof(*recs));
            count = 0;
            err = read_object(fs, p.paddr[pi], cur);
            if (err != APFSRW_OK)
                goto out;
            err = btree_entry(fs, cur, &p.info, p.index[pi], &kp, &kl, &vp,
                &vl);
            if (err != APFSRW_OK)
                goto out;
            wr64((void *)(uintptr_t)vp, (uint64_t)child_new);
            err = load_leaf_records(fs, cur, &p.info, recs, &count);
            if (err != APFSRW_OK)
                goto out;
            err = insert_record(recs, &count, sepkey, sepklen, sepval, 8);
            if (err != APFSRW_OK)
                goto out;
            lvl = pi;
        }
    }
out:
    if (recs != NULL) {
        free_records(recs, count);
        free(recs);
    }
    free(b2);
    free(b1);
    free(cur);
    return err;
}

/*
 * Add a NEW object map entry (as opposed to omap_set, which repoints an
 * existing one), splitting the map's own nodes when they fill up.
 */
static int omap_insert(struct apfsrw *fs, apfs_paddr_t tree_root,
    apfs_oid_t oid, apfs_paddr_t paddr, apfs_paddr_t *new_tree_root)
{
    uint8_t key[16], val[16];

    memset(key, 0, sizeof(key));
    wr64(key, oid);
    wr64(key + 8, (uint64_t)(fs->xid + 1));
    memset(val, 0, sizeof(val));
    wr32(val + 4, fs->block_size);       /* ov_size */
    wr64(val + 8, (uint64_t)paddr);      /* ov_paddr */

    return phys_insert(fs, tree_root, BKEY_OMAP, key, (uint16_t)sizeof(key),
        val, (uint16_t)sizeof(val), 0, new_tree_root);
}

/* Record the full route down the file-system tree, not just the leaf. */
struct fpath {
    apfs_paddr_t paddr[APFSRW_BTREE_MAX_DEPTH];
    apfs_oid_t oid[APFSRW_BTREE_MAX_DEPTH];
    uint32_t index[APFSRW_BTREE_MAX_DEPTH];
    uint32_t n;
    struct apfs_btree_info info;
};

static int descend_fs_path(struct apfsrw *fs, const void *key, uint16_t klen,
    struct fpath *p)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *ip;
    apfs_paddr_t cur = fs->root_tree_paddr;
    int err = APFSRW_OK;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    p->n = 0;

    for (;;) {
        uint32_t nkeys, i, chosen = 0;
        apfs_oid_t child_oid = 0;
        int have_child = 0;

        if (p->n >= APFSRW_BTREE_MAX_DEPTH) {
            err = APFSRW_EINVAL;
            goto out;
        }
        err = read_object(fs, cur, node);
        if (err != APFSRW_OK)
            goto out;
        if (p->n == 0) {
            ip = btree_info_for_node(fs, node);
            if (ip == NULL) {
                err = APFSRW_EINVAL;
                goto out;
            }
            memcpy(&p->info, ip, sizeof(p->info));
        }
        p->paddr[p->n] = cur;
        p->oid[p->n] = rd64(&node->btn_o.o_oid);

        if (rd16(&node->btn_level) == 0) {
            p->index[p->n] = 0;
            p->n++;
            break;
        }
        nkeys = rd32(&node->btn_nkeys);
        for (i = 0; i < nkeys; i++) {
            const void *kp, *vp;
            uint16_t kl, vl;

            err = btree_entry(fs, node, &p->info, i, &kp, &kl, &vp, &vl);
            if (err != APFSRW_OK)
                goto out;
            if (vl < sizeof(apfs_oid_t))
                continue;
            if (i == 0 || rec_cmp(kp, kl, key, klen) <= 0) {
                child_oid = rd64(vp);
                chosen = i;
                have_child = 1;
            } else {
                break;
            }
        }
        if (!have_child) {
            err = APFSRW_ENOENT;
            goto out;
        }
        p->index[p->n] = chosen;
        p->n++;
        {
            struct apfs_omap_val ov;

            err = omap_lookup_tree(fs, fs->volume_omap_tree_paddr, child_oid,
                fs->xid + 1, &ov);
            if (err != APFSRW_OK)
                goto out;
            cur = (apfs_paddr_t)rd64(&ov.ov_paddr);
        }
    }
out:
    free(node);
    return err;
}

/* Write one file-system tree node to a fresh block under the given oid and
 * repoint its object map entry. The node keeps its virtual oid, so nothing
 * above it needs rewriting. */
static int fs_publish_node(struct apfsrw *fs, uint8_t *node, apfs_oid_t oid,
    apfs_paddr_t old_paddr, int is_new, apfs_paddr_t *out)
{
    apfs_paddr_t new_omap_root = 0;
    uint64_t blk = 0;
    int err;

    err = alloc_blocks(fs, 1, &blk);
    if (err != APFSRW_OK)
        return err;
    wr64(node + offsetof(struct apfs_obj_phys, o_oid), oid);
    wr64(node + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    seal_object(fs, node);
    err = write_block(fs, (apfs_paddr_t)blk, node);
    if (err != APFSRW_OK)
        return err;
    if (!is_new && old_paddr > 0)
        defer_free(fs, (uint64_t)old_paddr, 1);

    if (is_new)
        err = omap_insert(fs, fs->volume_omap_tree_paddr, oid,
            (apfs_paddr_t)blk, &new_omap_root);
    else
        err = omap_set(fs, fs->volume_omap_tree_paddr, oid,
            (apfs_paddr_t)blk, &new_omap_root);
    if (err != APFSRW_OK)
        return err;
    fs->volume_omap_tree_paddr = new_omap_root;
    if (out != NULL)
        *out = (apfs_paddr_t)blk;
    return APFSRW_OK;
}

/* Fetch one record from the file-system tree by exact key. */
static int fstree_get(struct apfsrw *fs, const void *key, uint16_t klen,
    void *val_out, uint16_t val_cap, uint16_t *val_len_out)
{
    struct apfs_btree_node_phys *leaf = NULL;
    struct apfs_btree_info info;
    apfs_paddr_t leaf_paddr = 0;
    apfs_oid_t leaf_oid = 0;
    uint32_t i;
    int err;

    err = descend_fs(fs, key, klen, &leaf_paddr, &leaf_oid, &info);
    if (err != APFSRW_OK)
        return err;
    leaf = calloc(1, fs->block_size);
    if (leaf == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, leaf_paddr, leaf);
    if (err != APFSRW_OK)
        goto out;
    err = APFSRW_ENOENT;
    for (i = 0; i < rd32(&leaf->btn_nkeys); i++) {
        const void *kp, *vp;
        uint16_t kl, vl;

        if (btree_entry(fs, leaf, &info, i, &kp, &kl, &vp, &vl) != APFSRW_OK)
            continue;
        if (rec_cmp(kp, kl, key, klen) != 0)
            continue;
        if (vl > val_cap) {
            err = APFSRW_EOVERFLOW;
            goto out;
        }
        memcpy(val_out, vp, vl);
        *val_len_out = vl;
        err = APFSRW_OK;
        goto out;
    }
out:
    free(leaf);
    return err;
}

/*
 * Rewrite the separator a parent holds for one child, after that child's
 * smallest key changed. Repeats upward while the child sits at index 0, since
 * then the parent's own minimum moved too.
 */
static int fstree_fix_separator(struct apfsrw *fs, struct fpath *p, uint32_t lvl,
    const void *newkey, uint16_t newklen)
{
    struct apfs_btree_node_phys *node;
    struct rw_rec *recs = NULL;
    uint8_t *nbuf = NULL;
    uint32_t count = 0;
    int err = APFSRW_OK;

    node = calloc(1, fs->block_size);
    nbuf = calloc(1, fs->block_size);
    recs = calloc(APFSRW_MAX_RECORDS, sizeof(*recs));
    if (node == NULL || nbuf == NULL || recs == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }

    while (lvl > 0) {
        uint32_t pi = lvl - 1;
        uint32_t idx = p->index[pi];
        apfs_paddr_t out_pa = 0;

        err = read_object(fs, p->paddr[pi], node);
        if (err != APFSRW_OK)
            goto out;
        free_records(recs, count);
        memset(recs, 0, APFSRW_MAX_RECORDS * sizeof(*recs));
        count = 0;
        err = load_leaf_records(fs, node, &p->info, recs, &count);
        if (err != APFSRW_OK)
            goto out;
        if (idx >= count) {
            err = APFSRW_EINVAL;
            goto out;
        }
        if (newklen > recs[idx].klen) {
            uint8_t *nk = realloc(recs[idx].key, newklen);

            if (nk == NULL) {
                err = APFSRW_ENOMEM;
                goto out;
            }
            recs[idx].key = nk;
        }
        memcpy(recs[idx].key, newkey, newklen);
        recs[idx].klen = newklen;

        err = build_node(fs, node, (pi == 0), (uint16_t)(p->n - 1 - pi),
            &p->info, recs, count, nbuf);
        if (err != APFSRW_OK)
            goto out;
        err = fs_publish_node(fs, nbuf, p->oid[pi], p->paddr[pi], 0, &out_pa);
        if (err != APFSRW_OK)
            goto out;
        if (pi == 0)
            fs->root_tree_paddr = out_pa;
        if (idx != 0)
            break;                       /* parent's own minimum unchanged */
        lvl = pi;
    }
out:
    if (recs != NULL) {
        free_records(recs, count);
        free(recs);
    }
    free(nbuf);
    free(node);
    return err;
}

/*
 * Insert (or replace) a record in the file-system tree, splitting nodes when
 * they fill up.
 *
 * Classic b-tree insert, walked from the leaf upward. The file-system tree's
 * children are VIRTUAL oids, which simplifies the common case enormously: a
 * node that merely changes keeps its oid, so only its object map entry moves
 * and its parent is untouched. Parents are only rewritten when a split adds a
 * new separator.
 *
 * On a root split the root node keeps apfs_root_tree_oid and becomes an index
 * node over two new children, so the tree grows a level without the volume
 * superblock changing.
 */
static int fstree_put(struct apfsrw *fs, const void *key, uint16_t klen,
    const void *val, uint16_t vlen, int replace)
{
    struct fpath p;
    struct apfs_btree_node_phys *cur = NULL;
    struct rw_rec *recs = NULL;
    uint8_t *nbuf = NULL, *nbuf2 = NULL;
    uint32_t count = 0, lvl;
    int64_t added_nodes = 0;
    int err;

    err = descend_fs_path(fs, key, klen, &p);
    if (err != APFSRW_OK)
        return err;

    cur = calloc(1, fs->block_size);
    nbuf = calloc(1, fs->block_size);
    nbuf2 = calloc(1, fs->block_size);
    recs = calloc(APFSRW_MAX_RECORDS, sizeof(*recs));
    if (cur == NULL || nbuf == NULL || nbuf2 == NULL || recs == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }

    lvl = p.n - 1;                       /* start at the leaf */
    err = read_object(fs, p.paddr[lvl], cur);
    if (err != APFSRW_OK)
        goto out;
    err = load_leaf_records(fs, cur, &p.info, recs, &count);
    if (err != APFSRW_OK)
        goto out;

    if (replace) {
        uint32_t i;
        int found = 0;

        for (i = 0; i < count; i++) {
            if (rec_cmp(recs[i].key, recs[i].klen, key, klen) != 0)
                continue;
            if (vlen > recs[i].vlen) {
                uint8_t *nv = realloc(recs[i].val, vlen);

                if (nv == NULL) {
                    err = APFSRW_ENOMEM;
                    goto out;
                }
                recs[i].val = nv;
            }
            memcpy(recs[i].val, val, vlen);
            recs[i].vlen = vlen;
            found = 1;
            break;
        }
        if (!found) {
            err = APFSRW_ENOENT;
            goto out;
        }
    } else {
        err = insert_record(recs, &count, key, klen, val, vlen);
        if (err != APFSRW_OK)
            goto out;
    }

    for (;;) {
        int is_root = (lvl == 0);
        uint16_t tree_level = (uint16_t)(p.n - 1 - lvl);
        uint32_t sp, rcount;
        apfs_oid_t left_oid, right_oid;
        apfs_paddr_t left_pa = 0, right_pa = 0;

        err = build_node(fs, cur, is_root, tree_level, &p.info, recs, count,
            nbuf);
        if (err == APFSRW_OK) {
            /*
             * Fits. The node keeps its virtual oid, so the parent's CHILD
             * POINTER needs no change - but if the record landed at index 0
             * the parent's separator, which is this child's smallest key, is
             * now wrong and has to be rewritten.
             */
            int min_moved = !is_root && !replace &&
                rec_cmp(recs[0].key, recs[0].klen, key, klen) == 0;

            err = fs_publish_node(fs, nbuf, p.oid[lvl], p.paddr[lvl], 0,
                &left_pa);
            if (err != APFSRW_OK)
                goto out;
            if (is_root)
                fs->root_tree_paddr = left_pa;
            if (min_moved)
                err = fstree_fix_separator(fs, &p, lvl, key, klen);
            break;
        }
        if (err != APFSRW_ENOSPC)
            goto out;

        /* Split. */
        sp = split_point(fs, recs, count, 0);
        rcount = count - sp;

        if (is_root) {
            uint8_t rootbuf_tmpl[1];
            struct rw_rec sep[2];
            uint8_t k0[APFSRW_MAX_KEY], k1[APFSRW_MAX_KEY];
            uint8_t v0[8], v1[8];

            (void)rootbuf_tmpl;
            left_oid = alloc_oid(fs);
            right_oid = alloc_oid(fs);
            err = build_node(fs, cur, 0, tree_level, &p.info, recs, sp, nbuf);
            if (err != APFSRW_OK)
                goto out;
            err = fs_publish_node(fs, nbuf, left_oid, 0, 1, &left_pa);
            if (err != APFSRW_OK)
                goto out;
            err = build_node(fs, cur, 0, tree_level, &p.info, recs + sp,
                rcount, nbuf2);
            if (err != APFSRW_OK)
                goto out;
            err = fs_publish_node(fs, nbuf2, right_oid, 0, 1, &right_pa);
            if (err != APFSRW_OK)
                goto out;
            added_nodes += 2;

            /* The old root becomes an index node over the two halves. */
            memcpy(k0, recs[0].key, recs[0].klen);
            memcpy(k1, recs[sp].key, recs[sp].klen);
            wr64(v0, left_oid);
            wr64(v1, right_oid);
            sep[0].key = k0; sep[0].klen = recs[0].klen;
            sep[0].val = v0; sep[0].vlen = 8;
            sep[1].key = k1; sep[1].klen = recs[sp].klen;
            sep[1].val = v1; sep[1].vlen = 8;
            err = build_node(fs, cur, 1, (uint16_t)(tree_level + 1), &p.info,
                sep, 2, nbuf);
            if (err != APFSRW_OK)
                goto out;
            err = fs_publish_node(fs, nbuf, p.oid[0], p.paddr[0], 0, &left_pa);
            if (err != APFSRW_OK)
                goto out;
            fs->root_tree_paddr = left_pa;
            break;
        }

        /* Not the root: keep this node's oid for the left half. */
        left_oid = p.oid[lvl];
        right_oid = alloc_oid(fs);
        err = build_node(fs, cur, 0, tree_level, &p.info, recs, sp, nbuf);
        if (err != APFSRW_OK)
            goto out;
        err = fs_publish_node(fs, nbuf, left_oid, p.paddr[lvl], 0, &left_pa);
        if (err != APFSRW_OK)
            goto out;
        err = build_node(fs, cur, 0, tree_level, &p.info, recs + sp, rcount,
            nbuf2);
        if (err != APFSRW_OK)
            goto out;
        err = fs_publish_node(fs, nbuf2, right_oid, 0, 1, &right_pa);
        if (err != APFSRW_OK)
            goto out;
        added_nodes += 1;

        /* Hand the new separator to the parent and go round again. */
        {
            uint8_t sepkey[APFSRW_MAX_KEY];
            uint8_t sepval[8];
            uint16_t sepklen = recs[sp].klen;

            memcpy(sepkey, recs[sp].key, sepklen);
            wr64(sepval, right_oid);

            free_records(recs, count);
            memset(recs, 0, APFSRW_MAX_RECORDS * sizeof(*recs));
            count = 0;
            lvl--;
            err = read_object(fs, p.paddr[lvl], cur);
            if (err != APFSRW_OK)
                goto out;
            err = load_leaf_records(fs, cur, &p.info, recs, &count);
            if (err != APFSRW_OK)
                goto out;
            err = insert_record(recs, &count, sepkey, sepklen, sepval, 8);
            if (err != APFSRW_OK)
                goto out;
        }
    }

    fs->alloc_delta += added_nodes;
    err = fstree_bump_counts(fs, replace ? 0 : 1, added_nodes, klen, vlen);
out:
    if (recs != NULL) {
        free_records(recs, count);
        free(recs);
    }
    free(nbuf2);
    free(nbuf);
    free(cur);
    return err;
}

/*
 * Remove one record from the fs tree. No split to handle; an index-0 removal
 * rewrites the parent separator, and emptying a leaf entirely is refused.
 */
static int fstree_del(struct apfsrw *fs, const void *key, uint16_t klen)
{
    struct fpath p;
    struct apfs_btree_node_phys *cur = NULL;
    struct rw_rec *recs = NULL;
    uint8_t *nbuf = NULL;
    uint32_t count = 0, lvl, i, idx = 0;
    uint16_t gone_klen = 0, gone_vlen = 0;
    apfs_paddr_t out_pa = 0;
    int found = 0;
    int err;

    err = descend_fs_path(fs, key, klen, &p);
    if (err != APFSRW_OK)
        return err;

    cur = calloc(1, fs->block_size);
    nbuf = calloc(1, fs->block_size);
    recs = calloc(APFSRW_MAX_RECORDS, sizeof(*recs));
    if (cur == NULL || nbuf == NULL || recs == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }

    lvl = p.n - 1;                       /* the leaf */
    err = read_object(fs, p.paddr[lvl], cur);
    if (err != APFSRW_OK)
        goto out;
    err = load_leaf_records(fs, cur, &p.info, recs, &count);
    if (err != APFSRW_OK)
        goto out;

    for (i = 0; i < count; i++) {
        if (rec_cmp(recs[i].key, recs[i].klen, key, klen) != 0)
            continue;
        idx = i;
        found = 1;
        break;
    }
    if (!found) {
        err = APFSRW_ENOENT;
        goto out;
    }
    if (count == 1) {
        /* Would leave an empty leaf; collapsing the tree is not implemented. */
        err = APFSRW_ENOTSUP;
        goto out;
    }

    gone_klen = recs[idx].klen;
    gone_vlen = recs[idx].vlen;
    free(recs[idx].key);
    free(recs[idx].val);
    for (i = idx; i + 1 < count; i++)
        recs[i] = recs[i + 1];
    count--;
    recs[count].key = NULL;
    recs[count].val = NULL;
    recs[count].klen = 0;
    recs[count].vlen = 0;

    err = build_node(fs, cur, (lvl == 0), (uint16_t)(p.n - 1 - lvl), &p.info,
        recs, count, nbuf);
    if (err != APFSRW_OK)
        goto out;
    err = fs_publish_node(fs, nbuf, p.oid[lvl], p.paddr[lvl], 0, &out_pa);
    if (err != APFSRW_OK)
        goto out;
    if (lvl == 0)
        fs->root_tree_paddr = out_pa;
    else if (idx == 0) {
        /* The smallest key in this leaf changed. */
        err = fstree_fix_separator(fs, &p, lvl, recs[0].key, recs[0].klen);
        if (err != APFSRW_OK)
            goto out;
    }

    err = fstree_bump_counts(fs, -1, 0, gone_klen, gone_vlen);
out:
    if (recs != NULL) {
        free_records(recs, count);
        free(recs);
    }
    free(nbuf);
    free(cur);
    return err;
}

/*
 * Copy an object-map leaf, repointing one oid at a new physical block.
 */
static int omap_tree_copy_update(struct apfsrw *fs, apfs_paddr_t src,
    apfs_paddr_t dst, apfs_oid_t oid, apfs_paddr_t new_paddr)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *info;
    uint32_t i, best = UINT32_MAX;
    apfs_xid_t best_xid = 0;
    int err;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, src, node);
    if (err != APFSRW_OK)
        goto out;
    if ((rd16(&node->btn_flags) & APFS_BTNODE_LEAF) == 0) {
        err = APFSRW_ENOTSUP;            /* multi-level omap not handled yet */
        goto out;
    }
    info = btree_info_for_node(fs, node);
    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *kp, *vp;
        uint16_t kl, vl;
        const struct apfs_omap_key *k;

        err = btree_entry(fs, node, info, i, &kp, &kl, &vp, &vl);
        if (err != APFSRW_OK)
            goto out;
        if (kl < sizeof(*k) || vl < sizeof(struct apfs_omap_val))
            continue;
        k = (const struct apfs_omap_key *)kp;
        if (rd64(&k->ok_oid) != oid || rd64(&k->ok_xid) > fs->xid + 1)
            continue;
        if (best == UINT32_MAX || rd64(&k->ok_xid) >= best_xid) {
            best_xid = rd64(&k->ok_xid);
            best = i;
        }
    }
    if (best == UINT32_MAX) {
        err = APFSRW_ENOENT;
        goto out;
    }
    {
        const void *kp, *vp;
        uint16_t kl, vl;

        err = btree_entry(fs, node, info, best, &kp, &kl, &vp, &vl);
        if (err != APFSRW_OK)
            goto out;
        /*
         * A modified virtual object is identified by (oid, xid), so the map
         * entry must move to the NEW transaction together with the object it
         * points at (spec p.11 "Objects", virtual storage). Bumping the
         * object's o_xid alone makes fsck_apfs report
         * "invalid o_xid (0x6, expected 0x5)". Sort order is unaffected: the
         * key is ordered by oid first and there is one entry per oid here.
         */
        wr64((void *)(uintptr_t)&((struct apfs_omap_key *)(uintptr_t)kp)->
            ok_xid, (uint64_t)(fs->xid + 1));
        wr64((void *)(uintptr_t)&((struct apfs_omap_val *)(uintptr_t)vp)->
            ov_paddr, (uint64_t)new_paddr);
    }
    /*
     * An object map tree node is a PHYSICAL object, so its oid IS its address
     * (spec p.11 "Objects"): copying it to a new block without restamping
     * o_oid leaves it claiming to live somewhere else, which fsck_apfs reports
     * as "om: bt: invalid o_oid".
     */
    wr64(&node->btn_o.o_oid, (uint64_t)dst);
    wr64(&node->btn_o.o_xid, fs->xid + 1);
    seal_object(fs, node);
    err = write_block(fs, dst, node);
out:
    free(node);
    return err;
}

/*
 * Publish a new checkpoint. The container superblock is the commit point and
 * must land LAST: mount selects a checkpoint purely by picking the largest
 * valid xid in the descriptor ring (spec p.26 mount steps 2-4), so nothing
 * before it is reachable until it is written.
 */
static int publish_checkpoint(struct apfsrw *fs, apfs_paddr_t new_comap)
{
    uint64_t desc_base = rd64(&fs->nx.nx_xp_desc_base);
    uint32_t desc_blocks = rd32(&fs->nx.nx_xp_desc_blocks) & 0x7fffffffU;
    uint32_t next = rd32(&fs->nx.nx_xp_desc_next);
    apfs_xid_t new_xid = fs->xid + 1;
    uint32_t map_index, sb_index;
    uint8_t *block;
    int err;

    if (desc_blocks < 2) {
        return APFSRW_ENOTSUP;
    }
    map_index = next % desc_blocks;
    sb_index = (map_index + 1) % desc_blocks;

    block = calloc(1, fs->block_size);
    if (block == NULL)
        return APFSRW_ENOMEM;

    /*
     * Carry the existing checkpoint map forward under the new xid. The
     * ephemeral objects it maps (space manager, reaper) were updated in place,
     * so their addresses are unchanged.
     */
    {
        uint32_t old_index = rd32(&fs->nx.nx_xp_desc_index);
        uint32_t len = rd32(&fs->nx.nx_xp_desc_len);
        uint32_t i;
        int found = 0;

        for (i = 0; i < len; i++) {
            uint32_t slot = (old_index + i) % desc_blocks;
            const struct apfs_checkpoint_map_phys *cpm;

            if (read_object(fs, (apfs_paddr_t)(desc_base + slot), block) !=
                APFSRW_OK)
                continue;
            cpm = (const struct apfs_checkpoint_map_phys *)block;
            if (object_type(cpm->cpm_o.o_type) !=
                APFS_OBJECT_TYPE_CHECKPOINT_MAP)
                continue;
            found = 1;
            break;
        }
        if (!found) {
            err = APFSRW_ENOENT;
            goto out;
        }
        /*
         * The ephemeral objects this map names (space manager, reaper) belong
         * to the checkpoint being published, so they must carry its xid. They
         * were updated in place, which leaves them stamped with the PREVIOUS
         * xid; the APFS kext then rejects the whole checkpoint and rolls the
         * container back, even though fsck_apfs -n accepts it.
         */
        {
            const struct apfs_checkpoint_map_phys *cpm =
                (const struct apfs_checkpoint_map_phys *)block;
            uint32_t n = rd32(&cpm->cpm_count);
            uint8_t *eph = calloc(1, fs->block_size);
            uint32_t j;

            if (eph == NULL) {
                err = APFSRW_ENOMEM;
                goto out;
            }
            if (n > (fs->block_size - sizeof(*cpm)) /
                sizeof(struct apfs_checkpoint_mapping))
                n = 0;
            for (j = 0; j < n; j++) {
                apfs_paddr_t ep =
                    (apfs_paddr_t)rd64(&cpm->cpm_map[j].cpm_paddr);

                if (read_object(fs, ep, eph) != APFSRW_OK)
                    continue;
                wr64(eph + offsetof(struct apfs_obj_phys, o_xid), new_xid);
                seal_object(fs, eph);
                if (write_block(fs, ep, eph) != APFSRW_OK) {
                    free(eph);
                    err = APFSRW_EIO;
                    goto out;
                }
            }
            free(eph);
        }

        wr64(block + offsetof(struct apfs_obj_phys, o_xid), new_xid);
        wr64(block + offsetof(struct apfs_obj_phys, o_oid),
            desc_base + map_index);
        seal_object(fs, block);
        err = write_block(fs, (apfs_paddr_t)(desc_base + map_index), block);
        if (err != APFSRW_OK)
            goto out;
    }

    /*
     * Patch the superblock block as it exists on disk. Our struct stops at
     * nx_fs_oid[], so serialising it would zero every field past that -
     * nx_ephemeral_info, nx_flags, nx_efi_jumpstart and the fusion/keylocker
     * fields - which fsck_apfs reports as invalid nx_ephemeral_info.
     */
    err = read_object(fs, fs->nx_paddr, block);
    if (err != APFSRW_OK)
        goto out;
    wr64(block + offsetof(struct apfs_obj_phys, o_xid), new_xid);
    wr64(block + offsetof(struct apfs_obj_phys, o_oid), 1);
    wr64(block + offsetof(struct apfs_nx_superblock, nx_next_xid),
        new_xid + 1);
    if (fs->next_oid != 0)
        wr64(block + offsetof(struct apfs_nx_superblock, nx_next_oid),
            fs->next_oid);
    wr64(block + offsetof(struct apfs_nx_superblock, nx_omap_oid),
        (uint64_t)new_comap);
    wr32(block + offsetof(struct apfs_nx_superblock, nx_xp_desc_index),
        map_index);
    wr32(block + offsetof(struct apfs_nx_superblock, nx_xp_desc_len), 2);
    wr32(block + offsetof(struct apfs_nx_superblock, nx_xp_desc_next),
        (sb_index + 1) % desc_blocks);
    seal_object(fs, block);
    err = write_block(fs, (apfs_paddr_t)(desc_base + sb_index), block);
    if (err != APFSRW_OK)
        goto out;
    /* The checkpoint moved to a new slot in the descriptor ring. */
    fs->nx_paddr = (apfs_paddr_t)(desc_base + sb_index);

    /* Block zero is a copy of the newest superblock; keep it consistent. */
    err = write_block(fs, 0, block);
out:
    free(block);
    return err;
}

/*
 * Bottom-up copy-on-write of the metadata chain, then the commit.
 * Order matters: every new object must be on disk before the superblock that
 * references it (spec p.11 "objects are never modified in place" + p.26).
 */
static int cow_commit(struct apfsrw *fs, apfs_paddr_t new_root,
    uint64_t new_next_obj_id, uint64_t extra_files, uint64_t extra_dirs,
    uint64_t extra_links, apfs_paddr_t new_extref, uint64_t alloc_delta)
{
    uint8_t *block = NULL;
    uint64_t vomap = 0, vsb = 0, ctree = 0, comap = 0;
    int err;

    (void)new_root;
    err = alloc_blocks(fs, 1, &vomap);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &vsb);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &ctree);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &comap);
    if (err != APFSRW_OK)
        return err;

    block = calloc(1, fs->block_size);
    if (block == NULL)
        return APFSRW_ENOMEM;

    /*
     * The volume object map tree was already copied and repointed as each leaf
     * was rewritten (fstree_put -> omap_set), so fs->volume_omap_tree_paddr is
     * current; the only thing left is to publish an omap_phys that names it.
     */
    err = read_object(fs, fs->volume_omap_paddr, block);
    if (err != APFSRW_OK)
        goto out;
    wr64(block + offsetof(struct apfs_obj_phys, o_oid), vomap);
    wr64(block + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    wr64(block + offsetof(struct apfs_omap_phys, om_tree_oid),
        (uint64_t)fs->volume_omap_tree_paddr);
    seal_object(fs, block);
    err = write_block(fs, (apfs_paddr_t)vomap, block);
    if (err != APFSRW_OK)
        goto out;

    err = read_object(fs, fs->fs_paddr, block);
    if (err != APFSRW_OK)
        goto out;
    wr64(block + offsetof(struct apfs_superblock, apfs_omap_oid), vomap);
    wr64(block + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    if (new_next_obj_id != 0)
        wr64(block + offsetof(struct apfs_superblock, apfs_next_obj_id),
            new_next_obj_id);
    /* apfsck checks each of these against what it finds in the tree. */
    wr64(block + offsetof(struct apfs_superblock, apfs_num_files),
        rd64(&fs->apfs.apfs_num_files) + extra_files);
    wr64(block + offsetof(struct apfs_superblock, apfs_num_directories),
        rd64(&fs->apfs.apfs_num_directories) + extra_dirs);
    wr64(block + offsetof(struct apfs_superblock, apfs_num_symlinks),
        rd64(&fs->apfs.apfs_num_symlinks) + extra_links);
    /* apfs_fs_alloc_count counts blocks the VOLUME owns; fsck_apfs checks it
     * against the extents it finds. */
    wr64(block + offsetof(struct apfs_superblock, apfs_fs_alloc_count),
        (uint64_t)((int64_t)rd64(&fs->apfs.apfs_fs_alloc_count) +
        (int64_t)alloc_delta + fs->alloc_delta));
    if (new_extref != 0)
        wr64(block + offsetof(struct apfs_superblock,
            apfs_extentref_tree_oid), (uint64_t)new_extref);
    seal_object(fs, block);
    err = write_block(fs, (apfs_paddr_t)vsb, block);
    if (err != APFSRW_OK)
        goto out;

    err = omap_tree_copy_update(fs, fs->container_omap_tree_paddr,
        (apfs_paddr_t)ctree, fs->fs_oid, (apfs_paddr_t)vsb);
    if (err != APFSRW_OK)
        goto out;

    err = read_object(fs, fs->container_omap_paddr, block);
    if (err != APFSRW_OK)
        goto out;
    wr64(block + offsetof(struct apfs_obj_phys, o_oid), comap);
    wr64(block + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    wr64(block + offsetof(struct apfs_omap_phys, om_tree_oid), ctree);
    seal_object(fs, block);
    err = write_block(fs, (apfs_paddr_t)comap, block);
    if (err != APFSRW_OK)
        goto out;

    if (apfsrw_sync(fs) != 0) {
        err = APFSRW_EIO;
        goto out;
    }
    /*
     * Hand back the superseded metadata BEFORE publishing, so the space
     * manager is final by the time the checkpoint that describes it lands.
     */
    defer_free(fs, (uint64_t)fs->volume_omap_paddr, 1);
    defer_free(fs, (uint64_t)fs->fs_paddr, 1);
    defer_free(fs, (uint64_t)fs->container_omap_tree_paddr, 1);
    defer_free(fs, (uint64_t)fs->container_omap_paddr, 1);
    /*
     * NOT the old extent reference root: phys_path_publish already freed every
     * node on that path. Freeing it again here would clear the bitmap bit of
     * whatever had since been allocated into that block, which fsck_apfs
     * reports as underallocation.
     */
    if (apfsrw_sync(fs) != 0) {
        err = APFSRW_EIO;
        goto out;
    }

    err = publish_checkpoint(fs, (apfs_paddr_t)comap);
    if (err == APFSRW_OK && apfsrw_sync(fs) != 0)
        err = APFSRW_EIO;
    if (err == APFSRW_OK) {
        flush_deferred(fs);
        fs->alloced_count = 0;
        fs->alloc_delta = 0;
        (void)apfsrw_sync(fs);

        /* Adopt the state just published, or later operations keep reading
         * the superseded superblock and object map. */
        fs->xid += 1;
        fs->fs_paddr = (apfs_paddr_t)vsb;
        fs->volume_omap_oid = vomap;
        fs->volume_omap_paddr = (apfs_paddr_t)vomap;
        fs->container_omap_paddr = (apfs_paddr_t)comap;
        fs->container_omap_tree_paddr = (apfs_paddr_t)ctree;
        if (read_object(fs, fs->fs_paddr, block) == APFSRW_OK) {
            memcpy(&fs->apfs, block, sizeof(fs->apfs));
        } else {
            /* Never continue on a stale volume superblock. */
            fs->fs_oid = 0;
            (void)load_volume(fs);
        }
        if (read_object(fs, fs->nx_paddr, block) == APFSRW_OK)
            memcpy(&fs->nx, block, sizeof(fs->nx));
    }
out:
    free(block);
    return err;
}

/*
 * bt_key_count and bt_node_count in the ROOT node's btree_info_t describe the
 * whole TREE, not that node, so inserting into a leaf deeper down still has to
 * update the root (spec p.126-127). fsck_apfs checks it:
 * "invalid btn_btree.bt_key_count (expected N, actual M)".
 *
 * When the root IS the leaf, repack_leaf has already written the right count.
 */
static int fstree_bump_counts(struct apfsrw *fs, int64_t key_delta,
    int64_t node_delta, uint32_t klen, uint32_t vlen)
{
    struct apfs_btree_node_phys *root = NULL;
    struct apfs_btree_info *bi;
    apfs_paddr_t new_omap_root = 0;
    uint64_t new_root = 0;
    int err;

    root = calloc(1, fs->block_size);
    if (root == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, fs->root_tree_paddr, root);
    if (err != APFSRW_OK)
        goto out;
    if (rd16(&root->btn_flags) & APFS_BTNODE_LEAF) {
        err = APFSRW_OK;                 /* root is the leaf: already counted */
        goto out;
    }
    bi = (struct apfs_btree_info *)((uint8_t *)root + fs->block_size -
        sizeof(*bi));
    wr64(&bi->bt_key_count,
        (uint64_t)((int64_t)rd64(&bi->bt_key_count) + key_delta));
    wr64(&bi->bt_node_count,
        (uint64_t)((int64_t)rd64(&bi->bt_node_count) + node_delta));
    /*
     * bt_longest_key / bt_longest_val are high-water marks over the whole tree
     * ("ever been stored"), and apfsck requires them to be at least the real
     * maximum. Once the root is an index node it never sees the leaf records
     * again, so each insert has to raise them here.
     */
    if (klen > rd32(&bi->bt_longest_key))
        wr32(&bi->bt_longest_key, klen);
    if (vlen > rd32(&bi->bt_longest_val))
        wr32(&bi->bt_longest_val, vlen);

    err = alloc_blocks(fs, 1, &new_root);
    if (err != APFSRW_OK)
        goto out;
    wr64(&root->btn_o.o_xid, fs->xid + 1);
    seal_object(fs, root);
    err = write_block(fs, (apfs_paddr_t)new_root, root);
    if (err != APFSRW_OK)
        goto out;
    defer_free(fs, (uint64_t)fs->root_tree_paddr, 1);

    err = omap_set(fs, fs->volume_omap_tree_paddr, fs->root_tree_oid,
        (apfs_paddr_t)new_root, &new_omap_root);
    if (err != APFSRW_OK)
        goto out;
    fs->volume_omap_tree_paddr = new_omap_root;
    fs->root_tree_paddr = (apfs_paddr_t)new_root;
out:
    free(root);
    return err;
}

/*
 * Add a physical extent record to the extent reference tree.
 *
 * Every extent a file owns must appear here with a reference count, and
 * fsck_apfs cross-checks the two. The tree's nodes are physical, so publishing
 * a new copy means pointing apfs_extentref_tree_oid at the new root. Key is the
 * extent's address; value is j_phys_ext_val_t { len_and_kind, owning_obj_id,
 * refcnt } with the length in BLOCKS and kind APFS_KIND_NEW in the top nibble
 * (spec p.104-105).
 */
static int extentref_add(struct apfsrw *fs, uint64_t paddr, uint32_t nblocks,
    uint64_t owner, uint64_t *new_root)
{
    uint8_t key[8], val[20];
    apfs_paddr_t root = 0;
    int err;

    wr64(key, make_jkey(paddr, APFS_TYPE_EXTENT));
    memset(val, 0, sizeof(val));
    wr64(val, ((uint64_t)nblocks & APFS_OBJ_ID_MASK) |
        ((uint64_t)APFS_KIND_NEW << APFS_OBJ_TYPE_SHIFT));
    wr64(val + 8, owner);
    wr32(val + 16, 1);

    if (fs->extref_paddr == 0)
        fs->extref_paddr =
            (apfs_paddr_t)rd64(&fs->apfs.apfs_extentref_tree_oid);
    /* Upsert: a freed extent's record stays behind, so a reused block's
     * paddr key may already exist; the stale record is simply replaced. */
    err = phys_insert(fs, fs->extref_paddr, BKEY_JKEY,
        key, (uint16_t)sizeof(key), val, (uint16_t)sizeof(val), 1, &root);
    if (err == APFSRW_OK) {
        fs->extref_paddr = root;
        *new_root = (uint64_t)root;
    }
    return err;
}

/*
 * Split a path into its parent directory and final component, resolving the
 * parent to an object id. "/usr/lib/foo" -> parent is /usr/lib, name "foo".
 */
static int split_parent(struct apfsrw *fs, const char *path, uint64_t *parent,
    const char **name_out, uint16_t *namelen_out)
{
    const char *slash = strrchr(path, '/');
    const char *name;
    char dir[1024];
    uint8_t type;
    int err;

    if (path[0] != '/')
        return APFSRW_EINVAL;
    name = (slash != NULL) ? slash + 1 : path;
    if (*name == '\0')
        return APFSRW_EINVAL;
    if (strlen(name) > 255)
        return APFSRW_EINVAL;

    if (slash == path) {
        *parent = APFSRW_ROOT_FILEID;
    } else {
        size_t dlen = (size_t)(slash - path);

        if (dlen >= sizeof(dir))
            return APFSRW_EINVAL;
        memcpy(dir, path, dlen);
        dir[dlen] = '\0';
        err = resolve_path(fs, dir, parent, &type);
        if (err != APFSRW_OK)
            return err;
        if (type != APFSRW_DT_DIR)
            return APFSRW_ENOTDIR;
    }
    *name_out = name;
    *namelen_out = (uint16_t)strlen(name);
    return APFSRW_OK;
}

/*
 * Create one file-system object: a regular file, a directory or a symlink.
 *
 * Records written (spec p.71 "File-System Objects", p.98-103):
 *   INODE      (3)  always, with NAME and - for files - DSTREAM extended
 *                   fields; the DSTREAM is where the size lives
 *   DSTREAM_ID (6)  files only, reference count for the data stream
 *   FILE_EXTENT(8)  files only, one extent covering the content
 *   XATTR      (4)  symlinks only: com.apple.fs.symlink holds the target
 *   DIR_REC    (9)  in the parent, keyed by the name hash
 * and the parent's nchildren is incremented to match.
 */
static int create_entry(struct apfsrw *fs, const char *path, uint16_t ftype,
    const void *data, size_t size, const char *linktarget, uint16_t mode,
    uint32_t uid, uint32_t gid, uint64_t *id_out)
{
    uint8_t *zero = NULL;
    uint64_t parent = 0, fileid, data_block = 0, nblocks = 0, existing;
    uint32_t hash;
    uint16_t namelen = 0;
    const char *name = NULL;
    uint64_t now;
    uint8_t key[8 + 4 + 256];
    uint8_t val[1024];
    uint8_t dtype;
    int err;

    if (fs == NULL || path == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (size > 0xffffffffULL)
        return APFSRW_EOVERFLOW;

    err = split_parent(fs, path, &parent, &name, &namelen);
    if (err != APFSRW_OK) {
        if (apfsrw_getenv("APFSRW_DEBUG"))
            fprintf(stderr, "  split_parent(%s) -> %d\n", path, err);
        return err;
    }
    if (lookup_dirent(fs, parent, name, namelen, &existing, NULL) == APFSRW_OK)
        return APFSRW_EEXIST;
    err = drec_name_hash(fs, name, namelen, &hash);
    if (err != APFSRW_OK)
        return err;
    now = apfsrw_now_ns();

    zero = calloc(1, fs->block_size);
    if (zero == NULL)
        return APFSRW_ENOMEM;

    /*
     * Inside a batch the volume superblock is not re-read between creates, so
     * apfs_next_obj_id there is stale after the first one - taking the id from
     * it would hand every file the SAME object id.
     */
    fileid = (fs->batch && fs->batch_next_oid != 0)
        ? fs->batch_next_oid : rd64(&fs->apfs.apfs_next_obj_id);
    if (fileid < APFSRW_ROOT_FILEID)
        fileid = APFSRW_ROOT_FILEID + 1;

    if (ftype == APFSRW_DT_REG && size > 0) {
        uint64_t i;

        nblocks = (size + fs->block_size - 1) / fs->block_size;
        err = alloc_blocks(fs, (uint32_t)nblocks, &data_block);
        if (err != APFSRW_OK)
            goto out;
        for (i = 0; i < nblocks; i++) {
            size_t off = (size_t)i * fs->block_size;
            size_t n = size > off ? size - off : 0;

            memset(zero, 0, fs->block_size);
            if (n > fs->block_size)
                n = fs->block_size;
            if (n > 0)
                memcpy(zero, (const uint8_t *)data + off, n);
            err = write_raw(fs, (apfs_paddr_t)(data_block + i), zero);
            if (err != APFSRW_OK)
                goto out;
        }
    }

    /* INODE */
    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    memset(val, 0, sizeof(val));
    wr64(val + offsetof(struct apfs_j_inode_val, parent_id), parent);
    wr64(val + offsetof(struct apfs_j_inode_val, private_id), fileid);
    wr64(val + offsetof(struct apfs_j_inode_val, create_time), now);
    wr64(val + offsetof(struct apfs_j_inode_val, mod_time), now);
    wr64(val + offsetof(struct apfs_j_inode_val, change_time), now);
    wr64(val + offsetof(struct apfs_j_inode_val, access_time), now);
    wr32(val + offsetof(struct apfs_j_inode_val, u),
        ftype == APFSRW_DT_DIR ? 0 : 1);   /* nchildren for dirs, nlink else */
    wr32(val + offsetof(struct apfs_j_inode_val, owner), uid);
    wr32(val + offsetof(struct apfs_j_inode_val, group), gid);
    wr16(val + offsetof(struct apfs_j_inode_val, mode), mode);
    wr64(val + offsetof(struct apfs_j_inode_val, internal_flags),
        APFS_INODE_NO_RSRC_FORK);
    {
        uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
        uint16_t nsz = (uint16_t)(namelen + 1);
        uint16_t npad = (uint16_t)((nsz + 7u) & ~7u);
        int want_dstream = (ftype == APFSRW_DT_REG);
        uint16_t nexts = (uint16_t)(want_dstream ? 2 : 1);
        uint32_t doff = fixed + 4 + nexts * 4;

        wr16(val + fixed, nexts);
        wr16(val + fixed + 2,
            (uint16_t)(npad + (want_dstream ? 40 : 0)));
        val[fixed + 4] = APFS_INO_EXT_TYPE_NAME;
        val[fixed + 5] = 0x02;
        wr16(val + fixed + 6, nsz);
        if (want_dstream) {
            val[fixed + 8] = APFS_INO_EXT_TYPE_DSTREAM;
            val[fixed + 9] = 0x20;
            wr16(val + fixed + 10, 40);
        }
        memcpy(val + doff, name, namelen);
        val[doff + namelen] = '\0';
        if (want_dstream) {
            wr64(val + doff + npad, size);
            wr64(val + doff + npad + 8, nblocks * fs->block_size);
            wr64(val + doff + npad + 24, size);
        }
        err = fstree_put(fs, key, 8, val,
            (uint16_t)(doff + npad + (want_dstream ? 40 : 0)), 0);
        if (err != APFSRW_OK)
            goto out;
    }

    if (ftype == APFSRW_DT_REG && size > 0) {
        wr64(key, make_jkey(fileid, APFS_TYPE_DSTREAM_ID));
        memset(val, 0, sizeof(val));
        wr32(val, 1);
        err = fstree_put(fs, key, 8, val, 4, 0);
        if (err != APFSRW_OK)
            goto out;

        wr64(key, make_jkey(fileid, APFS_TYPE_FILE_EXTENT));
        wr64(key + 8, 0);
        memset(val, 0, sizeof(val));
        wr64(val, nblocks * fs->block_size);
        wr64(val + 8, data_block);
        err = fstree_put(fs, key, 16, val, 24, 0);
        if (err != APFSRW_OK)
            goto out;
    }

    if (ftype == APFSRW_DT_LNK) {
        /*
         * A symlink's target is an extended attribute, not file content
         * (spec p.83 "Extended Attributes": com.apple.fs.symlink). Stored
         * inline with XATTR_DATA_EMBEDDED.
         */
        static const char xname[] = "com.apple.fs.symlink";
        uint16_t tlen = (uint16_t)(strlen(linktarget) + 1);
        uint16_t xnlen = (uint16_t)sizeof(xname);

        if (tlen > sizeof(val) - 4)
            return APFSRW_EOVERFLOW;
        wr64(key, make_jkey(fileid, APFS_TYPE_XATTR));
        wr16(key + 8, xnlen);
        memcpy(key + 10, xname, xnlen);
        memset(val, 0, sizeof(val));
        wr16(val, APFS_XATTR_DATA_EMBEDDED |
            APFS_XATTR_FILE_SYSTEM_OWNED);
        wr16(val + 2, tlen);
        memcpy(val + 4, linktarget, tlen);
        err = fstree_put(fs, key, (uint16_t)(10 + xnlen), val,
            (uint16_t)(4 + tlen), 0);
        if (err != APFSRW_OK)
            goto out;
    }

    /* DIR_REC in the parent */
    dtype = (uint8_t)ftype;
    wr64(key, make_jkey(parent, APFS_TYPE_DIR_REC));
    wr32(key + 8, ((hash << 10) & 0xfffffc00U) | ((namelen + 1U) & 0x3ffU));
    memcpy(key + 12, name, namelen);
    key[12 + namelen] = '\0';
    memset(val, 0, sizeof(val));
    wr64(val, fileid);
    wr64(val + 8, now);
    wr16(val + 16, dtype);
    err = fstree_put(fs, key, (uint16_t)(12 + namelen + 1), val, 18, 0);
    if (err != APFSRW_OK)
        goto out;

    /* The parent gained a child. */
    {
        uint8_t pkey[8];
        uint8_t pval[1024];
        uint16_t pvlen = 0;

        wr64(pkey, make_jkey(parent, APFS_TYPE_INODE));
        err = fstree_get(fs, pkey, 8, pval, sizeof(pval), &pvlen);
        if (err != APFSRW_OK) {
            if (apfsrw_getenv("APFSRW_DEBUG"))
                fprintf(stderr, "  parent inode %llu lookup -> %d\n",
                    (unsigned long long)parent, err);
            goto out;
        }
        if (pvlen < sizeof(struct apfs_j_inode_val)) {
            err = APFSRW_EINVAL;
            goto out;
        }
        wr32(pval + offsetof(struct apfs_j_inode_val, u),
            rd32(pval + offsetof(struct apfs_j_inode_val, u)) + 1);
        err = fstree_put(fs, pkey, 8, pval, pvlen, 1);
        if (err != APFSRW_OK)
            goto out;
    }

    if (ftype == APFSRW_DT_REG && size > 0) {
        uint64_t new_extref = 0;

        err = extentref_add(fs, data_block, (uint32_t)nblocks, fileid,
            &new_extref);
        if (err != APFSRW_OK)
            goto out;
        fs->alloc_delta += (int64_t)nblocks;
    }

    if (fs->batch) {
        /* Accumulate; the commit happens at apfsrw_batch_end(). */
        fs->batch_files += (ftype == APFSRW_DT_REG);
        fs->batch_dirs += (ftype == APFSRW_DT_DIR);
        fs->batch_links += (ftype == APFSRW_DT_LNK);
        fs->batch_next_oid = fileid + 1;
        err = APFSRW_OK;

    } else {
        err = cow_commit(fs, fs->root_tree_paddr, fileid + 1,
            ftype == APFSRW_DT_REG ? 1 : 0, ftype == APFSRW_DT_DIR ? 1 : 0,
            ftype == APFSRW_DT_LNK ? 1 : 0, fs->extref_paddr, 0);
    }
    if (err == APFSRW_OK && id_out != NULL)
        *id_out = fileid;
out:
    if (err != APFSRW_OK)
        txn_rollback(fs);
    free(zero);
    return err;
}

/*
 * Group many creates into one transaction. Everything still goes through the
 * normal copy-on-write machinery; only the commit - and its fsyncs - is
 * deferred. A failure inside a batch leaves the container at the LAST COMMITTED
 * checkpoint, so the partial work is simply never published.
 */
int apfsrw_batch_begin(struct apfsrw *fs)
{
    if (fs == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (fs->batch)
        return APFSRW_EINVAL;
    fs->batch = 1;
    fs->batch_files = 0;
    fs->batch_dirs = 0;
    fs->batch_links = 0;
    fs->batch_next_oid = 0;
    return APFSRW_OK;
}

/* Blocks superseded but not yet released; they cannot be reused until a
 * checkpoint lands, so a long batch should checkpoint when this grows. */
uint32_t apfsrw_batch_pending(struct apfsrw *fs)
{
    return fs == NULL ? 0 : fs->deferred_count;
}

int apfsrw_batch_end(struct apfsrw *fs)
{
    int err;

    if (fs == NULL)
        return APFSRW_EINVAL;
    if (!fs->batch)
        return APFSRW_EINVAL;
    fs->batch = 0;
    if (fs->batch_next_oid == 0)
        return APFSRW_OK;                /* nothing was created */
    err = cow_commit(fs, fs->root_tree_paddr, fs->batch_next_oid,
        fs->batch_files, fs->batch_dirs, fs->batch_links, fs->extref_paddr, 0);
    if (err != APFSRW_OK)
        txn_rollback(fs);
    return err;
}

int apfsrw_create_file(struct apfsrw *fs, const char *path, const void *data,
    size_t size, uint16_t mode, uint32_t uid, uint32_t gid)
{
    if (data == NULL && size != 0)
        return APFSRW_EINVAL;
    return create_entry(fs, path, APFSRW_DT_REG, data, size, NULL,
        (uint16_t)(0100000u | (mode & 07777u)), uid, gid, NULL);
}

int apfsrw_mkdir(struct apfsrw *fs, const char *path, uint16_t mode,
    uint32_t uid, uint32_t gid)
{
    return create_entry(fs, path, APFSRW_DT_DIR, NULL, 0, NULL,
        (uint16_t)(0040000u | (mode & 07777u)), uid, gid, NULL);
}

int apfsrw_mknod(struct apfsrw *fs, const char *path, uint16_t ftype,
    uint16_t mode, uint32_t uid, uint32_t gid)
{
    /*
     * create_entry's per-type work is all keyed off REG/DIR/LNK, so these
     * types correctly get no data stream, no symlink xattr and nlink 1.
     */
    if (ftype != APFSRW_DT_SOCK && ftype != APFSRW_DT_FIFO &&
        ftype != APFSRW_DT_CHR && ftype != APFSRW_DT_BLK)
        return APFSRW_EINVAL;
    return create_entry(fs, path, ftype, NULL, 0, NULL, mode, uid, gid, NULL);
}

/* Delete every FILE_EXTENT record of a stream and defer-free its blocks
 * (spec p.102 j_file_extent_key_t). Extent-ref entries are left stale. */
#define APFSRW_DROP_MAX_EXTENTS 32

struct drop_extents_ctx {
    uint64_t stream_id;
    uint32_t n;
    int overflow;
    struct { uint64_t logical, phys, len; } ext[APFSRW_DROP_MAX_EXTENTS];
};

static int drop_extents_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct drop_extents_ctx *c = (struct drop_extents_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp, *valp;
        uint16_t key_len, val_len;
        const struct apfs_j_file_extent_key *key;
        const struct apfs_j_file_extent_val *val;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            return err;
        if (key_len < sizeof(*key) || val_len < sizeof(*val))
            continue;
        key = (const struct apfs_j_file_extent_key *)keyp;
        if (key_id(rd64(&key->hdr.obj_id_and_type)) != c->stream_id ||
            key_type(rd64(&key->hdr.obj_id_and_type)) != APFS_TYPE_FILE_EXTENT)
            continue;
        if (c->n == APFSRW_DROP_MAX_EXTENTS) {
            c->overflow = 1;
            return 1;
        }
        val = (const struct apfs_j_file_extent_val *)valp;
        c->ext[c->n].logical = rd64(&key->logical_addr);
        c->ext[c->n].phys = rd64(&val->phys_block_num);
        c->ext[c->n].len =
            rd64(&val->len_and_flags) & APFS_FILE_EXTENT_LEN_MASK;
        c->n++;
    }
    return APFSRW_OK;
}

static int drop_extents(struct apfsrw *fs, uint64_t stream_id,
    uint64_t *freed_blocks)
{
    struct drop_extents_ctx c;
    uint64_t freed = 0;
    uint32_t i;
    int err;

    memset(&c, 0, sizeof(c));
    c.stream_id = stream_id;
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, stream_id,
        stream_id, drop_extents_cb, &c);
    if (err != APFSRW_OK && err != 1)
        return err;
    if (c.overflow)
        return APFSRW_ENOTSUP;

    for (i = 0; i < c.n; i++) {
        uint8_t key[16];

        wr64(key, make_jkey(stream_id, APFS_TYPE_FILE_EXTENT));
        wr64(key + 8, c.ext[i].logical);
        err = fstree_del(fs, key, 16);
        if (err != APFSRW_OK)
            return err;
        if (c.ext[i].phys != 0 && c.ext[i].len != 0) {
            uint32_t nb = (uint32_t)((c.ext[i].len +
                fs->block_size - 1) / fs->block_size);

            err = defer_free(fs, c.ext[i].phys, nb);
            if (err != APFSRW_OK)
                return err;
            freed += nb;
        }
    }
    if (freed_blocks != NULL)
        *freed_blocks = freed;
    return APFSRW_OK;
}

/* Patch the DSTREAM xfield sizes in place (spec p.106 j_dstream_t). */
static int inode_set_dstream(uint8_t *val, uint16_t val_len, uint64_t size,
    uint64_t alloced)
{
    uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
    uint32_t num, i, desc, data;

    if (val_len < fixed + 4U)
        return APFSRW_ENOENT;
    num = rd16(val + fixed);
    desc = fixed + 4U;
    data = desc + num * 4U;
    if (data > val_len)
        return APFSRW_EINVAL;
    for (i = 0; i < num; i++) {
        uint8_t type = val[desc + i * 4U];
        uint16_t fsize = rd16(val + desc + i * 4U + 2U);

        if (data + fsize > val_len)
            return APFSRW_EINVAL;
        if (type == APFS_INO_EXT_TYPE_DSTREAM) {
            if (fsize < 32)
                return APFSRW_EINVAL;
            wr64(val + data, size);
            wr64(val + data + 8, alloced);
            wr64(val + data + 24, size);
            return APFSRW_OK;
        }
        data += ((uint32_t)fsize + 7U) & ~7U;
    }
    return APFSRW_ENOENT;
}

/*
 * Replace a regular file's entire content, keeping its object id. Old extents
 * are dropped and deferred-freed; the new content lands in one fresh extent.
 */
int apfsrw_set_file_content(struct apfsrw *fs, const char *path,
    const void *data, size_t size)
{
    uint8_t *block = NULL;
    uint64_t fileid = 0, priv = 0, old_size = 0;
    uint64_t data_block = 0, nblocks = 0, freed = 0, now;
    uint8_t type = 0;
    uint8_t key[16];
    uint8_t ival[1024];
    uint16_t ivlen = 0;
    int err;

    if (fs == NULL || path == NULL || (data == NULL && size != 0))
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (fs->batch)
        return APFSRW_EINVAL;
    if (size > 0xffffffffULL)
        return APFSRW_EOVERFLOW;

    err = resolve_path(fs, path, &fileid, &type);
    if (err != APFSRW_OK)
        return err;
    if (type != APFSRW_DT_REG)
        return APFSRW_EINVAL;

    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_get(fs, key, 8, ival, sizeof(ival), &ivlen);
    if (err != APFSRW_OK)
        return err;
    if (ivlen < sizeof(struct apfs_j_inode_val))
        return APFSRW_EINVAL;
    priv = rd64(ival + offsetof(struct apfs_j_inode_val, private_id));
    (void)inode_dstream_size(ival, ivlen, &old_size);

    err = drop_extents(fs, priv, &freed);
    if (err != APFSRW_OK)
        goto fail;

    if (size > 0) {
        uint64_t i;

        block = calloc(1, fs->block_size);
        if (block == NULL) {
            err = APFSRW_ENOMEM;
            goto fail;
        }
        nblocks = (size + fs->block_size - 1) / fs->block_size;
        err = alloc_blocks(fs, (uint32_t)nblocks, &data_block);
        if (err != APFSRW_OK)
            goto fail;
        for (i = 0; i < nblocks; i++) {
            size_t off = (size_t)i * fs->block_size;
            size_t n = size - off;

            if (n > fs->block_size)
                n = fs->block_size;
            memset(block, 0, fs->block_size);
            memcpy(block, (const uint8_t *)data + off, n);
            err = write_raw(fs, (apfs_paddr_t)(data_block + i), block);
            if (err != APFSRW_OK)
                goto fail;
        }

        /* One extent covering the whole content. */
        {
            uint8_t ekey[16], eval[24];

            wr64(ekey, make_jkey(priv, APFS_TYPE_FILE_EXTENT));
            wr64(ekey + 8, 0);
            memset(eval, 0, sizeof(eval));
            wr64(eval, nblocks * fs->block_size);
            wr64(eval + 8, data_block);
            err = fstree_put(fs, ekey, 16, eval, 24, 0);
            if (err != APFSRW_OK)
                goto fail;
        }

        /* A file created empty has no DSTREAM_ID record yet. */
        {
            uint8_t dkey[8], dval[4];
            uint16_t dvlen = 0;

            wr64(dkey, make_jkey(priv, APFS_TYPE_DSTREAM_ID));
            err = fstree_get(fs, dkey, 8, dval, sizeof(dval), &dvlen);
            if (err == APFSRW_ENOENT) {
                wr32(dval, 1);
                err = fstree_put(fs, dkey, 8, dval, 4, 0);
            }
            if (err != APFSRW_OK)
                goto fail;
        }

        {
            uint64_t new_extref = 0;

            err = extentref_add(fs, data_block, (uint32_t)nblocks,
                priv, &new_extref);
            if (err != APFSRW_OK)
                goto fail;
        }
    } else {
        uint8_t dkey[8];

        wr64(dkey, make_jkey(priv, APFS_TYPE_DSTREAM_ID));
        err = fstree_del(fs, dkey, 8);
        if (err != APFSRW_OK && err != APFSRW_ENOENT)
            goto fail;
    }

    now = apfsrw_now_ns();
    wr64(ival + offsetof(struct apfs_j_inode_val, mod_time), now);
    wr64(ival + offsetof(struct apfs_j_inode_val, change_time), now);
    err = inode_set_dstream(ival, ivlen, size, nblocks * fs->block_size);
    if (err != APFSRW_OK)
        goto fail;
    err = fstree_put(fs, key, 8, ival, ivlen, 1);
    if (err != APFSRW_OK)
        goto fail;

    fs->alloc_delta += (int64_t)nblocks - (int64_t)freed;
    err = cow_commit(fs, fs->root_tree_paddr,
        rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0, fs->extref_paddr, 0);
    if (err != APFSRW_OK)
        goto fail;
    free(block);
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    free(block);
    return err;
}

int apfsrw_unlink(struct apfsrw *fs, const char *path)
{
    uint64_t parent = 0, fileid = 0, isize = 0, priv = 0;
    const char *name = NULL;
    uint16_t namelen = 0;
    uint32_t hash = 0;
    uint8_t dtype = 0;
    uint8_t key[8 + 4 + 256];
    uint8_t pkey[8];
    uint8_t pval[1024];
    uint16_t pvlen = 0;
    int err;

    if (fs == NULL || path == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;

    err = split_parent(fs, path, &parent, &name, &namelen);
    if (err != APFSRW_OK)
        return err;
    err = lookup_dirent(fs, parent, name, namelen, &fileid, &dtype);
    if (err != APFSRW_OK)
        return err;
    err = lookup_inode(fs, fileid, &isize, &priv);
    if (err != APFSRW_OK)
        return err;

    /* A file's data stream goes with it: extents deleted, blocks freed. */
    if (dtype == APFSRW_DT_REG) {
        uint64_t freed = 0;
        uint8_t dkey[8];

        err = drop_extents(fs, priv, &freed);
        if (err != APFSRW_OK)
            goto fail;
        wr64(dkey, make_jkey(priv, APFS_TYPE_DSTREAM_ID));
        err = fstree_del(fs, dkey, 8);
        if (err != APFSRW_OK && err != APFSRW_ENOENT)
            goto fail;
        fs->alloc_delta -= (int64_t)freed;
    }

    /* Read the entry's inode so a non-empty directory can be rejected. */
    wr64(pkey, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_get(fs, pkey, 8, pval, sizeof(pval), &pvlen);
    if (err != APFSRW_OK)
        goto fail;
    if (dtype == APFSRW_DT_DIR &&
        rd32(pval + offsetof(struct apfs_j_inode_val, u)) != 0) {
        err = APFSRW_ENOTEMPTY;
        goto fail;
    }

    /* Symlinks keep their target in an xattr; drop it with the inode. */
    if (dtype == APFSRW_DT_LNK) {
        static const char xname[] = "com.apple.fs.symlink";
        uint8_t xkey[8 + 2 + sizeof(xname)];

        wr64(xkey, make_jkey(fileid, APFS_TYPE_XATTR));
        wr16(xkey + 8, (uint16_t)sizeof(xname));
        memcpy(xkey + 10, xname, sizeof(xname));
        err = fstree_del(fs, xkey, (uint16_t)(10 + sizeof(xname)));
        if (err != APFSRW_OK && err != APFSRW_ENOENT)
            goto fail;
    }

    /* The directory entry in the parent. */
    err = drec_name_hash(fs, name, namelen, &hash);
    if (err != APFSRW_OK)
        goto fail;
    wr64(key, make_jkey(parent, APFS_TYPE_DIR_REC));
    wr32(key + 8, ((hash << 10) & 0xfffffc00U) | ((namelen + 1U) & 0x3ffU));
    memcpy(key + 12, name, namelen);
    key[12 + namelen] = '\0';
    err = fstree_del(fs, key, (uint16_t)(12 + namelen + 1));
    if (err != APFSRW_OK)
        goto fail;

    /* Then the inode itself. */
    err = fstree_del(fs, pkey, 8);
    if (err != APFSRW_OK)
        goto fail;

    /* One fewer child in the parent. */
    {
        uint8_t ppkey[8];
        uint8_t ppval[1024];
        uint16_t ppvlen = 0;

        wr64(ppkey, make_jkey(parent, APFS_TYPE_INODE));
        err = fstree_get(fs, ppkey, 8, ppval, sizeof(ppval), &ppvlen);
        if (err != APFSRW_OK)
            goto fail;
        wr32(ppval + offsetof(struct apfs_j_inode_val, u),
            rd32(ppval + offsetof(struct apfs_j_inode_val, u)) - 1);
        err = fstree_put(fs, ppkey, 8, ppval, ppvlen, 1);
        if (err != APFSRW_OK)
            goto fail;
    }

    /*
     * cow_commit takes the count deltas unsigned and adds them to the stored
     * totals, so -1 wraps to exactly "one fewer".
     */
    err = cow_commit(fs, fs->root_tree_paddr,
        rd64(&fs->apfs.apfs_next_obj_id),
        dtype == APFSRW_DT_REG ? (uint64_t)-1 : 0,
        dtype == APFSRW_DT_DIR ? (uint64_t)-1 : 0,
        dtype == APFSRW_DT_LNK ? (uint64_t)-1 : 0, fs->extref_paddr, 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    return err;
}

/* Rebuild an inode value with a new NAME extended field, carrying every other
 * field over unchanged (spec p.108-109 xf_blob_t / x_field_t). */
static int inode_val_rename(const uint8_t *old, uint16_t old_len,
    const char *name, uint16_t namelen, uint64_t new_parent,
    uint8_t *out, uint16_t *out_len)
{
    uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
    uint32_t num, i, desc, odata, ndata;

    if (old_len < fixed + 4U)
        return APFSRW_EINVAL;
    memcpy(out, old, fixed);
    wr64(out + offsetof(struct apfs_j_inode_val, parent_id), new_parent);
    num = rd16(old + fixed);
    memcpy(out + fixed, old + fixed, 4);
    desc = fixed + 4U;
    odata = desc + num * 4U;
    ndata = odata;
    if (odata > old_len)
        return APFSRW_EINVAL;

    for (i = 0; i < num; i++) {
        uint8_t type = old[desc + i * 4U];
        uint16_t osize = rd16(old + desc + i * 4U + 2U);
        uint16_t nsize = osize;

        if (odata + osize > old_len)
            return APFSRW_EINVAL;
        if (type == APFS_INO_EXT_TYPE_NAME)
            nsize = (uint16_t)(namelen + 1);
        if (ndata + ((nsize + 7U) & ~7U) > 1024U)
            return APFSRW_EOVERFLOW;
        out[desc + i * 4U] = type;
        out[desc + i * 4U + 1U] = old[desc + i * 4U + 1U];
        wr16(out + desc + i * 4U + 2U, nsize);
        memset(out + ndata, 0, (nsize + 7U) & ~7U);
        if (type == APFS_INO_EXT_TYPE_NAME) {
            memcpy(out + ndata, name, namelen);
            out[ndata + namelen] = '\0';
        } else {
            memcpy(out + ndata, old + odata, osize);
        }
        odata += ((uint32_t)osize + 7U) & ~7U;
        ndata += ((uint32_t)nsize + 7U) & ~7U;
    }
    /* xf_used_data: total bytes of field data. */
    wr16(out + fixed + 2, (uint16_t)(ndata - odata +
        rd16(old + fixed + 2)));
    *out_len = (uint16_t)ndata;
    return APFSRW_OK;
}

int apfsrw_rename(struct apfsrw *fs, const char *from, const char *to)
{
    uint64_t fparent = 0, tparent = 0, fileid = 0, existing = 0;
    const char *fname = NULL, *tname = NULL;
    uint16_t fnamelen = 0, tnamelen = 0;
    uint32_t hash;
    uint8_t dtype = 0;
    uint64_t now;
    uint8_t key[8 + 4 + 256];
    uint8_t ival[1024], nval[1024];
    uint16_t ivlen = 0, nvlen = 0;
    int err;

    if (fs == NULL || from == NULL || to == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (fs->batch)
        return APFSRW_EINVAL;

    err = split_parent(fs, from, &fparent, &fname, &fnamelen);
    if (err != APFSRW_OK)
        return err;
    err = lookup_dirent(fs, fparent, fname, fnamelen, &fileid, &dtype);
    if (err != APFSRW_OK)
        return err;
    /* The target is replaced, as rename(2) requires. */
    err = split_parent(fs, to, &tparent, &tname, &tnamelen);
    if (err != APFSRW_OK)
        return err;
    if (lookup_dirent(fs, tparent, tname, tnamelen, &existing, NULL) ==
        APFSRW_OK) {
        if (existing == fileid)
            return APFSRW_OK;
        err = apfsrw_unlink(fs, to);
        if (err != APFSRW_OK)
            return err;
        /* unlink committed; re-resolve both parents' current state. */
        err = split_parent(fs, from, &fparent, &fname, &fnamelen);
        if (err != APFSRW_OK)
            return err;
        err = split_parent(fs, to, &tparent, &tname, &tnamelen);
        if (err != APFSRW_OK)
            return err;
    }
    now = apfsrw_now_ns();

    /* Rewrite the inode: new parent id, new NAME extended field. */
    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_get(fs, key, 8, ival, sizeof(ival), &ivlen);
    if (err != APFSRW_OK)
        return err;
    err = inode_val_rename(ival, ivlen, tname, tnamelen, tparent, nval,
        &nvlen);
    if (err != APFSRW_OK)
        return err;
    wr64(nval + offsetof(struct apfs_j_inode_val, change_time), now);
    err = fstree_put(fs, key, 8, nval, nvlen, 1);
    if (err != APFSRW_OK)
        goto fail;

    /* Old directory entry out, new one in. */
    err = drec_name_hash(fs, fname, fnamelen, &hash);
    if (err != APFSRW_OK)
        goto fail;
    wr64(key, make_jkey(fparent, APFS_TYPE_DIR_REC));
    wr32(key + 8, ((hash << 10) & 0xfffffc00U) | ((fnamelen + 1U) & 0x3ffU));
    memcpy(key + 12, fname, fnamelen);
    key[12 + fnamelen] = '\0';
    err = fstree_del(fs, key, (uint16_t)(12 + fnamelen + 1));
    if (err != APFSRW_OK)
        goto fail;

    err = drec_name_hash(fs, tname, tnamelen, &hash);
    if (err != APFSRW_OK)
        goto fail;
    wr64(key, make_jkey(tparent, APFS_TYPE_DIR_REC));
    wr32(key + 8, ((hash << 10) & 0xfffffc00U) | ((tnamelen + 1U) & 0x3ffU));
    memcpy(key + 12, tname, tnamelen);
    key[12 + tnamelen] = '\0';
    {
        uint8_t dval[18];

        memset(dval, 0, sizeof(dval));
        wr64(dval, fileid);
        wr64(dval + 8, now);
        wr16(dval + 16, dtype);
        err = fstree_put(fs, key, (uint16_t)(12 + tnamelen + 1), dval, 18,
            0);
        if (err != APFSRW_OK)
            goto fail;
    }

    if (fparent != tparent) {
        uint8_t pkey[8];
        uint8_t pval[1024];
        uint16_t pvlen = 0;

        wr64(pkey, make_jkey(fparent, APFS_TYPE_INODE));
        err = fstree_get(fs, pkey, 8, pval, sizeof(pval), &pvlen);
        if (err != APFSRW_OK)
            goto fail;
        wr32(pval + offsetof(struct apfs_j_inode_val, u),
            rd32(pval + offsetof(struct apfs_j_inode_val, u)) - 1);
        err = fstree_put(fs, pkey, 8, pval, pvlen, 1);
        if (err != APFSRW_OK)
            goto fail;

        wr64(pkey, make_jkey(tparent, APFS_TYPE_INODE));
        err = fstree_get(fs, pkey, 8, pval, sizeof(pval), &pvlen);
        if (err != APFSRW_OK)
            goto fail;
        wr32(pval + offsetof(struct apfs_j_inode_val, u),
            rd32(pval + offsetof(struct apfs_j_inode_val, u)) + 1);
        err = fstree_put(fs, pkey, 8, pval, pvlen, 1);
        if (err != APFSRW_OK)
            goto fail;
    }

    err = cow_commit(fs, fs->root_tree_paddr,
        rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0, fs->extref_paddr, 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    return err;
}

int apfsrw_symlink(struct apfsrw *fs, const char *path, const char *target,
    uint32_t uid, uint32_t gid)
{
    if (target == NULL)
        return APFSRW_EINVAL;
    return create_entry(fs, path, APFSRW_DT_LNK, NULL, 0, target,
        (uint16_t)(0120000u | 0777u), uid, gid, NULL);
}

int apfsrw_create_root_file(struct apfsrw *fs, const char *name,
    const void *data, size_t size)
{
    char path[300];

    if (name == NULL)
        return APFSRW_EINVAL;
    if (name[0] == '/')
        snprintf(path, sizeof(path), "%s", name);
    else
        snprintf(path, sizeof(path), "/%s", name);
    return apfsrw_create_file(fs, path, data, size, 0644, 0, 0);
}
