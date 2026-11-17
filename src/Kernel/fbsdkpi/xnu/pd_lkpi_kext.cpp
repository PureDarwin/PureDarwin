// SPDX-License-Identifier: MIT
// a library kext with no xnu side of its own, PDFreeBSDKPI carries the seam. The kext needs one source
#include <IOKit/IOLib.h>

__attribute__((visibility("hidden"), used)) static const char sKextAnchor[] = "fbsdkpi library";
