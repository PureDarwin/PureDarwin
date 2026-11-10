// the objc4-internal calls PureDarwin's CoreFoundation and Foundation use, on libobjc2
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <objc/runtime.h>
#include <objc/objc-arc.h>

#ifdef __cplusplus
extern "C" {
#endif

// libobjc2 keeps small objects in the low three pointer bits on 64-bit
#define OBJC_HAVE_TAGGED_POINTERS 1
static inline bool _objc_isTaggedPointer(const void *ptr) { return ((uintptr_t)ptr & 7) != 0; }

// plain isa pointers, no packed bits to mask off
static const uintptr_t objc_debug_isa_class_mask = ~(uintptr_t)0;

// the root implementations, without sending a message the object may override
id objc_retain_fast_np(id obj);
void objc_release_fast_np(id obj);
size_t object_getRetainCount_np(id obj);
static inline id _objc_rootRetain(id obj) { return objc_retain_fast_np(obj); }
static inline void _objc_rootRelease(id obj) { objc_release_fast_np(obj); }
static inline id _objc_rootAutorelease(id obj) { return objc_autorelease(obj); }
static inline uintptr_t _objc_rootRetainCount(id obj) { return object_getRetainCount_np(obj); }
static inline uintptr_t _objc_rootHash(id obj) { return (uintptr_t)obj; }
static inline bool _objc_rootIsDeallocating(id obj) { (void)obj; return false; }

#ifdef __cplusplus
}
#endif
