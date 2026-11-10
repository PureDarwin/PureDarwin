// PDUSBAudio.h - a USB Audio Class 1.0 device: its AudioControl interface, feature unit and streams (USB Audio Class 1.0, section 4.3)
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef _PDUSBAUDIO_H
#define _PDUSBAUDIO_H

#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBPipe.h>

class PDUSBAudioEngine;
class IOAudioControl;

// the stream the device was set up with
struct PDUSBAudioFormat {
    UInt8 interfaceNumber;
    UInt8 alternate;
    UInt8 channels;
    UInt8 subframeBytes;
    UInt8 bits;
    UInt8 endpoint;
    UInt8 syncType;
    UInt8 feedbackEndpoint;
    UInt8 interval;
    bool input;
    bool rateControl;
    UInt16 maxPacket;
    UInt32 rate;
};

class PDUSBAudio : public IOAudioDevice
{
    OSDeclareDefaultStructors(PDUSBAudio);

public:
    bool init(OSDictionary *properties) override;
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool initHardware(IOService *provider) override;
    void stop(IOService *provider) override;
    IOReturn message(UInt32 type, IOService *provider, void *argument = 0) override;
    // the engine asks for these while it starts, before coreaudiod can see it
    void addControls(IOAudioEngine *engine);
    // unmutes the units without controls of their own at 0 dB, again at each input start since a wireless
    // headset can reset them when its link comes back
    void openOtherUnits();
    // the stream's alternate for a run, with the pipes it brings, or alt 0 after one
    IOReturn selectStream(bool input, bool on, IOUSBPipe **pipe, IOUSBPipe **feedback);

private:
    IOUSBInterface *fControl;
    IOUSBInterface *fStreaming;
    IOUSBInterface *fInStreaming;
    IOUSBDevice *fDevice;
    PDUSBAudioEngine *fEngine;
    PDUSBAudioEngine *fInEngine;
    PDUSBAudioFormat fFormat;
    PDUSBAudioFormat fInFormat;
    bool fHasOutput, fHasInput;
    UInt8 fFeatureUnit;
    bool fHasVolume, fHasMute;
    // every other feature unit, the capture path's among them, as id and master controls
    UInt8 fOtherUnits[8], fOtherControls[8];
    UInt32 fOtherCount;

    bool parseDescriptors();
    bool parseStreamingAlternate(const UInt8 *p, const UInt8 *end, UInt8 interfaceNumber, UInt8 alternate,
                                 PDUSBAudioFormat *out);
    IOUSBInterface *findInterface(UInt8 number);
    IOReturn classRequest(UInt8 type, UInt8 request, UInt16 value, UInt16 index, void *data, UInt16 length);
    void setSampleRate(const PDUSBAudioFormat &f);

    IOUSBInterface *openStream(const PDUSBAudioFormat &f, IOUSBPipe **pipe, IOUSBPipe **feedback);
    PDUSBAudioEngine *startEngine(const PDUSBAudioFormat &f, IOUSBPipe *pipe, IOUSBPipe *feedback, const char *name);
    static IOReturn volumeChanged(OSObject *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue);
    static IOReturn muteChanged(OSObject *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue);
};

#endif
