#include <darwintest.h>
#include <darwintest_multiprocess.h>

#include <notify.h>
#include <xpc/private.h>

T_DECL(notify_matching, "notifyd.matching plist registration",
		T_META_LAUNCHD_PLIST("com.apple.notifyd.test.matching.plist"))
{
	xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_main_queue(),
			^(xpc_object_t event) {
				T_ASSERT_EQ_STR(xpc_dictionary_get_string(event, "Notification"), "com.apple.notifyd.test.matching", NULL);
				T_END;
			});

	uint32_t status = notify_post("com.apple.notifyd.test.matching");
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, NULL);

	dispatch_main();
}

T_DECL(notify_matching_state, "notifyd.matching plist registration with event that carries state",
		T_META_LAUNCHD_PLIST("com.apple.notifyd.test.matching.plist"))
{
	xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_main_queue(),
			^(xpc_object_t event) {
				T_ASSERT_EQ_STR(xpc_dictionary_get_string(event, "Notification"), "com.apple.notifyd.test.matching", NULL);
				T_ASSERT_EQ((int)xpc_dictionary_get_uint64(event, "_State"), 42, NULL);
				T_END;
			});

	int token;
	uint32_t status = notify_register_check("com.apple.notifyd.test.matching", &token);
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, NULL);

	status = notify_set_state(token, 42);
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, NULL);

	status = notify_post("com.apple.notifyd.test.matching");
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, NULL);

	dispatch_main();
}

T_HELPER_DECL(notify_matching_system_impl, "implementation for notify_matching_system")
{
	xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_main_queue(),
			^(xpc_object_t event) {
				T_ASSERT_EQ_STR(xpc_dictionary_get_string(event, "Notification"), "com.apple.notifyd.test.matching", NULL);
				T_END;
			});

	uint32_t status = notify_post("com.apple.notifyd.test.matching");
	T_ASSERT_EQ(status, NOTIFY_STATUS_OK, NULL);

	dispatch_main();
}

T_DECL(notify_matching_system, "notifyd.matching plist registration in the system domain",
		T_META_ASROOT(true))
{
	dt_helper_t helpers[] = {
		dt_launchd_helper_domain("com.apple.notifyd.test.matching.plist", "notify_matching_system_impl", NULL, LAUNCH_SYSTEM_DOMAIN)
	};
	dt_run_helpers(helpers, 1, 60);
}

T_DECL(notify_matching_runtime, "notifyd.matching at runtime",
		T_META("launchd_plist", "RunAtLoad.plist"))
{
	xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_main_queue(),
			^(xpc_object_t event) {
				T_ASSERT_EQ_STR(xpc_dictionary_get_string(event, "Notification"), "com.apple.notifyd.test.matching_runtime", NULL);
				T_END;
			});

	xpc_object_t registration = xpc_dictionary_create(NULL, NULL, 0);
	xpc_dictionary_set_string(registration, "Notification", "com.apple.notifyd.test.matching_runtime");
	xpc_set_event("com.apple.notifyd.matching", "foo", registration);

	// Gross. xpc_set_event is asynchronous, wait for a few sec before posting
	dispatch_after(_dispatch_time_after_sec(5), dispatch_get_main_queue(), ^{
		uint32_t status = notify_post("com.apple.notifyd.test.matching_runtime");
		T_ASSERT_EQ(status, NOTIFY_STATUS_OK, NULL);
	});

	dispatch_main();
}

T_DECL(notify_matching_uid, "notifyd.matching uid registration",
		T_META("launchd_plist", "RunAtLoad.plist"))
{
	char *name = NULL;
	asprintf(&name, "user.uid.%d.com.apple.notifyd.test.matching_uid", getuid());

	xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_main_queue(),
			^(xpc_object_t event) {
				T_ASSERT_EQ_STR(xpc_dictionary_get_string(event, "Notification"), name, NULL);
				T_END;
			});

	xpc_object_t registration = xpc_dictionary_create(NULL, NULL, 0);
	xpc_dictionary_set_string(registration, "Notification", name);
	xpc_set_event("com.apple.notifyd.matching", "foo", registration);

	// Gross. xpc_set_event is asynchronous, wait for a few sec before posting
	dispatch_after(_dispatch_time_after_sec(5), dispatch_get_main_queue(), ^{
		uint32_t status = notify_post(name);
		T_ASSERT_EQ(status, NOTIFY_STATUS_OK, NULL);
	});

	dispatch_main();
}
