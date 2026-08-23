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

#include "IOAudioDebug.h"
#include "IOAudioControl.h"
#include "IOAudioControlUserClient.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"
// Unused macros in open source
#define AudioTrace_Start(a, b, c, d, e, f)
#define AudioTrace_End(a, b, c, d, e, f)
#define AudioTrace(a, b, c, d, e, f)

#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

// <rdar://8518215>
enum
{
	kCommandGateStatus_Normal				= 0,
	kCommandGateStatus_RemovalPending,
	kCommandGateStatus_Invalid
};

#define super IOService

OSDefineMetaClassAndStructors(IOAudioControl, IOService)
OSMetaClassDefineReservedUsed(IOAudioControl, 0);
OSMetaClassDefineReservedUsed(IOAudioControl, 1);
OSMetaClassDefineReservedUsed(IOAudioControl, 2);
OSMetaClassDefineReservedUsed(IOAudioControl, 3);

OSMetaClassDefineReservedUnused(IOAudioControl, 4);
OSMetaClassDefineReservedUnused(IOAudioControl, 5);
OSMetaClassDefineReservedUnused(IOAudioControl, 6);
OSMetaClassDefineReservedUnused(IOAudioControl, 7);
OSMetaClassDefineReservedUnused(IOAudioControl, 8);
OSMetaClassDefineReservedUnused(IOAudioControl, 9);
OSMetaClassDefineReservedUnused(IOAudioControl, 10);
OSMetaClassDefineReservedUnused(IOAudioControl, 11);
OSMetaClassDefineReservedUnused(IOAudioControl, 12);
OSMetaClassDefineReservedUnused(IOAudioControl, 13);
OSMetaClassDefineReservedUnused(IOAudioControl, 14);
OSMetaClassDefineReservedUnused(IOAudioControl, 15);
OSMetaClassDefineReservedUnused(IOAudioControl, 16);
OSMetaClassDefineReservedUnused(IOAudioControl, 17);
OSMetaClassDefineReservedUnused(IOAudioControl, 18);
OSMetaClassDefineReservedUnused(IOAudioControl, 19);
OSMetaClassDefineReservedUnused(IOAudioControl, 20);
OSMetaClassDefineReservedUnused(IOAudioControl, 21);
OSMetaClassDefineReservedUnused(IOAudioControl, 22);
OSMetaClassDefineReservedUnused(IOAudioControl, 23);

// New code

// OSMetaClassDefineReservedUsed(IOAudioControl, 3);
IOReturn IOAudioControl::createUserClient(task_t task, void *securityID, UInt32 taskType, IOAudioControlUserClient **newUserClient, OSDictionary *properties)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioControlUserClient *userClient;
    
    userClient = IOAudioControlUserClient::withAudioControl(this, task, securityID, taskType, properties);
    
    if (userClient) {
        *newUserClient = userClient;
    } else {
        result = kIOReturnNoMemory;
    }
    
    return result;
}

void IOAudioControl::sendChangeNotification(UInt32 notificationType)
{
    OSCollectionIterator *iterator;
    IOAudioControlUserClient *client;
    
    if (!userClients || !isStarted) {
        return;
    }

	// If we're doing a config change, just queue the notification for later.
	if (reserved->providerEngine->configurationChangeInProgress) {
		OSNumber *notificationNumber;
		UInt32		i, count;
		bool		dupe = FALSE;

		if (!reserved->notificationQueue) {
			reserved->notificationQueue = OSArray::withCapacity (1);
			if (!reserved->notificationQueue) {
				return;
			}
		}

		notificationNumber = OSNumber::withNumber (notificationType, sizeof (notificationType) * 8);
		if (!notificationNumber)
			return;

		// Check to see if this is a unique notification, there is no need to send dupes.
		count = reserved->notificationQueue->getCount ();
		for (i = 0; i < count; i++) {
			if (notificationNumber->isEqualTo ((OSNumber *)reserved->notificationQueue->getObject (i))) {
				dupe = TRUE;
				break;		// no need to send duplicate notifications
			}
		}
		if (!dupe) {
			reserved->notificationQueue->setObject (notificationNumber);
		}
		notificationNumber->release ();
	} else {
		iterator = OSCollectionIterator::withCollection(userClients);
		if (iterator) {
			while ( (client = (IOAudioControlUserClient *)iterator->getNextObject()) ) {
				client->sendChangeNotification(notificationType);
			}
	
			iterator->release();
		}
	}
}

void IOAudioControl::sendQueuedNotifications(void)
{
	UInt32				i;
	UInt32				count;

	// Send our the queued notications and release the queue.
	if (reserved && reserved->notificationQueue) {
		count = reserved->notificationQueue->getCount ();
		for (i = 0; i < count; i++) {
			if (!isInactive()) {		// <radr://9320521,9040208>
				sendChangeNotification(((OSNumber *)reserved->notificationQueue->getObject(i))->unsigned32BitValue());
			}
		}
		reserved->notificationQueue->release();
		reserved->notificationQueue = NULL;
	}
}

// Original code here...
IOAudioControl *IOAudioControl::withAttributes(UInt32 type,
                                               OSObject *initialValue,
                                               UInt32 channelID,
                                               const char *channelName,
                                               UInt32 cntrlID,
                                               UInt32 subType,
                                               UInt32 usage)
{
    IOAudioControl *control;

    control = new IOAudioControl;

    if (control) {
        if (!control->init(type, initialValue, channelID, channelName, cntrlID, subType, usage)) {
            control->release();
            control = 0;
        }
    }

    return control;
}

bool IOAudioControl::init(UInt32 _type,
                          OSObject *initialValue,
                          UInt32 newChannelID,
                          const char *channelName,
                          UInt32 cntrlID,
                          UInt32 _subType,
                          UInt32 _usage,
                          OSDictionary *properties)
{
    if (!super::init(properties)) {
        return false;
    }
    
    if (initialValue == NULL) {
        return false;
    }

    if (_type == 0) {
        return false;
    }
    
    setType(_type);

    setChannelID(newChannelID);
    setControlID(cntrlID);

	setSubType(_subType);
    
    if (channelName) {
        setChannelName(channelName);
    }
    
    if (_usage != 0) {
        setUsage(_usage);
    }
    
    _setValue(initialValue);

    userClients = OSSet::withCapacity(1);
    if (!userClients) {
        return false;
    }
    
	reserved = IOMallocType (ExpansionData);
	if (!reserved) {
		return false;
	}

	reserved->providerEngine = NULL;
	reserved->notificationQueue = NULL;
	reserved->commandGateStatus = kCommandGateStatus_Normal;	// <rdar://8518215>
	reserved->commandGateUsage = 0;								// <rdar://8518215>
    isStarted = false;

    return true;
}

void IOAudioControl::setType(UInt32 _type)
{
    this->type = _type;
    setProperty(kIOAudioControlTypeKey, type, sizeof(UInt32)*8);
}

void IOAudioControl::setSubType(UInt32 _subType)
{
    this->subType = _subType;
    setProperty(kIOAudioControlSubTypeKey, subType, sizeof(UInt32)*8);
}

void IOAudioControl::setChannelName(const char *channelName)
{
    setProperty(kIOAudioControlChannelNameKey, channelName);
}

void IOAudioControl::setUsage(UInt32 _usage)
{
    this->usage = _usage;
    setProperty(kIOAudioControlUsageKey, usage, sizeof(UInt32)*8);
}

void IOAudioControl::setCoreAudioPropertyID(UInt32 propertyID)
{
    setProperty(kIOAudioControlCoreAudioPropertyIDKey, propertyID, sizeof(UInt32)*8);
    setUsage(kIOAudioControlUsageCoreAudioProperty);
}

void IOAudioControl::setReadOnlyFlag()
{
    setProperty(kIOAudioControlValueIsReadOnlyKey, (bool)true);
}

UInt32 IOAudioControl::getType()
{
    return type;
}

UInt32 IOAudioControl::getSubType()
{
    return subType;
}

UInt32 IOAudioControl::getUsage()
{
    return usage;
}

void IOAudioControl::free()
{
    audioDebugIOLog(4, "+ IOAudioControl[%p]::free()\n", this);

    if (userClients) {
        // should we do some sort of notification here?
        userClients->release();
        userClients = NULL;
    }

	OSSafeReleaseNULL(valueChangeTarget);
	
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }

        commandGate->release();
        commandGate = NULL;
    }

    if (workLoop) {
        workLoop->release();
        workLoop = NULL;
    }

	if (reserved) {
		OSSafeReleaseNULL(reserved->notificationQueue);

		IOFreeType (reserved, ExpansionData);
		reserved = NULL;
	}
	
	OSSafeReleaseNULL(value);

    super::free();
    audioDebugIOLog(4, "- IOAudioControl[%p]::free()\n", this);
}

bool IOAudioControl::start(IOService *provider)
{
	AudioTrace_Start(kAudioTIOAudioControl, kTPIOAudioControlStart, (uintptr_t)this, (uintptr_t)provider, 0, 0);
    if (!super::start(provider)) {
        return false;
    }

    isStarted = true;
	reserved->providerEngine = OSDynamicCast (IOAudioEngine, provider);

	AudioTrace_End(kAudioTIOAudioControl, kTPIOAudioControlStart, (uintptr_t)this, (uintptr_t)provider, true, 0);
    return true;
}

bool IOAudioControl::attachAndStart(IOService *provider)
{
    bool result = true;
    
    if (attach(provider)) {
        if (!isStarted) {
            result = start(provider);
            if (!result) {
                detach(provider);
            }
        }
    } else {
        result = false;
    }

    return result;
}

void IOAudioControl::stop(IOService *provider)
{
    audioDebugIOLog(4, "+ IOAudioControl[%p]::stop(%p)\n", this, provider);

    if (userClients && (userClients->getCount() > 0)) {
        IOCommandGate *cg;
        
        cg = getCommandGate();
        
		if (cg) {
			cg->runAction(detachUserClientsAction);
		}
    }
    
    if (valueChangeTarget) {
        valueChangeTarget->release();
        valueChangeTarget = NULL;
        valueChangeHandler.intHandler = NULL;
    }
    
	// <rdar://7233118>, <rdar://7029696> Remove the event source here as performing heavy workloop operation in free() could lead
	// to deadlock since the context which free() is called is not known. stop() is called on the workloop, so it is safe to remove 
	// the event source here.
	if (reserved->commandGateUsage == 0) {							// <rdar://8518215>
		reserved->commandGateStatus = kCommandGateStatus_Invalid;	// <rdar://8518215>
		
		if (commandGate) {
			if (workLoop) {
				workLoop->removeEventSource(commandGate);
			}
			
			commandGate->release();
			commandGate = NULL;
		}
	}
	else {	// <rdar://8518215>
		reserved->commandGateStatus = kCommandGateStatus_RemovalPending;
	}

    super::stop(provider);

    isStarted = false;
	
    audioDebugIOLog(4, "- IOAudioControl[%p]::stop(%p)\n", this, provider);
}

bool IOAudioControl::getIsStarted()
{
    return isStarted;
}

IOWorkLoop *IOAudioControl::getWorkLoop()
{
    return workLoop;
}

void IOAudioControl::setWorkLoop(IOWorkLoop *wl)
{
	if (!workLoop) {
		workLoop = wl;
	
		if (workLoop) {
			workLoop->retain();
	
			commandGate = IOCommandGate::commandGate(this);
	
			if (commandGate) {
				workLoop->addEventSource(commandGate);
			}
		}
	}
}

IOCommandGate *IOAudioControl::getCommandGate()
{
    return commandGate;
}

// <rdar://7529580>
IOReturn IOAudioControl::_setValueAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, target);
        if (audioControl) {
            IOCommandGate *cg;
            
            cg = audioControl->getCommandGate();
            
            if (cg) {
				setCommandGateUsage(audioControl, true);	// <rdar://8518215>
                result = cg->runAction(setValueAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(audioControl, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioControl::setValueAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;

    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->setValue((OSObject *)arg1);
        }
    }

    return result;
}

IOReturn IOAudioControl::setValue(OSObject *newValue)
{
    IOReturn result = kIOReturnSuccess;
    
    if (OSDynamicCast(OSNumber, newValue)) {
        audioDebugIOLog(3, "+ IOAudioControl[%p]::setValue(int = %d)\n", this, ((OSNumber *)newValue)->unsigned32BitValue());
		AudioTrace_Start(kAudioTIOAudioControl, kTPIOAudioControlSetValue, (uintptr_t)this, (uintptr_t)newValue, ((OSNumber *)newValue)->unsigned32BitValue(), 0);
    } else {
        audioDebugIOLog(3, "+ IOAudioControl[%p]::setValue(%p)\n", this, newValue);
		AudioTrace_Start(kAudioTIOAudioControl, kTPIOAudioControlSetValue, (uintptr_t)this, (uintptr_t)newValue, 0, 0);
    }

    if (newValue) {
        if (!value || !value->isEqualTo(newValue)) {
            result = validateValue(newValue);
            if (result == kIOReturnSuccess) {
                result = performValueChange(newValue);
                if (result == kIOReturnSuccess) {
                    result = updateValue(newValue);
                } else {
                    audioErrorIOLog("  Error 0x%x received from driver - value not set!\n", result);
                }
            } else {
                audioErrorIOLog("  Error 0x%x - invalid value.\n", result);
            }
        }
    } else {
        result = kIOReturnBadArgument;
    }

    if (OSDynamicCast(OSNumber, newValue)) {
        audioDebugIOLog(3, "- IOAudioControl[%p]::setValue(int = %d) returns 0x%lX\n", this, ((OSNumber *)newValue)->unsigned32BitValue(), (long unsigned int)result );
		AudioTrace_End(kAudioTIOAudioControl, kTPIOAudioControlSetValue, (uintptr_t)this, (uintptr_t)newValue, ((OSNumber *)newValue)->unsigned32BitValue(), result);
    } else {
        audioDebugIOLog(3, "- IOAudioControl[%p]::setValue(%p) returns 0x%lX\n", this, newValue, (long unsigned int)result );
		AudioTrace_End(kAudioTIOAudioControl, kTPIOAudioControlSetValue, (uintptr_t)this, (uintptr_t)newValue, 0, result);
    }

    return result;
}

IOReturn IOAudioControl::setValue(SInt32 intValue)
{
	audioDebugIOLog(3, "+ IOAudioControl[%p]::setValue(SInt = %d)\n", this, intValue);
	AudioTrace_Start(kAudioTIOAudioControl, kTPIOAudioControlSetValue, (uintptr_t)this, (uintptr_t)NULL, intValue, 0);
    IOReturn result = kIOReturnError;
    OSNumber *number;
    
    number = OSNumber::withNumber(intValue, sizeof(SInt32)*8);
    if (number) {
        result = setValue(number);
        number->release();
    }
	
	audioDebugIOLog(3, "- IOAudioControl[%p]::setValue(SInt = %d) returns 0x%lX\n", this, intValue, (long unsigned int)result );
	AudioTrace_End(kAudioTIOAudioControl, kTPIOAudioControlSetValue, (uintptr_t)this, (uintptr_t)NULL, intValue, result);
    return result;
}

IOReturn IOAudioControl::validateValue(OSObject *_value)
{
    return kIOReturnSuccess;
}

IOReturn IOAudioControl::updateValue(OSObject *newValue)
{
    IOReturn result;
    
    result = _setValue(newValue);
    if (result == kIOReturnSuccess) {
        sendValueChangeNotification();
    }
    
    return result;
}

IOReturn IOAudioControl::_setValue(OSObject *newValue)
{
    if (value != newValue) {
        if (value) {
            value->release();
        }
        value = newValue;
        value->retain();
        
        setProperty(kIOAudioControlValueKey, value);
    }
    
    return kIOReturnSuccess;
}

IOReturn IOAudioControl::hardwareValueChanged(OSObject *newValue)
{
    IOReturn result = kIOReturnSuccess;

    if (newValue) {
        if (!value || !value->isEqualTo(newValue)) {
            result = validateValue(newValue);
            if (result == kIOReturnSuccess) {
                result = updateValue(newValue);
            } else {
                audioErrorIOLog("IOAudioControl::hardwareValueChanged - Error 0x%x - invalid value.\n", result);
            }
        }
    } else {
        result = kIOReturnBadArgument;
    }
    
    audioDebugIOLog(4, "+- IOAudioControl[%p]::hardwareValueChanged(%p) returns 0x%lX\n", this, newValue, (long unsigned int)result );
    return result;
}

void IOAudioControl::setValueChangeHandler(IntValueChangeHandler intValueChangeHandler, OSObject *target)
{
    valueChangeHandlerType = kIntValueChangeHandler;
    valueChangeHandler.intHandler = intValueChangeHandler;
    setValueChangeTarget(target);
}

void IOAudioControl::setValueChangeHandler(DataValueChangeHandler dataValueChangeHandler, OSObject *target)
{
    valueChangeHandlerType = kDataValueChangeHandler;
    valueChangeHandler.dataHandler = dataValueChangeHandler;
    setValueChangeTarget(target);
}

void IOAudioControl::setValueChangeHandler(ObjectValueChangeHandler objectValueChangeHandler, OSObject *target)
{
    valueChangeHandlerType = kObjectValueChangeHandler;
    valueChangeHandler.objectHandler = objectValueChangeHandler;
    setValueChangeTarget(target);
}

void IOAudioControl::setValueChangeTarget(OSObject *target)
{
    if (target) {
        target->retain();
    }
    
    if (valueChangeTarget) {
        valueChangeTarget->release();
    }
    
    valueChangeTarget = target;
}

IOReturn IOAudioControl::performValueChange(OSObject *newValue)
{
    IOReturn result = kIOReturnError;
    
    if (valueChangeHandler.intHandler != NULL) {
        switch(valueChangeHandlerType) {
            case kIntValueChangeHandler:
                OSNumber *oldNumber, *newNumber;
                
                if ((oldNumber = OSDynamicCast(OSNumber, getValue())) == NULL) {
                    audioErrorIOLog("IOAudioControl::performValueChange - Error: can't call handler - int handler set and old value is not an OSNumber.\n");
                    break;
                }
                
                if ((newNumber = OSDynamicCast(OSNumber, newValue)) == NULL) {
                    audioErrorIOLog("IOAudioControl::performValueChange - Error: can't call handler - int handler set and new value is not an OSNumber.\n");
                    break;
                }
                
                result = valueChangeHandler.intHandler(valueChangeTarget, this, oldNumber->unsigned32BitValue(), newNumber->unsigned32BitValue());
                
                break;
            case kDataValueChangeHandler:
                OSData *oldData, *newData;
                const void *oldBytes, *newBytes;
                UInt32 oldSize, newSize;
                
                if (getValue()) {
                    if ((oldData = OSDynamicCast(OSData, getValue())) == NULL) {
                        audioErrorIOLog("IOAudioControl::performValueChange - Error: can't call handler - data handler set and old value is not an OSData.\n");
                        break;
                    }
                    
                    oldBytes = oldData->getBytesNoCopy();
                    oldSize = oldData->getLength();
                } else {
                    oldBytes = NULL;
                    oldSize = 0;
                }
                
                if (newValue) {
                    if ((newData = OSDynamicCast(OSData, newValue)) == NULL) {
                        audioErrorIOLog("IOAudioControl::performValueChange - Error: can't call handler - data handler set and new value is not an OSData.\n");
                        break;
                    }
                    
                    newBytes = newData->getBytesNoCopy();
                    newSize = newData->getLength();
                } else {
                    newBytes = NULL;
                    newSize = 0;
                }
                
                result = valueChangeHandler.dataHandler(valueChangeTarget, this, oldBytes, oldSize, newBytes, newSize);
                
                break;
            case kObjectValueChangeHandler:
                result = valueChangeHandler.objectHandler(valueChangeTarget, this, getValue(), newValue);
                break;
        }
    }

    audioDebugIOLog(4, "+- IOAudioControl[%p]::performValueChange(%p) returns 0x%lX\n", this, newValue, (long unsigned int)result );
    return result;
}

IOReturn IOAudioControl::flushValue()
{
    return performValueChange(getValue());
}

OSObject *IOAudioControl::getValue()
{
    return value;
}

SInt32 IOAudioControl::getIntValue()
{
    OSNumber *number;
    SInt32 intValue = 0;
    
    number = OSDynamicCast(OSNumber, getValue());
    if (number) {
        intValue = (SInt32)number->unsigned32BitValue();
    }
    
    return intValue;
}

const void *IOAudioControl::getDataBytes()
{
    const void *bytes = NULL;
    OSData *data;
    
    data = OSDynamicCast(OSData, getValue());
    if (data) {
        bytes = data->getBytesNoCopy();
    }
    
    return bytes;
}

UInt32 IOAudioControl::getDataLength()
{
    UInt32 length = 0;
    OSData *data;
    
    data = OSDynamicCast(OSData, getValue());
    if (data) {
        length = data->getLength();
    }
    
    return length;
}

void IOAudioControl::sendValueChangeNotification()
{
    OSCollectionIterator *iterator;
    IOAudioControlUserClient *client;
    
    if (!userClients) {
        return;
    }

    iterator = OSCollectionIterator::withCollection(userClients);
    if (iterator) {
        while ( (client = (IOAudioControlUserClient *)iterator->getNextObject()) ) {
            client->sendValueChangeNotification();
        }
        
        iterator->release();
    }
}

void IOAudioControl::setControlID(UInt32 newControlID)
{
    controlID = newControlID;
    setProperty(kIOAudioControlIDKey, newControlID, sizeof(UInt32)*8);
}

UInt32 IOAudioControl::getControlID()
{
    return controlID;
}

void IOAudioControl::setChannelID(UInt32 newChannelID)
{
    channelID = newChannelID;
    setProperty(kIOAudioControlChannelIDKey, newChannelID, sizeof(UInt32)*8);
}

UInt32 IOAudioControl::getChannelID()
{
    return channelID;
}

void IOAudioControl::setChannelNumber(SInt32 channelNumber)
{
    setProperty(kIOAudioControlChannelNumberKey, channelNumber, sizeof(SInt32)*8);
}

IOReturn IOAudioControl::createUserClient(task_t task, void *securityID, UInt32 taskType, IOAudioControlUserClient **newUserClient)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioControlUserClient *userClient;
    
    userClient = IOAudioControlUserClient::withAudioControl(this, task, securityID, taskType);
    
    if (userClient) {
        *newUserClient = userClient;
    } else {
        result = kIOReturnNoMemory;
    }
    
    return result;
}

IOReturn IOAudioControl::newUserClient(task_t task, void *securityID, UInt32 taskType, IOUserClient **handler)
{
	return kIOReturnUnsupported;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

IOReturn IOAudioControl::newUserClient(task_t task, void *securityID, UInt32 taskType, OSDictionary *properties, IOUserClient **handler)
{
    IOReturn					result = kIOReturnSuccess;
    IOAudioControlUserClient *	client = NULL;
	
    audioDebugIOLog(4, "+ IOAudioControl[%p]::newUserClient()\n", this);
	
	*handler = NULL;		// <rdar://8370885>

    if ( !isInactive () )	// <rdar://7324947>
	{
		if ( kIOReturnSuccess != newUserClient ( task, securityID, taskType, handler ) )
		{
			result = createUserClient ( task, securityID, taskType, &client, properties );
			
			if ( ( kIOReturnSuccess == result ) && ( NULL != client ) )
			{
				if ( workLoop )	// <rdar://7324947>
				{
					result = workLoop->runAction ( _addUserClientAction, this, client );	// <rdar://7324947>, <rdar://7529580>
		
					if ( result == kIOReturnSuccess )
					{
						*handler = client;
					}
					else 
					{
						client->release();		// <rdar://8370885>
					}
				}
				else
				{
					client->release();			// <rdar://8370885>
					result = kIOReturnError;
				}
			}
		}
    }
    else	// <rdar://7324947>
    {
        result = kIOReturnNoDevice;
    }
	
    audioDebugIOLog(4, "- IOAudioControl[%p]::newUserClient() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

void IOAudioControl::clientClosed(IOAudioControlUserClient *client)
{
    audioDebugIOLog(4, "+ IOAudioControl[%p]::clientClosed(%p)\n", this, client);

    if (client) {
		if (workLoop) {														// <rdar://7529580>
			workLoop->runAction(_removeUserClientAction, this, client);		// <rdar://7529580>
		}
    }
    audioDebugIOLog(4, "- IOAudioControl[%p]::clientClosed(%p)\n", this, client);
}

// <rdar://7529580>
IOReturn IOAudioControl::_addUserClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, target);
        if (audioControl) {
            IOCommandGate *cg;
            
            cg = audioControl->getCommandGate();
            
            if (cg) {
				setCommandGateUsage(audioControl, true);	// <rdar://8518215>
                result = cg->runAction(addUserClientAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(audioControl, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioControl::addUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;

    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->addUserClient((IOAudioControlUserClient *)arg1);
        }
    }

    return result;
}

// <rdar://7529580>
IOReturn IOAudioControl::_removeUserClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, target);
        if (audioControl) {
            IOCommandGate *cg;
            
            cg = audioControl->getCommandGate();
            
            if (cg) {
				setCommandGateUsage(audioControl, true);	// <rdar://8518215>
                result = cg->runAction(removeUserClientAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(audioControl, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioControl::removeUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;

    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->removeUserClient((IOAudioControlUserClient *)arg1);
        }
    }

    return result;
}

IOReturn IOAudioControl::detachUserClientsAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->detachUserClients();
        }
    }
    
    return result;
}

IOReturn IOAudioControl::addUserClient(IOAudioControlUserClient *newUserClient)
{
    IOReturn					result = kIOReturnSuccess;		// <rdar://8370885>

    audioDebugIOLog(4, "+- IOAudioControl[%p]::addUserClient(%p)\n", this, newUserClient);

    assert(userClients);

	// <rdar://8370885>
	if (!isInactive()) {		
		if (!newUserClient->attach(this)) {
			result = kIOReturnError;
		} else if (!newUserClient->start(this) || !userClients) {
			newUserClient->detach(this);
			result = kIOReturnError;
		} else {
			userClients->setObject(newUserClient);
		}
	} else {
        result = kIOReturnNoDevice;
    }
	
    return result;	// <rdar://8370885>
}

IOReturn IOAudioControl::removeUserClient(IOAudioControlUserClient *userClient)
{
    audioDebugIOLog(4, "+- IOAudioControl[%p]::removeUserClient(%p)\n", this, userClient);

    assert(userClients);

    userClient->retain();
    
    userClients->removeObject(userClient);
    
    if (!isInactive()) {
        userClient->terminate();
    }
    
    userClient->release();

    return kIOReturnSuccess;
}

IOReturn IOAudioControl::detachUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
    assert(userClients);
    
    if (!isInactive()) {
        OSIterator *iterator;
        
        iterator = OSCollectionIterator::withCollection(userClients);
        
        if (iterator) {
            IOAudioControlUserClient *userClient;
            
            while ( (userClient = (IOAudioControlUserClient *)iterator->getNextObject()) ) {
                userClient->terminate();
            }
            
            iterator->release();
        }
    }
    
    userClients->flushCollection();
    
    audioDebugIOLog(4, "+- IOAudioControl[%p]::detachUserClients() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

// <rdar://8518215>
void IOAudioControl::setCommandGateUsage(IOAudioControl *control, bool increment)
{
	if (control->reserved) {
		if (increment) {
			switch (control->reserved->commandGateStatus)
			{
				case kCommandGateStatus_Normal:
				case kCommandGateStatus_RemovalPending:
					control->reserved->commandGateUsage++;
					break;
				case kCommandGateStatus_Invalid:
					// Should never be here. If so, something went bad...
					break;
			}
		}
		else {
			switch (control->reserved->commandGateStatus)
			{
				case kCommandGateStatus_Normal:
					if (control->reserved->commandGateUsage > 0) {
						control->reserved->commandGateUsage--;
					}
					break;
				case kCommandGateStatus_RemovalPending:
					if (control->reserved->commandGateUsage > 0) {
						control->reserved->commandGateUsage--;
						
						if (control->reserved->commandGateUsage == 0) {
							control->reserved->commandGateStatus = kCommandGateStatus_Invalid;
							
							if (control->commandGate) {
								if (control->workLoop) {
									control->workLoop->removeEventSource(control->commandGate);
								}
								
								control->commandGate->release();
								control->commandGate = NULL;
							}
						}
					}
					break;
				case kCommandGateStatus_Invalid:
					// Should never be here. If so, something went bad...
					break;
			}
		}
	}
}

IOReturn IOAudioControl::setProperties(OSObject *properties)
{
    OSDictionary *props;
    IOReturn result = kIOReturnSuccess;

    if (properties && (props = OSDynamicCast(OSDictionary, properties))) {
        OSNumber *number = OSDynamicCast(OSNumber, props->getObject(kIOAudioControlValueKey));
        
        if (number) {
        	if (workLoop) {																// <rdar://7529580>
				result = workLoop->runAction(_setValueAction, this, (void *)number);	// <rdar://7529580>
            } else {
                result = kIOReturnError;
            }			
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}
