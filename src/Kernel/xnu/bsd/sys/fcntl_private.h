/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*-
 * Copyright (c) 1983, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fcntl.h	8.3 (Berkeley) 1/21/94
 */


#ifndef _SYS_FCNTL_PRIVATE_H_
#define _SYS_FCNTL_PRIVATE_H_

#include <sys/cdefs.h>
#include <sys/_types/_mode_t.h>

#if __DARWIN_C_LEVEL >= 200809L
#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
#define AT_REMOVEDIR_DATALESS   0x0100  /* Remove a dataless directory without materializing first */
#define AT_SYSTEM_DISCARDED     0x1000  /* Indicated file/folder was discarded by system */
#endif
#endif

/*
 * Constants used for fcntl(2)
 */

/* command values */
#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
#define F_OPENFROM      56              /* SPI: open a file relative to fd (must be a dir) */
#define F_UNLINKFROM    57              /* SPI: open a file relative to fd (must be a dir) */
#define F_CHECK_OPENEVT 58              /* SPI: if a process is marked OPENEVT, or in O_EVTONLY on opens of this vnode */

/* Deprecated/Removed in 10.9 */
#define F_MARKDEPENDENCY 60             /* this process hosts the device supporting the fs backing this fd */

#define F_SETSTATICCONTENT              68              /*
	                                                 * indicate to the filesystem/storage driver that the content to be
	                                                 * written is usually static.  a nonzero value enables it, 0 disables it.
	                                                 */
#define F_MOVEDATAEXTENTS       69              /* Swap only the data associated with two files */

#define F_GETDEFAULTPROTLEVEL   79 /* Get the default protection level for the filesystem */
#define F_MAKECOMPRESSED                80 /* Make the file compressed; truncate & toggle BSD bits */
#define F_SET_GREEDY_MODE               81      /*
	                                         * indicate to the filesystem/storage driver that the content to be
	                                         * written should be written in greedy mode for additional speed at
	                                         * the cost of storage efficiency. A nonzero value enables it, 0 disables it.
	                                         */

#define F_SETIOTYPE             82  /*
	                             * Use parameters to describe content being written to the FD. See
	                             * flag definitions below for argument bits.
	                             */

#define F_RECYCLE                       84      /* Recycle vnode; debug/development builds only */

#define F_OFD_GETLKPID          94      /* get record locking information */

#define F_SETCONFINED           95      /* "confine" OFD to process */
#define F_GETCONFINED           96      /* is-fd-confined? */

#define F_ASSERT_BG_ACCESS      108      /* Assert background access to a file */
#define F_RELEASE_BG_ACCESS     109      /* Release background access to a file */

#endif /* (_POSIX_C_SOURCE && !_DARWIN_C_SOURCE) */

/* file descriptor flags (F_GETFD, F_SETFD) */
#define FD_CLOFORK      2               /* close-on-fork flag */

/*
 * ISOCHRONOUS attempts to sustain a minimum platform-dependent throughput
 * for the duration of the I/O delivered to the driver.
 */
#define F_IOTYPE_ISOCHRONOUS 0x0001

#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
/* fassertbgaccess_t used by F_ASSERT_BG_ACCESS */
typedef struct fassertbgaccess {
	unsigned int       fbga_flags;   /* unused */
	unsigned int       reserved;     /* (to maintain 8-byte alignment) */
	unsigned long long ttl;          /* IN: time to live for the assertion (nanoseconds; continuous) */
} fassertbgaccess_t;

/*
 * SPI: Argument data for F_OPENFROM
 */
struct fopenfrom {
	unsigned int    o_flags;        /* same as open(2) */
	mode_t          o_mode;         /* same as open(2) */
	char *          o_pathname;     /* relative pathname */
};

#ifdef KERNEL
/*
 * LP64 version of fopenfrom.  Memory pointers
 * grow when we're dealing with a 64-bit process.
 *
 * WARNING - keep in sync with fopenfrom (above)
 */
struct user32_fopenfrom {
	unsigned int    o_flags;
	mode_t          o_mode;
	user32_addr_t   o_pathname;
};

struct user_fopenfrom {
	unsigned int    o_flags;
	mode_t          o_mode;
	user_addr_t     o_pathname;
};
#endif /* KERNEL */


#endif /* (_POSIX_C_SOURCE && !_DARWIN_C_SOURCE) */

#endif /* !_SYS_FCNTL_PRIVATE_H_ */
