/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * Distributed Objects over CFMessagePort, which already carries the Mach
 * plumbing: a service registers a name with the bootstrap server, clients
 * look it up, and requests are synchronous with a reply.
 *
 * A call is archived as a dictionary - selector, type encoding, arguments -
 * and the reply carries either the return value or an exception.
 *
 * An object argument that can be archived travels by value. Anything else is
 * vended by reference: the sender keeps it in a table, and the far side gets an
 * NSDistantObject that calls back. Cocoa passes objects by reference by default
 * and copies only for bycopy, so this is the usual case rather than the
 * exception - a client registering itself for callbacks relies on it.
 *
 * Receiving those callbacks needs a port the far side can reach, and Darwin has
 * no bootstrap_register: a name can only be claimed by the job that declares it
 * in its launchd plist MachServices. So a process that vends objects by
 * reference must declare one, and NSConnection checks in under
 * "<process name>.doproxy" the first time it is asked to vend.
 */

#import <Foundation/NSConnection.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSData.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSException.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSSelectInputSource.h>
#import <Foundation/NSSocket_bsd.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSNull.h>
#include <CoreFoundation/CFMessagePort.h>
#include <CoreFoundation/CFRunLoop.h>
#include <dispatch/dispatch.h>
#include <unistd.h>
#include <stdlib.h>
#include <objc/runtime.h>
#include <string.h>

NSString * const NSConnectionDidDieNotification = @"NSConnectionDidDieNotification";
NSString * const NSFailedAuthenticationException = @"NSFailedAuthenticationException";
NSString * const NSInvalidPortNameServerException = @"NSInvalidPortNameServerException";
NSString * const NSObjectInaccessibleException = @"NSObjectInaccessibleException";

/* Message ids on the wire. */
enum {
    kNSConnectionSignatureRequest = 1,
    kNSConnectionInvocationRequest = 2
};

static NSString * const kSelectorKey = @"sel";
static NSString * const kTypesKey = @"types";
static NSString * const kArgumentsKey = @"args";
static NSString * const kReturnKey = @"ret";
static NSString * const kExceptionKey = @"exc";
static NSString * const kTargetKey = @"target";     /* vended-object token */
static NSString * const kProxyTokenKey = @"__doproxy";
static NSString * const kProxyServiceKey = @"__doservice";

/* Objects this process has vended by reference, keyed by token. */
static NSMutableDictionary *_vendedObjects = nil;
static NSString *_vendServiceName = nil;
static CFMessagePortRef _vendPort = NULL;
static NSUInteger _vendCounter = 0;

@interface NSConnection (Private)
- (NSData *)_handleRequestWithIdentifier:(SInt32)identifier data:(NSData *)data;
- (NSData *)_sendRequestWithIdentifier:(SInt32)identifier data:(NSData *)data;
@end

/* ---- argument boxing ---------------------------------------------------- */

/* The archiver handles the plist types natively and falls back to
 * -encodeWithCoder: for everything else, so that pair is the real test of
 * what can be sent - NSCoding conformance is not declared on our value
 * classes. Scalars travel as raw bytes because NSValue is neither. */
static BOOL _isArchivable(id object) {
    return [object isKindOfClass:[NSString class]] ||
           [object isKindOfClass:[NSNumber class]] ||
           [object isKindOfClass:[NSData class]] ||
           [object isKindOfClass:[NSArray class]] ||
           [object isKindOfClass:[NSDictionary class]] ||
           [object respondsToSelector:@selector(encodeWithCoder:)];
}

/* The port callbacks for vended objects share the connection callback below;
 * this forward declaration keeps them in one place. */
static CFDataRef _vendCallback(CFMessagePortRef local, SInt32 identifier,
                               CFDataRef data, void *info);

/* Claim "<process name>.doproxy". Returns nil if launchd was not told to hand
 * the name over, in which case nothing can be vended by reference. */
static NSString *_ensureVendService(void) {
    if (_vendServiceName != nil) {
        return _vendServiceName;
    }

    NSString *name = [NSString stringWithFormat:@"%@.doproxy",
                        [[NSProcessInfo processInfo] processName]];
    CFMessagePortContext context = { 0, NULL, NULL, NULL, NULL };
    Boolean shouldFreeInfo = false;
    CFMessagePortRef port = CFMessagePortCreateLocal(kCFAllocatorDefault,
                                                     (CFStringRef)name,
                                                     _vendCallback,
                                                     &context, &shouldFreeInfo);

    if (port == NULL) {
        return nil;
    }

    CFRunLoopSourceRef source =
        CFMessagePortCreateRunLoopSource(kCFAllocatorDefault, port, 0);

    if (source != NULL) {
        CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
        CFRelease(source);
    }

    _vendPort = port;
    _vendServiceName = [name copy];
    _vendedObjects = [[NSMutableDictionary alloc] initWithCapacity:0];

    return _vendServiceName;
}

/* Hand out a token for an object so the far side can call back into it. */
static NSDictionary *_vendObject(id object) {
    NSString *service = _ensureVendService();

    if (service == nil) {
        return nil;
    }

    NSString *token = [NSString stringWithFormat:@"%lu",
                        (unsigned long)(++_vendCounter)];

    [_vendedObjects setObject:object forKey:token];

    return [NSDictionary dictionaryWithObjectsAndKeys:
        token, kProxyTokenKey, service, kProxyServiceKey, nil];
}

static id _boxArgument(NSInvocation *invocation, NSUInteger index, const char *type) {
    while (*type == 'r' || *type == 'n' || *type == 'N' || *type == 'o' ||
           *type == 'O' || *type == 'R' || *type == 'V') {
        type++;
    }

    if (*type == '@') {
        id object = nil;

        [invocation getArgument:&object atIndex:(NSInteger)index];

        /* Cocoa sends objects by reference unless they are bycopy, so an
         * unarchivable argument is vended rather than refused. */
        if (object != nil && !_isArchivable(object)) {
            NSDictionary *reference = _vendObject(object);

            if (reference == nil) {
                [NSException raise:NSInvalidArgumentException
                            format:@"argument %lu (%s) cannot be sent by value "
                                   @"and this process vends no reference port; "
                                   @"declare MachServices \"%@.doproxy\" in its "
                                   @"launchd job to pass it by reference",
                                   (unsigned long)index,
                                   object_getClassName(object),
                                   [[NSProcessInfo processInfo] processName]];
                return nil;
            }
            return reference;
        }
        return object;
    }
    if (*type == ':') {
        SEL selector = NULL;

        [invocation getArgument:&selector atIndex:(NSInteger)index];
        return (selector != NULL) ? NSStringFromSelector(selector) : nil;
    }

    NSUInteger size = [[invocation methodSignature] _sizeOfArgumentAtIndex:index];
    void *bytes = calloc(1, size > 0 ? size : 1);

    [invocation getArgument:bytes atIndex:(NSInteger)index];

    NSData *raw = [NSData dataWithBytes:bytes length:size];

    free(bytes);
    return raw;
}

/* Turn a reference marker back into a proxy aimed at the vending process. */
static id _proxyForReference(NSDictionary *reference) {
    NSString *service = [reference objectForKey:kProxyServiceKey];
    NSString *token = [reference objectForKey:kProxyTokenKey];

    if (![service isKindOfClass:[NSString class]] ||
        ![token isKindOfClass:[NSString class]]) {
        return nil;
    }

    NSConnection *connection =
        [NSConnection connectionWithRegisteredName:service host:@""];

    if (connection == nil) {
        return nil;
    }

    NSDistantObject *proxy = [[NSDistantObject alloc] _initWithConnection:connection];

    [proxy _setTargetToken:token];

    return [proxy autorelease];
}

static void _unboxArgument(NSInvocation *invocation, NSUInteger index,
                           const char *type, id boxed) {
    while (*type == 'r' || *type == 'n' || *type == 'N' || *type == 'o' ||
           *type == 'O' || *type == 'R' || *type == 'V') {
        type++;
    }

    if (*type == '@') {
        id object = boxed;

        /* A reference marker becomes a proxy back to the sender. */
        if ([object isKindOfClass:[NSDictionary class]] &&
            [object objectForKey:kProxyTokenKey] != nil) {
            object = _proxyForReference(object);
        }

        [invocation setArgument:&object atIndex:(NSInteger)index];
        return;
    }
    if (*type == ':') {
        SEL selector = (boxed != nil) ? NSSelectorFromString(boxed) : NULL;

        [invocation setArgument:&selector atIndex:(NSInteger)index];
        return;
    }

    NSUInteger size = [[invocation methodSignature] _sizeOfArgumentAtIndex:index];
    void *bytes = calloc(1, size > 0 ? size : 1);

    if ([boxed isKindOfClass:[NSData class]] && [boxed length] == size) {
        memcpy(bytes, [boxed bytes], size);
    }
    [invocation setArgument:bytes atIndex:(NSInteger)index];
    free(bytes);
}

/* ---- CFMessagePort callback --------------------------------------------- */

/* Requests arriving for objects this process vended by reference. The message
 * format is the connection's, with kTargetKey naming which object. */
static CFDataRef _vendCallback(CFMessagePortRef local, SInt32 identifier,
                               CFDataRef data, void *info) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    CFDataRef result = NULL;

    if (identifier == kNSConnectionSignatureRequest) {
        /* Signature lookups carry the selector only; any vended object will
         * answer, since they share a protocol in practice. */
        NSString *name = [NSKeyedUnarchiver unarchiveObjectWithData:(NSData *)data];
        NSMethodSignature *signature = nil;

        for (NSString *token in _vendedObjects) {
            signature = [[_vendedObjects objectForKey:token]
                            methodSignatureForSelector:NSSelectorFromString(name)];
            if (signature != nil) {
                break;
            }
        }

        NSString *types = (signature != nil)
            ? [NSString stringWithUTF8String:[signature _types]] : @"";
        NSData *reply = [NSKeyedArchiver archivedDataWithRootObject:types];

        result = (reply != nil) ? (CFDataRef)CFRetain((CFDataRef)reply) : NULL;
        [pool drain];
        return result;
    }

    if (identifier != kNSConnectionInvocationRequest) {
        [pool drain];
        return NULL;
    }

    NSDictionary *call = [NSKeyedUnarchiver unarchiveObjectWithData:(NSData *)data];
    id target = [_vendedObjects objectForKey:[call objectForKey:kTargetKey]];

    if (target == nil) {
        [pool drain];
        return NULL;
    }

    NSString *types = [call objectForKey:kTypesKey];
    SEL selector = NSSelectorFromString([call objectForKey:kSelectorKey]);
    NSMethodSignature *signature =
        [NSMethodSignature signatureWithObjCTypes:[types UTF8String]];

    if (signature == nil || selector == NULL) {
        [pool drain];
        return NULL;
    }

    @try {
        NSInvocation *invocation =
            [NSInvocation invocationWithMethodSignature:signature];
        NSArray *arguments = [call objectForKey:kArgumentsKey];
        NSUInteger count = [signature numberOfArguments];

        [invocation setSelector:selector];

        for (NSUInteger i = 2; i < count && (i - 2) < [arguments count]; i++) {
            id boxed = [arguments objectAtIndex:i - 2];

            if ([boxed isKindOfClass:[NSNull class]]) {
                boxed = nil;
            }
            _unboxArgument(invocation, i, [signature getArgumentTypeAtIndex:i], boxed);
        }

        [invocation invokeWithTarget:target];
    }
    @catch (NSException *exception) {
        NSLog(@"NSConnection: vended call %@ raised %@: %@",
              NSStringFromSelector(selector), [exception name], [exception reason]);
    }

    [pool drain];
    return NULL;
}

static CFDataRef _connectionCallback(CFMessagePortRef local, SInt32 identifier,
                                     CFDataRef data, void *info) {
    NSConnection *connection = (NSConnection *)info;
    NSData *reply = [connection _handleRequestWithIdentifier:identifier
                                                        data:(NSData *)data];

    if (reply == nil) {
        return NULL;
    }
    return (CFDataRef)CFRetain((CFDataRef)reply);
}

/* ---- NSDistantObject ---------------------------------------------------- */

@implementation NSDistantObject

- (id)_initWithConnection:(NSConnection *)connection {
    _connection = connection;
    _signatureCache = [[NSMutableDictionary alloc] init];
    return self;
}

- (void)_setTargetToken:(NSString *)token {
    [_targetToken autorelease];
    _targetToken = [token copy];
}

- (NSString *)_targetToken {
    return _targetToken;
}

- (void)dealloc {
    [_signatureCache release];
    [super dealloc];
}

- (NSConnection *)connectionForProxy {
    return _connection;
}

- (void)setProtocolForProxy:(Protocol *)protocol {
    _protocol = protocol;
}

/* Signatures come from the protocol when one has been set, and otherwise from
 * the other side - which is a round trip, so the answers are cached. */
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    NSString *name = NSStringFromSelector(selector);
    NSMethodSignature *cached = [_signatureCache objectForKey:name];

    if (cached != nil) {
        return cached;
    }

    if (_protocol != NULL) {
        struct objc_method_description description =
            protocol_getMethodDescription(_protocol, selector, YES, YES);

        if (description.types == NULL) {
            description = protocol_getMethodDescription(_protocol, selector, NO, YES);
        }
        if (description.types != NULL) {
            NSMethodSignature *signature =
                [NSMethodSignature signatureWithObjCTypes:description.types];

            [_signatureCache setObject:signature forKey:name];
            return signature;
        }
    }

    NSData *request = [NSKeyedArchiver archivedDataWithRootObject:name];
    NSData *reply = [_connection _sendRequestWithIdentifier:kNSConnectionSignatureRequest
                                                       data:request];

    if (reply == nil) {
        return nil;
    }

    NSString *types = [NSKeyedUnarchiver unarchiveObjectWithData:reply];

    if (![types isKindOfClass:[NSString class]] || [types length] == 0) {
        return nil;
    }

    NSMethodSignature *signature =
        [NSMethodSignature signatureWithObjCTypes:[types UTF8String]];

    if (signature != nil) {
        [_signatureCache setObject:signature forKey:name];
    }
    return signature;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
    NSMethodSignature *signature = [invocation methodSignature];

    NSUInteger count = [signature numberOfArguments];
    NSMutableArray *arguments = [NSMutableArray arrayWithCapacity:count];

    /* Arguments 0 and 1 are the receiver and selector; the receiver is
     * whatever the far side vends, so only 2 onwards travel. */
    for (NSUInteger i = 2; i < count; i++) {
        id boxed = _boxArgument(invocation, i, [signature getArgumentTypeAtIndex:i]);

        [arguments addObject:(boxed != nil) ? boxed : (id)[NSNull null]];
    }

    NSMutableDictionary *call = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        NSStringFromSelector([invocation selector]), kSelectorKey,
        [NSString stringWithUTF8String:[signature _types]], kTypesKey,
        arguments, kArgumentsKey,
        nil];

    /* Aimed at a vended object rather than the far side's root. */
    if (_targetToken != nil) {
        [call setObject:_targetToken forKey:kTargetKey];
    }

    NSData *request = [NSKeyedArchiver archivedDataWithRootObject:call];


    /* A oneway void method does not wait for the far side to answer. */
    if ([signature isOneway] && [signature methodReturnLength] == 0) {
        [_connection _sendRequestWithIdentifier:kNSConnectionInvocationRequest
                                           data:request];
        return;
    }

    NSData *reply = [_connection _sendRequestWithIdentifier:kNSConnectionInvocationRequest
                                                       data:request];

    if (reply == nil) {
        [NSException raise:NSObjectInaccessibleException
                    format:@"no reply from the remote object for %@",
                           NSStringFromSelector([invocation selector])];
        return;
    }

    NSDictionary *result = [NSKeyedUnarchiver unarchiveObjectWithData:reply];
    NSString *failure = [result objectForKey:kExceptionKey];

    if (failure != nil) {
        [NSException raise:NSObjectInaccessibleException
                    format:@"%@", failure];
        return;
    }

    NSUInteger returnLength = [signature methodReturnLength];

    if (returnLength == 0) {
        return;
    }

    id boxed = [result objectForKey:kReturnKey];
    const char *returnType = [signature methodReturnType];

    while (*returnType == 'r' || *returnType == 'n' || *returnType == 'N' ||
           *returnType == 'o' || *returnType == 'O' || *returnType == 'R' ||
           *returnType == 'V') {
        returnType++;
    }

    if (*returnType == '@') {
        id object = ([boxed isKindOfClass:[NSNull class]]) ? nil : boxed;

        [invocation setReturnValue:&object];
        return;
    }

    void *bytes = calloc(1, returnLength);

    if ([boxed isKindOfClass:[NSData class]] && [boxed length] == returnLength) {
        memcpy(bytes, [boxed bytes], returnLength);
    }
    [invocation setReturnValue:bytes];
    free(bytes);
}

@end

/* ---- NSConnection ------------------------------------------------------- */

@implementation NSConnection

+ (NSConnection *)defaultConnection {
    static NSConnection *shared = nil;

    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _isValid = YES;
        _requestTimeout = 30.0;
        _replyTimeout = 30.0;
    }
    return self;
}

- (void)dealloc {
    [self invalidate];
    [_registeredName release];
    [_rootObject release];
    [_rootProxy release];
    [super dealloc];
}

+ (NSConnection *)connectionWithRegisteredName:(NSString *)name host:(NSString *)host {
    if (name == nil) {
        return nil;
    }

    CFMessagePortRef remote = CFMessagePortCreateRemote(kCFAllocatorDefault,
                                                        (CFStringRef)name);

    if (remote == NULL) {
        return nil;
    }

    NSConnection *connection = [[[self alloc] init] autorelease];

    connection->_remotePort = (void *)remote;
    connection->_registeredName = [name copy];
    return connection;
}

+ (id)rootProxyForConnectionWithRegisteredName:(NSString *)name host:(NSString *)host {
    NSConnection *connection = [self connectionWithRegisteredName:name host:host];

    return (connection != nil) ? [connection rootProxy] : nil;
}

- (BOOL)registerName:(NSString *)name {
    if (name == nil) {
        return NO;
    }

    CFMessagePortContext context = { 0, self, NULL, NULL, NULL };
    Boolean shouldFreeInfo = false;
    CFMessagePortRef local = CFMessagePortCreateLocal(kCFAllocatorDefault,
                                                      (CFStringRef)name,
                                                      _connectionCallback,
                                                      &context, &shouldFreeInfo);

    if (local == NULL) {
        return NO;
    }

    _localPort = (void *)local;
    [_registeredName release];
    _registeredName = [name copy];

    /* NSRunLoop is select()-driven and does not turn a CFRunLoop, so a
     * CFRunLoopSource here would never be serviced. Dispatch does not care
     * which run loop the process happens to be running. */
    /* Serviced on a dispatch queue rather than a CFRunLoopSource: NSRunLoop is
     * select()-driven and never turns a CFRunLoop, so a source here would only
     * be serviced by an app that drives CFRunLoopRun() itself. */
    _queue = dispatch_queue_create("org.puredarwin.NSConnection", NULL);
    CFMessagePortSetDispatchQueue(local, (dispatch_queue_t)_queue);

    /* Requests are serviced on the dispatch queue, but a vended object is
     * normally kept alive by [[NSRunLoop currentRunLoop] run], and that
     * returns at once unless the loop has an input source. This pipe is never
     * written to; it exists so -run blocks the way a DO service expects. */
    if (pipe(_keepAlivePipe) == 0) {
        _keepAliveSource = [[NSSelectInputSource socketInputSourceWithSocket:
            [[NSSocket_bsd socketWithDescriptor:_keepAlivePipe[0]] retain]] retain];
        [_keepAliveSource setSelectEventMask:NSSelectReadEvent];
        [[NSRunLoop currentRunLoop] addInputSource:_keepAliveSource
                                           forMode:NSDefaultRunLoopMode];
    }
    return YES;
}

- (void)setRootObject:(id)object {
    [object retain];
    [_rootObject release];
    _rootObject = object;
}

- (id)rootObject {
    return _rootObject;
}

- (NSDistantObject *)rootProxy {
    if (_rootProxy == nil && _remotePort != NULL) {
        _rootProxy = [[NSDistantObject alloc] _initWithConnection:self];
    }
    return _rootProxy;
}

- (void)setDelegate:(id)delegate {
    _delegate = delegate;
}

- (id)delegate {
    return _delegate;
}

- (void)setRequestTimeout:(NSTimeInterval)interval {
    _requestTimeout = interval;
}

- (NSTimeInterval)requestTimeout {
    return _requestTimeout;
}

- (void)setReplyTimeout:(NSTimeInterval)interval {
    _replyTimeout = interval;
}

- (NSTimeInterval)replyTimeout {
    return _replyTimeout;
}

/* Requests are serviced on the connection's dispatch queue, so attaching to a
 * particular run loop is a no-op. */
- (void)addRunLoop:(NSRunLoop *)runLoop {
}

- (void)removeRunLoop:(NSRunLoop *)runLoop {
}

/* CFMessagePort serialises requests through its run-loop source already. */
- (void)enableMultipleThreads {
}

- (void)setIndependentConversationQueueing:(BOOL)flag {
}

- (BOOL)isValid {
    if (!_isValid) {
        return NO;
    }
    if (_remotePort != NULL && !CFMessagePortIsValid((CFMessagePortRef)_remotePort)) {
        return NO;
    }
    if (_localPort != NULL && !CFMessagePortIsValid((CFMessagePortRef)_localPort)) {
        return NO;
    }
    return YES;
}

- (void)invalidate {
    if (!_isValid) {
        return;
    }
    _isValid = NO;

    if (_keepAliveSource != nil) {
        [[NSRunLoop currentRunLoop] removeInputSource:_keepAliveSource
                                              forMode:NSDefaultRunLoopMode];
        [_keepAliveSource release];
        _keepAliveSource = nil;
        close(_keepAlivePipe[0]);
        close(_keepAlivePipe[1]);
    }
    if (_localPort != NULL && _queue != NULL) {
        CFMessagePortSetDispatchQueue((CFMessagePortRef)_localPort, NULL);
    }
    if (_queue != NULL) {
        dispatch_release((dispatch_queue_t)_queue);
        _queue = NULL;
    }
    if (_localPort != NULL) {
        CFMessagePortInvalidate((CFMessagePortRef)_localPort);
        CFRelease((CFMessagePortRef)_localPort);
        _localPort = NULL;
    }
    if (_remotePort != NULL) {
        CFMessagePortInvalidate((CFMessagePortRef)_remotePort);
        CFRelease((CFMessagePortRef)_remotePort);
        _remotePort = NULL;
    }

    [[NSNotificationCenter defaultCenter] postNotificationName:NSConnectionDidDieNotification
                                                        object:self];
}

- (NSData *)_sendRequestWithIdentifier:(SInt32)identifier data:(NSData *)data {
    if (_remotePort == NULL) {
        return nil;
    }

    CFDataRef reply = NULL;
    SInt32 status = CFMessagePortSendRequest((CFMessagePortRef)_remotePort,
                                             identifier, (CFDataRef)data,
                                             _requestTimeout, _replyTimeout,
                                             kCFRunLoopDefaultMode, &reply);

    if (status != kCFMessagePortSuccess) {
        return nil;
    }
    return (reply != NULL) ? [(NSData *)reply autorelease] : nil;
}

- (NSData *)_handleRequestWithIdentifier:(SInt32)identifier data:(NSData *)data {
    if (_rootObject == nil) {
        return nil;
    }

    if (identifier == kNSConnectionSignatureRequest) {
        NSString *name = [NSKeyedUnarchiver unarchiveObjectWithData:data];

        if (![name isKindOfClass:[NSString class]]) {
            return nil;
        }

        NSMethodSignature *signature =
            [_rootObject methodSignatureForSelector:NSSelectorFromString(name)];
        NSString *types = (signature != nil)
            ? [NSString stringWithUTF8String:[signature _types]] : @"";

        return [NSKeyedArchiver archivedDataWithRootObject:types];
    }

    if (identifier != kNSConnectionInvocationRequest) {
        return nil;
    }

    NSDictionary *call = [NSKeyedUnarchiver unarchiveObjectWithData:data];


    if (![call isKindOfClass:[NSDictionary class]]) {
        return nil;
    }

    NSString *selectorName = [call objectForKey:kSelectorKey];
    NSString *types = [call objectForKey:kTypesKey];
    NSArray *arguments = [call objectForKey:kArgumentsKey];


    SEL selector = NSSelectorFromString(selectorName);
    NSMethodSignature *signature =
        [NSMethodSignature signatureWithObjCTypes:[types UTF8String]];

    if (signature == nil || selector == NULL) {
        return nil;
    }

    NSDictionary *result = nil;

    @try {
        NSInvocation *invocation =
            [NSInvocation invocationWithMethodSignature:signature];

        [invocation setSelector:selector];

        NSUInteger count = [signature numberOfArguments];

        for (NSUInteger i = 2; i < count && (i - 2) < [arguments count]; i++) {
            id boxed = [arguments objectAtIndex:i - 2];

            if ([boxed isKindOfClass:[NSNull class]]) {
                boxed = nil;
            }
            _unboxArgument(invocation, i, [signature getArgumentTypeAtIndex:i], boxed);
        }

        [invocation invokeWithTarget:_rootObject];

        NSUInteger returnLength = [signature methodReturnLength];

        if (returnLength == 0) {
            result = [NSDictionary dictionary];
        } else {
            const char *returnType = [signature methodReturnType];

            while (*returnType == 'r' || *returnType == 'n' || *returnType == 'N' ||
                   *returnType == 'o' || *returnType == 'O' || *returnType == 'R' ||
                   *returnType == 'V') {
                returnType++;
            }

            id boxed = nil;

            if (*returnType == '@') {
                id object = nil;

                [invocation getReturnValue:&object];
                boxed = (object != nil) ? object : (id)[NSNull null];
            } else {
                void *bytes = calloc(1, returnLength);

                [invocation getReturnValue:bytes];
                boxed = [NSData dataWithBytes:bytes length:returnLength];
                free(bytes);
            }
            result = [NSDictionary dictionaryWithObject:boxed forKey:kReturnKey];
        }
    }
    @catch (NSException *exception) {
        result = [NSDictionary dictionaryWithObject:[exception reason]
                                             forKey:kExceptionKey];
    }

    return [NSKeyedArchiver archivedDataWithRootObject:result];
}

@end
