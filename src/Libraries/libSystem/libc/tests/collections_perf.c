/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
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

#import <TargetConditionals.h>
#include <os/collections.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <darwintest.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <libproc.h>
#include <sys/fcntl.h>


// 11400714819323198485 is coprime with 2^64, so this will touch every number
// before looping. It's also the closest number to 2^64 / the golden ratio, so
// it will give a pretty even distribution
static inline uint64_t _seq_next(uint64_t prev) {
	return prev + 11400714819323198485llu;
}

#define MAPS_COUNT 4096
#define ACTIONS_PER_MEASUREMENT_COUNT 512
#define INSERT_TO_FIND_RATIO 64
#define TOTAL_ITERATIONS 16

#define PERF_OUTPUT_FILENAME "/tmp/libcollection_perf_data.csv"

// This is fairly arbitrary, and just makes sure that any value bias is fairly
// accounted for
static inline uint64_t _value_for_key(uint64_t key) {
	return key ^ 0xFFFF0000FF0000FF;
}

static uint64_t _insert_n_entries_to_maps(os_map_64_t *maps, uint64_t seed) {
	uint64_t current_seq = seed;
	for ( int i = 0; i < ACTIONS_PER_MEASUREMENT_COUNT; i++){
		void *value = (void *)_value_for_key(current_seq);
		for (int j = 0; j < MAPS_COUNT; j++) {
			os_map_insert(&maps[j], current_seq, value);
		}
		current_seq = _seq_next(current_seq);
	}
	return current_seq;
}

static uint64_t _find_n_entries_in_maps(os_map_64_t *maps, uint64_t seed) {
	uint64_t current_seq = seed;
	for ( int i = 0; i < ACTIONS_PER_MEASUREMENT_COUNT; i++){
		for (int j = 0; j < MAPS_COUNT; j++) {
			(void)os_map_find(&maps[j], current_seq);
		}
		current_seq = _seq_next(current_seq);
	}
	return current_seq;
}

uint64_t absoluteTimeFromMachTime(uint64_t machTime) {
    // get the mach timescale
    static double __TimeScale = 0.0;
    if (__TimeScale == 0.0) {
        struct mach_timebase_info info;
        if (mach_timebase_info(&info) == KERN_SUCCESS) {
            __TimeScale = ((double)info.numer / (double)info.denom);
        }
    }
    return (uint64_t)(machTime * __TimeScale);
}

static uint64_t
_get_dirtymemory()
{
    int rval = 0;
    struct rusage_info_v4 rusage;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t)&rusage)) {
	    rval = errno;
	    T_FAIL("rusage failed with %d", rval);
	    return 0;
    } else {
	    return rusage.ri_phys_footprint;
    }
}

T_DECL(map_perf, "Test map performance for small hash maps",
		T_META("owner", "Core Darwin Daemons & Tools"),
		T_META_CHECK_LEAKS(false),
		T_META_ASROOT(true))
{
#if !(TARGET_OS_IOS | TARGET_OS_OSX)
	T_PASS("Map_perf doesn't run on this platform");
	return;
#endif

	os_map_64_t perf_maps[MAPS_COUNT];

	for (int i = 0; i < MAPS_COUNT; i++) {
		os_map_init(&perf_maps[i], NULL);
	}

	uint64_t current_seq = _seq_next(0);


	int output_fd = creat(PERF_OUTPUT_FILENAME, S_IWUSR);
	assert(output_fd);
	FILE *output_file = fdopen(output_fd, "w");
	assert(output_file);

	fprintf(output_file,
		"MAP_SIZE,INSERT_TIME,FIND_TIME,FIND_MISSING_TIME,MEM_USAGE\n");

	uint64_t baseline_memory_usage = _get_dirtymemory();
	T_LOG("Baseline memory usage is %llu", baseline_memory_usage);

	for (int i = 0; i < TOTAL_ITERATIONS; i++) {
		uint64_t map_size = ACTIONS_PER_MEASUREMENT_COUNT * (i + 1);
		T_LOG("Starting performance testing with map size %llu",
		      map_size);

		uint64_t insert_start_mach_time = mach_absolute_time();
		uint64_t next_seq = _insert_n_entries_to_maps(perf_maps,
							      current_seq);
		uint64_t insert_end_mach_time = mach_absolute_time();

		uint64_t insert_start_time = absoluteTimeFromMachTime(insert_start_mach_time);
		uint64_t insert_end_time = absoluteTimeFromMachTime(insert_end_mach_time);

		T_LOG("Insertions complete, with start time %llu, end time %llu",
		      insert_start_time, insert_end_time);

		uint64_t insert_average_time = ((insert_end_time - insert_start_time)) /
			(MAPS_COUNT * ACTIONS_PER_MEASUREMENT_COUNT);

		T_LOG("DATA: INSERTION TIME %llu, %llu", map_size,
		      insert_average_time);

		uint64_t find_start_mach_time = mach_absolute_time();
		for (int j = 0; j < INSERT_TO_FIND_RATIO; j++) {
			_find_n_entries_in_maps(perf_maps, current_seq);
		}
		uint64_t find_end_mach_time = mach_absolute_time();

		uint64_t find_start_time = absoluteTimeFromMachTime(find_start_mach_time);
		uint64_t find_end_time = absoluteTimeFromMachTime(find_end_mach_time);

		T_LOG("Finds complete, with start time %llu, end time %llu",
		      find_start_time, find_end_time);

		uint64_t find_average_time = ((find_end_time - find_start_time)) /
			(MAPS_COUNT * ACTIONS_PER_MEASUREMENT_COUNT * INSERT_TO_FIND_RATIO);

		T_LOG("DATA: FIND TIME FOR %llu, %llu", map_size,
		      find_average_time);

		uint64_t no_find_start_mach_time = mach_absolute_time();
		for (int j = 0; j < INSERT_TO_FIND_RATIO; j++) {
			_find_n_entries_in_maps(perf_maps, next_seq);
		}
		uint64_t no_find_end_mach_time = mach_absolute_time();

		uint64_t no_find_start_time = absoluteTimeFromMachTime(no_find_start_mach_time);
		uint64_t no_find_end_time = absoluteTimeFromMachTime(no_find_end_mach_time);

		T_LOG("Find not present complete, with start time %llu, end time %llu",
		      no_find_start_time, no_find_end_time);

		uint64_t no_find_average_time = ((no_find_end_time - no_find_start_time)) /
			(MAPS_COUNT * ACTIONS_PER_MEASUREMENT_COUNT * INSERT_TO_FIND_RATIO);

		T_LOG("DATA: NO-FIND TIME FOR %llu, %llu",
		      map_size, no_find_average_time);


		uint64_t latest_memory_usage = _get_dirtymemory();
		T_LOG("New memory usage is %llu", latest_memory_usage);

		uint64_t average_memory_usage = latest_memory_usage / MAPS_COUNT;
		T_LOG("DATA: MEMORY USAGE FOR %llu, %llu", map_size,
		      average_memory_usage);

		fprintf(output_file, "%llu,%llu,%llu,%llu,%llu\n", map_size,
			insert_average_time, find_average_time,
			no_find_average_time, average_memory_usage);

		current_seq = next_seq;
	}

	fclose(output_file);
	close(output_fd);

	T_PASS("Finished, output generated at: " PERF_OUTPUT_FILENAME);
}
