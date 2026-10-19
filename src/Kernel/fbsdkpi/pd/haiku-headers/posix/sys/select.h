/*
 * Copyright 2002-2024, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H


#include <config/types.h>

#include <sys/time.h>
#include <signal.h>
#include <string.h>


/* Custom FD_SETSIZEs can be defined if more bits are needed, but the default
 * should suffice for most uses. */
#ifndef FD_SETSIZE
#	define FD_SETSIZE	1024
#endif

typedef __haiku_uint32 fd_mask;

#define NFDBITS		(sizeof(fd_mask) * 8)	/* bits per mask */

typedef struct fd_set {
	fd_mask bits[((FD_SETSIZE) + ((NFDBITS) - 1)) / (NFDBITS)];
} fd_set;

#define _FD_BITSINDEX(fd) ((fd) / NFDBITS)
#define _FD_BIT(fd) (1L << ((fd) % NFDBITS))

#define FD_ZERO(set) memset((set), 0, sizeof(fd_set))
#define FD_SET(fd, set) ((set)->bits[_FD_BITSINDEX(fd)] |= _FD_BIT(fd))
#define FD_CLR(fd, set) ((set)->bits[_FD_BITSINDEX(fd)] &= ~_FD_BIT(fd))
#define FD_ISSET(fd, set) ((set)->bits[_FD_BITSINDEX(fd)] & _FD_BIT(fd))
#define FD_COPY(source, target) (*(target) = *(source))


#ifdef __cplusplus
extern "C" {
#endif

extern int pselect(int numBits, struct fd_set *readBits, struct fd_set *writeBits,
			struct fd_set *errorBits, const struct timespec *timeout, const sigset_t *sigMask);
extern int select(int numBits, struct fd_set *readBits, struct fd_set *writeBits,
			struct fd_set *errorBits, struct timeval *timeout);

#ifdef __cplusplus
}
#endif


#endif	/* _SYS_SELECT_H */
