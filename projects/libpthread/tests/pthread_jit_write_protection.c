#include <darwintest.h>
#include <darwintest_perf.h>

#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <libkern/OSCacheControl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include <mach/vm_param.h>
#include <pthread.h>

#if __has_include(<ptrauth.h>)
#include <ptrauth.h>
#endif

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#error "These tests are only expected to run on macOS"
#endif // TARGET_OS_OSX

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

/* Enumerations */
typedef enum _access_type {
	ACCESS_WRITE,
	ACCESS_EXECUTE,
} access_type_t;

typedef enum _fault_strategy {
	FAULT_STRAT_NONE,
	FAULT_STRAT_RX,
	FAULT_STRAT_RW
} fault_strategy_t;

/* Structures */
typedef struct {
	uint64_t fault_count;
	fault_strategy_t fault_strategy;
	bool fault_expected;
} fault_state_t;

/* Globals */
static void * rwx_addr = NULL;
static pthread_key_t jit_test_fault_state_key;

/*
 * Return instruction encodings; a default value is given so that this test can
 * be built for an architecture that may not support the tested feature.
 */
#ifdef __arm__
static uint32_t ret_encoding = 0xe12fff1e;
#elif defined(__arm64__)
static uint32_t ret_encoding = 0xd65f03c0;
#elif defined(__x86_64__)
static uint32_t ret_encoding = 0x909090c3;;
#else
#error "Unsupported architecture"
#endif

/* Allocate a fault_state_t, and associate it with the current thread. */
static fault_state_t *
fault_state_create(void)
{
	fault_state_t * fault_state = malloc(sizeof(fault_state_t));

	if (fault_state) {
		fault_state->fault_count = 0;
		fault_state->fault_strategy = FAULT_STRAT_NONE;
		fault_state->fault_expected = false;

		if (pthread_setspecific(jit_test_fault_state_key, fault_state)) {
			free(fault_state);
			fault_state = NULL;
		}
	}

	return fault_state;
}

/* Disassociate the given fault state from the current thread, and destroy it. */
static void
fault_state_destroy(void * fault_state)
{
	if (fault_state == NULL) {
		T_ASSERT_FAIL("Attempted to fault_state_destroy NULL");
	}

	free(fault_state);
}

/*
 * A signal handler that attempts to resolve anticipated faults through use of
 * the pthread_jit_write_protect functions.
 */
static void
access_failed_handler(int signum)
{
	fault_state_t * fault_state;

	/* This handler should ONLY handle SIGBUS. */
	if (signum != SIGBUS) {
		T_ASSERT_FAIL("Unexpected signal sent to handler");
	}

	if (!(fault_state = pthread_getspecific(jit_test_fault_state_key))) {
		T_ASSERT_FAIL("Failed to retrieve fault state");
	}

	if (!(fault_state->fault_expected)) {
		T_ASSERT_FAIL("Unexpected fault taken");
	}

	/* We should not see a second fault. */
	fault_state->fault_expected = false;

	switch (fault_state->fault_strategy) {
	case FAULT_STRAT_NONE:
		T_ASSERT_FAIL("No fault strategy");

		/* Just in case we try to do something different. */
		break;
	case FAULT_STRAT_RX:
		pthread_jit_write_protect_np(TRUE);
		break;
	case FAULT_STRAT_RW:
		pthread_jit_write_protect_np(FALSE);
		break;
	}

	fault_state->fault_count++;
}

/*
 * Attempt the specified access; if the access faults, this will return true;
 * otherwise, it will return false.
 */
static bool
does_access_fault(access_type_t access_type, void * addr)
{
	uint64_t old_fault_count;
	uint64_t new_fault_count;

	fault_state_t * fault_state;

	struct sigaction old_action; /* Save area for any existing action. */
	struct sigaction new_action; /* The action we wish to install for SIGBUS. */

	bool retval = false;

	void (*func)(void);

	new_action.sa_handler = access_failed_handler; /* A handler for write failures. */
	new_action.sa_mask    = 0;                     /* Don't modify the mask. */
	new_action.sa_flags   = 0;                     /* Flags?  Who needs those? */

	if (addr == NULL) {
		T_ASSERT_FAIL("Access attempted against NULL");
	}

	if (!(fault_state = pthread_getspecific(jit_test_fault_state_key))) {
		T_ASSERT_FAIL("Failed to retrieve fault state");
	}

	old_fault_count = fault_state->fault_count;

	/* Install a handler so that we can catch SIGBUS. */
	sigaction(SIGBUS, &new_action, &old_action);

	/* Perform the requested operation. */
	switch (access_type) {
	case ACCESS_WRITE:
		fault_state->fault_strategy = FAULT_STRAT_RW;
		fault_state->fault_expected = true;

		__sync_synchronize();

		/* Attempt to scrawl a return instruction to the given address. */
		*((volatile uint32_t *)addr) = ret_encoding;

		__sync_synchronize();

		fault_state->fault_expected = false;
		fault_state->fault_strategy = FAULT_STRAT_NONE;

		/* Invalidate the instruction cache line that we modified. */
		sys_cache_control(kCacheFunctionPrepareForExecution, addr, sizeof(ret_encoding));

		break;
	case ACCESS_EXECUTE:
		/* This is a request to branch to the given address. */
#if __has_feature(ptrauth_calls)
		func = ptrauth_sign_unauthenticated((void *)addr, ptrauth_key_function_pointer, 0);
#else
		func = (void (*)(void))addr;
#endif


		fault_state->fault_strategy = FAULT_STRAT_RX;
		fault_state->fault_expected = true;

		__sync_synchronize();

		/* Branch. */
		func();

		__sync_synchronize();

		fault_state->fault_expected = false;
		fault_state->fault_strategy = FAULT_STRAT_NONE;

		break;
	}

	/* Restore the old SIGBUS handler. */
	sigaction(SIGBUS, &old_action, NULL);

	new_fault_count = fault_state->fault_count;

	if (new_fault_count > old_fault_count) {
		/* Indicate that we took a fault. */
		retval = true;
	}

	return retval;
}

static void *
expect_write_fail_thread(__unused void * arg)
{
	fault_state_create();

	if (does_access_fault(ACCESS_WRITE, rwx_addr)) {
		pthread_exit((void *)0);
	} else {
		pthread_exit((void *)1);
	}
}

T_DECL(pthread_jit_write_protect,
    "Verify that the pthread_jit_write_protect interfaces work correctly")
{
	void * addr = NULL;
	size_t alloc_size = PAGE_SIZE;
	fault_state_t * fault_state = NULL;
	int err = 0;
	bool key_created = false;
	void * join_value = NULL;
	pthread_t pthread;
	bool expect_fault = pthread_jit_write_protect_supported_np();

	T_SETUPBEGIN;

	/* Set up the necessary state for the test. */
	err = pthread_key_create(&jit_test_fault_state_key, fault_state_destroy);

	T_ASSERT_POSIX_ZERO(err, 0, "Create pthread key");

	key_created = true;

	fault_state = fault_state_create();

	T_ASSERT_NOTNULL(fault_state, "Create fault state");

	/*
	 * Create a JIT enabled mapping that we can use to test restriction of
	 * RWX mappings.
	 */
	rwx_addr = mmap(addr, alloc_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);

	T_ASSERT_NE_PTR(rwx_addr, MAP_FAILED, "Map range as MAP_JIT");

	T_SETUPEND;

	/*
	 * Validate that we fault when we should, and that we do not fault when
	 * we should not fault.
	 */
	pthread_jit_write_protect_np(FALSE);

	T_EXPECT_EQ(does_access_fault(ACCESS_WRITE, rwx_addr), 0, "Write with RWX->RW");

	pthread_jit_write_protect_np(TRUE);

	T_EXPECT_EQ(does_access_fault(ACCESS_EXECUTE, rwx_addr), 0, "Execute with RWX->RX");

	pthread_jit_write_protect_np(TRUE);

	T_EXPECT_EQ(does_access_fault(ACCESS_WRITE, rwx_addr), expect_fault, "Write with RWX->RX");

	pthread_jit_write_protect_np(FALSE);

	T_EXPECT_EQ(does_access_fault(ACCESS_EXECUTE, rwx_addr), expect_fault, "Execute with RWX->RW");

	pthread_jit_write_protect_np(FALSE);

	if (expect_fault) {
		/*
		 * Create another thread for testing multithreading; mark this as setup
		 * as this test is not targeted towards the pthread create/join APIs.
		 */
		T_SETUPBEGIN;

		T_ASSERT_POSIX_ZERO(pthread_create(&pthread, NULL, expect_write_fail_thread, NULL), "pthread_create expect_write_fail_thread");

		T_ASSERT_POSIX_ZERO(pthread_join(pthread, &join_value), "pthread_join expect_write_fail_thread");

		T_SETUPEND;

		/*
		 * Validate that the other thread was unable to write to the JIT region
		 * without independently using the pthread_jit_write_protect code.
		 */
		T_ASSERT_NULL((join_value), "Write on other thread with RWX->RX, "
		    "RWX->RW on parent thread");
	}

	/* We're done with the test; tear down our extra state. */
	/*
	 * This would be better dealt with using T_ATEND, but this would require
	 * making many variables global.  This can be changed in the future.
	 * For now, mark this as SETUP (even though this is really teardown).
	 */
	T_SETUPBEGIN;

	T_ASSERT_POSIX_SUCCESS(munmap(rwx_addr, alloc_size), "Unmap MAP_JIT mapping");

	if (fault_state) {
		T_ASSERT_POSIX_ZERO(pthread_setspecific(jit_test_fault_state_key, NULL), "Remove fault_state");

		fault_state_destroy(fault_state);
	}

	if (key_created) {
		T_ASSERT_POSIX_ZERO(pthread_key_delete(jit_test_fault_state_key), "Delete fault state key");
	}

	T_SETUPEND;
}

T_DECL(thread_self_restrict_rwx_perf,
    "Test the performance of the thread_self_restrict_rwx interfaces",
    T_META_TAG_PERF, T_META_CHECK_LEAKS(false))
{
	dt_stat_time_t dt_stat_time;

	dt_stat_time = dt_stat_time_create("rx->rw->rx time");

	T_STAT_MEASURE_LOOP(dt_stat_time) {
		pthread_jit_write_protect_np(FALSE);
		pthread_jit_write_protect_np(TRUE);
	}

	dt_stat_finalize(dt_stat_time);
}
