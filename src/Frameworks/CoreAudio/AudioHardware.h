// AudioHardware.h - the audio object properties of the system, its device and the device's stream
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef AUDIOHARDWARE_H
#define AUDIOHARDWARE_H

#include <CoreAudio/CoreAudioTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 AudioObjectID;
typedef AudioObjectID AudioDeviceID;
typedef AudioObjectID AudioStreamID;
typedef UInt32 AudioObjectPropertySelector;
typedef UInt32 AudioObjectPropertyScope;
typedef UInt32 AudioObjectPropertyElement;

typedef struct AudioObjectPropertyAddress {
    AudioObjectPropertySelector mSelector;
    AudioObjectPropertyScope mScope;
    AudioObjectPropertyElement mElement;
} AudioObjectPropertyAddress;

typedef OSStatus (*AudioObjectPropertyListenerProc)(AudioObjectID inObjectID, UInt32 inNumberAddresses,
                                                    const AudioObjectPropertyAddress *inAddresses, void *inClientData);

enum {
    kAudioObjectUnknown = 0,
    kAudioObjectSystemObject = 1,
};

enum {
    kAudioObjectPropertyScopeGlobal = 'glob',
    kAudioObjectPropertyScopeInput = 'inpt',
    kAudioObjectPropertyScopeOutput = 'outp',
    kAudioObjectPropertyScopePlayThrough = 'ptru',
    kAudioObjectPropertyScopeWildcard = '****',
    kAudioObjectPropertyElementMain = 0,
    kAudioObjectPropertyElementMaster = 0,
    kAudioObjectPropertyElementWildcard = 0xFFFFFFFF,
    kAudioObjectPropertySelectorWildcard = '****',
};

enum {
    kAudioHardwareNoError = 0,
    kAudioHardwareNotRunningError = 'stop',
    kAudioHardwareUnspecifiedError = 'what',
    kAudioHardwareUnknownPropertyError = 'who?',
    kAudioHardwareBadPropertySizeError = '!siz',
    kAudioHardwareIllegalOperationError = 'nope',
    kAudioHardwareBadObjectError = '!obj',
    kAudioHardwareBadDeviceError = '!dev',
    kAudioHardwareBadStreamError = '!str',
    kAudioHardwareUnsupportedOperationError = 'unop',
    kAudioDeviceUnsupportedFormatError = '!dat',
};

enum {
    kAudioObjectPropertyBaseClass = 'bcls',
    kAudioObjectPropertyClass = 'clas',
    kAudioObjectPropertyOwner = 'stdv',
    kAudioObjectPropertyName = 'lnam',
    kAudioObjectPropertyManufacturer = 'lmak',
    kAudioObjectPropertyOwnedObjects = 'ownd',
    kAudioObjectClassID = 'aobj',
    kAudioSystemObjectClassID = 'asys',
    kAudioDeviceClassID = 'adev',
    kAudioStreamClassID = 'astr',

    kAudioHardwarePropertyDevices = 'dev#',
    kAudioHardwarePropertyDefaultInputDevice = 'dIn ',
    kAudioHardwarePropertyDefaultOutputDevice = 'dOut',
    kAudioHardwarePropertyDefaultSystemOutputDevice = 'sOut',
    kAudioHardwarePropertyTranslateUIDToDevice = 'uidd',

    kAudioDevicePropertyDeviceUID = 'uid ',
    kAudioDevicePropertyModelUID = 'muid',
    kAudioDevicePropertyTransportType = 'tran',
    kAudioDevicePropertyDeviceIsAlive = 'livn',
    kAudioDevicePropertyDeviceIsRunning = 'goin',
    kAudioDevicePropertyDeviceIsRunningSomewhere = 'gone',
    kAudioDevicePropertyNominalSampleRate = 'nsrt',
    kAudioDevicePropertyAvailableNominalSampleRates = 'nsr#',
    kAudioDevicePropertyLatency = 'ltnc',
    kAudioDevicePropertySafetyOffset = 'saft',
    kAudioDevicePropertyStreams = 'stm#',
    kAudioDevicePropertyStreamConfiguration = 'slay',
    kAudioDevicePropertyBufferFrameSize = 'fsiz',
    kAudioDevicePropertyBufferFrameSizeRange = 'fsz#',
    kAudioDevicePropertyClockDomain = 'clkd',
    kAudioDevicePropertyPreferredChannelsForStereo = 'dch2',
    kAudioDevicePropertyPreferredChannelLayout = 'srnd',
    kAudioDevicePropertyDataSource = 'ssrc',
    kAudioDevicePropertyDataSources = 'ssc#',
    kAudioDevicePropertyDataSourceNameForIDCFString = 'lscn',
    kAudioDevicePropertyVolumeScalar = 'volm',
    kAudioDevicePropertyMute = 'mute',
    kAudioDevicePropertyStreamFormat = 'sfmt',

    kAudioStreamPropertyDirection = 'sdir',
    kAudioStreamPropertyTerminalType = 'term',
    kAudioStreamPropertyStartingChannel = 'schn',
    kAudioStreamPropertyLatency = 'ltnc',
    kAudioStreamPropertyVirtualFormat = 'sfmt',
    kAudioStreamPropertyPhysicalFormat = 'pft ',

    kAudioDeviceTransportTypeBuiltIn = 'bltn',
    kAudioStreamTerminalTypeSpeaker = 'spkr',
};

Boolean AudioObjectHasProperty(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress);
OSStatus AudioObjectIsPropertySettable(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress, Boolean *outIsSettable);
OSStatus AudioObjectGetPropertyDataSize(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
                                        UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *outDataSize);
OSStatus AudioObjectGetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
                                    UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *ioDataSize, void *outData);
OSStatus AudioObjectSetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
                                    UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 inDataSize, const void *inData);
OSStatus AudioObjectAddPropertyListener(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
                                        AudioObjectPropertyListenerProc inListener, void *inClientData);
OSStatus AudioObjectRemovePropertyListener(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
                                           AudioObjectPropertyListenerProc inListener, void *inClientData);
OSStatus AudioDeviceDuck(AudioDeviceID inDevice, Float32 inDuckedLevel, const AudioTimeStamp *inStartTime, Float32 inRampDuration);

UInt64 AudioGetCurrentHostTime(void);
Float64 AudioGetHostClockFrequency(void);
UInt64 AudioConvertHostTimeToNanos(UInt64 inHostTime);
UInt64 AudioConvertNanosToHostTime(UInt64 inNanos);

// the one output device, /dev/dsp0, and its fixed format
enum {
    kPDAudioOutputDevice = 0x40,
    kPDAudioOutputStream = 0x41,
    kPDAudioDeviceRate = 48000,
    kPDAudioDeviceChannels = 2,
};
Boolean PDAudioOutputDeviceIsPresent(void);
UInt32 PDAudioOutputBufferFrameSize(void);

#ifdef __cplusplus
}
#endif

#endif
