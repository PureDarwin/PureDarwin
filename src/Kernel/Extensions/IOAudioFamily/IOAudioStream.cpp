/*
 * Copyright (c) 1998-2013 Apple Computer, Inc. All rights reserved.
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
#include "IOAudioStream.h"
#include "IOAudioEngine.h"
#include "IOAudioEngineUserClient.h"
#include "IOAudioControl.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
// Unused macros in open source
#define AudioTrace_Start(a, b, c, d, e, f)
#define AudioTrace_End(a, b, c, d, e, f)
#define AudioTrace(a, b, c, d, e, f)

#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>

typedef struct IOAudioStreamFormatExtensionDesc {
    UInt32								version;
    UInt32								flags;
	UInt32								framesPerPacket;
	UInt32								bytesPerPacket;
} IOAudioStreamFormatExtensionDesc;

typedef struct IOAudioStreamFormatDesc {
    IOAudioStreamFormat					format;
    IOAudioSampleRate					minimumSampleRate;
    IOAudioSampleRate					maximumSampleRate;
    IOAudioStream::AudioIOFunction		*ioFunctionList;
    UInt32								numIOFunctions;
    IOAudioStreamFormatExtensionDesc	formatExtension;
} IOAudioStreamFormatDesc;

#define super IOService
OSDefineMetaClassAndStructors(IOAudioStream, IOService)

OSMetaClassDefineReservedUsed(IOAudioStream, 0);
OSMetaClassDefineReservedUsed(IOAudioStream, 1);
OSMetaClassDefineReservedUsed(IOAudioStream, 2);
OSMetaClassDefineReservedUsed(IOAudioStream, 3);
OSMetaClassDefineReservedUsed(IOAudioStream, 4);
OSMetaClassDefineReservedUsed(IOAudioStream, 5);
OSMetaClassDefineReservedUsed(IOAudioStream, 6);
OSMetaClassDefineReservedUsed(IOAudioStream, 7);
OSMetaClassDefineReservedUsed(IOAudioStream, 8);
OSMetaClassDefineReservedUsed(IOAudioStream, 9);
OSMetaClassDefineReservedUsed(IOAudioStream, 10);
OSMetaClassDefineReservedUsed(IOAudioStream, 11);

OSMetaClassDefineReservedUnused(IOAudioStream, 12);
OSMetaClassDefineReservedUnused(IOAudioStream, 13);
OSMetaClassDefineReservedUnused(IOAudioStream, 14);
OSMetaClassDefineReservedUnused(IOAudioStream, 15);
OSMetaClassDefineReservedUnused(IOAudioStream, 16);
OSMetaClassDefineReservedUnused(IOAudioStream, 17);
OSMetaClassDefineReservedUnused(IOAudioStream, 18);
OSMetaClassDefineReservedUnused(IOAudioStream, 19);
OSMetaClassDefineReservedUnused(IOAudioStream, 20);
OSMetaClassDefineReservedUnused(IOAudioStream, 21);
OSMetaClassDefineReservedUnused(IOAudioStream, 22);
OSMetaClassDefineReservedUnused(IOAudioStream, 23);
OSMetaClassDefineReservedUnused(IOAudioStream, 24);
OSMetaClassDefineReservedUnused(IOAudioStream, 25);
OSMetaClassDefineReservedUnused(IOAudioStream, 26);
OSMetaClassDefineReservedUnused(IOAudioStream, 27);
OSMetaClassDefineReservedUnused(IOAudioStream, 28);
OSMetaClassDefineReservedUnused(IOAudioStream, 29);
OSMetaClassDefineReservedUnused(IOAudioStream, 30);
OSMetaClassDefineReservedUnused(IOAudioStream, 31);
OSMetaClassDefineReservedUnused(IOAudioStream, 32);
OSMetaClassDefineReservedUnused(IOAudioStream, 33);
OSMetaClassDefineReservedUnused(IOAudioStream, 34);
OSMetaClassDefineReservedUnused(IOAudioStream, 35);
OSMetaClassDefineReservedUnused(IOAudioStream, 36);
OSMetaClassDefineReservedUnused(IOAudioStream, 37);
OSMetaClassDefineReservedUnused(IOAudioStream, 38);
OSMetaClassDefineReservedUnused(IOAudioStream, 39);
OSMetaClassDefineReservedUnused(IOAudioStream, 40);
OSMetaClassDefineReservedUnused(IOAudioStream, 41);
OSMetaClassDefineReservedUnused(IOAudioStream, 42);
OSMetaClassDefineReservedUnused(IOAudioStream, 43);
OSMetaClassDefineReservedUnused(IOAudioStream, 44);
OSMetaClassDefineReservedUnused(IOAudioStream, 45);
OSMetaClassDefineReservedUnused(IOAudioStream, 46);
OSMetaClassDefineReservedUnused(IOAudioStream, 47);

// New code added here:

#define CMPSAMPLERATE(left, right) ((left.whole < right->whole) ? -1 : (left.whole == right->whole) ? (left.fraction < right->fraction) ? -1 : 0 : 1)

// <rdar://problem/5994776> Amount of frames allowed to go over the pseudo mix buffer size. We use the source buffer as a mix buffer in encoded format mode.
// But we can't clip an arbitrary amount of data. We need to limit it to what the size of the source buffer is. The problem seems to
// be that the source buffer size is hidden by some VBR change. The true size of the source buffer is saved off an then readded after we can use it. 
// So through imperical testing it looks like the source buffer size is 4 times the IOBufferSize. We set it to 3 times to be safe. 

#define kMixBufferMaxSize ( 2043 ) //  Limit to 2 pages but there is 16 bytes taken out of the sample buffer for VBR stuff

// <rdar://problem/10305944> function to log streaming errors safely. On production builds it will increment error counters. For Debug builds it will still log via kprintf
void IOAudioStream::safeLogError(int error , long unsigned int arg1 , long unsigned int arg2 , long unsigned int arg3 , long unsigned int arg4 , void * arg5, void *arg6)
{
	if (reserved)
	{
		switch (error) 
		{
			case kErrorLogClipMoreThanOneBufferAhead:
				//arg1 = clippedPosition.fLoopCount,
				//arg2 = clippedPosition.fSampleFrame, 
				//arg3 = clientBufferListStart->mixedPosition.fLoopCount,
				//arg4 = clientBufferListStart->mixedPosition.fSampleFrame
				
				reserved->mStreamErrorCounts[kErrorLogClipMoreThanOneBufferAhead]++;
				reserved->mStreamErrorCountsUpdated = true;
				audioDebugIOLog(1,"+-IOAudioStream[%p]:: clipIfNecessary() - Error: attempting to clip to a position more than one buffer ahead of last clip position (%lx,%lx)->(%lx,%lx).\n", this, arg1, arg2, arg3, arg4);
				break;
			case kErrorLogClipMoreThanOneBufferAheadPart2:
				//arg1 = clippedPosition.fLoopCount,
				//arg2 = clippedPosition.fSampleFrame, 			
				reserved->mStreamErrorCounts[kErrorLogClipMoreThanOneBufferAheadPart2]++;
				reserved->mStreamErrorCountsUpdated = true;

				audioDebugIOLog(1,"+-IOAudioStream[%p]::safeLogError clipIfNecessary() - adjusting clipped position to (%lx,%lx)\n", this, arg1 , arg2);				
				break;
			case kErrorLogClipPositionIsOff:
				//arg1 = clientBufferListStart->numSampleFrames
				//arg2 = audioEngine->getNumSampleFramesPerBuffer()
				//arg3 = clippedPosition.fSampleFrame
				reserved->mStreamErrorCounts[kErrorLogClipPositionIsOff]++;
				reserved->mStreamErrorCountsUpdated = true;
			
				audioDebugIOLog (1,"+-IOAudioStream[%p]:: clipIfNecessary() - clip position is off %ld < %ld - %ld \n",this, arg1 , arg2, arg3);
				break;
			case kErrorLogAlreadyClipped:
				//arg1 = clippedPosition.fLoopCount
				//arg2 = clippedPosition.fSampleFrame
				//arg3 = clientBufferListStart->mixedPosition.fLoopCount
				//arg4 = clientBufferListStart->mixedPosition.fSampleFrame
				
				reserved->mStreamErrorCounts[kErrorLogAlreadyClipped]++;
				reserved->mStreamErrorCountsUpdated = true;
				audioDebugIOLog(1,"+-IOAudioStream[%p]::safeLogError clipIfNecessary() - Error: already clipped to a position (0x%lx,0x%lx) past data to be clipped (0x%lx, 0x%lx) - data ignored.\n", this, arg1 , arg2 ,arg3 ,arg4);
				break;
			case kErrorLogClipBuffersAreNULL:
				//arg1 = firstSampleFrame
				//arg2 = numSampleFrames
				//arg3 = 0
				//arg4 = 0
				//arg5 = mixBuffer
				//arg6 = sampleBuffer
				
				reserved->mStreamErrorCounts[kErrorLogClipBuffersAreNULL]++;
				reserved->mStreamErrorCountsUpdated = true;
				audioDebugIOLog(1,"+-IOAudioStream[%p]::safeLogError clipOutputSamples(0x%lx, 0x%lx) - Internal Error: mixBuffer = %p - sampleBuffer = %p\n", this ,arg1, arg2, arg5 , arg6);
				break;
			case kErrorLogClipReturnsAnError:
				//arg1 = firstSampleFrame
				//arg2 = numSampleFrames
				//arg3 = result
				
				reserved->mStreamErrorCounts[kErrorLogClipReturnsAnError]++;
				reserved->mStreamErrorCountsUpdated = true;
				audioDebugIOLog(1,"+-IOAudioStream[%p]::safeLogError clipOutputSamples(0x%lx, 0x%lx) - clipping function returned error: 0x%lx\n", this, arg1 , arg2 , arg3);
				break;
			case kErrorLogDumpCounters:
				audioDebugIOLog(1,"+-IOAudioStream[%p]::safeLogError kErrorLogDumpCounters mStreamErrorCountsUpdated=%d\n", this,reserved->mStreamErrorCountsUpdated);

				if ( reserved->mStreamErrorCounts[kErrorLogClipMoreThanOneBufferAhead] )
				{
					IOLog("IOAudioStream::clipIfNecessary() - Error: counted %u clip more than one buffer ahead errors.\n", reserved->mStreamErrorCounts[kErrorLogClipMoreThanOneBufferAhead] );
					reserved->mStreamErrorCounts[kErrorLogClipMoreThanOneBufferAhead] = 0;
				}
				
				//if ( reserved->mStreamErrorCounts[kErrorLogClipMoreThanOneBufferAheadPart2] )
				// Do not need to log this error as it is redundant with the first part
				
				if ( reserved->mStreamErrorCounts[kErrorLogClipPositionIsOff] )
				{					
					IOLog("IOAudioStream::clipIfNecessary() - Error: counted %u clip position is off errors.\n", reserved->mStreamErrorCounts[kErrorLogClipPositionIsOff] );
					reserved->mStreamErrorCounts[kErrorLogClipPositionIsOff] = 0;
				}
				
				if ( reserved->mStreamErrorCounts[kErrorLogAlreadyClipped] )
				{
					IOLog("IOAudioStream::clipIfNecessary() - Error: counted %u already clipped errors.\n", reserved->mStreamErrorCounts[kErrorLogAlreadyClipped] );
					reserved->mStreamErrorCounts[kErrorLogAlreadyClipped] = 0;
				}
				
				if ( reserved->mStreamErrorCounts[kErrorLogClipBuffersAreNULL] )
				{	
					IOLog("IOAudioStream::clipIfNecessary() - Error: counted %u clip buffers are NULL errors.\n", reserved->mStreamErrorCounts[kErrorLogClipBuffersAreNULL] );
					reserved->mStreamErrorCounts[kErrorLogClipBuffersAreNULL] = 0;
				}
				
				if ( reserved->mStreamErrorCounts[kErrorLogClipReturnsAnError] )
				{
					IOLog("IOAudioStream::clipIfNecessary() - Error: counted %u clip returns an error conditions.\n", reserved->mStreamErrorCounts[kErrorLogClipReturnsAnError] );
					reserved->mStreamErrorCounts[kErrorLogClipReturnsAnError] = 0;
				}
				
				break;
			default:
				
				break;
		}
	}
	
	return;
}

bool IOAudioStream::validateFormat(IOAudioStreamFormat *streamFormat, IOAudioStreamFormatExtension *formatExtension, IOAudioStreamFormatDesc *formatDesc, const IOAudioSampleRate *sampleRate)
{
    bool foundFormat = false;
    
    audioDebugIOLog(3, "+ IOAudioStream[%p]::validateFormat(%p, %p, %p)\n", this, streamFormat, formatExtension, formatDesc);
	AudioTrace_Start(kAudioTIOAudioStream, kTPIOAudioStreamValidateFormat, (uintptr_t)this, streamFormat ? streamFormat->fNumChannels : 0, streamFormat ? streamFormat->fBitDepth : 0, sampleRate ? sampleRate->whole : 0);
    
    if (streamFormat && availableFormats && (numAvailableFormats > 0) && sampleRate) {
        UInt32 formatIndex;

        for (formatIndex = 0; formatIndex < numAvailableFormats; formatIndex++) {
			audioDebugIOLog(3, "  %ld: streamFormat->fNumChannels = %ld\n", (long int)availableFormats[formatIndex].format.fNumChannels, (long int)streamFormat->fNumChannels);
			audioDebugIOLog(3, "  0x%lx: streamFormat->fSampleFormat = 0x%lx\n", (long unsigned int)availableFormats[formatIndex].format.fSampleFormat, (long unsigned int)streamFormat->fSampleFormat);
			audioDebugIOLog(3, "  0x%lx: streamFormat->fNumericRepresentation = 0x%lx\n", (long unsigned int)availableFormats[formatIndex].format.fNumericRepresentation, (long unsigned int)streamFormat->fNumericRepresentation);
			audioDebugIOLog(3, "  %d: streamFormat->fBitDepth = %d\n", availableFormats[formatIndex].format.fBitDepth, streamFormat->fBitDepth);
			audioDebugIOLog(3, "  %d: streamFormat->fBitWidth = %d\n", availableFormats[formatIndex].format.fBitWidth, streamFormat->fBitWidth);
			audioDebugIOLog(3, "  %d: streamFormat->fAlignment = %d\n", availableFormats[formatIndex].format.fAlignment, streamFormat->fAlignment);
			audioDebugIOLog(3, "  %d: streamFormat->fByteOrder = %d\n", availableFormats[formatIndex].format.fByteOrder, streamFormat->fByteOrder);
            if ((availableFormats[formatIndex].format.fNumChannels == streamFormat->fNumChannels)
				&& (availableFormats[formatIndex].format.fSampleFormat == streamFormat->fSampleFormat)
				&& (availableFormats[formatIndex].format.fNumericRepresentation == streamFormat->fNumericRepresentation)
				&& (availableFormats[formatIndex].format.fBitDepth == streamFormat->fBitDepth)
				&& (availableFormats[formatIndex].format.fBitWidth == streamFormat->fBitWidth)
				&& (availableFormats[formatIndex].format.fAlignment == streamFormat->fAlignment)
				&& (availableFormats[formatIndex].format.fByteOrder == streamFormat->fByteOrder)
				&& (availableFormats[formatIndex].format.fIsMixable == streamFormat->fIsMixable)) {
				
				bool passSRCheck = true;
				if (0 != sampleRate->whole) { 
					if ((CMPSAMPLERATE (availableFormats[formatIndex].minimumSampleRate, sampleRate) > 0) || (CMPSAMPLERATE (availableFormats[formatIndex].maximumSampleRate, sampleRate) < 0)) {
						passSRCheck = false;
					}
				}
				if (passSRCheck) {
					// <rdar://10957396> Only update the tag if required
					if ( streamFormat->fDriverTag != availableFormats[formatIndex].format.fDriverTag ) {
						streamFormat->fDriverTag = availableFormats[formatIndex].format.fDriverTag;
					}

	//				streamFormat->fIsMixable = availableFormats[formatIndex].format.fIsMixable;
					if (formatDesc) {
						memcpy(formatDesc, &availableFormats[formatIndex], sizeof(IOAudioStreamFormatDesc));
					}
					foundFormat = true;
					break;
				}
            }
        }
    }
    
    audioDebugIOLog(3, "- IOAudioStream[%p]::validateFormat(%p, %p, %p) returns %d\n", this, streamFormat, formatExtension, formatDesc, foundFormat );
	AudioTrace_End(kAudioTIOAudioStream, kTPIOAudioStreamValidateFormat, (uintptr_t)this, 0, 0, foundFormat);
    return foundFormat;
}

const IOAudioStreamFormatExtension *IOAudioStream::getFormatExtension()
{
	assert(reserved);
	return &reserved->streamFormatExtension;
}

IOReturn IOAudioStream::setFormat(const IOAudioStreamFormat *streamFormat, const IOAudioStreamFormatExtension *formatExtension, bool callDriver)
{
    IOReturn result = kIOReturnSuccess;
    OSDictionary *formatDict = NULL;
	IOAudioStreamFormatExtension validFormatExtension;
    
	AudioTrace_Start(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 3, streamFormat ? (((UInt64)streamFormat->fNumChannels << 32) | streamFormat->fBitDepth) : 0, callDriver);
    if (streamFormat) {
		if (!formatExtension) {
			IOAudioStreamFormatDesc formatDesc;
			validateFormat((IOAudioStreamFormat *)streamFormat, NULL, &formatDesc);
			memcpy (&validFormatExtension, &formatDesc.formatExtension, sizeof (validFormatExtension));
		} else {
			validFormatExtension = *formatExtension;
		}
        if ( (formatDict = createDictionaryFromFormat(streamFormat, &validFormatExtension)) ) {
			AudioTrace(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 3, (((UInt64)formatDict->getCount() << 32) | validFormatExtension.fVersion), 0);
            result = setFormat(streamFormat, &validFormatExtension, formatDict, callDriver);
            formatDict->release();
        } else {
            result = kIOReturnError;
        }
    } else {
        result = kIOReturnBadArgument;
    }

	AudioTrace_End(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 3, 0, result);
    return result;
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

IOReturn IOAudioStream::setFormat(const IOAudioStreamFormat *streamFormat, const IOAudioStreamFormatExtension *formatExtension, OSDictionary *formatDict, bool callDriver)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioStreamFormat validFormat;
    IOAudioStreamFormatDesc formatDesc;
	IOAudioStreamFormatExtension validFormatExtension;
    const IOAudioSampleRate *requestedSampleRate = NULL;
    OSDictionary *sampleRateDict;
    
	AudioTrace_Start(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 4, streamFormat ? (((UInt64)streamFormat->fNumChannels << 32) | streamFormat->fBitDepth) : 0, callDriver);
    audioDebugIOLog(3, "+ IOAudioStream[%p]::setFormat(%p, %p)\n", this, streamFormat, formatDict);

    if (!streamFormat || !formatDict)
	{
		AudioTrace(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 4, (uintptr_t)formatDict, 0);
        result = kIOReturnBadArgument;
    }
	else
	{
	#ifdef DEBUG
		setProperty("IOAudioStreamPendingFormat", formatDict);
	#endif
		
		validFormat = *streamFormat;
		if (NULL != formatExtension) {
			validFormatExtension = *formatExtension;
		} else {
			validFormatExtension.fVersion = kFormatExtensionCurrentVersion;
			validFormatExtension.fFlags = 0;
			validFormatExtension.fFramesPerPacket = 1;
			validFormatExtension.fBytesPerPacket = streamFormat->fNumChannels * (streamFormat->fBitWidth / 8);
		}

		sampleRateDict = OSDynamicCast(OSDictionary, formatDict->getObject(kIOAudioSampleRateKey));
		if (sampleRateDict) {
			requestedSampleRate = IOAudioEngine::createSampleRateFromDictionary(sampleRateDict);
		} else {
			requestedSampleRate = audioEngine->getSampleRate();
		}

		if (validateFormat(&validFormat, &validFormatExtension, &formatDesc, requestedSampleRate)) {
	//        OSDictionary *sampleRateDict;
			IOAudioSampleRate *newSampleRate = NULL;
			OSSet *userClientsToLock;
			
			sampleRateDict = OSDynamicCast(OSDictionary, formatDict->getObject(kIOAudioSampleRateKey));
			if (sampleRateDict) {
				const IOAudioSampleRate *currentSampleRate;
				
				newSampleRate = IOAudioEngine::createSampleRateFromDictionary(sampleRateDict);
				currentSampleRate = audioEngine->getSampleRate();
				if (newSampleRate && (newSampleRate->whole == currentSampleRate->whole) && (newSampleRate->fraction == currentSampleRate->fraction)) {
					newSampleRate = NULL;
				}
			}
			
			// In order to avoid deadlocks, we need to ensure we hold all of the user client locks
			// before making calls while holding our IO lock.  Everything works fine as long
			// as the order of the locks is workLoop -> user client -> stream.
			// Any other order is sure to cause trouble.
			
			// Because we pause the engine while doing the format change, the user clients will be removed
			// from our list before we complete.  Therefore, we must make a copy of the list to allow
			// all of the clients to be unlocked when we are done.
			userClientsToLock = OSSet::withCapacity(numClients);
			if (userClientsToLock) {
				OSCollectionIterator *clientIterator;
				IOAudioClientBuffer *clientBuf;
				IOAudioEngineUserClient *userClient;
				
				lockStreamForIO();										// <rdar://13186726> 
				
				clientBuf = userClientList;
				while (clientBuf) {
					assert(clientBuf->userClient);
					
					userClientsToLock->setObject(clientBuf->userClient);
					clientBuf = clientBuf->nextClient;
				}
				
				unlockStreamForIO();									// <rdar://13186726> 
			
				clientIterator = OSCollectionIterator::withCollection(userClientsToLock);
				if (!clientIterator) {
					userClientsToLock->release();
					result = kIOReturnNoMemory;
					goto Done;
				}
				
				while ( (userClient = (IOAudioEngineUserClient *)clientIterator->getNextObject()) ) {
					userClient->lockBuffers();
				}
				
				clientIterator->release();
				
				audioEngine->pauseAudioEngine();
				
				if (callDriver) {
					result = audioEngine->performFormatChange(this, &validFormat, &validFormatExtension, newSampleRate);
					if (result != kIOReturnSuccess)
					{
						AudioTrace(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 4, result, 2);
					}
					if ( result == kIOReturnUnsupported )
					{
						result = audioEngine->performFormatChange(this, &validFormat, newSampleRate);
						if (result != kIOReturnSuccess)
						{
							AudioTrace(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 4, result, 4);
						}
					}
				}
				
				if (result == kIOReturnSuccess) {
					OSDictionary *newFormatDict;
					
					if (formatDesc.ioFunctionList && (formatDesc.numIOFunctions > 0)) {
						setIOFunctionList(formatDesc.ioFunctionList, formatDesc.numIOFunctions);
					}
					
					newFormatDict = createDictionaryFromFormat(&validFormat, &validFormatExtension);
					if (newFormatDict) {
						UInt32 oldNumChannels;
						
						lockStreamForIO();

						if (mixBuffer != NULL) {
							// If we have a mix buffer and the new format is not mixable, free the mix buffer
							if (!validFormat.fIsMixable) {
								setMixBuffer(NULL, 0);
							} else if (streamAllocatedMixBuffer && (format.fNumChannels != validFormat.fNumChannels)) {	// We need to reallocate the mix buffer
								UInt32 newMixBufSize;
			
								assert(audioEngine);
								newMixBufSize = validFormat.fNumChannels * kIOAudioEngineDefaultMixBufferSampleSize * audioEngine->numSampleFramesPerBuffer;
			
								if (newMixBufSize > 0) {
									void *newMixBuf = IOMallocData(newMixBufSize);
									if (newMixBuf) {
										setMixBuffer(newMixBuf, newMixBufSize);
										streamAllocatedMixBuffer = true;
									}
								}
							}
						}
						unlockStreamForIO();
						
						oldNumChannels = format.fNumChannels;
						
						format = validFormat;
						setProperty(kIOAudioStreamFormatKey, newFormatDict);
						newFormatDict->release();
		
						if (format.fNumChannels != oldNumChannels) {
							audioEngine->updateChannelNumbers();
						}
		
						if (newSampleRate) {
							audioEngine->setSampleRate(newSampleRate);
						}
					} else {
						result = kIOReturnError;
					}
				} else {
					if ( kIOReturnNotReady != result ) {	//	<rdar://8094567>
					IOLog("IOAudioStream::setFormat - audio engine unable to change format\n");
					}
				}
				
				if (result == kIOReturnSuccess) {
					audioEngine->sendFormatChangeNotification(this);
				}
				
				audioEngine->resumeAudioEngine();
				
				// Unlock all of the user clients we originally locked
				assert(userClientsToLock);
				clientIterator = OSCollectionIterator::withCollection(userClientsToLock);
				if (clientIterator) {
					while ( (userClient = (IOAudioEngineUserClient *)clientIterator->getNextObject()) ) {
						userClient->unlockBuffers();
					}
					clientIterator->release();
				} else {
					// Uh oh... we're in trouble now!
					// We have to unlock the clients, but we can't get an iterator on the collection.
					// All existing clients will now hang trying to play audio
					result = kIOReturnNoMemory;
				}
				
				userClientsToLock->release();
			} else {
				result = kIOReturnNoMemory;
			}
		} else {
			IOLog("IOAudioStream::setFormat - invalid format.\n");
			AudioTrace(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 4, requestedSampleRate ? requestedSampleRate->whole : 0, 1);
			result = kIOReturnBadArgument;
		}
	}
    
Done:
    
    audioDebugIOLog(3, "IOAudioStream[%p]::setFormat(%p, %p) returns 0x%lx", this, streamFormat, formatDict, (long unsigned int)result);

	AudioTrace_End(kAudioTIOAudioStream, kTPIOAudioStreamSetFormat, (uintptr_t)this, 4, 0, result);
    return result;
}

void IOAudioStream::addAvailableFormat(const IOAudioStreamFormat *streamFormat, const IOAudioStreamFormatExtension *formatExtension, const IOAudioSampleRate *minRate, const IOAudioSampleRate *maxRate, const AudioIOFunction *ioFunctionList, UInt32 numFunctions)
{
    assert(availableFormatDictionaries);

    if (streamFormat && minRate && maxRate) {
        IOAudioStreamFormatDesc *newAvailableFormatList;
		IOAudioStreamFormatExtension	localFormatExtension;
        
		newAvailableFormatList = IONew(IOAudioStreamFormatDesc, (numAvailableFormats+1));
        if (newAvailableFormatList) {
            if (availableFormats && (numAvailableFormats > 0)) {
                memcpy(newAvailableFormatList, availableFormats, numAvailableFormats * sizeof(IOAudioStreamFormatDesc));
            }
            newAvailableFormatList[numAvailableFormats].format = *streamFormat;
            newAvailableFormatList[numAvailableFormats].minimumSampleRate = *minRate;
            newAvailableFormatList[numAvailableFormats].maximumSampleRate = *maxRate;
            if (formatExtension) {
				localFormatExtension = *formatExtension;
                newAvailableFormatList[numAvailableFormats].formatExtension.flags = formatExtension->fFlags;
                newAvailableFormatList[numAvailableFormats].formatExtension.framesPerPacket = formatExtension->fFramesPerPacket;
                newAvailableFormatList[numAvailableFormats].formatExtension.bytesPerPacket = formatExtension->fBytesPerPacket;
            } else {
                newAvailableFormatList[numAvailableFormats].formatExtension.flags = localFormatExtension.fFlags = 0;
                newAvailableFormatList[numAvailableFormats].formatExtension.framesPerPacket = localFormatExtension.fFramesPerPacket = 1;
                newAvailableFormatList[numAvailableFormats].formatExtension.bytesPerPacket = localFormatExtension.fBytesPerPacket = streamFormat->fNumChannels * (streamFormat->fBitWidth / 8);
            }

			newAvailableFormatList[numAvailableFormats].ioFunctionList = NULL;
			newAvailableFormatList[numAvailableFormats].numIOFunctions = 0;

            if (ioFunctionList && (numFunctions > 0)) {
                newAvailableFormatList[numAvailableFormats].ioFunctionList = IONew(AudioIOFunction, numFunctions);

				if (newAvailableFormatList[numAvailableFormats].ioFunctionList) {
    	            newAvailableFormatList[numAvailableFormats].numIOFunctions = numFunctions;
	                memcpy(newAvailableFormatList[numAvailableFormats].ioFunctionList, ioFunctionList, numFunctions * sizeof(AudioIOFunction));
				}
            }

            IODelete(availableFormats, IOAudioStreamFormatDesc, numAvailableFormats);
            availableFormats = newAvailableFormatList;
            numAvailableFormats++;
        }
        
        OSDictionary *formatDict = createDictionaryFromFormat(streamFormat, &localFormatExtension);
        if (formatDict) {
            OSDictionary *sampleRateDict;
        
            sampleRateDict = IOAudioEngine::createDictionaryFromSampleRate(minRate);
            if (sampleRateDict) {
                formatDict->setObject(gMinimumSampleRateKey, sampleRateDict);
                sampleRateDict->release();
				
                sampleRateDict = IOAudioEngine::createDictionaryFromSampleRate(maxRate);
                if (sampleRateDict) {
					OSArray *newAvailableFormats;
					OSArray *oldAvailableFormats;
					
					oldAvailableFormats = availableFormatDictionaries;
					newAvailableFormats = OSDynamicCast(OSArray, availableFormatDictionaries->copyCollection());  // copyCollection() does a deep copy
					
					if (newAvailableFormats) {
						formatDict->setObject(gMaximumSampleRateKey, sampleRateDict);
						newAvailableFormats->setObject(formatDict);
						availableFormatDictionaries = newAvailableFormats;
						setProperty(kIOAudioStreamAvailableFormatsKey, availableFormatDictionaries);
						oldAvailableFormats->release();
						if (streamFormat->fNumChannels > maxNumChannels) {
							maxNumChannels = streamFormat->fNumChannels;
						}
					}
					
					sampleRateDict->release();
                }
            }
            formatDict->release();
        }
    }
}

void IOAudioStream::addAvailableFormat(const IOAudioStreamFormat *streamFormat, const IOAudioStreamFormatExtension *formatExtension, const IOAudioSampleRate *minRate, const IOAudioSampleRate *maxRate, AudioIOFunction ioFunction)
{
    addAvailableFormat(streamFormat, formatExtension, minRate, maxRate, &ioFunction, 1);
}

bool IOAudioStream::validateFormat(IOAudioStreamFormat *streamFormat, IOAudioStreamFormatExtension *formatExtension, IOAudioStreamFormatDesc *formatDesc)
{
	return validateFormat(streamFormat, formatExtension, formatDesc, audioEngine->getSampleRate());
}

void IOAudioStream::setTerminalType(const UInt32 terminalType)
{
    if (terminalType) {
        setProperty(kIOAudioStreamTerminalTypeKey, terminalType, 32);
    }
}

IOReturn IOAudioStream::mixOutputSamples(const void *sourceBuf, void *mixBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	bcopy (sourceBuf, (float *)mixBuf + (firstSampleFrame * streamFormat->fNumChannels), numSampleFrames * sizeof (float) * streamFormat->fNumChannels);

	return kIOReturnSuccess;
}

void IOAudioStream::setSampleLatency(UInt32 numSamples)
{
    audioDebugIOLog(3, "+-IOAudioStream[%p]::setSampleLatency(0x%lx)\n", this, (long unsigned int)numSamples);
    setProperty(kIOAudioStreamSampleLatencyKey, numSamples, sizeof(UInt32)*8);
}

UInt32 IOAudioStream::getNumSampleFramesRead()
{
	assert(reserved);
    audioDebugIOLog(4, "+-IOAudioStream[%p]::getNumSampleFramesRead() returns %ld\n", this, (long unsigned int)reserved->mSampleFramesReadByEngine);
	return reserved->mSampleFramesReadByEngine;
}

void IOAudioStream::setDefaultNumSampleFramesRead(UInt32 inDefaultNumFramesRead)
{
	assert(reserved);
    audioDebugIOLog(4, "+-IOAudioStream[%p]::setDefaultNumSampleFramesRead(%ld)\n", this, (long unsigned int)inDefaultNumFramesRead);
	reserved->mSampleFramesReadByEngine = inDefaultNumFramesRead;
}

// Original code from here on:
const OSSymbol *IOAudioStream::gDirectionKey = NULL;
const OSSymbol *IOAudioStream::gNumChannelsKey = NULL;
const OSSymbol *IOAudioStream::gSampleFormatKey = NULL;
const OSSymbol *IOAudioStream::gNumericRepresentationKey = NULL;
const OSSymbol *IOAudioStream::gBitDepthKey = NULL;
const OSSymbol *IOAudioStream::gBitWidthKey = NULL;
const OSSymbol *IOAudioStream::gAlignmentKey = NULL;
const OSSymbol *IOAudioStream::gByteOrderKey = NULL;
const OSSymbol *IOAudioStream::gIsMixableKey = NULL;
const OSSymbol *IOAudioStream::gDriverTagKey = NULL;
const OSSymbol *IOAudioStream::gMinimumSampleRateKey = NULL;
const OSSymbol *IOAudioStream::gMaximumSampleRateKey = NULL;

void IOAudioStream::initKeys()
{
    if (!gNumChannelsKey) {
        gNumChannelsKey = OSSymbol::withCString(kIOAudioStreamNumChannelsKey);
        gSampleFormatKey = OSSymbol::withCString(kIOAudioStreamSampleFormatKey);
        gNumericRepresentationKey = OSSymbol::withCString(kIOAudioStreamNumericRepresentationKey);
        gBitDepthKey = OSSymbol::withCString(kIOAudioStreamBitDepthKey);
        gBitWidthKey = OSSymbol::withCString(kIOAudioStreamBitWidthKey);
        gAlignmentKey = OSSymbol::withCString(kIOAudioStreamAlignmentKey);
        gByteOrderKey = OSSymbol::withCString(kIOAudioStreamByteOrderKey);
        gIsMixableKey = OSSymbol::withCString(kIOAudioStreamIsMixableKey);
        gDriverTagKey = OSSymbol::withCString(kIOAudioStreamDriverTagKey);

        gDirectionKey = OSSymbol::withCString(kIOAudioStreamDirectionKey);
        
        gMinimumSampleRateKey = OSSymbol::withCString(kIOAudioStreamMinimumSampleRateKey);
        gMaximumSampleRateKey = OSSymbol::withCString(kIOAudioStreamMaximumSampleRateKey);
    }
}

OSDictionary *IOAudioStream::createDictionaryFromFormat(const IOAudioStreamFormat *streamFormat, const IOAudioStreamFormatExtension *formatExtension, OSDictionary *formatDict)
{
    OSDictionary *newDict = NULL;

    if (streamFormat) {
        if (formatDict) {
            newDict = formatDict;
        } else {
            newDict = OSDictionary::withCapacity(7);
        }

        if (newDict) {
            OSNumber *num;

            if (!gNumChannelsKey) {
                initKeys();
            }
            
            num = OSNumber::withNumber(streamFormat->fNumChannels, 32);
            newDict->setObject(gNumChannelsKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fSampleFormat, 32);
            newDict->setObject(gSampleFormatKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fNumericRepresentation, 32);
            newDict->setObject(gNumericRepresentationKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fBitDepth, 8);
            newDict->setObject(gBitDepthKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fBitWidth, 8);
            newDict->setObject(gBitWidthKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fAlignment, 8);
            newDict->setObject(gAlignmentKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fByteOrder, 8);
            newDict->setObject(gByteOrderKey, num);
            num->release();

            num = OSNumber::withNumber(streamFormat->fIsMixable, 8);
            newDict->setObject(gIsMixableKey, num);
            num->release();
            
            num = OSNumber::withNumber(streamFormat->fDriverTag, 32);
            newDict->setObject(gDriverTagKey, num);
            num->release();

			if (formatExtension && formatExtension->fVersion >= kFormatExtensionCurrentVersion) {
				num = OSNumber::withNumber(formatExtension->fFlags, 32);
				newDict->setObject(kIOAudioStreamFormatFlagsKey, num);
				num->release();
			
				num = OSNumber::withNumber(formatExtension->fFramesPerPacket, 32);
				newDict->setObject(kIOAudioStreamFramesPerPacketKey, num);
				num->release();

				num = OSNumber::withNumber(formatExtension->fBytesPerPacket, 32);
				newDict->setObject(kIOAudioStreamBytesPerPacketKey, num);
				num->release();
			}
        }
    }


    return newDict;
}

IOAudioStreamFormat *IOAudioStream::createFormatFromDictionary(const OSDictionary *formatDict, IOAudioStreamFormat *streamFormat, IOAudioStreamFormatExtension *formatExtension)
{
    IOAudioStreamFormat *format = NULL;
    static IOAudioStreamFormat staticFormat;

    if (formatDict) {
        if (streamFormat) {
            format = streamFormat;
        } else {
            format = &staticFormat;
        }

        if (format) {
            OSNumber *num;

            if (!gNumChannelsKey) {
                initKeys();
            }

            bzero(format, sizeof(IOAudioStreamFormat));

            num = OSDynamicCast(OSNumber, formatDict->getObject(gNumChannelsKey));
            if (num) {
                format->fNumChannels = num->unsigned32BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gSampleFormatKey));
            if (num) {
                format->fSampleFormat = num->unsigned32BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gNumericRepresentationKey));
            if (num) {
                format->fNumericRepresentation = num->unsigned32BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gBitDepthKey));
            if (num) {
                format->fBitDepth = num->unsigned8BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gBitWidthKey));
            if (num) {
                format->fBitWidth = num->unsigned8BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gAlignmentKey));
            if (num) {
                format->fAlignment = num->unsigned8BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gByteOrderKey));
            if (num) {
                format->fByteOrder = num->unsigned8BitValue();
            }

            num = OSDynamicCast(OSNumber, formatDict->getObject(gIsMixableKey));
            if (num) {
                format->fIsMixable = num->unsigned8BitValue();
            }
            
            num = OSDynamicCast(OSNumber, formatDict->getObject(gDriverTagKey));
            if (num) {
                format->fDriverTag = num->unsigned32BitValue();
            }

			if (formatExtension) {
				formatExtension->fVersion = kFormatExtensionCurrentVersion;

				num = OSDynamicCast(OSNumber, formatDict->getObject(kIOAudioStreamFormatFlagsKey));
				if (num) {
					formatExtension->fFlags = num->unsigned32BitValue();
				}

				num = OSDynamicCast(OSNumber, formatDict->getObject(kIOAudioStreamFramesPerPacketKey));
				if (num) {
					formatExtension->fFramesPerPacket = num->unsigned32BitValue();
				}

				num = OSDynamicCast(OSNumber, formatDict->getObject(kIOAudioStreamBytesPerPacketKey));
				if (num) {
					formatExtension->fBytesPerPacket = num->unsigned32BitValue();
				}

			}
        }
    }

    return format;
}


bool IOAudioStream::initWithAudioEngine(IOAudioEngine *engine, IOAudioStreamDirection dir, UInt32 startChannelID, const char *streamDescription, OSDictionary *properties)
{
	UInt32				streamID;

    if (!gNumChannelsKey) {
        initKeys();
    }

    if (!engine) {
        return false;
    }

    if (!super::init(properties)) {
        return false;
    }

    audioEngine = engine;
    
	reserved = IOMallocType (ExpansionData);
	if (!reserved) {
		return false;
	}
	

	
    workLoop = audioEngine->getWorkLoop();
    if (!workLoop) {
        return false;
    }

    workLoop->retain();
    
    commandGate = IOCommandGate::commandGate(this);
    if (!commandGate) {
        return false;
    }
    
    streamIOLock = IORecursiveLockAlloc();
    if (!streamIOLock) {
        return false;
    }
    
    setDirection(dir);
    
    startingChannelID = startChannelID;
    setProperty(kIOAudioStreamStartingChannelIDKey, startingChannelID, sizeof(UInt32)*8);
    
    maxNumChannels = 0;
    
    if (streamDescription) {
        setProperty(kIOAudioStreamDescriptionKey, streamDescription);
    }
    
    availableFormatDictionaries = OSArray::withCapacity(1);
    if (!availableFormatDictionaries) {
        return false;
    }
    setProperty(kIOAudioStreamAvailableFormatsKey, availableFormatDictionaries);
    
	// This needs to change to passing up a token rather than the "this" pointer.
	streamID = engine->getNextStreamID (this);
    setProperty(kIOAudioStreamIDKey, streamID, sizeof(UInt32)*8);
//    setProperty(kIOAudioStreamIDKey, (UInt32)this, sizeof(UInt32)*8);
    
    streamAvailable = true;
    setProperty(kIOAudioStreamAvailableKey, (UInt8)1, sizeof(UInt8)*8);
    
    numClients = 0;
    updateNumClients();
    
    resetClipInfo();
    
    clientBufferListStart = NULL;
    clientBufferListEnd = NULL;
    
    userClientList = NULL;
    
    audioIOFunctions = NULL;
    numIOFunctions = false;
    
    streamAllocatedMixBuffer = false;

    workLoop->addEventSource(commandGate);

    return true;
}

void IOAudioStream::free()
{
    if (availableFormatDictionaries) {
        availableFormatDictionaries->release();
        availableFormatDictionaries = NULL;
    }
    
    if (mixBuffer && streamAllocatedMixBuffer) {
        IOFreeData(mixBuffer, mixBufferSize);
        mixBuffer = NULL;
        mixBufferSize = 0;
    }
    
    if (defaultAudioControls) {
        removeDefaultAudioControls();
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
    
    if (streamIOLock) {
        IORecursiveLockFree(streamIOLock);
        streamIOLock = NULL;
    }
    
    if (audioIOFunctions && (numIOFunctions > 0)) {
        IODelete(audioIOFunctions, AudioIOFunction, numIOFunctions);
        audioIOFunctions = NULL;
        numIOFunctions = 0;
    }
    
    if (availableFormats && (numAvailableFormats > 0)) {
        UInt32 formatNum;
        
        for (formatNum = 0; formatNum < numAvailableFormats; formatNum++) {
            if (availableFormats[formatNum].ioFunctionList && (availableFormats[formatNum].numIOFunctions > 0)) {
                IODelete(availableFormats[formatNum].ioFunctionList,
						 AudioIOFunction, availableFormats[formatNum].numIOFunctions);
            }
        }
        
        IODelete(availableFormats, IOAudioStreamFormatDesc, numAvailableFormats);
        availableFormats = NULL;
        numAvailableFormats = 0;
    }

	if (reserved) {
		IOFreeType (reserved, ExpansionData);
	}

    super::free();
}

void IOAudioStream::stop(IOService *provider)
{
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }
        
        commandGate->release();
        commandGate = NULL;
    }
    
    super::stop(provider);
}

IOWorkLoop *IOAudioStream::getWorkLoop() const
{	
    return workLoop;
}

IOReturn IOAudioStream::setProperties(OSObject *properties)
{
    OSDictionary *props;
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioStream[%p]::setProperties(%p)\n", this, properties);

    if (properties && (props = OSDynamicCast(OSDictionary, properties))) {
        OSCollectionIterator *iterator;
        OSObject *iteratorKey;

        iterator = OSCollectionIterator::withCollection(props);
        if (iterator) {
            while ( (iteratorKey = iterator->getNextObject()) ) {
                OSSymbol *key;

                key = OSDynamicCast(OSSymbol, iteratorKey);
                if (key && key->isEqualTo(kIOAudioStreamFormatKey)) {
                    OSDictionary *formatDict = OSDynamicCast(OSDictionary, props->getObject(key));
                    if (formatDict) {
                        assert(workLoop);													// <rdar://8568040,8691669>
						result = workLoop->runAction(_setFormatAction, this, formatDict);	// <rdar://8568040,8691669>
                    }
                }
            }
            iterator->release();
        } else {
            result = kIOReturnError;
        }
    } else {
        result = kIOReturnBadArgument;
    }

    audioDebugIOLog(3, "- IOAudioStream[%p]::setProperties(%p) returns 0x%lX\n", this, properties, (long unsigned int)result );
    return result;
}

void IOAudioStream::setDirection(IOAudioStreamDirection dir)
{
    direction = dir;
    setProperty(kIOAudioStreamDirectionKey, direction, 8);
}

IOAudioStreamDirection IOAudioStream::getDirection()
{
    return direction;
}

void IOAudioStream::setSampleBuffer(void *buffer, UInt32 size)
{
    lockStreamForIO();
    
    sampleBuffer = buffer;

    if (sampleBuffer) {
        sampleBufferSize = size;
        bzero(sampleBuffer, sampleBufferSize);
    } else {
        sampleBufferSize = 0;
    }
    
    unlockStreamForIO();
}

void *IOAudioStream::getSampleBuffer()
{
    return sampleBuffer;
}

UInt32 IOAudioStream::getSampleBufferSize()
{
    return sampleBufferSize;
}

void IOAudioStream::setMixBuffer(void *buffer, UInt32 size)
{
    lockStreamForIO();
      
    if (mixBuffer && streamAllocatedMixBuffer) {
        IOFreeData(mixBuffer, mixBufferSize);
        mixBuffer = NULL;
        mixBufferSize = 0;
        streamAllocatedMixBuffer = false;
    }
    
    mixBuffer = buffer;

    if (mixBuffer) {
        mixBufferSize = size;
        bzero(mixBuffer, mixBufferSize);
    } else {
        mixBufferSize = 0;
    }
    
    unlockStreamForIO();
}

void *IOAudioStream::getMixBuffer()
{
    return mixBuffer;
}

UInt32 IOAudioStream::getMixBufferSize()
{
    return mixBufferSize;
}

void IOAudioStream::numSampleFramesPerBufferChanged()
{
    if (mixBuffer && streamAllocatedMixBuffer) {
        setMixBuffer(NULL, 0);
    }
}

void IOAudioStream::clearSampleBuffer()
{
	lockStreamForIO();
    if (sampleBuffer && (sampleBufferSize > 0)) {
        bzero(sampleBuffer, sampleBufferSize);
    }
    
    if (mixBuffer && (mixBufferSize > 0)) {
        bzero(mixBuffer, mixBufferSize);
    }
	unlockStreamForIO();
}

void IOAudioStream::setIOFunction(AudioIOFunction ioFunction)
{
    setIOFunctionList(&ioFunction, 1);
}

void IOAudioStream::setIOFunctionList(const AudioIOFunction *ioFunctionList, UInt32 numFunctions)
{
    lockStreamForIO();

    if (audioIOFunctions && (numIOFunctions > 0)) {
        IODelete(audioIOFunctions, AudioIOFunction, numIOFunctions);
        audioIOFunctions = NULL;
        numIOFunctions = 0;
    }
    
    if (ioFunctionList && (numFunctions != 0)) {
        audioIOFunctions = IONew(AudioIOFunction, numFunctions);
        if (audioIOFunctions) {
            memcpy(audioIOFunctions, ioFunctionList, numFunctions * sizeof(AudioIOFunction));
            numIOFunctions = numFunctions;
        }
    }

    unlockStreamForIO();
}

const IOAudioStreamFormat *IOAudioStream::getFormat()
{
    return &format;
}

// <rdar://8568040,8691669>
IOReturn IOAudioStream::_setFormatAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioStream *stream = OSDynamicCast(IOAudioStream, target);
        if (stream) {
            if (stream->commandGate) {
                result = stream->commandGate->runAction(setFormatAction, arg0, arg1, arg2, arg3);
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioStream::setFormatAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioStream *audioStream = OSDynamicCast(IOAudioStream, owner);
        if (audioStream) {
            result = audioStream->setFormat((OSDictionary *)arg1);
        }
    }
    
    return result;
}

IOReturn IOAudioStream::setFormat(const IOAudioStreamFormat *streamFormat, bool callDriver)
{
    return setFormat(streamFormat, (IOAudioStreamFormatExtension *)NULL, callDriver);
}

IOReturn IOAudioStream::setFormat(OSDictionary *formatDict)
{
    IOReturn result = kIOReturnSuccess;

    if (formatDict) {
        IOAudioStreamFormat streamFormat;
		IOAudioStreamFormatExtension formatExtension;
        if (createFormatFromDictionary(formatDict, &streamFormat, &formatExtension)) {
            result = setFormat(&streamFormat, &formatExtension, formatDict);
        } else {
            result = kIOReturnBadArgument;
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

IOReturn IOAudioStream::setFormat(const IOAudioStreamFormat *streamFormat, OSDictionary *formatDict, bool callDriver)
{
	return setFormat(streamFormat, NULL, formatDict, callDriver);
}

IOReturn IOAudioStream::hardwareFormatChanged(const IOAudioStreamFormat *streamFormat)
{
	assert(reserved);
    return setFormat(streamFormat, &reserved->streamFormatExtension, false);
}

void IOAudioStream::addAvailableFormat(const IOAudioStreamFormat *streamFormat, const IOAudioSampleRate *minRate, const IOAudioSampleRate *maxRate, AudioIOFunction ioFunction)
{
    addAvailableFormat(streamFormat, NULL, minRate, maxRate, &ioFunction, 1);
}

void IOAudioStream::addAvailableFormat(const IOAudioStreamFormat *streamFormat, const IOAudioSampleRate *minRate, const IOAudioSampleRate *maxRate, const AudioIOFunction *ioFunctionList, UInt32 numFunctions)
{
    addAvailableFormat(streamFormat, NULL, minRate, maxRate, ioFunctionList, numFunctions);
}

void IOAudioStream::clearAvailableFormats()
{
	OSArray*	oldAvailableFormats;
	OSArray*	clearedAvailableFormats;
	
    assert(availableFormatDictionaries);
	
	oldAvailableFormats = availableFormatDictionaries;
	
    clearedAvailableFormats = OSArray::withCapacity(1);
    if (!clearedAvailableFormats) {
        return;
    }
	availableFormatDictionaries = clearedAvailableFormats;
    setProperty(kIOAudioStreamAvailableFormatsKey, availableFormatDictionaries);
	
    oldAvailableFormats->release();

	//	<rdar://9059646> Clean up the available formats array.
	if (availableFormats && (numAvailableFormats > 0)) {
		IODelete(availableFormats, IOAudioStreamFormatDesc, numAvailableFormats);
	}
	availableFormats = NULL;
	numAvailableFormats = 0;
}

bool IOAudioStream::validateFormat(IOAudioStreamFormat *streamFormat, IOAudioStreamFormatDesc *formatDesc)
{
	return validateFormat(streamFormat, NULL, formatDesc);
}

UInt32 IOAudioStream::getStartingChannelID()
{
    return startingChannelID;
}

UInt32 IOAudioStream::getMaxNumChannels()
{
    return maxNumChannels;
}

void IOAudioStream::setStartingChannelNumber(UInt32 channelNumber)
{
    setProperty(kIOAudioStreamStartingChannelNumberKey, channelNumber, sizeof(UInt32)*8);
}

void IOAudioStream::updateNumClients()
{
    setProperty(kIOAudioStreamNumClientsKey, numClients, sizeof(UInt32)*8);
}

IOReturn IOAudioStream::addClient(IOAudioClientBuffer *clientBuffer)
{
    IOReturn result = kIOReturnBadArgument;
	
    audioDebugIOLog(3, "+ IOAudioStream[%p]::addClient(%p)\n", this, clientBuffer);

    if (clientBuffer) {
        assert(clientBuffer->audioStream == this);

        lockStreamForIO();
        
        // <rdar://11731381> Make sure this buffer is not in the list
		bool bufferInList = false;
        if ((clientBuffer->nextClip == NULL) && (clientBuffer->previousClip == NULL) && (clientBuffer != clientBufferListStart) && (clientBuffer->nextClient == NULL) && (clientBuffer != userClientList)) {
            
			// <rdar://11731381> Make sure that the clientBuffer is not at the end of the list.
			IOAudioClientBuffer *tmpClientBuffer = userClientList;
			while (tmpClientBuffer && (tmpClientBuffer != clientBuffer)) {
				tmpClientBuffer = tmpClientBuffer->nextClient;
			}
			if (tmpClientBuffer) {
				audioDebugIOLog(3, "   clientBuffer %p is already in the list.\n", tmpClientBuffer);
				bufferInList = true;
			}
		}
		else {
			audioDebugIOLog(3, "   unexpected clientBuffer values (%p, %p, %p, %p, %p, %p)\n", clientBuffer->nextClip, clientBuffer->previousClip, clientBuffer, clientBufferListStart, clientBuffer->nextClient, userClientList );
			bufferInList = true;
		}
		
		if (!bufferInList) {	// <rdar://11731381>
			
            // It's OK to allow a new client if this is a mixable format
            // or if its not mixable but we don't have any clients
            // or if we are an input stream
            if (format.fIsMixable || (numClients == 0) || (getDirection() == kIOAudioStreamDirectionInput)) {
                numClients++;
                updateNumClients();
                
                clientBuffer->nextClient = userClientList;
                userClientList = clientBuffer;
                
                if (getDirection() == kIOAudioStreamDirectionOutput) {
                
                    clientBuffer->mixedPosition.fLoopCount = 0;
                    clientBuffer->mixedPosition.fSampleFrame = 0;
                    
                    clientBuffer->previousClip = NULL;
                    clientBuffer->nextClip = NULL;
                    
                    if (!mixBuffer && format.fIsMixable && sampleBuffer && (sampleBufferSize > 0)) {
                        assert(audioEngine);
                        
                        UInt32 mixBufSize = format.fNumChannels * kIOAudioEngineDefaultMixBufferSampleSize * audioEngine->numSampleFramesPerBuffer;
                        
                        if (mixBufSize > 0) {
                            void *mixBuf = IOMallocData(mixBufSize);
                            if (mixBuf) {
                                setMixBuffer(mixBuf, mixBufSize);
                                streamAllocatedMixBuffer = true;
                            }
                        }
                    }
                }
                
                result = kIOReturnSuccess;
            } else {
				audioDebugIOLog(3, "   clientBuffer invalid (%d, %d, %d).\n", format.fIsMixable, numClients, getDirection());
                result = kIOReturnExclusiveAccess;
            }
        }
		else {
			// <rdar://13412666> Return success if the buffers are already registered
			result = kIOReturnSuccess;
		}
        
        unlockStreamForIO();
    }
    
    audioDebugIOLog(3, "- IOAudioStream[%p]::addClient(%p) returns 0x%lX\n", this, clientBuffer, (long unsigned int)result );
    return result;
}

void IOAudioStream::removeClient(IOAudioClientBuffer *clientBuffer)
{
    audioDebugIOLog(3, "+ IOAudioStream[%p]::removeClient(%p)\n", this, clientBuffer);

    if (clientBuffer) {
        IOAudioClientBuffer *tmpClientBuffer, *previousClientBuffer = NULL;
        
        assert(clientBuffer->audioStream == this);
        
        lockStreamForIO();
        
        tmpClientBuffer = userClientList;
        while (tmpClientBuffer && (tmpClientBuffer != clientBuffer)) {
            previousClientBuffer = tmpClientBuffer;
            tmpClientBuffer = tmpClientBuffer->nextClient;
        }
        
        if (tmpClientBuffer) {
            if (previousClientBuffer) {
                previousClientBuffer->nextClient = tmpClientBuffer->nextClient;
            } else {
                assert(tmpClientBuffer == userClientList);
                userClientList = tmpClientBuffer->nextClient;
            }
            
            tmpClientBuffer->nextClient = NULL;

            numClients--;
            updateNumClients();
        }
        
        // Make sure the buffer is in the list
        if ((clientBuffer->nextClip != NULL) || (clientBuffer->previousClip != NULL) || (clientBuffer == clientBufferListStart)) {
            if (getDirection() == kIOAudioStreamDirectionOutput) {
                if (numClients == 0) {
                    resetClipInfo();
                }
                
                if (clientBuffer->previousClip != NULL) {
                    clientBuffer->previousClip->nextClip = clientBuffer->nextClip;
                }
                
                if (clientBuffer->nextClip != NULL) {
                    clientBuffer->nextClip->previousClip = clientBuffer->previousClip;
                }
                
                if (clientBufferListEnd == clientBuffer) {
                    assert(clientBuffer->nextClip == NULL);
                    clientBufferListEnd = clientBuffer->previousClip;
                }
                
                if (clientBufferListStart == clientBuffer) {
                    assert(clientBuffer->previousClip == NULL);
                    clientBufferListStart = clientBuffer->nextClip;
                    if (clientBufferListStart != NULL) {
                        clipIfNecessary();
                    }
                }
            }
        }
        
		// clear these values for bug 2851917
		clientBuffer->previousClip = NULL;
		clientBuffer->nextClip = NULL;
		clientBuffer->nextClient = NULL;
		unlockStreamForIO();
		
		//<rdar://problem/10305944>  dump any streaming errors
	    safeLogError(kErrorLogDumpCounters,0,0,0,0,0,0); // dump the counters 
    }
	
    audioDebugIOLog(3, "- IOAudioStream[%p]::removeClient(%p)\n", this, clientBuffer);
	return;
}

UInt32 IOAudioStream::getNumClients()
{
    return numClients;
}

void dumpList(IOAudioClientBuffer *start)
{
    IOAudioClientBuffer *tmp;
    
    tmp = start;
    while (tmp) {
        audioDebugIOLog(3, "  (%lx,%lx)\n", (long unsigned int)tmp->mixedPosition.fLoopCount, (long unsigned int)tmp->mixedPosition.fSampleFrame);
        tmp = tmp->nextClip;
    }
}

void validateList(IOAudioClientBuffer *start)
{
    IOAudioClientBuffer *tmp;
    
    tmp = start;
    while (tmp) {
        if (tmp->nextClip && (CMP_IOAUDIOENGINEPOSITION(&tmp->mixedPosition, &tmp->nextClip->mixedPosition) > 0)) {
            audioDebugIOLog(3, "+-IOAudioStream: ERROR - client buffer list not sorted!\n");
            dumpList(start);
            break;
        }
        tmp = tmp->nextClip;
    }
}

IOReturn IOAudioStream::readInputSamples(IOAudioClientBuffer *clientBuffer, UInt32 firstSampleFrame)
{
    IOReturn result = kIOReturnError;
    
    assert(audioEngine);
    assert(getDirection() == kIOAudioStreamDirectionInput);
    assert(reserved);
	
    if (clientBuffer) {
        UInt32 numWrappedFrames = 0;
        UInt32 numReadFrames = 0;
        UInt32 numSampleFramesPerBuffer;
		
        numSampleFramesPerBuffer = audioEngine->getNumSampleFramesPerBuffer();
        
        if ((firstSampleFrame + clientBuffer->numSampleFrames) > numSampleFramesPerBuffer) {
            numWrappedFrames = clientBuffer->numSampleFrames - (numSampleFramesPerBuffer - firstSampleFrame);
        }
        
        if (audioIOFunctions && (numIOFunctions != 0)) {
            UInt32 functionNum;
            
            for (functionNum = 0; functionNum < numIOFunctions; functionNum++) {
                if (audioIOFunctions[functionNum]) {
                    result = audioIOFunctions[functionNum](sampleBuffer, clientBuffer->sourceBuffer, firstSampleFrame, clientBuffer->numSampleFrames - numWrappedFrames, &format, this);
                    if (result != kIOReturnSuccess) {
                        break;
                    }
                }
            }
            
            if (numWrappedFrames > 0) {
                for (functionNum = 0; functionNum < numIOFunctions; functionNum++) {
                    if (audioIOFunctions[functionNum]) {
                        result = audioIOFunctions[functionNum](sampleBuffer, &((float *)clientBuffer->sourceBuffer)[(numSampleFramesPerBuffer - firstSampleFrame) * format.fNumChannels], 0, numWrappedFrames, &format, this);
                        if (result != kIOReturnSuccess) {
                            break;
                        }
                    }
                }
            }
        } else {
			numReadFrames = clientBuffer->numSampleFrames - numWrappedFrames;
            // numReadFrames passed by reference, value may or may not be modified by the engine.
            result = audioEngine->convertInputSamplesVBR(sampleBuffer, clientBuffer->sourceBuffer, firstSampleFrame, numReadFrames, &format, this);
			// override the default value set before this call with driver actual value
            reserved->mSampleFramesReadByEngine = numReadFrames;

			if ((result == kIOReturnSuccess) && (numWrappedFrames > 0)) {
				numReadFrames = numWrappedFrames;
				if (format.fIsMixable) {	// <rdar://8572755>
					// Use float format to compute offset for destination buffer
                result = audioEngine->convertInputSamplesVBR(sampleBuffer, &((float *)clientBuffer->sourceBuffer)[(numSampleFramesPerBuffer - firstSampleFrame) * format.fNumChannels], 0, numReadFrames, &format, this);
				} else {
					// Use native format to compute offset for destination buffer
					result = audioEngine->convertInputSamplesVBR(sampleBuffer, ((UInt8 *)clientBuffer->sourceBuffer) + ((numSampleFramesPerBuffer - firstSampleFrame) * format.fNumChannels * (format.fBitWidth / 8)), 0, numReadFrames, &format, this);
				}
				reserved->mSampleFramesReadByEngine += numReadFrames;
            }
        }
    } else {
        result = kIOReturnBadArgument;
    }
    
    return result;
}

IOReturn IOAudioStream::processOutputSamples(IOAudioClientBuffer *clientBuffer, UInt32 firstSampleFrame, UInt32 loopCount, bool samplesAvailable)
{
    IOReturn result = kIOReturnSuccess;
	
    //audioDebugIOLog(6, "IOAudioStream[%p]::processOutputSamples(%p, 0x%lx)\n", this, clientBuffer, firstSampleFrame);
    //audioDebugIOLog(6, "m(%lx,%lx,%lx)\n", loopCount, firstSampleFrame, clientBuffer->numSampleFrames);

    assert(direction == kIOAudioStreamDirectionOutput);
    if (clientBuffer) {
        // We can go ahead if we have a mix buffer or if the format is not mixable
        if (mixBuffer || !format.fIsMixable) {
            UInt32 numSampleFramesPerBuffer = audioEngine->getNumSampleFramesPerBuffer();
            UInt32 nextSampleFrame = 0;
            UInt32 mixBufferWrapped = false;
            UInt32 numSamplesToMix = 0;
            IOAudioClientBuffer *tmpBuf = NULL;
                    
            assert(audioEngine);
        

            
            // If we haven't mixed any samples for this client yet,
            // we have to figure out which loop those samples belong to
            if (IOAUDIOENGINEPOSITION_IS_ZERO(&clientBuffer->mixedPosition)) {
                clientBuffer->mixedPosition.fSampleFrame = firstSampleFrame;
                clientBuffer->mixedPosition.fLoopCount = loopCount;
            } else {
                // If firstSampleFrame is not the same as the previous mixed position sample frame,
                // then adjust it to the firstSampleFrame - looping if necessary
                if ((clientBuffer->mixedPosition.fSampleFrame != firstSampleFrame) || (clientBuffer->mixedPosition.fLoopCount != loopCount)) {
                    audioDebugIOLog(6, "IOAudioStream[%p]::processOutputSamples(%p) - Mix start position (%lx,%lx) is not previous mixed position (%lx,%lx)\n", 
										this, 
										clientBuffer, 
										(long unsigned int)loopCount, 
										(long unsigned int)firstSampleFrame, 
										(long unsigned int)clientBuffer->mixedPosition.fLoopCount, 
										(long unsigned int)clientBuffer->mixedPosition.fSampleFrame);
                    clientBuffer->mixedPosition.fLoopCount = loopCount;
                    clientBuffer->mixedPosition.fSampleFrame = firstSampleFrame;
                }

                // Check to see if the first sample frame is more than one buffer behind the last mixed position
                // of all of the buffers.  We need to deal with the case where we didn't get any samples
                // for this buffer for more than a buffer cycle.  In that case, we need to jump to 
                // the loop that the last buffer is on.  This assumes that a client never gets more than one
                // buffer cycle ahead of the playback head
                if ((clientBuffer != clientBufferListEnd) &&
                    (clientBufferListEnd != NULL) &&
                    ((clientBufferListEnd->mixedPosition.fLoopCount > (clientBuffer->mixedPosition.fLoopCount + 1)) ||
                     ((clientBufferListEnd->mixedPosition.fLoopCount == (clientBuffer->mixedPosition.fLoopCount + 1)) &&
                      (clientBufferListEnd->mixedPosition.fSampleFrame > clientBuffer->mixedPosition.fSampleFrame)))) {
                    // Adjust the loop count to be on the loop before the last mixed position
                    if (clientBuffer->mixedPosition.fSampleFrame > clientBufferListEnd->mixedPosition.fSampleFrame) {
                        clientBuffer->mixedPosition.fLoopCount = clientBufferListEnd->mixedPosition.fLoopCount - 1;
                    } else {
                        clientBuffer->mixedPosition.fLoopCount = clientBufferListEnd->mixedPosition.fLoopCount;
                    }
					audioDebugIOLog(6, "IOAudioStream[%p]::processOutputSamples(%p) - more than one buffer behind (%lx,%lx) adjusting to (%lx,%lx)\n", 
										this, 
										clientBuffer, 
										(long unsigned int)clientBufferListEnd->mixedPosition.fLoopCount, 
										(long unsigned int)clientBufferListEnd->mixedPosition.fSampleFrame, 
										(long unsigned int)clientBuffer->mixedPosition.fLoopCount, 
										(long unsigned int)firstSampleFrame);
					//dumpList(clientBufferListStart);
                }
            }
            
            // If we've already clipped, we need to verify all of the samples are after the clipped position
            // Those that are not will be discarded - they can't be played
            if (!IOAUDIOENGINEPOSITION_IS_ZERO(&clippedPosition)) {
                if (clientBuffer->mixedPosition.fLoopCount == clippedPosition.fLoopCount) {
                    if (clientBuffer->mixedPosition.fSampleFrame < clippedPosition.fSampleFrame) {
                        audioEngine->resetClipPosition(this, clientBuffer->mixedPosition.fSampleFrame);

#ifdef DEBUG
                        UInt32 samplesMissed;
                        samplesMissed = clippedPosition.fSampleFrame - clientBuffer->mixedPosition.fSampleFrame;
                        audioDebugIOLog(6, "IOAudioStream[%p]::processOutputSamples(%p) - Reset clip position (%lx,%lx)->(%lx,%lx) - %lx samples.\n", 
										this, 
										clientBuffer, 
										(long unsigned int)clippedPosition.fLoopCount, 
										(long unsigned int)clippedPosition.fSampleFrame, 
										(long unsigned int)clientBuffer->mixedPosition.fLoopCount, 
										(long unsigned int)clientBuffer->mixedPosition.fSampleFrame, 
										(long unsigned int)samplesMissed);
#endif

                        clippedPosition = clientBuffer->mixedPosition;
                    }
                } else if (clientBuffer->mixedPosition.fLoopCount < clippedPosition.fLoopCount) {
                    audioEngine->resetClipPosition(this, clientBuffer->mixedPosition.fSampleFrame);
                    
#ifdef DEBUG
                    UInt32 samplesMissed;
                    samplesMissed = (clippedPosition.fLoopCount - clientBuffer->mixedPosition.fLoopCount - 1) * numSampleFramesPerBuffer;
                    samplesMissed += clippedPosition.fSampleFrame + numSampleFramesPerBuffer - clientBuffer->mixedPosition.fSampleFrame;
                    audioDebugIOLog(6, "IOAudioStream[%p]::processOutputSamples(%p) - Reset clip position (%lx,%lx)->(%lx,%lx) - %lx samples.\n", 
										this, 
										clientBuffer, 
										(long unsigned int)clippedPosition.fLoopCount, 
										(long unsigned int)clippedPosition.fSampleFrame, 
										(long unsigned int)clientBuffer->mixedPosition.fLoopCount, 
										(long unsigned int)clientBuffer->mixedPosition.fSampleFrame, 
										(long unsigned int)samplesMissed);
#endif

                    clippedPosition = clientBuffer->mixedPosition;
                }
            }
            
            // We only need to mix samples if there are samples available
            // If the watchdog timer was responsible for this call, then 
            // there won't be any samples, so there's no point in mixing
            // or resetting the clip position
            if (samplesAvailable) {
                numSamplesToMix = clientBuffer->numSampleFrames;
            }
            
            if (numSamplesToMix > 0) {
/*
#ifdef DEBUG
                UInt32 currentSampleFrame = audioEngine->getCurrentSampleFrame();
                if (currentSampleFrame > firstSampleFrame) {
                    if ((firstSampleFrame + clientBuffer->numSampleFrames) > currentSampleFrame) {
                        //audioDebugIOLog(6, "IOAudioStream[%p]::processOutputSamples(%p) - Error: Some samples already played: first=%lx num=%lx curr=%lx\n", this, clientBuffer, firstSampleFrame, clientBuffer->numSampleFrames, currentSampleFrame);
                        audioDebugIOLog(6, "mix() missed first=%lx num=%lx curr=%lx\n", firstSampleFrame, clientBuffer->numSampleFrames, currentSampleFrame);
                    }
                } else {
                    if ((clientBuffer->numSampleFrames + firstSampleFrame) > (currentSampleFrame + numSampleFramesPerBuffer)) {
                        //audioDebugIOLog(6, "IOAudioStream[%p]::processOutputSamples(%p) - Error: Some samples already played: first=%lx num=%lx curr=%lx\n", this, clientBuffer, firstSampleFrame, clientBuffer->numSampleFrames, currentSampleFrame);
                        audioDebugIOLog(6, "mix() missed first=%lx num=%lx curr=%lx\n", firstSampleFrame, clientBuffer->numSampleFrames, currentSampleFrame);
                    }
                }
#endif
*/

				// Check if the buffer wraps
				if (numSampleFramesPerBuffer > (firstSampleFrame + numSamplesToMix)) {	// No wrap
					if (format.fIsMixable) {
						if (numClients == 1) {
							result = mixOutputSamples (clientBuffer->sourceBuffer, mixBuffer, firstSampleFrame, numSamplesToMix, &format, this);
						} else {
							result = audioEngine->mixOutputSamples(clientBuffer->sourceBuffer, mixBuffer, firstSampleFrame, numSamplesToMix, &format, this);
						}
					} else {
						result = kIOReturnSuccess;
					}
					nextSampleFrame = firstSampleFrame + numSamplesToMix;
				} else {	// Buffer wraps around
					mixBufferWrapped = true;
					if (format.fIsMixable) {
						if (numClients == 1) {
							result = mixOutputSamples (clientBuffer->sourceBuffer, mixBuffer, firstSampleFrame, numSampleFramesPerBuffer - firstSampleFrame, &format, this);
						} else {
							result = audioEngine->mixOutputSamples(clientBuffer->sourceBuffer, mixBuffer, firstSampleFrame, numSampleFramesPerBuffer - firstSampleFrame, &format, this);
						}
					} else {
						result = kIOReturnSuccess;
					}
					if (result != kIOReturnSuccess) {
						IOLog("IOAudioStream::processOutputSamples - Error: 0x%x returned from audioEngine->mixOutputSamples\n", result);
					}
					nextSampleFrame = numSamplesToMix - (numSampleFramesPerBuffer - firstSampleFrame);
					if (format.fIsMixable) {
						if (numClients == 1) {
							result = mixOutputSamples (((float *)clientBuffer->sourceBuffer) + ((numSampleFramesPerBuffer - firstSampleFrame) * format.fNumChannels), mixBuffer, 0, nextSampleFrame, &format, this);
						} else {
							result = audioEngine->mixOutputSamples(((float *)clientBuffer->sourceBuffer) + ((numSampleFramesPerBuffer - firstSampleFrame) * format.fNumChannels), mixBuffer, 0, nextSampleFrame, &format, this);
						}
					} else {
						result = kIOReturnSuccess;
					}
				}
            
                if (result == kIOReturnSuccess) {
                    // Reset startingSampleFrame and startingLoopCount if we haven't clipped
                    // anything yet and this buffer mixed samples before the previous
                    // starting frame
                    if (IOAUDIOENGINEPOSITION_IS_ZERO(&clippedPosition)) {
                        if (IOAUDIOENGINEPOSITION_IS_ZERO(&startingPosition) ||
                            (clientBuffer->mixedPosition.fLoopCount < startingPosition.fLoopCount) ||
                            ((clientBuffer->mixedPosition.fLoopCount == startingPosition.fLoopCount) && (firstSampleFrame < startingPosition.fSampleFrame))) {
                            
                            startingPosition.fLoopCount = clientBuffer->mixedPosition.fLoopCount;
                            startingPosition.fSampleFrame = firstSampleFrame;
                        }
                    }
                } else {
                    audioErrorIOLog("IOAudioStream::processOutputSamples - Error: 0x%lx returned from audioEngine->mixOutputSamples\n", (long unsigned int)result);
                }
                
                if (mixBufferWrapped) {
                    clientBuffer->mixedPosition.fLoopCount++;
                }
                clientBuffer->mixedPosition.fSampleFrame = nextSampleFrame;
                
            } else {	// We missed all of the samples
                clientBuffer->mixedPosition.fSampleFrame += clientBuffer->numSampleFrames;
                if (clientBuffer->mixedPosition.fSampleFrame >= numSampleFramesPerBuffer) {
                    clientBuffer->mixedPosition.fSampleFrame -= numSampleFramesPerBuffer;
                    clientBuffer->mixedPosition.fLoopCount++;
                }
            }
            
            // If this buffer isn't in the list yet, then we look at the beginning of the list
            if ((clientBuffer->nextClip == NULL) && (clientBuffer->previousClip == NULL) && (clientBuffer != clientBufferListStart)) {
                // If the buffer has mixed past the first buffer in the list, then we can start at the beginning
                // If not, then tmpBuf is just NULL and we insert at the beginning
                if ((clientBufferListStart != NULL) && (CMP_IOAUDIOENGINEPOSITION(&clientBuffer->mixedPosition, &clientBufferListStart->mixedPosition) > 0)) {
                    tmpBuf = clientBufferListStart;
                }
            } else { // Otherwise, we look forward from the current position
                tmpBuf = clientBuffer;
            }
                
            // Add it to the beginning if the buffer is new and has not mixed past any other buffers
            if (tmpBuf == NULL) {
                assert(clientBuffer->nextClip == NULL);
                assert(clientBuffer->previousClip == NULL);
                
                clientBuffer->nextClip = clientBufferListStart;
                clientBufferListStart = clientBuffer;
                
                if (clientBuffer->nextClip == NULL) {
                    clientBufferListEnd = clientBuffer;
                } else {
                    clientBuffer->nextClip->previousClip = clientBuffer;
                }
            } else {
                //Find the insertion point for the new location for this buffer
                while ((tmpBuf->nextClip != NULL) && (CMP_IOAUDIOENGINEPOSITION(&clientBuffer->mixedPosition, &tmpBuf->nextClip->mixedPosition) > 0)) {
                    tmpBuf = tmpBuf->nextClip;
                }
            
                if (tmpBuf != clientBuffer) {
                    // If the buffer is to change position, move updated client buffer to its new sorted position
                    // First remove the client buffer from its current position
                    if (clientBuffer->previousClip != NULL) {
                        clientBuffer->previousClip->nextClip = clientBuffer->nextClip;
                    } else if (clientBuffer == clientBufferListStart) {	// If we don't have a previous clip set, we may be the starting entry
                        clientBufferListStart = clientBuffer->nextClip;
                    } // If we have don't have a previousClip set and are not the start, then this is the first time this buffer is being mixed
                    
                    if (clientBuffer->nextClip != NULL) {
                        clientBuffer->nextClip->previousClip = clientBuffer->previousClip;
                    } else if (clientBuffer == clientBufferListEnd) {	// If we don't have a next clip set, we may be the last entry
                        // We should never get here, because we only are moving this buffer forward
                        // and that is impossible if it is the last one
                        clientBufferListEnd = clientBuffer->previousClip;
                    } // If we don't have a next clip and are not the end, then this is the first time this buffer is being mixed
                    
                    // Insert it after tmpBuf
                    clientBuffer->nextClip = tmpBuf->nextClip;
                    clientBuffer->previousClip= tmpBuf;
                    tmpBuf->nextClip = clientBuffer;
                    if (clientBuffer->nextClip) {
                        clientBuffer->nextClip->previousClip = clientBuffer;
                    }
                    if (clientBuffer->nextClip == NULL) {
                        assert(clientBufferListEnd == tmpBuf);
                        clientBufferListEnd = clientBuffer;
                    }
                    
#ifdef DEBUG
                    validateList(clientBufferListStart);
#endif
                }
            }
            
            // We should attempt to clip if we mixed some samples of if we
            // were called as a result of the watchdog timer (indicated
            // by samplesAvailable being false)
            if ((numSamplesToMix > 0) || !samplesAvailable) {
				if (!format.fIsMixable) {
					mixBuffer = clientBuffer->sourceBuffer;
				}
				
				reserved->mClipOutputStatus = kIOReturnSuccess;
                
				clipIfNecessary();
				if (!format.fIsMixable) {
					mixBuffer = NULL;
				}
				
				// gets set based on IOAudioEngine::clipOutputSamples return value inside IOAudioStream::clipOutputSamples
				result = reserved->mClipOutputStatus;
            }
        } else {
            audioErrorIOLog("IOAudioStream::processOutputSamples - Internal Error: No mix buffer\n");
            result = kIOReturnError;
        }
    } else {
        result = kIOReturnBadArgument;
    }
    
    return result;
}

void IOAudioStream::resetClipInfo()
{
    startingPosition.fLoopCount = 0;
    startingPosition.fSampleFrame = 0;
    clippedPosition.fLoopCount = 0;
    clippedPosition.fSampleFrame = 0;
}

void IOAudioStream::clipIfNecessary()
{
    //audioDebugIOLog(6, "IOAudioStream[%p]::clipIfNecessary()\n", this);

    if (clientBufferListStart != NULL) {
        // Only try to clip if there is not an unmixed buffer
        if (!IOAUDIOENGINEPOSITION_IS_ZERO(&clientBufferListStart->mixedPosition)) {
        
            // Check to see if we've clipped any samples yet
            if (IOAUDIOENGINEPOSITION_IS_ZERO(&clippedPosition)) {
                clippedPosition = startingPosition;
            }
            
#ifdef DEBUG
            IOAudioClientBuffer *tmp;
            
            tmp = clientBufferListStart->nextClip;
            while (tmp) {
                if ((tmp->mixedPosition.fLoopCount > (clippedPosition.fLoopCount + 1)) ||
                    ((tmp->mixedPosition.fLoopCount == clippedPosition.fLoopCount) && 
                     (tmp->mixedPosition.fSampleFrame > clippedPosition.fSampleFrame))) {
                     
                    if (clientBufferListStart->mixedPosition.fSampleFrame > clippedPosition.fSampleFrame) {
                        if ((tmp->mixedPosition.fSampleFrame > clippedPosition.fSampleFrame) &&
                            (clientBufferListStart->mixedPosition.fSampleFrame > tmp->mixedPosition.fSampleFrame)) {
                            audioDebugIOLog(6, "IOAudioStream[%p]::clipIfNecessary() - Error: Clipping across future buffer boundary - glitching! (%lx,%lx)->(%lx,%lx) buf=(%lx,%lx)\n", 
											this, 
											(long unsigned int)clippedPosition.fLoopCount, 
											(long unsigned int)clippedPosition.fSampleFrame, 
											(long unsigned int)clientBufferListStart->mixedPosition.fLoopCount, 
											(long unsigned int)clientBufferListStart->mixedPosition.fSampleFrame, 
											(long unsigned int)tmp->mixedPosition.fLoopCount, 
											(long unsigned int)tmp->mixedPosition.fSampleFrame);
                            dumpList(clientBufferListStart);
                            break;
                        }
                    } else if (clippedPosition.fSampleFrame > clientBufferListStart->mixedPosition.fSampleFrame) {
                        if ((tmp->mixedPosition.fSampleFrame < clientBufferListStart->mixedPosition.fSampleFrame) ||
                            (tmp->mixedPosition.fSampleFrame > clippedPosition.fSampleFrame)) {
                            audioDebugIOLog(6, "IOAudioStream[%p]::clipIfNecessary() - Error: Clipping across future buffer boundary - glitching! (%lx,%lx)->(%lx,%lx) buf=(%lx,%lx)\n", 
											this, 
											(long unsigned int)clippedPosition.fLoopCount, 
											(long unsigned int)clippedPosition.fSampleFrame, 
											(long unsigned int)clientBufferListStart->mixedPosition.fLoopCount, 
											(long unsigned int)clientBufferListStart->mixedPosition.fSampleFrame, 
											(long unsigned int)tmp->mixedPosition.fLoopCount, 
											(long unsigned int)tmp->mixedPosition.fSampleFrame);
                            dumpList(clientBufferListStart);
                            break;
                        }
                    }
                }
                tmp = tmp->nextClip;
            }
#endif
            
            // Check to see if it is on the same loop as the starting position
            // If not, adjust it to the same loop
            if (((clientBufferListStart->mixedPosition.fLoopCount == (clippedPosition.fLoopCount + 1)) &&
                 (clientBufferListStart->mixedPosition.fSampleFrame >= clippedPosition.fSampleFrame)) ||
                (clientBufferListStart->mixedPosition.fLoopCount > (clippedPosition.fLoopCount + 1))) {
                safeLogError(kErrorLogClipMoreThanOneBufferAhead,clippedPosition.fLoopCount,(long unsigned int)clippedPosition.fSampleFrame,(long unsigned int) clientBufferListStart->mixedPosition.fLoopCount,(long unsigned int) clientBufferListStart->mixedPosition.fSampleFrame,0,0); //<rdar://problem/10305944> 
                if (clientBufferListStart->mixedPosition.fSampleFrame >= clippedPosition.fSampleFrame) {
                    clippedPosition.fLoopCount = clientBufferListStart->mixedPosition.fLoopCount;
                } else {
                    clippedPosition.fLoopCount = clientBufferListStart->mixedPosition.fLoopCount - 1;
                }
				safeLogError(kErrorLogClipMoreThanOneBufferAheadPart2,clippedPosition.fLoopCount,(long unsigned int)clippedPosition.fSampleFrame,0,0,0,0); //<rdar://problem/10305944> 
            }
            
			// Add a test to see if we'd be clipping more samples than delivered because the HAL might skip some samples around a loop increment
			// If the HAL skipped samples around a loop increment, then just start from where it wants to
			if (clientBufferListStart->mixedPosition.fLoopCount + 1 == clippedPosition.fLoopCount && (clientBufferListStart->numSampleFrames < audioEngine->getNumSampleFramesPerBuffer() - clippedPosition.fSampleFrame)) {
				clientBufferListStart->mixedPosition.fLoopCount = clippedPosition.fLoopCount;
				safeLogError(kErrorLogClipPositionIsOff,clientBufferListStart->numSampleFrames,(long int) audioEngine->getNumSampleFramesPerBuffer(),(long int) clippedPosition.fSampleFrame,0,0,0);
			}
/*
	static UInt32 lastSampleFrame;
	if (clippedPosition.fSampleFrame != lastSampleFrame) {
		audioDebugIOLog(3, "Family sample frames wrong %ld %ld\n", clippedPosition.fSampleFrame, lastSampleFrame);
	}
	lastSampleFrame = clippedPosition.fSampleFrame + (clientBufferListStart->mixedPosition.fSampleFrame - clippedPosition.fSampleFrame);
*/
			UInt32 numSamplesToClip;				//<rdar://problem/5994776>

			if (clientBufferListStart->mixedPosition.fLoopCount == clippedPosition.fLoopCount) {
                if (clientBufferListStart->mixedPosition.fSampleFrame > clippedPosition.fSampleFrame)  {
					 numSamplesToClip = clientBufferListStart->mixedPosition.fSampleFrame - clippedPosition.fSampleFrame;
					if (!format.fIsMixable) {
						if ( numSamplesToClip <= kMixBufferMaxSize ) { 	// <rdar://problem/5994776>
							clipOutputSamples(clippedPosition.fSampleFrame, numSamplesToClip );
						} else {
							reserved->mClipOutputStatus = kIOReturnOverrun;
#ifdef DEBUG
							audioDebugIOLog(6,"IOAudioStream[%p]::clipIfNecessary() clipOutputSamples clip too large for source buffer numSamplesToClip=%lu clientBufferListStart->numSampleFrames %lu\n", 
											this, (long unsigned int)numSamplesToClip, (long unsigned int)clientBufferListStart->numSampleFrames );
							IOLog("IOAudioStream::clipIfNecessary() clipOutputSamples clip too large for source buffer numSamplesToClip=%lu clientBufferListStart->numSampleFrames %lu\n",
											(long unsigned int)numSamplesToClip, (long unsigned int)clientBufferListStart->numSampleFrames );
#endif
						}
					} else {
						clipOutputSamples(clippedPosition.fSampleFrame, numSamplesToClip);
					}
                    clippedPosition.fSampleFrame = clientBufferListStart->mixedPosition.fSampleFrame;
                } else if (clientBufferListStart->mixedPosition.fSampleFrame < clippedPosition.fSampleFrame) {
					safeLogError(kErrorLogAlreadyClipped,(long unsigned int) clippedPosition.fLoopCount,(long unsigned int) clippedPosition.fSampleFrame,(long unsigned int) clientBufferListStart->mixedPosition.fLoopCount,(long unsigned int) clientBufferListStart->mixedPosition.fSampleFrame,0,0); //<rdar://problem/10305944> 
                }
            } else {	// Clip wraps around
                UInt32 numSampleFramesPerBuffer;
                
                assert(audioEngine);
                
                numSampleFramesPerBuffer = audioEngine->getNumSampleFramesPerBuffer();
				numSamplesToClip = numSampleFramesPerBuffer - clippedPosition.fSampleFrame;
				
				if (!format.fIsMixable) {
					if ( numSamplesToClip <= kMixBufferMaxSize) { // <rdar://problem/5994776>
						clipOutputSamples(clippedPosition.fSampleFrame, numSamplesToClip );
					} else {
						reserved->mClipOutputStatus = kIOReturnOverrun;
#ifdef DEBUG
						audioDebugIOLog(6,"IOAudioStream[%p]::clipIfNecessary() clipOutputSamples wrap clip too large for source buffer numSamplesToClip=%lu clientBufferListStart->numSampleFrames %lu\n", 
									this, (long unsigned int)numSamplesToClip, (long unsigned int)clientBufferListStart->numSampleFrames );					
						IOLog("IOAudioStream::clipIfNecessary() clipOutputSamples wrap clip too large for source buffer numSamplesToClip=%lu clientBufferListStart->numSampleFrames %lu\n",
									(long unsigned int)numSamplesToClip, (long unsigned int)clientBufferListStart->numSampleFrames );
#endif
					}
				} else {
					clipOutputSamples(clippedPosition.fSampleFrame, numSamplesToClip );
				}
				
				if (!format.fIsMixable) {
					UInt32 remainingSamplesToClip = (numSampleFramesPerBuffer - clippedPosition.fSampleFrame);		//<rdar://problem/5994776>
					
					// Move the mix buffer to where we left off because the clip routine always starts at the beginning of the source buffer,
					// but that's not the right place when we don't have a source buffer and are using the mixbuffer as a pseduo-source buffer.
					audioDebugIOLog(6,"IOAudioStream[%p]::clipIfNecessary() clipOutputSamples wrap  mixBuffer=%p remainingSamplesToClip=0x%lu clientBufferListStart->mixedPosition.fSampleFrame=%lu\n", 
									this, mixBuffer, (long unsigned int)remainingSamplesToClip, (long unsigned int)clientBufferListStart->mixedPosition.fSampleFrame );				
					if ( remainingSamplesToClip + clientBufferListStart->mixedPosition.fSampleFrame <= kMixBufferMaxSize) // <rdar://problem/5994776>
					{
						mixBuffer = (char *)mixBuffer + (remainingSamplesToClip * format.fNumChannels * (format.fBitWidth / 8));
						clipOutputSamples(0, clientBufferListStart->mixedPosition.fSampleFrame);
					} else {
						reserved->mClipOutputStatus = kIOReturnOverrun;
#ifdef DEBUG
						audioDebugIOLog(6,"IOAudioStream[%p]::clipIfNecessary() clipOutputSamples mixBufferOffset  too large for source buffer numSamplesToClip=%lu clientBufferListStart->numSampleFrames=%lu remainingSamplesToClip=%lu\n", 
									this, (long unsigned int)clientBufferListStart->mixedPosition.fSampleFrame, (long unsigned int)clientBufferListStart->numSampleFrames, (long unsigned int)remainingSamplesToClip );							
						IOLog("IOAudioStream::clipIfNecessary() clipOutputSamples mixBufferOffset  too large for source buffer numSamplesToClip=%lu clientBufferListStart->numSampleFrames=%lu remainingSamplesToClip=%lu\n",
									(long unsigned int)clientBufferListStart->mixedPosition.fSampleFrame, (long unsigned int)clientBufferListStart->numSampleFrames, (long unsigned int)remainingSamplesToClip );
#endif
					}
				} else {
					clipOutputSamples(0, clientBufferListStart->mixedPosition.fSampleFrame);	
				}
                clippedPosition = clientBufferListStart->mixedPosition;
            }
        }
    }
}

void IOAudioStream::clipOutputSamples(UInt32 firstSampleFrame, UInt32 numSampleFrames)
{
    IOReturn result = kIOReturnSuccess;
    
    //audioDebugIOLog(6, "IOAudioStream[%p]::clipOutputSamples(0x%lx, 0x%lx)\n", this, firstSampleFrame, numSampleFrames);
    //audioDebugIOLog(6, "c(%lx,%lx) %lx\n", firstSampleFrame, numSampleFrames, audioEngine->getCurrentSampleFrame());

    assert(direction == kIOAudioStreamDirectionOutput);
    assert(audioEngine);
    assert(reserved);
    
    if (!mixBuffer || !sampleBuffer) {
		safeLogError(kErrorLogClipBuffersAreNULL,(long unsigned int) firstSampleFrame,(long unsigned int) numSampleFrames,0,0,mixBuffer,sampleBuffer); //<rdar://problem/10305944> 
        return;
    }
    
/*
#ifdef DEBUG
    UInt32 currentSampleFrame = audioEngine->getCurrentSampleFrame();
    
    if (currentSampleFrame > firstSampleFrame) {
        if ((firstSampleFrame + numSampleFrames) > currentSampleFrame) {
            //audioDebugIOLog(6, "IOAudioStream[%p]::clipOutputSamples(0x%lx, 0x%lx) - too late for some samples - current position = 0x%lx.\n", this, firstSampleFrame, numSampleFrames, currentSampleFrame);
            audioDebugIOLog(6, "clip(%lx,%lx) missed curr=%lx.\n", firstSampleFrame, numSampleFrames, currentSampleFrame);
        }
    } else {
        if ((numSampleFrames + firstSampleFrame) > (currentSampleFrame + audioEngine->getNumSampleFramesPerBuffer())) {
            //audioDebugIOLog(6, "IOAudioStream[%p]::clipOutputSamples(0x%lx, 0x%lx) - too late for some samples - current position = 0x%lx.\n", this, firstSampleFrame, numSampleFrames, currentSampleFrame);
            audioDebugIOLog(6, "clip(%lx,%lx) missed curr=%lx.\n", firstSampleFrame, numSampleFrames, currentSampleFrame);
        }
    }
#endif
*/
    
    if (audioIOFunctions && (numIOFunctions != 0)) {
        UInt32 functionNum;
        
        for (functionNum = 0; functionNum < numIOFunctions; functionNum++) {
            if (audioIOFunctions[functionNum]) {
                result = audioIOFunctions[functionNum](mixBuffer, sampleBuffer, firstSampleFrame, numSampleFrames, &format, this);
                if (result != kIOReturnSuccess) {
                    break;
                }
            }
        }
    } else {
        result = audioEngine->clipOutputSamples(mixBuffer, sampleBuffer, firstSampleFrame, numSampleFrames, &format, this);
    }
    
    if (result != kIOReturnSuccess) {
		safeLogError(kErrorLogClipReturnsAnError,(long unsigned int) firstSampleFrame,(long unsigned int) numSampleFrames,result,0,0,0); //<rdar://problem/10305944> 
    }
	reserved->mClipOutputStatus = result;
}

void IOAudioStream::lockStreamForIO()
{
    assert(streamIOLock);
    
    IORecursiveLockLock(streamIOLock);
}

void IOAudioStream::unlockStreamForIO()
{
    assert(streamIOLock);
    
    IORecursiveLockUnlock(streamIOLock);
}

void IOAudioStream::setStreamAvailable(bool available)
{
    if (streamAvailable != available) {
        streamAvailable = available;
        setProperty(kIOAudioStreamAvailableKey, available ? 1 : 0, sizeof(UInt8)*8);
        
        assert(audioEngine);
        audioEngine->updateChannelNumbers();
    }
}

bool IOAudioStream::getStreamAvailable()
{
    return streamAvailable;
}

IOReturn IOAudioStream::addDefaultAudioControl(IOAudioControl *defaultAudioControl)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (defaultAudioControl) {
        UInt32 controlChannelID;
        
        if (defaultAudioControl->getChannelID() == kIOAudioControlChannelIDAll) {
            if (((getDirection() == kIOAudioStreamDirectionOutput) && (defaultAudioControl->getUsage() == kIOAudioControlUsageInput)) ||
                ((getDirection() == kIOAudioStreamDirectionInput) && (defaultAudioControl->getUsage() == kIOAudioControlUsageOutput))) {
                result = kIOReturnError;
                audioErrorIOLog("IOAudioStream::addDefaultAudioControl() - Error: invalid audio control - stream direction is opposite of control usage.\n");
                goto Done;
            }
            
            controlChannelID = defaultAudioControl->getChannelID();
            
            if ((controlChannelID != 0) && ((controlChannelID < startingChannelID) || (controlChannelID >= (startingChannelID + maxNumChannels)))) {
                result = kIOReturnError;
                audioErrorIOLog("IOAudioStream::addDefaultAudioControl() - Error: audio control channel is not in this stream.\n");
                goto Done;
            }
            
            if (defaultAudioControl->attachAndStart(this)) {
                if (!defaultAudioControls) {
                    defaultAudioControls = OSSet::withObjects((const OSObject **)&defaultAudioControl, 1, 1);
                } else {
                    defaultAudioControls->setObject(defaultAudioControl);
                }
            } else {
                result = kIOReturnError;
            }
        } else {	// Control for an individual channel - attach to audio engine instead
            assert(audioEngine);
            result = audioEngine->addDefaultAudioControl(defaultAudioControl);
        }
    }
   
Done:

    return result;
}

void IOAudioStream::removeDefaultAudioControls()
{
    if (defaultAudioControls) {
        if (!isInactive()) {
            OSCollectionIterator *controlIterator;
            
            controlIterator = OSCollectionIterator::withCollection(defaultAudioControls);
            
            if (controlIterator) {
                IOAudioControl *control;
                
                while ( (control = (IOAudioControl *)controlIterator->getNextObject()) ) {
					control->detach(this);					//	<rdar://14773236>

                    if (control->getProvider() == this) {
                        control->terminate();
                    }
                }
                
                controlIterator->release();
            }
        }
        
        defaultAudioControls->flushCollection();
    }
}
