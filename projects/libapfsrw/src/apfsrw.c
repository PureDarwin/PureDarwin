/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#ifndef APFSRW_KERNEL
#define _FILE_OFFSET_BITS 64
#define _XOPEN_SOURCE 700
#endif

#include "apfsrw/apfsrw.h"
#ifndef APFSRW_KERNEL
#include "apfsrw_lzvn.h"
#include "libzbitmap.h"
#endif

#include "apfsrw/apfsrw_port.h"

#define APFS_NX_MAGIC 0x4253584eU
#define APFS_APSB_MAGIC 0x42535041U
#define APFS_OBJECT_TYPE_MASK 0x0000ffffU
#define APFS_OBJ_PHYSICAL 0x40000000U
#define APFS_OBJECT_TYPE_BTREE 0x00000002U
#define APFS_OBJECT_TYPE_BTREE_NODE 0x00000003U
#define APFS_OBJECT_TYPE_OMAP 0x0000000bU
#define APFS_OBJECT_TYPE_FS 0x0000000dU
#define APFS_OBJECT_TYPE_FSTREE 0x0000000eU
#define APFS_OBJECT_TYPE_SPACEMAN 0x00000005U
#define APFS_OBJECT_TYPE_CHECKPOINT_MAP 0x0000000cU
#define APFS_OBJECT_TYPE_SPACEMAN_CIB 0x00000007U
// spec p.177 "Object Types and Flags": OBJ_EPHEMERAL 0x80000000
#define APFS_OBJ_EPHEMERAL 0x80000000U
#define APFS_BTNODE_ROOT 0x0001U
#define APFS_BTNODE_LEAF 0x0002U
#define APFS_BTNODE_FIXED_KV_SIZE 0x0004U
#define APFS_BTNODE_HASHED 0x0008U
// spec p.132 BTNODE_NOHEADER: the node's obj_phys_t is not populated, so it carries no checksum to
// verify. The header SLOT still exists - btn_flags is at the usual offset - it is simply zero-filled
#define APFS_BTNODE_NOHEADER 0x0010U
// btree_info_t.bt_flags. spec p.132 "B-Tree Flags".
// Neither PHYSICAL nor EPHEMERAL set means child links are virtual oids
#define APFS_BTREE_EPHEMERAL 0x00000008U
#define APFS_BTREE_PHYSICAL 0x00000010U
// spec p.132 "B-Tree Flags": BTREE_HASHED - nonleaf children store a hash
// of the child node alongside its oid (btn_index_node_val_t, spec p.127)
#define APFS_BTREE_HASHED 0x00000080U
// omap_val_t.ov_flags, spec p.48. NOHEADER means no obj_phys_t header,
// so there is no checksum and node fields are read without the header offset
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
// Hard links (spec p.72 j_obj_types. Records p.104-105)
#define APFS_TYPE_SIBLING_LINK 5U
#define APFS_TYPE_SIBLING_MAP 12U
// spec p.111 "Extended-Field Types"
#define APFS_DREC_EXT_TYPE_SIBLING_ID 1U
// spec p.96 APFS_INCOMPAT_CASE_INSENSITIVE
#define APFS_INCOMPAT_CASE_INSENSITIVE 0x00000001ULL
// spec p.96 APFS_INCOMPAT_NORMALIZATION_INSENSITIVE
#define APFS_INCOMPAT_NORMALIZATION_INSENSITIVE 0x00000008ULL
#define APFS_INCOMPAT_SEALED_VOLUME 0x00000020ULL
// spec p.94-95 "Extended-Attribute Flags"
#define APFS_XATTR_DATA_STREAM 0x0001U
#define APFS_XATTR_DATA_EMBEDDED 0x0002U
// spec p.94-95: the symlink target xattr must be marked system-owned
#define APFS_XATTR_FILE_SYSTEM_OWNED 0x0004U
// decmpfs_disk_header, from src/Kernel/xnu/bsd/sys/decmpfs.h (APSL) - NOT the APFS spec,
// which never mentions decmpfs. DECMPFS_MAGIC is 'cmpf'
#define APFS_DECMPFS_MAGIC 0x636d7066U
#define APFS_DECMPFS_TYPE_UNCOMPRESSED 1U
// Measured on 25G83, outside the spec: type 8 = LZVN in the ResourceFork stream,
// type 9 = raw content inline in the decmpfs xattr. See apfs-notes.txt
#define APFS_DECMPFS_TYPE_LZVN_RSRC 8U
#define APFS_DECMPFS_TYPE_LZBITMAP_RSRC 14U
#define APFS_DECMPFS_TYPE_RAW_XATTR 9U
#define APFS_DECMPFS_RAW_MARKER 0xccU
#define APFS_DECMPFS_CHUNK 65536U
#define APFS_DECMPFS_XATTR "com.apple.decmpfs\0"
#define APFS_RSRCFORK_XATTR "com.apple.ResourceFork\0"
// spec p.111 "Extended-Field Types": INO_EXT_TYPE_DSTREAM 8
// j_inode_flags: INODE_NO_RSRC_FORK (fsck_apfs asks for it on files with no resource fork)
#define APFS_INODE_NO_RSRC_FORK 0x8000ULL
#define APFS_INO_EXT_TYPE_NAME 4U
#define APFS_INO_EXT_TYPE_DSTREAM 8U
// Holes leave size above the allocation. fsck_apfs then wants these two
#define APFS_INODE_IS_SPARSE 0x200ULL
#define APFS_INO_EXT_TYPE_SPARSE_BYTES 13U
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

// spec p.40-41 checkpoint_mapping_t / checkpoint_map_phys_t
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

// spec p.159 chunk_info_t / chunk_info_block
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

// spec p.160-161 spaceman_device_t and spaceman_phys_t, leading fields only.
// sm_dev[] is at a fixed offset and the CIB address array sits at sm_dev[].sm_addr_offset
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
    uint8_t sm_fq[120];                  // spaceman_free_queue_t sm_fq[3]
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

// spec p.106 j_xattr_dstream_t: an xattr whose value is a data stream
struct apfs_j_xattr_dstream {
    uint64_t xattr_obj_id;
    uint64_t size;
    uint64_t alloced_size;
    uint64_t default_crypto_id;
    uint64_t total_bytes_written;
    uint64_t total_bytes_read;
} __attribute__((packed));

// decmpfs_disk_header (xnu bsd/sys/decmpfs.h), little-endian on disk
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
    // Kernel builds have no descriptor. This carries the device vnode
    void *io_ctx;
    off_t base_off;                     // container start inside the file
    uint8_t *frozen;                    // Savepoint: blocks that must not be reused
    uint64_t data_cursor;               // Where file data last landed
    uint64_t meta_cursor;               // The last metadata block: the next goes right below it
    uint32_t alloc_minrun;              // Reverse scans skip free runs shorter than this
    int writable;
    uint64_t image_blocks;
    uint32_t block_size;
    uint64_t block_count;
    struct apfs_nx_superblock nx;
    struct apfs_superblock apfs;
    apfs_xid_t xid;
    uint32_t vol_slot;                  // nx_fs_oid[] slot this handle works on
    apfs_oid_t fs_oid;
    apfs_paddr_t fs_paddr;
    apfs_paddr_t container_omap_paddr;
    apfs_paddr_t container_omap_tree_paddr;
    apfs_oid_t volume_omap_oid;
    apfs_paddr_t volume_omap_paddr;
    apfs_paddr_t volume_omap_tree_paddr;
    apfs_xid_t want_xid;
    int trust_block0;                   // Refresh after our own commit: skip the ring scan
    apfs_paddr_t nx_paddr;
    apfs_oid_t root_tree_oid;
    apfs_oid_t next_oid;
    int64_t alloc_delta;
    int64_t other_delta;                // sockets/fifos/devices created - removed
    int32_t role_pending;               // apfs_role to stamp at the next commit, or -1
    // A volume created this transaction:
    // its superblock still has to be entered in the container object map and nx_fs_oid[] at commit
    int newvol_pending;
    uint32_t newvol_slot;
    apfs_oid_t newvol_oid;
    apfs_paddr_t newvol_paddr;
    int batch;
    int batch_dirty;            // a mutation is folded in but not committed
    uint64_t batch_files;
    uint64_t batch_dirs;
    uint64_t batch_links;
    uint64_t batch_next_oid;
    // state after the batch's last finished op: a failed op rolls back to it, not to the checkpoint
    uint32_t op_alloced, op_deferred;
    apfs_paddr_t op_root, op_vomap_tree, op_extref, op_fext;
    int64_t op_alloc_delta, op_other_delta;
    // Current extent reference tree root. Must be tracked here:
    // inside a batch fs->apfs is not re-read, so the copy there goes stale after the first insert
    apfs_paddr_t extref_paddr;
    uint64_t *deferred;
    uint32_t deferred_count;
    uint32_t deferred_cap;
    uint64_t *alloced;
    uint32_t alloced_count;
    uint32_t alloced_cap;
    apfs_paddr_t root_tree_paddr;
    // a sealed volume keeps its file extents in a tree of their own (spec p.153)
    apfs_paddr_t fext_paddr;
    // Block cache: copies of blocks this handle read or wrote, write-through.
    // Only another handle's commit can make it stale (apfsrw_cache_drop)
    uint8_t *bc_data;
    struct bc_ent { apfs_paddr_t paddr; uint8_t valid, verified; } *bc_ents;
    // Allocation cache: the space manager, the chunk-info block and the bitmap the
    // allocator last touched, written back once per commit instead of per block
    uint8_t *ac_sm, *ac_cib, *ac_bm;
    apfs_paddr_t ac_sm_paddr, ac_cib_paddr, ac_bm_paddr;
    uint8_t ac_sm_dirty, ac_cib_dirty, ac_bm_dirty;
    // Record buffers: a free list per power-of-two class, so rewriting a leaf (one
    // copy of every key and value) does not go to the allocator each time
    void *rec_free[10];
    unsigned rec_free_n[10];
};
#define APFSRW_BCACHE_ENTS	1024

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

// Sums stay below 2^63 for well over 4KB of words, so the modulo can be taken
// after a run of words rather than per word: no 64-bit division per word
static uint64_t fletcher64(const uint8_t *block, size_t size)
{
    uint64_t lo = 0;
    uint64_t hi = 0;
    uint64_t check1;
    uint64_t check2;
    size_t off;

    for (off = APFS_MAX_CKSUM_SIZE; off + sizeof(uint32_t) <= size;) {
        size_t end = off + 32 * sizeof(uint32_t);

        if (end > size)
            end = size;
        for (; off + sizeof(uint32_t) <= end; off += sizeof(uint32_t)) {
            lo += rd32(block + off);
            hi += lo;
        }
        lo %= 0xffffffffULL;
        hi %= 0xffffffffULL;
    }
    check1 = 0xffffffffULL - ((lo + hi) % 0xffffffffULL);
    check2 = 0xffffffffULL - ((lo + check1) % 0xffffffffULL);
    return (check2 << 32) | check1;
}

#ifndef APFSRW_KERNEL
uint64_t apfsrw_now_ns(void)
{
    struct timespec ts;

    return (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        ? (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec : 0;
}

static ssize_t apfsrw_pread(struct apfsrw *fs, void *buf, size_t n, off_t off)
{
    return pread(fs->fd, buf, n, off + fs->base_off);
}

static ssize_t apfsrw_pwrite(struct apfsrw *fs, const void *buf, size_t n,
    off_t off)
{
    return pwrite(fs->fd, buf, n, off + fs->base_off);
}
#else
ssize_t apfsrw_pread(struct apfsrw *fs, void *buf, size_t n, off_t off);
ssize_t apfsrw_pwrite(struct apfsrw *fs, const void *buf, size_t n, off_t off);
#endif

static int ac_flush(struct apfsrw *fs);
static void rec_pool_drain(struct apfsrw *fs);

static uint32_t bc_slot(apfs_paddr_t paddr)
{
    uint64_t u = (uint64_t)paddr;

    return (uint32_t)((u ^ (u >> 10) ^ (u >> 20)) % APFSRW_BCACHE_ENTS);
}

static int bc_ready(struct apfsrw *fs)
{
    if (fs->bc_data == NULL) {
        fs->bc_data = malloc((size_t)APFSRW_BCACHE_ENTS * fs->block_size);
        fs->bc_ents = calloc(APFSRW_BCACHE_ENTS, sizeof(*fs->bc_ents));
        if (fs->bc_data == NULL || fs->bc_ents == NULL) {
            free(fs->bc_data);
            free(fs->bc_ents);
            fs->bc_data = NULL;
            fs->bc_ents = NULL;
        }
    }
    return fs->bc_data != NULL;
}

static int bc_get(struct apfsrw *fs, apfs_paddr_t paddr, void *out, int *verified)
{
    struct bc_ent *e;

    if (!bc_ready(fs))
        return 0;
    e = &fs->bc_ents[bc_slot(paddr)];
    if (!e->valid || e->paddr != paddr)
        return 0;
    memcpy(out, fs->bc_data + (size_t)bc_slot(paddr) * fs->block_size, fs->block_size);
    if (verified != NULL)
        *verified = e->verified;
    return 1;
}

static void bc_put(struct apfsrw *fs, apfs_paddr_t paddr, const void *buf, int verified)
{
    struct bc_ent *e;

    if (!bc_ready(fs))
        return;
    e = &fs->bc_ents[bc_slot(paddr)];
    memcpy(fs->bc_data + (size_t)bc_slot(paddr) * fs->block_size, buf, fs->block_size);
    e->paddr = paddr;
    e->valid = 1;
    e->verified = (uint8_t)verified;
}

static void bc_mark_verified(struct apfsrw *fs, apfs_paddr_t paddr)
{
    struct bc_ent *e;

    if (fs->bc_ents == NULL)
        return;
    e = &fs->bc_ents[bc_slot(paddr)];
    if (e->valid && e->paddr == paddr)
        e->verified = 1;
}

static void bc_drop(struct apfsrw *fs, apfs_paddr_t paddr, uint64_t n)
{
    uint64_t i;

    if (fs->bc_ents == NULL)
        return;
    for (i = 0; i < n; i++) {
        struct bc_ent *e = &fs->bc_ents[bc_slot(paddr + (apfs_paddr_t)i)];

        if (e->valid && e->paddr == paddr + (apfs_paddr_t)i)
            e->valid = 0;
    }
}

void apfsrw_cache_drop(struct apfsrw *fs)
{
    if (fs == NULL)
        return;
    (void)ac_flush(fs);
    fs->ac_sm_paddr = fs->ac_cib_paddr = fs->ac_bm_paddr = 0;
    if (fs->bc_ents != NULL)
        memset(fs->bc_ents, 0, APFSRW_BCACHE_ENTS * sizeof(*fs->bc_ents));
}

// A read of a block the allocation cache holds dirty must see the cached state
static int ac_holds_dirty(const struct apfsrw *fs, apfs_paddr_t paddr)
{
    return (fs->ac_bm_dirty && paddr == fs->ac_bm_paddr) ||
        (fs->ac_cib_dirty && paddr == fs->ac_cib_paddr) ||
        (fs->ac_sm_dirty && paddr == fs->ac_sm_paddr);
}

// Someone else wrote a block the allocation cache holds: forget that copy
static void ac_forget_written(struct apfsrw *fs, apfs_paddr_t paddr, const void *buf)
{
    if (buf != fs->ac_bm && paddr == fs->ac_bm_paddr)
        fs->ac_bm_paddr = 0;
    if (buf != fs->ac_cib && paddr == fs->ac_cib_paddr)
        fs->ac_cib_paddr = 0;
    if (buf != fs->ac_sm && paddr == fs->ac_sm_paddr)
        fs->ac_sm_paddr = 0;
}

static int read_block_v(struct apfsrw *fs, apfs_paddr_t paddr, void *out,
    int *verified)
{
    ssize_t n;

    if (fs == NULL || out == NULL || paddr < 0 ||
        (uint64_t)paddr >= fs->image_blocks)
        return APFSRW_EINVAL;
    if (verified != NULL)
        *verified = 0;
    if (ac_holds_dirty(fs, paddr) && ac_flush(fs) != APFSRW_OK)
        return APFSRW_EIO;
    if (bc_get(fs, paddr, out, verified))
        return APFSRW_OK;
    n = apfsrw_pread(fs, out, fs->block_size,
        (off_t)((uint64_t)paddr * fs->block_size));
    if (n < 0)
        return APFSRW_EIO;
    if ((size_t)n != fs->block_size)
        return APFSRW_EIO;
    bc_put(fs, paddr, out, 0);
    return APFSRW_OK;
}

// Blocks with no obj_phys_t header (allocation bitmaps, file data) carry no checksum,
// so they are read and written without verification or sealing
static int read_raw(struct apfsrw *fs, apfs_paddr_t paddr, void *out)
{
    ssize_t n;

    if (paddr < 0 || (uint64_t)paddr >= fs->block_count)
        return APFSRW_EINVAL;
    if (ac_holds_dirty(fs, paddr) && ac_flush(fs) != APFSRW_OK)
        return APFSRW_EIO;
    if (bc_get(fs, paddr, out, NULL))
        return APFSRW_OK;
    n = apfsrw_pread(fs, out, fs->block_size,
        (off_t)((uint64_t)paddr * fs->block_size));
    if (n < 0 || (size_t)n != fs->block_size)
        return APFSRW_EIO;
    bc_put(fs, paddr, out, 0);
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
    int verified = 0;
    int err;

    err = read_block_v(fs, paddr, out, &verified);
    if (err != APFSRW_OK)
        return err;
    if (verified)
        return APFSRW_OK;
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
    bc_mark_verified(fs, paddr);
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

        // A nonleaf node needs only bt_key_size. Its values are oid_t. bt_val_size may legitimately
        // be unavailable here when the caller could not supply the root's btree_info_t
        if (fixed_key == 0)
            return APFSRW_EINVAL;
        if (fixed_val == 0 && rd16(&node->btn_level) == 0)
            return APFSRW_EINVAL;
        toc = (const struct apfs_kvoff *)(base + data_off + table_off +
            index * sizeof(*toc));
        ko = rd16(&toc->k);
        kl = (uint16_t)fixed_key;
        vo = rd16(&toc->v);
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
    // A BTNODE_NOHEADER node has a zero-filled obj_phys_t,
    // so o_type reads as 0 and cannot be validated (spec p.132 "B-Tree Node Flags")
    if ((rd16(&node->btn_flags) & APFS_BTNODE_NOHEADER) == 0 &&
        object_type(node->btn_o.o_type) != APFS_OBJECT_TYPE_BTREE &&
        object_type(node->btn_o.o_type) != APFS_OBJECT_TYPE_BTREE_NODE) {
        err = APFSRW_EINVAL;
        goto out;
    }

    // btree_info_t lives only in the root node (spec p.126) and must be copied.
    // The caller's node is freed on the way out, so a borrowed pointer would dangle
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
        // Hashed trees store oid + hash, so the value is larger than an oid_t.
        // Only the leading oid is needed
        if (val_len < sizeof(apfs_oid_t))
            continue;

        // Prune by object id. Child i holds the keys in [key(i), key(i+1)), so stop once a
        // separator passes oid_max and skip a child whose next separator is below oid_min
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

            if (info != NULL &&
                (rd32(&info->bt_flags) & APFS_BTREE_HASHED) != 0)
                child += fs->root_tree_oid;

            // Use xid + 1.
            // Nodes rewritten earlier in the transaction in progress carry the next transaction id
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
    {
        const struct apfs_btree_info *root_info = btree_info_for_node(fs, node);

        if (root_info == NULL) {
            err = APFSRW_EINVAL;
            goto out;
        }
        memcpy(&info_copy, root_info, sizeof(info_copy));
        info = &info_copy;
    }

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
            // Take the last child whose key does not exceed (oid, xid). The first child also
            // covers everything below its own key, so seed with entry 0 and only advance past it
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
        // Non-root nodes carry no btree_info_t, so the root's stays in use (spec p.126 btree_info_t)
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
    // apfsrw_open_kernel() already knows the device size
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

    {
        uint64_t desc_base = rd64(&fs->nx.nx_xp_desc_base);
        uint32_t desc_blocks = rd32(&fs->nx.nx_xp_desc_blocks) & 0x7fffffffU;
        apfs_xid_t best = rd64(&fs->nx.nx_o.o_xid);
        uint32_t j;

        if (fs->want_xid != 0 && best > fs->want_xid)
            best = 0;
        // Commits mirror the newest superblock into block zero, so the slot it names is current.
        // Any mismatch falls back to the full scan
        if (fs->trust_block0 && fs->want_xid == 0 && best >= fs->xid &&
            desc_blocks != 0 && desc_blocks < 65536U &&
            rd32(&fs->nx.nx_xp_desc_len) != 0) {
            uint32_t slot = (rd32(&fs->nx.nx_xp_desc_index) +
                rd32(&fs->nx.nx_xp_desc_len) - 1) % desc_blocks;

            if (read_object(fs, (apfs_paddr_t)(desc_base + slot), block) ==
                APFSRW_OK &&
                rd32(&((struct apfs_nx_superblock *)block)->nx_magic) ==
                APFS_NX_MAGIC &&
                rd64(&((struct apfs_nx_superblock *)block)->nx_o.o_xid) == best) {
                memcpy(&fs->nx, block, sizeof(fs->nx));
                fs->nx_paddr = (apfs_paddr_t)(desc_base + slot);
                desc_blocks = 0;
            }
        }
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

    if (fs->vol_slot == APFSRW_SLOT_CONTAINER) {
        fs->fs_oid = 0;
        err = APFSRW_OK;
        goto out;
    }

    // The volume is the one in slot vol_slot of nx_fs_oid[].
    // Apple numbers the same slots as diskNs(slot+1)
    max_fs = rd32(&fs->nx.nx_max_file_systems);
    if (max_fs > APFS_NX_MAX_FILE_SYSTEMS)
        max_fs = APFS_NX_MAX_FILE_SYSTEMS;
    i = fs->vol_slot;
    fs->fs_oid = i < max_fs ? rd64(&fs->nx.nx_fs_oid[i]) : 0;
    if (fs->fs_oid == 0) {
        err = APFSRW_ENOENT;
        goto out;
    }
    err = omap_lookup_tree(fs, fs->container_omap_tree_paddr, fs->fs_oid,
        fs->xid, &ov);
    if (err != APFSRW_OK) {
        fs->fs_oid = 0;
        goto out;
    }
    fs->fs_paddr = (apfs_paddr_t)rd64(&ov.ov_paddr);

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

    // apfs_fext_tree_oid and _type (superblock bytes 1032 and 1040), set only on a sealed volume
    fs->fext_paddr = 0;
    if (rd64(&fs->apfs.apfs_incompatible_features) & APFS_INCOMPAT_SEALED_VOLUME) {
        uint64_t fext = rd64(block + 1032);

        if (fext != 0 && (rd32(block + 1040) & APFS_OBJ_PHYSICAL))
            fs->fext_paddr = (apfs_paddr_t)fext;
        else if (fext != 0 && omap_lookup_tree(fs, fs->volume_omap_tree_paddr, fext,
            fs->xid, &ov) == APFSRW_OK)
            fs->fext_paddr = (apfs_paddr_t)rd64(&ov.ov_paddr);
    }
out:
    free(block);
    return err;
}

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
    uint16_t mode;
    uint32_t uid, gid, bsd_flags, nlink;
    uint64_t atime, mtime, ctime, crtime;
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

            // uncompressed_size is only meaningful for a compressed file. The real length of the data
            // stream lives in the DSTREAM extended field, so prefer that and fall back otherwise
            if (inode_dstream_size(valp, val_len, &dsize) == APFSRW_OK)
                c->size = dsize;
            else
                c->size = rd64(&inode->uncompressed_size);
            c->private_id = rd64(&inode->private_id);
            c->mode = rd16(&inode->mode);
            c->uid = rd32(&inode->owner);
            c->gid = rd32(&inode->group);
            c->bsd_flags = rd32(&inode->bsd_flags);
            c->nlink = (uint32_t)rd32(&inode->u);
            c->atime = rd64(&inode->access_time);
            c->mtime = rd64(&inode->mod_time);
            c->ctime = rd64(&inode->change_time);
            c->crtime = rd64(&inode->create_time);
            c->found = 1;
            return 1;                    // Stop the walk
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
    // names match after folding: 0 exact, 1 NFD, 2 NFD and case
    int match;
};

// how a volume matches names: case-insensitive folds case too, both kinds are NFD-insensitive
static int name_match_mode(const struct apfsrw *fs)
{
    uint64_t f = rd64(&fs->apfs.apfs_incompatible_features);

    if (f & APFS_INCOMPAT_CASE_INSENSITIVE)
        return 2;

    return (f & APFS_INCOMPAT_NORMALIZATION_INSENSITIVE) ? 1 : 0;
}

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
        if ((name_len - 1 == c->want_len && memcmp(entry_name, c->name, c->want_len) == 0) ||
            (c->match && apfsrw_name_equal(entry_name, name_len - 1, c->name, c->want_len, c->match == 2))) {
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
    c.match = name_match_mode(fs);
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

// Resolve a '/' separated path to a file id from the root directory, always object id 2 (spec p.71).
// Each component is one DIR_REC lookup, symlinks are left alone
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
int apfsrw_open_kernel(void *io_ctx, uint64_t image_blocks, int writable,
    uint64_t xid, uint32_t vol_slot, struct apfsrw **out)
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
    fs->vol_slot = vol_slot;
    fs->role_pending = -1;
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
    return apfsrw_open_volume(path, writable, xid, 0, out);
}

int apfsrw_open_volume(const char *path, int writable, uint64_t xid,
    uint32_t vol_slot, struct apfsrw **out)
{
    struct apfsrw *fs;
    int flags = writable ? O_RDWR : O_RDONLY;
    int err;

    if (path == NULL || out == NULL)
        return APFSRW_EINVAL;
    fs = calloc(1, sizeof(*fs));
    if (fs == NULL)
        return APFSRW_ENOMEM;
    fs->vol_slot = vol_slot;
    // "file@@byteoffset" addresses a container inside a larger image,
    // and a trailing "@vN" picks volume slot N
    {
        const char *at = strstr(path, "@@");
        const char *vs = strstr(path, "@v");
        const char *end = at != NULL ? at : vs;
        char *p2 = NULL;

        fs->role_pending = -1;
        if (vs != NULL)
            fs->vol_slot = (uint32_t)strtoul(vs + 2, NULL, 10);
        if (at != NULL)
            fs->base_off = (off_t)strtoull(at + 2, NULL, 0);
        if (end != NULL) {
            at = end;
            p2 = malloc((size_t)(at - path) + 1);
            if (p2 == NULL) {
                free(fs);
                return APFSRW_ENOMEM;
            }
            memcpy(p2, path, (size_t)(at - path));
            p2[at - path] = '\0';
            path = p2;
        }
        fs->fd = open(path, flags);
        free(p2);
    }
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

int apfsrw_refresh(struct apfsrw *fs)
{
    int err;

    if (fs == NULL)
        return APFSRW_EINVAL;
    if (fs->batch || fs->deferred_count != 0 || fs->alloced_count != 0)
        return APFSRW_EINVAL;
    (void)ac_flush(fs);
    fs->ac_sm_paddr = fs->ac_cib_paddr = fs->ac_bm_paddr = 0;
    fs->fs_oid = 0;
    fs->extref_paddr = 0;
    fs->next_oid = 0;
    fs->alloc_delta = 0;
    fs->other_delta = 0;
    fs->newvol_pending = 0;
    fs->trust_block0 = fs->xid != 0;
    err = load_volume(fs);
    fs->trust_block0 = 0;
    return err;
}

uint32_t apfsrw_volume_slot(struct apfsrw *fs)
{
    return fs == NULL ? 0 : fs->vol_slot;
}

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
    (void)ac_flush(fs);
    rec_pool_drain(fs);
    free(fs->ac_sm);
    free(fs->ac_cib);
    free(fs->ac_bm);
    free(fs->bc_data);
    free(fs->bc_ents);
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
    info->volume_omap_tree_paddr = (uint64_t)fs->volume_omap_tree_paddr;
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

// One FILE_EXTENT record per extent, keyed by stream id and logical address, each copied to its
// own offset (spec p.102). The low 56 bits of len_and_flags are bytes, phys_block_num 0 is a hole
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
            continue;                    // sparse hole: already zeroed
        n = apfsrw_pread(fs, c->buf + logical, (size_t)len,
            (off_t)(phys * fs->block_size));
        if (n < 0 || (uint64_t)n != len) {
            c->err = APFSRW_EIO;
            return 1;
        }
    }
    return APFSRW_OK;
}

// fext_tree_key_t { private_id, logical_addr } -> fext_tree_val_t { len_and_flags, phys_block_num }
static int read_fext_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct read_extents_ctx *c = (struct read_extents_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp, *valp;
        uint16_t key_len, val_len;
        uint64_t logical, len, phys;
        int err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp, &val_len);

        if (err != APFSRW_OK)
            return err;
        if (key_len < 16 || val_len < 16 || rd64(keyp) != c->stream_id)
            continue;
        c->nextents++;
        logical = rd64((const uint8_t *)keyp + 8);
        len = rd64(valp) & APFS_FILE_EXTENT_LEN_MASK;
        phys = rd64((const uint8_t *)valp + 8);
        if (logical >= c->size || phys == 0)
            continue;
        if (len > c->size - logical)
            len = c->size - logical;
        if (apfsrw_pread(fs, c->buf + logical, (size_t)len,
            (off_t)(phys * fs->block_size)) != (ssize_t)len) {
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
    if (c.err == APFSRW_OK && c.nextents == 0 && fs->fext_paddr != 0) {
        err = btree_walk_leaves_oid(fs, fs->fext_paddr, stream_id, stream_id,
            read_fext_cb, &c);
        if (err != APFSRW_OK && err != 1)
            return err;
    }
    if (c.err == APFSRW_OK && c.nextents == 0)
        // A non-empty stream with no FILE_EXTENT records is stored inline in a decmpfs xattr.
        // Report that, a zero-filled buffer would be worse
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

// Extended attributes are j_xattr_key_t { j_key_t hdr. uint16 name_len. char name[] } / j_xattr_val_t
// { uint16 flags. uint16 xdata_len. uint8 xdata[] } (spec p.82). name_len INCLUDES the trailing NUL
struct xattr_ctx {
    uint64_t file_id;
    const char *want;               // NULL = enumerate
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

// Histogram decmpfs compression types across the whole volume,
// so the codecs worth implementing are chosen from measurement
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
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, file_id, file_id,
        xattr_cb, &c);
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
    {
        struct inode_ctx ic;

        memset(&ic, 0, sizeof(ic));
        ic.file_id = file_id;
        err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, file_id,
            file_id, inode_leaf_cb, &ic);
        if (err != APFSRW_OK && err != 1)
            return err;
        out->mode = ic.mode;
        out->uid = ic.uid;
        out->gid = ic.gid;
        out->bsd_flags = ic.bsd_flags;
        out->nlink = (ic.mode & 0170000) == 0040000 ? ic.nlink + 2 : ic.nlink;
        out->atime_ns = ic.atime;
        out->mtime_ns = ic.mtime;
        out->ctime_ns = ic.ctime;
        out->crtime_ns = ic.crtime;
    }
    memset(&c, 0, sizeof(c));
    c.stream_id = stream_id;
    // extent keys carry the stream id, so only that range is walked
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, stream_id, stream_id,
        stat_cb, &c);
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
            continue;                    // The superblock itself
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

// Walk the space manager's allocation bitmaps. The CIB addresses are a paddr_t array at sm_addr_offset,
// each chunk_info entry owns one bitmap block (spec p.159-161)
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
        err = APFSRW_ENOTSUP;            // CAB indirection not handled
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

// A large decmpfs file keeps its payload in the com.apple.ResourceFork xattr as a j_xattr_dstream_t
// (spec p.106). Read its FILE_EXTENT records keyed by xattr_obj_id, like a file's
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
    // The first offset must point past the table, or this is not the layout
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
            // Raw chunk: one marker byte then the bytes verbatim. Detect it by length,
            // LZVN forks use 0x06 and LZBITMAP ones 0xff, so one marker value would reject the other
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
        // One marker byte then the content verbatim.
        // Measured on 25G83: every type-9 file has inline_len == size + 1 and first byte 0xcc
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

// Symlink target, from the inline com.apple.fs.symlink xattr (spec p.83)
int apfsrw_readlink(struct apfsrw *fs, const char *path, char *buf,
    size_t bufsize, size_t *len_out)
{
    struct xattr_ctx c;
    uint64_t file_id;
    size_t len;
    int err;

    if (fs == NULL || path == NULL || buf == NULL || bufsize == 0)
        return APFSRW_EINVAL;
    err = resolve_path(fs, path, &file_id, NULL);
    if (err != APFSRW_OK)
        return err;
    err = lookup_xattr(fs, file_id, "com.apple.fs.symlink", &c);
    if (err != APFSRW_OK)
        return err;
    len = c.len;
    if (len > 0 && c.data[len - 1] == '\0')
        len--;
    if (len >= bufsize)
        return APFSRW_EOVERFLOW;
    memcpy(buf, c.data, len);
    buf[len] = '\0';
    if (len_out != NULL)
        *len_out = len;
    return APFSRW_OK;
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
        // An empty file has no file-extent record to find
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

// Write

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
    bc_put(fs, paddr, buf, 0);
    ac_forget_written(fs, paddr, buf);
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
    bc_put(fs, paddr, buf, 1);
    ac_forget_written(fs, paddr, buf);
    return APFSRW_OK;
}

// Recompute o_cksum over the object, excluding the checksum field itself (spec p.10 obj_phys_t o_cksum.
// Algorithm from the PureDarwin kext)
static void seal_object(struct apfsrw *fs, void *block)
{
    wr64(block, fletcher64((const uint8_t *)block, fs->block_size));
}

// CRC-32C (Castagnoli), reflected form. Used only for the dirent name hash
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

// j_drec_hashed_key_t.name_len_and_hash (spec p.78-79). Low 10 bits are the name length with its NUL,
// bits 31:10 a 22-bit hash: NFD, UTF-32, CRC-32C, complement
int apfsrw_name_hash(const char *name, size_t len, int fold, uint32_t *out)
{
    enum { MAXCP = 1024 };
    uint32_t *cp;
    long n, i;

    if (len == 0 || len > 255)
        return APFSRW_EINVAL;

    cp = (uint32_t *)malloc(MAXCP * sizeof(uint32_t));
    if (cp == NULL)
        return APFSRW_ENOMEM;

    n = apfsrw_name_fold(name, len, fold, cp, MAXCP);
    if (n <= 0) {
        free(cp);
        return APFSRW_EINVAL;
    }

    // little-endian UTF-32, in place
    for (i = 0; i < n; i++) {
        wr32((uint8_t *)&cp[i], cp[i]);
    }

    *out = (~crc32c((const uint8_t *)cp, (size_t)n * 4)) & 0x003fffffU;
    free(cp);
    return APFSRW_OK;
}

static int drec_name_hash(struct apfsrw *fs, const char *name, size_t len,
    uint32_t *out)
{
    return apfsrw_name_hash(name, len, name_match_mode(fs) == 2, out);
}

static int spaceman_set_range(struct apfsrw *fs, uint64_t first, uint32_t n,
    int set);

static int free_blocks(struct apfsrw *fs, uint64_t first, uint32_t n)
{
    return spaceman_set_range(fs, first, n, 0);
}

// Write back whatever the allocation cache changed. Bitmap first, then the chunk
// info that counts it, then the space manager
enum { AC_BM = 1, AC_CIB = 2, AC_SM = 4 };

// Write back the named slots. Within a transaction any order is fine: every slot is
// flushed before the checkpoint that publishes them
static int ac_flush_slots(struct apfsrw *fs, unsigned which)
{
    int err;

    if ((which & AC_BM) && fs->ac_bm_dirty) {
        fs->ac_bm_dirty = 0;
        err = write_raw(fs, fs->ac_bm_paddr, fs->ac_bm);
        if (err != APFSRW_OK)
            return err;
    }
    if ((which & AC_CIB) && fs->ac_cib_dirty) {
        fs->ac_cib_dirty = 0;
        seal_object(fs, fs->ac_cib);
        err = write_block(fs, fs->ac_cib_paddr, fs->ac_cib);
        if (err != APFSRW_OK)
            return err;
    }
    if ((which & AC_SM) && fs->ac_sm_dirty) {
        fs->ac_sm_dirty = 0;
        seal_object(fs, fs->ac_sm);
        err = write_block(fs, fs->ac_sm_paddr, fs->ac_sm);
        if (err != APFSRW_OK)
            return err;
    }
    return APFSRW_OK;
}

static int ac_flush(struct apfsrw *fs)
{
    return ac_flush_slots(fs, AC_BM | AC_CIB | AC_SM);
}

static int ac_ready(struct apfsrw *fs)
{
    if (fs->ac_sm == NULL) {
        fs->ac_sm = calloc(1, fs->block_size);
        fs->ac_cib = calloc(1, fs->block_size);
        fs->ac_bm = calloc(1, fs->block_size);
        if (fs->ac_sm == NULL || fs->ac_cib == NULL || fs->ac_bm == NULL) {
            free(fs->ac_sm);
            free(fs->ac_cib);
            free(fs->ac_bm);
            fs->ac_sm = fs->ac_cib = fs->ac_bm = NULL;
        }
    }
    return fs->ac_sm != NULL;
}

// Point one cache slot at paddr, writing back what it held if that changed
static int ac_load(struct apfsrw *fs, uint8_t *buf, apfs_paddr_t *cur,
    uint8_t *dirty, apfs_paddr_t paddr, int raw)
{
    int err;

    if (*cur == paddr)
        return APFSRW_OK;
    if (*dirty) {
        err = ac_flush_slots(fs, buf == fs->ac_bm ? AC_BM : buf == fs->ac_cib ? AC_CIB : AC_SM);
        if (err != APFSRW_OK)
            return err;
    }
    *cur = 0;
    err = raw ? read_raw(fs, paddr, buf) : read_object(fs, paddr, buf);
    if (err != APFSRW_OK)
        return err;
    *cur = paddr;
    return APFSRW_OK;
}

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

// Give back everything this transaction allocated.
// The checkpoint that would have referenced it never landed
static void op_mark(struct apfsrw *fs)
{
    fs->op_alloced = fs->alloced_count;
    fs->op_deferred = fs->deferred_count;
    fs->op_root = fs->root_tree_paddr;
    fs->op_vomap_tree = fs->volume_omap_tree_paddr;
    fs->op_extref = fs->extref_paddr;
    fs->op_fext = fs->fext_paddr;
    fs->op_alloc_delta = fs->alloc_delta;
    fs->op_other_delta = fs->other_delta;
}

// undo only the failed op of a batch: earlier ops' blocks were never rewritten in place
static void op_rollback(struct apfsrw *fs)
{
    uint32_t i;

    for (i = fs->op_alloced; i < fs->alloced_count; i++)
        free_blocks(fs, fs->alloced[i], 1);
    fs->alloced_count = fs->op_alloced;
    fs->deferred_count = fs->op_deferred;
    fs->root_tree_paddr = fs->op_root;
    fs->volume_omap_tree_paddr = fs->op_vomap_tree;
    fs->extref_paddr = fs->op_extref;
    fs->fext_paddr = fs->op_fext;
    fs->alloc_delta = fs->op_alloc_delta;
    fs->other_delta = fs->op_other_delta;
}

static void txn_rollback(struct apfsrw *fs)
{
    uint32_t i;

    if (fs->batch) {
        op_rollback(fs);
        return;
    }
    (void)ac_flush(fs);
    for (i = 0; i < fs->alloced_count; i++)
        free_blocks(fs, fs->alloced[i], 1);
    fs->alloced_count = 0;
    fs->deferred_count = 0;
    fs->alloc_delta = 0;
    fs->other_delta = 0;
    (void)apfsrw_sync(fs);
    // The failed op may have re-pointed cached tree/omap paddrs at blocks just freed.
    // re-derive everything from the last committed checkpoint
    fs->fs_oid = 0;
    (void)load_volume(fs);
}

// Free a set of blocks with one bitmap, chunk-info and spaceman write per chunk.
// A commit's frees cluster in one chunk, so the whole set costs three writes
static int spaceman_free_many(struct apfsrw *fs, const uint64_t *blocks,
    uint32_t count)
{
    struct apfs_spaceman_phys *sm = NULL;
    struct apfs_chunk_info_block *cib = NULL;
    uint8_t *bitmap = NULL;
    apfs_paddr_t sm_paddr = 0;
    uint32_t cib_count, addr_offset, i;
    uint32_t done = 0, sm_dirty = 0;
    int err;

    if (count == 0)
        return APFSRW_OK;
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

    for (i = 0; i < cib_count && done < count; i++) {
        apfs_paddr_t cib_paddr;
        uint32_t chunks, c, cib_dirty = 0;

        memcpy(&cib_paddr, (const uint8_t *)sm + addr_offset + i * 8U, 8);
        cib_paddr = (apfs_paddr_t)rd64(&cib_paddr);
        if (cib_paddr <= 0)
            continue;
        err = read_object(fs, cib_paddr, cib);
        if (err != APFSRW_OK)
            goto out;
        chunks = rd32(&cib->cib_chunk_info_count);
        for (c = 0; c < chunks && done < count; c++) {
            struct apfs_chunk_info *ci = &cib->cib_chunk_info[c];
            uint64_t base = rd64(&ci->ci_addr);
            uint32_t nblk = rd32(&ci->ci_block_count);
            apfs_paddr_t bm = (apfs_paddr_t)rd64(&ci->ci_bitmap_addr);
            uint32_t changed = 0, j;

            if (bm <= 0)
                continue;
            // Does anything in the set fall in this chunk?
            for (j = 0; j < count; j++)
                if (blocks[j] >= base && blocks[j] < base + nblk)
                    break;
            if (j == count)
                continue;

            err = read_raw(fs, bm, bitmap);
            if (err != APFSRW_OK)
                goto out;
            for (j = 0; j < count; j++) {
                uint32_t bit;
                uint8_t mask;

                if (blocks[j] < base || blocks[j] >= base + nblk)
                    continue;
                bit = (uint32_t)(blocks[j] - base);
                mask = (uint8_t)(1U << (bit & 7));
                done++;
                if ((bitmap[bit >> 3] & mask) == 0)
                    continue;           // already free
                bitmap[bit >> 3] &= (uint8_t)~mask;
                changed++;
            }
            if (changed == 0)
                continue;
            err = write_raw(fs, bm, bitmap);
            if (err != APFSRW_OK)
                goto out;
            wr32(&ci->ci_free_count, rd32(&ci->ci_free_count) + changed);
            wr64(&sm->sm_dev[0].sm_free_count,
                rd64(&sm->sm_dev[0].sm_free_count) + changed);
            cib_dirty = 1;
            sm_dirty = 1;
        }
        if (cib_dirty) {
            seal_object(fs, cib);
            err = write_block(fs, cib_paddr, cib);
            if (err != APFSRW_OK)
                goto out;
        }
    }
    if (sm_dirty) {
        seal_object(fs, sm);
        err = write_block(fs, sm_paddr, sm);
    } else {
        err = APFSRW_OK;
    }
out:
    free(bitmap);
    free(cib);
    free(sm);
    return err;
}

#ifndef APFSRW_KERNEL
int apfsrw_punch(int fd, off_t off, off_t len);
static int block_frozen(const struct apfsrw *fs, uint64_t b);

static int u64_cmp(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;

    return x < y ? -1 : x > y;
}

// freed blocks hold nothing anyone reads again: punch them out of a sparse image file.
// blocks a savepoint froze keep their contents for a rollback
static void punch_freed(struct apfsrw *fs, uint64_t *b, uint32_t n)
{
    uint32_t i = 0;

    qsort(b, n, sizeof(*b), u64_cmp);
    while (i < n) {
        uint32_t j = i;

        if (block_frozen(fs, b[i])) {
            i++;
            continue;
        }
        while (j + 1 < n && b[j + 1] == b[j] + 1 && !block_frozen(fs, b[j + 1]))
            j++;
        bc_drop(fs, (apfs_paddr_t)b[i], b[j] - b[i] + 1);
        if (apfsrw_punch(fs->fd, fs->base_off + (off_t)(b[i] * fs->block_size),
            (off_t)((b[j] - b[i] + 1) * fs->block_size)) != 0)
            return;                     // not a regular file, or no hole support
        i = j + 1;
    }
}
#endif

static void flush_deferred(struct apfsrw *fs)
{
#ifndef APFSRW_KERNEL
    if (fs->deferred_count != 0)
        punch_freed(fs, fs->deferred, fs->deferred_count);
#endif
    if (fs->deferred_count != 0 &&
        spaceman_free_many(fs, fs->deferred, fs->deferred_count) != APFSRW_OK) {
        uint32_t i;

        // Fall back to the one-at-a-time path so the blocks are never leaked
        for (i = 0; i < fs->deferred_count; i++)
            free_blocks(fs, fs->deferred[i], 1);
    }
    fs->deferred_count = 0;
}

// A savepoint freezes every block that was in use when it was taken,
// so the saved checkpoint's metadata can never be overwritten
static int block_frozen(const struct apfsrw *fs, uint64_t b)
{
    return fs->frozen != NULL && b < fs->block_count &&
        (fs->frozen[b >> 3] & (uint8_t)(1U << (b & 7))) != 0;
}

// Allocate n contiguous blocks. With a non-zero hint,
// try to place them at exactly that address first (keeps sequential writes in one extent)
static int alloc_blocks_from(struct apfsrw *fs, uint32_t n, uint64_t hint,
    uint64_t from, int reverse, uint64_t *out)
{
    struct apfs_spaceman_phys *sm = NULL;
    struct apfs_chunk_info_block *cib = NULL;
    uint8_t *bitmap = NULL;
    apfs_paddr_t sm_paddr = 0;
    uint32_t cib_count, addr_offset, i;
    int pass;
    int err;

    if (n == 0)
        return APFSRW_EINVAL;
    err = resolve_ephemeral(fs, rd64(&fs->nx.nx_spaceman_oid), &sm_paddr);
    if (err != APFSRW_OK)
        return err;
    if (!ac_ready(fs))
        return APFSRW_ENOMEM;
    sm = (struct apfs_spaceman_phys *)fs->ac_sm;
    cib = (struct apfs_chunk_info_block *)fs->ac_cib;
    bitmap = fs->ac_bm;
    err = ac_load(fs, fs->ac_sm, &fs->ac_sm_paddr, &fs->ac_sm_dirty, sm_paddr, 0);
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

    for (pass = (hint != 0) ? 0 : (reverse ? 2 : 1); pass < 3; pass++)
    for (i = 0; i < cib_count; i++) {
        apfs_paddr_t cib_paddr;
        uint32_t chunks, c;

        // A missed hint on the metadata side goes back to the top-down scan, not upward from 0
        if (pass == 1 && reverse)
            break;
        // The hint names one chunk, hence one chunk-info block: touching the others
        // only evicts the cached one
        uint32_t ci_idx = (reverse && pass == 2) ? cib_count - 1 - i : i;

        if (pass == 0) {
            uint32_t bpc = rd32(&sm->sm_blocks_per_chunk), cpc = rd32(&sm->sm_chunks_per_cib);

            if (bpc != 0 && cpc != 0 && ci_idx != (hint / bpc) / cpc)
                continue;
        }

        memcpy(&cib_paddr, (const uint8_t *)sm + addr_offset + ci_idx * 8U, 8);
        cib_paddr = (apfs_paddr_t)rd64(&cib_paddr);
        if (cib_paddr <= 0)
            continue;
        err = ac_load(fs, fs->ac_cib, &fs->ac_cib_paddr, &fs->ac_cib_dirty,
            cib_paddr, 0);
        if (err != APFSRW_OK)
            goto out;
        chunks = rd32(&cib->cib_chunk_info_count);
        if (chunks > (fs->block_size - sizeof(*cib)) /
            sizeof(struct apfs_chunk_info)) {
            err = APFSRW_EINVAL;
            goto out;
        }
        for (c = 0; c < chunks; c++) {
            struct apfs_chunk_info *ci = &cib->cib_chunk_info[
                (reverse && pass == 2) ? chunks - 1 - c : c];
            apfs_paddr_t bm_paddr = (apfs_paddr_t)rd64(&ci->ci_bitmap_addr);
            uint64_t chunk_addr = rd64(&ci->ci_addr);
            uint32_t nblk = rd32(&ci->ci_block_count);
            uint32_t free_count = rd32(&ci->ci_free_count);
            uint32_t start, run, k;

            if (free_count < n || nblk == 0)
                continue;
            // Pass 0 only looks at the chunk holding the hint.
            // Pass 1 scans upward from from. Pass 2 takes anything
            if (pass == 0 && (hint < chunk_addr ||
                hint + n > chunk_addr + nblk))
                continue;
            if (pass == 1 && from >= chunk_addr + nblk)
                continue;
            if (bm_paddr == 0) {
                // ci_bitmap_addr == 0: the chunk is entirely free with no bitmap block yet.
                // Give it one from the internal pool
                if (fs->ac_bm_dirty && ac_flush(fs) != APFSRW_OK)
                    continue;
                if (alloc_ip_block(fs, sm, &bm_paddr) != APFSRW_OK)
                    continue;
                memset(bitmap, 0, fs->block_size);
                fs->ac_bm_paddr = bm_paddr;
                wr64(&ci->ci_bitmap_addr, (uint64_t)bm_paddr);
                fs->ac_cib_dirty = 1;
            } else if (ac_load(fs, fs->ac_bm, &fs->ac_bm_paddr,
                &fs->ac_bm_dirty, bm_paddr, 1) != APFSRW_OK) {
                continue;
            }

            run = 0;
            start = 0;
            if (pass == 0) {
                start = (uint32_t)(hint - chunk_addr);
                for (k = start; k < start + n; k++)
                    if ((bitmap[k >> 3] & (uint8_t)(1U << (k & 7))) ||
                        block_frozen(fs, chunk_addr + k))
                        break;
                if (k < start + n)
                    continue;
                goto take;
            }
            if (pass == 2 && reverse) {
                // Highest free run in the chunk:
                // tree nodes grow down from the top so they do not fragment file data
                uint32_t want = n > fs->alloc_minrun ? n : fs->alloc_minrun;

                // A minimum run keeps the cursor out of the small holes old nodes leave
                if (free_count < want)
                    continue;
                for (k = nblk; k-- > 0; ) {
                    if (chunk_addr + k >= fs->block_count ||
                        (bitmap[k >> 3] & (uint8_t)(1U << (k & 7))) ||
                        block_frozen(fs, chunk_addr + k)) {
                        run = 0;
                        continue;
                    }
                    if (++run < want)
                        continue;
                    start = k + want - n;   // the top of the run
                    goto take;
                }
                continue;
            }
            k = 0;
            if (pass == 1 && from > chunk_addr)
                k = (uint32_t)(from - chunk_addr);
            for (; k < nblk; k++) {
                if (chunk_addr + k >= fs->block_count)
                    break;
                if ((bitmap[k >> 3] & (uint8_t)(1U << (k & 7))) ||
                    block_frozen(fs, chunk_addr + k)) {
                    run = 0;
                    continue;
                }
                if (run == 0)
                    start = k;
                if (++run < n)
                    continue;
                goto take;
            }
            continue;
take:
            {
                for (k = start; k < start + n; k++)
                    bitmap[k >> 3] |= (uint8_t)(1U << (k & 7));
                wr32(&ci->ci_free_count, free_count - n);
                wr64(&sm->sm_dev[0].sm_free_count,
                    rd64(&sm->sm_dev[0].sm_free_count) - n);
                fs->ac_bm_dirty = 1;
                fs->ac_cib_dirty = 1;
                fs->ac_sm_dirty = 1;
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
    return err;
}

// Metadata: top-down, and right below the previous node when that spot is free,
// so one commit's nodes stay contiguous
static int alloc_blocks(struct apfsrw *fs, uint32_t n, uint64_t *out)
{
    uint64_t hint = fs->meta_cursor >= n + 1 ? fs->meta_cursor - n : 0;
    int err;

    fs->alloc_minrun = 64;
    err = alloc_blocks_from(fs, n, hint, 0, 1, out);
    if (err == APFSRW_ENOSPC) {
        fs->alloc_minrun = 0;
        err = alloc_blocks_from(fs, n, 0, 0, 1, out);
    }
    fs->alloc_minrun = 0;
    if (err == APFSRW_OK)
        fs->meta_cursor = *out;
    return err;
}

// Start of the first free run of at least want blocks (read-only scan). 0 if there is none
static uint64_t spaceman_find_run(struct apfsrw *fs, uint32_t want)
{
    struct apfs_spaceman_phys *sm = calloc(1, fs->block_size);
    struct apfs_chunk_info_block *cib = calloc(1, fs->block_size);
    uint8_t *bitmap = calloc(1, fs->block_size);
    apfs_paddr_t sm_paddr = 0;
    uint64_t found = 0;
    uint32_t cib_count, addr_offset, i;

    if (sm == NULL || cib == NULL || bitmap == NULL)
        goto out;
    if (resolve_ephemeral(fs, rd64(&fs->nx.nx_spaceman_oid), &sm_paddr) ||
        read_object(fs, sm_paddr, sm))
        goto out;
    cib_count = rd32(&sm->sm_dev[0].sm_cib_count);
    addr_offset = rd32(&sm->sm_dev[0].sm_addr_offset);
    for (i = 0; i < cib_count && found == 0; i++) {
        apfs_paddr_t cib_paddr;
        uint32_t chunks, c;

        memcpy(&cib_paddr, (const uint8_t *)sm + addr_offset + i * 8U, 8);
        cib_paddr = (apfs_paddr_t)rd64(&cib_paddr);
        if (cib_paddr <= 0 || read_object(fs, cib_paddr, cib))
            continue;
        chunks = rd32(&cib->cib_chunk_info_count);
        for (c = 0; c < chunks && found == 0; c++) {
            struct apfs_chunk_info *ci = &cib->cib_chunk_info[c];
            apfs_paddr_t bm_paddr = (apfs_paddr_t)rd64(&ci->ci_bitmap_addr);
            uint64_t chunk_addr = rd64(&ci->ci_addr);
            uint32_t nblk = rd32(&ci->ci_block_count), run = 0, k;

            if (rd32(&ci->ci_free_count) < want)
                continue;
            if (bm_paddr == 0) {
                found = chunk_addr;      // Wholly free chunk
                break;
            }
            if (read_raw(fs, bm_paddr, bitmap))
                continue;
            for (k = 0; k < nblk; k++) {
                if (bitmap[k >> 3] & (uint8_t)(1U << (k & 7))) {
                    run = 0;
                    continue;
                }
                if (++run == want) {
                    found = chunk_addr + k + 1 - want;
                    break;
                }
            }
        }
    }
out:
    free(bitmap);
    free(cib);
    free(sm);
    return found;
}

// File data goes right after the previous extent if possible, else forward from where data last landed,
// clear of tree nodes. The cursor starts at the first sizeable free run
static int alloc_data_blocks(struct apfsrw *fs, uint32_t n, uint64_t hint,
    uint64_t *out)
{
    int err;

    if (fs->data_cursor == 0)
        fs->data_cursor = spaceman_find_run(fs, 256);
    err = alloc_blocks_from(fs, n, hint, fs->data_cursor, 0, out);

    if (err == APFSRW_OK)
        fs->data_cursor = *out + n;
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
// Largest legal fs-tree key is a hashed drec: 12 + 255-byte name + NUL = 268
// (spec p.98 j_drec_hashed_key_t). Kept tight. These sit on the kernel stack
#define APFSRW_MAX_KEY 300
// Deferred-free blocks tolerated before a batch checkpoints mid-flight
#define APFSRW_BATCH_FLUSH 16384u
// spec p.128 BTREE_TOC_ENTRY_INCREMENT
#define APFS_BTREE_TOC_ENTRY_INCREMENT 8u

struct rw_rec {
    uint8_t *key;
    uint8_t *val;
    uint16_t klen;
    uint16_t vlen;
};

#define REC_CLASSES 10
#define REC_HDR 16

static unsigned rec_class(size_t n)
{
    unsigned c = 0;
    size_t s = 16;

    while (s < n && c < REC_CLASSES - 1) {
        s <<= 1;
        c++;
    }
    return c;
}

static void *rec_alloc(struct apfsrw *fs, size_t n)
{
    unsigned c = rec_class(n ? n : 1);
    uint8_t *b;

    if (c == REC_CLASSES - 1 && n > ((size_t)16 << c)) {
        b = malloc(n + REC_HDR);           // oversize: never pooled
        if (b == NULL)
            return NULL;
        memset(b, 0xff, REC_HDR);
        memcpy(b + 8, &n, sizeof(n));      // its length, for rec_realloc
        return b + REC_HDR;
    }
    if (fs->rec_free[c] != NULL) {
        b = (uint8_t *)fs->rec_free[c];
        fs->rec_free[c] = *(void **)(b + 8);
        fs->rec_free_n[c]--;
        return b + REC_HDR;
    }
    b = malloc(((size_t)16 << c) + REC_HDR);
    if (b == NULL)
        return NULL;
    memset(b, 0, REC_HDR);
    b[0] = (uint8_t)c;
    return b + REC_HDR;
}

static void rec_free(struct apfsrw *fs, void *p)
{
    uint8_t *b;
    unsigned c;

    if (p == NULL)
        return;
    b = (uint8_t *)p - REC_HDR;
    c = b[0];
    if (c >= REC_CLASSES || fs->rec_free_n[c] >= 2048) {
        free(b);
        return;
    }
    *(void **)(b + 8) = fs->rec_free[c];
    fs->rec_free[c] = b;
    fs->rec_free_n[c]++;
}

static void *rec_realloc(struct apfsrw *fs, void *p, size_t n)
{
    uint8_t *b;
    void *np;
    size_t have;

    if (p == NULL)
        return rec_alloc(fs, n);
    b = (uint8_t *)p - REC_HDR;
    if (b[0] < REC_CLASSES)
        have = (size_t)16 << b[0];
    else
        memcpy(&have, b + 8, sizeof(have));
    if (have >= n)
        return p;
    np = rec_alloc(fs, n);
    if (np == NULL)
        return NULL;
    memcpy(np, p, have);
    rec_free(fs, p);
    return np;
}

static void rec_pool_drain(struct apfsrw *fs)
{
    for (unsigned c = 0; c < REC_CLASSES; c++) {
        while (fs->rec_free[c] != NULL) {
            uint8_t *b = (uint8_t *)fs->rec_free[c];

            fs->rec_free[c] = *(void **)(b + 8);
            free(b);
        }
        fs->rec_free_n[c] = 0;
    }
}

static void free_records(struct apfsrw *fs, struct rw_rec *r, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        rec_free(fs, r[i].key);
        rec_free(fs, r[i].val);
    }
}

// File-system tree order: object id, then record type, then type-specific (spec p.123, p.71).
// Directory entries tiebreak on the name hash, verified against real volumes
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
        // The hash is only 22 bits, so distinct names in one directory collide routinely. Hashes alone
        // make two files compare equal and the insert reports a false EEXIST, so fall back to the name
        na = (uint32_t)(la - 12);
        nb = (uint32_t)(lb - 12);
        n = na < nb ? na : nb;
        c = memcmp(ka + 12, kb + 12, n);
        if (c != 0)
            return c < 0 ? -1 : 1;
        if (na != nb)
            return na < nb ? -1 : 1;
    } else if ((key_type(a) == APFS_TYPE_FILE_EXTENT ||
        key_type(a) == APFS_TYPE_SIBLING_LINK) && la >= 16 && lb >= 16) {
        // Second key word: logical address, or sibling id (spec p.104)
        uint64_t la2 = rd64(ka + 8);
        uint64_t lb2 = rd64(kb + 8);

        if (la2 != lb2)
            return la2 < lb2 ? -1 : 1;
    } else if (key_type(a) == APFS_TYPE_XATTR && la >= 10 && lb >= 10) {
        // j_xattr_key_t: name_len then the name (spec p.94)
        uint32_t na = rd16(ka + 8), nb = rd16(kb + 8), n = na < nb ? na : nb;
        int c;

        if (10U + na > la || 10U + nb > lb)
            return 0;
        c = memcmp(ka + 10, kb + 10, n);
        if (c != 0)
            return c < 0 ? -1 : 1;
        if (na != nb)
            return na < nb ? -1 : 1;
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
        out[i].key = rec_alloc(fs, kl ? kl : 1);
        out[i].val = rec_alloc(fs, vl ? vl : 1);
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

static int insert_record(struct apfsrw *fs, struct rw_rec *r, uint32_t *count, const void *key,
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
    r[pos].key = rec_alloc(fs, klen);
    r[pos].val = rec_alloc(fs, vlen ? vlen : 1);
    if (r[pos].key == NULL || r[pos].val == NULL)
        return APFSRW_ENOMEM;
    memcpy(r[pos].key, key, klen);
    memcpy(r[pos].val, val, vlen);
    r[pos].klen = klen;
    r[pos].vlen = vlen;
    (*count)++;
    return APFSRW_OK;
}

// remove record idx and close the gap
static void drop_record(struct apfsrw *fs, struct rw_rec *r, uint32_t *count,
    uint32_t idx)
{
    uint32_t i;

    rec_free(fs, r[idx].key);
    rec_free(fs, r[idx].val);
    for (i = idx; i + 1 < *count; i++) {
        r[i] = r[i + 1];
    }
    (*count)--;
    memset(&r[*count], 0, sizeof(r[*count]));
}




// Node building

static int build_node(struct apfsrw *fs, const struct apfs_btree_node_phys *tmpl,
    int is_root, uint16_t level, const struct apfs_btree_info *info,
    struct rw_rec *r, uint32_t count, uint8_t *node)
{
    uint32_t data_off = (uint32_t)offsetof(struct apfs_btree_node_phys,
        btn_data);
    int fixed = (rd16(&tmpl->btn_flags) & APFS_BTNODE_FIXED_KV_SIZE) != 0;
    uint16_t ent = (uint16_t)(fixed ? 4 : sizeof(struct apfs_kvloc));
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
            // Root is the only node, so the tree-wide counts are this node's.
            // Above level 0 the caller maintains them as records are added
            wr64(&bi->bt_key_count, count);
            wr64(&bi->bt_node_count, 1);
        }
    }
    return APFSRW_OK;
}

// choose where to split a record array. Fixed-size records: the largest prefix that fills half a node,
// capped so both halves make progress. Variable-size ones: the point that best balances the bytes
static uint32_t split_point(struct apfsrw *fs, struct rw_rec *r, uint32_t count,
    int fixed)
{
    uint32_t ent = fixed ? 4u : (uint32_t)sizeof(struct apfs_kvloc);
    uint32_t avail = fs->block_size -
        (uint32_t)offsetof(struct apfs_btree_node_phys, btn_data) -
        (uint32_t)sizeof(struct apfs_btree_info);
    uint32_t used = 0, i;

    if (!fixed) {
        uint64_t total = 0, left = 0, best_max = UINT64_MAX;
        uint32_t best = 1;

        // variable-size records can be nearly a node each (a 3.8K xattr):
        // split where the larger half is smallest, so a big record sits alone rather than overflow
        for (i = 0; i < count; i++)
            total += ent + r[i].klen + r[i].vlen;
        for (i = 1; i < count; i++) {
            uint64_t m;

            left += ent + r[i - 1].klen + r[i - 1].vlen;
            m = left > total - left ? left : total - left;
            if (m < best_max) {
                best_max = m;
                best = i;
            }
        }
        (void)avail;
        return best;
    }
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

// Virtual oids for new b-tree nodes come from the container's counter.
// On a fresh volume root_tree_oid is 1028 while apfs_next_obj_id is 16
static apfs_oid_t alloc_oid(struct apfsrw *fs)
{
    if (fs->next_oid == 0)
        fs->next_oid = rd64(&fs->nx.nx_next_oid);
    return fs->next_oid++;
}

static int fstree_bump_counts(struct apfsrw *fs, int64_t key_delta,
    int64_t node_delta, uint32_t klen, uint32_t vlen);

// Multi-level tree update

struct bpath {
    apfs_paddr_t paddr[APFSRW_BTREE_MAX_DEPTH];
    uint32_t index[APFSRW_BTREE_MAX_DEPTH];
    uint32_t n;                          // paddr[n-1] is the leaf
    struct apfs_btree_info info;         // The root's, copied
};

enum bkey_kind { BKEY_OMAP, BKEY_JKEY };

// Order two keys of the given flavour
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

// Descend a tree whose child links are PHYSICAL (object maps, the extent reference tree),
// recording the node and chosen child index at every level so the path can be copied back up afterwards
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
            // Entry 0 also covers everything below its own key
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

// Publish a rewritten leaf up a physical path. The leaf goes to a fresh block, then each parent
// is copied with its child repointed, up to a new root, restamping every copy (spec p.11)
// A node this transaction allocated is referenced by no checkpoint yet,
// so it can be rewritten where it is instead of copied again
static int txn_owns_block(const struct apfsrw *fs, apfs_paddr_t paddr)
{
    uint32_t i;

    if (paddr <= 0)
        return 0;
    // in a batch only this op's blocks: an earlier op's must stay intact for a rollback to it
    for (i = fs->alloced_count; i > (fs->batch ? fs->op_alloced : 0); i--)
        if (fs->alloced[i - 1] == (uint64_t)paddr)
            return 1;
    return 0;
}

static int phys_path_publish_key(struct apfsrw *fs, struct bpath *p,
    uint8_t *leaf, const void *new_minkey, uint16_t minkey_len,
    int64_t key_delta, apfs_paddr_t *new_root)
{
    struct apfs_btree_node_phys *node = NULL;
    uint64_t child_new = 0;
    uint32_t level;
    int err;

    if (txn_owns_block(fs, p->paddr[p->n - 1])) {
        child_new = (uint64_t)p->paddr[p->n - 1];
    } else {
        err = alloc_blocks(fs, 1, &child_new);
        if (err != APFSRW_OK)
            return err;
        defer_free(fs, (uint64_t)p->paddr[p->n - 1], 1);
    }
    wr64(leaf + offsetof(struct apfs_obj_phys, o_oid), child_new);
    wr64(leaf + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    seal_object(fs, leaf);
    err = write_block(fs, (apfs_paddr_t)child_new, leaf);
    if (err != APFSRW_OK)
        return err;

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
                new_minkey = NULL;       // parent's own minimum unchanged
        } else {
            new_minkey = NULL;
        }

        // bt_key_count on the ROOT counts the whole tree (spec p.126-127)
        if (pi == 0 && key_delta != 0 &&
            (rd16(&node->btn_flags) & APFS_BTNODE_ROOT) != 0) {
            struct apfs_btree_info *bi = (struct apfs_btree_info *)
                ((uint8_t *)node + fs->block_size - sizeof(*bi));

            wr64(&bi->bt_key_count,
                (uint64_t)((int64_t)rd64(&bi->bt_key_count) + key_delta));
        }

        if (txn_owns_block(fs, p->paddr[pi])) {
            parent_new = (uint64_t)p->paddr[pi];
        } else {
            err = alloc_blocks(fs, 1, &parent_new);
            if (err != APFSRW_OK)
                goto out;
            defer_free(fs, (uint64_t)p->paddr[pi], 1);
        }
        wr64(&node->btn_o.o_oid, parent_new);
        wr64(&node->btn_o.o_xid, fs->xid + 1);
        seal_object(fs, node);
        err = write_block(fs, (apfs_paddr_t)parent_new, node);
        if (err != APFSRW_OK)
            goto out;
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

// Point an object map entry at a new physical address, moving it into the new transaction.
// Returns the new object map tree root, since the whole path is copied
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

static int phys_write_node(struct apfsrw *fs, uint8_t *node,
    apfs_paddr_t old_paddr, apfs_paddr_t *out)
{
    uint64_t blk = 0;
    int reuse = txn_owns_block(fs, old_paddr);
    int err = APFSRW_OK;

    if (reuse)
        blk = (uint64_t)old_paddr;
    else
        err = alloc_blocks(fs, 1, &blk);
    if (err != APFSRW_OK)
        return err;
    wr64(node + offsetof(struct apfs_obj_phys, o_oid), blk);
    wr64(node + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    seal_object(fs, node);
    err = write_block(fs, (apfs_paddr_t)blk, node);
    if (err != APFSRW_OK)
        return err;
    if (!reuse && old_paddr > 0)
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
                uint8_t *nv = rec_realloc(fs, recs[r].val, vlen);

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
        err = insert_record(fs, recs, &count, key, klen, val, vlen);
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
                    // Only index 0 changes the parent's own minimum
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

        // Point the parent at the left half and give it the new sibling
        {
            uint8_t sepkey[APFSRW_MAX_KEY];
            uint8_t sepval[8];
            uint16_t sepklen = recs[sp].klen;
            const void *kp, *vp;
            uint16_t kl, vl;
            uint32_t pi = lvl - 1;

            memcpy(sepkey, recs[sp].key, sepklen);
            wr64(sepval, (uint64_t)right_pa);

            free_records(fs, recs, count);
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
            err = insert_record(fs, recs, &count, sepkey, sepklen, sepval, 8);
            if (err != APFSRW_OK)
                goto out;
            lvl = pi;
        }
    }
out:
    if (recs != NULL) {
        free_records(fs, recs, count);
        free(recs);
    }
    free(b2);
    free(b1);
    free(cur);
    return err;
}

static int omap_insert(struct apfsrw *fs, apfs_paddr_t tree_root,
    apfs_oid_t oid, apfs_paddr_t paddr, apfs_paddr_t *new_tree_root)
{
    uint8_t key[16], val[16];

    memset(key, 0, sizeof(key));
    wr64(key, oid);
    wr64(key + 8, (uint64_t)(fs->xid + 1));
    memset(val, 0, sizeof(val));
    wr32(val + 4, fs->block_size);       // ov_size
    wr64(val + 8, (uint64_t)paddr);      // ov_paddr

    return phys_insert(fs, tree_root, BKEY_OMAP, key, (uint16_t)sizeof(key),
        val, (uint16_t)sizeof(val), 0, new_tree_root);
}

// Record the full route down the file-system tree, the leaf alone is too little
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

static int fs_publish_node(struct apfsrw *fs, uint8_t *node, apfs_oid_t oid,
    apfs_paddr_t old_paddr, int is_new, apfs_paddr_t *out)
{
    apfs_paddr_t new_omap_root = 0;
    uint64_t blk = 0;
    int err;

    // The object map already points this oid at a block of this transaction
    if (!is_new && txn_owns_block(fs, old_paddr)) {
        wr64(node + offsetof(struct apfs_obj_phys, o_oid), oid);
        wr64(node + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
        seal_object(fs, node);
        err = write_block(fs, old_paddr, node);
        if (err == APFSRW_OK && out != NULL)
            *out = old_paddr;
        return err;
    }
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

// Fetch one record from the file-system tree by exact key
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
        free_records(fs, recs, count);
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
            uint8_t *nk = rec_realloc(fs, recs[idx].key, newklen);

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
            break;                       // parent's own minimum unchanged
        lvl = pi;
    }
out:
    if (recs != NULL) {
        free_records(fs, recs, count);
        free(recs);
    }
    free(nbuf);
    free(node);
    return err;
}

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

    lvl = p.n - 1;                       // Start at the leaf
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
                uint8_t *nv = rec_realloc(fs, recs[i].val, vlen);

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
        err = insert_record(fs, recs, &count, key, klen, val, vlen);
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

        // Split
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

            // The old root becomes an index node over the two halves
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

        // Not the root: keep this node's oid for the left half
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

        // Hand the new separator to the parent and go round again
        {
            uint8_t sepkey[APFSRW_MAX_KEY];
            uint8_t sepval[8];
            uint16_t sepklen = recs[sp].klen;

            memcpy(sepkey, recs[sp].key, sepklen);
            wr64(sepval, right_oid);

            free_records(fs, recs, count);
            memset(recs, 0, APFSRW_MAX_RECORDS * sizeof(*recs));
            count = 0;
            lvl--;
            err = read_object(fs, p.paddr[lvl], cur);
            if (err != APFSRW_OK)
                goto out;
            err = load_leaf_records(fs, cur, &p.info, recs, &count);
            if (err != APFSRW_OK)
                goto out;
            err = insert_record(fs, recs, &count, sepkey, sepklen, sepval, 8);
            if (err != APFSRW_OK)
                goto out;
        }
    }

    fs->alloc_delta += added_nodes;
    err = fstree_bump_counts(fs, replace ? 0 : 1, added_nodes, klen, vlen);
out:
    if (recs != NULL) {
        free_records(fs, recs, count);
        free(recs);
    }
    free(nbuf2);
    free(nbuf);
    free(cur);
    return err;
}

static int omap_del(struct apfsrw *fs, apfs_oid_t oid);

static int fstree_del(struct apfsrw *fs, const void *key, uint16_t klen)
{
    struct fpath p;
    struct apfs_btree_node_phys *cur = NULL;
    struct rw_rec *recs = NULL;
    uint8_t *nbuf = NULL;
    uint32_t count = 0, lvl, i, idx = 0;
    uint16_t gone_klen = 0, gone_vlen = 0;
    int64_t gone_nodes = 0;
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

    lvl = p.n - 1;                       // The leaf
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
    gone_klen = recs[idx].klen;
    gone_vlen = recs[idx].vlen;
    drop_record(fs, recs, &count, idx);
    // an emptied non-root node leaves the tree: parent entry, object map entry and block
    while (count == 0 && lvl != 0) {
        err = omap_del(fs, p.oid[lvl]);
        if (err != APFSRW_OK)
            goto out;
        defer_free(fs, (uint64_t)p.paddr[lvl], 1);
        gone_nodes++;
        lvl--;
        idx = p.index[lvl];
        err = read_object(fs, p.paddr[lvl], cur);
        if (err != APFSRW_OK)
            goto out;
        err = load_leaf_records(fs, cur, &p.info, recs, &count);
        if (err != APFSRW_OK)
            goto out;
        if (idx >= count) {
            err = APFSRW_EINVAL;
            goto out;
        }
        drop_record(fs, recs, &count, idx);
    }

    // only the root can be empty here, and an empty tree is a single leaf
    err = build_node(fs, cur, (lvl == 0),
        count == 0 ? 0 : (uint16_t)(p.n - 1 - lvl), &p.info, recs, count, nbuf);
    if (err != APFSRW_OK)
        goto out;
    err = fs_publish_node(fs, nbuf, p.oid[lvl], p.paddr[lvl], 0, &out_pa);
    if (err != APFSRW_OK)
        goto out;
    if (lvl == 0)
        fs->root_tree_paddr = out_pa;
    else if (idx == 0 && count > 0) {
        // The smallest key in this node changed
        err = fstree_fix_separator(fs, &p, lvl, recs[0].key, recs[0].klen);
        if (err != APFSRW_OK)
            goto out;
    }

    fs->alloc_delta -= gone_nodes;
    err = fstree_bump_counts(fs, -1, -gone_nodes, gone_klen, gone_vlen);
out:
    if (recs != NULL) {
        free_records(fs, recs, count);
        free(recs);
    }
    free(nbuf);
    free(cur);
    return err;
}

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
        err = APFSRW_ENOTSUP;            // multi-level omap not handled yet
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
        wr64((void *)(uintptr_t)&((struct apfs_omap_key *)(uintptr_t)kp)->
            ok_xid, (uint64_t)(fs->xid + 1));
        wr64((void *)(uintptr_t)&((struct apfs_omap_val *)(uintptr_t)vp)->
            ov_paddr, (uint64_t)new_paddr);
    }
    // An object map tree node is a physical object, so its oid is its address (spec p.11).
    // A copy that keeps the old o_oid makes fsck_apfs report om: bt: invalid o_oid
    wr64(&node->btn_o.o_oid, (uint64_t)dst);
    wr64(&node->btn_o.o_xid, fs->xid + 1);
    seal_object(fs, node);
    err = write_block(fs, dst, node);
out:
    free(node);
    return err;
}

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
    err = ac_flush(fs);
    if (err != APFSRW_OK)
        return err;
    map_index = next % desc_blocks;
    sb_index = (map_index + 1) % desc_blocks;

    block = calloc(1, fs->block_size);
    if (block == NULL)
        return APFSRW_ENOMEM;
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
    if (fs->newvol_pending)
        wr64(block + offsetof(struct apfs_nx_superblock, nx_fs_oid) +
            fs->newvol_slot * 8U, fs->newvol_oid);
    wr32(block + offsetof(struct apfs_nx_superblock, nx_xp_desc_index),
        map_index);
    wr32(block + offsetof(struct apfs_nx_superblock, nx_xp_desc_len), 2);
    wr32(block + offsetof(struct apfs_nx_superblock, nx_xp_desc_next),
        (sb_index + 1) % desc_blocks);
    seal_object(fs, block);
    err = write_block(fs, (apfs_paddr_t)(desc_base + sb_index), block);
    if (err != APFSRW_OK)
        goto out;
    // The checkpoint moved to a new slot in the descriptor ring
    fs->nx_paddr = (apfs_paddr_t)(desc_base + sb_index);

    // Block zero is a copy of the newest superblock. Keep it consistent
    err = write_block(fs, 0, block);
out:
    free(block);
    return err;
}

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
    // apfsck checks each of these against what it finds in the tree
    wr64(block + offsetof(struct apfs_superblock, apfs_num_files),
        rd64(&fs->apfs.apfs_num_files) + extra_files);
    wr64(block + offsetof(struct apfs_superblock, apfs_num_directories),
        rd64(&fs->apfs.apfs_num_directories) + extra_dirs);
    wr64(block + offsetof(struct apfs_superblock, apfs_num_symlinks),
        rd64(&fs->apfs.apfs_num_symlinks) + extra_links);
    wr64(block + offsetof(struct apfs_superblock, apfs_num_other_fsobjects),
        (uint64_t)((int64_t)rd64(&fs->apfs.apfs_num_other_fsobjects) +
        fs->other_delta));
    // apfs_role lives past our struct (spec apfs_superblock_t, byte 964)
    if (fs->role_pending >= 0)
        wr16(block + 964, (uint16_t)fs->role_pending);
    // apfs_fs_alloc_count counts blocks the VOLUME owns.
    // fsck_apfs checks it against the extents it finds
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
    if (fs->newvol_pending) {
        apfs_paddr_t ctree2 = 0;

        err = omap_insert(fs, (apfs_paddr_t)ctree, fs->newvol_oid,
            fs->newvol_paddr, &ctree2);
        if (err != APFSRW_OK)
            goto out;
        ctree = (uint64_t)ctree2;
    }

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

    err = ac_flush(fs);
    if (err != APFSRW_OK)
        goto out;
    if (apfsrw_sync(fs) != 0) {
        err = APFSRW_EIO;
        goto out;
    }
    // Hand back the superseded metadata BEFORE publishing,
    // so the space manager is final by the time the checkpoint that describes it lands
    defer_free(fs, (uint64_t)fs->volume_omap_paddr, 1);
    defer_free(fs, (uint64_t)fs->fs_paddr, 1);
    defer_free(fs, (uint64_t)fs->container_omap_tree_paddr, 1);
    defer_free(fs, (uint64_t)fs->container_omap_paddr, 1);
    // Leave the old extent reference root alone, phys_path_publish already freed that path. A second
    // free would clear the bit of a block allocated since, which fsck_apfs reports as underallocation

    err = publish_checkpoint(fs, (apfs_paddr_t)comap);
    if (err == APFSRW_OK && apfsrw_sync(fs) != 0)
        err = APFSRW_EIO;
    if (err == APFSRW_OK) {
        flush_deferred(fs);
        fs->alloced_count = 0;
        fs->alloc_delta = 0;
        fs->other_delta = 0;
        fs->newvol_pending = 0;
        // Frees trail the durable checkpoint, so they need not be waited on
        (void)apfsrw_sync_nowait(fs);

        // Adopt the state just published, or later operations
        // keep reading the superseded superblock and object map
        fs->xid += 1;
        fs->fs_paddr = (apfs_paddr_t)vsb;
        fs->volume_omap_oid = vomap;
        fs->volume_omap_paddr = (apfs_paddr_t)vomap;
        fs->container_omap_paddr = (apfs_paddr_t)comap;
        fs->container_omap_tree_paddr = (apfs_paddr_t)ctree;
        if (read_object(fs, fs->fs_paddr, block) == APFSRW_OK) {
            memcpy(&fs->apfs, block, sizeof(fs->apfs));
        } else {
            // Never continue on a stale volume superblock
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

// End of one mutation. Commits on its own outside a batch, folds into the batch otherwise.
// A commit costs about 130 block writes whatever the mutation was
static int txn_finish(struct apfsrw *fs, uint64_t next_oid, uint64_t dfiles,
    uint64_t ddirs, uint64_t dlinks)
{
    if (!fs->batch)
        return cow_commit(fs, fs->root_tree_paddr, next_oid, dfiles, ddirs,
            dlinks, fs->extref_paddr, 0);

    fs->batch_files += dfiles;
    fs->batch_dirs += ddirs;
    fs->batch_links += dlinks;
    // Object ids only move forward.
    // a mutation that allocated none reports the current watermark, which must not pull the batch back
    if (next_oid > fs->batch_next_oid)
        fs->batch_next_oid = next_oid;
    fs->batch_dirty = 1;
    op_mark(fs);
    return APFSRW_OK;
}

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
        err = APFSRW_OK;                 // root is the leaf: already counted
        goto out;
    }
    bi = (struct apfs_btree_info *)((uint8_t *)root + fs->block_size -
        sizeof(*bi));
    wr64(&bi->bt_key_count,
        (uint64_t)((int64_t)rd64(&bi->bt_key_count) + key_delta));
    wr64(&bi->bt_node_count,
        (uint64_t)((int64_t)rd64(&bi->bt_node_count) + node_delta));
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

// Remove one record from a physical tree. A non-root node left empty is unlinked from its parent
// and freed, repeating upward (fsck_apfs rejects btn_nkeys 0). An emptied root becomes an empty leaf
static int phys_delete(struct apfsrw *fs, apfs_paddr_t tree_root,
    enum bkey_kind kind, const void *key, uint16_t klen,
    apfs_paddr_t *new_root)
{
    struct bpath p;
    struct apfs_btree_node_phys *cur = NULL;
    struct rw_rec *recs = NULL;
    uint8_t *b1 = NULL;
    uint8_t newmin[APFSRW_MAX_KEY];
    uint16_t newmin_len = 0, tree_level;
    uint32_t count = 0, i, idx = 0, lvl, l;
    int64_t gone_nodes = 0;
    apfs_paddr_t child_new = 0;
    int found = 0;
    int err;

    err = descend_phys(fs, tree_root, kind, key, klen, &p);
    if (err != APFSRW_OK)
        return err;

    cur = calloc(1, fs->block_size);
    b1 = calloc(1, fs->block_size);
    recs = calloc(APFSRW_MAX_RECORDS, sizeof(*recs));
    if (cur == NULL || b1 == NULL || recs == NULL) {
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
    for (i = 0; i < count; i++) {
        // object map keys match on the oid, newest version not past the key's xid
        if (kind == BKEY_OMAP) {
            if (recs[i].klen >= 16 && klen >= 16 &&
                rd64(recs[i].key) == rd64(key) &&
                rd64(recs[i].key + 8) <= rd64((const uint8_t *)key + 8)) {
                idx = i;
                found = 1;
            }
            continue;
        }
        if (rec_cmp(recs[i].key, recs[i].klen, key, klen) == 0) {
            idx = i;
            found = 1;
            break;
        }
    }
    if (!found) {
        err = APFSRW_ENOENT;
        goto out;
    }
    drop_record(fs, recs, &count, idx);
    while (count == 0 && lvl != 0) {
        defer_free(fs, (uint64_t)p.paddr[lvl], 1);
        gone_nodes++;
        lvl--;
        idx = p.index[lvl];
        err = read_object(fs, p.paddr[lvl], cur);
        if (err != APFSRW_OK)
            goto out;
        err = load_leaf_records(fs, cur, &p.info, recs, &count);
        if (err != APFSRW_OK)
            goto out;
        if (idx >= count) {
            err = APFSRW_EINVAL;
            goto out;
        }
        drop_record(fs, recs, &count, idx);
    }
    if (idx == 0 && lvl != 0 && count > 0 &&
        recs[0].klen <= sizeof(newmin)) {
        memcpy(newmin, recs[0].key, recs[0].klen);
        newmin_len = recs[0].klen;
    }

    // only the root can be empty here, and an empty tree is a single leaf
    tree_level = count == 0 ? 0 : (uint16_t)(p.n - 1 - lvl);
    err = build_node(fs, cur, (lvl == 0), tree_level, &p.info, recs, count,
        b1);
    if (err != APFSRW_OK)
        goto out;
    // a root leaf got its counts from build_node
    if (lvl == 0 && tree_level > 0) {
        struct apfs_btree_info *bi = (struct apfs_btree_info *)
            (b1 + fs->block_size - sizeof(*bi));

        wr64(&bi->bt_key_count, rd64(&bi->bt_key_count) - 1);
        wr64(&bi->bt_node_count,
            (uint64_t)((int64_t)rd64(&bi->bt_node_count) - gone_nodes));
    }
    err = phys_write_node(fs, b1, p.paddr[lvl], &child_new);
    if (err != APFSRW_OK)
        goto out;

    // Repoint the path, carrying a changed minimum key up like phys_insert
    for (l = lvl; l > 0; l--) {
        const void *kp, *vp;
        uint16_t kl, vl;
        uint32_t pi = l - 1;
        int min_moved = (l == lvl) && (newmin_len != 0);

        err = read_object(fs, p.paddr[pi], cur);
        if (err != APFSRW_OK)
            goto out;
        err = btree_entry(fs, cur, &p.info, p.index[pi], &kp, &kl, &vp, &vl);
        if (err != APFSRW_OK)
            goto out;
        wr64((void *)(uintptr_t)vp, (uint64_t)child_new);
        if (min_moved && kl == newmin_len) {
            memcpy((void *)(uintptr_t)kp, newmin, newmin_len);
            if (p.index[pi] != 0)
                newmin_len = 0;
        } else {
            newmin_len = 0;
        }
        if (pi == 0) {
            struct apfs_btree_info *bi = (struct apfs_btree_info *)
                ((uint8_t *)cur + fs->block_size - sizeof(*bi));

            wr64(&bi->bt_key_count, rd64(&bi->bt_key_count) - 1);
            wr64(&bi->bt_node_count,
                (uint64_t)((int64_t)rd64(&bi->bt_node_count) - gone_nodes));
        }
        err = phys_write_node(fs, (uint8_t *)cur, p.paddr[pi], &child_new);
        if (err != APFSRW_OK)
            goto out;
    }
    *new_root = child_new;
    fs->alloc_delta -= gone_nodes;
out:
    if (recs != NULL) {
        free_records(fs, recs, count);
        free(recs);
    }
    free(b1);
    free(cur);
    return err;
}

// Drop the volume object map entry of a freed file-system tree node
static int omap_del(struct apfsrw *fs, apfs_oid_t oid)
{
    uint8_t key[16];
    apfs_paddr_t root = 0;
    int err;

    wr64(key, oid);
    wr64(key + 8, (uint64_t)(fs->xid + 1));
    err = phys_delete(fs, fs->volume_omap_tree_paddr, BKEY_OMAP, key,
        (uint16_t)sizeof(key), &root);
    if (err == APFSRW_OK)
        fs->volume_omap_tree_paddr = root;
    return err;
}

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
    // Upsert: a freed extent's record stays behind, so a reused block's paddr key may already exist.
    // The stale record is simply replaced
    err = phys_insert(fs, fs->extref_paddr, BKEY_JKEY,
        key, (uint16_t)sizeof(key), val, (uint16_t)sizeof(val), 1, &root);
    if (err == APFSRW_OK) {
        fs->extref_paddr = root;
        *new_root = (uint64_t)root;
    }
    return err;
}

// Drop the extent reference for a freed extent. Records are written one per extent (extentref_add),
// so the key is exact. a missing one is not an error
static int extentref_del(struct apfsrw *fs, uint64_t paddr)
{
    uint8_t key[8];
    apfs_paddr_t root = 0;
    int err;

    if (fs->extref_paddr == 0)
        fs->extref_paddr =
            (apfs_paddr_t)rd64(&fs->apfs.apfs_extentref_tree_oid);
    wr64(key, make_jkey(paddr, APFS_TYPE_EXTENT));
    err = phys_delete(fs, fs->extref_paddr, BKEY_JKEY, key,
        (uint16_t)sizeof(key), &root);
    if (err == APFSRW_ENOENT || err == APFSRW_ENOTSUP)
        return APFSRW_OK;
    if (err == APFSRW_OK)
        fs->extref_paddr = root;
    return err;
}

// Split a path into its parent directory and final component, resolving the parent to an object id.
// "/usr/lib/foo" -> parent is /usr/lib, name "foo"
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

static int extent_put(struct apfsrw *fs, uint64_t stream_id, uint64_t logical,
    uint64_t phys, uint64_t len);

// Runs come from one chunk, so an extent never spans more than this
#define APFSRW_MAX_EXTENT_BLOCKS 32768U

static int stream_write_data(struct apfsrw *fs, uint64_t stream_id,
    uint64_t logical, const void *data, uint64_t size, uint64_t hint,
    uint64_t *blocks_out, uint32_t *pieces_out, uint64_t *first_phys)
{
    uint64_t bs = fs->block_size, done = 0, total = 0;
    uint32_t pieces = 0;
    uint8_t *block = calloc(1, bs);
    int err = APFSRW_OK;

    if (block == NULL)
        return APFSRW_ENOMEM;
    while (done < size) {
        uint64_t remain = (size - done + bs - 1) / bs, phys = 0, root = 0, i;
        uint32_t got = remain > APFSRW_MAX_EXTENT_BLOCKS ?
            APFSRW_MAX_EXTENT_BLOCKS : (uint32_t)remain;

        for (;;) {
            err = alloc_data_blocks(fs, got, pieces == 0 ? hint : 0, &phys);
            if (err == APFSRW_OK)
                break;
            if (err != APFSRW_ENOSPC || got == 1)
                goto out;
            got /= 2;
        }
        // Whole blocks straight from the caller's buffer in one write.
        // Only a partial last block goes through the bounce block
        {
            uint64_t whole = (size - done) / bs;
            uint64_t nwhole = whole < got ? whole : got;

            if (nwhole > 0) {
                bc_drop(fs, (apfs_paddr_t)phys, nwhole);
                if (apfsrw_pwrite(fs, (const uint8_t *)data + done,
                    (size_t)(nwhole * bs), (off_t)(phys * bs)) !=
                    (ssize_t)(nwhole * bs)) {
                    err = APFSRW_EIO;
                    goto out;
                }
            }
            for (i = nwhole; i < got; i++) {
                uint64_t off = done + i * bs;
                uint64_t n = size > off ? size - off : 0;

                if (n > bs)
                    n = bs;
                memset(block, 0, bs);
                if (n > 0)
                    memcpy(block, (const uint8_t *)data + off, n);
                err = write_raw(fs, (apfs_paddr_t)(phys + i), block);
                if (err != APFSRW_OK)
                    goto out;
            }
        }
        err = extent_put(fs, stream_id, logical + done, phys, (uint64_t)got * bs);
        if (err != APFSRW_OK)
            goto out;
        err = extentref_add(fs, phys, got, stream_id, &root);
        if (err != APFSRW_OK)
            goto out;
        if (pieces == 0 && first_phys != NULL)
            *first_phys = phys;
        done += (uint64_t)got * bs;
        total += got;
        pieces++;
    }
out:
    free(block);
    if (blocks_out != NULL)
        *blocks_out = total;
    if (pieces_out != NULL)
        *pieces_out = pieces;
    return err;
}

// what a copied entry brings beyond create_entry's arguments: source times and flags,
// its xattrs, and for a compressed file the size its decmpfs data expands to
struct cp_xattr {
    const char *name;
    uint16_t flags;
    const uint8_t *data;
    size_t len;
};

struct cp_extra {
    uint64_t atime, mtime, ctime, crtime;
    uint32_t bsd_flags;
    int compressed;             // no data stream: decmpfs and resource-fork xattrs hold the content
    int rsrc;                   // a com.apple.ResourceFork xattr is among the xattrs
    uint64_t ucsize;
    const struct cp_xattr *xattrs;
    uint32_t nxattrs;
};

#define APFS_INODE_HAS_RSRC_FORK 0x4000ULL
#define APFS_INODE_HAS_UNCOMPRESSED_SIZE 0x40000ULL

static int xattr_key_for(uint64_t fileid, const char *name, uint8_t *key,
    uint16_t *klen);

// the next object id to hand out: an open batch runs ahead of the committed superblock
static uint64_t cur_next_oid(struct apfsrw *fs)
{
    return (fs->batch && fs->batch_next_oid != 0) ?
        fs->batch_next_oid : rd64(&fs->apfs.apfs_next_obj_id);
}

static int stream_write_data(struct apfsrw *fs, uint64_t stream_id,
    uint64_t logical, const void *data, uint64_t size, uint64_t hint,
    uint64_t *blocks_out, uint32_t *pieces_out, uint64_t *first_phys);

// write a new inode's xattrs. Values past the embedded limit get a data stream of their own
// (j_xattr_dstream_t, spec p.106) under the next free object id
static int put_xattrs(struct apfsrw *fs, uint64_t fileid,
    const struct cp_extra *x, uint64_t *next_oid)
{
    uint8_t key[10 + 256], val[4 + APFSRW_XATTR_MAX_EMBEDDED];
    uint16_t klen = 0;
    uint32_t i;
    int err;

    for (i = 0; i < x->nxattrs; i++) {
        const struct cp_xattr *a = &x->xattrs[i];
        uint16_t owned = a->flags & APFS_XATTR_FILE_SYSTEM_OWNED;

        err = xattr_key_for(fileid, a->name, key, &klen);
        if (err != APFSRW_OK)
            return err;
        // a value keeps the form it had in the source: stream-backed stays a stream
        if (!(a->flags & APFS_XATTR_DATA_STREAM) && a->len <= APFSRW_XATTR_MAX_EMBEDDED) {
            wr16(val, (uint16_t)(APFS_XATTR_DATA_EMBEDDED | owned));
            wr16(val + 2, (uint16_t)a->len);
            if (a->len > 0)
                memcpy(val + 4, a->data, a->len);
            err = fstree_put(fs, key, klen, val, (uint16_t)(4 + a->len), 0);
        } else {
            uint64_t oid = (*next_oid)++, nblocks = 0;
            uint8_t dkey[8], dval[4];

            err = stream_write_data(fs, oid, 0, a->data, a->len, 0, &nblocks,
                NULL, NULL);
            if (err != APFSRW_OK)
                return err;
            fs->alloc_delta += (int64_t)nblocks;
            wr64(dkey, make_jkey(oid, APFS_TYPE_DSTREAM_ID));
            wr32(dval, 1);
            err = fstree_put(fs, dkey, 8, dval, 4, 0);
            if (err != APFSRW_OK)
                return err;
            memset(val, 0, 4 + sizeof(struct apfs_j_xattr_dstream));
            wr16(val, (uint16_t)(APFS_XATTR_DATA_STREAM | owned));
            wr16(val + 2, (uint16_t)sizeof(struct apfs_j_xattr_dstream));
            wr64(val + 4 + offsetof(struct apfs_j_xattr_dstream, xattr_obj_id), oid);
            wr64(val + 4 + offsetof(struct apfs_j_xattr_dstream, size), a->len);
            wr64(val + 4 + offsetof(struct apfs_j_xattr_dstream, alloced_size),
                nblocks * fs->block_size);
            wr64(val + 4 + offsetof(struct apfs_j_xattr_dstream, total_bytes_written),
                a->len);
            err = fstree_put(fs, key, klen, val,
                (uint16_t)(4 + sizeof(struct apfs_j_xattr_dstream)), 0);
        }
        if (err != APFSRW_OK)
            return err;
    }
    return APFSRW_OK;
}

static int create_entry(struct apfsrw *fs, const char *path, uint16_t ftype,
    const void *data, size_t size, const char *linktarget, uint16_t mode,
    uint32_t uid, uint32_t gid, uint64_t *id_out, const struct cp_extra *x)
{
    uint8_t *zero = NULL;
    uint64_t parent = 0, fileid, data_block = 0, nblocks = 0, existing, next_oid;
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
    fileid = cur_next_oid(fs);
    if (fileid < APFSRW_ROOT_FILEID)
        fileid = APFSRW_ROOT_FILEID + 1;
    next_oid = fileid + 1;

    if (ftype == APFSRW_DT_REG && size > 0) {
        // Data first: extents and their references, before the inode
        err = stream_write_data(fs, fileid, 0, data, size, 0, &nblocks, NULL,
            &data_block);
        if (err != APFSRW_OK)
            goto out;
    }

    // INODE
    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    memset(val, 0, sizeof(val));
    wr64(val + offsetof(struct apfs_j_inode_val, parent_id), parent);
    wr64(val + offsetof(struct apfs_j_inode_val, private_id), fileid);
    wr64(val + offsetof(struct apfs_j_inode_val, create_time), x ? x->crtime : now);
    wr64(val + offsetof(struct apfs_j_inode_val, mod_time), x ? x->mtime : now);
    wr64(val + offsetof(struct apfs_j_inode_val, change_time), x ? x->ctime : now);
    wr64(val + offsetof(struct apfs_j_inode_val, access_time), x ? x->atime : now);
    wr32(val + offsetof(struct apfs_j_inode_val, u),
        ftype == APFSRW_DT_DIR ? 0 : 1);   // nchildren for dirs, nlink else
    wr32(val + offsetof(struct apfs_j_inode_val, owner), uid);
    wr32(val + offsetof(struct apfs_j_inode_val, group), gid);
    wr16(val + offsetof(struct apfs_j_inode_val, mode), mode);
    wr64(val + offsetof(struct apfs_j_inode_val, internal_flags),
        x && x->rsrc ? APFS_INODE_HAS_RSRC_FORK : APFS_INODE_NO_RSRC_FORK);
    if (x != NULL) {
        wr32(val + offsetof(struct apfs_j_inode_val, bsd_flags), x->bsd_flags);
        if (x->compressed) {
            wr64(val + offsetof(struct apfs_j_inode_val, internal_flags),
                (x->rsrc ? APFS_INODE_HAS_RSRC_FORK : APFS_INODE_NO_RSRC_FORK) |
                APFS_INODE_HAS_UNCOMPRESSED_SIZE);
            wr64(val + offsetof(struct apfs_j_inode_val, uncompressed_size), x->ucsize);
        }
    }
    {
        uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
        uint16_t nsz = (uint16_t)(namelen + 1);
        uint16_t npad = (uint16_t)((nsz + 7u) & ~7u);
        int want_dstream = (ftype == APFSRW_DT_REG) && !(x && x->compressed);
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

    if (ftype != APFSRW_DT_REG && ftype != APFSRW_DT_DIR &&
        ftype != APFSRW_DT_LNK)
        fs->other_delta += 1;
    // Every DSTREAM extended field needs its DSTREAM_ID record, even at size 0 (fsck_apfs:
    // "dstream does not have an associated dstream id")
    if (ftype == APFSRW_DT_REG && !(x && x->compressed)) {
        wr64(key, make_jkey(fileid, APFS_TYPE_DSTREAM_ID));
        memset(val, 0, sizeof(val));
        wr32(val, 1);
        err = fstree_put(fs, key, 8, val, 4, 0);
        if (err != APFSRW_OK)
            goto out;
    }

    if (ftype == APFSRW_DT_LNK) {
        // A symlink's target is an extended attribute,
        // com.apple.fs.symlink (spec p.83), stored inline with XATTR_DATA_EMBEDDED
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

    if (x != NULL && x->nxattrs != 0) {
        err = put_xattrs(fs, fileid, x, &next_oid);
        if (err != APFSRW_OK)
            goto out;
    }

    // DIR_REC in the parent
    dtype = (uint8_t)ftype;
    wr64(key, make_jkey(parent, APFS_TYPE_DIR_REC));
    wr32(key + 8, ((hash << 10) & 0xfffffc00U) | ((namelen + 1U) & 0x3ffU));
    memcpy(key + 12, name, namelen);
    key[12 + namelen] = '\0';
    memset(val, 0, sizeof(val));
    wr64(val, fileid);
    wr64(val + 8, x ? x->crtime : now);
    wr16(val + 16, dtype);
    err = fstree_put(fs, key, (uint16_t)(12 + namelen + 1), val, 18, 0);
    if (err != APFSRW_OK)
        goto out;

    // The parent gained a child
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

    if (ftype == APFSRW_DT_REG && size > 0)
        fs->alloc_delta += (int64_t)nblocks;

    err = txn_finish(fs, next_oid, ftype == APFSRW_DT_REG ? 1 : 0,
        ftype == APFSRW_DT_DIR ? 1 : 0, ftype == APFSRW_DT_LNK ? 1 : 0);
    if (err == APFSRW_OK && id_out != NULL)
        *id_out = fileid;
out:
    if (err != APFSRW_OK)
        txn_rollback(fs);
    free(zero);
    return err;
}

int apfsrw_batch_begin(struct apfsrw *fs)
{
    if (fs == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (fs->batch)
        return APFSRW_EINVAL;
    fs->batch = 1;
    fs->batch_dirty = 0;
    fs->batch_files = 0;
    fs->batch_dirs = 0;
    fs->batch_links = 0;
    fs->batch_next_oid = 0;
    op_mark(fs);
    return APFSRW_OK;
}

// Blocks superseded but not yet released.
// They cannot be reused until a checkpoint lands, so a long batch should checkpoint when this grows
uint32_t apfsrw_batch_pending(struct apfsrw *fs)
{
    return fs == NULL ? 0 : fs->deferred_count;
}

// Whether an open batch holds mutations that are not on disk yet
int apfsrw_batch_dirty(struct apfsrw *fs)
{
    return fs != NULL && fs->batch && fs->batch_dirty;
}

uint32_t apfsrw_batch_blocks(struct apfsrw *fs)
{
    return fs == NULL || !fs->batch ? 0 : fs->alloced_count;
}

int apfsrw_batch_owns(struct apfsrw *fs, uint64_t paddr, uint64_t n)
{
    uint32_t i;

    if (fs == NULL || !fs->batch)
        return 0;
    for (i = 0; i < fs->alloced_count; i++)
        if (fs->alloced[i] >= paddr && fs->alloced[i] - paddr < n)
            return 1;
    return 0;
}

int apfsrw_batch_end(struct apfsrw *fs)
{
    int err;

    if (fs == NULL)
        return APFSRW_EINVAL;
    if (!fs->batch)
        return APFSRW_EINVAL;
    fs->batch = 0;
    if (!fs->batch_dirty)
        return APFSRW_OK;                // Nothing was mutated
    fs->batch_dirty = 0;
    // Mutations that allocate no object still have to carry the watermark
    if (fs->batch_next_oid == 0)
        fs->batch_next_oid = rd64(&fs->apfs.apfs_next_obj_id);
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
        (uint16_t)(0100000u | (mode & 07777u)), uid, gid, NULL, NULL);
}

int apfsrw_mkdir(struct apfsrw *fs, const char *path, uint16_t mode,
    uint32_t uid, uint32_t gid)
{
    return create_entry(fs, path, APFSRW_DT_DIR, NULL, 0, NULL,
        (uint16_t)(0040000u | (mode & 07777u)), uid, gid, NULL, NULL);
}

int apfsrw_mknod(struct apfsrw *fs, const char *path, uint16_t ftype,
    uint16_t mode, uint32_t uid, uint32_t gid)
{
    if (ftype != APFSRW_DT_SOCK && ftype != APFSRW_DT_FIFO &&
        ftype != APFSRW_DT_CHR && ftype != APFSRW_DT_BLK)
        return APFSRW_EINVAL;
    return create_entry(fs, path, ftype, NULL, 0, NULL, mode, uid, gid, NULL, NULL);
}

// Delete every FILE_EXTENT record of a stream and defer-free its blocks
// (spec p.102 j_file_extent_key_t). Extent-ref entries are left stale
static int drop_extents(struct apfsrw *fs, uint64_t stream_id,
    uint64_t *freed_blocks);

// Patch the DSTREAM xfield sizes in place (spec p.106 j_dstream_t)
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
        err = stream_write_data(fs, priv, 0, data, size, 0, &nblocks, NULL,
            &data_block);
        if (err != APFSRW_OK)
            goto fail;

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
    err = txn_finish(fs, rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0);
    if (err != APFSRW_OK)
        goto fail;
    free(block);
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    free(block);
    return err;
}

// Savepoint
#ifndef APFSRW_KERNEL
#define APFSRW_SP_MAGIC 0x50535752 /* "RWSP" */

struct sp_hdr {
    uint32_t magic, version, block_size, nblocks;
    uint64_t block_count, xid;
    uint8_t uuid[16];
};

struct sp_list {
    uint64_t *paddr;
    uint32_t n, cap;
};

static int sp_add(struct sp_list *l, uint64_t p)
{
    uint32_t i;

    for (i = 0; i < l->n; i++)
        if (l->paddr[i] == p)
            return APFSRW_OK;
    if (l->n == l->cap) {
        uint32_t ncap = l->cap ? l->cap * 2 : 64;
        uint64_t *np = realloc(l->paddr, ncap * sizeof(*np));

        if (np == NULL)
            return APFSRW_ENOMEM;
        l->paddr = np;
        l->cap = ncap;
    }
    l->paddr[l->n++] = p;
    return APFSRW_OK;
}

static int sp_collect(struct apfsrw *fs, struct sp_list *l, uint8_t *inuse)
{
    struct apfs_spaceman_phys *sm = calloc(1, fs->block_size);
    struct apfs_chunk_info_block *cib = calloc(1, fs->block_size);
    uint8_t *bm = calloc(1, fs->block_size);
    apfs_paddr_t sm_paddr = 0, rp_paddr = 0;
    uint32_t desc_blocks = rd32(&fs->nx.nx_xp_desc_blocks) & 0x7fffffffU;
    uint32_t cib_count, addr_off, i, c;
    uint64_t desc_base = rd64(&fs->nx.nx_xp_desc_base), b;
    int err = APFSRW_ENOMEM;

    if (sm == NULL || cib == NULL || bm == NULL)
        goto out;
    err = sp_add(l, 0);
    for (b = 0; b < desc_blocks && err == APFSRW_OK; b++)
        err = sp_add(l, desc_base + b);
    if (err != APFSRW_OK)
        goto out;
    err = resolve_ephemeral(fs, rd64(&fs->nx.nx_spaceman_oid), &sm_paddr);
    if (err != APFSRW_OK)
        goto out;
    err = sp_add(l, (uint64_t)sm_paddr);
    if (err == APFSRW_OK &&
        resolve_ephemeral(fs, rd64(&fs->nx.nx_reaper_oid), &rp_paddr) ==
        APFSRW_OK)
        err = sp_add(l, (uint64_t)rp_paddr);
    if (err != APFSRW_OK)
        goto out;
    err = read_object(fs, sm_paddr, sm);
    if (err != APFSRW_OK)
        goto out;
    {
        uint64_t ipb = rd64(&sm->sm_ip_bm_base);
        uint32_t ipn = rd32(&sm->sm_ip_bm_block_count);

        for (i = 0; i < ipn && err == APFSRW_OK; i++)
            err = sp_add(l, ipb + i);
        if (err != APFSRW_OK)
            goto out;
    }
    cib_count = rd32(&sm->sm_dev[0].sm_cib_count);
    addr_off = rd32(&sm->sm_dev[0].sm_addr_offset);
    for (i = 0; i < cib_count; i++) {
        apfs_paddr_t cib_paddr;

        memcpy(&cib_paddr, (uint8_t *)sm + addr_off + i * 8U, 8);
        cib_paddr = (apfs_paddr_t)rd64(&cib_paddr);
        if (cib_paddr <= 0)
            continue;
        err = sp_add(l, (uint64_t)cib_paddr);
        if (err != APFSRW_OK)
            goto out;
        err = read_object(fs, cib_paddr, cib);
        if (err != APFSRW_OK)
            goto out;
        for (c = 0; c < rd32(&cib->cib_chunk_info_count); c++) {
            struct apfs_chunk_info *ci = &cib->cib_chunk_info[c];
            uint64_t base = rd64(&ci->ci_addr);
            uint32_t nblk = rd32(&ci->ci_block_count), k;
            apfs_paddr_t bmp = (apfs_paddr_t)rd64(&ci->ci_bitmap_addr);

            if (bmp == 0)
                continue;
            err = sp_add(l, (uint64_t)bmp);
            if (err != APFSRW_OK)
                goto out;
            if (inuse == NULL)
                continue;
            err = read_raw(fs, bmp, bm);
            if (err != APFSRW_OK)
                goto out;
            for (k = 0; k < nblk && base + k < fs->block_count; k++)
                if (bm[k >> 3] & (uint8_t)(1U << (k & 7)))
                    inuse[(base + k) >> 3] |=
                        (uint8_t)(1U << ((base + k) & 7));
        }
    }
    err = APFSRW_OK;
out:
    free(bm);
    free(cib);
    free(sm);
    return err;
}

int apfsrw_savepoint(struct apfsrw *fs, const char *file)
{
    struct sp_list l;
    struct sp_hdr h;
    uint8_t *blk = NULL, *inuse = NULL;
    FILE *f = NULL;
    uint32_t i;
    int err;

    if (fs == NULL || file == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable || fs->batch)
        return APFSRW_EPERM;
    memset(&l, 0, sizeof(l));
    inuse = calloc(1, (size_t)((fs->block_count + 7) / 8));
    blk = calloc(1, fs->block_size);
    if (inuse == NULL || blk == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }
    err = sp_collect(fs, &l, inuse);
    if (err != APFSRW_OK)
        goto out;

    f = fopen(file, "wb");
    if (f == NULL) {
        err = APFSRW_EIO;
        goto out;
    }
    memset(&h, 0, sizeof(h));
    h.magic = APFSRW_SP_MAGIC;
    h.version = 1;
    h.block_size = fs->block_size;
    h.nblocks = l.n;
    h.block_count = fs->block_count;
    h.xid = fs->xid;
    memcpy(h.uuid, fs->nx.nx_uuid, 16);
    if (fwrite(&h, sizeof(h), 1, f) != 1) {
        err = APFSRW_EIO;
        goto out;
    }
    for (i = 0; i < l.n; i++) {
        err = read_raw(fs, (apfs_paddr_t)l.paddr[i], blk);
        if (err != APFSRW_OK)
            goto out;
        if (fwrite(&l.paddr[i], 8, 1, f) != 1 ||
            fwrite(blk, fs->block_size, 1, f) != 1) {
            err = APFSRW_EIO;
            goto out;
        }
    }
    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        err = APFSRW_EIO;
        goto out;
    }
    free(fs->frozen);
    fs->frozen = inuse;
    inuse = NULL;
    err = APFSRW_OK;
out:
    if (f != NULL)
        fclose(f);
    free(inuse);
    free(blk);
    free(l.paddr);
    return err;
}

void apfsrw_savepoint_release(struct apfsrw *fs)
{
    if (fs == NULL)
        return;
    free(fs->frozen);
    fs->frozen = NULL;
}

int apfsrw_rollback(struct apfsrw *fs, const char *file)
{
    struct sp_hdr h;
    uint8_t *blk = NULL;
    FILE *f;
    uint32_t i;
    int err = APFSRW_OK;

    if (fs == NULL || file == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    f = fopen(file, "rb");
    if (f == NULL)
        return APFSRW_ENOENT;
    if (fread(&h, sizeof(h), 1, f) != 1 || h.magic != APFSRW_SP_MAGIC ||
        h.version != 1 || h.block_size != fs->block_size ||
        h.block_count != fs->block_count ||
        memcmp(h.uuid, fs->nx.nx_uuid, 16) != 0) {
        fclose(f);
        return APFSRW_EINVAL;
    }
    blk = calloc(1, fs->block_size);
    if (blk == NULL) {
        fclose(f);
        return APFSRW_ENOMEM;
    }
    for (i = 0; i < h.nblocks; i++) {
        uint64_t paddr;

        if (fread(&paddr, 8, 1, f) != 1 ||
            fread(blk, fs->block_size, 1, f) != 1 ||
            paddr >= fs->block_count) {
            err = APFSRW_EIO;
            break;
        }
        err = write_raw(fs, (apfs_paddr_t)paddr, blk);
        if (err != APFSRW_OK)
            break;
    }
    fclose(f);
    free(blk);
    if (err != APFSRW_OK)
        return err;
    if (apfsrw_sync(fs) != 0)
        return APFSRW_EIO;
    free(fs->frozen);
    fs->frozen = NULL;
    fs->batch = 0;
    fs->alloced_count = 0;
    fs->deferred_count = 0;
    fs->alloc_delta = 0;
    fs->other_delta = 0;
    fs->data_cursor = 0;
    // Re-derive everything from what is now the newest checkpoint
    fs->fs_oid = 0;
    return load_volume(fs);
}

#endif /* !APFSRW_KERNEL */

// Ranged writes

struct ext_rec {
    uint64_t logical, phys, len;         // Bytes. len is a block multiple
};

struct collect_ctx {
    uint64_t stream_id;
    struct ext_rec *ext;
    uint32_t n, cap;
};

static int collect_extents_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct collect_ctx *c = (struct collect_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const struct apfs_j_file_extent_key *key;
        const struct apfs_j_file_extent_val *val;
        const void *keyp, *valp;
        uint16_t klen, vlen;
        int err;

        err = btree_entry(fs, node, info, i, &keyp, &klen, &valp, &vlen);
        if (err != APFSRW_OK)
            return err;
        if (klen < sizeof(*key) || vlen < sizeof(*val))
            continue;
        key = (const struct apfs_j_file_extent_key *)keyp;
        if (key_id(rd64(&key->hdr.obj_id_and_type)) != c->stream_id ||
            key_type(rd64(&key->hdr.obj_id_and_type)) != APFS_TYPE_FILE_EXTENT)
            continue;
        if (c->n == c->cap) {
            uint32_t ncap = c->cap ? c->cap * 2 : 16;
            struct ext_rec *ne = realloc(c->ext, ncap * sizeof(*ne));

            if (ne == NULL)
                return APFSRW_ENOMEM;
            c->ext = ne;
            c->cap = ncap;
        }
        val = (const struct apfs_j_file_extent_val *)valp;
        c->ext[c->n].logical = rd64(&key->logical_addr);
        c->ext[c->n].phys = rd64(&val->phys_block_num);
        c->ext[c->n].len = rd64(&val->len_and_flags) & APFS_FILE_EXTENT_LEN_MASK;
        c->n++;
    }
    return APFSRW_OK;
}

// Every extent of a stream, in logical order (the walk visits keys sorted)
static int collect_extents(struct apfsrw *fs, uint64_t stream_id,
    struct ext_rec **out, uint32_t *n_out)
{
    struct collect_ctx c;
    int err;

    memset(&c, 0, sizeof(c));
    c.stream_id = stream_id;
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, stream_id, stream_id,
        collect_extents_cb, &c);
    if (err != APFSRW_OK && err != 1) {
        free(c.ext);
        return err;
    }
    *out = c.ext;
    *n_out = c.n;
    return APFSRW_OK;
}

static int extent_put(struct apfsrw *fs, uint64_t stream_id, uint64_t logical,
    uint64_t phys, uint64_t len)
{
    uint8_t key[16], val[24];

    wr64(key, make_jkey(stream_id, APFS_TYPE_FILE_EXTENT));
    wr64(key + 8, logical);
    memset(val, 0, sizeof(val));
    wr64(val, len);
    wr64(val + 8, phys);
    return fstree_put(fs, key, 16, val, 24, 0);
}

static int extent_del(struct apfsrw *fs, uint64_t stream_id, uint64_t logical)
{
    uint8_t key[16];

    wr64(key, make_jkey(stream_id, APFS_TYPE_FILE_EXTENT));
    wr64(key + 8, logical);
    return fstree_del(fs, key, 16);
}

static int stream_cut(struct apfsrw *fs, uint64_t stream_id,
    const struct ext_rec *ext, uint32_t n, uint64_t b0, uint64_t b1,
    uint64_t *freed)
{
    uint64_t bs = fs->block_size;
    uint32_t i;
    int err;

    for (i = 0; i < n; i++) {
        uint64_t eb0 = ext[i].logical / bs;
        uint64_t eb1 = eb0 + ext[i].len / bs;
        uint64_t c0, c1, root = 0;

        if (eb1 <= b0 || eb0 >= b1)
            continue;
        c0 = eb0 > b0 ? eb0 : b0;           // Cut [c0, c1) of this extent
        c1 = eb1 < b1 ? eb1 : b1;

        err = extent_del(fs, stream_id, ext[i].logical);
        if (err != APFSRW_OK)
            return err;
        if (ext[i].phys != 0) {
            err = extentref_del(fs, ext[i].phys);
            if (err != APFSRW_OK)
                return err;
            err = defer_free(fs, ext[i].phys + (c0 - eb0), (uint32_t)(c1 - c0));
            if (err != APFSRW_OK)
                return err;
            *freed += c1 - c0;
        }
        if (c0 > eb0) {
            err = extent_put(fs, stream_id, ext[i].logical, ext[i].phys,
                (c0 - eb0) * bs);
            if (err != APFSRW_OK)
                return err;
            if (ext[i].phys != 0) {
                err = extentref_add(fs, ext[i].phys, (uint32_t)(c0 - eb0),
                    stream_id, &root);
                if (err != APFSRW_OK)
                    return err;
            }
        }
        if (eb1 > c1) {
            uint64_t rphys = ext[i].phys ? ext[i].phys + (c1 - eb0) : 0;

            err = extent_put(fs, stream_id, c1 * bs, rphys, (eb1 - c1) * bs);
            if (err != APFSRW_OK)
                return err;
            if (rphys != 0) {
                err = extentref_add(fs, rphys, (uint32_t)(eb1 - c1),
                    stream_id, &root);
                if (err != APFSRW_OK)
                    return err;
            }
        }
    }
    return APFSRW_OK;
}

static int drop_extents(struct apfsrw *fs, uint64_t stream_id,
    uint64_t *freed_blocks)
{
    struct ext_rec *ext = NULL;
    uint64_t freed = 0;
    uint32_t n = 0;
    int err;

    err = collect_extents(fs, stream_id, &ext, &n);
    if (err != APFSRW_OK)
        return err;
    err = stream_cut(fs, stream_id, ext, n, 0, ~0ull, &freed);
    free(ext);
    if (err == APFSRW_OK && freed_blocks != NULL)
        *freed_blocks = freed;
    return err;
}

static uint64_t extent_block_at(const struct ext_rec *ext, uint32_t n,
    uint64_t pos, uint64_t bs)
{
    uint32_t i;

    for (i = 0; i < n; i++) {
        if (pos >= ext[i].logical && pos < ext[i].logical + ext[i].len)
            return ext[i].phys ? ext[i].phys + (pos - ext[i].logical) / bs : 0;
    }
    return 0;
}

static int stream_inode(struct apfsrw *fs, const char *path, uint8_t *ival,
    uint16_t *ivlen, uint8_t *key, uint64_t *priv, uint64_t *size)
{
    uint64_t fileid = 0;
    uint8_t type = 0;
    int err;

    err = resolve_path(fs, path, &fileid, &type);
    if (err != APFSRW_OK)
        return err;
    if (type != APFSRW_DT_REG)
        return APFSRW_EINVAL;
    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_get(fs, key, 8, ival, 1024, ivlen);
    if (err != APFSRW_OK)
        return err;
    if (*ivlen < sizeof(struct apfs_j_inode_val))
        return APFSRW_EINVAL;
    *priv = rd64(ival + offsetof(struct apfs_j_inode_val, private_id));
    *size = 0;
    (void)inode_dstream_size(ival, *ivlen, size);
    return APFSRW_OK;
}

// Record how many bytes of the stream are holes, adding the xfield on first use
static int inode_set_sparse(uint8_t *val, uint16_t *val_len, uint64_t sparse)
{
    uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
    uint32_t num, i, desc, data;
    uint64_t flags;

    if (*val_len < fixed + 4U)
        return APFSRW_ENOENT;
    flags = rd64(val + offsetof(struct apfs_j_inode_val, internal_flags));
    if (sparse != 0)
        flags |= APFS_INODE_IS_SPARSE;
    num = rd16(val + fixed);
    desc = fixed + 4U;
    data = desc + num * 4U;
    if (data > *val_len)
        return APFSRW_EINVAL;
    for (i = 0; i < num; i++) {
        uint8_t type = val[desc + i * 4U];
        uint16_t fsize = rd16(val + desc + i * 4U + 2U);

        if (data + fsize > *val_len)
            return APFSRW_EINVAL;
        if (type == APFS_INO_EXT_TYPE_SPARSE_BYTES && fsize >= 8) {
            wr64(val + data, sparse);
            wr64(val + offsetof(struct apfs_j_inode_val, internal_flags), flags);
            return APFSRW_OK;
        }
        data += ((uint32_t)fsize + 7U) & ~7U;
    }
    if (sparse == 0)
        return APFSRW_OK;
    if (data + 12U > 1024U || data > *val_len)
        return APFSRW_EOVERFLOW;
    // The new descriptor goes after the others, so shift the field data by 4
    memmove(val + desc + num * 4U + 4U, val + desc + num * 4U,
        data - (desc + num * 4U));
    val[desc + num * 4U] = APFS_INO_EXT_TYPE_SPARSE_BYTES;
    val[desc + num * 4U + 1U] = 0x20;
    wr16(val + desc + num * 4U + 2U, 8);
    wr64(val + data + 4U, sparse);
    wr16(val + fixed, (uint16_t)(num + 1));
    wr16(val + fixed + 2, (uint16_t)(rd16(val + fixed + 2) + 8));
    wr64(val + offsetof(struct apfs_j_inode_val, internal_flags), flags);
    *val_len = (uint16_t)(data + 12U);
    return APFSRW_OK;
}

// fsck_apfs wants holes below EOF as explicit phys-0 extents,
// alloced_size as the sum of all extent lengths and sparse_bytes as the phys-0 part of it
static int stream_fill_holes(struct apfsrw *fs, uint64_t stream_id,
    uint64_t size, uint64_t *total_out, uint64_t *sparse_out)
{
    struct ext_rec *ext = NULL;
    uint64_t bs = fs->block_size;
    uint64_t span = (size + bs - 1) / bs * bs, pos = 0, total = 0, sparse = 0;
    uint32_t n = 0, i, j;
    int err;

    err = collect_extents(fs, stream_id, &ext, &n);
    if (err != APFSRW_OK)
        return err;
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && ext[j - 1].logical > ext[j].logical; j--) {
            struct ext_rec t = ext[j];

            ext[j] = ext[j - 1];
            ext[j - 1] = t;
        }
    for (i = 0; i <= n && err == APFSRW_OK; i++) {
        uint64_t next = i < n ? ext[i].logical : span;

        if (next > span)
            next = span;
        if (next > pos) {
            err = extent_put(fs, stream_id, pos, 0, next - pos);
            total += next - pos;
            sparse += next - pos;
        }
        if (i < n) {
            total += ext[i].len;
            if (ext[i].phys == 0)
                sparse += ext[i].len;
            if (ext[i].logical + ext[i].len > pos)
                pos = ext[i].logical + ext[i].len;
        }
    }
    free(ext);
    *total_out = total;
    *sparse_out = sparse;
    return err;
}

static int stream_finish(struct apfsrw *fs, uint8_t *key, uint8_t *ival,
    uint16_t ivlen, uint64_t priv, uint64_t size, uint64_t alloced_blocks,
    int64_t delta)
{
    uint8_t dkey[8], dval[4];
    uint16_t dvlen = 0;
    uint64_t now;
    int err;

    wr64(dkey, make_jkey(priv, APFS_TYPE_DSTREAM_ID));
    err = fstree_get(fs, dkey, 8, dval, sizeof(dval), &dvlen);
    if (err == APFSRW_ENOENT) {
        wr32(dval, 1);
        err = fstree_put(fs, dkey, 8, dval, 4, 0);
    }
    if (err != APFSRW_OK)
        return err;

    now = apfsrw_now_ns();
    wr64(ival + offsetof(struct apfs_j_inode_val, mod_time), now);
    wr64(ival + offsetof(struct apfs_j_inode_val, change_time), now);
    {
        uint64_t total = 0, sparse = 0;

        (void)alloced_blocks;
        err = stream_fill_holes(fs, priv, size, &total, &sparse);
        if (err == APFSRW_OK)
            err = inode_set_dstream(ival, ivlen, size, total);
        if (err == APFSRW_OK)
            err = inode_set_sparse(ival, &ivlen, sparse);
        if (err != APFSRW_OK)
            return err;
    }
    err = fstree_put(fs, key, 8, ival, ivlen, 1);
    if (err != APFSRW_OK)
        return err;
    fs->alloc_delta += delta;
    return txn_finish(fs, rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0);
}

static uint64_t extents_blocks(const struct ext_rec *ext, uint32_t n,
    uint64_t bs)
{
    uint64_t total = 0;
    uint32_t i;

    for (i = 0; i < n; i++)
        if (ext[i].phys != 0)
            total += ext[i].len / bs;
    return total;
}

int apfsrw_write_range(struct apfsrw *fs, const char *path, uint64_t off,
    const void *data, size_t len)
{
    struct ext_rec *ext = NULL;
    uint8_t *block = NULL;
    uint8_t ival[1024], key[8];
    uint16_t ivlen = 0;
    uint64_t priv = 0, old_size = 0, bs, b0, b1, nb, newp = 0, freed = 0;
    uint64_t i, alloced, new_size;
    uint32_t n = 0;
    int err;

    if (fs == NULL || path == NULL || (data == NULL && len != 0))
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (len == 0)
        return APFSRW_OK;
    if (off + len < off || off + len > 0xffffffffULL)
        return APFSRW_EOVERFLOW;

    err = stream_inode(fs, path, ival, &ivlen, key, &priv, &old_size);
    if (err != APFSRW_OK)
        return err;
    err = collect_extents(fs, priv, &ext, &n);
    if (err != APFSRW_OK)
        return err;

    bs = fs->block_size;
    b0 = off / bs;
    b1 = (off + len + bs - 1) / bs;
    nb = b1 - b0;
    // Assemble the new content of every touched block, then place it
    block = calloc(1, nb * bs);
    if (block == NULL) {
        err = APFSRW_ENOMEM;
        goto fail;
    }
    for (i = 0; i < nb; i++) {
        uint64_t lb = (b0 + i) * bs;             // This block's logical start
        uint64_t old = extent_block_at(ext, n, lb, bs);
        uint64_t lo = off > lb ? off - lb : 0;
        uint64_t hi = (off + len < lb + bs) ? off + len - lb : bs;
        uint8_t *dst = block + i * bs;

        if (old != 0 && lb < old_size) {
            err = read_raw(fs, (apfs_paddr_t)old, dst);
            if (err != APFSRW_OK)
                goto fail;
            if (old_size < lb + bs)
                memset(dst + (old_size - lb), 0, bs - (old_size - lb));
        }
        memcpy(dst + lo, (const uint8_t *)data + (lb + lo - off), hi - lo);
    }

    err = stream_cut(fs, priv, ext, n, b0, b1, &freed);
    if (err != APFSRW_OK)
        goto fail;
    {
        uint64_t hint = 0, blocks = 0;
        uint32_t k, pieces = 0;

        for (k = 0; k < n; k++)
            if (ext[k].phys != 0 && ext[k].logical + ext[k].len == b0 * bs)
                hint = ext[k].phys + ext[k].len / bs;
        err = stream_write_data(fs, priv, b0 * bs, block, nb * bs, hint,
            &blocks, &pieces, &newp);
        if (err != APFSRW_OK)
            goto fail;
        if (pieces != 1)
            goto finish;
    }
    {
        struct ext_rec *cur = NULL;
        uint64_t mlog = b0 * bs, mphys = newp, mlen = nb * bs, root = 0;
        uint32_t cn = 0, i;

        err = collect_extents(fs, priv, &cur, &cn);
        if (err != APFSRW_OK)
            goto fail;
        int merged = 0;

        for (i = 0; i < cn; i++) {
            const struct ext_rec *e = &cur[i];
            int left = e->phys != 0 && e->logical + e->len == mlog &&
                e->phys + e->len / bs == mphys;
            int right = e->phys != 0 && e->logical == mlog + mlen &&
                e->phys == mphys + mlen / bs;

            if (!left && !right)
                continue;
            err = extent_del(fs, priv, e->logical);
            if (err == APFSRW_OK)
                err = extentref_del(fs, e->phys);
            if (err != APFSRW_OK)
                break;
            if (left) {
                mlog = e->logical;
                mphys = e->phys;
            }
            mlen += e->len;
            merged = 1;
        }
        free(cur);
        if (err != APFSRW_OK)
            goto fail;
        if (merged) {
            // Replace the piece just written with the merged extent
            err = extent_del(fs, priv, b0 * bs);
            if (err == APFSRW_OK)
                err = extentref_del(fs, newp);
            if (err == APFSRW_OK)
                err = extent_put(fs, priv, mlog, mphys, mlen);
            if (err == APFSRW_OK)
                err = extentref_add(fs, mphys, (uint32_t)(mlen / bs), priv,
                    &root);
            if (err != APFSRW_OK)
                goto fail;
        }
    }
finish:

    new_size = off + len > old_size ? off + len : old_size;
    alloced = extents_blocks(ext, n, bs) - freed + nb;
    err = stream_finish(fs, key, ival, ivlen, priv, new_size, alloced,
        (int64_t)nb - (int64_t)freed);
    if (err != APFSRW_OK)
        goto fail;
    free(block);
    free(ext);
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    free(block);
    free(ext);
    return err;
}

int apfsrw_truncate(struct apfsrw *fs, const char *path, uint64_t size)
{
    struct ext_rec *ext = NULL;
    uint8_t ival[1024], key[8];
    uint16_t ivlen = 0;
    uint64_t priv = 0, old_size = 0, bs, freed = 0, alloced;
    uint32_t n = 0;
    int err;

    if (fs == NULL || path == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (size > 0xffffffffULL)
        return APFSRW_EOVERFLOW;

    err = stream_inode(fs, path, ival, &ivlen, key, &priv, &old_size);
    if (err != APFSRW_OK)
        return err;
    if (size == old_size)
        return APFSRW_OK;
    bs = fs->block_size;

    if (size < old_size) {
        uint64_t keep = (size + bs - 1) / bs;
        uint64_t old;

        err = collect_extents(fs, priv, &ext, &n);
        if (err != APFSRW_OK)
            return err;
        old = (size % bs) ? extent_block_at(ext, n, size, bs) : 0;
        err = stream_cut(fs, priv, ext, n, keep, ~0ull, &freed);
        if (err != APFSRW_OK)
            goto fail;
        if (old != 0) {
            // The last block stays. Rewrite it with its tail zeroed
            uint64_t lb = (size / bs) * bs, newp = 0, root = 0;
            uint8_t *block = calloc(1, bs);

            if (block == NULL) {
                err = APFSRW_ENOMEM;
                goto fail;
            }
            err = read_raw(fs, (apfs_paddr_t)old, block);
            if (err == APFSRW_OK) {
                memset(block + (size - lb), 0, bs - (size - lb));
                err = alloc_data_blocks(fs, 1, 0, &newp);
            }
            if (err == APFSRW_OK)
                err = write_raw(fs, (apfs_paddr_t)newp, block);
            free(block);
            if (err != APFSRW_OK)
                goto fail;
            free(ext);
            ext = NULL;
            err = collect_extents(fs, priv, &ext, &n);
            if (err != APFSRW_OK)
                goto fail;
            err = stream_cut(fs, priv, ext, n, size / bs, size / bs + 1,
                &freed);
            if (err != APFSRW_OK)
                goto fail;
            err = extent_put(fs, priv, lb, newp, bs);
            if (err != APFSRW_OK)
                goto fail;
            err = extentref_add(fs, newp, 1, priv, &root);
            if (err != APFSRW_OK)
                goto fail;
            freed -= 1;                  // One freed, one allocated
        }
        free(ext);
        ext = NULL;
    }
    err = collect_extents(fs, priv, &ext, &n);
    if (err != APFSRW_OK)
        goto fail;
    alloced = extents_blocks(ext, n, bs);
    err = stream_finish(fs, key, ival, ivlen, priv, size, alloced,
        -(int64_t)freed);
    if (err != APFSRW_OK)
        goto fail;
    free(ext);
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    free(ext);
    return err;
}

struct nodstream_ctx {
    uint64_t *ids;
    uint32_t n, cap;
};

static int nodstream_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct nodstream_ctx *c = (struct nodstream_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp, *valp;
        uint16_t klen, vlen;
        uint64_t key, size, priv;
        uint8_t dkey[8], dval[4];
        uint16_t dvlen = 0;
        int err = btree_entry(fs, node, info, i, &keyp, &klen, &valp, &vlen);

        if (err != APFSRW_OK)
            return err;
        if (klen < 8 || vlen < sizeof(struct apfs_j_inode_val))
            continue;
        key = rd64(keyp);
        if (key_type(key) != APFS_TYPE_INODE)
            continue;
        if ((rd16((const uint8_t *)valp +
            offsetof(struct apfs_j_inode_val, mode)) & 0170000) != 0100000)
            continue;
        if (inode_dstream_size(valp, vlen, &size) != APFSRW_OK)
            continue;
        priv = rd64((const uint8_t *)valp +
            offsetof(struct apfs_j_inode_val, private_id));
        wr64(dkey, make_jkey(priv, APFS_TYPE_DSTREAM_ID));
        if (fstree_get(fs, dkey, 8, dval, sizeof(dval), &dvlen) !=
            APFSRW_ENOENT)
            continue;
        if (c->n == c->cap) {
            uint32_t ncap = c->cap ? c->cap * 2 : 32;
            uint64_t *ni = realloc(c->ids, ncap * sizeof(*ni));

            if (ni == NULL)
                return APFSRW_ENOMEM;
            c->ids = ni;
            c->cap = ncap;
        }
        c->ids[c->n++] = priv;
    }
    return APFSRW_OK;
}

// Bring a volume made by mkapfs and an older apfsrw up to what fsck_apfs expects: INODE_NO_RSRC_FORK
// on the root and private-dir inodes, a DSTREAM_ID record per regular file. Idempotent
int apfsrw_fixup_mkapfs(struct apfsrw *fs)
{
    static const uint64_t ids[2] = { 2, 3 };
    struct nodstream_ctx nc;
    uint8_t key[8], val[1024];
    uint16_t vlen = 0;
    uint32_t j;
    int changed = 0, i, err;

    if (fs == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (fs->batch)
        return APFSRW_EINVAL;

    memset(&nc, 0, sizeof(nc));
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, 0, ~0ull,
        nodstream_cb, &nc);
    if (err != APFSRW_OK && err != 1) {
        free(nc.ids);
        return err;
    }
    for (j = 0; j < nc.n; j++) {
        uint8_t dval[4];

        wr64(key, make_jkey(nc.ids[j], APFS_TYPE_DSTREAM_ID));
        wr32(dval, 1);
        err = fstree_put(fs, key, 8, dval, 4, 0);
        if (err != APFSRW_OK) {
            free(nc.ids);
            goto fail;
        }
        changed = 1;
    }
    free(nc.ids);

    for (i = 0; i < 2; i++) {
        uint64_t flags;

        wr64(key, make_jkey(ids[i], APFS_TYPE_INODE));
        err = fstree_get(fs, key, 8, val, sizeof(val), &vlen);
        if (err == APFSRW_ENOENT)
            continue;
        if (err != APFSRW_OK)
            goto fail;
        flags = rd64(val + offsetof(struct apfs_j_inode_val, internal_flags));
        // Mkapfs also leaves mode 0: as a non-root mount the vnode is then VNON
        // and namei panics crossing into it ("Root of filesystem not a directory")
        if ((flags & APFS_INODE_NO_RSRC_FORK) &&
            (rd16(val + offsetof(struct apfs_j_inode_val, mode)) & 0170000))
            continue;
        wr64(val + offsetof(struct apfs_j_inode_val, internal_flags),
            flags | APFS_INODE_NO_RSRC_FORK);
        if ((rd16(val + offsetof(struct apfs_j_inode_val, mode)) & 0170000) == 0)
            wr16(val + offsetof(struct apfs_j_inode_val, mode), 040755);
        err = fstree_put(fs, key, 8, val, vlen, 1);
        if (err != APFSRW_OK)
            goto fail;
        changed = 1;
    }
    if (!changed)
        return APFSRW_OK;
    err = cow_commit(fs, fs->root_tree_paddr,
        rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0, fs->extref_paddr, 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    return err;
}

int apfsrw_set_volume_role(struct apfsrw *fs, uint16_t role)
{
    int err;

    if (fs == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (fs->batch)
        return APFSRW_EINVAL;
    fs->role_pending = role;
    err = cow_commit(fs, fs->root_tree_paddr,
        rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0, fs->extref_paddr, 0);
    fs->role_pending = -1;
    if (err != APFSRW_OK)
        txn_rollback(fs);
    return err;
}

// Fields of apfs_superblock_t past our struct (spec p.51-52)
#define APSB_FORMATTED_BY_OFF 272
#define APSB_MODIFIED_BY_OFF  320
#define APSB_VOLNAME_OFF      704
#define APSB_NEXT_DOC_ID_OFF  960
#define APSB_ROLE_OFF         964
#define APSB_ROOT_TO_XID_OFF  968

int apfsrw_list_volumes(struct apfsrw *fs, struct apfsrw_volume_entry *out,
    uint32_t cap, uint32_t *count)
{
    uint8_t *block;
    uint32_t max_fs, i, n = 0;

    if (fs == NULL || count == NULL)
        return APFSRW_EINVAL;
    block = calloc(1, fs->block_size);
    if (block == NULL)
        return APFSRW_ENOMEM;
    max_fs = rd32(&fs->nx.nx_max_file_systems);
    if (max_fs > APFS_NX_MAX_FILE_SYSTEMS)
        max_fs = APFS_NX_MAX_FILE_SYSTEMS;
    for (i = 0; i < max_fs; i++) {
        apfs_oid_t oid = rd64(&fs->nx.nx_fs_oid[i]);
        struct apfs_omap_val ov;
        struct apfsrw_volume_entry *e;

        if (oid == 0)
            continue;
        if (omap_lookup_tree(fs, fs->container_omap_tree_paddr, oid,
            fs->xid, &ov) != APFSRW_OK)
            continue;
        if (read_object(fs, (apfs_paddr_t)rd64(&ov.ov_paddr), block) !=
            APFSRW_OK)
            continue;
        if (rd32(block + offsetof(struct apfs_superblock, apfs_magic)) !=
            APFS_APSB_MAGIC)
            continue;
        if (out != NULL && n < cap) {
            e = &out[n];
            memset(e, 0, sizeof(*e));
            e->slot = i;
            e->fs_oid = oid;
            e->role = rd16(block + APSB_ROLE_OFF);
            memcpy(e->uuid,
                block + offsetof(struct apfs_superblock, apfs_vol_uuid), 16);
            memcpy(e->name, block + APSB_VOLNAME_OFF, sizeof(e->name) - 1);
            e->num_files = rd64(block +
                offsetof(struct apfs_superblock, apfs_num_files));
            e->num_directories = rd64(block +
                offsetof(struct apfs_superblock, apfs_num_directories));
        }
        n++;
    }
    free(block);
    *count = n;
    return APFSRW_OK;
}

static int write_root_from(struct apfsrw *fs,
    const struct apfs_btree_node_phys *tmpl, apfs_paddr_t paddr, apfs_oid_t oid,
    struct rw_rec *recs, uint32_t count)
{
    struct apfs_btree_info info;
    uint8_t *node;
    int err;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    if ((rd16(&tmpl->btn_flags) & APFS_BTNODE_ROOT) == 0) {
        err = APFSRW_EINVAL;
        goto out;
    }
    memcpy(&info, (uint8_t *)tmpl + fs->block_size - sizeof(info),
        sizeof(info));
    if ((rd16(&tmpl->btn_flags) & APFS_BTNODE_FIXED_KV_SIZE) == 0) {
        wr32(&info.bt_longest_key, 0);
        wr32(&info.bt_longest_val, 0);
    }
    err = build_node(fs, tmpl, 1, 0, &info, recs, count, node);
    if (err != APFSRW_OK)
        goto out;
    wr64(node + offsetof(struct apfs_obj_phys, o_oid), oid);
    wr64(node + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    seal_object(fs, node);
    err = write_block(fs, paddr, node);
out:
    free(node);
    return err;
}

static int write_root_like(struct apfsrw *fs, apfs_paddr_t tmpl_paddr,
    apfs_paddr_t paddr, apfs_oid_t oid, struct rw_rec *recs, uint32_t count)
{
    struct apfs_btree_node_phys *tmpl = calloc(1, fs->block_size);
    int err;

    if (tmpl == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, tmpl_paddr, tmpl);
    if (err == APFSRW_OK)
        err = write_root_from(fs, tmpl, paddr, oid, recs, count);
    free(tmpl);
    return err;
}

// An empty root as newfs_apfs lays it out, for a container with no volume to copy one from
static void synth_root(struct apfsrw *fs, uint8_t *b, uint32_t type,
    uint32_t subtype, uint16_t btn_flags, uint32_t bt_flags, uint32_t ksz,
    uint32_t vsz)
{
    uint8_t *info = b + fs->block_size - sizeof(struct apfs_btree_info);

    memset(b, 0, fs->block_size);
    wr32(b + offsetof(struct apfs_obj_phys, o_type), type);
    wr32(b + offsetof(struct apfs_obj_phys, o_subtype), subtype);
    wr16(b + offsetof(struct apfs_btree_node_phys, btn_flags), btn_flags);
    wr32(info + offsetof(struct apfs_btree_info, bt_flags), bt_flags);
    wr32(info + offsetof(struct apfs_btree_info, bt_node_size), fs->block_size);
    wr32(info + offsetof(struct apfs_btree_info, bt_key_size), ksz);
    wr32(info + offsetof(struct apfs_btree_info, bt_val_size), vsz);
    wr32(info + offsetof(struct apfs_btree_info, bt_longest_key), ksz);
    wr32(info + offsetof(struct apfs_btree_info, bt_longest_val), vsz);
    wr64(info + offsetof(struct apfs_btree_info, bt_node_count), 1);
}

// Commits a new volume into a container opened without one (APFSRW_SLOT_CONTAINER):
// the container object map gains the volume, the checkpoint gains its nx_fs_oid slot
static int container_commit(struct apfsrw *fs)
{
    uint8_t *block = NULL;
    uint64_t comap = 0;
    apfs_paddr_t ctree = 0;
    int err;

    // the insert copies the committed root and frees it
    err = omap_insert(fs, fs->container_omap_tree_paddr, fs->newvol_oid,
        fs->newvol_paddr, &ctree);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &comap);
    if (err != APFSRW_OK)
        return err;
    block = calloc(1, fs->block_size);
    if (block == NULL)
        return APFSRW_ENOMEM;
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
    err = ac_flush(fs);
    if (err != APFSRW_OK)
        goto out;
    if (apfsrw_sync(fs) != 0) {
        err = APFSRW_EIO;
        goto out;
    }
    defer_free(fs, (uint64_t)fs->container_omap_paddr, 1);
    err = publish_checkpoint(fs, (apfs_paddr_t)comap);
    if (err == APFSRW_OK && apfsrw_sync(fs) != 0)
        err = APFSRW_EIO;
    if (err == APFSRW_OK) {
        flush_deferred(fs);
        fs->alloced_count = 0;
        fs->alloc_delta = 0;
        fs->other_delta = 0;
        fs->newvol_pending = 0;
        (void)apfsrw_sync_nowait(fs);
        fs->xid += 1;
        fs->container_omap_paddr = (apfs_paddr_t)comap;
        fs->container_omap_tree_paddr = ctree;
        if (read_object(fs, fs->nx_paddr, block) == APFSRW_OK)
            memcpy(&fs->nx, block, sizeof(fs->nx));
    }
out:
    free(block);
    return err;
}

static int put_special_dir(struct apfsrw *fs, uint64_t id, const char *name,
    uint64_t now)
{
    uint8_t key[8 + 4 + 256], val[1024];
    uint16_t namelen = (uint16_t)strlen(name);
    uint16_t nsz = (uint16_t)(namelen + 1);
    uint16_t npad = (uint16_t)((nsz + 7u) & ~7u);
    uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
    uint32_t hash;
    int err;

    wr64(key, make_jkey(id, APFS_TYPE_INODE));
    memset(val, 0, sizeof(val));
    wr64(val + offsetof(struct apfs_j_inode_val, parent_id), 1);
    wr64(val + offsetof(struct apfs_j_inode_val, private_id), id);
    wr64(val + offsetof(struct apfs_j_inode_val, create_time), now);
    wr64(val + offsetof(struct apfs_j_inode_val, mod_time), now);
    wr64(val + offsetof(struct apfs_j_inode_val, change_time), now);
    wr64(val + offsetof(struct apfs_j_inode_val, access_time), now);
    wr64(val + offsetof(struct apfs_j_inode_val, internal_flags),
        APFS_INODE_NO_RSRC_FORK);
    wr16(val + offsetof(struct apfs_j_inode_val, mode), 040755);
    wr16(val + fixed, 1);
    wr16(val + fixed + 2, npad);
    val[fixed + 4] = APFS_INO_EXT_TYPE_NAME;
    val[fixed + 5] = 0x02;
    wr16(val + fixed + 6, nsz);
    memcpy(val + fixed + 8, name, nsz);
    err = fstree_put(fs, key, 8, val, (uint16_t)(fixed + 8 + npad), 0);
    if (err != APFSRW_OK)
        return err;

    err = drec_name_hash(fs, name, namelen, &hash);
    if (err != APFSRW_OK)
        return err;
    wr64(key, make_jkey(1, APFS_TYPE_DIR_REC));
    wr32(key + 8, ((hash << 10) & 0xfffffc00U) | (nsz & 0x3ffU));
    memcpy(key + 12, name, nsz);
    memset(val, 0, 18);
    wr64(val, id);
    wr64(val + 8, now);
    wr16(val + 16, APFSRW_DT_DIR);
    return fstree_put(fs, key, (uint16_t)(12 + nsz), val, 18, 0);
}

int apfsrw_create_volume(struct apfsrw *fs, const char *name, uint16_t role,
    const uint8_t uuid[16], uint32_t *slot_out)
{
    uint8_t *block = NULL;
    uint64_t vomap = 0, vtree = 0, froot = 0, extref = 0, snap = 0, vsb = 0;
    uint64_t now = apfsrw_now_ns();
    apfs_oid_t vsb_oid, root_oid;
    apfs_paddr_t extref_tmpl;
    struct rw_rec rec;
    uint8_t rkey[16], rval[16];
    uint32_t max_fs, slot, saved_slot;
    size_t namelen;
    int fresh, err;

    if (fs == NULL || name == NULL || uuid == NULL)
        return APFSRW_EINVAL;
    namelen = strlen(name);
    if (namelen == 0 || namelen > 255)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;
    if (fs->batch)
        return APFSRW_EINVAL;

    max_fs = rd32(&fs->nx.nx_max_file_systems);
    if (max_fs > APFS_NX_MAX_FILE_SYSTEMS)
        max_fs = APFS_NX_MAX_FILE_SYSTEMS;
    for (slot = 0; slot < max_fs; slot++)
        if (rd64(&fs->nx.nx_fs_oid[slot]) == 0)
            break;
    if (slot == max_fs)
        return APFSRW_ENOSPC;
    // no volume loaded: the trees, object map and superblock are made, not copied
    fresh = fs->fs_oid == 0;

    err = alloc_blocks(fs, 1, &vomap);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &vtree);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &froot);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &extref);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &snap);
    if (err == APFSRW_OK)
        err = alloc_blocks(fs, 1, &vsb);
    if (err != APFSRW_OK)
        goto fail;
    vsb_oid = alloc_oid(fs);
    root_oid = alloc_oid(fs);

    block = calloc(1, fs->block_size);
    if (block == NULL) {
        err = APFSRW_ENOMEM;
        goto fail;
    }
    if (fresh) {
        // header and btree_info values as newfs_apfs writes them
        synth_root(fs, block, APFS_OBJECT_TYPE_BTREE, APFS_OBJECT_TYPE_FSTREE,
            APFS_BTNODE_ROOT | APFS_BTNODE_LEAF, 0x42, 0, 0);
        err = write_root_from(fs, (struct apfs_btree_node_phys *)block,
            (apfs_paddr_t)froot, root_oid, NULL, 0);
        if (err != APFSRW_OK)
            goto fail;
        synth_root(fs, block, APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE, 0x0f,
            APFS_BTNODE_ROOT | APFS_BTNODE_LEAF, 0x52, 0, 0);
        err = write_root_from(fs, (struct apfs_btree_node_phys *)block,
            (apfs_paddr_t)extref, extref, NULL, 0);
        if (err != APFSRW_OK)
            goto fail;
        synth_root(fs, block, APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE, 0x10,
            APFS_BTNODE_ROOT | APFS_BTNODE_LEAF, 0x52, 0, 0);
        err = write_root_from(fs, (struct apfs_btree_node_phys *)block,
            (apfs_paddr_t)snap, snap, NULL, 0);
        if (err != APFSRW_OK)
            goto fail;
    } else {
        err = write_root_like(fs, fs->root_tree_paddr, (apfs_paddr_t)froot,
            root_oid, NULL, 0);
        if (err != APFSRW_OK)
            goto fail;
        extref_tmpl = fs->extref_paddr != 0 ? fs->extref_paddr :
            (apfs_paddr_t)rd64(&fs->apfs.apfs_extentref_tree_oid);
        err = write_root_like(fs, extref_tmpl, (apfs_paddr_t)extref, extref,
            NULL, 0);
        if (err != APFSRW_OK)
            goto fail;
        err = write_root_like(fs,
            (apfs_paddr_t)rd64(&fs->apfs.apfs_snap_meta_tree_oid),
            (apfs_paddr_t)snap, snap, NULL, 0);
        if (err != APFSRW_OK)
            goto fail;
    }

    // Volume object map with its one entry: the root tree
    memset(rkey, 0, sizeof(rkey));
    wr64(rkey, root_oid);
    wr64(rkey + 8, fs->xid + 1);
    memset(rval, 0, sizeof(rval));
    wr32(rval + 4, fs->block_size);
    wr64(rval + 8, froot);
    rec.key = rkey;
    rec.val = rval;
    rec.klen = 16;
    rec.vlen = 16;
    if (fresh) {
        synth_root(fs, block, APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE,
            APFS_OBJECT_TYPE_OMAP, APFS_BTNODE_ROOT | APFS_BTNODE_LEAF |
            APFS_BTNODE_FIXED_KV_SIZE, 0x12, 16, 16);
        err = write_root_from(fs, (struct apfs_btree_node_phys *)block,
            (apfs_paddr_t)vtree, vtree, &rec, 1);
        if (err != APFSRW_OK)
            goto fail;
        memset(block, 0, fs->block_size);
        wr32(block + offsetof(struct apfs_obj_phys, o_type),
            APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_OMAP);
        wr32(block + offsetof(struct apfs_omap_phys, om_tree_type),
            APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE);
        wr32(block + offsetof(struct apfs_omap_phys, om_snapshot_tree_type),
            APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE);
    } else {
        err = write_root_like(fs, fs->volume_omap_tree_paddr,
            (apfs_paddr_t)vtree, vtree, &rec, 1);
        if (err != APFSRW_OK)
            goto fail;
        err = read_object(fs, fs->volume_omap_paddr, block);
        if (err != APFSRW_OK)
            goto fail;
    }
    wr64(block + offsetof(struct apfs_obj_phys, o_oid), vomap);
    wr64(block + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    wr32(block + offsetof(struct apfs_omap_phys, om_snap_count), 0);
    wr64(block + offsetof(struct apfs_omap_phys, om_tree_oid), vtree);
    wr64(block + offsetof(struct apfs_omap_phys, om_snapshot_tree_oid), 0);
    wr64(block + offsetof(struct apfs_omap_phys, om_most_recent_snap), 0);
    wr64(block + offsetof(struct apfs_omap_phys, om_pending_revert_min), 0);
    wr64(block + offsetof(struct apfs_omap_phys, om_pending_revert_max), 0);
    seal_object(fs, block);
    err = write_block(fs, (apfs_paddr_t)vomap, block);
    if (err != APFSRW_OK)
        goto fail;

    // Volume superblock: this volume's, with everything volume-specific replaced.
    // The five blocks above are what apfs_fs_alloc_count counts
    if (fresh) {
        // case-insensitive and unencrypted, the fields newfs_apfs sets that nothing below replaces
        memset(block, 0, fs->block_size);
        wr32(block + offsetof(struct apfs_obj_phys, o_type), APFS_OBJECT_TYPE_FS);
        wr32(block + offsetof(struct apfs_superblock, apfs_magic), APFS_APSB_MAGIC);
        wr64(block + offsetof(struct apfs_superblock, apfs_features), 0x2);
        wr64(block + offsetof(struct apfs_superblock, apfs_incompatible_features), 0x1);
        wr16(block + offsetof(struct apfs_superblock, apfs_meta_crypto) +
            offsetof(struct apfs_crypto_state, major_version), 5);
        wr32(block + offsetof(struct apfs_superblock, apfs_meta_crypto) +
            offsetof(struct apfs_crypto_state, persistent_class), 6);
        wr16(block + offsetof(struct apfs_superblock, apfs_meta_crypto) +
            offsetof(struct apfs_crypto_state, key_revision), 1);
        wr32(block + offsetof(struct apfs_superblock, apfs_root_tree_type),
            APFS_OBJECT_TYPE_BTREE);
        wr32(block + offsetof(struct apfs_superblock, apfs_extentref_tree_type),
            APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE);
        wr32(block + offsetof(struct apfs_superblock, apfs_snap_meta_tree_type),
            APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE);
        wr64(block + offsetof(struct apfs_superblock, apfs_fs_flags), 0x1);
    } else {
        err = read_object(fs, fs->fs_paddr, block);
        if (err != APFSRW_OK)
            goto fail;
    }
    wr64(block + offsetof(struct apfs_obj_phys, o_oid), vsb_oid);
    wr64(block + offsetof(struct apfs_obj_phys, o_xid), fs->xid + 1);
    wr32(block + offsetof(struct apfs_superblock, apfs_fs_index), slot);
    wr64(block + offsetof(struct apfs_superblock, apfs_unmount_time), 0);
    wr64(block + offsetof(struct apfs_superblock, apfs_fs_reserve_block_count), 0);
    wr64(block + offsetof(struct apfs_superblock, apfs_fs_quota_block_count), 0);
    wr64(block + offsetof(struct apfs_superblock, apfs_fs_alloc_count), 5);
    wr64(block + offsetof(struct apfs_superblock, apfs_omap_oid), vomap);
    wr64(block + offsetof(struct apfs_superblock, apfs_root_tree_oid), root_oid);
    wr64(block + offsetof(struct apfs_superblock, apfs_extentref_tree_oid), extref);
    wr64(block + offsetof(struct apfs_superblock, apfs_snap_meta_tree_oid), snap);
    wr64(block + offsetof(struct apfs_superblock, apfs_revert_to_xid), 0);
    wr64(block + offsetof(struct apfs_superblock, apfs_revert_to_sblock_oid), 0);
    wr64(block + offsetof(struct apfs_superblock, apfs_next_obj_id), 16);
    memset(block + offsetof(struct apfs_superblock, apfs_num_files), 0, 7 * 8);
    memcpy(block + offsetof(struct apfs_superblock, apfs_vol_uuid), uuid, 16);
    wr64(block + offsetof(struct apfs_superblock, apfs_last_mod_time), now);
    memset(block + APSB_FORMATTED_BY_OFF, 0, APSB_VOLNAME_OFF - APSB_FORMATTED_BY_OFF);
    memcpy(block + APSB_FORMATTED_BY_OFF, "PureDarwin libapfsrw", 20);
    wr64(block + APSB_FORMATTED_BY_OFF + 32, now);
    wr64(block + APSB_FORMATTED_BY_OFF + 40, fs->xid + 1);
    memset(block + APSB_VOLNAME_OFF, 0, 256);
    memcpy(block + APSB_VOLNAME_OFF, name, namelen);
    wr32(block + APSB_NEXT_DOC_ID_OFF, 3);
    wr16(block + APSB_ROLE_OFF, role);
    wr16(block + APSB_ROLE_OFF + 2, 0);
    memset(block + APSB_ROOT_TO_XID_OFF, 0, fs->block_size - APSB_ROOT_TO_XID_OFF);
    seal_object(fs, block);
    err = write_block(fs, (apfs_paddr_t)vsb, block);
    if (err != APFSRW_OK)
        goto fail;

    fs->newvol_pending = 1;
    fs->newvol_slot = slot;
    fs->newvol_oid = vsb_oid;
    fs->newvol_paddr = (apfs_paddr_t)vsb;
    if (fresh)
        err = container_commit(fs);
    else
        err = cow_commit(fs, fs->root_tree_paddr,
            rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0, fs->extref_paddr, 0);
    if (err != APFSRW_OK)
        goto fail;
    free(block);
    block = NULL;

    saved_slot = fs->vol_slot;
    fs->vol_slot = slot;
    err = apfsrw_refresh(fs);
    if (err == APFSRW_OK)
        err = put_special_dir(fs, APFSRW_ROOT_FILEID, "root", now);
    if (err == APFSRW_OK)
        err = put_special_dir(fs, APFSRW_ROOT_FILEID + 1, "private-dir", now);
    if (err == APFSRW_OK)
        err = cow_commit(fs, fs->root_tree_paddr, 16, 0, 0, 0, 0, 0);
    if (err != APFSRW_OK)
        txn_rollback(fs);
    fs->vol_slot = saved_slot;
    if (apfsrw_refresh(fs) != APFSRW_OK && err == APFSRW_OK)
        err = APFSRW_EIO;
    if (err == APFSRW_OK && slot_out != NULL)
        *slot_out = slot;
    return err;

fail:
    fs->newvol_pending = 0;
    txn_rollback(fs);
    free(block);
    return err;
}

#ifndef APFSRW_KERNEL
#define APFS_OBJECT_TYPE_NX_SUPERBLOCK 0x00000001U
#define APFS_OBJECT_TYPE_NX_REAPER 0x00000011U
#define APFS_NX_INCOMPAT_VERSION2 0x2ULL
#define APFS_OMAP_MANUALLY_MANAGED 0x1U
#define APFS_CHECKPOINT_MAP_LAST 0x1U
#define APFS_SM_FLAG_VERSIONED 0x1U
#define APFS_NR_BHM_FLAG 0x1U
#define APFS_OID_SPACEMAN 0x400U
#define APFS_OID_REAPER 0x401U
#define APFS_IP_BM_BLOCKS 16U               // SPACEMAN_IP_BM_TX_MULTIPLIER bitmap blocks
#define APFS_NX_EPH_INFO_OFF 0x520U         // nx_ephemeral_info[0] (spec p.27)
#define APFS_SM_VERSION_OFF 0x150U          // sm_version, then sm_struct_size
#define APFS_SM_STRUCT_SIZE 0x9d8U
#define APFS_SM_FQ_SIZE 40U                 // spaceman_free_queue_t

// checkpoint areas and free-queue node limits newfs_apfs (macOS 26.6) chose at these sizes.
// between rows the smaller row applies
static const struct mkc_row {
    uint64_t blocks;
    uint32_t desc, data;
    uint16_t fq_ip, fq_main;
} mkc_rows[] = {
    { 8192, 8, 124, 1, 3 },
    { 32768, 8, 304, 1, 8 },
    { 65536, 8, 332, 1, 15 },
    { 131072, 8, 388, 1, 29 },
    { 262144, 16, 992, 1, 116 },
    { 524288, 24, 1964, 1, 231 },
    { 1048576, 48, 4112, 1, 512 },
    { 2097152, 68, 6160, 1, 512 },
    { 4194304, 108, 10256, 1, 512 },
    { 10485760, 228, 22544, 1, 512 },
    { 26214400, 280, 27672, 3, 512 },
    { 52428800, 280, 27680, 5, 512 },
};

static int mkc_open(const char *path, uint64_t block_count, struct apfsrw **out)
{
    const char *at = strstr(path, "@@");
    size_t n = at != NULL ? (size_t)(at - path) : strlen(path);
    struct apfsrw *fs = calloc(1, sizeof(*fs));
    char *name = malloc(n + 1);
    struct stat st;
    uint64_t end;

    if (fs == NULL || name == NULL) {
        free(fs);
        free(name);
        return APFSRW_ENOMEM;
    }
    memcpy(name, path, n);
    name[n] = '\0';
    fs->base_off = at != NULL ? (off_t)strtoull(at + 2, NULL, 0) : 0;
    fs->fd = open(name, O_RDWR | O_CREAT, 0644);
    free(name);
    fs->writable = 1;
    fs->role_pending = -1;
    fs->block_size = APFSRW_BLOCK_SIZE;
    fs->block_count = block_count;
    end = (uint64_t)fs->base_off + block_count * APFSRW_BLOCK_SIZE;
    // a regular file grows (sparse) to hold the container, a device must already be big enough
    if (fs->fd < 0 || fstat(fs->fd, &st) != 0 ||
        (S_ISREG(st.st_mode) && (uint64_t)st.st_size < end &&
        ftruncate(fs->fd, (off_t)end) != 0)) {
        apfsrw_close(fs);
        return APFSRW_EIO;
    }
    fs->image_blocks = end / APFSRW_BLOCK_SIZE;
    *out = fs;
    return APFSRW_OK;
}

static void mkc_header(uint8_t *b, uint64_t oid, uint32_t type)
{
    memset(b, 0, APFSRW_BLOCK_SIZE);
    wr64(b + offsetof(struct apfs_obj_phys, o_oid), oid);
    wr64(b + offsetof(struct apfs_obj_phys, o_xid), 1);
    wr32(b + offsetof(struct apfs_obj_phys, o_type), type);
}

static void mkc_setbits(uint8_t *bm, uint64_t first, uint64_t n)
{
    uint64_t k;

    for (k = first; k < first + n; k++)
        bm[k >> 3] |= (uint8_t)(1U << (k & 7));
}

int apfsrw_mkcontainer(const char *path, uint64_t block_count,
    const uint8_t uuid[16])
{
    const uint32_t bs = APFSRW_BLOCK_SIZE, bpc = APFSRW_BLOCK_SIZE * 8;
    const uint32_t cpcib = (uint32_t)((bs - sizeof(struct apfs_chunk_info_block)) /
        sizeof(struct apfs_chunk_info));
    const uint32_t addr_off = APFS_SM_STRUCT_SIZE + 8 + 8 + 2 * APFS_IP_BM_BLOCKS;
    const struct mkc_row *row = NULL;
    struct apfsrw *fs = NULL;
    uint64_t chunks, cibs, ip_blocks, used_chunks, used, c;
    uint64_t data_base, ipbm_base, ip_base, omap, max_fs;
    uint8_t *b = NULL;
    size_t i;
    int err;

    if (path == NULL || uuid == NULL)
        return APFSRW_EINVAL;
    for (i = 0; i < sizeof(mkc_rows) / sizeof(mkc_rows[0]); i++)
        if (block_count >= mkc_rows[i].blocks)
            row = &mkc_rows[i];
    if (row == NULL)
        return APFSRW_EINVAL;

    chunks = (block_count + bpc - 1) / bpc;
    cibs = (chunks + cpcib - 1) / cpcib;
    ip_blocks = 3 * (chunks + cibs);
    data_base = 1 + row->desc;
    ipbm_base = data_base + row->data;
    ip_base = ipbm_base + APFS_IP_BM_BLOCKS;
    omap = ip_base + ip_blocks;
    used = omap + 2;
    used_chunks = (used + bpc - 1) / bpc;
    max_fs = (block_count + 131071) / 131072;
    if (max_fs > APFS_NX_MAX_FILE_SYSTEMS)
        max_fs = APFS_NX_MAX_FILE_SYSTEMS;
    // one internal-pool bitmap block, and every CIB address inline in the space manager (no CABs)
    if (ip_blocks > bpc || addr_off + cibs * 8 > bs ||
        cibs + used_chunks > ip_blocks)
        return APFSRW_ENOTSUP;

    err = mkc_open(path, block_count, &fs);
    if (err != APFSRW_OK)
        return err;
    b = calloc(1, bs);
    if (b == NULL) {
        err = APFSRW_ENOMEM;
        goto out;
    }

    // stale superblocks in the descriptor ring would outrank ours at mount, so clear it first
    err = write_raw(fs, 0, b);
    for (c = 1; err == APFSRW_OK && c < data_base; c++)
        err = write_raw(fs, (apfs_paddr_t)c, b);
    for (c = 1; err == APFSRW_OK && c < APFS_IP_BM_BLOCKS; c++)
        err = write_raw(fs, (apfs_paddr_t)(ipbm_base + c), b);
    if (err != APFSRW_OK)
        goto out;

    // internal pool: the CIBs, then a bitmap for each chunk the metadata above reaches into
    for (c = 0; c < cibs; c++) {
        struct apfs_chunk_info_block *cib = (struct apfs_chunk_info_block *)b;
        uint64_t first = c * cpcib, k;
        uint32_t count = (uint32_t)(chunks - first < cpcib ? chunks - first : cpcib);

        mkc_header(b, ip_base + c, APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_SPACEMAN_CIB);
        wr32(&cib->cib_index, (uint32_t)c);
        wr32(&cib->cib_chunk_info_count, count);
        for (k = 0; k < count; k++) {
            struct apfs_chunk_info *ci = &cib->cib_chunk_info[k];
            uint64_t addr = (first + k) * bpc;
            uint64_t nblk = block_count - addr < bpc ? block_count - addr : bpc;
            uint64_t inuse = used > addr ? (used - addr < nblk ? used - addr : nblk) : 0;

            wr64(&ci->ci_xid, 1);
            wr64(&ci->ci_addr, addr);
            wr32(&ci->ci_block_count, (uint32_t)nblk);
            wr32(&ci->ci_free_count, (uint32_t)(nblk - inuse));
            if (first + k < used_chunks)
                wr64(&ci->ci_bitmap_addr, ip_base + cibs + first + k);
        }
        seal_object(fs, b);
        err = write_block(fs, (apfs_paddr_t)(ip_base + c), b);
        if (err != APFSRW_OK)
            goto out;
    }
    for (c = 0; c < used_chunks; c++) {
        uint64_t addr = c * bpc;

        memset(b, 0, bs);
        mkc_setbits(b, 0, used - addr < bpc ? used - addr : bpc);
        err = write_raw(fs, (apfs_paddr_t)(ip_base + cibs + c), b);
        if (err != APFSRW_OK)
            goto out;
    }
    memset(b, 0, bs);
    mkc_setbits(b, 0, cibs + used_chunks);
    err = write_raw(fs, (apfs_paddr_t)ipbm_base, b);
    if (err != APFSRW_OK)
        goto out;

    // container object map: flags and an empty tree as newfs_apfs writes them
    synth_root(fs, b, APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE, APFS_OBJECT_TYPE_OMAP,
        APFS_BTNODE_ROOT | APFS_BTNODE_LEAF | APFS_BTNODE_FIXED_KV_SIZE, 0x12, 16, 16);
    wr32(b + bs - sizeof(struct apfs_btree_info) +
        offsetof(struct apfs_btree_info, bt_longest_key), 0);
    wr32(b + bs - sizeof(struct apfs_btree_info) +
        offsetof(struct apfs_btree_info, bt_longest_val), 0);
    err = write_root_from(fs, (struct apfs_btree_node_phys *)b,
        (apfs_paddr_t)(omap + 1), omap + 1, NULL, 0);
    if (err != APFSRW_OK)
        goto out;
    mkc_header(b, omap, APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_OMAP);
    wr32(b + offsetof(struct apfs_omap_phys, om_flags), APFS_OMAP_MANUALLY_MANAGED);
    wr32(b + offsetof(struct apfs_omap_phys, om_tree_type),
        APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE);
    wr32(b + offsetof(struct apfs_omap_phys, om_snapshot_tree_type),
        APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_BTREE);
    wr64(b + offsetof(struct apfs_omap_phys, om_tree_oid), omap + 1);
    seal_object(fs, b);
    err = write_block(fs, (apfs_paddr_t)omap, b);
    if (err != APFSRW_OK)
        goto out;

    // space manager, first in the checkpoint data area
    {
        struct apfs_spaceman_phys *sm = (struct apfs_spaceman_phys *)b;
        uint32_t k;

        mkc_header(b, APFS_OID_SPACEMAN, APFS_OBJ_EPHEMERAL | APFS_OBJECT_TYPE_SPACEMAN);
        wr32(&sm->sm_block_size, bs);
        wr32(&sm->sm_blocks_per_chunk, bpc);
        wr32(&sm->sm_chunks_per_cib, cpcib);
        wr32(&sm->sm_cibs_per_cab, (uint32_t)((bs - sizeof(struct apfs_chunk_info_block)) / 8));
        wr64(&sm->sm_dev[0].sm_block_count, block_count);
        wr64(&sm->sm_dev[0].sm_chunk_count, chunks);
        wr32(&sm->sm_dev[0].sm_cib_count, (uint32_t)cibs);
        wr64(&sm->sm_dev[0].sm_free_count, block_count - used);
        wr32(&sm->sm_dev[0].sm_addr_offset, addr_off);
        // the empty tier-2 device's CIB array would start right after the main one
        wr32(&sm->sm_dev[1].sm_addr_offset, (uint32_t)(addr_off + cibs * 8));
        wr32(&sm->sm_flags, APFS_SM_FLAG_VERSIONED);
        wr32(&sm->sm_ip_bm_tx_multiplier, APFS_IP_BM_BLOCKS);
        wr64(&sm->sm_ip_block_count, ip_blocks);
        wr32(&sm->sm_ip_bm_size_in_blocks, 1);
        wr32(&sm->sm_ip_bm_block_count, APFS_IP_BM_BLOCKS);
        wr64(&sm->sm_ip_bm_base, ipbm_base);
        wr64(&sm->sm_ip_base, ip_base);
        // free queues start empty, their trees are made on first use
        wr16(sm->sm_fq + 24, row->fq_ip);
        wr16(sm->sm_fq + APFS_SM_FQ_SIZE + 24, row->fq_main);
        // bitmap slot 0 is live (xid 1), 1..15 are chained free
        wr16(&sm->sm_ip_bm_free_head, 1);
        wr16(&sm->sm_ip_bm_free_tail, APFS_IP_BM_BLOCKS - 1);
        wr32(&sm->sm_ip_bm_xid_offset, APFS_SM_STRUCT_SIZE);
        wr32(&sm->sm_ip_bitmap_offset, APFS_SM_STRUCT_SIZE + 8);
        wr32(&sm->sm_ip_bm_free_next_offset, APFS_SM_STRUCT_SIZE + 16);
        wr32(b + APFS_SM_VERSION_OFF, 1);
        wr32(b + APFS_SM_VERSION_OFF + 4, APFS_SM_STRUCT_SIZE);
        wr64(b + APFS_SM_STRUCT_SIZE, 1);
        wr16(b + APFS_SM_STRUCT_SIZE + 8, 0);
        for (k = 0; k < APFS_IP_BM_BLOCKS; k++)
            wr16(b + APFS_SM_STRUCT_SIZE + 16 + 2 * k,
                k == 0 || k == APFS_IP_BM_BLOCKS - 1 ? 0xffff : (uint16_t)(k + 1));
        for (c = 0; c < cibs; c++)
            wr64(b + addr_off + 8 * c, ip_base + c);
        seal_object(fs, b);
        err = write_block(fs, (apfs_paddr_t)data_base, b);
        if (err != APFSRW_OK)
            goto out;
    }

    // reaper: idle, with the flag the spec says is always set
    mkc_header(b, APFS_OID_REAPER, APFS_OBJ_EPHEMERAL | APFS_OBJECT_TYPE_NX_REAPER);
    wr64(b + 32, 1);                                // nr_next_reap_id
    wr32(b + 64, APFS_NR_BHM_FLAG);                 // nr_flags
    wr32(b + 108, bs - 112);                        // nr_state_buffer_size
    seal_object(fs, b);
    err = write_block(fs, (apfs_paddr_t)(data_base + 1), b);
    if (err != APFSRW_OK)
        goto out;

    // checkpoint: its map in descriptor block 1, the superblock in 2
    {
        struct apfs_checkpoint_map_phys *cpm = (struct apfs_checkpoint_map_phys *)b;

        mkc_header(b, 1, APFS_OBJ_PHYSICAL | APFS_OBJECT_TYPE_CHECKPOINT_MAP);
        wr32(&cpm->cpm_flags, APFS_CHECKPOINT_MAP_LAST);
        wr32(&cpm->cpm_count, 2);
        wr32(&cpm->cpm_map[0].cpm_type, APFS_OBJ_EPHEMERAL | APFS_OBJECT_TYPE_SPACEMAN);
        wr32(&cpm->cpm_map[0].cpm_size, bs);
        wr64(&cpm->cpm_map[0].cpm_oid, APFS_OID_SPACEMAN);
        wr64(&cpm->cpm_map[0].cpm_paddr, data_base);
        wr32(&cpm->cpm_map[1].cpm_type, APFS_OBJ_EPHEMERAL | APFS_OBJECT_TYPE_NX_REAPER);
        wr32(&cpm->cpm_map[1].cpm_size, bs);
        wr64(&cpm->cpm_map[1].cpm_oid, APFS_OID_REAPER);
        wr64(&cpm->cpm_map[1].cpm_paddr, data_base + 1);
        seal_object(fs, b);
        err = write_block(fs, 1, b);
        if (err != APFSRW_OK)
            goto out;
    }
    {
        struct apfs_nx_superblock *nx = (struct apfs_nx_superblock *)b;

        mkc_header(b, 1, APFS_OBJ_EPHEMERAL | APFS_OBJECT_TYPE_NX_SUPERBLOCK);
        wr32(&nx->nx_magic, APFS_NX_MAGIC);
        wr32(&nx->nx_block_size, bs);
        wr64(&nx->nx_block_count, block_count);
        wr64(&nx->nx_incompatible_features, APFS_NX_INCOMPAT_VERSION2);
        memcpy(nx->nx_uuid, uuid, 16);
        wr64(&nx->nx_next_oid, APFS_OID_REAPER + 1);
        wr64(&nx->nx_next_xid, 2);
        wr32(&nx->nx_xp_desc_blocks, row->desc);
        wr32(&nx->nx_xp_data_blocks, row->data);
        wr64(&nx->nx_xp_desc_base, 1);
        wr64(&nx->nx_xp_data_base, data_base);
        wr32(&nx->nx_xp_desc_next, 2);
        wr32(&nx->nx_xp_data_next, 2);
        wr32(&nx->nx_xp_desc_index, 0);
        wr32(&nx->nx_xp_desc_len, 2);
        wr32(&nx->nx_xp_data_index, 0);
        wr32(&nx->nx_xp_data_len, 2);
        wr64(&nx->nx_spaceman_oid, APFS_OID_SPACEMAN);
        wr64(&nx->nx_omap_oid, omap);
        wr64(&nx->nx_reaper_oid, APFS_OID_REAPER);
        wr32(&nx->nx_max_file_systems, (uint32_t)max_fs);
        // min_block_count is the main free-queue limit up to 128 MiB, NX_EPH_MIN_BLOCK_COUNT above
        wr64(b + APFS_NX_EPH_INFO_OFF,
            ((uint64_t)(block_count > 32768 ? 8 : row->fq_main) << 32) | (4U << 16) | 1U);
        seal_object(fs, b);
        err = write_block(fs, 2, b);
        if (err == APFSRW_OK && apfsrw_sync(fs) != 0)
            err = APFSRW_EIO;
        // block zero last: until it lands the image is not a container
        if (err == APFSRW_OK)
            err = write_block(fs, 0, b);
        if (err == APFSRW_OK && apfsrw_sync(fs) != 0)
            err = APFSRW_EIO;
    }
out:
    free(b);
    apfsrw_close(fs);
    return err;
}
#endif /* !APFSRW_KERNEL */

int apfsrw_setattr(struct apfsrw *fs, const char *path,
    const struct apfsrw_attr *attr)
{
    uint64_t fileid = 0;
    uint8_t key[8];
    uint8_t ival[1024];
    uint16_t ivlen = 0;
    int err;

    if (fs == NULL || path == NULL || attr == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;

    err = resolve_path(fs, path, &fileid, NULL);
    if (err != APFSRW_OK)
        return err;
    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_get(fs, key, 8, ival, sizeof(ival), &ivlen);
    if (err != APFSRW_OK)
        return err;
    if (ivlen < sizeof(struct apfs_j_inode_val))
        return APFSRW_EINVAL;

    if (attr->mask & APFSRW_ATTR_MODE) {
        uint16_t old = rd16(ival + offsetof(struct apfs_j_inode_val, mode));

        wr16(ival + offsetof(struct apfs_j_inode_val, mode),
            (uint16_t)((old & ~07777U) | (attr->mode & 07777U)));
    }
    if (attr->mask & APFSRW_ATTR_UID)
        wr32(ival + offsetof(struct apfs_j_inode_val, owner), attr->uid);
    if (attr->mask & APFSRW_ATTR_GID)
        wr32(ival + offsetof(struct apfs_j_inode_val, group), attr->gid);
    if (attr->mask & APFSRW_ATTR_FLAGS)
        wr32(ival + offsetof(struct apfs_j_inode_val, bsd_flags),
            attr->bsd_flags);
    if (attr->mask & APFSRW_ATTR_ATIME)
        wr64(ival + offsetof(struct apfs_j_inode_val, access_time),
            attr->atime_ns);
    if (attr->mask & APFSRW_ATTR_MTIME)
        wr64(ival + offsetof(struct apfs_j_inode_val, mod_time),
            attr->mtime_ns);
    if (attr->mask & APFSRW_ATTR_CRTIME)
        wr64(ival + offsetof(struct apfs_j_inode_val, create_time),
            attr->crtime_ns);
    // An access-time update alone is not a status change
    if (attr->mask != APFSRW_ATTR_ATIME)
        wr64(ival + offsetof(struct apfs_j_inode_val, change_time),
            apfsrw_now_ns());

    err = fstree_put(fs, key, 8, ival, ivlen, 1);
    if (err != APFSRW_OK)
        goto fail;
    err = txn_finish(fs, cur_next_oid(fs), 0, 0, 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    return err;
}

static int xattr_key_for(uint64_t fileid, const char *name, uint8_t *key,
    uint16_t *klen)
{
    size_t n = strlen(name);

    if (n == 0 || n > 255)
        return APFSRW_EINVAL;
    wr64(key, make_jkey(fileid, APFS_TYPE_XATTR));
    wr16(key + 8, (uint16_t)(n + 1));
    memcpy(key + 10, name, n + 1);
    *klen = (uint16_t)(10 + n + 1);
    return APFSRW_OK;
}

int apfsrw_set_xattr(struct apfsrw *fs, const char *path, const char *name,
    const void *data, size_t len, int mode)
{
    struct xattr_ctx c;
    uint64_t fileid = 0;
    uint8_t key[10 + 256], val[4 + APFSRW_XATTR_MAX_EMBEDDED];
    uint16_t klen = 0;
    int err, exists;

    if (fs == NULL || path == NULL || name == NULL ||
        (data == NULL && len != 0))
        return APFSRW_EINVAL;
    if (len > APFSRW_XATTR_MAX_EMBEDDED)
        return APFSRW_ENOTSUP;
    if (!fs->writable)
        return APFSRW_EPERM;

    err = resolve_path(fs, path, &fileid, NULL);
    if (err != APFSRW_OK)
        return err;
    err = lookup_xattr(fs, fileid, name, &c);
    if (err != APFSRW_OK && err != APFSRW_ENOENT)
        return err;
    exists = (err == APFSRW_OK);
    if (exists && (c.flags & APFS_XATTR_DATA_STREAM))
        return APFSRW_ENOTSUP;
    if ((mode == 1 && exists) || (mode == 2 && !exists))
        return exists ? APFSRW_EEXIST : APFSRW_ENOENT;

    err = xattr_key_for(fileid, name, key, &klen);
    if (err != APFSRW_OK)
        return err;
    wr16(val, APFS_XATTR_DATA_EMBEDDED);
    wr16(val + 2, (uint16_t)len);
    if (len > 0)
        memcpy(val + 4, data, len);
    err = fstree_put(fs, key, klen, val, (uint16_t)(4 + len), exists);
    if (err != APFSRW_OK)
        goto fail;
    err = txn_finish(fs, rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    return err;
}

int apfsrw_remove_xattr(struct apfsrw *fs, const char *path, const char *name)
{
    struct xattr_ctx c;
    uint64_t fileid = 0;
    uint8_t key[10 + 256];
    uint16_t klen = 0;
    int err;

    if (fs == NULL || path == NULL || name == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;

    err = resolve_path(fs, path, &fileid, NULL);
    if (err != APFSRW_OK)
        return err;
    err = lookup_xattr(fs, fileid, name, &c);
    if (err != APFSRW_OK)
        return err;
    // A stream-backed value owns extents. Dropping those is not done yet
    if (c.flags & APFS_XATTR_DATA_STREAM)
        return APFSRW_ENOTSUP;
    err = xattr_key_for(fileid, name, key, &klen);
    if (err != APFSRW_OK)
        return err;
    err = fstree_del(fs, key, klen);
    if (err != APFSRW_OK)
        goto fail;
    err = txn_finish(fs, rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    return err;
}

static int inode_val_rename(const uint8_t *old, uint16_t old_len,
    const char *name, uint16_t namelen, uint64_t new_parent,
    uint8_t *out, uint16_t *out_len);

// Hard links

static int drec_key_for(struct apfsrw *fs, uint64_t parent, const char *name,
    uint16_t namelen, uint8_t *key, uint16_t *klen)
{
    uint32_t hash;
    int err = drec_name_hash(fs, name, namelen, &hash);

    if (err != APFSRW_OK)
        return err;
    wr64(key, make_jkey(parent, APFS_TYPE_DIR_REC));
    wr32(key + 8, ((hash << 10) & 0xfffffc00U) | ((namelen + 1U) & 0x3ffU));
    memcpy(key + 12, name, namelen);
    key[12 + namelen] = '\0';
    *klen = (uint16_t)(12 + namelen + 1);
    return APFSRW_OK;
}

// j_drec_val_t, optionally carrying the DREC_EXT_TYPE_SIBLING_ID extended field (spec p.111):
// xf_blob_t + one x_field_t + the 8-byte id
static uint16_t drec_val_build(uint64_t fileid, uint64_t added, uint16_t flags,
    uint64_t sibling_id, uint8_t *out)
{
    memset(out, 0, 34);
    wr64(out, fileid);
    wr64(out + 8, added);
    wr16(out + 16, flags);
    if (sibling_id == 0)
        return 18;
    wr16(out + 18, 1);                   // xf_num_exts
    wr16(out + 20, 8);                   // xf_used_data
    out[22] = APFS_DREC_EXT_TYPE_SIBLING_ID;
    out[23] = 0x02;                      // XF_DO_NOT_COPY, as macOS writes it
    wr16(out + 24, 8);
    wr64(out + 26, sibling_id);
    return 34;
}

static uint64_t drec_sibling_id(const uint8_t *val, uint16_t vlen)
{
    uint32_t num, i, desc, data;

    if (vlen < 22)
        return 0;
    num = rd16(val + 18);
    desc = 22;
    data = desc + num * 4U;
    for (i = 0; i < num; i++) {
        uint8_t type = val[desc + i * 4U];
        uint16_t size = rd16(val + desc + i * 4U + 2U);

        if (data + size > vlen)
            return 0;
        if (type == APFS_DREC_EXT_TYPE_SIBLING_ID && size >= 8)
            return rd64(val + data);
        data += ((uint32_t)size + 7U) & ~7U;
    }
    return 0;
}

static int inode_name(const uint8_t *val, uint16_t vlen, const char **name,
    uint16_t *namelen)
{
    uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
    uint32_t num, i, desc, data;

    if (vlen < fixed + 4U)
        return APFSRW_ENOENT;
    num = rd16(val + fixed);
    desc = fixed + 4U;
    data = desc + num * 4U;
    for (i = 0; i < num; i++) {
        uint8_t type = val[desc + i * 4U];
        uint16_t size = rd16(val + desc + i * 4U + 2U);

        if (data + size > vlen)
            return APFSRW_EINVAL;
        if (type == APFS_INO_EXT_TYPE_NAME) {
            *name = (const char *)val + data;
            *namelen = (uint16_t)(size ? size - 1 : 0);
            return APFSRW_OK;
        }
        data += ((uint32_t)size + 7U) & ~7U;
    }
    return APFSRW_ENOENT;
}

// SIBLING_LINK (inode id, sibling id) -> (parent,
// name) and the SIBLING_MAP (sibling id) -> inode id that goes with it (spec p.104-105)
static int sibling_put(struct apfsrw *fs, uint64_t fileid, uint64_t sid,
    uint64_t parent, const char *name, uint16_t namelen)
{
    uint8_t key[16], val[10 + 256];
    int err;

    wr64(key, make_jkey(fileid, APFS_TYPE_SIBLING_LINK));
    wr64(key + 8, sid);
    wr64(val, parent);
    wr16(val + 8, (uint16_t)(namelen + 1));
    memcpy(val + 10, name, namelen);
    val[10 + namelen] = '\0';
    err = fstree_put(fs, key, 16, val, (uint16_t)(10 + namelen + 1), 0);
    if (err != APFSRW_OK)
        return err;
    wr64(key, make_jkey(sid, APFS_TYPE_SIBLING_MAP));
    wr64(val, fileid);
    return fstree_put(fs, key, 8, val, 8, 0);
}

static int sibling_del(struct apfsrw *fs, uint64_t fileid, uint64_t sid)
{
    uint8_t key[16];
    int err;

    wr64(key, make_jkey(fileid, APFS_TYPE_SIBLING_LINK));
    wr64(key + 8, sid);
    err = fstree_del(fs, key, 16);
    if (err != APFSRW_OK && err != APFSRW_ENOENT)
        return err;
    wr64(key, make_jkey(sid, APFS_TYPE_SIBLING_MAP));
    err = fstree_del(fs, key, 8);
    return err == APFSRW_ENOENT ? APFSRW_OK : err;
}

struct sibling_all_ctx {
    uint64_t fileid;
    uint64_t *sids;
    uint32_t n, cap;
};

static int sibling_all_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct sibling_all_ctx *c = (struct sibling_all_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp, *valp;
        uint16_t klen, vlen;
        int err = btree_entry(fs, node, info, i, &keyp, &klen, &valp, &vlen);

        if (err != APFSRW_OK)
            return err;
        if (klen < 16 || key_id(rd64(keyp)) != c->fileid ||
            key_type(rd64(keyp)) != APFS_TYPE_SIBLING_LINK)
            continue;
        if (c->n == c->cap) {
            uint32_t ncap = c->cap ? c->cap * 2 : 8;
            uint64_t *ns = realloc(c->sids, ncap * sizeof(*ns));

            if (ns == NULL)
                return APFSRW_ENOMEM;
            c->sids = ns;
            c->cap = ncap;
        }
        c->sids[c->n++] = rd64((const uint8_t *)keyp + 8);
    }
    return APFSRW_OK;
}

static int sibling_drop_all(struct apfsrw *fs, uint64_t fileid)
{
    struct sibling_all_ctx c;
    uint32_t i;
    int err;

    memset(&c, 0, sizeof(c));
    c.fileid = fileid;
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, fileid, fileid,
        sibling_all_cb, &c);
    if (err != APFSRW_OK && err != 1) {
        free(c.sids);
        return err;
    }
    for (i = 0; i < c.n && err != APFSRW_EIO; i++)
        err = sibling_del(fs, fileid, c.sids[i]);
    free(c.sids);
    return err == 1 ? APFSRW_OK : err;
}

struct sibling_find_ctx {
    uint64_t fileid, skip_sid, parent;
    char name[256];
    uint16_t namelen;
    int found;
};

static int sibling_find_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct sibling_find_ctx *c = (struct sibling_find_ctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp, *valp;
        uint16_t klen, vlen, nl;
        int err = btree_entry(fs, node, info, i, &keyp, &klen, &valp, &vlen);

        if (err != APFSRW_OK)
            return err;
        if (klen < 16 || vlen < 10 || key_id(rd64(keyp)) != c->fileid ||
            key_type(rd64(keyp)) != APFS_TYPE_SIBLING_LINK)
            continue;
        if (rd64((const uint8_t *)keyp + 8) == c->skip_sid)
            continue;
        nl = rd16((const uint8_t *)valp + 8);
        if (nl == 0 || nl > 256 || 10U + nl > vlen)
            continue;
        c->parent = rd64(valp);
        memcpy(c->name, (const uint8_t *)valp + 10, nl);
        c->namelen = (uint16_t)(nl - 1);
        c->found = 1;
        return 1;
    }
    return APFSRW_OK;
}

static int sibling_find_other(struct apfsrw *fs, uint64_t fileid,
    uint64_t skip_sid, struct sibling_find_ctx *c)
{
    int err;

    memset(c, 0, sizeof(*c));
    c->fileid = fileid;
    c->skip_sid = skip_sid;
    err = btree_walk_leaves_oid(fs, fs->root_tree_paddr, fileid, fileid,
        sibling_find_cb, c);
    if (err != APFSRW_OK && err != 1)
        return err;
    return c->found ? APFSRW_OK : APFSRW_ENOENT;
}

static int parent_nchildren_add(struct apfsrw *fs, uint64_t parent, int32_t d)
{
    uint8_t pkey[8], pval[1024];
    uint16_t pvlen = 0;
    int err;

    wr64(pkey, make_jkey(parent, APFS_TYPE_INODE));
    err = fstree_get(fs, pkey, 8, pval, sizeof(pval), &pvlen);
    if (err != APFSRW_OK)
        return err;
    wr32(pval + offsetof(struct apfs_j_inode_val, u),
        (uint32_t)((int32_t)rd32(pval + offsetof(struct apfs_j_inode_val, u)) +
        d));
    return fstree_put(fs, pkey, 8, pval, pvlen, 1);
}

int apfsrw_link(struct apfsrw *fs, const char *existing, const char *newpath)
{
    uint64_t eparent = 0, nparent = 0, fileid = 0, other = 0, next, now;
    const char *ename = NULL, *nname = NULL;
    uint16_t enamelen = 0, nnamelen = 0, klen = 0, dvlen = 0, ivlen = 0;
    uint8_t dtype = 0;
    uint8_t key[8 + 4 + 256], dval[64], ival[1024];
    int err;

    if (fs == NULL || existing == NULL || newpath == NULL)
        return APFSRW_EINVAL;
    if (!fs->writable)
        return APFSRW_EPERM;

    err = split_parent(fs, existing, &eparent, &ename, &enamelen);
    if (err != APFSRW_OK)
        return err;
    err = lookup_dirent(fs, eparent, ename, enamelen, &fileid, &dtype);
    if (err != APFSRW_OK)
        return err;
    if (dtype == APFSRW_DT_DIR)
        return APFSRW_EPERM;
    err = split_parent(fs, newpath, &nparent, &nname, &nnamelen);
    if (err != APFSRW_OK)
        return err;
    if (lookup_dirent(fs, nparent, nname, nnamelen, &other, NULL) == APFSRW_OK)
        return APFSRW_EEXIST;

    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_get(fs, key, 8, ival, sizeof(ival), &ivlen);
    if (err != APFSRW_OK)
        return err;
    next = cur_next_oid(fs);
    now = apfsrw_now_ns();

    // The existing entry needs sibling records of its own
    err = drec_key_for(fs, eparent, ename, enamelen, key, &klen);
    if (err != APFSRW_OK)
        return err;
    err = fstree_get(fs, key, klen, dval, sizeof(dval), &dvlen);
    if (err != APFSRW_OK)
        return err;
    if (drec_sibling_id(dval, dvlen) == 0) {
        uint64_t sid = next++;

        err = sibling_put(fs, fileid, sid, eparent, ename, enamelen);
        if (err != APFSRW_OK)
            goto fail;
        dvlen = drec_val_build(fileid, rd64(dval + 8), rd16(dval + 16), sid,
            dval);
        err = fstree_put(fs, key, klen, dval, dvlen, 1);
        if (err != APFSRW_OK)
            goto fail;
    }

    {
        uint64_t sid = next++;

        err = sibling_put(fs, fileid, sid, nparent, nname, nnamelen);
        if (err != APFSRW_OK)
            goto fail;
        err = drec_key_for(fs, nparent, nname, nnamelen, key, &klen);
        if (err != APFSRW_OK)
            goto fail;
        dvlen = drec_val_build(fileid, now, dtype, sid, dval);
        err = fstree_put(fs, key, klen, dval, dvlen, 0);
        if (err != APFSRW_OK)
            goto fail;
    }

    wr32(ival + offsetof(struct apfs_j_inode_val, u),
        rd32(ival + offsetof(struct apfs_j_inode_val, u)) + 1);
    wr64(ival + offsetof(struct apfs_j_inode_val, change_time), now);
    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_put(fs, key, 8, ival, ivlen, 1);
    if (err != APFSRW_OK)
        goto fail;
    err = parent_nchildren_add(fs, nparent, 1);
    if (err != APFSRW_OK)
        goto fail;
    err = txn_finish(fs, next, 0, 0, 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    return err;
}

static int unlink_one_link(struct apfsrw *fs, uint64_t parent,
    const char *name, uint16_t namelen, uint64_t fileid, uint8_t *ival,
    uint16_t ivlen)
{
    uint8_t key[8 + 4 + 256], dval[64], nval[1024];
    uint16_t klen = 0, dvlen = 0, nvlen = ivlen;
    uint64_t sid, now = apfsrw_now_ns();
    const char *pname = NULL;
    uint16_t pnamelen = 0;
    int err;

    err = drec_key_for(fs, parent, name, namelen, key, &klen);
    if (err != APFSRW_OK)
        return err;
    err = fstree_get(fs, key, klen, dval, sizeof(dval), &dvlen);
    if (err != APFSRW_OK)
        return err;
    sid = drec_sibling_id(dval, dvlen);
    err = fstree_del(fs, key, klen);
    if (err != APFSRW_OK)
        goto fail;
    if (sid != 0) {
        err = sibling_del(fs, fileid, sid);
        if (err != APFSRW_OK)
            goto fail;
    }

    memcpy(nval, ival, ivlen);
    // If this was the primary name, another link takes over
    if (inode_name(ival, ivlen, &pname, &pnamelen) == APFSRW_OK &&
        pnamelen == namelen && memcmp(pname, name, namelen) == 0 &&
        rd64(ival + offsetof(struct apfs_j_inode_val, parent_id)) == parent) {
        struct sibling_find_ctx sc;

        if (sibling_find_other(fs, fileid, sid, &sc) == APFSRW_OK) {
            err = inode_val_rename(ival, ivlen, sc.name, sc.namelen,
                sc.parent, nval, &nvlen);
            if (err != APFSRW_OK)
                goto fail;
        }
    }
    wr32(nval + offsetof(struct apfs_j_inode_val, u),
        rd32(nval + offsetof(struct apfs_j_inode_val, u)) - 1);
    wr64(nval + offsetof(struct apfs_j_inode_val, change_time), now);
    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_put(fs, key, 8, nval, nvlen, 1);
    if (err != APFSRW_OK)
        goto fail;
    err = parent_nchildren_add(fs, parent, -1);
    if (err != APFSRW_OK)
        goto fail;
    err = txn_finish(fs, rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
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

    if (dtype != APFSRW_DT_DIR) {
        wr64(pkey, make_jkey(fileid, APFS_TYPE_INODE));
        err = fstree_get(fs, pkey, 8, pval, sizeof(pval), &pvlen);
        if (err != APFSRW_OK)
            return err;
        if (rd32(pval + offsetof(struct apfs_j_inode_val, u)) > 1)
            return unlink_one_link(fs, parent, name, namelen, fileid, pval,
                pvlen);
    }

    // A file's data stream goes with it: extents deleted, blocks freed
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

    // Read the entry's inode so a non-empty directory can be rejected
    wr64(pkey, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_get(fs, pkey, 8, pval, sizeof(pval), &pvlen);
    if (err != APFSRW_OK)
        goto fail;
    if (dtype == APFSRW_DT_DIR &&
        rd32(pval + offsetof(struct apfs_j_inode_val, u)) != 0) {
        err = APFSRW_ENOTEMPTY;
        goto fail;
    }

    // Symlinks keep their target in an xattr. Drop it with the inode
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

    // The directory entry in the parent
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

    // Then the inode itself, with any sibling records a hard link left
    err = fstree_del(fs, pkey, 8);
    if (err != APFSRW_OK)
        goto fail;
    err = sibling_drop_all(fs, fileid);
    if (err != APFSRW_OK)
        goto fail;

    // One fewer child in the parent
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

    if (dtype != APFSRW_DT_REG && dtype != APFSRW_DT_DIR &&
        dtype != APFSRW_DT_LNK)
        fs->other_delta -= 1;
    // Removals carry negative deltas. They sum with the creates in the batch
    err = txn_finish(fs, rd64(&fs->apfs.apfs_next_obj_id),
        dtype == APFSRW_DT_REG ? (uint64_t)-1 : 0,
        dtype == APFSRW_DT_DIR ? (uint64_t)-1 : 0,
        dtype == APFSRW_DT_LNK ? (uint64_t)-1 : 0);
    if (err != APFSRW_OK)
        goto fail;
    return APFSRW_OK;

fail:
    txn_rollback(fs);
    return err;
}

// Rebuild an inode value with a new NAME extended field,
// carrying every other field over unchanged (spec p.108-109 xf_blob_t / x_field_t)
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
    // xf_used_data: total bytes of field data
    wr16(out + fixed + 2, (uint16_t)(ndata - odata +
        rd16(old + fixed + 2)));
    *out_len = (uint16_t)ndata;
    return APFSRW_OK;
}

int apfsrw_rename(struct apfsrw *fs, const char *from, const char *to)
{
    uint64_t fparent = 0, tparent = 0, fileid = 0, existing = 0, sid = 0;
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

    err = split_parent(fs, from, &fparent, &fname, &fnamelen);
    if (err != APFSRW_OK)
        return err;
    err = lookup_dirent(fs, fparent, fname, fnamelen, &fileid, &dtype);
    if (err != APFSRW_OK)
        return err;
    // The target is replaced, as rename(2) requires
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
        // unlink committed. re-resolve both parents' current state
        err = split_parent(fs, from, &fparent, &fname, &fnamelen);
        if (err != APFSRW_OK)
            return err;
        err = split_parent(fs, to, &tparent, &tname, &tnamelen);
        if (err != APFSRW_OK)
            return err;
    }
    now = apfsrw_now_ns();

    // A hard link keeps its sibling id. The inode's NAME only follows the primary link
    {
        uint8_t dval[64];
        uint16_t klen = 0, dvlen = 0;

        err = drec_key_for(fs, fparent, fname, fnamelen, key, &klen);
        if (err != APFSRW_OK)
            return err;
        err = fstree_get(fs, key, klen, dval, sizeof(dval), &dvlen);
        if (err != APFSRW_OK)
            return err;
        sid = drec_sibling_id(dval, dvlen);
    }

    wr64(key, make_jkey(fileid, APFS_TYPE_INODE));
    err = fstree_get(fs, key, 8, ival, sizeof(ival), &ivlen);
    if (err != APFSRW_OK)
        return err;
    {
        const char *pname = NULL;
        uint16_t pnamelen = 0;
        int primary = sid == 0 ||
            (inode_name(ival, ivlen, &pname, &pnamelen) == APFSRW_OK &&
            pnamelen == fnamelen && memcmp(pname, fname, fnamelen) == 0 &&
            rd64(ival + offsetof(struct apfs_j_inode_val, parent_id)) ==
            fparent);

        if (primary) {
            err = inode_val_rename(ival, ivlen, tname, tnamelen, tparent,
                nval, &nvlen);
            if (err != APFSRW_OK)
                return err;
        } else {
            memcpy(nval, ival, ivlen);
            nvlen = ivlen;
        }
    }
    wr64(nval + offsetof(struct apfs_j_inode_val, change_time), now);
    err = fstree_put(fs, key, 8, nval, nvlen, 1);
    if (err != APFSRW_OK)
        goto fail;
    if (sid != 0) {
        err = sibling_del(fs, fileid, sid);
        if (err == APFSRW_OK)
            err = sibling_put(fs, fileid, sid, tparent, tname, tnamelen);
        if (err != APFSRW_OK)
            goto fail;
    }

    // Old directory entry out, new one in
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
        uint8_t dval[64];
        uint16_t dvlen = drec_val_build(fileid, now, dtype, sid, dval);

        err = fstree_put(fs, key, (uint16_t)(12 + tnamelen + 1), dval, dvlen,
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

    err = txn_finish(fs, rd64(&fs->apfs.apfs_next_obj_id), 0, 0, 0);
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
        (uint16_t)(0120000u | 0777u), uid, gid, NULL, NULL);
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

#ifndef APFSRW_KERNEL
// volume-to-volume copy

#define APFS_UF_COMPRESSED 0x20U

struct cp_srcx {
    char name[256];
    uint16_t flags;
    uint8_t *data;
    size_t len;
    uint64_t oid;               // a stream-backed value's object id, read after the walk
};

struct cp_xctx {
    uint64_t id;
    struct cp_srcx *v;
    uint32_t n, cap;
    int err;
};

static int cp_xattr_cb(struct apfsrw *fs,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
    struct cp_xctx *c = (struct cp_xctx *)ctx;
    uint32_t i;

    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp, *valp;
        uint16_t klen, vlen, nlen, xlen, flags;
        struct cp_srcx *x;
        uint64_t key;
        int err = btree_entry(fs, node, info, i, &keyp, &klen, &valp, &vlen);

        if (err != APFSRW_OK)
            return err;
        if (klen < 11 || vlen < 4)
            continue;
        key = rd64(keyp);
        if (key_id(key) != c->id || key_type(key) != APFS_TYPE_XATTR)
            continue;
        nlen = rd16((const uint8_t *)keyp + 8);
        if (nlen < 2 || nlen > 256 || 10U + nlen > klen)
            continue;
        flags = rd16(valp);
        xlen = rd16((const uint8_t *)valp + 2);
        if (4U + xlen > vlen)
            continue;
        if (c->n == c->cap) {
            uint32_t ncap = c->cap ? c->cap * 2 : 8;
            struct cp_srcx *nv = realloc(c->v, ncap * sizeof(*nv));

            if (nv == NULL)
                return APFSRW_ENOMEM;
            c->v = nv;
            c->cap = ncap;
        }
        x = &c->v[c->n];
        memset(x, 0, sizeof(*x));
        memcpy(x->name, (const uint8_t *)keyp + 10, nlen);
        x->name[nlen - 1] = '\0';
        x->flags = flags;
        if (flags & APFS_XATTR_DATA_STREAM) {
            if (xlen < sizeof(struct apfs_j_xattr_dstream))
                continue;
            x->oid = rd64((const uint8_t *)valp + 4 +
                offsetof(struct apfs_j_xattr_dstream, xattr_obj_id));
            x->len = (size_t)rd64((const uint8_t *)valp + 4 +
                offsetof(struct apfs_j_xattr_dstream, size));
        } else {
            x->len = xlen;
            x->data = malloc(xlen + 1U);
            if (x->data == NULL)
                return APFSRW_ENOMEM;
            memcpy(x->data, (const uint8_t *)valp + 4, xlen);
            x->data[xlen] = 0;
        }
        c->n++;
    }
    return APFSRW_OK;
}

static void cp_xfree(struct cp_xctx *c)
{
    uint32_t i;

    for (i = 0; i < c->n; i++)
        free(c->v[i].data);
    free(c->v);
    memset(c, 0, sizeof(*c));
}

// every xattr of one inode, stream-backed values read in full
static int cp_read_xattrs(struct apfsrw *src, uint64_t id, struct cp_xctx *c)
{
    uint32_t i;
    int err;

    memset(c, 0, sizeof(*c));
    c->id = id;
    err = btree_walk_leaves_oid(src, src->root_tree_paddr, id, id, cp_xattr_cb, c);
    if (err != APFSRW_OK && err != 1)
        return err;
    for (i = 0; i < c->n; i++) {
        struct cp_srcx *x = &c->v[i];

        if (!(x->flags & APFS_XATTR_DATA_STREAM))
            continue;
        x->data = calloc(1, x->len + 1);
        if (x->data == NULL)
            return APFSRW_ENOMEM;
        if (x->len != 0) {
            err = read_extents(src, x->oid, x->data, x->len);
            if (err != APFSRW_OK)
                return err;
        }
    }
    return APFSRW_OK;
}

static struct cp_srcx *cp_find(struct cp_xctx *c, const char *name)
{
    uint32_t i;

    for (i = 0; i < c->n; i++)
        if (strcmp(c->v[i].name, name) == 0)
            return &c->v[i];
    return NULL;
}

struct cp_link {
    uint64_t src_id;
    char *path;
};

struct cp_state {
    struct apfsrw *src, *dst;
    const struct apfsrw_copy_opts *o;
    struct apfsrw_copy_stats *st;
    struct cp_link *links;
    size_t nlinks, caplinks;
};

struct cp_list {
    struct apfsrw_dirent *e;
    size_t n, cap;
};

static int cp_collect(const struct apfsrw_dirent *entry, void *ctx)
{
    struct cp_list *l = (struct cp_list *)ctx;

    if (l->n == l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 64;
        struct apfsrw_dirent *ne = realloc(l->e, ncap * sizeof(*ne));

        if (ne == NULL)
            return APFSRW_ENOMEM;
        l->e = ne;
        l->cap = ncap;
    }
    l->e[l->n++] = *entry;
    return 0;
}

static const char *cp_link_find(struct cp_state *s, uint64_t id)
{
    size_t i;

    for (i = 0; i < s->nlinks; i++)
        if (s->links[i].src_id == id)
            return s->links[i].path;
    return NULL;
}

static int cp_link_add(struct cp_state *s, uint64_t id, const char *path)
{
    if (s->nlinks == s->caplinks) {
        size_t ncap = s->caplinks ? s->caplinks * 2 : 64;
        struct cp_link *nl = realloc(s->links, ncap * sizeof(*nl));

        if (nl == NULL)
            return APFSRW_ENOMEM;
        s->links = nl;
        s->caplinks = ncap;
    }
    s->links[s->nlinks].src_id = id;
    s->links[s->nlinks].path = strdup(path);
    if (s->links[s->nlinks].path == NULL)
        return APFSRW_ENOMEM;
    s->nlinks++;
    return APFSRW_OK;
}

// which source xattrs travel: none unless asked, except the decmpfs pair of a file kept compressed.
// the symlink target is written by create_entry itself
static int cp_keep_xattr(const struct cp_state *s, const struct cp_srcx *x,
    int keep_compressed)
{
    int cmp = strcmp(x->name, "com.apple.decmpfs") == 0 ||
        strcmp(x->name, "com.apple.ResourceFork") == 0;

    if (strcmp(x->name, "com.apple.fs.symlink") == 0)
        return 0;
    if (cmp)
        return keep_compressed;
    return s->o->xattrs;
}

static int cp_entry(struct cp_state *s, const struct apfsrw_dirent *e,
    const char *srcpath, const char *path);

static int cp_strcmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

// with an only list, an entry travels when listed or when something listed lies below it.
// the list is sorted, so the first path >= "p/" says whether anything is under p
static int cp_only(const struct cp_state *s, const char *p)
{
    const char *const *v = s->o->only;
    size_t lo = 0, hi = s->o->nonly, n = strlen(p);
    char *pre = malloc(n + 2);
    int hit = 0;

    if (pre == NULL)
        return 0;
    memcpy(pre, p, n);
    pre[n] = '/';
    pre[n + 1] = '\0';
    if (bsearch(&p, v, s->o->nonly, sizeof(*v), cp_strcmp) != NULL)
        hit = 1;
    while (!hit && lo < hi) {
        size_t mid = (lo + hi) / 2;

        if (strcmp(v[mid], pre) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (!hit && lo < s->o->nonly && strncmp(v[lo], pre, n + 1) == 0)
        hit = 1;
    free(pre);
    return hit;
}

static char *cp_join(const char *dir, const char *name)
{
    char *p = malloc(strlen(dir) + strlen(name) + 2);

    if (p != NULL)
        sprintf(p, "%s%s%s", dir, strcmp(dir, "/") ? "/" : "", name);
    return p;
}

static int cp_dir(struct cp_state *s, uint64_t dir, const char *srcdir,
    const char *dstdir)
{
    struct cp_list l = { NULL, 0, 0 };
    struct list_ctx lc;
    size_t i;
    int err;

    lc.parent_id = dir;
    lc.cb = cp_collect;
    lc.user = &l;
    err = btree_walk_leaves_oid(s->src, s->src->root_tree_paddr, dir, dir,
        list_leaf_cb, &lc);
    for (i = 0; err == APFSRW_OK && i < l.n; i++) {
        char *sp = cp_join(srcdir, l.e[i].name), *path = cp_join(dstdir, l.e[i].name);
        uint32_t k;

        for (k = 0; sp != NULL && k < s->o->nskip; k++)
            if (strcmp(sp, s->o->skip[k]) == 0)
                break;
        if (sp == NULL || path == NULL)
            err = APFSRW_ENOMEM;
        else if (k < s->o->nskip)
            s->st->skipped++;
        else if (s->o->only != NULL && !cp_only(s, sp))
            ;
        else
            err = cp_entry(s, &l.e[i], sp, path);
        free(sp);
        free(path);
        if (err == APFSRW_OK && apfsrw_batch_pending(s->dst) >= s->o->flush_blocks) {
            err = apfsrw_batch_end(s->dst);
            if (err == APFSRW_OK)
                err = apfsrw_batch_begin(s->dst);
        }
    }
    free(l.e);
    return err;
}

static int cp_entry(struct cp_state *s, const struct apfsrw_dirent *e,
    const char *srcpath, const char *path)
{
    struct inode_ctx ic;
    struct cp_xctx xc;
    struct cp_extra x;
    struct cp_xattr *out = NULL;
    uint8_t *data = NULL;
    size_t size = 0;
    uint16_t ftype;
    uint32_t uid, gid, i;
    int keep_cmp = 0, err;

    memset(&xc, 0, sizeof(xc));
    memset(&ic, 0, sizeof(ic));
    ic.file_id = e->file_id;
    err = btree_walk_leaves_oid(s->src, s->src->root_tree_paddr, e->file_id,
        e->file_id, inode_leaf_cb, &ic);
    if (err != APFSRW_OK && err != 1)
        return err;
    if (!ic.found)
        return APFSRW_ENOENT;
    switch (ic.mode & 0170000) {
    case 0040000: ftype = APFSRW_DT_DIR; break;
    case 0100000: ftype = APFSRW_DT_REG; break;
    case 0120000: ftype = APFSRW_DT_LNK; break;
    case 0010000: ftype = APFSRW_DT_FIFO; break;
    case 0020000: ftype = APFSRW_DT_CHR; break;
    case 0060000: ftype = APFSRW_DT_BLK; break;
    case 0140000: ftype = APFSRW_DT_SOCK; break;
    default: s->st->skipped++; return APFSRW_OK;
    }

    // a second name of a hard-linked file becomes a link to the first copy
    if (ftype == APFSRW_DT_REG && ic.nlink > 1) {
        const char *first = cp_link_find(s, e->file_id);

        if (first != NULL) {
            err = apfsrw_link(s->dst, first, path);
            if (err == APFSRW_OK)
                s->st->hardlinks++;
            return err == APFSRW_EEXIST ? APFSRW_OK : err;
        }
    }

    if (s->o->xattrs || ftype == APFSRW_DT_LNK ||
        (s->o->compressed && (ic.bsd_flags & APFS_UF_COMPRESSED))) {
        err = cp_read_xattrs(s->src, e->file_id, &xc);
        if (err != APFSRW_OK)
            goto out;
    }
    keep_cmp = s->o->compressed && ftype == APFSRW_DT_REG &&
        (ic.bsd_flags & APFS_UF_COMPRESSED) && cp_find(&xc, "com.apple.decmpfs") != NULL;

    memset(&x, 0, sizeof(x));
    x.atime = ic.atime;
    x.mtime = ic.mtime;
    x.ctime = ic.ctime;
    x.crtime = ic.crtime;
    x.bsd_flags = s->o->flags ? ic.bsd_flags & ~APFS_UF_COMPRESSED : 0;
    if (xc.n != 0) {
        out = calloc(xc.n, sizeof(*out));
        if (out == NULL) {
            err = APFSRW_ENOMEM;
            goto out;
        }
    }
    for (i = 0; i < xc.n; i++) {
        if (!cp_keep_xattr(s, &xc.v[i], keep_cmp))
            continue;
        out[x.nxattrs].name = xc.v[i].name;
        out[x.nxattrs].flags = xc.v[i].flags;
        out[x.nxattrs].data = xc.v[i].data;
        out[x.nxattrs].len = xc.v[i].len;
        x.nxattrs++;
    }
    x.xattrs = out;
    if (keep_cmp) {
        const struct cp_srcx *d = cp_find(&xc, "com.apple.decmpfs");

        x.compressed = 1;
        x.rsrc = cp_find(&xc, "com.apple.ResourceFork") != NULL;
        x.bsd_flags |= APFS_UF_COMPRESSED;
        x.ucsize = d->len >= sizeof(struct apfs_decmpfs_header) ?
            rd64(d->data + offsetof(struct apfs_decmpfs_header, uncompressed_size)) : ic.size;
        s->st->compressed++;
    } else if (ftype == APFSRW_DT_REG) {
        err = apfsrw_read_file_by_id(s->src, e->file_id, &data, &size);
        if (err != APFSRW_OK) {
            fprintf(stderr, "apfsrw: copy: read %s: %s\n", path, apfsrw_strerror(err));
            goto out;
        }
    }
    uid = s->o->owner0 ? 0 : ic.uid;
    gid = s->o->owner0 ? 0 : ic.gid;

    if (ftype == APFSRW_DT_LNK) {
        const struct cp_srcx *t = cp_find(&xc, "com.apple.fs.symlink");

        if (t == NULL || t->len == 0) {
            err = APFSRW_EINVAL;
            goto out;
        }
        err = create_entry(s->dst, path, ftype, NULL, 0, (const char *)t->data,
            ic.mode, uid, gid, NULL, &x);
    } else {
        err = create_entry(s->dst, path, ftype, data, size, NULL, ic.mode, uid,
            gid, NULL, &x);
    }
    if (err == APFSRW_ENOTSUP) {
        fprintf(stderr, "apfsrw: copy: skipping %s (name)\n", path);
        s->st->skipped++;
        err = APFSRW_OK;
        goto out;
    }
    if (err == APFSRW_EEXIST && ftype == APFSRW_DT_DIR)
        err = APFSRW_OK;                 // merging into a directory already there
    else if (err == APFSRW_EEXIST) {
        s->st->skipped++;
        err = APFSRW_OK;
        goto out;
    } else if (err != APFSRW_OK) {
        fprintf(stderr, "apfsrw: copy: create %s: %s\n", path, apfsrw_strerror(err));
        goto out;
    } else {
        s->st->xattrs += x.nxattrs;
        if (ftype == APFSRW_DT_DIR)
            s->st->dirs++;
        else if (ftype == APFSRW_DT_REG)
            s->st->files++;
        else if (ftype == APFSRW_DT_LNK)
            s->st->symlinks++;
        else
            s->st->other++;
        s->st->bytes += size;
        if (ftype == APFSRW_DT_REG && ic.nlink > 1)
            err = cp_link_add(s, e->file_id, path);
    }
    if (err == APFSRW_OK && ftype == APFSRW_DT_DIR)
        err = cp_dir(s, e->file_id, srcpath, path);
out:
    free(data);
    free(out);
    cp_xfree(&xc);
    return err;
}

int apfsrw_copy_tree(struct apfsrw *src, const char *srcpath, struct apfsrw *dst,
    const char *dstpath, const struct apfsrw_copy_opts *opts,
    struct apfsrw_copy_stats *stats)
{
    struct cp_state s;
    uint64_t sid, did;
    uint8_t st, dt;
    size_t i;
    int err, eerr;

    if (src == NULL || dst == NULL || opts == NULL || stats == NULL)
        return APFSRW_EINVAL;
    if (!dst->writable)
        return APFSRW_EPERM;
    if (dst->batch)
        return APFSRW_EINVAL;
    memset(stats, 0, sizeof(*stats));
    err = resolve_path(src, srcpath ? srcpath : "/", &sid, &st);
    if (err == APFSRW_OK)
        err = resolve_path(dst, dstpath ? dstpath : "/", &did, &dt);
    if (err != APFSRW_OK)
        return err;
    if (st != APFSRW_DT_DIR || dt != APFSRW_DT_DIR)
        return APFSRW_ENOTDIR;

    memset(&s, 0, sizeof(s));
    if (opts->only != NULL)
        qsort((void *)opts->only, opts->nonly, sizeof(*opts->only), cp_strcmp);
    s.src = src;
    s.dst = dst;
    s.o = opts;
    s.st = stats;
    err = apfsrw_batch_begin(dst);
    if (err != APFSRW_OK)
        return err;
    err = cp_dir(&s, sid, srcpath ? srcpath : "/", dstpath ? dstpath : "/");
    eerr = apfsrw_batch_end(dst);
    if (err == APFSRW_OK)
        err = eerr;
    for (i = 0; i < s.nlinks; i++)
        free(s.links[i].path);
    free(s.links);
    return err;
}
#endif /* !APFSRW_KERNEL */
