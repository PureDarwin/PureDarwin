/*
* Copyright (c) 2017 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/

#include <sys/types.h>
#include <sys/sysctl.h>
#include <TargetConditionals.h>

#include "Loading.h" // For internalInstall()
#include "BootArgs.h"

namespace dyld {
#if defined(__x86_64__) && !TARGET_OS_SIMULATOR
    bool isTranslated();
#endif
}

namespace dyld3 {

uint64_t BootArgs::_flags = 0;

bool BootArgs::forceCustomerCache() {
    return (_flags & kForceCustomerCacheMask);
}

bool BootArgs::forceDevelopmentCache() {
    return (_flags & kForceDevelopmentCacheMask);
}

bool BootArgs::forceDyld2() {
#if defined(__x86_64__) && !TARGET_OS_SIMULATOR
    if (dyld::isTranslated()) { return true; }
#endif
    // If both force dyld2 and dyld3 are set then use dyld3
    if (_flags & kForceDyld3CacheMask) { return false; }
    if (_flags & kForceDyld2CacheMask) { return true; }
    return false;
}

bool BootArgs::forceDyld3() {
    return (_flags & kForceDyld3CacheMask);
}

bool BootArgs::enableDyldTestMode() {
    return (_flags & kDyldTestModeMask);
}

bool BootArgs::enableCompactImageInfo() {
    return (_flags & kEnableCompactImageInfoMask);
}

bool BootArgs::forceReadOnlyDataConst() {
    return (_flags & kForceReadOnlyDataConstMask);
}

bool BootArgs::forceReadWriteDataConst() {
    return (_flags & kForceReadWriteDataConstMask);
}

void BootArgs::setFlags(uint64_t flags) {
#if TARGET_IPHONE_SIMULATOR
    return;
#else
    // don't check for boot-args on customer installs
    if ( !internalInstall() )
        return;
    _flags = flags;
#endif
}
};
