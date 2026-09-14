/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#ifndef _PUREDARWIN_APFS_H_
#define _PUREDARWIN_APFS_H_

#ifndef XNU_KERNEL_PRIVATE
#define XNU_KERNEL_PRIVATE 1
#endif

#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/vnode.h>
#include <stdint.h>

#define APFS_MODULE_NAME "apfs"
#define APFS_NX_MAGIC    0x4253584eU /* NX_MAGIC 'BSXN', bytes read as "NXSB" */
#define APFS_BS_BYTES    4096
#define APFS_MAX_CKSUM_SIZE 8
#define APFS_NX_MAX_FILE_SYSTEMS 100
#define APFS_ROOT_FILEID 2
#define APFS_APSB_MAGIC  0x42535041U /* APFS_MAGIC 'BSPA', bytes read as "APSB" */

#define APFS_OBJECT_TYPE_MASK 0x0000ffffU
#define APFS_OBJECT_TYPE_NX_SUPERBLOCK 0x00000001U
#define APFS_OBJECT_TYPE_BTREE 0x00000002U
#define APFS_OBJECT_TYPE_BTREE_NODE 0x00000003U
#define APFS_OBJECT_TYPE_SPACEMAN 0x00000005U
#define APFS_OBJECT_TYPE_SPACEMAN_CAB 0x00000006U
#define APFS_OBJECT_TYPE_SPACEMAN_CIB 0x00000007U
#define APFS_OBJECT_TYPE_SPACEMAN_BITMAP 0x00000008U
#define APFS_OBJECT_TYPE_OMAP 0x0000000bU
#define APFS_OBJECT_TYPE_CHECKPOINT_MAP 0x0000000cU
#define APFS_OBJECT_TYPE_FS 0x0000000dU
#define APFS_OBJECT_TYPE_FSTREE 0x0000000eU

#define APFS_CHECKPOINT_MAP_LAST 0x00000001U
#define APFS_CHECKPOINT_BLOCK_COUNT_MASK 0x7fffffffU

#define APFS_BTNODE_ROOT 0x0001
#define APFS_BTNODE_LEAF 0x0002
#define APFS_BTNODE_FIXED_KV_SIZE 0x0004

#define APFS_OBJ_ID_MASK 0x0fffffffffffffffULL
#define APFS_OBJ_TYPE_MASK 0xf000000000000000ULL
#define APFS_OBJ_TYPE_SHIFT 60
#define APFS_TYPE_INODE 3
#define APFS_TYPE_FILE_EXTENT 8
#define APFS_TYPE_XATTR 4
#define APFS_TYPE_DIR_REC 9

/* spec p.94-95 "Extended-Attribute Flags" */
#define APFS_XATTR_DATA_STREAM 0x0001U
#define APFS_XATTR_DATA_EMBEDDED 0x0002U
#define APFS_XATTR_FILE_SYSTEM_OWNED 0x0004U
/* spec p.83 "Extended Attributes": symlink targets live in this xattr. */
#define APFS_XATTR_SYMLINK_NAME "com.apple.fs.symlink"
/* spec p.132 "B-Tree Flags" */
#define APFS_BTREE_PHYSICAL 0x00000010U
#define APFS_BTREE_MAX_DEPTH 16U
#define APFS_DREC_LEN_MASK 0x000003ffU
#define APFS_DREC_HASH_SHIFT 10U
#define APFS_DREC_HASH_MASK 0xfffffc00U
/* spec p.96 APFS_INCOMPAT_CASE_INSENSITIVE */
#define APFS_INCOMPAT_CASE_INSENSITIVE 0x00000001ULL
/* spec p.111 "Extended-Field Types" */
#define APFS_INO_EXT_TYPE_NAME 4U
#define APFS_INO_EXT_TYPE_DSTREAM 8U
/* j_inode_flags: INODE_NO_RSRC_FORK */
#define APFS_INODE_NO_RSRC_FORK 0x8000ULL
/* j_obj_types / j_obj_kinds */
#define APFS_TYPE_EXTENT 2U
#define APFS_KIND_NEW 1U

#define APFSLOG(fmt, args...) \
	do { printf("apfs: " fmt "\n", ## args); } while (0)

int apfs_vfs_register(void);
int apfs_vfs_unregister(void);

extern struct vnodeopv_desc apfs_vnodeop_opv_desc;

typedef int64_t apfs_paddr_t;
typedef uint64_t apfs_oid_t;
typedef uint64_t apfs_xid_t;

struct apfs_prange {
	apfs_paddr_t pr_start_paddr;
	uint64_t pr_block_count;
};

struct apfs_obj_phys {
	uint8_t o_cksum[APFS_MAX_CKSUM_SIZE];
	apfs_oid_t o_oid;
	apfs_xid_t o_xid;
	uint32_t o_type;
	uint32_t o_subtype;
};

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
};

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

struct apfs_chunk_info {
	apfs_xid_t ci_xid;
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

struct apfs_spaceman_free_queue {
	uint64_t sfq_count;
	apfs_oid_t sfq_tree_oid;
	apfs_xid_t sfq_oldest_xid;
	uint16_t sfq_tree_node_limit;
	uint16_t sfq_pad16;
	uint32_t sfq_pad32;
	uint64_t sfq_reserved;
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
	struct apfs_spaceman_free_queue sm_fq[3];
	uint16_t sm_ip_bm_free_head;
	uint16_t sm_ip_bm_free_tail;
	uint32_t sm_ip_bm_xid_offset;
	uint32_t sm_ip_bitmap_offset;
	uint32_t sm_ip_bm_free_next_offset;
	uint32_t sm_version;
	uint32_t sm_struct_size;
} __attribute__((packed));

struct apfs_nloc {
	uint16_t off;
	uint16_t len;
} __attribute__((packed));

struct apfs_kvloc {
	struct apfs_nloc k;
	struct apfs_nloc v;
} __attribute__((packed));

struct apfs_kvoff {
	uint16_t k;
	uint16_t v;
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

#define APFS_FILE_EXTENT_LEN_MASK 0x00ffffffffffffffULL

struct apfs_inode_info {
	uint64_t fileid;
	enum vtype type;
	uint16_t mode;
	uint32_t uid;
	uint32_t gid;
	uint64_t size;
	uint32_t nlink;
	uint64_t parent_id;
};

struct apfsrw;
struct apfs_node;

#define APFS_NODE_HASH_SIZE 256
#define APFS_NODE_HASH(id) ((uint32_t)((id) & (APFS_NODE_HASH_SIZE - 1)))

struct apfs_mount {
	struct mount *mp;
	/* Shared write path (projects/libapfsrw), bound to devvp at mount. */
	struct apfsrw *rw;
	/*
	 * One in-core vnode per file id. Minting a fresh vnode per lookup breaks
	 * anything that hangs state off the vnode - notably AF_UNIX sockets,
	 * where unp_bind() stores the listener in vp->v_socket and unp_connect()
	 * reads it back off the vnode it gets from the path lookup.
	 */
	void *am_hash_lock;		/* IOLock* */
	int am_probe_logged;		/* container dumped to the log once */
	void *am_rw_lock;		/* IOLock*: serialises every apfsrw
					 * mutation + the reload after it */
	LIST_HEAD(apfs_node_bucket, apfs_node) am_node_hash[APFS_NODE_HASH_SIZE];
	vnode_t devvp;
	vnode_t root_vp;
	dev_t dev;
	int dev_opened;
	uint32_t block_size;
	uint64_t block_count;
	uint32_t max_file_systems;
	struct apfs_nx_superblock nx;
	struct apfs_superblock apfs;
	apfs_xid_t xid;
	apfs_oid_t fs_oid;
	apfs_paddr_t fs_paddr;
	apfs_oid_t container_omap_oid;
	apfs_paddr_t container_omap_paddr;
	apfs_paddr_t container_omap_tree_paddr;
	apfs_oid_t spaceman_oid;
	apfs_paddr_t spaceman_paddr;
	apfs_oid_t volume_omap_oid;
	apfs_paddr_t volume_omap_paddr;
	apfs_paddr_t volume_omap_tree_paddr;
	apfs_oid_t root_tree_oid;
	apfs_paddr_t root_tree_paddr;
};

struct apfs_node {
	LIST_ENTRY(apfs_node) a_hash;	/* am_node_hash linkage */
	int a_alloc_wip;		/* vnode_create() in progress */
	int a_unhashed;
	struct apfs_mount *amp;
	vnode_t vp;
	uint64_t fileid;
	enum vtype type;
	uint16_t mode;
	uint32_t uid;
	uint32_t gid;
	uint64_t size;
	uint32_t nlink;
	uint64_t parent_id;
};

#define VFSTOAPFS(mp) ((struct apfs_mount *)vfs_fsprivate(mp))
#define VTOAPFS(vp)   ((struct apfs_node *)vnode_fsnode(vp))

int apfs_vget(struct apfs_mount *amp, uint64_t fileid, vnode_t dvp,
    vnode_t *vpp);
int apfs_load_volume(struct apfs_mount *amp, vfs_context_t ctx);
int apfs_lookup_inode(struct apfs_mount *amp, uint64_t fileid,
    struct apfs_inode_info *info);
int apfs_lookup_dirent(struct apfs_mount *amp, uint64_t dirid,
    const char *name, size_t namelen, uint64_t *fileid, uint8_t *dtype);
int apfs_iterate_dir(struct apfs_mount *amp, uint64_t dirid,
    off_t start_index, struct uio *uio, int *numdirent, int *eofflag);
int apfs_read_file(struct apfs_node *node, struct uio *uio);
int apfs_lookup_xattr(struct apfs_mount *amp, uint64_t fileid, const char *name,
    void *buf, size_t bufsize, size_t *outlen);
int apfs_reload_container(struct apfs_mount *amp, vfs_context_t ctx);

#endif /* _PUREDARWIN_APFS_H_ */
