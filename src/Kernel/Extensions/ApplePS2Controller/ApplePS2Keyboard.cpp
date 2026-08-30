/*
 * ApplePS2Keyboard: driver for the keyboard nub published by
 * ApplePS2Controller. Decodes i8042 set-1 scan codes into USB HID usages and
 * feeds them to the raw /dev/usb_hid_kbd queue and to IOHIKeyboard, the same
 * two consumers the USB boot-protocol keyboard uses.
 *
 * Modeled on Apple's historic ApplePS2Keyboard (APSL); rewritten for the
 * PureDarwin bring-up against IOHIDFamily-1633's IOHIKeyboard.
 */
#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include "ApplePS2Keyboard.h"
#include "PDHIDKeyboardMap.h"
#include "PDHIDEventQueue.h"

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(ApplePS2Keyboard, IOHIKeyboard);

#define NOUSAGE 0x00    /* HID reserves usage 0 for "no key" */

/*
 * PC set-1 (XT) make code -> USB HID keyboard usage, US layout.
 * Index is the make code (0x00-0x7F); break codes have bit 7 set.
 */
static const UInt8 PS2ToUSBMap[kPS2ScanCodeCount] =
{
/* 00-07: -,esc,1,2,3,4,5,6 */
    NOUSAGE, 0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
/* 08-0f: 7,8,9,0,-,=,bs,tab */
    0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A, 0x2B,
/* 10-17: q,w,e,r,t,y,u,i */
    0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C,
/* 18-1f: o,p,[,],ret,Lctrl,a,s */
    0x12, 0x13, 0x2F, 0x30, 0x28, 0xE0, 0x04, 0x16,
/* 20-27: d,f,g,h,j,k,l,; */
    0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33,
/* 28-2f: ',`,Lshift,\,z,x,c,v */
    0x34, 0x35, 0xE1, 0x31, 0x1D, 0x1B, 0x06, 0x19,
/* 30-37: b,n,m,comma,period,slash,Rshift,kp* */
    0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0xE5, 0x55,
/* 38-3f: Lalt,space,caps,F1,F2,F3,F4,F5 */
    0xE2, 0x2C, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
/* 40-47: F6,F7,F8,F9,F10,numlock,scrolllock,kp7 */
    0x3F, 0x40, 0x41, 0x42, 0x43, 0x53, 0x47, 0x5F,
/* 48-4f: kp8,kp9,kp-,kp4,kp5,kp6,kp+,kp1 */
    0x60, 0x61, 0x56, 0x5C, 0x5D, 0x5E, 0x57, 0x59,
/* 50-57: kp2,kp3,kp0,kp.,-,-,ISO backslash,F11 */
    0x5A, 0x5B, 0x62, 0x63, NOUSAGE, NOUSAGE, 0x64, 0x44,
/* 58-5f: F12 */
    0x45, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 60-67 */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 68-6f */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 70-77 */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 78-7f */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE
};

/*
 * The same, for make codes carrying an 0xE0 prefix: the keys the XT keyboard
 * did not have.
 */
static const UInt8 PS2ExtendedToUSBMap[kPS2ScanCodeCount] =
{
/* 00-07 */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 08-0f */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 10-17 */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 18-1f: kp-enter,Rctrl */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, 0x58, 0xE4, NOUSAGE, NOUSAGE,
/* 20-27 */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 28-2f */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 30-37: kp/,printscreen */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, 0x54, NOUSAGE, 0x46,
/* 38-3f: Ralt */
    0xE6, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 40-47: home */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, 0x4A,
/* 48-4f: up,pgup,left,right,end */
    0x52, 0x4B, NOUSAGE, 0x50, NOUSAGE, 0x4F, NOUSAGE, 0x4D,
/* 50-57: down,pgdn,insert,delete */
    0x51, 0x4E, 0x49, 0x4C, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 58-5f: Lgui,Rgui,application */
    NOUSAGE, NOUSAGE, NOUSAGE, 0xE3, 0xE7, 0x65, NOUSAGE, NOUSAGE,
/* 60-67 */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 68-6f */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 70-77 */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE,
/* 78-7f */
    NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE, NOUSAGE
};

bool ApplePS2Keyboard::init(OSDictionary * dict)
{
    if (!super::init(dict)) {
        return false;
    }
    _device             = NULL;
    _extended           = false;
    _pauseCountdown     = 0;
    _interruptInstalled = false;
    _consoleGrabbed     = false;
    bzero(_usageDown, sizeof(_usageDown));
    return true;
}

ApplePS2Keyboard * ApplePS2Keyboard::probe(IOService * provider, SInt32 * score)
{
    /* The controller guarantees the keyboard is present and quiesced when it
     * publishes the nub, so no reset-and-probe dance here (QEMU's i8042 and
     * real translated controllers both speak set 1 by default). */
    if (!super::probe(provider, score)) {
        return NULL;
    }
    return this;
}

bool ApplePS2Keyboard::start(IOService * provider)
{
    if (!super::start(provider)) {
        return false;
    }

    _device = OSDynamicCast(ApplePS2KeyboardDevice, provider);
    if (_device == NULL) {
        return false;
    }

    setProperty("Transport", "PS2");

    _device->installInterruptAction(this,
        (PS2InterruptAction)&ApplePS2Keyboard::interruptOccurred);
    _interruptInstalled = true;

    setKeyboardEnable(true);

    /* Shared with the USB HID keyboard: whichever starts first makes the node. */
    PDHIDPublishKeyboardDevice();

    IOLog("ApplePS2Keyboard: started (set-1 scancodes -> HID usages)\n");
    return true;
}

void ApplePS2Keyboard::stop(IOService * provider)
{
    setKeyboardEnable(false);
    if (_interruptInstalled) {
        _device->uninstallInterruptAction();
        _interruptInstalled = false;
    }
    _device = NULL;
    super::stop(provider);
}

void ApplePS2Keyboard::interruptOccurred(void * target, UInt8 data)
{
    ((ApplePS2Keyboard *)target)->decodeScancode(data);
}

void ApplePS2Keyboard::decodeScancode(UInt8 data)
{
    UInt8 usage;
    UInt8 code;
    bool  goingDown;

    /*
     * Pause arrives as the fixed sequence E1 1D 45 / E1 9D C5; decoded byte by
     * byte it reads as a control press that never comes back up.
     */
    if (_pauseCountdown != 0) {
        _pauseCountdown--;
        if (_pauseCountdown == 0 && !(data & kSC_UpBit)) {
            dispatchUsage(0x48, true);
            dispatchUsage(0x48, false);
        }
        return;
    }
    if (data == kSC_Pause) {
        _pauseCountdown = 2;
        _extended = false;
        return;
    }

    if (data == kSC_Extend) {
        _extended = true;
        return;
    }

    /* ACK/resend chatter left over from a command: not a key. */
    if (data == kSC_Acknowledge || data == kSC_Resend) {
        _extended = false;
        return;
    }

    goingDown = !(data & kSC_UpBit);
    code      = data & ~kSC_UpBit;

    if (_extended) {
        _extended = false;
        /*
         * Print Screen and the keypad bracket themselves with a shift the user
         * never pressed (E0 2A / E0 B7), which would flip the case of text.
         */
        if (code == kSC_ShiftLeft || code == kSC_ShiftRight) {
            return;
        }
        usage = PS2ExtendedToUSBMap[code];
    } else {
        usage = PS2ToUSBMap[code];
    }

    if (usage == NOUSAGE) {
        return;
    }

    /*
     * A held key repeats by resending its make code, but IOHIKeyboard and the
     * queue's clients both want one event per transition.
     */
    if (isUsageDown(usage) == goingDown) {
        return;
    }
    setUsageDown(usage, goingDown);

    dispatchUsage(usage, goingDown);
}

bool ApplePS2Keyboard::isUsageDown(UInt8 usage)
{
    return (_usageDown[usage >> 3] & (1U << (usage & 7))) != 0;
}

void ApplePS2Keyboard::setUsageDown(UInt8 usage, bool down)
{
    if (down) {
        _usageDown[usage >> 3] |= (UInt8)(1U << (usage & 7));
    } else {
        _usageDown[usage >> 3] &= (UInt8)~(1U << (usage & 7));
    }
}

void ApplePS2Keyboard::dispatchUsage(UInt8 usage, bool down)
{
    bool           grabbed = PDHIDKeyboardIsGrabbed();
    UInt8          adb;
    AbsoluteTime   now;

    PDHIDPushKeyboardEvent(usage, down);

    if (grabbed && !_consoleGrabbed) {
        /* Keys held when the grab started would stay latched on the console
         * for the compositor's lifetime without a matching release. */
        clock_get_uptime((uint64_t *)&now);
        for (unsigned int held = 0; held < 256; held++) {
            if (!isUsageDown((UInt8)held)) {
                continue;
            }
            adb = PDHIDUsageToADB((UInt8)held);
            if (adb != kPDHIDNoADBCode) {
                dispatchKeyboardEvent(adb, false, *(AbsoluteTime *)&now);
            }
        }
    }
    _consoleGrabbed = grabbed;

    if (grabbed) {
        return;
    }

    adb = PDHIDUsageToADB(usage);
    if (adb == kPDHIDNoADBCode) {
        return;
    }

    clock_get_uptime((uint64_t *)&now);
    dispatchKeyboardEvent(adb, down, *(AbsoluteTime *)&now);
}

void ApplePS2Keyboard::setLEDs(UInt8 ledState)
{
    PS2Request * request = _device->allocateRequest();
    if (request == NULL) {
        return;
    }
    request->commands[0].command = kPS2C_WriteDataPort;
    request->commands[0].inOrOut = kDP_SetKeyboardLEDs;
    request->commands[1].command = kPS2C_ReadDataPortAndCompare;
    request->commands[1].inOrOut = kSC_Acknowledge;
    request->commands[2].command = kPS2C_WriteDataPort;
    request->commands[2].inOrOut = ledState;
    request->commands[3].command = kPS2C_ReadDataPortAndCompare;
    request->commands[3].inOrOut = kSC_Acknowledge;
    request->commandsCount   = 4;
    request->completionTarget = NULL;
    request->completionAction = NULL;
    _device->submitRequest(request);   /* auto-freed (no completion routine) */
}

void ApplePS2Keyboard::setKeyboardEnable(bool enable)
{
    PS2Request * request = _device->allocateRequest();
    if (request == NULL) {
        return;
    }
    request->commands[0].command = kPS2C_WriteDataPort;
    request->commands[0].inOrOut = enable ? kDP_Enable : kDP_SetDefaultsAndDisable;
    request->commands[1].command = kPS2C_ReadDataPortAndCompare;
    request->commands[1].inOrOut = kSC_Acknowledge;
    request->commandsCount   = 2;
    request->completionTarget = NULL;
    request->completionAction = NULL;
    _device->submitRequest(request);
}

void ApplePS2Keyboard::setAlphaLockFeedback(bool locked)
{
    /* caps lock LED = bit 2 */
    setLEDs(locked ? 0x04 : 0x00);
}

const unsigned char * ApplePS2Keyboard::defaultKeymapOfLength(UInt32 * length)
{
    *length = sizeof(gPDHIDUSAKeyMap);
    return gPDHIDUSAKeyMap;
}

UInt32 ApplePS2Keyboard::maxKeyCodes()
{
    return NX_NUMKEYCODES;
}

UInt32 ApplePS2Keyboard::deviceType()
{
    return NX_EVS_DEVICE_TYPE_KEYBOARD;
}

UInt32 ApplePS2Keyboard::interfaceID()
{
    return NX_EVS_DEVICE_INTERFACE_ADB;
}
