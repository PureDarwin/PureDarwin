// AudioUnit.c - the HAL, default and system output units, all on the HAL's output device
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// an IOProc converts the client's callback into the device's float format, there is no input device

#include <AudioToolbox/AudioToolbox.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SLICE 4096
#define MAX_CHANNELS 16

struct OpaqueAudioComponent {
    AudioComponentDescription desc;
    CFStringRef name;
};

static struct OpaqueAudioComponent sComponents[] = {
    { { kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0 }, CFSTR("Apple: AUHAL") },
    { { kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput, kAudioUnitManufacturer_Apple, 0, 0 }, CFSTR("Apple: AUDefaultOutput") },
    { { kAudioUnitType_Output, kAudioUnitSubType_SystemOutput, kAudioUnitManufacturer_Apple, 0, 0 }, CFSTR("Apple: AUSystemOutput") },
};
#define COMPONENT_COUNT (sizeof(sComponents) / sizeof(sComponents[0]))

static Boolean matches(const AudioComponentDescription *have, const AudioComponentDescription *want)
{
    return (want->componentType == 0 || want->componentType == have->componentType) &&
           (want->componentSubType == 0 || want->componentSubType == have->componentSubType) &&
           (want->componentManufacturer == 0 || want->componentManufacturer == have->componentManufacturer);
}

AudioComponent AudioComponentFindNext(AudioComponent after, const AudioComponentDescription *desc)
{
    size_t start = after ? (size_t)(after - sComponents) + 1 : 0;

    if (desc == NULL)
        return NULL;
    for (size_t i = start; i < COMPONENT_COUNT; i++) {
        if (matches(&sComponents[i].desc, desc))
            return &sComponents[i];
    }
    return NULL;
}

UInt32 AudioComponentCount(const AudioComponentDescription *desc)
{
    UInt32 n = 0;
    for (AudioComponent c = AudioComponentFindNext(NULL, desc); c; c = AudioComponentFindNext(c, desc))
        n++;
    return n;
}

OSStatus AudioComponentGetDescription(AudioComponent component, AudioComponentDescription *outDesc)
{
    if (component == NULL || outDesc == NULL)
        return kAudio_ParamError;
    *outDesc = component->desc;
    return noErr;
}

OSStatus AudioComponentCopyName(AudioComponent component, CFStringRef *outName)
{
    if (component == NULL || outName == NULL)
        return kAudio_ParamError;
    *outName = CFRetain(component->name);
    return noErr;
}

struct propertyListener {
    AudioUnitPropertyID id;
    AudioUnitPropertyListenerProc proc;
    void *data;
    struct propertyListener *next;
};

struct ComponentInstanceRecord {
    AudioComponent component;
    pthread_mutex_t lock;
    AudioStreamBasicDescription client;
    AURenderCallbackStruct render, input;
    UInt32 maxFrames, bufferFrames, outputEnabled, shouldAllocate;
    Float32 volume;
    AudioChannelLayoutTag layoutTag;
    Boolean initialized;
    volatile Boolean running;
    OSStatus lastError;
    struct propertyListener *listeners;
    AudioDeviceIOProcID ioProc;
    AudioStreamBasicDescription device;
    AudioConverterRef converter;
    uint8_t *clientMemory;
    union {
        AudioBufferList list;
        uint8_t storage[offsetof(AudioBufferList, mBuffers) + MAX_CHANNELS * sizeof(AudioBuffer)];
    } clientBuffers;
    Float64 sampleTime;
};

static AudioStreamBasicDescription floatStereo(Float64 rate)
{
    return (AudioStreamBasicDescription){ rate, kAudioFormatLinearPCM, kAudioFormatFlagsNativeFloatPacked, 8, 1, 8, 2, 32, 0 };
}

static AudioStreamBasicDescription deviceFormat(void)
{
    AudioStreamBasicDescription f = floatStereo(PDAudioOutputSampleRate());
    AudioObjectPropertyAddress a = { kAudioDevicePropertyStreamFormat, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };
    UInt32 size = sizeof(f);

    AudioObjectGetPropertyData(kPDAudioOutputDevice, &a, 0, NULL, &size, &f);
    return f;
}

OSStatus AudioComponentInstanceNew(AudioComponent component, AudioComponentInstance *outInstance)
{
    if (component == NULL || outInstance == NULL)
        return kAudio_ParamError;
    AudioUnit u = calloc(1, sizeof(*u));
    if (u == NULL)
        return kAudio_MemFullError;
    u->component = component;
    pthread_mutex_init(&u->lock, NULL);
    u->client = floatStereo(PDAudioOutputSampleRate());
    u->maxFrames = 1156;
    u->bufferFrames = PDAudioOutputBufferFrameSize();
    u->outputEnabled = 1;
    u->volume = 1.0f;
    *outInstance = u;
    return noErr;
}

AudioComponent AudioComponentInstanceGetComponent(AudioComponentInstance instance)
{
    return instance ? instance->component : NULL;
}

static void notifyListeners(AudioUnit u, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element)
{
    struct propertyListener copies[8];
    int n = 0;

    pthread_mutex_lock(&u->lock);
    for (struct propertyListener *l = u->listeners; l && n < 8; l = l->next) {
        if (l->id == id)
            copies[n++] = *l;
    }
    pthread_mutex_unlock(&u->lock);
    for (int i = 0; i < n; i++)
        copies[i].proc(copies[i].data, u, id, scope, element);
}

// the client format's buffers, sized for the largest slice
static OSStatus allocateClientBuffers(AudioUnit u)
{
    Boolean planar = (u->client.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
    UInt32 buffers = planar ? u->client.mChannelsPerFrame : 1;
    size_t each = (size_t)u->client.mBytesPerFrame * MAX_SLICE;

    free(u->clientMemory);
    u->clientMemory = calloc(buffers, each);
    if (u->clientMemory == NULL)
        return kAudio_MemFullError;
    u->clientBuffers.list.mNumberBuffers = buffers;
    for (UInt32 i = 0; i < buffers; i++)
        u->clientBuffers.list.mBuffers[i] = (AudioBuffer){ planar ? 1 : u->client.mChannelsPerFrame, (UInt32)each,
                                                           u->clientMemory + i * each };
    return noErr;
}

OSStatus AudioUnitInitialize(AudioUnit u)
{
    if (u == NULL)
        return kAudio_ParamError;
    if (u->initialized)
        return noErr;
    if (!PDAudioOutputDeviceIsPresent())
        return kAudioUnitErr_NoConnection;
    u->device = deviceFormat();
    OSStatus err = AudioConverterNew(&u->client, &u->device, &u->converter);
    if (err != noErr)
        return kAudioUnitErr_FormatNotSupported;
    if ((err = allocateClientBuffers(u)) != noErr) {
        AudioConverterDispose(u->converter);
        u->converter = NULL;
        return err;
    }
    u->initialized = true;
    return noErr;
}

OSStatus AudioOutputUnitStop(AudioUnit u);

OSStatus AudioUnitUninitialize(AudioUnit u)
{
    if (u == NULL)
        return kAudio_ParamError;
    AudioOutputUnitStop(u);
    if (u->ioProc)
        AudioDeviceDestroyIOProcID(kPDAudioOutputDevice, u->ioProc);
    u->ioProc = NULL;
    if (u->converter)
        AudioConverterDispose(u->converter);
    u->converter = NULL;
    free(u->clientMemory);
    u->clientMemory = NULL;
    u->initialized = false;
    return noErr;
}

OSStatus AudioComponentInstanceDispose(AudioComponentInstance u)
{
    if (u == NULL)
        return kAudio_ParamError;
    AudioUnitUninitialize(u);
    for (struct propertyListener *l = u->listeners, *next; l; l = next) {
        next = l->next;
        free(l);
    }
    pthread_mutex_destroy(&u->lock);
    free(u);
    return noErr;
}

// the size and writability of each property the unit has
static OSStatus propertyInfo(AudioUnit u, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, UInt32 *size,
                             Boolean *writable)
{
    Boolean w = true;

    if (scope > kAudioUnitScope_Output)
        return kAudioUnitErr_InvalidScope;
    switch (id) {
    case kAudioUnitProperty_StreamFormat:
        if (element != 0)
            return kAudioUnitErr_InvalidElement;
        *size = sizeof(AudioStreamBasicDescription);
        w = scope == kAudioUnitScope_Input;
        break;
    case kAudioUnitProperty_SampleRate:
        *size = sizeof(Float64);
        w = scope == kAudioUnitScope_Input;
        break;
    case kAudioOutputUnitProperty_CurrentDevice:
        *size = sizeof(AudioDeviceID);
        break;
    case kAudioOutputUnitProperty_EnableIO:
    case kAudioOutputUnitProperty_HasIO:
        if (element > 1)
            return kAudioUnitErr_InvalidElement;
        *size = sizeof(UInt32);
        w = id == kAudioOutputUnitProperty_EnableIO;
        break;
    case kAudioOutputUnitProperty_IsRunning:
        *size = sizeof(UInt32);
        w = false;
        break;
    case kAudioUnitProperty_MaximumFramesPerSlice:
    case kAudioUnitProperty_BufferFrameSize:
    case kAudioUnitProperty_ShouldAllocateBuffer:
        *size = sizeof(UInt32);
        break;
    case kAudioUnitProperty_Latency:
        *size = sizeof(Float64);
        w = false;
        break;
    case kAudioUnitProperty_LastRenderError:
        *size = sizeof(OSStatus);
        w = false;
        break;
    case kAudioUnitProperty_ElementCount:
        *size = sizeof(UInt32);
        w = false;
        break;
    case kAudioUnitProperty_SetRenderCallback:
    case kAudioOutputUnitProperty_SetInputCallback:
        *size = sizeof(AURenderCallbackStruct);
        break;
    case kAudioUnitProperty_AudioChannelLayout:
        *size = sizeof(AudioChannelLayout);
        break;
    case kAudioOutputUnitProperty_ChannelMap:
        *size = (UInt32)(u->client.mChannelsPerFrame * sizeof(SInt32));
        break;
    default:
        return kAudioUnitErr_InvalidProperty;
    }
    if (writable)
        *writable = w;
    return noErr;
}

OSStatus AudioUnitGetPropertyInfo(AudioUnit u, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, UInt32 *outSize,
                                  Boolean *outWritable)
{
    UInt32 size = 0;
    if (u == NULL)
        return kAudio_ParamError;
    OSStatus err = propertyInfo(u, id, scope, element, &size, outWritable);
    if (err == noErr && outSize)
        *outSize = size;
    return err;
}

OSStatus AudioUnitGetProperty(AudioUnit u, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, void *out,
                              UInt32 *ioSize)
{
    UInt32 size = 0, value = 0;

    if (u == NULL || out == NULL || ioSize == NULL)
        return kAudio_ParamError;
    OSStatus err = propertyInfo(u, id, scope, element, &size, NULL);
    if (err != noErr)
        return err;
    if (*ioSize < size)
        return kAudio_ParamError;

    pthread_mutex_lock(&u->lock);
    switch (id) {
    case kAudioUnitProperty_StreamFormat: {
        AudioStreamBasicDescription f = scope == kAudioUnitScope_Input ? u->client : deviceFormat();
        memcpy(out, &f, sizeof(f));
        break;
    }
    case kAudioUnitProperty_SampleRate: {
        Float64 rate = scope == kAudioUnitScope_Input ? u->client.mSampleRate : PDAudioOutputSampleRate();
        memcpy(out, &rate, sizeof(rate));
        break;
    }
    case kAudioUnitProperty_Latency: {
        Float64 latency = (Float64)u->bufferFrames / PDAudioOutputSampleRate();
        memcpy(out, &latency, sizeof(latency));
        break;
    }
    case kAudioOutputUnitProperty_CurrentDevice:
        value = PDAudioOutputDeviceIsPresent() ? kPDAudioOutputDevice : kAudioObjectUnknown;
        break;
    case kAudioOutputUnitProperty_EnableIO:
        value = element == 0 ? u->outputEnabled : 0;
        break;
    case kAudioOutputUnitProperty_HasIO:
        value = element == 0 && PDAudioOutputDeviceIsPresent();
        break;
    case kAudioOutputUnitProperty_IsRunning:
        value = u->running;
        break;
    case kAudioUnitProperty_MaximumFramesPerSlice:
        value = u->maxFrames;
        break;
    case kAudioUnitProperty_BufferFrameSize:
        value = u->bufferFrames;
        break;
    case kAudioUnitProperty_ShouldAllocateBuffer:
        value = u->shouldAllocate;
        break;
    case kAudioUnitProperty_ElementCount:
        value = scope == kAudioUnitScope_Global ? 1 : 2;
        break;
    case kAudioUnitProperty_LastRenderError:
        memcpy(out, &u->lastError, sizeof(OSStatus));
        break;
    case kAudioUnitProperty_SetRenderCallback:
        memcpy(out, &u->render, sizeof(u->render));
        break;
    case kAudioOutputUnitProperty_SetInputCallback:
        memcpy(out, &u->input, sizeof(u->input));
        break;
    case kAudioUnitProperty_AudioChannelLayout: {
        AudioChannelLayout l = { scope == kAudioUnitScope_Output ? kAudioChannelLayoutTag_Stereo : u->layoutTag, 0, 0, { { 0 } } };
        if (l.mChannelLayoutTag == 0) {
            pthread_mutex_unlock(&u->lock);
            return kAudioUnitErr_InvalidProperty;
        }
        memcpy(out, &l, sizeof(l));
        break;
    }
    case kAudioOutputUnitProperty_ChannelMap:
        for (UInt32 i = 0; i < u->client.mChannelsPerFrame; i++)
            ((SInt32 *)out)[i] = (SInt32)i;
        break;
    }
    if (size == sizeof(UInt32) && id != kAudioUnitProperty_LastRenderError && id != kAudioOutputUnitProperty_ChannelMap &&
        id != kAudioUnitProperty_StreamFormat && id != kAudioUnitProperty_SampleRate)
        memcpy(out, &value, sizeof(value));
    pthread_mutex_unlock(&u->lock);
    *ioSize = size;
    return noErr;
}

OSStatus AudioUnitSetProperty(AudioUnit u, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, const void *data,
                              UInt32 dataSize)
{
    UInt32 size = 0;
    Boolean writable = false;

    if (u == NULL)
        return kAudio_ParamError;
    OSStatus err = propertyInfo(u, id, scope, element, &size, &writable);
    if (err != noErr)
        return err;
    if (!writable)
        return kAudioUnitErr_PropertyNotWritable;
    // clearing the callbacks passes NULL
    if (data == NULL && id != kAudioUnitProperty_SetRenderCallback && id != kAudioOutputUnitProperty_SetInputCallback)
        return kAudio_ParamError;
    if (data && dataSize < size && id != kAudioUnitProperty_AudioChannelLayout && id != kAudioOutputUnitProperty_ChannelMap)
        return kAudio_ParamError;

    UInt32 value = data && dataSize >= sizeof(UInt32) ? *(const UInt32 *)data : 0;
    pthread_mutex_lock(&u->lock);
    switch (id) {
    case kAudioUnitProperty_StreamFormat: {
        const AudioStreamBasicDescription *f = data;
        if (u->initialized) {
            err = kAudioUnitErr_Initialized;
        } else if (!PDAudioPCMFormatIsSupported(f)) {
            err = kAudioUnitErr_FormatNotSupported;
        } else {
            u->client = *f;
            UInt32 sample = f->mBitsPerChannel / 8;
            if (u->client.mBytesPerFrame == 0)
                u->client.mBytesPerFrame = (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? sample : sample * f->mChannelsPerFrame;
            u->client.mBytesPerPacket = u->client.mBytesPerFrame;
            u->client.mFramesPerPacket = 1;
        }
        break;
    }
    case kAudioUnitProperty_SampleRate:
        if (u->initialized)
            err = kAudioUnitErr_Initialized;
        else if (*(const Float64 *)data <= 0)
            err = kAudioUnitErr_InvalidPropertyValue;
        else
            u->client.mSampleRate = *(const Float64 *)data;
        break;
    case kAudioOutputUnitProperty_CurrentDevice:
        if (value != kPDAudioOutputDevice || !PDAudioOutputDeviceIsPresent())
            err = kAudioUnitErr_InvalidPropertyValue;
        break;
    case kAudioOutputUnitProperty_EnableIO:
        // there is no input device to enable
        if (element == 1 && value)
            err = kAudioUnitErr_InvalidPropertyValue;
        else if (element == 0)
            u->outputEnabled = value != 0;
        break;
    case kAudioUnitProperty_MaximumFramesPerSlice:
        if (value == 0 || value > MAX_SLICE)
            err = kAudioUnitErr_InvalidPropertyValue;
        else
            u->maxFrames = value;
        break;
    case kAudioUnitProperty_BufferFrameSize:
        if (value < 64 || value > MAX_SLICE)
            err = kAudioUnitErr_InvalidPropertyValue;
        else
            u->bufferFrames = value;
        break;
    case kAudioUnitProperty_ShouldAllocateBuffer:
        u->shouldAllocate = value;
        break;
    case kAudioUnitProperty_SetRenderCallback:
        u->render = data ? *(const AURenderCallbackStruct *)data : (AURenderCallbackStruct){ NULL, NULL };
        break;
    case kAudioOutputUnitProperty_SetInputCallback:
        u->input = data ? *(const AURenderCallbackStruct *)data : (AURenderCallbackStruct){ NULL, NULL };
        break;
    case kAudioUnitProperty_AudioChannelLayout:
        if (dataSize < offsetof(AudioChannelLayout, mChannelDescriptions))
            err = kAudio_ParamError;
        else
            u->layoutTag = ((const AudioChannelLayout *)data)->mChannelLayoutTag;
        break;
    case kAudioOutputUnitProperty_ChannelMap:
        // channels are always routed in order, a map that asks for anything else is refused
        for (UInt32 i = 0; i < dataSize / sizeof(SInt32); i++) {
            SInt32 c = ((const SInt32 *)data)[i];
            if (c != (SInt32)i && c != -1)
                err = kAudioUnitErr_InvalidPropertyValue;
        }
        break;
    }
    pthread_mutex_unlock(&u->lock);
    if (err == noErr)
        notifyListeners(u, id, scope, element);
    return err;
}

OSStatus AudioUnitAddPropertyListener(AudioUnit u, AudioUnitPropertyID id, AudioUnitPropertyListenerProc proc, void *data)
{
    if (u == NULL || proc == NULL)
        return kAudio_ParamError;
    struct propertyListener *l = calloc(1, sizeof(*l));
    if (l == NULL)
        return kAudio_MemFullError;
    *l = (struct propertyListener){ id, proc, data, NULL };
    pthread_mutex_lock(&u->lock);
    l->next = u->listeners;
    u->listeners = l;
    pthread_mutex_unlock(&u->lock);
    return noErr;
}

OSStatus AudioUnitRemovePropertyListenerWithUserData(AudioUnit u, AudioUnitPropertyID id, AudioUnitPropertyListenerProc proc, void *data)
{
    if (u == NULL || proc == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&u->lock);
    for (struct propertyListener **p = &u->listeners; *p;) {
        struct propertyListener *l = *p;
        if (l->id == id && l->proc == proc && l->data == data) {
            *p = l->next;
            free(l);
        } else {
            p = &l->next;
        }
    }
    pthread_mutex_unlock(&u->lock);
    return noErr;
}

OSStatus AudioUnitGetParameter(AudioUnit u, AudioUnitParameterID id, AudioUnitScope scope, AudioUnitElement element,
                               AudioUnitParameterValue *outValue)
{
    (void)scope;
    (void)element;
    if (u == NULL || outValue == NULL)
        return kAudio_ParamError;
    if (id != kHALOutputParam_Volume)
        return kAudioUnitErr_InvalidParameter;
    *outValue = u->volume;
    return noErr;
}

OSStatus AudioUnitSetParameter(AudioUnit u, AudioUnitParameterID id, AudioUnitScope scope, AudioUnitElement element,
                               AudioUnitParameterValue value, UInt32 bufferOffset)
{
    (void)scope;
    (void)element;
    (void)bufferOffset;
    if (u == NULL)
        return kAudio_ParamError;
    if (id != kHALOutputParam_Volume)
        return kAudioUnitErr_InvalidParameter;
    if (value < 0 || value > 1)
        return kAudioUnitErr_InvalidParameter;
    u->volume = value;
    return noErr;
}

OSStatus AudioUnitReset(AudioUnit u, AudioUnitScope scope, AudioUnitElement element)
{
    (void)scope;
    (void)element;
    if (u == NULL)
        return kAudio_ParamError;
    if (u->converter)
        AudioConverterReset(u->converter);
    return noErr;
}

// pulling the output bus runs the client's render callback, the input bus has no device
OSStatus AudioUnitRender(AudioUnit u, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *timeStamp, UInt32 bus, UInt32 frames,
                         AudioBufferList *ioData)
{
    if (u == NULL || timeStamp == NULL || ioData == NULL)
        return kAudio_ParamError;
    if (!u->initialized)
        return kAudioUnitErr_Uninitialized;
    if (bus != 0)
        return kAudioUnitErr_NoConnection;
    if (frames > u->maxFrames)
        return kAudioUnitErr_TooManyFramesToProcess;
    if (u->render.inputProc == NULL)
        return kAudioUnitErr_NoConnection;
    AudioUnitRenderActionFlags local = 0;
    return u->render.inputProc(u->render.inputProcRefCon, flags ? flags : &local, timeStamp, 0, frames, ioData);
}

// the converter's input: one render callback for up to a slice of client frames
static OSStatus renderInput(AudioConverterRef converter, UInt32 *packets, AudioBufferList *ioData,
                            AudioStreamPacketDescription **descriptions, void *userData)
{
    AudioUnit u = userData;
    UInt32 frames = *packets > u->maxFrames ? u->maxFrames : *packets;
    AudioBufferList *l = &u->clientBuffers.list;
    AudioUnitRenderActionFlags flags = 0;
    AudioTimeStamp ts = { 0 };

    (void)converter;
    (void)descriptions;
    for (UInt32 i = 0; i < l->mNumberBuffers; i++)
        l->mBuffers[i].mDataByteSize = frames * u->client.mBytesPerFrame;
    ts.mSampleTime = u->sampleTime;
    ts.mHostTime = mach_absolute_time();
    ts.mRateScalar = 1.0;
    ts.mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;

    OSStatus err = u->render.inputProc ? u->render.inputProc(u->render.inputProcRefCon, &flags, &ts, 0, frames, l) : noErr;
    if (err != noErr) {
        u->lastError = err;
        return err;
    }
    if (u->render.inputProc == NULL || (flags & kAudioUnitRenderAction_OutputIsSilence)) {
        for (UInt32 i = 0; i < l->mNumberBuffers; i++)
            memset(l->mBuffers[i].mData, 0, (size_t)frames * u->client.mBytesPerFrame);
    }
    u->sampleTime += frames;
    for (UInt32 i = 0; i < l->mNumberBuffers && i < ioData->mNumberBuffers; i++)
        ioData->mBuffers[i] = l->mBuffers[i];
    *packets = frames;
    return noErr;
}

// the device's IOProc: one cycle of client frames, converted into the device buffer
static OSStatus outputProc(AudioObjectID device, const AudioTimeStamp *now, const AudioBufferList *input, const AudioTimeStamp *inputTime,
                           AudioBufferList *output, const AudioTimeStamp *outputTime, void *data)
{
    AudioUnit u = data;
    AudioBuffer *out = &output->mBuffers[0];
    UInt32 bpf = u->device.mBytesPerFrame;
    UInt32 frames = out->mDataByteSize / bpf;
    UInt32 n = frames;

    (void)device;
    (void)now;
    (void)input;
    (void)inputTime;
    (void)outputTime;
    OSStatus err = AudioConverterFillComplexBuffer(u->converter, renderInput, u, &n, output, NULL);
    // a failing callback gets silence for this cycle, as the HAL does
    if (err != noErr)
        n = 0;
    out->mDataByteSize = frames * bpf;

    float *samples = out->mData;
    UInt32 count = n * u->device.mChannelsPerFrame;
    float volume = u->volume;
    if (volume != 1.0f) {
        for (UInt32 i = 0; i < count; i++)
            samples[i] *= volume;
    }
    memset(samples + count, 0, (size_t)(frames - n) * bpf);
    return noErr;
}

OSStatus AudioOutputUnitStart(AudioUnit u)
{
    if (u == NULL)
        return kAudio_ParamError;
    if (!u->initialized)
        return kAudioUnitErr_Uninitialized;
    if (!u->outputEnabled)
        return kAudioUnitErr_NoConnection;
    pthread_mutex_lock(&u->lock);
    if (u->running) {
        pthread_mutex_unlock(&u->lock);
        return noErr;
    }
    OSStatus err = u->ioProc ? noErr : AudioDeviceCreateIOProcID(kPDAudioOutputDevice, outputProc, u, &u->ioProc);
    if (err == noErr)
        err = AudioDeviceStart(kPDAudioOutputDevice, u->ioProc);
    u->running = err == noErr;
    pthread_mutex_unlock(&u->lock);
    if (err != noErr)
        return err;
    notifyListeners(u, kAudioOutputUnitProperty_IsRunning, kAudioUnitScope_Global, 0);
    return noErr;
}

// a stop from the render callback itself returns at once, from elsewhere it waits out the cycle
OSStatus AudioOutputUnitStop(AudioUnit u)
{
    if (u == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&u->lock);
    if (!u->running) {
        pthread_mutex_unlock(&u->lock);
        return noErr;
    }
    u->running = false;
    AudioDeviceIOProcID proc = u->ioProc;
    pthread_mutex_unlock(&u->lock);
    AudioDeviceStop(kPDAudioOutputDevice, proc);
    notifyListeners(u, kAudioOutputUnitProperty_IsRunning, kAudioUnitScope_Global, 0);
    return noErr;
}
