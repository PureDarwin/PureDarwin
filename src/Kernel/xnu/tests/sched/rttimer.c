// Copyright (c) 2024 Apple Inc.  All rights reserved.

#include <libproc_internal.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/thread_info.h>
#include <mach/thread_policy.h>
#include <os/workgroup_private.h>
#include <os/workgroup.h>
#include <pthread.h>
#include <pthread/workgroup_private.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/kdebug.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <darwintest.h>
#include <darwintest_utils.h>
#include <TargetConditionals.h>
#include "../test_utils.h"
#include "sched_test_utils.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.scheduler"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("scheduler")
	);

static const uint32_t CALIBRATION_CYCLES = 10000;

uint64_t waitStart = 0ULL;
uint64_t waitEnd = 8ULL * NSEC_PER_MSEC;
#if TARGET_OS_WATCH || TARGET_OS_TV
/* Increase step stride for slower APs. */
uint64_t waitStep = 2000ULL * NSEC_PER_USEC;
#else /* TARGET_OS_WATCH || TARGET_OS_TV */
uint64_t waitStep = 500ULL * NSEC_PER_USEC;
#endif /* TARGET_OS_WATCH || TARGET_OS_TV */
uint64_t testDuration = 5ULL * NSEC_PER_SEC;
uint64_t wasteCPUThreads = 0ULL;
uint64_t wasteRTCPUThreads = 0ULL;
uint64_t wasteCPUTimeQuanta = 10ULL * NSEC_PER_MSEC;
uint64_t wasteCPUTimePercentActive = 50ULL;
uint64_t wasteCPUTimeQuantaRandomVariationPercent = 50ULL;
uint32_t rtPolicyPeriod = 0ULL * USEC_PER_SEC;
uint64_t rtPolicyComputation = 5ULL * USEC_PER_SEC;
uint64_t rtPolicyConstraint = 10ULL * USEC_PER_SEC;
bool     rtPolicyPreemptible = false;

/* Workgroup (for CLPC, and required to get RT on visionOS)  */
os_workgroup_t g_rt_workgroup = NULL;
os_workgroup_join_token_s g_rt_workgroup_join_token = { 0 };

static const char workload_config_plist[] = {
#embed "rttimer.workload_config.plist" suffix(,)
	0,
};


static void
workload_config_load(void)
{
	/* Try to load the test workload config plist. */
	size_t len = 0;
	int ret = sysctlbyname("kern.workload_config", NULL, &len, (void*) (const void*) workload_config_plist, strlen(workload_config_plist));
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctlbyname(kern.workload_config)");
}

static void
workload_config_unload(void)
{
	/* clear the loaded workload config plist.. */
	size_t len = 0;
	sysctlbyname("kern.workload_config", NULL, &len, "", 1);
}

static void
setup_workgroup(void)
{
	int ret;
	/* Create a named workgroup. */
	os_workgroup_attr_s attr = OS_WORKGROUP_ATTR_INITIALIZER_DEFAULT;
	ret = os_workgroup_attr_set_flags(&attr, OS_WORKGROUP_ATTR_NONPROPAGATING);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "os_workgroup_set_flags(OS_WORKGROUP_ATTR_NONPROPAGATING)");
	g_rt_workgroup = os_workgroup_create_with_workload_id("rttimer", "com.apple.test", &attr);
	T_QUIET; T_ASSERT_NOTNULL(g_rt_workgroup, "created the test workgroup");
}

static thread_basic_info_data_t
thread_info_get()
{
	thread_basic_info_data_t value;
	mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
	thread_info(pthread_mach_thread_np(pthread_self()), THREAD_BASIC_INFO, (thread_info_t)&value, &info_count);
	return value;
}


static void
make_realtime()
{
	thread_time_constraint_policy_data_t policy;
	policy.period      = (uint32_t)(nanos_to_abs(rtPolicyPeriod));
	policy.computation = (uint32_t)(nanos_to_abs(rtPolicyComputation));
	policy.constraint  = (uint32_t)(nanos_to_abs(rtPolicyConstraint));
	policy.preemptible = rtPolicyPreemptible;

	int ret = thread_policy_set(
		pthread_mach_thread_np(pthread_self()),
		THREAD_TIME_CONSTRAINT_POLICY,
		(thread_policy_t)&policy,
		THREAD_TIME_CONSTRAINT_POLICY_COUNT);
	T_QUIET; T_ASSERT_MACH_SUCCESS(ret, "thread_policy_set self to realtime");
}

static void *
cpu_waster(void * arg)
{
	int ret;
	char * name;
	ret = asprintf(&name, "cpu_waster#%d", (int) arg);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "asprintf");
	ret = pthread_setname_np(name);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_setname_np(\"%s\")", name);

	while (1) {
		uint64_t time_quanta_in_ns = wasteCPUTimeQuanta;
		if (wasteCPUTimeQuantaRandomVariationPercent) {
			uint64_t maximum_possible_variation_in_ns = wasteCPUTimeQuanta * wasteCPUTimeQuantaRandomVariationPercent / 100ULL;
			uint64_t actual_variation_in_ns = arc4random_uniform((uint32_t)maximum_possible_variation_in_ns);
			time_quanta_in_ns += actual_variation_in_ns;
		}

		uint64_t time_acitve_in_ns = time_quanta_in_ns * wasteCPUTimePercentActive / 100ULL;
		uint64_t time_sleeping_in_ns = time_quanta_in_ns - time_acitve_in_ns;

		// Chew some cpu
		uint64_t time_active_in_abs = nanos_to_abs(time_acitve_in_ns);
		uint64_t test_start_time = mach_absolute_time();
		uint64_t test_desired_end_time = test_start_time + time_active_in_abs;
		while (mach_absolute_time() < test_desired_end_time) {
		}

		// Sleep a bit
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = time_sleeping_in_ns;
		nanosleep(&ts, NULL);
	}
	return NULL;
}

static void *
perform_test(__unused void * arg)
{
	make_realtime();

	T_LOG("Requested                 Test        Average    Worst");
	T_LOG("WAIT(ns)   CPU(us)  cpu%%  Elapsed(ns) Miss(ns)   Miss(ns)");

	for (uint64_t delay_in_ns = waitStart; delay_in_ns <= waitEnd; delay_in_ns += waitStep) {
		uint64_t delay_in_abs = nanos_to_abs(delay_in_ns);

		uint64_t test_start_time = mach_absolute_time();
		uint64_t test_desired_end_time = test_start_time + nanos_to_abs(testDuration);
		uint64_t test_actual_end_time = 0;
		uint64_t elapsed_reading_count = 0;
		uint64_t total_elapsed_time = 0;
		uint64_t avg_elapsed_reading = 0;
		uint64_t worst_miss = 0;

		thread_basic_info_data_t start_info = thread_info_get();
		do {
			// This is the actual timer wait
			uint64_t t1 = mach_absolute_time();
			mach_wait_until(t1 + delay_in_abs);
			uint64_t t2 = mach_absolute_time();

			// Now we calculate the elapsed time
			int64_t elapsed_ns = abs_to_nanos(t2 - t1 - delay_in_abs);
			elapsed_reading_count++;
			total_elapsed_time += elapsed_ns;
			avg_elapsed_reading = total_elapsed_time / elapsed_reading_count;

			if (elapsed_ns > worst_miss) {
				worst_miss = elapsed_ns;
			}
		} while ((test_actual_end_time = mach_absolute_time()) < test_desired_end_time);

		thread_basic_info_data_t end_info = thread_info_get();

		uint64_t user_delta_micros = ((end_info.user_time.seconds * USEC_PER_SEC) + end_info.user_time.microseconds) -
		    ((start_info.user_time.seconds * USEC_PER_SEC) + start_info.user_time.microseconds);

		uint64_t system_delta_micros = ((end_info.system_time.seconds * USEC_PER_SEC) + end_info.system_time.microseconds) -
		    ((start_info.system_time.seconds * USEC_PER_SEC) + start_info.system_time.microseconds);

		uint64_t total_delta_micros = user_delta_micros + system_delta_micros;
		uint64_t test_actual_elapsed = abs_to_nanos(test_actual_end_time - test_start_time);
		avg_elapsed_reading = total_elapsed_time / elapsed_reading_count;

		T_LOG("%09llu, %7llu, %4.1f, %10llu, %09llu, %09llu",
		    delay_in_ns, total_delta_micros, (double)end_info.cpu_usage / 10.0,
		    test_actual_elapsed, avg_elapsed_reading, worst_miss);

		T_QUIET; T_EXPECT_LE(avg_elapsed_reading, 500 * NSEC_PER_USEC, "average miss is <=0.5ms.");
		if (avg_elapsed_reading > 500 * NSEC_PER_USEC) {
			sched_kdebug_test_fail(delay_in_ns, total_delta_micros, avg_elapsed_reading, worst_miss);
		}
	}

	return NULL;
}

static void *
calibration(__unused void * arg)
{
	make_realtime();

	uint64_t delta_measurement = 0;
	for (uint32_t i = 0; i < CALIBRATION_CYCLES; ++i) {
		uint64_t last_time = mach_absolute_time();
		uint64_t delta = mach_absolute_time() - last_time;
		delta_measurement += abs_to_nanos(delta);
	}

	T_LOG( "mach_absolute_time minimum resolution:      %llu ns", abs_to_nanos(1ULL));
	T_LOG( "averaged minimum measurement time:          %llu ns", delta_measurement / CALIBRATION_CYCLES);
	T_LOG( "testDuration:                               %llu ns", testDuration);
	T_LOG( "waitStep:                                   %llu ns", waitStep);

	return NULL;
}

T_DECL(rttimer, "Check that realtime thread timer's average miss is <= 0.5ms",
    T_META_TAG_VM_NOT_ELIGIBLE, XNU_T_META_SOC_SPECIFIC,
    XNU_T_META_REQUIRES_DEVELOPMENT_KERNEL,     /* needed to set workload config */
    T_META_CHECK_LEAKS(false), /* could affect timing */
    T_META_RUN_CONCURRENTLY(false),
    T_META_ASROOT(true) /* needed to set workload config */
    )
{
	T_QUIET; T_ASSERT_POSIX_SUCCESS(proc_disable_wakemon(getpid()), "proc_disable_wakemon(getpid())");

	if (platform_is_virtual_machine()) {
		T_SKIP("Test not supposed to run on virtual machine. rdar://132930927");
	}

	pthread_t thread = NULL;
	int ret;

	/* Load the workload config. */
	workload_config_load();
	T_ATEND(workload_config_unload);

	/* Create the workgroup. The main thread does not need to join and become realtime. */
	setup_workgroup();

	/* Calibration */
	ret = pthread_create_with_workgroup_np(&thread, g_rt_workgroup, NULL, calibration, NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_create(calibration)");
	ret = pthread_join(thread, NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_join(calibration)");

	/* No-load tests */
	T_LOG("");
	T_LOG("Performing no-load tests.");
	ret = pthread_create_with_workgroup_np(&thread, g_rt_workgroup, NULL, perform_test, NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_create(perform_test) no-load");
	ret = pthread_join(thread, NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_join(perform_test) no-load");

	/* Heavy-load tests */
	int thread_count = 2 * dt_ncpu();
	T_LOG("");
	T_LOG("Performing heavy-load tests. Spawning %d default priority cpu waster threads.", thread_count);
	for (int i = 0; i < thread_count; i++) {
		ret = pthread_create(&thread, NULL, cpu_waster, (void *) (uintptr_t) i);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_create(cpu_waster#%d)", i);
	}
	ret = pthread_create_with_workgroup_np(&thread, g_rt_workgroup, NULL, perform_test, NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_create(perform_test) heavy-load");
	ret = pthread_join(thread, NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_join(perform_test) heavy-load");

	T_PASS("realtime thread timer's average miss is <= 0.5ms");
	T_END;
}
