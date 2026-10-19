/*
 * Copyright 2002-2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H


#include <BeBuild.h>
#include <config/types.h>
#include <stddef.h>


/* BSD compatibility */
typedef unsigned long 		u_long;
typedef unsigned int 		u_int;
typedef unsigned short 		u_short;
typedef unsigned char 		u_char;


/* SysV compatibility */
typedef unsigned long 		ulong;
typedef unsigned short 		ushort;
typedef unsigned int 		uint;
typedef unsigned char		unchar;


#if __GNUC__ > 2 && !defined(__ssize_t__)
#define __ssize_t__
typedef long signed int	ssize_t;
#endif

typedef __haiku_int64 		blkcnt_t;
typedef __haiku_std_int32	blksize_t;
typedef __haiku_int64		fsblkcnt_t;
typedef __haiku_int64		fsfilcnt_t;
typedef __haiku_int64		off_t;
typedef __haiku_int64		ino_t;
typedef __haiku_std_int32	cnt_t;
typedef __haiku_int32		dev_t;
typedef __haiku_int32		pid_t;
typedef __haiku_int32		id_t;

typedef __haiku_std_uint32	uid_t;
typedef __haiku_std_uint32	gid_t;
typedef __haiku_std_uint32  mode_t;
#ifndef PD_LINUXKPI
typedef __haiku_std_uint32	umode_t;
#endif
typedef __haiku_std_int32	nlink_t;

#ifdef __HAIKU_BEOS_COMPATIBLE_TYPES
typedef int		daddr_t;	/* disk address */
#else
typedef off_t	daddr_t;	/* disk address */
#endif
typedef char*				caddr_t;

typedef __haiku_addr_t		addr_t;
typedef __haiku_int32 		key_t;

typedef __haiku_std_int32	clockid_t;
typedef struct __timer_t*	timer_t;


/* pthread types */

typedef struct	_pthread_thread		*pthread_t;
typedef struct  _pthread_attr		*pthread_attr_t;
typedef struct  _pthread_barrier	pthread_barrier_t;
typedef struct  _pthread_barrierattr	*pthread_barrierattr_t;
typedef struct  _pthread_mutex		pthread_mutex_t;
typedef struct  _pthread_mutexattr	*pthread_mutexattr_t;
typedef struct  _pthread_cond		pthread_cond_t;
typedef struct  _pthread_condattr	*pthread_condattr_t;
typedef int							pthread_key_t;
typedef struct  _pthread_once		pthread_once_t;
typedef struct  _pthread_rwlock		pthread_rwlock_t;
typedef struct  _pthread_rwlockattr	*pthread_rwlockattr_t;
typedef struct  _pthread_spinlock	pthread_spinlock_t;

struct _pthread_mutex {
	__haiku_std_uint32	flags;
	__haiku_std_int32	lock;
	__haiku_std_int32	unused;
	__haiku_std_int32	owner;
	__haiku_std_int32	owner_count;
};

struct _pthread_barrier {
	__haiku_std_uint32	flags;
	__haiku_std_int32	lock;
	__haiku_std_int32	mutex;
	__haiku_std_int32	waiter_count;
	__haiku_std_int32	waiter_max;
};

struct _pthread_cond {
	__haiku_std_uint32	flags;
	__haiku_std_int32	unused;
	pthread_mutex_t*	mutex;
	__haiku_std_int32	waiter_count;
	__haiku_std_int32	lock;
};

struct _pthread_once {
	__haiku_std_int32	state;
};

struct _pthread_rwlock {
	__haiku_std_uint32	flags;
	__haiku_std_int32	owner;
	union {
		struct {
			__haiku_std_int32	mutex;
			__haiku_std_int32	unused;
			__haiku_std_int32	reader_count;
			__haiku_std_int32	writer_count;
			void*				waiters[2];
		} local;
		struct {
			__haiku_std_int32	sem;
		} shared;
	} u;
};

struct _pthread_spinlock {
	__haiku_std_int32		lock;
};


#include <time.h>

#endif
