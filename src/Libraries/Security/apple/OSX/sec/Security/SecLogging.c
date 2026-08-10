/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <Security/SecLogging.h>
#include "securityd_client.h"
#include "SecuritydXPC.h"
#include <os/activity.h>

static bool dict_to_error_request(enum SecXPCOperation op, CFDictionaryRef query, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetPList(message, kSecXPCKeyQuery, query, error);
    }, NULL);
}

static CFDictionaryRef void_to_dict_error_request(enum SecXPCOperation op, CFErrorRef *error) {
    __block CFDictionaryRef dict = NULL;
    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, CFErrorRef *error) {
        return (dict = SecXPCDictionaryCopyPList(response, kSecXPCKeyResult, error));
    });
    return dict;
}

CFArrayRef SecGetCurrentServerLoggingInfo(CFErrorRef *error)
{
    __block CFArrayRef result;
    os_activity_initiate("SecGetCurrentServerLoggingInfo", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_get_log_settings, void_to_dict_error_request, error);
    });
    return result;
}

bool SecSetLoggingInfoForXPCScope(CFPropertyListRef /* String or Dictionary of strings */ settings, CFErrorRef *error)
{
    __block bool result;
    os_activity_initiate("SecSetLoggingInfoForXPCScope", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_set_xpc_log_settings, dict_to_error_request, settings, error);
    });
    return result;
}

bool SecSetLoggingInfoForCircleScope(CFPropertyListRef /* String or Dictionary of strings */ settings, CFErrorRef *error)
{
    __block bool result;
    os_activity_initiate("SecSetLoggingInfoForCircleScope", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_set_circle_log_settings, dict_to_error_request, settings, error);
    });
    return result;
}
