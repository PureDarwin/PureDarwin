/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfsrw/apfsrw_port.h"

#ifdef APFSRW_KERNEL

#include "apfsrw/apfsrw.h"

uint64_t
apfsrw_now_ns(void)
{
	struct timeval tv;

	microtime(&tv);
	return (uint64_t)tv.tv_sec * 1000000000ull +
	    (uint64_t)tv.tv_usec * 1000ull;
}

#define APFSRW_ALLOC_HDR 16

// Commit cost breakdown, reported and reset by the kext's lock-hold report
uint64_t apfsrw_kern_sync_ns, apfsrw_kern_sync_n, apfsrw_kern_bdwrites, apfsrw_kern_breads;
// Blocks written per phase, to show what a commit actually costs:
// 0 the mutation itself, 1 cow_commit, 2 publish_checkpoint, 3 deferred frees
extern int apfsrw_wphase;
uint64_t apfsrw_wblocks[4];
extern uint64_t apfsrw_commits;

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

static int
apfsrw_kern_io(struct apfsrw *fs, void *buf, size_t n, off_t off, int is_write)
{
	struct apfsrw_kern_dev *dev = apfsrw_io_context(fs);
	vnode_t devvp;
	uint32_t bs;
	size_t done;

	if (dev == NULL || buf == NULL || n == 0)
		return -1;
	devvp = (vnode_t)dev->devvp;
	bs = dev->block_size;
	if (devvp == NULLVP || dev->dev_bsize == 0 || bs == 0)
		return -1;
	if (off < 0 || ((uint64_t)off % bs) != 0 || (n % bs) != 0)
		return -1;

	// One container block per buffer, so cache entries stay
	// block-sized however long the extent being transferred is
	for (done = 0; done < n; done += bs) {
		uint64_t paddr = ((uint64_t)off + done) / bs;
		daddr64_t blkno = (daddr64_t)(paddr * (bs / dev->dev_bsize));
		uint8_t *p = (uint8_t *)buf + done;
		buf_t bp = NULL;
		int error;

		if (is_write) {
			bp = buf_getblk(devvp, blkno, (int)bs, 0, 0, BLK_META);
			if (bp == NULL)
				return -1;
			memcpy((void *)buf_dataptr(bp), p, bs);
			// Delayed write. apfsrw_sync() flushes at commit barriers
			buf_bdwrite(bp);
			apfsrw_kern_bdwrites++;
			apfsrw_wblocks[apfsrw_wphase & 3]++;
			continue;
		}
		apfsrw_kern_breads++;
		error = (int)buf_meta_bread(devvp, blkno, (int)bs, NOCRED, &bp);
		if (error != 0 || bp == NULL) {
			if (bp != NULL)
				buf_brelse(bp);
			return -1;
		}
		memcpy(p, (const void *)buf_dataptr(bp), bs);
		buf_brelse(bp);
	}
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
	struct apfsrw_kern_dev *dev = apfsrw_io_context(fs);

	if (dev == NULL || dev->devvp == NULL)
		return -1;
	uint64_t t0 = apfsrw_now_ns();

	buf_flushdirtyblks((vnode_t)dev->devvp, 1, 0, "apfsrw");
	apfsrw_kern_sync_ns += apfsrw_now_ns() - t0;
	apfsrw_kern_sync_n++;
	return 0;
}

int
apfsrw_sync_nowait(struct apfsrw *fs)
{
	struct apfsrw_kern_dev *dev = apfsrw_io_context(fs);

	if (dev == NULL || dev->devvp == NULL)
		return -1;
	buf_flushdirtyblks((vnode_t)dev->devvp, 0, 0, "apfsrw");
	return 0;
}

#endif /* APFSRW_KERNEL */
