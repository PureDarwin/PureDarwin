#include <darwintest.h>
#include <darwintest_utils.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <signal.h>
#include <mach/mach_vm.h>
#include <libproc.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#if defined(__arm64__)
#include <System/arm/cpu_capabilities.h>
#endif /* defined(__arm64__) */

// PT: These two files must be included before IOCircularDataQueueImplementation.h
#include <stdatomic.h>
#include <os/base_private.h>
#include <IOKit/IOCircularDataQueue.h>
#if 0
#include "device_user.h"
#include <../iokit/IOKit/IOCircularDataQueueImplementation.h>
#else
#include <IOKit/IOCircularDataQueueImplementation.h>
#endif

#include "service_helpers.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.iokit"),
	T_META_RUN_CONCURRENTLY(true),
	T_META_ASROOT(true),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IOKit"));

T_DECL(iodataqueues, "Test IODataQueues", T_META_TAG_VM_PREFERRED)
{
	io_service_t service = IO_OBJECT_NULL;
	io_connect_t connect = IO_OBJECT_NULL;

	T_QUIET; T_ASSERT_POSIX_SUCCESS(IOTestServiceFindService("TestIODataQueues", &service),
	    "Find service");
	T_QUIET; T_ASSERT_NE(service, MACH_PORT_NULL, "got service");

	T_ASSERT_MACH_SUCCESS(IOServiceOpen(service, mach_task_self(), 1, &connect), "open service");

	kern_return_t ret;
	IOCircularDataQueue * queue;

	for (int cycle = 0; cycle < 2; cycle++) {
		ret = IOCircularDataQueueCreateWithConnection(kIOCircularDataQueueCreateConsumer, connect, 53, &queue);
#if defined(__arm64__) && defined(__LP64__)
		if (0 == (kHasFeatLSE2 & _get_cpu_capabilities())) {
			assert(kIOReturnUnsupported == ret);
			break;
		} else {
			assert(kIOReturnSuccess == ret);
		}

		char buf[16];
		size_t length = sizeof(buf);
		ret = IOCircularDataQueueCopyLatest(queue, &buf[0], &length);
		assert(kIOReturnSuccess == ret);
		printf("[%ld]%s\n", length, &buf[0]);

		if (0) {
			// requires write access so disabled
			ret = IOCircularDataQueueEnqueue(queue, "goodbye", sizeof("goodbye"));
			assert(kIOReturnSuccess == ret);
		}
		ret = IOCircularDataQueueCopyLatest(queue, &buf[0], &length);
		assert(kIOReturnSuccess == ret);
		printf("[%ld]%s\n", length, &buf[0]);

		ret = IOCircularDataQueueDestroy(&queue);
		assert(kIOReturnSuccess == ret);
#else /* defined(__arm64__) && defined(__LP64__) */
		assert(kIOReturnUnsupported == ret);
		break;
#endif /* !(defined(__arm64__) && defined(__LP64__)) */
	}

	IOObjectRelease(service);
}
