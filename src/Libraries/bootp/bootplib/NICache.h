/*
 * Copyright (c) 2000-2010 Apple Inc. All rights reserved.
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

/*
 * NICache.h
 * - netinfo cache routines
 */

#ifndef _S_NICACHE_H
#define _S_NICACHE_H

#include "netinfo.h"
#include "dynarray.h"

#define CACHE_MIN			10
#define CACHE_MAX			256

struct PLCacheEntry;
typedef struct PLCacheEntry PLCacheEntry_t;

struct PLCacheEntry {
    ni_proplist		pl;
    void *		value1;
    void *		value2;
    PLCacheEntry_t *	next;
    PLCacheEntry_t *	prev;
};

struct PLCache {
    PLCacheEntry_t *	head;
    PLCacheEntry_t *	tail;
    int			max_entries;
    int			count;
};
typedef struct PLCache PLCache_t;

typedef boolean_t NICacheFunc_t(void * arg, struct in_addr iaddr);

#endif /* _S_NICACHE_H */
