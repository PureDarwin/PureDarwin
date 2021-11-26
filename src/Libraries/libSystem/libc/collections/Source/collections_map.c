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

#include <os/collections_map.h>

#include <os/base_private.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>

static inline bool
os_map_str_key_equals(const char * a, const char *b)
{
	return a == b || strcmp(a, b) == 0;
}

static inline uint32_t
os_map_str_hash(const char *key)
{
	uint32_t hash = 0;

	for (; *key; key++) {
		hash += (unsigned char)(*key);
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

static inline bool
os_map_32_key_equals(uint32_t a, uint32_t b)
{
	return a == b;
}

static inline uint32_t
os_map_32_hash(uint32_t x)
{
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return (uint32_t)x;
}

static inline bool
os_map_64_key_equals(uint64_t a, uint64_t b)
{
	return a == b;
}

static inline uint32_t
os_map_64_hash(uint64_t key)
{
	return os_map_32_hash((uint32_t)key ^ (uint32_t)(key >> 32));
}

static inline bool
os_map_128_key_equals(os_map_128_key_t a, os_map_128_key_t b)
{
    return a.x[0] == b.x[0] &&
        a.x[1] == b.x[1];
}

static inline uint32_t
os_map_128_hash(os_map_128_key_t key)
{
    return os_map_64_hash(key.x[0] ^ key.x[1]);
}

// The following symbols are required for each include of collections_map.in.c
// IN_MAP(, _t)
//      EXAMPLE: os_map_64_t
//      The opaque representation of the map.
// IN_MAP(, _hash)
//      EXAMPLE: os_map_64_hash
//      The default hash function for the map
// IN_MAP(,_key_equals)
//      Example: os_map_64_key_equals
//      The equality check for this map

#define IN_MAP(PREFIX, SUFFIX) PREFIX ## os_map_str ## SUFFIX
#define os_map_key_t const char *
#define MAP_SUPPORTS_ENTRY
#include "collections_map.in.c"
#undef IN_MAP
#undef os_map_key_t
#undef MAP_SUPPORTS_ENTRY

#define IN_MAP(PREFIX, SUFFIX) PREFIX ## os_map_32 ## SUFFIX
#define os_map_key_t uint32_t
#include "collections_map.in.c"
#undef IN_MAP
#undef os_map_key_t

#define IN_MAP(PREFIX, SUFFIX) PREFIX ## os_map_64 ## SUFFIX
#define os_map_key_t uint64_t
#include "collections_map.in.c"
#undef IN_MAP
#undef os_map_key_t

#define IN_MAP(PREFIX, SUFFIX) PREFIX ## os_map_128 ## SUFFIX
#define os_map_key_t os_map_128_key_t
#include "collections_map.in.c"
#undef IN_MAP
#undef os_map_key_t
