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


#ifndef __DYLD_BOOTARGS_H__
#define __DYLD_BOOTARGS_H__

#include <cstdint>

#define VIS_HIDDEN __attribute__((visibility("hidden")))

namespace dyld3 {
#if BUILDING_DYLD
    struct VIS_HIDDEN BootArgs {
        static bool forceCustomerCache();
        static bool forceDevelopmentCache();
        static bool forceDyld2();
        static bool forceDyld3();
        static bool enableDyldTestMode();
        static bool enableCompactImageInfo();
        static bool forceReadOnlyDataConst();
        static bool forceReadWriteDataConst();
        static void setFlags(uint64_t flags);
    private:
        static const uint64_t kForceCustomerCacheMask = 1<<0;
        static const uint64_t kDyldTestModeMask = 1<<1;
        static const uint64_t kForceDevelopmentCacheMask = 1<<2;
        static const uint64_t kForceDyld2CacheMask = 1<<15;
        static const uint64_t kForceDyld3CacheMask = 1<<16;
        static const uint64_t kEnableCompactImageInfoMask = 1<<17;
        static const uint64_t kForceReadOnlyDataConstMask = 1<<18;
        static const uint64_t kForceReadWriteDataConstMask = 1<<19;
        //FIXME: Move this into __DATA_CONST once it is enabled for dyld
        static uint64_t _flags;
    };
#endif

} // namespace dyld3

#endif /* __DYLD_BOOTARGS_H__ */
