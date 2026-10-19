#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include "common.h"

CFRunLoopSourceRef IOPSNotificationCreateRunLoopSource(void (*)(void *), void *);
static void changed(void *ctx) { (void)ctx; }

int main(void)
{
    IOPMAssertionID id1 = 0, id2 = 0;
    CHECK(IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, CFSTR("test"), &id1) == kIOReturnSuccess &&
          id1, "assertion created (%u)", (unsigned)id1);
    CHECK(IOPMAssertionCreateWithName(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, CFSTR("test2"), &id2) == kIOReturnSuccess &&
          id2 != id1, "second assertion has its own ID");
    CHECK(IOPMAssertionRelease(id1) == kIOReturnSuccess, "release");
    CHECK(IOPMAssertionRelease(id1) == kIOReturnNotFound, "double release is not found");
    CHECK(IOPMAssertionRelease(id2) == kIOReturnSuccess, "release second");
    CHECK(IOPMAssertionCreateWithName(NULL, kIOPMAssertionLevelOn, CFSTR("x"), &id1) == kIOReturnBadArgument, "NULL type rejected");
    io_object_t none = IO_OBJECT_NULL;
    CHECK(IODeregisterForSystemPower(&none) == kIOReturnBadArgument, "deregistering nothing is a bad argument");
    CFRunLoopSourceRef src = IOPSNotificationCreateRunLoopSource(changed, NULL);
    CHECK(src != NULL, "power source notification run loop source");
    if (src) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), src, kCFRunLoopDefaultMode);
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), src, kCFRunLoopDefaultMode);
        CFRelease(src);
    }
    DONE("IOKit power");
}
