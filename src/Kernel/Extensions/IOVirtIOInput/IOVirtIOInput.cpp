/*
 * IOVirtIOInput.cpp - QEMU's virtio tablet, mouse and keyboard (VIRTIO 1.2, section 5.8).
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
// completions are timer-polled as in IOVirtIONet, events go into PD's raw HID queues
#include "IOVirtIOInput.h"
#include "PDHIDEventQueue.h"

#define super IOService
OSDefineMetaClassAndStructors(IOVirtIOInput, IOService);

enum {
    kCfgIdName  = 0x01,
    kCfgEvBits  = 0x11,
    kCfgAbsInfo = 0x12,
};

enum { kEvSyn = 0, kEvKey = 1, kEvRel = 2, kEvAbs = 3 };
enum { kRelX = 0, kRelY = 1, kRelWheel = 8 };
enum { kAbsX = 0, kAbsY = 1 };
enum { kBtnLeft = 0x110, kBtnRight = 0x111, kBtnMiddle = 0x112 };

// virtio_input_config, with the union at offset 8
enum { kCfgSelect = 0, kCfgSubsel = 1, kCfgSize = 2, kCfgUnion = 8 };

// evdev key code to USB HID keyboard usage
static const UInt8 gEvdevToUsage[128] = {
    [1] = 0x29, [2] = 0x1e, [3] = 0x1f, [4] = 0x20, [5] = 0x21, [6] = 0x22,
    [7] = 0x23, [8] = 0x24, [9] = 0x25, [10] = 0x26, [11] = 0x27, [12] = 0x2d,
    [13] = 0x2e, [14] = 0x2a, [15] = 0x2b, [16] = 0x14, [17] = 0x1a,
    [18] = 0x08, [19] = 0x15, [20] = 0x17, [21] = 0x1c, [22] = 0x18,
    [23] = 0x0c, [24] = 0x12, [25] = 0x13, [26] = 0x2f, [27] = 0x30,
    [28] = 0x28, [29] = 0xe0, [30] = 0x04, [31] = 0x16, [32] = 0x07,
    [33] = 0x09, [34] = 0x0a, [35] = 0x0b, [36] = 0x0d, [37] = 0x0e,
    [38] = 0x0f, [39] = 0x33, [40] = 0x34, [41] = 0x35, [42] = 0xe1,
    [43] = 0x31, [44] = 0x1d, [45] = 0x1b, [46] = 0x06, [47] = 0x19,
    [48] = 0x05, [49] = 0x11, [50] = 0x10, [51] = 0x36, [52] = 0x37,
    [53] = 0x38, [54] = 0xe5, [55] = 0x55, [56] = 0xe2, [57] = 0x2c,
    [58] = 0x39, [59] = 0x3a, [60] = 0x3b, [61] = 0x3c, [62] = 0x3d,
    [63] = 0x3e, [64] = 0x3f, [65] = 0x40, [66] = 0x41, [67] = 0x42,
    [68] = 0x43, [69] = 0x53, [70] = 0x47, [71] = 0x5f, [72] = 0x60,
    [73] = 0x61, [74] = 0x56, [75] = 0x5c, [76] = 0x5d, [77] = 0x5e,
    [78] = 0x57, [79] = 0x59, [80] = 0x5a, [81] = 0x5b, [82] = 0x62,
    [83] = 0x63, [86] = 0x64, [87] = 0x44, [88] = 0x45, [96] = 0x58,
    [97] = 0xe4, [98] = 0x54, [99] = 0x46, [100] = 0xe6, [102] = 0x4a,
    [103] = 0x52, [104] = 0x4b, [105] = 0x50, [106] = 0x4f, [107] = 0x4d,
    [108] = 0x51, [109] = 0x4e, [110] = 0x49, [111] = 0x4c, [113] = 0x7f,
    [114] = 0x81, [115] = 0x80, [117] = 0x67, [119] = 0x48, [125] = 0xe3,
    [126] = 0xe7, [127] = 0x65,};

bool IOVirtIOInput::start(IOService *provider)
{
    char name[64];

    if (!super::start(provider))
        return false;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;
    fPCIDevice->retain();
    if (!fPCIDevice->open(this)) {
        IOLog("IOVirtIOInput: failed to open PCI device\n");
        return false;
    }

    // events are polled, and an unacknowledged intx would hold a shared level line high
    fPCIDevice->configWrite16(kIOPCIConfigCommand,
        fPCIDevice->configRead16(kIOPCIConfigCommand) | kIOPCICommandInterruptDisable);

    if (!fTransport.attach(fPCIDevice)) {
        IOLog("IOVirtIOInput: failed to find/map virtio-pci capabilities\n");
        return false;
    }
    if (!fTransport.negotiateFeatures(0)) {
        IOLog("IOVirtIOInput: device rejected feature negotiation\n");
        return false;
    }

    bzero(name, sizeof(name));
    readConfig(kCfgIdName, 0, name, sizeof(name) - 1);
    fHasAbs = readAbsRange(kAbsX, &fAbsMin[0], &fAbsMax[0]) &&
              readAbsRange(kAbsY, &fAbsMin[1], &fAbsMax[1]);
    fMouseIndex = PDHIDAllocateMouseIndex();
    IOLog("IOVirtIOInput: \"%s\"%s\n", name, fHasAbs ? ", absolute" : "");
    if (name[0])
        setProperty("Product", name);

    if (!fTransport.initQueue(&fEventQ, 0, kEventCount)) {
        IOLog("IOVirtIOInput: failed to init the event queue\n");
        return false;
    }
    fEventBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        kEventCount * sizeof(VirtIOInputEvent), 0xFFFFFFFFULL);
    if (!fEventBuf)
        return false;
    bzero(fEventBuf->getBytesNoCopy(), kEventCount * sizeof(VirtIOInputEvent));
    // a fresh queue hands out descriptors in order, so slot i owns descriptor i
    for (unsigned i = 0; i < kEventCount; i++)
        postEvent(i);

    fTransport.setDriverOk();
    fTransport.notify(&fEventQ);

    PDHIDPublishMouseDevice();
    PDHIDPublishKeyboardDevice();

    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
        return false;
    fWorkLoop->retain();
    fPollTimer = IOTimerEventSource::timerEventSource(this, &IOVirtIOInput::pollTimerAction);
    if (!fPollTimer || fWorkLoop->addEventSource(fPollTimer) != kIOReturnSuccess) {
        IOLog("IOVirtIOInput: failed to create poll timer\n");
        return false;
    }
    fRunning = true;
    fPollTimer->setTimeoutMS(5);

    registerService();
    return true;
}

void IOVirtIOInput::stop(IOService *provider)
{
    fRunning = false;
    if (fPollTimer) {
        fPollTimer->cancelTimeout();
        if (fWorkLoop)
            fWorkLoop->removeEventSource(fPollTimer);
    }
    fTransport.detach();
    if (fPCIDevice)
        fPCIDevice->close(this);
    super::stop(provider);
}

void IOVirtIOInput::free()
{
    if (fPollTimer) { fPollTimer->release(); fPollTimer = NULL; }
    if (fWorkLoop) { fWorkLoop->release(); fWorkLoop = NULL; }
    if (fEventBuf) { fEventBuf->release(); fEventBuf = NULL; }
    if (fPCIDevice) { fPCIDevice->release(); fPCIDevice = NULL; }
    super::free();
}

// selects one config field and copies it out, returns its size (0 when absent)
uint8_t IOVirtIOInput::readConfig(uint8_t select, uint8_t subsel, void *out, uint8_t outLength)
{
    volatile uint8_t *cfg = fTransport.deviceConfig();
    uint8_t size;

    if (!cfg)
        return 0;
    cfg[kCfgSelect] = select;
    cfg[kCfgSubsel] = subsel;
    size = cfg[kCfgSize];
    for (uint8_t i = 0; i < size && i < outLength; i++)
        ((uint8_t *)out)[i] = cfg[kCfgUnion + i];
    return size;
}

bool IOVirtIOInput::readAbsRange(uint8_t axis, int32_t *min, int32_t *max)
{
    // virtio_input_absinfo: min, max, fuzz, flat, res
    int32_t info[5];

    bzero(info, sizeof(info));
    if (readConfig(kCfgAbsInfo, axis, info, sizeof(info)) < 8)
        return false;
    *min = OSSwapLittleToHostInt32(info[0]);
    *max = OSSwapLittleToHostInt32(info[1]);
    return *max > *min;
}

void IOVirtIOInput::postEvent(unsigned slot)
{
    VirtIOChainEntry entry = {
        fEventBuf->getPhysicalAddress() + slot * sizeof(VirtIOInputEvent),
        (uint32_t)sizeof(VirtIOInputEvent), true };
    fTransport.addDescChain(&fEventQ, &entry, 1);
}

void IOVirtIOInput::pollTimerAction(OSObject *owner, IOTimerEventSource *sender)
{
    IOVirtIOInput *self = OSDynamicCast(IOVirtIOInput, owner);

    if (!self)
        return;
    self->pollEvents();
    if (self->fRunning)
        sender->setTimeoutMS(5);
}

void IOVirtIOInput::pollEvents()
{
    VirtIOInputEvent *events = (VirtIOInputEvent *)fEventBuf->getBytesNoCopy();
    bool reposted = false;
    uint32_t len;
    uint16_t id;

    while (fTransport.pollForCompletion(&fEventQ, 0, &len, &id)) {
        if (id < kEventCount) {
            if (len >= sizeof(VirtIOInputEvent))
                handleEvent(&events[id]);
            postEvent(id);
            reposted = true;
        }
    }
    if (reposted)
        fTransport.notify(&fEventQ);
}

void IOVirtIOInput::handleEvent(const VirtIOInputEvent *event)
{
    uint16_t type = OSSwapLittleToHostInt16(event->type);
    uint16_t code = OSSwapLittleToHostInt16(event->code);
    int32_t value = (int32_t)OSSwapLittleToHostInt32(event->value);

    switch (type) {
    case kEvSyn:
        flushFrame();
        break;
    case kEvAbs:
        if (code == kAbsX || code == kAbsY) {
            fAbs[code] = value;
            fAbsChanged = true;
        }
        break;
    case kEvRel:
        if (code == kRelX) fRelX += value;
        else if (code == kRelY) fRelY += value;
        else if (code == kRelWheel) fWheel += value;
        break;
    case kEvKey:
        if (code >= kBtnLeft && code <= kBtnMiddle) {
            // the queue uses the HID boot order: left, right, middle
            UInt8 bit = (UInt8)(1u << (code - kBtnLeft));
            fButtons = value ? (fButtons | bit) : (fButtons & ~bit);
        } else if (code < sizeof(gEvdevToUsage) && gEvdevToUsage[code] && value != 2) {
            // value 2 is autorepeat, the compositor repeats keys itself
            PDHIDPushKeyboardEvent(gEvdevToUsage[code], value != 0);
        }
        break;
    }
}

static UInt16 scaleAxis(int32_t value, int32_t min, int32_t max)
{
    if (value <= min) return 0;
    if (value >= max) return 65535;
    return (UInt16)(((int64_t)(value - min) * 65535) / (max - min));
}

static SInt8 clampStep(int32_t value)
{
    return (SInt8)(value > 127 ? 127 : value < -127 ? -127 : value);
}

void IOVirtIOInput::flushFrame()
{
    if (fAbsChanged && fHasAbs) {
        PDHIDPushAbsoluteMouseEvent(fMouseIndex, fButtons,
                                    scaleAxis(fAbs[0], fAbsMin[0], fAbsMax[0]),
                                    scaleAxis(fAbs[1], fAbsMin[1], fAbsMax[1]),
                                    clampStep(fWheel));
        fWheel = 0;
        fSentButtons = fButtons;
    }
    // the queue carries signed bytes, so a large motion goes out in several steps
    while (fRelX || fRelY || fWheel || fButtons != fSentButtons) {
        SInt8 dx = clampStep(fRelX), dy = clampStep(fRelY), wheel = clampStep(fWheel);

        PDHIDPushMouseEvent(fMouseIndex, fButtons, dx, dy, wheel);
        fRelX -= dx;
        fRelY -= dy;
        fWheel -= wheel;
        fSentButtons = fButtons;
    }
    fAbsChanged = false;
}
