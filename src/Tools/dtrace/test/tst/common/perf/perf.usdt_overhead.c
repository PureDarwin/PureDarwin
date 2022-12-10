/*
 * Measures the overhead of registering / unregistering USDT providers.
 */
#include <darwintest.h>
#include <darwintest_perf.h>

#include <dtrace.h>

#include <unistd.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/sysctl.h>


static dtrace_hdl_t	*g_dtp;

static void
check_usdt_enabled(void)
{
	int err;
	int dof_mode;
	size_t dof_mode_size = sizeof(dof_mode);
	err = sysctlbyname("kern.dtrace.dof_mode", &dof_mode, &dof_mode_size, NULL, 0);
	if (!err && dof_mode == 0) {
		T_SKIP("usdt probes are not enabled on this system");
	}
	else if (err) {
		T_LOG("could not figure out whether usdt probes are enabled");
	}
}

#define FORK_MEASURE_LOOP(s) do { \
	pid_t pid; \
	int status; \
	while (!dt_stat_stable(s)) { \
		T_STAT_MEASURE(s) { \
			pid = fork(); \
			if (pid == 0) \
				exit(0); \
			else if (pid == -1) \
				T_FAIL("fork returned -1"); \
		} \
		waitpid(pid, &status, 0); \
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { \
			T_FAIL("forked process failed to exit properly"); \
		} \
	} \
} while(0)

#define SPAWN_MEASURE_LOOP(s, binary) do {\
	char *args[] = {binary, NULL}; \
	int err; \
	pid_t pid; \
	int status; \
	while (!dt_stat_stable(s)) { \
		T_STAT_MEASURE(s) { \
			err = posix_spawn(&pid, args[0], NULL, NULL, args, NULL); \
			if (err) { \
				T_FAIL("posix_spawn returned %d", err); \
			} \
			waitpid(pid, &status, 0); \
		} \
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { \
			T_FAIL("Child process of posix_spawn failed to run"); \
		} \
	} \
} while(0)

static int
enable_dtrace_probes(char const* str)
{
	int err;
	dtrace_prog_t *prog;
	dtrace_proginfo_t info;

	T_SETUPBEGIN;
	if ((g_dtp = dtrace_open(DTRACE_VERSION, 0, &err)) == NULL) {
		T_FAIL("failed to initialize dtrace");
		return -1;
	}

	prog = dtrace_program_strcompile(g_dtp, str, DTRACE_PROBESPEC_NAME, 0, 0, NULL);
	if (!prog) {
		T_FAIL("failed to compile program");
		return -1;
	}

	if (dtrace_program_exec(g_dtp, prog, &info) == -1) {
		fprintf(stderr, "failed to enable probes");
		return -1;
	}
	T_SETUPEND;

	return 0;
}

/*
 * Cleanup the probe tracepoints enabled via enable_dtrace_probes.
 */
static void
disable_dtrace(void)
{
    dtrace_close(g_dtp);
}

static void fork_test(void)
{
	dt_stat_time_t time_s = dt_stat_time_create("time");
	FORK_MEASURE_LOOP(time_s);
	dt_stat_finalize(time_s);

	dt_stat_thread_cpu_time_t cpu_time_s = dt_stat_thread_cpu_time_create("on-cpu time");
	FORK_MEASURE_LOOP(cpu_time_s);
	dt_stat_finalize(cpu_time_s);
}


T_DECL(dtrace_fork_baseline, "baseline fork latency")
{
	check_usdt_enabled();
	fork_test();
}

T_DECL(dtrace_fork, "dtrace on fork latency", T_META_CHECK_LEAKS(false))
{
	check_usdt_enabled();
	enable_dtrace_probes("BEGIN");
	fork_test();
	disable_dtrace();
}

static void
posix_spawn_test(bool dtrace_on, const char *helper)
{
	check_usdt_enabled();
	if (dtrace_on) {
		enable_dtrace_probes("BEGIN");
	}

	dt_stat_time_t time_s = dt_stat_time_create("time");
	SPAWN_MEASURE_LOOP(time_s, helper);
	dt_stat_finalize(time_s);

	dt_stat_thread_cpu_time_t cpu_time_s = dt_stat_thread_cpu_time_create("on-cpu time");
	SPAWN_MEASURE_LOOP(cpu_time_s, helper);
	dt_stat_finalize(cpu_time_s);

	if (dtrace_on) {
		disable_dtrace();
	}

}

T_DECL(dtrace_spawn_baseline, "baseline posix_spawn latency", T_META_CHECK_LEAKS(false))
{
	posix_spawn_test(false, "./usdt_overhead_helper.0");
}

T_DECL(dtrace_spawn_1, "spawn latency with dtrace on and one USDT provider", T_META_CHECK_LEAKS(false))
{
	posix_spawn_test(true, "./usdt_overhead_helper.1");
}

T_DECL(dtrace_spawn_10, "spawn latency with dtrace on and 10 USDT providers", T_META_CHECK_LEAKS(false))
{
	posix_spawn_test(true, "./usdt_overhead_helper.10");
}
