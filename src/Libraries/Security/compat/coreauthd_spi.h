/*
 * LocalAuthentication's daemon SPI. The framework is closed source and
 * PureDarwin has no biometric or password prompt to raise, so a context can
 * never be created; SecItem treats that as "authentication unavailable" and
 * fails the request rather than silently granting access.
 */
#ifndef PD_COREAUTHD_SPI_H
#define PD_COREAUTHD_SPI_H

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

#define kLAErrorDomain "com.apple.LocalAuthentication"

enum {
    kLAErrorUserCancel     = -2,
    kLAErrorNotInteractive = -1004,
    kLAErrorParameter      = -1001,
};

/* Option keys are CFIndex-valued, boxed into CFNumbers by the caller. Their
 * real values are private to LocalAuthentication, but nothing on PureDarwin
 * ever reads the dictionary back, so only distinctness matters here. */
enum {
    kLAOptionAuthenticationReason = 1,
    kLAOptionCallerName           = 2,
    kLAOptionNotInteractive       = 3,
};

CFTypeRef LACreateNewContextWithACMContext(CFDataRef acmContext, CFErrorRef *error);
CFDataRef LACopyACMContext(CFTypeRef context, CFErrorRef *error);
bool LAEvaluateAndUpdateACL(CFTypeRef context, CFDataRef acl, CFTypeRef operation,
                            CFDictionaryRef options, CFDataRef *updatedACL, CFErrorRef *error);

__END_DECLS

#endif /* PD_COREAUTHD_SPI_H */
