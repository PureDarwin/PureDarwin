// NSXPCConnection.m - connections to services that cannot be reached
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// with no Foundation proxy layer over libxpc, every proxy message fails with NSXPCConnectionInvalid as for a missing service
#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSProxy.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSString.h>
#import <Foundation/NSError.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/FoundationErrors.h>
#include <objc/runtime.h>
#include <dispatch/dispatch.h>
#include <string.h>

@interface _NSXPCFailingProxy : NSProxy {
    Protocol *_protocol;
    id _errorHandler;
}
- (instancetype)initWithProtocol:(Protocol *)protocol errorHandler:(void (^)(NSError *))handler;
@end

@implementation _NSXPCFailingProxy

- (instancetype)initWithProtocol:(Protocol *)protocol errorHandler:(void (^)(NSError *))handler {
    _protocol = protocol;
    _errorHandler = [handler copy];
    return self;
}

- (void)dealloc {
    [_errorHandler release];
    [super dealloc];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel {
    struct objc_method_description method = { NULL, NULL };

    if (_protocol != NULL) {
        method = protocol_getMethodDescription(_protocol, sel, YES, YES);
        if (method.types == NULL)
            method = protocol_getMethodDescription(_protocol, sel, NO, YES);
    }
    // a selector the interface doesn't name still gets a reply-less signature
    return [NSMethodSignature signatureWithObjCTypes:method.types ? method.types : "v@:"];
}

// the reply block is dropped, only the error handler hears about the failure
- (void)forwardInvocation:(NSInvocation *)invocation {
    void (^handler)(NSError *) = _errorHandler;
    NSError *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSXPCConnectionInvalid
                                     userInfo:@{ @"NSDebugDescription" : @"The connection to the service is invalid." }];

    if (handler != nil)
        handler(error);
    if ([[invocation methodSignature] methodReturnLength] > 0) {
        char zero[64];
        memset(zero, 0, sizeof(zero));
        if ([[invocation methodSignature] methodReturnLength] <= sizeof(zero))
            [invocation setReturnValue:zero];
    }
}

- (BOOL)conformsToProtocol:(Protocol *)protocol {
    return _protocol != NULL && protocol_conformsToProtocol(_protocol, protocol);
}

@end

@implementation NSXPCInterface

+ (NSXPCInterface *)interfaceWithProtocol:(Protocol *)protocol {
    NSXPCInterface *interface = [[[self alloc] init] autorelease];

    interface->_protocol = protocol;
    return interface;
}

- (Protocol *)protocol { return _protocol; }
- (void)setProtocol:(Protocol *)protocol { _protocol = protocol; }
- (void)setClasses:(NSSet *)classes forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply { }
- (NSSet *)classesForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply { return [NSSet set]; }
- (void)setInterface:(NSXPCInterface *)ifc forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply { }
- (NSXPCInterface *)interfaceForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply { return nil; }

@end

@implementation NSXPCConnection

- (instancetype)initWithServiceName:(NSString *)serviceName {
    if ((self = [super init]) != nil)
        _serviceName = [serviceName copy];
    return self;
}

- (instancetype)initWithMachServiceName:(NSString *)name options:(NSXPCConnectionOptions)options {
    return [self initWithServiceName:name];
}

- (instancetype)initWithListenerEndpoint:(NSXPCListenerEndpoint *)endpoint {
    return [self initWithServiceName:nil];
}

- (void)dealloc {
    [_serviceName release];
    [_remoteObjectInterface release];
    [_exportedInterface release];
    [_exportedObject release];
    [_interruptionHandler release];
    [_invalidationHandler release];
    [super dealloc];
}

- (NSString *)serviceName { return _serviceName; }
- (NSXPCInterface *)remoteObjectInterface { return _remoteObjectInterface; }

- (void)setRemoteObjectInterface:(NSXPCInterface *)interface {
    [_remoteObjectInterface autorelease];
    _remoteObjectInterface = [interface retain];
}

- (NSXPCInterface *)exportedInterface { return _exportedInterface; }

- (void)setExportedInterface:(NSXPCInterface *)interface {
    [_exportedInterface autorelease];
    _exportedInterface = [interface retain];
}

- (id)exportedObject { return _exportedObject; }

- (void)setExportedObject:(id)object {
    [_exportedObject autorelease];
    _exportedObject = [object retain];
}

- (void (^)(void))interruptionHandler { return _interruptionHandler; }

- (void)setInterruptionHandler:(void (^)(void))handler {
    id old = _interruptionHandler;
    _interruptionHandler = [handler copy];
    [old release];
}

- (void (^)(void))invalidationHandler { return _invalidationHandler; }

- (void)setInvalidationHandler:(void (^)(void))handler {
    id old = _invalidationHandler;
    _invalidationHandler = [handler copy];
    [old release];
}

- (pid_t)processIdentifier { return 0; }
- (uid_t)effectiveUserIdentifier { return 0; }
- (gid_t)effectiveGroupIdentifier { return 0; }

- (id)remoteObjectProxy {
    return [self remoteObjectProxyWithErrorHandler:nil];
}

- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *))handler {
    return [[[_NSXPCFailingProxy alloc] initWithProtocol:[_remoteObjectInterface protocol] errorHandler:handler] autorelease];
}

- (id)synchronousRemoteObjectProxyWithErrorHandler:(void (^)(NSError *))handler {
    return [self remoteObjectProxyWithErrorHandler:handler];
}

// the service can't be reached, so the connection invalidates once it starts
- (void)resume {
    [self invalidate];
}

- (void)activate {
    [self resume];
}

- (void)suspend {
}

- (void)invalidate {
    void (^handler)(void);

    @synchronized(self) {
        if (_invalidated)
            return;
        _invalidated = YES;
        handler = [_invalidationHandler retain];
        [_invalidationHandler release];
        _invalidationHandler = nil;
    }
    if (handler != nil) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            handler();
            [handler release];
        });
    }
}

- (void)scheduleSendBarrierBlock:(void (^)(void))block {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), block);
}

@end
