/*
 * Copyright 2004-2024, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _UNISTD_H_
#define _UNISTD_H_


#include <stdint.h>
#include <sys/types.h>

#include <config/HaikuConfig.h>


/* access modes */
#define R_OK	4
#define W_OK	2
#define X_OK	1
#define F_OK	0

/* standard file descriptors */
#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

/* function arguments needed by lockf() */
#define F_ULOCK		0	/* unlock locked sections */
#define F_LOCK		1	/* lock a section for exclusive use */
#define F_TLOCK		2	/* test and lock a section for exclusive use */
#define F_TEST		3	/* test a section for locks by other processes */

/* POSIX version support */
#define _POSIX_VERSION			(200809L)
#define	_POSIX2_VERSION			(200809L)
#define _XOPEN_VERSION			(700)


#define _POSIX_VDISABLE			((unsigned char)-2)

/*
 * POSIX options and options groups.
 * Please keep this list in the same order as the applicable standard.
 * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap02.html#tag_02_01_06
 */
#define _POSIX_ADVISORY_INFO				(200809L)
#define _POSIX_ASYNCHRONOUS_IO				(-1)
#define _POSIX_BARRIERS						(200809L)
#define _POSIX_CHOWN_RESTRICTED				(1)
#define _POSIX_CLOCK_SELECTION				(200809L)
#define _POSIX_CPUTIME						(200809L)
#define _POSIX_DEVICE_CONTROL				(202405L)
#define _POSIX_FSYNC						(200809L)
#define _POSIX_IPV6							(200809L)
#define _POSIX_JOB_CONTROL					(1)
#define _POSIX_MAPPED_FILES					(200809L)
#define _POSIX_MEMLOCK						(-1)
#define _POSIX_MEMLOCK_RANGE				(200809L)
#define _POSIX_MEMORY_PROTECTION			(200809L)
#define _POSIX_MESSAGE_PASSING				(-1)
#define _POSIX_MONOTONIC_CLOCK				(200809L) /* aio_suspend() is missing in Haiku1 */
#define _POSIX_NO_TRUNC						(0)
#define _POSIX_PRIORITIZED_IO				(-1)
#define _POSIX_PRIORITY_SCHEDULING			(-1)
#define _POSIX_RAW_SOCKETS					(-1)
#define _POSIX_READER_WRITER_LOCKS			(200809L)
#define _POSIX_REALTIME_SIGNALS				(200809L)
#define _POSIX_REGEXP						(200809L)
#define _POSIX_SAVED_IDS					(1)
#define _POSIX_SEMAPHORES					(200112L)
#define _POSIX_SHARED_MEMORY_OBJECTS		(200809L)
#define _POSIX_SHELL						(200809L)
#define _POSIX_SPAWN						(200809L)
#define _POSIX_SPIN_LOCKS					(-1)
#define _POSIX_SPORADIC_SERVER				(-1)
#define _POSIX_SYNCHRONIZED_IO				(200809L)
#define _POSIX_THREAD_ATTR_STACKADDR		(200809L)
#define _POSIX_THREAD_ATTR_STACKSIZE		(200809L)
#define _POSIX_THREAD_CPUTIME				(200809L)
#define _POSIX_THREAD_PRIO_INHERIT			(-1) /* not inplemented */
#define _POSIX_THREAD_PRIO_PROTECT          (-1)
#define _POSIX_THREAD_PRIORITY_SCHEDULING	(-1)
#define _POSIX_THREAD_PROCESS_SHARED		(200809L)
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT	(-1)
#define _POSIX_THREAD_ROBUST_PRIO_PROTECT	(-1)
#define _POSIX_THREAD_SAFE_FUNCTIONS		(200809L)
#define _POSIX_THREAD_SPORADIC_SERVER		(-1)
#define _POSIX_THREADS						(200809L)
#define _POSIX_TIMEOUTS						(-1)
#define _POSIX_TIMERS						(200809L)
#define _POSIX_TRACE						(-1)
#define _POSIX_TRACE_EVENT_FILTER			(-1)
#define _POSIX_TRACE_INHERIT				(-1)
#define _POSIX_TRACE_LOG					(-1)
#define _POSIX_TYPED_MEMORY_OBJECTS			(-1)

#define	__ILP32_OFF32						(-1)
#ifdef __HAIKU_ARCH_64_BIT
#define	__ILP32_OFFBIG						(-1)
#define	__LP64_OFF64						(0)
#else /* __HAIKU_ARCH_32_BIT */
#define	__ILP32_OFFBIG						(0)
#define	__LP64_OFF64						(-1)
#endif
#define	__LPBIG_OFFBIG						(-1)


#define	_POSIX_V6_ILP32_OFF32		__ILP32_OFF32	/* OB */
#define	_POSIX_V6_ILP32_OFFBIG		__ILP32_OFFBIG	/* OB */
#define	_POSIX_V6_LP64_OFF64		__LP64_OFF64	/* OB */
#define	_POSIX_V6_LPBIG_OFFBIG		__LPBIG_OFFBIG	/* OB */

#define	_POSIX_V7_ILP32_OFF32		__ILP32_OFF32
#define	_POSIX_V7_ILP32_OFFBIG		__ILP32_OFFBIG
#define	_POSIX_V7_LP64_OFF64		__LP64_OFF64
#define	_POSIX_V7_LPBIG_OFFBIG		__LPBIG_OFFBIG


/* POSIX2 */
#define	_POSIX2_C_BIND						(200809L)
#define	_POSIX2_C_DEV						(-1)
#define	_POSIX2_CHAR_TERM					(1)
#define	_POSIX2_FORT_DEV					(-1)
#define	_POSIX2_FORT_RUN					(-1)
#define	_POSIX2_LOCALEDEF					(-1)
#define	_POSIX2_PBS							(-1)
#define	_POSIX2_PBS_ACCOUNTING				(-1)
#define	_POSIX2_PBS_CHECKPOINT				(-1)
#define	_POSIX2_PBS_LOCATE					(-1)
#define	_POSIX2_PBS_MESSAGE					(-1)
#define	_POSIX2_PBS_TRACK					(-1)
#define	_POSIX2_SW_DEV						(-1)
#define	_POSIX2_UPE							(-1)

/* XSI */
#define _XOPEN_CRYPT						(-1)
#define _XOPEN_ENH_I18N						(-1)
#define	_XOPEN_LEGACY						(-1)
#define	_XOPEN_REALTIME						(-1)
#define	_XOPEN_REALTIME_THREADS				(-1)
#define	_XOPEN_SHM							(-1)
#define	_XOPEN_STREAMS						(-1)
#define	_XOPEN_UNIX							(-1)
#define _XOPEN_UUCP							(-1)

/* pathconf() constants */
/* BeOS supported values, do not touch */
#define _PC_CHOWN_RESTRICTED	1
#define _PC_MAX_CANON			2
#define _PC_MAX_INPUT			3
#define _PC_NAME_MAX			4
#define _PC_NO_TRUNC			5
#define _PC_PATH_MAX			6
#define _PC_PIPE_BUF			7
#define _PC_VDISABLE			8
#define _PC_LINK_MAX			25
/* new values */
#define _PC_SYNC_IO				26
#define _PC_ASYNC_IO			27
#define _PC_PRIO_IO				28
#define _PC_SOCK_MAXBUF			29
#define _PC_FILESIZEBITS		30
#define _PC_REC_INCR_XFER_SIZE	31
#define _PC_REC_MAX_XFER_SIZE	32
#define _PC_REC_MIN_XFER_SIZE	33
#define _PC_REC_XFER_ALIGN		34
#define _PC_ALLOC_SIZE_MIN		35
#define _PC_SYMLINK_MAX			36
#define _PC_2_SYMLINKS			37
#define _PC_XATTR_EXISTS		38
#define _PC_XATTR_ENABLED		39

/* sysconf() constants */
/* BeOS supported values, do not touch */
#define _SC_ARG_MAX				15
#define _SC_CHILD_MAX			16
#define _SC_CLK_TCK				17
#define _SC_JOB_CONTROL			18
#define _SC_NGROUPS_MAX			19
#define _SC_OPEN_MAX			20
#define _SC_SAVED_IDS			21
#define _SC_STREAM_MAX			22
#define _SC_TZNAME_MAX			23
#define _SC_VERSION				24
/* new values */
#define _SC_GETGR_R_SIZE_MAX	25
#define _SC_GETPW_R_SIZE_MAX	26
#define _SC_PAGE_SIZE			27
#define _SC_PAGESIZE			_SC_PAGE_SIZE
#define _SC_SEM_NSEMS_MAX		28
#define _SC_SEM_VALUE_MAX		29
#define _SC_SEMAPHORES			30
#define _SC_THREADS				31
/* TODO: check */
#define _SC_IOV_MAX						32
#define _SC_UIO_MAXIOV					_SC_IOV_MAX
#define _SC_NPROCESSORS_CONF			34
#define _SC_NPROCESSORS_ONLN			35
#define _SC_ATEXIT_MAX					37
#define _SC_PASS_MAX					39
#define _SC_PHYS_PAGES					40
#define _SC_AVPHYS_PAGES				41
#define _SC_PIPE						42
#define _SC_SELECT						43
#define _SC_POLL						44
#define _SC_MAPPED_FILES				45
#define _SC_THREAD_PROCESS_SHARED		46
#define _SC_THREAD_STACK_MIN			47
#define _SC_THREAD_ATTR_STACKADDR		48
#define _SC_THREAD_ATTR_STACKSIZE		49
#define _SC_THREAD_PRIORITY_SCHEDULING	50
#define _SC_REALTIME_SIGNALS			51
#define	_SC_MEMORY_PROTECTION			52
#define _SC_SIGQUEUE_MAX				53
#define _SC_RTSIG_MAX					54
#define _SC_MONOTONIC_CLOCK				55
#define _SC_DELAYTIMER_MAX				56
#define _SC_TIMER_MAX					57
#define	_SC_TIMERS						58
#define	_SC_CPUTIME						59
#define	_SC_THREAD_CPUTIME				60
#define _SC_HOST_NAME_MAX				61
#define _SC_REGEXP						62
#define _SC_SYMLOOP_MAX					63
#define _SC_SHELL						64
#define _SC_TTY_NAME_MAX				65
#define _SC_ADVISORY_INFO				66
#define _SC_BARRIERS					67
#define _SC_CLOCK_SELECTION				68
#define _SC_FSYNC						69
#define _SC_IPV6						70
#define _SC_MEMLOCK						71
#define _SC_MEMLOCK_RANGE				72
#define _SC_MESSAGE_PASSING				73
#define _SC_PRIORITIZED_IO				74
#define _SC_PRIORITY_SCHEDULING			75
#define _SC_READER_WRITER_LOCKS			76
#define _SC_SHARED_MEMORY_OBJECTS		77
#define _SC_SPAWN						78
#define _SC_SPIN_LOCKS					79
#define _SC_SPORADIC_SERVER				80
#define _SC_SYNCHRONIZED_IO				81
#define _SC_THREAD_PRIO_INHERIT			82
#define _SC_THREAD_PRIO_PROTECT			83
#define _SC_THREAD_ROBUST_PRIO_INHERIT	84
#define _SC_THREAD_ROBUST_PRIO_PROTECT	85
#define _SC_THREAD_SAFE_FUNCTIONS		86
#define _SC_THREAD_SPORADIC_SERVER		87
#define _SC_TIMEOUTS					88
#define _SC_TRACE						89
#define _SC_TRACE_EVENT_FILTER			90
#define _SC_TRACE_INHERIT				91
#define _SC_TRACE_LOG					92
#define _SC_TYPED_MEMORY_OBJECTS		93
#define _SC_V6_ILP32_OFF32				94	/* OB */
#define _SC_V6_ILP32_OFFBIG				95	/* OB */
#define _SC_V6_LP64_OFF64				96	/* OB */
#define _SC_V6_LPBIG_OFFBIG				97	/* OB */
#define _SC_V7_ILP32_OFF32				98
#define _SC_V7_ILP32_OFFBIG				99
#define _SC_V7_LP64_OFF64				100
#define _SC_V7_LPBIG_OFFBIG				101
#define _SC_2_C_BIND					102
#define _SC_2_C_DEV						103
#define _SC_2_CHAR_TERM					104
#define _SC_2_FORT_DEV					105
#define _SC_2_FORT_RUN					106
#define _SC_2_LOCALEDEF					107
#define _SC_2_PBS						108	/* OB BE */
#define _SC_2_PBS_ACCOUNTING			109	/* OB BE */
#define _SC_2_PBS_CHECKPOINT			110	/* OB BE */
#define _SC_2_PBS_LOCATE				111 /* OB BE */
#define _SC_2_PBS_MESSAGE				112	/* OB BE */
#define _SC_2_PBS_TRACK					113	/* OB BE */
#define _SC_2_SW_DEV					114
#define _SC_2_UPE						115
#define _SC_2_VERSION					116
#define _SC_XOPEN_CRYPT					117
#define _SC_XOPEN_ENH_I18N				118
#define _SC_XOPEN_REALTIME				119
#define _SC_XOPEN_REALTIME_THREADS		120
#define _SC_XOPEN_SHM					121
#define _SC_XOPEN_STREAMS				122
#define _SC_XOPEN_UNIX					123
#define _SC_XOPEN_UUCP					124
#define _SC_XOPEN_VERSION				125

#if _POSIX_ASYNCHRONOUS_IO >= 0
#define _SC_AIO_LISTIO_MAX				126
#define _SC_AIO_MAX						127
#define _SC_AIO_PRIO_DELTA_MAX			128
#endif

#define _SC_BC_BASE_MAX					129
#define _SC_BC_DIM_MAX					130
#define _SC_BC_SCALE_MAX				131
#define _SC_BC_STRING_MAX				132

#define _SC_COLL_WEIGHTS_MAX			133
#define _SC_EXPR_NEST_MAX				134
#define _SC_LINE_MAX					135
#define _SC_LOGIN_NAME_MAX				136
#define _SC_MQ_OPEN_MAX					137
#define _SC_MQ_PRIO_MAX					138
#define _SC_THREAD_DESTRUCTOR_ITERATIONS	139
#define _SC_THREAD_KEYS_MAX				140
#define _SC_THREAD_THREADS_MAX			141
#define _SC_RE_DUP_MAX					142

#define _SC_DEVICE_CONTROL				143


/* confstr() constants */
#define _CS_PATH				1

/* lseek() constants */
#ifndef SEEK_SET
#	define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#	define SEEK_CUR 1
#endif
#ifndef SEEK_END
#	define SEEK_END 2
#endif
#ifndef SEEK_DATA
#	define SEEK_DATA 3
#endif
#ifndef SEEK_HOLE
#	define SEEK_HOLE 4
#endif

#define CLOSE_RANGE_CLOEXEC	0x4


#ifdef __cplusplus
extern "C" {
#endif

/* file functions */
extern int		access(const char *path, int accessMode);
extern int		faccessat(int fd, const char *path, int accessMode, int flag);

extern int		chdir(const char *path);
extern int		fchdir(int fd);
extern char		*getcwd(char *buffer, size_t size);

extern int		pipe(int fildes[2]);
extern int		pipe2(int fildes[2], int flags);
extern int		dup(int fd);
extern int		dup2(int fd1, int fd2);
extern int		dup3(int fd1, int fd2, int flags);
extern int		close(int fd);
extern int		link(const char *toPath, const char *path);
extern int		linkat(int toFD, const char *toPath, int pathFD,
					const char *path, int flag);
extern int		unlink(const char *name);
extern int		unlinkat(int fd, const char *path, int flag);
extern int		rmdir(const char *path);

extern ssize_t  readlink(const char *path, char *buffer, size_t bufferSize);
extern ssize_t	readlinkat(int fd, const char *path, char *buffer,
					size_t bufferSize);
extern int      symlink(const char *toPath, const char *symlinkPath);
extern int		symlinkat(const char *toPath, int fd, const char *symlinkPath);

extern int      ftruncate(int fd, off_t newSize);
extern int      truncate(const char *path, off_t newSize);

/* We want the 3rd and 4th arguments to ioctl to be optional, but we can't use varargs because
 * then we have no way to know if the optional arguments are present, and calling va_next is
 * undefined behavior (it may or may not work depending on the calling convention).
 *
 * In C++, we implement ioctl using default function arguments, but in C this isn't possible.
 * So, the C implementation is done as vararg macro that use preprocessor magic to call the
 * 4 argument function and fill in the omitted arguments.
 *
 * The legacy ioctl function in C with vararg arguments is still provided for ABI compatibility
 * with existing applications, but should not be used anymore by newly compiled code.
 */
extern int		__ioctl(int fd, ulong cmd, void* argument, size_t size);
#ifndef __cplusplus
extern int		ioctl(int fd, unsigned long op, ...);
#define _IOCTL2(a, b) 		__ioctl(a, b, NULL, 0)
#define _IOCTL3(a, b, c)	__ioctl(a, b, (void*)(c), 0)
#define _IOCTL4(a, b, c, d)	__ioctl(a, b, (void*)(c), d)
#define _IOCTL(ARG1, ARG2, ARG3, ARG4, NAME, ...) NAME
#define ioctl(...) _IOCTL(__VA_ARGS__, _IOCTL4, _IOCTL3, _IOCTL2)(__VA_ARGS__)
#else
inline int
ioctl(int fd, unsigned long op, void* argument = NULL, size_t size = 0)
{
	return __ioctl(fd, op, argument, size);
}
#endif

extern ssize_t	read(int fd, void *buffer, size_t count);
extern ssize_t  read_pos(int fd, off_t pos, void *buffer, size_t count);
extern ssize_t	pread(int fd, void *buffer, size_t count, off_t pos);
extern ssize_t	write(int fd, const void *buffer, size_t count);
extern ssize_t  write_pos(int fd, off_t pos, const void *buffer,size_t count);
extern ssize_t	pwrite(int fd, const void *buffer, size_t count, off_t pos);
extern off_t	lseek(int fd, off_t offset, int whence);

extern void		sync(void);
extern int		fsync(int fd);
extern int		fdatasync(int fd);

extern int		chown(const char *path, uid_t owner, gid_t group);
extern int		fchown(int fd, uid_t owner, gid_t group);
extern int		lchown(const char *path, uid_t owner, gid_t group);
extern int		fchownat(int fd, const char *path, uid_t owner, gid_t group,
					int flag);

extern int		getpagesize(void);
extern int      getdtablesize(void);
extern long		sysconf(int name);
extern long		fpathconf(int fd, int name);
extern long		pathconf(const char *path, int name);
extern size_t	confstr(int name, char *buf, size_t len);
extern int		lockf(int fd, int function, off_t size);

/* process functions */
extern pid_t	fork(void);
extern pid_t	vfork(void);
extern int		execve(const char *path, char * const argv[],
					char *const environment[]);
extern int		execl(const char *path, const char *arg, ...);
extern int		execv(const char *path, char *const argv[]);
extern int		execlp(const char *file, const char *arg, ...);
extern int		execle(const char *path, const char *arg , ... /*, char **envp */);
extern int		execvp(const char *file, char *const argv[]);
extern int		execvpe(const char *file, char *const argv[],
					char *const environment[]);

extern void		_exit(int status) __attribute__ ((noreturn));

extern pid_t	tcgetpgrp(int fd);
extern int		tcsetpgrp(int fd, pid_t pgrpid);

extern int		brk(void *addr);
extern void		*sbrk(intptr_t increment);

extern unsigned	int	alarm(unsigned int seconds);
extern useconds_t	ualarm(useconds_t microSeconds, useconds_t interval);
extern unsigned int	sleep(unsigned int seconds);
extern int			usleep(unsigned int microSeconds);
#ifndef PD_LINUXKPI
extern int 			pause(void);
#endif

/* process */
extern pid_t	getpid(void);
extern pid_t	getpgrp(void);
extern pid_t	getppid(void);
extern pid_t	getsid(pid_t pid);
extern pid_t	getpgid(pid_t pid);

extern pid_t	setsid(void);
extern int		setpgid(pid_t pid, pid_t pgid);
extern pid_t	setpgrp(void);

extern int		chroot(const char *path);

extern int		nice(int incr);

/* access permissions */
extern gid_t	getegid(void);
extern uid_t	geteuid(void);
extern gid_t	getgid(void);
extern uid_t	getuid(void);
extern int		getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
extern int		getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);

extern int		setgid(gid_t gid);
extern int		setuid(uid_t uid);
extern int		setegid(gid_t gid);
extern int		seteuid(uid_t uid);
extern int		setregid(gid_t rgid, gid_t egid);
extern int		setreuid(uid_t ruid, uid_t euid);
extern int		setresgid(gid_t rgid, gid_t egid, gid_t sgid);
extern int		setresuid(uid_t ruid, uid_t euid, uid_t suid);

extern int		getgrouplist(const char* user, gid_t baseGroup,
					gid_t* groupList, int* groupCount);
extern int		getgroups(int groupCount, gid_t groupList[]);
extern int		initgroups(const char* user, gid_t baseGroup);
extern int		setgroups(int groupCount, const gid_t* groupList);

extern char		*getlogin(void);
extern int		getlogin_r(char *name, size_t nameSize);

/* host name */
extern int		sethostname(const char *hostName, size_t nameSize);
extern int		gethostname(char *hostName, size_t nameSize);

/* tty */
extern int		isatty(int fd);
extern char		*ttyname(int fd);
extern int		ttyname_r(int fd, char *buffer, size_t bufferSize);

/* misc */
extern char 	*crypt(const char *key, const char *salt);
extern void 	encrypt(char block[64], int edflag);
extern int		getopt(int argc, char *const *argv, const char *shortOpts);
extern void 	swab(const void *src, void *dest, ssize_t nbytes);

int				getentropy(void *buf, size_t buflen);

/* getopt() related external variables */
extern char *optarg;
extern int optind, opterr, optopt;

#ifdef __cplusplus
}
#endif

#endif  /* _UNISTD_H_ */
