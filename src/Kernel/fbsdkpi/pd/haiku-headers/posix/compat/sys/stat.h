/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _COMPAT_SYS_STAT_H_
#define _COMPAT_SYS_STAT_H_


#include <sys/stat.h>


/* helper struct allowing us to avoid problems with the st_*time macros */
typedef struct {
	time_t	tv_sec;
} stat_beos_time;


struct stat_beos {
	dev_t			st_dev;			/* device ID that this file resides on */
	ino_t			st_ino;			/* this file's serial inode ID */
	mode_t			st_mode;		/* file mode (rwx for user, group, etc) */
	nlink_t			st_nlink;		/* number of hard links to this file */
	uid_t			st_uid;			/* user ID of the owner of this file */
	gid_t			st_gid;			/* group ID of the owner of this file */
	off_t			st_size;		/* size in bytes of this file */
	dev_t			st_rdev;		/* device type (not used) */
	blksize_t		st_blksize;		/* preferred block size for I/O */
	stat_beos_time	st_atim;		/* last access time */
	stat_beos_time	st_mtim;		/* last modification time */
	stat_beos_time	st_ctim;		/* last change time, not creation time */
	stat_beos_time	st_crtim;		/* creation time */
};


#ifdef __cplusplus
extern "C" {
#endif

extern void		convert_to_stat_beos(const struct stat* stat,
					struct stat_beos* beosStat);
extern void		convert_from_stat_beos(const struct stat_beos* beosStat,
					struct stat* stat);

#ifdef __cplusplus
}
#endif

#endif	/* _COMPAT_SYS_STAT_H_ */
