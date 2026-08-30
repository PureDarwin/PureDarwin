/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

#ifndef _hfs_config_
#define _hfs_config_

#include <TargetConditionals.h>

#define HFS_COMPRESSION 1
#define FIFO 1

/* Compatibility declarations for the 7195 HFS sources. */
#ifdef KERNEL
extern int tsleep(void *chan, int pri, const char *wmesg, int timo);
extern int proc_is_forcing_hfs_case_sensitivity(struct proc *p);
#endif

#ifndef F_SETSTATICCONTENT
#define F_SETSTATICCONTENT 68
#endif
#ifndef F_MAKECOMPRESSED
#define F_MAKECOMPRESSED 80
#endif
#ifndef F_SET_GREEDY_MODE
#define F_SET_GREEDY_MODE 81
#endif
#ifndef F_SETIOTYPE
#define F_SETIOTYPE 82
#endif
#ifndef F_IOTYPE_ISOCHRONOUS
#define F_IOTYPE_ISOCHRONOUS 0x0001
#endif
#ifndef FSOPT_EXCHANGE_DATA_ONLY
#define FSOPT_EXCHANGE_DATA_ONLY 0x0000010
#endif

#ifndef nspace_snapshot_event
#define nspace_snapshot_event(...) (0)
#endif
#ifndef resolve_nspace_item
#define resolve_nspace_item(...) (-95)
#endif

// #define HFS_MALLOC_DEBUG 1
// #define HFS_LEAK_DEBUG 1

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) // iOS (real hardware)

#define QUOTA 0
#define CONFIG_PROTECT 1
#define CONFIG_SECLUDED_RENAME 1


#else // OS X

#define QUOTA 1
#define NAMEDSTREAMS 1
#define CONFIG_HFS_DIRLINK 1
#define CONFIG_SEARCHFS 1

#endif

#endif /* defined(_hfs_config_) */
