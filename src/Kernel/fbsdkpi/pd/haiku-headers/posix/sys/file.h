/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_FILE_H
#define _SYS_FILE_H


#include <sys/types.h>


/* for use with flock() */
#define	LOCK_SH		0x01	/* shared file lock */
#define	LOCK_EX		0x02	/* exclusive file lock */
#define	LOCK_NB		0x04	/* don't block when locking */
#define	LOCK_UN		0x08	/* unlock file */


#ifdef __cplusplus
extern "C" {
#endif

extern int	flock(int fd, int op);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FILE_H */
