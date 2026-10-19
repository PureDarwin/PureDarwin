/*
 * Copyright 2007 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_ERRNO_H_
#define _FBSD_COMPAT_SYS_ERRNO_H_


#include <posix/errno.h>


#define EDOOFUS 		EINVAL

/* pseudo-errors returned inside freebsd compat layer to modify return to process */
#define	ERESTART		(B_ERRORS_END + 1)		/* restart syscall */
#define EJUSTRETURN		(B_ERRORS_END + 2)		/* don't modify regs, just return */

#endif
