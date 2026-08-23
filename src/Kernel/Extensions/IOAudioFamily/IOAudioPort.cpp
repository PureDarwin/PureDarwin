/*
 * Copyright (c) 1998-2014 Apple Computer, Inc. All rights reserved.
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

#include "IOAudioPort.h"
#include "IOAudioControl.h"
#include "IOAudioDevice.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"
#include "IOAudioDebug.h"
// Unused macros in open source
#define AudioTrace_Start(a, b, c, d, e, f)
#define AudioTrace_End(a, b, c, d, e, f)
#define AudioTrace(a, b, c, d, e, f)

#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>

#include <IOKit/IOLib.h>

#define super IOService

OSDefineMetaClassAndStructors(IOAudioPort, IOService)
OSMetaClassDefineReservedUnused(IOAudioPort, 0);
OSMetaClassDefineReservedUnused(IOAudioPort, 1);
OSMetaClassDefineReservedUnused(IOAudioPort, 2);
OSMetaClassDefineReservedUnused(IOAudioPort, 3);
OSMetaClassDefineReservedUnused(IOAudioPort, 4);
OSMetaClassDefineReservedUnused(IOAudioPort, 5);
OSMetaClassDefineReservedUnused(IOAudioPort, 6);
OSMetaClassDefineReservedUnused(IOAudioPort, 7);
OSMetaClassDefineReservedUnused(IOAudioPort, 8);
OSMetaClassDefineReservedUnused(IOAudioPort, 9);
OSMetaClassDefineReservedUnused(IOAudioPort, 10);
OSMetaClassDefineReservedUnused(IOAudioPort, 11);
OSMetaClassDefineReservedUnused(IOAudioPort, 12);
OSMetaClassDefineReservedUnused(IOAudioPort, 13);
OSMetaClassDefineReservedUnused(IOAudioPort, 14);
OSMetaClassDefineReservedUnused(IOAudioPort, 15);
OSMetaClassDefineReservedUnused(IOAudioPort, 16);
OSMetaClassDefineReservedUnused(IOAudioPort, 17);
OSMetaClassDefineReservedUnused(IOAudioPort, 18);
OSMetaClassDefineReservedUnused(IOAudioPort, 19);
OSMetaClassDefineReservedUnused(IOAudioPort, 20);
OSMetaClassDefineReservedUnused(IOAudioPort, 21);
OSMetaClassDefineReservedUnused(IOAudioPort, 22);
OSMetaClassDefineReservedUnused(IOAudioPort, 23);
OSMetaClassDefineReservedUnused(IOAudioPort, 24);
OSMetaClassDefineReservedUnused(IOAudioPort, 25);
OSMetaClassDefineReservedUnused(IOAudioPort, 26);
OSMetaClassDefineReservedUnused(IOAudioPort, 27);
OSMetaClassDefineReservedUnused(IOAudioPort, 28);
OSMetaClassDefineReservedUnused(IOAudioPort, 29);
OSMetaClassDefineReservedUnused(IOAudioPort, 30);
OSMetaClassDefineReservedUnused(IOAudioPort, 31);

IOAudioPort *IOAudioPort::withAttributes(UInt32 portType, const char *portName, UInt32 subType, OSDictionary *properties)
{
    IOAudioPort *port;

    port = new IOAudioPort;
    if (port) {
        if (!port->initWithAttributes(portType, portName, subType, properties)) {
            port->release();
            port = 0;
        }
    }

    return port;
}

bool IOAudioPort::initWithAttributes(UInt32 portType, const char *portName, UInt32 subType, OSDictionary *properties)
{
    if (!init(properties)) {
        return false;
    }

    if (portType == 0) {
        return false;
    }

    audioDevice = 0;
    isRegistered = false;
    
    setType(portType);
    
    if (portName != 0) {
        setName(portName);
    }

    if (subType != 0) {
        setSubType(subType);
    }

    audioControls = OSSet::withCapacity(1);
    if (!audioControls) {
        return false;
    }

    return true;
}

void IOAudioPort::free()
{
    if (audioControls) {
        audioControls->release();
    }

    super::free();
}

void IOAudioPort::setType(UInt32 portType)
{	
    setProperty(kIOAudioPortTypeKey, portType, sizeof(UInt32)*8);
}

void IOAudioPort::setSubType(UInt32 subType)
{
    setProperty(kIOAudioPortSubTypeKey, subType, sizeof(UInt32)*8);
}

void IOAudioPort::setName(const char *portName)
{
    setProperty(kIOAudioPortNameKey, portName);
}

bool IOAudioPort::start(IOService *provider)
{
    AudioTrace_Start(kAudioTIOAudioPort, kTPIOAudioPortStart, (uintptr_t)this, (uintptr_t)provider, 0, 0);
    if (!super::start(provider)) {
        return false;
    }

    if (!(audioDevice = OSDynamicCast(IOAudioDevice, provider))) {
        return false;
    }

    AudioTrace_End(kAudioTIOAudioPort, kTPIOAudioPortStart, (uintptr_t)this, (uintptr_t)provider, true, 0);
    return true;
}

void IOAudioPort::stop(IOService *provider)
{
    deactivateAudioControls();
    super::stop(provider);
}

void IOAudioPort::registerService(IOOptionBits options)
{
    super::registerService(options);

    if (audioControls && !isRegistered) {
        OSCollectionIterator *iterator;

        iterator = OSCollectionIterator::withCollection(audioControls);
	if (iterator) {
            IOAudioControl *control;
    
            while ( (control = (IOAudioControl *)iterator->getNextObject()) ) {
                if (control->getProvider() == this) {
                    control->registerService();
                }
            }
            iterator->release();
	}
    }

    isRegistered = true;
}

IOAudioDevice *IOAudioPort::getAudioDevice()
{
    return audioDevice;
}

IOReturn IOAudioPort::addAudioControl(IOAudioControl *control)
{
    bool controlWasStarted;
    
    if (!control || !audioControls) {
        return kIOReturnBadArgument;
    }

    if (!control->attach(this)) {
        return kIOReturnError;
    }

    controlWasStarted = control->getIsStarted();
    
    if (!controlWasStarted) {
        if (!control->start(this)) {
            control->detach(this);
            return kIOReturnError;
        }
    }
    
    audioControls->setObject(control);

    if (isRegistered && !controlWasStarted) {
        control->registerService();
    }

    return kIOReturnSuccess;
}

void IOAudioPort::deactivateAudioControls()
{
    OSCollectionIterator *iterator;

    if (!audioControls) {
        return;
    }

    iterator = OSCollectionIterator::withCollection(audioControls);

    if (iterator) {
        IOAudioControl *control;

        while ( (control = (IOAudioControl *)iterator->getNextObject()) ) {
            // Should we check to see if we're the provider?
            if (!isInactive()) {
                control->terminate();
            }
        }

        iterator->release();
    }

    audioControls->flushCollection();
}
