/*
 * Copyright (c) 1998-2010 Apple Computer, Inc. All rights reserved.
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

#include "IOAudioToggleControl.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"

#define super IOAudioControl

OSDefineMetaClassAndStructors(IOAudioToggleControl, IOAudioControl)
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 0);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 1);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 2);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 3);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 4);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 5);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 6);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 7);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 8);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 9);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 10);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 11);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 12);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 13);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 14);
OSMetaClassDefineReservedUnused(IOAudioToggleControl, 15);

// New code added here
IOAudioToggleControl *IOAudioToggleControl::createPassThruMuteControl (bool initialValue,
                                               UInt32 channelID,
                                               const char *channelName,
                                               UInt32 cntrlID)
{
    return create(initialValue, channelID, channelName, cntrlID, kIOAudioToggleControlSubTypeMute, kIOAudioControlUsagePassThru);
}

// Original code...
IOAudioToggleControl *IOAudioToggleControl::create(bool initialValue,
                                               UInt32 channelID,
                                               const char *channelName,
                                               UInt32 cntrlID,
                                               UInt32 subType,
                                               UInt32 usage)
{
    IOAudioToggleControl *control;

    control = new IOAudioToggleControl;

    if (control) {
        if (!control->init(initialValue, channelID, channelName, cntrlID, subType, usage)) {
             control->release();
             control = 0;
        }
    }

    return control;
}

IOAudioToggleControl *IOAudioToggleControl::createMuteControl(bool initialValue,
                                                                UInt32 channelID,
                                                                const char *channelName,
                                                                UInt32 cntrlID,
                                                                UInt32 usage)
{
    return create(initialValue, channelID, channelName, cntrlID, kIOAudioToggleControlSubTypeMute, usage);
}

bool IOAudioToggleControl::init(bool initialValue,
                              UInt32 channelID,
                              const char *channelName,
                              UInt32 cntrlID,
                              UInt32 subType,
                              UInt32 usage,
                              OSDictionary *properties)
{
    bool result = false;
    OSNumber *number;
    
    number = OSNumber::withNumber((initialValue == 0) ? 0 : 1, 8);
    
    if (number) {
    	result = super::init(kIOAudioControlTypeToggle, number, channelID, channelName, cntrlID, subType, usage, properties);
        
        number->release();
    }
    
    return result;
}

