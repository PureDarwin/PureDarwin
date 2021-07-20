/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#if !defined(__i386__) && !defined(__x86_64__)

#include <IOKit/system.h>

#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSContainers.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UInt32 IOPCIDevice::ioRead32( UInt16 offset, IOMemoryMap * map )
{
    UInt32      value;

    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return (0);
    }

    value = OSReadLittleInt32( (volatile void *)map->getVirtualAddress(), offset);
    OSSynchronizeIO();

    return (value);
}

UInt16 IOPCIDevice::ioRead16( UInt16 offset, IOMemoryMap * map )
{
    UInt16      value;

    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return (0);
    }

    value = OSReadLittleInt16( (volatile void *)map->getVirtualAddress(), offset);
    OSSynchronizeIO();

    return (value);
}

UInt8 IOPCIDevice::ioRead8( UInt16 offset, IOMemoryMap * map )
{
    UInt32      value;

    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return (0);
    }

    value = ((volatile UInt8 *) map->getVirtualAddress())[ offset ];
    OSSynchronizeIO();

    return (value);
}

void IOPCIDevice::ioWrite32( UInt16 offset, UInt32 value,
                             IOMemoryMap * map )
{
    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return ;
    }

    OSWriteLittleInt32( (volatile void *)map->getVirtualAddress(), offset, value);
    OSSynchronizeIO();
}

void IOPCIDevice::ioWrite16( UInt16 offset, UInt16 value,
                             IOMemoryMap * map )
{
    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return ;
    }

    OSWriteLittleInt16( (volatile void *)map->getVirtualAddress(), offset, value);
    OSSynchronizeIO();
}

void IOPCIDevice::ioWrite8( UInt16 offset, UInt8 value,
                            IOMemoryMap * map )
{
    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return ;
    }

    ((volatile UInt8 *) map->getVirtualAddress())[ offset ] = value;
    OSSynchronizeIO();
}

#endif //  !defined(__i386__) && !defined(__x86_64__)


