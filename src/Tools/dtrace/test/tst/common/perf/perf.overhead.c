#include <darwintest.h>
#include <darwintest_perf.h>
#include <unistd.h>
#include <dtrace.h>


static dtrace_hdl_t	*g_dtp;

/*
 * Measures the performance overhead of various dtrace providers
 */
T_GLOBAL_META(T_META_NAMESPACE("dtrace.overhead"));

/*
 * Enable the probes of a dtrace program, contained in str. This function does
 * not start tracing, but only enable the probe tracepoints.
 */
int
enable_dtrace_probes(char const* str)
{
	int err;
	dtrace_prog_t *prog;
	dtrace_proginfo_t info;

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
		T_FAIL("failed to enable probes");
		return -1;
	}

	return 0;
}

/*
 * Cleanup the probe tracepoints enabled via enable_dtrace_probes.
 */
void
disable_dtrace(void)
{
    dtrace_close(g_dtp);
}

T_DECL(overhead_baseline, "baseline", T_META_CHECK_LEAKS(false)) {
	geteuid();
	dt_stat_time_t s = dt_stat_time_create("time");
	
	T_STAT_MEASURE_LOOP(s) {
		geteuid();
	}

	dt_stat_finalize(s);

	dt_stat_thread_instructions_t i = dt_stat_thread_instructions_create("instructions");
	T_STAT_MEASURE_LOOP(i) {
		geteuid();
	}

	dt_stat_finalize(i);
}

T_DECL(overhead_syscall, "syscall", T_META_CHECK_LEAKS(false)) {
	geteuid();
	dt_stat_time_t s = dt_stat_time_create("time");
	enable_dtrace_probes("syscall::geteuid:");

	T_STAT_MEASURE_LOOP(s) {
		geteuid();
	}
	

	dt_stat_thread_instructions_t i = dt_stat_thread_instructions_create("instructions");
	T_STAT_MEASURE_LOOP(i) {
		geteuid();
	}

	disable_dtrace();
	dt_stat_finalize(i);
	dt_stat_finalize(s);
}

T_DECL(overhead_fbt, "fbt", T_META_CHECK_LEAKS(false)) {
	geteuid();
	dt_stat_time_t s = dt_stat_time_create("fbt");
	enable_dtrace_probes("fbt:mach_kernel:geteuid:");
	T_STAT_MEASURE_LOOP(s) {
		geteuid();
	}

	dt_stat_thread_instructions_t i = dt_stat_thread_instructions_create("instructions");
	T_STAT_MEASURE_LOOP(i) {
		geteuid();
	}

	disable_dtrace();
	dt_stat_finalize(i);
	dt_stat_finalize(s);
}

