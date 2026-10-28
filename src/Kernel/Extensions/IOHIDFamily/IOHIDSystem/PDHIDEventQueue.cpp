/*
 * Raw HID event queue, shared by the USB and PS/2 drivers because there can
 * only be one /dev node per queue. The node names keep their "usb_" prefix
 * because that is what the X input driver and the wlroots backend open.
 */
#include "PDHIDEventQueue.h"

#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IOService.h>
#include <kern/thread_call.h>
#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/uio.h>

extern "C" int devfs_is_ready(void);
extern "C" void cons_cinput(char ch);

#define DEADKEY kPDHIDNoADBCode

static const UInt8 gUSBToADB[] = {
    DEADKEY, DEADKEY, DEADKEY, DEADKEY,
    0x00, 0x0b, 0x08, 0x02, 0x0e, 0x03, 0x05, 0x04, 0x22, 0x26, 0x28, 0x25,
    0x2e, 0x2d, 0x1f, 0x23, 0x0c, 0x0f, 0x01, 0x11, 0x20, 0x09, 0x0d, 0x07,
    0x10, 0x06, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1a, 0x1c, 0x19, 0x1d,
    0x24, 0x35, 0x33, 0x30, 0x31, 0x1b, 0x18, 0x21, 0x1e, 0x2a, 0x2a, 0x29,
    0x27, 0x32, 0x2b, 0x2f, 0x2c, 0x39, 0x7a, 0x78, 0x63, 0x76, 0x60, 0x61,
    0x62, 0x64, 0x65, 0x6d, 0x67, 0x6f, 0x69, 0x6b, 0x71, 0x72, 0x73, 0x74,
    0x75, 0x77, 0x79, 0x7c, 0x7b, 0x7d, 0x7e, 0x47, 0x4b, 0x43, 0x4e, 0x45,
    0x4c, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5b, 0x5c, 0x52, 0x41,
    0x0a, 0x6e, 0x7f, 0x51, 0x69, 0x6b, 0x71, 0x6a, 0x40, 0x4f, 0x50, 0x5a,
    DEADKEY, DEADKEY, DEADKEY, DEADKEY, DEADKEY, 0x72,
};

/* Usages 0xE0-0xE7: L/R control, shift, alt, gui. */
static const UInt8 gUSBModToADB[8] = {
    0x3b, 0x38, 0x3a, 0x37, 0x3e, 0x3c, 0x3d, 0x36
};

UInt8
PDHIDUsageToADB(UInt8 usage)
{
    if (usage >= 0xE0 && usage <= 0xE7)
        return gUSBModToADB[usage - 0xE0];
    if (usage >= sizeof(gUSBToADB) / sizeof(gUSBToADB[0]))
        return kPDHIDNoADBCode;
    return gUSBToADB[usage];
}

static void *gKbdNode;
static void *gPS2KbdNode;
static bool gWantPS2KbdNode;
static int gKbdMajor = -1;
static thread_call_t gKbdRetryCall;
static IONotifier *gKbdBSDNotifier;
static IOLock *gKbdLock;
static UInt32 gKbdSequence;
static UInt32 gKbdReadIndex;
static UInt32 gKbdWriteIndex;
static PDHIDKbdEvent gKbdEvents[64];

// console side, guarded by gKbdLock and drained by gConsoleCall
static thread_call_t gConsoleCall;
static UInt8 gModifiers;
static UInt8 gLockLEDs;
static bool gLEDsDirty;
static UInt32 gConsReadIndex;
static UInt32 gConsWriteIndex;
static char gConsBytes[256];

struct PDHIDLEDEntry {
    PDHIDLEDHandler handler;
    void *target;
};

static IOLock *gLEDLock;
static PDHIDLEDEntry gLEDHandlers[8];

static UInt8 gNextMouseIndex;

static void *gMouseNode;
static void *gPS2MouseNode;
static bool gWantPS2MouseNode;
static int gMouseMajor = -1;
static thread_call_t gMouseRetryCall;
static IONotifier *gMouseBSDNotifier;
static IOLock *gMouseLock;
static UInt32 gMouseSequence;
static UInt32 gMouseReadIndex;
static UInt32 gMouseWriteIndex;
static PDHIDMouseEvent gMouseEvents[64];

static void pd_hid_kbd_publish_retry(thread_call_param_t, thread_call_param_t);
static void pd_hid_console_work(thread_call_param_t, thread_call_param_t);
static void pd_hid_mouse_publish_retry(thread_call_param_t, thread_call_param_t);
static bool pd_hid_kbd_iobsd_published(void *, void *, IOService *, IONotifier *);
static bool pd_hid_mouse_iobsd_published(void *, void *, IOService *, IONotifier *);

static int pd_hid_open(dev_t, int, int, struct proc *) { return 0; }
static int pd_hid_close(dev_t, int, int, struct proc *) { return 0; }

/* spec_open() calls d_open on every open but spec_close() only reaches d_close
 * once vcount() drops to zero, so this tracks "somebody has it open" rather
 * than a balanced count. */
static volatile bool gKbdGrabbed;

static int
pd_hid_kbd_open(dev_t, int, int, struct proc *)
{
    gKbdGrabbed = true;
    return 0;
}

static int
pd_hid_kbd_close(dev_t, int, int, struct proc *)
{
    gKbdGrabbed = false;
    return 0;
}

bool
PDHIDKeyboardIsGrabbed(void)
{
    return gKbdGrabbed;
}

static int
pd_hid_kbd_read(dev_t, struct uio *uio, int)
{
    PDHIDKbdEvent event;
    int error = 0;

    if (!gKbdLock) return ENXIO;
    if (uio_resid(uio) < (user_ssize_t)sizeof(event)) return EINVAL;

    IOLockLock(gKbdLock);
    if (gKbdReadIndex == gKbdWriteIndex) {
        error = EAGAIN;
    } else {
        event = gKbdEvents[gKbdReadIndex % (UInt32)(sizeof(gKbdEvents) / sizeof(gKbdEvents[0]))];
        gKbdReadIndex++;
    }
    IOLockUnlock(gKbdLock);

    if (error) return error;
    return uiomove((const char *)&event, (int)sizeof(event), uio);
}

static int
pd_hid_mouse_read(dev_t, struct uio *uio, int)
{
    PDHIDMouseEvent event;
    int error = 0;

    if (!gMouseLock) return ENXIO;
    if (uio_resid(uio) < (user_ssize_t)sizeof(event)) return EINVAL;

    IOLockLock(gMouseLock);
    if (gMouseReadIndex == gMouseWriteIndex) {
        error = EAGAIN;
    } else {
        event = gMouseEvents[gMouseReadIndex % (UInt32)(sizeof(gMouseEvents) / sizeof(gMouseEvents[0]))];
        gMouseReadIndex++;
    }
    IOLockUnlock(gMouseLock);

    if (error) return error;
    return uiomove((const char *)&event, (int)sizeof(event), uio);
}

static struct cdevsw pd_hid_kbd_cdevsw = {
    pd_hid_kbd_open, pd_hid_kbd_close, pd_hid_kbd_read, eno_rdwrt,
    eno_ioctl, eno_stop, eno_reset, 0, eno_select, eno_mmap,
    eno_strat, eno_getc, eno_putc, 0
};

static struct cdevsw pd_hid_mouse_cdevsw = {
    pd_hid_open, pd_hid_close, pd_hid_mouse_read, eno_rdwrt,
    eno_ioctl, eno_stop, eno_reset, 0, eno_select, eno_mmap,
    eno_strat, eno_getc, eno_putc, 0
};

static void
schedule_retry(thread_call_t *call, thread_call_func_t func)
{
    AbsoluteTime deadline;
    if (!*call) {
        *call = thread_call_allocate(func, NULL);
        if (!*call) return;
    }
    clock_interval_to_deadline(1, kSecondScale, &deadline);
    thread_call_enter_delayed(*call, deadline);
}

// drivers on different threads can get here first, so each object is swapped in once
static bool
pd_hid_kbd_setup(void)
{
    if (!gKbdLock) {
        IOLock *lock = IOLockAlloc();
        if (lock && !OSCompareAndSwapPtr(NULL, lock, (void * volatile *)&gKbdLock)) IOLockFree(lock);
    }
    if (!gLEDLock) {
        IOLock *lock = IOLockAlloc();
        if (lock && !OSCompareAndSwapPtr(NULL, lock, (void * volatile *)&gLEDLock)) IOLockFree(lock);
    }
    if (!gConsoleCall) {
        thread_call_t call = thread_call_allocate(pd_hid_console_work, NULL);
        if (call && !OSCompareAndSwapPtr(NULL, call, (void * volatile *)&gConsoleCall)) thread_call_free(call);
    }
    return gKbdLock && gLEDLock && gConsoleCall;
}

void
PDHIDPublishKeyboardDevice(void)
{
    if (gKbdNode && (!gWantPS2KbdNode || gPS2KbdNode)) return;
    if (!pd_hid_kbd_setup()) return;
    if (gKbdMajor < 0) {
        gKbdMajor = cdevsw_add(-1, &pd_hid_kbd_cdevsw);
        if (gKbdMajor < 0) return;
    }
    if (!devfs_is_ready()) {
        if (!gKbdBSDNotifier) {
            OSDictionary *matching = IOService::resourceMatching("IOBSD");
            if (matching) {
                gKbdBSDNotifier = IOService::addMatchingNotification(gIOPublishNotification,
                                                                     matching,
                                                                     pd_hid_kbd_iobsd_published,
                                                                     NULL, NULL);
                matching->release();
            }
        }
        schedule_retry(&gKbdRetryCall, pd_hid_kbd_publish_retry);
        return;
    }
    if (!gKbdNode) {
        gKbdNode = devfs_make_node(makedev(gKbdMajor, 0), DEVFS_CHAR,
                                   0, 0, 0666, "usb_hid_kbd");
        if (!gKbdNode)
            schedule_retry(&gKbdRetryCall, pd_hid_kbd_publish_retry);
        else
            IOLog("PDHIDEventQueue: published /dev/usb_hid_kbd\n");
    }

    if (gWantPS2KbdNode && !gPS2KbdNode) {
        gPS2KbdNode = devfs_make_node(makedev(gKbdMajor, 0), DEVFS_CHAR,
                                     0, 0, 0666, "ps2_kbd");
        if (!gPS2KbdNode)
            schedule_retry(&gKbdRetryCall, pd_hid_kbd_publish_retry);
        else
            IOLog("PDHIDEventQueue: published /dev/ps2_kbd\n");
    }
}

void
PDHIDPublishPS2KeyboardDevice(void)
{
    gWantPS2KbdNode = true;
    PDHIDPublishKeyboardDevice();
}

UInt8
PDHIDAllocateMouseIndex(void)
{
    return gNextMouseIndex++;
}

void
PDHIDPublishMouseDevice(void)
{
    if (gMouseNode && (!gWantPS2MouseNode || gPS2MouseNode)) return;
    if (!gMouseLock) {
        gMouseLock = IOLockAlloc();
        if (!gMouseLock) return;
    }
    if (gMouseMajor < 0) {
        gMouseMajor = cdevsw_add(-1, &pd_hid_mouse_cdevsw);
        if (gMouseMajor < 0) return;
    }
    if (!devfs_is_ready()) {
        if (!gMouseBSDNotifier) {
            OSDictionary *matching = IOService::resourceMatching("IOBSD");
            if (matching) {
                gMouseBSDNotifier = IOService::addMatchingNotification(gIOPublishNotification,
                                                                       matching,
                                                                       pd_hid_mouse_iobsd_published,
                                                                       NULL, NULL);
                matching->release();
            }
        }
        schedule_retry(&gMouseRetryCall, pd_hid_mouse_publish_retry);
        return;
    }
    if (!gMouseNode) {
        gMouseNode = devfs_make_node(makedev(gMouseMajor, 0), DEVFS_CHAR,
                                     0, 0, 0666, "usb_hid_mouse");
        if (!gMouseNode)
            schedule_retry(&gMouseRetryCall, pd_hid_mouse_publish_retry);
        else
            IOLog("PDHIDEventQueue: published /dev/usb_hid_mouse\n");
    }

    if (gWantPS2MouseNode && !gPS2MouseNode) {
        gPS2MouseNode = devfs_make_node(makedev(gMouseMajor, 0), DEVFS_CHAR,
                                       0, 0, 0666, "ps2_mouse");
        if (!gPS2MouseNode)
            schedule_retry(&gMouseRetryCall, pd_hid_mouse_publish_retry);
        else
            IOLog("PDHIDEventQueue: published /dev/ps2_mouse\n");
    }
}

void
PDHIDPublishPS2MouseDevice(void)
{
    gWantPS2MouseNode = true;
    PDHIDPublishMouseDevice();
}

static void pd_hid_kbd_publish_retry(thread_call_param_t, thread_call_param_t)
{
    PDHIDPublishKeyboardDevice();
}

static void pd_hid_mouse_publish_retry(thread_call_param_t, thread_call_param_t)
{
    PDHIDPublishMouseDevice();
}

static bool pd_hid_kbd_iobsd_published(void *, void *, IOService *, IONotifier *notifier)
{
    PDHIDPublishKeyboardDevice();
    if (gKbdNode && notifier) {
        notifier->remove();
        if (gKbdBSDNotifier == notifier) gKbdBSDNotifier = NULL;
    }
    return true;
}

static bool pd_hid_mouse_iobsd_published(void *, void *, IOService *, IONotifier *notifier)
{
    PDHIDPublishMouseDevice();
    if (gMouseNode && notifier) {
        notifier->remove();
        if (gMouseBSDNotifier == notifier) gMouseBSDNotifier = NULL;
    }
    return true;
}

// US layout for keyboard page usages 0x04-0x38 (HID Usage Tables 1.12, section 10)
static const char gUsageToASCII[2][0x39] = {
    {
        0, 0, 0, 0,
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
        '\r', 0x1b, 0x7f, '\t', ' ', '-', '=', '[', ']', '\\', '#', ';', '\'', '`', ',', '.', '/',
    },
    {
        0, 0, 0, 0,
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
        '\r', 0x1b, 0x7f, '\t', ' ', '_', '+', '{', '}', '|', '~', ':', '"', '~', '<', '>', '?',
    },
};

// keypad usages 0x54-0x63 with Num Lock on
static const char gKeypadToASCII[] = "/*-+\r1234567890.";

// gKbdLock held
static void
pd_hid_console_put(const char *bytes, unsigned count)
{
    const UInt32 size = (UInt32)sizeof(gConsBytes);

    for (unsigned i = 0; i < count; i++) {
        if (gConsWriteIndex - gConsReadIndex >= size) return;
        gConsBytes[gConsWriteIndex % size] = bytes[i];
        gConsWriteIndex++;
    }
}

// gKbdLock held, returns true when bytes or LED changes are queued for the thread call
static bool
pd_hid_console_key(UInt8 usage, bool down)
{
    const char *esc = NULL;
    bool shift, ctrl;

    if (usage >= 0xE0 && usage <= 0xE7) {
        if (down)
            gModifiers |= (UInt8)(1U << (usage - 0xE0));
        else
            gModifiers &= (UInt8)~(1U << (usage - 0xE0));
        return false;
    }
    if (!down) return false;

    if (usage == 0x39 || usage == 0x53 || usage == 0x47) {
        gLockLEDs ^= usage == 0x39 ? kPDHIDLEDCapsLock : usage == 0x53 ? kPDHIDLEDNumLock : kPDHIDLEDScrollLock;
        gLEDsDirty = true;
        return true;
    }
    if (gKbdGrabbed) return false;

    shift = (gModifiers & 0x22) != 0;
    ctrl = (gModifiers & 0x11) != 0;

    if (usage >= 0x04 && usage < 0x39) {
        char c = gUsageToASCII[shift ? 1 : 0][usage];

        if (usage <= 0x1D && (gLockLEDs & kPDHIDLEDCapsLock))
            c = gUsageToASCII[shift ? 0 : 1][usage];
        if (ctrl) {
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            if (c >= '@' && c <= '_')
                c = (char)(c & 0x1f);
            else if (c == ' ')
                c = 0;
            else if (c == '/' || c == '?')
                c = 0x1f;
        }
        pd_hid_console_put(&c, 1);
        return true;
    }
    if (usage >= 0x54 && usage <= 0x63) {
        if (usage != 0x58 && !(gLockLEDs & kPDHIDLEDNumLock)) return false;
        pd_hid_console_put(&gKeypadToASCII[usage - 0x54], 1);
        return true;
    }

    // ANSI cursor and editing sequences as xterm sends them
    switch (usage) {
    case 0x4F: esc = "\033[C"; break;
    case 0x50: esc = "\033[D"; break;
    case 0x51: esc = "\033[B"; break;
    case 0x52: esc = "\033[A"; break;
    case 0x4A: esc = "\033[H"; break;
    case 0x4D: esc = "\033[F"; break;
    case 0x49: esc = "\033[2~"; break;
    case 0x4C: esc = "\033[3~"; break;
    case 0x4B: esc = "\033[5~"; break;
    case 0x4E: esc = "\033[6~"; break;
    default: return false;
    }
    pd_hid_console_put(esc, (unsigned)strlen(esc));
    return true;
}

static void
pd_hid_console_work(thread_call_param_t, thread_call_param_t)
{
    char bytes[32];

    for (;;) {
        unsigned count = 0;
        bool leds;
        UInt8 state;

        IOLockLock(gKbdLock);
        while (count < sizeof(bytes) && gConsReadIndex != gConsWriteIndex) {
            bytes[count++] = gConsBytes[gConsReadIndex % (UInt32)sizeof(gConsBytes)];
            gConsReadIndex++;
        }
        leds = gLEDsDirty;
        gLEDsDirty = false;
        state = gLockLEDs;
        IOLockUnlock(gKbdLock);

        if (!count && !leds) break;

        // cons_cinput takes the tty mutex and echoes, so it runs here and never in a driver's path
        for (unsigned i = 0; i < count; i++) {
            cons_cinput(bytes[i]);
        }

        if (leds) {
            IOLockLock(gLEDLock);
            for (unsigned i = 0; i < sizeof(gLEDHandlers) / sizeof(gLEDHandlers[0]); i++) {
                if (gLEDHandlers[i].handler) gLEDHandlers[i].handler(gLEDHandlers[i].target, state);
            }
            IOLockUnlock(gLEDLock);
        }
    }
}

bool
PDHIDRegisterLEDHandler(PDHIDLEDHandler handler, void *target)
{
    bool ok = false;

    if (!handler || !pd_hid_kbd_setup()) return false;

    IOLockLock(gLEDLock);
    for (unsigned i = 0; i < sizeof(gLEDHandlers) / sizeof(gLEDHandlers[0]); i++) {
        if (gLEDHandlers[i].handler) continue;
        gLEDHandlers[i].handler = handler;
        gLEDHandlers[i].target = target;
        ok = true;
        break;
    }
    IOLockUnlock(gLEDLock);

    // a keyboard plugged in later picks up the current lock state
    if (ok) {
        IOLockLock(gKbdLock);
        gLEDsDirty = true;
        IOLockUnlock(gKbdLock);
        thread_call_enter(gConsoleCall);
    }
    return ok;
}

void
PDHIDUnregisterLEDHandler(PDHIDLEDHandler handler, void *target)
{
    if (!gLEDLock) return;

    IOLockLock(gLEDLock);
    for (unsigned i = 0; i < sizeof(gLEDHandlers) / sizeof(gLEDHandlers[0]); i++) {
        if (gLEDHandlers[i].handler != handler || gLEDHandlers[i].target != target) continue;
        gLEDHandlers[i].handler = NULL;
        gLEDHandlers[i].target = NULL;
    }
    IOLockUnlock(gLEDLock);
}

UInt8
PDHIDKeyboardLEDState(void)
{
    return gLockLEDs;
}

void
PDHIDPushKeyboardEvent(UInt8 usage, bool down)
{
    PDHIDKbdEvent event;
    UInt32 slot;
    bool kick;

    if (!gKbdLock) PDHIDPublishKeyboardDevice();
    if (!pd_hid_kbd_setup()) return;

    event.sequence = ++gKbdSequence;
    event.usage = usage;
    event.down = down ? 1 : 0;
    event.reserved[0] = event.reserved[1] = 0;

    IOLockLock(gKbdLock);
    slot = gKbdWriteIndex % (UInt32)(sizeof(gKbdEvents) / sizeof(gKbdEvents[0]));
    gKbdEvents[slot] = event;
    gKbdWriteIndex++;
    if (gKbdWriteIndex - gKbdReadIndex > (UInt32)(sizeof(gKbdEvents) / sizeof(gKbdEvents[0])))
        gKbdReadIndex = gKbdWriteIndex - (UInt32)(sizeof(gKbdEvents) / sizeof(gKbdEvents[0]));
    kick = pd_hid_console_key(usage, down);
    IOLockUnlock(gKbdLock);

    if (kick) thread_call_enter(gConsoleCall);
}

void
PDHIDPushMouseEvent(UInt8 mouseIndex, UInt8 buttons, SInt8 dx, SInt8 dy, SInt8 wheel)
{
    PDHIDMouseEvent event;
    UInt32 slot;

    if (!gMouseLock) {
        PDHIDPublishMouseDevice();
        if (!gMouseLock) return;
    }

    event.sequence = ++gMouseSequence;
    event.mouseIndex = mouseIndex;
    event.buttons = buttons;
    event.dx = dx;
    event.dy = dy;
    event.wheel = wheel;
    bzero(event.reserved, sizeof(event.reserved));

    IOLockLock(gMouseLock);
    slot = gMouseWriteIndex % (UInt32)(sizeof(gMouseEvents) / sizeof(gMouseEvents[0]));
    gMouseEvents[slot] = event;
    gMouseWriteIndex++;
    if (gMouseWriteIndex - gMouseReadIndex > (UInt32)(sizeof(gMouseEvents) / sizeof(gMouseEvents[0])))
        gMouseReadIndex = gMouseWriteIndex - (UInt32)(sizeof(gMouseEvents) / sizeof(gMouseEvents[0]));
    IOLockUnlock(gMouseLock);
}
