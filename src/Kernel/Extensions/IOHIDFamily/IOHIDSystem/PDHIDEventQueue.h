#ifndef _PD_HID_EVENT_QUEUE_H
#define _PD_HID_EVENT_QUEUE_H

#include <IOKit/IOTypes.h>

/*
 * The raw event queues behind /dev/usb_hid_kbd and /dev/usb_hid_mouse. Key
 * events carry USB HID usages whatever the transport, so a PS/2 driver has to
 * translate its scan codes to usages before pushing here.
 */

struct PDHIDKbdEvent {
    UInt32 sequence;
    UInt8  usage;
    UInt8  down;
    UInt8  reserved[2];
};

struct PDHIDMouseEvent {
    UInt32 sequence;
    UInt8  mouseIndex;
    UInt8  buttons;
    SInt8  dx;
    SInt8  dy;
    SInt8  wheel;
    UInt8  reserved[3];
};

void PDHIDPublishKeyboardDevice(void);
void PDHIDPublishMouseDevice(void);
void PDHIDPushKeyboardEvent(UInt8 usage, bool down);

/* True while a userspace client holds /dev/usb_hid_kbd open. A compositor or
 * X server reading the raw HID queue is the sole owner of the keyboard for as
 * long as it runs, so the driver must stop feeding the same keys to the
 * console keyboard path as well. */
bool PDHIDKeyboardIsGrabbed(void);

void PDHIDPushMouseEvent(UInt8 mouseIndex, UInt8 buttons, SInt8 dx, SInt8 dy, SInt8 wheel);

/* Claims the next mouseIndex. Shared so a PS/2 and a USB mouse cannot both
 * report as device 0. */
UInt8 PDHIDAllocateMouseIndex(void);

/* No ADB keycode corresponds to this usage. */
#define kPDHIDNoADBCode 0xFF

/*
 * HID keyboard usage -> ADB keycode for the console path, covering both the
 * ordinary keys and the 0xE0-0xE7 modifiers.
 */
UInt8 PDHIDUsageToADB(UInt8 usage);

#endif
