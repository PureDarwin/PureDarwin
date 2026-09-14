/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

/*
 * Platform shim so the same apfsrw.c builds both as a userspace library and
 * inside apfs.kext: one implementation of the B-tree write path, validated
 * against fsck_apfs. Everything below is behind APFSRW_KERNEL.
 */
#ifndef APFSRW_PORT_H
#define APFSRW_PORT_H

struct apfsrw;

#ifndef APFSRW_KERNEL

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define apfsrw_getenv(n)	getenv(n)
#define apfsrw_sync(fs)		fsync((fs)->fd)
uint64_t apfsrw_now_ns(void);

#else /* APFSRW_KERNEL */

/* Kernel headers must precede the malloc/free/fprintf macros below, or the
 * macros rewrite identifiers inside XNU's own headers. */
#include <sys/mount.h>
#include <sys/vnode.h>
#include <stdint.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/ubc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <string.h>

/* XNU has no realloc and _FREE() wants a type, so the allocator carries the
 * size in a small header; apfsrw.c calls malloc/calloc/realloc/free unchanged. */
void *apfsrw_kern_malloc(size_t size);
void *apfsrw_kern_calloc(size_t count, size_t size);
void *apfsrw_kern_realloc(void *ptr, size_t size);
void apfsrw_kern_free(void *ptr);

#define malloc(n)		apfsrw_kern_malloc(n)
#define calloc(c, n)		apfsrw_kern_calloc((c), (n))
#define realloc(p, n)		apfsrw_kern_realloc((p), (n))
#define free(p)			apfsrw_kern_free(p)

int snprintf(char *, size_t, const char *, ...) __printflike(3, 4);
void microtime(struct timeval *tv);

/* Ordering barriers in the commit protocol: the checkpoint must not land
 * before the metadata it describes. */
int apfsrw_sync(struct apfsrw *fs);
uint64_t apfsrw_now_ns(void);

/* libkern exposes no strrchr. */
static inline char *apfsrw_strrchr(const char *s, int c)
{
	const char *last = (const char *)0;

	for (;; s++) {
		if (*s == (char)c)
			last = s;
		if (*s == '\0')
			break;
	}
	return (char *)(uintptr_t)last;
}
#define strrchr(s, c)		apfsrw_strrchr((s), (c))

/* libkern exposes no memchr; libzbitmap needs one. */
static inline void *apfsrw_memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = (const unsigned char *)s;

	while (n-- != 0) {
		if (*p == (unsigned char)c)
			return (void *)(uintptr_t)p;
		p++;
	}
	return (void *)0;
}
#define memchr(s, c, n)		apfsrw_memchr((s), (c), (n))

/* Debug tracing is userspace-only; the kernel build compiles it out. */
#define apfsrw_getenv(n)	((char *)0)
#define fprintf(stream, ...)	do { } while (0)
#define stderr			((void *)0)

#endif /* APFSRW_KERNEL */

#endif /* APFSRW_PORT_H */
