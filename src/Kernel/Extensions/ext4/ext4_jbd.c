/*
 * ext4_jbd.c - JBD2 journal: replay on mount, write-ahead commit per vnop.
 */
#include "ext4.h"
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <string.h>

#define JBD2_MAGIC              0xc03b3998u

#define JBD2_DESCRIPTOR_BLOCK   1
#define JBD2_COMMIT_BLOCK       2
#define JBD2_SUPERBLOCK_V1      3
#define JBD2_SUPERBLOCK_V2      4
#define JBD2_REVOKE_BLOCK       5

#define JBD2_FEATURE_INCOMPAT_REVOKE       0x00000001
#define JBD2_FEATURE_INCOMPAT_64BIT        0x00000002
#define JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT 0x00000004
#define JBD2_FEATURE_INCOMPAT_CSUM_V2      0x00000008
#define JBD2_FEATURE_INCOMPAT_CSUM_V3      0x00000010
#define JBD2_FEATURE_INCOMPAT_FAST_COMMIT  0x00000020

#define JBD2_FLAG_ESCAPE    1
#define JBD2_FLAG_SAME_UUID 2
#define JBD2_FLAG_DELETED   4
#define JBD2_FLAG_LAST_TAG  8

#define be32(x) OSSwapBigToHostInt32(x)
#define be16(x) OSSwapBigToHostInt16(x)
#define to_be32(x) OSSwapHostToBigInt32(x)
#define to_be64(x) OSSwapHostToBigInt64(x)

struct jbd2_header {
	uint32_t h_magic;
	uint32_t h_blocktype;
	uint32_t h_sequence;
} __attribute__((packed));

struct jbd2_superblock {
	struct jbd2_header s_header;
	uint32_t s_blocksize;
	uint32_t s_maxlen;
	uint32_t s_first;
	uint32_t s_sequence;
	uint32_t s_start;
	uint32_t s_errno;
	uint32_t s_feature_compat;
	uint32_t s_feature_incompat;
	uint32_t s_feature_ro_compat;
	uint8_t  s_uuid[16];
	uint32_t s_nr_users;
	uint32_t s_dynsuper;
	uint32_t s_max_transaction;
	uint32_t s_max_trans_data;
	uint8_t  s_checksum_type;
	uint8_t  s_padding2[3];
	uint32_t s_num_fc_blks;
	uint32_t s_head;
	uint32_t s_padding[40];
	uint32_t s_checksum;
	uint8_t  s_users[16 * 48];
} __attribute__((packed));

struct jbd2_block_tag3 {
	uint32_t t_blocknr;
	uint32_t t_flags;
	uint32_t t_blocknr_high;
	uint32_t t_checksum;
} __attribute__((packed));

struct jbd2_block_tag {
	uint32_t t_blocknr;
	uint16_t t_checksum;
	uint16_t t_flags;
	/* uint32_t t_blocknr_high follows if 64BIT */
} __attribute__((packed));

struct jbd2_commit_header {
	struct jbd2_header h_header;
	uint8_t  h_chksum_type;
	uint8_t  h_chksum_size;
	uint8_t  h_padding[2];
	uint32_t h_chksum[8];
	uint64_t h_commit_sec;
	uint32_t h_commit_nsec;
} __attribute__((packed));

struct jbd2_revoke_header {
	struct jbd2_header r_header;
	uint32_t r_count;
} __attribute__((packed));

/* one deferred metadata block in the open transaction */
struct e4j_txblk {
	uint64_t pblk;
	char    *data;          /* em_blocksize bytes */
};

#define E4J_MAX_TX_BLOCKS 128   /* per-vnop metadata writes; loudly enforced */

struct e4jnl {
	struct ext4_inode j_inode;   /* journal inode (s_journal_inum) */
	ino_t     j_ino;
	uint32_t  j_maxlen;          /* journal length in fs blocks */
	uint32_t  j_first;           /* first log block (usually 1) */
	uint32_t  j_sequence;        /* next transaction sequence */
	uint32_t  j_features;        /* jsb s_feature_incompat */
	uint32_t  j_csum_seed;       /* crc32c(~0, jsb uuid) */
	int       j_csum3;           /* CSUM_V3 in effect */
	uint64_t *j_map;             /* journal lblk -> fs pblk (j_maxlen entries) */
	/* open transaction */
	struct e4j_txblk j_tx[E4J_MAX_TX_BLOCKS];
	uint32_t  j_tx_count;
	uint64_t  j_tx_first_us;     /* microuptime when the txn opened */
	uint32_t  j_sb_start;        /* jsb (start,seq) last written to disk - */
	uint32_t  j_sb_seq;          /*   lets commit skip the redundant re-mark */
	int       j_tx_overflow;     /* txn exceeded E4J_MAX_TX_BLOCKS: fall back
	                                to write-through for the rest (logged) */
};

static int e4j_write_jsb(struct ext4mount *emp, struct e4jnl *j,
    uint32_t start, uint32_t sequence);

static void
e4j_flush_device(struct ext4mount *emp)
{
	(void)VNOP_IOCTL(emp->em_devvp, DKIOCSYNCHRONIZECACHE, NULL, FWRITE,
	    vfs_context_current());
}

/* Read/write one journal block (journal-file logical block jlblk). */
static int
e4j_read(struct ext4mount *emp, struct e4jnl *j, uint32_t jlblk, char *out)
{
	buf_t bp = NULL;
	int error;

	if (jlblk >= j->j_maxlen || j->j_map[jlblk] == 0)
		return EIO;
	error = ext4_blkread(emp, j->j_map[jlblk], &bp);
	if (error)
		return error;
	memcpy(out, (char *)buf_dataptr(bp), emp->em_blocksize);
	buf_brelse(bp);
	return 0;
}

static int
e4j_write(struct ext4mount *emp, struct e4jnl *j, uint32_t jlblk,
    const char *data)
{
	buf_t bp = NULL;
	int error;

	if (jlblk >= j->j_maxlen || j->j_map[jlblk] == 0)
		return EIO;
	error = ext4_blkread(emp, j->j_map[jlblk], &bp);
	if (error)
		return error;
	memcpy((char *)buf_dataptr(bp), data, emp->em_blocksize);
	/* async issue: commit's DKIOCSYNCHRONIZECACHE is the ordering
	 * barrier (the AHCI path executes queued writes before the flush),
	 * so there is no reason to stall per block. jsb writes (e4j_write_jsb
	 * -> e4j_write) share this: the flush after them orders them too. */
	return buf_bawrite(bp);
}

/* Write a whole fs block in place (used by replay and checkpoint). */
static int
e4j_write_fs_block(struct ext4mount *emp, uint64_t pblk, const char *data)
{
	buf_t bp = NULL;
	int error;

	error = ext4_blkread(emp, pblk, &bp);
	if (error)
		return error;
	memcpy((char *)buf_dataptr(bp), data, emp->em_blocksize);
	return buf_bawrite(bp);   /* async; flush after the batch is the barrier */
}

struct e4j_revoke {
	uint64_t blocknr;
	uint32_t seq;
};

#define E4J_MAX_REVOKES 1024

static int
e4j_tag_size(const struct e4jnl *j)
{
	if (j->j_csum3)
		return (int)sizeof(struct jbd2_block_tag3);
	if (j->j_features & JBD2_FEATURE_INCOMPAT_64BIT)
		return (int)sizeof(struct jbd2_block_tag) + 4;
	return (int)sizeof(struct jbd2_block_tag);
}

/* Parse one tag at `p`; returns blocknr/flags. */
static void
e4j_parse_tag(const struct e4jnl *j, const char *p, uint64_t *blocknr,
    uint32_t *flags)
{
	if (j->j_csum3) {
		const struct jbd2_block_tag3 *t = (const void *)p;
		*blocknr = be32(t->t_blocknr) |
		    ((uint64_t)be32(t->t_blocknr_high) << 32);
		*flags = be32(t->t_flags);
	} else {
		const struct jbd2_block_tag *t = (const void *)p;
		*blocknr = be32(t->t_blocknr);
		if (j->j_features & JBD2_FEATURE_INCOMPAT_64BIT)
			*blocknr |= ((uint64_t)be32(*(const uint32_t *)
			    (p + sizeof(*t))) << 32);
		*flags = be16(t->t_flags);
	}
}

static uint32_t
e4j_wrap(const struct e4jnl *j, uint32_t blk)
{
	if (blk >= j->j_maxlen)
		blk = j->j_first + (blk - j->j_maxlen);
	return blk;
}

/*
 * Replay the journal. Standard three-pass structure (scan/revoke/replay),
 * simplified: revoke table is a flat array.
 */
static int
e4j_replay(struct ext4mount *emp, struct e4jnl *j, uint32_t start,
    uint32_t first_seq)
{
	uint32_t bs = emp->em_blocksize;
	char *blk = NULL, *data = NULL;
	struct e4j_revoke *revokes = NULL;
	uint32_t nrevokes = 0;
	uint32_t seq, blkno, end_seq;
	int pass, error = 0;
	uint32_t replayed = 0;

	blk = (char *)_MALLOC(bs, M_TEMP, M_WAITOK);
	data = (char *)_MALLOC(bs, M_TEMP, M_WAITOK);
	revokes = (struct e4j_revoke *)_MALLOC(
	    sizeof(*revokes) * E4J_MAX_REVOKES, M_TEMP, M_WAITOK | M_ZERO);
	if (!blk || !data || !revokes) {
		error = ENOMEM;
		goto out;
	}

	end_seq = first_seq;

	/* pass 0: scan to find the last complete transaction (end_seq).
	 * pass 1: collect revoke records.
	 * pass 2: apply data blocks of transactions < end_seq. */
	for (pass = 0; pass < 3; pass++) {
		seq = first_seq;
		blkno = start;

		for (;;) {
			struct jbd2_header *h;
			uint32_t btype, hseq;

			if (pass != 0 && seq >= end_seq)
				break;
			error = e4j_read(emp, j, blkno, blk);
			if (error)
				goto scan_done;
			h = (struct jbd2_header *)blk;
			if (be32(h->h_magic) != JBD2_MAGIC)
				goto scan_done;
			btype = be32(h->h_blocktype);
			hseq = be32(h->h_sequence);
			if (hseq != seq)
				goto scan_done;

			if (btype == JBD2_DESCRIPTOR_BLOCK) {
				/* walk tags; data blocks follow the descriptor */
				uint32_t limit = bs - (j->j_csum3 ? 4 : 0);
				uint32_t p = sizeof(struct jbd2_header);
				uint32_t dblk = e4j_wrap(j, blkno + 1);
				int tagsz = e4j_tag_size(j);

				while (p + (uint32_t)tagsz <= limit) {
					uint64_t tgt = 0;
					uint32_t flags = 0;

					e4j_parse_tag(j, blk + p, &tgt, &flags);
					p += tagsz;
					if (!(flags & JBD2_FLAG_SAME_UUID))
						p += 16;

					if (pass == 2) {
						uint32_t r;
						int revoked = 0;

						for (r = 0; r < nrevokes; r++) {
							if (revokes[r].blocknr == tgt &&
							    revokes[r].seq >= seq) {
								revoked = 1;
								break;
							}
						}
						if (!revoked) {
							error = e4j_read(emp, j, dblk, data);
							if (error)
								goto scan_done;
							if (flags & JBD2_FLAG_ESCAPE) {
								uint32_t m = to_be32(JBD2_MAGIC);
								memcpy(data, &m, 4);
							}
							error = e4j_write_fs_block(emp, tgt, data);
							if (error)
								goto scan_done;
							replayed++;
						}
					}
					dblk = e4j_wrap(j, dblk + 1);
					if (flags & JBD2_FLAG_LAST_TAG)
						break;
				}
				blkno = dblk;
			} else if (btype == JBD2_COMMIT_BLOCK) {
				seq++;
				if (pass == 0)
					end_seq = seq;
				blkno = e4j_wrap(j, blkno + 1);
			} else if (btype == JBD2_REVOKE_BLOCK) {
				if (pass == 1) {
					struct jbd2_revoke_header *rh =
					    (struct jbd2_revoke_header *)blk;
					uint32_t count = be32(rh->r_count);
					uint32_t p = sizeof(*rh);
					int w = (j->j_features &
					    (JBD2_FEATURE_INCOMPAT_64BIT)) ? 8 : 4;

					if (count > bs)
						count = bs;
					while (p + (uint32_t)w <= count &&
					    nrevokes < E4J_MAX_REVOKES) {
						uint64_t rb;
						if (w == 8)
							rb = OSSwapBigToHostInt64(
							    *(uint64_t *)(blk + p));
						else
							rb = be32(*(uint32_t *)(blk + p));
						revokes[nrevokes].blocknr = rb;
						revokes[nrevokes].seq = seq;
						nrevokes++;
						p += w;
					}
				}
				blkno = e4j_wrap(j, blkno + 1);
			} else {
				goto scan_done;
			}
		}
scan_done:
		error = 0;   /* scan termination is normal */
	}

	if (end_seq > first_seq) {
		E4LOG("journal replay: %u block(s) from txn %u..%u",
		    replayed, first_seq, end_seq - 1);
		emp->em_stats.jnl_replays = replayed;
	}

	/* Mark journal clean past everything we replayed. */
	error = e4j_write_jsb(emp, j, 0, end_seq + 1);
	if (error == 0)
		e4j_flush_device(emp);
	j->j_sequence = end_seq + 1;

out:
	if (blk)
		_FREE(blk, M_TEMP);
	if (data)
		_FREE(data, M_TEMP);
	if (revokes)
		_FREE(revokes, M_TEMP);
	return error;
}

static int
e4j_write_jsb(struct ext4mount *emp, struct e4jnl *j, uint32_t start,
    uint32_t sequence)
{
	char *blk;
	struct jbd2_superblock *sb;
	int error;

	blk = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
	if (blk == NULL)
		return ENOMEM;
	error = e4j_read(emp, j, 0, blk);
	if (error) {
		_FREE(blk, M_TEMP);
		return error;
	}
	sb = (struct jbd2_superblock *)blk;
	sb->s_start = to_be32(start);
	sb->s_sequence = to_be32(sequence);
	sb->s_errno = 0;
	if (j->j_features &
	    (JBD2_FEATURE_INCOMPAT_CSUM_V2 | JBD2_FEATURE_INCOMPAT_CSUM_V3)) {
		sb->s_checksum = 0;
		sb->s_checksum = to_be32(ext4_crc32c(~0u, sb, sizeof(*sb)));
	}
	error = e4j_write(emp, j, 0, blk);
	_FREE(blk, M_TEMP);
	return error;
}

int
ext4_jnl_mount(struct ext4mount *emp)
{
	struct e4jnl *j = NULL;
	struct jbd2_superblock *jsb;
	uint32_t jnl_ino = le32(emp->em_sb.s_journal_inum);
	uint64_t jsize;
	uint32_t i;
	char *blk = NULL;
	int error;

	emp->em_jnl = NULL;
	if (jnl_ino == 0) {
		E4LOG("no journal (s_journal_inum=0)");
		return 0;
	}

	j = (struct e4jnl *)_MALLOC(sizeof(*j), M_TEMP, M_WAITOK | M_ZERO);
	if (j == NULL)
		return ENOMEM;
	j->j_ino = jnl_ino;

	error = ext4_read_inode(emp, jnl_ino, &j->j_inode);
	if (error) {
		E4LOG("journal inode %u unreadable: %d", jnl_ino, error);
		goto fail;
	}
	jsize = le32(j->j_inode.i_size_lo) |
	    ((uint64_t)le32(j->j_inode.i_size_high) << 32);
	j->j_maxlen = (uint32_t)(jsize / emp->em_blocksize);
	if (j->j_maxlen < 8) {
		E4LOG("journal too small (%u blocks)", j->j_maxlen);
		error = EINVAL;
		goto fail;
	}

	/* Build the journal-lblk -> fs-pblk map once. */
	j->j_map = (uint64_t *)_MALLOC(sizeof(uint64_t) * j->j_maxlen, M_TEMP,
	    M_WAITOK | M_ZERO);
	if (j->j_map == NULL) {
		error = ENOMEM;
		goto fail;
	}
	for (i = 0; i < j->j_maxlen; i++) {
		uint64_t pblk = 0;
		error = ext4_bmap(emp, jnl_ino, &j->j_inode, i, &pblk);
		if (error || pblk == 0) {
			E4LOG("journal lblk %u unmapped (err %d)", i, error);
			error = error ? error : EIO;
			goto fail;
		}
		j->j_map[i] = pblk;
	}

	blk = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
	if (blk == NULL) {
		error = ENOMEM;
		goto fail;
	}
	error = e4j_read(emp, j, 0, blk);
	if (error)
		goto fail;
	jsb = (struct jbd2_superblock *)blk;
	if (be32(jsb->s_header.h_magic) != JBD2_MAGIC ||
	    (be32(jsb->s_header.h_blocktype) != JBD2_SUPERBLOCK_V1 &&
	     be32(jsb->s_header.h_blocktype) != JBD2_SUPERBLOCK_V2)) {
		E4LOG("bad journal superblock (magic 0x%x type %u)",
		    be32(jsb->s_header.h_magic), be32(jsb->s_header.h_blocktype));
		error = EINVAL;
		goto fail;
	}
	if (be32(jsb->s_blocksize) != emp->em_blocksize) {
		E4LOG("journal blocksize %u != fs blocksize %u",
		    be32(jsb->s_blocksize), emp->em_blocksize);
		error = EINVAL;
		goto fail;
	}
	if (be32(jsb->s_maxlen) < j->j_maxlen)
		j->j_maxlen = be32(jsb->s_maxlen);
	j->j_first = be32(jsb->s_first);
	if (j->j_first == 0)
		j->j_first = 1;
	j->j_sequence = be32(jsb->s_sequence);
	if (j->j_sequence == 0)
		j->j_sequence = 1;
	j->j_features = be32(jsb->s_feature_incompat);
	j->j_csum3 = (j->j_features & JBD2_FEATURE_INCOMPAT_CSUM_V3) != 0;
	j->j_csum_seed = ext4_crc32c(~0u, jsb->s_uuid, sizeof(jsb->s_uuid));

	if (j->j_features & JBD2_FEATURE_INCOMPAT_FAST_COMMIT) {
		E4LOG("journal has fast-commit blocks; not supported");
		error = EINVAL;
		goto fail;
	}

	E4LOG("journal: ino=%u len=%u first=%u seq=%u features=0x%x csum3=%d",
	    jnl_ino, j->j_maxlen, j->j_first, j->j_sequence, j->j_features,
	    j->j_csum3);

	if (be32(jsb->s_start) != 0) {
		E4LOG("journal dirty (start=%u seq=%u): replaying",
		    be32(jsb->s_start), j->j_sequence);
		emp->em_jnl = j;   /* replay uses e4j_* helpers */
		error = e4j_replay(emp, j, be32(jsb->s_start), j->j_sequence);
		emp->em_jnl = NULL;
		if (error) {
			E4LOG("journal replay failed: %d", error);
			goto fail;
		}
	}

	_FREE(blk, M_TEMP);

	/* Mark the fs "journal in use" the way real ext4 does: the RECOVER
	 * incompat flag tells e2fsck/other implementations to run journal
	 * recovery before touching anything. Set while mounted rw (em_jnl is
	 * still NULL here so this write-throughs, unjournaled); cleared by a
	 * clean unmount. */
	emp->em_sb.s_feature_incompat =
	    le32(le32(emp->em_sb.s_feature_incompat) | EXT4_FEATURE_INCOMPAT_RECOVER);
	(void)ext4_write_super(emp);

	emp->em_jnl = j;
	return 0;

fail:
	if (blk)
		_FREE(blk, M_TEMP);
	if (j) {
		if (j->j_map)
			_FREE(j->j_map, M_TEMP);
		_FREE(j, M_TEMP);
	}
	return error;
}

void
ext4_jnl_unmount(struct ext4mount *emp)
{
	struct e4jnl *j = (struct e4jnl *)emp->em_jnl;
	uint32_t i;

	if (j == NULL)
		return;
	/* flush any still-open batched txn, then mark the journal clean */
	(void)ext4_jnl_commit(emp);
	(void)e4j_write_jsb(emp, j, 0, j->j_sequence);
	/* clean unmount: drop the RECOVER flag (write-through, journal off) */
	emp->em_jnl = NULL;
	emp->em_sb.s_feature_incompat =
	    le32(le32(emp->em_sb.s_feature_incompat) & ~EXT4_FEATURE_INCOMPAT_RECOVER);
	(void)ext4_write_super(emp);
	emp->em_jnl = j;
	e4j_flush_device(emp);
	for (i = 0; i < j->j_tx_count; i++)
		if (j->j_tx[i].data)
			_FREE(j->j_tx[i].data, M_TEMP);
	if (j->j_map)
		_FREE(j->j_map, M_TEMP);
	_FREE(j, M_TEMP);
	emp->em_jnl = NULL;
}

/*
 * Commit policy: batch transactions instead of committing on every vnop.
 * A txn is committed once it holds >= 64 blocks or has been open ~1s
 * (mirroring ext4's own 5s commit interval, but tighter). fsync/VFS_SYNC/
 * unmount force a commit regardless. The crash window is the last <1s of
 * metadata ops; the fs itself stays consistent either way.
 */
#define E4J_COMMIT_BLOCKS   64
#define E4J_COMMIT_AGE_US   1000000ull

int
ext4_jnl_should_commit(struct ext4mount *emp)
{
	struct e4jnl *j = (struct e4jnl *)emp->em_jnl;
	struct timeval tv;

	if (j == NULL || (j->j_tx_count == 0 && !j->j_tx_overflow))
		return 0;
	if (j->j_tx_overflow || j->j_tx_count >= E4J_COMMIT_BLOCKS)
		return 1;
	microuptime(&tv);
	return ((uint64_t)tv.tv_sec * 1000000ull + (uint64_t)tv.tv_usec) -
	    j->j_tx_first_us >= E4J_COMMIT_AGE_US;
}

int
ext4_jnl_log_block(struct ext4mount *emp, uint64_t pblk, const void *data)
{
	struct e4jnl *j = (struct e4jnl *)emp->em_jnl;
	uint32_t i;
	char *copy;

	if (j == NULL)
		return ENOENT;   /* caller falls back to write-through */
	if (j->j_tx_overflow)
		return ENOSPC;
	if (j->j_tx_count == 0) {
		struct timeval tv;
		microuptime(&tv);
		j->j_tx_first_us = (uint64_t)tv.tv_sec * 1000000ull +
		    (uint64_t)tv.tv_usec;
	}

	for (i = 0; i < j->j_tx_count; i++) {
		if (j->j_tx[i].pblk == pblk) {
			memcpy(j->j_tx[i].data, data, emp->em_blocksize);
			return 0;
		}
	}
	if (j->j_tx_count >= E4J_MAX_TX_BLOCKS) {
		E4LOG("journal txn overflow (>%u blocks); falling back to "
		    "write-through for this transaction", E4J_MAX_TX_BLOCKS);
		j->j_tx_overflow = 1;
		return ENOSPC;
	}
	copy = (char *)_MALLOC(emp->em_blocksize, M_TEMP, M_WAITOK);
	if (copy == NULL)
		return ENOMEM;
	memcpy(copy, data, emp->em_blocksize);
	j->j_tx[j->j_tx_count].pblk = pblk;
	j->j_tx[j->j_tx_count].data = copy;
	j->j_tx_count++;
	return 0;
}

int
ext4_jnl_read_overlay(struct ext4mount *emp, uint64_t pblk, void *out)
{
	struct e4jnl *j = (struct e4jnl *)emp->em_jnl;
	uint32_t i;

	if (j == NULL)
		return 0;
	for (i = 0; i < j->j_tx_count; i++) {
		if (j->j_tx[i].pblk == pblk) {
			memcpy(out, j->j_tx[i].data, emp->em_blocksize);
			return 1;
		}
	}
	return 0;
}

int
ext4_jnl_commit(struct ext4mount *emp)
{
	struct e4jnl *j = (struct e4jnl *)emp->em_jnl;
	uint32_t bs = emp->em_blocksize;
	uint32_t seq, jblk, i, done;
	char *blk = NULL;
	int error = 0;

	if (j == NULL || (j->j_tx_count == 0 && !j->j_tx_overflow))
		return 0;
	if (j->j_tx_count == 0) {
		j->j_tx_overflow = 0;
		return 0;
	}

	seq = j->j_sequence;
	blk = (char *)_MALLOC(bs, M_TEMP, M_WAITOK);
	if (blk == NULL) {
		error = ENOMEM;
		goto out;
	}

	/* mark journal dirty: expect txn `seq` at j_first */
	error = e4j_write_jsb(emp, j, j->j_first, seq);
	if (error)
		goto out;

	/* descriptor(s) + data + commit */
	jblk = j->j_first;
	done = 0;
	while (done < j->j_tx_count) {
		struct jbd2_header *h;
		uint32_t limit = bs - (j->j_csum3 ? 4 : 0);
		uint32_t p = sizeof(struct jbd2_header);
		uint32_t first_in_desc = done;
		uint32_t ntags = 0;
		int tagsz = e4j_tag_size(j);

		memset(blk, 0, bs);
		h = (struct jbd2_header *)blk;
		h->h_magic = to_be32(JBD2_MAGIC);
		h->h_blocktype = to_be32(JBD2_DESCRIPTOR_BLOCK);
		h->h_sequence = to_be32(seq);

		/* fill tags (first tag carries a 16-byte null uuid) */
		while (done < j->j_tx_count) {
			uint32_t need = (uint32_t)tagsz + (ntags == 0 ? 16 : 0);
			uint32_t flags;
			uint64_t tgt;
			int escape;

			if (p + need > limit)
				break;
			tgt = j->j_tx[done].pblk;
			escape = (be32(*(uint32_t *)j->j_tx[done].data) == JBD2_MAGIC);
			flags = (ntags == 0 ? 0 : JBD2_FLAG_SAME_UUID) |
			    (escape ? JBD2_FLAG_ESCAPE : 0);
			if (done + 1 == j->j_tx_count ||
			    p + need + (uint32_t)tagsz > limit)
				flags |= JBD2_FLAG_LAST_TAG;

			if (j->j_csum3) {
				struct jbd2_block_tag3 *t = (void *)(blk + p);
				uint32_t seq_be = to_be32(seq);
				uint32_t csum;

				t->t_blocknr = to_be32((uint32_t)tgt);
				t->t_blocknr_high = to_be32((uint32_t)(tgt >> 32));
				t->t_flags = to_be32(flags);
				csum = ext4_crc32c(j->j_csum_seed, &seq_be, 4);
				if (escape) {
					/* checksum covers the escaped (zeroed-
					 * magic) image that lands on disk */
					char tmp[4] = {0, 0, 0, 0};
					csum = ext4_crc32c(csum, tmp, 4);
					csum = ext4_crc32c(csum,
					    j->j_tx[done].data + 4, bs - 4);
				} else {
					csum = ext4_crc32c(csum,
					    j->j_tx[done].data, bs);
				}
				t->t_checksum = to_be32(csum);
			} else {
				struct jbd2_block_tag *t = (void *)(blk + p);
				t->t_blocknr = to_be32((uint32_t)tgt);
				t->t_checksum = 0;
				t->t_flags = OSSwapHostToBigInt16((uint16_t)flags);
				if (j->j_features & JBD2_FEATURE_INCOMPAT_64BIT)
					*(uint32_t *)(blk + p + sizeof(*t)) =
					    to_be32((uint32_t)(tgt >> 32));
			}
			p += (uint32_t)tagsz;
			if (ntags == 0)
				p += 16;   /* null uuid already zeroed */
			ntags++;
			done++;
			if (flags & JBD2_FLAG_LAST_TAG)
				break;
		}

		if (j->j_csum3) {
			uint32_t csum;
			*(uint32_t *)(blk + bs - 4) = 0;
			csum = ext4_crc32c(j->j_csum_seed, blk, bs);
			*(uint32_t *)(blk + bs - 4) = to_be32(csum);
		}

		error = e4j_write(emp, j, jblk, blk);
		if (error)
			goto out;
		jblk = e4j_wrap(j, jblk + 1);

		/* data blocks for this descriptor */
		for (i = first_in_desc; i < done; i++) {
			char *src = j->j_tx[i].data;
			if (be32(*(uint32_t *)src) == JBD2_MAGIC) {
				memcpy(blk, src, bs);
				memset(blk, 0, 4);
				error = e4j_write(emp, j, jblk, blk);
			} else {
				error = e4j_write(emp, j, jblk, src);
			}
			if (error)
				goto out;
			jblk = e4j_wrap(j, jblk + 1);
		}
	}

	/* commit block */
	{
		struct jbd2_commit_header *c;

		memset(blk, 0, bs);
		c = (struct jbd2_commit_header *)blk;
		c->h_header.h_magic = to_be32(JBD2_MAGIC);
		c->h_header.h_blocktype = to_be32(JBD2_COMMIT_BLOCK);
		c->h_header.h_sequence = to_be32(seq);
		if (j->j_csum3 ||
		    (j->j_features & JBD2_FEATURE_INCOMPAT_CSUM_V2)) {
			c->h_chksum[0] = 0;
			c->h_chksum[0] = to_be32(
			    ext4_crc32c(j->j_csum_seed, blk, bs));
		}
		error = e4j_write(emp, j, jblk, blk);
		if (error)
			goto out;
	}

	e4j_flush_device(emp);

	/* checkpoint: apply in place */
	for (i = 0; i < j->j_tx_count; i++) {
		error = e4j_write_fs_block(emp, j->j_tx[i].pblk, j->j_tx[i].data);
		if (error) {
			E4LOG("checkpoint write pblk=%llu failed: %d "
			    "(journal has the data; replay will repair)",
			    (unsigned long long)j->j_tx[i].pblk, error);
			goto out;
		}
	}

	e4j_flush_device(emp);

	/* advance sequence; s_start stays j_first (replay decides by
	 * sequence match, and re-applying a checkpointed txn is idempotent) */
	j->j_sequence = seq + 1;
	(void)e4j_write_jsb(emp, j, j->j_first, j->j_sequence);
	j->j_sb_start = j->j_first;
	j->j_sb_seq = j->j_sequence;

	emp->em_stats.jnl_commits++;

out:
	for (i = 0; i < j->j_tx_count; i++) {
		if (j->j_tx[i].data)
			_FREE(j->j_tx[i].data, M_TEMP);
		j->j_tx[i].data = NULL;
	}
	j->j_tx_count = 0;
	j->j_tx_overflow = 0;
	if (blk)
		_FREE(blk, M_TEMP);
	if (error)
		E4LOG("journal commit failed: %d", error);
	return error;
}
