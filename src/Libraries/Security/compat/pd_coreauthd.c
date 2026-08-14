/*
 * No LocalAuthentication on PureDarwin: there is nothing to prompt the user
 * with, so every context creation fails as non-interactive.
 */
#include <coreauthd_spi.h>

static void pd_no_auth(CFErrorRef *error)
{
    if (error != NULL && *error == NULL)
        *error = CFErrorCreate(kCFAllocatorDefault, CFSTR(kLAErrorDomain),
                               kLAErrorNotInteractive, NULL);
}

CFTypeRef LACreateNewContextWithACMContext(CFDataRef acmContext, CFErrorRef *error)
{
    (void)acmContext;
    pd_no_auth(error);
    return NULL;
}

CFDataRef LACopyACMContext(CFTypeRef context, CFErrorRef *error)
{
    (void)context;
    pd_no_auth(error);
    return NULL;
}

bool LAEvaluateAndUpdateACL(CFTypeRef context, CFDataRef acl, CFTypeRef operation,
                            CFDictionaryRef options, CFDataRef *updatedACL, CFErrorRef *error)
{
    (void)context; (void)acl; (void)operation; (void)options;
    if (updatedACL != NULL)
        *updatedACL = NULL;
    pd_no_auth(error);
    return false;
}
