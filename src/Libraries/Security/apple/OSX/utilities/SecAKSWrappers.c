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

//
//  SecAKSWrappers.c
//  utilities
//

#include <utilities/SecAKSWrappers.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>

#if TARGET_OS_SIMULATOR
#  define change_notification "com.apple.will.never.happen"
#elif TARGET_OS_IPHONE
#  include <MobileKeyBag/MobileKeyBag.h>
#  define change_notification kMobileKeyBagLockStatusNotificationID
#elif TARGET_OS_MAC
#  include <AppleKeyStoreEvents.h>
#  define change_notification kAppleKeyStoreLockStatusNotificationID
#else
#  error "unsupported target platform"
#endif

#if TARGET_OS_OSX && TARGET_HAS_KEYSTORE
// OS X
const keybag_handle_t keybagHandle = session_keybag_handle;
#elif TARGET_HAS_KEYSTORE                                                         // iOS, but not simulator
const keybag_handle_t keybagHandle = device_keybag_handle;
#endif

#if TARGET_HAS_KEYSTORE
const AKSAssertionType_t lockAssertType = kAKSAssertTypeOther;
#endif

CFGiblisGetSingleton(dispatch_queue_t, GetKeybagAssertionQueue, sUserKeyBagAssertionLockQueue, ^{
    *sUserKeyBagAssertionLockQueue = dispatch_queue_create("AKS Lock Assertion Queue", NULL);
})

#if TARGET_HAS_KEYSTORE
static uint32_t count = 0;
#endif
const char * const kUserKeybagStateChangeNotification = change_notification;

bool SecAKSUserKeybagHoldLockAssertion(uint64_t timeout, CFErrorRef *error){
#if !TARGET_HAS_KEYSTORE
    return true;
#else
    __block kern_return_t status = kAKSReturnSuccess;

    dispatch_sync(GetKeybagAssertionQueue(), ^{
        if (count == 0) {
            secnotice("lockassertions", "Requesting lock assertion for %lld seconds", timeout);
            status = aks_assert_hold(keybagHandle, lockAssertType, timeout);
        }

        if (status == kAKSReturnSuccess)
            ++count;
    });
    return SecKernError(status, error, CFSTR("Kern return error"));
#endif /* !TARGET_HAS_KEYSTORE */
}

bool SecAKSUserKeybagDropLockAssertion(CFErrorRef *error){
#if !TARGET_HAS_KEYSTORE
    return true;
#else
    __block kern_return_t status = kAKSReturnSuccess;

    dispatch_sync(GetKeybagAssertionQueue(), ^{
        if (count && (--count == 0)) {
            secnotice("lockassertions", "Dropping lock assertion");
            status = aks_assert_drop(keybagHandle, lockAssertType);
        }
    });

    return SecKernError(status, error, CFSTR("Kern return error"));
#endif /* !TARGET_HAS_KEYSTORE */
}
bool SecAKSDoWithUserBagLockAssertion(CFErrorRef *error, dispatch_block_t action)
{
#if !TARGET_HAS_KEYSTORE
    action();
    return true;
#else
    // Acquire lock assertion, ref count?

    bool status = false;
    uint64_t timeout = 60ull;
    if (SecAKSUserKeybagHoldLockAssertion(timeout, error)) {
        action();
        status = SecAKSUserKeybagDropLockAssertion(error);
    }
    return status;
#endif  /* !TARGET_HAS_KEYSTORE */
}

bool SecAKSDoWithUserBagLockAssertionSoftly(dispatch_block_t action)
{
#if !TARGET_HAS_KEYSTORE
    action();
    return true;
#else
    uint64_t timeout = 60ull;
    CFErrorRef localError = NULL;

    bool heldAssertion = SecAKSUserKeybagHoldLockAssertion(timeout, &localError);
    if (!heldAssertion) {
        secnotice("secaks", "SecAKSDoWithUserBagLockAssertionSoftly: failed to get assertion proceeding bravely: %@", localError);
        CFReleaseNull(localError);
    }

    action();

    if (heldAssertion) {
        (void)SecAKSUserKeybagDropLockAssertion(&localError);
        CFReleaseNull(localError);
    }
    return true;
#endif  /* !TARGET_HAS_KEYSTORE */
}


CFDataRef SecAKSCopyBackupBagWithSecret(size_t size, uint8_t *secret, CFErrorRef *error) {
#if !TARGET_HAS_KEYSTORE
    return NULL;
#else
    CFDataRef result = NULL;
    void *keybagBytes = NULL;
    int keybagSize = 0;

    keybag_handle_t backupKeybagHandle = -1;
    kern_return_t ret;

    require_quiet(SecRequirementError(0 <= size && size <= INT_MAX, error, CFSTR("Invalid size: %zu"), size), fail);

    ret = aks_create_bag(secret, (int) size, kAppleKeyStoreAsymmetricBackupBag, &backupKeybagHandle);

    require_quiet(SecKernError(ret, error, CFSTR("bag allocation failed: %d"), ret), fail);

    ret = aks_save_bag(backupKeybagHandle, &keybagBytes, &keybagSize);

    require_quiet(SecKernError(ret, error, CFSTR("save bag failed: %d"), ret), fail);

    ret = aks_unload_bag(backupKeybagHandle);
    
    if (ret != KERN_SUCCESS) {
        secerror("unload bag failed: %d", ret);
    }

    result = CFDataCreate(kCFAllocatorDefault, keybagBytes, keybagSize);

    require_quiet(SecAllocationError(result, error, CFSTR("Bag CFData Allocation Failed")), fail);

fail:
    if (keybagBytes)
        free(keybagBytes);
    return result;
#endif
}

keyclass_t SecAKSSanitizedKeyclass(keyclass_t keyclass)
{
    keyclass_t sanitizedKeyclass = keyclass;
#if TARGET_HAS_KEYSTORE
    if (keyclass > key_class_last) {
        // idea is that AKS may return a keyclass value with extra bits above key_class_last from aks_wrap_key, but we only keep metadata keys for the canonical key classes
        // so just sanitize all our inputs to the canonical values
        sanitizedKeyclass = keyclass & key_class_last;
        secinfo("SanitizeKeyclass", "sanitizing request for keyclass %d to keyclass %d", keyclass, sanitizedKeyclass);
    }
#endif
    return sanitizedKeyclass;
}
