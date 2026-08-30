/*
 * Minimal real userspace IOKit client. See PDIOKitLib.h for scope.
 *
 * Built on the mig-generated device_user.c (from
 * src/Kernel/xnu/osfmk/device/device.defs, subsystem "iokit" base 2800) -
 * the exact same .defs the kernel's already-compiled iokit_server/
 * is_iokit_subsystem dispatch table (in device_server.o, built by xnu's own
 * stock Makefiles) expects. Verified the generated client and the kernel's
 * already-built server agree on the routine table before writing this.
 */
#include <PDIOKitLib.h>
#include <mach/mach.h>
#include <mach/mach_traps.h>
#include <stdio.h>
#include <string.h>

#include "device_user.h"

/* These older client names are absent from the pinned generated SDK headers. */
extern kern_return_t host_get_io_master(host_t, io_main_t *);
extern kern_return_t mach_port_mod_refs(ipc_space_t, mach_port_name_t,
    mach_port_right_t, mach_port_delta_t);
extern kern_return_t mach_port_deallocate(ipc_space_t, mach_port_name_t);

const mach_port_t kIOMasterPortDefault = MACH_PORT_NULL;


extern mach_port_t mach_task_self_;

static void
IOKitEnsureTaskSelf(void)
{
	if (mach_task_self_ == MACH_PORT_NULL) {
		mach_task_self_ = task_self_trap();
	}
}

kern_return_t
IOMasterPort(mach_port_t bootstrapPort, mach_port_t *masterPort)
{
	(void)bootstrapPort;
	IOKitEnsureTaskSelf();
	return host_get_io_master(mach_host_self(), masterPort);
}

char *
IOServiceMatching(const char *className)
{
	static char matching[512];
	static const char fmt[] =
	    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
	    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
	    "<plist version=\"1.0\"><dict>"
	    "<key>IOProviderClass</key><string>%s</string>"
	    "</dict></plist>";
	int len;

	if (className == NULL) {
		return NULL;
	}
	len = snprintf(matching, sizeof(matching), fmt, className);
	if (len < 0 || (size_t)len >= sizeof(matching)) {
		return NULL;
	}
	return matching;
}

kern_return_t
IOServiceGetMatchingService(mach_port_t masterPort, char *matching,
    io_service_t *service)
{
	if (matching == NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	return io_service_get_matching_service(masterPort, matching, service);
}

kern_return_t
IOServiceGetMatchingServices(mach_port_t masterPort, char *matching,
    io_iterator_t *existing)
{
	if (matching == NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	return io_service_get_matching_services(masterPort, matching, existing);
}

kern_return_t
IOServiceOpen(io_service_t service, task_t owningTask, uint32_t type,
    io_connect_t *connect)
{
	kern_return_t result = KERN_FAILURE;
	kern_return_t kr;

	IOKitEnsureTaskSelf();
	if (owningTask == MACH_PORT_NULL) {
		owningTask = mach_task_self_;
	}

	kr = io_service_open_extended(service, owningTask, type, NDR_record,
	    NULL, 0, &result, connect);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	return result;
}

kern_return_t
IOServiceClose(io_connect_t connect)
{
	return io_service_close(connect);
}

kern_return_t
IOConnectMapMemory64(io_connect_t connect, uint32_t memoryType,
    task_t intoTask, mach_vm_address_t *address, mach_vm_size_t *size,
    uint32_t options)
{
	IOKitEnsureTaskSelf();
	if (intoTask == MACH_PORT_NULL) {
		intoTask = mach_task_self_;
	}
	return io_connect_map_memory_into_task(connect, memoryType, intoTask,
	    address, size, options);
}

kern_return_t
IOConnectUnmapMemory64(io_connect_t connect, uint32_t memoryType,
    task_t fromTask, mach_vm_address_t address)
{
	IOKitEnsureTaskSelf();
	if (fromTask == MACH_PORT_NULL) {
		fromTask = mach_task_self_;
	}
	return io_connect_unmap_memory_from_task(connect, memoryType,
	    fromTask, address);
}

kern_return_t
IOConnectCallMethod(io_connect_t connect, uint32_t selector,
    const uint64_t *input, uint32_t inputCnt, const void *inputStruct,
    size_t inputStructCnt, uint64_t *output, uint32_t *outputCnt,
    void *outputStruct, size_t *outputStructCntP)
{
	mach_msg_type_number_t inbandInputSize = 0;
	mach_msg_type_number_t inbandOutputSize = 0;
	mach_vm_address_t oolInput = 0;
	mach_vm_size_t oolInputSize = 0;
	mach_vm_address_t oolOutput = 0;
	mach_vm_size_t oolOutputSize = 0;
	void *inbandInput = NULL;
	void *inbandOutput = NULL;
	uint32_t zeroOutputCnt = 0;
	kern_return_t kr;

	if (inputStruct != NULL && inputStructCnt <= sizeof(io_struct_inband_t)) {
		inbandInput = (void *)inputStruct;
		inbandInputSize = (mach_msg_type_number_t)inputStructCnt;
	} else if (inputStruct != NULL) {
		oolInput = (mach_vm_address_t)(uintptr_t)inputStruct;
		oolInputSize = (mach_vm_size_t)inputStructCnt;
	}

	if (outputCnt == NULL) {
		outputCnt = &zeroOutputCnt;
	}

	if (outputStructCntP != NULL) {
		if (*outputStructCntP == kIOConnectMethodVarOutputSize) {
			return KERN_NOT_SUPPORTED;
		}
		if (*outputStructCntP <= sizeof(io_struct_inband_t)) {
			inbandOutput = outputStruct;
			inbandOutputSize = (mach_msg_type_number_t)*outputStructCntP;
		} else {
			oolOutput = (mach_vm_address_t)(uintptr_t)outputStruct;
			oolOutputSize = (mach_vm_size_t)*outputStructCntP;
		}
	}

	kr = io_connect_method(connect, selector, (uint64_t *)input, inputCnt,
	    inbandInput, inbandInputSize, oolInput, oolInputSize,
	    inbandOutput, &inbandOutputSize, output, outputCnt, oolOutput,
	    &oolOutputSize);

	if (kr == KERN_SUCCESS && outputStructCntP != NULL) {
		if (*outputStructCntP <= sizeof(io_struct_inband_t)) {
			*outputStructCntP = (size_t)inbandOutputSize;
		} else {
			*outputStructCntP = (size_t)oolOutputSize;
		}
	}

	return kr;
}

kern_return_t
IOConnectCallStructMethod(io_connect_t connect, uint32_t selector,
    const void *inputStruct, size_t inputStructCnt, void *outputStruct,
    size_t *outputStructCnt)
{
	return IOConnectCallMethod(connect, selector, NULL, 0, inputStruct,
	    inputStructCnt, NULL, NULL, outputStruct, outputStructCnt);
}

kern_return_t
IOConnectCallScalarMethod(io_connect_t connect, uint32_t selector,
    const uint64_t *input, uint32_t inputCnt, uint64_t *output,
    uint32_t *outputCnt)
{
	return IOConnectCallMethod(connect, selector, input, inputCnt, NULL,
	    0, output, outputCnt, NULL, NULL);
}

kern_return_t
IOObjectRetain(io_object_t object)
{
	kern_return_t kr;

	kr = mach_port_mod_refs(mach_task_self(), object, MACH_PORT_RIGHT_SEND,
	    1);
	if (kr == KERN_INVALID_RIGHT) {
		kr = mach_port_mod_refs(mach_task_self(), object,
		    MACH_PORT_RIGHT_DEAD_NAME, 1);
	}
	return kr;
}

kern_return_t
IOObjectGetClass(io_object_t object, io_name_t className)
{
	return io_object_get_class(object, className);
}

kern_return_t
IOObjectConformsTo(io_object_t object, const char *className,
    boolean_t *conforms)
{
	io_name_t name;
	size_t i;

	if (className == NULL || conforms == NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	for (i = 0; i < sizeof(name); i++) {
		name[i] = className[i];
		if (className[i] == '\0') {
			return io_object_conforms_to(object, name, conforms);
		}
	}
	return KERN_INVALID_ARGUMENT;
}

io_object_t
IOIteratorNext(io_iterator_t iterator)
{
	io_object_t object = IO_OBJECT_NULL;

	if (io_iterator_next(iterator, &object) != KERN_SUCCESS) {
		return IO_OBJECT_NULL;
	}
	return object;
}

kern_return_t
IOIteratorReset(io_iterator_t iterator)
{
	return io_iterator_reset(iterator);
}

kern_return_t
IORegistryGetRootEntry(mach_port_t masterPort, io_registry_entry_t *root)
{
	return io_registry_get_root_entry(masterPort, root);
}

kern_return_t
IORegistryEntryGetName(io_registry_entry_t entry, io_name_t name)
{
	return io_registry_entry_get_name(entry, name);
}

kern_return_t
IORegistryEntryGetChildIterator(io_registry_entry_t entry,
    const io_name_t plane, io_iterator_t *iterator)
{
	io_name_t planeCopy;
	size_t i;

	for (i = 0; i < sizeof(planeCopy); i++) {
		planeCopy[i] = plane[i];
		if (plane[i] == '\0') {
			return io_registry_entry_get_child_iterator(entry,
			    planeCopy, iterator);
		}
	}
	return KERN_INVALID_ARGUMENT;
}

kern_return_t
IOObjectRelease(io_object_t object)
{
	return mach_port_deallocate(mach_task_self(), object);
}
