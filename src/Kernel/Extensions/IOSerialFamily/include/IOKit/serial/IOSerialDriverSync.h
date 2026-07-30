/*
 *Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 *@APPLE_LICENSE_HEADER_START@
 *
 *The contents of this file constitute Original Code as defined in and
 *are subject to the Apple Public Source License Version 1.1 (the
 *"License").  You may not use this file except in compliance with the
 *License.  Please obtain a copy of the License at
 *http://www.apple.com/publicsource and read it before using this file.
 *
 *This Original Code and all software distributed under the License are
 *distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *License for the specific language governing rights and limitations
 *under the License.
 *
 *@APPLE_LICENSE_HEADER_END@
 */

/*  
 * IOSerialDriverSync.h
 * This file contains the abstract parent class for any IOSerialDriver
 * interface, which is a generic serial interface, designed for devices
 * that do character oriented transfers (async) rather than block or
 * packet transfers.  Most notably RS-232, Printer, Mouse, and Keyboard
 * type devices.
 *
 * 2000-10-21	gvdl	Initial real change to IOKit serial family.
 */

#ifndef _SERIAL_IOSERIALDRIVERSYNC_H
#define _SERIAL_IOSERIALDRIVERSYNC_H

#include <IOKit/IOService.h>

class IOSerialDriverSync : public IOService
{
    OSDeclareAbstractStructors(IOSerialDriverSync);

public:
    virtual IOReturn acquirePort(bool sleep, void *refCon) = 0;

    virtual IOReturn releasePort(void *refCon) = 0;

    virtual IOReturn setState(UInt32 state, UInt32 mask, void *refCon) = 0;

    virtual UInt32 getState(void *refCon) = 0;

    virtual IOReturn watchState(UInt32 *state, UInt32 mask, void *refCon) = 0;

    virtual UInt32 nextEvent(void *refCon) = 0;

    virtual IOReturn executeEvent(UInt32 event, UInt32 data, void *refCon) = 0;

    virtual IOReturn requestEvent(UInt32 event, UInt32 *data, void *refCon) = 0;

    virtual IOReturn enqueueEvent(UInt32 event, UInt32 data,
                                  bool sleep, void *refCon) = 0;

    virtual IOReturn dequeueEvent(UInt32 *event, UInt32 *data,
                                  bool sleep, void *refCon) = 0;

    virtual IOReturn enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count,
                                 bool sleep, void *refCon) = 0;

    virtual IOReturn dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count,
                                 UInt32 min, void *refCon) = 0;

OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  0);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  1);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  2);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  3);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  4);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  5);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  6);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  7);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  8);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync,  9);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync, 10);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync, 11);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync, 12);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync, 13);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync, 14);
OSMetaClassDeclareReservedUnused(IOSerialDriverSync, 15);

};

#endif /* !_SERIAL_IOSERIALDRIVERSYNC_H */
