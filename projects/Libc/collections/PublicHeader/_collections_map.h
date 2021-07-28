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

#ifndef __OS_COLLECTIONS_MAP_H
#define __OS_COLLECTIONS_MAP_H

#include <os/collections_map.h>

OS_ASSUME_NONNULL_BEGIN
__BEGIN_DECLS

#ifndef os_map_str_payload_handler_t
typedef bool (^os_map_str_payload_handler_t) (const char *, void *);
#endif

#ifndef os_map_32_payload_handler_t
typedef bool (^os_map_32_payload_handler_t) (uint32_t, void *);
#endif

#ifndef os_map_64_payload_handler_t
typedef bool (^os_map_64_payload_handler_t) (uint64_t, void *);
#endif

#ifndef os_map_128_payload_handler_t
typedef bool (^os_map_128_payload_handler_t) (os_map_128_key_t, void *);
#endif

OS_EXPORT
const char *
os_map_str_entry(os_map_str_t *m, const char *key);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline const char * _Nullable
os_map_entry(os_map_str_t *m, const char *key)
{
	return os_map_str_entry(m, key);
}

__END_DECLS
OS_ASSUME_NONNULL_END

#define IN_MAP(PREFIX, SUFFIX) PREFIX ## os_map_str ## SUFFIX
#define os_map_key_t const char *
#include "_collections_map.in.h"
#undef IN_MAP
#undef os_map_key_t

#define IN_MAP(PREFIX, SUFFIX) PREFIX ## os_map_32 ## SUFFIX
#define os_map_key_t uint32_t
#include "_collections_map.in.h"
#undef IN_MAP
#undef os_map_key_t

#define IN_MAP(PREFIX, SUFFIX) PREFIX ## os_map_64 ## SUFFIX
#define os_map_key_t uint64_t
#include "_collections_map.in.h"
#undef IN_MAP
#undef os_map_key_t

#define IN_MAP(PREFIX, SUFFIX) PREFIX ## os_map_128 ## SUFFIX
#define os_map_key_t os_map_128_key_t
#include "_collections_map.in.h"
#undef IN_MAP
#undef os_map_key_t

#endif /* __OS_COLLECTIONS_MAP_H */
