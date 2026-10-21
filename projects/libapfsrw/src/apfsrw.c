#define _FILE_OFFSET_BITS 64
#define _XOPEN_SOURCE 700

#include "apfsrw/apfsrw.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define APFS_NX_MAGIC 0x4253584eU
#define APFS_APSB_MAGIC 0x42535041U
#define APFS_OBJECT_TYPE_MASK 0x0000ffffU
#define APFS_OBJECT_TYPE_BTREE 0x00000002U
#define APFS_OBJECT_TYPE_BTREE_NODE 0x00000003U
#define APFS_OBJECT_TYPE_OMAP 0x0000000bU
#define APFS_OBJECT_TYPE_FS 0x0000000dU
#define APFS_OBJECT_TYPE_FSTREE 0x0000000eU
#define APFS_BTNODE_ROOT 0x0001U
#define APFS_BTNODE_LEAF 0x0002U
#define APFS_BTNODE_FIXED_KV_SIZE 0x0004U
#define APFS_OBJ_ID_MASK 0x0fffffffffffffffULL
#define APFS_OBJ_TYPE_MASK 0xf000000000000000ULL
#define APFS_OBJ_TYPE_SHIFT 60U
#define APFS_TYPE_INODE 3U
#define APFS_TYPE_FILE_EXTENT 8U
#define APFS_TYPE_DIR_REC 9U
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
    apfs_oid_t root_tree_oid;
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

static int read_block(struct apfsrw *fs, apfs_paddr_t paddr, void *out)
{
    ssize_t n;

    if (fs == NULL || out == NULL || paddr < 0 ||
        (uint64_t)paddr >= fs->image_blocks)
        return APFSRW_EINVAL;
    n = pread(fs->fd, out, fs->block_size,
        (off_t)((uint64_t)paddr * fs->block_size));
    if (n < 0)
        return APFSRW_EIO;
    if ((size_t)n != fs->block_size)
        return APFSRW_EIO;
    return APFSRW_OK;
}

static int read_object(struct apfsrw *fs, apfs_paddr_t paddr, void *out)
{
    uint64_t expected;
    uint64_t actual;
    int err;

    err = read_block(fs, paddr, out);
    if (err != APFSRW_OK)
        return err;
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
    if (info != NULL)
        val_end = (uint16_t)(val_end - sizeof(*info));
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

        if (fixed_key == 0 || fixed_val == 0)
            return APFSRW_EINVAL;
        toc = (const struct apfs_kvoff *)(base + data_off + table_off +
            index * sizeof(*toc));
        ko = rd16(&toc->k);
        kl = (uint16_t)fixed_key;
        vo = rd16(&toc->v);
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
    apfs_oid_t oid, apfs_xid_t xid, struct apfs_omap_val *out)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *info;
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
    if (rd16(&node->btn_level) != 0) {
        err = APFSRW_ENOTSUP;
        goto out;
    }
    info = btree_info_for_node(fs, node);
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
    struct stat st;
    struct apfs_omap_phys omap;
    struct apfs_omap_val ov;
    uint8_t *block;
    uint32_t max_fs;
    uint32_t i;
    int err;

    if (fstat(fs->fd, &st) != 0)
        return APFSRW_EIO;
    fs->image_blocks = (uint64_t)st.st_size / APFSRW_BLOCK_SIZE;
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

static int lookup_inode(struct apfsrw *fs, uint64_t file_id, uint64_t *size)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *info;
    uint32_t i;
    int err;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, fs->root_tree_paddr, node);
    if (err != APFSRW_OK)
        goto out;
    if ((rd16(&node->btn_flags) & APFS_BTNODE_LEAF) == 0) {
        err = APFSRW_ENOTSUP;
        goto out;
    }
    info = btree_info_for_node(fs, node);
    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        uint64_t key;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            goto out;
        if (key_len < sizeof(struct apfs_j_key) ||
            val_len < sizeof(struct apfs_j_inode_val))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_id(key) == file_id && key_type(key) == APFS_TYPE_INODE) {
            const struct apfs_j_inode_val *inode =
                (const struct apfs_j_inode_val *)valp;
            *size = rd64(&inode->uncompressed_size);
            err = APFSRW_OK;
            goto out;
        }
    }
    err = APFSRW_ENOENT;
out:
    free(node);
    return err;
}

static int lookup_file_extent(struct apfsrw *fs, uint64_t file_id,
    uint64_t *phys, uint64_t *len)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *info;
    uint32_t i;
    int err;

    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, fs->root_tree_paddr, node);
    if (err != APFSRW_OK)
        goto out;
    if ((rd16(&node->btn_flags) & APFS_BTNODE_LEAF) == 0) {
        err = APFSRW_ENOTSUP;
        goto out;
    }
    info = btree_info_for_node(fs, node);
    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        uint64_t key;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            goto out;
        if (key_len < sizeof(struct apfs_j_file_extent_key) ||
            val_len < sizeof(struct apfs_j_file_extent_val))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_id(key) == file_id &&
            key_type(key) == APFS_TYPE_FILE_EXTENT) {
            const struct apfs_j_file_extent_val *extent =
                (const struct apfs_j_file_extent_val *)valp;
            *len = rd64(&extent->len_and_flags) &
                APFS_FILE_EXTENT_LEN_MASK;
            *phys = rd64(&extent->phys_block_num);
            err = APFSRW_OK;
            goto out;
        }
    }
    err = APFSRW_ENOENT;
out:
    free(node);
    return err;
}

static int lookup_root_dirent(struct apfsrw *fs, const char *name,
    uint64_t *file_id)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *info;
    uint32_t i;
    size_t want_len;
    int err;

    if (name[0] == '/')
        name++;
    want_len = strlen(name);
    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, fs->root_tree_paddr, node);
    if (err != APFSRW_OK)
        goto out;
    info = btree_info_for_node(fs, node);
    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        uint64_t key;
        uint32_t name_len;
        const char *entry_name;
        const struct apfs_j_drec_val *drec;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            goto out;
        if (key_len < 13 || val_len < sizeof(*drec))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_id(key) != APFSRW_ROOT_FILEID ||
            key_type(key) != APFS_TYPE_DIR_REC)
            continue;
        name_len = rd32((const uint8_t *)keyp + 8) & 0x3ffU;
        if (name_len == 0 || 12U + name_len > key_len)
            continue;
        entry_name = (const char *)keyp + 12;
        if (name_len - 1 == want_len &&
            memcmp(entry_name, name, want_len) == 0) {
            drec = (const struct apfs_j_drec_val *)valp;
            *file_id = rd64(&drec->file_id);
            err = APFSRW_OK;
            goto out;
        }
    }
    err = APFSRW_ENOENT;
out:
    free(node);
    return err;
}

int apfsrw_open(const char *path, int writable, struct apfsrw **out)
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
    err = load_volume(fs);
    if (err != APFSRW_OK) {
        apfsrw_close(fs);
        return err;
    }
    *out = fs;
    return APFSRW_OK;
}

void apfsrw_close(struct apfsrw *fs)
{
    if (fs == NULL)
        return;
    if (fs->fd >= 0)
        close(fs->fd);
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

int apfsrw_list_root(struct apfsrw *fs, apfsrw_dirent_cb cb, void *ctx)
{
    struct apfs_btree_node_phys *node;
    const struct apfs_btree_info *info;
    uint32_t i;
    int err;

    if (fs == NULL || cb == NULL)
        return APFSRW_EINVAL;
    node = calloc(1, fs->block_size);
    if (node == NULL)
        return APFSRW_ENOMEM;
    err = read_object(fs, fs->root_tree_paddr, node);
    if (err != APFSRW_OK)
        goto out;
    if ((rd16(&node->btn_flags) & APFS_BTNODE_LEAF) == 0) {
        err = APFSRW_ENOTSUP;
        goto out;
    }
    info = btree_info_for_node(fs, node);
    for (i = 0; i < rd32(&node->btn_nkeys); i++) {
        const void *keyp;
        const void *valp;
        uint16_t key_len;
        uint16_t val_len;
        uint64_t key;
        uint32_t name_len;
        struct apfsrw_dirent entry;
        const struct apfs_j_drec_val *drec;

        err = btree_entry(fs, node, info, i, &keyp, &key_len, &valp,
            &val_len);
        if (err != APFSRW_OK)
            goto out;
        if (key_len < 13 || val_len < sizeof(*drec))
            continue;
        memcpy(&key, keyp, sizeof(key));
        if (key_id(key) != APFSRW_ROOT_FILEID ||
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
        err = cb(&entry, ctx);
        if (err != 0)
            goto out;
    }
    err = APFSRW_OK;
out:
    free(node);
    return err;
}

int apfsrw_read_root_file(struct apfsrw *fs, const char *path,
    uint8_t **data_out, size_t *size_out)
{
    uint64_t file_id;
    uint64_t file_size;
    uint64_t extent_phys;
    uint64_t extent_len;
    uint8_t *data;
    ssize_t n;
    int err;

    if (fs == NULL || path == NULL || data_out == NULL || size_out == NULL)
        return APFSRW_EINVAL;
    err = lookup_root_dirent(fs, path, &file_id);
    if (err != APFSRW_OK)
        return err;
    err = lookup_inode(fs, file_id, &file_size);
    if (err != APFSRW_OK)
        return err;
    if (file_size >= SIZE_MAX)
        return APFSRW_EOVERFLOW;
    if (file_size == 0) {
        data = malloc(1);
        if (data == NULL)
            return APFSRW_ENOMEM;
        data[0] = '\0';
        *data_out = data;
        *size_out = 0;
        return APFSRW_OK;
    }
    err = lookup_file_extent(fs, file_id, &extent_phys, &extent_len);
    if (err != APFSRW_OK)
        return err;
    if (file_size > extent_len)
        return APFSRW_EOVERFLOW;
    data = malloc((size_t)file_size + 1);
    if (data == NULL)
        return APFSRW_ENOMEM;
    n = pread(fs->fd, data, (size_t)file_size,
        (off_t)(extent_phys * fs->block_size));
    if (n < 0 || (uint64_t)n != file_size) {
        free(data);
        return APFSRW_EIO;
    }
    data[file_size] = '\0';
    *data_out = data;
    *size_out = (size_t)file_size;
    return APFSRW_OK;
}

int apfsrw_create_root_file(struct apfsrw *fs, const char *name,
    const void *data, size_t size)
{
    (void)fs;
    (void)name;
    (void)data;
    (void)size;
    return APFSRW_ENOTSUP;
}
