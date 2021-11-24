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

#ifndef _OS_COLLECTIONS_MAP_H
#define _OS_COLLECTIONS_MAP_H

#include <os/base.h>
#include <stdint.h>
#include <stdbool.h>
#include <TargetConditionals.h>
#include <sys/types.h>

OS_ASSUME_NONNULL_BEGIN

struct os_map_config_s
{
	const char *name; // Only used when DEBUG is set
	uint32_t initial_size; // If 0, default will be used
};

// Increment this when changing os_map_config_s
#define OS_MAP_CONFIG_S_VERSION 1

typedef struct os_map_config_s os_map_config_t;


// *** HASH MAPS ***
// Stores values for keys. Not safe for concurrent use.


struct os_map_128_key_s {
	uint64_t x[2];
};

typedef struct os_map_128_key_s os_map_128_key_t;

#if TARGET_RT_64_BIT
#define OPAQUE_MAP_SIZE 3
#else
#define OPAQUE_MAP_SIZE 4
#endif

struct _os_opaque_str_map_s {
	void *data[OPAQUE_MAP_SIZE];
};

struct _os_opaque_32_map_s {
	void *data[OPAQUE_MAP_SIZE];
};

struct _os_opaque_64_map_s {
	void *data[OPAQUE_MAP_SIZE];
};

struct _os_opaque_128_map_s {
	void *data[OPAQUE_MAP_SIZE];
};

// Map with string keys and void * values
// Keys must be valid pointers.
typedef struct _os_opaque_str_map_s os_map_str_t ;
// Map with uint32_t keys and void * values
typedef struct _os_opaque_32_map_s os_map_32_t ;
// Map with uint64_t keys and void * values
typedef struct _os_opaque_64_map_s os_map_64_t ;
// Map with os_map_128_key_t keys and void * values
typedef struct _os_opaque_128_map_s os_map_128_t ;

/*!
* @function os_map_init
* Initialize a map.
*
* @param t
* The table to initialize
*
* @param config
* The configuration to use for this table
*
* @discussion
* An initialized map will use additional memory which can be freed with
* os_map_destroy.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_init(os_map_64_t *m, os_map_config_t * _Nullable config);

/*!
* @function os_map_destroy
* Destroy a map.
*
* @param t
* The table to destroy.
*
* @discussion
* This will free all memory used by the map, but will not take any action
* on the keys/values that were in the map at the time.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_destroy(os_map_64_t *m);

/*!
* @function os_map_insert
* Insert an element into a map.
*
* @param t
* The table to initialize
*
* @param key
* The key to insert the value at
*
* @param val
* The value to insert; cannot be NULL (use os_map_delete to remove entries)
*
* @discussion
* This will insert an element into the map, growing the map if needed. Does not
* support replacing an existing key, so inserting twice without a remove will
* cause undefined behavior.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_insert(os_map_64_t *m, uint64_t key, void *val);

/*!
* @function os_map_find
* Find an element in a map.
*
* @param t
* The map to search
*
* @param key
* The key to search for
*
* @result
* The value stored at key, or NULL if no value is present.
*
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void * _Nullable
os_map_find(os_map_64_t *m, uint64_t key);

/*!
* @function os_map_delete
* Remove an element from a map.
*
* @param t
* The map to remove the element from
*
* @param key
* The key of the element to be removed
*
* @result
* The value stored at key, or NULL if no value is present.
*
* @discussion
* Has no effect if the key is not present
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void * _Nullable
os_map_delete(os_map_64_t *m, uint64_t key);

typedef bool (^os_map_64_payload_handler_t) (uint64_t, void *);

/*!
* @function os_map_clear
* Removes all elements from a map.
*
* @param t
* The map to remove the elements from
*
* @param handler
* A handler that will be called for all elements in the table. Handler may be
* NULL.
*
* @discussion
* Has no effect if the key is not present
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_clear(os_map_64_t *m,
		  OS_NOESCAPE os_map_64_payload_handler_t _Nullable handler);

/*!
* @function os_map_count
* Gets the number of items present in a map
*
* @param t
* The map to get the count of
*
* @result
* Returns the number of items present in the map
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline size_t
os_map_count(os_map_64_t *m);

/*!
* @function os_map_foreach
*Iterate over the key/value pairs in a map.
*
* @param t
* The map to iterate over
*
* @param handler
* The handler to call for each entry in the map.
*
* @discussion
* Will invoke handler for each key/value pair in the map. Modifying the map
* during this iteration is not permitted. The handler may be called on the
* entries in any order.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_foreach(os_map_64_t *m,
		 OS_NOESCAPE os_map_64_payload_handler_t handler);

/*!
* @function os_map_entry
* Gets the exact entry, if present, for a given key and NULL otherwise.
*
* @param t
* The map to search
*
* @param key
* The key to search for
*
* @discussion
* Only available for os_map_str_t.
* Gets the exact entry, if present, for a given key and NULL otherwise. So, for
* two equal strings a, b where a != b. We guarentee that
* os_map_entry(t, a) == os_map_entry(t, b)
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline const char *
os_map_entry(os_map_str_t *m, const char *key);

OS_ASSUME_NONNULL_END

#include <os/_collections_map.h>

#endif /* _OS_COLLECTIONS_MAP_H */
