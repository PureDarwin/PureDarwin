// AudioHardware.c - the audio HAL's object model over coreaudiod's device table
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// without coreaudiod or an engine behind it there are no devices at all, kPDAudioOutputDevice stands for the default output

#include "PDAudioClient.h"
#include <mach/mach_time.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

// coreaudiod's device table as last fetched, kept fresh by its change messages once subscribed
static pthread_mutex_t sStateLock = PTHREAD_MUTEX_INITIALIZER;
static pd_audio_state_reply sState;
static Boolean sSubscribed;
static mach_port_t sServer, sChanges;

static void notify(AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope);

mach_port_t PDAudioServerPort(void)
{
    pthread_mutex_lock(&sStateLock);
    if (!sServer && bootstrap_look_up(bootstrap_port, kPDAudioServerName, &sServer) != KERN_SUCCESS)
        sServer = MACH_PORT_NULL;
    mach_port_t p = sServer;
    pthread_mutex_unlock(&sStateLock);
    return p;
}

static Boolean fetchState(pd_audio_state_reply *out)
{
    mach_port_t server = PDAudioServerPort(), reply;
    union {
        pd_audio_get_state_msg request;
        pd_audio_state_reply reply;
        uint8_t bytes[sizeof(pd_audio_state_reply) + MAX_TRAILER_SIZE];
    } *in;

    if (!server || !(in = calloc(1, sizeof(*in))))
        return false;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply);
    in->request.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    in->request.header.msgh_remote_port = server;
    in->request.header.msgh_local_port = reply;
    in->request.header.msgh_size = sizeof(in->request);
    in->request.header.msgh_id = kPDAudioServerGetState;
    kern_return_t kr = mach_msg(&in->request.header, MACH_SEND_MSG | MACH_RCV_MSG | MACH_RCV_TIMEOUT, sizeof(in->request),
                                sizeof(*in), reply, 2000, MACH_PORT_NULL);
    mach_port_mod_refs(mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1);
    Boolean ok = kr == MACH_MSG_SUCCESS && in->reply.header.msgh_id == kPDAudioServerGetStateReply && in->reply.status == KERN_SUCCESS;
    if (ok)
        *out = in->reply;
    free(in);
    return ok;
}

static const struct pd_audio_device_info *infoIn(const pd_audio_state_reply *s, AudioObjectID id)
{
    for (UInt32 i = 0; i < s->count && i < kPDAudioMaxDevices; i++) {
        if (s->devices[i].id == id)
            return &s->devices[i];
    }
    return NULL;
}

// what a listener would want to hear about, from the old table to the new one
static void notifyChanges(const pd_audio_state_reply *a, const pd_audio_state_reply *b)
{
    Boolean listChanged = a->count != b->count;

    for (UInt32 i = 0; i < a->count; i++) {
        const struct pd_audio_device_info *o = &a->devices[i], *n = infoIn(b, o->id);
        if (!n) {
            listChanged = true;
            notify(o->id, kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal);
            continue;
        }
        if (o->volume != n->volume)
            notify(o->id, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);
        if (o->mute != n->mute)
            notify(o->id, kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
        if ((o->flags ^ n->flags) & kPDAudioDeviceRunning)
            notify(o->id, kAudioDevicePropertyDeviceIsRunningSomewhere, kAudioObjectPropertyScopeGlobal);
    }
    for (UInt32 i = 0; i < b->count; i++) {
        if (!infoIn(a, b->devices[i].id))
            listChanged = true;
    }
    if (listChanged) {
        notify(kAudioObjectSystemObject, kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal);
        notify(kAudioObjectSystemObject, kAudioObjectPropertyOwnedObjects, kAudioObjectPropertyScopeGlobal);
    }
    if (a->defaultOutput != b->defaultOutput) {
        notify(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal);
        notify(kPDAudioOutputDevice, kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal);
    }
    if (a->defaultSystemOutput != b->defaultSystemOutput)
        notify(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultSystemOutputDevice, kAudioObjectPropertyScopeGlobal);
    if (a->defaultInput != b->defaultInput)
        notify(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal);

    // the alias device takes on the default's level
    const struct pd_audio_device_info *o = infoIn(a, a->defaultOutput), *n = infoIn(b, b->defaultOutput);
    if ((o == NULL) != (n == NULL) || (o && (o->volume != n->volume || o->id != n->id)))
        notify(kPDAudioOutputDevice, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);
    if ((o == NULL) != (n == NULL) || (o && (o->mute != n->mute || o->id != n->id)))
        notify(kPDAudioOutputDevice, kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
}

static void *changesThread(void *arg)
{
    (void)arg;
    union {
        pd_audio_changed_msg m;
        uint8_t bytes[sizeof(pd_audio_changed_msg) + MAX_TRAILER_SIZE];
    } in;
    pd_audio_state_reply *old = malloc(sizeof(*old)), *now = malloc(sizeof(*now));

    for (;;) {
        if (mach_msg(&in.m.header, MACH_RCV_MSG, 0, sizeof(in), sChanges, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
            continue;
        // one refetch covers every message already queued
        while (mach_msg(&in.m.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(in), sChanges, 0, MACH_PORT_NULL) == MACH_MSG_SUCCESS)
            ;
        if (!old || !now || !fetchState(now))
            continue;
        pthread_mutex_lock(&sStateLock);
        *old = sState;
        sState = *now;
        pthread_mutex_unlock(&sStateLock);
        notifyChanges(old, now);
    }
    return NULL;
}

// subscribes once coreaudiod answers, before that every call asks it again
static Boolean currentState(pd_audio_state_reply *out)
{
    pthread_mutex_lock(&sStateLock);
    Boolean subscribed = sSubscribed;
    if (subscribed && out)
        *out = sState;
    pthread_mutex_unlock(&sStateLock);
    if (subscribed)
        return true;

    mach_port_t server = PDAudioServerPort();
    pd_audio_state_reply *s = malloc(sizeof(*s));
    if (!server || !s || !fetchState(s)) {
        free(s);
        return false;
    }

    pthread_mutex_lock(&sStateLock);
    if (!sSubscribed) {
        pd_audio_subscribe_msg m = { 0 };
        pthread_t t;

        mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &sChanges);
        m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
        m.header.msgh_remote_port = server;
        m.header.msgh_local_port = sChanges;
        m.header.msgh_size = sizeof(m);
        m.header.msgh_id = kPDAudioServerSubscribe;
        if (mach_msg(&m.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(m), 0, MACH_PORT_NULL, 1000, MACH_PORT_NULL) == MACH_MSG_SUCCESS &&
            pthread_create(&t, NULL, changesThread, NULL) == 0) {
            pthread_detach(t);
            sSubscribed = true;
        }
    }
    sState = *s;
    if (out)
        *out = sState;
    pthread_mutex_unlock(&sStateLock);
    free(s);
    return true;
}

Boolean PDAudioCopyState(pd_audio_state_reply *out)
{
    return currentState(out);
}

AudioObjectID PDAudioResolveDevice(AudioObjectID device)
{
    if (device != kPDAudioOutputDevice)
        return device;
    pthread_mutex_lock(&sStateLock);
    Boolean subscribed = sSubscribed;
    AudioObjectID d = sState.defaultOutput;
    pthread_mutex_unlock(&sStateLock);
    if (subscribed)
        return d;
    pd_audio_state_reply *s = malloc(sizeof(*s));
    d = s && currentState(s) ? s->defaultOutput : kAudioObjectUnknown;
    free(s);
    return d;
}

Boolean PDAudioCopyDeviceInfo(AudioObjectID device, struct pd_audio_device_info *out)
{
    pd_audio_state_reply *s = malloc(sizeof(*s));
    const struct pd_audio_device_info *i = NULL;

    if (s && currentState(s)) {
        if (device == kPDAudioOutputDevice)
            device = s->defaultOutput;
        i = infoIn(s, device);
        if (i && out)
            *out = *i;
    }
    free(s);
    return i != NULL;
}

OSStatus PDAudioServerSetValue(UInt32 what, AudioObjectID device, UInt32 value, Float32 scalar)
{
    mach_port_t server = PDAudioServerPort(), reply;
    union {
        pd_audio_set_msg request;
        pd_audio_set_reply reply;
        uint8_t bytes[sizeof(pd_audio_set_msg) + sizeof(pd_audio_set_reply) + MAX_TRAILER_SIZE];
    } in = { 0 };

    if (!server)
        return kAudioHardwareNotRunningError;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply);
    in.request.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    in.request.header.msgh_remote_port = server;
    in.request.header.msgh_local_port = reply;
    in.request.header.msgh_size = sizeof(in.request);
    in.request.header.msgh_id = kPDAudioServerSet;
    in.request.what = what;
    in.request.device = device;
    in.request.value = value;
    in.request.scalar = scalar;
    kern_return_t kr = mach_msg(&in.request.header, MACH_SEND_MSG | MACH_RCV_MSG | MACH_RCV_TIMEOUT, sizeof(in.request),
                                sizeof(in), reply, 2000, MACH_PORT_NULL);
    mach_port_mod_refs(mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1);
    if (kr != MACH_MSG_SUCCESS || in.reply.header.msgh_id != kPDAudioServerSetReply)
        return kAudioHardwareUnspecifiedError;
    if (in.reply.status != KERN_SUCCESS)
        return kAudioHardwareIllegalOperationError;

    // the change message follows, the cache is refreshed now so a read right after the set sees it
    pd_audio_state_reply *s = malloc(sizeof(*s));
    if (s && fetchState(s)) {
        pthread_mutex_lock(&sStateLock);
        if (sSubscribed)
            sState = *s;
        pthread_mutex_unlock(&sStateLock);
    }
    free(s);
    return noErr;
}

Boolean PDAudioOutputDeviceIsPresent(void)
{
    return PDAudioResolveDevice(kPDAudioOutputDevice) != kAudioObjectUnknown || PDAudioSessionIfConnected(kPDAudioOutputDevice);
}

// the alias device's ring keeps the rate it was made with, coreaudiod converts it for the device it lands on
Float64 PDAudioOutputSampleRate(void)
{
    const struct pd_audio_session *s = PDAudioSession();
    return s ? s->sampleRate : 48000;
}

UInt32 PDAudioOutputChannels(void)
{
    const struct pd_audio_session *s = PDAudioSession();
    return s ? s->channels : 2;
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

// a device's rate and channels: the alias device's ring when it has one, else what coreaudiod reports
static void deviceShape(AudioObjectID device, const struct pd_audio_device_info *info, Float64 *rate, UInt32 *channels)
{
    const struct pd_audio_session *s = device == kPDAudioOutputDevice ? PDAudioSession() : NULL;

    *rate = s ? s->sampleRate : info->sampleRate;
    *channels = s ? s->channels : info->outputChannels;
}

static AudioStreamBasicDescription deviceFormat(AudioObjectID device, const struct pd_audio_device_info *info)
{
    Float64 rate;
    UInt32 ch;

    deviceShape(device, info, &rate, &ch);
    return (AudioStreamBasicDescription){ rate, kAudioFormatLinearPCM, kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
                                          4 * ch, 1, 4 * ch, ch, 32, 0 };
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

static OSStatus copyCString(const char *s, void *out, UInt32 *size)
{
    *size = sizeof(CFStringRef);
    if (out)
        *(CFStringRef *)out = CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8);
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

static OSStatus systemProperty(const pd_audio_state_reply *st, const AudioObjectPropertyAddress *a, UInt32 qualifierSize,
                               const void *qualifier, void *out, UInt32 *size)
{
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
        *size = st->count * (UInt32)sizeof(AudioDeviceID);
        for (UInt32 i = 0; out && i < st->count; i++)
            ((AudioDeviceID *)out)[i] = st->devices[i].id;
        return noErr;
    case kAudioHardwarePropertyDefaultOutputDevice:
        VALUE(AudioDeviceID, st->defaultOutput);
    case kAudioHardwarePropertyDefaultSystemOutputDevice:
        VALUE(AudioDeviceID, st->defaultSystemOutput);
    case kAudioHardwarePropertyDefaultInputDevice:
        VALUE(AudioDeviceID, st->defaultInput);
    case kAudioHardwarePropertyTranslateUIDToDevice: {
        CFStringRef uid = qualifierSize == sizeof(CFStringRef) && qualifier ? *(CFStringRef const *)qualifier : NULL;
        AudioDeviceID found = kAudioObjectUnknown;
        char want[128];

        if (uid && CFStringGetCString(uid, want, sizeof(want), kCFStringEncodingUTF8)) {
            for (UInt32 i = 0; i < st->count; i++) {
                if (!strcmp(st->devices[i].uid, want))
                    found = st->devices[i].id;
            }
        }
        VALUE(AudioDeviceID, found);
    }
    }
    return kNoProperty;
}

static OSStatus deviceProperty(AudioObjectID device, const struct pd_audio_device_info *info, const AudioObjectPropertyAddress *a,
                               void *out, UInt32 *size)
{
    Boolean output = isOutputScope(a->mScope) && (info->flags & kPDAudioDeviceHasOutput);
    AudioStreamID stream = device == kPDAudioOutputDevice ? kPDAudioOutputStream : device + 1;
    Float64 rate;
    UInt32 channels;

    deviceShape(device, info, &rate, &channels);
    if (!output)
        channels = 0;
    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
        VALUE(UInt32, kAudioDeviceClassID);
    case kAudioObjectPropertyBaseClass:
        VALUE(UInt32, kAudioObjectClassID);
    case kAudioObjectPropertyOwner:
        VALUE(AudioObjectID, kAudioObjectSystemObject);
    case kAudioObjectPropertyName:
        return copyCString(info->name, out, size);
    case kAudioObjectPropertyManufacturer:
        return copyCString(info->manufacturer, out, size);
    case kAudioDevicePropertyDeviceUID:
        return copyCString(info->uid, out, size);
    case kAudioDevicePropertyModelUID:
        return copyCFString(CFSTR("PureDarwinIOAudio"), out, size);
    case kAudioDevicePropertyTransportType:
        VALUE(UInt32, kAudioDeviceTransportTypeBuiltIn);
    case kAudioDevicePropertyDeviceIsAlive:
        VALUE(UInt32, 1);
    case kAudioDevicePropertyDeviceIsRunning:
        VALUE(UInt32, PDAudioDeviceIsRunning(device));
    case kAudioDevicePropertyDeviceIsRunningSomewhere:
        VALUE(UInt32, PDAudioDeviceIsRunning(device) || (info->flags & kPDAudioDeviceRunning) != 0);
    case kAudioDevicePropertyClockDomain:
        VALUE(UInt32, 0);
    case kAudioDevicePropertyNominalSampleRate:
        VALUE(Float64, rate);
    case kAudioDevicePropertyAvailableNominalSampleRates:
        VALUE(AudioValueRange, ((AudioValueRange){ rate, rate }));
    case kAudioDevicePropertyBufferFrameSize:
        VALUE(UInt32, PDAudioOutputBufferFrameSize());
    case kAudioDevicePropertyBufferFrameSizeRange:
        VALUE(AudioValueRange, ((AudioValueRange){ 64, 4096 }));
    case kAudioDevicePropertyLatency:
        if (!output)
            return kNoProperty;
        VALUE(UInt32, info->latencyFrames);
    case kAudioDevicePropertySafetyOffset:
        if (!output)
            return kNoProperty;
        VALUE(UInt32, 0);
    case kAudioDevicePropertyStreams:
    case kAudioObjectPropertyOwnedObjects:
        *size = output ? sizeof(AudioStreamID) : 0;
        if (out && output)
            *(AudioStreamID *)out = stream;
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
        VALUE(AudioStreamBasicDescription, deviceFormat(device, info));
    // element 0 is the main control, every channel shares it
    case kAudioDevicePropertyVolumeScalar:
        if (!output)
            return kNoProperty;
        VALUE(Float32, info->volume);
    case kAudioDevicePropertyMute:
        if (!output)
            return kNoProperty;
        VALUE(UInt32, info->mute);
    }
    return kNoProperty;
}

static OSStatus streamProperty(AudioObjectID device, const struct pd_audio_device_info *info, const AudioObjectPropertyAddress *a,
                               void *out, UInt32 *size)
{
    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
        VALUE(UInt32, kAudioStreamClassID);
    case kAudioObjectPropertyBaseClass:
        VALUE(UInt32, kAudioObjectClassID);
    case kAudioObjectPropertyOwner:
        VALUE(AudioObjectID, device);
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
        VALUE(AudioStreamBasicDescription, deviceFormat(device, info));
    }
    return kNoProperty;
}

static OSStatus lookup(AudioObjectID object, const AudioObjectPropertyAddress *a, UInt32 qualifierSize, const void *qualifier,
                       void *out, UInt32 *size)
{
    pd_audio_state_reply *st;
    struct pd_audio_device_info info;
    OSStatus r = kNoProperty;

    if (a == NULL)
        return kAudioHardwareIllegalOperationError;
    if (!(st = calloc(1, sizeof(*st))))
        return kAudioHardwareUnspecifiedError;
    Boolean have = PDAudioCopyState(st);

    if (object == kAudioObjectSystemObject) {
        r = systemProperty(st, a, qualifierSize, qualifier, out, size);
    } else if (object == kPDAudioOutputDevice || object == kPDAudioOutputStream) {
        const struct pd_audio_device_info *i = have ? infoIn(st, st->defaultOutput) : NULL;
        if (!i && !PDAudioSessionIfConnected(kPDAudioOutputDevice))
            r = kAudioHardwareBadObjectError;
        else {
            // a ring that outlived every device still answers with what it was made with
            if (i)
                info = *i;
            else
                info = (struct pd_audio_device_info){ .flags = kPDAudioDeviceHasOutput, .volume = 1, .name = "Output",
                                                      .manufacturer = "PureDarwin", .uid = "PureDarwinDefaultOutput" };
            r = object == kPDAudioOutputDevice ? deviceProperty(object, &info, a, out, size)
                                               : streamProperty(kPDAudioOutputDevice, &info, a, out, size);
        }
    } else {
        const struct pd_audio_device_info *dev = have ? infoIn(st, object) : NULL, *str = have ? infoIn(st, object - 1) : NULL;
        if (dev)
            r = deviceProperty(object, dev, a, out, size);
        else if (str && (str->flags & kPDAudioDeviceHasOutput))
            r = streamProperty(object - 1, str, a, out, size);
        else
            r = kAudioHardwareBadObjectError;
    }
    free(st);
    return r == kNoProperty ? kAudioHardwareUnknownPropertyError : r;
}

Boolean AudioObjectHasProperty(AudioObjectID object, const AudioObjectPropertyAddress *address)
{
    UInt32 size = 0;
    return lookup(object, address, 0, NULL, NULL, &size) == noErr;
}

static Boolean isDevice(AudioObjectID object)
{
    return object == kPDAudioOutputDevice || PDAudioCopyDeviceInfo(object, NULL);
}

OSStatus AudioObjectIsPropertySettable(AudioObjectID object, const AudioObjectPropertyAddress *address, Boolean *outIsSettable)
{
    UInt32 size = 0;
    OSStatus err = lookup(object, address, 0, NULL, NULL, &size);
    AudioObjectPropertySelector sel = address ? address->mSelector : 0;

    if (err != noErr)
        return err;
    if (outIsSettable) {
        if (object == kAudioObjectSystemObject)
            *outIsSettable = sel == kAudioHardwarePropertyDefaultOutputDevice || sel == kAudioHardwarePropertyDefaultSystemOutputDevice ||
                             sel == kAudioHardwarePropertyDefaultInputDevice;
        else
            *outIsSettable = isDevice(object) && (sel == kAudioDevicePropertyBufferFrameSize || sel == kAudioDevicePropertyVolumeScalar ||
                                                  sel == kAudioDevicePropertyMute);
    }
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

    // sized again with the value, the device list may have grown in between
    void *buffer = malloc((size ? size : 1) + kPDAudioMaxDevices * sizeof(AudioObjectID));
    if (buffer == NULL)
        return kAudioHardwareUnspecifiedError;
    lookup(object, address, qualifierSize, qualifier, buffer, &size);
    UInt32 n = *ioDataSize < size ? *ioDataSize : size;
    memcpy(outData, buffer, n);
    *ioDataSize = n;
    free(buffer);
    return noErr;
}

static Boolean matches(const struct listener *l, AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope)
{
    Boolean scopeOK = l->address.mScope == scope || l->address.mScope == kAudioObjectPropertyScopeWildcard ||
                      scope == kAudioObjectPropertyScopeGlobal || l->address.mScope == kAudioObjectPropertyScopeGlobal;

    return l->object == object && scopeOK &&
           (l->address.mSelector == selector || l->address.mSelector == kAudioObjectPropertySelectorWildcard);
}

static void notify(AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress changed = { selector, scope, kAudioObjectPropertyElementMain };
    struct listener copies[16];
    int n = 0;

    pthread_mutex_lock(&sLock);
    for (struct listener *l = sListeners; l && n < 16; l = l->next) {
        if (matches(l, object, selector, scope))
            copies[n++] = *l;
    }
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

    AudioObjectPropertySelector sel = address->mSelector;
    if (object == kAudioObjectSystemObject) {
        UInt32 what = sel == kAudioHardwarePropertyDefaultOutputDevice         ? kPDAudioSetDefaultOutput
                      : sel == kAudioHardwarePropertyDefaultSystemOutputDevice ? kPDAudioSetDefaultSystemOutput
                      : sel == kAudioHardwarePropertyDefaultInputDevice        ? kPDAudioSetDefaultInput
                                                                               : 0;
        AudioDeviceID device = PDAudioResolveDevice(*(const AudioDeviceID *)data);
        if (!what)
            return kAudioHardwareIllegalOperationError;
        return PDAudioServerSetValue(what, device, 0, 0) == noErr ? noErr : kAudioHardwareBadDeviceError;
    }
    if (sel == kAudioDevicePropertyBufferFrameSize) {
        UInt32 frames = *(const UInt32 *)data;
        if (frames < 64 || frames > 4096)
            return kAudioHardwareIllegalOperationError;
        pthread_mutex_lock(&sLock);
        sBufferFrameSize = frames;
        pthread_mutex_unlock(&sLock);
        notify(object, address->mSelector, address->mScope);
        return noErr;
    }
    if (sel == kAudioDevicePropertyVolumeScalar)
        return PDAudioServerSetValue(kPDAudioSetVolume, PDAudioResolveDevice(object), 0, *(const Float32 *)data);
    if (sel == kAudioDevicePropertyMute)
        return PDAudioServerSetValue(kPDAudioSetMute, PDAudioResolveDevice(object), *(const UInt32 *)data != 0, 0);
    // the device clock is fixed, setting it to its own value is allowed
    if (sel == kAudioDevicePropertyNominalSampleRate) {
        Float64 rate = 0;
        UInt32 n = sizeof(rate);
        AudioObjectGetPropertyData(object, address, 0, NULL, &n, &rate);
        return *(const Float64 *)data == rate ? noErr : kAudioDeviceUnsupportedFormatError;
    }
    return kAudioHardwareIllegalOperationError;
}

OSStatus AudioObjectAddPropertyListener(AudioObjectID object, const AudioObjectPropertyAddress *address,
                                        AudioObjectPropertyListenerProc listener, void *clientData)
{
    if (address == NULL || listener == NULL)
        return kAudioHardwareIllegalOperationError;
    // listeners on a device that comes later are fine, its id is stable
    if (object == kAudioObjectUnknown)
        return kAudioHardwareBadObjectError;
    struct listener *l = calloc(1, sizeof(*l));
    if (l == NULL)
        return kAudioHardwareUnspecifiedError;
    *l = (struct listener){ object, *address, listener, clientData, NULL };
    pthread_mutex_lock(&sLock);
    l->next = sListeners;
    sListeners = l;
    pthread_mutex_unlock(&sLock);
    // a listener is only heard once coreaudiod knows to tell this process
    currentState(NULL);
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

// coreaudiod mixes every client at full level and has no ducking yet
OSStatus AudioDeviceDuck(AudioDeviceID device, Float32 duckedLevel, const AudioTimeStamp *startTime, Float32 rampDuration)
{
    (void)duckedLevel;
    (void)startTime;
    (void)rampDuration;
    if (!isDevice(device) || !PDAudioOutputDeviceIsPresent())
        return kAudioHardwareBadDeviceError;
    return kAudioHardwareUnsupportedOperationError;
}
