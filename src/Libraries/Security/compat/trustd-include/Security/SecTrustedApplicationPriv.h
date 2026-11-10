// SecTrustedApplicationPriv.h - trusted applications, macOS keychain ACL entries, of which none exist here
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// the keychain library is not built, pd_security_unvendored.c registers the type and reports that
// requirements cannot be copied
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include "../../../apple/OSX/libsecurity_codesigning/lib/CSCommon.h"

typedef struct __SecTrustedApplication *SecTrustedApplicationRef;

CFTypeID SecTrustedApplicationGetTypeID(void);
OSStatus SecRequirementsCopyString(CFTypeRef input, uint32_t flags, CFStringRef *text);
OSStatus SecTrustedApplicationCopyRequirement(SecTrustedApplicationRef appRef, SecRequirementRef *requirement);
