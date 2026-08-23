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
#include "IOAudioEngineUserClient.h"
#include "IOAudioEngine.h"
#include "IOAudioTypes.h"
#include "IOAudioStream.h"
#include "IOAudioDebug.h"
#include "IOAudioDefines.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOKitKeys.h>

#include <sys/random.h>

#include <AssertMacros.h>
// Unused macros in open source
#define AudioTrace_Start(a, b, c, d, e, f)
#define AudioTrace_End(a, b, c, d, e, f)
#define AudioTrace(a, b, c, d, e, f)

enum
{
	kCommandGateStatus_Normal				= 0,
	kCommandGateStatus_RemovalPending,
	kCommandGateStatus_Invalid
};

enum
{
    kLoopCountMaximumDifference             = 5
};

#pragma mark -
#pragma mark IOAudioClientBufferSet
#define super OSObject

class IOAudioClientBufferSet : public OSObject
{
    OSDeclareDefaultStructors(IOAudioClientBufferSet);

public:
    UInt32							bufferSetID;
    IOAudioEngineUserClient	*		userClient;
    IOAudioClientBuffer64	*		outputBufferList;
    IOAudioClientBuffer64	*		inputBufferList;
    IOAudioEnginePosition			nextOutputPosition;
    AbsoluteTime					outputTimeout;
    AbsoluteTime					sampleInterval;
    IOAudioClientBufferSet *		mNextBufferSet;
    thread_call_t					watchdogThreadCall;
    UInt32							generationCount;
    bool							timerPending;
    
    bool init(UInt32 setID, IOAudioEngineUserClient *client);
    void free();
    
#ifdef DEBUG
    void retain() const;
    void release() const;
#endif

    void resetNextOutputPosition();

    void allocateWatchdogTimer();
    void freeWatchdogTimer();
    
    void setWatchdogTimeout(AbsoluteTime *timeout);
    void cancelWatchdogTimer();

    static void watchdogTimerFired(IOAudioClientBufferSet *clientBufferSet, UInt32 generationCount);
};

OSDefineMetaClassAndStructors(IOAudioClientBufferSet, OSObject)


bool IOAudioClientBufferSet::init(UInt32 setID, IOAudioEngineUserClient *client)
{
	bool			result = false;
	
    audioDebugIOLog(4, "+ IOAudioClientBufferSet[%p]::init(%lx, %p)\n", this, (long unsigned int)setID, client);

	if ( super::init () )
	{
		if ( 0 != client )
		{
			bufferSetID = setID;
			client->retain();
			userClient = client;
			
			outputBufferList = NULL;
			inputBufferList = NULL;
			mNextBufferSet = NULL;
			watchdogThreadCall = NULL;
			generationCount = 0;
			timerPending = false;
			
			resetNextOutputPosition();
			result = true;
		}
	}
	
    audioDebugIOLog(4, "- IOAudioClientBufferSet[%p]::init(%lx, %p) returns %d\n", this, (long unsigned int)setID, client, result );
    return result;
}

void IOAudioClientBufferSet::free()
{
    if (watchdogThreadCall) {
        freeWatchdogTimer();
    }
    
    if (userClient != NULL) {
        userClient->release();
        userClient = NULL;
    }
    
    super::free();
	
    audioDebugIOLog(4, "+- IOAudioClientBufferSet[%p]::free()\n", this);
	return;
}

#ifdef DEBUG
void IOAudioClientBufferSet::retain() const
{
   super::retain();
}

void IOAudioClientBufferSet::release() const
{
    super::release();
}
#endif

void IOAudioClientBufferSet::resetNextOutputPosition()
{
    nextOutputPosition.fLoopCount = 0;
    nextOutputPosition.fSampleFrame = 0;
}

void IOAudioClientBufferSet::allocateWatchdogTimer()
{
    if (watchdogThreadCall == NULL) {
        watchdogThreadCall = thread_call_allocate((thread_call_func_t)IOAudioClientBufferSet::watchdogTimerFired, (thread_call_param_t)this);
    }
	
    audioDebugIOLog(4, "+- IOAudioClientBufferSet[%p]::allocateWatchdogTimer()\n", this);
	return;
}

void IOAudioClientBufferSet::freeWatchdogTimer()
{
    if (watchdogThreadCall != NULL) {
        cancelWatchdogTimer();
        thread_call_free(watchdogThreadCall);
        watchdogThreadCall = NULL;
    }
	
    audioDebugIOLog(4, "+- IOAudioClientBufferSet[%p]::freeWatchdogTimer()\n", this);
	return;
}

void IOAudioClientBufferSet::setWatchdogTimeout(AbsoluteTime *timeout)
{
	bool				result;
    UInt64 nanos;

    if (watchdogThreadCall == NULL) {
        // allocate it here
        audioErrorIOLog("IOAudioClientBufferSet::setWatchdogTimeout() - no thread call.\n");
    }
    
    assert(watchdogThreadCall);
    
    outputTimeout = *timeout;
    
    generationCount++;
    
	userClient->lockBuffers();

	retain();
    
    timerPending = true;

    absolutetime_to_nanoseconds(outputTimeout, &nanos);
    AudioTrace(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientSetWatchdogTimeout, (uintptr_t)this, generationCount, (nanos/1000), outputTimeout);
    
    result = thread_call_enter1_delayed(watchdogThreadCall, (thread_call_param_t)(uintptr_t)generationCount, outputTimeout);
	if (result) {
		release();		// canceled the previous call
	}

	userClient->unlockBuffers();
}

void IOAudioClientBufferSet::cancelWatchdogTimer()
{
    audioDebugIOLog(4, "+- IOAudioClientBufferSet[%p]::cancelWatchdogTimer()\n", this);
    AudioTrace_Start(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientCancelWatchdogTimer, (uintptr_t)this, timerPending, 0, 0);

	if (NULL != userClient) {
		userClient->retain();
		userClient->lockBuffers();
		if (timerPending) {
			timerPending = false;
			if (thread_call_cancel(watchdogThreadCall))
				release();
		}
		userClient->unlockBuffers();
		userClient->release();
	}
	
    AudioTrace_End(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientCancelWatchdogTimer, (uintptr_t)this, timerPending, 0, 0);
	return;
}

void IOAudioClientBufferSet::watchdogTimerFired(IOAudioClientBufferSet *clientBufferSet, UInt32 generationCount)
{
    IOAudioEngineUserClient *userClient;

	assert(clientBufferSet);
    assert(clientBufferSet->userClient);

    AudioTrace(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientWatchdogTimerFired, (uintptr_t)0x1111, generationCount, clientBufferSet->nextOutputPosition.fLoopCount, clientBufferSet->nextOutputPosition.fSampleFrame);
	if (clientBufferSet) {
#ifdef DEBUG
		AbsoluteTime now;
		clock_get_uptime(&now);
		
		audioDebugIOLog(3, "+ IOAudioClientBufferSet[%p]::watchdogTimerFired(%ld):(%llu)(%llu)(%lx,%lx)\n",
							clientBufferSet, 
							(long int)generationCount, 
							now, 
							clientBufferSet->outputTimeout, 
							(long unsigned int)clientBufferSet->nextOutputPosition.fLoopCount, 
							(long unsigned int)clientBufferSet->nextOutputPosition.fSampleFrame);
		
#endif /* DEBUG */

		userClient = clientBufferSet->userClient;
		if (userClient) {
			userClient->retain();
			userClient->lockBuffers();
	
			if(clientBufferSet->timerPending != false) {
				userClient->performWatchdogOutput(clientBufferSet, generationCount);
			}
	
			clientBufferSet->release();

			userClient->unlockBuffers();
			userClient->release();
		}

		// clientBufferSet->release code was down here...
	
#ifdef DEBUG
		audioDebugIOLog(3, "- IOAudioClientBufferSet[%p]::watchdogTimerFired(%ld):(%llu)(%llu)(%lx,%lx)\n",
							clientBufferSet, 
							(long int)generationCount, 
							now, 
							clientBufferSet->outputTimeout, 
							(long unsigned int)clientBufferSet->nextOutputPosition.fLoopCount, 
							(long unsigned int)clientBufferSet->nextOutputPosition.fSampleFrame);
#endif /* DEBUG */
	} else {
		IOLog ("IOAudioClientBufferSet::watchdogTimerFired assert (clientBufferSet == NULL) failed\n");
	}
	return;
}

#pragma mark -
#pragma mark IOAudioEngineUserClient
#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOAudioEngineUserClient, IOUserClient)

OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 0);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 1);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 2);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 3);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 4);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 5);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 6);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 7);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 8);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 9);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 10);
OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 11);


OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 12);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 13);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 14);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 15);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 16);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 17);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 18);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 19);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 20);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 21);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 22);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 23);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 24);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 25);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 26);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 27);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 28);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 29);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 30);
OSMetaClassDefineReservedUnused(IOAudioEngineUserClient, 31);

// New code added here

// OSMetaClassDefineReservedUsed(IOAudioEngineUserClient, 5);
//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

bool IOAudioEngineUserClient::initWithAudioEngine(IOAudioEngine *engine, task_t task, void *securityToken, UInt32 type, OSDictionary* properties)
{
	bool			result = false;
	
    audioDebugIOLog(3, "+ IOAudioEngineUserClient[%p]::initWithAudioEngine(%p, 0x%lx, %p, 0x%lx, %p)\n", this, engine, (long unsigned int)task, securityToken, (long unsigned int)type, properties);
    
	// Declare Rosetta compatibility
	if ( properties )
	{
		properties->setObject ( kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue );
	}
	
	if ( initWithTask ( task, securityToken, type, properties ) )
	{
		if ( engine && task )
		{
			clientTask = task;
			audioEngine = engine;
			
			setOnline(false);

			clientBufferSetList = NULL;
			
			clientBufferLock = IORecursiveLockAlloc();
			if ( clientBufferLock )
			{
				workLoop = audioEngine->getWorkLoop();
				if ( workLoop )
				{
					workLoop->retain();
					
					commandGate = IOCommandGate::commandGate(this);
					if ( commandGate )
					{
						reserved = IOMallocType(ExpansionData);
						if ( reserved )
						{
							reserved->commandGateStatus = kCommandGateStatus_Normal;	// <rdar://8518215>
							
							read_random(&reserved->connectionID, sizeof(reserved->connectionID)); // <rdar://22327973>
							audioDebugIOLog(3, "  ConnectionID:0x%x\n", reserved->connectionID);
                            
							workLoop->addEventSource(commandGate);
							
							trap.object = this;
							trap.func = (IOTrap) &IOAudioEngineUserClient::performClientIO;
							result = true;
						}
					}
				}
			}
		}
	}

    audioDebugIOLog(3, "- IOAudioEngineUserClient[%p]::initWithAudioEngine(%p, 0x%lx, %p, 0x%lx, %p) returns %d\n", this, engine, (long unsigned int)task, securityToken, (long unsigned int)type, properties, result);
    return result;
}

// Used so that a pointer to a kernel IOAudioStream isn't passed out of the kernel ( 32 bit version )
//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

IOReturn IOAudioEngineUserClient::safeRegisterClientBuffer(UInt32 audioStreamIndex, void * sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID) {

	audioDebugIOLog(3, "IOAudioEngineUserClient::safeRegisterClientBuffer deprecated for 32 bit %p \n", sourceBuffer); 
	IOAudioStream *					audioStream;
	IOReturn						result = kIOReturnBadArgument;
	
	audioDebugIOLog(3, "+ IOAudioEngineUserClient::safeRegisterClientBuffer32 %p \n", sourceBuffer); 
	
	__Require_Action_String(audioEngine != NULL, Exit, result = kIOReturnError, "audioEngine is NULL");

	audioStream = audioEngine->getStreamForID(audioStreamIndex);
	if (!audioStream)
	{
		audioDebugIOLog(3, "no stream associated with audioStreamIndex 0x%lx \n", (long unsigned int)audioStreamIndex); 
	}
	else
	{
		result = registerClientBuffer(audioStream, sourceBuffer, bufSizeInBytes, bufferSetID);
	}
	
 Exit:
	audioDebugIOLog(3, "- IOAudioEngineUserClient::safeRegisterClientBuffer32 %p returns 0x%lX\n", sourceBuffer, (long unsigned int)result ); 
	return result;
	
}

// Used so that a pointer to a kernel IOAudioStream isn't passed out of the kernel ( 64 bit version ) <rdar://problems/5321701>
// New method added for 64 bit support <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::safeRegisterClientBuffer64(UInt32 audioStreamIndex, mach_vm_address_t * sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID) 
{
	IOReturn				retVal = kIOReturnBadArgument; 
	IOAudioStream *			audioStream;
	audioDebugIOLog(3, "+ IOAudioEngineUserClient::safeRegisterClientBuffer64 %p \n", sourceBuffer); 
	
	__Require_Action_String(audioEngine != NULL, Exit, retVal = kIOReturnError, "audioEngine is NULL");

	audioStream = audioEngine->getStreamForID(audioStreamIndex);
	if (!audioStream) {
		audioDebugIOLog(3, "  no stream associated with audioStreamIndex 0x%lx \n", (long unsigned int)audioStreamIndex);
	}
	else
	{
		retVal = registerClientBuffer64(audioStream, * sourceBuffer, bufSizeInBytes, bufferSetID);
	}
 Exit:
	audioDebugIOLog(3, "- IOAudioEngineUserClient::safeRegisterClientBuffer64  returns 0x%lX\n", (long unsigned int)retVal  ); 
	return retVal;
}
// Used to pass extra information about how many samples are actually in a buffer and other things related to interesting non-mixable audio formats.
IOReturn IOAudioEngineUserClient::registerClientParameterBuffer (void  * paramBuffer, UInt32 bufferSetID)
{
	IOReturn						result = kIOReturnSuccess;
	IOAudioClientBufferSet			*bufferSet = NULL;
	IOAudioClientBufferExtendedInfo64 *extendedInfo = nullptr;

	audioDebugIOLog(3, "+ IOAudioEngineUserClient::registerClientParameterBuffer() - result = 0x%x\n", result);
	
    if (!isInactive()) {
        if (!paramBuffer || ((IOAudioStreamDataDescriptor *)paramBuffer)->fVersion > kStreamDataDescriptorCurrentVersion) {
            return kIOReturnBadArgument;
        }
        
        lockBuffers();		// added here because it was turned off in findBufferSet // MPC

		// this buffer set can't have already been registered with extended info
        extendedInfo = findExtendedInfo64 (bufferSetID);
		if (extendedInfo) 
		{
			unlockBuffers();
            return kIOReturnBadArgument;
		}

		// make sure that this buffer set has already been registered for output
        bufferSet = findBufferSet(bufferSetID);

		unlockBuffers();
		
        if (bufferSet) {
			IOAudioClientBufferExtendedInfo64 *info;
			
			extendedInfo = IOMallocType(IOAudioClientBufferExtendedInfo64);
			if (!extendedInfo) {
				return kIOReturnError;
			}

			// Can only be for output buffers, so always kIODirectionIn
			extendedInfo->mAudioClientBufferExtended32.paramBufferDescriptor = IOMemoryDescriptor::withAddressRange(* (mach_vm_address_t*)paramBuffer, (((IOAudioStreamDataDescriptor *)paramBuffer)->fNumberOfStreams * 4) + 8, kIODirectionIn, clientTask);
			if (!extendedInfo->mAudioClientBufferExtended32.paramBufferDescriptor) 
			{
				result = kIOReturnInternalError;
				goto Exit;
			}
			
			if ((result = extendedInfo->mAudioClientBufferExtended32.paramBufferDescriptor->prepare()) != kIOReturnSuccess) 
			{
				goto Exit;
			}
			
			extendedInfo->mAudioClientBufferExtended32.paramBufferMap = extendedInfo->mAudioClientBufferExtended32.paramBufferDescriptor->map();
			
			if (extendedInfo->mAudioClientBufferExtended32.paramBufferMap == NULL) 
			{
				IOLog("IOAudioEngineUserClient::registerClientParameterBuffer() - error mapping memory.\n");
				result = kIOReturnVMError;
				goto Exit;
			}
			
			extendedInfo->mAudioClientBufferExtended32.paramBuffer = (void *)extendedInfo->mAudioClientBufferExtended32.paramBufferMap->getVirtualAddress();
			if (extendedInfo->mAudioClientBufferExtended32.paramBuffer == NULL)
			{
				result = kIOReturnVMError;
				goto Exit;
			}
	
			extendedInfo->mUnmappedParamBuffer64 = * (mach_vm_address_t*)paramBuffer;
			
			if (reserved->extendedInfo) 
			{
				// Get to the end of the linked list of extended info and add this new entry there
				info = reserved->extendedInfo;
				while (info->mNextExtended64)
				{
					info = info->mNextExtended64;
				}

				info->mNextExtended64 = extendedInfo;
			} 
			else 
			{
				// The list is empty, so this the start of the list
				reserved->extendedInfo = extendedInfo;
			}
		}
	}
	else
	{
        result = kIOReturnNoDevice;
    }

Exit:
	audioDebugIOLog(3, "- IOAudioEngineUserClient::registerClientParameterBuffer() - result = 0x%lX\n", (long unsigned int)result);
	if (result != kIOReturnSuccess && extendedInfo)
	{
		OSSafeReleaseNULL(extendedInfo->mAudioClientBufferExtended32.paramBufferMap);
		OSSafeReleaseNULL(extendedInfo->mAudioClientBufferExtended32.paramBufferDescriptor);
		IOFreeType(extendedInfo, IOAudioClientBufferExtendedInfo64);
	}
	return result;
}

IOAudioClientBufferExtendedInfo *IOAudioEngineUserClient::findExtendedInfo(UInt32 bufferSetID)
{
	IOAudioClientBufferExtendedInfo64 *extendedInfo; // <rdar://problems/5321701>
    
IOAudioClientBufferExtendedInfo * retVal = NULL;
    extendedInfo = reserved->extendedInfo;
    while (extendedInfo && (extendedInfo->mAudioClientBufferExtended32.bufferSetID != bufferSetID)) 
	{
        extendedInfo = extendedInfo->mNextExtended64;
    }
    if ( extendedInfo)
	{
    	retVal = &(extendedInfo->mAudioClientBufferExtended32);
	}
	return retVal;
}

// New method added for 64 bit support <rdar://problems/5321701>
IOAudioClientBufferExtendedInfo64 *IOAudioEngineUserClient::findExtendedInfo64(UInt32 bufferSetID)
{
    IOAudioClientBufferExtendedInfo64 *extendedInfo; // <rdar://problems/5321701>
    
    extendedInfo = reserved->extendedInfo;
    while (extendedInfo && (extendedInfo->mAudioClientBufferExtended32.bufferSetID != bufferSetID)) {
        extendedInfo = extendedInfo->mNextExtended64;
    }
    
    return extendedInfo;
}
IOReturn IOAudioEngineUserClient::getNearestStartTime(IOAudioStream *audioStream, IOAudioTimeStamp *ioTimeStamp, UInt32 isInput)
{
	IOReturn ret = kIOReturnError;
	
	// <rdar://7363756>, <rdar://7529580>
	if ( workLoop )
	{
		ret = workLoop->runAction(_getNearestStartTimeAction, this, (void *)audioStream, (void *)ioTimeStamp, (void *)(uintptr_t)isInput);	// <rdar://7529580>
	}

	return ret;
}

// <rdar://7529580>
IOReturn IOAudioEngineUserClient::_getNearestStartTimeAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, target);
        if (userClient) {
			if (userClient->commandGate) {
				setCommandGateUsage(userClient, true);	// <rdar://8518215>
                result = userClient->commandGate->runAction(getNearestStartTimeAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(userClient, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::getNearestStartTimeAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        
        if (userClient) {
			
			UInt32 tempBoolArgument = (UInt64)arg4;

            result = userClient->getClientNearestStartTime((IOAudioStream *)arg1, (IOAudioTimeStamp *)arg2, tempBoolArgument);
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::getClientNearestStartTime(IOAudioStream *audioStream, IOAudioTimeStamp *ioTimeStamp, UInt32 isInput)
{
    IOReturn result = kIOReturnError;

    if (audioEngine && !isInactive()) {
		result = audioEngine->getNearestStartTime(audioStream, ioTimeStamp, isInput);
	}

	return result;
}

IOAudioEngineUserClient *IOAudioEngineUserClient::withAudioEngine(IOAudioEngine *engine, task_t clientTask, void *securityToken, UInt32 type, OSDictionary *properties)
{
    IOAudioEngineUserClient *client;
    
    client = new IOAudioEngineUserClient;

    if (client) {
        if (!client->initWithAudioEngine(engine, clientTask, securityToken, type, properties)) {
            client->release();
            client = NULL;
        }
    }

    audioDebugIOLog(3, "+- IOAudioEngineUserClient::withAudioEngine(%p, 0x%lx, %p, 0x%lx, %p) returns %p\n", engine, (long unsigned int)clientTask, securityToken, (long unsigned int)type, properties, client );
    return client;
}

// Original code
IOAudioEngineUserClient *IOAudioEngineUserClient::withAudioEngine(IOAudioEngine *engine, task_t clientTask, void *securityToken, UInt32 type)
{
	IOAudioEngineUserClient *client;

	client = new IOAudioEngineUserClient;

	if (client) {
		if (!client->initWithAudioEngine(engine, clientTask, securityToken, type)) {
			client->release();
			client = NULL;
		}
	}

	audioDebugIOLog(3, "+- IOAudioEngineUserClient::withAudioEngine(%p, 0x%lx, %p, 0x%lx) returns %p\n", engine, (long unsigned int)clientTask, securityToken, (long unsigned int)type, client);
	return client;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

bool IOAudioEngineUserClient::initWithAudioEngine(IOAudioEngine *engine, task_t task, void *securityToken, UInt32 type)
{
	bool			result = false;
	
    audioDebugIOLog(3, "+ IOAudioEngineUserClient[%p]::initWithAudioEngine(%p, 0x%lx, %p, 0x%lx)\n", this, engine, (long unsigned int)task, securityToken, (long unsigned int)type);
    
	if ( initWithTask( task, securityToken, type ) )
	{
		if ( engine && task )
		{
			clientTask = task;
			audioEngine = engine;
			
			setOnline(false);

			clientBufferSetList = NULL;
			
			clientBufferLock = IORecursiveLockAlloc();
			if ( clientBufferLock )
			{
				workLoop = audioEngine->getWorkLoop();
				if ( workLoop )
				{
					workLoop->retain();
					
					commandGate = IOCommandGate::commandGate(this);
					if ( commandGate )
					{
						reserved = IOMallocType(ExpansionData);
						if ( reserved )
						{
							reserved->commandGateStatus = kCommandGateStatus_Normal;	// <rdar://8518215>
							
							read_random(&reserved->connectionID, sizeof(reserved->connectionID)); // <rdar://22327973>
							audioDebugIOLog(3, "  ConnectionID:0x%x\n", reserved->connectionID);
                            
							workLoop->addEventSource(commandGate);
							
							trap.object = this;
							trap.func = (IOTrap) &IOAudioEngineUserClient::performClientIO;
							result = true;
						}
					}
				}
			}
		}
	}
	
    audioDebugIOLog(3, "- IOAudioEngineUserClient[%p]::initWithAudioEngine(%p, 0x%lx, %p, 0x%lx) returns %d\n", this, engine, (long unsigned int)task, securityToken, (long unsigned int)type, result );
    return result;
}

void IOAudioEngineUserClient::free()
{
	IOAudioClientBufferExtendedInfo64 *			cur;
	IOAudioClientBufferExtendedInfo64 *			prev;

    freeClientBufferSetList();			// Moved above clientBufferLock code below
    
    if (clientBufferLock) {
        IORecursiveLockFree(clientBufferLock);
        clientBufferLock = NULL;
    }
    
    if (notificationMessage) {
        IOFreeType(notificationMessage, IOAudioNotificationMessage);
        notificationMessage = NULL;
    }
    
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
		if (NULL != reserved->extendedInfo) {
			cur = reserved->extendedInfo;
			while (cur) {
				prev = cur;
				cur = cur->mNextExtended64;
				OSSafeReleaseNULL(prev->mAudioClientBufferExtended32.paramBufferMap);
				OSSafeReleaseNULL(prev->mAudioClientBufferExtended32.paramBufferDescriptor);
				IODelete(prev, IOAudioClientBufferExtendedInfo64, 1);
			}
		}
		IOFreeType (reserved, ExpansionData);
	}

    super::free();
	
    audioDebugIOLog(4, "+- IOAudioEngineUserClient[%p]::free()\n", this);
	return;
}

void IOAudioEngineUserClient::freeClientBufferSetList()
{
	// <rdar://9180891> Lock the buffers to synchronize access to the client buffer list.
	// This is to prevent race condition between freeClientBufferList, unregisterClientBuffer64 
	// & stopClient when calling IOAudioStream::removeClient().
	lockBuffers();
	
    while (clientBufferSetList) {
        IOAudioClientBufferSet *nextSet;
        
		// Move call up here to fix 3472373
        clientBufferSetList->cancelWatchdogTimer();

        while (clientBufferSetList->outputBufferList) {
            IOAudioClientBuffer64 *nextBuffer = clientBufferSetList->outputBufferList->mNextBuffer64;
            
            freeClientBuffer(clientBufferSetList->outputBufferList);
            
            clientBufferSetList->outputBufferList = nextBuffer;
        }

        while (clientBufferSetList->inputBufferList) {
            IOAudioClientBuffer64 *next = clientBufferSetList->inputBufferList->mNextBuffer64;
            
            freeClientBuffer(clientBufferSetList->inputBufferList);
            
            clientBufferSetList->inputBufferList = next;
        }
        
        nextSet = clientBufferSetList->mNextBufferSet;
        
        clientBufferSetList->release();
        
        clientBufferSetList = nextSet;
    }
    
	unlockBuffers();	// <rdar://9180891>
}

void IOAudioEngineUserClient::freeClientBuffer(IOAudioClientBuffer64 *clientBuffer) 
{
    if (clientBuffer) {
        if (clientBuffer->mAudioClientBuffer32.audioStream) {
            clientBuffer->mAudioClientBuffer32.audioStream->removeClient(&(clientBuffer->mAudioClientBuffer32) ); 
            clientBuffer->mAudioClientBuffer32.audioStream->release();
			clientBuffer->mAudioClientBuffer32.audioStream = NULL;
        }
        
        if (clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor != NULL) {
            clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor->complete();
            clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor->release();
			clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor = NULL;
        }
        
        if (clientBuffer->mAudioClientBuffer32.sourceBufferMap != NULL) {
            clientBuffer->mAudioClientBuffer32.sourceBufferMap->release();
			clientBuffer->mAudioClientBuffer32.sourceBufferMap = NULL;
        }

        IOFreeType(clientBuffer, IOAudioClientBuffer64);
		clientBuffer = NULL;
    }
}

void IOAudioEngineUserClient::stop(IOService *provider)
{
	// <rdar://7363756>
	if ( commandGate )
	{
		commandGate->runAction(stopClientAction);
	}
    
    // We should be both inactive and offline at this point, 
    // so it is safe to free the client buffer set list without holding the lock
    
    freeClientBufferSetList();

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
	
    audioDebugIOLog(3, "+- IOAudioEngineUserClient[%p]::stop(%p)\n", this, provider);
	return;
}

IOReturn IOAudioEngineUserClient::clientClose()
{
    IOReturn result = kIOReturnSuccess;
    
	// <rdar://7363756>
    if (audioEngine && workLoop && !isInactive()) {					// <rdar://7529580>
        result = workLoop->runAction(_closeClientAction, this);		// <rdar://7529580>
    }
    
    audioDebugIOLog(4, "+- IOAudioEngineUserClient[%p]::clientClose() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

IOReturn IOAudioEngineUserClient::clientDied()
{
	IOReturn			result;
	
    result = clientClose();
	
    audioDebugIOLog(3, "+- IOAudioEngineUserClient[%p]::clientDied() returns 0x%lX\n", this, (long unsigned int)result);
	return result;
}

// <rdar://7529580>
IOReturn IOAudioEngineUserClient::_closeClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, target);
        if (userClient) {
			if (userClient->commandGate) {
				setCommandGateUsage(userClient, true);	// <rdar://8518215>
                result = userClient->commandGate->runAction(closeClientAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(userClient, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::closeClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        if (userClient) {
            result = userClient->closeClient();
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::closeClient()
{
    if (audioEngine && !isInactive()) {
        if (isOnline()) {
            stopClient();
        }
        audioEngine->clientClosed(this);
        audioEngine = NULL;
    }
    
    audioDebugIOLog(4, "+- IOAudioEngineUserClient[%p]::closeClient() returns 0x%lX\n", this, (long unsigned int)kIOReturnSuccess );
    return kIOReturnSuccess;
}

void IOAudioEngineUserClient::setOnline(bool newOnline)
{
    if (online != newOnline) {
        online = newOnline;
        setProperty(kIOAudioEngineUserClientActiveKey, (unsigned long long)(online ? 1 : 0), sizeof(unsigned long long)*8);
    }
	
    audioDebugIOLog(3, "+- IOAudioEngineUserClient[%p]::setOnline(%d)\n", this, newOnline);
	return;
}

bool IOAudioEngineUserClient::isOnline()
{
    return online;
}

void IOAudioEngineUserClient::lockBuffers()
{
    assert(clientBufferLock);
    
    IORecursiveLockLock(clientBufferLock);
}

void IOAudioEngineUserClient::unlockBuffers()
{
    assert(clientBufferLock);
    
    IORecursiveLockUnlock(clientBufferLock);
}

IOReturn IOAudioEngineUserClient::clientMemoryForType(UInt32 type, UInt32 *flags, IOMemoryDescriptor **memory)
{
    IOReturn						result = kIOReturnSuccess;
	IOBufferMemoryDescriptor		*theMemoryDescriptor = NULL;

    audioDebugIOLog(3, "+ IOAudioEngineUserClient[%p]::clientMemoryForType(0x%lx, 0x%lx, %p)\n", this, (long unsigned int)type, (long unsigned int)*flags, memory);

	assert(audioEngine);
	__Require_Action_String(audioEngine != NULL, Exit, result = kIOReturnError, "audioEngine is NULL");

    switch(type) {
        case kIOAudioStatusBuffer:
			theMemoryDescriptor = audioEngine->getStatusDescriptor();
            break;
		case kIOAudioBytesInInputBuffer:
			theMemoryDescriptor = audioEngine->getBytesInInputBufferArrayDescriptor();
			break;
		case kIOAudioBytesInOutputBuffer:
			theMemoryDescriptor = audioEngine->getBytesInOutputBufferArrayDescriptor();
			break;
        default:
            result = kIOReturnUnsupported;
            break;
    }

	if (!result && theMemoryDescriptor) {
		theMemoryDescriptor->retain();		// Don't release it, it will be released by mach-port automatically
		*memory = theMemoryDescriptor;
		*flags = kIOMapReadOnly;
	} else {
		result = kIOReturnError;
	}

 Exit:
    audioDebugIOLog(3, "- IOAudioEngineUserClient[%p]::clientMemoryForType(0x%lx, 0x%lx, %p) returns 0x%lX\n", this, (long unsigned int)type, (long unsigned int)*flags, memory, (long unsigned int)result );
    return result;
}

IOExternalMethod *IOAudioEngineUserClient::getExternalMethodForIndex(UInt32 index)
{
    return NULL;
}

IOExternalTrap *IOAudioEngineUserClient::getExternalTrapForIndex( UInt32 index )
{
	IOExternalTrap *result = NULL;
	
    if (index == kIOAudioEngineTrapPerformClientIO)
    {
		result = &trap;
	}
    else if (index == (0x1000 | kIOAudioEngineTrapPerformClientIO))
    {
        // this is not really a special flag on the trap index. this is a byte swapped index which was used
        // for System 9. the index must have only been 16 bits back then. we should probably get rid of this
        reserved->classicMode = 1;              // System 9?
        result = &trap;
    }
    return result;
}

IOReturn IOAudioEngineUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon)
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(4, "+ IOAudioEngineUserClient[%p]::registerNotificationPort(0x%lx, 0x%lx, 0x%lx)\n", this, (long unsigned int)port, (long unsigned int)type, (long unsigned int)refCon);

    switch (type) {
        case kIOAudioEngineAllNotifications:
			// <rdar://7363756>, <rdar://7529580>
			if ( workLoop )
			{
				result = workLoop->runAction(_registerNotificationAction, this, (void *)port, (void *)(uintptr_t)refCon);	// <rdar://7529580>
			}
			else
			{
				result = kIOReturnError;
			}
            
            break;
        default:
            audioErrorIOLog("IOAudioEngineUserClient::registerNotificationPort() - ERROR: invalid notification type specified - no notifications will be sent.\n");
            result = kIOReturnBadArgument;
            break;
    }
    // Create a single message, but keep a dict or something of all of the IOAudioStreams registered for
    // refCon is IOAudioStream *
    
    audioDebugIOLog(3, "- IOAudioEngineUserClient[%p]::registerNotificationPort(0x%lx, 0x%lx, 0x%lx) returns 0x%lX\n", this, (long unsigned int)port, (long unsigned int)type, (long unsigned int)refCon, (long unsigned int)result );
    return result;
}

// <rdar://7529580>
IOReturn IOAudioEngineUserClient::_registerNotificationAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, target);
        if (userClient) {
            if (userClient->commandGate) {
				setCommandGateUsage(userClient, true);	// <rdar://8518215>
                result = userClient->commandGate->runAction(registerNotificationAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(userClient, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::registerNotificationAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    audioDebugIOLog(4, "+ IOAudioEngineUserClient::registerNotificationAction(%p, %p)\n", owner, arg1);

    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        
        if (userClient) {
			UInt64	refCon = (UInt64) arg2;

            result = userClient->registerNotification((mach_port_t)arg1, refCon);
        }
    }
    
    audioDebugIOLog(3, "- IOAudioEngineUserClient::registerNotificationAction(%p, %p) returns 0x%lX\n", owner, arg1, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngineUserClient::registerNotification(mach_port_t port, UInt32 refCon)
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(4, "+ IOAudioEngineUserClient[%p]::registerFormatNotification(0x%lx, 0x%lx)\n", this, (long unsigned int)port, (long unsigned int)refCon);

    if (!isInactive()) {
        if (port == MACH_PORT_NULL) {	// We need to remove this notification
            if (notificationMessage != NULL) {
                notificationMessage->messageHeader.msgh_remote_port = MACH_PORT_NULL;
            }
        } else {
            if (notificationMessage == NULL) {
                notificationMessage = IOMallocType(IOAudioNotificationMessage);
                
                if (notificationMessage) {
                    notificationMessage->messageHeader.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
                    notificationMessage->messageHeader.msgh_size = sizeof(IOAudioNotificationMessage);
                    notificationMessage->messageHeader.msgh_local_port = MACH_PORT_NULL;
                    notificationMessage->messageHeader.msgh_reserved = 0;
                    notificationMessage->messageHeader.msgh_id = 0;
                    notificationMessage->messageHeader.msgh_remote_port = port;
                    notificationMessage->ref = refCon;              
                } else {
                    result = kIOReturnNoMemory;
                }
            } else {
            	notificationMessage->messageHeader.msgh_remote_port = port;
                notificationMessage->ref = refCon;         
            }
        }
    } else {
        result = kIOReturnNoDevice;
    }
    
    audioDebugIOLog(3, "- IOAudioEngineUserClient[%p]::registerFormatNotification(0x%lx, 0x%lx) returns 0x%lX\n", this, (long unsigned int)port, (long unsigned int)refCon, (long unsigned int)result );
    return result;
}


IOReturn IOAudioEngineUserClient::externalMethod ( uint32_t selector, IOExternalMethodArguments * arguments, 
	IOExternalMethodDispatch * dispatch, OSObject * target, void * reference)
{
	IOReturn result = kIOReturnBadArgument;
	audioDebugIOLog(3, "+ IOAudioEngineUserClient::externalMethod, selector=0x%x,   arg0 0x%llX, arg1 0x%llx, arg2 0x%llx arg3 0x%llx \n", 
					selector, arguments->scalarInput[0], arguments->scalarInput[1], arguments->scalarInput[2], arguments->scalarInput[3]);
    audioDebugIOLog(3, "  scalarInputCount=0x%x  structureInputSize 0x%x, scalarOutputCount 0x%x, structureOutputSize 0x%x \n", 
					arguments->scalarInputCount, arguments->structureInputSize, arguments->scalarOutputCount, arguments->structureOutputSize );

	// require entitlement for all external methods
	OSObject *entitlement = copyClientEntitlement(current_task(), kDriverHelper_DriverHostEntitlement);
	bool entitled = false;
	if ((entitlement != NULL) && (entitlement == kOSBooleanTrue)) {
		entitled = true;
	}
	__Require_Action_String(entitled, Exit, result = kIOReturnNotPrivileged, "not entitled");

	// Dispatch the method call
	switch (selector)
	{
	case kIOAudioEngineCallRegisterClientBuffer:
		if (arguments != 0)		
		{
			if ( arguments->scalarInputCount >= 4 )		//	<rdar://9204853>
			{
				result = registerBuffer64((IOAudioStream *)arguments->scalarInput[0], (mach_vm_address_t)arguments->scalarInput[1],
						(UInt32)arguments->scalarInput[2], (UInt32)arguments->scalarInput[3] );
			}
			else
			{
				audioDebugIOLog(3, "  kIOAudioEngineCallRegisterClientBuffer: invalid input argument count %d. Need at least 4.\n",
					arguments->scalarInputCount);
			}
		}
		break;
	case kIOAudioEngineCallUnregisterClientBuffer:
		if (arguments != 0)		
		{
			if ( arguments->scalarInputCount >= 2 )		//	<rdar://9204853>
			{
				result = unregisterBuffer64((mach_vm_address_t)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1] );
			}
			else
			{
				audioDebugIOLog(3, "  kIOAudioEngineCallUnregisterClientBuffer: invalid input argument count %d. Need at least 2.\n",
					arguments->scalarInputCount);
			}
		}
		break;
	case kIOAudioEngineCallGetConnectionID:
		if (arguments != 0)
		{
			if ( arguments->scalarOutputCount >= 1 )
			{
				result = getConnectionID((uint32_t *) &arguments->scalarOutput[0]);
			}
			else
			{
				audioDebugIOLog(3, "  kIOAudioEngineCallGetConnectionID: invalid output argument count %d. Need at least 1.\n",
					arguments->scalarOutputCount);
			}
		}
		break;
	case kIOAudioEngineCallStart:
		result = clientStart();
		break;
	case kIOAudioEngineCallStop:
		result = clientStop();
		break;
	case kIOAudioEngineCallGetNearestStartTime:
		if (arguments != 0)
		{
			if ( arguments->scalarInputCount >= 3 )
			{
				result = getNearestStartTime((IOAudioStream *)arguments->scalarInput[0], (IOAudioTimeStamp *)arguments->scalarInput[1],
                                                (UInt32)arguments->scalarInput[2]);
			}
			else
			{
				audioDebugIOLog(3, "  kIOAudioEngineCallGetNearestStartTime: invalid input argument count %d. Need at least 3.\n",
					arguments->scalarInputCount);
			}
		}
		break;
	default:
		result = super::externalMethod(selector, arguments, dispatch, target, reference );
		break;
	}

 Exit:

	audioDebugIOLog(3, "- IOAudioEngineUserClient::externalMethod returns 0x%lX\n", (long unsigned int)result );
	OSSafeReleaseNULL(entitlement);
	return result;
}

// 32 bit version <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::registerBuffer(IOAudioStream *audioStream, void * sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID)
{
    audioDebugIOLog(3, "+-IOAudioEngineUserClient::registerBuffer Deprecated 0x%llx %p 0x%lx 0x%lx\n", (unsigned long long )audioStream, sourceBuffer, (long unsigned int)bufSizeInBytes, (long unsigned int)bufferSetID); 

    return kIOReturnUnsupported;
}

// 64 bit version <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::registerBuffer64(IOAudioStream *audioStream, mach_vm_address_t sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID)
{
	IOReturn ret = kIOReturnError;
	
	audioDebugIOLog(3, "+ IOAudioEngineUserClient::registerBuffer64 0x%llx 0x%llx 0x%lx 0x%lx\n", (unsigned long long )audioStream, sourceBuffer, (long unsigned int)bufSizeInBytes, (long unsigned int)bufferSetID); 
	
	// <rdar://7363756>, <rdar://7529580>
	if ( workLoop )
	{
		ret = workLoop->runAction(_registerBufferAction, this, audioStream, &sourceBuffer, (void *)(uintptr_t)bufSizeInBytes, (void *)(uintptr_t)bufferSetID);	// <rdar://7529580>
	}
	
	audioDebugIOLog(3, "- IOAudioEngineUserClient::registerBuffer64 0x%llx 0x%llx 0x%lx 0x%lx returns 0x%lX\n", (unsigned long long )audioStream, sourceBuffer, (long unsigned int)bufSizeInBytes, (long unsigned int)bufferSetID, (long unsigned int)ret ); 
	return ret;
}

// 32 bit version <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::unregisterBuffer( void * sourceBuffer, UInt32 bufferSetID)
{
 	audioDebugIOLog(3, "+-IOAudioEngineUserClient::unregisterBuffer 32 bit version NOT SUPPORTED \n" ); 
    return kIOReturnUnsupported;
}

// 64 bit version <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::unregisterBuffer64( mach_vm_address_t  sourceBuffer, UInt32 bufferSetID)
{
	IOReturn ret = kIOReturnError;
    
	// <rdar://7363756>, <rdar://7529580>
	if ( workLoop )
	{
		ret = workLoop->runAction(_unregisterBufferAction, this, ( void * ) &sourceBuffer, (void *)(uintptr_t)bufferSetID);	// <rdar://7529580>
	}
	
	return ret;
}

// <rdar://7529580>
IOReturn IOAudioEngineUserClient::_registerBufferAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, target);
        if (userClient) {
            if (userClient->commandGate) {
				setCommandGateUsage(userClient, true);	// <rdar://8518215>
                result = userClient->commandGate->runAction(registerBufferAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(userClient, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::registerBufferAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
	
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        
        if (userClient) {
			UInt32 bufSizeInBytes	= (UInt32)((UInt64)arg3 & 0x00000000FFFFFFFFLLU);
			UInt32 bufferSetID		= (UInt32)((UInt64)arg4 & 0x00000000FFFFFFFFLLU);
			UInt32 audioStreamIndex	= (UInt32)((UInt64)arg1 & 0x00000000FFFFFFFFLLU);
			
			result = userClient->safeRegisterClientBuffer64( audioStreamIndex, ( mach_vm_address_t * ) arg2, bufSizeInBytes, bufferSetID);
        }
    }
    
	audioDebugIOLog(4, "+- IOAudioEngineUserClient::registerBufferAction %p returns 0x%lX\n", arg1, (long unsigned int)result );
    return result;
}

// <rdar://7529580>
IOReturn IOAudioEngineUserClient::_unregisterBufferAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, target);
        if (userClient) {
            if (userClient->commandGate) {
				setCommandGateUsage(userClient, true);	// <rdar://8518215>
                result = userClient->commandGate->runAction(unregisterBufferAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(userClient, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::unregisterBufferAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        
        if (userClient) {
			UInt32 bufferSetID =  (UInt32)((UInt64)arg2 & 0x00000000FFFFFFFFLLU);
            result = userClient->unregisterClientBuffer64( ( mach_vm_address_t * )arg1, bufferSetID);
        }
    }
    
    return result;
}
// 32 bit version <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::registerClientBuffer(IOAudioStream *audioStream, void * sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID)
{
	audioDebugIOLog(3, "+-IOAudioEngineUserClient[%p]::registerClientBuffer  32 bit version Deprecated (%p[%ld], %p, 0x%lx, 0x%lx)\n", this, audioStream, (long int)audioStream->getStartingChannelID(), sourceBuffer, (long unsigned int)bufSizeInBytes, (long unsigned int)bufferSetID);
	return kIOReturnUnsupported;
}
// 64 bit version <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::registerClientBuffer64(IOAudioStream *audioStream, mach_vm_address_t  sourceBuffer, UInt32 bufSizeInBytes, UInt32 bufferSetID)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioClientBuffer64 *clientBuffer;
    IODirection bufferDirection;
    const IOAudioStreamFormat *streamFormat;
   
    audioDebugIOLog(3, "+ IOAudioEngineUserClient[%p]::registerClientBuffer64  (%p[%ld], 0x%llx, 0x%lx, 0x%lx)\n", this, audioStream, (long int)audioStream->getStartingChannelID(), sourceBuffer, (long unsigned int)bufSizeInBytes, (long unsigned int)bufferSetID);
    if (!isInactive()) 
	{
        IOAudioClientBufferSet *clientBufferSet;
        IOAudioClientBuffer64 **clientBufferList;
        
	// rdar://problem/25156927
	//   ensure bufSizeInBytes is minimally large enough for the IOAudioBufferDataDescriptor metadata
        if (!sourceBuffer || !audioStream || (bufSizeInBytes < offsetof(IOAudioBufferDataDescriptor, fData)) )
		{
			audioDebugIOLog(3, "  bad argument\n");
           return kIOReturnBadArgument;
        }
        
		streamFormat = audioStream->getFormat();
        if (!streamFormat) 
		{
			audioDebugIOLog(3, "  no format\n");
            return kIOReturnError;
        }
        
        // Return an error if this is an unmixable stream and it already has a client
        if (!streamFormat->fIsMixable && (audioStream->getNumClients() != 0)) 
		{
			audioDebugIOLog(3, "  mix problem or client exists\n");
            return kIOReturnExclusiveAccess;
        }
        
		// <rdar://10282154> Check if the source buffer is already registered in the
		// buffer set. If it is already registered, unregister the existing source 
		// buffer from the buffer set.
		lockBuffers();
		
        clientBufferSet = findBufferSet(bufferSetID);
		if (clientBufferSet) {
			if (audioStream->getDirection() == kIOAudioStreamDirectionOutput) {
				clientBufferList = &clientBufferSet->outputBufferList;
			} else {
				clientBufferList = &clientBufferSet->inputBufferList;
			}
			
			assert(clientBufferList);
			
			if (*clientBufferList != NULL) {
				IOAudioClientBuffer64 *clientBufPtr = *clientBufferList;
				while (clientBufPtr != NULL) {
					if (clientBufPtr->mUnmappedSourceBuffer64 == sourceBuffer) {
						audioDebugIOLog(3, "  source buffer (0x%llx) already registered in buffer set 0x%lx. Unregister existing source buffer...\n", sourceBuffer, (long unsigned int)bufferSetID);
						unregisterClientBuffer64(&sourceBuffer, bufferSetID);
						break;
					}
					clientBufPtr = clientBufPtr->mNextBuffer64;
				}
			}
		}
		
		unlockBuffers();

        // allocate IOAudioClientBuffer to hold buffer descriptor, etc...
        clientBuffer = IOMallocType(IOAudioClientBuffer64);
        if (!clientBuffer) 
		{
			audioDebugIOLog(3, "  no clientbuffer\n");
	        result = kIOReturnNoMemory;
            goto Exit;
        }
		

       
        clientBuffer->mAudioClientBuffer32.userClient = this;
        
        bufferDirection = audioStream->getDirection() == kIOAudioStreamDirectionOutput ? kIODirectionIn : kIODirectionOut;
        
        audioStream->retain();
        clientBuffer->mAudioClientBuffer32.audioStream = audioStream;

         clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)sourceBuffer, (mach_vm_size_t)bufSizeInBytes, kIODirectionNone, clientTask);
        if (!clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor) 
		{
			audioDebugIOLog(3, "  no sourcebufferdescriptor\n");
			result = kIOReturnInternalError;
            goto Exit;
        }
        
        if ( kIOReturnSuccess != (result = clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor->prepare( kIODirectionOutIn ) ) ) 
		{
				audioDebugIOLog(3, "  prepare error \n");
				goto Exit;
	      }
        
        clientBuffer->mAudioClientBuffer32.sourceBufferMap = clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor->map();
        
		
        if (clientBuffer->mAudioClientBuffer32.sourceBufferMap == NULL) 
		{
            audioErrorIOLog("IOAudioEngineUserClient::registerClientBuffer64() - error mapping memory.\n");
            result = kIOReturnVMError;
            goto Exit;
        }
        
        clientBuffer->mAudioClientBuffer32.sourceBuffer = (void *)clientBuffer->mAudioClientBuffer32.sourceBufferMap->getVirtualAddress();
        if (clientBuffer->mAudioClientBuffer32.sourceBuffer == NULL) 
		{
            result = kIOReturnVMError;
            goto Exit;
        }

	// rdar://problem/25156927
	//   validate the IOAudioBufferDataDescriptor
	do {
		IOAudioBufferDataDescriptor *sourceDesc = (IOAudioBufferDataDescriptor *)(clientBuffer->mAudioClientBuffer32.sourceBuffer);

		if (sourceDesc->fActualDataByteSize == 0 && sourceDesc->fActualNumSampleFrames == 0 &&
		    sourceDesc->fTotalDataByteSize == 0  && sourceDesc->fNominalDataByteSize == 0) {
			break;
		}

		UInt64 sampleSizeInBytes = (streamFormat->fIsMixable) ? kIOAudioEngineDefaultMixBufferSampleSize : (streamFormat->fBitWidth / 8);
		UInt64 frameSizeInBytes = sampleSizeInBytes * streamFormat->fNumChannels;
		UInt64 actualFramesByteSize = frameSizeInBytes * sourceDesc->fActualNumSampleFrames;
		bool overflow = (frameSizeInBytes > UINT32_MAX) || (actualFramesByteSize > UINT32_MAX);

		if ((overflow == false) &&
		    (sourceDesc->fTotalDataByteSize == bufSizeInBytes) &&
		    (sourceDesc->fActualDataByteSize <= (bufSizeInBytes - offsetof(IOAudioBufferDataDescriptor, fData))) &&
		    (sourceDesc->fNominalDataByteSize <= (bufSizeInBytes - offsetof(IOAudioBufferDataDescriptor, fData))) &&
		    (sourceDesc->fActualDataByteSize >= actualFramesByteSize)) {
			break;
		}

		audioDebugIOLog(3, "  bad argument\n");
		result = kIOReturnBadArgument;
		goto Exit;

	} while (0);

		// offset past per buffer info
        audioDebugIOLog(3, "  clientBuffer->mAudioClientBuffer32.sourceBuffer before offset: %p, offset size: %ld\n", clientBuffer->mAudioClientBuffer32.sourceBuffer, offsetof(IOAudioBufferDataDescriptor, fData));
		clientBuffer->mAudioClientBuffer32.bufferDataDescriptor = (IOAudioBufferDataDescriptor *)(clientBuffer->mAudioClientBuffer32.sourceBuffer);
		clientBuffer->mAudioClientBuffer32.sourceBuffer = (UInt8 *)(clientBuffer->mAudioClientBuffer32.sourceBuffer) + offsetof(IOAudioBufferDataDescriptor, fData);
        audioDebugIOLog(3, "  clientBuffer->mAudioClientBuffer32.sourceBuffer after offset: %p\n", clientBuffer->mAudioClientBuffer32.sourceBuffer);

		numSampleFrames = bufSizeInBytes;
		if (streamFormat->fIsMixable) {
			// If it's mixable the data is floats, so that's the size of each sample
			clientBuffer->mAudioClientBuffer32.numSampleFrames = ( bufSizeInBytes - offsetof(IOAudioBufferDataDescriptor, fData) ) / (kIOAudioEngineDefaultMixBufferSampleSize * streamFormat->fNumChannels);
		} else {
			// If it's not mixable then the size is whatever the bitwidth is
			clientBuffer->mAudioClientBuffer32.numSampleFrames = ( bufSizeInBytes - offsetof(IOAudioBufferDataDescriptor, fData) ) / ((streamFormat->fBitWidth / 8) * streamFormat->fNumChannels);
		}
        clientBuffer->mAudioClientBuffer32.numChannels = streamFormat->fNumChannels;
        clientBuffer->mUnmappedSourceBuffer64 = sourceBuffer;
		clientBuffer->mAudioClientBuffer32.unmappedSourceBuffer = (void *)sourceBuffer;
		clientBuffer->mNextBuffer64 = NULL;
        clientBuffer->mAudioClientBuffer32.mNextBuffer32 = NULL;
        clientBuffer->mAudioClientBuffer32.nextClip = NULL;
        clientBuffer->mAudioClientBuffer32.previousClip = NULL;
        clientBuffer->mAudioClientBuffer32.nextClient = NULL;
        
        lockBuffers();
        
        clientBufferSet = findBufferSet(bufferSetID);
        if (clientBufferSet == NULL) {
			audioDebugIOLog(3, "  creating new IOAudioClientBufferSet \n" );
			clientBufferSet = new IOAudioClientBufferSet;

            if (clientBufferSet == NULL) {
                result = kIOReturnNoMemory;
                unlockBuffers();
                goto Exit;
            }
            
            if (!clientBufferSet->init(bufferSetID, this)) {
                result = kIOReturnError;
                unlockBuffers();
                goto Exit;
            }

            clientBufferSet->mNextBufferSet = clientBufferSetList;

            clientBufferSetList = clientBufferSet;
        }
        
        if (audioStream->getDirection() == kIOAudioStreamDirectionOutput) {
			audioDebugIOLog(3, "  output \n" );
            clientBufferList = &clientBufferSet->outputBufferList;
            if (clientBufferSet->watchdogThreadCall == NULL) {
                clientBufferSet->allocateWatchdogTimer();
                if (clientBufferSet->watchdogThreadCall == NULL) {
                    result = kIOReturnNoMemory;
                    unlockBuffers();
                    goto Exit;
                }
            }
        } else {
 			audioDebugIOLog(3, "  input \n" );
            clientBufferList = &clientBufferSet->inputBufferList;
        }
        
        assert(clientBufferList);
        
        if (*clientBufferList == NULL) {
            *clientBufferList = clientBuffer;
        } else {
            IOAudioClientBuffer64 *clientBufPtr = *clientBufferList;
            while (clientBufPtr->mNextBuffer64 != NULL) {
                clientBufPtr = clientBufPtr->mNextBuffer64;
            }
			audioDebugIOLog(3, "  assigning  clientBufPtr->mAudioClientBuffer32.mNextBuffer32 %p \n", &clientBuffer->mAudioClientBuffer32 );
            clientBufPtr->mNextBuffer64 = clientBuffer;			
			clientBufPtr->mAudioClientBuffer32.mNextBuffer32 = &clientBuffer->mAudioClientBuffer32;
        }
        
        //  <rdar://11731381>   Add the client while holding the buffers lock to avoid race conditions.
        if (isOnline()) {
			audioDebugIOLog(3, "  isOnline adding client \n" );
			
            result = audioStream->addClient( &clientBuffer->mAudioClientBuffer32 );
            
            // Clean up the client buffer list in the event of failure.
            if (kIOReturnSuccess != result) {
                if (*clientBufferList == clientBuffer) {
                    *clientBufferList = NULL;
                } else {
                    IOAudioClientBuffer64 *clientBufPtr = *clientBufferList;
                    while ((NULL != clientBufPtr) && (clientBufPtr->mNextBuffer64 != clientBuffer)) {
                        clientBufPtr = clientBufPtr->mNextBuffer64;
                    }
                    clientBufPtr->mNextBuffer64 = NULL;
                    clientBufPtr->mAudioClientBuffer32.mNextBuffer32 = NULL;
                }
            }
        }
		else {
			audioDebugIOLog(3, "  !isOnline \n" );
		}

        unlockBuffers();
        
    Exit:
        
        if (result != kIOReturnSuccess) {
 			audioDebugIOLog(3, "  result (0x%x) != kIOReturnSuccess \n", result );
           if (clientBuffer != NULL) {
                if (clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor != NULL) {
                    clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor->release();
					clientBuffer->mAudioClientBuffer32.sourceBufferDescriptor = NULL;
                }
                if (clientBuffer->mAudioClientBuffer32.sourceBufferMap != NULL) {
                    clientBuffer->mAudioClientBuffer32.sourceBufferMap->release();
					clientBuffer->mAudioClientBuffer32.sourceBufferMap = NULL;
                }
                if (clientBuffer->mAudioClientBuffer32.audioStream) {
                    clientBuffer->mAudioClientBuffer32.audioStream->release();
					clientBuffer->mAudioClientBuffer32.audioStream = NULL;
                }
                IOFreeType(clientBuffer, IOAudioClientBuffer64);
				clientBuffer = NULL;
            }
        }
		
    } else {
		audioDebugIOLog(3, "  !isActive - no Device \n" );
        result = kIOReturnNoDevice;
    }
	audioDebugIOLog(3, "- IOAudioEngineUserClient::registerClientBuffer64() result 0x%lX\n", (long unsigned int)result);
   
    return result;
}
// 32 bit version <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::unregisterClientBuffer( void * sourceBuffer, UInt32 bufferSetID)
{
	IOReturn result = kIOReturnUnsupported;
	audioDebugIOLog(3, "+-IOAudioEngineUserClient[%p]::unregisterClientBuffer NOT SUPPORTED for 32 bit buffer( %p, 0x%lx)\n", this, sourceBuffer, (long unsigned int)bufferSetID);
	return result;
}
// 64 bit version <rdar://problems/5321701>
IOReturn IOAudioEngineUserClient::unregisterClientBuffer64( mach_vm_address_t * sourceBuffer, UInt32 bufferSetID)
{
    IOReturn result = kIOReturnBadArgument;
    
    audioDebugIOLog(3, "+ IOAudioEngineUserClient[%p]::unregisterClientBuffer64(0x%p, 0x%lx)\n", this, sourceBuffer, (long unsigned int)bufferSetID);

    if (sourceBuffer) {
        IOAudioClientBufferSet *bufferSet;
        
        lockBuffers();
        
        bufferSet = findBufferSet(bufferSetID);
        
        if (bufferSet) {
            IOAudioClientBuffer64 *clientBuf = NULL, *previousBuf = NULL;
            IOAudioClientBuffer64 **clientBufferList = NULL;
            
            if (bufferSet->outputBufferList) 
			{
                clientBufferList = &bufferSet->outputBufferList;
				audioDebugIOLog(3, "  searching for sourceBuffer 0x%llx \n", *sourceBuffer);
                clientBuf = bufferSet->outputBufferList;
                previousBuf = NULL;
				
                while (clientBuf && (clientBuf->mUnmappedSourceBuffer64 != *sourceBuffer)) 
				{
					audioDebugIOLog(3, "  checking against 0x%llx \n", clientBuf->mUnmappedSourceBuffer64);
                   previousBuf = clientBuf;
                    clientBuf = clientBuf->mNextBuffer64;
                }
            }
			else
			{
				audioDebugIOLog(3, "  clientBuf for output not found \n");
			}
            
            // If we didn't find the buffer in the output list, check the input list
            if (!clientBuf && bufferSet->inputBufferList) {
				audioDebugIOLog(3, "  checking input \n");
				clientBufferList = &bufferSet->inputBufferList;
                clientBuf = bufferSet->inputBufferList;
                previousBuf = NULL;
                while (clientBuf && (clientBuf->mUnmappedSourceBuffer64 != *sourceBuffer)) {
                    previousBuf = clientBuf;
                    clientBuf = clientBuf->mNextBuffer64;
                }
            }

            if (clientBuf) {  
				
                assert(clientBuf->mUnmappedSourceBuffer64 == *sourceBuffer);
                
                if (previousBuf) {
                    previousBuf->mNextBuffer64 = clientBuf->mNextBuffer64;
                } else {
                    assert(clientBufferList);
                    *clientBufferList = clientBuf->mNextBuffer64;
                }
                
                if (bufferSet->outputBufferList == NULL) {
                    if (bufferSet->inputBufferList == NULL) {
                        removeBufferSet(bufferSet);
                    } else if (bufferSet->watchdogThreadCall != NULL) {
                        bufferSet->freeWatchdogTimer();
                    }
                }

                freeClientBuffer(clientBuf);		// Moved below above if statement
                
                result = kIOReturnSuccess;
            } else 
			{
				audioDebugIOLog(3, "  no clientbuffer found \n" );
				result = kIOReturnNotFound;
            }            
        } else 
		{
			audioDebugIOLog(3, "  no bufferSet found for id 0x%lx \n", (long unsigned int)bufferSetID);
            result = kIOReturnNotFound;
        }
        
        unlockBuffers();
    }
    else
	{
	    audioDebugIOLog(3, "  no sourcebuffer \n" );	
	}
	audioDebugIOLog(3, "- IOAudioEngineUserClient::unregisterClientBuffer64 no sourcebuffer returns 0x%lX\n", (long unsigned int)result );	
	return result;
}

IOAudioClientBufferSet *IOAudioEngineUserClient::findBufferSet(UInt32 bufferSetID)
{
    IOAudioClientBufferSet *bufferSet = NULL;
    
	audioDebugIOLog(7, "+ IOAudioEngineUserClient::findBufferSet ( bufferSetID %ld )\n", (long int)bufferSetID );		// <rdar://9725460>
	
	if (0 == clientBufferSetList)
	{
 		audioDebugIOLog(3, "  null clientBufferSetList\n");
	}	
    bufferSet = clientBufferSetList;
    while (bufferSet && (bufferSet->bufferSetID != bufferSetID)) {
        bufferSet = bufferSet->mNextBufferSet;
    }
    if ( !bufferSet || ( bufferSet->bufferSetID != bufferSetID ) )
	{
		audioDebugIOLog(3, "  did not find clientBufferSetList for ID 0x%lx \n", (long unsigned int)bufferSetID);
	}
	audioDebugIOLog(7, "- IOAudioEngineUserClient::findBufferSet ( bufferSetID %ld ) returns %p\n", (long int)bufferSetID, bufferSet );		// <rdar://9725460>
    return bufferSet;
}

void IOAudioEngineUserClient::removeBufferSet(IOAudioClientBufferSet *bufferSet)
{
    IOAudioClientBufferSet *prevSet, *nextSet;
    
    lockBuffers();
    
    nextSet = clientBufferSetList;
    prevSet = NULL;
    while (nextSet && (nextSet != bufferSet)) {
        prevSet = nextSet;
        nextSet = nextSet->mNextBufferSet;
    }
    
    if (nextSet) {
        assert(nextSet == bufferSet);
        
        nextSet->cancelWatchdogTimer();
        
        if (prevSet) {
            prevSet->mNextBufferSet = nextSet->mNextBufferSet;
        } else {
            clientBufferSetList = nextSet->mNextBufferSet;
        }
        
        nextSet->release();
    }
    
    unlockBuffers();
	
    audioDebugIOLog(3, "+- IOAudioEngineUserClient[%p]::removeBufferSet(%p)\n", this, bufferSet);
	return;
}

IOReturn IOAudioEngineUserClient::performClientIO(UInt32 firstSampleFrame, UInt32 loopCount, bool inputIO, UInt32 bufferSetID, UInt32 sampleIntervalHi, UInt32 sampleIntervalLo)
{
    IOReturn result = kIOReturnSuccess;
    
    assert(audioEngine);

    AudioTrace_Start(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientPerformClientIO, (uintptr_t)this, ((uint64_t)firstSampleFrame << 32) | loopCount, ((uint64_t)inputIO << 32) | bufferSetID, ((uint64_t)sampleIntervalHi << 32) | sampleIntervalLo);
    __Require_Action_String(audioEngine != NULL, Exit, result = kIOReturnError, "audioEngine is NULL");
    __Require_Action_String(!isInactive(), Exit, result = kIOReturnNoDevice, "audioEngine is inActive");

    lockBuffers();
    
    if (isOnline()
        && (audioEngine->getState() == kIOAudioEngineRunning)
        && audioEngine->status
        && (audioEngine->status->fCurrentLoopCount || audioEngine->status->fLastLoopTime))			//	<rdar://14608361>	Wait for first takeTimeStamp call before allowing audio
    {
        IOAudioClientBufferSet  *bufferSet;
        __Require_Action_String(firstSampleFrame < audioEngine->numSampleFramesPerBuffer, UnlockExit, result = kIOReturnBadArgument, "firstSampleFrame out of range");
        bufferSet = findBufferSet(bufferSetID);
        // rdar://107077069 - performClientIO call is called from HAL even in the case of there being no bufferset, so the expected behavior is to jump out of the call and return Success
        __Require_Quiet(bufferSet != NULL, UnlockExit);
        if (inputIO)
        {
            result = performClientInput(firstSampleFrame, bufferSet);
        } else
        {
            result = performClientOutput(firstSampleFrame, loopCount, bufferSet, sampleIntervalHi, sampleIntervalLo);
        }
    }
    else
    {
        result = kIOReturnOffline;
    }
    
UnlockExit:
    unlockBuffers();
    
 Exit:
    AudioTrace_End(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientPerformClientIO, (uintptr_t)this, 0, 0, result);
    return result;
}

// model a SwapFloat32 after CF
inline uint32_t CFSwapInt32(uint32_t arg) {
#if defined(__i386__) && defined(__GNUC__)
    __asm__("bswap %0" : "+r" (arg));
    return arg;
#elif defined(__x86_64__) && defined(__GNUC__)		// <rdar://6612182>
    __asm__("bswap %0" : "+r" (arg));
    return arg;
#elif defined(__ppc__) && defined(__GNUC__)
    uint32_t result;
    __asm__("lwbrx %0,0,%1" : "=r" (result) : "r" (&arg), "m" (arg));
    return result;
#else
    uint32_t result;
    result = ((arg & 0xFF) << 24) | ((arg & 0xFF00) << 8) | ((arg >> 8) & 0xFF00) | ((arg >> 24) & 0xFF);
    return result;
#endif
}


void FlipFloats(void *p, long fcnt)
{
	UInt32 *ip = (UInt32 *)p;
	
	while (fcnt--) {
		*ip = CFSwapInt32(*ip);
		ip++;
	}
}

static inline IOAudioBufferDataDescriptor * FlipBufferDataDescriptor(IOAudioBufferDataDescriptor *in, IOAudioBufferDataDescriptor *tmp, UInt32 doFlip)
{	
	if (in && doFlip) {
		tmp->fActualDataByteSize = CFSwapInt32(in->fActualDataByteSize);
		tmp->fActualNumSampleFrames = CFSwapInt32(in->fActualNumSampleFrames);
		tmp->fTotalDataByteSize = CFSwapInt32(in->fTotalDataByteSize);
		tmp->fNominalDataByteSize = CFSwapInt32(in->fNominalDataByteSize);
		return tmp;
	}
	return in;
}

IOReturn IOAudioEngineUserClient::performClientOutput(UInt32 firstSampleFrame, UInt32 loopCount, IOAudioClientBufferSet *bufferSet, UInt32 sampleIntervalHi, UInt32 sampleIntervalLo)
{
    IOReturn    tmpResult;
    IOReturn    result = kIOReturnSuccess;

    AudioTrace_Start(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientPerformClientOutput, (uintptr_t)this, ((uint64_t)firstSampleFrame << 32) | loopCount, (uintptr_t)bufferSet, ((uint64_t)sampleIntervalHi << 32) | sampleIntervalLo);

    __Require_Action_String(audioEngine != NULL, Exit, result = kIOReturnError, "audioEngine is NULL");

	__Require_Action_String(((loopCount >= audioEngine->status->fCurrentLoopCount)
                                && (loopCount <= audioEngine->status->fCurrentLoopCount + kLoopCountMaximumDifference)),
                                Exit, result = kIOReturnIsoTooOld, "loop count too old");

    // this value will look different in hiRes mode (32.32 Fixed point integer) vs. old mode (single 64 bit integer)
    // but the number of bits is the same
    bufferSet->sampleInterval = ((uint64_t)sampleIntervalHi << 32) | sampleIntervalLo;

    if (bufferSet->outputBufferList != NULL)
    {
        IOAudioEnginePosition			outputEndingPosition;
        IOAudioClientBuffer64			*clientBuf;
        UInt32							sampleFrames, numSampleFramesPerBuffer;
        UInt32							clientIndex;

        clientIndex = 0;
        
        clientBuf = bufferSet->outputBufferList;

        IOAudioBufferDataDescriptor     localBufferDataDescriptor;
        IOAudioBufferDataDescriptor *   localBufferDataDescriptorPtr = FlipBufferDataDescriptor(clientBuf->mAudioClientBuffer32.bufferDataDescriptor, &localBufferDataDescriptor, reserved->classicMode);

        if (localBufferDataDescriptorPtr)
        {
            sampleFrames = localBufferDataDescriptorPtr->fActualNumSampleFrames;
        }
        else
        {
            sampleFrames = bufferSet->outputBufferList->mAudioClientBuffer32.numSampleFrames;
        }

        numSampleFramesPerBuffer = audioEngine->getNumSampleFramesPerBuffer();
        
        outputEndingPosition.fLoopCount = loopCount;
        outputEndingPosition.fSampleFrame = firstSampleFrame + sampleFrames;
        
        if (outputEndingPosition.fSampleFrame >= numSampleFramesPerBuffer)
        {
            outputEndingPosition.fSampleFrame -= numSampleFramesPerBuffer;
            outputEndingPosition.fLoopCount++;
        }
        
        // We only want to do output if we haven't already gone past the new samples
        // If the samples are late, the watchdog will already have skipped them
        if (CMP_IOAUDIOENGINEPOSITION(&outputEndingPosition, &bufferSet->nextOutputPosition) >= 0)
        {
            AbsoluteTime outputTimeout;
            
            clientBuf = bufferSet->outputBufferList;
            
            while(clientBuf)
            {
                IOAudioStream *					audioStream;
                UInt32							maxNumSampleFrames;
                
                audioStream = clientBuf->mAudioClientBuffer32.audioStream;
        
                assert(audioStream);
                assert(audioStream->getDirection() == kIOAudioStreamDirectionOutput);
                assert(clientBuf->mAudioClientBuffer32.sourceBuffer != NULL);
                
                audioStream->lockStreamForIO();
                
                maxNumSampleFrames = clientBuf->mAudioClientBuffer32.numSampleFrames;
                // <rdar://6865619>, <rdar://6917678> Validate the parameters passed in IOAudioBufferDataDescriptor vs the maximum buffer size.
                if (sampleFrames > maxNumSampleFrames)
                {
                    audioStream->unlockStreamForIO();
                    result = kIOReturnBadArgument;
                    goto Exit;
                }
                // get the per buffer info
                if (NULL != localBufferDataDescriptorPtr)
                {
                    clientBuf->mAudioClientBuffer32.numSampleFrames = sampleFrames;
                    
                    if ((localBufferDataDescriptorPtr->fActualDataByteSize > (clientBuf->mAudioClientBuffer32.sourceBufferDescriptor->getLength () - offsetof(IOAudioBufferDataDescriptor, fData)))
                        || (localBufferDataDescriptorPtr->fActualDataByteSize > localBufferDataDescriptorPtr->fTotalDataByteSize)
                        || (localBufferDataDescriptorPtr->fNominalDataByteSize > localBufferDataDescriptorPtr->fTotalDataByteSize))
                    {
                        audioStream->unlockStreamForIO();
                        result = kIOReturnBadArgument;
                        goto Exit;
                    }
                }

#if __i386__ || __x86_64__	// <rdar://6612182>
                // for those who don't remember, classicMode means OS 9, so this can probably be removed
                if (reserved->classicMode && clientBuf->mAudioClientBuffer32.sourceBuffer != NULL)
                {
                    const IOAudioStreamFormat *fmt = audioStream->getFormat();
                    if (fmt->fIsMixable && fmt->fSampleFormat == kIOAudioStreamSampleFormatLinearPCM)
                    {
                        FlipFloats(clientBuf->mAudioClientBuffer32.sourceBuffer, clientBuf->mAudioClientBuffer32.numSampleFrames * clientBuf->mAudioClientBuffer32.numChannels);
                    }
                }
#endif

                tmpResult = audioStream->processOutputSamples(&(clientBuf->mAudioClientBuffer32), firstSampleFrame, loopCount, true);

                clientBuf->mAudioClientBuffer32.numSampleFrames = maxNumSampleFrames;
                
                audioStream->unlockStreamForIO();
                
                if (tmpResult != kIOReturnSuccess)
                {
                    result = tmpResult;
                }
                
                clientBuf = clientBuf->mNextBuffer64;
                
                if (clientBuf)
                {		// need to update localBufferDataDescriptor for the current client buffer
                    localBufferDataDescriptorPtr = FlipBufferDataDescriptor ( clientBuf->mAudioClientBuffer32.bufferDataDescriptor, &localBufferDataDescriptor, reserved->classicMode );
                    
                    if (localBufferDataDescriptorPtr)
                    {
                        sampleFrames = localBufferDataDescriptorPtr->fActualNumSampleFrames;
                    }
                    else
                    {
                        sampleFrames = clientBuf->mAudioClientBuffer32.numSampleFrames;
                    }
                }
                
                clientIndex++;
            }
            
            bufferSet->nextOutputPosition = outputEndingPosition;
            
            tmpResult = audioEngine->calculateSampleTimeout(&bufferSet->sampleInterval, sampleFrames, &bufferSet->nextOutputPosition, &outputTimeout);

            if (tmpResult == kIOReturnSuccess)
            {
                assert(bufferSet->watchdogThreadCall != NULL);			// We better have a thread call if we are doing output

                bufferSet->setWatchdogTimeout(&outputTimeout);
            }
            else
            {
                result = tmpResult;
            }
        }
        else
        {
            result = kIOReturnIsoTooOld;
        }
    }

Exit:
    AudioTrace_End(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientPerformClientOutput, (uintptr_t)this, 0, 0, result);
    return result;
}

IOReturn IOAudioEngineUserClient::performClientInput(UInt32 firstSampleFrame, IOAudioClientBufferSet *bufferSet)
{
    IOReturn						result = kIOReturnSuccess;
    IOAudioClientBuffer64			*clientBuf;
	UInt32							sampleFrames = 0;
    
	audioDebugIOLog ( 4, "+  IOAudioEngineUserClient[%p]::performClientInput ( firstSampleFrame %ld,  bufferSet %p)\n", this, (long int)firstSampleFrame, bufferSet );			// <rdar://problem/9725460>
    AudioTrace_Start(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientPerformClientInput, (uintptr_t)this, firstSampleFrame, (uintptr_t)bufferSet, 0);

    clientBuf = bufferSet->inputBufferList;

	IOAudioBufferDataDescriptor localBufferDataDescriptor;
	IOAudioBufferDataDescriptor * localBufferDataDescriptorPtr = 0;

	if (NULL != clientBuf) {    
		localBufferDataDescriptorPtr = FlipBufferDataDescriptor ( clientBuf->mAudioClientBuffer32.bufferDataDescriptor, &localBufferDataDescriptor, reserved->classicMode );
		if (NULL != localBufferDataDescriptorPtr) {
			audioDebugIOLog(6, "  performClientInput ------------------------------\n");
			audioDebugIOLog ( 6, "  found buffer descriptor, using actual frames = %ld\n", 
								(long int)localBufferDataDescriptorPtr->fActualNumSampleFrames);
			sampleFrames = localBufferDataDescriptorPtr->fActualNumSampleFrames;
		} else {
			audioDebugIOLog(6, "  no buffer descriptor found, using bufferSet->inputBufferList->numSampleFrames\n"); 
			sampleFrames = bufferSet->inputBufferList->mAudioClientBuffer32.numSampleFrames;
		}
	}
	
    while (clientBuf) {
        IOAudioStream *					audioStream;
		UInt32							maxNumSampleFrames;
		UInt32							numSampleFramesRead;
        IOReturn						tmpResult;
        
        audioStream = clientBuf->mAudioClientBuffer32.audioStream;
        
        assert(audioStream);
        assert(audioStream->getDirection() == kIOAudioStreamDirectionInput);
        assert(clientBuf->mAudioClientBuffer32.sourceBuffer != NULL);

        audioStream->lockStreamForIO();

		maxNumSampleFrames = clientBuf->mAudioClientBuffer32.numSampleFrames;
		// <rdar://6865619>, <rdar://6917678> Validate the parameters passed in IOAudioBufferDataDescriptor vs the maximum buffer size.
		if ( sampleFrames > maxNumSampleFrames )
		{
			audioDebugIOLog ( 6, "  **** VBR INPUT ERROR! - actual sample frames (%ld) is larger than max sample frames (%ld)\n", (long int)sampleFrames, (long int)maxNumSampleFrames);
			audioStream->unlockStreamForIO();
			result = kIOReturnBadArgument;
			goto Exit;
		}
		
		if (NULL != localBufferDataDescriptorPtr) {

			clientBuf->mAudioClientBuffer32.numSampleFrames = sampleFrames;

			audioDebugIOLog ( 6, "  clientBuffer = %p:  actual frames = %lu, actual bytes = %lu, nominal bytes = %lu, total bytes = %lu, source buffer size = %lu\n", 
									clientBuf, 
									(long unsigned int)clientBuf->mAudioClientBuffer32.numSampleFrames, 
									(long unsigned int)localBufferDataDescriptorPtr->fActualDataByteSize, 
									(long unsigned int)localBufferDataDescriptorPtr->fNominalDataByteSize, 
									(long unsigned int)localBufferDataDescriptorPtr->fTotalDataByteSize, 
									(long unsigned int)clientBuf->mAudioClientBuffer32.sourceBufferDescriptor->getLength () - offsetof ( IOAudioBufferDataDescriptor, fData ) );

	#ifdef DEBUG					
			if (clientBuf->mAudioClientBuffer32.numSampleFrames != localBufferDataDescriptorPtr->fActualDataByteSize / (audioStream->format.fNumChannels * sizeof(float))) {
				audioDebugIOLog ( 6, "  DEBUGGING - calculated sample frames (%ld) does not match actual sample frames (%ld)\n",
									localBufferDataDescriptorPtr->fActualDataByteSize / (audioStream->format.fNumChannels * sizeof(float)), 
									(long int)clientBuf->mAudioClientBuffer32.numSampleFrames);
			}
	#endif
			if ((localBufferDataDescriptorPtr->fActualDataByteSize > (clientBuf->mAudioClientBuffer32.sourceBufferDescriptor->getLength () - offsetof(IOAudioBufferDataDescriptor, fData))) ||
				(localBufferDataDescriptorPtr->fActualDataByteSize > localBufferDataDescriptorPtr->fTotalDataByteSize)) {
				audioDebugIOLog (1, "  *** VBR INPUT ERROR! clientBuffer = %p: actual frames = %ld, actual bytes = %ld, nominal bytes = %ld, total bytes = %ld, source buffer size = %ld\n", 
									clientBuf, 
									(long unsigned int)clientBuf->mAudioClientBuffer32.numSampleFrames, 
									(long unsigned int)localBufferDataDescriptorPtr->fActualDataByteSize, 
									(long unsigned int)localBufferDataDescriptorPtr->fNominalDataByteSize, 
									(long unsigned int)localBufferDataDescriptorPtr->fTotalDataByteSize, 
									(long unsigned int)clientBuf->mAudioClientBuffer32.sourceBufferDescriptor->getLength () - offsetof ( IOAudioBufferDataDescriptor, fData ) );
				audioStream->unlockStreamForIO(); 
				result = kIOReturnBadArgument;
				goto Exit;
			}	
		}

		// set the default number of frames read.  This allows drivers to override readInputSamples and still work in the VBR world
		audioStream->setDefaultNumSampleFramesRead(sampleFrames);
        
        tmpResult = audioStream->readInputSamples( &( clientBuf->mAudioClientBuffer32 ), firstSampleFrame);
        
#if __i386__ || __x86_64__	// <rdar://6612182>
		if (reserved->classicMode && clientBuf->mAudioClientBuffer32.sourceBuffer != NULL) {
			const IOAudioStreamFormat *fmt = audioStream->getFormat();
			if (fmt->fIsMixable && fmt->fSampleFormat == kIOAudioStreamSampleFormatLinearPCM)
			{
				FlipFloats(clientBuf->mAudioClientBuffer32.sourceBuffer, clientBuf->mAudioClientBuffer32.numSampleFrames * clientBuf->mAudioClientBuffer32.numChannels);
			}
		}
#endif        

		// get how many samples the driver actually read & update the rest of the structures
		numSampleFramesRead = audioStream->getNumSampleFramesRead();
		localBufferDataDescriptorPtr->fActualDataByteSize = numSampleFramesRead * audioStream->format.fNumChannels * sizeof(float);
		localBufferDataDescriptorPtr->fActualNumSampleFrames = numSampleFramesRead;
		FlipBufferDataDescriptor(localBufferDataDescriptorPtr, clientBuf->mAudioClientBuffer32.bufferDataDescriptor, reserved->classicMode); // save changes back to clientBuf

		audioDebugIOLog ( 5, "  numSampleFramesRead = %ld, fActualNumSampleFrames = %ld, fActualDataByteSize = %ld\n", 
							(long int)numSampleFramesRead, 
							(long int)localBufferDataDescriptorPtr->fActualNumSampleFrames, 
							(long int)localBufferDataDescriptorPtr->fActualDataByteSize );

		clientBuf->mAudioClientBuffer32.numSampleFrames = maxNumSampleFrames;

        audioStream->unlockStreamForIO();
        
        if (tmpResult != kIOReturnSuccess) {
			audioDebugIOLog ( 3, "  readInputSamples failed - result 0x%x\n", tmpResult );
            result = tmpResult;
        }
        
		audioDebugIOLog ( 7, "  next clientBuf \n" );
		clientBuf = clientBuf->mNextBuffer64;
		
		if (clientBuf) {  // need to update localBufferDataDescriptor for the current client buffer
			localBufferDataDescriptorPtr = FlipBufferDataDescriptor ( clientBuf->mAudioClientBuffer32.bufferDataDescriptor, &localBufferDataDescriptor, reserved->classicMode );
			
			if (NULL != localBufferDataDescriptorPtr) {
				sampleFrames = localBufferDataDescriptorPtr->fActualNumSampleFrames;
			} else {
				sampleFrames = clientBuf->mAudioClientBuffer32.numSampleFrames;
			}
		}
    }

Exit:    
	audioDebugIOLog ( 4, "-  IOAudioEngineUserClient[%p]::performClientInput ( firstSampleFrame %ld,  bufferSet %p) returns 0x%lX\n", this, (long int)firstSampleFrame, bufferSet, (long unsigned int)result );
    AudioTrace_End(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientPerformClientInput, (uintptr_t)this, 0, 0, result);
    return result;
}

void IOAudioEngineUserClient::performWatchdogOutput(IOAudioClientBufferSet *clientBufferSet, UInt32 generationCount)
{
	IOReturn tmpResult;

	audioDebugIOLog(3, "+ IOAudioEngineUserClient[%p]::performWatchdogOutput(%p, %ld) - (%lx,%lx)\n", 
						this, clientBufferSet, 
						(long int)generationCount, 
						(long unsigned int)clientBufferSet->nextOutputPosition.fLoopCount, 
						(long unsigned int)clientBufferSet->nextOutputPosition.fSampleFrame);

    AudioTrace_Start(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientPerformWatchdogOutput, (uintptr_t)this, generationCount, clientBufferSet->nextOutputPosition.fLoopCount, clientBufferSet->nextOutputPosition.fSampleFrame);
    
    lockBuffers();
    
    if (audioEngine && !isInactive() && isOnline()) {
        if (clientBufferSet->timerPending) {
            // If the generation count of the clientBufferSet is different than the
            // generation count passed in, then a new client IO was received just before
            // the timer fired, and we don't need to do the fake IO
            // We just leave the timerPending field set
            if (clientBufferSet->generationCount == generationCount) {
                IOAudioClientBuffer64 *clientBuffer;
				IOAudioBufferDataDescriptor localBufferDataDescriptor;			// <rdar://8500809>
				IOAudioBufferDataDescriptor * localBufferDataDescriptorPtr;		// <rdar://8500809>
				UInt32 sampleFrames, numSampleFramesPerBuffer;				// <rdar://8500809>
                
                clientBuffer = clientBufferSet->outputBufferList;
                
                while (clientBuffer) {
                    IOAudioStream *	audioStream;
					UInt32			maxNumSampleFrames;			// <rdar://8500809>
                    
                    audioStream = clientBuffer->mAudioClientBuffer32.audioStream;
                    
                    assert(audioStream);
                    assert(audioStream->getDirection() == kIOAudioStreamDirectionOutput);
                    
					// <rdar://8500809>	Similar to performClientOutput, look at the buffer data descriptor to find the number of
					// sample frames to process.
					localBufferDataDescriptorPtr = FlipBufferDataDescriptor ( clientBuffer->mAudioClientBuffer32.bufferDataDescriptor, &localBufferDataDescriptor, reserved->classicMode );

					if (NULL != localBufferDataDescriptorPtr) {
						audioDebugIOLog(6, "  performWatchdogOutput ------------------------------\n");
						audioDebugIOLog ( 6, "  actual frames = %ld, actual bytes = %ld, nominal bytes = %ld, total bytes = %ld\n",
										 (long int)localBufferDataDescriptorPtr->fActualNumSampleFrames, 
										 (long int)localBufferDataDescriptorPtr->fActualDataByteSize, 
										 (long int)localBufferDataDescriptorPtr->fNominalDataByteSize, 
										 (long int)localBufferDataDescriptorPtr->fTotalDataByteSize );
						sampleFrames = localBufferDataDescriptorPtr->fNominalDataByteSize / ( clientBuffer->mAudioClientBuffer32.numChannels * sizeof(float) );
					} else {
						audioDebugIOLog(6, "  no buffer descriptor found, using bufferSet->outputBufferList->numSampleFrames\n"); 
						sampleFrames = clientBuffer->mAudioClientBuffer32.numSampleFrames;
					}

                    audioStream->lockStreamForIO();
                    
					// <rdar://8500809> Make sure that the number of sample frames to process is less than the total number of sample frames
					// in the buffer.
					maxNumSampleFrames = clientBuffer->mAudioClientBuffer32.numSampleFrames;
					
					if (sampleFrames <= maxNumSampleFrames) {	// <rdar://10320402>
						// Update the client buffer's numSampleFrames field if there is data descriptor.
						if (NULL != localBufferDataDescriptorPtr) {
							clientBuffer->mAudioClientBuffer32.numSampleFrames = sampleFrames;
						}

						audioStream->processOutputSamples( &(clientBuffer->mAudioClientBuffer32), clientBufferSet->nextOutputPosition.fSampleFrame, clientBufferSet->nextOutputPosition.fLoopCount, false);
						
						// Restore the client buffer's numSampleFrames field.
						clientBuffer->mAudioClientBuffer32.numSampleFrames = maxNumSampleFrames;
					}

                    audioStream->unlockStreamForIO();
                    
                    clientBuffer = clientBuffer->mNextBuffer64;
                }

                if (clientBufferSet->outputBufferList != NULL) {
                    AbsoluteTime outputTimeout;
                    
					// <rdar://8101171> Use the nominal number of sample frames in client buffer if it is available. Don't use
					// fActualNumSampleFrames as it will vary (due to cadence) in the case of device aggregation.
					localBufferDataDescriptorPtr = FlipBufferDataDescriptor ( clientBufferSet->outputBufferList->mAudioClientBuffer32.bufferDataDescriptor, &localBufferDataDescriptor, reserved->classicMode );	// <rdar://8500809>
					
					if (NULL != localBufferDataDescriptorPtr) {
						audioDebugIOLog(6, "  performWatchdogOutput ------------------------------\n");
						audioDebugIOLog ( 6, "  actual frames = %ld, actual bytes = %ld, nominal bytes = %ld, total bytes = %ld\n",
										 (long int)localBufferDataDescriptorPtr->fActualNumSampleFrames, 
										 (long int)localBufferDataDescriptorPtr->fActualDataByteSize, 
										 (long int)localBufferDataDescriptorPtr->fNominalDataByteSize, 
										 (long int)localBufferDataDescriptorPtr->fTotalDataByteSize );
						sampleFrames = localBufferDataDescriptorPtr->fNominalDataByteSize / ( clientBufferSet->outputBufferList->mAudioClientBuffer32.numChannels * sizeof(float) );
					} else {
						audioDebugIOLog(6, "  no buffer descriptor found, using bufferSet->outputBufferList->numSampleFrames\n"); 
						sampleFrames = clientBufferSet->outputBufferList->mAudioClientBuffer32.numSampleFrames;
					}

                    numSampleFramesPerBuffer = audioEngine->getNumSampleFramesPerBuffer();
					
					audioDebugIOLog(6, "numSampleFrames = %u, numSampleFramesPerBuffer = %u\n", (unsigned int)sampleFrames, (unsigned int)numSampleFramesPerBuffer);

					clientBufferSet->nextOutputPosition.fSampleFrame += sampleFrames;
                    
                    if (clientBufferSet->nextOutputPosition.fSampleFrame >= numSampleFramesPerBuffer) {
                        clientBufferSet->nextOutputPosition.fSampleFrame -= numSampleFramesPerBuffer;
                        clientBufferSet->nextOutputPosition.fLoopCount++;
                    }
                    
                    // <rdar://10145205> Sanity check the calculated timeout value
					tmpResult = audioEngine->calculateSampleTimeout(&clientBufferSet->sampleInterval, sampleFrames, &clientBufferSet->nextOutputPosition, &outputTimeout);
                    if ( kIOReturnSuccess == tmpResult ) {
	                    clientBufferSet->setWatchdogTimeout(&outputTimeout);
					}
					else {
						audioDebugIOLog(3, "IOAudioEngineUserClient[%p]::performWatchdogOutput failed to calculateSampleTimeout (returned %#x)\n", this, tmpResult);

						clientBufferSet->timerPending = false;
					}
                } else {
                    clientBufferSet->timerPending = false;
                }
            }
        }
    } else {
        clientBufferSet->timerPending = false;
    }
    
    unlockBuffers();
    AudioTrace_End(kAudioTIOAudioEngineUserClient, kTPIOAudioEngineUserClientPerformWatchdogOutput, (uintptr_t)this, generationCount, clientBufferSet->nextOutputPosition.fLoopCount, clientBufferSet->nextOutputPosition.fSampleFrame);
	audioDebugIOLog(3, "- IOAudioEngineUserClient[%p]::performWatchdogOutput(%p, %ld) - (%lx,%lx)\n", 
						this, clientBufferSet, 
						(long int)generationCount, 
						(long unsigned int)clientBufferSet->nextOutputPosition.fLoopCount, 
						(long unsigned int)clientBufferSet->nextOutputPosition.fSampleFrame);
	return;
}

IOReturn IOAudioEngineUserClient::getConnectionID(UInt32 *connectionID)
{
	audioDebugIOLog(3, "+ IOAudioEngineUserClient[%p]::getConnectionID(%p)\n", this, connectionID);

	*connectionID = reserved->connectionID;
	audioDebugIOLog(3, "- IOAudioEngineUserClient[%p]::getConnectionID(%p) ConnectionID:0x%x\n", this, connectionID, *connectionID);
    
	return kIOReturnSuccess;
}

IOReturn IOAudioEngineUserClient::clientStart()
{
	IOReturn ret = kIOReturnError;

	// <rdar://7363756>, <rdar://7529580>
	if ( workLoop )
	{
		ret = workLoop->runAction(_startClientAction, this);	// <rdar://7529580>
	}

	return ret;
}

IOReturn IOAudioEngineUserClient::clientStop()
{
	IOReturn ret = kIOReturnError;
    
	// <rdar://7363756>, <rdar://7529580>
	if ( workLoop )
	{
		ret = workLoop->runAction(_stopClientAction, this);		// <rdar://7529580>
	}

	return ret;
}

// <rdar://7529580>
IOReturn IOAudioEngineUserClient::_startClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, target);
        if (userClient) {
            if (userClient->commandGate) {
				setCommandGateUsage(userClient, true);	// <rdar://8518215>
                result = userClient->commandGate->runAction(startClientAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(userClient, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::startClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        if (userClient) {
            result = userClient->startClient();
        }
    }
    
    return result;
}

// <rdar://7529580>
IOReturn IOAudioEngineUserClient::_stopClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, target);
        if (userClient) {
            if (userClient->commandGate) {
				setCommandGateUsage(userClient, true);	// <rdar://8518215>
                result = userClient->commandGate->runAction(stopClientAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(userClient, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::stopClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngineUserClient *userClient = OSDynamicCast(IOAudioEngineUserClient, owner);
        if (userClient) {
            result = userClient->stopClient();
        }
    }
    
    return result;
}

IOReturn IOAudioEngineUserClient::startClient()
{
    IOReturn	result = kIOReturnNoDevice;
    bool		engineStillPaused = false;
	
    audioDebugIOLog(3, "+ IOAudioEngineUserClient[%p]::startClient() - %ld\n", this, (long int)( audioEngine ? audioEngine->numActiveUserClients : 0 ) );
	
	retain();

    if (audioEngine && !isInactive()) {
		audioDebugIOLog(3, "  audioEngine && !isInactive(). State = %d \n", audioEngine->getState());
		
		// <rdar://15485249> Pause here until the engine is no longer paused
		if (audioEngine->getState() == kIOAudioEnginePaused) {
			audioDebugIOLog(3, "Will need to wait for engine to resume\n");
			result = audioEngine->waitForEngineResume ();
			
			if ( result != kIOReturnSuccess ) {
				engineStillPaused = true;
			}
		}

		// We only need to start things up if we're not already online
		if (!engineStillPaused && !isInactive()) {
			audioDebugIOLog(3, "  audioEngine->getState() != kIOAudioEnginePaused \n");

			if (!isOnline()) {
				setOnline(true);
				audioDebugIOLog(3, "  !isOnline() setting online \n");
				result = audioEngine->startClient(this);
				
				if (result == kIOReturnSuccess) {
					audioDebugIOLog(3, "  engine started \n");
				   IOAudioClientBufferSet *bufferSet;
					
					lockBuffers();
					
					// add buffers to streams
					bufferSet = clientBufferSetList;
					while (bufferSet) {
						IOAudioClientBuffer64 *clientBuffer;
						audioDebugIOLog(3, "  bufferSet %p \n", bufferSet);
						
						clientBuffer = bufferSet->outputBufferList;
						while (clientBuffer) {
							if (clientBuffer->mAudioClientBuffer32.audioStream) {
								audioDebugIOLog(3, "  output clientBuffer %p \n", clientBuffer);
							   	result = clientBuffer->mAudioClientBuffer32.audioStream->addClient( &clientBuffer->mAudioClientBuffer32 ); 
								if (result != kIOReturnSuccess) {
									audioEngine->stopClient(this);				//	<rdar://13412666> stopClient on failure
									break;
								}
							}
							clientBuffer = clientBuffer->mNextBuffer64;
						}
			
						if (result == kIOReturnSuccess) {
							clientBuffer = bufferSet->inputBufferList;
							while (clientBuffer) {
								audioDebugIOLog(3, "  input clientBuffer %p \n", clientBuffer);
								if (clientBuffer->mAudioClientBuffer32.audioStream) {
									result = clientBuffer->mAudioClientBuffer32.audioStream->addClient( &( clientBuffer->mAudioClientBuffer32 ) );
									if (result != kIOReturnSuccess) {
										audioEngine->stopClient(this);				//	<rdar://13412666> stopClient on failure
										break;
									}
								}
								clientBuffer = clientBuffer->mNextBuffer64;
							}
						}
						
						bufferSet->resetNextOutputPosition();
			
						bufferSet = bufferSet->mNextBufferSet;
					}
					
					unlockBuffers();
				}
				else
				{
					audioDebugIOLog(3, "  engine NOT started \n");
				}
			} 
			else {
				result = kIOReturnSuccess;
			}
		}
		
		if ( isInactive() )
		{
			audioDebugIOLog(3, "Device no longer exists\n");
			result = kIOReturnNoDevice;
		}
    }

	if (kIOReturnSuccess != result) {
		audioDebugIOLog(3, "  error (0x%x) - setting offline \n", result );
		setOnline(false);
	}

	release();

    audioDebugIOLog(3, "- IOAudioEngineUserClient[%p]::startClient() - %ld returns 0x%lX\n", this, (long int)( audioEngine ? audioEngine->numActiveUserClients : 0 ), (long unsigned int)result );
	return result;
}

IOReturn IOAudioEngineUserClient::stopClient()
{
    IOReturn result = kIOReturnSuccess;
    
    if (isOnline()) {
        IOAudioClientBufferSet *bufferSet;
        
        lockBuffers();
        
        bufferSet = clientBufferSetList;
        while (bufferSet) {
            IOAudioClientBuffer64 *clientBuffer;
            
            bufferSet->cancelWatchdogTimer();
            
            clientBuffer = bufferSet->outputBufferList;
            while (clientBuffer) {
                if (clientBuffer->mAudioClientBuffer32.audioStream) {
                    clientBuffer->mAudioClientBuffer32.audioStream->removeClient( &( clientBuffer->mAudioClientBuffer32 ) ); 
                }
                clientBuffer = clientBuffer->mNextBuffer64;
            }

            clientBuffer = bufferSet->inputBufferList;
            while (clientBuffer) {
                if (clientBuffer->mAudioClientBuffer32.audioStream) {
                    clientBuffer->mAudioClientBuffer32.audioStream->removeClient( &(clientBuffer->mAudioClientBuffer32 ));  
                }
                clientBuffer = clientBuffer->mNextBuffer64;
            }
            
            bufferSet = bufferSet->mNextBufferSet;
        }
        
        unlockBuffers();

        if (audioEngine) {
            result = audioEngine->stopClient(this);
        }
    
        setOnline(false);
    }
    
    audioDebugIOLog(4, "+- IOAudioEngineUserClient[%p]::stopClient() - %ld returns 0x%lX\n", this, (long int)( audioEngine ? audioEngine->numActiveUserClients : 0 ), (long unsigned int)result );
    return result;
}

// Must be done on workLoop
void IOAudioEngineUserClient::sendFormatChangeNotification(IOAudioStream *audioStream)
{
	//  <rdar://problems/6674310&6687920>
    if ( ( !isInactive () ) && audioStream && notificationMessage && ( notificationMessage->messageHeader.msgh_remote_port != MACH_PORT_NULL ) ) {
        io_object_t clientStreamRef;
        
        audioStream->retain();
        if (exportObjectToClient(clientTask, audioStream, &clientStreamRef) == kIOReturnSuccess) {
            kern_return_t kr;
            
            notificationMessage->type = kIOAudioEngineStreamFormatChangeNotification;
            notificationMessage->sender = clientStreamRef;
            
            kr = mach_msg_send_from_kernel(&notificationMessage->messageHeader, notificationMessage->messageHeader.msgh_size);
            if (kr != MACH_MSG_SUCCESS) {
                audioErrorIOLog("IOAudioEngineUserClient::sendFormatChangeNotification() failed - msg_send returned: %d\n", kr);
                // Should also release the clientStreamRef here...
            }
        } else {
            audioErrorIOLog("IOAudioEngineUserClient::sendFormatChangeNotification() - ERROR - unable to export stream object for notification - notification not sent\n");
        }
    } else {
		if (notificationMessage) {
			audioDebugIOLog(5, "IOAudioEngineUserClient[%p]::sendFormatChangeNotification() - ERROR - notification not sent - audioStream = %p - notificationMessage = %p - port = %p\n", this, audioStream, notificationMessage, notificationMessage->messageHeader.msgh_remote_port);
		} else {
			audioDebugIOLog(4, "IOAudioEngineUserClient[%p]::sendFormatChangeNotification() - ERROR - notification not sent - audioStream = %p - notificationMessage = %p\n", this, audioStream, notificationMessage);
		}
    }
	
    audioDebugIOLog(3, "+- IOAudioEngineUserClient[%p]::sendFormatChangeNotification(%p)\n", this, audioStream);
	return;
}

IOReturn IOAudioEngineUserClient::sendNotification(UInt32 notificationType)
{
    IOReturn result = kIOReturnSuccess;
    
    if (notificationType == kIOAudioEnginePausedNotification) {
        stopClient();
    }
        
    if (notificationMessage && (notificationMessage->messageHeader.msgh_remote_port != MACH_PORT_NULL)) {
        kern_return_t kr;
        
        notificationMessage->type = notificationType;
        notificationMessage->sender = NULL;
        
        kr = mach_msg_send_from_kernel(&notificationMessage->messageHeader, notificationMessage->messageHeader.msgh_size);
        if (kr != MACH_MSG_SUCCESS) {
            result = kIOReturnError;
        }
    }
    
    audioDebugIOLog(4, "+- IOAudioEngineUserClient[%p]::sendNotification(%ld) returns 0x%lX\n", this, (long int)notificationType, (long unsigned int)result );
    return result;
}

// <rdar://8518215>
void IOAudioEngineUserClient::setCommandGateUsage(IOAudioEngineUserClient *userClient, bool increment)
{
	if (userClient->reserved) {
		if (increment) {
			switch (userClient->reserved->commandGateStatus)
			{
				case kCommandGateStatus_Normal:
				case kCommandGateStatus_RemovalPending:
					userClient->reserved->commandGateUsage++;
					break;
				case kCommandGateStatus_Invalid:
					// Should never be here. If so, something went bad...
					break;
			}
		}
		else {
			switch (userClient->reserved->commandGateStatus)
			{
				case kCommandGateStatus_Normal:
					if (userClient->reserved->commandGateUsage > 0) {
						userClient->reserved->commandGateUsage--;
					}
					break;
				case kCommandGateStatus_RemovalPending:
					if (userClient->reserved->commandGateUsage > 0) {
						userClient->reserved->commandGateUsage--;
						
						if (userClient->reserved->commandGateUsage == 0) {
							userClient->reserved->commandGateStatus = kCommandGateStatus_Invalid;
							
							if (userClient->commandGate) {
								if (userClient->workLoop) {
									userClient->workLoop->removeEventSource(userClient->commandGate);
								}
								
								userClient->commandGate->release();
								userClient->commandGate = NULL;
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
