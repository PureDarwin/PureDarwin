/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfsrw/apfsrw_port.h"

#ifdef APFSRW_KERNEL

#include "apfsrw/apfsrw.h"
#include <kern/clock.h>
#include <sys/disk.h>
#include <sys/fcntl.h>

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

// block reads that missed the buffer cache path, delayed writes, and sync barriers
static uint64_t apfsrw_st_reads, apfsrw_st_read_abs, apfsrw_st_writes, apfsrw_st_write_abs;
static uint64_t apfsrw_st_syncs, apfsrw_st_sync_abs;
static uint64_t apfsrw_st_superseded;

void
apfsrw_kern_iostat(uint64_t st[7])
{
	uint64_t ns;

	st[0] = apfsrw_st_reads;
	absolutetime_to_nanoseconds(apfsrw_st_read_abs, &ns);
	st[1] = ns / 1000000ull;
	st[2] = apfsrw_st_writes;
	st[3] = apfsrw_st_syncs;
	absolutetime_to_nanoseconds(apfsrw_st_sync_abs, &ns);
	st[4] = ns / 1000000ull;
	absolutetime_to_nanoseconds(apfsrw_st_write_abs, &ns);
	st[5] = ns / 1000000ull;
	st[6] = apfsrw_st_superseded;
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
	if (dev->io != NULL)
		return dev->io(dev->io_ref, buf, n, (uint64_t)off, is_write);
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
			uint64_t w0 = mach_absolute_time();

			bp = buf_getblk(devvp, blkno, (int)bs, 0, 0, BLK_META);
			if (bp == NULL)
				return -1;
			memcpy((void *)buf_dataptr(bp), p, bs);
			// Delayed write. apfsrw_sync() flushes at commit barriers
			buf_bdwrite(bp);
			apfsrw_st_write_abs += mach_absolute_time() - w0;
			apfsrw_st_writes++;
			continue;
		}
		uint64_t t0 = mach_absolute_time();
		error = (int)buf_meta_bread(devvp, blkno, (int)bs, NOCRED, &bp);
		apfsrw_st_read_abs += mach_absolute_time() - t0;
		apfsrw_st_reads++;
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

void
apfsrw_discard_superseded(struct apfsrw *fs, const uint64_t *alloced,
    uint32_t nalloced, const uint64_t *deferred, uint32_t ndeferred)
{
	struct apfsrw_kern_dev *dev = apfsrw_io_context(fs);
	uint64_t *owned;
	uint32_t cap = 1, i;

	// Direct IOMedia writes have already been submitted. This only cancels our
	// buffer-cache writes, never the device barrier or frees of committed blocks.
	if (dev == NULL || dev->io != NULL || dev->devvp == NULL ||
	    dev->dev_bsize == 0 || dev->block_size < dev->dev_bsize ||
	    dev->block_size % dev->dev_bsize != 0 || nalloced == 0 || ndeferred == 0)
		return;
	// Optional optimization: overflow or allocation failure leaves normal flushing intact.
	if (nalloced > (1U << 30))
		return;
	while (cap < nalloced * 2U)
		cap <<= 1;
	owned = calloc(cap, sizeof(*owned));
	if (owned == NULL)
		return;
	for (i = 0; i < nalloced; i++) {
		uint32_t slot = (uint32_t)(alloced[i] * 0x9e3779b97f4a7c15ULL) & (cap - 1);

		// Block zero is not transaction-allocated; it is the container superblock.
		if (alloced[i] == 0)
			continue;
		while (owned[slot] != 0 && owned[slot] != alloced[i])
			slot = (slot + 1) & (cap - 1);
		owned[slot] = alloced[i];
	}
	for (i = 0; i < ndeferred; i++) {
		uint32_t slot = (uint32_t)(deferred[i] * 0x9e3779b97f4a7c15ULL) & (cap - 1);

		if (deferred[i] == 0)
			continue;
		while (owned[slot] != 0 && owned[slot] != deferred[i])
			slot = (slot + 1) & (cap - 1);
		if (owned[slot] != 0) {
			daddr64_t blkno = (daddr64_t)(deferred[i] * (dev->block_size / dev->dev_bsize));

			if (buf_invalblkno((vnode_t)dev->devvp, blkno, BUF_WAIT) == 0)
				apfsrw_st_superseded++;
		}
	}
	free(owned);
}

int
apfsrw_sync(struct apfsrw *fs)
{
	struct apfsrw_kern_dev *dev = apfsrw_io_context(fs);

	if (dev != NULL && dev->io != NULL)
		return dev->sync != NULL ? dev->sync(dev->io_ref) : 0;
	if (dev == NULL || dev->devvp == NULL)
		return -1;
	uint64_t t0 = mach_absolute_time();
	dk_synchronize_t ds;

	if (dev->sync_wait != NULL)
		dev->sync_wait(dev->wait_ref, 0);
	buf_flushdirtyblks((vnode_t)dev->devvp, 1, 0, "apfsrw");
	// the blocks have left the buffer cache, the device may still hold them in a write cache:
	// a barrier that stops here lets a checkpoint reach the media before what it points to
	bzero(&ds, sizeof(ds));
	(void)VNOP_IOCTL((vnode_t)dev->devvp, DKIOCSYNCHRONIZE, (caddr_t)&ds, FWRITE, vfs_context_kernel());
	apfsrw_st_sync_abs += mach_absolute_time() - t0;
	if (dev->sync_wait != NULL)
		dev->sync_wait(dev->wait_ref, 1);
	apfsrw_st_syncs++;
	return 0;
}

int
apfsrw_sync_nowait(struct apfsrw *fs)
{
	struct apfsrw_kern_dev *dev = apfsrw_io_context(fs);

	if (dev != NULL && dev->io != NULL)
		return dev->sync != NULL ? dev->sync(dev->io_ref) : 0;
	if (dev == NULL || dev->devvp == NULL)
		return -1;
	buf_flushdirtyblks((vnode_t)dev->devvp, 0, 0, "apfsrw");
	return 0;
}

#endif /* APFSRW_KERNEL */
