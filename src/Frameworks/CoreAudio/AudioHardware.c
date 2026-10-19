// AudioHardware.c - the audio HAL's object model over /dev/dsp0, 48 kHz stereo
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// without the node there are no devices at all

#include <CoreAudio/AudioHardware.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DEVICE_NODE "/dev/dsp0"
#define DEVICE_UID "PureDarwinDSP0"

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static UInt32 sBufferFrameSize = 512;

struct listener {
    AudioObjectID object;
    AudioObjectPropertyAddress address;
    AudioObjectPropertyListenerProc proc;
    void *data;
    struct listener *next;
};
static struct listener *sListeners;

Boolean PDAudioOutputDeviceIsPresent(void)
{
    struct stat st;
    return stat(DEVICE_NODE, &st) == 0 && S_ISCHR(st.st_mode);
}

UInt32 PDAudioOutputBufferFrameSize(void)
{
    pthread_mutex_lock(&sLock);
    UInt32 n = sBufferFrameSize;
    pthread_mutex_unlock(&sLock);
    return n;
}

static mach_timebase_info_data_t timebase(void)
{
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return tb;
}

UInt64 AudioGetCurrentHostTime(void)
{
    return mach_absolute_time();
}

Float64 AudioGetHostClockFrequency(void)
{
    mach_timebase_info_data_t tb = timebase();
    return 1e9 * tb.denom / tb.numer;
}

UInt64 AudioConvertHostTimeToNanos(UInt64 hostTime)
{
    mach_timebase_info_data_t tb = timebase();
    return (UInt64)((__uint128_t)hostTime * tb.numer / tb.denom);
}

UInt64 AudioConvertNanosToHostTime(UInt64 nanos)
{
    mach_timebase_info_data_t tb = timebase();
    return (UInt64)((__uint128_t)nanos * tb.denom / tb.numer);
}

static AudioStreamBasicDescription deviceFormat(void)
{
    return (AudioStreamBasicDescription){ kPDAudioDeviceRate, kAudioFormatLinearPCM,
                                          kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
                                          4, 1, 4, kPDAudioDeviceChannels, 16, 0 };
}

static Boolean isOutputScope(AudioObjectPropertyScope scope)
{
    return scope == kAudioObjectPropertyScopeOutput || scope == kAudioObjectPropertyScopeGlobal ||
           scope == kAudioObjectPropertyScopeWildcard;
}

// a property's value for an object: a size, and the bytes when out is given
enum { kNoProperty = -1 };

static OSStatus copyCFString(CFStringRef s, void *out, UInt32 *size)
{
    *size = sizeof(CFStringRef);
    if (out)
        *(CFStringRef *)out = CFRetain(s);
    return noErr;
}

#define VALUE(type, v)                                  \
    do {                                                \
        *size = sizeof(type);                           \
        if (out) {                                      \
            type _v = (v);                              \
            memcpy(out, &_v, sizeof(type));             \
        }                                               \
        return noErr;                                   \
    } while (0)

static OSStatus systemProperty(const AudioObjectPropertyAddress *a, UInt32 qualifierSize, const void *qualifier, void *out, UInt32 *size)
{
    Boolean present = PDAudioOutputDeviceIsPresent();

    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
        VALUE(UInt32, kAudioSystemObjectClassID);
    case kAudioObjectPropertyBaseClass:
        VALUE(UInt32, kAudioObjectClassID);
    case kAudioObjectPropertyOwner:
        VALUE(AudioObjectID, kAudioObjectUnknown);
    case kAudioObjectPropertyName:
        return copyCFString(CFSTR("PureDarwin Audio"), out, size);
    case kAudioObjectPropertyManufacturer:
        return copyCFString(CFSTR("PureDarwin"), out, size);
    case kAudioHardwarePropertyDevices:
    case kAudioObjectPropertyOwnedObjects:
        *size = present ? sizeof(AudioDeviceID) : 0;
        if (out && present)
            *(AudioDeviceID *)out = kPDAudioOutputDevice;
        return noErr;
    case kAudioHardwarePropertyDefaultOutputDevice:
    case kAudioHardwarePropertyDefaultSystemOutputDevice:
        VALUE(AudioDeviceID, present ? kPDAudioOutputDevice : kAudioObjectUnknown);
    case kAudioHardwarePropertyDefaultInputDevice:
        VALUE(AudioDeviceID, kAudioObjectUnknown);
    case kAudioHardwarePropertyTranslateUIDToDevice: {
        CFStringRef uid = qualifierSize == sizeof(CFStringRef) && qualifier ? *(CFStringRef const *)qualifier : NULL;
        Boolean match = present && uid && CFEqual(uid, CFSTR(DEVICE_UID));
        VALUE(AudioDeviceID, match ? kPDAudioOutputDevice : kAudioObjectUnknown);
    }
    }
    return kNoProperty;
}

static OSStatus deviceProperty(const AudioObjectPropertyAddress *a, void *out, UInt32 *size)
{
    Boolean output = isOutputScope(a->mScope);
    UInt32 channels = output ? kPDAudioDeviceChannels : 0;

    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
        VALUE(UInt32, kAudioDeviceClassID);
    case kAudioObjectPropertyBaseClass:
        VALUE(UInt32, kAudioObjectClassID);
    case kAudioObjectPropertyOwner:
        VALUE(AudioObjectID, kAudioObjectSystemObject);
    case kAudioObjectPropertyName:
        return copyCFString(CFSTR("Built-in Output"), out, size);
    case kAudioObjectPropertyManufacturer:
        return copyCFString(CFSTR("PureDarwin"), out, size);
    case kAudioDevicePropertyDeviceUID:
        return copyCFString(CFSTR(DEVICE_UID), out, size);
    case kAudioDevicePropertyModelUID:
        return copyCFString(CFSTR("PureDarwinIOAudio"), out, size);
    case kAudioDevicePropertyTransportType:
        VALUE(UInt32, kAudioDeviceTransportTypeBuiltIn);
    case kAudioDevicePropertyDeviceIsAlive:
        VALUE(UInt32, 1);
    case kAudioDevicePropertyDeviceIsRunning:
    case kAudioDevicePropertyDeviceIsRunningSomewhere:
        VALUE(UInt32, 0);
    case kAudioDevicePropertyClockDomain:
        VALUE(UInt32, 0);
    case kAudioDevicePropertyNominalSampleRate:
        VALUE(Float64, kPDAudioDeviceRate);
    case kAudioDevicePropertyAvailableNominalSampleRates:
        VALUE(AudioValueRange, ((AudioValueRange){ kPDAudioDeviceRate, kPDAudioDeviceRate }));
    case kAudioDevicePropertyBufferFrameSize:
        VALUE(UInt32, PDAudioOutputBufferFrameSize());
    case kAudioDevicePropertyBufferFrameSizeRange:
        VALUE(AudioValueRange, ((AudioValueRange){ 64, 4096 }));
    case kAudioDevicePropertyLatency:
    case kAudioDevicePropertySafetyOffset:
        if (!output)
            return kNoProperty;
        VALUE(UInt32, 0);
    case kAudioDevicePropertyStreams:
    case kAudioObjectPropertyOwnedObjects:
        *size = output ? sizeof(AudioStreamID) : 0;
        if (out && output)
            *(AudioStreamID *)out = kPDAudioOutputStream;
        return noErr;
    case kAudioDevicePropertyStreamConfiguration: {
        UInt32 n = output ? 1 : 0;
        *size = (UInt32)(offsetof(AudioBufferList, mBuffers) + n * sizeof(AudioBuffer));
        if (out) {
            AudioBufferList *l = out;
            l->mNumberBuffers = n;
            if (n)
                l->mBuffers[0] = (AudioBuffer){ channels, 0, NULL };
        }
        return noErr;
    }
    case kAudioDevicePropertyPreferredChannelsForStereo:
        if (!output)
            return kNoProperty;
        *size = 2 * sizeof(UInt32);
        if (out) {
            ((UInt32 *)out)[0] = 1;
            ((UInt32 *)out)[1] = 2;
        }
        return noErr;
    case kAudioDevicePropertyPreferredChannelLayout:
        if (!output)
            return kNoProperty;
        *size = (UInt32)(offsetof(AudioChannelLayout, mChannelDescriptions) + 2 * sizeof(AudioChannelDescription));
        if (out) {
            AudioChannelLayout *l = out;
            memset(l, 0, *size);
            l->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
            l->mNumberChannelDescriptions = 2;
            l->mChannelDescriptions[0].mChannelLabel = kAudioChannelLabel_Left;
            l->mChannelDescriptions[1].mChannelLabel = kAudioChannelLabel_Right;
        }
        return noErr;
    case kAudioDevicePropertyStreamFormat:
        if (!output)
            return kNoProperty;
        VALUE(AudioStreamBasicDescription, deviceFormat());
    }
    return kNoProperty;
}

static OSStatus streamProperty(const AudioObjectPropertyAddress *a, void *out, UInt32 *size)
{
    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
        VALUE(UInt32, kAudioStreamClassID);
    case kAudioObjectPropertyBaseClass:
        VALUE(UInt32, kAudioObjectClassID);
    case kAudioObjectPropertyOwner:
        VALUE(AudioObjectID, kPDAudioOutputDevice);
    case kAudioStreamPropertyDirection:
        VALUE(UInt32, 0);
    case kAudioStreamPropertyTerminalType:
        VALUE(UInt32, kAudioStreamTerminalTypeSpeaker);
    case kAudioStreamPropertyStartingChannel:
        VALUE(UInt32, 1);
    case kAudioStreamPropertyLatency:
        VALUE(UInt32, 0);
    case kAudioStreamPropertyVirtualFormat:
    case kAudioStreamPropertyPhysicalFormat:
        VALUE(AudioStreamBasicDescription, deviceFormat());
    }
    return kNoProperty;
}

static OSStatus lookup(AudioObjectID object, const AudioObjectPropertyAddress *a, UInt32 qualifierSize, const void *qualifier,
                       void *out, UInt32 *size)
{
    OSStatus r;

    if (a == NULL)
        return kAudioHardwareIllegalOperationError;
    if (object == kAudioObjectSystemObject)
        r = systemProperty(a, qualifierSize, qualifier, out, size);
    else if (object == kPDAudioOutputDevice && PDAudioOutputDeviceIsPresent())
        r = deviceProperty(a, out, size);
    else if (object == kPDAudioOutputStream && PDAudioOutputDeviceIsPresent())
        r = streamProperty(a, out, size);
    else
        return kAudioHardwareBadObjectError;
    return r == kNoProperty ? kAudioHardwareUnknownPropertyError : r;
}

Boolean AudioObjectHasProperty(AudioObjectID object, const AudioObjectPropertyAddress *address)
{
    UInt32 size = 0;
    return lookup(object, address, 0, NULL, NULL, &size) == noErr;
}

OSStatus AudioObjectIsPropertySettable(AudioObjectID object, const AudioObjectPropertyAddress *address, Boolean *outIsSettable)
{
    UInt32 size = 0;
    OSStatus err = lookup(object, address, 0, NULL, NULL, &size);

    if (err != noErr)
        return err;
    if (outIsSettable)
        *outIsSettable = object == kPDAudioOutputDevice && address->mSelector == kAudioDevicePropertyBufferFrameSize;
    return noErr;
}

OSStatus AudioObjectGetPropertyDataSize(AudioObjectID object, const AudioObjectPropertyAddress *address, UInt32 qualifierSize,
                                        const void *qualifier, UInt32 *outDataSize)
{
    if (outDataSize == NULL)
        return kAudioHardwareIllegalOperationError;
    return lookup(object, address, qualifierSize, qualifier, NULL, outDataSize);
}

OSStatus AudioObjectGetPropertyData(AudioObjectID object, const AudioObjectPropertyAddress *address, UInt32 qualifierSize,
                                    const void *qualifier, UInt32 *ioDataSize, void *outData)
{
    UInt32 size = 0;

    if (ioDataSize == NULL || outData == NULL)
        return kAudioHardwareIllegalOperationError;
    OSStatus err = lookup(object, address, qualifierSize, qualifier, NULL, &size);
    if (err != noErr)
        return err;
    // arrays may be read partially, single values must fit
    Boolean isArray = address->mSelector == kAudioHardwarePropertyDevices || address->mSelector == kAudioDevicePropertyStreams ||
                      address->mSelector == kAudioObjectPropertyOwnedObjects;
    if (*ioDataSize < size && !isArray)
        return kAudioHardwareBadPropertySizeError;

    void *buffer = malloc(size ? size : 1);
    if (buffer == NULL)
        return kAudioHardwareUnspecifiedError;
    lookup(object, address, qualifierSize, qualifier, buffer, &size);
    UInt32 n = *ioDataSize < size ? *ioDataSize : size;
    memcpy(outData, buffer, n);
    *ioDataSize = n;
    free(buffer);
    return noErr;
}

static void notify(AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress changed = { selector, scope, kAudioObjectPropertyElementMain };
    struct listener *matches[16];
    int n = 0;

    pthread_mutex_lock(&sLock);
    for (struct listener *l = sListeners; l && n < 16; l = l->next) {
        if (l->object == object && (l->address.mSelector == selector || l->address.mSelector == kAudioObjectPropertySelectorWildcard))
            matches[n++] = l;
    }
    struct listener copies[16];
    for (int i = 0; i < n; i++)
        copies[i] = *matches[i];
    pthread_mutex_unlock(&sLock);
    for (int i = 0; i < n; i++)
        copies[i].proc(object, 1, &changed, copies[i].data);
}

OSStatus AudioObjectSetPropertyData(AudioObjectID object, const AudioObjectPropertyAddress *address, UInt32 qualifierSize,
                                    const void *qualifier, UInt32 dataSize, const void *data)
{
    UInt32 size = 0;
    OSStatus err = lookup(object, address, qualifierSize, qualifier, NULL, &size);

    if (err != noErr)
        return err;
    if (data == NULL || dataSize < size)
        return kAudioHardwareBadPropertySizeError;
    if (object == kPDAudioOutputDevice && address->mSelector == kAudioDevicePropertyBufferFrameSize) {
        UInt32 frames = *(const UInt32 *)data;
        if (frames < 64 || frames > 4096)
            return kAudioHardwareIllegalOperationError;
        pthread_mutex_lock(&sLock);
        sBufferFrameSize = frames;
        pthread_mutex_unlock(&sLock);
        notify(object, address->mSelector, address->mScope);
        return noErr;
    }
    // the device clock is fixed, setting it to its own value is allowed
    if (object == kPDAudioOutputDevice && address->mSelector == kAudioDevicePropertyNominalSampleRate)
        return *(const Float64 *)data == kPDAudioDeviceRate ? noErr : kAudioDeviceUnsupportedFormatError;
    if (object == kAudioObjectSystemObject && address->mSelector == kAudioHardwarePropertyDefaultOutputDevice)
        return *(const AudioDeviceID *)data == kPDAudioOutputDevice ? noErr : kAudioHardwareBadDeviceError;
    return kAudioHardwareIllegalOperationError;
}

OSStatus AudioObjectAddPropertyListener(AudioObjectID object, const AudioObjectPropertyAddress *address,
                                        AudioObjectPropertyListenerProc listener, void *clientData)
{
    if (address == NULL || listener == NULL)
        return kAudioHardwareIllegalOperationError;
    if (object != kAudioObjectSystemObject && object != kPDAudioOutputDevice && object != kPDAudioOutputStream)
        return kAudioHardwareBadObjectError;
    struct listener *l = calloc(1, sizeof(*l));
    if (l == NULL)
        return kAudioHardwareUnspecifiedError;
    *l = (struct listener){ object, *address, listener, clientData, NULL };
    pthread_mutex_lock(&sLock);
    l->next = sListeners;
    sListeners = l;
    pthread_mutex_unlock(&sLock);
    return noErr;
}

OSStatus AudioObjectRemovePropertyListener(AudioObjectID object, const AudioObjectPropertyAddress *address,
                                           AudioObjectPropertyListenerProc listener, void *clientData)
{
    OSStatus result = kAudioHardwareUnknownPropertyError;

    if (address == NULL || listener == NULL)
        return kAudioHardwareIllegalOperationError;
    pthread_mutex_lock(&sLock);
    for (struct listener **p = &sListeners; *p;) {
        struct listener *l = *p;
        if (l->object == object && l->address.mSelector == address->mSelector && l->address.mScope == address->mScope &&
            l->proc == listener && l->data == clientData) {
            *p = l->next;
            free(l);
            result = noErr;
            break;
        }
        p = &l->next;
    }
    pthread_mutex_unlock(&sLock);
    return result;
}

// no other client shares the device, so there is nothing to duck
OSStatus AudioDeviceDuck(AudioDeviceID device, Float32 duckedLevel, const AudioTimeStamp *startTime, Float32 rampDuration)
{
    (void)duckedLevel;
    (void)startTime;
    (void)rampDuration;
    if (device != kPDAudioOutputDevice || !PDAudioOutputDeviceIsPresent())
        return kAudioHardwareBadDeviceError;
    return kAudioHardwareUnsupportedOperationError;
}
