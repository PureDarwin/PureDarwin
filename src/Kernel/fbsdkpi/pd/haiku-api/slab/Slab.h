// SPDX-License-Identifier: MIT
// haiku's object caches, as fixed-size aligned allocations
#ifndef _PD_HAIKU_SLAB_SLAB_H
#define _PD_HAIKU_SLAB_SLAB_H
#include <OS.h>
#include <KernelExport.h>
enum {
	CACHE_DONT_WAIT_FOR_MEMORY		= 1 << 0,
	CACHE_DONT_LOCK_KERNEL_SPACE	= 1 << 1,
	CACHE_PRIORITY_VIP				= 1 << 2,
	CACHE_NO_DEPOT					= 1 << 3,
	CACHE_ALIGN_ON_SIZE				= 1 << 30,
};
typedef struct object_cache object_cache;
typedef status_t (*object_cache_constructor)(void *cookie, void *object);
typedef void (*object_cache_destructor)(void *cookie, void *object);
typedef void (*object_cache_reclaimer)(void *cookie, int32 level);
#ifdef __cplusplus
extern "C" {
#endif
object_cache *create_object_cache(const char *name, size_t objectSize,
	uint32 flags);
object_cache *create_object_cache_etc(const char *name, size_t objectSize,
	size_t alignment, size_t maxByteUsage, size_t magazineCapacity,
	size_t maxMagazineCount, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer);
void delete_object_cache(object_cache *cache);
void *object_cache_alloc(object_cache *cache, uint32 flags);
void object_cache_free(object_cache *cache, void *object, uint32 flags);
status_t object_cache_reserve(object_cache *cache, size_t object_count,
	uint32 flags);
#ifdef __cplusplus
}
#endif
#endif
