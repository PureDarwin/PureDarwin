/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#ifndef _SECAKSWRAPPERS_H_
#define _SECAKSWRAPPERS_H_

#include <TargetConditionals.h>
#include "utilities/SecCFError.h"
#include <AssertMacros.h>
#include <dispatch/dispatch.h>

#include <CoreFoundation/CFData.h>

#if defined(USE_KEYSTORE)
#define TARGET_HAS_KEYSTORE USE_KEYSTORE

#else

#if RC_HORIZON
  #define TARGET_HAS_KEYSTORE 0
#elif TARGET_OS_SIMULATOR
  #define TARGET_HAS_KEYSTORE 0
#elif TARGET_OS_OSX
  #if TARGET_CPU_X86
    #define TARGET_HAS_KEYSTORE 0
  #else
    #define TARGET_HAS_KEYSTORE 1
  #endif
#elif TARGET_OS_IPHONE
  #define TARGET_HAS_KEYSTORE 1
#else
  #error "unknown keystore status for this platform"
#endif

#endif // USE_KEYSTORE

#if __has_include(<libaks.h>)
#include <libaks.h>
#else
#undef INCLUDE_MOCK_AKS
#define INCLUDE_MOCK_AKS 1
#endif

#if __has_include(<MobileKeyBag/MobileKeyBag.h>)
#include <MobileKeyBag/MobileKeyBag.h>
#else
#undef INCLUDE_MOCK_AKS
#define INCLUDE_MOCK_AKS 1
#endif

#if INCLUDE_MOCK_AKS
#include "tests/secdmockaks/mockaks.h"
#endif


bool hwaes_key_available(void);

//
// MARK: User lock state
//

enum {
    user_keybag_handle = (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) ? device_keybag_handle : session_keybag_handle,
};

extern const char * const kUserKeybagStateChangeNotification;

static inline bool SecAKSGetLockedState(keybag_state_t *state, CFErrorRef* error)
{
    kern_return_t status = aks_get_lock_state(user_keybag_handle, state);

    return SecKernError(status, error, CFSTR("aks_get_lock_state failed: %x"), status);
}

// returns true if any of the bits in bits is set in the current state of the user bag
static inline bool SecAKSLockedAnyStateBitIsSet(bool* isSet, keybag_state_t bits, CFErrorRef* error)
{
    keybag_state_t state;
    bool success = SecAKSGetLockedState(&state, error);
    
    require_quiet(success, exit);
    
    if (isSet)
        *isSet = (state & bits);
    
exit:
    return success;

}

static inline bool SecAKSGetIsLocked(bool* isLocked, CFErrorRef* error)
{
    return SecAKSLockedAnyStateBitIsSet(isLocked, keybag_state_locked, error);
}

static inline bool SecAKSGetIsUnlocked(bool* isUnlocked, CFErrorRef* error)
{
    bool isLocked = false;
    bool success = SecAKSGetIsLocked(&isLocked, error);

    if (success && isUnlocked)
        *isUnlocked = !isLocked;

    return success;
}

static inline bool SecAKSGetHasBeenUnlocked(bool* hasBeenUnlocked, CFErrorRef* error)
{
    return SecAKSLockedAnyStateBitIsSet(hasBeenUnlocked, keybag_state_been_unlocked, error);
}

bool SecAKSDoWithUserBagLockAssertion(CFErrorRef *error, dispatch_block_t action);

//just like SecAKSDoWithUserBagLockAssertion, but always perform action regardless if we got the assertion or not
bool SecAKSDoWithUserBagLockAssertionSoftly(dispatch_block_t action);
//
// if you can't use the block version above, use these.
// !!!!!Remember to balance them!!!!!!
//
bool SecAKSUserKeybagDropLockAssertion(CFErrorRef *error);
bool SecAKSUserKeybagHoldLockAssertion(uint64_t timeout, CFErrorRef *error);


CFDataRef SecAKSCopyBackupBagWithSecret(size_t size, uint8_t *secret, CFErrorRef *error);

keyclass_t SecAKSSanitizedKeyclass(keyclass_t keyclass);

#endif
