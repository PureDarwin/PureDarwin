#include <stddef.h>
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/clock_types.h>

typedef mach_port_t io_object_t;
typedef io_object_t io_service_t;
typedef io_object_t io_registry_entry_t;

extern kern_return_t IOObjectRelease(io_object_t object);
extern io_registry_entry_t IORegistryEntryFromPath(mach_port_t masterPort,
    const char *path);
/* Kernel MIG routine (device.defs io_service_wait_quiet -> is_io_service_wait_quiet):
 * does the real thread-blocking IOService::waitQuiet() on the given service. */
extern kern_return_t io_service_wait_quiet(io_object_t service,
    mach_timespec_t wait_time);

kern_return_t
IOKitWaitQuiet(mach_port_t masterPort, mach_timespec_t *waitTime)
{
	mach_timespec_t defaultWait = { 0, (int)-1 };
	io_registry_entry_t root;
	kern_return_t kr;

	root = IORegistryEntryFromPath(masterPort, "IOService:/");
	if (root == MACH_PORT_NULL) {
		return KERN_FAILURE;
	}

	if (waitTime == NULL) {
		waitTime = &defaultWait;
	}

	kr = io_service_wait_quiet(root, *waitTime);
	IOObjectRelease(root);
	return kr;
}
