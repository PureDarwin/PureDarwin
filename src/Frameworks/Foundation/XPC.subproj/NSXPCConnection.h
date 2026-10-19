/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// PureDarwin has libxpc without the Foundation proxy layer, so these act like a connection to an
// unreachable service: proxy messages fail with NSXPCConnectionInvalid and the connection is invalidated
#ifndef NSXPCConnection_h
#define NSXPCConnection_h

#import <Foundation/NSObject.h>

@class NSXPCListener;
@class NSXPCListenerEndpoint;
@class NSXPCInterface;
@class NSString, NSError, NSSet;

typedef NS_OPTIONS(NSUInteger, NSXPCConnectionOptions) {
    NSXPCConnectionPrivileged = (1 << 12UL)
};

@protocol NSXPCProxyCreating
- (id)remoteObjectProxy;
- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *error))handler;
@optional
- (id)synchronousRemoteObjectProxyWithErrorHandler:(void (^)(NSError *error))handler;
@end

@interface NSXPCInterface : NSObject {
    Protocol *_protocol;
}

+ (NSXPCInterface *)interfaceWithProtocol:(Protocol *)protocol;

@property (assign) Protocol *protocol;

- (void)setClasses:(NSSet *)classes forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;
- (NSSet *)classesForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;
- (void)setInterface:(NSXPCInterface *)ifc forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;
- (NSXPCInterface *)interfaceForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;

@end

@interface NSXPCConnection : NSObject <NSXPCProxyCreating> {
    NSString *_serviceName;
    NSXPCInterface *_remoteObjectInterface;
    NSXPCInterface *_exportedInterface;
    id _exportedObject;
    id _interruptionHandler;
    id _invalidationHandler;
    BOOL _invalidated;
}

- (instancetype)initWithServiceName:(NSString *)serviceName;
- (instancetype)initWithMachServiceName:(NSString *)name options:(NSXPCConnectionOptions)options;
- (instancetype)initWithListenerEndpoint:(NSXPCListenerEndpoint *)endpoint;

@property (readonly, copy) NSString *serviceName;
@property (retain) NSXPCInterface *remoteObjectInterface;
@property (retain) NSXPCInterface *exportedInterface;
@property (retain) id exportedObject;
@property (copy) void (^interruptionHandler)(void);
@property (copy) void (^invalidationHandler)(void);
@property (readonly) pid_t processIdentifier;
@property (readonly) uid_t effectiveUserIdentifier;
@property (readonly) gid_t effectiveGroupIdentifier;

- (void)resume;
- (void)suspend;
- (void)activate;
- (void)invalidate;
- (void)scheduleSendBarrierBlock:(void (^)(void))block;

@end

#endif /* NSXPCConnection_h */
