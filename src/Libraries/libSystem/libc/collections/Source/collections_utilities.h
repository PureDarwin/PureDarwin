/*
* Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef collections_utilities_h
#define collections_utilities_h

#ifndef roundup
#define roundup(x, y)   ((((x) % (y)) == 0) ? \
	                (x) : ((x) + ((y) - ((x) % (y)))))
#endif /* roundup */

/* Macros for min/max. */
#ifndef MIN
#define MIN(a, b) (((a)<(b))?(a):(b))
#endif /* MIN */
#ifndef MAX
#define MAX(a, b) (((a)>(b))?(a):(b))
#endif  /* MAX */

#define MAP_MINSHIFT  5
#define MAP_MINSIZE   (1 << MAP_MINSHIFT)

#ifdef DEBUG

#define DEBUG_ASSERT(X) assert(X)

#define DEBUG_ASSERT_MAP_INVARIANTS(m) \
	assert(m->data); \
	assert(m->size >= MAP_MINSIZE); \
	assert(m->count < m->size)

#else

#define DEBUG_ASSERT(X)

#define DEBUG_ASSERT_MAP_INVARIANTS(map)


#endif

#endif /* collections_utilities_h */
