//
//  magazine_rack.c
//  libmalloc
//
//  Created by Matt Wright on 8/29/16.
//
//

#include <darwintest.h>
#include "magazine_testing.h"

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

T_DECL(basic_magazine_init, "allocate magazine counts")
{
	struct rack_s rack;

	for (int i=1; i < 64; i++) {
		memset(&rack, 'a', sizeof(rack));
		rack_init(&rack, RACK_TYPE_NONE, i, 0);
		T_ASSERT_NOTNULL(rack.magazines, "%d magazine initialisation", i);
	}
}

T_DECL(basic_magazine_deinit, "allocate deallocate magazines")
{
	struct rack_s rack;
	memset(&rack, 'a', sizeof(rack));

	rack_init(&rack, RACK_TYPE_NONE, 1, 0);
	T_ASSERT_NOTNULL(rack.magazines, "magazine init");

	rack_destroy(&rack);
	T_ASSERT_NULL(rack.magazines, "magazine deinit");
}

void *
pressure_thread(void *arg)
{
	T_LOG("pressure thread started\n");
	while (1) {
		malloc_zone_pressure_relief(0, 0);
	}
}

void *
thread(void *arg)
{
	uintptr_t sz = (uintptr_t)arg;
	T_LOG("thread started (allocation size: %lu bytes)\n", sz);
	void *temp = malloc(sz);

	uint64_t c = 100;
	while (c-- > 0) {
		uint32_t num = arc4random_uniform(100000);
		void **allocs = malloc(sizeof(void *) * num);

		for (int i=0; i<num; i++) {
			allocs[i] = malloc(sz);
		}
		for (int i=0; i<num; i++) {
			free(allocs[num - 1 - i]);
		}
		free((void *)allocs);
	}
	free(temp);
	return NULL;
}

T_DECL(rack_tiny_region_remove, "exercise region deallocation race (rdar://66713029)")
{
	pthread_t p1;
	pthread_create(&p1, NULL, pressure_thread, NULL);

	const int threads = 8;
	pthread_t p[threads];

	for (int i=0; i<threads; i++) {
		pthread_create(&p[i], NULL, thread, (void *)128);
	}
	for (int i=0; i<threads; i++) {
		pthread_join(p[i], NULL);
	}
	T_PASS("finished without crashing");
}

T_DECL(rack_small_region_remove, "exercise region deallocation race (rdar://66713029)")
{
	pthread_t p1;
	pthread_create(&p1, NULL, pressure_thread, NULL);

	const int threads = 8;
	pthread_t p[threads];

	for (int i=0; i<threads; i++) {
		pthread_create(&p[i], NULL, thread, (void *)1024);
	}
	for (int i=0; i<threads; i++) {
		pthread_join(p[i], NULL);
	}
	T_PASS("finished without crashing");
}

T_DECL(rack_medium_region_remove, "exercise region deallocation race (rdar://66713029)",
	   T_META_ENVVAR("MallocMediumZone=1"),
	   T_META_ENVVAR("MallocMediumActivationThreshold=1"),
	   T_META_ENABLED(CONFIG_MEDIUM_ALLOCATOR))
{
	pthread_t p1;
	pthread_create(&p1, NULL, pressure_thread, NULL);

	const int threads = 8;
	pthread_t p[threads];

	for (int i=0; i<threads; i++) {
		pthread_create(&p[i], NULL, thread, (void *)65536);
	}
	for (int i=0; i<threads; i++) {
		pthread_join(p[i], NULL);
	}
	T_PASS("finished without crashing");
}
