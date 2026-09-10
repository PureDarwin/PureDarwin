/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSConnection_h
#define NSConnection_h

#import <Foundation/NSObject.h>
#import <Foundation/NSProxy.h>
#import <Foundation/NSDate.h>

@class NSString, NSArray, NSMutableDictionary, NSConnection, NSRunLoop, NSPort;

FOUNDATION_EXPORT NSString * const NSConnectionDidDieNotification;
FOUNDATION_EXPORT NSString * const NSFailedAuthenticationException;
FOUNDATION_EXPORT NSString * const NSInvalidPortNameServerException;
FOUNDATION_EXPORT NSString * const NSObjectInaccessibleException;

/* Stands in for an object vended by another process. Every message it does
 * not implement is packaged up and sent over the connection. */
@interface NSDistantObject : NSProxy {
    NSConnection *_connection;
    Protocol *_protocol;
    NSMutableDictionary *_signatureCache;
    NSString *_targetToken;     /* non-nil for a vended object, not the root */
}

- (NSConnection *)connectionForProxy;
- (void)setProtocolForProxy:(Protocol *)protocol;

@end

@interface NSConnection : NSObject {
    void *_localPort;           /* CFMessagePortRef, service side */
    void *_remotePort;          /* CFMessagePortRef, client side */
    void *_queue;               /* dispatch_queue_t servicing the local port */
    id _keepAliveSource;
    int _keepAlivePipe[2];
    id _rootObject;
    id _delegate;
    NSString *_registeredName;
    NSDistantObject *_rootProxy;
    NSTimeInterval _requestTimeout;
    NSTimeInterval _replyTimeout;
    BOOL _isValid;
}

+ (NSConnection *)defaultConnection;

+ (NSConnection *)connectionWithRegisteredName:(NSString *)name host:(NSString *)host;
+ (id)rootProxyForConnectionWithRegisteredName:(NSString *)name host:(NSString *)host;

- (BOOL)registerName:(NSString *)name;

- (void)setRootObject:(id)object;
- (id)rootObject;
- (NSDistantObject *)rootProxy;

- (void)setDelegate:(id)delegate;
- (id)delegate;

- (void)setRequestTimeout:(NSTimeInterval)interval;
- (NSTimeInterval)requestTimeout;
- (void)setReplyTimeout:(NSTimeInterval)interval;
- (NSTimeInterval)replyTimeout;

/* This connection is serviced by a dispatch queue on a CFMessagePort, not by
 * an NSPort on a run loop, so there is no port for a caller to schedule. Both
 * answer nil; callers guard on that and skip the scheduling they would
 * otherwise do. */
- (NSPort *)receivePort;
- (NSPort *)sendPort;

- (void)addRunLoop:(NSRunLoop *)runLoop;
- (void)removeRunLoop:(NSRunLoop *)runLoop;
- (void)enableMultipleThreads;
- (void)setIndependentConversationQueueing:(BOOL)flag;

- (BOOL)isValid;
- (void)invalidate;

@end

#endif /* NSConnection_h */
