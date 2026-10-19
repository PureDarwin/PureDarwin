/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FCNTL_H
#define _FCNTL_H


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


/* commands that can be passed to fcntl() */
#define	F_DUPFD			0x0001		/* duplicate fd */
#define	F_GETFD			0x0002		/* get fd flags */
#define	F_SETFD			0x0004		/* set fd flags */
#define	F_GETFL			0x0008		/* get file status flags and access mode */
#define	F_SETFL			0x0010		/* set file status flags */
#define F_GETLK         0x0020		/* get locking information */
#define F_SETLK         0x0080		/* set locking information */
#define F_SETLKW        0x0100		/* as above, but waits if blocked */
#define F_DUPFD_CLOEXEC 0x0200		/* duplicate fd with close on exec set */
#define F_DUPFD_CLOFORK 0x0400		/* duplicate fd with close on fork set */

/* advisory locking types */
#define F_RDLCK         0x0040		/* read or shared lock */
#define F_UNLCK         0x0200		/* unlock */
#define F_WRLCK         0x0400		/* write or exclusive lock */

/* file descriptor flags for fcntl() */
#define FD_CLOEXEC		1			/* close on exec */
#define FD_CLOFORK		2			/* close on fork */

/* file access modes for open() */
#define O_RDONLY		0x0000		/* read only */
#define O_WRONLY		0x0001		/* write only */
#define O_RDWR			0x0002		/* read and write */
#define O_ACCMODE   	0x0003		/* mask to get the access modes above */
#define O_RWMASK		O_ACCMODE

/* flags for open() */
#define	O_EXCL			0x0100		/* exclusive creat */
#define O_CREAT			0x0200		/* create and open file */
#define O_TRUNC			0x0400		/* open with truncation */
#define O_NOCTTY		0x1000		/* don't make tty the controlling tty */
#define	O_NOTRAVERSE	0x2000		/* do not traverse leaf link */

/* flags for open() and fcntl() */
#define O_CLOEXEC		0x00000040	/* close on exec */
#define	O_NONBLOCK		0x00000080	/* non blocking io */
#define	O_NDELAY		O_NONBLOCK
#define O_APPEND		0x00000800	/* to end of file */
#define O_SYNC			0x00010000	/* write synchronized I/O file integrity */
#define O_RSYNC			0x00020000	/* read synchronized I/O file integrity */
#define O_DSYNC			0x00040000	/* write synchronized I/O data integrity */
#define O_NOFOLLOW		0x00080000	/* fail on symlinks */
#define O_DIRECT		0x00100000	/* do not use the file system cache if */
									/* possible */
#define O_NOCACHE		O_DIRECT
#define O_DIRECTORY		0x00200000	/* fail if not a directory */
#define O_CLOFORK		0x00400000	/* close on fork */

/* flags for the *at() functions */
#define AT_FDCWD		(-100)		/* CWD FD for the *at() functions */

#define AT_SYMLINK_NOFOLLOW	0x01	/* fstatat(), fchmodat(), fchownat(),
										utimensat() */
#define AT_SYMLINK_FOLLOW	0x02	/* linkat() */
#define AT_REMOVEDIR		0x04	/* unlinkat() */
#define AT_EACCESS			0x08	/* faccessat() */

/* file advisory information, unused by Haiku */
#define POSIX_FADV_NORMAL		0	/* no advice */
#define POSIX_FADV_SEQUENTIAL	1	/* expect sequential access */
#define POSIX_FADV_RANDOM		2	/* expect access in a random order */
#define POSIX_FADV_WILLNEED		3	/* expect access in the near future */
#define POSIX_FADV_DONTNEED		4	/* expect no access in the near future */
#define POSIX_FADV_NOREUSE		5	/* expect access only once */

/* advisory file locking */

struct flock {
	short	l_type;
	short	l_whence;
	off_t	l_start;
	off_t	l_len;
	pid_t	l_pid;
};


#ifdef __cplusplus
extern "C" {
#endif

extern int	creat(const char *path, mode_t mode);
extern int	open(const char *path, int openMode, ...);
	/* the third argument is the permissions of the created file when O_CREAT
	   is passed in oflags */
extern int	openat(int fd, const char *path, int openMode, ...);

extern int	fcntl(int fd, int op, ...);

extern int	posix_fadvise(int fd, off_t offset, off_t len, int advice);
extern int	posix_fallocate(int fd, off_t offset, off_t len);

#ifdef __cplusplus
}
#endif

#endif	/* _FCNTL_H */
