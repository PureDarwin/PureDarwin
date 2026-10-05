/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

// Host substitutes for the kernel APIs used by apfsrw_kern.c. The library and
// its kernel I/O port are compiled unchanged, without an Apple SDK.
#ifndef TEST_ENV_H
#define TEST_ENV_H
#define _POSIX_C_SOURCE 200809L
#define APFSRW_PORT_H
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "apfsrw/apfsrw.h"
typedef int64_t daddr64_t;
typedef char *caddr_t;
typedef void *vnode_t;
typedef struct test_buf *buf_t;
typedef struct { int unused; } dk_synchronize_t;
#define NULLVP NULL
#define NOCRED NULL
#define BLK_META 0
#define BUF_WAIT 1
#define DKIOCSYNCHRONIZE 123
#define FWRITE 2
#define M_TEMP 0
#define M_WAITOK 0
#define _MALLOC(n,t,f) malloc(n)
#define _FREE(p,t) free(p)
#define bzero(p,n) memset(p,0,n)
#define __DECONST(t,p) ((t)(uintptr_t)(p))
#define apfsrw_getenv(n) ((char *)0)
uint64_t mach_absolute_time(void);
void absolutetime_to_nanoseconds(uint64_t, uint64_t *);
void microtime(struct timeval *);
buf_t buf_getblk(vnode_t, daddr64_t, int, int, int, int);
void *buf_dataptr(buf_t);
void buf_bdwrite(buf_t);
int buf_meta_bread(vnode_t, daddr64_t, int, void *, buf_t *);
void buf_brelse(buf_t);
int buf_invalblkno(vnode_t, daddr64_t, int);
void buf_flushdirtyblks(vnode_t, int, int, const char *);
int VNOP_IOCTL(vnode_t, int, char *, int, void *);
void *vfs_context_kernel(void);
int apfsrw_sync(struct apfsrw *);
int apfsrw_sync_nowait(struct apfsrw *);
void apfsrw_discard_superseded(struct apfsrw *, const uint64_t *, uint32_t,
    const uint64_t *, uint32_t);
uint64_t apfsrw_now_ns(void);
#endif
