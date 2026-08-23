/*
 * RavynHDAudioEngine: IOAudioEngine over the HDA output stream owned by
 * RavynHDAudio. See RavynHDAudioEngine.h for the split.
 */

#include "RavynHDAudioEngine.h"
#include "RavynHDAudio.h"

#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/IOLib.h>

/* Kept in step with RavynHDAudio.cpp; the stream descriptor block layout is
 * fixed by the Intel HDA specification. */
#define HDA_SD_BASE     0x80
#define HDA_SD_SIZE     0x20
#define SD_CTL_STS      0x00
#define SD_LPIB         0x04
#define SD_CTL_RUN      (1u << 1)

/* The one format every HDA codec's default DAC is guaranteed to support, and
 * what RavynHDAudio programs the converter and SD_FORMAT for. The family
 * rate-converts and mixes clients into this. */
#define kHDASampleRate      48000
#define kHDABitsPerSample   16
#define kHDANumChannels     2
#define kHDABytesPerFrame   ((kHDABitsPerSample / 8) * kHDANumChannels)

#define super IOAudioEngine
OSDefineMetaClassAndStructors(RavynHDAudioEngine, IOAudioEngine);

uint32_t
RavynHDAudioEngine::sdBase() const
{
    return HDA_SD_BASE + fStreamIndex * HDA_SD_SIZE;
}

bool
RavynHDAudioEngine::initWithHDA(RavynHDAudio *device, volatile uint8_t *regs,
                                uint32_t streamIndex, void *pcmBuffer,
                                uint32_t pcmBytes)
{
    if (!super::init(NULL)) {
        return false;
    }

    if (device == NULL || regs == NULL || pcmBuffer == NULL || pcmBytes == 0) {
        return false;
    }

    fDevice      = device;
    fRegs        = regs;
    fStreamIndex = streamIndex;
    fPcmBuffer   = pcmBuffer;
    fPcmBytes    = pcmBytes;
    fOutputStream = NULL;

    return true;
}

bool
RavynHDAudioEngine::initHardware(IOService *provider)
{
    IOAudioStreamFormat format;
    IOAudioSampleRate rate;

    if (!super::initHardware(provider)) {
        return false;
    }

    setDescription("Intel HD Audio");

    /* The DMA ring is the family's sample buffer: clipOutputSamples() writes
     * converted frames straight into it and the hardware reads from it. */
    UInt32 numSampleFrames = fPcmBytes / kHDABytesPerFrame;

    fOutputStream = new IOAudioStream;
    if (fOutputStream == NULL) {
        return false;
    }
    if (!fOutputStream->initWithAudioEngine(this, kIOAudioStreamDirectionOutput,
                                            1 /* first channel */)) {
        fOutputStream->release();
        fOutputStream = NULL;
        return false;
    }

    bzero(&format, sizeof(format));
    format.fNumChannels          = kHDANumChannels;
    format.fSampleFormat         = kIOAudioStreamSampleFormatLinearPCM;
    format.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
    format.fBitDepth             = kHDABitsPerSample;
    format.fBitWidth             = kHDABitsPerSample;
    format.fAlignment            = kIOAudioStreamAlignmentLowByte;
    format.fByteOrder            = kIOAudioStreamByteOrderLittleEndian;
    format.fIsMixable            = true;
    format.fDriverTag            = 0;

    rate.whole = kHDASampleRate;
    rate.fraction = 0;

    fOutputStream->addAvailableFormat(&format, &rate, &rate);
    fOutputStream->setSampleBuffer(fPcmBuffer, fPcmBytes);
    fOutputStream->setFormat(&format);

    if (addAudioStream(fOutputStream) != kIOReturnSuccess) {
        fOutputStream->release();
        fOutputStream = NULL;
        return false;
    }

    setSampleRate(&rate);
    setNumSampleFramesPerBuffer(numSampleFrames);

    IOLog("RavynHDAudioEngine: %u sample frames, %u Hz, %u ch, %u-bit\n",
          (unsigned)numSampleFrames, (unsigned)kHDASampleRate,
          (unsigned)kHDANumChannels, (unsigned)kHDABitsPerSample);

    return true;
}

void
RavynHDAudioEngine::free()
{
    if (fOutputStream != NULL) {
        fOutputStream->release();
        fOutputStream = NULL;
    }
    /* fRegs and fPcmBuffer belong to RavynHDAudio. */
    super::free();
}

IOReturn
RavynHDAudioEngine::performAudioEngineStart()
{
    if (fRegs == NULL) {
        return kIOReturnNotReady;
    }

    /* The legacy /dev/dsp0 path may already own the DMA engine. */
    if (fDevice != NULL && !fDevice->claimStream(true)) {
        return kIOReturnBusy;
    }

    /* takeTimeStamp() with no argument stamps "now" as the point the engine's
     * sample counter starts from; the family uses it to derive playback
     * position for clients. It has to happen as close to starting the DMA as
     * possible. */
    takeTimeStamp(false);

    volatile uint32_t *ctl = (volatile uint32_t *)(fRegs + sdBase() + SD_CTL_STS);
    *ctl |= SD_CTL_RUN;

    return kIOReturnSuccess;
}

IOReturn
RavynHDAudioEngine::performAudioEngineStop()
{
    if (fRegs == NULL) {
        return kIOReturnSuccess;
    }

    volatile uint32_t *ctl = (volatile uint32_t *)(fRegs + sdBase() + SD_CTL_STS);
    *ctl &= ~SD_CTL_RUN;

    if (fDevice != NULL) {
        fDevice->releaseStream(true);
    }

    return kIOReturnSuccess;
}

UInt32
RavynHDAudioEngine::getCurrentSampleFrame()
{
    if (fRegs == NULL) {
        return 0;
    }

    /* LPIB is the controller's own link position in bytes, and it is the only
     * honest answer to "where is the hardware reading". It wraps at the cyclic
     * buffer length, which is exactly our buffer, so no accumulator is needed
     * here - the family wants a position within the buffer, not a total. */
    uint32_t lpib = *(volatile uint32_t *)(fRegs + sdBase() + SD_LPIB);

    if (lpib >= fPcmBytes) {
        lpib %= fPcmBytes;
    }

    return lpib / kHDABytesPerFrame;
}

IOReturn
RavynHDAudioEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf,
                                      UInt32 firstSampleFrame,
                                      UInt32 numSampleFrames,
                                      const IOAudioStreamFormat *streamFormat,
                                      IOAudioStream *audioStream)
{
    /* The family hands us its mix buffer as floats nominally in [-1.0, 1.0),
     * one per channel per frame, and expects the hardware format written into
     * sampleBuf at the same frame offset. Anything outside that range has to be
     * clamped rather than wrapped, or overdriven audio turns into noise. */
    const float *in = (const float *)mixBuf;
    SInt16 *out = (SInt16 *)sampleBuf;

    UInt32 numChannels = streamFormat->fNumChannels;
    UInt32 firstSample = firstSampleFrame * numChannels;
    UInt32 numSamples  = numSampleFrames * numChannels;

    for (UInt32 i = 0; i < numSamples; i++) {
        float sample = in[firstSample + i];

        if (sample > 1.0f) {
            sample = 1.0f;
        } else if (sample < -1.0f) {
            sample = -1.0f;
        }

        /* 32767 rather than 32768 so +1.0 does not wrap to negative. */
        out[firstSample + i] = (SInt16)(sample * 32767.0f);
    }

    return kIOReturnSuccess;
}

#if TARGET_OS_OSX && TARGET_CPU_ARM64
bool
RavynHDAudioEngine::driverDesiresHiResSampleIntervals(void)
{
    /* Ask the HAL for fixed-point sample intervals. getCurrentSampleFrame()
     * reports a byte-accurate hardware position from SD_LPIB, so the integer
     * intervals the alternative uses would throw that precision away - Apple's
     * header calls that "more error on ARM machines".
     *
     * UNTESTED on ARM: there is no arm64 target with an HDA controller here
     * yet, so this compiles for arm64 but has never run. */
    return true;
}
#endif
