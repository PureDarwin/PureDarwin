#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <spawn.h>
#include <pthread.h>

#include <darwintest.h>

T_GLOBAL_META(
	T_META_RUN_CONCURRENTLY(true),
	T_META_IGNORECRASHES("sigchld.*"),
	T_META_CHECK_LEAKS(false),
	T_META_TAG_VM_PREFERRED,
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("signals"));

static int exitcode = 123;

static sigset_t set;

static void
dummy_sigchld_handler(int sig)
{
}

static void
prepare_for_sigwait()
{
	/*
	 * SIGCHLD's default disposition to ignore, which means sigwait won't see it pending.
	 * This requires us to set a dummy signal handler.
	 */
	struct sigaction act;
	act.sa_handler = dummy_sigchld_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGCHLD, &act, NULL);

	/* Now block SIGCHLD */
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &set, NULL);
}

static void
wait_for_signal(int expected_exitcode, pid_t expected_pid)
{
	siginfo_t siginfo;
	int sig, result;

	result = sigwait(&set, &sig);
	T_ASSERT_POSIX_SUCCESS(result, "sigwait failed");
	T_ASSERT_EQ_INT(sig, SIGCHLD, "sigwait returned SIGCHLD");

	result = waitid(P_PID, expected_pid, &siginfo, WEXITED | WNOHANG);
	T_ASSERT_POSIX_SUCCESS(result, "waitid failed");
	T_ASSERT_NE_INT(siginfo.si_pid, 0, "waitid returned no child");

	T_ASSERT_EQ_INT(siginfo.si_signo, SIGCHLD, "si_signo is SIGCHLD");
	T_ASSERT_EQ_INT(siginfo.si_code, CLD_EXITED, "si_code is CLD_EXITED");
	T_ASSERT_EQ_INT(siginfo.si_status, expected_exitcode, "si_status");
}


T_DECL(sigchldreturn, "checks that a child process exited with an exitcode returns correctly to parent")
{
	int pid;

	prepare_for_sigwait();

	/* Now fork a child that just exits */
	pid = fork();
	T_QUIET; T_ASSERT_NE_INT(pid, -1, "fork() failed!");

	if (pid == 0) {
		/* Child process! */
		exit(exitcode);
	}

	wait_for_signal(exitcode, pid);
}

T_DECL(sigabrt_test, "check that child process' exitcode contains signum = SIGABRT")
{
	int ret;
	siginfo_t siginfo;
	pid_t pid = fork();
	int expected_signal = SIGABRT;
	if (pid == 0) {
		/* child exits with SIGABRT */
		T_LOG("In child process. Now signalling SIGABRT");
		(void)signal(SIGABRT, SIG_DFL);
		raise(SIGABRT);
		T_LOG("Child should not print");
	} else {
		ret = waitid(P_PID, (id_t) pid, &siginfo, WEXITED);
		T_ASSERT_POSIX_SUCCESS(0, "waitid");
		if (siginfo.si_signo != SIGCHLD) {
			T_FAIL("Signal was not SIGCHLD.");
		}
		T_LOG("si_status = 0x%x , expected = 0x%x \n", siginfo.si_status, expected_signal);
		if (siginfo.si_status != expected_signal) {
			T_FAIL("Unexpected exitcode");
		}
	}
}

T_DECL(sigkill_test, "check that child process' exitcode contains signum = SIGKILL")
{
	int ret;
	siginfo_t siginfo;
	pid_t pid = fork();
	int expected_signal = SIGKILL;
	if (pid == 0) {
		/* child exits with SIGKILL */
		T_LOG("In child process. Now signalling SIGKILL");
		raise(SIGKILL);
		T_LOG("Child should not print");
	} else {
		ret = waitid(P_PID, (id_t) pid, &siginfo, WEXITED);
		T_ASSERT_POSIX_SUCCESS(0, "waitid");
		if (siginfo.si_signo != SIGCHLD) {
			T_FAIL("Signal was not SIGCHLD.");
		}
		T_LOG("si_status = 0x%x , expected = 0x%x \n", siginfo.si_status, expected_signal);
		if (siginfo.si_status != expected_signal) {
			T_FAIL("Unexpected exitcode");
		}
	}
}

T_DECL(sigchild_posix_spawn_fail, "check SIGCHLD is correctly delivered when posix_spawn fails")
{
	int pid;
	char *args[4];

	exitcode = 0;

	prepare_for_sigwait();

	args[0] = "sh";
	args[1] = "-c";
	args[2] = "exit 0";
	args[3] = NULL;

	T_ASSERT_POSIX_SUCCESS(posix_spawn(&pid, "/bin/sh", NULL, NULL, args, NULL), "posix_spawn failed");

	for (int i = 0; i < 500; i++) {
		int ret = posix_spawn(&pid, "does not exist", NULL, NULL, args, NULL);
		T_QUIET; T_ASSERT_EQ(ret, ENOENT, "posix_spawn should fail with ENOENT");
	}

	wait_for_signal(exitcode, pid);
}
