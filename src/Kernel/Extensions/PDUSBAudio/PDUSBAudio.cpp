// PDUSBAudio.cpp - USB Audio Class 1.0 descriptors, alternate and rate choice, and feature unit controls (USB Audio Class 1.0, sections 4.3, 4.5 and 5.2)
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "PDUSBAudio.h"
#include "PDUSBAudioEngine.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioTypes.h>

#define super IOAudioDevice
OSDefineMetaClassAndStructors(PDUSBAudio, IOAudioDevice);

// descriptor types and subtypes (audio 1.0 appendix A)
enum {
    kDescInterface = 4,
    kDescEndpoint = 5,
    kDescCSInterface = 0x24,
    kDescCSEndpoint = 0x25,
    kACHeader = 1,
    kACFeatureUnit = 6,
    kASGeneral = 1,
    kASFormatType = 2,
    kEPGeneral = 1,
    kAudioClass = 1,
    kSubclassControl = 1,
    kSubclassStreaming = 2,
    kFormatPCM = 1,
    kFormatTypeI = 1,
};

// class requests (audio 1.0 5.2.1) and feature unit control selectors (A.10.2)
enum {
    kSetCur = 0x01,
    kGetCur = 0x81,
    kGetMin = 0x82,
    kGetMax = 0x83,
    kGetRes = 0x84,
    kMuteControl = 1,
    kVolumeControl = 2,
    kSamplingFreqControl = 1,
    kReqInterfaceOut = 0x21,
    kReqInterfaceIn = 0xA1,
    kReqEndpointOut = 0x22,
    kReqEndpointIn = 0xA2,
};

#define kLog(fmt, ...) IOLog("PDUSBAudio: " fmt "\n", ##__VA_ARGS__)

static UInt32 rate24(const UInt8 *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16);
}

bool PDUSBAudio::init(OSDictionary *properties)
{
    if (!super::init(properties))
        return false;
    fControl = fStreaming = fInStreaming = NULL;
    fDevice = NULL;
    fEngine = fInEngine = NULL;
    bzero(&fFormat, sizeof(fFormat));
    bzero(&fInFormat, sizeof(fInFormat));
    fHasOutput = fHasInput = false;
    fOtherCount = 0;
    fFeatureUnit = 0;
    fHasVolume = fHasMute = false;
    return true;
}

IOService *PDUSBAudio::probe(IOService *provider, SInt32 *score)
{
    IOUSBInterface *i = OSDynamicCast(IOUSBInterface, provider);

    if (!i || i->GetInterfaceClass() != kAudioClass || i->GetInterfaceSubClass() != kSubclassControl)
        return NULL;
    return super::probe(provider, score);
}

// the device's interface nubs are its clients, FindNextInterface has no body in this IOUSBFamily
IOUSBInterface *PDUSBAudio::findInterface(UInt8 number)
{
    OSIterator *it = fDevice->getClientIterator();
    IOUSBInterface *found = NULL;
    OSObject *o;

    while (it && (o = it->getNextObject())) {
        IOUSBInterface *i = OSDynamicCast(IOUSBInterface, o);
        if (i && i->GetInterfaceNumber() == number) {
            found = i;
            break;
        }
    }
    if (it)
        it->release();
    return found;
}

IOReturn PDUSBAudio::classRequest(UInt8 type, UInt8 request, UInt16 value, UInt16 index, void *data, UInt16 length)
{
    IOUSBDevRequest r;

    r.bmRequestType = type;
    r.bRequest = request;
    r.wValue = value;
    r.wIndex = index;
    r.wLength = length;
    r.pData = data;
    r.wLenDone = 0;
    return fDevice->DeviceRequest(&r);
}

// one alternate setting of an AudioStreaming interface, from its interface descriptor up to the next one
bool PDUSBAudio::parseStreamingAlternate(const UInt8 *p, const UInt8 *end, UInt8 interfaceNumber, UInt8 alternate,
                                         PDUSBAudioFormat *out)
{
    PDUSBAudioFormat f;
    bool pcm = false, typeI = false, haveEndpoint = false;
    UInt32 best = 0;

    bzero(&f, sizeof(f));
    f.interfaceNumber = interfaceNumber;
    f.alternate = alternate;
    for (p += p[0]; p + 2 <= end && p[0] >= 2 && p + p[0] <= end && p[1] != kDescInterface; p += p[0]) {
        if (p[1] == kDescCSInterface && p[2] == kASGeneral && p[0] >= 7) {
            pcm = (p[5] | (p[6] << 8)) == kFormatPCM;
        } else if (p[1] == kDescCSInterface && p[2] == kASFormatType && p[0] >= 8 && p[3] == kFormatTypeI) {
            typeI = true;
            f.channels = p[4];
            f.subframeBytes = p[5];
            f.bits = p[6];
            UInt8 n = p[7];
            if (n == 0 && p[0] >= 14) {
                // a continuous range, 48 kHz when it fits
                UInt32 lo = rate24(p + 8), hi = rate24(p + 11);
                best = (lo <= 48000 && hi >= 48000) ? 48000 : (lo <= 44100 && hi >= 44100) ? 44100 : hi;
            }
            for (UInt8 k = 0; k < n && 8 + 3 * k + 3 <= p[0]; k++) {
                UInt32 r = rate24(p + 8 + 3 * k);
                bool better = r == 48000 || (best != 48000 && (r == 44100 || (best != 44100 && r > best)));
                if (better)
                    best = r;
            }
        } else if (p[1] == kDescEndpoint && p[0] >= 7) {
            UInt8 addr = p[2], attr = p[3];
            UInt16 size = p[4] | (p[5] << 8);
            bool in = (addr & 0x80) != 0;

            if ((attr & 3) != 1)
                continue;
            // an IN beside an OUT data endpoint, or one marked usage 1 (usb 2.0 9.6.6), is the feedback
            if (in && (((attr >> 4) & 3) == 1 || (haveEndpoint && !f.input))) {
                f.feedbackEndpoint = addr;
                continue;
            }
            if (haveEndpoint && !(f.input && !in))
                continue;
            if (haveEndpoint && f.input && !in)
                f.feedbackEndpoint = f.endpoint;
            f.endpoint = addr;
            f.input = in;
            f.maxPacket = size;
            f.syncType = (attr >> 2) & 3;
            f.interval = p[6];
            if (!in && p[0] >= 9 && p[8])
                f.feedbackEndpoint = p[8] | 0x80;
            haveEndpoint = true;
        } else if (p[1] == kDescCSEndpoint && p[2] == kEPGeneral && p[0] >= 4) {
            f.rateControl = (p[3] & 1) != 0;
        }
    }
    if (!pcm || !typeI || !haveEndpoint || !best || !f.channels)
        return false;
    if (f.subframeBytes != 2 && f.subframeBytes != 3)
        return false;
    f.rate = best;
    *out = f;
    return true;
}

// the configuration in one pass: the AudioControl header and feature unit, then every streaming alternate
bool PDUSBAudio::parseDescriptors()
{
    const IOUSBConfigurationDescriptor *cfg = fDevice->GetFullConfigurationDescriptor(0);
    UInt8 controlNumber = fControl->GetInterfaceNumber();
    UInt8 curNumber = 0xff, curClass = 0, curSub = 0;
    UInt16 adc = 0;
    bool found = false;
    // a device can carry several audio functions, this one streams only on the interfaces its header lists
    UInt8 owned[16];
    UInt32 ownedCount = 0;

    if (!cfg)
        return false;
    const UInt8 *base = (const UInt8 *)cfg, *end = base + cfg->wTotalLength;
    for (const UInt8 *p = base; p + 2 <= end && p[0] >= 2 && p + p[0] <= end; p += p[0]) {
        if (p[1] == kDescInterface && p[0] >= 9) {
            curNumber = p[2];
            curClass = p[5];
            curSub = p[6];
            bool mine = false;
            for (UInt32 k = 0; k < ownedCount; k++)
                mine |= owned[k] == p[2];
            if (curClass == kAudioClass && curSub == kSubclassStreaming && p[3] != 0 && mine) {
                PDUSBAudioFormat f;
                if (parseStreamingAlternate(p, end, p[2], p[3], &f)) {
                    PDUSBAudioFormat &best = f.input ? fInFormat : fFormat;
                    bool &have = f.input ? fHasInput : fHasOutput;
                    // stereo first, 16-bit next, then the higher rate
                    int score = (f.channels == 2 ? 8 : 0) + (f.subframeBytes == 2 ? 4 : 0) + (f.rate == 48000 ? 2 : 0);
                    int had = !have ? -1 : (best.channels == 2 ? 8 : 0) + (best.subframeBytes == 2 ? 4 : 0) +
                                               (best.rate == 48000 ? 2 : 0);
                    if (score > had) {
                        best = f;
                        have = true;
                        found = true;
                    }
                }
            }
        } else if (p[1] == kDescCSInterface && curNumber == controlNumber && curClass == kAudioClass) {
            if (p[2] == kACHeader && p[0] >= 5) {
                adc = p[3] | (p[4] << 8);
                for (UInt32 k = 0; p[0] >= 8 && k < p[7] && 8 + k < p[0] && ownedCount < 16; k++)
                    owned[ownedCount++] = p[8 + k];
            } else if (p[2] == kACFeatureUnit && p[0] >= 7) {
                UInt8 size = p[5];
                // the master channel, else the first logical channel
                UInt32 master = size ? p[6] : 0, first = size && p[0] >= 7 + 2 * size ? p[6 + size] : 0;
                UInt32 controls = master ? master : first;
                if ((controls & 3) && !fFeatureUnit) {
                    fFeatureUnit = p[3];
                    fHasMute = (controls & 1) != 0;
                    fHasVolume = (controls & 2) != 0;
                } else if ((controls & 3) && fOtherCount < 8) {
                    fOtherUnits[fOtherCount] = p[3];
                    fOtherControls[fOtherCount++] = (UInt8)controls;
                }
            }
        }
    }
    if (adc >= 0x200) {
        kLog("audio class %x.%02x is not supported yet, only 1.0", adc >> 8, adc & 0xff);
        return false;
    }
    if (!found) {
        kLog("no 16 or 24-bit PCM alternate");
        return false;
    }
    for (int k = 0; k < 2; k++) {
        const PDUSBAudioFormat &f = k ? fInFormat : fFormat;
        if (!(k ? fHasInput : fHasOutput))
            continue;
        kLog("%s: interface %u alt %u, %u ch, %u-bit in %u bytes, %u Hz, ep 0x%02x max %u, sync %u, feedback 0x%02x",
             k ? "input" : "output", f.interfaceNumber, f.alternate, f.channels, f.bits, f.subframeBytes, f.rate,
             f.endpoint, f.maxPacket, f.syncType, f.feedbackEndpoint);
    }
    kLog("feature unit %u%s%s", fFeatureUnit, fHasVolume ? ", volume" : "", fHasMute ? ", mute" : "");
    return true;
}

// the endpoint takes the rate when it has the control, a fixed-rate device needs nothing
void PDUSBAudio::setSampleRate(const PDUSBAudioFormat &f)
{
    UInt8 data[3] = { (UInt8)f.rate, (UInt8)(f.rate >> 8), (UInt8)(f.rate >> 16) };

    if (!f.rateControl)
        return;
    IOReturn r = classRequest(kReqEndpointOut, kSetCur, kSamplingFreqControl << 8, f.endpoint, data, 3);
    UInt8 back[3] = { 0, 0, 0 };
    classRequest(kReqEndpointIn, kGetCur, kSamplingFreqControl << 8, f.endpoint, back, 3);
    kLog("0x%02x at %u Hz: set 0x%x, reads %u Hz", f.endpoint, f.rate, r, rate24(back));
}

// a stream's alternate is chosen right before it runs and dropped after, as hosts do: alt 0, the streaming
// alternate, then the rate. the selection replaces the interface's pipes, the new ones are handed back
IOReturn PDUSBAudio::selectStream(bool input, bool on, IOUSBPipe **pipe, IOUSBPipe **feedback)
{
    const PDUSBAudioFormat &f = input ? fInFormat : fFormat;
    IOUSBInterface *i = input ? fInStreaming : fStreaming;
    IOUSBFindEndpointRequest req;

    if (pipe)
        *pipe = NULL;
    if (feedback)
        *feedback = NULL;
    if (!i)
        return kIOReturnNotOpen;
    IOReturn r = i->SetAlternateInterface(this, 0);
    if (!on || r != kIOReturnSuccess)
        return r;
    if ((r = i->SetAlternateInterface(this, f.alternate)) != kIOReturnSuccess) {
        kLog("interface %u alt %u failed: 0x%x", f.interfaceNumber, f.alternate, r);
        return r;
    }
    setSampleRate(f);
    req.type = kUSBIsoc;
    req.direction = f.input ? kUSBIn : kUSBOut;
    req.maxPacketSize = 0;
    req.interval = 0;
    if (pipe)
        *pipe = i->FindNextPipe(NULL, &req);
    if (feedback && !f.input && f.syncType == 1 && f.feedbackEndpoint) {
        req.direction = kUSBIn;
        *feedback = i->FindNextPipe(NULL, &req);
    }
    return pipe && !*pipe ? kIOReturnNoDevice : kIOReturnSuccess;
}

// the units without a control of their own, a microphone's among them, are left open: unmuted at 0 dB, or at
// their top step when that is lower (audio 1.0 5.2.2.4.3)
void PDUSBAudio::openOtherUnits()
{
    UInt8 acIf = fControl->GetInterfaceNumber();

    for (UInt32 k = 0; k < fOtherCount; k++) {
        UInt16 index = (fOtherUnits[k] << 8) | acIf;
        IOReturn rm = kIOReturnSuccess, rv = kIOReturnSuccess;
        UInt8 mute = 0xff;
        SInt16 cur = 0, level = 0;

        if (fOtherControls[k] & 1) {
            UInt8 off = 0;
            rm = classRequest(kReqInterfaceOut, kSetCur, kMuteControl << 8, index, &off, 1);
            classRequest(kReqInterfaceIn, kGetCur, kMuteControl << 8, index, &mute, 1);
        }
        if (fOtherControls[k] & 2) {
            SInt16 hi = 0;
            rv = classRequest(kReqInterfaceIn, kGetMax, kVolumeControl << 8, index, &hi, 2);
            if (rv == kIOReturnSuccess) {
                level = OSSwapLittleToHostInt16(hi) < 0 ? OSSwapLittleToHostInt16(hi) : 0;
                UInt8 data[2] = { (UInt8)level, (UInt8)((UInt16)level >> 8) };
                rv = classRequest(kReqInterfaceOut, kSetCur, kVolumeControl << 8, index, data, 2);
            }
            classRequest(kReqInterfaceIn, kGetCur, kVolumeControl << 8, index, &cur, 2);
            cur = OSSwapLittleToHostInt16(cur);
        }
        kLog("feature unit %u: unmute 0x%x, mute now %u, level %d/256 dB set 0x%x, now %d/256 dB", fOtherUnits[k], rm,
             mute, level, rv, cur);
    }
}

// the streaming interface at its alternate, with the data pipe and, for async output, the feedback pipe
IOUSBInterface *PDUSBAudio::openStream(const PDUSBAudioFormat &f, IOUSBPipe **pipe, IOUSBPipe **feedback)
{
    IOUSBFindEndpointRequest req;
    IOUSBInterface *i = findInterface(f.interfaceNumber);

    *pipe = *feedback = NULL;
    if (!i) {
        kLog("streaming interface %u has no nub", f.interfaceNumber);
        return NULL;
    }
    if (!i->open(this)) {
        kLog("streaming interface %u is in use", f.interfaceNumber);
        return NULL;
    }
    i->retain();
    IOReturn r = i->SetAlternateInterface(this, f.alternate);
    if (r == kIOReturnSuccess) {
        setSampleRate(f);
        req.type = kUSBIsoc;
        req.direction = f.input ? kUSBIn : kUSBOut;
        req.maxPacketSize = 0;
        req.interval = 0;
        *pipe = i->FindNextPipe(NULL, &req);
        if (*pipe && !f.input && f.syncType == 1 && f.feedbackEndpoint) {
            req.direction = kUSBIn;
            *feedback = i->FindNextPipe(NULL, &req);
        }
    }
    if (!*pipe) {
        kLog("interface %u alt %u gave no isochronous pipe (0x%x)", f.interfaceNumber, f.alternate, r);
        i->close(this);
        i->release();
        return NULL;
    }
    return i;
}

PDUSBAudioEngine *PDUSBAudio::startEngine(const PDUSBAudioFormat &f, IOUSBPipe *pipe, IOUSBPipe *feedback, const char *name)
{
    PDUSBAudioEngine *e = new PDUSBAudioEngine;
    char location[24];

    if (!e || !e->initWithFormat(f, pipe, feedback, fDevice->GetSpeed() == kUSBDeviceSpeedHigh, f.input)) {
        if (e) e->release();
        return NULL;
    }
    // vendor, product and the streaming interface name the stream, the same again after a replug
    snprintf(location, sizeof(location), "%04x:%04x:%u", fDevice->GetVendorID(), fDevice->GetProductID(), f.interfaceNumber);
    e->setLocation(location);
    if (activateAudioEngine(e) != kIOReturnSuccess) {
        e->release();
        return NULL;
    }
    // the stream rests at alt 0 and runs only while a client plays, endless silence stops some wireless
    // headsets playing what follows (PDUSBAudioIdleStream turns it on)
    selectStream(f.input, false, NULL, NULL);
    kLog("%s %s ready", name, f.input ? "input" : "output");
    return e;
}

IOReturn PDUSBAudio::volumeChanged(OSObject *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue)
{
    PDUSBAudio *self = OSDynamicCast(PDUSBAudio, target);
    SInt16 v = (SInt16)newValue;
    UInt8 data[2] = { (UInt8)v, (UInt8)((UInt16)v >> 8) };

    if (!self || !self->fDevice)
        return kIOReturnBadArgument;
    return self->classRequest(kReqInterfaceOut, kSetCur, kVolumeControl << 8,
                              (self->fFeatureUnit << 8) | self->fControl->GetInterfaceNumber(), data, 2);
}

IOReturn PDUSBAudio::muteChanged(OSObject *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue)
{
    PDUSBAudio *self = OSDynamicCast(PDUSBAudio, target);
    UInt8 data = newValue ? 1 : 0;

    if (!self || !self->fDevice)
        return kIOReturnBadArgument;
    return self->classRequest(kReqInterfaceOut, kSetCur, kMuteControl << 8,
                              (self->fFeatureUnit << 8) | self->fControl->GetInterfaceNumber(), &data, 1);
}

// the feature unit's master volume is in 1/256 dB steps, the level control counts in those units
void PDUSBAudio::addControls(IOAudioEngine *engine)
{
    UInt16 index = (fFeatureUnit << 8) | fControl->GetInterfaceNumber();

    if (fHasVolume) {
        SInt16 lo = 0, hi = 0, cur = 0, res = 0;
        bool ok = classRequest(kReqInterfaceIn, kGetMin, kVolumeControl << 8, index, &lo, 2) == kIOReturnSuccess &&
                  classRequest(kReqInterfaceIn, kGetMax, kVolumeControl << 8, index, &hi, 2) == kIOReturnSuccess;
        classRequest(kReqInterfaceIn, kGetRes, kVolumeControl << 8, index, &res, 2);
        if (classRequest(kReqInterfaceIn, kGetCur, kVolumeControl << 8, index, &cur, 2) != kIOReturnSuccess)
            cur = hi;
        lo = OSSwapLittleToHostInt16(lo);
        hi = OSSwapLittleToHostInt16(hi);
        cur = OSSwapLittleToHostInt16(cur);
        // 0x8000 is the spec's silence and not a level, the bottom step sits one resolution above it
        if (lo == (SInt16)0x8000)
            lo = (SInt16)(lo + (res > 0 ? res : 1));
        if (ok && hi > lo) {
            IOAudioLevelControl *volume = IOAudioLevelControl::createVolumeControl(cur, lo, hi, (IOFixed)lo << 8, (IOFixed)hi << 8,
                kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageOutput);
            if (volume) {
                volume->setValueChangeHandler(volumeChanged, this);
                engine->addDefaultAudioControl(volume);
                volume->release();
            }
            kLog("feature unit %u volume %d..%d (1/256 dB), step %d", fFeatureUnit, lo, hi, res);
        }
    }
    if (fHasMute) {
        IOAudioToggleControl *mute = IOAudioToggleControl::createMuteControl(false, kIOAudioControlChannelIDAll,
            kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageOutput);
        if (mute) {
            mute->setValueChangeHandler(muteChanged, this);
            engine->addDefaultAudioControl(mute);
            mute->release();
        }
    }
}

bool PDUSBAudio::initHardware(IOService *provider)
{
    char name[64] = "", maker[64] = "";
    IOUSBPipe *pipe = NULL, *feedback = NULL;

    if (!super::initHardware(provider))
        return false;
    fControl = OSDynamicCast(IOUSBInterface, provider);
    if (!fControl || !(fDevice = fControl->GetDevice()))
        return false;
    fControl->retain();
    fDevice->retain();
    if (!fControl->open(this))
        return false;
    if (!parseDescriptors())
        return false;

    if (fDevice->GetProductStringIndex())
        fDevice->GetStringDescriptor(fDevice->GetProductStringIndex(), name, sizeof(name));
    if (fDevice->GetManufacturerStringIndex())
        fDevice->GetStringDescriptor(fDevice->GetManufacturerStringIndex(), maker, sizeof(maker));
    setDeviceName(name[0] ? name : "USB Audio");
    setDeviceShortName("USB");
    setManufacturerName(maker[0] ? maker : "USB");
    setDeviceTransportType(kIOAudioDeviceTransportTypeUSB);

    if (fHasOutput && (fStreaming = openStream(fFormat, &pipe, &feedback)))
        fEngine = startEngine(fFormat, pipe, feedback, name[0] ? name : "USB Audio");
    if (fHasInput && (fInStreaming = openStream(fInFormat, &pipe, &feedback)))
        fInEngine = startEngine(fInFormat, pipe, NULL, name[0] ? name : "USB Audio");
    openOtherUnits();
    return fEngine || fInEngine;
}

// an unplug stops the stream before the pipes go, so no transfer completes into freed memory
// an unplug stops the streams before the pipes go, so no transfer completes into freed memory
IOReturn PDUSBAudio::message(UInt32 type, IOService *provider, void *argument)
{
    if (type == kIOMessageServiceIsTerminated || type == kIOMessageServiceIsRequestingClose) {
        if (fEngine)
            fEngine->shutDown();
        if (fInEngine)
            fInEngine->shutDown();
    }
    return super::message(type, provider, argument);
}

void PDUSBAudio::stop(IOService *provider)
{
    PDUSBAudioEngine *engines[2] = { fEngine, fInEngine };
    IOUSBInterface *streams[2] = { fStreaming, fInStreaming };

    for (int k = 0; k < 2; k++) {
        if (engines[k]) {
            engines[k]->shutDown();
            engines[k]->release();
        }
        if (streams[k]) {
            streams[k]->close(this);
            streams[k]->release();
        }
    }
    fEngine = fInEngine = NULL;
    fStreaming = fInStreaming = NULL;
    if (fControl) {
        fControl->close(this);
        fControl->release();
        fControl = NULL;
    }
    if (fDevice) {
        fDevice->release();
        fDevice = NULL;
    }
    super::stop(provider);
}
