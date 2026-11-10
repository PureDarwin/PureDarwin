// IOKitLib.h - IOKitLib.h for Argon on Linux: the registry calls PureDarwin's apps make, answered from sysfs
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef mach_port_t io_object_t;
typedef io_object_t io_iterator_t;
typedef io_object_t io_service_t;
typedef io_object_t io_registry_entry_t;
typedef char io_name_t[128];
typedef char io_string_t[512];
typedef UInt32 IOOptionBits;
#define IO_OBJECT_NULL ((io_object_t)0)
#define kIOMasterPortDefault ((mach_port_t)0)
#define kIOMainPortDefault ((mach_port_t)0)
#define kIOServicePlane "IOService"
#define kIORegistryIterateRecursively 0x00000001
#define kIORegistryIterateParents 0x00000002

CFMutableDictionaryRef IOServiceMatching(const char *name);
kern_return_t IOServiceGetMatchingServices(mach_port_t main, CFDictionaryRef matching, io_iterator_t *existing);
io_service_t IOServiceGetMatchingService(mach_port_t main, CFDictionaryRef matching);
io_object_t IOIteratorNext(io_iterator_t iterator);
kern_return_t IOObjectRelease(io_object_t object);
kern_return_t IOObjectRetain(io_object_t object);
kern_return_t IOObjectGetClass(io_object_t object, io_name_t className);
CFTypeRef IORegistryEntryCreateCFProperty(io_registry_entry_t entry, CFStringRef key, CFAllocatorRef allocator, IOOptionBits options);
kern_return_t IORegistryEntryCreateCFProperties(io_registry_entry_t entry, CFMutableDictionaryRef *properties, CFAllocatorRef allocator, IOOptionBits options);
kern_return_t IORegistryEntryGetName(io_registry_entry_t entry, io_name_t name);
kern_return_t IORegistryEntryGetChildEntry(io_registry_entry_t entry, const io_name_t plane, io_registry_entry_t *child);
io_registry_entry_t IORegistryEntryFromPath(mach_port_t main, const io_string_t path);
io_registry_entry_t IORegistryGetRootEntry(mach_port_t main);
kern_return_t IORegistryCreateIterator(mach_port_t main, const io_name_t plane, IOOptionBits options, io_iterator_t *iterator);
#ifdef __cplusplus
}
#endif
