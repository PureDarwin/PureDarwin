/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

/*
 * Implements SecItem.c things using Obj-c.
 */

#include <Security/SecBasePriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <utilities/SecCFRelease.h>
#import <utilities/debugging.h>

#include <os/activity.h>

#import <Foundation/Foundation.h>

OSStatus _SecItemAddAndNotifyOnSync(CFDictionaryRef attributes, CFTypeRef * CF_RETURNS_RETAINED result, void (^syncCallback)(bool didSync, CFErrorRef error)) {
    __block SecCFDictionaryCOW attrs = { attributes };
    OSStatus status = errSecParam;

    os_activity_t activity = os_activity_create("_SecItemAddAndNotifyOnSync", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    status = SecOSStatusWith(^bool(CFErrorRef *statuserror) {
        return SecItemAuthDoQuery(&attrs, NULL, SecItemAdd, statuserror, ^bool(TKTokenRef token, CFDictionaryRef authedattributes, CFDictionaryRef unused, CFDictionaryRef auth_params, CFErrorRef *autherror) {
            if (token != NULL) {
                syncCallback(false, NULL);
                return false;
            }

            __block CFTypeRef raw_result = NULL;
            __block CFErrorRef raw_error = NULL;

            id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
                syncCallback(false, (__bridge CFErrorRef)error);
            });
            if (rpc == NULL) {
                return false;
            }
            SecuritydXPCCallback* xpcCallback = [[SecuritydXPCCallback alloc] initWithCallback: ^void(bool didSync, NSError* error) {
                syncCallback(didSync, (__bridge CFErrorRef) error);
            }];

            dispatch_semaphore_t wait_for_secd = dispatch_semaphore_create(0);
            [rpc SecItemAddAndNotifyOnSync: (__bridge NSDictionary*) authedattributes syncCallback:xpcCallback complete: ^void (NSDictionary* opDictResult, NSArray* opArrayResult, NSError* operror) {
                raw_result = opDictResult  ? CFBridgingRetain(opDictResult)  :
                             opArrayResult ? CFBridgingRetain(opArrayResult) : NULL;
                raw_error = (CFErrorRef) CFBridgingRetain(operror);
                dispatch_semaphore_signal(wait_for_secd);
            }];
            dispatch_semaphore_wait(wait_for_secd, DISPATCH_TIME_FOREVER);

            if(autherror) {
                *autherror = raw_error;
            }

            bool ok = false;

            // SecItemResultProcess isn't intended to handle error cases, so bypass it.
            if(!raw_error) {
                ok = SecItemResultProcess(authedattributes, auth_params, token, raw_result, result, autherror);
            }
            CFReleaseNull(raw_result);
            return ok;
       });
    });
    CFReleaseNull(attrs.mutable_dictionary);

    return status;
}

void SecItemSetCurrentItemAcrossAllDevices(CFStringRef accessGroup,
                                           CFStringRef identifier,
                                           CFStringRef viewHint,
                                           CFDataRef newCurrentItemReference,
                                           CFDataRef newCurrentItemHash,
                                           CFDataRef oldCurrentItemReference,
                                           CFDataRef oldCurrentItemHash,
                                           void (^complete)(CFErrorRef error))
{
    os_activity_t activity = os_activity_create("SecItemSetCurrentItemAcrossAllDevices", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
            complete((__bridge CFErrorRef) error);
        });
        [rpc secItemSetCurrentItemAcrossAllDevices:(__bridge NSData*)newCurrentItemReference
                                newCurrentItemHash:(__bridge NSData*)newCurrentItemHash
                                       accessGroup:(__bridge NSString*)accessGroup
                                        identifier:(__bridge NSString*)identifier
                                          viewHint:(__bridge NSString*)viewHint
                           oldCurrentItemReference:(__bridge NSData*)oldCurrentItemReference
                                oldCurrentItemHash:(__bridge NSData*)oldCurrentItemHash
                                          complete: ^ (NSError* operror) {
            complete((__bridge CFErrorRef) operror);
        }];
    }
}

void SecItemFetchCurrentItemAcrossAllDevices(CFStringRef accessGroup,
                                             CFStringRef identifier,
                                             CFStringRef viewHint,
                                             bool fetchCloudValue,
                                             void (^complete)(CFDataRef persistentRef, CFErrorRef error))
{
    os_activity_t activity = os_activity_create("SecItemFetchCurrentItemAcrossAllDevices", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
            complete(NULL, (__bridge CFErrorRef) error);
        });
        [rpc secItemFetchCurrentItemAcrossAllDevices:(__bridge NSString*)accessGroup
                                          identifier:(__bridge NSString*)identifier
                                            viewHint:(__bridge NSString*)viewHint
                                     fetchCloudValue:fetchCloudValue
                                            complete: ^(NSData* persistentRef, NSError* operror) {
                                                complete((__bridge CFDataRef) persistentRef, (__bridge CFErrorRef) operror);
                                            }];
    }
}

void _SecItemFetchDigests(NSString *itemClass, NSString *accessGroup, void (^complete)(NSArray<NSDictionary *> *, NSError *))
{
    os_activity_t activity = os_activity_create("_SecItemFetchDigests", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
        complete(NULL, error);
    });
    [rpc secItemDigest:itemClass accessGroup:accessGroup complete:complete];
}

void _SecKeychainDeleteMultiUser(NSString *musr, void (^complete)(bool, NSError *))
{
    os_activity_t activity = os_activity_create("_SecKeychainDeleteMultiUser", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:musr];
    if (uuid == NULL) {
        complete(false, NULL);
        return;
    }

    uuid_t musrUUID;
    [uuid getUUIDBytes:musrUUID];

    id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
        complete(false, error);
    });
    [rpc secKeychainDeleteMultiuser:[NSData dataWithBytes:musrUUID length:sizeof(uuid_t)] complete:^(bool status, NSError *error) {
        complete(status, error);
    }];
}

void SecItemVerifyBackupIntegrity(BOOL lightweight,
                                  void(^completion)(NSDictionary<NSString*, NSString*>* results, NSError* error))
{
    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(true, ^(NSError *error) {
            completion(@{@"summary" : @"XPC Error"}, error);
        });
        [rpc secItemVerifyBackupIntegrity:lightweight completion:completion];
    }
}

OSStatus SecItemDeleteKeychainItemsForAppClip(CFStringRef applicationIdentifier)
{
    __block OSStatus status = errSecInternal;
    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(true, ^(NSError *error) {
            secerror("xpc: failure to obtain XPC proxy object for app clip deletion, %@", error);
        });
        [rpc secItemDeleteForAppClipApplicationIdentifier:(__bridge NSString*)applicationIdentifier
                                               completion:^(OSStatus xpcStatus) {
            // Other errors turn into errSecInternal for caller
            secnotice("xpc", "app clip deletion result: %i", (int)xpcStatus);
            if (xpcStatus == errSecMissingEntitlement || xpcStatus == errSecSuccess) {
                status = xpcStatus;
            }
        }];
    }
    return status;
}

OSStatus SecItemPersistKeychainWritesAtHighPerformanceCost(CFErrorRef* error)
{
    os_activity_t activity = os_activity_create("SecItemPersistKeychainWritesAtHighPerformanceCost", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block OSStatus status = errSecInternal;
    __block CFErrorRef activityError = NULL;
    @autoreleasepool {
        secnotice("xpc", "This process is requesting a expensive full keychain database checkpoint");
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(true, ^(NSError *error) {
            secerror("xpc: failure to obtain XPC proxy object for Item Persistence, %@", error);
            activityError = (CFErrorRef)CFBridgingRetain(error);
        });
        [rpc secItemPersistKeychainWritesAtHighPerformanceCost:^(OSStatus xpcStatus,
                                                            NSError* xpcError) {
            if(xpcStatus != errSecSuccess) {
                secerror("xpc: Failed to persist keychain writes: %d %@", (int)xpcStatus, xpcError);
                activityError = (CFErrorRef)CFBridgingRetain(xpcError);
            } else {
                secnotice("xpc", "Successfully persisted keychain data to disk");
            }
            status = xpcStatus;
        }];
    }
    if(activityError) {
        if(error) {
            *error = CFRetainSafe(activityError);
        }
        CFReleaseNull(activityError);
    }
    return status;
}
