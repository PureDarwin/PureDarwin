/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfs.h"

#include <IOKit/IOLocks.h>
#include <libkern/OSByteOrder.h>
#include <mach/mach_time.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/ubc.h>
#include <sys/uio.h>
#include <sys/vnode_if.h>
#include <string.h>

typedef int (*apfs_leaf_cb)(struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx);
static int apfs_btree_walk_leaves_oid(struct apfs_mount *amp,
    uint64_t oid_min, uint64_t oid_max, apfs_leaf_cb cb, void *ctx,
    const char *tag);
static int apfs_omap_lookup_tree(struct apfs_mount *amp,
    apfs_paddr_t tree_paddr, apfs_oid_t oid, apfs_xid_t xid,
    struct apfs_omap_val *out);

static uint16_t
le16(uint16_t v)
{
	return OSSwapLittleToHostInt16(v);
}

static uint32_t
le32(uint32_t v)
{
	return OSSwapLittleToHostInt32(v);
}

static uint64_t
le64(uint64_t v)
{
	return OSSwapLittleToHostInt64(v);
}

static int64_t
le64s(int64_t v)
{
	return (int64_t)OSSwapLittleToHostInt64((uint64_t)v);
}

static uint16_t
hle16(uint16_t v)
{
	return OSSwapHostToLittleInt16(v);
}

static uint32_t
hle32(uint32_t v)
{
	return OSSwapHostToLittleInt32(v);
}

static uint64_t
hle64(uint64_t v)
{
	return OSSwapHostToLittleInt64(v);
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
		lo = (lo + le32(word)) % 0xffffffffULL;
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
	if (le64(expected) != actual)
		return EINVAL;
	return 0;
}

static uint32_t
apfs_object_type(uint32_t type)
{
	return le32(type) & APFS_OBJECT_TYPE_MASK;
}

static uint64_t
apfs_key_id(uint64_t obj_id_and_type)
{
	return le64(obj_id_and_type) & APFS_OBJ_ID_MASK;
}

static uint8_t
apfs_key_type(uint64_t obj_id_and_type)
{
	return (uint8_t)((le64(obj_id_and_type) & APFS_OBJ_TYPE_MASK) >>
	    APFS_OBJ_TYPE_SHIFT);
}

static int
apfs_read_phys(struct apfs_mount *amp, apfs_paddr_t paddr, void *out,
    size_t out_size)
{
	buf_t bp = NULL;
	int error;

	if (amp == NULL || amp->io_devvp == NULLVP || out == NULL)
		return EINVAL;
	if (paddr < 0 || out_size > amp->block_size)
		return EINVAL;

	error = (int)buf_meta_bread(amp->io_devvp, apfs_devblk(amp, paddr),
	    amp->block_size, NOCRED, &bp);
	if (error) {
		if (bp)
			buf_brelse(bp);
		return error;
	}
	memcpy(out, (const void *)buf_dataptr(bp), out_size);
	buf_brelse(bp);
	return 0;
}

static int
apfs_read_object_phys(struct apfs_mount *amp, apfs_paddr_t paddr, void *out)
{
	int error;

	error = apfs_read_phys(amp, paddr, out, amp->block_size);
	if (error)
		return error;
	error = apfs_verify_object_checksum(out, amp->block_size);
	if (error) {
		static int cksum_log_budget = 12;

		if (cksum_log_budget-- > 0) {
			const uint8_t *b = (const uint8_t *)out;
			uint64_t cks, oid, xid;
			uint32_t ot;

			memcpy(&cks, b, 8);
			memcpy(&oid, b + 8, 8);
			memcpy(&xid, b + 16, 8);
			memcpy(&ot, b + 24, 4);
			APFSLOG("bad object checksum at paddr 0x%llx: cks=0x%llx "
			    "oid=0x%llx xid=%llu otype=0x%x head=%02x %02x %02x %02x",
			    (unsigned long long)paddr, (unsigned long long)cks,
			    (unsigned long long)oid, (unsigned long long)xid, ot,
			    b[32], b[33], b[34], b[35]);
		}
		return error;
	}
	return 0;
}

static int
apfs_read_object_prefix(struct apfs_mount *amp, apfs_paddr_t paddr, void *out,
    size_t out_size)
{
	void *block;
	int error;

	if (out == NULL || out_size > amp->block_size)
		return EINVAL;
	block = _MALLOC(amp->block_size, M_TEMP, M_WAITOK);
	if (block == NULL)
		return ENOMEM;

	error = apfs_read_object_phys(amp, paddr, block);
	if (error == 0)
		memcpy(out, block, out_size);
	_FREE(block, M_TEMP);
	return error;
}

static int
apfs_copy_phys(struct apfs_mount *amp, apfs_paddr_t paddr, size_t offset,
    size_t count, void *dst)
{
	buf_t bp = NULL;
	int error;

	if (paddr < 0 || offset > amp->block_size ||
	    count > amp->block_size - offset)
		return EINVAL;
	error = (int)buf_meta_bread(amp->io_devvp, apfs_devblk(amp, paddr),
	    amp->block_size, NOCRED, &bp);
	if (error) {
		if (bp)
			buf_brelse(bp);
		return error;
	}
	memcpy(dst, (const char *)buf_dataptr(bp) + offset, count);
	buf_brelse(bp);
	return 0;
}

// Whole-block file data straight from the device in one request.
// Skips the buffer cache: commits flush data blocks before their extents are visible
#define APFS_MAX_RUN_BYTES	(1024u * 1024u)

static int
apfs_read_run(struct apfs_mount *amp, apfs_paddr_t paddr, size_t nbytes,
    void *dst)
{
	buf_t bp;
	int error;

	bp = buf_alloc(amp->io_devvp);
	if (bp == NULL)
		return ENOMEM;
	buf_setflags(bp, B_READ);
	buf_setblkno(bp, apfs_devblk(amp, paddr));
	buf_setlblkno(bp, apfs_devblk(amp, paddr));
	buf_setcount(bp, (uint32_t)nbytes);
	buf_setdataptr(bp, (uintptr_t)dst);
	error = VNOP_STRATEGY(bp);
	if (error == 0)
		error = buf_biowait(bp);
	if (error == 0 && buf_resid(bp) != 0)
		error = EIO;
	buf_free(bp);
	return error;
}

static const struct apfs_btree_info *
apfs_btree_info_for_node(const struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node)
{
	uint16_t flags = le16(node->btn_flags);

	if ((flags & APFS_BTNODE_ROOT) == 0)
		return NULL;
	return (const struct apfs_btree_info *)
	    ((const uint8_t *)node + amp->block_size -
	    sizeof(struct apfs_btree_info));
}

static int
apfs_btree_entry(const struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node, const struct apfs_btree_info *info,
    uint32_t index, const void **key, uint16_t *key_len, const void **val,
    uint16_t *val_len)
{
	const uint8_t *base = (const uint8_t *)node;
	uint16_t flags = le16(node->btn_flags);
	uint32_t nkeys = le32(node->btn_nkeys);
	uint16_t table_off = le16(node->btn_table_space.off);
	uint16_t table_len = le16(node->btn_table_space.len);
	uint32_t data_off = offsetof(struct apfs_btree_node_phys, btn_data);
	uint32_t key_base = data_off + table_off + table_len;
	uint32_t val_end = amp->block_size;
	uint16_t k_off, k_len, v_off, v_len;

	if (index >= nkeys)
		return ERANGE;
	if (flags & APFS_BTNODE_ROOT)
		val_end -= sizeof(struct apfs_btree_info);

	if (flags & APFS_BTNODE_FIXED_KV_SIZE) {
		const struct apfs_kvoff *toc = (const struct apfs_kvoff *)
		    (base + data_off + table_off + index * sizeof(*toc));

		if (info == NULL)
			return EINVAL;
		k_off = le16(toc->k);
		v_off = le16(toc->v);
		k_len = (uint16_t)le32(info->bt_key_size);
		// bt_val_size describes LEAF values.
		// a nonleaf node's value is the child's oid_t regardless
		v_len = (flags & APFS_BTNODE_LEAF)
		    ? (uint16_t)le32(info->bt_val_size)
		    : (uint16_t)sizeof(apfs_oid_t);
	} else {
		const struct apfs_kvloc *toc = (const struct apfs_kvloc *)
		    (base + data_off + table_off + index * sizeof(*toc));

		k_off = le16(toc->k.off);
		k_len = le16(toc->k.len);
		v_off = le16(toc->v.off);
		v_len = le16(toc->v.len);
	}

	if (k_len == 0 || key_base + k_off + k_len > amp->block_size)
		return EINVAL;
	if (v_off == 0xffff)
		return ENOENT;
	if (v_len == 0 || v_off > val_end || v_len > v_off)
		return EINVAL;

	*key = base + key_base + k_off;
	*key_len = k_len;
	*val = base + val_end - v_off;
	*val_len = v_len;
	return 0;
}

static int
apfs_omap_lookup_tree(struct apfs_mount *amp, apfs_paddr_t tree_paddr,
    apfs_oid_t oid, apfs_xid_t xid, struct apfs_omap_val *out)
{
	struct apfs_btree_node_phys *node;
	const struct apfs_btree_info *info;
	struct apfs_btree_info info_copy;
	struct apfs_omap_val best;
	apfs_xid_t best_xid = 0;
	uint32_t i;
	int found = 0;
	int error = 0;

	node = (struct apfs_btree_node_phys *)_MALLOC(amp->block_size, M_TEMP,
	    M_WAITOK);
	if (node == NULL)
		return ENOMEM;

	error = apfs_read_object_phys(amp, tree_paddr, node);
	if (error)
		goto out;

	if (apfs_object_type(node->btn_o.o_type) != APFS_OBJECT_TYPE_BTREE &&
	    apfs_object_type(node->btn_o.o_type) != APFS_OBJECT_TYPE_BTREE_NODE) {
		error = EINVAL;
		goto out;
	}

	{
		const struct apfs_btree_info *root_info =
		    apfs_btree_info_for_node(amp, node);

		if (root_info == NULL) {
			error = EINVAL;
			goto out;
		}
		memcpy(&info_copy, root_info, sizeof(info_copy));
		info = &info_copy;
	}

	// Descend to the leaf that would hold this oid. Trees sort by oid then xid (spec p.123).
	// Child links are physical, and only the root carries a btree_info_t (spec p.126)
	{
		uint32_t guard = 0;

		while (le16(node->btn_level) != 0) {
			apfs_paddr_t child = -1;

			for (i = 0; i < le32(node->btn_nkeys); i++) {
				const struct apfs_omap_key *k;
				const void *keyp, *valp;
				uint16_t key_len, val_len;

				error = apfs_btree_entry(amp, node, info, i,
				    &keyp, &key_len, &valp, &val_len);
				if (error)
					goto out;
				if (key_len < sizeof(*k) ||
				    val_len < sizeof(apfs_oid_t))
					continue;
				k = (const struct apfs_omap_key *)keyp;
				// Take the last child whose key does not exceed (oid, xid).
				// Entry 0 also covers everything below its own key, so seed with it
				if (i == 0 || le64(k->ok_oid) < oid ||
				    (le64(k->ok_oid) == oid &&
				    le64(k->ok_xid) <= xid)) {
					apfs_oid_t c;

					memcpy(&c, valp, sizeof(c));
					child = (apfs_paddr_t)le64(c);
				} else {
					break;
				}
			}
			if (child <= 0) {
				error = ENOENT;
				goto out;
			}
			if (++guard > APFS_BTREE_MAX_DEPTH) {
				error = EINVAL;
				goto out;
			}
			error = apfs_read_object_phys(amp, child, node);
			if (error)
				goto out;
		}
	}
	for (i = 0; i < le32(node->btn_nkeys); i++) {
		const struct apfs_omap_key *key;
		const struct apfs_omap_val *val;
		const void *keyp, *valp;
		uint16_t key_len, val_len;
		apfs_oid_t key_oid;
		apfs_xid_t key_xid;

		error = apfs_btree_entry(amp, node, info, i, &keyp, &key_len,
		    &valp, &val_len);
		if (error)
			goto out;
		if (key_len < sizeof(*key) || val_len < sizeof(*val))
			continue;

		key = (const struct apfs_omap_key *)keyp;
		val = (const struct apfs_omap_val *)valp;
		key_oid = le64(key->ok_oid);
		key_xid = le64(key->ok_xid);
		if (key_oid != oid || key_xid > xid)
			continue;
		if (!found || key_xid >= best_xid) {
			memcpy(&best, val, sizeof(best));
			best_xid = key_xid;
			found = 1;
		}
	}

	if (!found) {
		error = ENOENT;
		goto out;
	}

	memcpy(out, &best, sizeof(best));
out:
	_FREE(node, M_TEMP);
	return error;
}

static int
apfs_read_omap(struct apfs_mount *amp, apfs_paddr_t paddr,
    struct apfs_omap_phys *omap)
{
	int error = apfs_read_object_prefix(amp, paddr, omap, sizeof(*omap));

	if (error)
		return error;
	if (apfs_object_type(omap->om_o.o_type) != APFS_OBJECT_TYPE_OMAP)
		return EINVAL;
	return 0;
}

static int
apfs_omap_lookup(struct apfs_mount *amp, apfs_paddr_t omap_paddr,
    apfs_oid_t oid, apfs_xid_t xid, struct apfs_omap_val *out,
    apfs_paddr_t *tree_paddr)
{
	struct apfs_omap_phys omap;
	int error;

	error = apfs_read_omap(amp, omap_paddr, &omap);
	if (error)
		return error;
	if (tree_paddr)
		*tree_paddr = le64s(omap.om_tree_oid);
	return apfs_omap_lookup_tree(amp, le64s(omap.om_tree_oid), oid, xid, out);
}

int
apfs_load_volume(struct apfs_mount *amp, __unused vfs_context_t ctx)
{
	struct apfs_omap_phys omap;
	struct apfs_omap_val ov;
	apfs_oid_t fs_oid;
	uint32_t max_fs = amp->max_file_systems;
	int error;

	if (max_fs > APFS_NX_MAX_FILE_SYSTEMS)
		max_fs = APFS_NX_MAX_FILE_SYSTEMS;
	if (max_fs == 0)
		return ENOENT;

	amp->xid = le64(amp->nx.nx_o.o_xid);
	amp->container_omap_oid = le64(amp->nx.nx_omap_oid);
	amp->container_omap_paddr = (apfs_paddr_t)amp->container_omap_oid;

	error = apfs_read_omap(amp, amp->container_omap_paddr, &omap);
	if (error) {
		APFSLOG("container omap 0x%llx read failed: %d",
		    (unsigned long long)amp->container_omap_oid, error);
		return error;
	}
	amp->container_omap_tree_paddr = le64s(omap.om_tree_oid);

	fs_oid = amp->vol_slot < max_fs ?
	    le64(amp->nx.nx_fs_oid[amp->vol_slot]) : 0;
	if (fs_oid == 0)
		return ENOENT;
	error = apfs_omap_lookup_tree(amp, amp->container_omap_tree_paddr,
	    fs_oid, amp->xid, &ov);
	if (error) {
		APFSLOG("volume oid 0x%llx omap lookup failed: %d",
		    (unsigned long long)fs_oid, error);
		return error;
	}

	amp->fs_oid = fs_oid;
	amp->fs_paddr = le64s(ov.ov_paddr);
	error = apfs_read_object_prefix(amp, amp->fs_paddr, &amp->apfs,
	    sizeof(amp->apfs));
	if (error)
		return error;
	if (le32(amp->apfs.apfs_magic) != APFS_APSB_MAGIC)
		return EINVAL;

	amp->volume_omap_oid = le64(amp->apfs.apfs_omap_oid);
	amp->volume_omap_paddr = (apfs_paddr_t)amp->volume_omap_oid;
	amp->root_tree_oid = le64(amp->apfs.apfs_root_tree_oid);

	error = apfs_omap_lookup(amp, amp->volume_omap_paddr,
	    amp->root_tree_oid, amp->xid, &ov, &amp->volume_omap_tree_paddr);
	if (error) {
		APFSLOG("root tree oid 0x%llx omap lookup failed: %d",
		    (unsigned long long)amp->root_tree_oid, error);
		return error;
	}
	amp->root_tree_paddr = le64s(ov.ov_paddr);

	if (!amp->am_probe_logged) {
		APFSLOG("volume oid=0x%llx paddr=0x%llx omap=0x%llx "
		    "root_tree=0x%llx->0x%llx",
		    (unsigned long long)amp->fs_oid,
		    (unsigned long long)amp->fs_paddr,
		    (unsigned long long)amp->volume_omap_oid,
		    (unsigned long long)amp->root_tree_oid,
		    (unsigned long long)amp->root_tree_paddr);
		amp->am_probe_logged = 1;
	}
	return 0;
}

// Optional exact target for a directory-record lookup:
// descend by key instead of visiting every leaf that holds the directory's records
struct apfs_walk_target {
	uint64_t dirid;
	uint32_t hash;		// 22-bit name hash
};

// Order of j_drec_hashed_key_t: object id, type, then the 22-bit hash.
// Names only break ties, which the leaf callback resolves
static int
apfs_drec_key_cmp(const void *keyp, uint16_t key_len,
    const struct apfs_walk_target *t)
{
	uint64_t id_type;
	uint32_t len_hash, hash;
	uint64_t id;
	uint8_t type;

	if (key_len < sizeof(uint64_t))
		return -1;
	memcpy(&id_type, keyp, sizeof(id_type));
	id = apfs_key_id(id_type);
	type = apfs_key_type(id_type);
	if (id != t->dirid)
		return id < t->dirid ? -1 : 1;
	if (type != APFS_TYPE_DIR_REC)
		return type < APFS_TYPE_DIR_REC ? -1 : 1;
	if (key_len < sizeof(uint64_t) + sizeof(uint32_t))
		return -1;
	memcpy(&len_hash, (const uint8_t *)keyp + sizeof(uint64_t),
	    sizeof(len_hash));
	hash = le32(len_hash) >> 10;
	if (hash != t->hash)
		return hash < t->hash ? -1 : 1;
	return 0;
}

// CRC-32C, reflected. Only for the directory record name hash
static uint32_t
apfs_crc32c(const uint8_t *data, size_t len)
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

// Spec p.78-79: NFD name as UTF-32, case-folded on case-insensitive volumes,
// CRC-32C complemented, low 22 bits. ASCII only. Other names fall back to the full walk
static int
apfs_drec_name_hash(const struct apfs_mount *amp, const char *name,
    size_t len, uint32_t *out)
{
	uint8_t *utf32;
	size_t i;
	int fold = (le64(amp->apfs.apfs_incompatible_features) &
	    APFS_INCOMPAT_CASE_INSENSITIVE) != 0;

	if (len == 0 || len > 255)
		return EINVAL;
	for (i = 0; i < len; i++)
		if ((uint8_t)name[i] >= 0x80)
			return ENOTSUP;
	utf32 = (uint8_t *)_MALLOC(len * 4, M_TEMP, M_WAITOK | M_ZERO);
	if (utf32 == NULL)
		return ENOMEM;
	for (i = 0; i < len; i++) {
		uint8_t c = (uint8_t)name[i];

		if (fold && c >= 'A' && c <= 'Z')
			c = (uint8_t)(c - 'A' + 'a');
		utf32[i * 4] = c;
	}
	*out = (~apfs_crc32c(utf32, len * 4)) & 0x003fffffU;
	_FREE(utf32, M_TEMP);
	return 0;
}

static int
apfs_btree_walk_node(struct apfs_mount *amp, apfs_paddr_t paddr,
    const struct apfs_btree_info *root_info, uint32_t depth,
    uint64_t oid_min, uint64_t oid_max, apfs_leaf_cb cb, void *ctx,
    const struct apfs_walk_target *target)
{
	struct apfs_btree_node_phys *node;
	const struct apfs_btree_info *info;
	struct apfs_btree_info info_storage;
	const struct apfs_btree_info *own;
	uint32_t nkeys, i;
	int error;

	if (depth > APFS_BTREE_MAX_DEPTH)
		return EINVAL;
	node = (struct apfs_btree_node_phys *)_MALLOC(amp->block_size, M_TEMP,
	    M_WAITOK);
	if (node == NULL)
		return ENOMEM;
	error = apfs_read_object_phys(amp, paddr, node);
	if (error)
		goto out;

	own = apfs_btree_info_for_node(amp, node);
	if (own != NULL) {
		memcpy(&info_storage, own, sizeof(info_storage));
		info = &info_storage;
	} else {
		info = root_info;
	}

	if (le16(node->btn_level) == 0) {
		error = cb(amp, node, info, ctx);
		goto out;
	}

	nkeys = le32(node->btn_nkeys);
	if (target != NULL) {
		// Child i covers [key_i, key_i+1). Take the last child whose key is <= the target,
		// plus its left neighbour when the boundary hash is equal
		int best = -1, tie = 0;

		for (i = 0; i < nkeys; i++) {
			const void *keyp, *valp;
			uint16_t key_len, val_len;
			int c;

			error = apfs_btree_entry(amp, node, info, i, &keyp,
			    &key_len, &valp, &val_len);
			if (error)
				goto out;
			c = apfs_drec_key_cmp(keyp, key_len, target);
			if (c > 0)
				break;
			best = (int)i;
			tie = (c == 0);
		}
		if (best < 0) {
			error = 0;
			goto out;
		}
		for (i = (tie && best > 0) ? (uint32_t)best - 1 : (uint32_t)best;
		    i <= (uint32_t)best; i++) {
			const void *keyp, *valp;
			uint16_t key_len, val_len;
			apfs_oid_t child;
			apfs_paddr_t child_paddr;

			error = apfs_btree_entry(amp, node, info, i, &keyp,
			    &key_len, &valp, &val_len);
			if (error)
				goto out;
			if (val_len < sizeof(apfs_oid_t))
				continue;
			memcpy(&child, valp, sizeof(child));
			child = le64(child);
			if (info != NULL &&
			    (le32(info->bt_flags) & APFS_BTREE_PHYSICAL) != 0) {
				child_paddr = (apfs_paddr_t)child;
			} else {
				struct apfs_omap_val ov;

				error = apfs_omap_lookup_tree(amp,
				    amp->volume_omap_tree_paddr, child, amp->xid,
				    &ov);
				if (error)
					goto out;
				child_paddr = (apfs_paddr_t)le64(ov.ov_paddr);
			}
			error = apfs_btree_walk_node(amp, child_paddr, info,
			    depth + 1, oid_min, oid_max, cb, ctx, target);
			if (error)
				goto out;
		}
		error = 0;
		goto out;
	}
	for (i = 0; i < nkeys; i++) {
		const void *keyp, *valp;
		uint16_t key_len, val_len;
		apfs_oid_t child;
		apfs_paddr_t child_paddr;
		uint64_t sep;

		error = apfs_btree_entry(amp, node, info, i, &keyp, &key_len,
		    &valp, &val_len);
		if (error)
			goto out;
		if (val_len < sizeof(apfs_oid_t))
			continue;

		if (key_len >= sizeof(uint64_t)) {
			memcpy(&sep, keyp, sizeof(sep));
			if (apfs_key_id(sep) > oid_max)
				break;
		}
		if (i + 1 < nkeys) {
			const void *nkeyp, *nvalp;
			uint16_t nkey_len, nval_len;

			if (apfs_btree_entry(amp, node, info, i + 1, &nkeyp,
			    &nkey_len, &nvalp, &nval_len) == 0 &&
			    nkey_len >= sizeof(uint64_t)) {
				memcpy(&sep, nkeyp, sizeof(sep));
				if (apfs_key_id(sep) < oid_min)
					continue;
			}
		}

		memcpy(&child, valp, sizeof(child));
		child = le64(child);

		if (info != NULL &&
		    (le32(info->bt_flags) & APFS_BTREE_PHYSICAL) != 0) {
			child_paddr = (apfs_paddr_t)child;
		} else {
			struct apfs_omap_val ov;

			error = apfs_omap_lookup_tree(amp,
			    amp->volume_omap_tree_paddr, child, amp->xid, &ov);
			if (error)
				goto out;
			child_paddr = (apfs_paddr_t)le64(ov.ov_paddr);
		}

		error = apfs_btree_walk_node(amp, child_paddr, info, depth + 1,
		    oid_min, oid_max, cb, ctx, NULL);
		if (error)
			goto out;
	}
	error = 0;
out:
	_FREE(node, M_TEMP);
	return error;
}

// Visit only the leaves that can hold records for object ids in [oid_min, oid_max].
// Pass 0 / UINT64_MAX to visit the whole tree
static int
apfs_btree_walk_leaves_oid(struct apfs_mount *amp, uint64_t oid_min,
    uint64_t oid_max, apfs_leaf_cb cb, void *ctx, const char *tag)
{
	int error;

	// A commit frees and quickly reuses tree blocks,
	// so the root must be read and the whole walk done under the lock. It is recursive
	apfs_rw_lock_tag(amp, tag);
	error = apfs_btree_walk_node(amp, amp->root_tree_paddr, NULL, 0,
	    oid_min, oid_max, cb, ctx, NULL);
	apfs_rw_unlock(amp);
	return error;
}

// Directory record lookup by (dirid, name hash): one root-to-leaf path
static int
apfs_btree_walk_drec(struct apfs_mount *amp,
    const struct apfs_walk_target *target, apfs_leaf_cb cb, void *ctx,
    const char *tag)
{
	int error;

	apfs_rw_lock_tag(amp, tag);
	error = apfs_btree_walk_node(amp, amp->root_tree_paddr, NULL, 0,
	    target->dirid, target->dirid, cb, ctx, target);
	apfs_rw_unlock(amp);
	return error;
}

static enum vtype
apfs_vtype_from_mode(uint16_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFDIR:
		return VDIR;
	case S_IFREG:
		return VREG;
	case S_IFLNK:
		return VLNK;
	case S_IFCHR:
		return VCHR;
	case S_IFBLK:
		return VBLK;
	case S_IFIFO:
		return VFIFO;
	case S_IFSOCK:
		return VSOCK;
	default:
		return VNON;
	}
}

static int
apfs_inode_dstream_size(const void *val, uint16_t val_len, uint64_t *out)
{
	const uint8_t *p = (const uint8_t *)val;
	uint32_t fixed = (uint32_t)sizeof(struct apfs_j_inode_val);
	uint32_t num, i, desc, data;
	uint16_t n16;

	if (val_len < fixed + 4U)
		return ENOENT;
	memcpy(&n16, p + fixed, sizeof(n16));
	num = le16(n16);
	desc = fixed + 4U;
	data = desc + num * 4U;
	if (data > val_len)
		return EINVAL;
	for (i = 0; i < num; i++) {
		uint8_t type = p[desc + i * 4U];
		uint16_t size;

		memcpy(&size, p + desc + i * 4U + 2U, sizeof(size));
		size = le16(size);
		if (data + size > val_len)
			return EINVAL;
		if (type == APFS_INO_EXT_TYPE_DSTREAM) {
			uint64_t sz;

			if (size < sizeof(sz))
				return EINVAL;
			memcpy(&sz, p + data, sizeof(sz));
			*out = le64(sz);
			return 0;
		}
		data += ((uint32_t)size + 7U) & ~7U;
	}
	return ENOENT;
}

struct apfs_inode_lookup_ctx {
	uint64_t fileid;
	struct apfs_inode_info *info_out;
	int found;
};

static int
apfs_lookup_inode_cb(struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
	struct apfs_inode_lookup_ctx *c = (struct apfs_inode_lookup_ctx *)ctx;
	struct apfs_inode_info *info_out = c->info_out;
	uint32_t i;
	int error;

	for (i = 0; i < le32(node->btn_nkeys); i++) {
		const struct apfs_j_key *key;
		const struct apfs_j_inode_val *val;
		const void *keyp, *valp;
		uint16_t key_len, val_len;
		uint16_t mode;
		uint64_t dsize;

		error = apfs_btree_entry(amp, node, info, i, &keyp, &key_len,
		    &valp, &val_len);
		if (error)
			return error;
		if (key_len < sizeof(*key) || val_len < sizeof(*val))
			continue;

		key = (const struct apfs_j_key *)keyp;
		if (apfs_key_id(key->obj_id_and_type) != c->fileid ||
		    apfs_key_type(key->obj_id_and_type) != APFS_TYPE_INODE)
			continue;

		val = (const struct apfs_j_inode_val *)valp;
		mode = le16(val->mode);
		memset(info_out, 0, sizeof(*info_out));
		info_out->fileid = c->fileid;
		info_out->type = apfs_vtype_from_mode(mode);
		// Mkapfs leaves the root directory with mode 0.
		// as the root of a non-root mount a VNON vnode panics namei
		if (c->fileid == APFS_ROOT_FILEID && info_out->type == VNON)
			info_out->type = VDIR;
		info_out->mode = mode & 07777;
		info_out->uid = le32(val->owner);
		info_out->gid = le32(val->group);
		info_out->bsd_flags = le32(val->bsd_flags);
		info_out->atime_ns = le64(val->access_time);
		info_out->mtime_ns = le64(val->mod_time);
		info_out->ctime_ns = le64(val->change_time);
		info_out->crtime_ns = le64(val->create_time);
		if (apfs_inode_dstream_size(valp, val_len, &dsize) == 0)
			info_out->size = dsize;
		else
			info_out->size = le64(val->uncompressed_size);
		info_out->parent_id = le64(val->parent_id);
		if (info_out->type == VDIR)
			info_out->nlink = (uint32_t)le32((uint32_t)val->u.nchildren) + 2;
		else
			info_out->nlink = (uint32_t)le32((uint32_t)val->u.nlink);
		c->found = 1;
		return 1;			// Stop the walk
	}
	return 0;
}

int
apfs_lookup_inode(struct apfs_mount *amp, uint64_t fileid,
    struct apfs_inode_info *info_out)
{
	struct apfs_inode_lookup_ctx c;
	int error;

	if (amp == NULL || info_out == NULL)
		return EINVAL;
	c.fileid = fileid;
	c.info_out = info_out;
	c.found = 0;
	error = apfs_btree_walk_leaves_oid(amp, fileid,
	    fileid, apfs_lookup_inode_cb, &c, __func__);
	if (error != 0 && error != 1)
		return error;
	return c.found ? 0 : ENOENT;
}

static int
apfs_emit_dirent(uint64_t fileid, uint8_t type, const char *name,
    uint16_t namelen, struct uio *uio)
{
	struct dirent dent;
	uint16_t reclen;

	if (namelen > NAME_MAX)
		namelen = NAME_MAX;

	memset(&dent, 0, sizeof(dent));
	dent.d_ino = (ino_t)fileid;
	dent.d_type = type;
	dent.d_namlen = (uint8_t)namelen;
	memcpy(dent.d_name, name, namelen);
	dent.d_name[namelen] = '\0';
	reclen = (uint16_t)((offsetof(struct dirent, d_name) + namelen + 1 + 3) & ~3);
	dent.d_reclen = reclen;

	if (uio_resid(uio) < reclen)
		return EMSGSIZE;
	return uiomove((caddr_t)&dent, reclen, uio);
}

static int
apfs_parse_dir_key(const void *keyp, uint16_t key_len, const uint8_t **name,
    uint16_t *namelen)
{
	uint32_t len_hash;

	if (key_len < sizeof(struct apfs_j_key) + sizeof(uint32_t))
		return EINVAL;
	memcpy(&len_hash, (const uint8_t *)keyp + sizeof(struct apfs_j_key),
	    sizeof(len_hash));
	*namelen = (uint16_t)(le32(len_hash) & APFS_DREC_LEN_MASK);
	if (*namelen == 0)
		return EINVAL;
	(*namelen)--;
	if (sizeof(struct apfs_j_key) + sizeof(uint32_t) + *namelen > key_len)
		return EINVAL;
	*name = (const uint8_t *)keyp + sizeof(struct apfs_j_key) +
	    sizeof(uint32_t);
	return 0;
}

struct apfs_dirent_lookup_ctx {
	uint64_t dirid;
	const char *name;
	size_t namelen;
	uint64_t fileid;
	uint8_t dtype;
	int found;
};

static int
apfs_lookup_dirent_cb(struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
	struct apfs_dirent_lookup_ctx *c = (struct apfs_dirent_lookup_ctx *)ctx;
	uint32_t i;
	int error;

	for (i = 0; i < le32(node->btn_nkeys); i++) {
		const struct apfs_j_key *key;
		const struct apfs_j_drec_val *val;
		const void *keyp, *valp;
		uint16_t key_len, val_len;
		const uint8_t *entry_name;
		uint16_t entry_namelen;

		error = apfs_btree_entry(amp, node, info, i, &keyp, &key_len,
		    &valp, &val_len);
		if (error)
			return error;
		if (val_len < sizeof(*val))
			continue;

		key = (const struct apfs_j_key *)keyp;
		if (apfs_key_id(key->obj_id_and_type) != c->dirid ||
		    apfs_key_type(key->obj_id_and_type) != APFS_TYPE_DIR_REC)
			continue;
		if (apfs_parse_dir_key(keyp, key_len, &entry_name,
		    &entry_namelen))
			continue;
		if (entry_namelen != c->namelen ||
		    memcmp(entry_name, c->name, c->namelen) != 0)
			continue;

		val = (const struct apfs_j_drec_val *)valp;
		c->fileid = le64(val->file_id);
		c->dtype = (uint8_t)(le16(val->flags) & 0x0f);
		c->found = 1;
		return 1;			// Stop the walk
	}
	return 0;
}

int
apfs_lookup_dirent(struct apfs_mount *amp, uint64_t dirid, const char *name,
    size_t namelen, uint64_t *fileid, uint8_t *dtype)
{
	struct apfs_dirent_lookup_ctx c;
	int error;

	if (amp == NULL || name == NULL || fileid == NULL)
		return EINVAL;
	if (namelen > NAME_MAX)
		return ENAMETOOLONG;

	c.dirid = dirid;
	c.name = name;
	c.namelen = namelen;
	c.fileid = 0;
	c.dtype = 0;
	c.found = 0;
	{
		struct apfs_walk_target t;

		t.dirid = dirid;
		if (apfs_drec_name_hash(amp, name, namelen, &t.hash) == 0)
			error = apfs_btree_walk_drec(amp, &t,
			    apfs_lookup_dirent_cb, &c, __func__);
		else
			error = apfs_btree_walk_leaves_oid(amp, dirid, dirid,
			    apfs_lookup_dirent_cb, &c, __func__);
	}
	if (error != 0 && error != 1)
		return error;
	if (!c.found)
		return ENOENT;
	*fileid = c.fileid;
	if (dtype)
		*dtype = c.dtype;
	return 0;
}

// Extended attributes. Key is j_xattr_key_t with name_len counting the NUL, value is j_xattr_val_t
// (spec p.105-106). Only DATA_EMBEDDED values are handled, which is what symlink targets use
struct apfs_xattr_lookup_ctx {
	uint64_t fileid;
	const char *name;
	size_t namelen;		// Not counting the NUL
	void *buf;
	size_t bufsize;
	size_t outlen;
	int found;
};

static int
apfs_lookup_xattr_cb(struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
	struct apfs_xattr_lookup_ctx *c = (struct apfs_xattr_lookup_ctx *)ctx;
	uint32_t i;
	int error;

	for (i = 0; i < le32(node->btn_nkeys); i++) {
		const struct apfs_j_key *key;
		const void *keyp, *valp;
		uint16_t key_len, val_len;
		const uint8_t *kb, *vb;
		uint16_t name_len, flags, xdata_len;

		error = apfs_btree_entry(amp, node, info, i, &keyp, &key_len,
		    &valp, &val_len);
		if (error)
			return error;

		key = (const struct apfs_j_key *)keyp;
		if (apfs_key_id(key->obj_id_and_type) != c->fileid ||
		    apfs_key_type(key->obj_id_and_type) != APFS_TYPE_XATTR)
			continue;
		if (key_len < sizeof(*key) + 2 || val_len < 4)
			continue;

		kb = (const uint8_t *)keyp;
		name_len = (uint16_t)(kb[8] | (kb[9] << 8));
		if (name_len == 0 || key_len < sizeof(*key) + 2 + name_len)
			continue;
		// name_len includes the NUL, so compare one byte less
		if ((size_t)(name_len - 1) != c->namelen ||
		    memcmp(kb + 10, c->name, c->namelen) != 0)
			continue;

		vb = (const uint8_t *)valp;
		flags = (uint16_t)(vb[0] | (vb[1] << 8));
		xdata_len = (uint16_t)(vb[2] | (vb[3] << 8));
		if (!(flags & APFS_XATTR_DATA_EMBEDDED))
			return ENOTSUP;
		if (val_len < (uint16_t)(4 + xdata_len))
			continue;
		if (xdata_len > c->bufsize)
			return ERANGE;

		memcpy(c->buf, vb + 4, xdata_len);
		c->outlen = xdata_len;
		c->found = 1;
		return 1;			// Stop the walk
	}
	return 0;
}

int
apfs_lookup_xattr(struct apfs_mount *amp, uint64_t fileid, const char *name,
    void *buf, size_t bufsize, size_t *outlen)
{
	struct apfs_xattr_lookup_ctx c;
	int error;

	if (amp == NULL || name == NULL || buf == NULL || outlen == NULL)
		return EINVAL;

	memset(&c, 0, sizeof(c));
	c.fileid = fileid;
	c.name = name;
	c.namelen = strlen(name);
	c.buf = buf;
	c.bufsize = bufsize;

	error = apfs_btree_walk_leaves_oid(amp, fileid,
	    fileid, apfs_lookup_xattr_cb, &c, __func__);
	if (error != 0 && error != 1)
		return error;
	if (!c.found)
		return ENOATTR;
	*outlen = c.outlen;
	return 0;
}

// Every xattr name of a file, NUL-terminated and back to back, the way listxattr(2) hands them out.
// The symlink target is not a user xattr
struct apfs_list_xattr_ctx {
	uint64_t fileid;
	char *buf;
	size_t bufsize;
	size_t used;
};

static int
apfs_list_xattr_cb(struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
	struct apfs_list_xattr_ctx *c = (struct apfs_list_xattr_ctx *)ctx;
	uint32_t i;
	int error;

	for (i = 0; i < le32(node->btn_nkeys); i++) {
		const struct apfs_j_key *key;
		const void *keyp, *valp;
		uint16_t key_len, val_len, name_len;
		const uint8_t *kb;

		error = apfs_btree_entry(amp, node, info, i, &keyp, &key_len,
		    &valp, &val_len);
		if (error)
			return error;
		key = (const struct apfs_j_key *)keyp;
		if (apfs_key_id(key->obj_id_and_type) != c->fileid ||
		    apfs_key_type(key->obj_id_and_type) != APFS_TYPE_XATTR)
			continue;
		if (key_len < sizeof(*key) + 2)
			continue;
		kb = (const uint8_t *)keyp;
		name_len = (uint16_t)(kb[8] | (kb[9] << 8));
		if (name_len == 0 || key_len < sizeof(*key) + 2 + name_len)
			continue;
		if (name_len - 1 == strlen(APFS_XATTR_SYMLINK_NAME) &&
		    memcmp(kb + 10, APFS_XATTR_SYMLINK_NAME, name_len - 1) == 0)
			continue;
		if (c->buf != NULL) {
			if (c->used + name_len > c->bufsize)
				return ERANGE;
			memcpy(c->buf + c->used, kb + 10, name_len);
			c->buf[c->used + name_len - 1] = '\0';
		}
		c->used += name_len;
	}
	return 0;
}

int
apfs_list_xattrs(struct apfs_mount *amp, uint64_t fileid, char *buf,
    size_t bufsize, size_t *outlen)
{
	struct apfs_list_xattr_ctx c;
	int error;

	if (amp == NULL || outlen == NULL)
		return EINVAL;
	memset(&c, 0, sizeof(c));
	c.fileid = fileid;
	c.buf = buf;
	c.bufsize = bufsize;
	error = apfs_btree_walk_leaves_oid(amp, fileid, fileid,
	    apfs_list_xattr_cb, &c, __func__);
	if (error != 0 && error != 1)
		return error;
	*outlen = c.used;
	return 0;
}

struct apfs_iterate_dir_ctx {
	uint64_t dirid;
	off_t start_index;
	off_t logical_index;
	struct uio *uio;
	int entries;
	int done;
};

static int
apfs_iterate_dir_cb(struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
	struct apfs_iterate_dir_ctx *c = (struct apfs_iterate_dir_ctx *)ctx;
	uint32_t i;
	int error;

	for (i = 0; i < le32(node->btn_nkeys); i++) {
		const struct apfs_j_key *key;
		const struct apfs_j_drec_val *val;
		const void *keyp, *valp;
		uint16_t key_len, val_len;
		const uint8_t *name;
		uint16_t namelen;

		error = apfs_btree_entry(amp, node, info, i, &keyp, &key_len,
		    &valp, &val_len);
		if (error)
			return error;
		if (val_len < sizeof(*val))
			continue;

		key = (const struct apfs_j_key *)keyp;
		if (apfs_key_id(key->obj_id_and_type) != c->dirid ||
		    apfs_key_type(key->obj_id_and_type) != APFS_TYPE_DIR_REC)
			continue;

		// Leaves are visited in key order,
		// so a running index is a stable directory offset across the whole tree
		if (c->logical_index++ < c->start_index)
			continue;
		if (apfs_parse_dir_key(keyp, key_len, &name, &namelen))
			continue;

		val = (const struct apfs_j_drec_val *)valp;
		error = apfs_emit_dirent(le64(val->file_id),
		    (uint8_t)(le16(val->flags) & 0x0f), (const char *)name,
		    namelen, c->uio);
		if (error == EMSGSIZE) {
			c->done = 1;
			return 1;		// Buffer full, stop without an error
		}
		if (error)
			return error;
		c->entries++;
	}
	return 0;
}

int
apfs_iterate_dir(struct apfs_mount *amp, uint64_t dirid, off_t start_index,
    struct uio *uio, int *numdirent, int *eofflag)
{
	struct apfs_iterate_dir_ctx c;
	int error;

	c.dirid = dirid;
	c.start_index = start_index;
	c.logical_index = 0;
	c.uio = uio;
	c.entries = 0;
	c.done = 0;
	error = apfs_btree_walk_leaves_oid(amp, dirid,
	    dirid, apfs_iterate_dir_cb, &c, __func__);
	if (error == 1)
		error = 0;
	if (numdirent)
		*numdirent = c.entries;
	if (eofflag)
		*eofflag = (error == 0 && !c.done);
	return error;
}

struct apfs_extent_at_ctx {
	uint64_t fileid;
	uint64_t want_off;
	uint64_t logical;
	uint64_t len;
	uint64_t phys;
	uint64_t next_logical;		// First extent past want_off: hole end
	int found;
};

static int
apfs_extent_at_cb(struct apfs_mount *amp,
    const struct apfs_btree_node_phys *node,
    const struct apfs_btree_info *info, void *ctx)
{
	struct apfs_extent_at_ctx *c = (struct apfs_extent_at_ctx *)ctx;
	uint32_t i;
	int error;

	for (i = 0; i < le32(node->btn_nkeys); i++) {
		const struct apfs_j_file_extent_key *key;
		const struct apfs_j_file_extent_val *val;
		const void *keyp, *valp;
		uint16_t key_len, val_len;
		uint64_t logical, len;

		error = apfs_btree_entry(amp, node, info, i, &keyp, &key_len,
		    &valp, &val_len);
		if (error)
			return error;
		if (key_len < sizeof(*key) || val_len < sizeof(*val))
			continue;
		key = (const struct apfs_j_file_extent_key *)keyp;
		if (apfs_key_id(key->hdr.obj_id_and_type) != c->fileid ||
		    apfs_key_type(key->hdr.obj_id_and_type) !=
		    APFS_TYPE_FILE_EXTENT)
			continue;

		val = (const struct apfs_j_file_extent_val *)valp;
		logical = le64(key->logical_addr);
		len = le64(val->len_and_flags) & APFS_FILE_EXTENT_LEN_MASK;
		if (len == 0)
			continue;
		if (c->want_off < logical) {
			// Keys ascend, so this is the extent ending the hole
			c->next_logical = logical;
			return 1;
		}
		if (c->want_off >= logical + len)
			continue;
		c->logical = logical;
		c->len = len;
		c->phys = le64(val->phys_block_num);
		c->found = 1;
		return 1;			// Stop the walk
	}
	return 0;
}

// Bounce size per locked fill, the copy to the caller happens unlocked
#define APFS_READ_CHUNK	(256u * 1024u)

// Fill n bytes from file_off into dst.
// Runs under am_rw_lock so the extent in hand cannot be freed and reused by a commit halfway through
// pd_fault_trace=1: where read time goes - extent lookups vs block copies
uint64_t apfs_trace_walk_ns, apfs_trace_copy_ns, apfs_trace_pageins,
    apfs_trace_pagein_ns;
static int apfs_trace_on = -1;

int
apfs_trace_enabled(void)
{
	if (apfs_trace_on < 0) {
		int v = 0;

		apfs_trace_on = PE_parse_boot_argn("pd_fault_trace", &v,
		    sizeof(v)) && v;
	}
	return apfs_trace_on;
}

void
apfs_trace_add(uint64_t *acc, uint64_t t0)
{
	uint64_t ns;

	if (!apfs_trace_enabled())
		return;
	absolutetime_to_nanoseconds(mach_absolute_time() - t0, &ns);
	*acc += ns;
}

static int
apfs_read_locked(struct apfs_node *apnode, uint64_t file_off, size_t n,
    uint8_t *dst)
{
	struct apfs_mount *amp = apnode->amp;
	struct apfs_extent_at_ctx cur;
	size_t done = 0;
	int error = 0;

	memset(&cur, 0, sizeof(cur));
	cur.fileid = apnode->fileid;
	if (apnode->a_ext_valid && apnode->a_ext_xid == amp->xid) {
		cur.logical = apnode->a_ext_logical;
		cur.len = apnode->a_ext_len;
		cur.phys = apnode->a_ext_phys;
		cur.found = 1;
	}
	while (done < n) {
		uint64_t off = file_off + done;
		uint64_t extent_off, block_index;
		size_t block_off, count;

		// One tree walk per extent
		if (!cur.found || off < cur.logical ||
		    off >= cur.logical + cur.len) {
			uint64_t t0 = mach_absolute_time();

			cur.want_off = off;
			cur.found = 0;
			cur.next_logical = ~0ull;
			error = apfs_btree_walk_leaves_oid(amp, cur.fileid,
			    cur.fileid, apfs_extent_at_cb, &cur, __func__);
			apfs_trace_add(&apfs_trace_walk_ns, t0);
			if (error != 0 && error != 1)
				return error;
			if (cur.found) {
				apnode->a_ext_logical = cur.logical;
				apnode->a_ext_len = cur.len;
				apnode->a_ext_phys = cur.phys;
				apnode->a_ext_xid = amp->xid;
				apnode->a_ext_valid = 1;
			}
			if (!cur.found) {
				// A hole: zeros up to the next extent (or EOF)
				size_t z = n - done;

				if (cur.next_logical - off < z)
					z = (size_t)(cur.next_logical - off);
				memset(dst + done, 0, z);
				done += z;
				continue;
			}
		}
		if (cur.phys == 0) {
			// Explicit sparse extent
			size_t z = n - done;

			if (cur.logical + cur.len - off < z)
				z = (size_t)(cur.logical + cur.len - off);
			memset(dst + done, 0, z);
			done += z;
			continue;
		}
		extent_off = off - cur.logical;
		block_index = extent_off / amp->block_size;
		block_off = (size_t)(extent_off % amp->block_size);
		count = amp->block_size - block_off;
		if (cur.len - extent_off < count)
			count = (size_t)(cur.len - extent_off);
		if (n - done < count)
			count = n - done;
		{
			uint64_t t0 = mach_absolute_time();
			uint64_t avail = cur.len - extent_off;
			size_t run = 0;

			if (block_off == 0 && n - done >= 2 * amp->block_size &&
			    avail >= 2 * amp->block_size) {
				run = n - done;
				if (avail < run)
					run = (size_t)avail;
				if (run > APFS_MAX_RUN_BYTES)
					run = APFS_MAX_RUN_BYTES;
				run -= run % amp->block_size;
			}
			if (run != 0) {
				count = run;
				error = apfs_read_run(amp,
				    (apfs_paddr_t)(cur.phys + block_index), run,
				    dst + done);
			} else {
				error = apfs_copy_phys(amp,
				    (apfs_paddr_t)(cur.phys + block_index),
				    block_off, count, dst + done);
			}
			apfs_trace_add(&apfs_trace_copy_ns, t0);
		}
		if (error)
			return error;
		done += count;
	}
	return 0;
}

int
apfs_read_file(struct apfs_node *apnode, struct uio *uio)
{
	struct apfs_mount *amp;
	uint8_t *bounce;
	uint64_t filesize;
	size_t bsize;
	int error = 0;

	if (apnode == NULL || uio == NULL)
		return EINVAL;
	amp = apnode->amp;
	if (amp == NULL)
		return EINVAL;
	filesize = apnode->size;
	if (uio_resid(uio) <= 0 || (uint64_t)uio_offset(uio) >= filesize)
		return 0;

	bsize = APFS_READ_CHUNK;
	if ((uint64_t)uio_resid(uio) < bsize)
		bsize = (size_t)uio_resid(uio);
	bounce = (uint8_t *)_MALLOC(bsize, M_TEMP, M_WAITOK);
	if (bounce == NULL)
		return ENOMEM;

	while (uio_resid(uio) > 0 && (uint64_t)uio_offset(uio) < filesize) {
		uint64_t off = (uint64_t)uio_offset(uio);
		size_t n = bsize;

		if (filesize - off < n)
			n = (size_t)(filesize - off);
		if ((uint64_t)uio_resid(uio) < n)
			n = (size_t)uio_resid(uio);
		apfs_rw_lock_tag(amp, __func__);
		error = apfs_read_locked(apnode, off, n, bounce);
		apfs_rw_unlock(amp);
		if (error)
			break;
		error = uiomove((caddr_t)bounce, (int)n, uio);
		if (error)
			break;
	}
	_FREE(bounce, M_TEMP);
	return error;
}
