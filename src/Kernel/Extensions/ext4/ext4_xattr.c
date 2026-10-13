/*
 * Extended attributes, the Linux ext4 layout. In the inode, after the 128 byte
 * base inode and i_extra_isize, sits a 4 byte magic, then the entries, with
 * their values packed from the end of the inode down. What does not fit there
 * goes in one external block (i_file_acl): a 32 byte header, the entries, and
 * the values packed from the end of the block. Values larger than that are not
 * stored, the caller falls back to an AppleDouble file for them.
 */
#include "ext4.h"
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/xattr.h>
#include <string.h>
#include <IOKit/IOLocks.h>

#define EXT4_XATTR_MAGIC      0xEA020000u
#define EXT4_XATTR_ROUND(n)   (((n) + 3u) & ~3u)
#define EXT4_XATTR_ENTRY_SIZE(namelen) EXT4_XATTR_ROUND(16u + (namelen))
#define EXT4_FEATURE_COMPAT_EXT_ATTR 0x0008u
#define EXT4_XATTR_BLOCK_HEADER 32u
#define EXT4_XATTR_MAX_ITEMS  256

struct ext4_xattr_entry {
	uint8_t  e_name_len;
	uint8_t  e_name_index;
	uint16_t e_value_offs;
	uint32_t e_value_inum;
	uint32_t e_value_size;
	uint32_t e_hash;
	char     e_name[];
} __attribute__((packed));

struct ext4_xattr_header {
	uint32_t h_magic;
	uint32_t h_refcount;
	uint32_t h_blocks;
	uint32_t h_hash;
	uint32_t h_checksum;
	uint32_t h_reserved[3];
} __attribute__((packed));

// one attribute, name and value point into a copy of the area or the caller's buffers
struct xattr_item {
	uint8_t index;
	uint8_t name_len;
	const char *name;
	uint32_t size;
	const uint8_t *value;
};

// the attributes of one inode, both places, read into private copies
struct xattr_set {
	uint8_t *ibody;          // copy of the in-inode area, NULL without one
	uint32_t ibody_size;
	struct xattr_item *iitems;
	int in;
	uint8_t *block;          // copy of the external block, NULL without one
	uint64_t pblk;
	uint32_t refcount;
	struct xattr_item *bitems;
	int bn;
};

static int
has_ext_attr(struct ext4mount *emp)
{
	return (le32(emp->em_sb.s_feature_compat) & EXT4_FEATURE_COMPAT_EXT_ATTR) != 0;
}

// the external block from the inode fields, the high half only with 64bit
static uint64_t
inode_acl(struct ext4mount *emp, const struct ext4_inode *raw)
{
	uint64_t acl = le32(raw->i_file_acl_lo);

	if (emp->em_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		acl |= (uint64_t)le16(*(const uint16_t *)&raw->i_osd2[2]) << 32;
	return acl;
}

static void
inode_set_acl(struct ext4mount *emp, struct ext4_inode *raw, uint64_t acl)
{
	raw->i_file_acl_lo = le32((uint32_t)acl);
	if (emp->em_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		*(uint16_t *)&raw->i_osd2[2] = le16((uint16_t)(acl >> 32));
}

uint64_t
ext4_inode_xattr_block(struct ext4mount *emp, const struct ext4_inode *raw)
{
	return inode_acl(emp, raw);
}

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

// reads the entries from first up to limit, values are at vbase + e_value_offs
static int
parse_entries(const uint8_t *area, uint32_t first, uint32_t limit, const uint8_t *vbase,
    uint32_t vlimit, struct xattr_item *items, int max, int *n)
{
	uint32_t off = first;

	*n = 0;
	while (off + 4 <= limit && *(const uint32_t *)(area + off) != 0) {
		const struct ext4_xattr_entry *e = (const struct ext4_xattr_entry *)(area + off);
		uint32_t vsize, voff;

		if (off + 16 > limit || off + EXT4_XATTR_ENTRY_SIZE(e->e_name_len) > limit || *n >= max)
			return EIO;
		vsize = le32(e->e_value_size);
		voff = le16(e->e_value_offs);
		// values in a separate inode (ea_inode) are not supported
		if (e->e_value_inum || voff + vsize > vlimit)
			return EIO;
		items[*n].index = e->e_name_index;
		items[*n].name_len = e->e_name_len;
		items[*n].name = e->e_name;
		items[*n].size = vsize;
		items[*n].value = vbase + voff;
		(*n)++;
		off += EXT4_XATTR_ENTRY_SIZE(e->e_name_len);
	}
	return 0;
}

// the bytes a list of entries needs, its terminator and packed values included
static uint32_t
entries_need(const struct xattr_item *items, int n)
{
	uint32_t need = 4;

	for (int i = 0; i < n; i++)
		need += EXT4_XATTR_ENTRY_SIZE(items[i].name_len) + EXT4_XATTR_ROUND(items[i].size);
	return need;
}

// e2fsprogs' ext2fs_ext_attr_hash_entry, over the name and the zero padded value words
static uint32_t
entry_hash(const struct xattr_item *it)
{
	uint32_t hash = 0;

	for (int i = 0; i < it->name_len; i++)
		hash = (hash << 5) ^ (hash >> 27) ^ (uint8_t)it->name[i];
	for (uint32_t off = 0; off < it->size; off += 4) {
		uint32_t w = 0;

		memcpy(&w, it->value + off, it->size - off < 4 ? it->size - off : 4);
		hash = (hash << 16) ^ (hash >> 16) ^ le32(w);
	}
	return hash;
}

// writes the entries at first, values from area_end down, offsets relative to vbase
static void
write_entries(uint8_t *area, uint32_t first, uint32_t area_end, uint32_t vbase,
    const struct xattr_item *items, int n, int hashed)
{
	uint32_t off = first, vend = area_end;

	for (int i = 0; i < n; i++) {
		struct ext4_xattr_entry *e = (struct ext4_xattr_entry *)(area + off);

		vend -= EXT4_XATTR_ROUND(items[i].size);
		memmove(area + vend, items[i].value, items[i].size);
		memset(area + vend + items[i].size, 0, EXT4_XATTR_ROUND(items[i].size) - items[i].size);
		e->e_name_len = items[i].name_len;
		e->e_name_index = items[i].index;
		e->e_value_offs = le16((uint16_t)(vend - vbase));
		e->e_value_inum = 0;
		e->e_value_size = le32(items[i].size);
		// e2fsck accepts 0 for in-inode entries, block entries need the real hash
		e->e_hash = hashed ? le32(entry_hash(&items[i])) : 0;
		memmove(e->e_name, items[i].name, items[i].name_len);
		off += EXT4_XATTR_ENTRY_SIZE(items[i].name_len);
	}
	*(uint32_t *)(area + off) = 0;
}

static int
item_cmp(const struct xattr_item *a, const struct xattr_item *b)
{
	if (a->index != b->index)
		return a->index - b->index;
	if (a->name_len != b->name_len)
		return a->name_len - b->name_len;
	return memcmp(a->name, b->name, a->name_len);
}

// block entries sorted by index, name length and name, as Linux keeps them
static void
sort_items(struct xattr_item *items, int n)
{
	for (int i = 1; i < n; i++) {
		struct xattr_item t = items[i];
		int j = i - 1;

		while (j >= 0 && item_cmp(&items[j], &t) > 0) {
			items[j + 1] = items[j];
			j--;
		}
		items[j + 1] = t;
	}
}

static void
block_csum_set(struct ext4mount *emp, uint64_t pblk, uint8_t *blk)
{
	struct ext4_xattr_header *h = (struct ext4_xattr_header *)blk;
	uint64_t nr = OSSwapHostToLittleInt64(pblk);
	uint32_t crc;

	if (!emp->em_has_metadata_csum)
		return;
	h->h_checksum = 0;
	crc = ext4_crc32c(emp->em_csum_seed, &nr, sizeof(nr));
	crc = ext4_crc32c(crc, blk, emp->em_blocksize);
	h->h_checksum = le32(crc);
}

// header, entries and the block hash, e2fsprogs' ext2fs_ext_attr_block_rehash
static void
block_build(struct ext4mount *emp, uint8_t *blk, uint32_t refcount,
    struct xattr_item *items, int n)
{
	struct ext4_xattr_header *h = (struct ext4_xattr_header *)blk;
	const struct ext4_xattr_entry *e;
	uint32_t hash = 0, off = EXT4_XATTR_BLOCK_HEADER;

	sort_items(items, n);
	memset(blk, 0, EXT4_XATTR_BLOCK_HEADER);
	write_entries(blk, EXT4_XATTR_BLOCK_HEADER, emp->em_blocksize, 0, items, n, 1);
	for (int i = 0; i < n; i++) {
		e = (const struct ext4_xattr_entry *)(blk + off);
		hash = (hash << 16) ^ (hash >> 16) ^ le32(e->e_hash);
		off += EXT4_XATTR_ENTRY_SIZE(e->e_name_len);
	}
	h->h_magic = le32(EXT4_XATTR_MAGIC);
	h->h_refcount = le32(refcount);
	h->h_blocks = le32(1);
	h->h_hash = le32(hash);
}

static void
set_free(struct xattr_set *s)
{
	if (s->ibody)
		_FREE(s->ibody, M_TEMP);
	if (s->block)
		_FREE(s->block, M_TEMP);
	if (s->iitems)
		_FREE(s->iitems, M_TEMP);
	if (s->bitems)
		_FREE(s->bitems, M_TEMP);
	memset(s, 0, sizeof(*s));
}

// copies both areas of the inode and parses them, the copies stay valid until set_free
static int
set_load(struct ext4mount *emp, ino_t ino, struct xattr_set *s)
{
	IOLock *lock = (IOLock *)emp->em_alloc_lock;
	uint32_t start, size;
	uint8_t *inode;
	buf_t bp = NULL;
	int error;

	memset(s, 0, sizeof(*s));
	if (lock == NULL)
		return EIO;
	s->iitems = _MALLOC(sizeof(struct xattr_item) * EXT4_XATTR_MAX_ITEMS, M_TEMP, M_WAITOK);
	s->bitems = _MALLOC(sizeof(struct xattr_item) * EXT4_XATTR_MAX_ITEMS, M_TEMP, M_WAITOK);
	if (s->iitems == NULL || s->bitems == NULL) {
		set_free(s);
		return ENOMEM;
	}

	IOLockLock(lock);
	error = inode_bytes(emp, ino, &bp, &inode);
	if (error) {
		IOLockUnlock(lock);
		set_free(s);
		return error;
	}
	s->pblk = inode_acl(emp, (const struct ext4_inode *)inode);
	if (ibody_area(emp, inode, &start, &size) == 0) {
		s->ibody = _MALLOC(size, M_TEMP, M_WAITOK);
		if (s->ibody) {
			memcpy(s->ibody, inode + start, size);
			s->ibody_size = size;
		}
	}
	buf_brelse(bp);
	IOLockUnlock(lock);

	if (s->ibody && le32(*(uint32_t *)s->ibody) == EXT4_XATTR_MAGIC) {
		error = parse_entries(s->ibody, 4, s->ibody_size, s->ibody + 4, s->ibody_size - 4,
		    s->iitems, EXT4_XATTR_MAX_ITEMS, &s->in);
		if (error) {
			set_free(s);
			return error;
		}
	}

	if (s->pblk != 0) {
		const struct ext4_xattr_header *h;

		if (s->pblk >= emp->em_blocks_count) {
			set_free(s);
			return EIO;
		}
		error = ext4_blkread(emp, s->pblk, &bp);
		if (error) {
			set_free(s);
			return error;
		}
		s->block = _MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
		if (s->block)
			memcpy(s->block, (void *)buf_dataptr(bp), emp->em_blocksize);
		buf_brelse(bp);
		if (s->block == NULL) {
			set_free(s);
			return ENOMEM;
		}
		h = (const struct ext4_xattr_header *)s->block;
		if (le32(h->h_magic) != EXT4_XATTR_MAGIC || le32(h->h_blocks) != 1) {
			E4LOG("ino %llu: bad xattr block %llu", (uint64_t)ino, s->pblk);
			set_free(s);
			return EIO;
		}
		s->refcount = le32(h->h_refcount);
		error = parse_entries(s->block, EXT4_XATTR_BLOCK_HEADER, emp->em_blocksize, s->block,
		    emp->em_blocksize, s->bitems, EXT4_XATTR_MAX_ITEMS, &s->bn);
		if (error) {
			set_free(s);
			return error;
		}
	}
	return 0;
}

static int
find_item(const struct xattr_item *items, int n, uint8_t index, const char *name, uint32_t nlen)
{
	for (int i = 0; i < n; i++) {
		if (items[i].index == index && items[i].name_len == nlen && !memcmp(items[i].name, name, nlen))
			return i;
	}
	return -1;
}

int
ext4_xattr_get(struct ext4mount *emp, ino_t ino, uint8_t index, const char *name,
    void *value, size_t cap, size_t *out_size)
{
	struct xattr_set s;
	const struct xattr_item *it = NULL;
	uint32_t nlen = (uint32_t)strlen(name);
	int i, error;

	if (!has_ext_attr(emp))
		return ENOATTR;
	error = set_load(emp, ino, &s);
	if (error)
		return error;
	if ((i = find_item(s.iitems, s.in, index, name, nlen)) >= 0)
		it = &s.iitems[i];
	else if ((i = find_item(s.bitems, s.bn, index, name, nlen)) >= 0)
		it = &s.bitems[i];
	if (it == NULL) {
		error = ENOATTR;
	} else {
		*out_size = it->size;
		if (value != NULL) {
			if (it->size > cap)
				error = ERANGE;
			else
				memcpy(value, it->value, it->size);
		}
	}
	set_free(&s);
	return error;
}

int
ext4_xattr_list(struct ext4mount *emp, ino_t ino, uint8_t index, char *buf, size_t cap,
    size_t *out_len)
{
	struct xattr_set s;
	size_t len = 0;
	int error;

	*out_len = 0;
	if (!has_ext_attr(emp))
		return 0;
	error = set_load(emp, ino, &s);
	if (error)
		return error;
	for (int pass = 0; pass < 2; pass++) {
		const struct xattr_item *items = pass ? s.bitems : s.iitems;
		int n = pass ? s.bn : s.in;

		for (int i = 0; i < n; i++) {
			if (items[i].index != index)
				continue;
			if (buf != NULL) {
				if (len + items[i].name_len + 1 > cap) {
					error = ERANGE;
					goto out;
				}
				memcpy(buf + len, items[i].name, items[i].name_len);
				buf[len + items[i].name_len] = '\0';
			}
			len += items[i].name_len + 1;
		}
	}
	*out_len = len;
out:
	set_free(&s);
	return error;
}

// rewrites the in-inode area from the item list, the values may point into s->ibody
static int
ibody_store(struct ext4mount *emp, ino_t ino, struct xattr_set *s)
{
	IOLock *lock = (IOLock *)emp->em_alloc_lock;
	uint8_t *inode, *area, *tmp;
	uint32_t start = 0, size = 0;
	buf_t bp = NULL;
	int error;

	tmp = _MALLOC(s->ibody_size, M_TEMP, M_WAITOK);
	if (tmp == NULL)
		return ENOMEM;
	memset(tmp, 0, s->ibody_size);
	if (s->in) {
		*(uint32_t *)tmp = le32(EXT4_XATTR_MAGIC);
		write_entries(tmp, 4, s->ibody_size, 4, s->iitems, s->in, 0);
	}

	IOLockLock(lock);
	error = inode_bytes(emp, ino, &bp, &inode);
	if (!error)
		error = ibody_area(emp, inode, &start, &size);
	if (!error && size != s->ibody_size)
		error = EIO;
	if (error) {
		if (bp)
			buf_brelse(bp);
		IOLockUnlock(lock);
		_FREE(tmp, M_TEMP);
		return error;
	}
	area = inode + start;
	memcpy(area, tmp, size);
	ext4_inode_csum_set(emp, ino, (char *)inode);
	error = ext4_meta_bwrite(emp, bp);
	IOLockUnlock(lock);
	_FREE(tmp, M_TEMP);
	return error;
}

// writes a whole block image to pblk with its checksum
static int
block_write(struct ext4mount *emp, uint64_t pblk, uint8_t *img)
{
	buf_t bp = NULL;
	int error;

	block_csum_set(emp, pblk, img);
	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;
	memcpy((void *)buf_dataptr(bp), img, emp->em_blocksize);
	return ext4_meta_bwrite(emp, bp);
}

// drops this inode's reference to its block, freeing it with the last one
static int
block_unref(struct ext4mount *emp, struct xattr_set *s)
{
	if (s->refcount > 1) {
		((struct ext4_xattr_header *)s->block)->h_refcount = le32(s->refcount - 1);
		return block_write(emp, s->pblk, s->block);
	}
	return ext4_free_block(emp, s->pblk);
}

static void
inode_blocks_add(struct ext4mount *emp, struct ext4_inode *raw, int delta)
{
	uint32_t per = emp->em_blocksize / 512, cur = le32(raw->i_blocks_lo);

	if (delta > 0)
		raw->i_blocks_lo = le32(cur + per);
	else
		raw->i_blocks_lo = le32(cur > per ? cur - per : 0);
}

// stores the block list: in place when this inode owns the block, else in a new one
static int
block_store(struct ext4node *ep, struct xattr_set *s)
{
	struct ext4mount *emp = ep->e_mount;
	uint8_t *img;
	uint64_t pblk = s->pblk;
	int error;

	if (s->bn == 0) {
		if (s->pblk == 0)
			return 0;
		error = block_unref(emp, s);
		if (error)
			return error;
		inode_set_acl(emp, &ep->e_raw, 0);
		inode_blocks_add(emp, &ep->e_raw, -1);
		return ext4_write_inode(emp, ep->e_ino, &ep->e_raw);
	}

	img = _MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
	if (img == NULL)
		return ENOMEM;
	memset(img, 0, emp->em_blocksize);
	block_build(emp, img, 1, s->bitems, s->bn);

	if (s->pblk != 0 && s->refcount <= 1) {
		error = block_write(emp, s->pblk, img);
		_FREE(img, M_TEMP);
		return error;
	}

	// first block of this inode, or a block shared with other inodes: copy on write
	error = ext4_alloc_block(emp, s->pblk, &pblk);
	if (!error)
		error = block_write(emp, pblk, img);
	_FREE(img, M_TEMP);
	if (error) {
		if (pblk != s->pblk)
			(void)ext4_free_block(emp, pblk);
		return error;
	}
	if (s->pblk != 0) {
		error = block_unref(emp, s);
		if (error)
			return error;
	} else {
		inode_blocks_add(emp, &ep->e_raw, 1);
	}
	inode_set_acl(emp, &ep->e_raw, pblk);
	return ext4_write_inode(emp, ep->e_ino, &ep->e_raw);
}

int
ext4_xattr_set(struct ext4node *ep, uint8_t index, const char *name, const void *value,
    size_t vsize, int options)
{
	struct ext4mount *emp = ep->e_mount;
	struct xattr_set s;
	uint32_t nlen = (uint32_t)strlen(name);
	int i, in_ibody = 0, in_block = 0, error;

	if (nlen == 0 || nlen > 255)
		return EINVAL;
	if (!has_ext_attr(emp))
		return ENOTSUP;
	error = set_load(emp, ep->e_ino, &s);
	if (error)
		return error;

	if ((i = find_item(s.iitems, s.in, index, name, nlen)) >= 0) {
		s.iitems[i] = s.iitems[--s.in];
		in_ibody = 1;
	} else if ((i = find_item(s.bitems, s.bn, index, name, nlen)) >= 0) {
		s.bitems[i] = s.bitems[--s.bn];
		in_block = 1;
	}
	if ((options & XATTR_CREATE) && (in_ibody || in_block)) {
		error = EEXIST;
		goto out;
	}
	if ((options & XATTR_REPLACE) && !in_ibody && !in_block) {
		error = ENOATTR;
		goto out;
	}
	if (value == NULL && !in_ibody && !in_block) {
		error = ENOATTR;
		goto out;
	}

	if (value != NULL) {
		struct xattr_item it = { index, (uint8_t)nlen, name, (uint32_t)vsize, value };

		if (vsize > emp->em_blocksize) {
			error = ENOSPC;
		} else if (s.ibody && s.in < EXT4_XATTR_MAX_ITEMS &&
		    4 + entries_need(s.iitems, s.in) + EXT4_XATTR_ENTRY_SIZE(nlen) +
		    EXT4_XATTR_ROUND(vsize) <= s.ibody_size) {
			s.iitems[s.in++] = it;
			in_ibody = 1;
		} else if (s.bn < EXT4_XATTR_MAX_ITEMS &&
		    EXT4_XATTR_BLOCK_HEADER + entries_need(s.bitems, s.bn) + EXT4_XATTR_ENTRY_SIZE(nlen) +
		    EXT4_XATTR_ROUND(vsize) <= emp->em_blocksize) {
			s.bitems[s.bn++] = it;
			in_block = 1;
		} else {
			error = ENOSPC;
		}
		// a value with no room is dropped from both places so a fallback copy is never shadowed
		if (error && !in_ibody && !in_block)
			goto out;
	}

	if (in_ibody) {
		int e = ibody_store(emp, ep->e_ino, &s);
		if (e && !error)
			error = e;
	}
	if (in_block) {
		int e = block_store(ep, &s);
		if (e && !error)
			error = e;
	}
out:
	set_free(&s);
	return error;
}

int
ext4_xattr_release_block(struct ext4mount *emp, struct ext4_inode *raw)
{
	struct xattr_set s;
	uint64_t pblk = inode_acl(emp, raw);
	buf_t bp = NULL;
	int error;

	if (pblk == 0)
		return 0;
	memset(&s, 0, sizeof(s));
	s.pblk = pblk;
	if (pblk < emp->em_blocks_count && ext4_blkread(emp, pblk, &bp) == 0) {
		const struct ext4_xattr_header *h = (const struct ext4_xattr_header *)buf_dataptr(bp);

		if (le32(h->h_magic) == EXT4_XATTR_MAGIC) {
			s.refcount = le32(h->h_refcount);
			s.block = _MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
			if (s.block)
				memcpy(s.block, h, emp->em_blocksize);
		}
		buf_brelse(bp);
	}
	// an unreadable or foreign block is left alone, e2fsck can reclaim it
	if (s.block == NULL)
		error = 0;
	else
		error = block_unref(emp, &s);
	if (s.block)
		_FREE(s.block, M_TEMP);
	if (error)
		return error;
	inode_set_acl(emp, raw, 0);
	inode_blocks_add(emp, raw, -1);
	return 0;
}
