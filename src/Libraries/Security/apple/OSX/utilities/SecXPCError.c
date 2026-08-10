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


#include <stdio.h>

#include <utilities/SecXPCError.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/der_plist.h>

CFStringRef sSecXPCErrorDomain = CFSTR("com.apple.security.xpc");

static const char* kDomainKey = "domain";
static const char* kCodeKey = "code";
static const char* kUserInfoKey = "userinfo";

CFErrorRef SecCreateCFErrorWithXPCObject(xpc_object_t xpc_error)
{
    CFErrorRef result = NULL;

    if (xpc_get_type(xpc_error) == XPC_TYPE_DICTIONARY) {
        CFStringRef domain = NULL;

        const char * domain_string = xpc_dictionary_get_string(xpc_error, kDomainKey);
        if (domain_string != NULL) {
            domain = CFStringCreateWithCString(kCFAllocatorDefault, domain_string, kCFStringEncodingUTF8);
        } else {
            domain = sSecXPCErrorDomain;
            CFRetain(domain);
        }
        CFIndex code = (CFIndex) xpc_dictionary_get_int64(xpc_error, kCodeKey);

        CFTypeRef user_info = NULL;
        size_t size = 0;
        const uint8_t *der = xpc_dictionary_get_data(xpc_error, kUserInfoKey, &size);
        if (der) {
            const uint8_t *der_end = der + size;
            der = der_decode_plist(kCFAllocatorDefault,
                                   &user_info, NULL, der, der_end);
            if (der != der_end)
                CFReleaseNull(user_info);
        }

        result = CFErrorCreate(NULL, domain, code, user_info);

        CFReleaseSafe(user_info);
        CFReleaseSafe(domain);
    } else {
        SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, &result, NULL, CFSTR("Remote error not dictionary!: %@"), xpc_error);
    }
    return result;
}

static void SecXPCDictionarySetCFString(xpc_object_t dict, const char *key, CFStringRef string)
{
    CFStringPerformWithCString(string, ^(const char *utf8Str) {
        xpc_dictionary_set_string(dict, key, utf8Str);
    });
}

xpc_object_t SecCreateXPCObjectWithCFError(CFErrorRef error)
{
    xpc_object_t error_xpc = xpc_dictionary_create(NULL, NULL, 0);

    SecXPCDictionarySetCFString(error_xpc, kDomainKey, CFErrorGetDomain(error));
    xpc_dictionary_set_int64(error_xpc, kCodeKey, CFErrorGetCode(error));

    CFDictionaryRef user_info = CFErrorCopyUserInfo(error);
    size_t size = der_sizeof_plist(user_info, NULL);
    if (size) {
        uint8_t *der = malloc(size);
        uint8_t *der_end = der + size;
        uint8_t *der_start = der_encode_plist(user_info, NULL, der, der_end);
        if (der_start) {
            assert(der == der_start);
            xpc_dictionary_set_data(error_xpc, kUserInfoKey, der_start, der_end - der_start);
        }
        free(der);
    }
    CFRelease(user_info);

    return error_xpc;
}
