/*
 * Copyright (c) 2017 Apple Inc.  All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSXPCConnection_Private.h>
#include <xpc/private.h>
#include <xpc/xpc.h>

#include "ipc/securityd_client.h"
#include "ipc/server_security_helpers.h"
#include "ipc/server_entitlement_helpers.h"
#include "ipc/server_endpoint.h"

#include "keychain/securityd/SecItemServer.h"
#include <Security/SecEntitlements.h>

#pragma mark - Securityd Server

@implementation SecuritydXPCServer
@synthesize connection = _connection;

- (instancetype)initWithConnection:(NSXPCConnection *)connection
{
    if ((self = [super init])) {
        _connection = connection;

        if (!fill_security_client(&self->_client, connection.effectiveUserIdentifier, connection.auditToken)) {
            return nil;
        }
    }
    return self;
}

- (instancetype)initWithSecurityClient:(SecurityClient*) existingClient
{
    if(!existingClient) {
        return nil;
    }
    if((self = [super init])) {
        _connection = nil;

        self->_client.task                                   = CFRetainSafe(existingClient->task);
        self->_client.accessGroups                           = CFRetainSafe(existingClient->accessGroups);
        self->_client.allowSystemKeychain                    = existingClient->allowSystemKeychain;
        self->_client.allowSyncBubbleKeychain                = existingClient->allowSyncBubbleKeychain;
        self->_client.isNetworkExtension                     = existingClient->isNetworkExtension;
        self->_client.canAccessNetworkExtensionAccessGroups  = existingClient->canAccessNetworkExtensionAccessGroups;
        self->_client.uid                                    = existingClient->uid;
        self->_client.musr                                   = CFRetainSafe(existingClient->musr);
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) && TARGET_HAS_KEYSTORE
        self->_client.keybag                                 = existingClient->keybag;
#endif
#if TARGET_OS_IPHONE
        self->_client.inMultiUser                            = existingClient->inMultiUser;
        self->_client.activeUser                             = existingClient->activeUser;
#endif
        self->_client.applicationIdentifier                  = CFRetainSafe(existingClient->applicationIdentifier);
        self->_client.isAppClip                              = existingClient->isAppClip;
    }
    return self;
}


- (bool)clientHasBooleanEntitlement: (NSString*) entitlement {
    return SecTaskGetBooleanValueForEntitlement(self->_client.task, (__bridge CFStringRef) entitlement);
}

-(void)dealloc {
    CFReleaseNull(self->_client.task);
    CFReleaseNull(self->_client.accessGroups);
    CFReleaseNull(self->_client.musr);
    CFReleaseNull(self->_client.applicationIdentifier);
}
@end


// Class to use for local dispatching of securityd xpcs. Adds capability of fake entitlements, because you don't have a real task on the other end.
@interface LocalSecuritydXPCServer : SecuritydXPCServer
@property NSMutableDictionary<NSString*, id>* fakeEntitlements;
- (instancetype)initWithSecurityClient:(SecurityClient*) existingClient fakeEntitlements:(NSDictionary<NSString*, id>*)fakeEntitlements;
@end

@implementation LocalSecuritydXPCServer
- (instancetype)initWithSecurityClient:(SecurityClient*) existingClient fakeEntitlements:(NSDictionary<NSString*, id>*)fakeEntitlements {
    if((self = [super initWithSecurityClient: existingClient])) {
        _fakeEntitlements = [fakeEntitlements mutableCopy];
    }
    return self;
}

- (bool)clientHasBooleanEntitlement: (NSString*) entitlement {
    if(self.fakeEntitlements) {
        return [self.fakeEntitlements[entitlement] isEqual: @YES];
    } else {
        return false;
    }
}
@end


#pragma mark - SecuritydXPCServerListener

// Responsible for bringing up new SecuritydXPCServer objects, and configuring them with their remote connection
@interface SecuritydXPCServerListener : NSObject <NSXPCListenerDelegate>
@property (retain,nonnull) NSXPCListener *listener;
@end

@implementation SecuritydXPCServerListener
-(instancetype)init
{
    if((self = [super init])){
        self.listener = [[NSXPCListener alloc] initWithMachServiceName:@(kSecuritydGeneralServiceName)];
        self.listener.delegate = self;
        [self.listener resume];
    }
    return self;
}

- (BOOL)listener:(__unused NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    // Anyone is allowed to get a connection to securityd, except if you have kSecEntitlementKeychainDeny entitlement
    // The SecuritydClient class _must_ check for required entitlements in each XPC handler.

    if([newConnection valueForEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        return NO;
    }

    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(SecuritydXPCProtocol)];
    // Configure the interface on the server side, too
    [SecuritydXPCClient configureSecuritydXPCProtocol: newConnection.exportedInterface];

    newConnection.exportedObject = [[SecuritydXPCServer alloc] initWithConnection:newConnection];
    [newConnection resume];

    return YES;
}
@end

void
SecCreateSecuritydXPCServer(void)
{
    static SecuritydXPCServerListener* listener = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        @autoreleasepool {
            listener = [[SecuritydXPCServerListener alloc] init];
        }
    });
}

id<SecuritydXPCProtocol> SecCreateLocalSecuritydXPCServer(void) {
    // Create a fake securitydxpcserver using the access groups of securityd and some number of fake entitlements
    SecurityClient* client = SecSecurityClientGet();

    // We know that SecuritydXPCServerListener will comply with SecuritydXPCProtocol via category, so help the compiler out
    return (id<SecuritydXPCProtocol>) [[LocalSecuritydXPCServer alloc] initWithSecurityClient: client fakeEntitlements: @{}];
}

CFTypeRef SecCreateLocalCFSecuritydXPCServer(void) {
    return CFBridgingRetain(SecCreateLocalSecuritydXPCServer());
}

void SecResetLocalSecuritydXPCFakeEntitlements(void) {
    if([(__bridge id) gSecurityd->secd_xpc_server isKindOfClass: [LocalSecuritydXPCServer class]]) {
        LocalSecuritydXPCServer* server = (__bridge LocalSecuritydXPCServer*)gSecurityd->secd_xpc_server;
        server.fakeEntitlements = [[NSMutableDictionary alloc] init];
    }
}

void SecAddLocalSecuritydXPCFakeEntitlement(CFStringRef entitlement, CFTypeRef value) {
    if([(__bridge id) gSecurityd->secd_xpc_server isKindOfClass: [LocalSecuritydXPCServer class]]) {
        LocalSecuritydXPCServer* server = (__bridge LocalSecuritydXPCServer*)gSecurityd->secd_xpc_server;
        server.fakeEntitlements[(__bridge NSString*)entitlement] = (__bridge id)value;
    }
}
