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
#import <objc/runtime.h>
#import <utilities/debugging.h>
#import <Security/SecXPCHelper.h>

#include <ipc/securityd_client.h>

@implementation SecuritydXPCClient
@synthesize connection = _connection;

- (instancetype) init
{
    if ((self = [super init])) {
        NSXPCInterface *interface = [NSXPCInterface interfaceWithProtocol:@protocol(SecuritydXPCProtocol)];

        self.connection = [[NSXPCConnection alloc] initWithMachServiceName:@(kSecuritydGeneralServiceName) options:0];
        if (self.connection == NULL) {
            return NULL;
        }

        self.connection.remoteObjectInterface = interface;
        [SecuritydXPCClient configureSecuritydXPCProtocol: self.connection.remoteObjectInterface];

        [self.connection resume];
    }

    return self;
}

+(void)configureSecuritydXPCProtocol: (NSXPCInterface*) interface {
    NSXPCInterface *rpcCallbackInterface = [NSXPCInterface interfaceWithProtocol: @protocol(SecuritydXPCCallbackProtocol)];
    [interface setInterface:rpcCallbackInterface
                forSelector:@selector(SecItemAddAndNotifyOnSync:syncCallback:complete:)
              argumentIndex:1
                    ofReply:0];

#if OCTAGON
    NSSet<Class> *errClasses = [SecXPCHelper safeErrorClasses];

    @try {
        [rpcCallbackInterface setClasses:errClasses forSelector:@selector(callCallback:error:) argumentIndex:1 ofReply:NO];

        [interface setClasses:errClasses forSelector:@selector(SecItemAddAndNotifyOnSync:
                                                               syncCallback:
                                                               complete:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(secItemSetCurrentItemAcrossAllDevices:
                                                               newCurrentItemHash:
                                                               accessGroup:
                                                               identifier:
                                                               viewHint:
                                                               oldCurrentItemReference:
                                                               oldCurrentItemHash:
                                                               complete:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(secItemFetchCurrentItemAcrossAllDevices:
                                                               identifier:
                                                               viewHint:
                                                               fetchCloudValue:
                                                               complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(secItemDigest:
                                                               accessGroup:
                                                               complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(secKeychainDeleteMultiuser:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(secItemVerifyBackupIntegrity:completion:) argumentIndex:1 ofReply:YES];

    }
    @catch(NSException* e) {
        secerror("Could not configure SecuritydXPCProtocol: %@", e);
        @throw e;
    }
#endif // OCTAGON
}
@end

@implementation SecuritydXPCCallback
@synthesize callback = _callback;

-(instancetype)initWithCallback: (SecBoolNSErrorCallback) callback {
    if((self = [super init])) {
        _callback = callback;
    }
    return self;
}

- (void)callCallback: (bool) result error:(NSError*) error {
    self.callback(result, error);
}
@end

id<SecuritydXPCProtocol> SecuritydXPCProxyObject(bool synchronous, void (^rpcErrorHandler)(NSError *))
{
    if (gSecurityd && gSecurityd->secd_xpc_server) {
        return (__bridge id<SecuritydXPCProtocol>)gSecurityd->secd_xpc_server;
    }

    static SecuritydXPCClient* rpc;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        rpc = [[SecuritydXPCClient alloc] init];
    });

    if (rpc == NULL) {
        rpcErrorHandler([NSError errorWithDomain:@"securityd" code:-1 userInfo:@{ NSLocalizedDescriptionKey : @"Could not create SecuritydXPCClient" }]);
        return NULL;
    } else {
        if (synchronous) {
            return [rpc.connection synchronousRemoteObjectProxyWithErrorHandler:rpcErrorHandler];
        } else {
            return [rpc.connection remoteObjectProxyWithErrorHandler:rpcErrorHandler];
        }
    }
}

