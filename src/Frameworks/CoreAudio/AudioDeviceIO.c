// AudioDeviceIO.c - IOProcs on output devices: a thread per device runs them and fills this process's ring for it in coreaudiod
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// the alias device kPDAudioOutputDevice has a ring coreaudiod plays on the current default, so its IOProcs
// keep running across a default change

#include "PDAudioClient.h"
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach/semaphore.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCS 16
#define MAX_DEVICES (kPDAudioMaxDevices + 1)

struct ioproc {
    AudioDeviceIOProc proc;
    void *data;
    Boolean running;
    Boolean used;
};

struct haldev {
    AudioObjectID id;
    Boolean connected;
    struct pd_audio_session session;
    struct ioproc procs[MAX_PROCS];
    Boolean inCycle;
    pthread_t thread;
    Boolean threadUp;
};

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sStarted = PTHREAD_COND_INITIALIZER;
static pthread_cond_t sCycleDone = PTHREAD_COND_INITIALIZER;
static struct haldev sDevices[MAX_DEVICES];

static Boolean connectDevice(AudioObjectID device, struct pd_audio_session *s)
{
    mach_port_t server = PDAudioServerPort(), reply;
    // a combined send and receive uses one buffer, the reply lands where the request was
    union {
        pd_audio_connect_msg request;
        pd_audio_connect_reply reply;
        uint8_t bytes[sizeof(pd_audio_connect_reply) + MAX_TRAILER_SIZE];
    } in = { 0 };
    pd_audio_connect_msg *out = &in.request;
    mach_vm_address_t ring = 0;

    if (!server)
        return false;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply);
    out->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    out->header.msgh_remote_port = server;
    out->header.msgh_local_port = reply;
    out->header.msgh_size = sizeof(*out);
    out->header.msgh_id = kPDAudioServerConnect;
    out->device = device == kPDAudioOutputDevice ? kPDAudioFollowDefaultOutput : device;
    kern_return_t kr = mach_msg(&out->header, MACH_SEND_MSG | MACH_RCV_MSG | MACH_RCV_TIMEOUT, sizeof(*out), sizeof(in), reply,
                                2000, MACH_PORT_NULL);
    mach_port_mod_refs(mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1);
    if (kr != MACH_MSG_SUCCESS)
        return false;
    if (in.reply.header.msgh_id != kPDAudioServerConnectReply || !(in.reply.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
        in.reply.status != KERN_SUCCESS) {
        mach_msg_destroy(&in.reply.header);
        return false;
    }

    size_t size = round_page(pd_audio_ring_size(in.reply.channels, in.reply.ringFrames));
    kr = mach_vm_map(mach_task_self(), &ring, size, 0, VM_FLAGS_ANYWHERE, in.reply.ring.name, 0, FALSE,
                     VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
    mach_port_deallocate(mach_task_self(), in.reply.ring.name);
    if (kr != KERN_SUCCESS) {
        mach_port_deallocate(mach_task_self(), in.reply.wake.name);
        mach_port_deallocate(mach_task_self(), in.reply.session.name);
        return false;
    }
    *s = (struct pd_audio_session){ (struct pd_audio_ring *)(uintptr_t)ring, in.reply.wake.name, in.reply.session.name,
                                    in.reply.sampleRate, in.reply.channels, in.reply.latencyFrames };
    return true;
}

static struct haldev *findDevice(AudioObjectID device)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (sDevices[i].id == device)
            return &sDevices[i];
    }
    return NULL;
}

// the device's connection, made the first time it is asked for and retried while coreaudiod has none
static struct haldev *deviceLocked(AudioObjectID device, Boolean connect)
{
    struct haldev *d = findDevice(device);

    if (d && d->connected)
        return d;
    if (!connect)
        return NULL;
    if (device != kPDAudioOutputDevice && !PDAudioCopyDeviceInfo(device, NULL))
        return NULL;
    if (!d)
        d = findDevice(0);
    if (!d)
        return NULL;
    if (!connectDevice(device, &d->session))
        return NULL;
    d->id = device;
    d->connected = true;
    return d;
}

const struct pd_audio_session *PDAudioSessionForDevice(AudioObjectID device)
{
    pthread_mutex_lock(&sLock);
    struct haldev *d = deviceLocked(device, true);
    pthread_mutex_unlock(&sLock);
    return d ? &d->session : NULL;
}

const struct pd_audio_session *PDAudioSessionIfConnected(AudioObjectID device)
{
    pthread_mutex_lock(&sLock);
    struct haldev *d = deviceLocked(device, false);
    pthread_mutex_unlock(&sLock);
    return d ? &d->session : NULL;
}

const struct pd_audio_session *PDAudioSession(void)
{
    return PDAudioSessionForDevice(kPDAudioOutputDevice);
}

static Boolean anyRunning(const struct haldev *d)
{
    for (int i = 0; i < MAX_PROCS; i++) {
        if (d->procs[i].used && d->procs[i].running)
            return true;
    }
    return false;
}

Boolean PDAudioDeviceIsRunning(AudioObjectID device)
{
    pthread_mutex_lock(&sLock);
    struct haldev *d = findDevice(device);
    Boolean r = d && d->connected && anyRunning(d);
    pthread_mutex_unlock(&sLock);
    return r;
}

static Float64 ticksPerFrame(const struct haldev *d)
{
    mach_timebase_info_data_t tb;

    mach_timebase_info(&tb);
    return 1e9 / d->session.sampleRate * tb.denom / tb.numer;
}

// runs every started IOProc once per buffer while the ring has room, each into its own buffer, and sums them
static void *ioThread(void *arg)
{
    struct haldev *d = arg;
    struct pd_audio_ring *r = d->session.ring;
    UInt32 ch = d->session.channels;
    Float64 tpf = ticksPerFrame(d);
    float *mix = NULL, *one = NULL;
    UInt32 capacity = 0;

    for (;;) {
        pthread_mutex_lock(&sLock);
        while (!anyRunning(d)) {
            r->running = 0;
            pthread_cond_wait(&sStarted, &sLock);
        }
        r->running = 1;
        pthread_mutex_unlock(&sLock);

        UInt32 frames = PDAudioOutputBufferFrameSize();
        if (frames > r->frames / 2)
            frames = r->frames / 2;
        if (frames > capacity) {
            free(mix);
            free(one);
            mix = malloc((size_t)frames * ch * sizeof(float));
            one = malloc((size_t)frames * ch * sizeof(float));
            capacity = mix && one ? frames : 0;
            if (!capacity)
                continue;
        }

        while (r->frames - (r->writeFrame - r->readFrame) >= frames) {
            struct ioproc procs[MAX_PROCS];
            int n = 0;

            // a stop off this thread waits for the cycle, so no proc runs after AudioDeviceStop returns
            pthread_mutex_lock(&sLock);
            for (int i = 0; i < MAX_PROCS; i++) {
                if (d->procs[i].used && d->procs[i].running)
                    procs[n++] = d->procs[i];
            }
            d->inCycle = n > 0;
            pthread_mutex_unlock(&sLock);
            if (n == 0)
                break;

            UInt64 queued = r->writeFrame - r->readFrame;
            AudioTimeStamp now = { 0 }, outTime = { 0 };
            AudioBufferList list = { 1, { { ch, frames * ch * (UInt32)sizeof(float), one } } };

            now.mSampleTime = (Float64)r->readFrame;
            now.mHostTime = mach_absolute_time();
            now.mFlags = kAudioTimeStampSampleHostTimeValid;
            outTime.mSampleTime = (Float64)r->writeFrame;
            outTime.mHostTime = r->readHostTime + (UInt64)(queued * tpf);
            outTime.mRateScalar = 1.0;
            outTime.mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;

            memset(mix, 0, (size_t)frames * ch * sizeof(float));
            for (int i = 0; i < n; i++) {
                memset(one, 0, (size_t)frames * ch * sizeof(float));
                list.mBuffers[0].mDataByteSize = frames * ch * (UInt32)sizeof(float);
                procs[i].proc(d->id, &now, NULL, &now, &list, &outTime, procs[i].data);
                for (UInt32 s = 0; s < frames * ch; s++)
                    mix[s] += one[s];
            }
            for (UInt32 f = 0; f < frames; f++)
                memcpy(&r->data[((r->writeFrame + f) % r->frames) * ch], &mix[f * ch], ch * sizeof(float));
            __sync_synchronize();
            r->writeFrame += frames;

            pthread_mutex_lock(&sLock);
            d->inCycle = false;
            pthread_cond_broadcast(&sCycleDone);
            pthread_mutex_unlock(&sLock);
        }

        // coreaudiod signals after each period it takes
        mach_timespec_t wait = { 0, 50 * 1000 * 1000 };
        semaphore_timedwait(d->session.wake, wait);
    }
    return NULL;
}

// an IOProcID carries its device's slot and its own, so one id never names a proc on another device
static AudioDeviceIOProcID makeID(const struct haldev *d, int i)
{
    return (AudioDeviceIOProcID)(uintptr_t)(((d - sDevices) << 8) | (i + 1));
}

static struct ioproc *findProc(struct haldev *d, AudioDeviceIOProcID id)
{
    uintptr_t v = (uintptr_t)id;
    uintptr_t slot = v >> 8, i = v & 0xff;

    if (i == 0 || i > MAX_PROCS || slot >= MAX_DEVICES || &sDevices[slot] != d || !d->procs[i - 1].used)
        return NULL;
    return &d->procs[i - 1];
}

static Boolean otherThread(const struct haldev *d)
{
    return d->threadUp && !pthread_equal(pthread_self(), d->thread);
}

OSStatus AudioDeviceCreateIOProcID(AudioObjectID device, AudioDeviceIOProc proc, void *clientData, AudioDeviceIOProcID *outID)
{
    if (proc == NULL || outID == NULL)
        return kAudioHardwareIllegalOperationError;
    pthread_mutex_lock(&sLock);
    struct haldev *d = deviceLocked(device, true);
    if (!d) {
        pthread_mutex_unlock(&sLock);
        return kAudioHardwareBadDeviceError;
    }
    for (int i = 0; i < MAX_PROCS; i++) {
        if (!d->procs[i].used) {
            d->procs[i] = (struct ioproc){ proc, clientData, false, true };
            *outID = makeID(d, i);
            pthread_mutex_unlock(&sLock);
            return noErr;
        }
    }
    pthread_mutex_unlock(&sLock);
    return kAudioHardwareUnspecifiedError;
}

OSStatus AudioDeviceDestroyIOProcID(AudioObjectID device, AudioDeviceIOProcID id)
{
    struct ioproc *p = NULL;

    pthread_mutex_lock(&sLock);
    struct haldev *d = deviceLocked(device, false);
    if (!d) {
        pthread_mutex_unlock(&sLock);
        return kAudioHardwareBadDeviceError;
    }
    while (d->inCycle && otherThread(d))
        pthread_cond_wait(&sCycleDone, &sLock);
    if ((p = findProc(d, id)))
        memset(p, 0, sizeof(*p));
    pthread_mutex_unlock(&sLock);
    return p ? noErr : kAudioHardwareIllegalOperationError;
}

// the old API names a proc by its function, so the first registration of it stands for it
static AudioDeviceIOProcID idForProc(struct haldev *d, AudioDeviceIOProc proc)
{
    for (int i = 0; d && i < MAX_PROCS; i++) {
        if (d->procs[i].used && d->procs[i].proc == proc)
            return makeID(d, i);
    }
    return NULL;
}

OSStatus AudioDeviceAddIOProc(AudioDeviceID device, AudioDeviceIOProc proc, void *clientData)
{
    AudioDeviceIOProcID id;
    return AudioDeviceCreateIOProcID(device, proc, clientData, &id);
}

OSStatus AudioDeviceRemoveIOProc(AudioDeviceID device, AudioDeviceIOProc proc)
{
    pthread_mutex_lock(&sLock);
    AudioDeviceIOProcID id = idForProc(deviceLocked(device, false), proc);
    pthread_mutex_unlock(&sLock);
    return AudioDeviceDestroyIOProcID(device, id);
}

static OSStatus setRunning(AudioObjectID device, AudioDeviceIOProcID id, Boolean run)
{
    struct ioproc *p;

    pthread_mutex_lock(&sLock);
    struct haldev *d = deviceLocked(device, true);
    if (!d) {
        pthread_mutex_unlock(&sLock);
        return kAudioHardwareBadDeviceError;
    }
    if (id == NULL) {
        pthread_mutex_unlock(&sLock);
        return noErr;
    }
    if (!(p = findProc(d, id)) && !(p = findProc(d, idForProc(d, (AudioDeviceIOProc)id)))) {
        pthread_mutex_unlock(&sLock);
        return kAudioHardwareIllegalOperationError;
    }
    p->running = run;
    while (!run && d->inCycle && otherThread(d))
        pthread_cond_wait(&sCycleDone, &sLock);
    if (run && !d->threadUp)
        d->threadUp = pthread_create(&d->thread, NULL, ioThread, d) == 0;
    pthread_cond_broadcast(&sStarted);
    semaphore_t wake = d->session.wake;
    pthread_mutex_unlock(&sLock);
    if (run)
        semaphore_signal(wake);
    return noErr;
}

OSStatus AudioDeviceStart(AudioObjectID device, AudioDeviceIOProcID id)
{
    return setRunning(device, id, true);
}

OSStatus AudioDeviceStop(AudioObjectID device, AudioDeviceIOProcID id)
{
    return setRunning(device, id, false);
}

OSStatus AudioDeviceGetCurrentTime(AudioObjectID device, AudioTimeStamp *outTime)
{
    const struct pd_audio_session *s = PDAudioSessionIfConnected(device);

    if (!s && !PDAudioCopyDeviceInfo(PDAudioResolveDevice(device), NULL))
        return kAudioHardwareBadDeviceError;
    if (!s || !PDAudioDeviceIsRunning(device))
        return kAudioHardwareNotRunningError;
    memset(outTime, 0, sizeof(*outTime));
    outTime->mSampleTime = (Float64)s->ring->readFrame;
    outTime->mHostTime = mach_absolute_time();
    outTime->mRateScalar = 1.0;
    outTime->mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;
    return noErr;
}
