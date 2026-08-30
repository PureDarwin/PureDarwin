#include <darwintest.h>
#include <signal.h>
#include <errno.h>

T_GLOBAL_META(
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("signals"),
	T_META_TAG_VM_PREFERRED,
	T_META_RUN_CONCURRENTLY(true),
	T_META_CHECK_LEAKS(false)
	);

T_DECL(signal_initproc_prohibited, "Check that signalling initproc is prohibited", T_META_ASROOT(TRUE), T_META_TAG_VM_PREFERRED)
{
	/* All user-initiated signals to launchd are prohibited. */
	bool saw_sigterm = false;
	bool saw_sigkill = false;
	int signal_max = SIGUSR2;

	for (int signal = 1; signal < signal_max; signal++) {
		T_WITH_ERRNO;
		T_ASSERT_POSIX_FAILURE(kill(1, signal),
		    EPERM,
		    "Shouln't be able to send signal '%s' to initproc",
		    strsignal(signal));

		saw_sigkill |= signal == SIGKILL;
		saw_sigterm |= signal == SIGTERM;
	}

	T_ASSERT_TRUE(saw_sigkill, "Tried sigkill");
	T_ASSERT_TRUE(saw_sigterm, "Tried sigterm");
}
