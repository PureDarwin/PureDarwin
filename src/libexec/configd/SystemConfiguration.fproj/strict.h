//
//  strict.h
//
//  Copyright (c) 2017-2020 Apple Inc. All rights reserved.
//
// Strictly enforces checks for memory allocation failures and setting pointers to NULL after free, based on NRSafeish.h
// from the NetworkRelay project and nw_strict.h from libnetcore. This header file is intended to help the compiler
// enforce better coding practices. It won't prevent clever engineers from introducing bugs but it may reduce a
// number of potentially hard to track down memory smashers.

#ifndef __STRICT_H__
#define __STRICT_H__

#include <stdlib.h>
#include <stdio.h>
#include <malloc/malloc.h>

#ifdef __BLOCKS__
#include <Block.h>
#endif // __BLOCKS__

#pragma mark - Abort

// If you include CrashReporterClient.h before this file, STRICT_ABORT will set the crash reason. If you have a more
// sophisticated way to abort, define STRICT_ABORT before including this file.
#ifndef STRICT_ABORT
#define DEFINED_STRICT_ABORT
#ifdef _H_CRASH_REPORTER_CLIENT
#define STRICT_ABORT(format, ...)                   \
    do {                                            \
	char *reason = NULL;                        \
	asprintf(&reason, format, ##__VA_ARGS__);   \
	CRSetCrashLogMessage(reason);               \
	__builtin_trap();                           \
    } while (0)
#else // _H_CRASH_REPORTER_CLIENT
#define STRICT_ABORT(format, ...)       \
    do {                                \
	__builtin_trap();               \
    } while (0)
#endif // _H_CRASH_REPORTER_CLIENT
#endif // STRICT_ABORT

#pragma mark - Unlikely Macros

#ifdef __cplusplus
#define _STRICT_LIKELY_BOOL(b)          (__builtin_expect(!!(static_cast<long>(b)), 1L))
#define _STRICT_UNLIKELY_BOOL(b)        (__builtin_expect(!!(static_cast<long>(b)), 0L))
#define _STRICT_UNLIKELY_IS_NULL(obj)   _STRICT_UNLIKELY_BOOL(nullptr == (obj))
#else // __cplusplus
#define _STRICT_LIKELY_BOOL(b)          (__builtin_expect(!!((long)(b)), 1L))
#define _STRICT_UNLIKELY_BOOL(b)        (__builtin_expect(!!((long)(b)), 0L))
#define _STRICT_UNLIKELY_IS_NULL(obj)   _STRICT_UNLIKELY_BOOL(NULL == (obj))
#endif // __cplusplus

#pragma mark - Alloc

__BEGIN_DECLS

#pragma clang assume_nonnull begin

static
__attribute__((__malloc__))
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
__alloc_size(1)
void *strict_malloc(size_t size)
{
    if (_STRICT_UNLIKELY_BOOL(size == 0)) {
	STRICT_ABORT("strict_malloc called with size 0");
	// Not reached
    }
    void *buffer = malloc(size);
    if (_STRICT_UNLIKELY_IS_NULL(buffer)) {
	STRICT_ABORT("strict_malloc(%zu) failed", size);
	// Not reached
    }
    return buffer;
}

#define STRICT_MALLOC_TYPE(type)    \
    (type*)strict_malloc(sizeof(type))

static
__attribute__((__malloc__))
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
__alloc_size(1,2)
void *strict_calloc(size_t count, size_t size)
{
    if (_STRICT_UNLIKELY_BOOL(count == 0)) {
	STRICT_ABORT("strict_calloc called with count 0");
	// Not reached
    }
    if (_STRICT_UNLIKELY_BOOL(size == 0)) {
	STRICT_ABORT("strict_calloc called with size 0");
	// Not reached
    }
    if (_STRICT_UNLIKELY_BOOL(count > SIZE_MAX/size)) {
	STRICT_ABORT("strict_calloc count * size would overflow");
    }
    void *buffer = calloc(count, size);
    if (_STRICT_UNLIKELY_IS_NULL(buffer)) {
	STRICT_ABORT("strict_calloc(%zu, %zu) failed", count, size);
	// Not reached
    }
    return buffer;
}

#define STRICT_CALLOC_TYPE(type)    \
    (type*)strict_calloc(1, sizeof(type))

#define strict_reallocf(ptr, size)                                  \
    _Pragma("clang diagnostic push")                                \
    _Pragma("clang diagnostic ignored \"-Wdirect-ivar-access\"")    \
    _strict_reallocf((void **)&(ptr), size)                         \
    _Pragma("clang diagnostic pop")

static
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
__alloc_size(2)
void *_strict_reallocf(void * _Nullable * _Nonnull ptr, size_t size)
{
    if (_STRICT_UNLIKELY_IS_NULL(ptr)) {
	STRICT_ABORT("_strict_reallocf called with NULL ptr");
	// Not reached
    }
    if (_STRICT_UNLIKELY_BOOL(size == 0)) {
	STRICT_ABORT("_strict_reallocf called with size 0");
	// Not reached
    }
    void *buffer = reallocf(*ptr, size);
    if (_STRICT_UNLIKELY_IS_NULL(buffer)) {
	STRICT_ABORT("_strict_reallocf(%zu) failed", size);
	// Not reached
    }
#ifdef __cplusplus
    *ptr = reinterpret_cast<void *>(NULL);
#else // __cplusplus
    *ptr = NULL;
#endif // __cplusplus
    return buffer;
}

#define STRICT_REALLOCF_TYPE(ptr, type)    \
    (type*)strict_reallocf(ptr, sizeof(type))

static
__attribute__((__malloc__))
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
__alloc_size(2)
void *strict_memalign(size_t alignment, size_t size)
{
    if (_STRICT_UNLIKELY_BOOL(size == 0)) {
	STRICT_ABORT("strict_memalign called with size 0");
	// Not reached
    }

    if (_STRICT_UNLIKELY_BOOL(alignment < sizeof(void*))) {
	STRICT_ABORT("strict_memalign called with alignment (%zu) < sizeof(void*)", alignment);
	// Not reached
    }

    // Ensure that alignment is a power of 2. This is likely to catch
    // someone swapping the alignment and size parameters by mistake.
    if (_STRICT_UNLIKELY_BOOL((alignment & (alignment - 1)) != 0)) {
	STRICT_ABORT("strict_memalign called with alignment (%zu) that is not a power of 2", alignment);
	// Not reached
    }

#ifdef __cplusplus
    void *buffer = nullptr;
#else // __cplusplus
    void *buffer = NULL;
#endif // __cplusplus
    if (_STRICT_UNLIKELY_BOOL((posix_memalign(&buffer, alignment, size) != 0 || buffer == NULL))) {
	STRICT_ABORT("posix_memalign(..., %zu, %zu) failed", alignment, size);
	// Not reached
    }
    return buffer;
}

static
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
char *strict_strdup(const char *string)
{
    if (_STRICT_UNLIKELY_IS_NULL(string)) {
	STRICT_ABORT("strict_strdup called with NULL string");
	// Not reached
    }

    char *result = strdup(string);
    if (_STRICT_UNLIKELY_BOOL(result == NULL)) {
	STRICT_ABORT("strdup() failed");
	// Not reached
    }
    return result;
}

#define STRICT_ALLOC_ALIGN_TYPE(align, type)    \
    (type*)strict_memalign(align, sizeof(type))

static
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
__alloc_size(2)
void *strict_malloc_zone_malloc(malloc_zone_t *zone, size_t size)
{
    if (_STRICT_UNLIKELY_BOOL(size == 0)) {
	STRICT_ABORT("strict_malloc_zone_malloc called with size 0");
	// Not reached
    }
	void *buffer = malloc_zone_malloc(zone, size);
    if (_STRICT_UNLIKELY_IS_NULL(buffer)) {
	STRICT_ABORT("strict_malloc_zone_malloc(%zu) failed", size);
	// Not reached
    }
	return buffer;
}

static
__attribute__((__malloc__))
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
__alloc_size(2,3)
void *strict_malloc_zone_calloc(malloc_zone_t *zone, size_t count, size_t size)
{
    if (_STRICT_UNLIKELY_BOOL(count == 0)) {
	STRICT_ABORT("strict_malloc_zone_calloc called with count 0");
	// Not reached
    }
    if (_STRICT_UNLIKELY_BOOL(size == 0)) {
	STRICT_ABORT("strict_malloc_zone_calloc called with size 0");
	// Not reached
    }
    if (_STRICT_UNLIKELY_BOOL(count > SIZE_MAX/size)) {
	STRICT_ABORT("strict_malloc_zone_calloc count * size would overflow");
    }
    void *buffer = malloc_zone_calloc(zone, count, size);
    if (_STRICT_UNLIKELY_IS_NULL(buffer)) {
	STRICT_ABORT("strict_malloc_zone_calloc(%zu, %zu) failed", count, size);
	// Not reached
    }
    return buffer;
}

#define STRICT_MALLOC_ZONE_CALLOC_TYPE(zone, type)    \
	(type*)strict_malloc_zone_calloc(zone, 1, sizeof(type))

static
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
__alloc_size(3)
void *_strict_malloc_zone_realloc(malloc_zone_t *zone, void * _Nullable * _Nonnull ptr, size_t size)
{
    if (_STRICT_UNLIKELY_IS_NULL(ptr)) {
	STRICT_ABORT("_strict_malloc_zone_realloc called with NULL ptr");
	// Not reached
    }
    if (_STRICT_UNLIKELY_BOOL(size == 0)) {
	STRICT_ABORT("_strict_malloc_zone_realloc called with size 0");
	// Not reached
    }
    void *buffer = malloc_zone_realloc(zone, *ptr, size);
    if (_STRICT_UNLIKELY_IS_NULL(buffer)) {
	STRICT_ABORT("_strict_malloc_zone_realloc(%zu) failed", size);
	// Not reached
    }
#ifdef __cplusplus
    *ptr = reinterpret_cast<void *>(NULL);
#else // __cplusplus
    *ptr = NULL;
#endif // __cplusplus
    return buffer;
}

#define strict_malloc_zone_realloc(zone, ptr, size)                 \
    _Pragma("clang diagnostic push")                                \
    _Pragma("clang diagnostic ignored \"-Wdirect-ivar-access\"")    \
    _strict_malloc_zone_realloc((void **)&(ptr), size)              \
    _Pragma("clang diagnostic pop")

static
__attribute__((__malloc__))
__attribute__((__warn_unused_result__))
inline __attribute__((always_inline))
__alloc_size(3)
void *strict_malloc_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size)
{
	if (_STRICT_UNLIKELY_BOOL(size == 0)) {
		STRICT_ABORT("strict_malloc_zone_memalign called with size 0");
		// Not reached
	}

	if (_STRICT_UNLIKELY_BOOL(alignment < sizeof(void*))) {
		STRICT_ABORT("strict_malloc_zone_memalign called with alignment (%zu) < sizeof(void*)", alignment);
		// Not reached
	}

	// Ensure that alignment is a power of 2. This is likely to catch
	// someone swapping the alignment and size parameters by mistake.
	if (_STRICT_UNLIKELY_BOOL((alignment & (alignment - 1)) != 0)) {
		STRICT_ABORT("strict_malloc_zone_memalign called with alignment (%zu) that is not a power of 2", alignment);
		// Not reached
	}

	void *buffer = malloc_zone_memalign(zone, alignment, size);
	if (_STRICT_UNLIKELY_BOOL(buffer == NULL)) {
		STRICT_ABORT("malloc_zone_memalign(..., %zu, %zu) failed", alignment, size);
		// Not reached
	}
	return buffer;
}

#pragma clang assume_nonnull end

__END_DECLS

#pragma mark - Dispose

#define _STRICT_DISPOSE_NOT_NULL_TEMPLATE(ptr, function)        \
    do {                                                        \
	if (_STRICT_UNLIKELY_IS_NULL(ptr)) {                     \
	    STRICT_ABORT(#ptr " is NULL");                      \
	} else {                                                \
	    function(ptr);                                      \
	    (ptr) = NULL;                                       \
	}                                                       \
    } while(0)

#define _STRICT_DISPOSE_TEMPLATE(ptr, function) \
    do {                                        \
	if ((ptr) != NULL) {                    \
	    function(ptr);                      \
	    (ptr) = NULL;                       \
	}                                       \
    } while(0)

#define STRICT_DISPOSE_XPC_NOT_NULL(obj) \
    _STRICT_DISPOSE_NOT_NULL_TEMPLATE(obj, xpc_release)

#define STRICT_DISPOSE_XPC(obj) \
    _STRICT_DISPOSE_TEMPLATE(obj, xpc_release)

#define STRICT_DISPOSE_ALLOCATED_NOT_NULL(ptr)  \
    _STRICT_DISPOSE_NOT_NULL_TEMPLATE(ptr, free)

#define STRICT_DISPOSE_ALLOCATED(ptr)   \
    _STRICT_DISPOSE_TEMPLATE(ptr, free)

#define strict_free(ptr) STRICT_DISPOSE_ALLOCATED(ptr)

#define strict_malloc_zone_free(zone, ptr)  \
    do {                                    \
	if ((ptr) != NULL) {                \
	    malloc_zone_free(zone, ptr);    \
	    (ptr) = NULL;                   \
	}                                   \
    } while (0)

#define STRICT_DISPOSE_DISPATCH_NOT_NULL(obj)   \
    _STRICT_DISPOSE_NOT_NULL_TEMPLATE(obj, dispatch_release)

#define STRICT_DISPOSE_DISPATCH(obj)    \
    _STRICT_DISPOSE_TEMPLATE(obj, dispatch_release)

#define STRICT_DISPOSE_BLOCK_NOT_NULL(obj)  \
    _STRICT_DISPOSE_NOT_NULL_TEMPLATE(obj, _Block_release)

#define STRICT_DISPOSE_BLOCK(obj)   \
    _STRICT_DISPOSE_TEMPLATE(obj, _Block_release)

// Avoid resetting the block if it is equal
// This can lead to the block getting destroyed
#define STRICT_RESET_BLOCK(reference, new_block)                   \
    do {                                                           \
	if (_STRICT_LIKELY_BOOL(reference != new_block)) {         \
	    __typeof(reference) new_block_temp = new_block;        \
	    if (new_block_temp != NULL) {                          \
		new_block_temp = Block_copy(new_block_temp);       \
	    }                                                      \
	    STRICT_DISPOSE_BLOCK(reference);                       \
	    reference = new_block_temp;                            \
	}                                                          \
    } while(0)

#define STRICT_DISPOSE_CF_OBJECT_NOT_NULL(obj)  \
    _STRICT_DISPOSE_NOT_NULL_TEMPLATE(obj, CFRelease)

#define STRICT_DISPOSE_CF_OBJECT(obj)  \
    _STRICT_DISPOSE_TEMPLATE(obj, CFRelease)

#define STRICT_DISPOSE_ADDRINFO_NOT_NULL(ptr)    \
    _STRICT_DISPOSE_NOT_NULL_TEMPLATE(ptr, freeaddrinfo)

#define STRICT_DISPOSE_ADDRINFO(ptr)    \
    _STRICT_DISPOSE_TEMPLATE(ptr, freeaddrinfo)

#pragma mark - Poison

#if !defined(BUILD_TEXT_BASED_API) || BUILD_TEXT_BASED_API == 0

#ifdef __BLOCKS__
#include <Block.h>
#endif // __BLOCKS__

#pragma GCC poison malloc			// use STRICT_MALLOC_TYPE or strict_malloc instead
#pragma GCC poison calloc			// use STRICT_CALLOC_TYPE or strict_calloc instead
#pragma GCC poison realloc			// use STRICT_REALLOCF_TYPE or strict_reallocf instead
#pragma GCC poison reallocf			// use STRICT_REALLOCF_TYPE or strict_reallocf instead
#pragma GCC poison posix_memalign	// use STRICT_ALLOC_ALIGN_TYPE instead
#pragma GCC poison strdup			// use strict_strdup instead
#pragma GCC poison free				// use strict_free or STRICT_DISPOSE_ALLOCATED instead
#pragma GCC poison CFRelease		// use STRICT_DISPOSE_CF_OBJECT instead

#pragma GCC poison malloc_zone_malloc   // use strict_malloc_zone_malloc instead
#pragma GCC poison malloc_zone_calloc   // use strict_malloc_zone_calloc instead
#pragma GCC poison malloc_zone_realloc  // use strict_malloc_zone_realloc instead
#pragma GCC poison malloc_zone_memalign	// use strict_malloc_zone_memalign instead
#pragma GCC poison malloc_zone_free     // use strict_malloc_zone_free instead

#ifndef DO_NOT_POISON_UNSAFE_STRING_FUNCTIONS
#ifdef strncat
#undef strncat
#endif // strncat
#pragma GCC poison strncat			// use strlcat instead

#ifdef strncpy
#undef strncpy
#endif // strncpy
#pragma GCC poison strncpy			// use strlcpy instead
#endif // DO_NOT_POISON_UNSAFE_STRING_FUNCTIONS

// The following may be defines. GCC poison doesn't work with defines,
// so we undef them first.
#ifdef dispatch_release
#undef dispatch_release
#endif // dispatch_release
#pragma GCC poison dispatch_release	// use STRICT_DISPOSE_DISPATCH instead

#ifdef xpc_release
#undef xpc_release
#endif // xpc_release
#pragma GCC poison xpc_release		// use STRICT_DISPOSE_XPC instead

#ifdef Block_release
#undef Block_release
#endif // Block_release
#pragma GCC poison Block_release	// use STRICT_DISPOSE_BLOCK instead

#endif // defined(BUILD_TEXT_BASED_API) && BUILD_TEXT_BASED_API

#ifdef DEFINED_STRICT_ABORT
#undef STRICT_ABORT
#endif // DEFINED_STRICT_ABORT

#endif // __STRICT_H__
