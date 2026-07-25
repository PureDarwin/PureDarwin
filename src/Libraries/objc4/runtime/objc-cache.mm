/*
 * Copyright (c) 1999-2007 Apple Inc.  All Rights Reserved.
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

/***********************************************************************
* objc-cache.m
* Method cache management
* Cache flushing
* Cache garbage collection
* Cache instrumentation
* Dedicated allocator for large caches
**********************************************************************/


/***********************************************************************
 * Method cache locking (GrP 2001-1-14)
 *
 * For speed, objc_msgSend does not acquire any locks when it reads 
 * method caches. Instead, all cache changes are performed so that any 
 * objc_msgSend running concurrently with the cache mutator will not 
 * crash or hang or get an incorrect result from the cache. 
 *
 * When cache memory becomes unused (e.g. the old cache after cache 
 * expansion), it is not immediately freed, because a concurrent 
 * objc_msgSend could still be using it. Instead, the memory is 
 * disconnected from the data structures and placed on a garbage list. 
 * The memory is now only accessible to instances of objc_msgSend that 
 * were running when the memory was disconnected; any further calls to 
 * objc_msgSend will not see the garbage memory because the other data 
 * structures don't point to it anymore. The collecting_in_critical
 * function checks the PC of all threads and returns FALSE when all threads 
 * are found to be outside objc_msgSend. This means any call to objc_msgSend 
 * that could have had access to the garbage has finished or moved past the 
 * cache lookup stage, so it is safe to free the memory.
 *
 * All functions that modify cache data or structures must acquire the 
 * cacheUpdateLock to prevent interference from concurrent modifications.
 * The function that frees cache garbage must acquire the cacheUpdateLock 
 * and use collecting_in_critical() to flush out cache readers.
 * The cacheUpdateLock is also used to protect the custom allocator used 
 * for large method cache blocks.
 *
 * Cache readers (PC-checked by collecting_in_critical())
 * objc_msgSend*
 * cache_getImp
 *
 * Cache readers/writers (hold cacheUpdateLock during access; not PC-checked)
 * cache_t::copyCacheNolock    (caller must hold the lock)
 * cache_t::eraseNolock        (caller must hold the lock)
 * cache_t::collectNolock      (caller must hold the lock)
 * cache_t::insert             (acquires lock)
 * cache_t::destroy            (acquires lock)
 *
 * UNPROTECTED cache readers (NOT thread-safe; used for debug info only)
 * cache_print
 * _class_printMethodCaches
 * _class_printDuplicateCacheEntries
 * _class_printMethodCacheStatistics
 *
 ***********************************************************************/


#if __OBJC2__

#include "objc-private.h"

#if TARGET_OS_OSX
//#include <Cambria/Traps.h>
//#include <Cambria/Cambria.h>
#endif

#if __arm__  ||  __x86_64__  ||  __i386__

// objc_msgSend has few registers available.
// Cache scan increments and wraps at special end-marking bucket.
#define CACHE_END_MARKER 1

// Historical fill ratio of 75% (since the new objc runtime was introduced).
static inline mask_t cache_fill_ratio(mask_t capacity) {
    return capacity * 3 / 4;
}

#elif __arm64__ && !__LP64__

// objc_msgSend has lots of registers available.
// Cache scan decrements. No end marker needed.
#define CACHE_END_MARKER 0

// Historical fill ratio of 75% (since the new objc runtime was introduced).
static inline mask_t cache_fill_ratio(mask_t capacity) {
    return capacity * 3 / 4;
}

#elif __arm64__ && __LP64__

// objc_msgSend has lots of registers available.
// Cache scan decrements. No end marker needed.
#define CACHE_END_MARKER 0

// Allow 87.5% fill ratio in the fast path for all cache sizes.
// Increasing the cache fill ratio reduces the fragmentation and wasted space
// in imp-caches at the cost of potentially increasing the average lookup of
// a selector in imp-caches by increasing collision chains. Another potential
// change is that cache table resizes / resets happen at different moments.
static inline mask_t cache_fill_ratio(mask_t capacity) {
    return capacity * 7 / 8;
}

// Allow 100% cache utilization for smaller cache sizes. This has the same
// advantages and disadvantages as the fill ratio. A very large percentage
// of caches end up with very few entries and the worst case of collision
// chains in small tables is relatively small.
// NOTE: objc_msgSend properly handles a cache lookup with a full cache.
#define CACHE_ALLOW_FULL_UTILIZATION 1

#else
#error unknown architecture
#endif

/* Initial cache bucket count. INIT_CACHE_SIZE must be a power of two. */
enum {
#if CACHE_END_MARKER || (__arm64__ && !__LP64__)
    // When we have a cache end marker it fills a bucket slot, so having a
    // initial cache size of 2 buckets would not be efficient when one of the
    // slots is always filled with the end marker. So start with a cache size
    // 4 buckets.
    INIT_CACHE_SIZE_LOG2 = 2,
#else
    // Allow an initial bucket size of 2 buckets, since a large number of
    // classes, especially metaclasses, have very few imps, and we support
    // the ability to fill 100% of the cache before resizing.
    INIT_CACHE_SIZE_LOG2 = 1,
#endif
    INIT_CACHE_SIZE      = (1 << INIT_CACHE_SIZE_LOG2),
    MAX_CACHE_SIZE_LOG2  = 16,
    MAX_CACHE_SIZE       = (1 << MAX_CACHE_SIZE_LOG2),
    FULL_UTILIZATION_CACHE_SIZE_LOG2 = 3,
    FULL_UTILIZATION_CACHE_SIZE = (1 << FULL_UTILIZATION_CACHE_SIZE_LOG2),
};

static int _collecting_in_critical(void);
static void _garbage_make_room(void);

#if DEBUG_TASK_THREADS
static kern_return_t objc_task_threads
(
	task_t target_task,
	thread_act_array_t *act_list,
	mach_msg_type_number_t *act_listCnt
);
#endif

#if DEBUG_TASK_THREADS
#undef HAVE_TASK_RESTARTABLE_RANGES
#endif

/***********************************************************************
* Cache statistics for OBJC_PRINT_CACHE_SETUP
**********************************************************************/
static unsigned int cache_counts[16];
static size_t cache_allocations;
static size_t cache_collections;

static void recordNewCache(mask_t capacity)
{
    size_t bucket = log2u(capacity);
    if (bucket < countof(cache_counts)) {
        cache_counts[bucket]++;
    }
    cache_allocations++;
}

static void recordDeadCache(mask_t capacity)
{
    size_t bucket = log2u(capacity);
    if (bucket < countof(cache_counts)) {
        cache_counts[bucket]--;
    }
}

/***********************************************************************
* Pointers used by compiled class objects
* These use asm to avoid conflicts with the compiler's internal declarations
**********************************************************************/

// EMPTY_BYTES includes space for a cache end marker bucket.
// This end marker doesn't actually have the wrap-around pointer 
// because cache scans always find an empty bucket before they might wrap.
// 1024 buckets is fairly common.
#if DEBUG
    // Use a smaller size to exercise heap-allocated empty caches.
#   define EMPTY_BYTES ((8+1)*16)
#else
#   define EMPTY_BYTES ((1024+1)*16)
#endif

#define stringize(x) #x
#define stringize2(x) stringize(x)

// "cache" is cache->buckets; "vtable" is cache->mask/occupied
// hack to avoid conflicts with compiler's internal declaration
asm("\n .section __TEXT,__const"
    "\n .globl __objc_empty_vtable"
    "\n .set __objc_empty_vtable, 0"
    "\n .globl __objc_empty_cache"
#if CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_LOW_4
    "\n .align 4"
    "\n L__objc_empty_cache: .space " stringize2(EMPTY_BYTES)
    "\n .set __objc_empty_cache, L__objc_empty_cache + 0xf"
#else
    "\n .align 3"
    "\n __objc_empty_cache: .space " stringize2(EMPTY_BYTES)
#endif
    );

#if CONFIG_USE_PREOPT_CACHES
__attribute__((used, section("__DATA_CONST,__objc_scoffs")))
uintptr_t objc_opt_offsets[__OBJC_OPT_OFFSETS_COUNT];
#endif

#if CACHE_END_MARKER
static inline mask_t cache_next(mask_t i, mask_t mask) {
    return (i+1) & mask;
}
#elif __arm64__
static inline mask_t cache_next(mask_t i, mask_t mask) {
    return i ? i-1 : mask;
}
#else
#error unexpected configuration
#endif


// mega_barrier doesn't really work, but it works enough on ARM that
// we leave well enough alone and keep using it there.
#if __arm__
#define mega_barrier() \
    __asm__ __volatile__( \
        "dsb    ish" \
        : : : "memory")

#endif

#if __arm64__

// Pointer-size register prefix for inline asm
# if __LP64__
#   define p "x"  // true arm64
# else
#   define p "w"  // arm64_32
# endif

// Use atomic double-word instructions to update cache entries.
// This requires cache buckets not cross cache line boundaries.
static ALWAYS_INLINE void
stp(uintptr_t onep, uintptr_t twop, void *destp)
{
    __asm__ ("stp %" p "[one], %" p "[two], [%x[dest]]"
             : "=m" (((uintptr_t *)(destp))[0]),
               "=m" (((uintptr_t *)(destp))[1])
             : [one] "r" (onep),
               [two] "r" (twop),
               [dest] "r" (destp)
             : /* no clobbers */
             );
}

static ALWAYS_INLINE void __unused
ldp(uintptr_t& onep, uintptr_t& twop, const void *srcp)
{
    __asm__ ("ldp %" p "[one], %" p "[two], [%x[src]]"
             : [one] "=r" (onep),
               [two] "=r" (twop)
             : "m" (((const uintptr_t *)(srcp))[0]),
               "m" (((const uintptr_t *)(srcp))[1]),
               [src] "r" (srcp)
             : /* no clobbers */
             );
}

#undef p
#endif


// Class points to cache. SEL is key. Cache buckets store SEL+IMP.
// Caches are never built in the dyld shared cache.

static inline mask_t cache_hash(SEL sel, mask_t mask) 
{
    uintptr_t value = (uintptr_t)sel;
#if CONFIG_USE_PREOPT_CACHES
    value ^= value >> 7;
#endif
    return (mask_t)(value & mask);
}

#if __arm64__

template<Atomicity atomicity, IMPEncoding impEncoding>
void bucket_t::set(bucket_t *base, SEL newSel, IMP newImp, Class cls)
{
    ASSERT(_sel.load(memory_order_relaxed) == 0 ||
           _sel.load(memory_order_relaxed) == newSel);

    static_assert(offsetof(bucket_t,_imp) == 0 &&
                  offsetof(bucket_t,_sel) == sizeof(void *),
                  "bucket_t layout doesn't match arm64 bucket_t::set()");

    uintptr_t encodedImp = (impEncoding == Encoded
                            ? encodeImp(base, newImp, newSel, cls)
                            : (uintptr_t)newImp);

    // LDP/STP guarantees that all observers get
    // either imp/sel or newImp/newSel
    stp(encodedImp, (uintptr_t)newSel, this);
}

#else

template<Atomicity atomicity, IMPEncoding impEncoding>
void bucket_t::set(bucket_t *base, SEL newSel, IMP newImp, Class cls)
{
    ASSERT(_sel.load(memory_order_relaxed) == 0 ||
           _sel.load(memory_order_relaxed) == newSel);

    // objc_msgSend uses sel and imp with no locks.
    // It is safe for objc_msgSend to see new imp but NULL sel
    // (It will get a cache miss but not dispatch to the wrong place.)
    // It is unsafe for objc_msgSend to see old imp and new sel.
    // Therefore we write new imp, wait a lot, then write new sel.
    
    uintptr_t newIMP = (impEncoding == Encoded
                        ? encodeImp(base, newImp, newSel, cls)
                        : (uintptr_t)newImp);

    if (atomicity == Atomic) {
        _imp.store(newIMP, memory_order_relaxed);
        
        if (_sel.load(memory_order_relaxed) != newSel) {
#ifdef __arm__
            mega_barrier();
            _sel.store(newSel, memory_order_relaxed);
#elif __x86_64__ || __i386__
            _sel.store(newSel, memory_order_release);
#else
#error Don't know how to do bucket_t::set on this architecture.
#endif
        }
    } else {
        _imp.store(newIMP, memory_order_relaxed);
        _sel.store(newSel, memory_order_relaxed);
    }
}

#endif

void cache_t::initializeToEmpty()
{
    _bucketsAndMaybeMask.store((uintptr_t)&_objc_empty_cache, std::memory_order_relaxed);
    _originalPreoptCache.store(nullptr, std::memory_order_relaxed);
}

#if CONFIG_USE_PREOPT_CACHES
/*
 * The shared cache builder will sometimes have prebuilt an IMP cache
 * for the class and left a `preopt_cache_t` pointer in _originalPreoptCache.
 *
 * However we have this tension:
 * - when the class is realized it has to have a cache that can't resolve any
 *   selector until the class is properly initialized so that every
 *   caller falls in the slowpath and synchronizes with the class initializing,
 * - we need to remember that cache pointer and we have no space for that.
 *
 * The caches are designed so that preopt_cache::bit_one is set to 1,
 * so we "disguise" the pointer so that it looks like a cache of capacity 1
 * where that bit one aliases with where the top bit of a SEL in the bucket_t
 * would live:
 *
 * +----------------+----------------+
 * |      IMP       |       SEL      | << a bucket_t
 * +----------------+----------------+--------------...
 * preopt_cache_t >>|               1| ...
 *                  +----------------+--------------...
 *
 * The shared cache guarantees that there's valid memory to read under "IMP"
 *
 * This lets us encode the original preoptimized cache pointer during
 * initialization, and we can reconstruct its original address and install
 * it back later.
 */
void cache_t::initializeToPreoptCacheInDisguise(const preopt_cache_t *cache)
{
    // preopt_cache_t::bit_one is 1 which sets the top bit
    // and is never set on any valid selector

    uintptr_t value = (uintptr_t)cache + sizeof(preopt_cache_t) -
            (bucket_t::offsetOfSel() + sizeof(SEL));

    _originalPreoptCache.store(nullptr, std::memory_order_relaxed);
    setBucketsAndMask((bucket_t *)value, 0);
    _occupied = cache->occupied;
}

void cache_t::maybeConvertToPreoptimized()
{
    const preopt_cache_t *cache = disguised_preopt_cache();

    if (cache == nil) {
        return;
    }

    if (!cls()->allowsPreoptCaches() ||
            (cache->has_inlines && !cls()->allowsPreoptInlinedSels())) {
        if (PrintCaches) {
            _objc_inform("CACHES: %sclass %s: dropping cache (from %s)",
                         cls()->isMetaClass() ? "meta" : "",
                         cls()->nameForLogging(), "setInitialized");
        }
        return setBucketsAndMask(emptyBuckets(), 0);
    }

    uintptr_t value = (uintptr_t)&cache->entries;
#if __has_feature(ptrauth_calls)
    value = (uintptr_t)ptrauth_sign_unauthenticated((void *)value,
            ptrauth_key_process_dependent_data, (uintptr_t)cls());
#endif
    value |= preoptBucketsHashParams(cache) | preoptBucketsMarker;
    _bucketsAndMaybeMask.store(value, memory_order_relaxed);
    _occupied = cache->occupied;
}

void cache_t::initializeToEmptyOrPreoptimizedInDisguise()
{
    if (os_fastpath(!DisablePreoptCaches)) {
        if (!objc::dataSegmentsRanges.inSharedCache((uintptr_t)this)) {
            if (dyld_shared_cache_some_image_overridden()) {
                // If the system has roots, then we must disable preoptimized
                // caches completely. If a class in another image has a
                // superclass in the root, the offset to the superclass will
                // be wrong. rdar://problem/61601961
                cls()->setDisallowPreoptCachesRecursively("roots");
            }
            return initializeToEmpty();
        }

        auto cache = _originalPreoptCache.load(memory_order_relaxed);
        if (cache) {
            return initializeToPreoptCacheInDisguise(cache);
        }
    }

    return initializeToEmpty();
}

const preopt_cache_t *cache_t::preopt_cache() const
{
    auto addr = _bucketsAndMaybeMask.load(memory_order_relaxed);
    addr &= preoptBucketsMask;
#if __has_feature(ptrauth_calls)
#if __BUILDING_OBJCDT__
    addr = (uintptr_t)ptrauth_strip((preopt_cache_entry_t *)addr,
            ptrauth_key_process_dependent_data);
#else
    addr = (uintptr_t)ptrauth_auth_data((preopt_cache_entry_t *)addr,
            ptrauth_key_process_dependent_data, (uintptr_t)cls());
#endif
#endif
    return (preopt_cache_t *)(addr - sizeof(preopt_cache_t));
}

const preopt_cache_t *cache_t::disguised_preopt_cache() const
{
    bucket_t *b = buckets();
    if ((intptr_t)b->sel() >= 0) return nil;

    uintptr_t value = (uintptr_t)b + bucket_t::offsetOfSel() + sizeof(SEL);
    return (preopt_cache_t *)(value - sizeof(preopt_cache_t));
}

Class cache_t::preoptFallbackClass() const
{
    return (Class)((uintptr_t)cls() + preopt_cache()->fallback_class_offset);
}

bool cache_t::isConstantOptimizedCache(bool strict, uintptr_t empty_addr) const
{
    uintptr_t addr = _bucketsAndMaybeMask.load(memory_order_relaxed);
    if (addr & preoptBucketsMarker) {
        return true;
    }
    if (strict) {
        return false;
    }
    return mask() == 0 && addr != empty_addr;
}

bool cache_t::shouldFlush(SEL sel, IMP imp) const
{
    // This test isn't backwards: disguised caches aren't "strict"
    // constant optimized caches
    if (!isConstantOptimizedCache(/*strict*/true)) {
        const preopt_cache_t *cache = disguised_preopt_cache();
        if (cache) {
            uintptr_t offs = (uintptr_t)sel - (uintptr_t)@selector(🤯);
            uintptr_t slot = ((offs >> cache->shift) & cache->mask);
            auto &entry = cache->entries[slot];

            return entry.sel_offs == offs &&
                (uintptr_t)cls() - entry.imp_offs ==
                (uintptr_t)ptrauth_strip(imp, ptrauth_key_function_pointer);
        }
    }

    return cache_getImp(cls(), sel) == imp;
}

bool cache_t::isConstantOptimizedCacheWithInlinedSels() const
{
    return isConstantOptimizedCache(/* strict */true) && preopt_cache()->has_inlines;
}
#endif // CONFIG_USE_PREOPT_CACHES

#if CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_OUTLINED

void cache_t::setBucketsAndMask(struct bucket_t *newBuckets, mask_t newMask)
{
    // objc_msgSend uses mask and buckets with no locks.
    // It is safe for objc_msgSend to see new buckets but old mask.
    // (It will get a cache miss but not overrun the buckets' bounds).
    // It is unsafe for objc_msgSend to see old buckets and new mask.
    // Therefore we write new buckets, wait a lot, then write new mask.
    // objc_msgSend reads mask first, then buckets.

#ifdef __arm__
    // ensure other threads see buckets contents before buckets pointer
    mega_barrier();

    _bucketsAndMaybeMask.store((uintptr_t)newBuckets, memory_order_relaxed);

    // ensure other threads see new buckets before new mask
    mega_barrier();

    _maybeMask.store(newMask, memory_order_relaxed);
    _occupied = 0;
#elif __x86_64__ || i386
    // ensure other threads see buckets contents before buckets pointer
    _bucketsAndMaybeMask.store((uintptr_t)newBuckets, memory_order_release);

    // ensure other threads see new buckets before new mask
    _maybeMask.store(newMask, memory_order_release);
    _occupied = 0;
#else
#error Don't know how to do setBucketsAndMask on this architecture.
#endif
}

mask_t cache_t::mask() const
{
    return _maybeMask.load(memory_order_relaxed);
}

#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16 || CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS

void cache_t::setBucketsAndMask(struct bucket_t *newBuckets, mask_t newMask)
{
    uintptr_t buckets = (uintptr_t)newBuckets;
    uintptr_t mask = (uintptr_t)newMask;

    ASSERT(buckets <= bucketsMask);
    ASSERT(mask <= maxMask);

    _bucketsAndMaybeMask.store(((uintptr_t)newMask << maskShift) | (uintptr_t)newBuckets, memory_order_relaxed);
    _occupied = 0;
}

mask_t cache_t::mask() const
{
    uintptr_t maskAndBuckets = _bucketsAndMaybeMask.load(memory_order_relaxed);
    return maskAndBuckets >> maskShift;
}

#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_LOW_4

void cache_t::setBucketsAndMask(struct bucket_t *newBuckets, mask_t newMask)
{
    uintptr_t buckets = (uintptr_t)newBuckets;
    unsigned mask = (unsigned)newMask;

    ASSERT(buckets == (buckets & bucketsMask));
    ASSERT(mask <= 0xffff);

    _bucketsAndMaybeMask.store(buckets | objc::mask16ShiftBits(mask), memory_order_relaxed);
    _occupied = 0;

    ASSERT(this->buckets() == newBuckets);
    ASSERT(this->mask() == newMask);
}

mask_t cache_t::mask() const
{
    uintptr_t maskAndBuckets = _bucketsAndMaybeMask.load(memory_order_relaxed);
    uintptr_t maskShift = (maskAndBuckets & maskMask);
    return 0xffff >> maskShift;
}

#else
#error Unknown cache mask storage type.
#endif

struct bucket_t *cache_t::buckets() const
{
    uintptr_t addr = _bucketsAndMaybeMask.load(memory_order_relaxed);
    return (bucket_t *)(addr & bucketsMask);
}

mask_t cache_t::occupied() const
{
    return _occupied;
}

void cache_t::incrementOccupied() 
{
    _occupied++;
}

unsigned cache_t::capacity() const
{
    return mask() ? mask()+1 : 0; 
}

Class cache_t::cls() const
{
    return (Class)((uintptr_t)this - offsetof(objc_class, cache));
}

size_t cache_t::bytesForCapacity(uint32_t cap)
{
    return sizeof(bucket_t) * cap;
}

#if CACHE_END_MARKER

bucket_t *cache_t::endMarker(struct bucket_t *b, uint32_t cap)
{
    return (bucket_t *)((uintptr_t)b + bytesForCapacity(cap)) - 1;
}

bucket_t *cache_t::allocateBuckets(mask_t newCapacity)
{
    // Allocate one extra bucket to mark the end of the list.
    // This can't overflow mask_t because newCapacity is a power of 2.
    bucket_t *newBuckets = (bucket_t *)calloc(bytesForCapacity(newCapacity), 1);

    bucket_t *end = endMarker(newBuckets, newCapacity);

#if __arm__
    // End marker's sel is 1 and imp points BEFORE the first bucket.
    // This saves an instruction in objc_msgSend.
    end->set<NotAtomic, Raw>(newBuckets, (SEL)(uintptr_t)1, (IMP)(newBuckets - 1), nil);
#else
    // End marker's sel is 1 and imp points to the first bucket.
    end->set<NotAtomic, Raw>(newBuckets, (SEL)(uintptr_t)1, (IMP)newBuckets, nil);
#endif
    
    if (PrintCaches) recordNewCache(newCapacity);

    return newBuckets;
}

#else

bucket_t *cache_t::allocateBuckets(mask_t newCapacity)
{
    if (PrintCaches) recordNewCache(newCapacity);

    return (bucket_t *)calloc(bytesForCapacity(newCapacity), 1);
}

#endif

struct bucket_t *cache_t::emptyBuckets()
{
    return (bucket_t *)((uintptr_t)&_objc_empty_cache & bucketsMask);
}

bucket_t *cache_t::emptyBucketsForCapacity(mask_t capacity, bool allocate)
{
#if CONFIG_USE_CACHE_LOCK
    cacheUpdateLock.assertLocked();
#else
    runtimeLock.assertLocked();
#endif

    size_t bytes = bytesForCapacity(capacity);

    // Use _objc_empty_cache if the buckets is small enough.
    if (bytes <= EMPTY_BYTES) {
        return emptyBuckets();
    }

    // Use shared empty buckets allocated on the heap.
    static bucket_t **emptyBucketsList = nil;
    static mask_t emptyBucketsListCount = 0;
    
    mask_t index = log2u(capacity);

    if (index >= emptyBucketsListCount) {
        if (!allocate) return nil;

        mask_t newListCount = index + 1;
        bucket_t *newBuckets = (bucket_t *)calloc(bytes, 1);
        emptyBucketsList = (bucket_t**)
            realloc(emptyBucketsList, newListCount * sizeof(bucket_t *));
        // Share newBuckets for every un-allocated size smaller than index.
        // The array is therefore always fully populated.
        for (mask_t i = emptyBucketsListCount; i < newListCount; i++) {
            emptyBucketsList[i] = newBuckets;
        }
        emptyBucketsListCount = newListCount;

        if (PrintCaches) {
            _objc_inform("CACHES: new empty buckets at %p (capacity %zu)", 
                         newBuckets, (size_t)capacity);
        }
    }

    return emptyBucketsList[index];
}

bool cache_t::isConstantEmptyCache() const
{
    return
        occupied() == 0  &&
        buckets() == emptyBucketsForCapacity(capacity(), false);
}

bool cache_t::canBeFreed() const
{
    return !isConstantEmptyCache() && !isConstantOptimizedCache();
}

ALWAYS_INLINE
void cache_t::reallocate(mask_t oldCapacity, mask_t newCapacity, bool freeOld)
{
    bucket_t *oldBuckets = buckets();
    bucket_t *newBuckets = allocateBuckets(newCapacity);

    // Cache's old contents are not propagated. 
    // This is thought to save cache memory at the cost of extra cache fills.
    // fixme re-measure this

    ASSERT(newCapacity > 0);
    ASSERT((uintptr_t)(mask_t)(newCapacity-1) == newCapacity-1);

    setBucketsAndMask(newBuckets, newCapacity - 1);
    
    if (freeOld) {
        collect_free(oldBuckets, oldCapacity);
    }
}


void cache_t::bad_cache(id receiver, SEL sel)
{
    // Log in separate steps in case the logging itself causes a crash.
    _objc_inform_now_and_on_crash
        ("Method cache corrupted. This may be a message to an "
         "invalid object, or a memory error somewhere else.");
#if CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_OUTLINED
    bucket_t *b = buckets();
    _objc_inform_now_and_on_crash
        ("%s %p, SEL %p, isa %p, cache %p, buckets %p, "
         "mask 0x%x, occupied 0x%x", 
         receiver ? "receiver" : "unused", receiver, 
         sel, cls(), this, b,
         _maybeMask.load(memory_order_relaxed),
         _occupied);
    _objc_inform_now_and_on_crash
        ("%s %zu bytes, buckets %zu bytes", 
         receiver ? "receiver" : "unused", malloc_size(receiver), 
         malloc_size(b));
#elif (CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16 || \
       CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS || \
       CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_LOW_4)
    uintptr_t maskAndBuckets = _bucketsAndMaybeMask.load(memory_order_relaxed);
    _objc_inform_now_and_on_crash
        ("%s %p, SEL %p, isa %p, cache %p, buckets and mask 0x%lx, "
         "occupied 0x%x",
         receiver ? "receiver" : "unused", receiver,
         sel, cls(), this, maskAndBuckets, _occupied);
    _objc_inform_now_and_on_crash
        ("%s %zu bytes, buckets %zu bytes",
         receiver ? "receiver" : "unused", malloc_size(receiver),
         malloc_size(buckets()));
#else
#error Unknown cache mask storage type.
#endif
    _objc_inform_now_and_on_crash
        ("selector '%s'", sel_getName(sel));
    _objc_inform_now_and_on_crash
        ("isa '%s'", cls()->nameForLogging());
    _objc_fatal
        ("Method cache corrupted. This may be a message to an "
         "invalid object, or a memory error somewhere else.");
}

void cache_t::insert(SEL sel, IMP imp, id receiver)
{
    runtimeLock.assertLocked();

    // Never cache before +initialize is done
    if (slowpath(!cls()->isInitialized())) {
        return;
    }

    if (isConstantOptimizedCache()) {
        _objc_fatal("cache_t::insert() called with a preoptimized cache for %s",
                    cls()->nameForLogging());
    }

#if DEBUG_TASK_THREADS
    return _collecting_in_critical();
#else
#if CONFIG_USE_CACHE_LOCK
    mutex_locker_t lock(cacheUpdateLock);
#endif

    ASSERT(sel != 0 && cls()->isInitialized());

    // Use the cache as-is if until we exceed our expected fill ratio.
    mask_t newOccupied = occupied() + 1;
    unsigned oldCapacity = capacity(), capacity = oldCapacity;
    if (slowpath(isConstantEmptyCache())) {
        // Cache is read-only. Replace it.
        if (!capacity) capacity = INIT_CACHE_SIZE;
        reallocate(oldCapacity, capacity, /* freeOld */false);
    }
    else if (fastpath(newOccupied + CACHE_END_MARKER <= cache_fill_ratio(capacity))) {
        // Cache is less than 3/4 or 7/8 full. Use it as-is.
    }
#if CACHE_ALLOW_FULL_UTILIZATION
    else if (capacity <= FULL_UTILIZATION_CACHE_SIZE && newOccupied + CACHE_END_MARKER <= capacity) {
        // Allow 100% cache utilization for small buckets. Use it as-is.
    }
#endif
    else {
        capacity = capacity ? capacity * 2 : INIT_CACHE_SIZE;
        if (capacity > MAX_CACHE_SIZE) {
            capacity = MAX_CACHE_SIZE;
        }
        reallocate(oldCapacity, capacity, true);
    }

    bucket_t *b = buckets();
    mask_t m = capacity - 1;
    mask_t begin = cache_hash(sel, m);
    mask_t i = begin;

    // Scan for the first unused slot and insert there.
    // There is guaranteed to be an empty slot.
    do {
        if (fastpath(b[i].sel() == 0)) {
            incrementOccupied();
            b[i].set<Atomic, Encoded>(b, sel, imp, cls());
            return;
        }
        if (b[i].sel() == sel) {
            // The entry was added to the cache by some other thread
            // before we grabbed the cacheUpdateLock.
            return;
        }
    } while (fastpath((i = cache_next(i, m)) != begin));

    bad_cache(receiver, (SEL)sel);
#endif // !DEBUG_TASK_THREADS
}

void cache_t::copyCacheNolock(objc_imp_cache_entry *buffer, int len)
{
#if CONFIG_USE_CACHE_LOCK
    cacheUpdateLock.assertLocked();
#else
    runtimeLock.assertLocked();
#endif
    int wpos = 0;

#if CONFIG_USE_PREOPT_CACHES
    if (isConstantOptimizedCache()) {
        auto cache = preopt_cache();
        auto mask = cache->mask;
        uintptr_t sel_base = objc_opt_offsets[OBJC_OPT_METHODNAME_START];
        uintptr_t imp_base = (uintptr_t)&cache->entries;

        for (uintptr_t index = 0; index <= mask && wpos < len; index++) {
            auto &ent = cache->entries[index];
            if (~ent.sel_offs) {
                buffer[wpos].sel = (SEL)(sel_base + ent.sel_offs);
                buffer[wpos].imp = (IMP)(imp_base - ent.imp_offs);
                wpos++;
            }
        }
        return;
    }
#endif
    {
        bucket_t *buckets = this->buckets();
        uintptr_t count = capacity();

        for (uintptr_t index = 0; index < count && wpos < len; index++) {
            if (buckets[index].sel()) {
                buffer[wpos].imp = buckets[index].imp(buckets, cls());
                buffer[wpos].sel = buckets[index].sel();
                wpos++;
            }
        }
    }
}

// Reset this entire cache to the uncached lookup by reallocating it.
// This must not shrink the cache - that breaks the lock-free scheme.
void cache_t::eraseNolock(const char *func)
{
#if CONFIG_USE_CACHE_LOCK
    cacheUpdateLock.assertLocked();
#else
    runtimeLock.assertLocked();
#endif

    if (isConstantOptimizedCache()) {
        auto c = cls();
        if (PrintCaches) {
            _objc_inform("CACHES: %sclass %s: dropping and disallowing preopt cache (from %s)",
                         c->isMetaClass() ? "meta" : "",
                         c->nameForLogging(), func);
        }
        setBucketsAndMask(emptyBuckets(), 0);
        c->setDisallowPreoptCaches();
    } else if (occupied() > 0) {
        auto capacity = this->capacity();
        auto oldBuckets = buckets();
        auto buckets = emptyBucketsForCapacity(capacity);

        setBucketsAndMask(buckets, capacity - 1); // also clears occupied
        collect_free(oldBuckets, capacity);
    }
}


void cache_t::destroy()
{
#if CONFIG_USE_CACHE_LOCK
    mutex_locker_t lock(cacheUpdateLock);
#else
    runtimeLock.assertLocked();
#endif
    if (canBeFreed()) {
        if (PrintCaches) recordDeadCache(capacity());
        free(buckets());
    }
}


/***********************************************************************
* cache collection.
**********************************************************************/

#if !TARGET_OS_WIN32

// A sentinel (magic value) to report bad thread_get_state status.
// Must not be a valid PC.
// Must not be zero - thread_get_state() on a new thread returns PC == 0.
#define PC_SENTINEL  1

static uintptr_t _get_pc_for_thread(thread_t thread)
#if defined(__i386__)
{
    i386_thread_state_t state;
    unsigned int count = i386_THREAD_STATE_COUNT;
    kern_return_t okay = thread_get_state (thread, i386_THREAD_STATE, (thread_state_t)&state, &count);
    return (okay == KERN_SUCCESS) ? state.__eip : PC_SENTINEL;
}
#elif defined(__x86_64__)
{
    x86_thread_state64_t			state;
    unsigned int count = x86_THREAD_STATE64_COUNT;
    kern_return_t okay = thread_get_state (thread, x86_THREAD_STATE64, (thread_state_t)&state, &count);
    return (okay == KERN_SUCCESS) ? state.__rip : PC_SENTINEL;
}
#elif defined(__arm__)
{
    arm_thread_state_t state;
    unsigned int count = ARM_THREAD_STATE_COUNT;
    kern_return_t okay = thread_get_state (thread, ARM_THREAD_STATE, (thread_state_t)&state, &count);
    return (okay == KERN_SUCCESS) ? state.__pc : PC_SENTINEL;
}
#elif defined(__arm64__)
{
    arm_thread_state64_t state;
    unsigned int count = ARM_THREAD_STATE64_COUNT;
    kern_return_t okay = thread_get_state (thread, ARM_THREAD_STATE64, (thread_state_t)&state, &count);
    return (okay == KERN_SUCCESS) ? (uintptr_t)arm_thread_state64_get_pc(state) : PC_SENTINEL;
}
#else
{
#error _get_pc_for_thread () not implemented for this architecture
}
#endif

#endif

/***********************************************************************
* _collecting_in_critical.
* Returns TRUE if some thread is currently executing a cache-reading 
* function. Collection of cache garbage is not allowed when a cache-
* reading function is in progress because it might still be using 
* the garbage memory.
**********************************************************************/
#if HAVE_TASK_RESTARTABLE_RANGES
#include <kern/restartable.h>
#else
typedef struct {
    uint64_t          location;
    unsigned short    length;
    unsigned short    recovery_offs;
    unsigned int      flags;
} task_restartable_range_t;
#endif

extern "C" task_restartable_range_t objc_restartableRanges[];

#if HAVE_TASK_RESTARTABLE_RANGES
static bool shouldUseRestartableRanges = true;
#endif

void cache_t::init()
{
#if HAVE_TASK_RESTARTABLE_RANGES
    mach_msg_type_number_t count = 0;
    kern_return_t kr;

    while (objc_restartableRanges[count].location) {
        count++;
    }

    kr = task_restartable_ranges_register(mach_task_self(),
                                          objc_restartableRanges, count);
    if (kr == KERN_SUCCESS) return;
    _objc_fatal("task_restartable_ranges_register failed (result 0x%x: %s)",
                kr, mach_error_string(kr));
#endif // HAVE_TASK_RESTARTABLE_RANGES
}

static int _collecting_in_critical(void)
{
#if TARGET_OS_WIN32
    return TRUE;
#elif HAVE_TASK_RESTARTABLE_RANGES
    // Only use restartable ranges if we registered them earlier.
    if (shouldUseRestartableRanges) {
        kern_return_t kr = task_restartable_ranges_synchronize(mach_task_self());
        if (kr == KERN_SUCCESS) return FALSE;
        _objc_fatal("task_restartable_ranges_synchronize failed (result 0x%x: %s)",
                    kr, mach_error_string(kr));
    }
#endif // !HAVE_TASK_RESTARTABLE_RANGES

    // Fallthrough if we didn't use restartable ranges.

    thread_act_port_array_t threads;
    unsigned number;
    unsigned count;
    kern_return_t ret;
    int result;

    mach_port_t mythread = pthread_mach_thread_np(objc_thread_self());

    // Get a list of all the threads in the current task
#if !DEBUG_TASK_THREADS
    ret = task_threads(mach_task_self(), &threads, &number);
#else
    ret = objc_task_threads(mach_task_self(), &threads, &number);
#endif

    if (ret != KERN_SUCCESS) {
        // See DEBUG_TASK_THREADS below to help debug this.
        _objc_fatal("task_threads failed (result 0x%x)\n", ret);
    }

    // Check whether any thread is in the cache lookup code
    result = FALSE;
    for (count = 0; count < number; count++)
    {
        int region;
        uintptr_t pc;

        // Don't bother checking ourselves
        if (threads[count] == mythread)
            continue;

        // Find out where thread is executing
#if TARGET_OS_OSX
//        if (oah_is_current_process_translated()) {
//            kern_return_t ret = objc_thread_get_rip(threads[count], (uint64_t*)&pc);
//            if (ret != KERN_SUCCESS) {
//                pc = PC_SENTINEL;
//            }
//        } else {
//            pc = _get_pc_for_thread (threads[count]);
//        }
#else
        pc = _get_pc_for_thread (threads[count]);
#endif

        // Check for bad status, and if so, assume the worse (can't collect)
        if (pc == PC_SENTINEL)
        {
            result = TRUE;
            goto done;
        }
        
        // Check whether it is in the cache lookup code
        for (region = 0; objc_restartableRanges[region].location != 0; region++)
        {
            uint64_t loc = objc_restartableRanges[region].location;
            if ((pc > loc) &&
                (pc - loc < (uint64_t)objc_restartableRanges[region].length))
            {
                result = TRUE;
                goto done;
            }
        }
    }

 done:
    // Deallocate the port rights for the threads
    for (count = 0; count < number; count++) {
        mach_port_deallocate(mach_task_self (), threads[count]);
    }

    // Deallocate the thread list
    vm_deallocate (mach_task_self (), (vm_address_t) threads, sizeof(threads[0]) * number);

    // Return our finding
    return result;
}


/***********************************************************************
* _garbage_make_room.  Ensure that there is enough room for at least
* one more ref in the garbage.
**********************************************************************/

// amount of memory represented by all refs in the garbage
static size_t garbage_byte_size = 0;

// do not empty the garbage until garbage_byte_size gets at least this big
static size_t garbage_threshold = 32*1024;

// table of refs to free
static bucket_t **garbage_refs = 0;

// current number of refs in garbage_refs
static size_t garbage_count = 0;

// capacity of current garbage_refs
static size_t garbage_max = 0;

// capacity of initial garbage_refs
enum {
    INIT_GARBAGE_COUNT = 128
};

static void _garbage_make_room(void)
{
    static int first = 1;

    // Create the collection table the first time it is needed
    if (first)
    {
        first = 0;
        garbage_refs = (bucket_t**)
            malloc(INIT_GARBAGE_COUNT * sizeof(void *));
        garbage_max = INIT_GARBAGE_COUNT;
    }

    // Double the table if it is full
    else if (garbage_count == garbage_max)
    {
        garbage_refs = (bucket_t**)
            realloc(garbage_refs, garbage_max * 2 * sizeof(void *));
        garbage_max *= 2;
    }
}


/***********************************************************************
* cache_t::collect_free.  Add the specified malloc'd memory to the list
* of them to free at some later point.
* size is used for the collection threshold. It does not have to be 
* precisely the block's size.
* Cache locks: cacheUpdateLock must be held by the caller.
**********************************************************************/
void cache_t::collect_free(bucket_t *data, mask_t capacity)
{
#if CONFIG_USE_CACHE_LOCK
    cacheUpdateLock.assertLocked();
#else
    runtimeLock.assertLocked();
#endif

    if (PrintCaches) recordDeadCache(capacity);

    _garbage_make_room ();
    garbage_byte_size += cache_t::bytesForCapacity(capacity);
    garbage_refs[garbage_count++] = data;
    cache_t::collectNolock(false);
}


/***********************************************************************
* cache_collect.  Try to free accumulated dead caches.
* collectALot tries harder to free memory.
* Cache locks: cacheUpdateLock must be held by the caller.
**********************************************************************/
void cache_t::collectNolock(bool collectALot)
{
#if CONFIG_USE_CACHE_LOCK
    cacheUpdateLock.assertLocked();
#else
    runtimeLock.assertLocked();
#endif

    // Done if the garbage is not full
    if (garbage_byte_size < garbage_threshold  &&  !collectALot) {
        return;
    }

    // Synchronize collection with objc_msgSend and other cache readers
    if (!collectALot) {
        if (_collecting_in_critical ()) {
            // objc_msgSend (or other cache reader) is currently looking in
            // the cache and might still be using some garbage.
            if (PrintCaches) {
                _objc_inform ("CACHES: not collecting; "
                              "objc_msgSend in progress");
            }
            return;
        }
    } 
    else {
        // No excuses.
        while (_collecting_in_critical()) 
            ;
    }

    // No cache readers in progress - garbage is now deletable

    // Log our progress
    if (PrintCaches) {
        cache_collections++;
        _objc_inform ("CACHES: COLLECTING %zu bytes (%zu allocations, %zu collections)", garbage_byte_size, cache_allocations, cache_collections);
    }
    
    // Dispose all refs now in the garbage
    // Erase each entry so debugging tools don't see stale pointers.
    while (garbage_count--) {
        auto dead = garbage_refs[garbage_count];
        garbage_refs[garbage_count] = nil;
        free(dead);
    }
    
    // Clear the garbage count and total size indicator
    garbage_count = 0;
    garbage_byte_size = 0;

    if (PrintCaches) {
        size_t i;
        size_t total_count = 0;
        size_t total_size = 0;

        for (i = 0; i < countof(cache_counts); i++) {
            int count = cache_counts[i];
            int slots = 1 << i;
            size_t size = count * slots * sizeof(bucket_t);

            if (!count) continue;

            _objc_inform("CACHES: %4d slots: %4d caches, %6zu bytes", 
                         slots, count, size);

            total_count += count;
            total_size += size;
        }

        _objc_inform("CACHES:      total: %4zu caches, %6zu bytes", 
                     total_count, total_size);
    }
}


/***********************************************************************
* objc_task_threads
* Replacement for task_threads(). Define DEBUG_TASK_THREADS to debug 
* crashes when task_threads() is failing.
*
* A failure in task_threads() usually means somebody has botched their 
* Mach or MIG traffic. For example, somebody's error handling was wrong 
* and they left a message queued on the MIG reply port for task_threads() 
* to trip over.
*
* The code below is a modified version of task_threads(). It logs 
* the msgh_id of the reply message. The msgh_id can identify the sender 
* of the message, which can help pinpoint the faulty code.
* DEBUG_TASK_THREADS also calls collecting_in_critical() during every 
* message dispatch, which can increase reproducibility of bugs.
*
* This code can be regenerated by running 
* `mig /usr/include/mach/task.defs`.
**********************************************************************/
#if DEBUG_TASK_THREADS

#include <mach/mach.h>
#include <mach/message.h>
#include <mach/mig.h>

#define __MIG_check__Reply__task_subsystem__ 1
#define mig_internal static inline
#define __DeclareSendRpc(a, b)
#define __BeforeSendRpc(a, b)
#define __AfterSendRpc(a, b)
#define msgh_request_port       msgh_remote_port
#define msgh_reply_port         msgh_local_port

#ifndef __MachMsgErrorWithTimeout
#define __MachMsgErrorWithTimeout(_R_) { \
        switch (_R_) { \
        case MACH_SEND_INVALID_DATA: \
        case MACH_SEND_INVALID_DEST: \
        case MACH_SEND_INVALID_HEADER: \
            mig_put_reply_port(InP->Head.msgh_reply_port); \
            break; \
        case MACH_SEND_TIMED_OUT: \
        case MACH_RCV_TIMED_OUT: \
        default: \
            mig_dealloc_reply_port(InP->Head.msgh_reply_port); \
        } \
    }
#endif  /* __MachMsgErrorWithTimeout */

#ifndef __MachMsgErrorWithoutTimeout
#define __MachMsgErrorWithoutTimeout(_R_) { \
        switch (_R_) { \
        case MACH_SEND_INVALID_DATA: \
        case MACH_SEND_INVALID_DEST: \
        case MACH_SEND_INVALID_HEADER: \
            mig_put_reply_port(InP->Head.msgh_reply_port); \
            break; \
        default: \
            mig_dealloc_reply_port(InP->Head.msgh_reply_port); \
        } \
    }
#endif  /* __MachMsgErrorWithoutTimeout */


#if ( __MigTypeCheck )
#if __MIG_check__Reply__task_subsystem__
#if !defined(__MIG_check__Reply__task_threads_t__defined)
#define __MIG_check__Reply__task_threads_t__defined

mig_internal kern_return_t __MIG_check__Reply__task_threads_t(__Reply__task_threads_t *Out0P)
{

	typedef __Reply__task_threads_t __Reply;
	boolean_t msgh_simple;
#if	__MigTypeCheck
	unsigned int msgh_size;
#endif	/* __MigTypeCheck */
	if (Out0P->Head.msgh_id != 3502) {
	    if (Out0P->Head.msgh_id == MACH_NOTIFY_SEND_ONCE)
		{ return MIG_SERVER_DIED; }
	    else
		{ return MIG_REPLY_MISMATCH; }
	}

	msgh_simple = !(Out0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX);
#if	__MigTypeCheck
	msgh_size = Out0P->Head.msgh_size;

	if ((msgh_simple || Out0P->msgh_body.msgh_descriptor_count != 1 ||
	    msgh_size != (mach_msg_size_t)sizeof(__Reply)) &&
	    (!msgh_simple || msgh_size != (mach_msg_size_t)sizeof(mig_reply_error_t) ||
	    ((mig_reply_error_t *)Out0P)->RetCode == KERN_SUCCESS))
		{ return MIG_TYPE_ERROR ; }
#endif	/* __MigTypeCheck */

	if (msgh_simple) {
		return ((mig_reply_error_t *)Out0P)->RetCode;
	}

#if	__MigTypeCheck
	if (Out0P->act_list.type != MACH_MSG_OOL_PORTS_DESCRIPTOR ||
	    Out0P->act_list.disposition != 17) {
		return MIG_TYPE_ERROR;
	}
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Reply__task_threads_t__defined) */
#endif /* __MIG_check__Reply__task_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine task_threads */
static kern_return_t objc_task_threads
(
	task_t target_task,
	thread_act_array_t *act_list,
	mach_msg_type_number_t *act_listCnt
)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_ool_ports_descriptor_t act_list;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_msg_type_number_t act_listCnt;
		mach_msg_trailer_t trailer;
	} Reply;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_ool_ports_descriptor_t act_list;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_msg_type_number_t act_listCnt;
	} __Reply;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	union {
		Request In;
		Reply Out;
	} Mess;

	Request *InP = &Mess.In;
	Reply *Out0P = &Mess.Out;

	mach_msg_return_t msg_result;

#ifdef	__MIG_check__Reply__task_threads_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Reply__task_threads_t__defined */

	__DeclareSendRpc(3402, "task_threads")

	InP->Head.msgh_bits =
		MACH_MSGH_BITS(19, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	/* msgh_size passed as argument */
	InP->Head.msgh_request_port = target_task;
	InP->Head.msgh_reply_port = mig_get_reply_port();
	InP->Head.msgh_id = 3402;

	__BeforeSendRpc(3402, "task_threads")
	msg_result = mach_msg(&InP->Head, MACH_SEND_MSG|MACH_RCV_MSG|MACH_MSG_OPTION_NONE, (mach_msg_size_t)sizeof(Request), (mach_msg_size_t)sizeof(Reply), InP->Head.msgh_reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	__AfterSendRpc(3402, "task_threads")
	if (msg_result != MACH_MSG_SUCCESS) {
		_objc_inform("task_threads received unexpected reply msgh_id 0x%zx", 
                             (size_t)Out0P->Head.msgh_id);
		__MachMsgErrorWithoutTimeout(msg_result);
		{ return msg_result; }
	}


#if	defined(__MIG_check__Reply__task_threads_t__defined)
	check_result = __MIG_check__Reply__task_threads_t((__Reply__task_threads_t *)Out0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ return check_result; }
#endif	/* defined(__MIG_check__Reply__task_threads_t__defined) */

	*act_list = (thread_act_array_t)(Out0P->act_list.address);
	*act_listCnt = Out0P->act_listCnt;

	return KERN_SUCCESS;
}

// DEBUG_TASK_THREADS
#endif

OBJC_EXPORT bucket_t * objc_cache_buckets(const cache_t * cache) {
    return cache->buckets();
}

#if CONFIG_USE_PREOPT_CACHES

OBJC_EXPORT const preopt_cache_t * _Nonnull objc_cache_preoptCache(const cache_t * _Nonnull cache) {
    return cache->preopt_cache();
}

OBJC_EXPORT bool objc_cache_isConstantOptimizedCache(const cache_t * _Nonnull cache, bool strict, uintptr_t empty_addr) {
    return cache->isConstantOptimizedCache(strict, empty_addr);
}

OBJC_EXPORT unsigned objc_cache_preoptCapacity(const cache_t * _Nonnull cache) {
    return cache->preopt_cache()->capacity();
}

OBJC_EXPORT Class _Nonnull objc_cache_preoptFallbackClass(const cache_t * _Nonnull cache) {
    return cache->preoptFallbackClass();
}

#endif

OBJC_EXPORT size_t objc_cache_bytesForCapacity(uint32_t cap) {
    return cache_t::bytesForCapacity(cap);
}

OBJC_EXPORT uint32_t objc_cache_occupied(const cache_t * _Nonnull cache) {
    return cache->occupied();
}

OBJC_EXPORT unsigned objc_cache_capacity(const struct cache_t * _Nonnull cache) {
    return cache->capacity();
}

// __OBJC2__
#endif
