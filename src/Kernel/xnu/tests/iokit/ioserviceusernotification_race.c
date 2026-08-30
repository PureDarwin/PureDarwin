#include <darwintest.h>
#include <darwintest_utils.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <mach/mach_vm.h>
#include <libproc.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

#include "service_helpers.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.iokit"),
	T_META_RUN_CONCURRENTLY(true),
	T_META_ASROOT(true),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IOKit"),
	T_META_OWNER("souvik_b"));

static size_t
absoluteDifference(size_t first, size_t second)
{
	if (first > second) {
		return first - second;
	} else {
		return second - first;
	}
}

static void
notificationReceived(void * refcon __unused, io_iterator_t iter __unused, uint32_t msgType __unused, void * msgArg __unused)
{
	// T_LOG("notification received");
}

struct Context {
	IONotificationPortRef notifyPort;
	io_iterator_t iter;
};

static void
notificationReceived2(void * refcon, io_iterator_t iter __unused, uint32_t msgType __unused, void * msgArg __unused)
{
	struct Context * ctx = (struct Context *)refcon;
	IONotificationPortDestroy(ctx->notifyPort);
	IOObjectRelease(ctx->iter);
	free(ctx);
	T_LOG("notification received, destroyed");
}

T_HELPER_DECL(ioserviceusernotification_race_helper, "ioserviceusernotification_race_helper")
{
	dispatch_async(dispatch_get_main_queue(), ^{
		io_iterator_t iter;
		io_iterator_t iter2;
		IONotificationPortRef notifyPort;
		IONotificationPortRef notifyPort2;
		io_service_t service;

		notifyPort = IONotificationPortCreate(kIOMainPortDefault);
		IONotificationPortSetDispatchQueue(notifyPort, dispatch_get_main_queue());
		notifyPort2 = IONotificationPortCreate(kIOMainPortDefault);
		IONotificationPortSetDispatchQueue(notifyPort2, dispatch_get_main_queue());

		service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("TestIOServiceUserNotificationUserClient"));
		T_ASSERT_NE(service, IO_OBJECT_NULL, "service is nonnull");

		// The first notification object is kept for the lifetime of the helper
		T_ASSERT_MACH_SUCCESS(
			IOServiceAddInterestNotification(notifyPort, service, kIOBusyInterest, notificationReceived, NULL, &iter),
			"add notification");

		struct Context * c = calloc(1, sizeof(struct Context));

		// The second notification object is released after a notification is received
		T_ASSERT_MACH_SUCCESS(
			IOServiceAddInterestNotification(notifyPort2, service, kIOBusyInterest, notificationReceived2, c, &iter2),
			"add notification 2");

		c->notifyPort = notifyPort2;
		c->iter = iter2;

		IOObjectRelease(service);
	});

	dispatch_main();
}

// how many notification objects to create
#define NUM_NOTIFICATION_ITERS 500

// how many times we should run the helper
#define NUM_HELPER_INVOCATIONS 20

// when calling the external method, call in groups of N
#define EXTERNAL_METHOD_GROUP_SIZE 5

// various sleep points in the test
#define WAIT_TIME1_MS 300
#define WAIT_TIME2_MS 300
#define WAIT_TIME3_MS 100
#define WAIT_TIME4_MS 300

// the test involves multiple sleep points. adding together they consume at least
// ((WAIT_TIME1_MS + WAIT_TIME2_MS) * NUM_HELPER_INVOCATIONS + WAIT_TIME3_MS * EXTERNAL_METHOD_GROUP_SIZE + WAIT_TIME4_MS) ms
// this (plus some leeway) should not exceed 30s
static_assert(((WAIT_TIME1_MS + WAIT_TIME2_MS) * NUM_HELPER_INVOCATIONS + WAIT_TIME3_MS * EXTERNAL_METHOD_GROUP_SIZE + WAIT_TIME4_MS) < 28 * MSEC_PER_SEC);

// test is only run on macOS since slower platforms can cause timeout
T_DECL(ioserviceusernotification_race,
    "Test IOServiceUserNotification race",
    T_META_ENABLED(TARGET_OS_OSX),
    T_META_TAG_VM_PREFERRED)
{
	io_service_t service = IO_OBJECT_NULL;
	io_connect_t connect = IO_OBJECT_NULL;
	IONotificationPortRef notifyPort = IONotificationPortCreate(kIOMainPortDefault);
	char test_path[MAXPATHLEN] = {0};
	char * helper_args[] = { test_path, "-n", "ioserviceusernotification_race_helper", NULL };
	io_iterator_t notificationIters[NUM_NOTIFICATION_ITERS];
	pid_t childPids[NUM_HELPER_INVOCATIONS] = {};
	size_t leaks = 1, outCount = 1;


	T_QUIET; T_ASSERT_POSIX_SUCCESS(proc_pidpath(getpid(), test_path, MAXPATHLEN), "get pid path");
	T_QUIET; T_ASSERT_POSIX_SUCCESS(IOTestServiceFindService("TestIOServiceUserNotification", &service),
	    "Find service");
	T_QUIET; T_ASSERT_NE(service, MACH_PORT_NULL, "got service");

	for (size_t i = 0; i < NUM_HELPER_INVOCATIONS; i++) {
		if (connect == IO_OBJECT_NULL) {
			T_ASSERT_MACH_SUCCESS(IOServiceOpen(service, mach_task_self(), 1, &connect), "open service");
		}
		// Call the external method. This re-registers the service
		T_QUIET; T_ASSERT_MACH_SUCCESS(IOConnectCallMethod(connect, 0,
		    NULL, 0, NULL, 0, NULL, 0, NULL, NULL), "call external method");

		usleep(WAIT_TIME1_MS);
		dt_launch_tool(&childPids[i], helper_args, false, NULL, NULL);
		T_LOG("launch helper -> pid %d", childPids[i]);
		usleep(WAIT_TIME2_MS);

		while (true) {
			for (size_t k = 0; k < EXTERNAL_METHOD_GROUP_SIZE; k++) {
				T_QUIET; T_ASSERT_MACH_SUCCESS(IOConnectCallMethod(connect, 0,
				    NULL, 0, NULL, 0, NULL, 0, NULL, NULL), "call external method");
				usleep(WAIT_TIME3_MS);
			}
			if ((random() % 1000) == 0) {
				break;
			}
		}

		T_LOG("kill helper %d", childPids[i]);
		kill(childPids[i], SIGKILL);

		if ((random() % 3) == 0) {
			IOServiceClose(connect);
			connect = IO_OBJECT_NULL;
		}
	}

	if (connect != IO_OBJECT_NULL) {
		IOServiceClose(connect);
		connect = IO_OBJECT_NULL;
	}

	for (size_t i = 0; i < NUM_HELPER_INVOCATIONS; i++) {
		waitpid(childPids[i], NULL, 0);
	}

	// Register for notifications
	for (size_t i = 0; i < sizeof(notificationIters) / sizeof(notificationIters[0]); i++) {
		T_QUIET; T_ASSERT_MACH_SUCCESS(
			IOServiceAddInterestNotification(notifyPort, service, kIOBusyInterest, notificationReceived, NULL, &notificationIters[i]),
			"add notification");
	}

	usleep(WAIT_TIME4_MS);

	// Release the notifications
	for (size_t i = 0; i < sizeof(notificationIters) / sizeof(notificationIters[0]); i++) {
		T_QUIET; T_ASSERT_MACH_SUCCESS(
			IOObjectRelease(notificationIters[i]),
			"remove notification");
		notificationIters[i] = MACH_PORT_NULL;
	}

	T_ASSERT_MACH_SUCCESS(IOServiceOpen(service, mach_task_self(), 1, &connect), "open service");

	T_ASSERT_MACH_SUCCESS(IOConnectCallMethod(connect, 1,
	    NULL, 0, NULL, 0, &leaks, &outCount, NULL, NULL), "call external method");

	T_LOG("IOServiceUserNotification leak count: %llu", leaks);

	// Check for leaks
	T_ASSERT_EQ(leaks, 0, "leaked IOServiceUserNotification");

	IOServiceClose(connect);
	IOObjectRelease(service);
	IONotificationPortDestroy(notifyPort);
}
