#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <darwintest.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/sysctl.h>
#include <spawn.h>
#include <signal.h>
#include <TargetConditionals.h>
#include "excserver_protect.h"

#define MAX_ARGV 3
#define EXC_CODE_SHIFT 32
#define EXC_GUARD_TYPE_SHIFT 29
#define MAX_TEST_NUM 21

#define TASK_EXC_GUARD_MP_DELIVER 0x10

extern char **environ;
static uint64_t exception_code = 0;
static exception_type_t exception_taken = 0;

#ifndef kGUARD_EXC_INVALID_OPTIONS
#define kGUARD_EXC_INVALID_OPTIONS 3
#endif

/*
 * This test verifies behaviors of immovable/pinned task/thread ports.
 *
 * 1. Compare and verifies port names of mach_{task, thread}_self(),
 * {TASK, THREAD}_KERNEL_PORT, and ports returned from task_threads()
 * and processor_set_tasks().
 * 2. Make sure correct exceptions are raised resulting from moving immovable
 * task/thread control, read and inspect ports.
 * 3. Make sure correct exceptions are raised resulting from deallocating pinned
 * task/thread control ports.
 * 4. Make sure immovable ports cannot be stashed:
 * rdar://70585367 (Disallow immovable port stashing with *_set_special_port() and mach_port_register())
 */
T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"),
	T_META_RUN_CONCURRENTLY(TRUE),
	T_META_TAG_VM_PREFERRED
	);

static uint64_t soft_exception_code[] = {
	EXC_GUARD, // Soft crash delivered as EXC_CORPSE_NOTIFY
	EXC_GUARD,
	EXC_GUARD,
	EXC_GUARD,
	EXC_GUARD,
	EXC_GUARD,
	EXC_GUARD,

	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,

	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_INVALID_OPTIONS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_INVALID_OPTIONS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_INVALID_OPTIONS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_INVALID_OPTIONS,
};

static uint64_t hard_exception_code[] = {
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_MOD_REFS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_MOD_REFS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_MOD_REFS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_MOD_REFS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_MOD_REFS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_MOD_REFS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_MOD_REFS,

	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_IMMOVABLE,

	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_INVALID_OPTIONS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_INVALID_OPTIONS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_INVALID_OPTIONS,
	(GUARD_TYPE_MACH_PORT << EXC_GUARD_TYPE_SHIFT) | kGUARD_EXC_INVALID_OPTIONS,
};

kern_return_t
catch_mach_exception_raise_state(mach_port_t exception_port,
    exception_type_t exception,
    const mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    int * flavor,
    const thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t * new_state_count)
{
#pragma unused(exception_port, exception, code, code_count, flavor, old_state, old_state_count, new_state, new_state_count)
	T_FAIL("Unsupported catch_mach_exception_raise_state");
	return KERN_NOT_SUPPORTED;
}

kern_return_t
catch_mach_exception_raise_identity_protected(
	mach_port_t               exception_port,
	uint64_t                  thread_id,
	mach_port_t               task_id_token,
	exception_type_t          exception,
	mach_exception_data_t     codes,
	mach_msg_type_number_t    codeCnt)
{
#pragma unused(exception_port, codeCnt)
	task_t task;
	pid_t pid;
	kern_return_t kr = task_identity_token_get_task_port(task_id_token, TASK_FLAVOR_READ, &task);
	T_ASSERT_MACH_SUCCESS(kr, "task_identity_token_get_task_port");
	kr = pid_for_task(task, &pid);
	T_ASSERT_MACH_SUCCESS(kr, "pid_for_task");
	T_LOG("Crashing child pid: %d, continuing...\n", pid);

	kr = mach_port_deallocate(mach_task_self(), task);
	T_QUIET; T_EXPECT_MACH_SUCCESS(kr, "mach_port_deallocate");

	T_ASSERT_GT_UINT(codeCnt, 0, "CodeCnt");
	T_LOG("Caught exception type: %d code: 0x%llx", exception, (uint64_t)codes[0]);
	if (exception == EXC_GUARD || exception == EXC_CORPSE_NOTIFY) {
		exception_taken = exception;
		exception_code = (uint64_t)codes[0];
	} else {
		T_FAIL("Unexpected exception");
	}
	return KERN_SUCCESS;
}

kern_return_t
catch_mach_exception_raise_state_identity(mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    int * flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t * new_state_count)
{
#pragma unused(exception_port, thread, task, exception, code, code_count, flavor, old_state, old_state_count, new_state, new_state_count)
	T_FAIL("Unsupported catch_mach_exception_raise_state_identity");
	return KERN_NOT_SUPPORTED;
}

kern_return_t
catch_mach_exception_raise(mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count)
{
#pragma unused(exception_port, thread, task, exception, code, code_count)
	T_FAIL("Unsupported catch_mach_exception_raise_state_identity");
	return KERN_NOT_SUPPORTED;
}

static void *
exception_server_thread(void *arg)
{
	kern_return_t kr;
	mach_port_t exc_port = *(mach_port_t *)arg;

	/* Handle exceptions on exc_port */
	kr = mach_msg_server_once(mach_exc_server, 4096, exc_port, 0);
	T_QUIET; T_EXPECT_MACH_SUCCESS(kr, "mach_msg_server_once");

	return NULL;
}

static mach_port_t
alloc_exception_port(void)
{
	kern_return_t kret;
	mach_port_t exc_port = MACH_PORT_NULL;
	mach_port_t task = mach_task_self();

	kret = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &exc_port);
	T_QUIET; T_EXPECT_MACH_SUCCESS(kret, "mach_port_allocate exc_port");

	kret = mach_port_insert_right(task, exc_port, exc_port, MACH_MSG_TYPE_MAKE_SEND);
	T_QUIET; T_EXPECT_MACH_SUCCESS(kret, "mach_port_insert_right exc_port");

	return exc_port;
}

static void
test_immovable_port_stashing(void)
{
	kern_return_t kr;
	mach_port_t port;

	kr = task_set_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, mach_task_self());
	T_EXPECT_EQ(kr, KERN_INVALID_RIGHT, "should disallow task_set_special_port() with immovable port");

	kr = thread_set_special_port(mach_thread_self(), THREAD_KERNEL_PORT, mach_thread_self());
	T_EXPECT_EQ(kr, KERN_INVALID_RIGHT, "should disallow task_set_special_port() with immovable port");

	mach_port_t stash[1] = {mach_task_self()};
	kr = mach_ports_register(mach_task_self(), stash, 1);
	T_EXPECT_EQ(kr, KERN_INVALID_RIGHT, "should disallow mach_ports_register() with immovable port");

	T_QUIET; T_ASSERT_MACH_SUCCESS(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port), "mach_port_allocate");
	T_QUIET; T_ASSERT_MACH_SUCCESS(mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND), "mach_port_insert_right");

	stash[0] = port;
	kr = mach_ports_register(mach_task_self(), stash, 1);
	T_EXPECT_MACH_SUCCESS(kr, "mach_ports_register() should succeed with movable port");
}

int
read_ipc_control_port_options(void)
{
	uint32_t opts = 0;
	size_t size = sizeof(&opts);
	int sysctl_ret = sysctlbyname("kern.ipc_control_port_options", &opts, &size, NULL, 0);

	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctl_ret, "kern.ipc_control_port_options");

	return opts;
}

bool
is_hard_immovable_control_port_enabled(void)
{
	T_LOG("Check if immovable control port is enabled");
	int opts = read_ipc_control_port_options();
	return (opts & 0x8) == 0x8;
}

bool
is_hard_pinning_enforcement_enabled(void)
{
	T_LOG("Check if hard pinning enforcement is enabled");
	int opts = read_ipc_control_port_options();
	return opts & 0x2;
}

static bool
is_test_possible_in_current_system()
{
	uint32_t task_exc_guard = 0;
	size_t te_size = sizeof(&task_exc_guard);
	int sysctl_ret;

	/*
	 * This test tries to check assumptions that are only valid if
	 * certain boot args are missing, because in the presence of those boot args
	 * we relax the security policy that this test is trying to enforce.
	 */
	size_t bootargs_size = 1024;
	char bootargs[bootargs_size];
	sysctl_ret = sysctlbyname("kern.bootargs", bootargs, &bootargs_size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctl_ret, "kern.bootargs");
	if (strstr(bootargs, "amfi_get_out_of_my_way=1") != NULL ||
	    strstr(bootargs, "amfi_unrestrict_task_for_pid=1") != NULL) {
		T_SKIP("This test can only verify behavior when AMFI boot args are unset");
		return false;
	}

	T_LOG("Check if task_exc_guard exception has been enabled\n");
	sysctl_ret = sysctlbyname("kern.task_exc_guard_default", &task_exc_guard, &te_size, NULL, 0);
	T_ASSERT_EQ(sysctl_ret, 0, "sysctl to check exc_guard config");

	if (!(task_exc_guard & TASK_EXC_GUARD_MP_DELIVER)) {
		T_SKIP("task_exc_guard exception is not enabled");
		return false;
	}

	if (!is_hard_immovable_control_port_enabled()) {
		T_SKIP("hard immovable control port isn't enabled");
		return false;
	}

	return true;
}

static void
test_imm_control_port_exc_behavior(const char* test_prog_name)
{
	uint64_t *test_exception_code;
	posix_spawnattr_t       attrs;
	pid_t client_pid = 0;
	mach_port_t exc_port;
	pthread_t s_exc_thread;
	uint64_t exc_id;
	char *child_args[MAX_ARGV];
	int ret = 0;

	T_SETUPBEGIN;
	bool is_test_possible = is_test_possible_in_current_system();
	T_SETUPEND;
	if (!is_test_possible) {
		return;
	}

	if (is_hard_pinning_enforcement_enabled()) {
		T_LOG("Hard pinning enforcement is on.");
		test_exception_code = hard_exception_code;
	} else {
		T_LOG("Hard pinning enforcement is off.");
		test_exception_code = soft_exception_code;
	}

	/* spawn a child and see if EXC_GUARD are correctly generated */
	for (int i = 0; i < MAX_TEST_NUM; i++) {
		/* Create the exception port for the child */
		exc_port = alloc_exception_port();
		T_QUIET; T_ASSERT_NE(exc_port, MACH_PORT_NULL, "Create a new exception port");

		/* Create exception serving thread */
		ret = pthread_create(&s_exc_thread, NULL, exception_server_thread, &exc_port);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_create exception_server_thread");

		/* Initialize posix_spawn attributes */
		posix_spawnattr_init(&attrs);

		int err = posix_spawnattr_setexceptionports_np(&attrs, EXC_MASK_GUARD | EXC_MASK_CORPSE_NOTIFY, exc_port,
		    (exception_behavior_t) (EXCEPTION_IDENTITY_PROTECTED | MACH_EXCEPTION_CODES), 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "posix_spawnattr_setflags");

		child_args[0] = (char*)test_prog_name;
		char test_num[10];
		sprintf(test_num, "%d", i);
		child_args[1] = test_num;
		child_args[2] = NULL;

		T_LOG("========== Spawning new child ==========");
		err = posix_spawn(&client_pid, child_args[0], NULL, &attrs, &child_args[0], environ);
		T_ASSERT_POSIX_SUCCESS(err, "posix_spawn control_port_options_client = %d test_num = %d", client_pid, i);

		int child_status;
		/* Wait for child and check for exception */
		if (-1 == waitpid(-1, &child_status, 0)) {
			T_FAIL("waitpid: child mia");
		}

		if (WIFEXITED(child_status)) {
			if (WEXITSTATUS(child_status)) {
				T_FAIL("Child exited with status = %x", child_status); T_END;
			} else {
				/* Skipping test because CS_OPS_CLEARPLATFORM is not supported in the current BATS container */
				T_SKIP("Failed to remove platform binary");
			}
		}

		sleep(1);
		kill(1, SIGKILL);

		ret = pthread_join(s_exc_thread, NULL);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "pthread_join");

		if (exception_taken == EXC_GUARD) {
			exc_id = exception_code >> EXC_CODE_SHIFT;
		} else {
			exc_id = exception_code;
		}

		T_LOG("Exception code: Received code = 0x%llx Expected code = 0x%llx", exc_id, test_exception_code[i]);
		T_EXPECT_EQ(exc_id, test_exception_code[i], "Exception code: Received == Expected");
	}
}

static void
test_imm_pinned_control_port_stashing(void)
{
	T_SETUPBEGIN;
	bool is_test_possible = is_test_possible_in_current_system();
	T_SETUPEND;
	if (!is_test_possible) {
		return;
	}

	/* try stashing immovable ports: rdar://70585367 */
	test_immovable_port_stashing();
}

T_DECL(imm_pinned_control_port_stashing, "Validate API behavior when presented with an immovable port",
    T_META_CHECK_LEAKS(false))
{
	test_imm_pinned_control_port_stashing();
}

/*
 * rdar://150644433: IPC policy changed substantially since these tests were written,
 * and these tests weren't updated to reflect changes in policy.
 * The flow these tests exercise is no longer throwing exceptions, and someone needs to dig into the
 * policy changes to understand how the tests needs to change.
 * This is a bit too far to go at the precise moment this comment was written, where
 * I'm just trying to re-enable other parts of this test program, so I've marked the tests as may-fail
 * for the future day where the coverage will be useful to recover.
 */
T_DECL(imm_pinned_control_port_hardened, "Test pinned & immovable task and thread control ports for platform restrictions binary",
    T_META_IGNORECRASHES(".*pinned_rights_child.*"),
    /* rdar://150644433 this test has fallen behind contemporary IPC policy, see comment above */
    T_META_ENABLED(false),
    T_META_CHECK_LEAKS(false))
{
	test_imm_control_port_exc_behavior("imm_pinned_control_port_crasher_3P_hardened");
}

T_DECL(imm_pinned_control_port, "Test pinned & immovable task and thread control ports for first party binary",
    T_META_IGNORECRASHES(".*pinned_rights_child.*"),
    T_META_CHECK_LEAKS(false),
    /* rdar://150644433 this test has fallen behind contemporary IPC policy, see comment above */
    T_META_ENABLED(false),
    T_META_TAG_VM_PREFERRED)
{
	test_imm_control_port_exc_behavior("imm_pinned_control_port_crasher");
}
