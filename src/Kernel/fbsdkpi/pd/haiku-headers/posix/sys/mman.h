/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H


#include <sys/cdefs.h>
#include <sys/types.h>


/* memory protection for mmap() and others; assignment compatible with
   B_{READ,WRITE,EXECUTE}_AREA */
#define PROT_READ		0x01
#define PROT_WRITE		0x02
#define PROT_EXEC		0x04
#define PROT_NONE		0x00

/* mmap() flags */
#define MAP_FILE		0x00			/* map a file to memory (this is the default behavior) */
#define MAP_SHARED		0x01			/* changes are seen by others */
#define MAP_PRIVATE		0x02			/* changes are only seen by caller */
#define MAP_FIXED		0x04			/* require mapping to specified addr */
#define MAP_ANONYMOUS	0x08			/* no underlying object */
#define MAP_ANON		MAP_ANONYMOUS
#define MAP_NORESERVE	0x10			/* don't commit memory */

/* mmap() error return code */
#define MAP_FAILED		((void*)-1)

/* msync() flags */
#define MS_ASYNC		0x01
#define MS_SYNC			0x02
#define MS_INVALIDATE	0x04

/* madvise() values */
#define MADV_NORMAL			1
#define MADV_SEQUENTIAL		2
#define MADV_RANDOM			3
#define MADV_WILLNEED		4
#define MADV_DONTNEED		5
#define MADV_FREE			6

/* posix_madvise() values */
#define POSIX_MADV_NORMAL		MADV_NORMAL
#define POSIX_MADV_SEQUENTIAL	MADV_SEQUENTIAL
#define POSIX_MADV_RANDOM		MADV_RANDOM
#define POSIX_MADV_WILLNEED		MADV_WILLNEED
#define POSIX_MADV_DONTNEED		MADV_DONTNEED


__BEGIN_DECLS

void*	mmap(void* address, size_t length, int protection, int flags,
			int fd, off_t offset);
int		munmap(void* address, size_t length);

int		mprotect(void* address, size_t length, int protection);
int		msync(void* address, size_t length, int flags);

int		madvise(void* address, size_t length, int advice);
int		posix_madvise(void* address, size_t length, int advice);

int		mlock(const void* address, size_t length);
int		munlock(const void* address, size_t length);

int		shm_open(const char* name, int openMode, mode_t permissions);
int		shm_unlink(const char* name);

__END_DECLS


#endif	/* _SYS_MMAN_H */
