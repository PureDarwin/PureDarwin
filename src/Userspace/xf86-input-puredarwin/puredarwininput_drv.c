#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86str.h"
#include "xf86Module.h"
#include "xf86Xinput.h"
#include "exevents.h"
#include "xkbsrv.h"
#include <X11/keysym.h>

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>

#define kPDIOHIDSuppressConsoleKeyboardSelector 13

/* Must match IOUSBHIDDriver's userspace event structs. */
struct USBHIDMouseEvent {
    uint32_t sequence;
    uint8_t  mouseIndex;
    uint8_t  buttons;
    int8_t   dx;
    int8_t   dy;
    int8_t   wheel;
    uint8_t  reserved[3];
};

struct USBHIDKbdEvent {
    uint32_t sequence;
    uint8_t  usage;   /* USB HID usage */
    uint8_t  down;
    uint8_t  reserved[2];
};

enum PDInputType { PD_MOUSE, PD_KEYBOARD };

typedef struct {
    enum PDInputType type;
    char            *device;
    uint8_t          lastButtons;
} PDInputPrivRec, *PDInputPrivPtr;

/*
 * USB HID usage -> Linux evdev keycode. X keycode = evdev + 8. This is the
 * standard modern mapping; with an evdev XKB keymap the symbols come out
 * right, and even without one the keys register as distinct keycodes.
 */
static const uint16_t usbToEvdev[256] = {
    [0x04] = 30, [0x05] = 48, [0x06] = 46, [0x07] = 32, [0x08] = 18,
    [0x09] = 33, [0x0a] = 34, [0x0b] = 35, [0x0c] = 23, [0x0d] = 36,
    [0x0e] = 37, [0x0f] = 38, [0x10] = 50, [0x11] = 49, [0x12] = 24,
    [0x13] = 25, [0x14] = 16, [0x15] = 19, [0x16] = 31, [0x17] = 20,
    [0x18] = 22, [0x19] = 47, [0x1a] = 17, [0x1b] = 45, [0x1c] = 21,
    [0x1d] = 44,
    [0x1e] = 2, [0x1f] = 3, [0x20] = 4, [0x21] = 5, [0x22] = 6,
    [0x23] = 7, [0x24] = 8, [0x25] = 9, [0x26] = 10, [0x27] = 11,
    [0x28] = 28, /* enter */ [0x29] = 1,  /* esc */ [0x2a] = 14, /* bksp */
    [0x2b] = 15, /* tab */   [0x2c] = 57, /* space */
    [0x2d] = 12, [0x2e] = 13, [0x2f] = 26, [0x30] = 27, [0x31] = 43,
    [0x32] = 43, [0x33] = 39, [0x34] = 40, [0x35] = 41, [0x36] = 51,
    [0x37] = 52, [0x38] = 53, [0x39] = 58, /* caps */
    [0x3a] = 59, [0x3b] = 60, [0x3c] = 61, [0x3d] = 62, [0x3e] = 63,
    [0x3f] = 64, [0x40] = 65, [0x41] = 66, [0x42] = 67, [0x43] = 68,
    [0x44] = 87, [0x45] = 88, /* F1..F12 */
    [0x4f] = 106, /* right */ [0x50] = 105, /* left */
    [0x51] = 108, /* down */  [0x52] = 103, /* up */
    /* modifiers 0xE0..0xE7 */
    [0xe0] = 29,  /* LCtrl */  [0xe1] = 42,  /* LShift */
    [0xe2] = 56,  /* LAlt */   [0xe3] = 125, /* LMeta */
    [0xe4] = 97,  /* RCtrl */  [0xe5] = 54,  /* RShift */
    [0xe6] = 100, /* RAlt */   [0xe7] = 126, /* RMeta */
};

static void
PDPtrCtrl(DeviceIntPtr dev, PtrCtrl *ctrl)
{
    (void)dev; (void)ctrl;
}

static void
PDKbdBell(int percent, DeviceIntPtr dev, void *ctrl, int unused)
{
    (void)percent; (void)dev; (void)ctrl; (void)unused;
}

static void
PDKbdCtrl(DeviceIntPtr dev, KeybdCtrl *ctrl)
{
    (void)dev; (void)ctrl;
}

static void
PDReadInput(InputInfoPtr pInfo)
{
    PDInputPrivPtr priv = pInfo->private;

    if (priv->type == PD_MOUSE) {
        struct USBHIDMouseEvent ev;
        while (read(pInfo->fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            uint8_t changed = ev.buttons ^ priv->lastButtons;
            int rel[2] = { ev.dx, ev.dy };

            if (ev.dx || ev.dy)
                xf86PostMotionEventP(pInfo->dev, 0 /*relative*/, 0, 2, rel);

            /* HID boot: bit0 left, bit1 right, bit2 middle -> X 1,3,2 */
            if (changed & 0x01)
                xf86PostButtonEvent(pInfo->dev, 0, 1, !!(ev.buttons & 0x01), 0, 0);
            if (changed & 0x02)
                xf86PostButtonEvent(pInfo->dev, 0, 3, !!(ev.buttons & 0x02), 0, 0);
            if (changed & 0x04)
                xf86PostButtonEvent(pInfo->dev, 0, 2, !!(ev.buttons & 0x04), 0, 0);

            if (ev.wheel > 0) {
                xf86PostButtonEvent(pInfo->dev, 0, 4, 1, 0, 0);
                xf86PostButtonEvent(pInfo->dev, 0, 4, 0, 0, 0);
            } else if (ev.wheel < 0) {
                xf86PostButtonEvent(pInfo->dev, 0, 5, 1, 0, 0);
                xf86PostButtonEvent(pInfo->dev, 0, 5, 0, 0, 0);
            }
            priv->lastButtons = ev.buttons;
        }
    } else {
        struct USBHIDKbdEvent ev;
        while (read(pInfo->fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            uint16_t evdev = usbToEvdev[ev.usage];
            if (evdev)
                xf86PostKeyboardEvent(pInfo->dev, evdev + 8, ev.down);
        }
    }
}

/* Best-effort: tell IOHIDSystem a "WindowServer" is now handling keyboard
 * input, so it stops also feeding keystrokes into the console tty. Failure
 * just means the console-input leak stays present - never fatal to X. */
static void
PDAcquireIOHIDSystem(InputInfoPtr pInfo)
{
    mach_port_t masterPort = MACH_PORT_NULL;
    io_service_t service = IO_OBJECT_NULL;
    io_connect_t connect = IO_OBJECT_NULL;
    kern_return_t kr;

    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr != KERN_SUCCESS)
        return;

    kr = IOServiceGetMatchingService(masterPort, IOServiceMatching("IOHIDSystem"), &service);
    if (kr != KERN_SUCCESS || service == IO_OBJECT_NULL)
        return;

    kr = IOServiceOpen(service, mach_task_self(), 0, &connect);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS)
        return;

    uint64_t arg = 0;
    kr = IOConnectCallScalarMethod(connect, kPDIOHIDSuppressConsoleKeyboardSelector, &arg, 1, NULL, NULL);
    if (kr != KERN_SUCCESS)
        xf86IDrvMsg(pInfo, X_WARNING, "IOHIDSystem suppressConsoleKeyboard failed (0x%x); "
                    "console tty may still see keystrokes\n", kr);
    else
        xf86IDrvMsg(pInfo, X_INFO, "acquired IOHIDSystem console-input ownership\n");

    /* Leave connect open for the life of the server - the suppression flag
     * is only meaningful while some client holds it, and losing the port
     * is harmless either way. */
}

static int
PDDeviceControl(DeviceIntPtr dev, int what)
{
    InputInfoPtr pInfo = dev->public.devicePrivate;
    PDInputPrivPtr priv = pInfo->private;

    switch (what) {
    case DEVICE_INIT:
        if (priv->type == PD_MOUSE) {
            CARD8 map[6];
            Atom btnLabels[5] = { 0 };
            Atom axisLabels[2] = { 0 };
            int i;
            for (i = 0; i < 6; i++) map[i] = i;
            InitPointerDeviceStruct((DevicePtr)dev, map, 5, btnLabels,
                                    PDPtrCtrl, GetMotionHistorySize(), 2, axisLabels);
            xf86InitValuatorAxisStruct(dev, 0, axisLabels[0], -1, -1, 1, 0, 1, Relative);
            xf86InitValuatorAxisStruct(dev, 1, axisLabels[1], -1, -1, 1, 0, 1, Relative);
        } else {
            XkbRMLVOSet rmlvo = { 0 };
            rmlvo.rules  = "evdev";
            rmlvo.model  = "pc105";
            rmlvo.layout = "us";
            InitKeyboardDeviceStruct(dev, &rmlvo, PDKbdBell, PDKbdCtrl);
        }
        break;

    case DEVICE_ON:
        if (pInfo->fd < 0) {
            pInfo->fd = open(priv->device, O_RDONLY | O_NONBLOCK);
            if (pInfo->fd < 0) {
                xf86IDrvMsg(pInfo, X_ERROR, "cannot open %s: %s\n",
                            priv->device, strerror(errno));
                return BadRequest;
            }
        }
        if (priv->type == PD_KEYBOARD)
            PDAcquireIOHIDSystem(pInfo);
        xf86AddEnabledDevice(pInfo);
        dev->public.on = TRUE;
        break;

    case DEVICE_OFF:
    case DEVICE_CLOSE:
        if (pInfo->fd >= 0) {
            xf86RemoveEnabledDevice(pInfo);
            close(pInfo->fd);
            pInfo->fd = -1;
        }
        dev->public.on = FALSE;
        break;
    }
    return Success;
}

static int
PDPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
    PDInputPrivPtr priv;
    char *typeStr;

    (void)drv; (void)flags;

    priv = calloc(1, sizeof(PDInputPrivRec));
    if (!priv)
        return BadAlloc;
    pInfo->private = priv;

    typeStr = xf86SetStrOption(pInfo->options, "PDType", "mouse");
    if (typeStr && !strcasecmp(typeStr, "keyboard")) {
        priv->type = PD_KEYBOARD;
        priv->device = xf86SetStrOption(pInfo->options, "Device", "/dev/usb_hid_kbd");
        pInfo->type_name = XI_KEYBOARD;
    } else {
        priv->type = PD_MOUSE;
        priv->device = xf86SetStrOption(pInfo->options, "Device", "/dev/usb_hid_mouse");
        pInfo->type_name = XI_MOUSE;
    }

    pInfo->device_control = PDDeviceControl;
    pInfo->read_input = PDReadInput;
    pInfo->fd = -1;

    xf86IDrvMsg(pInfo, X_INFO, "PureDarwin input: %s device %s\n",
                priv->type == PD_KEYBOARD ? "keyboard" : "mouse", priv->device);
    return Success;
}

static void
PDUnInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
    PDInputPrivPtr priv = pInfo ? pInfo->private : NULL;
    if (priv) {
        free(priv->device);
        free(priv);
        pInfo->private = NULL;
    }
    xf86DeleteInput(pInfo, flags);
}

static InputDriverRec PUREDARWININPUT = {
    .driverVersion = 1,
    .driverName    = "puredarwininput",
    .PreInit       = PDPreInit,
    .UnInit        = PDUnInit,
    .module        = NULL,
    .default_options = NULL,
};

static void *
PDPlug(void *module, void *options, int *errmaj, int *errmin)
{
    (void)options; (void)errmaj; (void)errmin;
    xf86AddInputDriver(&PUREDARWININPUT, module, 0);
    return module;
}

static XF86ModuleVersionInfo PDVersionRec = {
    "puredarwininput",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    1, 0, 0,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    { 0, 0, 0, 0 }
};

_X_EXPORT XF86ModuleData puredarwininputModuleData = {
    &PDVersionRec,
    PDPlug,
    NULL
};
