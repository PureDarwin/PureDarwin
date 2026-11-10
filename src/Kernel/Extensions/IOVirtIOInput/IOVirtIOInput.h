/*
 * IOVirtIOInput.h - QEMU's virtio tablet, mouse and keyboard.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#ifndef _PD_IOVIRTIO_INPUT_H
#define _PD_IOVIRTIO_INPUT_H

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "IOVirtIOTransport.h"

// virtio_input_event: evdev type, code and value, little endian (VIRTIO 1.2 5.8.6)
struct VirtIOInputEvent {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} __attribute__((packed));

class IOVirtIOInput : public IOService
{
    OSDeclareDefaultStructors(IOVirtIOInput);

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

private:
    static const unsigned kEventCount = 64;

    uint8_t readConfig(uint8_t select, uint8_t subsel, void *out, uint8_t outLength);
    bool readAbsRange(uint8_t axis, int32_t *min, int32_t *max);
    void postEvent(unsigned slot);
    void handleEvent(const VirtIOInputEvent *event);
    void flushFrame();
    void pollEvents();
    static void pollTimerAction(OSObject *owner, IOTimerEventSource *sender);

    IOPCIDevice        *fPCIDevice;
    IOVirtIOTransport   fTransport;
    VirtQueue           fEventQ;
    IOBufferMemoryDescriptor *fEventBuf;
    IOWorkLoop         *fWorkLoop;
    IOTimerEventSource *fPollTimer;
    bool                fRunning;

    UInt8   fMouseIndex;
    bool    fHasAbs;
    int32_t fAbsMin[2], fAbsMax[2];

    // state gathered between two SYN_REPORT events
    int32_t fAbs[2];
    bool    fAbsChanged;
    int32_t fRelX, fRelY, fWheel;
    UInt8   fButtons, fSentButtons;
};

#endif // _PD_IOVIRTIO_INPUT_H
