/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

/*
 * Kernel backing for the platform shim in apfsrw_port.h: an allocator that can
 * realloc, and block I/O against the mounted device vnode. Everything here is
 * kernel-only; the userspace library never compiles this file.
 */
#include "apfsrw/apfsrw_port.h"

#ifdef APFSRW_KERNEL

#include "apfsrw/apfsrw.h"

/*
 * XNU's _FREE() takes a type rather than a size and there is no realloc, so
 * carry the size in a header. The header is a full 16 bytes to keep the
 * returned pointer 16-byte aligned, which is what malloc() callers assume.
 */
uint64_t
apfsrw_now_ns(void)
{
	struct timeval tv;

	microtime(&tv);
	return (uint64_t)tv.tv_sec * 1000000000ull +
	    (uint64_t)tv.tv_usec * 1000ull;
}

#define APFSRW_ALLOC_HDR 16

void *
apfsrw_kern_malloc(size_t size)
{
	uint8_t *base;

	if (size == 0 || size > (SIZE_MAX - APFSRW_ALLOC_HDR))
		return NULL;
	base = (uint8_t *)_MALLOC(size + APFSRW_ALLOC_HDR, M_TEMP, M_WAITOK);
	if (base == NULL)
		return NULL;
	memcpy(base, &size, sizeof(size));
	return base + APFSRW_ALLOC_HDR;
}

void *
apfsrw_kern_calloc(size_t count, size_t size)
{
	size_t total;
	void *p;

	if (count != 0 && size > (SIZE_MAX / count))
		return NULL;
	total = count * size;
	p = apfsrw_kern_malloc(total);
	if (p != NULL)
		memset(p, 0, total);
	return p;
}

void
apfsrw_kern_free(void *ptr)
{
	if (ptr == NULL)
		return;
	_FREE((uint8_t *)ptr - APFSRW_ALLOC_HDR, M_TEMP);
}

void *
apfsrw_kern_realloc(void *ptr, size_t size)
{
	size_t old;
	void *np;

	if (ptr == NULL)
		return apfsrw_kern_malloc(size);
	if (size == 0) {
		apfsrw_kern_free(ptr);
		return NULL;
	}
	memcpy(&old, (uint8_t *)ptr - APFSRW_ALLOC_HDR, sizeof(old));
	np = apfsrw_kern_malloc(size);
	if (np == NULL)
		return NULL;
	memcpy(np, ptr, old < size ? old : size);
	apfsrw_kern_free(ptr);
	return np;
}

/*
 * Block I/O goes through the same buffer cache as the kext's read path
 * (buf_meta_bread); two caches over one device read stale or torn metadata.
 * The kernel build only ever does whole-block, block-aligned I/O.
 */
static int
apfsrw_kern_io(struct apfsrw *fs, void *buf, size_t n, off_t off, int is_write)
{
	vnode_t devvp = (vnode_t)apfsrw_io_context(fs);
	buf_t bp = NULL;
	daddr64_t blkno;
	int error;

	if (devvp == NULLVP || buf == NULL || n == 0)
		return -1;
	if (off < 0 || ((uint64_t)off % n) != 0)
		return -1;
	blkno = (daddr64_t)((uint64_t)off / n);

	if (is_write) {
		bp = buf_getblk(devvp, blkno, (int)n, 0, 0, BLK_META);
		if (bp == NULL)
			return -1;
		memcpy((void *)buf_dataptr(bp), buf, n);
		/* Delayed write; apfsrw_sync() flushes at the commit barriers. */
		buf_bdwrite(bp);
		return 0;
	}

	error = (int)buf_meta_bread(devvp, blkno, (int)n, NOCRED, &bp);
	if (error != 0 || bp == NULL) {
		if (bp != NULL)
			buf_brelse(bp);
		return -1;
	}
	memcpy(buf, (const void *)buf_dataptr(bp), n);
	buf_brelse(bp);
	return 0;
}

ssize_t
apfsrw_pread(struct apfsrw *fs, void *buf, size_t n, off_t off)
{
	return apfsrw_kern_io(fs, buf, n, off, 0) == 0 ? (ssize_t)n : -1;
}

ssize_t
apfsrw_pwrite(struct apfsrw *fs, const void *buf, size_t n, off_t off)
{
	return apfsrw_kern_io(fs, __DECONST(void *, buf), n, off, 1) < 0
	    ? -1 : (ssize_t)n;
}

int
apfsrw_sync(struct apfsrw *fs)
{
	vnode_t devvp = (vnode_t)apfsrw_io_context(fs);

	if (devvp == NULLVP)
		return -1;
	buf_flushdirtyblks(devvp, 1, 0, "apfsrw");
	return 0;
}

#endif /* APFSRW_KERNEL */
