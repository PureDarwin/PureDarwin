//
//  notify_sandbox.c
//  Libnotify
//
//  Created by Brycen Wershing on 12/8/20.
//

#include <darwintest.h>
#include <darwintest_multiprocess.h>
#include <notify.h>
#include <notify_private.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <unistd.h>
#include <sandbox.h>
#include <TargetConditionals.h>
#include <stdio.h>

static const char *allowedName = "com.apple.test.sandbox-allow";
static const char *disallowedName = "com.apple.test.sandbox-disallow";
static const char *postBackName = "test.post.back";
static const char *exitName = "test.exit";

T_HELPER_DECL(notify_sandbox_helper, "notify sandbox helper to test posting out-of-process") {
	dispatch_queue_t queue = dispatch_queue_create("Test Q", NULL);
	int backToken, exitToken;
	int allowedToken, disallowedToken;
	notify_register_dispatch(allowedName, &allowedToken, queue, ^(__unused int token){});
	notify_register_dispatch(disallowedName, &disallowedToken, queue, ^(__unused int token){});

    T_LOG("In helper process");

	notify_register_dispatch(postBackName, &backToken, queue, ^(__unused int token){
        T_LOG("Got post back in helper process");

		notify_post(allowedName);
		notify_post(disallowedName);
		notify_set_state(allowedToken, 72);
		notify_set_state(disallowedToken, 72);
	});

	notify_register_dispatch(exitName, &exitToken, queue, ^(__unused int token){
        T_LOG("Got exit in helper process");
		exit(0);
	});

	while(1) {
		sleep(1);
	}

    T_LOG("helper process returning");
}


T_HELPER_DECL(notify_sandbox_runner, "notify sandbox test runner") {
	uint64_t state;
	uint32_t status;

	char *sberr = NULL;
	const char *sandbox_name = "com.apple.notify-tests";
	if (sandbox_init(sandbox_name, SANDBOX_NAMED, &sberr) < 0) {
		T_FAIL("sandbox_init %s failed: %s", sandbox_name, sberr);
		sandbox_free_error(sberr);
		return;
	}

	notify_set_options(NOTIFY_OPT_DISPATCH | NOTIFY_OPT_REGEN | NOTIFY_OPT_FILTERED);

    volatile __block bool allowedToggle = false, disallowedToggle = false;
	int allowedToken, disallowedToken;
	dispatch_queue_t queue = dispatch_queue_create("Test Q", NULL);

	status = notify_register_dispatch(allowedName, &allowedToken, queue, ^(__unused int token){
        T_LOG("Received post to allowed name");
		allowedToggle = true;
	});
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "Register allowed token");

	status = notify_register_dispatch(disallowedName, &disallowedToken, queue, ^(__unused int token){
        T_LOG("Received post to disallowed name");
		disallowedToggle = true;
	});
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "Register disallowed token");

	sleep(1);

	T_ASSERT_EQ(allowedToggle, false, "check allowed token before posts");
	T_ASSERT_EQ(disallowedToggle, false, "check disallowed token before posts");

    status = notify_post(allowedName);
    T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "post allowed token");
    status = notify_post(disallowedName);
    T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "post disallowed token");
    status = notify_set_state(allowedToken, 42);
    T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "set state allowed token");
    status = notify_set_state(disallowedToken, 42);
    T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "set state disallowed token");

	sleep(1);

	T_ASSERT_EQ(allowedToggle, true, "check allowed token after posts");
	allowedToggle = false;
	T_ASSERT_EQ(disallowedToggle, false, "check disallowed token before posts");

	status = notify_get_state(allowedToken, &state);
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "expect get state to succeed");
	T_ASSERT_EQ(state, 42, "get state after set in process");
	status = notify_get_state(disallowedToken, &state);
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "expect get state to succeed");
	T_ASSERT_EQ(state, 0, "get state after set in process");

	notify_post(postBackName);

	sleep(2);

	T_ASSERT_EQ(allowedToggle, true, "check allowed token after posts from other process");
	T_ASSERT_EQ(disallowedToggle, true, "check disallowed token before posts from other process");

	status = notify_get_state(allowedToken, &state);
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "expect get state to succeed");
	T_ASSERT_EQ(state, 72, "get state after set in process");
	status = notify_get_state(disallowedToken, &state);
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "expect get state to succeed");
	T_ASSERT_EQ(state, 72, "get state after set in process");

	notify_post(exitName);

	T_PASS("Complete");
}


T_DECL(notify_sandbox,
       "notify sandbox",
       T_META("owner", "Core Darwin Daemons & Tools"),
       T_META("as_root", "true"),
       T_META_TIMEOUT(50))
{
#if !TARGET_OS_OSX
    T_PASS("Not supported on this platform");
    exit(0);
#endif

    dt_helper_t helpers[2];
    helpers[0] = dt_child_helper("notify_sandbox_runner");
    helpers[1] = dt_child_helper("notify_sandbox_helper");
    dt_run_helpers(helpers, 2, 1024);
}
