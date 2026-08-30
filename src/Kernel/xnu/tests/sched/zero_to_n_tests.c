#include <darwintest.h>
#include <darwintest_utils.h>
#include <dispatch/dispatch.h>
#include <perfdata/perfdata.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include "sched_test_utils.h"
#include "test_utils.h"

T_GLOBAL_META(
	T_META_TAG_PERF,
	T_META_RUN_CONCURRENTLY(false),
	/* <rdar://137716223> */
	T_META_BOOTARGS_SET("enable_skstsct=1 cpu-dynamic-cluster-power-down=0"),
	T_META_CHECK_LEAKS(false),
	T_META_ASROOT(true),
	T_META_REQUIRES_SYSCTL_EQ("kern.hv_vmm_present", 0),
	T_META_NAMESPACE("xnu.scheduler"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("scheduler"),
	T_META_OWNER("m_zinn"),
	T_META_TAG_VM_NOT_ELIGIBLE
	);

static void
log_cmd(char **cmd)
{
#define MAX_CMD_STR 1024
	char cmd_str[MAX_CMD_STR] = "";
	char *s;

	while ((s = *cmd) != NULL) {
		strlcat(cmd_str, s, MAX_CMD_STR);
		strlcat(cmd_str, " ", MAX_CMD_STR);
		cmd++;
	}
	T_LOG("%s\n", cmd_str);
}

static void
run_zn(char *name, char **cmd, int argc, char *const argv[])
{
	log_cmd(cmd);

	trace_handle_t trace = begin_collect_trace_fmt(COLLECT_TRACE_FLAG_DISABLE_SYSCALLS | COLLECT_TRACE_FLAG_DISABLE_CLUTCH, argc, argv, name);

	__block bool test_failed = true;
	__block bool test_skipped = false;
	__block dispatch_semaphore_t stdout_finished_sem = dispatch_semaphore_create(0);
	T_QUIET; T_ASSERT_NOTNULL(stdout_finished_sem, "dispatch_semaphore_create()");

	dt_launch_pipe_t *pipes = NULL;
	pid_t test_pid;
	test_pid = dt_launch_tool_pipe(cmd, false, &pipes, NULL, NULL, NULL, NULL);
	T_QUIET; T_ASSERT_NE(test_pid, 0, "dt_launch_tool_pipe() failed unexpectedly with errno %d", errno);
	T_QUIET; T_ASSERT_NOTNULL(pipes, "dt_launch_tool_pipe returned non-null pipes");

	dispatch_block_t cleanup_handler =
	    ^{ dispatch_semaphore_signal(stdout_finished_sem); };

	dt_pipe_data_handler_t stdout_handler = ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("%s", data);
		if (strstr(data, "TEST PASSED")) {
			test_failed = false;
			return true;
		}
		if (strstr(data, "TEST FAILED")) {
			test_failed = true;
			return true;
		}
		if (strstr(data, "TEST SKIPPED")) {
			test_skipped = true;
			return true;
		}
		return false;
	};

	dt_pipe_data_handler_t stderr_handler = ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("%s", data);
		return false;
	};

	dispatch_source_t stdout_reader = dt_create_dispatch_file_reader(pipes->pipe_out[0], BUFFER_PATTERN_LINE, stdout_handler, cleanup_handler, NULL);
	T_QUIET; T_ASSERT_NOTNULL(stdout_reader, "create darwintest dispatch file reader for stdout");

	dispatch_source_t stderr_reader = dt_create_dispatch_file_reader(pipes->pipe_err[0], BUFFER_PATTERN_LINE, stderr_handler, ^{}, NULL);
	T_QUIET; T_ASSERT_NOTNULL(stderr_reader, "create darwintest dispatch file reader for stderr");

	/* Wait for zero-to-n to exit, and check its return value. */
	int exitstatus;
	if (!dt_waitpid(test_pid, &exitstatus, NULL, 0) || exitstatus != 0) {
		T_FAIL("zero-to-n exitstatus=%d\n", exitstatus);
	}

	/* Test exited, end the trace. */
	end_collect_trace(trace);

	/* Wait for the readers to finish. */
	intptr_t rv = dispatch_semaphore_wait(stdout_finished_sem, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));
	T_QUIET; T_ASSERT_EQ((uint64_t) rv, 0ULL, "zn should finish within 30 seconds");

	/* Free the pipes. */
	free(pipes);

	if (test_skipped) {
		T_SKIP("%s", name);
	} else if (test_failed) {
		T_FAIL("%s", name);
	} else {
		T_PASS("%s", name);
	}

	T_END;
}

T_DECL(zn_rt, "Schedule 1 RT thread per performance core, and test max latency",
    XNU_T_META_SOC_SPECIFIC)
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--test-rt",
#if defined(__arm64__)
		       "--bind", "P",     /* <rdar://137716223> */
		       "--trace", "500000",
#elif defined(__x86_64__)
		       "--trace", "2000000",
#endif
		       NULL};

	run_zn("zn_rt", cmd, argc, argv);
}

T_DECL(zn_rt_ival, "Schedule 1 RT thread per performance core, and test max latency",
    XNU_T_META_SOC_SPECIFIC,
    T_META_ENABLED(false) /* TODO: Enable once <rdar://145756951> is fixed. */)
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--test-rt",
		       "--rt-interval",
#if defined(__x86_64__)
		       "--trace", "2000000",
#else
		       "--trace", "500000",
#endif
		       NULL};

	run_zn("zn_rt_ival", cmd, argc, argv);
}

T_DECL(zn_rt_ival_ll, "Schedule 1 RT thread per performance core, and test max latency",
    XNU_T_META_SOC_SPECIFIC)
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--test-rt",
		       "--rt-interval",
		       "--rt-ll",
#if defined(__x86_64__)
		       "--trace", "2000000",
#else
		       "--trace", "500000",
#endif
		       NULL};

	run_zn("zn_rt_ival_ll", cmd, argc, argv);
}

T_DECL(zn_rt_smt, "Schedule 1 RT thread per primary core, verify that the secondaries are idle iff the RT threads are running",
    T_META_ENABLED(TARGET_CPU_X86_64))
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--churn-pri", "4",
		       "--test-rt-smt",
		       "--trace", "2000000",
		       NULL};

	run_zn("zn_rt_smt", cmd, argc, argv);
}

T_DECL(zn_rt_ival_smt, "Schedule 1 RT thread per primary core, verify that the secondaries are idle iff the RT threads are running",
    T_META_ENABLED(TARGET_CPU_X86_64))
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--churn-pri", "4",
		       "--test-rt-smt",
		       "--rt-interval",
		       "--trace", "2000000",
		       NULL};

	run_zn("zn_rt_ival_smt", cmd, argc, argv);
}

T_DECL(zn_rt_avoid0, "Schedule 1 RT thread per primary core except for CPU 0",
    T_META_ENABLED(TARGET_CPU_X86_64))
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--test-rt-avoid0",
		       "--trace", "2000000",
		       NULL};

	run_zn("zn_rt_avoid0", cmd, argc, argv);
}

T_DECL(zn_rt_ival_avoid0, "Schedule 1 RT thread per primary core except for CPU 0",
    T_META_ENABLED(TARGET_CPU_X86_64))
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--test-rt-avoid0",
		       "--rt-interval",
		       "--trace", "2000000",
		       NULL};

	run_zn("zn_rt_ival_avoid0", cmd, argc, argv);
}

T_DECL(zn_rt_apt, "Emulate AVID Pro Tools with default latency deadlines")
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "chain", "realtime", "1000",
		       "--extra-thread-count", "-3",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--churn-pri", "31", "--churn-random",
		       "--test-rt",
#if defined(__arm64__)
		       "--bind", "P",     /* <rdar://137716223> */
		       "--trace", "500000",
#elif defined(__x86_64__)
		       "--trace", "2000000",
#endif
		       NULL};

	run_zn("zn_rt_apt", cmd, argc, argv);
}

T_DECL(zn_rt_ival_apt, "Emulate AVID Pro Tools with default latency deadlines",
    T_META_ENABLED(false) /* TODO: Enable once <rdar://145756951> is fixed. */)
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "chain", "realtime", "1000",
		       "--extra-thread-count", "-3",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--churn-pri", "31", "--churn-random",
		       "--test-rt",
		       "--rt-interval",
#if defined(__x86_64__)
		       "--trace", "2000000",
#else
		       "--trace", "500000",
#endif
		       NULL};

	run_zn("zn_rt_ival_apt", cmd, argc, argv);
}

T_DECL(zn_rt_apt_ll, "Emulate AVID Pro Tools with low latency deadlines",
    XNU_T_META_SOC_SPECIFIC)
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "chain", "realtime", "1000",
		       "--extra-thread-count", "-3",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--churn-pri", "31", "--churn-random",
		       "--test-rt",
		       "--rt-ll",
#if defined(__arm64__)
		       "--bind", "P",     /* <rdar://137716223> */
#endif   /* __arm64__*/
		       "--trace", "500000",
		       NULL};

	run_zn("zn_rt_apt_ll", cmd, argc, argv);
}

T_DECL(zn_rt_ival_apt_ll, "Emulate AVID Pro Tools with low latency deadlines",
    XNU_T_META_SOC_SPECIFIC)
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "chain", "realtime", "1000",
		       "--extra-thread-count", "-3",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--churn-pri", "31", "--churn-random",
		       "--test-rt",
		       "--rt-ll",
		       "--rt-interval",
		       "--trace", "500000",
		       NULL};

	run_zn("zn_rt_ival_apt_ll", cmd, argc, argv);
}

T_DECL(zn_rt_edf, "Test max latency of earliest deadline RT threads in the presence of later deadline threads",
    XNU_T_META_SOC_SPECIFIC)
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--extra-thread-count", "-1",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--rt-churn",
		       "--test-rt",
#if defined(__arm64__)
		       "--bind", "P",     /* <rdar://137716223> */
		       "--trace", "500000",
#elif defined(__x86_64__)
		       "--trace", "2000000",
#endif
		       NULL};

	run_zn("zn_rt_edf", cmd, argc, argv);
}

T_DECL(zn_rt_ival_edf, "Test max latency of earliest deadline RT threads in the presence of later deadline threads",
    XNU_T_META_SOC_SPECIFIC)
{
	char *cmd[] = {"/AppleInternal/CoreOS/tests/xnu/zero-to-n/zn",
		       "0", "broadcast-single-sem", "realtime", "1000",
		       "--extra-thread-count", "-1",
		       "--spin-time", "200000",
		       "--spin-all",
		       "--rt-churn",
		       "--test-rt",
		       "--rt-ll",     /* TODO: remove low-latency constraint once <rdar://145756951> is fixed */
		       "--rt-interval",
#if defined(__x86_64__)
		       "--trace", "2000000",
#else
		       "--trace", "500000",
#endif
		       NULL};

	run_zn("zn_rt_ival_edf", cmd, argc, argv);
}
