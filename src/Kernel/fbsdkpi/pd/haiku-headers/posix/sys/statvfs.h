/*
 * Copyright 2005-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _STAT_VFS_H_
#define _STAT_VFS_H_


#include <sys/types.h>


struct statvfs {
	unsigned long	f_bsize;	/* block size */
	unsigned long	f_frsize;	/* fundamental block size */
	fsblkcnt_t		f_blocks;	/* number of blocks on file system in units of f_frsize */
	fsblkcnt_t		f_bfree;	/* number of free blocks */
	fsblkcnt_t		f_bavail;	/* number of free blocks available to processes */
	fsfilcnt_t		f_files;	/* number of file serial numbers */
	fsfilcnt_t		f_ffree;	/* number of free file serial numbers */
	fsfilcnt_t		f_favail;	/* number of file serial numbers available to processes */
	unsigned long	f_fsid;		/* file system ID */
	unsigned long	f_flag;		/* see below */
	unsigned long	f_namemax;	/* maximum file name length */
};

#define ST_RDONLY	1
#define ST_NOSUID	2


#ifdef __cplusplus
extern "C" {
#endif

int statvfs(const char *path, struct statvfs *buffer);
int fstatvfs(int descriptor, struct statvfs *buffer);

#ifdef __cplusplus
}
#endif

#endif /* _STAT_VFS_H_ */
