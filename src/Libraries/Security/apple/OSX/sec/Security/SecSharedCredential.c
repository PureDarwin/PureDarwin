/*
 * Copyright (c) 2014-2020 Apple Inc. All Rights Reserved.
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
 *
 * SecSharedCredential.c - CoreFoundation-based functions to store and retrieve shared credentials.
 *
 */


#include <Security/SecSharedCredential.h>
#include <Security/SecBasePriv.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include "SecItemInternal.h"
#include <ipc/securityd_client.h>
#include "SecPasswordGenerate.h"

/* forward declarations */
OSStatus SecAddSharedWebCredentialSync(CFStringRef fqdn, CFStringRef account, CFStringRef password, CFErrorRef *error);
OSStatus SecCopySharedWebCredentialSync(CFStringRef fqdn, CFStringRef account, CFArrayRef *credentials, CFErrorRef *error);
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
OSStatus SecCopySharedWebCredentialSyncUsingAuthSvcs(CFStringRef fqdn, CFStringRef account, CFArrayRef *credentials, CFErrorRef *error);
#endif

#if SHAREDWEBCREDENTIALS

// OSX now has SWC enabled, but cannot link SharedWebCredentials framework: rdar://59958701
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST

OSStatus SecAddSharedWebCredentialSync(CFStringRef fqdn,
    CFStringRef account,
    CFStringRef password,
    CFErrorRef *error)
{
    OSStatus status = errSecUnimplemented;
    if (error) {
        SecError(status, error, CFSTR("SecAddSharedWebCredentialSync not supported on this platform"));
    }
    return status;
}

#else

OSStatus SecAddSharedWebCredentialSync(CFStringRef fqdn,
    CFStringRef account,
    CFStringRef password,
    CFErrorRef *error)
{
    OSStatus status;
    __block CFErrorRef* outError = error;
    __block CFMutableDictionaryRef args = CFDictionaryCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (fqdn) {
        CFDictionaryAddValue(args, kSecAttrServer, fqdn);
    }
    if (account) {
        CFDictionaryAddValue(args, kSecAttrAccount, account);
    }
    if (password) {
        CFDictionaryAddValue(args, kSecSharedPassword, password);
    }
    status = SecOSStatusWith(^bool (CFErrorRef *error) {
        CFTypeRef raw_result = NULL;
        bool xpc_result = false;
        bool internal_spi = false; // TODO: support this for SecurityDevTests
        if(internal_spi && gSecurityd && gSecurityd->sec_add_shared_web_credential) {
            xpc_result = gSecurityd->sec_add_shared_web_credential(args, NULL, NULL, NULL, SecAccessGroupsGetCurrent(), &raw_result, error);
        } else {
            xpc_result = cftype_client_to_bool_cftype_error_request(sec_add_shared_web_credential_id, args, SecSecurityClientGet(), &raw_result, error);
        }
        CFReleaseSafe(args);
        if (!xpc_result) {
            if (NULL == *error) {
                SecError(errSecInternal, error, CFSTR("Internal error (XPC failure)"));
            }
        }
        if (outError) {
            *outError = (error) ? *error : NULL;
            CFRetainSafe(*outError);
        } else {
            CFReleaseNull(*error);
        }
        CFReleaseNull(raw_result);
        return xpc_result;
    });

    return status;
}
#endif /* !TARGET_OS_OSX || !TARGET_OS_MACCATALYST */
#endif /* SHAREDWEBCREDENTIALS */

void SecAddSharedWebCredential(CFStringRef fqdn,
    CFStringRef account,
    CFStringRef password,
    void (^completionHandler)(CFErrorRef error))
{
	__block CFErrorRef error = NULL;
	__block dispatch_queue_t dst_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0);
#if SHAREDWEBCREDENTIALS

    /* sanity check input arguments */
	CFStringRef errStr = NULL;
	if (!fqdn || CFGetTypeID(fqdn) != CFStringGetTypeID() || !CFStringGetLength(fqdn) ||
		!account || CFGetTypeID(account) != CFStringGetTypeID() || !CFStringGetLength(account) ) {
		errStr = CFSTR("fqdn or account was not of type CFString, or not provided");
	}
	else if (password && CFGetTypeID(password) != CFStringGetTypeID()) {
		errStr = CFSTR("non-nil password was not of type CFString");
	}
	if (errStr) {
		SecError(errSecParam, &error, CFSTR("%@"), errStr);
		dispatch_async(dst_queue, ^{
			if (completionHandler) {
				completionHandler(error);
			}
			CFReleaseSafe(error);
		});
		return;
	}

	__block CFStringRef serverStr = CFRetainSafe(fqdn);
	__block CFStringRef accountStr = CFRetainSafe(account);
	__block CFStringRef passwordStr = CFRetainSafe(password);

	dispatch_async(dst_queue, ^{
		OSStatus status = SecAddSharedWebCredentialSync(serverStr, accountStr, passwordStr, &error);
		CFReleaseSafe(serverStr);
		CFReleaseSafe(accountStr);
		CFReleaseSafe(passwordStr);

		if (status && !error) {
			SecError(status, &error, CFSTR("Error adding shared password"));
		}
		dispatch_async(dst_queue, ^{
			if (completionHandler) {
				completionHandler(error);
			}
			CFReleaseSafe(error);
		});
	});
#else
    SecError(errSecParam, &error, CFSTR("SharedWebCredentials not supported on this platform"));
    dispatch_async(dst_queue, ^{
        if (completionHandler) {
            completionHandler(error);
        }
        CFReleaseSafe(error);
    });
#endif
}

#if SHAREDWEBCREDENTIALS

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST

OSStatus SecCopySharedWebCredentialSync(CFStringRef fqdn,
    CFStringRef account,
    CFArrayRef *credentials,
    CFErrorRef *error)
{
    OSStatus status = errSecUnimplemented;
    if (error) {
        SecError(status, error, CFSTR("SecCopySharedWebCredentialSync not supported on this platform"));
    }
    return status;
}

#else

OSStatus SecCopySharedWebCredentialSync(CFStringRef fqdn,
    CFStringRef account,
    CFArrayRef *credentials,
    CFErrorRef *error)
{
    OSStatus status;
    __block CFErrorRef* outError = error;
    __block CFMutableDictionaryRef args = CFDictionaryCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (fqdn) {
        CFDictionaryAddValue(args, kSecAttrServer, fqdn);
    }
    if (account) {
        CFDictionaryAddValue(args, kSecAttrAccount, account);
    }
    status = SecOSStatusWith(^bool (CFErrorRef *error) {
        CFTypeRef raw_result = NULL;
        bool xpc_result = false;
        bool internal_spi = false; // TODO: support this for SecurityDevTests
        if(internal_spi && gSecurityd && gSecurityd->sec_copy_shared_web_credential) {
            xpc_result = gSecurityd->sec_copy_shared_web_credential(args, NULL, NULL, NULL, SecAccessGroupsGetCurrent(), &raw_result, error);
        } else {
            xpc_result = cftype_client_to_bool_cftype_error_request(sec_copy_shared_web_credential_id, args, SecSecurityClientGet(), &raw_result, error);
        }
        CFReleaseSafe(args);
        if (!xpc_result) {
            if (NULL == *error) {
                SecError(errSecInternal, error, CFSTR("Internal error (XPC failure)"));
            }
        }
        if (outError) {
            *outError = (error) ? *error : NULL;
            CFRetainSafe(*outError);
        } else {
            CFReleaseNull(*error);
        }
        if (!raw_result) {
            raw_result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        }
        *credentials = raw_result;
        return xpc_result;
    });

    return status;
}
#endif /* !TARGET_OS_OSX || !TARGET_OS_MACCATALYST */
#endif /* SHAREDWEBCREDENTIALS */

void SecRequestSharedWebCredential(CFStringRef fqdn,
    CFStringRef account,
    void (^completionHandler)(CFArrayRef credentials, CFErrorRef error))
{
	__block CFErrorRef error = NULL;
	__block dispatch_queue_t dst_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0);
#if SHAREDWEBCREDENTIALS
    __block CFArrayRef result = NULL;

    /* sanity check input arguments, if provided */
	CFStringRef errStr = NULL;
	if (fqdn && (CFGetTypeID(fqdn) != CFStringGetTypeID() || !CFStringGetLength(fqdn))) {
		errStr = CFSTR("fqdn was empty or not a CFString");
    }
    else if (account && (CFGetTypeID(account) != CFStringGetTypeID() || !CFStringGetLength(account))) {
		errStr = CFSTR("account was empty or not a CFString");
	}
	if (errStr) {
		SecError(errSecParam, &error, CFSTR("%@"), errStr);
		dispatch_async(dst_queue, ^{
			if (completionHandler) {
				completionHandler(result, error);
			}
			CFReleaseSafe(error);
            CFReleaseSafe(result);
		});
		return;
	}

    __block CFStringRef serverStr = CFRetainSafe(fqdn);
	__block CFStringRef accountStr = CFRetainSafe(account);

    dispatch_async(dst_queue, ^{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
		OSStatus status = SecCopySharedWebCredentialSyncUsingAuthSvcs(serverStr, accountStr, &result, &error);
#else
		OSStatus status = SecCopySharedWebCredentialSync(serverStr, accountStr, &result, &error);
#endif
		CFReleaseSafe(serverStr);
		CFReleaseSafe(accountStr);

		if (status && !error) {
			SecError(status, &error, CFSTR("Error copying shared password"));
		}
		dispatch_async(dst_queue, ^{
			if (completionHandler) {
				completionHandler(result, error);
			}
			CFReleaseSafe(error);
			CFReleaseSafe(result);
		});
	});
#else
    SecError(errSecParam, &error, CFSTR("SharedWebCredentials not supported on this platform"));
    dispatch_async(dst_queue, ^{
        if (completionHandler) {
            completionHandler(NULL, error);
        }
        CFReleaseSafe(error);
    });
#endif /* SHAREDWEBCREDENTIALS */

}

CFStringRef SecCreateSharedWebCredentialPassword(void)
{

    CFStringRef password = NULL;
    CFErrorRef error = NULL;
    CFMutableDictionaryRef passwordRequirements = NULL;

    CFStringRef allowedCharacters = CFSTR("abcdefghkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    CFCharacterSetRef requiredCharactersLower = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR("abcdefghkmnopqrstuvwxyz"));
    CFCharacterSetRef requiredCharactersUppder = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR("ABCDEFGHJKLMNPQRSTUVWXYZ"));
    CFCharacterSetRef requiredCharactersNumbers = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR("3456789"));

    int groupSize = 3;
    int groupCount = 4;
    int totalLength = (groupSize * groupCount);
    CFNumberRef groupSizeRef = CFNumberCreate(NULL, kCFNumberIntType, &groupSize);
    CFNumberRef groupCountRef = CFNumberCreate(NULL, kCFNumberIntType, &groupCount);
    CFNumberRef totalLengthRef = CFNumberCreate(NULL, kCFNumberIntType, &totalLength);
    CFStringRef separator = CFSTR("-");

    CFMutableArrayRef requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, requiredCharactersLower);
    CFArrayAppendValue(requiredCharacterSets, requiredCharactersUppder);
    CFArrayAppendValue(requiredCharacterSets, requiredCharactersNumbers);

    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordGroupSize, groupSizeRef );
    CFDictionaryAddValue(passwordRequirements, kSecPasswordNumberOfGroups, groupCountRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordSeparator, separator);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, totalLengthRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, totalLengthRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordDefaultForType, CFSTR("false"));
    CFRelease(requiredCharactersLower);
    CFRelease(requiredCharactersUppder);
    CFRelease(requiredCharactersNumbers);
    CFRelease(groupSizeRef);
    CFRelease(groupCountRef);
    CFRelease(totalLengthRef);

    password = SecPasswordGenerate(kSecPasswordTypeSafari, &error, passwordRequirements);

    CFRelease(requiredCharacterSets);
    CFRelease(passwordRequirements);
    if ((error && error != errSecSuccess) || !password)
    {
        if (password) CFRelease(password);
        secwarning("SecPasswordGenerate failed to generate a password for SecCreateSharedWebCredentialPassword.");
        return NULL;
    } else {
        return password;
    }

}
