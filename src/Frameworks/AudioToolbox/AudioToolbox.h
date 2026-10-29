// AudioToolbox.h - audio components, the HAL output unit, PCM converters, format queries and stream parsing
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef AUDIOTOOLBOX_H
#define AUDIOTOOLBOX_H

#include <CoreAudio/CoreAudio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueAudioComponent *AudioComponent;
typedef struct ComponentInstanceRecord *AudioComponentInstance;
typedef AudioComponentInstance AudioUnit;
typedef UInt32 AudioUnitPropertyID;
typedef UInt32 AudioUnitScope;
typedef UInt32 AudioUnitElement;
typedef UInt32 AudioUnitParameterID;
typedef Float32 AudioUnitParameterValue;
typedef UInt32 AudioUnitRenderActionFlags;

typedef struct AudioComponentDescription {
    OSType componentType;
    OSType componentSubType;
    OSType componentManufacturer;
    UInt32 componentFlags;
    UInt32 componentFlagsMask;
} AudioComponentDescription;

typedef OSStatus (*AURenderCallback)(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
                                     UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);
typedef void (*AudioUnitPropertyListenerProc)(void *inRefCon, AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope,
                                              AudioUnitElement inElement);

typedef struct AURenderCallbackStruct {
    AURenderCallback inputProc;
    void *inputProcRefCon;
} AURenderCallbackStruct;

enum {
    kAudioUnitType_Output = 'auou',
    kAudioUnitSubType_HALOutput = 'ahal',
    kAudioUnitSubType_DefaultOutput = 'def ',
    kAudioUnitSubType_SystemOutput = 'sys ',
    kAudioUnitSubType_VoiceProcessingIO = 'vpio',
    kAudioUnitManufacturer_Apple = 'appl',
};

enum {
    kAudioUnitScope_Global = 0,
    kAudioUnitScope_Input = 1,
    kAudioUnitScope_Output = 2,
};

enum {
    kAudioUnitProperty_ClassInfo = 0,
    kAudioUnitProperty_SampleRate = 2,
    kAudioUnitProperty_ParameterList = 3,
    kAudioUnitProperty_StreamFormat = 8,
    kAudioUnitProperty_ElementCount = 11,
    kAudioUnitProperty_Latency = 12,
    kAudioUnitProperty_MaximumFramesPerSlice = 14,
    kAudioUnitProperty_AudioChannelLayout = 19,
    kAudioUnitProperty_SetRenderCallback = 23,
    kAudioUnitProperty_LastRenderError = 22,
    kAudioUnitProperty_ShouldAllocateBuffer = 51,
    kAudioOutputUnitProperty_CurrentDevice = 2000,
    kAudioOutputUnitProperty_IsRunning = 2001,
    kAudioOutputUnitProperty_ChannelMap = 2002,
    kAudioOutputUnitProperty_EnableIO = 2003,
    kAudioOutputUnitProperty_StartTime = 2004,
    kAudioOutputUnitProperty_SetInputCallback = 2005,
    kAudioOutputUnitProperty_HasIO = 2006,
    kAudioUnitProperty_BufferFrameSize = 'fsiz',
};

enum {
    kHALOutputParam_Volume = 14,
};

enum {
    kAudioUnitRenderAction_OutputIsSilence = 1U << 4,
    kAudioUnitRenderAction_PreRender = 1U << 2,
    kAudioUnitRenderAction_PostRender = 1U << 3,
};

enum {
    kAudioUnitErr_InvalidProperty = -10879,
    kAudioUnitErr_InvalidParameter = -10878,
    kAudioUnitErr_InvalidElement = -10877,
    kAudioUnitErr_NoConnection = -10876,
    kAudioUnitErr_FailedInitialization = -10875,
    kAudioUnitErr_TooManyFramesToProcess = -10874,
    kAudioUnitErr_FormatNotSupported = -10868,
    kAudioUnitErr_Uninitialized = -10867,
    kAudioUnitErr_InvalidScope = -10866,
    kAudioUnitErr_PropertyNotWritable = -10865,
    kAudioUnitErr_CannotDoInCurrentContext = -10863,
    kAudioUnitErr_InvalidPropertyValue = -10851,
    kAudioUnitErr_PropertyNotInUse = -10850,
    kAudioUnitErr_Initialized = -10849,
    kAudioUnitErr_InvalidOfflineRender = -10848,
    kAudioComponentErr_InstanceInvalidated = -66749,
};

AudioComponent AudioComponentFindNext(AudioComponent inComponent, const AudioComponentDescription *inDesc);
OSStatus AudioComponentGetDescription(AudioComponent inComponent, AudioComponentDescription *outDesc);
OSStatus AudioComponentCopyName(AudioComponent inComponent, CFStringRef *outName);
UInt32 AudioComponentCount(const AudioComponentDescription *inDesc);
OSStatus AudioComponentInstanceNew(AudioComponent inComponent, AudioComponentInstance *outInstance);
OSStatus AudioComponentInstanceDispose(AudioComponentInstance inInstance);
AudioComponent AudioComponentInstanceGetComponent(AudioComponentInstance inInstance);

OSStatus AudioUnitInitialize(AudioUnit inUnit);
OSStatus AudioUnitUninitialize(AudioUnit inUnit);
OSStatus AudioUnitGetPropertyInfo(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement,
                                  UInt32 *outDataSize, Boolean *outWritable);
OSStatus AudioUnitGetProperty(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement,
                              void *outData, UInt32 *ioDataSize);
OSStatus AudioUnitSetProperty(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement,
                              const void *inData, UInt32 inDataSize);
OSStatus AudioUnitAddPropertyListener(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitPropertyListenerProc inProc, void *inProcUserData);
OSStatus AudioUnitRemovePropertyListenerWithUserData(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitPropertyListenerProc inProc,
                                                     void *inProcUserData);
OSStatus AudioUnitGetParameter(AudioUnit inUnit, AudioUnitParameterID inID, AudioUnitScope inScope, AudioUnitElement inElement,
                               AudioUnitParameterValue *outValue);
OSStatus AudioUnitSetParameter(AudioUnit inUnit, AudioUnitParameterID inID, AudioUnitScope inScope, AudioUnitElement inElement,
                               AudioUnitParameterValue inValue, UInt32 inBufferOffsetInFrames);
OSStatus AudioUnitRender(AudioUnit inUnit, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
                         UInt32 inOutputBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);
OSStatus AudioUnitReset(AudioUnit inUnit, AudioUnitScope inScope, AudioUnitElement inElement);
OSStatus AudioOutputUnitStart(AudioUnit ci);
OSStatus AudioOutputUnitStop(AudioUnit ci);

// music devices: no software synth is installed, so no component of this type exists yet
typedef AudioComponentInstance MusicDeviceComponent;

enum {
    kAudioUnitType_MusicDevice = 'aumu',
    kAudioUnitType_Mixer = 'aumx',
    kAudioUnitSubType_DLSSynth = 'dls ',
    kAudioUnitSubType_MIDISynth = 'msyn',
    kAudioUnitParameterFlag_Global = 1U << 0,
    kAudioUnitParameterFlag_Input = 1U << 1,
    kAudioUnitParameterFlag_Output = 1U << 2,
};

OSStatus MusicDeviceMIDIEvent(MusicDeviceComponent inUnit, UInt32 inStatus, UInt32 inData1, UInt32 inData2, UInt32 inOffsetSampleFrame);
OSStatus MusicDeviceSysEx(MusicDeviceComponent inUnit, const UInt8 *inData, UInt32 inLength);

// graphs of audio units, each node an instance opened by AUGraphOpen and connected output to input
typedef struct OpaqueAUGraph *AUGraph;
typedef SInt32 AUNode;

typedef struct AudioUnitNodeConnection {
    AUNode sourceNode;
    UInt32 sourceOutputNumber;
    AUNode destNode;
    UInt32 destInputNumber;
} AudioUnitNodeConnection;

enum {
    kAUGraphErr_NodeNotFound = -10860,
    kAUGraphErr_InvalidConnection = -10861,
    kAUGraphErr_OutputNodeErr = -10862,
    kAUGraphErr_CannotDoInCurrentContext = -10863,
    kAUGraphErr_InvalidAudioUnit = -10864,
};

OSStatus NewAUGraph(AUGraph *outGraph);
OSStatus DisposeAUGraph(AUGraph inGraph);
OSStatus AUGraphAddNode(AUGraph inGraph, const AudioComponentDescription *inDescription, AUNode *outNode);
OSStatus AUGraphRemoveNode(AUGraph inGraph, AUNode inNode);
OSStatus AUGraphGetNodeCount(AUGraph inGraph, UInt32 *outNumberOfNodes);
OSStatus AUGraphGetIndNode(AUGraph inGraph, UInt32 inIndex, AUNode *outNode);
OSStatus AUGraphNodeInfo(AUGraph inGraph, AUNode inNode, AudioComponentDescription *outDescription, AudioUnit *outAudioUnit);
OSStatus AUGraphConnectNodeInput(AUGraph inGraph, AUNode inSourceNode, UInt32 inSourceOutputNumber, AUNode inDestNode,
                                 UInt32 inDestInputNumber);
OSStatus AUGraphDisconnectNodeInput(AUGraph inGraph, AUNode inDestNode, UInt32 inDestInputNumber);
OSStatus AUGraphOpen(AUGraph inGraph);
OSStatus AUGraphClose(AUGraph inGraph);
OSStatus AUGraphInitialize(AUGraph inGraph);
OSStatus AUGraphUninitialize(AUGraph inGraph);
OSStatus AUGraphStart(AUGraph inGraph);
OSStatus AUGraphStop(AUGraph inGraph);
OSStatus AUGraphIsOpen(AUGraph inGraph, Boolean *outIsOpen);
OSStatus AUGraphIsInitialized(AUGraph inGraph, Boolean *outIsInitialized);
OSStatus AUGraphIsRunning(AUGraph inGraph, Boolean *outIsRunning);
OSStatus AUGraphUpdate(AUGraph inGraph, Boolean *outIsUpdated);

typedef struct OpaqueAudioConverter *AudioConverterRef;
typedef UInt32 AudioConverterPropertyID;
typedef OSStatus (*AudioConverterComplexInputDataProc)(AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets,
                                                       AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription,
                                                       void *inUserData);

enum {
    kAudioConverterPropertyMinimumInputBufferSize = 'mibs',
    kAudioConverterPropertyMinimumOutputBufferSize = 'mobs',
    kAudioConverterPropertyMaximumInputPacketSize = 'xips',
    kAudioConverterPropertyMaximumOutputPacketSize = 'xops',
    kAudioConverterCurrentInputStreamDescription = 'acid',
    kAudioConverterCurrentOutputStreamDescription = 'acod',
    kAudioConverterDecompressionMagicCookie = 'dmgc',
    kAudioConverterSampleRateConverterQuality = 'srcq',
    kAudioConverterSampleRateConverterComplexity = 'srca',
    kAudioConverterPrimeMethod = 'prmm',
    kAudioConverterPrimeInfo = 'prim',
    kAudioConverterInputChannelLayout = 'icl ',
    kAudioConverterOutputChannelLayout = 'ocl ',
    kAudioConverterChannelMap = 'chmp',
};

enum {
    kAudioConverterErr_FormatNotSupported = 'fmt?',
    kAudioConverterErr_OperationNotSupported = 0x6F703F3F,
    kAudioConverterErr_PropertyNotSupported = 'prop',
    kAudioConverterErr_InvalidInputSize = 'insz',
    kAudioConverterErr_InvalidOutputSize = 'otsz',
    kAudioConverterErr_BadPropertySizeError = '!siz',
    kAudioConverterErr_RequiresPacketDescriptionsError = '!pkd',
    kAudioConverterErr_InputSampleRateOutOfRange = '!isr',
    kAudioConverterErr_OutputSampleRateOutOfRange = '!osr',
};

OSStatus AudioConverterNew(const AudioStreamBasicDescription *inSourceFormat, const AudioStreamBasicDescription *inDestinationFormat,
                           AudioConverterRef *outAudioConverter);
OSStatus AudioConverterDispose(AudioConverterRef inAudioConverter);
OSStatus AudioConverterReset(AudioConverterRef inAudioConverter);
OSStatus AudioConverterGetPropertyInfo(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 *outSize,
                                       Boolean *outWritable);
OSStatus AudioConverterGetProperty(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 *ioPropertyDataSize,
                                   void *outPropertyData);
OSStatus AudioConverterSetProperty(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 inPropertyDataSize,
                                   const void *inPropertyData);
OSStatus AudioConverterFillComplexBuffer(AudioConverterRef inAudioConverter, AudioConverterComplexInputDataProc inInputDataProc,
                                         void *inInputDataProcUserData, UInt32 *ioOutputDataPacketSize, AudioBufferList *outOutputData,
                                         AudioStreamPacketDescription *outPacketDescription);
OSStatus AudioConverterConvertBuffer(AudioConverterRef inAudioConverter, UInt32 inInputDataSize, const void *inInputData,
                                     UInt32 *ioOutputDataSize, void *outOutputData);

typedef struct AudioQueueBuffer {
    const UInt32 mAudioDataBytesCapacity;
    void *const mAudioData;
    UInt32 mAudioDataByteSize;
    void *mUserData;
    const UInt32 mPacketDescriptionCapacity;
    AudioStreamPacketDescription *const mPacketDescriptions;
    UInt32 mPacketDescriptionCount;
} AudioQueueBuffer;
typedef AudioQueueBuffer *AudioQueueBufferRef;
typedef struct OpaqueAudioQueue *AudioQueueRef;
typedef UInt32 AudioQueuePropertyID;
typedef UInt32 AudioQueueParameterID;
typedef Float32 AudioQueueParameterValue;
typedef void (*AudioQueueOutputCallback)(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer);
typedef void (*AudioQueueInputCallback)(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer,
                                        const AudioTimeStamp *inStartTime, UInt32 inNumberPacketDescriptions,
                                        const AudioStreamPacketDescription *inPacketDescs);
typedef void (*AudioQueuePropertyListenerProc)(void *inUserData, AudioQueueRef inAQ, AudioQueuePropertyID inID);

enum {
    kAudioQueueParam_Volume = 1,
    kAudioQueueProperty_IsRunning = 'aqrn',
    kAudioQueueDeviceProperty_SampleRate = 'aqsr',
    kAudioQueueDeviceProperty_NumberChannels = 'aqdc',
    kAudioQueueProperty_CurrentDevice = 'aqcd',
    kAudioQueueProperty_ChannelLayout = 'aqcl',
    kAudioQueueProperty_StreamDescription = 'aqft',
    kAudioQueueErr_InvalidBuffer = -66687,
    kAudioQueueErr_BufferEmpty = -66686,
    kAudioQueueErr_InvalidProperty = -66683,
    kAudioQueueErr_InvalidPropertySize = -66682,
    kAudioQueueErr_InvalidParameter = -66680,
    kAudioQueueErr_InvalidDevice = -66679,
    kAudioQueueErr_InvalidRunState = -66671,
    kAudioQueueErr_EnqueueDuringReset = -66632,
};

// a queue made with a NULL run loop calls back on a thread of its own, as on macOS
OSStatus AudioQueueNewOutput(const AudioStreamBasicDescription *inFormat, AudioQueueOutputCallback inCallbackProc, void *inUserData,
                             CFRunLoopRef inCallbackRunLoop, CFStringRef inCallbackRunLoopMode, UInt32 inFlags,
                             AudioQueueRef *outAQ);
// there is no capture device yet, so an input queue is refused the way macOS refuses one without a microphone
OSStatus AudioQueueNewInput(const AudioStreamBasicDescription *inFormat, AudioQueueInputCallback inCallbackProc, void *inUserData,
                            CFRunLoopRef inCallbackRunLoop, CFStringRef inCallbackRunLoopMode, UInt32 inFlags,
                            AudioQueueRef *outAQ);
OSStatus AudioQueueSetProperty(AudioQueueRef inAQ, AudioQueuePropertyID inID, const void *inData, UInt32 inDataSize);
OSStatus AudioQueueDispose(AudioQueueRef inAQ, Boolean inImmediate);
OSStatus AudioQueueAllocateBuffer(AudioQueueRef inAQ, UInt32 inBufferByteSize, AudioQueueBufferRef *outBuffer);
OSStatus AudioQueueAllocateBufferWithPacketDescriptions(AudioQueueRef inAQ, UInt32 inBufferByteSize, UInt32 inNumberPacketDescriptions,
                                                       AudioQueueBufferRef *outBuffer);
OSStatus AudioQueueFreeBuffer(AudioQueueRef inAQ, AudioQueueBufferRef inBuffer);
OSStatus AudioQueueEnqueueBuffer(AudioQueueRef inAQ, AudioQueueBufferRef inBuffer, UInt32 inNumPacketDescs,
                                 const AudioStreamPacketDescription *inPacketDescs);
OSStatus AudioQueueStart(AudioQueueRef inAQ, const AudioTimeStamp *inStartTime);
OSStatus AudioQueuePrime(AudioQueueRef inAQ, UInt32 inNumberOfFramesToPrepare, UInt32 *outNumberOfFramesPrepared);
OSStatus AudioQueueStop(AudioQueueRef inAQ, Boolean inImmediate);
OSStatus AudioQueuePause(AudioQueueRef inAQ);
OSStatus AudioQueueFlush(AudioQueueRef inAQ);
OSStatus AudioQueueReset(AudioQueueRef inAQ);
OSStatus AudioQueueGetParameter(AudioQueueRef inAQ, AudioQueueParameterID inParamID, AudioQueueParameterValue *outValue);
OSStatus AudioQueueSetParameter(AudioQueueRef inAQ, AudioQueueParameterID inParamID, AudioQueueParameterValue inValue);
OSStatus AudioQueueGetProperty(AudioQueueRef inAQ, AudioQueuePropertyID inID, void *outData, UInt32 *ioDataSize);
OSStatus AudioQueueGetPropertySize(AudioQueueRef inAQ, AudioQueuePropertyID inID, UInt32 *outDataSize);
OSStatus AudioQueueAddPropertyListener(AudioQueueRef inAQ, AudioQueuePropertyID inID, AudioQueuePropertyListenerProc inProc,
                                       void *inUserData);
OSStatus AudioQueueRemovePropertyListener(AudioQueueRef inAQ, AudioQueuePropertyID inID, AudioQueuePropertyListenerProc inProc,
                                          void *inUserData);
OSStatus AudioQueueGetCurrentTime(AudioQueueRef inAQ, void *inTimeline, AudioTimeStamp *outTimeStamp, Boolean *outTimelineDiscontinuity);

typedef UInt32 AudioFormatPropertyID;

enum {
    kAudioFormatProperty_FormatInfo = 'fmti',
    kAudioFormatProperty_FormatName = 'fnam',
    kAudioFormatProperty_FormatIsVBR = 'fvbr',
    kAudioFormatProperty_FormatIsExternallyFramed = 'fexf',
    kAudioFormatProperty_FormatList = 'flst',
    kAudioFormatProperty_OutputFormatList = 'ofls',
    kAudioFormatProperty_DecodeFormatIDs = 'acif',
    kAudioFormatProperty_EncodeFormatIDs = 'acof',
    kAudioFormatProperty_ChannelLayoutForTag = 'cmpl',
    kAudioFormatProperty_NumberOfChannelsForLayout = 'nchm',
    kAudioFormatProperty_TagForChannelLayout = 'cmpt',
};

enum {
    kAudioFormatUnspecifiedError = 'what',
    kAudioFormatUnsupportedPropertyError = 'prop',
    kAudioFormatBadPropertySizeError = '!siz',
    kAudioFormatBadSpecifierSizeError = '!spc',
    kAudioFormatUnsupportedDataFormatError = 'fmt?',
    kAudioFormatUnknownFormatError = '!fmt',
};

OSStatus AudioFormatGetPropertyInfo(AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void *inSpecifier,
                                    UInt32 *outPropertyDataSize);
OSStatus AudioFormatGetProperty(AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void *inSpecifier,
                                UInt32 *ioPropertyDataSize, void *outPropertyData);

typedef struct OpaqueAudioFileStreamID *AudioFileStreamID;
typedef UInt32 AudioFileStreamPropertyID;
typedef UInt32 AudioFileTypeID;
typedef UInt32 AudioFileStreamParseFlags;
typedef UInt32 AudioFileStreamPropertyFlags;
typedef void (*AudioFileStream_PropertyListenerProc)(void *inClientData, AudioFileStreamID inAudioFileStream,
                                                     AudioFileStreamPropertyID inPropertyID, AudioFileStreamPropertyFlags *ioFlags);
typedef void (*AudioFileStream_PacketsProc)(void *inClientData, UInt32 inNumberBytes, UInt32 inNumberPackets, const void *inInputData,
                                            AudioStreamPacketDescription *inPacketDescriptions);

enum {
    kAudioFileStreamError_UnsupportedFileType = 'typ?',
    kAudioFileStreamError_UnsupportedDataFormat = 'fmt?',
    kAudioFileStreamError_UnsupportedProperty = 'pty?',
    kAudioFileStreamError_BadPropertySize = '!siz',
    kAudioFileStreamError_NotOptimized = 'optm',
    kAudioFileStreamError_InvalidPacketOffset = 'pck?',
    kAudioFileStreamError_InvalidFile = 'dta?',
    kAudioFileStreamError_ValueUnknown = 'unk?',
    kAudioFileStreamError_DataUnavailable = 'more',
    kAudioFileStreamError_IllegalOperation = 'nope',
};

OSStatus AudioFileStreamOpen(void *inClientData, AudioFileStream_PropertyListenerProc inPropertyListenerProc,
                             AudioFileStream_PacketsProc inPacketsProc, AudioFileTypeID inFileTypeHint, AudioFileStreamID *outAudioFileStream);
OSStatus AudioFileStreamParseBytes(AudioFileStreamID inAudioFileStream, UInt32 inDataByteSize, const void *inData,
                                   AudioFileStreamParseFlags inFlags);
OSStatus AudioFileStreamGetPropertyInfo(AudioFileStreamID inAudioFileStream, AudioFileStreamPropertyID inPropertyID,
                                        UInt32 *outPropertyDataSize, Boolean *outWritable);
OSStatus AudioFileStreamGetProperty(AudioFileStreamID inAudioFileStream, AudioFileStreamPropertyID inPropertyID,
                                    UInt32 *ioPropertyDataSize, void *outPropertyData);
OSStatus AudioFileStreamClose(AudioFileStreamID inAudioFileStream);

// PureDarwin: the PCM formats the converter and the output unit accept
Boolean PDAudioPCMFormatIsSupported(const AudioStreamBasicDescription *format);

#ifdef __cplusplus
}
#endif

#endif
