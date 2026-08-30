/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _SYS_MEM_ACCT_PRIVATE_H
#define _SYS_MEM_ACCT_PRIVATE_H

#include <stdint.h>

#include <sys/types.h>

#define MEM_ACCT_PEAK                       1       /* reset/get peak value for a subsystem */
#define MEM_ACCT_SOFT_LIMIT                 2       /* set/get soft limit for a subsystem */
#define MEM_ACCT_HARD_LIMIT                 3       /* set/get hard limit for a subsystem */
#define MEM_ACCT_ALLOCATED                  4       /* set/get currently allocated memory for a subsystem */
#define MEM_ACCT_SUBSYSTEMS                 5       /* get all subsystem names */
#define MEM_ACCT_ALL_SUBSYSTEM_STATISTICS   6       /* returns all statistics for all subsystems */
#define MEM_ACCT_ALL_STATISTICS             7       /* returns all statistics for a specific subsystem */

#define MEM_ACCT_MAX                        8       /* Current maximum number of accounting objects we allow */

#define MEM_ACCT_NAME_LENGTH                16      /* max size for subsystem name */

struct memacct_statistics {
	uint64_t peak;
	int64_t allocated;
	uint64_t softlimit;
	uint64_t hardlimit;
	char ma_name[MEM_ACCT_NAME_LENGTH];
};

#endif /* _SYS_MEM_ACCT_PRIVATE_H */
