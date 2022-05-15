//
//  notify_register_mach_port.c
//  Libnotify
//
//  Created by Brycen Wershing on 10/23/19.
//

#include <stdlib.h>
#include <notify.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <darwintest.h>
#include <signal.h>
#include "../libnotify.h"
#include "notify_private.h"

static bool has_port_been_notified_1, has_port_been_notified_2;
static int key_1_token, key_2_token;

static void port_handler(mach_port_t port)
{
	int tid;
	mach_msg_empty_rcv_t msg;
	kern_return_t status;

	if (port == MACH_PORT_NULL) return;

	memset(&msg, 0, sizeof(msg));
	status = mach_msg(&msg.header, MACH_RCV_MSG, 0, sizeof(msg), port, 0, MACH_PORT_NULL);
	if (status != KERN_SUCCESS) return;

	tid = msg.header.msgh_id;
	if (tid == key_1_token)
	{
		has_port_been_notified_1 = true;
	}
	else if (tid == key_2_token)
	{
		has_port_been_notified_2 = true;
	}
	else
	{
		T_FAIL("port handler should only be called with tokens %d and %d, but it was called with %d",
		       key_1_token, key_2_token, tid);
	}
}

static void run_test()
{
	const char *KEY1 = "com.apple.notify.test.mach_port.1";
	const char *KEY2 = "com.apple.notify.test.mach_port.2";
	int rc;

    // Make sure that posts without an existing registration don't do anything
    // bad, like crash notifyd or break future posts
    notify_post(KEY1);
    notify_post(KEY1);
    notify_post(KEY1);

	mach_port_t watch_port;
	dispatch_source_t port_src;
	dispatch_queue_t watch_queue = dispatch_queue_create("Watch Q", NULL);

	rc = notify_register_mach_port(KEY1, &watch_port, 0, &key_1_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "register mach port should work");

	rc = notify_register_mach_port(KEY2, &watch_port, NOTIFY_REUSE, &key_2_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "register mach port should work");

	port_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, watch_port, 0, watch_queue);
	dispatch_source_set_event_handler(port_src, ^{
		port_handler(watch_port);
	});
	dispatch_activate(port_src);

	has_port_been_notified_1 = false;
	has_port_been_notified_2 = false;
	notify_post(KEY1);
	sleep(1);
	T_ASSERT_EQ(has_port_been_notified_1, true, "mach port 1 should receive notification");
	T_ASSERT_EQ(has_port_been_notified_2, false, "mach port 2 should not receive notification");

	has_port_been_notified_1 = false;
	has_port_been_notified_2 = false;
	notify_post(KEY2);
	sleep(1);
	T_ASSERT_EQ(has_port_been_notified_1, false, "mach port 1 should not receive notification");
	T_ASSERT_EQ(has_port_been_notified_2, true, "mach port 2 should receive notification");

	dispatch_release(port_src);

}

T_DECL(notify_register_mach_port, "Make sure mach port registrations works",
		T_META("owner", "Core Darwin Daemons & Tools"),
       T_META_ASROOT(YES))
{
	run_test();
}

T_DECL(notify_register_mach_port_filtered, "Make sure mach port registrations works",
		T_META("owner", "Core Darwin Daemons & Tools"),
       T_META_ASROOT(YES))
{
	notify_set_options(NOTIFY_OPT_DISPATCH | NOTIFY_OPT_REGEN | NOTIFY_OPT_FILTERED);

	run_test();
}
