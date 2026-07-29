/*
 * Copyright (c) 2006-2014 Apple Inc. All rights reserved.
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

#include <IOKit/storage/IOBDBlockStorageDevice.h>

#define super IODVDBlockStorageDevice
OSDefineMetaClassAndAbstractStructors(IOBDBlockStorageDevice, IODVDBlockStorageDevice)

bool IOBDBlockStorageDevice::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    // Ask our superclass' opinion.

    if (super::init(properties) == false)  return false;

    // Create our registry properties.

    setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeBD);

    return true;
}

OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  0);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  1);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  2);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  3);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  4);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  5);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  6);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  7);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  8);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice,  9);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice, 10);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice, 11);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice, 12);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice, 13);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice, 14);
OSMetaClassDefineReservedUnused(IOBDBlockStorageDevice, 15);
