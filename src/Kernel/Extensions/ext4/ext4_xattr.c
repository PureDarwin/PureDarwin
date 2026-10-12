/*
 * In-inode extended attributes, the Linux ext4 layout: after the 128 byte
 * base inode and i_extra_isize sits a 4 byte magic, then the entries, with
 * their values packed from the end of the inode down. PureDarwin only needs
 * a few small attributes (the BSD file flags ext4 has no bits for), so
 * values never spill into an external xattr block.
 */
#include "ext4.h"
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <string.h>
#include <IOKit/IOLocks.h>

#define EXT4_XATTR_MAGIC      0xEA020000u
#define EXT4_XATTR_ROUND(n)   (((n) + 3u) & ~3u)
#define EXT4_XATTR_ENTRY_SIZE(namelen) EXT4_XATTR_ROUND(16u + (namelen))
#define EXT4_FEATURE_COMPAT_EXT_ATTR 0x0008u
#define EXT4_XATTR_MAX_ENTRIES 16

struct ext4_xattr_entry {
	uint8_t  e_name_len;
	uint8_t  e_name_index;
	uint16_t e_value_offs;
	uint32_t e_value_inum;
	uint32_t e_value_size;
	uint32_t e_hash;
	char     e_name[];
} __attribute__((packed));

struct xattr_item {
	uint8_t index;
	uint8_t name_len;
	char name[64];
	uint32_t size;
	const uint8_t *value;
};

// finds the inode's bytes in its inode table block, the caller releases bp
static int
inode_bytes(struct ext4mount *emp, ino_t ino, buf_t *bpp, uint8_t **out)
{
	struct ext4_group_desc gd;
	uint32_t grp, index;
	uint64_t itable_block, byteoff;
	int error;

	if (ino == 0 || ino > emp->em_inodes_count)
		return EINVAL;
	grp = (uint32_t)((ino - 1) / emp->em_inodes_per_group);
	index = (uint32_t)((ino - 1) % emp->em_inodes_per_group);
	error = ext4_read_group_desc(emp, grp, &gd);
	if (error)
		return error;
	itable_block = le32(gd.bg_inode_table_lo);
	if (emp->em_desc_size >= 64)
		itable_block |= ((uint64_t)le32(gd.bg_inode_table_hi)) << 32;
	byteoff = (uint64_t)index * emp->em_inode_size;
	error = ext4_blkread(emp, itable_block + byteoff / emp->em_blocksize, bpp);
	if (error)
		return error;
	*out = (uint8_t *)buf_dataptr(*bpp) + (byteoff % emp->em_blocksize);
	return 0;
}

// the in-inode area: where the magic sits and how many bytes follow it
static int
ibody_area(struct ext4mount *emp, const uint8_t *inode, uint32_t *start, uint32_t *size)
{
	uint16_t extra = le16(((const struct ext4_inode *)inode)->i_extra_isize);
	uint32_t base = 128u + extra;

	if (emp->em_inode_size <= 128 || base + 8 > emp->em_inode_size)
		return ENOTSUP;
	*start = base;
	*size = emp->em_inode_size - base;
	return 0;
}

// reads every in-inode entry, the values point into the inode bytes
static int
ibody_parse(const uint8_t *area, uint32_t size, struct xattr_item *items, int *n)
{
	uint32_t off = 4;

	*n = 0;
	if (le32(*(const uint32_t *)area) != EXT4_XATTR_MAGIC)
		return 0;
	while (off + 4 <= size && *(const uint32_t *)(area + off) != 0) {
		const struct ext4_xattr_entry *e = (const struct ext4_xattr_entry *)(area + off);
		uint32_t vsize = le32(e->e_value_size), voff = le16(e->e_value_offs);

		if (off + 16 > size || off + EXT4_XATTR_ENTRY_SIZE(e->e_name_len) > size ||
		    e->e_value_inum || e->e_name_len >= sizeof(items[0].name) ||
		    4 + voff + vsize > size || *n >= EXT4_XATTR_MAX_ENTRIES)
			return EIO;
		items[*n].index = e->e_name_index;
		items[*n].name_len = e->e_name_len;
		memcpy(items[*n].name, e->e_name, e->e_name_len);
		items[*n].size = vsize;
		items[*n].value = area + 4 + voff;
		(*n)++;
		off += EXT4_XATTR_ENTRY_SIZE(e->e_name_len);
	}
	return 0;
}

int
ext4_xattr_ibody_get(struct ext4mount *emp, ino_t ino, uint8_t index, const char *name,
    void *value, uint32_t cap, uint32_t *out_size)
{
	struct xattr_item items[EXT4_XATTR_MAX_ENTRIES];
	uint32_t start, size, nlen = (uint32_t)strlen(name);
	buf_t bp = NULL;
	uint8_t *inode;
	int n, error;
	IOLock *lock = (IOLock *)emp->em_alloc_lock;

	if (lock == NULL)
		return EIO;
	IOLockLock(lock);
	error = inode_bytes(emp, ino, &bp, &inode);
	if (error) {
		IOLockUnlock(lock);
		return error;
	}
	error = ibody_area(emp, inode, &start, &size);
	if (!error)
		error = ibody_parse(inode + start, size, items, &n);
	if (!error) {
		error = ENOATTR;
		for (int i = 0; i < n; i++) {
			if (items[i].index != index || items[i].name_len != nlen || memcmp(items[i].name, name, nlen))
				continue;
			*out_size = items[i].size;
			error = items[i].size > cap ? ERANGE : 0;
			if (!error)
				memcpy(value, items[i].value, items[i].size);
			break;
		}
	}
	buf_brelse(bp);
	IOLockUnlock(lock);
	return error;
}

// rewrites the whole in-inode area with one attribute replaced, added or (value NULL) removed
int
ext4_xattr_ibody_set(struct ext4mount *emp, ino_t ino, uint8_t index, const char *name,
    const void *value, uint32_t vsize)
{
	struct xattr_item items[EXT4_XATTR_MAX_ENTRIES + 1];
	uint32_t start, size, nlen = (uint32_t)strlen(name), need = 4 + 4;
	uint8_t *inode, *area, *copy = NULL;
	buf_t bp = NULL;
	int n = 0, keep = 0, error;
	IOLock *lock = (IOLock *)emp->em_alloc_lock;

	if (nlen == 0 || nlen >= sizeof(items[0].name))
		return EINVAL;
	if (!(le32(emp->em_sb.s_feature_compat) & EXT4_FEATURE_COMPAT_EXT_ATTR))
		return ENOTSUP;
	if (lock == NULL)
		return EIO;
	IOLockLock(lock);
	error = inode_bytes(emp, ino, &bp, &inode);
	if (error) {
		IOLockUnlock(lock);
		return error;
	}
	error = ibody_area(emp, inode, &start, &size);
	area = inode + start;
	if (!error)
		error = ibody_parse(area, size, items, &n);
	if (error)
		goto out;

	// values point into the area being rewritten, so work from a copy
	copy = _MALLOC(size, M_TEMP, M_WAITOK);
	if (!copy) {
		error = ENOMEM;
		goto out;
	}
	memcpy(copy, area, size);
	for (int i = 0; i < n; i++) {
		items[i].value = copy + (items[i].value - area);
		if (items[i].index == index && items[i].name_len == nlen && !memcmp(items[i].name, name, nlen))
			continue;
		items[keep++] = items[i];
	}
	if (value) {
		items[keep].index = index;
		items[keep].name_len = (uint8_t)nlen;
		memcpy(items[keep].name, name, nlen);
		items[keep].size = vsize;
		items[keep].value = value;
		keep++;
	}
	for (int i = 0; i < keep; i++)
		need += EXT4_XATTR_ENTRY_SIZE(items[i].name_len) + EXT4_XATTR_ROUND(items[i].size);
	if (need > size) {
		error = ENOSPC;
		goto out;
	}

	memset(area, 0, size);
	if (keep) {
		uint32_t off = 4, vend = size;

		*(uint32_t *)area = le32(EXT4_XATTR_MAGIC);
		for (int i = 0; i < keep; i++) {
			struct ext4_xattr_entry *e = (struct ext4_xattr_entry *)(area + off);

			vend -= EXT4_XATTR_ROUND(items[i].size);
			memcpy(area + vend, items[i].value, items[i].size);
			e->e_name_len = items[i].name_len;
			e->e_name_index = items[i].index;
			e->e_value_offs = le16((uint16_t)(vend - 4));
			e->e_value_size = le32(items[i].size);
			// e2fsck accepts 0 for in-inode entries, it only checks a hash that is set
			e->e_hash = 0;
			memcpy(e->e_name, items[i].name, items[i].name_len);
			off += EXT4_XATTR_ENTRY_SIZE(items[i].name_len);
		}
	}
	ext4_inode_csum_set(emp, ino, (char *)inode);
	error = ext4_meta_bwrite(emp, bp);
	bp = NULL;
out:
	if (copy)
		_FREE(copy, M_TEMP);
	if (bp)
		buf_brelse(bp);
	IOLockUnlock(lock);
	return error;
}
