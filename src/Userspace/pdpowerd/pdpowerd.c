// power policy: for now the power button, which shuts the system down through launchd on a press
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/kern_event.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

// PDACPIPlatformExpert.h's kernel event for a press
#define kPDACPIEventVendor        "org.puredarwin.acpi"
#define kPDACPIEventClass         1
#define kPDACPIEventSubclass      1
#define kPDACPIEventPowerButton   1

// launchd's reboot2 mig routine, as pd_shutdown uses
extern void *reboot2(uint64_t flags);

int main(void)
{
    int fd = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
    if (fd < 0) {
        fprintf(stderr, "pdpowerd: no kernel event socket, errno %d\n", errno);
        return 1;
    }

    // the vendor code exists once PDACPIPlatform's namespace is up, a few seconds into boot
    struct kev_vendor_code vendor = {};
    strlcpy(vendor.vendor_string, kPDACPIEventVendor, sizeof(vendor.vendor_string));
    for (int i = 0; ioctl(fd, SIOCGKEVVENDOR, &vendor) != 0; i++) {
        if (i == 120) {
            fprintf(stderr, "pdpowerd: no %s events, nothing to watch\n", kPDACPIEventVendor);
            return 0;
        }
        sleep(1);
    }

    struct kev_request filter = { vendor.vendor_code, kPDACPIEventClass, kPDACPIEventSubclass };
    if (ioctl(fd, SIOCSKEVFILT, &filter) != 0) {
        fprintf(stderr, "pdpowerd: cannot filter kernel events, errno %d\n", errno);
        return 1;
    }
    fprintf(stderr, "pdpowerd: watching the power button\n");

    for (;;) {
        uint8_t buf[1024];
        ssize_t n = read(fd, buf, sizeof(buf));

        // an event without data is only KEV_MSG_HEADER_SIZE long, shorter than struct kern_event_msg
        if (n < (ssize_t)KEV_MSG_HEADER_SIZE) continue;

        struct kern_event_msg *ev = (struct kern_event_msg *)buf;
        if (ev->vendor_code != vendor.vendor_code || ev->event_code != kPDACPIEventPowerButton) continue;

        syslog(LOG_NOTICE, "power button pressed, shutting down");
        fprintf(stderr, "pdpowerd: power button pressed, shutting down\n");
        sync();
        reboot2(RB_HALT);
    }
}
