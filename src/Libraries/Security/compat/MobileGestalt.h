/*
 * MobileGestalt is closed source. Security asks it exactly one question - the
 * release type, used to decide whether internal-only policies apply - and a
 * NULL answer means "customer build", which is what PureDarwin is.
 */
#ifndef PD_MOBILEGESTALT_H
#define PD_MOBILEGESTALT_H

#include <CoreFoundation/CoreFoundation.h>

#define kMGQReleaseType CFSTR("ReleaseType")

static inline CFTypeRef MGCopyAnswer(CFStringRef question, void *unused)
{
    (void)question;
    (void)unused;
    return NULL;
}

#endif /* PD_MOBILEGESTALT_H */
