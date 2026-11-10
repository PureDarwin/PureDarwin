// PDUSBAudioEngine.cpp - one USB audio stream: isochronous transfers drain or fill the sample ring (USB Audio Class 1.0, section 3.7)
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "PDUSBAudioEngine.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <kern/clock.h>

#define super IOAudioEngine
OSDefineMetaClassAndStructors(PDUSBAudioEngine, IOAudioEngine);

#define kLog(fmt, ...) IOLog("PDUSBAudioEngine: " fmt "\n", ##__VA_ARGS__)

bool PDUSBAudioEngine::initWithFormat(const PDUSBAudioFormat &format, IOUSBPipe *pipe, IOUSBPipe *feedback, bool highSpeed,
                                      bool input)
{
    if (!super::init(NULL) || !pipe)
        return false;
    fFormat = format;
    fInput = input;
    fPipe = pipe;
    fPipe->retain();
    fFeedbackPipe = input ? NULL : feedback;
    if (fFeedbackPipe)
        fFeedbackPipe->retain();
    fStream = NULL;
    fRingMem = NULL;
    fRing = NULL;
    fDone = NULL;
    fBytesPerFrame = format.channels * format.subframeBytes;
    // a high-speed bInterval of n is a packet every 2^(n-1) microframes, full speed sends one a frame
    UInt32 shift = format.interval ? format.interval - 1 : 0;
    fPacketsPerSecond = highSpeed ? 8000 >> (shift > 3 ? 3 : shift) : 1000;
    fSamplesPerPacket = (UInt32)(((UInt64)format.rate << 16) / fPacketsPerSecond);
    fFraction = 0;
    fHead = 0;
    fInFlight = 0;
    fRunning = fGone = false;
    fLoggedErrors = 0;
    fLastService = 0;
    fMaxGapUs = 0;
    fMinQueued = kTransfers;
    bzero(fTransfers, sizeof(fTransfers));
    bzero(&fFeedback, sizeof(fFeedback));
    return true;
}

bool PDUSBAudioEngine::initHardware(IOService *provider)
{
    IOAudioStreamFormat format;
    IOAudioSampleRate rate;

    if (!super::initHardware(provider))
        return false;
    setDescription(fInput ? "USB Audio Input" : "USB Audio");

    fDone = IOInterruptEventSource::interruptEventSource(this, &PDUSBAudioEngine::completionsReady);
    if (!fDone || getWorkLoop()->addEventSource(fDone) != kIOReturnSuccess)
        return false;

    fRingMem = IOBufferMemoryDescriptor::withCapacity(kRingFrames * fBytesPerFrame, kIODirectionInOut);
    if (!fRingMem)
        return false;
    fRing = (UInt8 *)fRingMem->getBytesNoCopy();
    bzero(fRing, kRingFrames * fBytesPerFrame);

    for (int i = 0; i < kTransfers; i++) {
        fTransfers[i].engine = this;
        fTransfers[i].buffer = IOBufferMemoryDescriptor::withCapacity(fFormat.maxPacket * kPacketsPerTransfer, kIODirectionInOut);
        if (!fTransfers[i].buffer)
            return false;
    }
    if (fFeedbackPipe) {
        fFeedback.engine = this;
        fFeedback.buffer = IOBufferMemoryDescriptor::withCapacity(4, kIODirectionIn);
    }

    fStream = new IOAudioStream;
    if (!fStream || !fStream->initWithAudioEngine(this, fInput ? kIOAudioStreamDirectionInput : kIOAudioStreamDirectionOutput, 1))
        return false;
    bzero(&format, sizeof(format));
    format.fNumChannels = fFormat.channels;
    format.fSampleFormat = kIOAudioStreamSampleFormatLinearPCM;
    format.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
    format.fBitDepth = fFormat.bits;
    format.fBitWidth = fFormat.subframeBytes * 8;
    format.fAlignment = kIOAudioStreamAlignmentLowByte;
    format.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
    format.fIsMixable = true;
    rate.whole = fFormat.rate;
    rate.fraction = 0;
    fStream->addAvailableFormat(&format, &rate, &rate);
    fStream->setSampleBuffer(fRing, kRingFrames * fBytesPerFrame);
    fStream->setFormat(&format);
    if (addAudioStream(fStream) != kIOReturnSuccess)
        return false;
    setSampleRate(&rate);
    setNumSampleFramesPerBuffer(kRingFrames);

    // output runs a queue of transfers ahead of what plays, input lands a transfer behind what was sung,
    // clients stay a transfer beyond either
    UInt32 perPacket = fFormat.rate / fPacketsPerSecond + 1;
    UInt32 queued = kTransfers * kPacketsPerTransfer * perPacket;
    if (fInput) {
        setInputSampleLatency(kPacketsPerTransfer * perPacket);
        setSampleOffset(2 * kPacketsPerTransfer * perPacket);
    } else {
        setOutputSampleLatency(queued);
        setSampleOffset(queued + kPacketsPerTransfer * perPacket);
    }

    PDUSBAudio *device = OSDynamicCast(PDUSBAudio, provider);
    if (device && !fInput)
        device->addControls(this);
    kLog("%s %u Hz, %u ch, %u-bit, %u packets a second, %u transfers of %u packets queued", fInput ? "input" : "output",
         fFormat.rate, fFormat.channels, fFormat.bits, fPacketsPerSecond, kTransfers, kPacketsPerTransfer);
    return true;
}

// the head moves over the ring, a pass over its end is a loop for the family, dated back by the frames past it
void PDUSBAudioEngine::advanceHead(UInt32 frames)
{
    UInt32 head = fHead + frames;

    if (head >= kRingFrames) {
        AbsoluteTime now = fClock, past;

        head -= kRingFrames;
        nanoseconds_to_absolutetime((UInt64)head * 1000000000ULL / fFormat.rate, &past);
        now -= past;
        takeTimeStamp(true, &now);
    }
    fHead = head;
}

// output: the next packets from the ring at the head, sized by the nominal or fed-back rate.
// input: room for a full packet each, the device sends what it has
void PDUSBAudioEngine::fillTransfer(Transfer *t)
{
    UInt8 *out = (UInt8 *)t->buffer->getBytesNoCopy();
    UInt32 total = 0;

    for (int k = 0; k < kPacketsPerTransfer; k++) {
        t->frames[k].frActCount = 0;
        t->frames[k].frStatus = 0;
        if (fInput) {
            t->frames[k].frReqCount = fFormat.maxPacket;
            continue;
        }
        // between runs the stream carries silence at the nominal rate and the head stays put
        if (!fRunning) {
            UInt32 samples = (fFormat.rate / fPacketsPerSecond) * fBytesPerFrame <= fFormat.maxPacket ?
                             fFormat.rate / fPacketsPerSecond : fFormat.maxPacket / fBytesPerFrame;
            bzero(out + total, samples * fBytesPerFrame);
            t->frames[k].frReqCount = samples * fBytesPerFrame;
            total += samples * fBytesPerFrame;
            continue;
        }
        UInt32 acc = fFraction + fSamplesPerPacket;
        UInt32 samples = acc >> 16;

        fFraction = acc & 0xffff;
        if (samples * fBytesPerFrame > fFormat.maxPacket)
            samples = fFormat.maxPacket / fBytesPerFrame;
        for (UInt32 done = 0; done < samples;) {
            UInt32 run = kRingFrames - fHead;
            if (run > samples - done)
                run = samples - done;
            memcpy(out + total + done * fBytesPerFrame, fRing + fHead * fBytesPerFrame, run * fBytesPerFrame);
            done += run;
            advanceHead(run);
        }
        t->frames[k].frReqCount = samples * fBytesPerFrame;
        total += samples * fBytesPerFrame;
    }
    if (fRunning && !fInput) {
        bool sound = false;
        for (UInt32 b = 0; b < total && !sound; b++)
            sound = out[b] != 0;
        fRefills++;
        if (sound)
            fSoundTransfers++;
    }
}

// input: what each packet brought, appended to the ring at the head
void PDUSBAudioEngine::drainTransfer(Transfer *t)
{
    const UInt8 *in = (const UInt8 *)t->buffer->getBytesNoCopy();
    UInt32 offset = 0;

    for (int k = 0; k < kPacketsPerTransfer; k++) {
        UInt32 got = t->frames[k].frStatus == kIOReturnSuccess ? t->frames[k].frActCount / fBytesPerFrame : 0;
        UInt32 act = t->frames[k].frActCount;

        fPackets++;
        if (!act)
            fEmptyPackets++;
        else if (act < t->frames[k].frReqCount)
            fShortPackets++;
        if (act < fMinActual)
            fMinActual = act;
        if (act > fMaxActual)
            fMaxActual = act;
        for (UInt32 b = 0; fBytesPerFrame && b + 1 < got * fBytesPerFrame; b += fFormat.subframeBytes) {
            SInt32 v = (SInt16)(in[offset + b + fFormat.subframeBytes - 2] | (in[offset + b + fFormat.subframeBytes - 1] << 8));
            UInt32 mag = v < 0 ? -v : v;
            if (mag > fPeakSample)
                fPeakSample = mag;
        }
        // the first packet with data, byte for byte, to check format and offset
        if (act >= 8 && !fDumped) {
            const UInt8 *q = in + offset;
            fDumped = true;
            kLog("first input packet: %u of %u bytes, status 0x%x: %02x %02x %02x %02x %02x %02x %02x %02x", act,
                 t->frames[k].frReqCount, t->frames[k].frStatus, q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7]);
        }

        for (UInt32 done = 0; done < got;) {
            UInt32 run = kRingFrames - fHead;
            if (run > got - done)
                run = got - done;
            memcpy(fRing + fHead * fBytesPerFrame, in + offset + done * fBytesPerFrame, run * fBytesPerFrame);
            done += run;
            advanceHead(run);
        }
        offset += t->frames[k].frReqCount;
    }
}

void PDUSBAudioEngine::submit(Transfer *t)
{
    IOUSBIsocCompletion c;
    IOReturn r;

    c.target = this;
    c.action = &PDUSBAudioEngine::transferDone;
    c.parameter = t;
    t->done = false;
    t->busy = true;
    OSIncrementAtomic(&fInFlight);
    retain();
    if (fInput)
        r = fPipe->Read(t->buffer, 0, kPacketsPerTransfer, t->frames, &c);
    else
        r = fPipe->Write(t->buffer, 0, kPacketsPerTransfer, t->frames, &c);
    if (r != kIOReturnSuccess) {
        t->busy = false;
        OSDecrementAtomic(&fInFlight);
        if (fLoggedErrors++ < 8)
            kLog("isochronous %s failed: 0x%x", fInput ? "read" : "write", r);
        release();
    }
}

// the controller's thread: hand the transfer back and let the workloop do the rest
void PDUSBAudioEngine::transferDone(void *target, void *parameter, IOReturn status, UInt32 remaining)
{
    PDUSBAudioEngine *self = (PDUSBAudioEngine *)target;
    Transfer *t = (Transfer *)parameter;

    t->status = status;
    t->doneTime = mach_absolute_time();
    t->done = true;
    OSSynchronizeIO();
    t->busy = false;
    OSDecrementAtomic(&self->fInFlight);
    if (self->fDone)
        self->fDone->interruptOccurred(NULL, NULL, 0);
    self->release();
}

void PDUSBAudioEngine::completionsReady(OSObject *owner, IOInterruptEventSource *source, int count)
{
    PDUSBAudioEngine *self = OSDynamicCast(PDUSBAudioEngine, owner);
    UInt64 now = mach_absolute_time(), ns;

    if (!self)
        return;
    if (self->fRunning && self->fLastService) {
        absolutetime_to_nanoseconds(now - self->fLastService, &ns);
        if (ns / 1000 > self->fMaxGapUs)
            self->fMaxGapUs = (UInt32)(ns / 1000);
        // a transfer lasts kPacketsPerTransfer ms, a service later than two of them is counted
        if (ns > 2 * kPacketsPerTransfer * 1000000ULL)
            self->fLateServices++;
        self->fServices++;
    }
    self->fLastService = now;
    if (self->fRunning && self->fInFlight < self->fMinQueued)
        self->fMinQueued = self->fInFlight;

    for (int i = 0; i < kTransfers; i++) {
        Transfer *t = &self->fTransfers[i];

        if (!t->done || t->busy)
            continue;
        t->done = false;
        if (t->status != kIOReturnSuccess && t->status != kIOReturnAborted && self->fLoggedErrors++ < 8)
            kLog("transfer finished with 0x%x", t->status);
        // the head moves at the time this transfer came back, however late the workloop got to it
        self->fClock = t->doneTime;
        // a transfer can come back while this loop runs, after now was read
        UInt64 at = mach_absolute_time();
        absolutetime_to_nanoseconds(at > t->doneTime ? at - t->doneTime : 0, &ns);
        if (self->fRunning && ns / 1000 > self->fMaxLatencyUs)
            self->fMaxLatencyUs = (UInt32)(ns / 1000);
        if (self->fInput && t->status != kIOReturnAborted)
            self->drainTransfer(t);
        if ((self->fRunning || self->fStreaming) && !self->fGone) {
            self->fillTransfer(t);
            self->submit(t);
        }
    }
    if (self->fFeedback.done && !self->fFeedback.busy) {
        self->fFeedback.done = false;
        self->handleFeedback();
        if (self->fRunning && !self->fGone)
            self->submitFeedback();
    }
}

// an async device reports its rate as samples per frame, 10.14 in three bytes at full speed, 16.16 at high speed
void PDUSBAudioEngine::submitFeedback()
{
    IOUSBIsocCompletion c;

    if (!fFeedbackPipe || !fFeedback.buffer)
        return;
    c.target = this;
    c.action = &PDUSBAudioEngine::transferDone;
    c.parameter = &fFeedback;
    fFeedback.frames[0].frReqCount = fPacketsPerSecond == 1000 ? 3 : 4;
    fFeedback.frames[0].frActCount = 0;
    fFeedback.frames[0].frStatus = 0;
    fFeedback.done = false;
    fFeedback.busy = true;
    OSIncrementAtomic(&fInFlight);
    retain();
    if (fFeedbackPipe->Read(fFeedback.buffer, 0, 1, fFeedback.frames, &c) != kIOReturnSuccess) {
        fFeedback.busy = false;
        OSDecrementAtomic(&fInFlight);
        release();
    }
}

void PDUSBAudioEngine::handleFeedback()
{
    const UInt8 *b = (const UInt8 *)fFeedback.buffer->getBytesNoCopy();
    UInt32 n = fFeedback.frames[0].frActCount, value;

    if (fFeedback.status != kIOReturnSuccess || n < 3)
        return;
    value = n >= 4 && fPacketsPerSecond != 1000 ? (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24))
                                                : ((b[0] | (b[1] << 8) | (b[2] << 16)) << 2);
    UInt32 nominal = (UInt32)(((UInt64)fFormat.rate << 16) / fPacketsPerSecond);
    // a value far from the nominal rate is a misread
    if (value > nominal - nominal / 16 && value < nominal + nominal / 16)
        fSamplesPerPacket = value;
}

void PDUSBAudioEngine::publishStats()
{
    setProperty("PDUSBAudioMaxServiceGapUs", fMaxGapUs, 32);
    setProperty("PDUSBAudioMinQueuedTransfers", (UInt32)(fMinQueued < 0 ? 0 : fMinQueued), 32);
    setProperty("PDUSBAudioQueuedTransfers", (UInt32)kTransfers, 32);
    setProperty("PDUSBAudioLateServices", fLateServices, 32);
    setProperty("PDUSBAudioMaxLatencyUs", fMaxLatencyUs, 32);
    kLog("%s: %u of %u refills carried sound, the head at %u", fInput ? "input" : "output", fSoundTransfers, fRefills, fHead);
    if (fInput)
        kLog("input: %u packets, %u empty, %u short, %u to %u bytes, peak sample %u of 32767", fPackets, fEmptyPackets,
             fShortPackets, fMinActual == 0xffffffff ? 0 : fMinActual, fMaxActual, fPeakSample);
    kLog("%s: %u services, the longest %u us apart, %u more than %u ms apart, completions waited at most %u us, "
         "%d of %d transfers still queued at worst", fInput ? "input" : "output", fServices, fMaxGapUs, fLateServices,
         2 * kPacketsPerTransfer, fMaxLatencyUs, fMinQueued, kTransfers);
}

void PDUSBAudioEngine::startStreaming()
{
    if (fInput || fGone || fStreaming)
        return;
    fStreaming = true;
    for (int i = 0; i < kTransfers; i++) {
        fillTransfer(&fTransfers[i]);
        submit(&fTransfers[i]);
    }
    submitFeedback();
}

IOReturn PDUSBAudioEngine::setProperties(OSObject *properties)
{
    OSDictionary *d = OSDynamicCast(OSDictionary, properties);
    OSBoolean *idle = d ? OSDynamicCast(OSBoolean, d->getObject("PDUSBAudioIdleStream")) : NULL;

    if (!idle)
        return super::setProperties(properties);
    if (idle->isTrue() && !fRunning)
        startStreaming();
    else if (!idle->isTrue())
        fStreaming = false;
    setProperty("PDUSBAudioIdleStream", idle->isTrue());
    kLog("idle stream %s", idle->isTrue() ? "on" : "off");
    return kIOReturnSuccess;
}

IOReturn PDUSBAudioEngine::performAudioEngineStart()
{
    if (fGone)
        return kIOReturnNoDevice;
    // a run starts from silence, what the last one left in the ring is not played again
    bzero(fRing, kRingFrames * fBytesPerFrame);
    fMaxGapUs = 0;
    fMinQueued = kTransfers;
    fLateServices = fServices = 0;
    fMaxLatencyUs = 0;
    fLastService = 0;
    fSoundTransfers = fRefills = 0;
    fPackets = fEmptyPackets = fShortPackets = fMaxActual = fPeakSample = 0;
    fMinActual = 0xffffffff;
    fDumped = false;
    kLog("%s start, %s, %d transfers with the controller", fInput ? "input" : "output",
         fStreaming && fInFlight > 0 ? "on the running stream" : "from idle", (int)fInFlight);
    // on a running stream the queued transfers still hold silence, the ring's frames go into the next refills
    if (fStreaming && fInFlight > 0) {
        fHead = 0;
        fFraction = 0;
        fClock = mach_absolute_time();
        AbsoluteTime now = fClock;
        takeTimeStamp(false, &now);
        fRunning = true;
        return kIOReturnSuccess;
    }
    // a stop that timed out leaves transfers with the controller, they come back before a new run
    if (!waitIdle(200))
        return kIOReturnBusy;
    PDUSBAudio *device = OSDynamicCast(PDUSBAudio, getProvider());
    if (fInput && device)
        device->openOtherUnits();
    // the alternate and the rate are set again for every run, the pipe the selection opens replaces the old one
    if (device && !fStreaming) {
        IOUSBPipe *pipe = NULL, *feedback = NULL;
        IOReturn r = device->selectStream(fInput, true, &pipe, &feedback);
        if (r != kIOReturnSuccess || !pipe)
            return r != kIOReturnSuccess ? r : kIOReturnNoDevice;
        pipe->retain();
        fPipe->release();
        fPipe = pipe;
        if (fFeedbackPipe) {
            fFeedbackPipe->release();
            fFeedbackPipe = NULL;
        }
        if (feedback) {
            feedback->retain();
            fFeedbackPipe = feedback;
        }
    }
    fHead = 0;
    fFraction = 0;
    fSamplesPerPacket = (UInt32)(((UInt64)fFormat.rate << 16) / fPacketsPerSecond);
    fMaxGapUs = 0;
    fMinQueued = kTransfers;
    fLateServices = fServices = 0;
    fMaxLatencyUs = 0;
    fLastService = 0;
    fRunning = true;
    fClock = mach_absolute_time();
    for (int i = 0; i < kTransfers; i++)
        fillTransfer(&fTransfers[i]);
    // the first output transfers took their frames at once, so the start is dated that many frames back
    AbsoluteTime now = fClock, past;
    nanoseconds_to_absolutetime((UInt64)fHead * 1000000000ULL / fFormat.rate, &past);
    now -= past;
    takeTimeStamp(false, &now);
    for (int i = 0; i < kTransfers; i++)
        submit(&fTransfers[i]);
    submitFeedback();
    return kIOReturnSuccess;
}

// true once the controller has handed back every transfer
bool PDUSBAudioEngine::waitIdle(UInt32 ms)
{
    for (UInt32 waited = 0; waited < ms; waited++) {
        if (fInFlight <= 0)
            return true;
        IOSleep(1);
    }
    return fInFlight <= 0;
}

// queued output plays out, a few milliseconds, then anything left is aborted
IOReturn PDUSBAudioEngine::performAudioEngineStop()
{
    fRunning = false;
    if (fStreaming) {
        publishStats();
        return kIOReturnSuccess;
    }
    if (!waitIdle(100)) {
        fPipe->Abort();
        if (fFeedbackPipe)
            fFeedbackPipe->Abort();
        if (!waitIdle(500))
            kLog("%d transfers still with the controller after an abort", (int)fInFlight);
    }
    // the stream goes back to alt 0 between runs, and only once the controller has every transfer back
    PDUSBAudio *device = OSDynamicCast(PDUSBAudio, getProvider());
    if (device && !fGone && fInFlight <= 0)
        device->selectStream(fInput, false, NULL, NULL);
    publishStats();
    return kIOReturnSuccess;
}

void PDUSBAudioEngine::shutDown()
{
    if (fGone)
        return;
    fGone = true;
    fRunning = false;
    fStreaming = false;
    // an abort completes every queued transfer, retried while the controller still holds some
    for (int tries = 0; tries < 10 && fInFlight > 0; tries++) {
        if (fPipe)
            fPipe->Abort();
        if (fFeedbackPipe)
            fFeedbackPipe->Abort();
        waitIdle(100);
    }
    if (fInFlight > 0)
        kLog("%d transfers still with the controller at shutdown", (int)fInFlight);
}

void PDUSBAudioEngine::stop(IOService *provider)
{
    shutDown();
    super::stop(provider);
}

UInt32 PDUSBAudioEngine::getCurrentSampleFrame()
{
    return fHead;
}

IOReturn PDUSBAudioEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame,
                                             UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat,
                                             IOAudioStream *audioStream)
{
    const float *in = (const float *)mixBuf;
    UInt32 ch = streamFormat->fNumChannels, first = firstSampleFrame * ch, n = numSampleFrames * ch;
    UInt32 width = streamFormat->fBitWidth / 8;
    UInt8 *out = (UInt8 *)sampleBuf + first * width;

    for (UInt32 i = 0; i < n; i++) {
        float s = in[first + i];

        if (s > 1.0f)
            s = 1.0f;
        else if (s < -1.0f)
            s = -1.0f;
        if (width == 2) {
            SInt16 v = (SInt16)(s * 32767.0f);
            out[0] = (UInt8)v;
            out[1] = (UInt8)(v >> 8);
        } else {
            SInt32 v = (SInt32)(s * 8388607.0f);
            out[0] = (UInt8)v;
            out[1] = (UInt8)(v >> 8);
            out[2] = (UInt8)(v >> 16);
        }
        out += width;
    }
    return kIOReturnSuccess;
}

// the ring's integer samples to the floats a client reads, the destination starts at its first frame
IOReturn PDUSBAudioEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame,
                                               UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat,
                                               IOAudioStream *audioStream)
{
    UInt32 ch = streamFormat->fNumChannels, n = numSampleFrames * ch;
    UInt32 width = streamFormat->fBitWidth / 8;
    const UInt8 *in = (const UInt8 *)sampleBuf + firstSampleFrame * ch * width;
    float *out = (float *)destBuf;

    for (UInt32 i = 0; i < n; i++, in += width) {
        if (width == 2)
            out[i] = (SInt16)(in[0] | (in[1] << 8)) / 32768.0f;
        else
            out[i] = (SInt32)((in[0] << 8) | (in[1] << 16) | (in[2] << 24)) / 2147483648.0f;
    }
    return kIOReturnSuccess;
}

#if TARGET_OS_OSX && TARGET_CPU_ARM64
bool PDUSBAudioEngine::driverDesiresHiResSampleIntervals(void)
{
    return true;
}
#endif

// every transfer holds a reference, so nothing here is still with the controller
void PDUSBAudioEngine::free()
{
    if (fDone) {
        if (getWorkLoop())
            getWorkLoop()->removeEventSource(fDone);
        fDone->release();
        fDone = NULL;
    }
    for (int i = 0; i < kTransfers; i++) {
        if (fTransfers[i].buffer)
            fTransfers[i].buffer->release();
        fTransfers[i].buffer = NULL;
    }
    if (fFeedback.buffer)
        fFeedback.buffer->release();
    if (fStream)
        fStream->release();
    if (fRingMem)
        fRingMem->release();
    if (fPipe)
        fPipe->release();
    if (fFeedbackPipe)
        fFeedbackPipe->release();
    super::free();
}
