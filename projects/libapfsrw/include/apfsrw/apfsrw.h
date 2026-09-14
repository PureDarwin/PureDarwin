/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#ifndef APFSRW_APFSRW_H
#define APFSRW_APFSRW_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APFSRW_BLOCK_SIZE 4096U
#define APFSRW_ROOT_FILEID 2ULL

enum apfsrw_error {
    APFSRW_OK = 0,
    APFSRW_EINVAL = -1,
    APFSRW_EIO = -2,
    APFSRW_ENOMEM = -3,
    APFSRW_ENOENT = -4,
    APFSRW_ENOTSUP = -5,
    APFSRW_EOVERFLOW = -6,
    APFSRW_ENOTDIR = -7,
    APFSRW_ECOMPRESSED = -8,
    APFSRW_EPERM = -9,
    APFSRW_EEXIST = -10,
    APFSRW_ENOSPC = -11,
    APFSRW_ENOTEMPTY = -12,
};

enum apfsrw_dirent_type {
    APFSRW_DT_UNKNOWN = 0,
    APFSRW_DT_FIFO = 1,
    APFSRW_DT_CHR = 2,
    APFSRW_DT_REG = 8,
    APFSRW_DT_DIR = 4,
    APFSRW_DT_BLK = 6,
    APFSRW_DT_LNK = 10,
    APFSRW_DT_SOCK = 12,
};

struct apfsrw;

struct apfsrw_volume_info {
    uint32_t block_size;
    uint64_t block_count;
    uint64_t xid;
    uint64_t fs_oid;
    uint64_t fs_paddr;
    uint64_t root_tree_oid;
    uint64_t root_tree_paddr;
    uint64_t next_obj_id;
    uint64_t num_files;
    uint64_t num_directories;
};

struct apfsrw_file_stat {
    uint64_t file_id;
    uint64_t stream_id;
    uint64_t size;
    uint64_t num_extents;
    uint64_t num_holes;
    uint64_t extent_bytes;
    uint64_t first_phys_block;
    uint64_t first_extent_len;
    uint32_t compression_type;
    uint32_t inline_bytes;
    uint64_t compressed_size;
    uint16_t mode;              /* full st_mode, type bits included */
    uint32_t nlink;             /* directories: nchildren + 2 */
    uint32_t uid;
    uint32_t gid;
    uint32_t bsd_flags;
    uint64_t atime_ns;          /* nanoseconds since the epoch */
    uint64_t mtime_ns;
    uint64_t ctime_ns;
    uint64_t crtime_ns;
};

#define APFSRW_MAX_COMPRESSION_TYPES 256

struct apfsrw_xattr {
    char name[256];
    uint16_t flags;
    uint32_t size;
};

struct apfsrw_compression_stats {
    uint64_t total;
    uint64_t other;
    uint64_t bytes;
    uint64_t count[APFSRW_MAX_COMPRESSION_TYPES];
    uint64_t sample_id[APFSRW_MAX_COMPRESSION_TYPES];
    uint64_t sample_size[APFSRW_MAX_COMPRESSION_TYPES];
    uint32_t sample_xdata[APFSRW_MAX_COMPRESSION_TYPES];
    uint64_t inline_plus1[APFSRW_MAX_COMPRESSION_TYPES];
    uint64_t head_hist[APFSRW_MAX_COMPRESSION_TYPES][256];
};

typedef int (*apfsrw_xattr_cb)(const struct apfsrw_xattr *xattr, void *ctx);

struct apfsrw_space_info {
    uint64_t spaceman_paddr;
    uint64_t block_count;
    uint64_t chunk_count;
    uint64_t cib_count;
    uint64_t free_count;
    uint64_t blocks_per_chunk;
    uint64_t chunks_seen;
    uint64_t blocks_seen;
    uint64_t free_seen;
};

struct apfsrw_dirent {
    uint64_t file_id;
    uint8_t type;
    char name[256];
};

typedef int (*apfsrw_dirent_cb)(const struct apfsrw_dirent *entry,
    void *ctx);

int apfsrw_open(const char *path, int writable, struct apfsrw **out);

/* Kernel builds bind to an already-open device instead of a path. io_ctx is
 * a struct apfsrw_kern_dev the caller keeps alive for the mount's lifetime. */
struct apfsrw_kern_dev {
    void *devvp;            /* vnode_t */
    uint32_t dev_bsize;     /* sector size: the unit buf blknos are in */
    uint32_t block_size;    /* container block size: one buffer per block */
};
void *apfsrw_io_context(struct apfsrw *fs);
int apfsrw_open_kernel(void *io_ctx, uint64_t image_blocks, int writable,
    uint64_t xid, struct apfsrw **out);
/* xid == 0 mounts the newest checkpoint; otherwise the newest one whose
 * transaction id does not exceed xid. */
int apfsrw_open_xid(const char *path, int writable, uint64_t xid,
    struct apfsrw **out);
void apfsrw_close(struct apfsrw *fs);
const char *apfsrw_strerror(int error);

int apfsrw_get_volume_info(struct apfsrw *fs,
    struct apfsrw_volume_info *info);
int apfsrw_get_space_info(struct apfsrw *fs, struct apfsrw_space_info *out);
int apfsrw_list_root(struct apfsrw *fs, apfsrw_dirent_cb cb, void *ctx);
/* path is '/'-separated; NULL or "/" lists the root. Symlinks are not
 * followed. */
int apfsrw_list_dir(struct apfsrw *fs, const char *path,
    apfsrw_dirent_cb cb, void *ctx);
int apfsrw_read_root_file(struct apfsrw *fs, const char *path,
    uint8_t **data_out, size_t *size_out);
int apfsrw_read_file(struct apfsrw *fs, const char *path,
    uint8_t **data_out, size_t *size_out);
int apfsrw_stat_file(struct apfsrw *fs, const char *path,
    struct apfsrw_file_stat *out);
int apfsrw_read_file_by_id(struct apfsrw *fs, uint64_t file_id,
    uint8_t **data_out, size_t *size_out);
int apfsrw_read_resource_fork(struct apfsrw *fs, const char *path,
    uint8_t **data_out, size_t *size_out);
int apfsrw_list_xattrs(struct apfsrw *fs, const char *path,
    apfsrw_xattr_cb cb, void *ctx);
int apfsrw_compression_stats(struct apfsrw *fs,
    struct apfsrw_compression_stats *out);

int apfsrw_create_root_file(struct apfsrw *fs, const char *name,
    const void *data, size_t size);
/* Paths are absolute; the parent directory must already exist. */
int apfsrw_create_file(struct apfsrw *fs, const char *path, const void *data,
    size_t size, uint16_t mode, uint32_t uid, uint32_t gid);
/* Group creates into a single transaction: one commit, one pair of fsyncs,
 * instead of one per file. */
int apfsrw_batch_begin(struct apfsrw *fs);
int apfsrw_batch_end(struct apfsrw *fs);
uint32_t apfsrw_batch_pending(struct apfsrw *fs);
int apfsrw_mkdir(struct apfsrw *fs, const char *path, uint16_t mode,
    uint32_t uid, uint32_t gid);
/*
 * Nodes with no contents of their own: sockets, fifos and device nodes. mode
 * carries the full S_IF* type bits, as it does for mkdir/symlink.
 */
int apfsrw_mknod(struct apfsrw *fs, const char *path, uint16_t ftype,
    uint16_t mode, uint32_t uid, uint32_t gid);

/* Remove one entry. Directories must already be empty; a regular file's
 * extents are dropped and its blocks freed. */
int apfsrw_unlink(struct apfsrw *fs, const char *path);
int apfsrw_set_file_content(struct apfsrw *fs, const char *path,
    const void *data, size_t size);
int apfsrw_rename(struct apfsrw *fs, const char *from, const char *to);
/* In-place write of [off, off+len): only the blocks touched are rewritten
 * (copy-on-write); writing past the end grows the file. */
int apfsrw_write_range(struct apfsrw *fs, const char *path, uint64_t off,
    const void *data, size_t len);
/* Shrinking frees the tail; growing leaves a hole that reads as zeros. */
int apfsrw_truncate(struct apfsrw *fs, const char *path, uint64_t size);

/* Change inode attributes; only the fields named in mask are applied. mode
 * takes the permission bits (07777), the type bits are kept. */
#define APFSRW_ATTR_MODE    0x01U
#define APFSRW_ATTR_UID     0x02U
#define APFSRW_ATTR_GID     0x04U
#define APFSRW_ATTR_ATIME   0x08U
#define APFSRW_ATTR_MTIME   0x10U
#define APFSRW_ATTR_FLAGS   0x20U
#define APFSRW_ATTR_CRTIME  0x40U
struct apfsrw_attr {
    uint32_t mask;
    uint16_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t bsd_flags;
    uint64_t atime_ns;
    uint64_t mtime_ns;
    uint64_t crtime_ns;
};
int apfsrw_setattr(struct apfsrw *fs, const char *path,
    const struct apfsrw_attr *attr);
/* Current time in the unit inode timestamps use: ns since the epoch. */
uint64_t apfsrw_now_ns(void);

int apfsrw_symlink(struct apfsrw *fs, const char *path, const char *target,
    uint32_t uid, uint32_t gid);
/*
 * Savepoints: a byte copy of the current checkpoint and allocator state in
 * `file`, after which no block in use at that moment is reused. Rollback
 * writes the copy back, undoing every commit since - including a batch that
 * died halfway - from a fresh process if need be. Release once the work is
 * known good (the file can then be deleted).
 */
int apfsrw_savepoint(struct apfsrw *fs, const char *file);
void apfsrw_savepoint_release(struct apfsrw *fs);
int apfsrw_rollback(struct apfsrw *fs, const char *file);
/* Set flags mkapfs leaves off the root and private-dir inodes; run once on a
 * freshly made volume, before populating it. */
int apfsrw_fixup_mkapfs(struct apfsrw *fs);
/* Hard link: newpath becomes another name for existing (not a directory). */
int apfsrw_link(struct apfsrw *fs, const char *existing, const char *newpath);

#ifdef __cplusplus
}
#endif

#endif /* APFSRW_APFSRW_H */
