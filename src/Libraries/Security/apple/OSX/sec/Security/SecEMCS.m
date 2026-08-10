/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#define __KEYCHAINCORE__ 1

#include <Foundation/Foundation.h>
#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCFAllocator.h>
#include <corecrypto/ccpbkdf2.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccwrap.h>

#include <utilities/SecCFWrappers.h>
#include <AssertMacros.h>

#include "SecEMCSPriv.h"

static CFStringRef kiDMSSalt = CFSTR("salt");
static CFStringRef kiDMSIterrations = CFSTR("iter");
static CFStringRef kiDMSWrapEMCSKey = CFSTR("wkey");

#define MIN_ITERATIONS  1000
#define MIN_SALTLEN 16
#define KEY_LENGTH 16

/*
 *
 */

static CFDataRef
CopyWrappedKey(CFDataRef wrappingKey, CFDataRef unwrappedKey)
{
    const struct ccmode_ecb *ecb_mode = ccaes_ecb_encrypt_mode();
    ccecb_ctx_decl(ccecb_context_size(ecb_mode), key);
    CFMutableDataRef wrappedKey = NULL;

    require(CFDataGetLength(wrappingKey) == KEY_LENGTH, out);

    ccecb_init(ecb_mode, key, CFDataGetLength(wrappingKey), CFDataGetBytePtr(wrappingKey));

    wrappedKey = CFDataCreateMutableWithScratch(NULL, ccwrap_wrapped_size(CFDataGetLength(unwrappedKey)));
    require(wrappingKey, out);

    size_t obytes = 0;
    int wrap_status = ccwrap_auth_encrypt(ecb_mode, key, CFDataGetLength(unwrappedKey), CFDataGetBytePtr(unwrappedKey),
                                          &obytes, CFDataGetMutableBytePtr(wrappedKey));
    if (wrap_status == 0) {
        assert(obytes == (size_t)CFDataGetLength(wrappedKey));
    } else {
        CFReleaseNull(wrappedKey);
        goto out;
    }

 out:
    ccecb_ctx_clear(ccecb_context_size(ecb_mode), key);
    return wrappedKey;
}

static NSData *
CopyUnwrappedKey(CFDataRef wrappingKey, CFDataRef wrappedKey)
{
    const struct ccmode_ecb *ecb_mode = ccaes_ecb_decrypt_mode();
    ccecb_ctx_decl(ccecb_context_size(ecb_mode), key);
    NSMutableData *unwrappedKey = NULL;

    require(CFDataGetLength(wrappedKey) >= CCWRAP_SEMIBLOCK, out);
    require(CFDataGetLength(wrappingKey) == KEY_LENGTH, out);

    ccecb_init(ecb_mode, key, CFDataGetLength(wrappingKey), CFDataGetBytePtr(wrappingKey));

    unwrappedKey = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), ccwrap_unwrapped_size(CFDataGetLength(wrappedKey))));
    require(unwrappedKey, out);

    size_t obytes = 0;
    int unwrap_status = ccwrap_auth_decrypt(ecb_mode, key, CFDataGetLength(wrappedKey), CFDataGetBytePtr(wrappedKey),
                                            &obytes, [unwrappedKey mutableBytes]);
    if (unwrap_status == 0) {
        assert(obytes == (size_t)[unwrappedKey length]);
    } else {
        unwrappedKey = NULL;
        goto out;
    }

 out:
    ccecb_ctx_clear(ccecb_context_size(ecb_mode), key);
    return unwrappedKey;
}

/*
 *
 */

static CFDataRef
CreateDerivedKey(CFDataRef salt, long iterations, NSString *managedCredential)
{
    if (iterations < MIN_ITERATIONS || CFDataGetLength(salt) < MIN_SALTLEN)
        return NULL;

    /*
     * Assume users use the same normalization rules always
     */

    CFMutableDataRef key = CFDataCreateMutable(SecCFAllocatorZeroize(), KEY_LENGTH);
    if (key == NULL) {
        return NULL;
    }

    CFDataSetLength(key, KEY_LENGTH);

    int ret;
    ret = ccpbkdf2_hmac(ccsha256_di(),
                        strlen(managedCredential.UTF8String), managedCredential.UTF8String,
                        CFDataGetLength(salt), CFDataGetBytePtr(salt),
                        iterations,
                        KEY_LENGTH, CFDataGetMutableBytePtr(key));
    if (ret) {
        CFRelease(key);
        return NULL;
    }
    return key;
}


/*
 * Given a dictionary stored in iDMS and a passcode, return a crypto key
 */

NSData *
SecEMCSCreateDerivedEMCSKey(NSDictionary *iDMSData, NSString *managedCredential, NSError **error)
{
    CFDataRef userDerivedKey = NULL, emcsKey = NULL;
    CFNumberRef number = NULL;
    CFDataRef salt = NULL;
    NSData *key = NULL;
    long iterations;

    salt = CFDictionaryGetValue((__bridge CFDictionaryRef)iDMSData, kiDMSSalt);
    number = CFDictionaryGetValue((__bridge CFDictionaryRef)iDMSData, kiDMSIterrations);
    emcsKey = CFDictionaryGetValue((__bridge CFDictionaryRef)iDMSData, kiDMSWrapEMCSKey);

    /* validate parameters */
    if (!isData(salt) || !isNumber(number) || !isData(emcsKey))
        return NULL;

    if (!CFNumberGetValue(number, kCFNumberLongType, &iterations))
        return NULL;

    userDerivedKey = CreateDerivedKey(salt, iterations, managedCredential);
    if (userDerivedKey == NULL)
        return NULL;

    key = CopyUnwrappedKey(userDerivedKey, emcsKey);
    CFRelease(userDerivedKey);

    return key;
}

/*
 * Return a dictionary to be stored in iDMS
 */

NSDictionary *
SecEMCSCreateNewiDMSKey(NSDictionary *options,
                        NSData *oldEMCSKey,
                        NSString *managedCredential,
                        NSData **emcsKey,
                        NSError **error)
{
    CFMutableDataRef salt = NULL;
    const long iter = MIN_ITERATIONS;
    CFDataRef wrappedEMCSKey = NULL;
    CFMutableDataRef localEmcsKey = NULL;
    CFNumberRef iterations = NULL;
    CFDataRef userDerivedKey = NULL;
    CFDictionaryRef key = NULL;

    if (emcsKey)
        *emcsKey = NULL;

    if (oldEMCSKey) {
        if (CFGetTypeID((__bridge CFTypeRef)(oldEMCSKey)) != CFDataGetTypeID())
            return NULL;
        if (CFDataGetLength((__bridge CFDataRef)oldEMCSKey) != KEY_LENGTH)
            return NULL;
    }

    salt = CFDataCreateMutableWithScratch(NULL, MIN_SALTLEN);
    if (salt == NULL)
        goto out;

    if (SecRandomCopyBytes(NULL, CFDataGetLength(salt), CFDataGetMutableBytePtr(salt)) != 0)
        goto out;


    iterations = CFNumberCreate(NULL, kCFNumberLongType, &iter);
    if (iterations == NULL)
        goto out;

    if (oldEMCSKey) {
        localEmcsKey = CFDataCreateMutableCopy(SecCFAllocatorZeroize(), 0, (__bridge CFDataRef)oldEMCSKey);
    } else {
        localEmcsKey = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), KEY_LENGTH);
        if (localEmcsKey == NULL)
            goto out;
        if (SecRandomCopyBytes(NULL, CFDataGetLength(localEmcsKey), CFDataGetMutableBytePtr(localEmcsKey)) != 0)
            goto out;
    }

    userDerivedKey = CreateDerivedKey(salt, iter, managedCredential);
    if (userDerivedKey == NULL)
        goto out;

    wrappedEMCSKey = CopyWrappedKey(userDerivedKey, localEmcsKey);
    CFRelease(userDerivedKey);
    if (wrappedEMCSKey == NULL)
        goto out;

    const void *keys[] = {
        kiDMSSalt,
        kiDMSIterrations,
        kiDMSWrapEMCSKey,
    };
    const void *values[] = {
        salt,
        iterations,
        wrappedEMCSKey,
    };
    _Static_assert(sizeof(keys)/sizeof(keys[0]) == sizeof(values)/sizeof(values[0]), "keys != values");

    key = CFDictionaryCreate(NULL, keys, values, sizeof(keys)/sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (key && emcsKey)
        *emcsKey = CFRetain(localEmcsKey);

 out:
    CFReleaseNull(salt);
    CFReleaseNull(iterations);
    CFReleaseNull(localEmcsKey);
    CFReleaseNull(wrappedEMCSKey);

    return (__bridge NSDictionary *)key;
}
