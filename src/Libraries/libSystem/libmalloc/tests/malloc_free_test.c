//
//  malloc_free_test.c
//  libmalloc
//
//  test allocating and freeing all sizes
//

#include <darwintest.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc/malloc.h>
#include "../src/internal.h"

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

static inline void*
t_malloc(size_t s)
{
	void *ptr = malloc(s);
	T_QUIET; T_ASSERT_NOTNULL(ptr, "allocation");
	size_t sz = malloc_size(ptr);
	T_QUIET; T_EXPECT_LE(s, sz, "allocation size");
	const uint64_t pat = 0xdeadbeefcafebabeull;
	memset_pattern8(ptr, &pat, s);
	return ptr;
}

static void
test_malloc_free(size_t min, size_t max, size_t incr)
{
	for (size_t s =  min; s <= max; s += incr) {
		void *ptr = t_malloc(s);
		free(ptr); // try to go through mag_last_free SMALL_CACHE
	}
	for (size_t s = min, t = max; s <= max; s += incr, t -= incr) {
		void *ptr1 = t_malloc(s);
		void *ptr2 = t_malloc(t);
		free(ptr1); // try to defeat mag_last_free SMALL_CACHE
		free(ptr2);
	}
}

static void
test_malloc_free_random(size_t min, size_t max, size_t incr, size_t n)
{
	const size_t r = (max - min) / incr, P = 100;
	void *ptrs[P] = {};
	for (size_t i = 0, j = 0, k = 0; i < n + P; i++, j = k, k = (k + 1) % P) {
		void *ptr = NULL;
		if (i < n) ptr = t_malloc(min + arc4random_uniform(r) * incr);
		free(ptrs[j]);
		ptrs[k] = ptr;
	}
}

T_DECL(malloc_free_nano, "nanomalloc and free all sizes <= 256",
	   T_META_ENVVAR("MallocNanoZone=1"))
{
	test_malloc_free(0, 256, 1); // NANO_MAX_SIZE
	test_malloc_free_random(0, 256, 1, 10000);
}

T_DECL(malloc_free_tiny, "tiny malloc and free 16b increments <= 1008",
	   T_META_ENVVAR("MallocNanoZone=0"))
{
	test_malloc_free(0, TINY_LIMIT_THRESHOLD, 16);
	test_malloc_free_random(0, TINY_LIMIT_THRESHOLD, 16, 10000);
}

T_DECL(malloc_free, "malloc and free all 512b increments <= 256kb",
	   T_META_ENVVAR("MallocNanoZone=0"))
{
	test_malloc_free(1024, 256 * 1024, 512); // > LARGE_THRESHOLD_LARGEMEM
	test_malloc_free_random(1024, 256 * 1024, 512, 100000);
}

T_DECL(malloc_free_medium, "medium malloc and free all 32kb increments <= 8mb",
	   T_META_ENVVAR("MallocMediumZone=1"),
	   T_META_ENVVAR("MallocMediumActivationThreshold=1"),
	   T_META_ENABLED(CONFIG_MEDIUM_ALLOCATOR))
{
	test_malloc_free(SMALL_LIMIT_THRESHOLD, MEDIUM_LIMIT_THRESHOLD, 32 * 1024);
	test_malloc_free_random(SMALL_LIMIT_THRESHOLD, MEDIUM_LIMIT_THRESHOLD, 32 * 1024, 1000);
}

#pragma mark MallocAggressiveMadvise=1

T_DECL(malloc_free_tiny_aggressive_madvise, "tiny malloc and free all 16b increments with aggressive madvise",
	   T_META_ENVVAR("MallocNanoZone=0"),
	   T_META_ENVVAR("MallocAggressiveMadvise=1"),
	   T_META_ENABLED(CONFIG_AGGRESSIVE_MADVISE))
{
	test_malloc_free(0, TINY_LIMIT_THRESHOLD, 16);
	test_malloc_free_random(0, TINY_LIMIT_THRESHOLD, 16, 10000);
}

T_DECL(malloc_free_small_aggressive_madvise, "small malloc and free all 512b with aggressive madvise",
	   T_META_ENVVAR("MallocNanoZone=0"),
	   T_META_ENVVAR("MallocAggressiveMadvise=1"),
	   T_META_ENABLED(CONFIG_AGGRESSIVE_MADVISE))
{
	test_malloc_free(TINY_LIMIT_THRESHOLD, SMALL_LIMIT_THRESHOLD, 512);
	test_malloc_free_random(TINY_LIMIT_THRESHOLD, SMALL_LIMIT_THRESHOLD, 512, 100000);
}

T_DECL(malloc_free_medium_aggressive_madvise, "medium malloc and free all 32kb increments with aggressive madvise",
	   T_META_ENVVAR("MallocMediumZone=1"),
	   T_META_ENVVAR("MallocAggressiveMadvise=1"),
	   T_META_ENVVAR("MallocMediumActivationThreshold=1"),
	   T_META_ENABLED(CONFIG_MEDIUM_ALLOCATOR),
	   T_META_ENABLED(CONFIG_AGGRESSIVE_MADVISE))
{
	test_malloc_free(SMALL_LIMIT_THRESHOLD, MEDIUM_LIMIT_THRESHOLD, 32 * 1024);
	test_malloc_free_random(SMALL_LIMIT_THRESHOLD, MEDIUM_LIMIT_THRESHOLD, 32 * 1024, 1000);
}

#pragma mark MallocLargeCache=0

T_DECL(malloc_free_large_no_cache, "large malloc and free 1mb increments of first 8mb with large cache disabled",
	   T_META_ENVVAR("MallocLargeCache=0"),
	   T_META_ENABLED(CONFIG_LARGE_CACHE))
{
	test_malloc_free(MEDIUM_LIMIT_THRESHOLD, MEDIUM_LIMIT_THRESHOLD + (8 * 1024 * 1024), 1024 * 1024);
	test_malloc_free_random(MEDIUM_LIMIT_THRESHOLD, MEDIUM_LIMIT_THRESHOLD + (8 * 1024 * 1024), 1024 * 1024, 1000);
}

#pragma mark MallocSpaceEfficient=1

T_DECL(malloc_free_space_efficient, "malloc and free all 512b increments <= 256kb with MallocSpaceEfficient=1",
	   T_META_ENVVAR("MallocNanoZone=0"),
	   T_META_ENVVAR("MallocSpaceEfficient=1"),
	   T_META_ENABLED(CONFIG_AGGRESSIVE_MADVISE))
{
	test_malloc_free(0, 256 * 1024, 512); // > LARGE_THRESHOLD_LARGEMEM
	test_malloc_free_random(0, 256 * 1024, 512, 100000);
}

T_DECL(malloc_free_large_space_efficient, "large malloc and free 1mb increments of first 8mb with MallocSpaceEfficient=1",
	   T_META_ENVVAR("MallocSpaceEfficient=1"),
	   T_META_ENABLED(CONFIG_LARGE_CACHE))
{
	test_malloc_free(MEDIUM_LIMIT_THRESHOLD, MEDIUM_LIMIT_THRESHOLD + (8 * 1024 * 1024), 1024 * 1024);
	test_malloc_free_random(MEDIUM_LIMIT_THRESHOLD, MEDIUM_LIMIT_THRESHOLD + (8 * 1024 * 1024), 1024 * 1024, 1000);
}
