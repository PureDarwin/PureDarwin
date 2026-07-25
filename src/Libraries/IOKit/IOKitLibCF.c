/*
 * Real CF-shaped IOKitLib surface (IOServiceGetMatchingService,
 * IORegistryEntryCreateCFProperty, etc.) - the ABI apps compiled against
 * the real Apple SDK's IOKit/IOKitLib.h actually call (CFDictionaryRef/
 * CFTypeRef based), distinct from PDIOKitLib.h's simplified string-based
 * surface in IOKitLib.c (used internally by the GOP display driver, which
 * never needs CoreFoundation at all).
 *
 * Deliberately compiled WITHOUT CoreFoundation's real headers: this file
 * only needs CFDictionaryRef/CFStringRef/etc to be *some* opaque pointer
 * type (their real layout is private to CF, no caller ever touches it
 * directly) plus the handful of real CF entry points it calls declared
 * with matching signatures - real CoreFoundation.dylib provides the actual
 * bodies at final link time (see nix/pkgs/iokit.nix, which links this
 * object code from the in-tree build against the real corefoundation.nix
 * output). This lets the file build as part of the normal in-tree
 * userland CMake tree (reusing its already-working MIG/XNU-header
 * machinery for device.defs) without needing CF's own generated headers,
 * which only exist after CF's *own* separate CMake configure has run.
 *
 * Property values come back from the kernel as OSSerialize XML fragments
 * (see IOUserClient.cpp's is_io_registry_entry_get_property: just
 * obj->serialize(s), no <plist> document wrapper) - wrap them in one and
 * hand them to CFPropertyListCreateWithData, since CF's OSSerialize XML
 * vocabulary (dict/array/string/data/integer/true/false/key) is the same
 * as CFPropertyList's.
 */
/*
 * Deliberately NOT including PDIOKitLib.h here: it declares the OLD
 * string-based IOServiceMatching/IOServiceGetMatchingService signatures
 * (used by IOKitLib.c / the GOP driver), which conflict with the real
 * CF-based ones this file defines. Just pull in the small set of io_*
 * typedefs this file actually needs directly.
 */
#include <mach/mach.h>
#include <mach/mach_traps.h>
#include <mach/vm_map.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef mach_port_t io_object_t;
typedef io_object_t io_service_t;
typedef io_object_t io_iterator_t;
typedef io_object_t io_registry_entry_t;

#define IO_OBJECT_NULL ((io_object_t)0)
#define IO_SERVICE_NULL ((io_service_t)0)
#define IO_ITERATOR_NULL ((io_iterator_t)0)

typedef char io_name_t[128];
typedef char io_string_t[512];
typedef char io_struct_inband_t[4096];
typedef io_object_t io_connect_t;

#define kIOConnectMethodVarOutputSize ((size_t)-3)

#include "device_user.h"

typedef unsigned char CFBool;
typedef signed long CFIndex;
typedef unsigned long CFOptionFlags;
typedef unsigned long CFStringEncoding;
typedef unsigned long CFTypeID;
typedef const void *CFTypeRef;
typedef const struct __CFAllocator *CFAllocatorRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef struct __CFDictionary *CFMutableDictionaryRef;
typedef const struct __CFData *CFDataRef;

#define kCFStringEncodingUTF8 0x08000100u
#define kCFPropertyListImmutable 0
#define kCFPropertyListMutableContainers 1
#define kIORegistryIterateRecursively 0x00000001u

extern const CFAllocatorRef kCFAllocatorDefault;
extern const void *kCFTypeDictionaryKeyCallBacks;
extern const void *kCFTypeDictionaryValueCallBacks;

extern CFStringRef CFStringCreateWithCString(CFAllocatorRef alloc,
    const char *cStr, CFStringEncoding encoding);
extern CFBool CFStringGetCString(CFStringRef theString, char *buffer,
    CFIndex bufferSize, CFStringEncoding encoding);
extern CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef alloc,
    CFIndex capacity, const void *keyCallBacks, const void *valueCallBacks);
extern void CFDictionarySetValue(CFMutableDictionaryRef dict,
    const void *key, const void *value);
extern CFTypeRef CFDictionaryGetValue(CFDictionaryRef dict, const void *key);
extern void CFRelease(CFTypeRef cf);
extern CFDataRef CFDataCreate(CFAllocatorRef alloc, const uint8_t *bytes,
    CFIndex length);
extern CFTypeRef CFPropertyListCreateWithData(CFAllocatorRef alloc,
    CFDataRef data, CFOptionFlags options, void *format, CFTypeRef *error);

/*
 * These two private keys never leave this file: IOServiceMatching/
 * IOServiceNameMatching build a small real CFMutableDictionaryRef tagged
 * with one of them, and IOServiceGetMatchingService(s) reads it back out
 * to build the actual IOProviderClass/IONameMatch XML matching string the
 * kernel expects (io_string_t, so it must fit in 512 bytes - matching
 * real IOKit's own on-wire format for simple class/name matches). Real
 * fastfetch-style callers only ever pass these straight through, never
 * inspect the dictionary's actual contents, so this doesn't need to
 * support arbitrary matching dictionaries in general - just what
 * IOServiceMatching/IOServiceNameMatching themselves produce.
 */
static CFStringRef pd_key_class;
static CFStringRef pd_key_name;

static void
pd_init_keys(void)
{
	if (pd_key_class == NULL) {
		pd_key_class = CFStringCreateWithCString(kCFAllocatorDefault,
		    "__pd_ioprovider_class", kCFStringEncodingUTF8);
	}
	if (pd_key_name == NULL) {
		pd_key_name = CFStringCreateWithCString(kCFAllocatorDefault,
		    "__pd_ioname_match", kCFStringEncodingUTF8);
	}
}

static CFMutableDictionaryRef
pd_make_matching(CFStringRef key, const char *value)
{
	CFMutableDictionaryRef dict;
	CFStringRef cfValue;

	if (value == NULL) {
		return NULL;
	}
	pd_init_keys();
	dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
	    kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
	if (dict == NULL) {
		return NULL;
	}
	cfValue = CFStringCreateWithCString(kCFAllocatorDefault, value,
	    kCFStringEncodingUTF8);
	if (cfValue == NULL) {
		CFRelease(dict);
		return NULL;
	}
	CFDictionarySetValue(dict, key, cfValue);
	CFRelease(cfValue);
	return dict;
}

CFMutableDictionaryRef
IOServiceMatching(const char *name)
{
	return pd_make_matching(pd_key_class, name);
}

CFMutableDictionaryRef
IOServiceNameMatching(const char *name)
{
	return pd_make_matching(pd_key_name, name);
}

static kern_return_t
pd_matching_to_xml(CFDictionaryRef matching, io_string_t out)
{
	static const char classFmt[] =
	    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
	    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
	    "<plist version=\"1.0\"><dict>"
	    "<key>IOProviderClass</key><string>%s</string>"
	    "</dict></plist>";
	static const char nameFmt[] =
	    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
	    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
	    "<plist version=\"1.0\"><dict>"
	    "<key>IONameMatch</key><string>%s</string>"
	    "</dict></plist>";
	CFTypeRef value;
	char buf[256];
	const char *fmt;
	int len;

	if (matching == NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	pd_init_keys();

	value = CFDictionaryGetValue(matching, pd_key_class);
	fmt = classFmt;
	if (value == NULL) {
		value = CFDictionaryGetValue(matching, pd_key_name);
		fmt = nameFmt;
	}
	if (value == NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	if (!CFStringGetCString((CFStringRef)value, buf, sizeof(buf),
	    kCFStringEncodingUTF8)) {
		return KERN_INVALID_ARGUMENT;
	}

	len = snprintf(out, sizeof(io_string_t), fmt, buf);
	if (len < 0 || (size_t)len >= sizeof(io_string_t)) {
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_SUCCESS;
}

io_service_t
IOServiceGetMatchingService(mach_port_t masterPort, CFDictionaryRef matching)
{
	io_string_t xml;
	io_service_t service = IO_SERVICE_NULL;

	if (pd_matching_to_xml(matching, xml) == KERN_SUCCESS) {
		io_service_get_matching_service(masterPort, xml, &service);
	}
	if (matching) {
		CFRelease(matching);
	}
	return service;
}

kern_return_t
IOServiceGetMatchingServices(mach_port_t masterPort, CFDictionaryRef matching,
    io_iterator_t *existing)
{
	io_string_t xml;
	kern_return_t kr = KERN_INVALID_ARGUMENT;

	if (pd_matching_to_xml(matching, xml) == KERN_SUCCESS) {
		kr = io_service_get_matching_services(masterPort, xml, existing);
	}
	if (matching) {
		CFRelease(matching);
	}
	return kr;
}

/* Defined below; resolves kIOMasterPortDefault to the real master device port. */
static mach_port_t pd_default_master_port(void);

io_registry_entry_t
IORegistryEntryFromPath(mach_port_t masterPort, const io_string_t path)
{
	io_registry_entry_t entry = IO_OBJECT_NULL;
	io_string_t pathCopy;
	mach_port_t usePort;
	size_t i;

	/* The kernel's is_io_registry_entry_from_path rejects any port that isn't
	 * master_device_port with kIOReturnNotPrivileged, so kIOMasterPortDefault
	 * (MACH_PORT_NULL) must be resolved to the real master port first - same as
	 * IORegistryGetRootEntry/IOServiceGetMatchingService do. Without this, a
	 * default-port lookup (e.g. IOKitWaitQuiet's "IOService:/") silently
	 * returns a null entry. */
	usePort = (masterPort == MACH_PORT_NULL) ? pd_default_master_port() : masterPort;

	for (i = 0; i < sizeof(pathCopy) - 1 && path[i] != '\0'; i++) {
		pathCopy[i] = path[i];
	}
	pathCopy[i] = '\0';

	io_registry_entry_from_path(usePort, pathCopy, &entry);

	if (usePort != MACH_PORT_NULL && usePort != masterPort) {
		mach_port_deallocate(mach_task_self(), usePort);
	}
	return entry;
}

kern_return_t
IORegistryEntryGetName(io_registry_entry_t entry, io_name_t name)
{
	return io_registry_entry_get_name(entry, name);
}

kern_return_t
IORegistryEntryGetRegistryEntryID(io_registry_entry_t entry,
    uint64_t *entryID)
{
	return io_registry_entry_get_registry_entry_id(entry, entryID);
}

kern_return_t
IORegistryEntryGetParentEntry(io_registry_entry_t entry,
    const io_name_t plane, io_registry_entry_t *parent)
{
	io_iterator_t iterator = IO_ITERATOR_NULL;
	io_name_t planeCopy;
	kern_return_t kr;
	size_t i;

	for (i = 0; i < sizeof(planeCopy) - 1 && plane[i] != '\0'; i++) {
		planeCopy[i] = plane[i];
	}
	planeCopy[i] = '\0';

	kr = io_registry_entry_get_parent_iterator(entry, planeCopy, &iterator);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	*parent = IO_OBJECT_NULL;
	io_iterator_next(iterator, parent);
	mach_port_deallocate(mach_task_self(), iterator);

	return (*parent != IO_OBJECT_NULL) ? KERN_SUCCESS : KERN_FAILURE;
}

/*
 * Wrap a raw OSSerialize XML fragment (no <plist> document wrapper - see
 * this file's header comment) in one, and parse it into a real CFTypeRef
 * tree via CoreFoundation's own plist XML parser.
 */
static CFTypeRef
pd_parse_property_xml(const char *bytes, mach_msg_type_number_t len,
    CFOptionFlags options)
{
	static const char header[] =
	    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
	    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
	    "<plist version=\"1.0\">";
	static const char footer[] = "</plist>";
	char *wrapped;
	size_t headerLen = sizeof(header) - 1;
	size_t footerLen = sizeof(footer) - 1;
	size_t total = headerLen + len + footerLen;
	CFDataRef data;
	CFTypeRef result;

	wrapped = malloc(total);
	if (wrapped == NULL) {
		return NULL;
	}
	memcpy(wrapped, header, headerLen);
	memcpy(wrapped + headerLen, bytes, len);
	memcpy(wrapped + headerLen + len, footer, footerLen);

	data = CFDataCreate(kCFAllocatorDefault, (const uint8_t *)wrapped, (CFIndex)total);
	free(wrapped);
	if (data == NULL) {
		return NULL;
	}

	result = CFPropertyListCreateWithData(kCFAllocatorDefault, data,
	    options, NULL, NULL);
	CFRelease(data);
	return result;
}

CFTypeRef
IORegistryEntryCreateCFProperty(io_registry_entry_t entry, CFStringRef key,
    CFAllocatorRef allocator, uint32_t options)
{
	io_name_t name;
	char *bytes = NULL;
	mach_msg_type_number_t bytesCnt = 0;
	CFTypeRef result;

	(void)allocator;
	(void)options;

	if (!CFStringGetCString(key, name, sizeof(name), kCFStringEncodingUTF8)) {
		return NULL;
	}

	if (io_registry_entry_get_property(entry, name, &bytes, &bytesCnt)
	    != KERN_SUCCESS || bytes == NULL || bytesCnt == 0) {
		return NULL;
	}

	result = pd_parse_property_xml(bytes, bytesCnt, kCFPropertyListImmutable);
	vm_deallocate(mach_task_self(), (vm_address_t)bytes, bytesCnt);
	return result;
}

kern_return_t
IORegistryEntryCreateCFProperties(io_registry_entry_t entry,
    CFMutableDictionaryRef *properties, CFAllocatorRef allocator,
    uint32_t options)
{
	char *bytes = NULL;
	mach_msg_type_number_t bytesCnt = 0;
	CFTypeRef result;

	(void)allocator;
	(void)options;

	*properties = NULL;

	if (io_registry_entry_get_properties(entry, &bytes, &bytesCnt)
	    != KERN_SUCCESS || bytes == NULL || bytesCnt == 0) {
		return KERN_FAILURE;
	}

	result = pd_parse_property_xml(bytes, bytesCnt, kCFPropertyListMutableContainers);
	vm_deallocate(mach_task_self(), (vm_address_t)bytes, bytesCnt);
	if (result == NULL) {
		return KERN_FAILURE;
	}
	*properties = (CFMutableDictionaryRef)result;
	return KERN_SUCCESS;
}

/* --- Shared plumbing also needed by these entry points (same bodies as
 * IOKitLib.c; duplicated rather than shared across the two static-library
 * targets to keep each one a self-contained compilation unit). --- */

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

kern_return_t
IOObjectRelease(io_object_t object)
{
	return mach_port_deallocate(mach_task_self(), object);
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

static kern_return_t
pd_IOConnectCallMethod(io_connect_t connect, uint32_t selector,
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
	return pd_IOConnectCallMethod(connect, selector, NULL, 0, inputStruct,
	    inputStructCnt, NULL, NULL, outputStruct, outputStructCnt);
}

kern_return_t
IORegistryEntryGetChildEntry(io_registry_entry_t entry,
    const io_name_t plane, io_registry_entry_t *child)
{
	io_iterator_t iterator = IO_ITERATOR_NULL;
	io_name_t planeCopy;
	kern_return_t kr;
	size_t i;

	for (i = 0; i < sizeof(planeCopy) - 1 && plane[i] != '\0'; i++) {
		planeCopy[i] = plane[i];
	}
	planeCopy[i] = '\0';

	kr = io_registry_entry_get_child_iterator(entry, planeCopy, &iterator);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	*child = IO_OBJECT_NULL;
	io_iterator_next(iterator, child);
	mach_port_deallocate(mach_task_self(), iterator);

	return (*child != IO_OBJECT_NULL) ? KERN_SUCCESS : KERN_FAILURE;
}

uint32_t
IOObjectGetKernelRetainCount(io_object_t object)
{
	uint32_t count;

	if (io_object_get_retain_count(object, &count) != KERN_SUCCESS) {
		count = 0;
	}
	return count;
}

uint32_t
IOObjectGetRetainCount(io_object_t object)
{
	return IOObjectGetKernelRetainCount(object);
}

kern_return_t
IORegistryEntryGetPath(io_registry_entry_t entry, const io_name_t plane,
    io_string_t path)
{
	return io_registry_entry_get_path(entry, (char *)plane, path);
}

kern_return_t
IORegistryEntryGetNameInPlane(io_registry_entry_t entry,
    const io_name_t plane, io_name_t name)
{
	if (plane == NULL) {
		plane = "";
	}
	return io_registry_entry_get_name_in_plane(entry, (char *)plane, name);
}

kern_return_t
IORegistryEntryGetLocationInPlane(io_registry_entry_t entry,
    const io_name_t plane, io_name_t location)
{
	if (plane == NULL) {
		plane = "";
	}
	return io_registry_entry_get_location_in_plane(entry, (char *)plane, location);
}

kern_return_t
IOServiceGetBusyStateAndTime(io_service_t service, uint64_t *state,
    uint32_t *busy_state, uint64_t *accumulated_busy_time)
{
	kern_return_t kr = io_service_get_state(service, state, busy_state,
	    accumulated_busy_time);

	if (kr != KERN_SUCCESS) {
		*state = 0;
		*busy_state = 0;
		*accumulated_busy_time = 0;
	}
	return kr;
}

/*
 * Only the two plain (non-recursive-binary, non-buf) branches of the real
 * IOKitUser IORegistryEntrySearchCFProperty are implemented here - this
 * project's property pipeline always goes through the OSSerialize XML path
 * (pd_parse_property_xml above), never the binary-plist fast path real
 * macOS also supports.
 */
CFTypeRef
IORegistryEntrySearchCFProperty(io_registry_entry_t entry,
    const io_name_t plane, CFStringRef key, CFAllocatorRef allocator,
    uint32_t options)
{
	io_name_t name;
	char *bytes = NULL;
	mach_msg_type_number_t bytesCnt = 0;
	kern_return_t kr;
	CFTypeRef result;

	(void)allocator;

	if (!CFStringGetCString(key, name, sizeof(name), kCFStringEncodingUTF8)) {
		return NULL;
	}

	if (options & kIORegistryIterateRecursively) {
		kr = io_registry_entry_get_property_recursively(entry,
		    (char *)plane, name, options, &bytes, &bytesCnt);
	} else {
		kr = io_registry_entry_get_property(entry, name, &bytes, &bytesCnt);
	}

	if (kr != KERN_SUCCESS || bytes == NULL || bytesCnt == 0) {
		return NULL;
	}

	result = pd_parse_property_xml(bytes, bytesCnt, kCFPropertyListImmutable);
	vm_deallocate(mach_task_self(), (vm_address_t)bytes, bytesCnt);
	return result;
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
	for (i = 0; i < sizeof(name) - 1 && className[i] != '\0'; i++) {
		name[i] = className[i];
	}
	name[i] = '\0';

	return io_object_conforms_to(object, name, conforms);
}

/*
 * The real _IOObjectGetClass/_IOObjectConformsTo take a class-name-override
 * "options" bitmask (kIOClassNameOverrideNone and friends) that negotiates
 * a newer kernel protocol for translating renamed IOKit classes back to
 * their historical names. Our device.defs only has the plain
 * io_object_get_class/io_object_conforms_to routines - the ones a kernel
 * with no override-negotiation support would use - so these just forward
 * to them and ignore options; that IS the real "no override" behavior,
 * not a stand-in for missing functionality.
 */
kern_return_t
_IOObjectGetClass(io_object_t object, uint64_t options, io_name_t className)
{
	(void)options;
	return io_object_get_class(object, className);
}

boolean_t
_IOObjectConformsTo(io_object_t object, const io_name_t className,
    uint64_t options)
{
	boolean_t conforms = FALSE;

	(void)options;
	io_object_conforms_to(object, (char *)className, &conforms);
	return conforms;
}

static mach_port_t
pd_default_master_port(void)
{
	mach_port_t masterPort;

	if (IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS) {
		masterPort = MACH_PORT_NULL;
	}
	return masterPort;
}

io_registry_entry_t
IORegistryGetRootEntry(mach_port_t masterPort)
{
	kern_return_t kr;
	mach_port_t usePort;
	io_registry_entry_t entry = IO_OBJECT_NULL;

	usePort = (masterPort == MACH_PORT_NULL) ? pd_default_master_port() : masterPort;

	kr = io_registry_get_root_entry(usePort, &entry);
	if (kr != KERN_SUCCESS) {
		entry = IO_OBJECT_NULL;
	}

	if (usePort != MACH_PORT_NULL && usePort != masterPort) {
		mach_port_deallocate(mach_task_self(), usePort);
	}
	return entry;
}

kern_return_t
IORegistryEntryGetChildIterator(io_registry_entry_t entry,
    const io_name_t plane, io_iterator_t *iterator)
{
	return io_registry_entry_get_child_iterator(entry, (char *)plane, iterator);
}

CFStringRef
_IOObjectCopyClass(io_object_t object, uint64_t options)
{
	io_name_t name;
	CFStringRef str = NULL;

	if (!object) {
		return NULL;
	}
	_IOObjectGetClass(object, options, name);
	str = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
	return str;
}

CFStringRef
IOObjectCopyClass(io_object_t object)
{
	return _IOObjectCopyClass(object, 0);
}

CFStringRef
IOObjectCopySuperclassForClass(CFStringRef classname)
{
	io_name_t origName, superName;
	CFStringRef str = NULL;
	mach_port_t masterPort;
	kern_return_t kr;

	if (classname == NULL) {
		return NULL;
	}
	if (!CFStringGetCString(classname, origName, sizeof(origName), kCFStringEncodingUTF8)) {
		return NULL;
	}

	masterPort = pd_default_master_port();
	kr = io_object_get_superclass(masterPort, origName, superName);
	if (masterPort != MACH_PORT_NULL) {
		mach_port_deallocate(mach_task_self(), masterPort);
	}

	if (kr == KERN_SUCCESS) {
		str = CFStringCreateWithCString(kCFAllocatorDefault, superName, kCFStringEncodingUTF8);
	}
	return str;
}
