/*
 * Copyright (c) 1998-2023 Apple Computer, Inc. All rights reserved.
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
#include "IOAudioEngine.h"
#include "IOAudioEngineUserClient.h"
#include "IOAudioDevice.h"
#include "IOAudioStream.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"
#include "IOAudioControl.h"
// Unused macros in open source
#define AudioTrace_Start(a, b, c, d, e, f)
#define AudioTrace_End(a, b, c, d, e, f)
#define AudioTrace(a, b, c, d, e, f)

#include <AssertMacros.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>

#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSOrderedSet.h>

#include <kern/clock.h>

#define AbsoluteTime_to_scalar(x)       (*(uint64_t *)(x))

/* t1 < = > t2 */
#define CMP_ABSOLUTETIME(t1, t2)    (AbsoluteTime_to_scalar(t1) > AbsoluteTime_to_scalar(t2)? (int)+1 :   \
(AbsoluteTime_to_scalar(t1) < AbsoluteTime_to_scalar(t2)? (int)-1 : 0))

/* t1 += t2 */
#define ADD_ABSOLUTETIME(t1, t2)    (AbsoluteTime_to_scalar(t1) += AbsoluteTime_to_scalar(t2))

/* t1 -= t2 */
#define SUB_ABSOLUTETIME(t1, t2)    (AbsoluteTime_to_scalar(t1) -= AbsoluteTime_to_scalar(t2))

#define WATCHDOG_THREAD_LATENCY_PADDING_NS	(125000)	// 125us
#define DEFAULT_MIX_CLIP_OVERHEAD			10			// <rdar://12188841>

#define kDefaultEraseHeadComputationNs 100000 // 100 microseconds is conservative. Minimum quantum is 50us.
#define kDefaultEraseHeadConstraintNs  (2 * kDefaultEraseHeadComputationNs)

// <rdar://8518215>
enum
{
	kCommandGateStatus_Normal				= 0,
	kCommandGateStatus_RemovalPending,
	kCommandGateStatus_Invalid
};

#define super IOService

OSDefineMetaClassAndAbstractStructors(IOAudioEngine, IOService)

OSMetaClassDefineReservedUsed(IOAudioEngine, 0);
OSMetaClassDefineReservedUsed(IOAudioEngine, 1);
OSMetaClassDefineReservedUsed(IOAudioEngine, 2);
OSMetaClassDefineReservedUsed(IOAudioEngine, 3);
OSMetaClassDefineReservedUsed(IOAudioEngine, 4);
OSMetaClassDefineReservedUsed(IOAudioEngine, 5);
OSMetaClassDefineReservedUsed(IOAudioEngine, 6);
OSMetaClassDefineReservedUsed(IOAudioEngine, 7);
OSMetaClassDefineReservedUsed(IOAudioEngine, 8);
OSMetaClassDefineReservedUsed(IOAudioEngine, 9);
OSMetaClassDefineReservedUsed(IOAudioEngine, 10);
OSMetaClassDefineReservedUsed(IOAudioEngine, 11);
OSMetaClassDefineReservedUsed(IOAudioEngine, 12);
OSMetaClassDefineReservedUsed(IOAudioEngine, 13);
OSMetaClassDefineReservedUsed(IOAudioEngine, 14);
OSMetaClassDefineReservedUsed(IOAudioEngine, 15);
OSMetaClassDefineReservedUsed(IOAudioEngine, 16);

OSMetaClassDefineReservedUnused(IOAudioEngine, 17);
OSMetaClassDefineReservedUnused(IOAudioEngine, 18);
OSMetaClassDefineReservedUnused(IOAudioEngine, 19);
OSMetaClassDefineReservedUnused(IOAudioEngine, 20);
OSMetaClassDefineReservedUnused(IOAudioEngine, 21);
OSMetaClassDefineReservedUnused(IOAudioEngine, 22);
OSMetaClassDefineReservedUnused(IOAudioEngine, 23);
OSMetaClassDefineReservedUnused(IOAudioEngine, 24);
OSMetaClassDefineReservedUnused(IOAudioEngine, 25);
OSMetaClassDefineReservedUnused(IOAudioEngine, 26);
OSMetaClassDefineReservedUnused(IOAudioEngine, 27);
OSMetaClassDefineReservedUnused(IOAudioEngine, 28);
OSMetaClassDefineReservedUnused(IOAudioEngine, 29);
OSMetaClassDefineReservedUnused(IOAudioEngine, 30);
OSMetaClassDefineReservedUnused(IOAudioEngine, 31);
OSMetaClassDefineReservedUnused(IOAudioEngine, 32);
OSMetaClassDefineReservedUnused(IOAudioEngine, 33);
OSMetaClassDefineReservedUnused(IOAudioEngine, 34);
OSMetaClassDefineReservedUnused(IOAudioEngine, 35);
OSMetaClassDefineReservedUnused(IOAudioEngine, 36);
OSMetaClassDefineReservedUnused(IOAudioEngine, 37);
OSMetaClassDefineReservedUnused(IOAudioEngine, 38);
OSMetaClassDefineReservedUnused(IOAudioEngine, 39);
OSMetaClassDefineReservedUnused(IOAudioEngine, 40);
OSMetaClassDefineReservedUnused(IOAudioEngine, 41);
OSMetaClassDefineReservedUnused(IOAudioEngine, 42);
OSMetaClassDefineReservedUnused(IOAudioEngine, 43);
OSMetaClassDefineReservedUnused(IOAudioEngine, 44);
OSMetaClassDefineReservedUnused(IOAudioEngine, 45);
OSMetaClassDefineReservedUnused(IOAudioEngine, 46);
OSMetaClassDefineReservedUnused(IOAudioEngine, 47);

// OSMetaClassDefineReservedUsed(IOAudioEngine, 13);
IOReturn IOAudioEngine::setAttributeForConnection( SInt32 connectIndex, UInt32 attribute, uintptr_t value )
{
	return kIOReturnUnsupported;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 14);
IOReturn IOAudioEngine::getAttributeForConnection( SInt32 connectIndex, UInt32 attribute, uintptr_t * value )
{
	return kIOReturnUnsupported;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 15);
// sampleIntervalHiRes is a 32.32 Fixed point number with an integer AbsoluteTime in the high bits and
// a fractional AbsoluteTime in the low bits
IOReturn IOAudioEngine::calculateSampleTimeoutHiRes(uint64_t sampleIntervalHiRes, UInt32 numSampleFrames, IOAudioEnginePosition *startingPosition, AbsoluteTime *wakeupTimeTicks)
{
    IOReturn        result = kIOReturnBadArgument;
    AbsoluteTime    wakeupDeltaTicks = 0;
    uint64_t        wakeupDeltaNS = 0;

    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeoutHiRes, (uintptr_t)this, sampleIntervalHiRes, numSampleFrames, startingPosition ? ((((uint64_t)startingPosition->fSampleFrame) << 32) | startingPosition->fLoopCount) : 0xDEADBEEF);
    
    if (sampleIntervalHiRes && (numSampleFrames != 0) && startingPosition && wakeupTimeTicks)
    {
        IOAudioEnginePosition   wakeupPosition;
        UInt32                  wakeupOffset;
        AbsoluteTime            lastLoopTimeTicks;
        UInt32                  currentLoopCount;
        AbsoluteTime            wakeupIntervalTicks;
        AbsoluteTime            currentTimeTicks;                                    // <rdar://10145205>
        UInt32                  samplesFromLoopStart;
        AbsoluteTime            wakeupThreadLatencyPaddingIntervalTicks;
        AbsoluteTime            ticksForSampleBuffer;
        uint32_t                sampleFramesToMeasure = numSampleFramesPerBuffer;
        uint32_t                sampleFramesMeasured = 1;               // the sampleIntervalHiRes represents 1 sample
        uint64_t                measureInterval = sampleIntervalHiRes;

        // calculate the AbsoluteTime for the entire sample buffer (or as much as I can handle)
        
        while (!(sampleFramesToMeasure == 1) && !(measureInterval & 0x8000000000000000LL))
        {
            measureInterval <<= 1;                  // multiply by 2
            sampleFramesMeasured <<= 1;             // multiply by 2
            sampleFramesToMeasure >>= 1;            // divide by 2
        }

        AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeoutHiRes, (uintptr_t)this, ((uint64_t)sampleFramesMeasured << 32) | numSampleFramesPerBuffer, measureInterval, 5);
        // ticksForSampleBuffer now contains the AbsoluteTime for the entire sample buffer (or close as we can get) in the upper 32 bits
        ticksForSampleBuffer = (measureInterval & 0xFFFFFFFF00000000) >> 32;
            
        wakeupOffset = (numSampleFrames / reserved->mixClipOverhead) + sampleOffset;
        AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeoutHiRes, (uintptr_t)this, ((uint64_t)reserved->mixClipOverhead << 32) | sampleOffset, wakeupOffset, 1);

        if (wakeupOffset <= startingPosition->fSampleFrame)
        {
            wakeupPosition = *startingPosition;
            wakeupPosition.fSampleFrame -= wakeupOffset;
        } else
        {
            wakeupPosition.fLoopCount = startingPosition->fLoopCount - 1;
            wakeupPosition.fSampleFrame = numSampleFramesPerBuffer - (wakeupOffset - startingPosition->fSampleFrame);
        }
        
        getLoopCountAndTimeStamp(&currentLoopCount, &lastLoopTimeTicks);
        
        samplesFromLoopStart = ((wakeupPosition.fLoopCount - currentLoopCount) * numSampleFramesPerBuffer) + wakeupPosition.fSampleFrame;

        AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeoutHiRes, (uintptr_t)this, lastLoopTimeTicks, ((uint64_t)currentLoopCount << 32) | samplesFromLoopStart, 2);
        wakeupIntervalTicks = ((samplesFromLoopStart * ticksForSampleBuffer) + (sampleFramesMeasured / 2)) / sampleFramesMeasured;
        
        AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeoutHiRes, (uintptr_t)this, ticksForSampleBuffer, wakeupIntervalTicks, 3);

        nanoseconds_to_absolutetime(WATCHDOG_THREAD_LATENCY_PADDING_NS, &wakeupThreadLatencyPaddingIntervalTicks);
        
        SUB_ABSOLUTETIME(&wakeupIntervalTicks, &wakeupThreadLatencyPaddingIntervalTicks);
        
        *wakeupTimeTicks = lastLoopTimeTicks;
        ADD_ABSOLUTETIME(wakeupTimeTicks, &wakeupIntervalTicks);

        // <rdar://10145205> Sanity check the calculated time
        clock_get_uptime(&currentTimeTicks);

        if ( CMP_ABSOLUTETIME(wakeupTimeTicks, &currentTimeTicks) > 0 )
        {
            wakeupDeltaTicks = *wakeupTimeTicks;
            SUB_ABSOLUTETIME(&wakeupDeltaTicks, &currentTimeTicks);
            absolutetime_to_nanoseconds(wakeupDeltaTicks, &wakeupDeltaNS);
            result = kIOReturnSuccess;
        }
        else
        {
            AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeoutHiRes, (uintptr_t)this, *wakeupTimeTicks, currentTimeTicks, 4);
            result = kIOReturnIsoTooOld;
        }
    }

    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeoutHiRes, (uintptr_t)this, wakeupTimeTicks ? *wakeupTimeTicks : 0xDEADBEEF, wakeupDeltaNS, result );
    return result;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 16);
// This method turns on the use of Hi Resolution sample Intervals in the HAL's I/O trap.
// this is a one-way call. once done, it cannot be outdone
IOReturn IOAudioEngine::useHiResSampleInterval(void)
{
    reserved->useHiResSampleInterval = true;
    setProperty(kIOAudioEngineUseHiResSampleIntervalKey, kOSBooleanTrue);
    return kIOReturnSuccess;
}


// New Code:
// OSMetaClassDefineReservedUsed(IOAudioEngine, 12);
IOReturn IOAudioEngine::createUserClient(task_t task, void *securityID, UInt32 type, IOAudioEngineUserClient **newUserClient, OSDictionary *properties)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioEngineUserClient *userClient;
    
    userClient = IOAudioEngineUserClient::withAudioEngine(this, task, securityID, type, properties);
    
    if (userClient) {
        *newUserClient = userClient;
    } else {
        result = kIOReturnNoMemory;
    }
    
    return result;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 11);
void IOAudioEngine::setInputSampleOffset(UInt32 numSamples) {
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::setInputSampleOffset(0x%lx)\n", this, (long unsigned int)numSamples);
	assert(reserved);
	reserved->inputSampleOffset = numSamples;
    setProperty(kIOAudioEngineInputSampleOffsetKey, numSamples, sizeof(UInt32)*8);
    audioDebugIOLog(3, "- IOAudioEngine[%p]::setInputSampleOffset(0x%lx)\n", this, (long unsigned int)numSamples);
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 10);
void IOAudioEngine::setOutputSampleOffset(UInt32 numSamples) {
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::setOutputSampleOffset(0x%lx)\n", this, (long unsigned int)numSamples);
	setSampleOffset(numSamples);
    audioDebugIOLog(3, "- IOAudioEngine[%p]::setOutputSampleOffset(0x%lx)\n", this, (long unsigned int)numSamples);
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 9);
IOReturn IOAudioEngine::convertInputSamplesVBR(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 &numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	IOReturn result;

    result = convertInputSamples(sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
		
	return result;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 8);
void IOAudioEngine::setClockDomain(UInt32 inClockDomain) {

	UInt32		clockDomain;

	if (kIOAudioNewClockDomain == inClockDomain) {
		clockDomain = (UInt32) ((UInt64)this >> 2) ; // grab a couple of bits from the high address to help randomness

	} else {
		clockDomain = inClockDomain;
	}

	setProperty(kIOAudioEngineClockDomainKey, clockDomain, sizeof(UInt32)*8);
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 7);
void IOAudioEngine::setClockIsStable(bool clockIsStable) {
	setProperty(kIOAudioEngineClockIsStableKey, clockIsStable);
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 6);
IOAudioStream * IOAudioEngine::getStreamForID(UInt32 streamID) {
	IOAudioStream *			stream = NULL;

	assert(reserved);
	if (reserved->streams) {
		stream = OSDynamicCast (IOAudioStream, reserved->streams->getObject(streamID));
	}

	return stream;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 5);
UInt32 IOAudioEngine::getNextStreamID(IOAudioStream * newStream) {
	bool			inserted;

	assert(reserved);
	if (!reserved->streams) {
		reserved->streams = OSArray::withCapacity(1);
	}

	inserted = reserved->streams->setObject(newStream);

	return reserved->streams->getCount() - 1;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 4);
void IOAudioEngine::lockStreamForIO(IOAudioStream *stream) {
	stream->lockStreamForIO();
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 3);
void IOAudioEngine::unlockStreamForIO(IOAudioStream *stream) {
	stream->unlockStreamForIO();
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 2);
IOReturn IOAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioStreamFormatExtension *formatExtension, const IOAudioSampleRate *newSampleRate)
{
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::performFormatChange(%p, %p, %p, %p)\n", this, audioStream, newFormat, formatExtension, newSampleRate);

    return kIOReturnUnsupported;
}

// OSMetaClassDefineReservedUsed(IOAudioEngine, 1);
IOBufferMemoryDescriptor *IOAudioEngine::getStatusDescriptor()
{
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::getStatusDescriptor()\n", this);
	assert(reserved);

	return reserved->statusDescriptor;
}

IOReturn IOAudioEngine::getNearestStartTime(IOAudioStream *audioStream, IOAudioTimeStamp *ioTimeStamp, bool isInput)
{
	return kIOReturnSuccess;
}

IOBufferMemoryDescriptor * IOAudioEngine::getBytesInInputBufferArrayDescriptor()
{
	assert(reserved);

	return reserved->bytesInInputBufferArrayDescriptor;
}

IOBufferMemoryDescriptor * IOAudioEngine::getBytesInOutputBufferArrayDescriptor()
{
	assert(reserved);

	return reserved->bytesInOutputBufferArrayDescriptor;
}

IOReturn IOAudioEngine::eraseOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	if (mixBuf) {
		int csize = streamFormat->fNumChannels * kIOAudioEngineDefaultMixBufferSampleSize;
		bzero((UInt8*)mixBuf + firstSampleFrame * csize, numSampleFrames * csize);
	}
	if (sampleBuf) {
		int csize = streamFormat->fNumChannels * streamFormat->fBitWidth / 8;
		bzero((UInt8*)sampleBuf + (firstSampleFrame * csize), numSampleFrames * csize);
	}
	return kIOReturnSuccess;
}

void IOAudioEngine::setMixClipOverhead(UInt32 newMixClipOverhead)
{
	if (newMixClipOverhead > 1 && newMixClipOverhead < 99) {
		reserved->mixClipOverhead = newMixClipOverhead;
	}
}

// Original code from here forward:
SInt32 compareAudioStreams(IOAudioStream *stream1, IOAudioStream *stream2, void *ref)
{
    UInt32 startingChannelID1, startingChannelID2;
    
    startingChannelID1 = stream1->getStartingChannelID();
    startingChannelID2 = stream2->getStartingChannelID();
    
    return (startingChannelID1 > startingChannelID2) ? -1 : ((startingChannelID2 > startingChannelID1) ? 1 : 0);	// <rdar://9629411>
}

const OSSymbol *IOAudioEngine::gSampleRateWholeNumberKey = NULL;
const OSSymbol *IOAudioEngine::gSampleRateFractionKey = NULL;

void IOAudioEngine::initKeys()
{
    if (!gSampleRateWholeNumberKey) {
        gSampleRateWholeNumberKey = OSSymbol::withCString(kIOAudioSampleRateWholeNumberKey);
        gSampleRateFractionKey = OSSymbol::withCString(kIOAudioSampleRateFractionKey);
    }
}

OSDictionary *IOAudioEngine::createDictionaryFromSampleRate(const IOAudioSampleRate *sampleRate, OSDictionary *rateDict)
{
    OSDictionary *newDict = NULL;
    
    if (sampleRate) {
        if (rateDict) {
            newDict = rateDict;
        } else {
            newDict = OSDictionary::withCapacity(2);
        }
        
        if (newDict) {
            OSNumber *num;
            
            if (!gSampleRateWholeNumberKey) {
                initKeys();
            }
            
            num = OSNumber::withNumber(sampleRate->whole, sizeof(UInt32)*8);
            newDict->setObject(gSampleRateWholeNumberKey, num);
            num->release();
            
            num = OSNumber::withNumber(sampleRate->fraction, sizeof(UInt32)*8);
            newDict->setObject(gSampleRateFractionKey, num);
            num->release();
        }
    }
    
    return newDict;
}

IOAudioSampleRate *IOAudioEngine::createSampleRateFromDictionary(const OSDictionary *rateDict, IOAudioSampleRate *sampleRate)
{
    IOAudioSampleRate *rate = NULL;
    static IOAudioSampleRate staticSampleRate;
    
    if (rateDict) {
        if (sampleRate) {
            rate = sampleRate;
        } else {
            rate = &staticSampleRate;
        }
        
        if (rate) {
            OSNumber *num;
            
            if (!gSampleRateWholeNumberKey) {
                initKeys();
            }
            
            bzero(rate, sizeof(IOAudioSampleRate));
            
            num = OSDynamicCast(OSNumber, rateDict->getObject(gSampleRateWholeNumberKey));
            if (num) {
                rate->whole = num->unsigned32BitValue();
            }
            
            num = OSDynamicCast(OSNumber, rateDict->getObject(gSampleRateFractionKey));
            if (num) {
                rate->fraction = num->unsigned32BitValue();
            }
        }
    }
    
    return rate;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

bool IOAudioEngine::init(OSDictionary *properties)
{
	bool			result = false;
	
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::init(%p)\n", this, properties);

	OSDictionary * pDict = NULL;	
	
	if ( properties ) // properties is normally NULL
	{
		pDict = (OSDictionary*)properties->copyCollection();
			
		audioDebugIOLog(3, "  Make copy of properties(%p) != pDict(%p)\n", properties, pDict);
	}
	else
	{
		audioDebugIOLog(3, "  properties(%p) == NULL\n", properties);
	}

	require(super::init(pDict), finish);

	duringStartup = true;

	sampleRate.whole = 0;
	sampleRate.fraction = 0;

	numErasesPerBuffer = IOAUDIOENGINE_DEFAULT_NUM_ERASES_PER_BUFFER;
	isRegistered = false;

	numActiveUserClients = 0;

	reserved = IOMallocType (ExpansionData);
	require(reserved != nullptr, finish);

	reserved->pauseCount = 0;
	reserved->bytesInInputBufferArrayDescriptor = NULL;
	reserved->bytesInOutputBufferArrayDescriptor = NULL;
	reserved->mixClipOverhead = DEFAULT_MIX_CLIP_OVERHEAD;		// <rdar://12188841>
	reserved->streams = NULL;
	reserved->commandGateStatus = kCommandGateStatus_Normal;	// <rdar://8518215>
	reserved->commandGateUsage = 0;                                // <rdar://8518215>
	reserved->useHiResSampleInterval = false;
	reserved->eraseHeadComputationNs = kDefaultEraseHeadComputationNs;
	reserved->eraseHeadConstraintNs  = kDefaultEraseHeadConstraintNs;

	reserved->statusDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryKernelUserShared, round_page_32(sizeof(IOAudioEngineStatus)), page_size);

	reserved->eraseHeadWorkloop = IOWorkLoop::workLoop();
	require(reserved->eraseHeadWorkloop != nullptr, finish);

	reserved->eraseHeadTimerEvent =
		IOTimerEventSource::timerEventSource(kIOTimerEventSourceOptionsPriorityWorkLoop, this,
											 reinterpret_cast<IOTimerEventSource::Action>(&IOAudioEngine::timerCallback));
	require(reserved->eraseHeadTimerEvent, finish);

	require(reserved->statusDescriptor != nullptr, finish);

	status = (IOAudioEngineStatus *)reserved->statusDescriptor->getBytesNoCopy();
	require(status != nullptr, finish);

	outputStreams = OSOrderedSet::withCapacity(1, (OSOrderedSet::OSOrderFunction)compareAudioStreams);
	require(outputStreams != nullptr, finish);

	inputStreams = OSOrderedSet::withCapacity(1, (OSOrderedSet::OSOrderFunction)compareAudioStreams);
	require(inputStreams != nullptr, finish);

	setClockDomain ();

	maxNumOutputChannels = 0;
	maxNumInputChannels = 0;

	setSampleOffset (0);

	userClients = OSSet::withCapacity (1);
	require(userClients != nullptr, finish);

	bzero(status, round_page_32(sizeof(IOAudioEngineStatus)));
	status->fVersion = kIOAudioEngineCurrentStatusStructVersion;

	setState(kIOAudioEngineStopped);

	setProperty(kIOAudioEngineFlavorKey, (UInt32)kIOAudioStreamByteOrderLittleEndian, sizeof(UInt32)*8);
	result = true;

#if TARGET_OS_OSX && TARGET_CPU_ARM64
	if (driverDesiresHiResSampleIntervals())
	{
		useHiResSampleInterval();
	}
#endif

finish:
    audioDebugIOLog(3, "- IOAudioEngine[%p]::init(%p)\n", this, properties);
    return result;
}

// <rdar://12188841>
void IOAudioEngine::free()
{
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::free()\n", this);

	if (reserved) {
		if (reserved->statusDescriptor) {
			reserved->statusDescriptor->release();
			reserved->statusDescriptor = NULL;
			status = NULL;
		}

		if (reserved->bytesInInputBufferArrayDescriptor) {
			reserved->bytesInInputBufferArrayDescriptor->release();
			reserved->bytesInInputBufferArrayDescriptor = NULL;
		}

		if (reserved->bytesInOutputBufferArrayDescriptor) {
			reserved->bytesInOutputBufferArrayDescriptor->release();
			reserved->bytesInOutputBufferArrayDescriptor = NULL;
		}

		if (reserved->streams) {
			reserved->streams->release();
			reserved->streams = NULL;
		}

		if (reserved->eraseHeadTimerEvent) {
			reserved->eraseHeadTimerEvent->cancelTimeout();
			if (reserved->eraseHeadWorkloop) {
				reserved->eraseHeadWorkloop->removeEventSource(reserved->eraseHeadTimerEvent);
			}
			OSSafeReleaseNULL(reserved->eraseHeadTimerEvent);
		}

		OSSafeReleaseNULL(reserved->eraseHeadWorkloop);
		
		IOFreeType (reserved, ExpansionData);
	}

    if (outputStreams) {
        outputStreams->release();
        outputStreams = NULL;
    }

    if (inputStreams) {
        inputStreams->release();
        inputStreams = NULL;
    }

    if (userClients) {
        userClients->release();
        userClients = NULL;
    }
    
    if (defaultAudioControls) {
        removeAllDefaultAudioControls();
        defaultAudioControls->release();
        defaultAudioControls = NULL;
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

    super::free();
	
    audioDebugIOLog(3, "- IOAudioEngine[%p]::free()\n", this);
	return;
}

bool IOAudioEngine::initHardware(IOService *provider)
{
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::initHardware(%p)\n", this, provider);

    return true;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

bool IOAudioEngine::start(IOService *provider)
{
	bool			result = false;
	
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::start(%p)\n", this, provider);

    result = start(provider, OSDynamicCast(IOAudioDevice, provider));
	
    audioDebugIOLog(3, "- IOAudioEngine[%p]::start(%p) returns %d\n", this, provider, result);
	return result;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

bool IOAudioEngine::start(IOService *provider, IOAudioDevice *device)
{
    bool result = false;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::start(%p, %p)\n", this, provider, device);
    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEngineStart, (uintptr_t)this, (uintptr_t)provider, (uintptr_t)device, 0);
	
	if ( super::start ( provider ) )
	{
		if ( 0 != device )
		{
			setAudioDevice ( device );
			
			workLoop = audioDevice->getWorkLoop ();
			if ( workLoop )
			{
				workLoop->retain();
	
				commandGate = IOCommandGate::commandGate ( this );
				if ( commandGate )
				{
					workLoop->addEventSource ( commandGate );
					
					// for 2761764 & 3111501
					setWorkLoopOnAllAudioControls ( workLoop );
					
					result = initHardware ( provider );
					
					duringStartup = false;
				}
			}
		}
	}
        
    audioDebugIOLog(3, "- IOAudioEngine[%p]::start(%p, %p)\n", this, provider, device);
    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEngineStart, (uintptr_t)this, (uintptr_t)provider, (uintptr_t)device, result);
    return result;
}

void IOAudioEngine::stop(IOService *provider)
{
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::stop(%p)\n", this, provider);

    if (commandGate)
	{
        commandGate->runAction ( detachUserClientsAction );
    }
    
    audioDebugIOLog(4, "  about to stopAudioEngine ()\n" );
    stopAudioEngine ();
    audioDebugIOLog(4, "  about to detachAudioStreams ()\n" );

	removeTimer();													//	<rdar://14198236>
    detachAudioStreams ();
    audioDebugIOLog(4, "  about to removeAllDefaultAudioControls ()\n" );
    removeAllDefaultAudioControls ();
    audioDebugIOLog(4, "  completed removeAllDefaultAudioControls ()\n" );

	// <rdar://7233118>, <rdar://7029696> Remove the event source here as performing heavy workloop operation in free() could lead
	// to deadlock since the context which free() is called is not known. stop() is called on the workloop, so it is safe to remove 
	// the event source here.
	if (reserved->commandGateUsage == 0) {							// <rdar://8518215>
		reserved->commandGateStatus = kCommandGateStatus_Invalid;	// <rdar://8518215>
		
		if (commandGate)
		{
			if (workLoop)
			{
				workLoop->removeEventSource ( commandGate );
				audioDebugIOLog(3, "  completed removeEventSource ( ... )\n" );
			}
			
			commandGate->release();
			commandGate = NULL;
			audioDebugIOLog(3, "  completed release ()\n" );
		}
	}
	else {	// <rdar://8518215>
		reserved->commandGateStatus = kCommandGateStatus_RemovalPending;
	}

    audioDebugIOLog(4, "  about to super::stop ( ... )\n" );
    super::stop ( provider );
    audioDebugIOLog(3, "- IOAudioEngine[%p]::stop(%p)\n", this, provider);
}

IOWorkLoop *IOAudioEngine::getWorkLoop() const
{
	audioDebugIOLog(7, "+-IOAudioEngine[%p]::getWorkLoop()\n", this);

    return workLoop;
}

IOCommandGate *IOAudioEngine::getCommandGate() const
{
    audioDebugIOLog(7, "+-IOAudioEngine[%p]::getCommandGate()\n", this);

    return commandGate;
}

void IOAudioEngine::registerService(IOOptionBits options)
{
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::registerService(0x%lx)\n", this, (long unsigned int)options);

    if (!isRegistered) {
        OSCollectionIterator *iterator;
        IOAudioStream *stream;
        
        updateChannelNumbers();
        
        super::registerService(options);

        if (outputStreams && (outputStreams->getCount() > 0)) {
            iterator = OSCollectionIterator::withCollection(outputStreams);
            if (iterator) {
                while ( (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) ) {
                    stream->registerService();
                }
                iterator->release();
            }
        }

        if (inputStreams && (inputStreams->getCount() > 0)) {
            iterator = OSCollectionIterator::withCollection(inputStreams);
            if (iterator) {
                while ( (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) ) {
                    stream->registerService();
                }
                iterator->release();
            }
        }

        isRegistered = true;
    }
    
    audioDebugIOLog(3, "- IOAudioEngine[%p]::registerService(0x%lx)\n", this, (long unsigned int)options);
	return;
}

OSString *IOAudioEngine::getGlobalUniqueID()
{
    const OSMetaClass * const myMetaClass = getMetaClass();
    const char *className = NULL;
    const char *location = NULL;
    char *uniqueIDStr;
    OSString *localID = NULL;
    OSString *uniqueID = NULL;
    UInt32 uniqueIDSize;

    if (myMetaClass) {
        className = myMetaClass->getClassName();
    }
    
    location = getLocation();
    
    localID = getLocalUniqueID();
    
    uniqueIDSize = 3;
    
    if (className) {
        uniqueIDSize += strlen(className);
    }
    
    if (location) {
        uniqueIDSize += strlen(location);
    }
    
    if (localID) {
        uniqueIDSize += localID->getLength();
    }
        
    uniqueIDStr = (char *)IOMallocZeroData(uniqueIDSize);
    
    if (uniqueIDStr) {

        if (className) {
            snprintf(uniqueIDStr, uniqueIDSize, "%s:", className);
        }
        
        if (location) {
            strncat(uniqueIDStr, location, uniqueIDSize);
            strncat(uniqueIDStr, ":", uniqueIDSize);
        }
        
        if (localID) {
            strncat(uniqueIDStr, localID->getCStringNoCopy(), uniqueIDSize);
            localID->release();
        }
        
        uniqueID = OSString::withCString(uniqueIDStr);
        
        IOFreeData(uniqueIDStr, uniqueIDSize);
    }

    return uniqueID;
}

OSString *IOAudioEngine::getLocalUniqueID()
{
    OSString *localUniqueID;
	int strSize = (sizeof(UInt32)*2)+1;
    char localUniqueIDStr[strSize];
   
    snprintf(localUniqueIDStr, strSize, "%lx", (long unsigned int)index);
    
    localUniqueID = OSString::withCString(localUniqueIDStr);
    
    return localUniqueID;
}

void IOAudioEngine::setIndex(UInt32 newIndex)
{
    OSString *uniqueID;
    
    index = newIndex;
    
    uniqueID = getGlobalUniqueID();
    if (uniqueID) {
        setProperty(kIOAudioEngineGlobalUniqueIDKey, uniqueID);
        uniqueID->release();
    }
}

void IOAudioEngine::setAudioDevice(IOAudioDevice *device)
{
    audioDevice = device;
}

void IOAudioEngine::setDescription(const char *description)
{
    if (description) {
        setProperty(kIOAudioEngineDescriptionKey, description);
    }
}

void IOAudioEngine::resetStatusBuffer()
{
    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEngineResetStatusBuffer, (uintptr_t)this, 0, 0, 0);
	
    assert(status);
    
    status->fCurrentLoopCount = 0;
	status->fLastLoopTime = 0;
    status->fEraseHeadSampleFrame = 0;

    stopEngineAtPosition(NULL);

    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEngineResetStatusBuffer, (uintptr_t)this, 0, 0, 0);
    audioDebugIOLog(3, "+- IOAudioEngine[%p]::resetStatusBuffer()\n", this);
    return;
}

void IOAudioEngine::clearAllSampleBuffers()
{
    OSCollectionIterator *iterator;
    IOAudioStream *stream;
    
    if (outputStreams && (outputStreams->getCount() > 0)) {
        iterator = OSCollectionIterator::withCollection(outputStreams);
        if (iterator) {
            while ( (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) ) {
                stream->clearSampleBuffer();
            }
            iterator->release();
        }
    }
    
    if (inputStreams && (inputStreams->getCount() > 0)) {
        iterator = OSCollectionIterator::withCollection(inputStreams);
        if (iterator) {
            while ( (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) ) {
                stream->clearSampleBuffer();
            }
            iterator->release();
        }
    }
}

IOReturn IOAudioEngine::createUserClient(task_t task, void *securityID, UInt32 type, IOAudioEngineUserClient **newUserClient)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioEngineUserClient *userClient;
    
    userClient = IOAudioEngineUserClient::withAudioEngine(this, task, securityID, type);
    
    if (userClient) {
        *newUserClient = userClient;
    } else {
        result = kIOReturnNoMemory;
    }
    
    return result;
}

IOReturn IOAudioEngine::newUserClient(task_t task, void *securityID, UInt32 type, IOUserClient **handler)
{
    return kIOReturnUnsupported;
}

IOReturn IOAudioEngine::newUserClient(task_t task, void *securityID, UInt32 type, OSDictionary *properties, IOUserClient **handler)
{
    IOReturn				result = kIOReturnSuccess;
    IOAudioEngineUserClient	*client;
	
	if (kIOReturnSuccess == newUserClient(task, securityID, type, handler)) {
		return kIOReturnSuccess;
	}
	
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::newUserClient(0x%p, %p, 0x%lx, %p, %p)\n", this, task, securityID, (long unsigned int)type, properties, handler);
	
    if (!isInactive()) {
        result = createUserClient(task, securityID, type, &client, properties);
    
        if ((result == kIOReturnSuccess) && (client != NULL)) {
            if (!client->attach(this)) {
                client->release();
                result = kIOReturnError;
            } else if (!client->start(this)) {
                client->detach(this);
                client->release();
                result = kIOReturnError;
            } else {
                assert(workLoop);	// <rdar://7324947>
    
                result = workLoop->runAction(_addUserClientAction, this, client);	// <rdar://7324947>, <rdar://7529580>
                
                if (result == kIOReturnSuccess) {
                    *handler = client;
                }
			}
        } else {
            result = kIOReturnNoMemory;
        }
    } else {
        result = kIOReturnNoDevice;
    }

    audioDebugIOLog(3, "- IOAudioEngine[%p]::newUserClient(0x%p, %p, 0x%lx, %p, %p)\n", this, task, securityID, (long unsigned int)type, properties, handler);
    return result;
}

void IOAudioEngine::clientClosed(IOAudioEngineUserClient *client)
{
    if (client) {
        assert(workLoop);												// <rdar://7529580>

        workLoop->runAction(_removeUserClientAction, this, client);		//	<rdar://7529580>
    }
    audioDebugIOLog(3, "+- IOAudioEngine[%p]::clientClosed(%p)\n", this, client);
}

// <rdar://7529580>
IOReturn IOAudioEngine::_addUserClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, target);
        if (audioEngine) {
            IOCommandGate *cg;
            
            cg = audioEngine->getCommandGate();
            
            if (cg) {
				setCommandGateUsage(audioEngine, true);		// <rdar://8518215>
                result = cg->runAction(addUserClientAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(audioEngine, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngine::addUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    audioDebugIOLog(3, "+ IOAudioEngine::addUserClientAction(%p, %p)\n", owner, arg1);

    if (owner) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, owner);
        if (audioEngine) {
            result = audioEngine->addUserClient((IOAudioEngineUserClient *)arg1);
        }
    }
    
    audioDebugIOLog(3, "- IOAudioEngine::addUserClientAction(%p, %p) returns 0x%lX\n", owner, arg1, (long unsigned int)result );
    return result;
}

// <rdar://7529580>
IOReturn IOAudioEngine::_removeUserClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, target);
        if (audioEngine) {
            IOCommandGate *cg;
            
            cg = audioEngine->getCommandGate();
            
            if (cg) {
				setCommandGateUsage(audioEngine, true);		// <rdar://8518215>
                result = cg->runAction(removeUserClientAction, arg0, arg1, arg2, arg3);
				setCommandGateUsage(audioEngine, false);	// <rdar://8518215>
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioEngine::removeUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, owner);
        if (audioEngine) {
            result = audioEngine->removeUserClient((IOAudioEngineUserClient *)arg1);
        }
    }
    
    audioDebugIOLog(3, "+- IOAudioEngine::removeUserClientAction(%p, %p) returns 0x%lX\n", owner, arg1, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::detachUserClientsAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioEngine *audioEngine = OSDynamicCast(IOAudioEngine, owner);
        if (audioEngine) {
            result = audioEngine->detachUserClients();
        }
    }
    
    audioDebugIOLog(3, "+- IOAudioEngine::detachUserClientsAction(%p, %p, %p, %p, %p) returns 0x%lX\n", owner, arg1, arg2, arg3, arg4, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::addUserClient(IOAudioEngineUserClient *newUserClient)
{
    IOReturn result = kIOReturnSuccess;
    
    assert(userClients);
    
    userClients->setObject(newUserClient);
    
    audioDebugIOLog(3, "+- IOAudioEngine[%p]::addUserClient(%p) returns 0x%lX\n", this, newUserClient, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::removeUserClient(IOAudioEngineUserClient *userClient)
{
    IOReturn result = kIOReturnSuccess;
    
    assert(userClients);
    
    userClient->retain();
    
    userClients->removeObject(userClient);
    
    if (userClient->isOnline()) {
        decrementActiveUserClients();
    }
    
    if (!isInactive()) {
        userClient->terminate();
    }
    
    userClient->release();
    
    audioDebugIOLog(3, "+- IOAudioEngine[%p]::removeUserClient(%p) returns 0x%lX\n", this, userClient, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::detachUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::detachUserClients\n", this);

    assert(userClients);
    
    if (!isInactive()) {	// Iterate through and terminate each user client
		audioDebugIOLog ( 3, "  !isInactive ()\n" );
        OSIterator *iterator;
        
        iterator = OSCollectionIterator::withCollection(userClients);
        
        if (iterator) {
            IOAudioEngineUserClient *userClient;
            
            while ( (userClient = (IOAudioEngineUserClient *)iterator->getNextObject()) )
			{
				audioDebugIOLog ( 3, "  will invoke userClient->terminate ()\n" );
                userClient->terminate();
				audioDebugIOLog ( 3, "  completed userClient->terminate ()\n" );
            }
			audioDebugIOLog ( 3, "  will invoke iterator->release ()\n" );
            iterator->release();
			audioDebugIOLog ( 3, "  completed iterator->release ()\n" );
        }
    }
    
	audioDebugIOLog ( 3, "  will invoke userClients->flushCollection ()\n" );
    userClients->flushCollection();
	audioDebugIOLog ( 3, "  completed userClients->flushCollection ()\n" );
    
    if (getState() == kIOAudioEngineRunning) {
        IOAudioEnginePosition stopPosition;
        
        assert(status);
        
        stopPosition.fSampleFrame = getCurrentSampleFrame();
        stopPosition.fLoopCount = status->fCurrentLoopCount + 1;

		audioDebugIOLog ( 3, "  will invoke stopEngineAtPosition ()\n" );
        stopEngineAtPosition(&stopPosition);
		audioDebugIOLog ( 3, "  completed stopEngineAtPosition ()\n" );
    }
    
    audioDebugIOLog(3, "- IOAudioEngine[%p]::detachUserClients returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::startClient(IOAudioEngineUserClient *userClient)
{
    IOReturn        result = kIOReturnSuccess;
    int             retries = 10;

    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEngineStartClient, (uintptr_t)this, isInactive(), (uintptr_t)audioDevice, audioDevice ? audioDevice->getPowerState() : 0);
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::startClient(%p)\n", this, userClient);

    retain();

    if (audioDevice && commandGate && !audioDevice->isInactive() && !isInactive())
    {
        audioDevice->retain();
        setCommandGateUsage(this, true);                    //	<rdar://8518215,10885615>
        
        while ( retries-- && (result == kIOReturnSuccess) && (audioDevice->getPowerState() == kIOAudioDeviceSleep) )
        {
            IOReturn            kr;
            AbsoluteTime        deadline;
            
            clock_interval_to_deadline(1, kSecondScale, &deadline);                // wait up to 1 second for each time around the loop

            kr = commandGate->commandSleep( &audioDevice->currentPowerState, deadline, THREAD_ABORTSAFE );
            
            AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineStartClient, (uintptr_t)this, kr, isInactive(), 0);
            
            //  <rdar://9487554,10885615> On interruption or time out return the appropriate error. This
            //  is to prevent it being stuck in the while loop waiting for the power state
            //  change.
            switch (kr)
            {
                case THREAD_AWAKENED:
                case THREAD_TIMED_OUT:
                    // either the 1 second timer expired or we go awakened by an actual power change. in either case we need to check to
                    // see if we (and the device) are still active and if so, whether or not the power state is good
                    // if everything is still active, then we will go around the loop and check the power state
                    if (isInactive() || !commandGate || !audioDevice || audioDevice->isInactive() )
                    {
                        result = kIOReturnNoDevice;
                    }
                    break;

                // we might get an interrupted status if the thread attempting to connect gets force quit
                case THREAD_INTERRUPTED:
                    result = kIOReturnAborted;
                    break;
                    
                // we don't expeect RESTART or any other status, so log anything and abort
                case THREAD_RESTART:
                default:
                    audioErrorIOLog("Sound Assert: IOAudioEngine::startClient got kr 0x%08x\n", kr);
                    result = kIOReturnAborted;
                    break;
                    
            }
            if ((result == kIOReturnSuccess) && audioDevice && (audioDevice->getPowerState() != kIOAudioDeviceSleep))
            {
                break;              // break out of the while loop without decrementing retries (catches a last ms success)
            }
        }
        
        if (retries == -1)
        {
            // we are timing out after 10 seconds. we need an assert for that case
            audioErrorIOLog("Sound Assert: IOAudioEngine::startClient timed out waiting\n");
            result = kIOReturnTimeout;
        }
        
        if (userClient && (result == kIOReturnSuccess))
        {
            result = incrementActiveUserClients();
        }
        
        setCommandGateUsage(this, false);	//	<rdar://8518215,10885615>
        audioDevice->release();
    }
    else
    {
        result = kIOReturnNoDevice;
    }
    
    release();
    audioDebugIOLog(3, "- IOAudioEngine[%p]::startClient(%p) returns 0x%lX\n", this, userClient, (long unsigned int)result );
    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEngineStartClient, (uintptr_t)this, (uintptr_t)userClient, isInactive(), result);
    return result;
}

IOReturn IOAudioEngine::stopClient(IOAudioEngineUserClient *userClient)
{
    IOReturn result = kIOReturnSuccess;
    
    if (userClient) {
        if (userClient->isOnline()) {
            result = decrementActiveUserClients();
        }
    } else {
        result = kIOReturnBadArgument;
    }
    
    audioDebugIOLog(3, "+- IOAudioEngine[%p]::stopClient(%p) returns 0x%lX\n", this, userClient, (long unsigned int)result );
    return result;
}
    
IOReturn IOAudioEngine::incrementActiveUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::incrementActiveUserClients() - %ld\n", this, (long int)numActiveUserClients);
    
    numActiveUserClients++;

    setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, sizeof(UInt32)*8);

    if (numActiveUserClients == 1) {
        result = startAudioEngine();
    }
    
	if (result != kIOReturnSuccess) {
		decrementActiveUserClients();
	}
	
    audioDebugIOLog(3, "- IOAudioEngine[%p]::incrementActiveUserClients() - %ld returns %lX\n", this, (long int)numActiveUserClients, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::decrementActiveUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
    numActiveUserClients--;
    
    setProperty(kIOAudioEngineNumActiveUserClientsKey, numActiveUserClients, sizeof(UInt32)*8);

    if ((numActiveUserClients == 0) && (getState() == kIOAudioEngineRunning)) {
        IOAudioEnginePosition stopPosition;
        
        assert(status);
        
        stopPosition.fSampleFrame = getCurrentSampleFrame();
        stopPosition.fLoopCount = status->fCurrentLoopCount + 1;

        stopEngineAtPosition(&stopPosition);
    }
    
    audioDebugIOLog(3, "+- IOAudioEngine[%p]::decrementActiveUserClients() - %ld returns 0x%lX\n", this, (long int)numActiveUserClients, (long unsigned int)result );
    return result;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

IOReturn IOAudioEngine::addAudioStream(IOAudioStream *stream)
{
    IOReturn result = kIOReturnBadArgument;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::addAudioStream(%p)\n", this, stream);

    if (stream) {

        if (!stream->attach(this))
		{
            result = kIOReturnError;
        }
		else
		{
			if (!stream->start(this))
			{
				stream->detach(this);
				result = kIOReturnError;
			}
			else
			{
				switch ( stream->getDirection () )
				{
					case kIOAudioStreamDirectionOutput:
						assert(outputStreams);

						outputStreams->setObject(stream);
						
						maxNumOutputChannels += stream->getMaxNumChannels();
						
						if (outputStreams->getCount() == 1) {
							setRunEraseHead(true);
						}

						if (reserved->bytesInOutputBufferArrayDescriptor) {
							reserved->bytesInOutputBufferArrayDescriptor->release();
						}
						reserved->bytesInOutputBufferArrayDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryKernelUserShared, round_page(outputStreams->getCount() * sizeof(UInt32)), page_size);
						break;
					case kIOAudioStreamDirectionInput:
						assert(inputStreams);
						
						inputStreams->setObject(stream);
						
						maxNumInputChannels += stream->getMaxNumChannels();

						if (reserved->bytesInInputBufferArrayDescriptor) {
							reserved->bytesInInputBufferArrayDescriptor->release();
						}
						reserved->bytesInInputBufferArrayDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryKernelUserShared, round_page(inputStreams->getCount() * sizeof(UInt32)), page_size);
						break;
				}

				if (isRegistered) {
					stream->registerService();
				}
				
				result = kIOReturnSuccess;
			}
		}
    }

    audioDebugIOLog(3, "- IOAudioEngine[%p]::addAudioStream(%p) returns 0x%lX\n", this, stream, (long unsigned int)result );
    return result;
}

void IOAudioEngine::detachAudioStreams()
{
    OSCollectionIterator *iterator;
    IOAudioStream *stream;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::detachAudioStreams()\n", this);

    if (outputStreams && (outputStreams->getCount() > 0)) {
        iterator = OSCollectionIterator::withCollection(outputStreams);
        if (iterator) {
            while ( (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) ) {
                if (!isInactive()) {
					stream->detach(this);			//	<rdar://14773236>
                    stream->terminate();
                }
            }
            iterator->release();
        }
        outputStreams->flushCollection();
		if (reserved->bytesInOutputBufferArrayDescriptor) {
			reserved->bytesInOutputBufferArrayDescriptor->release();
			reserved->bytesInOutputBufferArrayDescriptor = NULL;
		}
    }
    
    if (inputStreams && (inputStreams->getCount() > 0)) {
        iterator = OSCollectionIterator::withCollection(inputStreams);
        if (iterator) {
            while ( (stream = OSDynamicCast(IOAudioStream, iterator->getNextObject())) ) {
                if (!isInactive()) {
					stream->detach(this);				//	<rdar://14773236>
                    stream->terminate();
                }
            }
            iterator->release();
        }
        inputStreams->flushCollection();
		if (reserved->bytesInInputBufferArrayDescriptor) {
			reserved->bytesInInputBufferArrayDescriptor->release();
			reserved->bytesInInputBufferArrayDescriptor = NULL;
		}
    }

	if (reserved->streams) {
		reserved->streams->flushCollection();
	}
	
    audioDebugIOLog(3, "- IOAudioEngine[%p]::detachAudioStreams()\n", this);
	return;
}

void IOAudioEngine::lockAllStreams()
{
    OSCollectionIterator *streamIterator;
    
    if (outputStreams) {
        streamIterator = OSCollectionIterator::withCollection(outputStreams);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while ( (stream = (IOAudioStream *)streamIterator->getNextObject()) ) {
                stream->lockStreamForIO();
            }
            streamIterator->release();
        }
    }

    if (inputStreams) {
        streamIterator = OSCollectionIterator::withCollection(inputStreams);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while ( (stream = (IOAudioStream *)streamIterator->getNextObject()) ) {
                stream->lockStreamForIO();
            }
            streamIterator->release();
        }
    }
}

void IOAudioEngine::unlockAllStreams()
{
    OSCollectionIterator *streamIterator;
    
    if (outputStreams) {
        streamIterator = OSCollectionIterator::withCollection(outputStreams);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while ( (stream = (IOAudioStream *)streamIterator->getNextObject()) ) {
                stream->unlockStreamForIO();
            }
            streamIterator->release();
        }
    }

    if (inputStreams) {
        streamIterator = OSCollectionIterator::withCollection(inputStreams);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while ( (stream = (IOAudioStream *)streamIterator->getNextObject()) ) {
                stream->unlockStreamForIO();
            }
            streamIterator->release();
        }
    }
}

IOAudioStream *IOAudioEngine::getAudioStream(IOAudioStreamDirection direction, UInt32 channelID)
{
    IOAudioStream *audioStream = NULL;
    OSCollection *streamCollection = NULL;
    
    if (direction == kIOAudioStreamDirectionOutput) {
        streamCollection = outputStreams;
    } else {	// input
        streamCollection = inputStreams;
    }
    
    if (streamCollection) {
        OSCollectionIterator *streamIterator;
        
        streamIterator = OSCollectionIterator::withCollection(streamCollection);
        if (streamIterator) {
            IOAudioStream *stream;
            
            while ( (stream = (IOAudioStream *)streamIterator->getNextObject()) ) {
                if ((channelID >= stream->startingChannelID) && (channelID < (stream->startingChannelID + stream->maxNumChannels))) {
                    audioStream = stream;
                    break;
                }
            }
            streamIterator->release();
        }
    }
    
    return audioStream;
}

void IOAudioEngine::updateChannelNumbers()
{
    OSCollectionIterator *iterator;
    SInt32 *outputChannelNumbers = NULL, *inputChannelNumbers = NULL;
    UInt32 currentChannelID;
    SInt32 currentChannelNumber;
   
   
	audioDebugIOLog ( 3, "+ IOAudioEngine[%p]::updateChannelNumbers ()\n", this );
	
	// BEGIN <rdar://6997438> maxNumOutputChannels may not represent the true number of output channels at this point
	//					because the the number of formats in the stream may have changed. We recalculate the correct value here.
	
	maxNumOutputChannels = 0;
	maxNumInputChannels = 0;
    assert(outputStreams);
    assert(inputStreams);

    if (outputStreams->getCount() > 0) {
        iterator = OSCollectionIterator::withCollection(outputStreams);
        if (iterator) {
            IOAudioStream *audioStream;
            
            while ( (audioStream = (IOAudioStream *)iterator->getNextObject()) ) {
				maxNumOutputChannels += audioStream->getMaxNumChannels();
			}
			iterator->release();
		}
	}

	if (inputStreams->getCount() > 0) {
        iterator = OSCollectionIterator::withCollection(inputStreams);
        if (iterator) {
            IOAudioStream *audioStream;
            
            while ( (audioStream = (IOAudioStream *)iterator->getNextObject()) ) {
				maxNumInputChannels += audioStream->getMaxNumChannels();
			}
			iterator->release();
		}
	}
	// END <rdar://6997438>
	
	audioDebugIOLog(3, "  o=%ld i=%ld\n", (long int)maxNumOutputChannels, (long int)maxNumInputChannels);
	
    if (maxNumOutputChannels > 0) {
        outputChannelNumbers = (SInt32 *)IONewData(SInt32, maxNumOutputChannels);
    }
    
    if (maxNumInputChannels > 0) {
        inputChannelNumbers = (SInt32 *)IONewData(SInt32, maxNumInputChannels);
    }
    
    currentChannelID = 1;
    currentChannelNumber = 1;

    if (outputStreams->getCount() > 0) {
        iterator = OSCollectionIterator::withCollection(outputStreams);
        if (iterator) {
            IOAudioStream *audioStream;
            
            while ( (audioStream = (IOAudioStream *)iterator->getNextObject()) ) {
                const IOAudioStreamFormat *format;
                
                format = audioStream->getFormat();
                if (format) {
                    UInt32 numChannels, maxNumChannels;
                    UInt32 i;
                    
                    numChannels = format->fNumChannels;
                    maxNumChannels = audioStream->getMaxNumChannels();
                    
//                    assert(currentChannelID + maxNumChannels <= maxNumOutputChannels);		// double check that this calc is right.  MPC
                    
                    if (audioStream->getStreamAvailable()) {
                        audioStream->setStartingChannelNumber(currentChannelNumber);
                    } else {
                        numChannels = 0;
                        audioStream->setStartingChannelNumber(0);
                    }
                    
                    for (i = 0; i < numChannels; i++) {
                        outputChannelNumbers[currentChannelID + i - 1] = currentChannelNumber + i;
                    }
                    
                    for (i = numChannels; i < maxNumChannels; i++) {
                        outputChannelNumbers[currentChannelID + i - 1] = kIOAudioControlChannelNumberInactive;
                    }

                    currentChannelID += maxNumChannels;
                    currentChannelNumber += numChannels;
                }
            }
            
            iterator->release();
        }
    }
    
    currentChannelID = 1;
    currentChannelNumber = 1;
    
    if (inputStreams->getCount() > 0) {
        iterator = OSCollectionIterator::withCollection(inputStreams);
        if (iterator) {
            IOAudioStream *audioStream;
            
            while ( (audioStream = (IOAudioStream *)iterator->getNextObject()) ) {
                const IOAudioStreamFormat *format;
                
                format = audioStream->getFormat();
                if (format) {
                    UInt32 numChannels, maxNumChannels;
                    UInt32 i;
                    
                    numChannels = format->fNumChannels;
                    maxNumChannels = audioStream->getMaxNumChannels();
                    
//                    assert(currentChannelID + maxNumChannels <= maxNumInputChannels);		// double check that this calc is right.  MPC
                    
                    if (audioStream->getStreamAvailable()) {
                        audioStream->setStartingChannelNumber(currentChannelNumber);
                    } else {
                        numChannels = 0;
                        audioStream->setStartingChannelNumber(0);
                    }
                    
                    for (i = 0; i < numChannels; i++) {
                        inputChannelNumbers[currentChannelID + i - 1] = currentChannelNumber + i;
                    }
                    
                    for (i = numChannels; i < maxNumChannels; i++) {
                        inputChannelNumbers[currentChannelID + i - 1] = kIOAudioControlChannelNumberInactive;
                    }
                    
                    currentChannelID += maxNumChannels;
                    currentChannelNumber += numChannels;
                }
            }
            
            iterator->release();
        }
    }
    
    if (defaultAudioControls) {
        iterator = OSCollectionIterator::withCollection(defaultAudioControls);
        if (iterator) {
            IOAudioControl *control;
            while ( (control = (IOAudioControl *)iterator->getNextObject()) ) {
                UInt32 channelID;
                
                channelID = control->getChannelID();
                
                if (channelID != 0) {
                    switch (control->getUsage()) {
                        case kIOAudioControlUsageOutput:
								if (outputChannelNumbers && (channelID <= maxNumOutputChannels)) {
									control->setChannelNumber(outputChannelNumbers[channelID - 1]);
								} else {
									control->setChannelNumber(kIOAudioControlChannelNumberInactive);
							}
                            break;
                        case kIOAudioControlUsageInput:
							if (inputChannelNumbers && (channelID <= maxNumInputChannels)) {
								control->setChannelNumber(inputChannelNumbers[channelID - 1]);
							} else {
								control->setChannelNumber(kIOAudioControlChannelNumberInactive);
							}
                            break;
                        case kIOAudioControlUsagePassThru:
                            if (inputChannelNumbers) {
                                if (channelID <= maxNumInputChannels) {
                                    control->setChannelNumber(inputChannelNumbers[channelID - 1]);
                                } else {
                                    control->setChannelNumber(kIOAudioControlChannelNumberInactive);
                                }
                            } else if (outputChannelNumbers) {
                                if (channelID <= maxNumOutputChannels) {
                                    control->setChannelNumber(outputChannelNumbers[channelID - 1]);
                                } else {
                                    control->setChannelNumber(kIOAudioControlChannelNumberInactive);
                                }
                            } else {
                                control->setChannelNumber(kIOAudioControlChannelNumberInactive);
                            }
                            break;
                        default:
                            break;
                    }
                } else {
                    control->setChannelNumber(0);
                }
            }
            iterator->release();
        }
    }
    
    if (outputChannelNumbers && (maxNumOutputChannels > 0)) {
        IODeleteData(outputChannelNumbers, SInt32, maxNumOutputChannels);
    }
    
    if (inputChannelNumbers && (maxNumInputChannels > 0)) {
        IODeleteData(inputChannelNumbers, SInt32, maxNumInputChannels);
    }

	audioDebugIOLog ( 3, "- IOAudioEngine[%p]::updateChannelNumbers ()\n", this );
	return;
}

IOReturn IOAudioEngine::startAudioEngine()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::startAudioEngine(state = %d)\n", this, getState());

    switch(getState()) {
        case kIOAudioEnginePaused:
            result = resumeAudioEngine();
            break;
        case kIOAudioEngineStopped:
            audioDevice->audioEngineStarting();
        case kIOAudioEngineResumed:
            resetStatusBuffer();
            
			reserved->pauseCount = 0;
            result = performAudioEngineStart();
            if (result == kIOReturnSuccess) {
                setState(kIOAudioEngineRunning);
                sendNotification(kIOAudioEngineStartedNotification);
            } else if (getState() == kIOAudioEngineStopped) {
                audioDevice->audioEngineStopped();
            }
            break;
        default:
            break;
    }
    
    audioDebugIOLog(3, "- IOAudioEngine[%p]::startAudioEngine() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::stopAudioEngine()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::stopAudioEngine()\n", this);

    switch (getState()) {
        case kIOAudioEngineRunning:
            result = performAudioEngineStop();
        case kIOAudioEngineResumed:
            if (result == kIOReturnSuccess) {
                setState(kIOAudioEngineStopped);
                sendNotification(kIOAudioEngineStoppedNotification);
    
                assert(audioDevice);
                audioDevice->audioEngineStopped();
            }
            break;
        default:
            break;
    }
        
    audioDebugIOLog(3, "- IOAudioEngine[%p]::stopAudioEngine() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::pauseAudioEngine()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::pauseAudioEngine()\n", this);

	reserved->pauseCount++;
    switch(getState()) {
        case kIOAudioEngineRunning:
        case kIOAudioEngineResumed:
            // We should probably have the streams locked around performAudioEngineStop()
            // but we can't ensure that it won't make a call out that would attempt to take
            // one of the clientBufferLocks on an IOAudioEngineUserClient
            // If it did, that would create the potential for a deadlock
            result = performAudioEngineStop();
            if (result == kIOReturnSuccess) {
                setState(kIOAudioEnginePaused);
                clearAllSampleBuffers();
				sendNotification(kIOAudioEnginePausedNotification);
            }
            break;
        default:
            break;
    }
    
    audioDebugIOLog(3, "- IOAudioEngine[%p]::pauseAudioEngine() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

IOReturn IOAudioEngine::resumeAudioEngine()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::resumeAudioEngine()\n", this);
	
	if (0 != reserved->pauseCount) {
		if (0 == --reserved->pauseCount) {
			if (getState() == kIOAudioEnginePaused) {
				setState(kIOAudioEngineResumed);
				sendNotification(kIOAudioEngineResumedNotification);

				// <rdar://15485249>
				if (commandGate) {
					audioDebugIOLog(3, "send commandWakeup on resume for [%p]\n", this);
					
					setCommandGateUsage(this, true);
					commandGate->commandWakeup(&state);
					setCommandGateUsage(this, false);
					
					// <rdar://15917322,16383922> don't rely on audioEngineStopPosition if there are no clients and we've just resumed.
					if ((numActiveUserClients == 0) && !IOAUDIOENGINEPOSITION_IS_ZERO(&audioEngineStopPosition) && ( kIOAudioDeviceSleep != audioDevice->getPowerState() )) {
						stopAudioEngine();
					}
				}
			}
		}
	}
	else {
		audioDebugIOLog(1, "  attempting to resume while not paused\n" );
	}
    
    audioDebugIOLog(3, "- IOAudioEngine[%p]::resumeAudioEngine() returns 0x%lX\n", this, (long unsigned int)result  );
    return result;
}

IOReturn IOAudioEngine::performAudioEngineStart()
{
    return kIOReturnSuccess;
}

IOReturn IOAudioEngine::performAudioEngineStop()
{
    return kIOReturnSuccess;
}

const IOAudioEngineStatus *IOAudioEngine::getStatus()
{
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::getStatus()\n", this);

    return status;
}

void IOAudioEngine::setNumSampleFramesPerBuffer(UInt32 numSampleFrames)
{
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::setNumSampleFramesPerBuffer(0x%lx)\n", this, (long unsigned int)numSampleFrames);

    if (getState() == kIOAudioEngineRunning) {
        audioErrorIOLog("IOAudioEngine::setNumSampleFramesPerBuffer(0x%ld) - Error: can't change num sample frames while engine is running.\n", (long int) numSampleFrames);
    } else {
        numSampleFramesPerBuffer = numSampleFrames;
        setProperty(kIOAudioEngineNumSampleFramesPerBufferKey, numSampleFramesPerBuffer, sizeof(UInt32)*8);
        
        // Notify all output streams
        if (outputStreams) {
            OSCollectionIterator *streamIterator;
            
            streamIterator = OSCollectionIterator::withCollection(outputStreams);
            if (streamIterator) {
                IOAudioStream *audioStream;
                
                while ( (audioStream = (IOAudioStream *)streamIterator->getNextObject()) ) {
                    audioStream->numSampleFramesPerBufferChanged();
                }
                streamIterator->release();
            }
        }
    }
    audioDebugIOLog(3, "- IOAudioEngine[%p]::setNumSampleFramesPerBuffer(0x%lx)\n", this, (long unsigned int)numSampleFrames);
	return;
}

UInt32 IOAudioEngine::getNumSampleFramesPerBuffer()
{
    audioDebugIOLog(7, "+-IOAudioEngine[%p]::getNumSampleFramesPerBuffer() returns %ld\n", this, (long int)numSampleFramesPerBuffer );

    return numSampleFramesPerBuffer;
}



IOAudioEngineState
IOAudioEngine::getState()
{
    return state;
}



IOAudioEngineState
IOAudioEngine::setState(IOAudioEngineState newState)
{
    IOAudioEngineState oldState;

    audioDebugIOLog(3, "+-IOAudioEngine[%p]::setState(0x%x. oldState=%#x)\n", this, newState, state);
    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEngineSetState, (uintptr_t)this, index, state, newState);
    
    oldState = state;
    state = newState;

    switch (state)
    {
        case kIOAudioEngineRunning:
            if (oldState != kIOAudioEngineRunning)
            {
                addTimer();
            }
            break;
            
        case kIOAudioEngineStopped:
            if (oldState == kIOAudioEngineRunning)
            {
                removeTimer();
                performErase();
            }
            
			// <rdar://15485249,17416423>
			if (oldState == kIOAudioEnginePaused)
            {
				if (commandGate)
                {
					audioDebugIOLog(3, "send commandWakeup on stop for [%p]\n", this);
					
					setCommandGateUsage(this, true);
					commandGate->commandWakeup(&state);
					setCommandGateUsage(this, false);
				}
			}
            break;
            
        default:
            break;
    }

    setProperty(kIOAudioEngineStateKey, newState, sizeof(UInt32)*8);

    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEngineSetState, (uintptr_t)this, index, state, oldState);
    return oldState;
}



const IOAudioSampleRate *IOAudioEngine::getSampleRate()
{
    return &sampleRate;
}

void IOAudioEngine::setSampleRate(const IOAudioSampleRate *newSampleRate)
{
    OSDictionary *sampleRateDict;
    
    AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineSetSampleRate, (uintptr_t)this, newSampleRate->whole, 0, 0);
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::setSampleRate(%p)\n", this, newSampleRate);

    sampleRate = *newSampleRate;
    
    sampleRateDict = createDictionaryFromSampleRate(&sampleRate);
    if (sampleRateDict) {
        setProperty(kIOAudioSampleRateKey, sampleRateDict);
        sampleRateDict->release();
    }
}

IOReturn IOAudioEngine::hardwareSampleRateChanged(const IOAudioSampleRate *newSampleRate)
{
    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEngineHardwareSampleRateChanged, (uintptr_t)this, newSampleRate ? newSampleRate->whole : 0, 0, 0);
    if ((newSampleRate->whole != sampleRate.whole) || (newSampleRate->fraction != sampleRate.fraction)) {
        bool engineWasRunning;
        
        engineWasRunning = (state == kIOAudioEngineRunning);
        
        if (engineWasRunning) {
            pauseAudioEngine();
        }
        
        setSampleRate(newSampleRate);
        if (!configurationChangeInProgress) {
            sendNotification(kIOAudioEngineChangeNotification);
        }
        
        if (engineWasRunning) {
            resumeAudioEngine();
        }
    }
    
    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEngineHardwareSampleRateChanged, (uintptr_t)this, 0, 0, 0);
    return kIOReturnSuccess;
}

void IOAudioEngine::setSampleLatency(UInt32 numSamples)
{
    audioDebugIOLog(4, "+ IOAudioEngine[%p]::setSampleLatency(0x%lx)\n", this, (long unsigned int)numSamples);
    setOutputSampleLatency(numSamples);
    setInputSampleLatency(numSamples);
    audioDebugIOLog(4, "- IOAudioEngine[%p]::setSampleLatency(0x%lx)\n", this, (long unsigned int)numSamples);
	return;
}

void IOAudioEngine::setOutputSampleLatency(UInt32 numSamples)
{
    audioDebugIOLog(4, "+ IOAudioEngine[%p]::setOutputSampleLatency(0x%lx)\n", this, (long unsigned int)numSamples);
    setProperty(kIOAudioEngineOutputSampleLatencyKey, numSamples, sizeof(UInt32)*8);
    audioDebugIOLog(4, "- IOAudioEngine[%p]::setOutputSampleLatency(0x%lx)\n", this, (long unsigned int)numSamples);
	return;
}

void IOAudioEngine::setInputSampleLatency(UInt32 numSamples)
{
    audioDebugIOLog(4, "+ IOAudioEngine[%p]::setInputSampleLatency(0x%lx)\n", this, (long unsigned int)numSamples);
    setProperty(kIOAudioEngineInputSampleLatencyKey, numSamples, sizeof(UInt32)*8);
    audioDebugIOLog(4, "- IOAudioEngine[%p]::setInputSampleLatency(0x%lx)\n", this, (long unsigned int)numSamples);
	return;
}

void IOAudioEngine::setSampleOffset(UInt32 numSamples)
{
    audioDebugIOLog(4, "+ IOAudioEngine[%p]::setSampleOffset(0x%lx)\n", this, (long unsigned int)numSamples);
    sampleOffset = numSamples;
    setProperty(kIOAudioEngineSampleOffsetKey, numSamples, sizeof(UInt32)*8);
    audioDebugIOLog(4, "- IOAudioEngine[%p]::setSampleOffset(0x%lx)\n", this, (long unsigned int)numSamples);
	return;
}

void IOAudioEngine::setRunEraseHead(bool erase)
{
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::setRunEraseHead(%d)\n", this, erase);
    runEraseHead = erase;
}

bool IOAudioEngine::getRunEraseHead()
{
    audioDebugIOLog(7, "+-IOAudioEngine[%p]::getRunEraseHead()\n", this);

    return runEraseHead;
}

AbsoluteTime IOAudioEngine::getTimerInterval()
{
    AbsoluteTime interval;
    const IOAudioSampleRate *currentRate;
    
    audioDebugIOLog(4, "+ IOAudioEngine[%p]::getTimerInterval()\n", this);

    assert(status);

    currentRate = getSampleRate();
    
    if ((getNumSampleFramesPerBuffer() == 0) || (currentRate && (currentRate->whole == 0))) {
        nanoseconds_to_absolutetime(NSEC_PER_SEC, &interval);
    } else if ((numErasesPerBuffer == 0) || (!getRunEraseHead())) {	// Run once per ring buffer
        nanoseconds_to_absolutetime(((UInt64)NSEC_PER_SEC * (UInt64)getNumSampleFramesPerBuffer() / (UInt64)currentRate->whole), &interval);
    } else {
        OSCollectionIterator *outputIterator;
        IOAudioStream *outputStream;
		UInt32 bufferSize;
		UInt32 newNumErasesPerBuffer;

		outputIterator = OSCollectionIterator::withCollection(outputStreams);

		if (outputIterator) 
		{
			while ( (outputStream = (IOAudioStream *)outputIterator->getNextObject()) ) {
				bufferSize = outputStream->getSampleBufferSize();
				if ((bufferSize / numErasesPerBuffer) > 65536) {
					newNumErasesPerBuffer = bufferSize / 65536;
					if (newNumErasesPerBuffer > numErasesPerBuffer) {
						numErasesPerBuffer = newNumErasesPerBuffer;
					}
				}
			}
			outputIterator->release();
		}
		nanoseconds_to_absolutetime(((UInt64)NSEC_PER_SEC * (UInt64)getNumSampleFramesPerBuffer() / (UInt64)currentRate->whole / (UInt64)numErasesPerBuffer), &interval);
    }
    audioDebugIOLog(3, "- IOAudioEngine[%p]::getTimerInterval()\n", this);
    return interval;
}

void IOAudioEngine::timerCallback(OSObject *target, IOAudioDevice *device)
{
    IOAudioEngine *audioEngine;

    audioDebugIOLog(7, "+ IOAudioEngine::timerCallback(%p, %p)\n", target, device);

    audioEngine = OSDynamicCast(IOAudioEngine, target);
    if (audioEngine) {
        audioEngine->timerFired();
    }
    audioDebugIOLog(7, "- IOAudioEngine::timerCallback(%p, %p)\n", target, device);
	return;
}

void IOAudioEngine::timerFired()
{
	assert(reserved);

    audioDebugIOLog(7, "+ IOAudioEngine[%p]::timerFired()\n", this);

	// Setup the next timer event.
	AbsoluteTime now = 0;
	AbsoluteTime previousWakeupTime = reserved->eraseHeadTimerWakeupTime;
	clock_get_uptime(&now);
	while (CMP_ABSOLUTETIME(&now, &reserved->eraseHeadTimerWakeupTime) > 0) {
		ADD_ABSOLUTETIME(&reserved->eraseHeadTimerWakeupTime, &reserved->eraseHeadTimerPeriod);
	}

	if (reserved->eraseHeadTimerEvent != nullptr) {
		reserved->eraseHeadTimerEvent->wakeAtTime(reserved->eraseHeadTimerWakeupTime);
	}

	AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineTimerFired, (uintptr_t)this, (uint64_t)now,
			   previousWakeupTime, (uint64_t)reserved->eraseHeadTimerWakeupTime);
    performErase();
    performFlush();
	
    audioDebugIOLog(7, "- IOAudioEngine[%p]::timerFired()\n", this);
	return;
}

// <rdar://12188841>
void IOAudioEngine::performErase()
{
    audioDebugIOLog(7, "+ IOAudioEngine[%p]::performErase()\n", this);
    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEnginePerformErase, (uintptr_t)this, 0, 0, 0 );
    assert(status);
    
    if (getRunEraseHead() && getState() == kIOAudioEngineRunning) {
		UInt32 streamIndex;
        IOAudioStream *outputStream;
		UInt32 currentSampleFrame, eraseHeadSampleFrame;
		
		assert(outputStreams);
		
		/* <rdar://30784792>, Assignment sequence of below 3 lines matter. eraseHeadSampleFrame is assigned first and then currentSampleFrame value is read.
		 * This is to avoid erasing valid samples when the CPU context switch happens between two performErase() calls(one from timer, other from driver )
		 */
		eraseHeadSampleFrame = status->fEraseHeadSampleFrame;
		currentSampleFrame = getCurrentSampleFrame();
		status->fEraseHeadSampleFrame = currentSampleFrame;
		
		AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEnginePerformErase, (uintptr_t)this, currentSampleFrame, eraseHeadSampleFrame, eraseHeadSampleFrame );
		
		//	<rdar://12188841> Modified code to remove OSCollectionIterator allocation on every call
		outputStreams->retain();
		for ( streamIndex = 0; streamIndex < outputStreams->getCount(); streamIndex++) {
			char *sampleBuf, *mixBuf;
			UInt32 sampleBufferFrameSize, mixBufferFrameSize;

			outputStream = (IOAudioStream *)outputStreams->getObject(streamIndex);
			if ( outputStream ) {
				outputStream->lockStreamForIO();

				sampleBuf = (char *)outputStream->getSampleBuffer();
				mixBuf = (char *)outputStream->getMixBuffer();
				
				sampleBufferFrameSize = outputStream->format.fNumChannels * outputStream->format.fBitWidth / 8;
				mixBufferFrameSize = outputStream->format.fNumChannels * kIOAudioEngineDefaultMixBufferSampleSize;
				
				if (currentSampleFrame < eraseHeadSampleFrame) {
					// <rdar://problem/10040608> Add additional checks to ensure buffer is still of the appropriate length
					if (	(outputStream->getSampleBufferSize() == 0) ||                      									  	//  <rdar://10905878> Don't use more stringent test if a driver is incorrectly reporting buffer size
							((currentSampleFrame * sampleBufferFrameSize <= outputStream->getSampleBufferSize() ) &&
							((currentSampleFrame * mixBufferFrameSize <= outputStream->getMixBufferSize() ) || !mixBuf ) &&			//	<rdar://10866244> Don't check mix buffer if it's not in use (eg. !fIsMixable)
							(numSampleFramesPerBuffer * sampleBufferFrameSize <= outputStream->getSampleBufferSize() ) &&
							((numSampleFramesPerBuffer * mixBufferFrameSize <= outputStream->getMixBufferSize() ) || !mixBuf ) &&	//	<rdar://10866244> Don't check mix buffer if it's not in use (eg. !fIsMixable)
							(numSampleFramesPerBuffer > eraseHeadSampleFrame)) ) {
						audioDebugIOLog(7, "IOAudioEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx\n", this, (long unsigned int)eraseHeadSampleFrame, (long unsigned int)numSampleFramesPerBuffer);
						audioDebugIOLog(7, "IOAudioEngine[%p]::performErase() - erasing from frame: 0x%x to 0x%lx\n", this, 0, (long unsigned int)currentSampleFrame);
						eraseOutputSamples(mixBuf, sampleBuf, 0, currentSampleFrame, &outputStream->format, outputStream);
						eraseOutputSamples(mixBuf, sampleBuf, eraseHeadSampleFrame, numSampleFramesPerBuffer - eraseHeadSampleFrame, &outputStream->format, outputStream);
					}
				} else {
					// <rdar://problem/10040608> Add additional checks to ensure buffer is still of the appropriate length
					if (	(outputStream->getSampleBufferSize() == 0) ||                       									//  <rdar://10905878> Don't use more stringent test if a driver is incorrectly reporting buffer size
							( (currentSampleFrame * sampleBufferFrameSize <= outputStream->getSampleBufferSize() ) &&
							( (currentSampleFrame * mixBufferFrameSize <= outputStream->getMixBufferSize() ) || !mixBuf ) ) ) {		//	<rdar://10866244> Don't check mix buffer if it's not in use (eg. !fIsMixable)
						audioDebugIOLog(7, "IOAudioEngine[%p]::performErase() - erasing from frame: 0x%lx to 0x%lx\n", this, (long unsigned int)eraseHeadSampleFrame, (long unsigned int)currentSampleFrame);
						eraseOutputSamples(mixBuf, sampleBuf, eraseHeadSampleFrame, currentSampleFrame - eraseHeadSampleFrame, &outputStream->format, outputStream);
					}
				}

				outputStream->unlockStreamForIO();
			}
		}
		outputStreams->release();
    }

    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEnginePerformErase, (uintptr_t)this, 0, 0, 0 );
    audioDebugIOLog(7, "- IOAudioEngine[%p]::performErase()\n", this);
	return;
}

void IOAudioEngine::stopEngineAtPosition(IOAudioEnginePosition *endingPosition)
{
    audioDebugIOLog(4, "+ IOAudioEngine[%p]::stopEngineAtPosition(%lx,%lx)\n", this, endingPosition ? (long unsigned int)endingPosition->fLoopCount : 0, endingPosition ? (long unsigned int)endingPosition->fSampleFrame : 0);

    if (endingPosition) {
        audioEngineStopPosition = *endingPosition;
    } else {
        audioEngineStopPosition.fLoopCount = 0;
        audioEngineStopPosition.fSampleFrame = 0;
    }
	
    audioDebugIOLog(3, "- IOAudioEngine[%p]::stopEngineAtPosition(%lx,%lx)\n", this, endingPosition ? (long unsigned int)endingPosition->fLoopCount : 0, endingPosition ? (long unsigned int)endingPosition->fSampleFrame : 0);
	return;
}

void IOAudioEngine::performFlush()
{
    audioDebugIOLog(6, "+ IOAudioEngine[%p]::performFlush()\n", this);

    if ((numActiveUserClients == 0) && (getState() == kIOAudioEngineRunning)) {
        IOAudioEnginePosition currentPosition;
        
        assert(status);
        
        currentPosition.fLoopCount = status->fCurrentLoopCount;
        currentPosition.fSampleFrame = getCurrentSampleFrame();
        
        if (CMP_IOAUDIOENGINEPOSITION(&currentPosition, &audioEngineStopPosition) > 0) {
            stopAudioEngine();
        }
    }
	
    audioDebugIOLog(6, "- IOAudioEngine[%p]::performFlush()\n", this);
	return;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

void IOAudioEngine::addTimer()
{
	assert(reserved);

    audioDebugIOLog(4, "+ IOAudioEngine[%p]::addTimer()\n", this);

	// Set the erase head real-time priority based on the required timer interval and constraints.
	AbsoluteTime computation = 0;
	AbsoluteTime constraint  = 0;
	reserved->eraseHeadTimerPeriod = getTimerInterval();

	nanoseconds_to_absolutetime(reserved->eraseHeadComputationNs, &computation);
	nanoseconds_to_absolutetime(reserved->eraseHeadConstraintNs, &constraint);

	thread_time_constraint_policy tcPolicyData = {
		.period      = static_cast<uint32_t>(AbsoluteTime_to_scalar(&reserved->eraseHeadTimerPeriod)),
		.computation = static_cast<uint32_t>(AbsoluteTime_to_scalar(&computation)),
		.constraint  = static_cast<uint32_t>(AbsoluteTime_to_scalar(&constraint)),
	};

	audioDebugIOLog(4, "  IOAudioEngine[%p]::addTimer() - setting thread policy with"
					" period: %llu, computation: %llu, constraint: %llu\n", this,
					reserved->eraseHeadTimerPeriod, computation, constraint);
	kern_return_t ret = thread_policy_set(reserved->eraseHeadWorkloop->getThread(),
										  THREAD_TIME_CONSTRAINT_POLICY,
										  reinterpret_cast<thread_policy_t>(&tcPolicyData),
										  THREAD_TIME_CONSTRAINT_POLICY_COUNT);
	if (ret != KERN_SUCCESS) {
		audioErrorIOLog("IOAudioEngine[%p]::addTimer() - failed to set thread constraint policy: %x\n", this, ret);
	}

	clock_get_uptime(&reserved->eraseHeadTimerWakeupTime);
	ADD_ABSOLUTETIME(&reserved->eraseHeadTimerWakeupTime, &reserved->eraseHeadTimerPeriod);
	if (reserved->eraseHeadTimerEvent != nullptr && reserved->eraseHeadWorkloop != nullptr) {
		reserved->eraseHeadWorkloop->addEventSource(reserved->eraseHeadTimerEvent);
		reserved->eraseHeadTimerEvent->enable();
		reserved->eraseHeadTimerEvent->wakeAtTime(reserved->eraseHeadTimerWakeupTime);
	}

    audioDebugIOLog(4, "- IOAudioEngine[%p]::addTimer()\n", this);
	return;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

void IOAudioEngine::removeTimer()
{
	assert(reserved);

    audioDebugIOLog(4, "+ IOAudioEngine[%p]::removeTimer()\n", this);

	if (reserved->eraseHeadTimerEvent != nullptr) {
		reserved->eraseHeadTimerEvent->cancelTimeout();
		reserved->eraseHeadTimerEvent->disable();
		if (reserved->eraseHeadWorkloop != nullptr) {
			reserved->eraseHeadWorkloop->removeEventSource(reserved->eraseHeadTimerEvent);
		}
	}

    audioDebugIOLog(4, "- IOAudioEngine[%p]::removeTimer()\n", this);
	return;
}

IOReturn IOAudioEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    audioDebugIOLog(6, "+-IOAudioEngine[%p]::clipOutputSamples(%p, %p, 0x%lx, 0x%lx, %p, %p)\n", this, mixBuf, sampleBuf, (long unsigned int)firstSampleFrame, (long unsigned int)numSampleFrames, streamFormat, audioStream);

    return kIOReturnUnsupported;
}

void IOAudioEngine::resetClipPosition(IOAudioStream *audioStream, UInt32 clipSampleFrame)
{
    audioDebugIOLog(6, "+-IOAudioEngine[%p]::resetClipPosition(%p, 0x%lx)\n", this, audioStream, (long unsigned int)clipSampleFrame);

    return;
}


IOReturn IOAudioEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    audioDebugIOLog(6, "+-IOAudioEngine[%p]::convertInputSamples(%p, %p, 0x%lx, 0x%lx, %p, %p)\n", this, sampleBuf, destBuf, (long unsigned int)firstSampleFrame, (long unsigned int)numSampleFrames, streamFormat, audioStream);

    return kIOReturnUnsupported;
}

void IOAudioEngine::takeTimeStamp(bool incrementLoopCount, AbsoluteTime *timestamp)
{
    AbsoluteTime		uptime, *ts;
	IOAudioEngineStatus	newStatus;
	__int128			newStatus128, oldStatus128;
	
    if (timestamp) {
        ts = timestamp;
    } else {
        clock_get_uptime(&uptime);
        ts = &uptime;
    }
    
    assert(status);
	
	AbsoluteTime deadline;
	clock_interval_to_deadline(100, kMillisecondScale, &deadline);
	
	// To atomically update the status, we copy the current value, modify it and swap it in if the value hasn't changed.
	// If it fails, we try again until we timeout.
	do
	{
		newStatus = *status;
		oldStatus128 = *reinterpret_cast<__int128*>(&newStatus);

		newStatus.fLastLoopTime = *ts;
		if (incrementLoopCount)
			newStatus.fCurrentLoopCount++;
		newStatus128 = *reinterpret_cast<__int128*>(&newStatus);
	}
	while ( !__sync_bool_compare_and_swap(reinterpret_cast<__int128*>(status), oldStatus128, newStatus128) && ( mach_absolute_time() < deadline ) );
	
	if ( mach_absolute_time() > deadline )
	{
		IOLog("Sound Assert: error updating timestamp\n");
	}

	AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineTakeTimeStamp, (uintptr_t)this, (uintptr_t)incrementLoopCount, (uintptr_t)(status ? status->fLastLoopTime : 0), (uintptr_t)(status ? status->fCurrentLoopCount : 0) );
}

IOReturn IOAudioEngine::getLoopCountAndTimeStamp(UInt32 *loopCount, AbsoluteTime *timestamp)
{
    IOReturn result = kIOReturnBadArgument;
	
    if (loopCount && timestamp) {
        assert(status);
	
		__int128 currentStatus128;

		AbsoluteTime deadline;
		clock_interval_to_deadline(100, kMillisecondScale, &deadline);
		
		// To ensure an atomic read, we read and atomically verify the value hasn't changed since we read it. If it has, we repeat until we timeout
		do
		{
			currentStatus128 = *reinterpret_cast<__int128*>(status);
		}
		while ( !__sync_bool_compare_and_swap(&currentStatus128, *reinterpret_cast<__int128*>(status), currentStatus128) && ( mach_absolute_time() < deadline ) );
		
		if ( mach_absolute_time() > deadline )
		{
			IOLog("Sound Assert: error reading timestamp\n");
		}
		
		IOAudioEngineStatus currentStatus = *reinterpret_cast<IOAudioEngineStatus*>(&currentStatus128);
		*loopCount = currentStatus.fCurrentLoopCount;
		*timestamp = currentStatus.fLastLoopTime;

        result = kIOReturnSuccess;
    }
	
	//AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineGetLoopCountAndTimeStamp, (uintptr_t)this, (uintptr_t)*timestamp, (uintptr_t)*loopCount, 0);
    
    return result;
}

IOReturn IOAudioEngine::calculateSampleTimeout(AbsoluteTime *sampleIntervalTicks, UInt32 numSampleFrames, IOAudioEnginePosition *startingPosition, AbsoluteTime *wakeupTimeTicks)
{
    IOReturn        result = kIOReturnBadArgument;
    AbsoluteTime    wakeupDeltaTicks = 0;
    uint64_t        wakeupDeltaNS = 0;

    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeout, (uintptr_t)this, sampleIntervalTicks ? *sampleIntervalTicks : 0xDEADBEEF, numSampleFrames, startingPosition ? ((((uint64_t)startingPosition->fSampleFrame) << 32) | startingPosition->fLoopCount) : 0xDEADBEEF);
    
    if (reserved->useHiResSampleInterval)
    {
        if (sampleIntervalTicks)
            result = calculateSampleTimeoutHiRes(*sampleIntervalTicks, numSampleFrames, startingPosition, wakeupTimeTicks);
    }
    else
    {
        if (sampleIntervalTicks && (numSampleFrames != 0) && startingPosition && wakeupTimeTicks)
        {
            IOAudioEnginePosition   wakeupPosition;
            UInt32                  wakeupOffset;
            AbsoluteTime            lastLoopTimeTicks;
            UInt32                  currentLoopCount;
            AbsoluteTime            wakeupIntervalTicks;
            AbsoluteTime            currentTimeTicks;									// <rdar://10145205>
            uint64_t                currentTimeNanos;
            UInt64                  wakeupIntervalScalar;
            UInt32                  samplesFromLoopStart;
            AbsoluteTime            wakeupThreadLatencyPaddingIntervalTicks;
                        
            // Total wakeup interval now calculated at 90% minus 125us
            
            wakeupOffset = (numSampleFrames / reserved->mixClipOverhead) + sampleOffset;
            AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeout, (uintptr_t)this, ((uint64_t)reserved->mixClipOverhead << 32) | sampleOffset, wakeupOffset, 1);

            if (wakeupOffset <= startingPosition->fSampleFrame)
            {
                wakeupPosition = *startingPosition;
                wakeupPosition.fSampleFrame -= wakeupOffset;
            } else
            {
                wakeupPosition.fLoopCount = startingPosition->fLoopCount - 1;
                wakeupPosition.fSampleFrame = numSampleFramesPerBuffer - (wakeupOffset - startingPosition->fSampleFrame);
            }
            
            getLoopCountAndTimeStamp(&currentLoopCount, &lastLoopTimeTicks);
            
            samplesFromLoopStart = ((wakeupPosition.fLoopCount - currentLoopCount) * numSampleFramesPerBuffer) + wakeupPosition.fSampleFrame;

            AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeout, (uintptr_t)this, lastLoopTimeTicks, ((uint64_t)currentLoopCount << 32) | samplesFromLoopStart, 2);

            wakeupIntervalScalar = AbsoluteTime_to_scalar(sampleIntervalTicks);
            wakeupIntervalScalar *= samplesFromLoopStart;
            
            //wakeupInterval = scalar_to_AbsoluteTime(&wakeupIntervalScalar);
            wakeupIntervalTicks = *(AbsoluteTime *)(&wakeupIntervalScalar);
            
            AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeout, (uintptr_t)this, wakeupIntervalScalar, wakeupIntervalTicks, 3);

            nanoseconds_to_absolutetime(WATCHDOG_THREAD_LATENCY_PADDING_NS, &wakeupThreadLatencyPaddingIntervalTicks);
            
            SUB_ABSOLUTETIME(&wakeupIntervalTicks, &wakeupThreadLatencyPaddingIntervalTicks);
            
            *wakeupTimeTicks = lastLoopTimeTicks;
            ADD_ABSOLUTETIME(wakeupTimeTicks, &wakeupIntervalTicks);

            // <rdar://10145205> Sanity check the calculated time
            clock_get_uptime(&currentTimeTicks);
            absolutetime_to_nanoseconds(currentTimeTicks, &currentTimeNanos);
            AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeout, (uintptr_t)this, currentTimeNanos, currentTimeTicks, 5);

            if ( CMP_ABSOLUTETIME(wakeupTimeTicks, &currentTimeTicks) > 0 )
            {
                wakeupDeltaTicks = *wakeupTimeTicks;
                SUB_ABSOLUTETIME(&wakeupDeltaTicks, &currentTimeTicks);
                absolutetime_to_nanoseconds(wakeupDeltaTicks, &wakeupDeltaNS);
                result = kIOReturnSuccess;
            }
            else
            {
                AudioTrace(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeout, (uintptr_t)this, *wakeupTimeTicks, currentTimeTicks, 4);
                result = kIOReturnIsoTooOld;
            }
        }
    }

    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEngineCalculateSampleTimeout, (uintptr_t)this, wakeupTimeTicks ? *wakeupTimeTicks : 0xDEADBEEF, wakeupDeltaNS, result );
    return result;
}

IOReturn IOAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::performFormatChange(%p, %p, %p)\n", this, audioStream, newFormat, newSampleRate);

    return kIOReturnSuccess;
}

void IOAudioEngine::sendFormatChangeNotification(IOAudioStream *audioStream)
{
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::sendFormatChangeNotification(%p)\n", this, audioStream);

    if (!configurationChangeInProgress) {
        OSCollectionIterator *userClientIterator;
        IOAudioEngineUserClient *userClient;
    
        assert(userClients);
    
        userClientIterator = OSCollectionIterator::withCollection(userClients);
        if (userClientIterator) {
            while ( (userClient = OSDynamicCast(IOAudioEngineUserClient, userClientIterator->getNextObject())) ) {
                userClient->sendFormatChangeNotification(audioStream);
            }
            
            userClientIterator->release();
        }
    }
	
    audioDebugIOLog(3, "- IOAudioEngine[%p]::sendFormatChangeNotification(%p)\n", this, audioStream);
	return;
}

void IOAudioEngine::sendNotification(UInt32 notificationType)
{
    OSCollectionIterator *userClientIterator;
    IOAudioEngineUserClient *userClient;

    assert(userClients);

    userClientIterator = OSCollectionIterator::withCollection(userClients);
    if (userClientIterator) {
        while ( (userClient = OSDynamicCast(IOAudioEngineUserClient, userClientIterator->getNextObject())) ) {
            userClient->sendNotification(notificationType);
        }
        
        userClientIterator->release();
    }
}

void IOAudioEngine::beginConfigurationChange()
{
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::beginConfigurationChange()\n", this);

    configurationChangeInProgress = true;
}

void IOAudioEngine::completeConfigurationChange()
{
    if (configurationChangeInProgress) {
        configurationChangeInProgress = false;
        sendNotification(kIOAudioEngineChangeNotification);

		// If any controls have notifications queued up, now's the time to send them.
		if (defaultAudioControls) {
			if (!isInactive()) {
				OSCollectionIterator *controlIterator;
				
				controlIterator = OSCollectionIterator::withCollection(defaultAudioControls);
				
				if (controlIterator) {
					IOAudioControl *control;
					
					while ( (control = (IOAudioControl *)controlIterator->getNextObject()) ) {
						control->sendQueuedNotifications();
					}
				
					controlIterator->release();
				}
			}
		}
    }
	
    audioDebugIOLog(3, "+- IOAudioEngine[%p]::completeConfigurationChange()\n", this);
	return;
}

void IOAudioEngine::cancelConfigurationChange()
{
    audioDebugIOLog(3, "+-IOAudioEngine[%p]::cancelConfigurationChange()\n", this);

    configurationChangeInProgress = false;
}

IOReturn IOAudioEngine::addDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    IOReturn result = kIOReturnBadArgument;

    if (defaultAudioControl) {
		if (workLoop) {
			defaultAudioControl->setWorkLoop(workLoop);
		}
        if (defaultAudioControl->attachAndStart(this)) {
            if (!defaultAudioControls) {
                defaultAudioControls = OSSet::withObjects((const OSObject **)&defaultAudioControl, 1, 1);
            } else {
                defaultAudioControls->setObject(defaultAudioControl);
            }
            
            if (isRegistered) {
                updateChannelNumbers();
            }
            
            result = kIOReturnSuccess;
        } else {
            result = kIOReturnError;
        }
    }

    return result;
}

IOReturn IOAudioEngine::removeDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    IOReturn result = kIOReturnNotFound;
    
    if (defaultAudioControl) {
        if ((state != kIOAudioEngineRunning) && (state != kIOAudioEngineResumed)) {
            if (defaultAudioControls && defaultAudioControls->containsObject(defaultAudioControl)) {
                defaultAudioControl->retain();
                
                defaultAudioControls->removeObject(defaultAudioControl);
                
                defaultAudioControl->detach(this); // <rdar://14347861>
                
                if (defaultAudioControl->getProvider() == this) {
                    defaultAudioControl->terminate();
                }
                
                defaultAudioControl->release();
                
                if (!configurationChangeInProgress) {
                    sendNotification(kIOAudioEngineChangeNotification);
                }
                
                result = kIOReturnSuccess;
            }
        } else {
            result = kIOReturnNotPermitted;
        }
    } else {
        result = kIOReturnBadArgument;
    }
    
    return result;
}

void IOAudioEngine::removeAllDefaultAudioControls()
{
    audioDebugIOLog(3, "+ IOAudioEngine[%p]::removeAllDefaultAudioControls()\n", this);

    if (defaultAudioControls) {
        if (!isInactive()) {
            OSCollectionIterator *controlIterator;
            
            controlIterator = OSCollectionIterator::withCollection(defaultAudioControls);
            
            if (controlIterator) {
                IOAudioControl *control;
                
                while ( (control = (IOAudioControl *)controlIterator->getNextObject()) ) {
                    control->detach(this); // <rdar://14347861>
                    
                    if (control->getProvider() == this) {
                        control->terminate();
                    }
                }
            
                controlIterator->release();
            }
        }
        
        defaultAudioControls->flushCollection();
    }
	
    audioDebugIOLog(3, "- IOAudioEngine[%p]::removeAllDefaultAudioControls()\n", this);
	return;
}

void IOAudioEngine::setWorkLoopOnAllAudioControls(IOWorkLoop *wl)
{
    if (defaultAudioControls) {
        if (!isInactive()) {
            OSCollectionIterator *controlIterator;
            
            controlIterator = OSCollectionIterator::withCollection(defaultAudioControls);
            
            if (controlIterator) {
                IOAudioControl *control;
				while ( (control = (IOAudioControl *)controlIterator->getNextObject()) ) {
					if (control->getProvider() == this) {
						control->setWorkLoop(wl);
					}
				}
                controlIterator->release();
            }
        }
    }
}

// <rdar://8518215>
void IOAudioEngine::setCommandGateUsage(IOAudioEngine *engine, bool increment)
{
	if (engine->reserved) {
		if (increment) {
			switch (engine->reserved->commandGateStatus)
			{
				case kCommandGateStatus_Normal:
				case kCommandGateStatus_RemovalPending:
					engine->reserved->commandGateUsage++;
					break;
				case kCommandGateStatus_Invalid:
					// Should never be here. If so, something went bad...
					break;
			}
		}
		else {
			switch (engine->reserved->commandGateStatus)
			{
				case kCommandGateStatus_Normal:
					if (engine->reserved->commandGateUsage > 0) {
						engine->reserved->commandGateUsage--;
					}
					break;
				case kCommandGateStatus_RemovalPending:
					if (engine->reserved->commandGateUsage > 0) {
						engine->reserved->commandGateUsage--;
						
						if (engine->reserved->commandGateUsage == 0) {
							engine->reserved->commandGateStatus = kCommandGateStatus_Invalid;
							
							if (engine->commandGate) {
								if (engine->workLoop) {
									engine->workLoop->removeEventSource(engine->commandGate);
								}
								
								engine->commandGate->release();
								engine->commandGate = NULL;
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

// <rdar://15485249>
IOReturn IOAudioEngine::waitForEngineResume ( void )
{
	IOReturn err, result = kIOReturnError;
    AudioTrace_Start(kAudioTIOAudioEngine, kTPIOAudioEngineWaitForEngineResume, (uintptr_t)this, 0, 0, 0);
    
	if (commandGate) {
        AbsoluteTime        deadline;
        
        retain();

		audioDebugIOLog(3, "Waiting on engine[%p] resume...\n", this);
		setCommandGateUsage(this, true);

        clock_interval_to_deadline(3, kSecondScale, &deadline); // <rdar://25475295>
        
        err = commandGate->commandSleep( &state, deadline, THREAD_ABORTSAFE );
        
		setCommandGateUsage(this, false);
		
		audioDebugIOLog(3, "...wait completed for engine[%p] with err=%#x\n", this, err);
		
		if ( THREAD_INTERRUPTED == err ) {
			result = kIOReturnAborted;
		}
		else if ( THREAD_TIMED_OUT == err ) {
			result = kIOReturnTimeout;
		}
		else if ( THREAD_RESTART == err ) {
			result = kIOReturnNotPermitted;
		}
		else if ( THREAD_AWAKENED == err )	{
			result = kIOReturnSuccess;
		}

		release();
	}
	
    AudioTrace_End(kAudioTIOAudioEngine, kTPIOAudioEngineWaitForEngineResume, (uintptr_t)this, 0, 0, result);
	return result;
}

