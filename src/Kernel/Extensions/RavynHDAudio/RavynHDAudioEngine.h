/*
 * RavynHDAudioEngine: the IOAudioEngine half of the HDA driver.
 *
 * RavynHDAudio owns the PCI device, the controller (CORB/RIRB), the codec and
 * the DMA ring buffer; it hands this engine the pieces it needs and IOAudioFamily
 * drives the rest. The family mixes every client into a float mix buffer, calls
 * clipOutputSamples() to convert that into the hardware's format inside the DMA
 * buffer, and uses getCurrentSampleFrame() to know where the hardware is reading.
 *
 * That is what replaces the old /dev/dsp0 write path: instead of one process
 * writing 16-bit/48kHz/stereo into the ring by hand, any number of clients get
 * their own buffers, mixed and rate-converted by the family.
 */

#ifndef _RAVYNHDAUDIOENGINE_H
#define _RAVYNHDAUDIOENGINE_H

#include <IOKit/audio/IOAudioEngine.h>

class IOAudioStream;
class RavynHDAudio;

class RavynHDAudioEngine : public IOAudioEngine
{
    OSDeclareDefaultStructors(RavynHDAudioEngine);

public:
    /* The device stays the owner of the register mapping and the DMA buffer;
     * the engine only borrows them, so nothing here is freed on stop. */
    virtual bool initWithHDA(RavynHDAudio *device,
                             volatile uint8_t *regs,
                             uint32_t streamIndex,
                             void *pcmBuffer,
                             uint32_t pcmBytes);

    bool initHardware(IOService *provider) override;
    void free() override;

    IOReturn performAudioEngineStart() override;
    IOReturn performAudioEngineStop() override;

    UInt32 getCurrentSampleFrame() override;

    IOReturn clipOutputSamples(const void *mixBuf, void *sampleBuf,
                               UInt32 firstSampleFrame, UInt32 numSampleFrames,
                               const IOAudioStreamFormat *streamFormat,
                               IOAudioStream *audioStream) override;

    /* ARM-only, and deliberately pure virtual in IOAudioEngine.h - Apple's own
     * comment there notes it "only exists on ARM platforms and which therefore
     * breaks binary compatibility if this is compiled on Intel". So the guard
     * has to match theirs exactly or the class is abstract on arm64 and fine on
     * x86, which is precisely how this was first missed. */
#if TARGET_OS_OSX && TARGET_CPU_ARM64
    bool driverDesiresHiResSampleIntervals(void) override;
#endif

private:
    RavynHDAudio       *fDevice;
    volatile uint8_t   *fRegs;
    uint32_t            fStreamIndex;
    void               *fPcmBuffer;
    uint32_t            fPcmBytes;
    IOAudioStream      *fOutputStream;

    uint32_t sdBase() const;
};

#endif /* _RAVYNHDAUDIOENGINE_H */
