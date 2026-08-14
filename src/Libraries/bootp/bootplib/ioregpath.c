/*
 * Copyright (c) 2002-2018 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "ioregpath.h"
#include <IOKit/IOKitLib.h>
#include "symbol_scope.h"

PRIVATE_EXTERN CFDictionaryRef
myIORegistryEntryCopyValue(const char * path)
{
    io_registry_entry_t 	service;
    kern_return_t       	status;
    CFMutableDictionaryRef	properties = NULL;

    service = IORegistryEntryFromPath(kIOMasterPortDefault, path);
    if (service == MACH_PORT_NULL) {
	return (NULL);
    }
    status = IORegistryEntryCreateCFProperties(service,
					       &properties,
					       kCFAllocatorDefault,
					       kNilOptions);
    if (status != KERN_SUCCESS) {
	properties = NULL;
    }
    IOObjectRelease(service);
    return (properties);
}

PRIVATE_EXTERN CFTypeRef
myIORegistryEntryCopyProperty(const char * path, CFStringRef prop)
{
    io_registry_entry_t 	service;
    CFTypeRef			val;

    service = IORegistryEntryFromPath(kIOMasterPortDefault, path);
    if (service == MACH_PORT_NULL) {
	return (NULL);
    }
    val = IORegistryEntryCreateCFProperty(service, prop,
					  kCFAllocatorDefault,
					  kNilOptions);
    IOObjectRelease(service);
    return (val);
}

PRIVATE_EXTERN CFDictionaryRef
myIORegistryEntryBSDNameMatchingCopyValue(const char * devname, Boolean parent)
{
    kern_return_t       	status;
    CFMutableDictionaryRef	properties = NULL;
    io_registry_entry_t 	service;

    service 
	= IOServiceGetMatchingService(kIOMasterPortDefault,
				      IOBSDNameMatching(kIOMasterPortDefault, 0, devname));
    if (service == MACH_PORT_NULL) {
	return (NULL);
    }
    if (parent) {
	io_registry_entry_t	parent_service;
	
	status = IORegistryEntryGetParentEntry(service, kIOServicePlane,
					       &parent_service);
	if (status == KERN_SUCCESS) {
	    status = IORegistryEntryCreateCFProperties(parent_service,
						       &properties,
						       kCFAllocatorDefault,
						       kNilOptions);
	    IOObjectRelease(parent_service);
	}
    }
    else {
	status = IORegistryEntryCreateCFProperties(service,
						   &properties,
						   kCFAllocatorDefault,
						   kNilOptions);
    }
    if (status != KERN_SUCCESS) {
	properties = NULL;
    }
    IOObjectRelease(service);
    return (properties);
}

