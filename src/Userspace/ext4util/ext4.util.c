/*
 * ext4.util - the loadable_fs(5) utility for PureDarwin's ext4.kext.
 *
 * diskarbitrationd identifies a volume by running the .util named in its
 * filesystem bundle's FSProbeExecutable:
 *
 *     ext4.util -p <bsdname> removable|fixed readonly|writable
 *
 * and reading the exit status plus stdout. The contract is sys/loadable_fs.h:
 *
 *     -p   FSUR_RECOGNIZED, printing the volume name on stdout, or
 *          FSUR_UNRECOGNIZED if this is not an ext4 volume
 *     -k   print the volume UUID on stdout
 *     -q   report whether the volume is clean (does it need fsck)
 */

#include <sys/loadable_fs.h>
#include <sys/param.h>

#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * ext2/3/4 superblock, at byte 1024 of the volume. Only the fields needed here
 * are named; the rest is padding to keep the offsets honest.
 */
#define EXT4_SUPERBLOCK_OFFSET  1024
#define EXT4_SUPER_MAGIC        0xEF53

/* s_state */
#define EXT4_VALID_FS           0x0001  /* cleanly unmounted */
#define EXT4_ERROR_FS           0x0002  /* errors detected */

/* s_feature_incompat bits this driver must understand to mount at all. */
#define EXT4_FEATURE_INCOMPAT_SUPP_MASK 0x0002fdff

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
	uint32_t s_first_ino;
	uint16_t s_inode_size;
	uint16_t s_block_group_nr;
	uint32_t s_feature_compat;
	uint32_t s_feature_incompat;
	uint32_t s_feature_ro_compat;
	uint8_t  s_uuid[16];
	char     s_volume_name[16];
	char     s_last_mounted[64];
} __attribute__((packed));

/* The superblock is little-endian on every ext4 volume, regardless of host. */
static uint16_t
le16(uint16_t v)
{
	const uint8_t *p = (const uint8_t *)&v;

	return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t
le32(uint32_t v)
{
	const uint8_t *p = (const uint8_t *)&v;

	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int
read_superblock(const char *bsdname, struct ext4_super_block *sb)
{
	char path[MAXPATHLEN];
	int  fd;
	ssize_t n;

	snprintf(path, sizeof(path), "/dev/r%s", bsdname);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		/* Some callers hand us a full path already. */
		snprintf(path, sizeof(path), "%s", bsdname);
		fd = open(path, O_RDONLY);
		if (fd < 0) {
			return -1;
		}
	}

	n = pread(fd, sb, sizeof(*sb), EXT4_SUPERBLOCK_OFFSET);
	close(fd);

	return (n == (ssize_t)sizeof(*sb)) ? 0 : -1;
}

static int
is_ext4(const struct ext4_super_block *sb)
{
	return le16(sb->s_magic) == EXT4_SUPER_MAGIC;
}

int
main(int argc, char *argv[])
{
	struct ext4_super_block sb;
	const char *bsdname;
	char        action;

	if (argc < 3 || argv[1][0] != '-') {
		fprintf(stderr, "usage: %s -p|-k|-q <device> [removable|fixed]"
				" [readonly|writable]\n", getprogname());
		return FSUR_INVAL;
	}

	action  = argv[1][1];
	bsdname = argv[2];

	if (read_superblock(bsdname, &sb) != 0) {
		return FSUR_IO_FAIL;
	}

	if (!is_ext4(&sb)) {
		return FSUR_UNRECOGNIZED;
	}

	switch (action) {
	case FSUC_PROBE: {
		if (le32(sb.s_feature_incompat) & ~EXT4_FEATURE_INCOMPAT_SUPP_MASK) {
			return FSUR_UNRECOGNIZED;
		}

		if (sb.s_volume_name[0] != '\0') {
			printf("%.*s\n", (int)sizeof(sb.s_volume_name),
			       sb.s_volume_name);
		}

		return FSUR_RECOGNIZED;
	}

	case 'k': {
		const uint8_t *u = sb.s_uuid;
		int            i;
		int            allzero = 1;

		for (i = 0; i < 16; i++) {
			if (u[i] != 0) {
				allzero = 0;
				break;
			}
		}

		/* A volume with no UUID has nothing to report. */
		if (allzero) {
			return FSUR_IO_FAIL;
		}

		printf("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-"
		       "%02X%02X%02X%02X%02X%02X\n",
		       u[0], u[1], u[2],  u[3],  u[4],  u[5],  u[6],  u[7],
		       u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);

		return FSUR_IO_SUCCESS;
	}

	case 'q': {
		uint16_t state = le16(sb.s_state);

		/*
		 * Clean means cleanly unmounted with no errors recorded. DA uses
		 * this to decide whether a repair pass is needed before mount.
		 */
		if ((state & EXT4_VALID_FS) && !(state & EXT4_ERROR_FS)) {
			return FSUR_IO_SUCCESS;
		}

		return FSUR_IO_FAIL;
	}

	default:
		return FSUR_INVAL;
	}
}
