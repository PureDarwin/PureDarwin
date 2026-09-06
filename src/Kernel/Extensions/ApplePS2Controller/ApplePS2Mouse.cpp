/*
 * ApplePS2Mouse: driver for the mouse nub published by ApplePS2Controller.
 * Decodes the 3- or 4-byte movement packet and pushes it to the raw
 * /dev/usb_hid_mouse queue, the same consumer IOUSBHIDMouse feeds.
 */
#include <IOKit/IOLib.h>
#include "ApplePS2Mouse.h"
#include "PDHIDEventQueue.h"

#define super IOService
OSDefineMetaClassAndStructors(ApplePS2Mouse, IOService);

/* Byte 0 of a movement packet. */
#define kPacketAlwaysOne    0x08
#define kPacketButtonMask   0x07
#define kPacketXSign        0x10
#define kPacketYSign        0x20

#define kMouseIdWheel       3       /* IntelliMouse: packets grow to 4 bytes */

bool ApplePS2Mouse::init(OSDictionary * dict)
{
    if (!super::init(dict)) {
        return false;
    }
    _device             = NULL;
    _interruptInstalled = false;
    _packetByte         = 0;
    _packetLength       = 3;
    _lastButtons        = 0;
    bzero(_packet, sizeof(_packet));
    return true;
}

ApplePS2Mouse * ApplePS2Mouse::probe(IOService * provider, SInt32 * score)
{
    if (!super::probe(provider, score)) {
        return NULL;
    }
    return this;
}

bool ApplePS2Mouse::start(IOService * provider)
{
    if (!super::start(provider)) {
        return false;
    }

    _device = OSDynamicCast(ApplePS2MouseDevice, provider);
    if (_device == NULL) {
        return false;
    }

    _mouseIndex = PDHIDAllocateMouseIndex();
    setProperty("Transport", "PS2");

    /*
     * The knock has to happen before the interrupt handler goes in: it reads
     * replies straight off the port, and stream data would be mistaken for one.
     */
    if (enableWheel()) {
        _packetLength = 4;
    }
    setSampleRate(100);
    sendCommand(kDP_SetMouseResolution);
    sendCommand(3);                 /* 8 counts/mm */
    sendCommand(kDP_SetMouseScaling1To1);
    sendCommand(kDP_SetMouseStreamMode);

    _device->installInterruptAction(this,
        (PS2InterruptAction)&ApplePS2Mouse::interruptOccurred);
    _interruptInstalled = true;

    setMouseEnable(true);

    /* Shared with the USB HID mouse: whichever starts first makes the node. */
    PDHIDPublishPS2MouseDevice();

    IOLog("ApplePS2Mouse: started ps2mouse%u (%u-byte packets)\n",
        _mouseIndex, _packetLength);
    return true;
}

void ApplePS2Mouse::stop(IOService * provider)
{
    setMouseEnable(false);
    if (_interruptInstalled) {
        _device->uninstallInterruptAction();
        _interruptInstalled = false;
    }
    _device = NULL;
    super::stop(provider);
}

bool ApplePS2Mouse::sendCommand(UInt8 command)
{
    PS2Request * request = _device->allocateRequest();
    bool         ok;

    if (request == NULL) {
        return false;
    }
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = command;
    request->commandsCount   = 1;
    request->completionTarget = NULL;
    request->completionAction = NULL;
    _device->submitRequestAndBlock(request);
    ok = (request->commandsCount == 1);
    _device->freeRequest(request);
    return ok;
}

bool ApplePS2Mouse::setSampleRate(UInt8 rate)
{
    return sendCommand(kDP_SetMouseSampleRate) && sendCommand(rate);
}

UInt8 ApplePS2Mouse::identify()
{
    PS2Request * request = _device->allocateRequest();
    UInt8        id = 0;

    if (request == NULL) {
        return 0;
    }
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = kDP_GetId;
    request->commands[1].command = kPS2C_ReadDataPort;
    request->commands[1].inOrOut = 0;
    request->commandsCount   = 2;
    request->completionTarget = NULL;
    request->completionAction = NULL;
    _device->submitRequestAndBlock(request);
    if (request->commandsCount == 2) {
        id = request->commands[1].inOrOut;
    }
    _device->freeRequest(request);
    return id;
}

bool ApplePS2Mouse::enableWheel()
{
    /* The Microsoft knock: three sample rates in this order, then ask again
     * who we are. A wheel mouse answers 3 instead of 0. */
    if (!setSampleRate(200) || !setSampleRate(100) || !setSampleRate(80)) {
        return false;
    }
    return identify() == kMouseIdWheel;
}

void ApplePS2Mouse::setMouseEnable(bool enable)
{
    PS2Request * request = _device->allocateRequest();
    if (request == NULL) {
        return;
    }
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = enable ? kDP_Enable : kDP_SetDefaultsAndDisable;
    request->commandsCount   = 1;
    request->completionTarget = NULL;
    request->completionAction = NULL;
    _device->submitRequest(request);   /* auto-freed (no completion routine) */
}

void ApplePS2Mouse::interruptOccurred(void * target, UInt8 data)
{
    ((ApplePS2Mouse *)target)->handlePacketByte(data);
}

void ApplePS2Mouse::handlePacketByte(UInt8 data)
{
    /*
     * Bit 3 of byte 0 is always set, which is the only way back into step with
     * the stream after a dropped byte.
     */
    if (_packetByte == 0 && !(data & kPacketAlwaysOne)) {
        return;
    }

    _packet[_packetByte++] = data;
    if (_packetByte < _packetLength) {
        return;
    }
    _packetByte = 0;
    dispatchPacket();
}

static SInt8 clampToSInt8(SInt32 value)
{
    if (value > 127) {
        return 127;
    }
    if (value < -127) {
        return -127;
    }
    return (SInt8)value;
}

void ApplePS2Mouse::dispatchPacket()
{
    UInt8  buttons = _packet[0] & kPacketButtonMask;
    SInt32 dx, dy, wheel = 0;

    /* Movement is 9-bit signed, with the high bit living in byte 0. */
    dx = (SInt32)_packet[1] - ((_packet[0] & kPacketXSign) ? 0x100 : 0);
    dy = (SInt32)_packet[2] - ((_packet[0] & kPacketYSign) ? 0x100 : 0);

    /* PS/2 counts Y upwards, HID counts it downwards. */
    dy = -dy;

    if (_packetLength == 4) {
        /* Wheel movement is a signed nibble, and turns the opposite way. */
        wheel = _packet[3] & 0x0F;
        if (wheel & 0x08) {
            wheel -= 0x10;
        }
        wheel = -wheel;
    }

    if (buttons == _lastButtons && dx == 0 && dy == 0 && wheel == 0) {
        return;
    }
    _lastButtons = buttons;

    PDHIDPushMouseEvent(_mouseIndex, buttons, clampToSInt8(dx), clampToSInt8(dy),
        clampToSInt8(wheel));
}
