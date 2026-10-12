/*
 * Minimal real userspace IOKit client - just the basics needed to look up a
 * service by class name, open a user client connection, and map its VRAM
 * (what a display DDX needs to reach IOGOPFramebuffer). Not a full port of
 * Apple's IOKitLib.h; add routines as real callers need them.
 *
 * Named PDIOKitLib.h (not IOKit/IOKitLib.h) on purpose: the vendored SDK
 * ships a real IOKit.framework/Headers/IOKitLib.h (CoreFoundation-based),
 * and clang's framework-style angle-bracket lookup for <IOKit/IOKitLib.h>
 * resolves to that one, not this file, once -F.../Frameworks is on the
 * search path - silently picking up the wrong header with mismatched
 * (CFDictionaryRef-based) signatures instead of a "file not found" error.
 */
#ifndef _PD_IOKITLIB_H
#define _PD_IOKITLIB_H

#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <stdint.h>
#include <sys/types.h>

__BEGIN_DECLS

typedef mach_port_t io_object_t;
typedef io_object_t io_service_t;
typedef io_object_t io_connect_t;
typedef io_object_t io_iterator_t;
typedef io_object_t io_registry_entry_t;

#define IO_OBJECT_NULL ((io_object_t)0)
#define IO_SERVICE_NULL ((io_service_t)0)
#define IO_ITERATOR_NULL ((io_iterator_t)0)
#define IO_CONNECT_NULL ((io_connect_t)0)

typedef char io_name_t[128];
typedef char io_string_t[512];

#define kIOMapAnywhere 0x00000001u
#define kIOConnectMethodVarOutputSize ((size_t)-3)

extern const mach_port_t kIOMasterPortDefault;

/* Look up the IOKit master port (host_get_io_master). Pass MACH_PORT_NULL
 * for bootstrapPort (the argument is vestigial, kept for API familiarity). */
kern_return_t IOMasterPort(mach_port_t bootstrapPort, mach_port_t *masterPort);

/*
 * Build an IOKit matching-dictionary string for a single
 * "IOProviderClass" == className match. Real IOServiceMatching returns a
 * CFMutableDictionaryRef; this project has no CoreFoundation, so this
 * returns a pointer to an internal XML property-list string instead (the same
 * on-wire format the real client eventually serializes down to). Copy it
 * before another IOServiceMatching call if you need to keep it.
 */
char *IOServiceMatching(const char *className);

kern_return_t IOServiceGetMatchingService(mach_port_t masterPort,
    char *matching, io_service_t *service);

kern_return_t IOServiceGetMatchingServices(mach_port_t masterPort,
    char *matching, io_iterator_t *existing);

kern_return_t IOServiceOpen(io_service_t service, task_t owningTask,
    uint32_t type, io_connect_t *connect);

kern_return_t IOServiceClose(io_connect_t connect);

kern_return_t IOConnectMapMemory64(io_connect_t connect, uint32_t memoryType,
    task_t intoTask, mach_vm_address_t *address, mach_vm_size_t *size,
    uint32_t options);

kern_return_t IOConnectUnmapMemory64(io_connect_t connect,
    uint32_t memoryType, task_t fromTask, mach_vm_address_t address);

kern_return_t IOConnectCallMethod(io_connect_t connect, uint32_t selector,
    const uint64_t *input, uint32_t inputCnt, const void *inputStruct,
    size_t inputStructCnt, uint64_t *output, uint32_t *outputCnt,
    void *outputStruct, size_t *outputStructCnt);

kern_return_t IOConnectCallStructMethod(io_connect_t connect,
    uint32_t selector, const void *inputStruct, size_t inputStructCnt,
    void *outputStruct, size_t *outputStructCnt);

kern_return_t IOConnectCallScalarMethod(io_connect_t connect,
    uint32_t selector, const uint64_t *input, uint32_t inputCnt,
    uint64_t *output, uint32_t *outputCnt);

kern_return_t IOObjectRetain(io_object_t object);

kern_return_t IOObjectGetClass(io_object_t object, io_name_t className);

kern_return_t IOObjectConformsTo(io_object_t object, const char *className,
    boolean_t *conforms);

io_object_t IOIteratorNext(io_iterator_t iterator);

kern_return_t IOIteratorReset(io_iterator_t iterator);

kern_return_t IORegistryGetRootEntry(mach_port_t masterPort,
    io_registry_entry_t *root);

kern_return_t IOServiceWaitQuiet(io_service_t service, mach_timespec_t *waitTime);
kern_return_t IOKitWaitQuiet(mach_port_t masterPort, mach_timespec_t *waitTime);

kern_return_t IORegistryEntryGetName(io_registry_entry_t entry,
    io_name_t name);

kern_return_t IORegistryEntryGetChildIterator(io_registry_entry_t entry,
    const io_name_t plane, io_iterator_t *iterator);

/* Just a mach port deallocate - io_object_t cleanup happens via the
 * kernel's no-more-senders notification (iokit_remove_reference), there is
 * no explicit "release" RPC in the device.defs interface. */
kern_return_t IOObjectRelease(io_object_t object);

__END_DECLS

#endif /* _PD_IOKITLIB_H */
