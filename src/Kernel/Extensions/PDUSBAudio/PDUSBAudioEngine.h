// PDUSBAudioEngine.h - the IOAudioEngine of one USB audio stream, moved by isochronous transfers (USB Audio Class 1.0, section 3.7)
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef _PDUSBAUDIOENGINE_H
#define _PDUSBAUDIOENGINE_H

#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/usb/IOUSBPipe.h>
#include "PDUSBAudio.h"

class IOAudioStream;

class PDUSBAudioEngine : public IOAudioEngine
{
    OSDeclareDefaultStructors(PDUSBAudioEngine);

public:
    bool initWithFormat(const PDUSBAudioFormat &format, IOUSBPipe *pipe, IOUSBPipe *feedback, bool highSpeed, bool input);
    bool initHardware(IOService *provider) override;
    void free() override;
    void stop(IOService *provider) override;

    IOReturn performAudioEngineStart() override;
    IOReturn performAudioEngineStop() override;
    UInt32 getCurrentSampleFrame() override;
    IOReturn clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames,
                               const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream) override;
    IOReturn convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames,
                                 const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream) override;
#if TARGET_OS_OSX && TARGET_CPU_ARM64
    bool driverDesiresHiResSampleIntervals(void) override;
#endif

    // the device is going away: nothing more is queued, and every transfer the controller holds is
    // aborted and waited for, so the buffers are always released
    void shutDown();
    // output can keep the device streaming silence between runs, a run then starts on a running stream
    void startStreaming();
    // PDUSBAudioIdleStream turns the silence between runs on or off, off by default
    IOReturn setProperties(OSObject *properties) override;

private:
    // eight transfers of four 1 ms packets keep 32 ms queued, a refill can be 28 ms late before the
    // controller runs out
    enum { kTransfers = 8, kPacketsPerTransfer = 4, kRingFrames = 8192 };
    struct Transfer {
        PDUSBAudioEngine *engine;
        IOBufferMemoryDescriptor *buffer;
        IOUSBIsocFrame frames[kPacketsPerTransfer];
        // the controller owns it while busy, done marks a completion the workloop has not handled yet
        volatile bool busy;
        volatile bool done;
        IOReturn status;
        // when the controller handed it back, the time the head moves at for this transfer
        UInt64 doneTime;
    };

    PDUSBAudioFormat fFormat;
    bool fInput;
    IOUSBPipe *fPipe;
    IOUSBPipe *fFeedbackPipe;
    IOAudioStream *fStream;
    IOBufferMemoryDescriptor *fRingMem;
    UInt8 *fRing;
    UInt32 fBytesPerFrame;
    UInt32 fPacketsPerSecond;
    Transfer fTransfers[kTransfers];
    Transfer fFeedback;
    IOInterruptEventSource *fDone;
    volatile bool fRunning;
    volatile bool fStreaming;
    volatile bool fGone;
    volatile SInt32 fInFlight;
    // the ring frame the next packet starts at or lands at, the family sees it as the hardware position
    volatile UInt32 fHead;
    // samples per packet in 16.16, nominal or from the feedback endpoint
    UInt32 fSamplesPerPacket;
    UInt32 fFraction;
    UInt32 fLoggedErrors;
    // how late the workloop got to completions, and how little was left queued when it did
    UInt64 fLastService;
    UInt32 fMaxGapUs;
    SInt32 fMinQueued;
    UInt32 fLateServices;
    UInt32 fMaxLatencyUs;
    // how many refills of this run carried a non-zero sample, out of how many
    UInt32 fSoundTransfers, fRefills;
    // input packets of this run: how many, how many came back empty or short, their sizes and the loudest sample
    UInt32 fPackets, fEmptyPackets, fShortPackets, fMinActual, fMaxActual, fPeakSample;
    bool fDumped;
    UInt32 fServices;
    // the host time the head moves at, a completion's or the start's
    UInt64 fClock;

    void advanceHead(UInt32 frames);
    void fillTransfer(Transfer *t);
    void drainTransfer(Transfer *t);
    void submit(Transfer *t);
    void submitFeedback();
    void handleFeedback();
    void publishStats();
    static void transferDone(void *target, void *parameter, IOReturn status, UInt32 remaining);
    static void completionsReady(OSObject *owner, IOInterruptEventSource *source, int count);
    bool waitIdle(UInt32 ms);
};

#endif
