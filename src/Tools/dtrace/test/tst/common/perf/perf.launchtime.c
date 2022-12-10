#include <darwin_shim.h>
#include <darwintest.h>
#include <darwintest_utils.h>
#include <perfdata/perfdata.h>

#include <spawn.h>
#include <sys/wait.h>

#define DTRACE_PATH "/usr/sbin/dtrace"
#define DTRACE_SCRIPT "BEGIN { exit(0) }"
#define ITERATIONS 8

static hrtime_t
run_dtrace(void)
{
	char *args[] = {DTRACE_PATH, "-n", DTRACE_SCRIPT, NULL};
	int status, err;
	pid_t pid;
	hrtime_t begin = gethrtime();
	err = posix_spawn(&pid, args[0], NULL, NULL, args, NULL); \
	if (err) {
		T_FAIL("failed to spawn %s", args[0]);
	}
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		T_FAIL("%s didn't exit properly", args[0]);
	}

	return gethrtime() - begin;
}

T_DECL(dtrace_launchtime, "measure the time to launch dtrace", T_META_CHECK_LEAKS(false), T_META_BOOTARGS_SET("-unsafe_kernel_text"))
{
	char filename[MAXPATHLEN] = "dtrace.launchtime." PD_FILE_EXT;
	dt_resultfile(filename, sizeof(filename));
	T_LOG("perfdata file: %s\n", filename);
	pdwriter_t wr = pdwriter_open(filename, "dtrace.launchtime", 1, 0);
	T_WITH_ERRNO;
	T_ASSERT_NOTNULL(wr, "pdwriter_open %s", filename);

	for (int i = 0; i < ITERATIONS; i++) {
		hrtime_t time = run_dtrace();
		pdwriter_new_value(wr, "launch_time", pdunit_nanoseconds, time);
	}

	pdwriter_close(wr);
}
