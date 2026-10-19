/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_STAT_H_
#define _SYS_STAT_H_


#include <sys/types.h>
#include <time.h>


struct stat {
	dev_t			st_dev;			/* device ID that this file resides on */
	ino_t			st_ino;			/* this file's serial inode ID */
	mode_t			st_mode;		/* file mode (rwx for user, group, etc) */
	nlink_t			st_nlink;		/* number of hard links to this file */
	uid_t			st_uid;			/* user ID of the owner of this file */
	gid_t			st_gid;			/* group ID of the owner of this file */
	off_t			st_size;		/* size in bytes of this file */
	dev_t			st_rdev;		/* device type (not used) */
	blksize_t		st_blksize;		/* preferred block size for I/O */
	struct timespec	st_atim;		/* last access time */
	struct timespec	st_mtim;		/* last modification time */
	struct timespec	st_ctim;		/* last change time, not creation time */
	struct timespec	st_crtim;		/* creation time */
	__haiku_uint32	st_type;		/* attribute/index type */
	blkcnt_t		st_blocks;		/* number of blocks allocated for object */
};

/* source compatibility with old stat structure */
#define st_atime	st_atim.tv_sec
#define st_mtime	st_mtim.tv_sec
#define st_ctime	st_ctim.tv_sec
#define st_crtime	st_crtim.tv_sec


/* extended file types */
#define S_ATTR_DIR			01000000000	/* attribute directory */
#define S_ATTR				02000000000	/* attribute */
#define S_INDEX_DIR			04000000000	/* index (or index directory) */
#define S_STR_INDEX			00100000000	/* string index */
#define S_INT_INDEX			00200000000	/* int32 index */
#define S_UINT_INDEX		00400000000	/* uint32 index */
#define S_LONG_LONG_INDEX	00010000000	/* int64 index */
#define S_ULONG_LONG_INDEX	00020000000	/* uint64 index */
#define S_FLOAT_INDEX		00040000000	/* float index */
#define S_DOUBLE_INDEX		00001000000	/* double index */
#define S_ALLOW_DUPS		00002000000	/* allow duplicate entries (currently unused) */

/* link types */
#define	S_LINK_SELF_HEALING	00001000000	/* link will be updated if you move its target */
#define S_LINK_AUTO_DELETE	00002000000	/* link will be deleted if you delete its target */

/* standard file types */
#define S_IFMT				00000170000 /* type of file */
#define	S_IFSOCK			00000140000 /* socket */
#define	S_IFLNK				00000120000 /* symbolic link */
#define S_IFREG 			00000100000 /* regular */
#define S_IFBLK 			00000060000 /* block special */
#define S_IFDIR 			00000040000 /* directory */
#define S_IFCHR 			00000020000 /* character special */
#define S_IFIFO 			00000010000 /* fifo */

#define S_ISREG(mode)		(((mode) & S_IFMT) == S_IFREG)
#define S_ISLNK(mode)		(((mode) & S_IFMT) == S_IFLNK)
#define S_ISBLK(mode)		(((mode) & S_IFMT) == S_IFBLK)
#define S_ISDIR(mode)		(((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode)		(((mode) & S_IFMT) == S_IFCHR)
#define S_ISFIFO(mode)		(((mode) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(mode)		(((mode) & S_IFMT) == S_IFSOCK)
#define S_ISINDEX(mode)		(((mode) & S_INDEX_DIR) == S_INDEX_DIR)

#define	S_IUMSK 07777		/* user settable bits */

#define S_ISUID 04000       /* set user id on execution */
#define S_ISGID 02000       /* set group id on execution */

#define S_ISVTX 01000       /* save swapped text even after use (sticky bit) */

#define S_IRWXU 00700       /* read, write, execute: owner */
#define S_IRUSR 00400       /* read permission: owner */
#define S_IWUSR 00200       /* write permission: owner */
#define S_IXUSR 00100       /* execute permission: owner */
#define S_IRWXG 00070       /* read, write, execute: group */
#define S_IRGRP 00040       /* read permission: group */
#define S_IWGRP 00020       /* write permission: group */
#define S_IXGRP 00010       /* execute permission: group */
#define S_IRWXO 00007       /* read, write, execute: other */
#define S_IROTH 00004       /* read permission: other */
#define S_IWOTH 00002       /* write permission: other */
#define S_IXOTH 00001       /* execute permission: other */

/* BSD extensions */
#define S_IREAD		S_IRUSR
#define S_IWRITE	S_IWUSR
#define S_IEXEC		S_IXUSR

#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)
#define ALLPERMS    (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)
#define	DEFFILEMODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
	/* default file mode, everyone can read/write */

/* special values for timespec::tv_nsec passed to utimensat(), futimens() */
#define UTIME_NOW	1000000000
#define UTIME_OMIT	1000000001

#ifdef __cplusplus
extern "C" {
#endif

extern int		chmod(const char *path, mode_t mode);
extern int		fchmod(int fd, mode_t mode);
extern int		fchmodat(int fd, const char *path, mode_t mode, int flag);
extern int		stat(const char *path, struct stat *st);
extern int		fstat(int fd, struct stat *st);
extern int		lstat(const char *path, struct stat *st);
extern int		fstatat(int fd, const char *path, struct stat *st, int flag);
extern int		mkdir(const char *path, mode_t mode);
extern int		mkdirat(int fd, const char *path, mode_t mode);
extern int		mkfifo(const char *path, mode_t mode);
extern int		mkfifoat(int fd, const char *path, mode_t mode);
extern int		mknod(const char *name, mode_t mode, dev_t dev);
extern int		mknodat(int fd, const char *name, mode_t mode, dev_t dev);
extern mode_t	umask(mode_t cmask);

extern int		utimensat(int fd, const char *path,
					const struct timespec times[2], int flag);
extern int		futimens(int fd, const struct timespec times[2]);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_STAT_H_ */
