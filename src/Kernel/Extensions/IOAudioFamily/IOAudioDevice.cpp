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
#include "IOAudioDevice.h"
#include "IOAudioEngine.h"
#include "IOAudioPort.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"
#include "IOAudioLevelControl.h"
#include "IOAudioToggleControl.h"
// Unused macros in open source
#define AudioTrace_Start(a, b, c, d, e, f)
#define AudioTrace_End(a, b, c, d, e, f)
#define AudioTrace(a, b, c, d, e, f)

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOKitKeys.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>

#include <sys/sysctl.h>

#define AbsoluteTime_to_scalar(x)       (*(uint64_t *)(x))

/* t1 < = > t2 */
#define CMP_ABSOLUTETIME(t1, t2)    (AbsoluteTime_to_scalar(t1) > AbsoluteTime_to_scalar(t2)? (int)+1 :   \
                                    (AbsoluteTime_to_scalar(t1) < AbsoluteTime_to_scalar(t2)? (int)-1 : 0))

/* t1 += t2 */
#define ADD_ABSOLUTETIME(t1, t2)    (AbsoluteTime_to_scalar(t1) += AbsoluteTime_to_scalar(t2))

/* t1 -= t2 */
#define SUB_ABSOLUTETIME(t1, t2)    (AbsoluteTime_to_scalar(t1) -= AbsoluteTime_to_scalar(t2))

#define NUM_POWER_STATES	2

class IOAudioTimerEvent : public OSObject
{
    friend class IOAudioDevice;

    OSDeclareDefaultStructors(IOAudioTimerEvent)

protected:
    OSObject *	target;
    IOAudioDevice::TimerEvent event;
    AbsoluteTime interval;
};

OSDefineMetaClassAndStructors(IOAudioTimerEvent, OSObject)

class IOAudioEngineEntry : public OSObject
{
    friend class IOAudioDevice;

    OSDeclareDefaultStructors(IOAudioEngineEntry);

protected:
    IOAudioEngine *audioEngine;
    bool shouldStopAudioEngine;
};

OSDefineMetaClassAndStructors(IOAudioEngineEntry, OSObject)

#define super IOService
OSDefineMetaClassAndStructors(IOAudioDevice, IOService)
OSMetaClassDefineReservedUsed(IOAudioDevice, 0);
OSMetaClassDefineReservedUsed(IOAudioDevice, 1);
OSMetaClassDefineReservedUsed(IOAudioDevice, 2);
OSMetaClassDefineReservedUsed(IOAudioDevice, 3);
OSMetaClassDefineReservedUsed(IOAudioDevice, 4);
OSMetaClassDefineReservedUsed(IOAudioDevice, 5);

OSMetaClassDefineReservedUnused(IOAudioDevice, 6);
OSMetaClassDefineReservedUnused(IOAudioDevice, 7);
OSMetaClassDefineReservedUnused(IOAudioDevice, 8);
OSMetaClassDefineReservedUnused(IOAudioDevice, 9);
OSMetaClassDefineReservedUnused(IOAudioDevice, 10);
OSMetaClassDefineReservedUnused(IOAudioDevice, 11);
OSMetaClassDefineReservedUnused(IOAudioDevice, 12);
OSMetaClassDefineReservedUnused(IOAudioDevice, 13);
OSMetaClassDefineReservedUnused(IOAudioDevice, 14);
OSMetaClassDefineReservedUnused(IOAudioDevice, 15);
OSMetaClassDefineReservedUnused(IOAudioDevice, 16);
OSMetaClassDefineReservedUnused(IOAudioDevice, 17);
OSMetaClassDefineReservedUnused(IOAudioDevice, 18);
OSMetaClassDefineReservedUnused(IOAudioDevice, 19);
OSMetaClassDefineReservedUnused(IOAudioDevice, 20);
OSMetaClassDefineReservedUnused(IOAudioDevice, 21);
OSMetaClassDefineReservedUnused(IOAudioDevice, 22);
OSMetaClassDefineReservedUnused(IOAudioDevice, 23);
OSMetaClassDefineReservedUnused(IOAudioDevice, 24);
OSMetaClassDefineReservedUnused(IOAudioDevice, 25);
OSMetaClassDefineReservedUnused(IOAudioDevice, 26);
OSMetaClassDefineReservedUnused(IOAudioDevice, 27);
OSMetaClassDefineReservedUnused(IOAudioDevice, 28);
OSMetaClassDefineReservedUnused(IOAudioDevice, 29);
OSMetaClassDefineReservedUnused(IOAudioDevice, 30);
OSMetaClassDefineReservedUnused(IOAudioDevice, 31);


void IOAudioDevice::setDeviceModelName(const char *modelName)
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::setDeviceModelName(%p)\n", this, modelName);

    if (modelName) {
        setProperty(kIOAudioDeviceModelIDKey, modelName);
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::setDeviceModelName(%p)\n", this, modelName);
}

void IOAudioDevice::setDeviceTransportType(const UInt32 transportType)
{
    if (transportType) {
        setProperty(kIOAudioDeviceTransportTypeKey, transportType, 32);
    }
}

// This needs to be overridden by driver if it wants to know about power manager changes.
// If overridden, be sure to still call super::setAggressiveness() so we can call our parent.
IOReturn IOAudioDevice::setAggressiveness(unsigned long type, unsigned long newLevel)
{
	return super::setAggressiveness(type, newLevel);
}

// This was modified for <rdar://problem/3942297> 
void IOAudioDevice::setIdleAudioSleepTime(unsigned long long sleepDelay)
{
	assert(reserved);
	
	audioDebugIOLog(3, "+ IOAudioDevice[%p]::setIdleAudioSleepTime: sleepDelay = %lx%lx\n", this, (long unsigned int)(sleepDelay >> 32), (long unsigned int)sleepDelay);
	
	if ( reserved->idleTimer ) {
		reserved->idleTimer->cancelTimeout();
	}
	
	if (reserved->idleSleepDelayTime != sleepDelay) { 	// <rdar://problem/6601320> 
		reserved->idleSleepDelayTime = sleepDelay;
	}
		
	if ( kNoIdleAudioPowerDown != sleepDelay ) {
		scheduleIdleAudioSleep();
	}
	audioDebugIOLog(3, "- IOAudioDevice[%p]::setIdleAudioSleepTime: sleepDelay = %lx%lx\n", this, (long unsigned int)(sleepDelay >> 32), (long unsigned int)sleepDelay);
}

// Set up a timer to power down the hardware if we haven't used it in a while.
//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

void IOAudioDevice::scheduleIdleAudioSleep(void)
{
    AbsoluteTime				fireTime;
    UInt64						nanos;
	bool						exit = false;

	assert(reserved);

	audioDebugIOLog(3, "+ IOAudioDevice[%p]::scheduleIdleAudioSleep: idleSleepDelayTime = %lx%lx\n", this, (long unsigned int)(reserved->idleSleepDelayTime >> 32), (long unsigned int)reserved->idleSleepDelayTime);
	if ( 0 == reserved->idleSleepDelayTime )
	{
		// For backwards compatibility, or drivers that don't care, tell them about idle right away.
		initiatePowerStateChange ();
	}
	else
	{
		if ( !reserved->idleTimer && ( kNoIdleAudioPowerDown != reserved->idleSleepDelayTime ) )
		{
			reserved->idleTimer = IOTimerEventSource::timerEventSource ( this, idleAudioSleepHandlerTimer );
			if ( !reserved->idleTimer )
			{
				exit = true;
			}
			else
			{
				workLoop->addEventSource ( reserved->idleTimer );
			}
		}
	
		if ( !exit && ( kNoIdleAudioPowerDown != reserved->idleSleepDelayTime ) )
		{
			// If the driver wants to know about idle sleep after a specific amount of time, then set the timer to tell them at that time.
			// If idleSleepDelayTime == 0xffffffff then don't ever tell the driver about going idle
			clock_get_uptime ( &fireTime );
			absolutetime_to_nanoseconds ( fireTime, &nanos );
			nanos += reserved->idleSleepDelayTime;
			nanoseconds_to_absolutetime ( nanos, &fireTime );
			reserved->idleTimer->wakeAtTime ( fireTime );		// will call idleAudioSleepHandlerTimer
		}
	}

	audioDebugIOLog(3, "- IOAudioDevice[%p]::scheduleIdleAudioSleep: idleSleepDelayTime = %lx%lx\n", this, (long unsigned int)(reserved->idleSleepDelayTime >> 32), (long unsigned int)reserved->idleSleepDelayTime);
	return;
}

void IOAudioDevice::idleAudioSleepHandlerTimer(OSObject *owner, IOTimerEventSource *sender)
{
	IOAudioDevice *				audioDevice;

	audioDevice = OSDynamicCast(IOAudioDevice, owner);
	assert(audioDevice);

	audioDebugIOLog(3, "+ IOAudioDevice[%p]idleAudioSleepHandlerTimer: pendingPowerState = %d, idleSleepDelayTime = %lx%lx\n", audioDevice, audioDevice->pendingPowerState, (long unsigned int)(audioDevice->reserved->idleSleepDelayTime >> 32), (long unsigned int)audioDevice->reserved->idleSleepDelayTime);
	if (audioDevice->reserved->idleSleepDelayTime != kNoIdleAudioPowerDown &&
		audioDevice->getPendingPowerState () == kIOAudioDeviceIdle) {
		// If we're still idle, tell the device to go idle now that the requested amount of time has elapsed.
		audioDevice->initiatePowerStateChange();
	}

	audioDebugIOLog(3, "- IOAudioDevice[%p]idleAudioSleepHandlerTimer: pendingPowerState = %d, idleSleepDelayTime = %lx%lx\n", audioDevice, audioDevice->pendingPowerState, (long unsigned int)(audioDevice->reserved->idleSleepDelayTime >> 32), (long unsigned int)audioDevice->reserved->idleSleepDelayTime);
	return;
}

void IOAudioDevice::setConfigurationApplicationBundle(const char *bundleID)
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::setConfigurationApplicationBundle(%p)\n", this, bundleID);

    if (bundleID) {
        setProperty(kIOAudioDeviceConfigurationAppKey, bundleID);
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::setConfigurationApplicationBundle(%p)\n", this, bundleID);
}

// OSMetaClassDefineReservedUsed(IOAudioDevice, 4);
void IOAudioDevice::setDeviceCanBeDefault(UInt32 defaultsFlags)
{
	setProperty(kIOAudioDeviceCanBeDefaults, defaultsFlags, sizeof(UInt32) * 8);
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

bool IOAudioDevice::init(OSDictionary *properties)
{
	bool			result = false;
	
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::init(%p)\n", this, properties);

	if ( super::init ( properties ) )
	{
		reserved = IOMallocType (ExpansionData);
		if ( 0 != reserved )
		{
			reserved->idleSleepDelayTime = 0;
			reserved->idleTimer = NULL;
			
			audioEngines = OSArray::withCapacity ( 2 );
			if ( 0 != audioEngines )
			{
				audioPorts = OSSet::withCapacity ( 1 );
				if ( 0 != audioPorts )
				{
					workLoop = IOWorkLoop::workLoop ();
					if ( 0 != workLoop )
					{
						familyManagePower = true;
						asyncPowerStateChangeInProgress = false;

						currentPowerState = kIOAudioDeviceIdle;
						pendingPowerState = kIOAudioDeviceIdle;

						numRunningAudioEngines = 0;
						duringStartup = true;
						result = true;
					}
				}
			}
		}
	}
    
    audioDebugIOLog(3, "- IOAudioDevice[%p]::init(%p) returns %d\n", this, properties, result);
    return result;
}

void IOAudioDevice::free()
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::free()\n", this);

    if (audioEngines) {
        deactivateAllAudioEngines();
        audioEngines->release();
        audioEngines = 0;
    }
	
	audioDebugIOLog ( 3, "  did deactiveateAllAudioEngines ()\n" );
	
    if (audioPorts) {
        detachAllAudioPorts();
        audioPorts->release();
        audioPorts = 0;
    }
    
	audioDebugIOLog ( 3, "  did detachAllAudioPorts ()\n" );
	
    if (timerEvents) {
        timerEvents->release();
        timerEvents = 0;
    }

	audioDebugIOLog ( 3, "  did timerEvents->release ()\n" );
	
    if (timerEventSource) {
        if (workLoop) {
			timerEventSource->cancelTimeout();					// <rdar://problem/7493627,8426296>
            workLoop->removeEventSource(timerEventSource);
        }
        
        timerEventSource->release();
        timerEventSource = NULL;
    }

	audioDebugIOLog ( 3, "  did workLoop->removeEventSource ( timerEventSource )\n" );
	
	if (reserved && reserved->idleTimer) {
		if (workLoop) {
			reserved->idleTimer->cancelTimeout();			// <rdar://problem/7493627,8426296>
			workLoop->removeEventSource(reserved->idleTimer);
		}

		reserved->idleTimer->release();
		reserved->idleTimer = NULL;
	}

	audioDebugIOLog ( 3, "  did workLoop->removeEventSource ( reserved->idleTimer )\n" );

    if (reserved) {
        IOFreeType (reserved, ExpansionData);
    }
    
    audioDebugIOLog ( 3, "  did IOFree ()\n" );
    
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }
        
        commandGate->release();
        commandGate = NULL;
    }

	audioDebugIOLog ( 3, "  did workLoop->removeEventSource ( commandGate )\n" );
	
    if (workLoop) {
        workLoop->release();
        workLoop = NULL;
    }

	audioDebugIOLog ( 3, "  did workLoop->release ()\n" );
	
    super::free();

    audioDebugIOLog(3, "- IOAudioDevice[%p]::free()\n", this);
}

bool IOAudioDevice::initHardware(IOService *provider)
{
    audioDebugIOLog(3, "+-IOAudioDevice[%p]::initHardware(%p)\n", this, provider);

    return true;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

bool IOAudioDevice::start(IOService *provider)
{
	bool			result = false;
	
    static IOPMPowerState powerStates[2] = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::start(%p)\n", this, provider);
    AudioTrace_Start(kAudioTIOAudioDevice, kTPIOAudioDeviceStart, (uintptr_t)this, (uintptr_t)provider, 0, 0);
	
	if ( super::start ( provider ) )
	{
		if ( 0 != provider->getProperty("preserveIODeviceTree\n") )		// <rdar://3206968>
		{
			provider->callPlatformFunction("mac-io-publishChildren\n", 0, (void*)this, (void*)0, (void*)0, (void*)0);
		}

		assert(workLoop);
		
		commandGate = IOCommandGate::commandGate(this);
		if ( 0 != commandGate)
		{
			workLoop->addEventSource(commandGate);

			setDeviceCanBeDefault (kIOAudioDeviceCanBeDefaultInput | kIOAudioDeviceCanBeDefaultOutput | kIOAudioDeviceCanBeSystemOutput);

			if ( initHardware ( provider ) )
			{
				if ( familyManagePower ) {
					PMinit ();
					provider->joinPMtree ( this );
					
					if ( NULL != pm_vars ) {
						//	duringStartup = true;
						registerPowerDriver ( this, powerStates, NUM_POWER_STATES );
						changePowerStateTo ( 1 );
						//	duringStartup = false;
					}
				}

				registerService();
				result = true;
			}
		}
	}
	
    audioDebugIOLog(3, "- IOAudioDevice[%p]::start(%p)\n", this, provider);
    AudioTrace_End(kAudioTIOAudioDevice, kTPIOAudioDeviceStart, (uintptr_t)this, (uintptr_t)provider, result, 0);
	return result;
}

void IOAudioDevice::stop(IOService *provider)
{    
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::stop(%p)\n", this, provider);
    
    removeAllTimerEvents();					// <rdar://problem/7493627,8426296>

    if (timerEventSource) {
        if (workLoop) {
			timerEventSource->cancelTimeout();					// <rdar://problem/7493627,8426296>
            workLoop->removeEventSource(timerEventSource);
        }
        
        timerEventSource->release();
        timerEventSource = NULL;
    }

    if (reserved->idleTimer) {
        if (workLoop) {
			reserved->idleTimer->cancelTimeout();				// <rdar://problem/7493627,8426296>
            workLoop->removeEventSource(reserved->idleTimer);
        }

        reserved->idleTimer->release();
        reserved->idleTimer = NULL;
    }

    deactivateAllAudioEngines();
    detachAllAudioPorts();

    if (familyManagePower) {
		if (pm_vars != NULL) {
			PMstop();
		}
    }
    
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }
        
        commandGate->release();
        commandGate = NULL;
    }

    super::stop(provider);
    audioDebugIOLog(3, "- IOAudioDevice[%p]::stop(%p)\n", this, provider);
}

bool IOAudioDevice::willTerminate(IOService *provider, IOOptionBits options)
{
	bool			result = false;
	
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::willTerminate(%p, %lx)\n", this, provider, (long unsigned int)options);

    OSCollectionIterator *engineIterator;
    
    engineIterator = OSCollectionIterator::withCollection(audioEngines);
    if (engineIterator) {
        IOAudioEngine *audioEngine;
        
        while ( (audioEngine = OSDynamicCast(IOAudioEngine, engineIterator->getNextObject())) ) {
            audioEngine->setState(kIOAudioEngineStopped);
        }
        engineIterator->release();
    }

	result = super::willTerminate(provider, options);
    audioDebugIOLog(3, "- IOAudioDevice[%p]::willTerminate(%p, %lx) returns %d\n", this, provider, (long unsigned int)options, result );
	return result;
}

void IOAudioDevice::setFamilyManagePower(bool manage)
{
    familyManagePower = manage;
}

IOReturn IOAudioDevice::setPowerState(unsigned long powerStateOrdinal, IOService *device)
{
    IOReturn result = IOPMAckImplied;
    
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::setPowerState(%lu, %p)\n", this, powerStateOrdinal, device);
    AudioTrace_Start(kAudioTIOAudioDevice, kTPIOAudioDeviceSetPowerState, (uintptr_t)this, powerStateOrdinal, (uintptr_t)device, ((UInt64)currentPowerState << 32) | pendingPowerState);
    
    if (!duringStartup) 
	{
        if (powerStateOrdinal >= NUM_POWER_STATES) 
		{
            result = IOPMNoSuchState;
        } else 
		{
			if (workLoop) {																					//	<rdar://8508064>
				workLoop->runAction(_setPowerStateAction, this, (void *)powerStateOrdinal, (void *)device);	//	<rdar://8508064>
			}
        }
    }
	duringStartup = false;
    audioDebugIOLog(3, "- IOAudioDevice[%p]::setPowerState(%lu, %p) returns 0x%lX\n", this, powerStateOrdinal, device, (long unsigned int)result );
    AudioTrace_End(kAudioTIOAudioDevice, kTPIOAudioDeviceSetPowerState, (uintptr_t)this, currentPowerState, (uintptr_t)device, result);
    
	return result;
}

// <rdar://8508064>
IOReturn IOAudioDevice::_setPowerStateAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = IOPMAckImplied;
    AudioTrace_Start(kAudioTIOAudioDevice, kTPIOAudioDevice_SetPowerStateAction, (uintptr_t)target, (uintptr_t)arg1, (uintptr_t)arg2, 0);
    
    if (target) {
        IOAudioDevice *audioDevice = OSDynamicCast(IOAudioDevice, target);
        if (audioDevice) {
            IOCommandGate *cg;
            
            cg = audioDevice->getCommandGate();
            
            if (cg) {
                result = cg->runAction(setPowerStateAction, arg0, arg1, arg2, arg3);
            }
        }
    }
    
    AudioTrace_End(kAudioTIOAudioDevice, kTPIOAudioDevice_SetPowerStateAction, (uintptr_t)target, (uintptr_t)arg1, (uintptr_t)arg2, result);
    return result;
}

IOReturn IOAudioDevice::setPowerStateAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = IOPMAckImplied;
    AudioTrace_Start(kAudioTIOAudioDevice, kTPIOAudioDeviceSetPowerStateAction, (uintptr_t)owner, (uintptr_t)arg1, (uintptr_t)arg2, 0);
   
    if (owner) {
        IOAudioDevice *audioDevice = OSDynamicCast(IOAudioDevice, owner);
        
        if (audioDevice) {
            result = audioDevice->protectedSetPowerState((unsigned long)arg1, (IOService *)arg2);
        }
    }
    
    AudioTrace_End(kAudioTIOAudioDevice, kTPIOAudioDeviceSetPowerStateAction, (uintptr_t)owner, (uintptr_t)arg1, (uintptr_t)arg2, result);
    return result;
}

IOReturn IOAudioDevice::protectedSetPowerState(unsigned long powerStateOrdinal, IOService *device)
{
    IOReturn result = IOPMAckImplied;

    audioDebugIOLog(3, "+ IOAudioDevice[%p]::protectedSetPowerState(%lu, %p)\n", this, powerStateOrdinal, device);
    AudioTrace_Start(kAudioTIOAudioDevice, kTPIOAudioDeviceProtectedSetPowerState, (uintptr_t)this, powerStateOrdinal, (uintptr_t)device, asyncPowerStateChangeInProgress);
    
    if (asyncPowerStateChangeInProgress) {
        waitForPendingPowerStateChange();
    }
    
    if (powerStateOrdinal == 0) {	// Sleep
        AudioTrace(kAudioTIOAudioDevice, kTPIOAudioDeviceProtectedSetPowerState, (uintptr_t)this, getPowerState(), currentPowerState, 0);
        if (getPowerState() != kIOAudioDeviceSleep) {
            pendingPowerState = kIOAudioDeviceSleep;

            // Stop all audio engines
            if (audioEngines && (numRunningAudioEngines > 0)) {
                OSCollectionIterator *audioEngineIterator;
                
                audioEngineIterator = OSCollectionIterator::withCollection(audioEngines);
                
                if (audioEngineIterator) {
                    IOAudioEngine *audioEngine;
                    
                    while ( (audioEngine = (IOAudioEngine *)audioEngineIterator->getNextObject()) ) {
                        if (audioEngine->getState() == kIOAudioEngineRunning) {
                            audioEngine->pauseAudioEngine();
                        }
                    }
                    
                    audioEngineIterator->release();
                }
            }
        }
    } else if (powerStateOrdinal == 1) {	// Wake
        AudioTrace(kAudioTIOAudioDevice, kTPIOAudioDeviceProtectedSetPowerState, (uintptr_t)this, getPowerState(), numRunningAudioEngines, 1);
        if (getPowerState() == kIOAudioDeviceSleep) {	// Need to change state if sleeping
            if (numRunningAudioEngines == 0) {
                pendingPowerState = kIOAudioDeviceIdle;
            } else {
                pendingPowerState = kIOAudioDeviceActive;
            }
        }
    }
    
    if (currentPowerState != pendingPowerState) {
        UInt32 microsecondsUntilComplete = 0;
        
        result = initiatePowerStateChange(&microsecondsUntilComplete);
        if (result == kIOReturnSuccess) {
            result = microsecondsUntilComplete;
        }
    }
    
    audioDebugIOLog(3, "- IOAudioDevice[%p]::protectedSetPowerState(%lu, %p) returns 0x%lX\n", this, powerStateOrdinal, device, (long unsigned int)result );
    AudioTrace_End(kAudioTIOAudioDevice, kTPIOAudioDeviceProtectedSetPowerState, (uintptr_t)this, powerStateOrdinal, (uintptr_t)device, result);
    return result;
}

void IOAudioDevice::waitForPendingPowerStateChange()
{
    IOReturn            kr = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::waitForPendingPowerStateChange()\n", this);
    AudioTrace_Start(kAudioTIOAudioDevice, kTPIOAudioDeviceWaitForPendingPowerStateChange, (uintptr_t)this, asyncPowerStateChangeInProgress, 0, 0);

    if (asyncPowerStateChangeInProgress) {
        IOCommandGate *cg;
        
        cg = getCommandGate();
        
        if (cg) {
            AbsoluteTime        deadline;
            
            cg->retain();
            clock_interval_to_deadline(1, kSecondScale, &deadline);
            
            kr = cg->commandSleep( &asyncPowerStateChangeInProgress, deadline, THREAD_ABORTSAFE );
            switch (kr)
            {
                case THREAD_AWAKENED:
                    // this is what we expect
                    break;
                    
                case THREAD_TIMED_OUT:
                case THREAD_INTERRUPTED:
                case THREAD_RESTART:
                default:
                    // unexpected, but since the method doesn't return a value, we just log it
                    audioErrorIOLog("Sound assert: IOAudioDevice::waitForPendingPowerStateChange got kr 0x%08x\n", kr);
                    break;
                    
            }
            cg->release();
            if (asyncPowerStateChangeInProgress)
            {
                audioErrorIOLog("Sound assert: IOAudioDevice::waitForPendingPowerStateChange() - internal error - power state change still in progress\n");
            }
        } else {
            audioErrorIOLog("Sound assert: IOAudioDevice::waitForPendingPowerStateChange() - internal error - unable to get the command gate.\n");
        }
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::waitForPendingPowerStateChange()\n", this);
    AudioTrace_End(kAudioTIOAudioDevice, kTPIOAudioDeviceWaitForPendingPowerStateChange, (uintptr_t)this, asyncPowerStateChangeInProgress, kr, 0);
	return;
}

IOReturn IOAudioDevice::initiatePowerStateChange(UInt32 *microsecondsUntilComplete)
{
    IOReturn result = kIOReturnSuccess;

    audioDebugIOLog(3, "+ IOAudioDevice[%p]::initiatePowerStateChange(%p) - current = %d - pending = %d\n", this, microsecondsUntilComplete, currentPowerState, pendingPowerState);
    AudioTrace_Start(kAudioTIOAudioDevice, kTPIOAudioDeviceInitiatePowerStateChange, (uintptr_t)this, currentPowerState, pendingPowerState, asyncPowerStateChangeInProgress);
   
    if (currentPowerState != pendingPowerState) {
        UInt32 localMicsUntilComplete, *micsUntilComplete = NULL;
        
        if (microsecondsUntilComplete != NULL) {
            micsUntilComplete = microsecondsUntilComplete;
        } else {
            micsUntilComplete = &localMicsUntilComplete;
        }
        
        *micsUntilComplete = 0;
        
        asyncPowerStateChangeInProgress = true;
        
        result = performPowerStateChange(currentPowerState, pendingPowerState, micsUntilComplete);
        
        if (result == kIOReturnSuccess) {
            if (*micsUntilComplete == 0) {
                asyncPowerStateChangeInProgress = false;
                protectedCompletePowerStateChange();
            }
        } else if ( result == IOPMWillAckLater ) {
            asyncPowerStateChangeInProgress = true;
        }
        else
        {
            asyncPowerStateChangeInProgress = false;
        }
    }
    
    audioDebugIOLog(3, "- IOAudioDevice[%p]::initiatePowerStateChange(%p) - current = %d - pending = %d returns 0x%lX\n", this, microsecondsUntilComplete, currentPowerState, pendingPowerState, (long unsigned int)result );
    AudioTrace_End(kAudioTIOAudioDevice, kTPIOAudioDeviceInitiatePowerStateChange, (uintptr_t)this, currentPowerState, microsecondsUntilComplete ? *microsecondsUntilComplete : 0, result);
    return result;
}

IOReturn IOAudioDevice::completePowerStateChange()
{
    IOReturn result = kIOReturnError;
    IOCommandGate *cg = getCommandGate();
    
    AudioTrace_Start(kAudioTIOAudioDevice, kTPIOAudioDeviceCompletePowerStateChange, (uintptr_t)this, currentPowerState, pendingPowerState, asyncPowerStateChangeInProgress);

    if (cg) {
        result = cg->runAction(completePowerStateChangeAction);
    }
    
    AudioTrace_End(kAudioTIOAudioDevice, kTPIOAudioDeviceCompletePowerStateChange, (uintptr_t)this, currentPowerState, pendingPowerState, result);
    return result;
}

IOReturn IOAudioDevice::completePowerStateChangeAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioDevice *audioDevice = OSDynamicCast(IOAudioDevice, owner);
        
        if (audioDevice) {
            result = audioDevice->protectedCompletePowerStateChange();
        }
    }
    
    return result;
}

IOReturn IOAudioDevice::protectedCompletePowerStateChange()
{
    IOReturn result = kIOReturnSuccess;

    audioDebugIOLog(3, "+ IOAudioDevice[%p]::protectedCompletePowerStateChange() - current = %d - pending = %d\n", this, currentPowerState, pendingPowerState);

    if (currentPowerState != pendingPowerState) {
		IOCommandGate *cg;

		cg = getCommandGate();
        // If we're waking, we fire off the timers and resync them
        // Then restart the audio engines that were running before the sleep
        if (currentPowerState == kIOAudioDeviceSleep) {	
            clock_get_uptime(&previousTimerFire);
            SUB_ABSOLUTETIME(&previousTimerFire, &minimumInterval);
            
            if (timerEvents && (timerEvents->getCount() > 0)) {
                dispatchTimerEvents(true);
            }
            
            if (audioEngines && (numRunningAudioEngines > 0)) {
                OSCollectionIterator *audioEngineIterator;
                
                audioEngineIterator = OSCollectionIterator::withCollection(audioEngines);
                
                if (audioEngineIterator) {
                    IOAudioEngine *audioEngine;
                    
                    while ( (audioEngine = (IOAudioEngine *)audioEngineIterator->getNextObject()) ) {
                        if (audioEngine->getState() == kIOAudioEnginePaused) {
                            audioEngine->resumeAudioEngine();
                        }
                    }
                        
                    audioEngineIterator->release();
                }
            }
        }
    
        if (asyncPowerStateChangeInProgress) {
            acknowledgeSetPowerState();
            asyncPowerStateChangeInProgress = false;
        
            if (cg) {
                cg->commandWakeup((void *)&asyncPowerStateChangeInProgress);
            }
        }
        
        currentPowerState = pendingPowerState;
		
		if (cg) {
			cg->commandWakeup(&currentPowerState);
		}
    }
    
    audioDebugIOLog(3, "- IOAudioDevice[%p]::protectedCompletePowerStateChange() - current = %d - pending = %d returns 0x%lX\n", this, currentPowerState, pendingPowerState, (long unsigned int)result );
    return result;
}

IOReturn IOAudioDevice::performPowerStateChange(IOAudioDevicePowerState oldPowerState,
                                                IOAudioDevicePowerState newPowerState,
                                                UInt32 *microsecondsUntilComplete)
{
    return kIOReturnSuccess;
}

IOAudioDevicePowerState IOAudioDevice::getPowerState()
{
    return currentPowerState;
}

IOAudioDevicePowerState IOAudioDevice::getPendingPowerState()
{
    return pendingPowerState;
}

void IOAudioDevice::audioEngineStarting()
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::audioEngineStarting() - numRunningAudioEngines = %ld\n", this, (long int)numRunningAudioEngines );

    numRunningAudioEngines++;
    
    if (numRunningAudioEngines == 1) {	// First audio engine starting - need to be in active state
        if (getPowerState() == kIOAudioDeviceIdle) {	// Go active
            if (asyncPowerStateChangeInProgress) {	// Sleep if there is a transition in progress
                waitForPendingPowerStateChange();
            }
            
            pendingPowerState = kIOAudioDeviceActive;
            
            initiatePowerStateChange();

            if (asyncPowerStateChangeInProgress) {	// Sleep if there is a transition in progress
                waitForPendingPowerStateChange();
            }
        } else if (getPendingPowerState () != kIOAudioDeviceSleep) {
			// Make sure that when the idle timer fires that we won't go to sleep.
            pendingPowerState = kIOAudioDeviceActive;
		}
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::audioEngineStarting() - numRunningAudioEngines = %ld\n", this, (long int)numRunningAudioEngines );
}

void IOAudioDevice::audioEngineStopped()
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::audioEngineStopped() - numRunningAudioEngines = %ld\n", this, (long int)numRunningAudioEngines );

	if ( numRunningAudioEngines > 0 ) {
	    numRunningAudioEngines--;
	}
    
    if (numRunningAudioEngines == 0) {	// Last audio engine stopping - need to be idle
        if (getPowerState() == kIOAudioDeviceActive) {	// Go idle
			if (asyncPowerStateChangeInProgress) {	// Sleep if there is a transition in progress
				waitForPendingPowerStateChange();
			}

			pendingPowerState = kIOAudioDeviceIdle;

			scheduleIdleAudioSleep();
        }
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::audioEngineStopped() - numRunningAudioEngines = %ld\n", this, (long int)numRunningAudioEngines );
}

IOWorkLoop *IOAudioDevice::getWorkLoop() const
{
    return workLoop;
}

IOCommandGate *IOAudioDevice::getCommandGate() const
{
    return commandGate;
}

void IOAudioDevice::setDeviceName(const char *deviceName)
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::setDeviceName(%p)\n", this, deviceName);

    if (deviceName) {
        setProperty(kIOAudioDeviceNameKey, deviceName);
		if (NULL == getProperty (kIOAudioDeviceModelIDKey)) {
			int			stringLen, tempLength;
			char *		string;

			stringLen = 1;
			stringLen += strlen (deviceName) + 1;
			stringLen += strlen (getName ());
			string = (char *)IOMallocData (stringLen);
			if ( string )									// we should not panic for this
			{	
				strncpy (string, getName (), stringLen);
				tempLength = strlen (".");					//	<rdar://problem/6411827>
				strncat (string, ":", tempLength);
				tempLength = strlen (deviceName);			//	<rdar://problem/6411827>
				strncat (string, deviceName, tempLength);
				setDeviceModelName (string);
				IOFreeData (string, stringLen);
			}
		}
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::setDeviceName(%p)\n", this, deviceName);
}

void IOAudioDevice::setDeviceShortName(const char *shortName)
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::setDeviceShortName(%p)\n", this, shortName);

    if (shortName) {
        setProperty(kIOAudioDeviceShortNameKey, shortName);
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::setDeviceShortName(%p)\n", this, shortName);
}

void IOAudioDevice::setManufacturerName(const char *manufacturerName)
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::setManufacturerName(%p)\n", this, manufacturerName);

    if (manufacturerName) {
        setProperty(kIOAudioDeviceManufacturerNameKey, manufacturerName);
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::setManufacturerName(%p)\n", this, manufacturerName);
}

IOReturn IOAudioDevice::activateAudioEngine(IOAudioEngine *audioEngine)
{
    return activateAudioEngine(audioEngine, true);
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

IOReturn IOAudioDevice::activateAudioEngine(IOAudioEngine *audioEngine, bool shouldStartAudioEngine)
{
	IOReturn			result = kIOReturnBadArgument;
	
	audioDebugIOLog(3, "+ IOAudioDevice[%p]::activateAudioEngine(%p, %d)\n", this, audioEngine, shouldStartAudioEngine);

	if ( audioEngine && audioEngines )
	{
		if ( !audioEngine->attach ( this ) )
		{
			result = kIOReturnError;
		}
		else
		{
			if ( shouldStartAudioEngine ) 
			{
				if (!audioEngine->start ( this ) )
				{
					audioEngine->detach ( this );
					result =  kIOReturnError;
				}
				else
				{
					result =  kIOReturnSuccess;
				}
			}
			else // <rdar://8681286>
			{
				result =  kIOReturnSuccess;
			}
			
			if ( kIOReturnSuccess == result ) // <rdar://8681286>
			{
				audioEngine->deviceStartedAudioEngine = shouldStartAudioEngine;
				
				audioEngines->setObject ( audioEngine );
				audioEngine->setIndex ( audioEngines->getCount() - 1 );
				
				audioEngine->registerService ();
			}
		}
	}

	audioDebugIOLog(3, "- IOAudioDevice[%p]::activateAudioEngine(%p, %d) returns 0x%lX\n", this, audioEngine, shouldStartAudioEngine, (long unsigned int)result );
	return result;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

void IOAudioDevice::deactivateAllAudioEngines()
{
    OSCollectionIterator *engineIterator;
    
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::deactivateAllAudioEngines()\n", this);

    if ( audioEngines )
	{
		engineIterator = OSCollectionIterator::withCollection ( audioEngines );
		if ( engineIterator )
		{
			IOAudioEngine *audioEngine;
			
			while ( (audioEngine = OSDynamicCast ( IOAudioEngine, engineIterator->getNextObject ()) ) )
			{
				audioEngine->stopAudioEngine ();
				if ( !isInactive () )
				{
					audioEngine->terminate ();
				}
			}
			engineIterator->release ();
		}

		audioEngines->flushCollection ();
    }

    audioDebugIOLog(3, "- IOAudioDevice[%p]::deactivateAllAudioEngines()\n", this);
	return;
}

IOReturn IOAudioDevice::attachAudioPort(IOAudioPort *port, IORegistryEntry *parent, IORegistryEntry *child)
{
    return kIOReturnSuccess;
}

void IOAudioDevice::detachAllAudioPorts()
{
}

void IOAudioDevice::flushAudioControls()
{
    if (audioPorts) {
        OSCollectionIterator *portIterator;

        portIterator = OSCollectionIterator::withCollection(audioPorts);
        if (portIterator) {
            IOAudioPort *audioPort;

            while ( (audioPort = (IOAudioPort *)portIterator->getNextObject()) ) {
                if (OSDynamicCast(IOAudioPort, audioPort)) {
                    if (audioPort->audioControls) {
                        OSCollectionIterator *controlIterator;

                        controlIterator = OSCollectionIterator::withCollection(audioPort->audioControls);

                        if (controlIterator) {
                            IOAudioControl *audioControl;

                            while ( (audioControl = (IOAudioControl *)controlIterator->getNextObject()) ) {
                                audioControl->flushValue();
                            }
                            controlIterator->release();
                        }
                    }
                }
            }
            portIterator->release();
        }
    }
    
    // This code will flush controls attached to an IOAudioPort and a default on a audio engine
    // more than once
    // We need to update this to create a single master list of controls and use that to flush
    // each only once
    if (audioEngines) {
        OSCollectionIterator *audioEngineIterator;
        
        audioEngineIterator = OSCollectionIterator::withCollection(audioEngines);
        if (audioEngineIterator) {
            IOAudioEngine *audioEngine;
            
            while ( (audioEngine = (IOAudioEngine *)audioEngineIterator->getNextObject()) ) {
                if (audioEngine->defaultAudioControls) {
                    OSCollectionIterator *controlIterator;
                    
                    controlIterator = OSCollectionIterator::withCollection(audioEngine->defaultAudioControls);
                    if (controlIterator) {
                        IOAudioControl *audioControl;
                        
                        while ( (audioControl = (IOAudioControl *)controlIterator->getNextObject()) ) {
                            audioControl->flushValue();
                        }
                        controlIterator->release();
                    }
                }
            }
            
            audioEngineIterator->release();
        }
    }
    audioDebugIOLog(3, "+- IOAudioDevice[%p]::flushAudioControls()\n", this);
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

IOReturn IOAudioDevice::addTimerEvent(OSObject *target, TimerEvent event, AbsoluteTime interval)
{
    IOReturn			result = kIOReturnSuccess;
    IOAudioTimerEvent *	newEvent;
    
#ifdef DEBUG
    UInt64 newInt;
    absolutetime_to_nanoseconds(interval, &newInt);
    audioDebugIOLog(3, "+ IOAudioDevice::addTimerEvent(%p, %p, %lums)\n", target, event, (long unsigned int)(newInt/1000000));
#endif

    if ( !event )
	{
        result = kIOReturnBadArgument;
    }
	else
	{
		newEvent = new IOAudioTimerEvent;
		newEvent->target = target;
		newEvent->event = event;
		newEvent->interval = interval;

		if (!timerEvents) {
			IOWorkLoop *wl;

			timerEvents = OSDictionary::withObjects((const OSObject **)&newEvent, (const OSSymbol **)&target, 1, 1);
			
			timerEventSource = IOTimerEventSource::timerEventSource(this, timerFired);
			wl = getWorkLoop();
			if ( !timerEventSource || !wl || ( kIOReturnSuccess != wl->addEventSource ( timerEventSource ) ) )
			{
				result = kIOReturnError;
			}
			else
			{
				timerEventSource->enable ();
			}
		}
		else
		{
			timerEvents->setObject((OSSymbol *)target, newEvent);
		}
		
		if ( kIOReturnSuccess == result )
		{
			newEvent->release();
			
			assert(timerEvents);

			if (timerEvents->getCount() == 1) {
				AbsoluteTime nextTimerFire;
				
				minimumInterval = interval;

				assert(timerEventSource);

				clock_get_uptime(&previousTimerFire);
				
				nextTimerFire = previousTimerFire;
				ADD_ABSOLUTETIME(&nextTimerFire, &minimumInterval);
				
				result = timerEventSource->wakeAtTime(nextTimerFire);
				
#ifdef DEBUG
				{
					UInt64 nanos;
					absolutetime_to_nanoseconds(minimumInterval, &nanos);
					audioDebugIOLog(5, "  scheduling timer to fire in %lums - previousTimerFire = {%llu}\n", (long unsigned int) (nanos / 1000000), previousTimerFire);
				}
#endif

				if (result != kIOReturnSuccess) {
					IOLog("IOAudioDevice::addTimerEvent() - error 0x%x setting timer wake time - timer events will be disabled.\n", result);
				}
			} else if (CMP_ABSOLUTETIME(&interval, &minimumInterval) < 0) {
				AbsoluteTime currentNextFire, desiredNextFire;
				
				clock_get_uptime(&desiredNextFire);
				ADD_ABSOLUTETIME(&desiredNextFire, &interval);

				currentNextFire = previousTimerFire;
				ADD_ABSOLUTETIME(&currentNextFire, &minimumInterval);
				
				minimumInterval = interval;

				if (CMP_ABSOLUTETIME(&desiredNextFire, &currentNextFire) < 0) {
					assert(timerEventSource);
					
#ifdef DEBUG
					{
						UInt64 nanos;
						absolutetime_to_nanoseconds(interval, &nanos);
						audioDebugIOLog(5, "  scheduling timer to fire in %lums at {%llu} - previousTimerFire = {%llu}\n", (long unsigned int) (nanos / 1000000), desiredNextFire, previousTimerFire);
					}
#endif

					result = timerEventSource->wakeAtTime(desiredNextFire);
					if (result != kIOReturnSuccess) {
						IOLog("IOAudioDevice::addTimerEvent() - error 0x%x setting timer wake time - timer events will be disabled.\n", result);
					}
				}
			}
		}
	}
    
#ifdef DEBUG
    audioDebugIOLog(3, "- IOAudioDevice::addTimerEvent(%p, %p, %lums) returns 0x%lX\n", target, event, (long unsigned int)(newInt/1000000), (long unsigned int)result );
#endif
    return result;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

void IOAudioDevice::removeTimerEvent(OSObject *target)
{
    IOAudioTimerEvent *removedTimerEvent;
    
    audioDebugIOLog(4, "+ IOAudioDevice::removeTimerEvent(%p)\n", target);
    
	if ( timerEvents )
	{
		removedTimerEvent = (IOAudioTimerEvent *)timerEvents->getObject((const OSSymbol *)target);
		if (removedTimerEvent) {
			removedTimerEvent->retain();
			timerEvents->removeObject((const OSSymbol *)target);
			if (timerEvents->getCount() == 0) {
				assert(timerEventSource);
				timerEventSource->cancelTimeout();
			} else if (CMP_ABSOLUTETIME(&removedTimerEvent->interval, &minimumInterval) <= 0) { // Need to find a new minimum interval
				OSCollectionIterator *iterator;
				IOAudioTimerEvent *timerEvent;
				AbsoluteTime nextTimerFire;
				OSSymbol *obj;

				iterator = OSCollectionIterator::withCollection(timerEvents);
				
				if (iterator) {
					obj = (OSSymbol *)iterator->getNextObject();
					timerEvent = (IOAudioTimerEvent *)timerEvents->getObject(obj);
		
					if (timerEvent) {
						minimumInterval = timerEvent->interval;
		
						while ((obj = (OSSymbol *)iterator->getNextObject()) && (timerEvent = (IOAudioTimerEvent *)timerEvents->getObject(obj))) {
							if (CMP_ABSOLUTETIME(&timerEvent->interval, &minimumInterval) < 0) {
								minimumInterval = timerEvent->interval;
							}
						}
					}
		
					iterator->release();
				}

				assert(timerEventSource);

				nextTimerFire = previousTimerFire;
				ADD_ABSOLUTETIME(&nextTimerFire, &minimumInterval);
				
#ifdef DEBUG
				{
					AbsoluteTime now, then;
					UInt64 nanos, mi;
					clock_get_uptime(&now);
					then = nextTimerFire;
					absolutetime_to_nanoseconds(minimumInterval, &mi);
					if (CMP_ABSOLUTETIME(&then, &now)) {
						SUB_ABSOLUTETIME(&then, &now);
						absolutetime_to_nanoseconds(then, &nanos);
						audioDebugIOLog(5, "IOAudioDevice::removeTimerEvent() - scheduling timer to fire in %lums at {%llu} - previousTimerFire = {%llu} - interval=%lums\n", (long unsigned int) (nanos / 1000000), nextTimerFire, previousTimerFire, (long unsigned int)(mi/1000000));
					
					} else {
						SUB_ABSOLUTETIME(&now, &then);
						absolutetime_to_nanoseconds(now, &nanos);
						audioDebugIOLog(5, "IOAudioDevice::removeTimerEvent() - scheduling timer to fire in -%lums - previousTimerFire = {%llu}\n", (long unsigned int) (nanos / 1000000), previousTimerFire);
					}
				}
#endif

				timerEventSource->wakeAtTime(nextTimerFire);
			}

			removedTimerEvent->release();
		}
	}
    audioDebugIOLog(4, "- IOAudioDevice::removeTimerEvent(%p)\n", target);
	return;
}

void IOAudioDevice::removeAllTimerEvents()
{
    audioDebugIOLog(3, "+ IOAudioDevice[%p]::removeAllTimerEvents()\n", this);

    if (timerEventSource) {
        timerEventSource->cancelTimeout();
    }
    
    if (timerEvents) {
        timerEvents->flushCollection();
    }
    audioDebugIOLog(3, "- IOAudioDevice[%p]::removeAllTimerEvents()\n", this);
}

void IOAudioDevice::timerFired(OSObject *target, IOTimerEventSource *sender)
{
    if (target) {
        IOAudioDevice *audioDevice = OSDynamicCast(IOAudioDevice, target);
        
        if (audioDevice) {
            audioDevice->dispatchTimerEvents(false);
        }
    }
}

void IOAudioDevice::dispatchTimerEvents(bool force)
{
	audioDebugIOLog(5, "+ IOAudioDevice::dispatchTimerEvents( %d )\n", force );
	
    if (timerEvents) {
#ifdef DEBUG
        AbsoluteTime now, delta;
        UInt64 nanos;
        
        clock_get_uptime(&now);
        delta = now;
        SUB_ABSOLUTETIME(&delta, &previousTimerFire);
        absolutetime_to_nanoseconds(delta, &nanos);
        audioDebugIOLog(5, "  woke up %lums after last fire - now = {%llu} - previousFire = {%llu}\n", (long unsigned int)(nanos / 1000000), now, previousTimerFire);
#endif	/* DEBUG */
		
        if (force || (getPowerState() != kIOAudioDeviceSleep)) {
            OSIterator *iterator;
            OSSymbol *target;
            AbsoluteTime nextTimerFire, currentInterval;
            
            currentInterval = minimumInterval;
        
            assert(timerEvents);
        
            iterator = OSCollectionIterator::withCollection(timerEvents);
        
            if (iterator) {
                while ( (target = (OSSymbol *)iterator->getNextObject()) ) {
                    IOAudioTimerEvent *timerEvent;
                    timerEvent = (IOAudioTimerEvent *)timerEvents->getObject(target);
        
                    if (timerEvent) {
                        (*timerEvent->event)(timerEvent->target, this);
                    }
                }
        
                iterator->release();
            }
        
            if (timerEvents->getCount() > 0) {
                ADD_ABSOLUTETIME(&previousTimerFire, &currentInterval);
                nextTimerFire = previousTimerFire;
                ADD_ABSOLUTETIME(&nextTimerFire, &minimumInterval);
        
                assert(timerEventSource);
                
#ifdef DEBUG
                {
                    AbsoluteTime later;
                    UInt64 mi;
                    later = nextTimerFire;
                    absolutetime_to_nanoseconds(minimumInterval, &mi);
                    if (CMP_ABSOLUTETIME(&later, &now)) {
                        SUB_ABSOLUTETIME(&later, &now);
                        absolutetime_to_nanoseconds(later, &nanos);
						audioDebugIOLog(5, "  scheduling timer to fire in %lums at {%llu} - previousTimerFire = {%llu} - interval=%lums\n", (long unsigned int) (nanos / 1000000), nextTimerFire, previousTimerFire, (long unsigned int)(mi/1000000));
                    }
					else 
					{
                        SUB_ABSOLUTETIME(&now, &later);
                        absolutetime_to_nanoseconds(now, &nanos);
                        audioDebugIOLog(5, "  scheduling timer to fire in -%lums - previousTimerFire = {%llu}\n", (long unsigned int) (nanos / 1000000), previousTimerFire);
                    }
                }
#endif	/* DEBUG */
    
                timerEventSource->wakeAtTime(nextTimerFire);
            }
        }
    }
	audioDebugIOLog(5, "- IOAudioDevice::dispatchTimerEvents()\n" );
	return;
}

