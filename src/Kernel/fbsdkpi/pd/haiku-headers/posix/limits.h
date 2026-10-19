/*
 * Copyright 2001-2020 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _LIBC_LIMITS_H_
#define _LIBC_LIMITS_H_
	/* Note: The header guard is checked in gcc's limits.h. */


#include <config/types.h>

#include <float.h>		/* for DBL_DIG, FLT_DIG, etc */

#include __HAIKU_ARCH_HEADER(limits.h)


#define LONGLONG_MIN    (-9223372036854775807LL - 1)  /* these are Be specific */
#define LONGLONG_MAX    (9223372036854775807LL)
#define ULONGLONG_MAX   (0xffffffffffffffffULL)

#define	ULLONG_MAX		ULONGLONG_MAX
#define	LLONG_MAX		LONGLONG_MAX
#define	LLONG_MIN		LONGLONG_MIN

#define MB_LEN_MAX		16

#define OFF_MAX			LLONG_MAX
#define OFF_MIN			LLONG_MIN

#define ARG_MAX			 		(128 * 1024)
#define ATEXIT_MAX			 	(32)
#define CHILD_MAX				(1024)
#define IOV_MAX					(1024)
#define FILESIZEBITS			(64)
#define HOST_NAME_MAX			(255)
#define LINE_MAX				(2048)
#define LINK_MAX				(1)
#define LOGIN_NAME_MAX			(32)
#define MAX_CANON		   		(255)
#define MAX_INPUT				(255)
#define NAME_MAX				(256)
#define NGROUPS_MAX		 		(32)
#define OPEN_MAX				(128)
#define PATH_MAX				(1024)
#define PIPE_BUF				(4 * 1024)
#define PIPE_MAX				(512)
#define PTHREAD_KEYS_MAX		(512)
#define PTHREAD_STACK_MIN		(2 * PAGESIZE)
#define PTHREAD_DESTRUCTOR_ITERATIONS	(4)
#define SSIZE_MAX		  		__HAIKU_SADDR_MAX
#define TTY_NAME_MAX			(256)
#define TZNAME_MAX		  		(32)
#define	SYMLINK_MAX				(1024)
#define	SYMLOOP_MAX				(16)

#define	BC_BASE_MAX				(99)
#define BC_DIM_MAX				(2048)
#define BC_SCALE_MAX			(99)
#define BC_STRING_MAX			(1000)

#define COLL_WEIGHTS_MAX		(10)
#define	EXPR_NEST_MAX			(32)

#define MQ_OPEN_MAX				(0)
#define MQ_PRIO_MAX				(0)

#define	RE_DUP_MAX				(0x7fff)

#define GETENTROPY_MAX			(256)

#ifdef _XOPEN_SOURCE
/* The XSI name for PAGESIZE, with extra underscore included. Only define if
 * _XOPEN_SOURCE was requested, otherwise it could conflict with the application.
 */
#define PAGE_SIZE				PAGESIZE
/* XSI extension: Default process priority. This is used by the implementation
 * of getpriority(), setpriority() and nice(). */
#define NZERO					20
#endif


#define _POSIX_ARG_MAX	  		(4096)
#define _POSIX_CHILD_MAX		(25)
#define _POSIX_DELAYTIMER_MAX	(32)
#define _POSIX_HOST_NAME_MAX	(255)
#define _POSIX_LINK_MAX	 		(8)
#define _POSIX_LOGIN_NAME_MAX	(9)
#define _POSIX_MAX_CANON		(255)
#define _POSIX_MAX_INPUT		(255)
#define _POSIX_MQ_OPEN_MAX		(8)
#define _POSIX_MQ_PRIO_MAX		(32)
#define _POSIX_NAME_MAX	 		(14)
#define _POSIX_NGROUPS_MAX  	(8)
#define _POSIX_OPEN_MAX	 		(20)
#define _POSIX_PATH_MAX	 		(256)
#define _POSIX_PIPE_BUF	 		(512)
#define _POSIX_RE_DUP_MAX		(255)
#define _POSIX_RTSIG_MAX		(8)
#define _POSIX_SEM_NSEMS_MAX	(256)
#define _POSIX_SEM_VALUE_MAX	(32767)
#define _POSIX_SIGQUEUE_MAX		(32)
#define _POSIX_SSIZE_MAX		(32767)
#define _POSIX_STREAM_MAX   	(8)
#define _POSIX_SYMLINK_MAX   	(255)
#define _POSIX_SYMLOOP_MAX   	(8)
#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS (4)
#define _POSIX_THREAD_KEYS_MAX	(128)
#define _POSIX_THREAD_THREADS_MAX (64)
#define _POSIX_TIMER_MAX		(32)
#define _POSIX_TTY_NAME_MAX		(9)
#define _POSIX_TZNAME_MAX   	(6)

#define _POSIX_CLOCKRES_MIN		(20000000)

#define _POSIX2_BC_BASE_MAX		(99)
#define _POSIX2_BC_DIM_MAX		(2048)
#define _POSIX2_BC_SCALE_MAX	(99)
#define _POSIX2_BC_STRING_MAX	(1000)
#define _POSIX2_CHARCLASS_NAME_MAX (14)
#define _POSIX2_COLL_WEIGHTS_MAX (2)
#define _POSIX2_EXPR_NEST_MAX	(32)
#define _POSIX2_LINE_MAX		(2048)
#define _POSIX2_RE_DUP_MAX		_POSIX_RE_DUP_MAX

#define _XOPEN_IOV_MAX			(16)
#define _XOPEN_NAME_MAX			(255)
#define _XOPEN_PATH_MAX			(1024)


/* _GCC_LIMITS_H_ is defined by GCC's internal limits.h to avoid
 * collisions with any defines in this file.
 */
#ifndef _GCC_LIMITS_H_
#	include_next <limits.h>
#endif

#endif /* _LIBC_LIMITS_H_ */
