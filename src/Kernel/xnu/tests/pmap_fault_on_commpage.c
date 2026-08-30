#include <darwintest.h>
#include <machine/cpu_capabilities.h>
#include "test_utils.h"

#include <stdlib.h>
#include <signal.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.arm"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("arm"),
	T_META_ENABLED(TARGET_CPU_ARM64),
	T_META_OWNER("xi_han"),
	T_META_RUN_CONCURRENTLY(true),
	XNU_T_META_SOC_SPECIFIC
	);

#if defined(__LP64__)
#define SIGNAL_EXPECTED        SIGBUS
#define SIGNAL_EXPECTED_STR    "SIGBUS"
#else
/* On arm64_32, _COMM_PAGE_START_ADDRESS is out of normal VA range, so a SIGSEGV is expected if there's a fault. */
#define SIGNAL_EXPECTED        SIGSEGV
#define SIGNAL_EXPECTED_STR    "SIGSEGV"
#endif

#define TEST_STATE_TESTING_NONE              0
#define TEST_STATE_TESTING_READ              1
#define TEST_STATE_TESTING_WRITE             2
static volatile sig_atomic_t test_state;

static void
test_handler(int signum)
{
	T_ASSERT_EQ(signum, SIGNAL_EXPECTED, "received signal");

	if (test_state == TEST_STATE_TESTING_READ) {
		T_FAIL("read access triggered a %s", SIGNAL_EXPECTED_STR);
	} else if (test_state == TEST_STATE_TESTING_WRITE) {
		T_PASS("write access triggered a %s", SIGNAL_EXPECTED_STR);
		exit(EXIT_SUCCESS);
	} else {
		T_FAIL("unexpected %s in test state %u", SIGNAL_EXPECTED_STR, (unsigned int)test_state);
	}
}

T_DECL(pmap_commpage_access_test,
    "Verify system behavior on user access to the commpage", T_META_TAG_VM_NOT_PREFERRED)
{
	test_state = TEST_STATE_TESTING_NONE;

	struct sigaction sa;
	sa.sa_handler = test_handler;
	sa.sa_mask = 0;
	sa.sa_flags = 0;
	sigaction(SIGNAL_EXPECTED, &sa, NULL);

	test_state = TEST_STATE_TESTING_READ;
	*(volatile uint32_t *)_COMM_PAGE_START_ADDRESS;

	T_PASS("read access must not trigger a %s", SIGNAL_EXPECTED_STR);

	test_state = TEST_STATE_TESTING_WRITE;
	*(volatile uint32_t *)_COMM_PAGE_START_ADDRESS = 0;

	T_FAIL("write access must trigger a %s", SIGNAL_EXPECTED_STR);
}
