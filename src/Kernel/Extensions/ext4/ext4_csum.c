#include "ext4.h"
#include <string.h>

/* crc32c (Castagnoli), reflected, poly 0x82F63B78 */
static uint32_t crc32c_table[256];
static int      crc32c_ready;

static void
crc32c_build(void)
{
	uint32_t i, k, c;
	for (i = 0; i < 256; i++) {
		c = i;
		for (k = 0; k < 8; k++)
			c = (c & 1) ? (c >> 1) ^ 0x82F63B78u : (c >> 1);
		crc32c_table[i] = c;
	}
	crc32c_ready = 1;
}

uint32_t
ext4_crc32c(uint32_t crc, const void *buf, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	if (!crc32c_ready)
		crc32c_build();
	while (len--)
		crc = crc32c_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
	return crc;
}

static uint16_t
ext4_crc16(uint16_t crc, const void *buf, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	int b;
	while (len--) {
		crc ^= *p++;
		for (b = 0; b < 8; b++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
	}
	return crc;
}

void
ext4_csum_init(struct ext4mount *emp)
{
	if (!crc32c_ready)
		crc32c_build();

	emp->em_feature_ro_compat = le32(emp->em_sb.s_feature_ro_compat);
	emp->em_has_metadata_csum =
	    (emp->em_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_METADATA_CSUM) != 0;
	emp->em_has_gdt_csum =
	    (emp->em_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM) != 0;
	emp->em_reserved_gdt_blocks = le16(emp->em_sb.s_reserved_gdt_blocks);

	/* GDT size in blocks: one descriptor per group. */
	emp->em_gdt_blocks = (uint32_t)
	    (((uint64_t)emp->em_groups_count * emp->em_desc_size +
	      emp->em_blocksize - 1) / emp->em_blocksize);
	/* Inode-table blocks per group. */
	emp->em_itable_blocks = (uint32_t)
	    (((uint64_t)emp->em_inodes_per_group * emp->em_inode_size +
	      emp->em_blocksize - 1) / emp->em_blocksize);

	if (emp->em_feature_incompat & EXT4_FEATURE_INCOMPAT_CSUM_SEED)
		emp->em_csum_seed = le32(emp->em_sb.s_checksum_seed);
	else if (emp->em_has_metadata_csum)
		emp->em_csum_seed =
		    ext4_crc32c(~0u, emp->em_sb.s_uuid, sizeof(emp->em_sb.s_uuid));
	else
		emp->em_csum_seed = 0;
}

/* offsetof-style constants for the packed on-disk group descriptor */
#define GD_OFF_CHECKSUM         0x1e
#define GD_OFF_BBMP_CSUM_LO     0x18
#define GD_OFF_IBMP_CSUM_LO     0x1a
#define GD_OFF_BBMP_CSUM_HI     0x38
#define GD_OFF_IBMP_CSUM_HI     0x3a
/* hi bitmap-csum halves exist only when the descriptor is large enough */
#define GD_MIN_SIZE_FOR_BMP_CSUM_HI 0x3c

static uint16_t
ext4_group_desc_csum(struct ext4mount *emp, uint32_t grp,
    const struct ext4_group_desc *gd)
{
	uint32_t grp_le = OSSwapHostToLittleInt32(grp);

	if (emp->em_has_metadata_csum) {
		uint8_t tmp[64];
		uint32_t sz = emp->em_desc_size;
		uint32_t crc;
		if (sz > sizeof(tmp))
			sz = sizeof(tmp);
		memcpy(tmp, gd, sz);
		tmp[GD_OFF_CHECKSUM] = 0;
		tmp[GD_OFF_CHECKSUM + 1] = 0;
		crc = ext4_crc32c(emp->em_csum_seed, &grp_le, sizeof(grp_le));
		crc = ext4_crc32c(crc, tmp, sz);
		return (uint16_t)(crc & 0xffff);
	} else {
		/* uninit_bg crc16: uuid, group#, gd[0..checksum), gd[checksum+2..] */
		uint16_t crc;
		crc = ext4_crc16(0xffff, emp->em_sb.s_uuid,
		    sizeof(emp->em_sb.s_uuid));
		crc = ext4_crc16(crc, &grp_le, sizeof(grp_le));
		crc = ext4_crc16(crc, gd, GD_OFF_CHECKSUM);
		if (emp->em_desc_size > GD_OFF_CHECKSUM + 2)
			crc = ext4_crc16(crc,
			    (const uint8_t *)gd + GD_OFF_CHECKSUM + 2,
			    emp->em_desc_size - (GD_OFF_CHECKSUM + 2));
		return crc;
	}
}

void
ext4_group_desc_csum_set(struct ext4mount *emp, uint32_t grp,
    struct ext4_group_desc *gd)
{
	if (!emp->em_has_metadata_csum && !emp->em_has_gdt_csum)
		return;
	gd->bg_checksum = OSSwapHostToLittleInt16(
	    ext4_group_desc_csum(emp, grp, gd));
}

int
ext4_group_desc_csum_verify(struct ext4mount *emp, uint32_t grp,
    const struct ext4_group_desc *gd)
{
	if (!emp->em_has_metadata_csum && !emp->em_has_gdt_csum)
		return 1;
	return le16(gd->bg_checksum) == ext4_group_desc_csum(emp, grp, gd);
}

/* store a split 32-bit csum into the lo (+hi if room) descriptor fields */
static void
gd_store_bmp_csum(struct ext4mount *emp, struct ext4_group_desc *gd,
    uint32_t lo_off, uint32_t hi_off, uint32_t crc)
{
	uint8_t *p = (uint8_t *)gd;
	uint16_t lo = OSSwapHostToLittleInt16((uint16_t)(crc & 0xffff));
	uint16_t hi = OSSwapHostToLittleInt16((uint16_t)((crc >> 16) & 0xffff));
	memcpy(p + lo_off, &lo, sizeof(lo));
	if (emp->em_desc_size >= GD_MIN_SIZE_FOR_BMP_CSUM_HI)
		memcpy(p + hi_off, &hi, sizeof(hi));
}

void
ext4_block_bitmap_csum_set(struct ext4mount *emp, struct ext4_group_desc *gd,
    const void *bitmap)
{
	uint32_t crc;
	if (!emp->em_has_metadata_csum)
		return;
	crc = ext4_crc32c(emp->em_csum_seed, bitmap, emp->em_blocks_per_group / 8);
	gd_store_bmp_csum(emp, gd, GD_OFF_BBMP_CSUM_LO, GD_OFF_BBMP_CSUM_HI, crc);
}

void
ext4_inode_bitmap_csum_set(struct ext4mount *emp, struct ext4_group_desc *gd,
    const void *bitmap)
{
	uint32_t crc;
	if (!emp->em_has_metadata_csum)
		return;
	crc = ext4_crc32c(emp->em_csum_seed, bitmap, emp->em_inodes_per_group / 8);
	gd_store_bmp_csum(emp, gd, GD_OFF_IBMP_CSUM_LO, GD_OFF_IBMP_CSUM_HI, crc);
}

/* inode checksum fields, as byte offsets into the on-disk inode */
#define INO_OFF_CHECKSUM_LO     0x7c   /* l_i_checksum_lo (inside i_osd2) */
#define INO_OFF_EXTRA_ISIZE     0x80
#define INO_OFF_CHECKSUM_HI     0x82

void
ext4_inode_csum_set(struct ext4mount *emp, ino_t ino, void *raw_full)
{
	uint8_t *p = (uint8_t *)raw_full;
	uint32_t ino_le, gen_le, crc;
	uint16_t lo, hi, extra;
	int have_hi;

	if (!emp->em_has_metadata_csum)
		return;

	/* i_generation lives at 0x64 and is already little-endian on disk */
	memcpy(&gen_le, p + 0x64, sizeof(gen_le));
	ino_le = OSSwapHostToLittleInt32((uint32_t)ino);

	/* the hi half only exists on large inodes whose extra_isize reaches it */
	memcpy(&extra, p + INO_OFF_EXTRA_ISIZE, sizeof(extra));
	have_hi = (emp->em_inode_size > EXT4_GOOD_OLD_INODE_SIZE &&
	    le16(extra) >= (INO_OFF_CHECKSUM_HI - INO_OFF_EXTRA_ISIZE + 2));

	/* zero the csum fields before hashing */
	memset(p + INO_OFF_CHECKSUM_LO, 0, 2);
	if (have_hi)
		memset(p + INO_OFF_CHECKSUM_HI, 0, 2);

	crc = ext4_crc32c(emp->em_csum_seed, &ino_le, sizeof(ino_le));
	crc = ext4_crc32c(crc, &gen_le, sizeof(gen_le));
	crc = ext4_crc32c(crc, p, emp->em_inode_size);

	lo = OSSwapHostToLittleInt16((uint16_t)(crc & 0xffff));
	memcpy(p + INO_OFF_CHECKSUM_LO, &lo, sizeof(lo));
	if (have_hi) {
		hi = OSSwapHostToLittleInt16((uint16_t)((crc >> 16) & 0xffff));
		memcpy(p + INO_OFF_CHECKSUM_HI, &hi, sizeof(hi));
	}
}

int
ext4_inode_csum_verify(struct ext4mount *emp, ino_t ino, const void *raw_full)
{
	const uint8_t *p = (const uint8_t *)raw_full;
	uint8_t tmp[512];
	uint32_t ino_le, gen_le, crc;
	uint16_t stored_lo, stored_hi = 0, extra;
	int have_hi;

	if (!emp->em_has_metadata_csum)
		return 1;
	if (emp->em_inode_size > sizeof(tmp))
		return 0;

	memcpy(tmp, raw_full, emp->em_inode_size);
	memcpy(&stored_lo, p + INO_OFF_CHECKSUM_LO, sizeof(stored_lo));
	memcpy(&extra, p + INO_OFF_EXTRA_ISIZE, sizeof(extra));
	have_hi = (emp->em_inode_size > EXT4_GOOD_OLD_INODE_SIZE &&
	    le16(extra) >= (INO_OFF_CHECKSUM_HI - INO_OFF_EXTRA_ISIZE + 2));
	if (have_hi)
		memcpy(&stored_hi, p + INO_OFF_CHECKSUM_HI, sizeof(stored_hi));

	memset(tmp + INO_OFF_CHECKSUM_LO, 0, 2);
	if (have_hi)
		memset(tmp + INO_OFF_CHECKSUM_HI, 0, 2);

	memcpy(&gen_le, p + 0x64, sizeof(gen_le));
	ino_le = OSSwapHostToLittleInt32((uint32_t)ino);
	crc = ext4_crc32c(emp->em_csum_seed, &ino_le, sizeof(ino_le));
	crc = ext4_crc32c(crc, &gen_le, sizeof(gen_le));
	crc = ext4_crc32c(crc, tmp, emp->em_inode_size);

	if (le16(stored_lo) != (uint16_t)(crc & 0xffff))
		return 0;
	if (have_hi && le16(stored_hi) != (uint16_t)((crc >> 16) & 0xffff))
		return 0;
	return 1;
}

#define SB_OFF_CHECKSUM 0x3fc

void
ext4_superblock_csum_set(struct ext4mount *emp, void *sb_buf)
{
	uint8_t *p = (uint8_t *)sb_buf;
	uint32_t crc;
	if (!emp->em_has_metadata_csum)
		return;
	crc = ext4_crc32c(~0u, p, SB_OFF_CHECKSUM);
	crc = OSSwapHostToLittleInt32(crc);
	memcpy(p + SB_OFF_CHECKSUM, &crc, sizeof(crc));
}

int
ext4_dir_block_has_tail(struct ext4mount *emp, const void *block)
{
	const struct ext4_dir_entry_tail *tail;

	if (!emp->em_has_metadata_csum ||
	    emp->em_blocksize < EXT4_DIR_ENTRY_TAIL_SIZE)
		return 0;

	tail = (const struct ext4_dir_entry_tail *)
	    ((const char *)block + emp->em_blocksize - EXT4_DIR_ENTRY_TAIL_SIZE);
	return tail->det_reserved_zero1 == 0 &&
	    le16(tail->det_rec_len) == EXT4_DIR_ENTRY_TAIL_SIZE &&
	    tail->det_reserved_zero2 == 0 &&
	    tail->det_reserved_ft == EXT4_FT_DIR_CSUM;
}

void
ext4_dir_block_init_tail(struct ext4mount *emp, void *block)
{
	struct ext4_dir_entry_tail *tail;

	if (!emp->em_has_metadata_csum ||
	    emp->em_blocksize < EXT4_DIR_ENTRY_TAIL_SIZE)
		return;

	tail = (struct ext4_dir_entry_tail *)
	    ((char *)block + emp->em_blocksize - EXT4_DIR_ENTRY_TAIL_SIZE);
	tail->det_reserved_zero1 = 0;
	tail->det_rec_len = le16(EXT4_DIR_ENTRY_TAIL_SIZE);
	tail->det_reserved_zero2 = 0;
	tail->det_reserved_ft = EXT4_FT_DIR_CSUM;
	tail->det_checksum = 0;
}

void
ext4_dir_block_csum_set(struct ext4mount *emp, ino_t ino,
    const struct ext4_inode *inode, void *block)
{
	struct ext4_dir_entry_tail *tail;
	uint32_t ino_le, gen_le, crc;

	if (!emp->em_has_metadata_csum ||
	    emp->em_blocksize < EXT4_DIR_ENTRY_TAIL_SIZE)
		return;

	tail = (struct ext4_dir_entry_tail *)
	    ((char *)block + emp->em_blocksize - EXT4_DIR_ENTRY_TAIL_SIZE);
	if (!ext4_dir_block_has_tail(emp, block))
		return;

	tail->det_checksum = 0;
	ino_le = OSSwapHostToLittleInt32((uint32_t)ino);
	gen_le = inode != NULL ? inode->i_generation : 0;
	crc = ext4_crc32c(emp->em_csum_seed, &ino_le, sizeof(ino_le));
	crc = ext4_crc32c(crc, &gen_le, sizeof(gen_le));
	crc = ext4_crc32c(crc, block,
	    emp->em_blocksize - EXT4_DIR_ENTRY_TAIL_SIZE);
	tail->det_checksum = OSSwapHostToLittleInt32(crc);
}

void
ext4_extent_block_csum_set(struct ext4mount *emp, ino_t ino,
    const struct ext4_inode *inode, void *block)
{
	struct ext4_extent_header *eh = (struct ext4_extent_header *)block;
	uint32_t ino_le, gen_le, crc;
	size_t tail_off;

	if (!emp->em_has_metadata_csum)
		return;
	if (le16(eh->eh_magic) != EXT4_EXT_MAGIC)
		return;

	tail_off = sizeof(struct ext4_extent_header) +
	    (size_t)le16(eh->eh_max) * sizeof(struct ext4_extent);
	if (tail_off + sizeof(uint32_t) > emp->em_blocksize)
		return;

	ino_le = OSSwapHostToLittleInt32((uint32_t)ino);
	gen_le = inode != NULL ? inode->i_generation : 0;
	crc = ext4_crc32c(emp->em_csum_seed, &ino_le, sizeof(ino_le));
	crc = ext4_crc32c(crc, &gen_le, sizeof(gen_le));
	crc = ext4_crc32c(crc, block, tail_off);
	crc = OSSwapHostToLittleInt32(crc);
	memcpy((char *)block + tail_off, &crc, sizeof(crc));
}

/*
 * sparse_super: backup superblock + GDT copies live only in group 0 and in
 * groups that are a power of 3, 5 or 7. (We assume sparse_super, which every
 * modern mke2fs sets; without it every group has a backup, handled by the
 * generic "always true" path.)
 */
static int
is_power_of(uint32_t grp, uint32_t p)
{
	uint32_t n = p;
	if (grp == 0)
		return 1;
	while (n < grp)
		n *= p;
	return n == grp;
}

int
ext4_group_has_super(struct ext4mount *emp, uint32_t grp)
{
	if ((emp->em_feature_ro_compat & 0x0001 /* SPARSE_SUPER */) == 0)
		return 1;
	if (grp == 0 || grp == 1)
		return 1;
	return is_power_of(grp, 3) || is_power_of(grp, 5) || is_power_of(grp, 7);
}

/*
 * Synthesize the block bitmap for a BLOCK_UNINIT group. Marks the group's own
 * super/GDT/reserved-GDT backup blocks (when present) plus every group's
 * bitmap/inode-table block that physically lands inside this group (covers the
 * flex_bg packed-metadata layout). The result is only trusted if its free
 * count matches the descriptor - otherwise the caller falls back to skipping
 * the group, so a miscomputed layout can never cause over-allocation.
 */
int
ext4_init_block_bitmap(struct ext4mount *emp, uint32_t grp,
    const struct ext4_group_desc *gd, uint8_t *map, uint32_t *free_out)
{
	uint64_t group_first = emp->em_first_data_block +
	    (uint64_t)grp * emp->em_blocks_per_group;
	uint64_t group_end = group_first + emp->em_blocks_per_group;
	uint32_t blocks_in_group = emp->em_blocks_per_group;
	uint32_t used = 0, i, j;
	uint32_t desc_free;

	if (group_end > emp->em_blocks_count) {
		group_end = emp->em_blocks_count;
		blocks_in_group = (uint32_t)(group_end - group_first);
	}

	memset(map, 0, emp->em_blocksize);

	/* mark a physical block used if it belongs to this group */
	#define MARK(pblk) do { \
		uint64_t _b = (pblk); \
		if (_b >= group_first && _b < group_end) { \
			uint32_t _bit = (uint32_t)(_b - group_first); \
			if (!ext4_bitmap_test(map, _bit)) { \
				ext4_bitmap_set(map, _bit); used++; \
			} \
		} \
	} while (0)

	/* this group's own superblock + GDT + reserved GDT backup */
	if (ext4_group_has_super(emp, grp)) {
		uint64_t b = group_first;
		uint32_t n = 1 + emp->em_gdt_blocks + emp->em_reserved_gdt_blocks;
		for (i = 0; i < n; i++)
			MARK(b + i);
	}

	/* every group's bitmaps + inode table (may be packed here via flex_bg) */
	for (j = 0; j < emp->em_groups_count; j++) {
		struct ext4_group_desc jgd;
		uint64_t bbmp, ibmp, itbl;
		if (ext4_read_group_desc(emp, j, &jgd) != 0)
			return 1;
		bbmp = ext4_gd_block_bitmap(emp, &jgd);
		ibmp = ext4_gd_inode_bitmap(emp, &jgd);
		itbl = le32(jgd.bg_inode_table_lo);
		if (emp->em_desc_size >= 64)
			itbl |= ((uint64_t)le32(jgd.bg_inode_table_hi)) << 32;
		MARK(bbmp);
		MARK(ibmp);
		for (i = 0; i < emp->em_itable_blocks; i++)
			MARK(itbl + i);
	}

	/* pad bits beyond the last group's real blocks are "used" */
	for (i = blocks_in_group; i < emp->em_blocks_per_group; i++) {
		if (!ext4_bitmap_test(map, i)) {
			ext4_bitmap_set(map, i);
			used++;
		}
	}

	desc_free = le16(gd->bg_free_blocks_count_lo);
	if (emp->em_desc_size >= 64)
		desc_free |= ((uint32_t)le16(gd->bg_free_blocks_count_hi)) << 16;

	if (blocks_in_group - used != desc_free) {
		E4LOG("init_block_bitmap grp %u: computed free %u != desc %u; "
		    "not trusting synthesized bitmap", grp,
		    blocks_in_group - used, desc_free);
		return 1;
	}
	if (free_out)
		*free_out = blocks_in_group - used;
	#undef MARK
	return 0;
}
