// ServiceManagement.c - SMJobBless with no Authorization Services or privileged helper installer
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// blessing fails with an error the caller can show

#include <CoreFoundation/CoreFoundation.h>

typedef struct AuthorizationOpaqueRef *AuthorizationRef;

const CFStringRef kSMDomainSystemLaunchd = CFSTR("kSMDomainSystemLaunchd");
const CFStringRef kSMDomainUserLaunchd = CFSTR("kSMDomainUserLaunchd");
const CFStringRef kSMErrorDomainFramework = CFSTR("CFErrorDomainSMFramework");

enum {
    kSMErrorInternalFailure = 2,
    kSMErrorInvalidSignature = 3,
    kSMErrorAuthorizationFailure = 4,
    kSMErrorToolNotValid = 5,
    kSMErrorJobNotFound = 6,
    kSMErrorServiceUnavailable = 7,
};

Boolean SMJobBless(CFStringRef domain, CFStringRef executableLabel, AuthorizationRef auth, CFErrorRef *outError)
{
    (void)auth;
    CFIndex code = domain == NULL || executableLabel == NULL ? kSMErrorToolNotValid : kSMErrorServiceUnavailable;
    if (outError)
        *outError = CFErrorCreate(kCFAllocatorDefault, kSMErrorDomainFramework, code, NULL);
    return false;
}

Boolean SMJobRemove(CFStringRef domain, CFStringRef jobLabel, AuthorizationRef auth, Boolean wait, CFErrorRef *outError)
{
    (void)domain;
    (void)auth;
    (void)wait;
    if (outError)
        *outError = CFErrorCreate(kCFAllocatorDefault, kSMErrorDomainFramework, jobLabel ? kSMErrorJobNotFound : kSMErrorToolNotValid, NULL);
    return false;
}
