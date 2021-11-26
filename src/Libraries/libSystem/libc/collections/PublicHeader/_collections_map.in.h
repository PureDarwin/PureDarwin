/*
* Copyright (c) 2019 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (mhe 'License'). You may not use this file except in
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

OS_ASSUME_NONNULL_BEGIN
__BEGIN_DECLS

#define os_map_t IN_MAP(,_t)

OS_EXPORT
void
IN_MAP(,_init)(os_map_t *m, os_map_config_t * _Nullable config,
			   int struct_version);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_init(os_map_t *m, os_map_config_t * _Nullable config) {
	IN_MAP(,_init)(m, config, OS_MAP_CONFIG_S_VERSION);
}

OS_EXPORT
void
IN_MAP(,_destroy)(os_map_t *m);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_destroy(os_map_t *m) {
	IN_MAP(,_destroy)(m);
}

OS_EXPORT
void
IN_MAP(,_insert)(os_map_t *m, os_map_key_t key, void *val);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_insert(os_map_t *m, os_map_key_t key, void *val) {
	IN_MAP(,_insert)(m, key, val);
}

OS_EXPORT
void *
IN_MAP(,_find)(os_map_t *m, os_map_key_t key);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void * _Nullable
os_map_find(os_map_t *m, os_map_key_t key) {
	return IN_MAP(,_find)(m, key);
}

OS_EXPORT
void *
IN_MAP(,_delete)(os_map_t *m, os_map_key_t key);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void * _Nullable
os_map_delete(os_map_t *m, os_map_key_t key) {
	return IN_MAP(,_delete)(m, key);
}

OS_EXPORT
void
IN_MAP(,_clear)(os_map_t *m,
					   OS_NOESCAPE IN_MAP(,_payload_handler_t) handler);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_clear(os_map_t *m,
					OS_NOESCAPE IN_MAP(,_payload_handler_t) handler) {
    IN_MAP(,_clear)(m, handler);
}

OS_EXPORT
size_t
IN_MAP(,_count)(os_map_t *m);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline size_t
os_map_count(os_map_t *m) {
	return IN_MAP(,_count)(m);
}

OS_EXPORT
void
IN_MAP(,_foreach)(os_map_t *m,
					OS_NOESCAPE IN_MAP(,_payload_handler_t) handler);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_map_foreach(os_map_t *m,
				 OS_NOESCAPE IN_MAP(,_payload_handler_t) handler) {
	IN_MAP(,_foreach)(m, handler);
}

__END_DECLS
OS_ASSUME_NONNULL_END
