// coreaudiod - PureDarwin's audio server: owns every IOAudioEngine output stream and mixes the clients into them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// IOAudioFamily only lets a process with com.apple.private.audio.driver-host drive an engine, so apps reach
// the hardware through here: each client fills a shared ring, an IO thread per engine mixes the rings one period
// ahead of that engine's playback head and hands the mix to the family through performClientIO

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/semaphore.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "PDAudioServer.h"

// IOAudioFamily's user client ABI (IOAudioTypes.h)
enum {
    kIOAudioStatusBuffer = 0,
    kIOAudioEngineCallRegisterClientBuffer = 0,
    kIOAudioEngineCallUnregisterClientBuffer = 1,
    kIOAudioEngineCallStart = 3,
    kIOAudioEngineCallStop = 4,
    kIOAudioEngineTrapPerformClientIO = 0,
    kIOAudioStreamDirectionOutput = 0,
    kIOAudioStreamDirectionInput = 1,
    kIOAudioControlSubTypeVolume = 'vlme',
    kIOAudioControlSubTypeMute = 'mute',
    kIOAudioControlUsageOutput = 'outp',
};

typedef struct {
    uint32_t fVersion;
    volatile uint32_t fCurrentLoopCount;
    volatile uint64_t fLastLoopTime;
    volatile uint32_t fEraseHeadSampleFrame;
} engine_status;

typedef struct {
    uint32_t fActualDataByteSize;
    uint32_t fActualNumSampleFrames;
    uint32_t fTotalDataByteSize;
    uint32_t fNominalDataByteSize;
    float fData[];
} buffer_descriptor;

#define PERIOD_FRAMES 512
#define CLIENT_RING_PERIODS 4
#define MAX_CLIENTS 64
#define MAX_SUBSCRIBERS 64
#define MAX_IDS 64
#define FIRST_DEVICE_ID 0x100
#define PREFS_DIR "/Library/Preferences"
#define PREFS_PATH PREFS_DIR "/org.puredarwin.audio.plist"

struct engine {
    bool used;
    bool gone;
    struct pd_audio_device_info info;
    io_service_t service;
    io_connect_t connect;
    uint32_t streamID;
    uint32_t channels;
    uint32_t rate;
    uint32_t bufferFrames;
    uint32_t sampleOffset;
    bool hiResInterval;
    engine_status *status;
    buffer_descriptor *mix;
    size_t mixSize;
    bool running;
    bool registered;
    double ticksPerFrame;
    pthread_t thread;
    bool threadUp;
    io_registry_entry_t volumeControl;
    io_registry_entry_t muteControl;
    int32_t volumeMin;
    int32_t volumeMax;
};

struct client {
    mach_port_t session;
    semaphore_t wake;
    struct pd_audio_ring *ring;
    size_t ringSize;
    uint32_t device;
    // fractional read position when the ring's rate differs from the engine's
    double phase;
};

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sClientsChanged = PTHREAD_COND_INITIALIZER;
static struct engine sEngines[kPDAudioMaxDevices];
static struct client sClients[MAX_CLIENTS];
static mach_port_t sSubscribers[MAX_SUBSCRIBERS];
static mach_port_t sNotifyPort;
static uint32_t sGeneration;
static uint32_t sDefaultOutput, sDefaultSystemOutput, sDefaultInput;
static CFMutableDictionaryRef sPrefs;

static struct {
    char uid[128];
    uint32_t id;
} sIds[MAX_IDS];
static uint32_t sNextId = FIRST_DEVICE_ID;

static uint32_t numberProperty(io_registry_entry_t entry, CFStringRef key, uint32_t fallback)
{
    CFTypeRef v = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);
    uint32_t n = fallback;

    if (v && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberSInt32Type, &n);
    if (v)
        CFRelease(v);
    return n;
}

static uint32_t dictNumber(CFDictionaryRef d, CFStringRef key, uint32_t fallback)
{
    CFNumberRef v = d ? CFDictionaryGetValue(d, key) : NULL;
    uint32_t n = fallback;

    if (v && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberSInt32Type, &n);
    return n;
}

static bool stringProperty(io_registry_entry_t entry, CFStringRef key, char *out, size_t size)
{
    CFTypeRef v = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);
    bool ok = v && CFGetTypeID(v) == CFStringGetTypeID() && CFStringGetCString(v, out, size, kCFStringEncodingUTF8);

    if (v)
        CFRelease(v);
    return ok && out[0];
}

static struct engine *engineWithID(uint32_t id)
{
    for (int i = 0; i < kPDAudioMaxDevices; i++) {
        if (sEngines[i].used && !sEngines[i].gone && sEngines[i].info.id == id)
            return &sEngines[i];
    }
    return NULL;
}

static struct engine *engineWithUID(const char *uid)
{
    for (int i = 0; i < kPDAudioMaxDevices; i++) {
        if (sEngines[i].used && !sEngines[i].gone && !strcmp(sEngines[i].info.uid, uid))
            return &sEngines[i];
    }
    return NULL;
}

// a uid keeps its id for coreaudiod's lifetime, so a replugged device comes back as the same object
static uint32_t idForUID(const char *uid)
{
    int slot = -1;

    for (int i = 0; i < MAX_IDS; i++) {
        if (sIds[i].id && !strcmp(sIds[i].uid, uid))
            return sIds[i].id;
        if (!sIds[i].id && slot < 0)
            slot = i;
    }
    uint32_t id = sNextId;
    sNextId += 0x10;
    if (slot >= 0) {
        strlcpy(sIds[slot].uid, uid, sizeof(sIds[slot].uid));
        sIds[slot].id = id;
    }
    return id;
}

// the engine a client plays on right now: its own device, or the current default output
static struct engine *clientEngine(const struct client *c)
{
    return engineWithID(c->device == kPDAudioFollowDefaultOutput ? sDefaultOutput : c->device);
}

static void broadcastLocked(uint32_t what)
{
    pd_audio_changed_msg m = { 0 };

    sGeneration++;
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (!sSubscribers[i])
            continue;
        m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
        m.header.msgh_remote_port = sSubscribers[i];
        m.header.msgh_size = sizeof(m);
        m.header.msgh_id = kPDAudioServerChanged;
        m.what = what;
        m.generation = sGeneration;
        kern_return_t kr = mach_msg(&m.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(m), 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
        if (kr == MACH_SEND_INVALID_DEST) {
            mach_port_deallocate(mach_task_self(), sSubscribers[i]);
            sSubscribers[i] = MACH_PORT_NULL;
        }
    }
}

static CFMutableDictionaryRef devicePrefs(const char *uid, bool create)
{
    CFMutableDictionaryRef all = (CFMutableDictionaryRef)CFDictionaryGetValue(sPrefs, CFSTR("Devices"));
    CFStringRef key = CFStringCreateWithCString(NULL, uid, kCFStringEncodingUTF8);
    CFMutableDictionaryRef d = NULL;

    if (!all && create) {
        all = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(sPrefs, CFSTR("Devices"), all);
        CFRelease(all);
    }
    if (all)
        d = (CFMutableDictionaryRef)CFDictionaryGetValue(all, key);
    if (!d && all && create) {
        d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(all, key, d);
        CFRelease(d);
    }
    CFRelease(key);
    return d;
}

static void loadPrefs(void)
{
    FILE *f = fopen(PREFS_PATH, "r");
    CFPropertyListRef plist = NULL;

    if (f) {
        char buf[65536];
        size_t n = fread(buf, 1, sizeof(buf), f);
        CFDataRef data = CFDataCreate(NULL, (const UInt8 *)buf, (CFIndex)n);

        fclose(f);
        plist = CFPropertyListCreateWithData(NULL, data, kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
        CFRelease(data);
    }
    if (plist && CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
        sPrefs = (CFMutableDictionaryRef)plist;
        return;
    }
    if (plist)
        CFRelease(plist);
    sPrefs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static void savePrefs(void)
{
    CFDataRef data = CFPropertyListCreateData(NULL, sPrefs, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    FILE *f;

    if (!data)
        return;
    mkdir(PREFS_DIR, 0755);
    if ((f = fopen(PREFS_PATH ".new", "w"))) {
        fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data), f);
        fclose(f);
        rename(PREFS_PATH ".new", PREFS_PATH);
    }
    CFRelease(data);
}

static bool prefUID(CFStringRef key, char *out, size_t size)
{
    CFStringRef v = CFDictionaryGetValue(sPrefs, key);
    return v && CFGetTypeID(v) == CFStringGetTypeID() && CFStringGetCString(v, out, size, kCFStringEncodingUTF8);
}

static void setPrefUID(CFStringRef key, const char *uid)
{
    CFStringRef v = CFStringCreateWithCString(NULL, uid, kCFStringEncodingUTF8);
    CFDictionarySetValue(sPrefs, key, v);
    CFRelease(v);
}

// the preferred device when it is here, else the current one while it stays, else the first that can do it
static uint32_t chooseDefault(CFStringRef key, uint32_t current, uint32_t flag)
{
    char uid[128];
    struct engine *e;

    if (prefUID(key, uid, sizeof(uid)) && (e = engineWithUID(uid)) && (e->info.flags & flag))
        return e->info.id;
    if ((e = engineWithID(current)) && (e->info.flags & flag))
        return current;
    for (int i = 0; i < kPDAudioMaxDevices; i++) {
        if (sEngines[i].used && !sEngines[i].gone && (sEngines[i].info.flags & flag))
            return sEngines[i].info.id;
    }
    return 0;
}

static bool chooseDefaultsLocked(void)
{
    uint32_t out = chooseDefault(CFSTR("DefaultOutput"), sDefaultOutput, kPDAudioDeviceHasOutput);
    uint32_t sys = chooseDefault(CFSTR("DefaultSystemOutput"), sDefaultSystemOutput, kPDAudioDeviceHasOutput);
    uint32_t in = chooseDefault(CFSTR("DefaultInput"), sDefaultInput, kPDAudioDeviceHasInput);
    bool changed = out != sDefaultOutput || sys != sDefaultSystemOutput || in != sDefaultInput;

    sDefaultOutput = out;
    sDefaultSystemOutput = sys;
    sDefaultInput = in;
    return changed;
}

static void setControlValue(io_registry_entry_t control, int32_t value)
{
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
    const void *k = CFSTR("IOAudioControlValue"), *v = n;
    CFDictionaryRef d = CFDictionaryCreate(NULL, &k, &v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    kern_return_t kr = IORegistryEntrySetCFProperties(control, d);

    if (kr != KERN_SUCCESS)
        fprintf(stderr, "coreaudiod: setting a control to %d failed: 0x%x\n", value, kr);
    CFRelease(d);
    CFRelease(n);
}

// hardware controls take the level and the mute, the mix does whatever the device has no control for
static void applyVolume(struct engine *e)
{
    if (e->volumeControl) {
        int32_t span = e->volumeMax - e->volumeMin;
        setControlValue(e->volumeControl, e->volumeMin + (int32_t)(e->info.volume * span + 0.5f));
    }
    if (e->muteControl)
        setControlValue(e->muteControl, e->info.mute ? 1 : 0);
}

static void findControls(struct engine *e)
{
    io_iterator_t it;
    io_registry_entry_t child;

    if (IORegistryEntryGetChildIterator(e->service, kIOServicePlane, &it) != KERN_SUCCESS)
        return;
    while ((child = IOIteratorNext(it))) {
        uint32_t sub = numberProperty(child, CFSTR("IOAudioControlSubType"), 0);
        uint32_t usage = numberProperty(child, CFSTR("IOAudioControlUsage"), kIOAudioControlUsageOutput);
        uint32_t channel = numberProperty(child, CFSTR("IOAudioControlChannelID"), 0);
        bool master = usage == kIOAudioControlUsageOutput && channel == 0;

        if (master && !e->volumeControl && sub == kIOAudioControlSubTypeVolume && IOObjectConformsTo(child, "IOAudioLevelControl")) {
            e->volumeControl = child;
            e->volumeMin = (int32_t)numberProperty(child, CFSTR("IOAudioLevelControlMinValue"), 0);
            e->volumeMax = (int32_t)numberProperty(child, CFSTR("IOAudioLevelControlMaxValue"), 1);
            e->info.flags |= kPDAudioDeviceHardwareVolume;
        } else if (master && !e->muteControl && sub == kIOAudioControlSubTypeMute && IOObjectConformsTo(child, "IOAudioToggleControl")) {
            e->muteControl = child;
            e->info.flags |= kPDAudioDeviceHardwareMute;
        } else {
            IOObjectRelease(child);
        }
    }
    IOObjectRelease(it);
}

// the engine's output stream: its id for registering buffers and its channel count, and whether it records too
static bool findStreams(struct engine *e)
{
    io_iterator_t it;
    io_registry_entry_t child;
    bool found = false;

    if (IORegistryEntryGetChildIterator(e->service, kIOServicePlane, &it) != KERN_SUCCESS)
        return false;
    while ((child = IOIteratorNext(it))) {
        if (IOObjectConformsTo(child, "IOAudioStream")) {
            uint32_t dir = numberProperty(child, CFSTR("IOAudioStreamDirection"), 1);
            CFDictionaryRef format = IORegistryEntryCreateCFProperty(child, CFSTR("IOAudioStreamFormat"), kCFAllocatorDefault, 0);
            uint32_t ch = dictNumber(format, CFSTR("IOAudioStreamNumChannels"), 2);

            if (dir == kIOAudioStreamDirectionOutput && !found) {
                e->streamID = numberProperty(child, CFSTR("IOAudioStreamID"), 0);
                e->channels = ch;
                found = true;
            }
            if (format)
                CFRelease(format);
        }
        IOObjectRelease(child);
    }
    IOObjectRelease(it);
    return found;
}

// the name people see: the IOAudioDevice's, numbered when two devices share it
static void deviceNames(struct engine *e)
{
    io_registry_entry_t parent = MACH_PORT_NULL;
    char name[64] = "", maker[64] = "";

    if (IORegistryEntryGetParentEntry(e->service, kIOServicePlane, &parent) == KERN_SUCCESS) {
        stringProperty(parent, CFSTR("IOAudioDeviceName"), name, sizeof(name));
        stringProperty(parent, CFSTR("IOAudioDeviceManufacturerName"), maker, sizeof(maker));
        IOObjectRelease(parent);
    }
    if (!name[0] && !stringProperty(e->service, CFSTR("IOAudioEngineDescription"), name, sizeof(name)))
        strlcpy(name, "Audio Output", sizeof(name));
    if (!maker[0])
        strlcpy(maker, "Unknown", sizeof(maker));

    strlcpy(e->info.name, name, sizeof(e->info.name));
    for (int n = 2; n < 100; n++) {
        bool taken = false;
        for (int i = 0; i < kPDAudioMaxDevices; i++) {
            if (&sEngines[i] != e && sEngines[i].used && !sEngines[i].gone && !strcmp(sEngines[i].info.name, e->info.name))
                taken = true;
        }
        if (!taken)
            break;
        snprintf(e->info.name, sizeof(e->info.name), "%s %d", name, n);
    }
    strlcpy(e->info.manufacturer, maker, sizeof(e->info.manufacturer));
}

static bool openEngine(struct engine *e)
{
    CFDictionaryRef rate;
    mach_vm_address_t addr = 0;
    mach_vm_size_t size = 0;
    kern_return_t kr;

    if (!stringProperty(e->service, CFSTR("IOAudioEngineGlobalUniqueID"), e->info.uid, sizeof(e->info.uid)))
        snprintf(e->info.uid, sizeof(e->info.uid), "IOAudioEngine:%u", e->service);
    if (!findStreams(e)) {
        fprintf(stderr, "coreaudiod: %s has no output stream\n", e->info.uid);
        return false;
    }
    e->bufferFrames = numberProperty(e->service, CFSTR("IOAudioEngineNumSampleFramesPerBuffer"), 0);
    e->sampleOffset = numberProperty(e->service, CFSTR("IOAudioEngineSampleOffset"), 0);
    rate = IORegistryEntryCreateCFProperty(e->service, CFSTR("IOAudioSampleRate"), kCFAllocatorDefault, 0);
    e->rate = dictNumber(rate, CFSTR("IOAudioSampleRateWholeNumber"), 48000);
    if (rate)
        CFRelease(rate);
    CFTypeRef hiRes = IORegistryEntryCreateCFProperty(e->service, CFSTR("IOAudioEngineUseHiResSampleInterval"),
                                                      kCFAllocatorDefault, 0);
    e->hiResInterval = hiRes == kCFBooleanTrue;
    if (hiRes)
        CFRelease(hiRes);
    if (!e->bufferFrames || e->bufferFrames < 4 * PERIOD_FRAMES) {
        fprintf(stderr, "coreaudiod: engine buffer of %u frames is too small\n", e->bufferFrames);
        return false;
    }

    if ((kr = IOServiceOpen(e->service, mach_task_self(), 0, &e->connect)) != KERN_SUCCESS) {
        fprintf(stderr, "coreaudiod: IOServiceOpen on the engine failed: 0x%x\n", kr);
        return false;
    }
    kr = IOConnectMapMemory64(e->connect, kIOAudioStatusBuffer, mach_task_self(), &addr, &size, kIOMapAnywhere);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "coreaudiod: mapping the engine status failed: 0x%x\n", kr);
        return false;
    }
    e->status = (engine_status *)(uintptr_t)addr;

    // the family mixes floats from the client buffer, which follows a data descriptor
    e->mixSize = round_page(sizeof(buffer_descriptor) + (size_t)PERIOD_FRAMES * e->channels * sizeof(float));
    vm_address_t mix = 0;
    if (vm_allocate(mach_task_self(), &mix, e->mixSize, VM_FLAGS_ANYWHERE) != KERN_SUCCESS)
        return false;
    e->mix = (buffer_descriptor *)mix;

    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    e->ticksPerFrame = 1e9 / e->rate * tb.denom / tb.numer;

    e->info.flags |= kPDAudioDeviceHasOutput;
    e->info.sampleRate = e->rate;
    e->info.outputChannels = e->channels;
    e->info.latencyFrames = e->sampleOffset + 2 * PERIOD_FRAMES;
    e->info.volume = 1.0f;
    findControls(e);
    deviceNames(e);
    fprintf(stderr, "coreaudiod: %s (%s) at %u Hz, %u channels, %u frame buffer, offset %u%s%s\n", e->info.name, e->info.uid,
            e->rate, e->channels, e->bufferFrames, e->sampleOffset, e->volumeControl ? ", hardware volume" : "",
            e->muteControl ? ", hardware mute" : "");
    return true;
}

// a fresh buffer set for every run, the family keeps the last run's play position in the old one
static kern_return_t registerMixBuffer(struct engine *e)
{
    uint64_t args[4] = { e->streamID, (uint64_t)(uintptr_t)e->mix, e->mixSize, 0 };
    uint64_t old[2] = { (uint64_t)(uintptr_t)e->mix, 0 };

    if (e->registered)
        IOConnectCallScalarMethod(e->connect, kIOAudioEngineCallUnregisterClientBuffer, old, 2, NULL, NULL);
    memset(e->mix, 0, sizeof(buffer_descriptor));
    return IOConnectCallScalarMethod(e->connect, kIOAudioEngineCallRegisterClientBuffer, args, 4, NULL, NULL);
}

static void setEngineRunning(struct engine *e, bool run)
{
    kern_return_t kr;

    if (run == e->running)
        return;
    if (run && (kr = registerMixBuffer(e)) != KERN_SUCCESS) {
        fprintf(stderr, "coreaudiod: registering the mix buffer failed: 0x%x\n", kr);
        return;
    }
    e->registered = true;
    kr = IOConnectCallScalarMethod(e->connect, run ? kIOAudioEngineCallStart : kIOAudioEngineCallStop, NULL, 0, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "coreaudiod: engine %s failed: 0x%x\n", run ? "start" : "stop", kr);
        return;
    }
    e->running = run;
    if (run)
        e->info.flags |= kPDAudioDeviceRunning;
    else
        e->info.flags &= ~kPDAudioDeviceRunning;
    broadcastLocked(kPDAudioChangedRunning);
}

// the engine's position: whole frames since it started, and the host time of the last wrap
static void enginePosition(struct engine *e, uint64_t *loopStartFrame, uint64_t *loopTime)
{
    uint32_t loop;
    uint64_t t;

    do {
        loop = e->status->fCurrentLoopCount;
        t = e->status->fLastLoopTime;
    } while (loop != e->status->fCurrentLoopCount);
    *loopStartFrame = (uint64_t)loop * e->bufferFrames;
    *loopTime = t;
}

static uint64_t frameNow(struct engine *e)
{
    uint64_t start, t, now = mach_absolute_time();

    enginePosition(e, &start, &t);
    return now > t ? start + (uint64_t)((now - t) / e->ticksPerFrame) : start;
}

static uint64_t hostTimeOfFrame(struct engine *e, uint64_t frame)
{
    uint64_t start, t;

    enginePosition(e, &start, &t);
    return frame >= start ? t + (uint64_t)((frame - start) * e->ticksPerFrame) : t;
}

// a client that only asked about the device holds a ring without playing, so the engine waits for one that plays
static bool anyPlaying(struct engine *e)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sClients[i].ring && sClients[i].ring->running && clientEngine(&sClients[i]) == e)
            return true;
    }
    return false;
}

// one period of a client into out, rate converted by linear interpolation when its ring runs at another rate
static void mixClient(struct engine *e, struct client *c, float *out)
{
    struct pd_audio_ring *r = c->ring;
    uint32_t ch = e->channels;
    uint64_t avail = r->writeFrame - r->readFrame;

    if (r->sampleRate == e->rate) {
        uint32_t n = avail < PERIOD_FRAMES ? (uint32_t)avail : PERIOD_FRAMES;

        for (uint32_t f = 0; f < n; f++) {
            const float *src = &r->data[((r->readFrame + f) % r->frames) * r->channels];
            for (uint32_t c2 = 0; c2 < ch && c2 < r->channels; c2++)
                out[f * ch + c2] += src[c2];
        }
        __sync_synchronize();
        r->readFrame += n;
        return;
    }

    double step = (double)r->sampleRate / e->rate, pos = c->phase;
    uint32_t f;

    for (f = 0; f < PERIOD_FRAMES && (uint64_t)pos + 1 < avail; f++, pos += step) {
        uint64_t i = (uint64_t)pos;
        float t = (float)(pos - i);
        const float *a = &r->data[((r->readFrame + i) % r->frames) * r->channels];
        const float *b = &r->data[((r->readFrame + i + 1) % r->frames) * r->channels];

        for (uint32_t c2 = 0; c2 < ch && c2 < r->channels; c2++)
            out[f * ch + c2] += a[c2] + (b[c2] - a[c2]) * t;
    }
    __sync_synchronize();
    r->readFrame += (uint64_t)pos;
    c->phase = pos - (uint64_t)pos;
}

// one period of every running client on this engine, summed into its mix buffer
static void mixClients(struct engine *e, uint64_t engineFrame)
{
    uint32_t ch = e->channels;
    float *out = e->mix->fData;

    memset(out, 0, (size_t)PERIOD_FRAMES * ch * sizeof(float));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        struct client *c = &sClients[i];

        if (!c->ring || clientEngine(c) != e)
            continue;
        if (c->ring->running)
            mixClient(e, c, out);
        c->ring->readHostTime = hostTimeOfFrame(e, engineFrame + PERIOD_FRAMES);
        semaphore_signal(c->wake);
    }

    // a level without a hardware control is applied here, squared so the slider feels even
    float gain = 1.0f;
    if (!e->volumeControl)
        gain = e->info.volume * e->info.volume;
    if (!e->muteControl && e->info.mute)
        gain = 0.0f;
    if (gain != 1.0f) {
        for (uint32_t s = 0; s < PERIOD_FRAMES * ch; s++)
            out[s] *= gain;
    }
}

static void *ioThread(void *arg)
{
    struct engine *e = arg;
    uint64_t next = 0;
    uint32_t lead = e->sampleOffset + PERIOD_FRAMES;
    uint64_t interval;

    if (e->hiResInterval)
        interval = (uint64_t)(e->ticksPerFrame * 4294967296.0);
    else
        interval = (uint64_t)e->ticksPerFrame;

    for (;;) {
        pthread_mutex_lock(&sLock);
        while (!e->gone && !anyPlaying(e)) {
            struct timespec idle;

            setEngineRunning(e, false);
            next = 0;
            clock_gettime(CLOCK_REALTIME, &idle);
            idle.tv_nsec += 10 * 1000 * 1000;
            if (idle.tv_nsec >= 1000000000) {
                idle.tv_sec++;
                idle.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&sClientsChanged, &sLock, &idle);
        }
        if (e->gone) {
            pthread_mutex_unlock(&sLock);
            return NULL;
        }
        setEngineRunning(e, true);
        pthread_mutex_unlock(&sLock);

        // the family takes no output before the engine has stamped its first wrap
        if (!e->status->fCurrentLoopCount && !e->status->fLastLoopTime) {
            mach_wait_until(mach_absolute_time() + (uint64_t)(e->ticksPerFrame * PERIOD_FRAMES));
            continue;
        }

        uint64_t now = frameNow(e);
        if (next != 0 && next < now + e->sampleOffset)
            fprintf(stderr, "coreaudiod: %s late by %lld frames, skipping ahead\n", e->info.name,
                    (long long)(now + e->sampleOffset - next));
        if (next == 0 || next < now + e->sampleOffset)
            next = now + lead + PERIOD_FRAMES;

        pthread_mutex_lock(&sLock);
        mixClients(e, next);
        pthread_mutex_unlock(&sLock);
        e->mix->fActualNumSampleFrames = PERIOD_FRAMES;
        e->mix->fActualDataByteSize = PERIOD_FRAMES * e->channels * sizeof(float);
        e->mix->fTotalDataByteSize = (uint32_t)e->mixSize;
        e->mix->fNominalDataByteSize = e->mix->fActualDataByteSize;
        kern_return_t kr = IOConnectTrap6(e->connect, kIOAudioEngineTrapPerformClientIO,
                                          (uintptr_t)(next % e->bufferFrames), (uintptr_t)(next / e->bufferFrames), 0,
                                          0, (uintptr_t)(interval >> 32), (uintptr_t)(interval & 0xffffffff));
        if (kr != KERN_SUCCESS)
            fprintf(stderr, "coreaudiod: performClientIO at %llu (head %llu) on %s failed: 0x%x, loop %u time %llu\n",
                    (unsigned long long)next, (unsigned long long)now, e->info.name, kr, e->status->fCurrentLoopCount,
                    (unsigned long long)e->status->fLastLoopTime);
        next += PERIOD_FRAMES;

        // wake when the head is a lead ahead of the next period
        mach_wait_until(hostTimeOfFrame(e, next - lead));
    }
    return NULL;
}

static void startIOThread(struct engine *e)
{
    pthread_attr_t attr;
    struct sched_param sp = { .sched_priority = 47 };

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    pthread_attr_setschedparam(&attr, &sp);
    e->threadUp = pthread_create(&e->thread, &attr, ioThread, e) == 0;
    if (!e->threadUp)
        e->threadUp = pthread_create(&e->thread, NULL, ioThread, e) == 0;
}

static void closeEngine(struct engine *e)
{
    if (e->connect)
        IOServiceClose(e->connect);
    if (e->mix)
        vm_deallocate(mach_task_self(), (vm_address_t)e->mix, e->mixSize);
    if (e->volumeControl)
        IOObjectRelease(e->volumeControl);
    if (e->muteControl)
        IOObjectRelease(e->muteControl);
    if (e->service)
        IOObjectRelease(e->service);
    memset(e, 0, sizeof(*e));
}

static void engineArrived(io_service_t service)
{
    struct engine *e = NULL;

    pthread_mutex_lock(&sLock);
    for (int i = 0; i < kPDAudioMaxDevices && !e; i++) {
        if (!sEngines[i].used)
            e = &sEngines[i];
    }
    if (!e) {
        pthread_mutex_unlock(&sLock);
        fprintf(stderr, "coreaudiod: no room for another engine\n");
        IOObjectRelease(service);
        return;
    }
    memset(e, 0, sizeof(*e));
    e->service = service;
    if (!openEngine(e)) {
        closeEngine(e);
        pthread_mutex_unlock(&sLock);
        return;
    }
    e->info.id = idForUID(e->info.uid);

    CFMutableDictionaryRef p = devicePrefs(e->info.uid, false);
    CFNumberRef vol = p ? CFDictionaryGetValue(p, CFSTR("Volume")) : NULL;
    CFBooleanRef mute = p ? CFDictionaryGetValue(p, CFSTR("Mute")) : NULL;
    if (vol && CFGetTypeID(vol) == CFNumberGetTypeID())
        CFNumberGetValue(vol, kCFNumberFloat32Type, &e->info.volume);
    if (mute && CFGetTypeID(mute) == CFBooleanGetTypeID())
        e->info.mute = CFBooleanGetValue(mute);
    e->used = true;
    applyVolume(e);
    startIOThread(e);
    uint32_t what = kPDAudioChangedDevices;
    if (chooseDefaultsLocked())
        what |= kPDAudioChangedDefaults;
    broadcastLocked(what);
    pthread_cond_broadcast(&sClientsChanged);
    pthread_mutex_unlock(&sLock);
}

static void engineTerminated(io_service_t service)
{
    struct engine *e = NULL;

    pthread_mutex_lock(&sLock);
    for (int i = 0; i < kPDAudioMaxDevices && !e; i++) {
        if (sEngines[i].used && !sEngines[i].gone && IOObjectIsEqualTo(sEngines[i].service, service))
            e = &sEngines[i];
    }
    if (!e) {
        pthread_mutex_unlock(&sLock);
        return;
    }
    fprintf(stderr, "coreaudiod: %s went away\n", e->info.name);
    e->gone = true;
    uint32_t what = kPDAudioChangedDevices;
    if (chooseDefaultsLocked())
        what |= kPDAudioChangedDefaults;
    broadcastLocked(what);
    pthread_cond_broadcast(&sClientsChanged);
    pthread_mutex_unlock(&sLock);

    if (e->threadUp)
        pthread_join(e->thread, NULL);
    pthread_mutex_lock(&sLock);
    closeEngine(e);
    pthread_mutex_unlock(&sLock);
}

static void enginesMatched(void *refcon, io_iterator_t it)
{
    io_service_t s;

    while ((s = IOIteratorNext(it))) {
        if (refcon)
            engineArrived(s);
        else {
            engineTerminated(s);
            IOObjectRelease(s);
        }
    }
}

static void dropClient(struct client *c)
{
    mach_port_mod_refs(mach_task_self(), c->session, MACH_PORT_RIGHT_RECEIVE, -1);
    semaphore_destroy(mach_task_self(), c->wake);
    vm_deallocate(mach_task_self(), (vm_address_t)c->ring, c->ringSize);
    memset(c, 0, sizeof(*c));
}

static void replyStatus(mach_msg_header_t *in, mach_msg_id_t id, int32_t status)
{
    pd_audio_connect_reply reply = { 0 };

    reply.header.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(in->msgh_bits), 0);
    reply.header.msgh_remote_port = in->msgh_remote_port;
    reply.header.msgh_size = id == kPDAudioServerConnectReply ? sizeof(reply) : sizeof(pd_audio_set_reply);
    reply.header.msgh_id = id;
    if (id == kPDAudioServerConnectReply)
        reply.status = status;
    else
        ((pd_audio_set_reply *)&reply)->status = status;
    mach_msg(&reply.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, reply.header.msgh_size, 0, MACH_PORT_NULL, 100, MACH_PORT_NULL);
}

static void handleConnect(mach_msg_header_t *in)
{
    struct client c = { 0 };
    uint32_t frames = PERIOD_FRAMES * CLIENT_RING_PERIODS;
    mach_port_t entry = MACH_PORT_NULL, previous;
    memory_object_size_t entrySize;
    vm_address_t ring = 0;
    int slot = -1;
    uint32_t channels, rate, latency;

    c.device = in->msgh_size >= sizeof(pd_audio_connect_msg) ? ((pd_audio_connect_msg *)in)->device : kPDAudioFollowDefaultOutput;
    pthread_mutex_lock(&sLock);
    for (int i = 0; i < MAX_CLIENTS && slot < 0; i++) {
        if (!sClients[i].ring)
            slot = i;
    }
    struct engine *e = clientEngine(&c);
    channels = e ? e->channels : 0;
    rate = e ? e->rate : 0;
    latency = e ? e->info.latencyFrames : 0;
    pthread_mutex_unlock(&sLock);
    if (!e)
        return replyStatus(in, kPDAudioServerConnectReply, KERN_NOT_SUPPORTED);
    if (slot < 0)
        return replyStatus(in, kPDAudioServerConnectReply, KERN_RESOURCE_SHORTAGE);

    c.ringSize = round_page(pd_audio_ring_size(channels, frames));
    entrySize = c.ringSize;
    if (vm_allocate(mach_task_self(), &ring, c.ringSize, VM_FLAGS_ANYWHERE) != KERN_SUCCESS ||
        mach_make_memory_entry_64(mach_task_self(), &entrySize, ring, VM_PROT_READ | VM_PROT_WRITE, &entry, MACH_PORT_NULL) !=
            KERN_SUCCESS ||
        semaphore_create(mach_task_self(), &c.wake, SYNC_POLICY_FIFO, 0) != KERN_SUCCESS ||
        mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &c.session) != KERN_SUCCESS) {
        if (ring)
            vm_deallocate(mach_task_self(), ring, c.ringSize);
        return replyStatus(in, kPDAudioServerConnectReply, KERN_RESOURCE_SHORTAGE);
    }
    c.ring = (struct pd_audio_ring *)ring;
    c.ring->version = kPDAudioRingVersion;
    c.ring->channels = channels;
    c.ring->frames = frames;
    c.ring->periodFrames = PERIOD_FRAMES;
    c.ring->sampleRate = rate;

    // the client's last send right going away is its exit
    mach_port_move_member(mach_task_self(), c.session, sNotifyPort);
    mach_port_request_notification(mach_task_self(), c.session, MACH_NOTIFY_NO_SENDERS, 1, c.session,
                                   MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);

    pd_audio_connect_reply reply = { 0 };
    reply.header.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(in->msgh_bits), 0) | MACH_MSGH_BITS_COMPLEX;
    reply.header.msgh_remote_port = in->msgh_remote_port;
    reply.header.msgh_size = sizeof(reply);
    reply.header.msgh_id = kPDAudioServerConnectReply;
    reply.body.msgh_descriptor_count = 3;
    reply.ring = (mach_msg_port_descriptor_t){ .name = entry, .disposition = MACH_MSG_TYPE_MOVE_SEND, .type = MACH_MSG_PORT_DESCRIPTOR };
    reply.wake = (mach_msg_port_descriptor_t){ .name = c.wake, .disposition = MACH_MSG_TYPE_COPY_SEND, .type = MACH_MSG_PORT_DESCRIPTOR };
    reply.session = (mach_msg_port_descriptor_t){ .name = c.session, .disposition = MACH_MSG_TYPE_MAKE_SEND, .type = MACH_MSG_PORT_DESCRIPTOR };
    reply.ndr = NDR_record;
    reply.status = KERN_SUCCESS;
    reply.sampleRate = rate;
    reply.channels = channels;
    reply.ringFrames = frames;
    reply.periodFrames = PERIOD_FRAMES;
    reply.latencyFrames = latency;
    if (mach_msg(&reply.header, MACH_SEND_MSG, sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL) !=
        MACH_MSG_SUCCESS) {
        dropClient(&c);
        return;
    }

    pthread_mutex_lock(&sLock);
    sClients[slot] = c;
    pthread_cond_broadcast(&sClientsChanged);
    pthread_mutex_unlock(&sLock);
}

static void handleGetState(mach_msg_header_t *in)
{
    static pd_audio_state_reply reply;

    memset(&reply, 0, sizeof(reply));
    pthread_mutex_lock(&sLock);
    for (int i = 0; i < kPDAudioMaxDevices; i++) {
        if (sEngines[i].used && !sEngines[i].gone)
            reply.devices[reply.count++] = sEngines[i].info;
    }
    reply.generation = sGeneration;
    reply.defaultOutput = sDefaultOutput;
    reply.defaultSystemOutput = sDefaultSystemOutput;
    reply.defaultInput = sDefaultInput;
    pthread_mutex_unlock(&sLock);

    reply.header.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(in->msgh_bits), 0);
    reply.header.msgh_remote_port = in->msgh_remote_port;
    reply.header.msgh_size = sizeof(reply);
    reply.header.msgh_id = kPDAudioServerGetStateReply;
    reply.ndr = NDR_record;
    reply.status = KERN_SUCCESS;
    mach_msg(&reply.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(reply), 0, MACH_PORT_NULL, 100, MACH_PORT_NULL);
}

static void handleSubscribe(mach_msg_header_t *in)
{
    mach_port_t port = in->msgh_remote_port;

    if (MACH_MSGH_BITS_REMOTE(in->msgh_bits) != MACH_MSG_TYPE_PORT_SEND || !MACH_PORT_VALID(port))
        return mach_msg_destroy(in);
    pthread_mutex_lock(&sLock);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (!sSubscribers[i]) {
            sSubscribers[i] = port;
            port = MACH_PORT_NULL;
            break;
        }
    }
    pthread_mutex_unlock(&sLock);
    if (port)
        mach_port_deallocate(mach_task_self(), port);
}

static int32_t setLocked(const pd_audio_set_msg *m, uint32_t *what)
{
    struct engine *e = engineWithID(m->device);
    CFStringRef keys[] = { NULL, CFSTR("DefaultOutput"), CFSTR("DefaultSystemOutput"), CFSTR("DefaultInput") };

    switch (m->what) {
    case kPDAudioSetDefaultOutput:
    case kPDAudioSetDefaultSystemOutput:
    case kPDAudioSetDefaultInput: {
        uint32_t need = m->what == kPDAudioSetDefaultInput ? kPDAudioDeviceHasInput : kPDAudioDeviceHasOutput;
        if (!e || !(e->info.flags & need))
            return KERN_INVALID_ARGUMENT;
        setPrefUID(keys[m->what], e->info.uid);
        if (chooseDefaultsLocked())
            *what |= kPDAudioChangedDefaults;
        return KERN_SUCCESS;
    }
    case kPDAudioSetVolume:
    case kPDAudioSetMute: {
        if (!e)
            return KERN_INVALID_ARGUMENT;
        CFMutableDictionaryRef p = devicePrefs(e->info.uid, true);
        if (m->what == kPDAudioSetVolume) {
            float v = m->scalar < 0 ? 0 : m->scalar > 1 ? 1 : m->scalar;
            CFNumberRef n = CFNumberCreate(NULL, kCFNumberFloat32Type, &v);
            e->info.volume = v;
            CFDictionarySetValue(p, CFSTR("Volume"), n);
            CFRelease(n);
        } else {
            e->info.mute = m->value ? 1 : 0;
            CFDictionarySetValue(p, CFSTR("Mute"), e->info.mute ? kCFBooleanTrue : kCFBooleanFalse);
        }
        applyVolume(e);
        *what |= kPDAudioChangedVolume;
        return KERN_SUCCESS;
    }
    }
    return KERN_INVALID_ARGUMENT;
}

static void handleSet(mach_msg_header_t *in)
{
    uint32_t what = 0;
    int32_t status = KERN_INVALID_ARGUMENT;

    if (in->msgh_size >= sizeof(pd_audio_set_msg)) {
        pthread_mutex_lock(&sLock);
        status = setLocked((const pd_audio_set_msg *)in, &what);
        if (status == KERN_SUCCESS)
            savePrefs();
        if (what)
            broadcastLocked(what);
        pthread_mutex_unlock(&sLock);
    }
    if (MACH_PORT_VALID(in->msgh_remote_port))
        replyStatus(in, kPDAudioServerSetReply, status);
}

static void handleNoSenders(mach_port_t session)
{
    pthread_mutex_lock(&sLock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sClients[i].ring && sClients[i].session == session)
            dropClient(&sClients[i]);
    }
    pthread_mutex_unlock(&sLock);
}

int main(void)
{
    mach_port_t service;
    io_iterator_t arrived, terminated;
    union {
        mach_msg_header_t header;
        uint8_t bytes[8192];
    } msg;

    if (bootstrap_check_in(bootstrap_port, kPDAudioServerName, &service) != KERN_SUCCESS) {
        fprintf(stderr, "coreaudiod: bootstrap_check_in %s failed\n", kPDAudioServerName);
        return 1;
    }
    loadPrefs();

    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &sNotifyPort);
    mach_port_move_member(mach_task_self(), service, sNotifyPort);

    // engines come and go with their hardware, a machine without one still answers with no devices
    IONotificationPortRef ioPort = IONotificationPortCreate(kIOMasterPortDefault);
    mach_port_t ioMachPort = IONotificationPortGetMachPort(ioPort);
    mach_port_move_member(mach_task_self(), ioMachPort, sNotifyPort);
    IOServiceAddMatchingNotification(ioPort, kIOTerminatedNotification, IOServiceMatching("IOAudioEngine"), enginesMatched,
                                     NULL, &terminated);
    enginesMatched(NULL, terminated);
    IOServiceAddMatchingNotification(ioPort, kIOFirstMatchNotification, IOServiceMatching("IOAudioEngine"), enginesMatched,
                                     (void *)1, &arrived);
    enginesMatched((void *)1, arrived);
    if (!sDefaultOutput)
        fprintf(stderr, "coreaudiod: no IOAudioEngine yet, waiting for one\n");

    for (;;) {
        if (mach_msg(&msg.header, MACH_RCV_MSG, 0, sizeof(msg), sNotifyPort, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL) !=
            MACH_MSG_SUCCESS)
            continue;
        if (msg.header.msgh_local_port == ioMachPort)
            IODispatchCalloutFromMessage(NULL, &msg.header, ioPort);
        else if (msg.header.msgh_id == MACH_NOTIFY_NO_SENDERS)
            handleNoSenders(msg.header.msgh_local_port);
        else if (msg.header.msgh_id == kPDAudioServerConnect)
            handleConnect(&msg.header);
        else if (msg.header.msgh_id == kPDAudioServerGetState)
            handleGetState(&msg.header);
        else if (msg.header.msgh_id == kPDAudioServerSubscribe)
            handleSubscribe(&msg.header);
        else if (msg.header.msgh_id == kPDAudioServerSet)
            handleSet(&msg.header);
        else
            mach_msg_destroy(&msg.header);
    }
}
