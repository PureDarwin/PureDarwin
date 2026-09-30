/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

// hand freed container blocks back to the host filesystem, so a sparse image file
// stays as small as the container's live data. Linux only, a no-op elsewhere
#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/types.h>

int apfsrw_punch(int fd, off_t off, off_t len);

int apfsrw_punch(int fd, off_t off, off_t len)
{
#ifdef FALLOC_FL_PUNCH_HOLE
    return fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, off, len);
#else
    (void)fd;
    (void)off;
    (void)len;
    return -1;
#endif
}
