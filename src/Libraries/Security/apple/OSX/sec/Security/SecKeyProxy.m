/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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
 * SecKeyProxy.m - Remote access to SecKey instance
 */

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>

#include <Security/SecBasePriv.h>
#include <Security/SecKeyInternal.h>
#include <Security/SecIdentityPriv.h>
#include <utilities/debugging.h>
#include <utilities/SecCFError.h>

#include <Security/SecKeyProxy.h>

// MARK: XPC-level protocol for communication between server and client.
@protocol SecKeyProxyProtocol
- (void)initializeWithReply:(void (^)(NSData * _Nullable certificate))reply;
- (void)getBlockSizeWithReply:(void (^)(size_t blockSize))reply;
- (void)getAttributesWithReply:(void (^)(NSDictionary *attributes))reply;
- (void)getExternalRepresentationWithReply:(void (^)(NSData *data, NSError *error))reply;
- (void)getDescriptionWithReply:(void (^)(NSString *description))reply;
- (void)getAlgorithmIDWithReply:(void (^)(NSInteger algorithmID))reply;
- (void)getPublicKey:(void (^)(NSXPCListenerEndpoint *endpoint))reply;
- (void)performOperation:(SecKeyOperationType)operation algorithm:(NSString *)algorithm parameters:(NSArray *)parameters reply:(void (^)(NSArray *result, NSError *error))reply;
@end

// MARK: XPC target object for SecKeyProxy side
// Logically could be embedded in SecKeyProxy, but that would cause ownership cycles, since target object is always owned by its associated XPC connection.
@interface SecKeyProxyTarget : NSObject<SecKeyProxyProtocol> {
    id _key;
    NSData *_certificate;
    SecKeyProxy *_publicKeyProxy;
}
- (instancetype)initWithKey:(id)key certificate:(nullable NSData *)certificate;
@property (readonly, nonatomic) SecKeyRef key;
@end

@implementation SecKeyProxyTarget
- (instancetype)initWithKey:(id)key certificate:(nullable NSData *)certificate {
    if (self = [super init]) {
        _key = key;
        _certificate = certificate;
    }
    return self;
}

- (SecKeyRef)key {
    return (__bridge SecKeyRef)_key;
}

- (void)initializeWithReply:(void (^)(NSData *_Nullable))reply {
    return reply(_certificate);
}

- (void)getBlockSizeWithReply:(void (^)(size_t))reply {
    return reply(SecKeyGetBlockSize(self.key));
}

- (void)getAttributesWithReply:(void (^)(NSDictionary *))reply {
    return reply(CFBridgingRelease(SecKeyCopyAttributes(self.key)));
}

- (void)getExternalRepresentationWithReply:(void (^)(NSData *, NSError *))reply {
    NSError *error;
    NSData *data = CFBridgingRelease(SecKeyCopyExternalRepresentation(self.key, (void *)&error));
    return reply(data, error);
}

- (void)getDescriptionWithReply:(void (^)(NSString *))reply {
    NSString *description = CFBridgingRelease(CFCopyDescription(self.key));

    // Strip wrapping "<SecKeyRef " and ">" if present.
    if ([description hasPrefix:@"<SecKeyRef "] && [description hasSuffix:@">"]) {
        description = [description substringWithRange:NSMakeRange(11, description.length - 12)];
    } else if ([description hasPrefix:@"<SecKeyRef: "] && [description hasSuffix:@">"]) {
        description = [description substringWithRange:NSMakeRange(12, description.length - 13)];
    }

    return reply(description);
}

- (void)getAlgorithmIDWithReply:(void (^)(NSInteger))reply {
    return reply(SecKeyGetAlgorithmId(self.key));
}

- (void)getPublicKey:(void (^)(NSXPCListenerEndpoint *endpoint))reply {
    if (_publicKeyProxy == nil) {
        id publicKey = CFBridgingRelease(SecKeyCopyPublicKey(self.key));
        if (publicKey == nil) {
            return reply(nil);
        }
        _publicKeyProxy = [[SecKeyProxy alloc] initWithKey:(__bridge SecKeyRef)publicKey];
    }
    return reply(_publicKeyProxy.endpoint);
}

- (void)performOperation:(SecKeyOperationType)operation algorithm:(NSString *)algorithm parameters:(NSArray *)parameters reply:(void (^)(NSArray *, NSError *))reply {
    NSMutableArray *algorithms = @[algorithm].mutableCopy;
    CFTypeRef in1 = (__bridge CFTypeRef)(parameters.count > 0 ? parameters[0] : nil);
    CFTypeRef in2 = (__bridge CFTypeRef)(parameters.count > 1 ? parameters[1] : nil);
    NSError *error;
    SecKeyOperationContext context = { self.key, operation, (__bridge CFMutableArrayRef)algorithms };
    id result = CFBridgingRelease(SecKeyRunAlgorithmAndCopyResult(&context, in1, in2, (void *)&error));
    return reply(result ? @[result] : @[], error);
}
@end

// MARK: SecKeyProxy implementation
@interface SecKeyProxy() <NSXPCListenerDelegate>
@end

@implementation SecKeyProxy
- (instancetype)initWithKey:(SecKeyRef)key certificate:(nullable NSData *)certificate {
    if (self = [super init]) {
        if (key != nil) {
            _key = CFBridgingRelease(CFRetainSafe(key));
        } else {
            _key = nil;
        }
        _certificate = certificate;
        _listener = [NSXPCListener anonymousListener];
        _listener.delegate = self;

        // All connections created to this proxy instance are serialized to this single queue.
        [_listener _setQueue: dispatch_queue_create("SecKeyProxy", NULL)];
        [_listener resume];
    }

    return self;
}

- (instancetype)initWithKey:(SecKeyRef)key {
    return [self initWithKey:key certificate:nil];
}

- (instancetype)initWithIdentity:(SecIdentityRef)identity {
    id key;
    id certificate;
    SecIdentityCopyPrivateKey(identity, (void *)&key);
    SecIdentityCopyCertificate(identity, (void *)&certificate);
    if (key == nil && certificate == nil) {
        return nil;
    }

    // Extract data from the certificate.
    NSData *certificateData = CFBridgingRelease(SecCertificateCopyData((SecCertificateRef)certificate));
    if (certificateData == nil) {
        return nil;
    }

    return [self initWithKey:(__bridge SecKeyRef)key certificate:certificateData];
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(SecKeyProxyProtocol)];
    newConnection.exportedObject = [[SecKeyProxyTarget alloc] initWithKey:_key certificate:_certificate];
    [newConnection _setQueue:[_listener _queue]];
    [newConnection resume];
    return YES;
}

- (void)invalidate {
    [_listener invalidate];
}

- (void)dealloc {
    [self invalidate];
}

- (NSXPCListenerEndpoint *)endpoint {
    return _listener.endpoint;
}

// MARK: Client side: remote-connected SecKey instance.
static OSStatus SecRemoteKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    // keyData and key->key are both actually type-punned NSXPCConnection owning pointers.
    key->key = (void *)keyData;
    return errSecSuccess;
}

static void SecRemoteKeyDestroy(SecKeyRef key) {
    NSXPCConnection *conn = CFBridgingRelease(key->key);
    [conn invalidate];
}

+ (id<SecKeyProxyProtocol>)targetForKey:(SecKeyRef)key error:(CFErrorRef *)error {
    NSXPCConnection *connection = (__bridge NSXPCConnection *)key->key;
    id<SecKeyProxyProtocol> result = [connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull _error) {
        if (error != NULL) {
            *error = (__bridge_retained CFErrorRef)_error;
        }
    }];
    return result;
}

static size_t SecRemoteKeyBlockSize(SecKeyRef key) {
    __block size_t localBlockSize = 0;
    [[SecKeyProxy targetForKey:key error:NULL] getBlockSizeWithReply:^(size_t blockSize) {
        localBlockSize = blockSize;
    }];
    return localBlockSize;
}

static CFDictionaryRef SecRemoteKeyCopyAttributeDictionary(SecKeyRef key) {
    __block NSDictionary *localAttributes;
    [[SecKeyProxy targetForKey:key error:NULL] getAttributesWithReply:^(NSDictionary *attributes) {
        localAttributes = attributes;
    }];
    return CFBridgingRetain(localAttributes);
}

static CFDataRef SecRemoteKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    __block NSData *localData;
    __block NSError *localError;
    [[SecKeyProxy targetForKey:key error:error] getExternalRepresentationWithReply:^(NSData *data, NSError *error) {
        localData = data;
        localError = error;
    }];
    if (localData == nil && error != NULL) {
        *error = (__bridge_retained CFErrorRef)localError;
    }
    return CFBridgingRetain(localData);
}

static CFStringRef SecRemoteKeyCopyDescription(SecKeyRef key) {
    __block NSString *localDescription;
    [[SecKeyProxy targetForKey:key error:NULL] getDescriptionWithReply:^(NSString *description) {
        localDescription = [NSString stringWithFormat:@"<SecKeyRef remoteKey: %@>", description];
    }];
    return CFBridgingRetain(localDescription);
}

static CFIndex SecRemoteKeyGetAlgorithmID(SecKeyRef key) {
    __block CFIndex localAlgorithmID = kSecNullAlgorithmID;
    [[SecKeyProxy targetForKey:key error:NULL] getAlgorithmIDWithReply:^(NSInteger algorithmID) {
        localAlgorithmID = algorithmID;
    }];
    return localAlgorithmID;
}

static SecKeyRef SecRemoteKeyCopyPublicKey(SecKeyRef key) {
    __block id publicKey;
    [[SecKeyProxy targetForKey:key error:NULL] getPublicKey:^(NSXPCListenerEndpoint *endpoint) {
        if (endpoint != nil) {
            publicKey = CFBridgingRelease([SecKeyProxy createKeyFromEndpoint:endpoint error:nil]);
        }
    }];
    return (__bridge_retained SecKeyRef)publicKey;
}

static CFTypeRef SecRemoteKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm, CFArrayRef algorithms, SecKeyOperationMode mode, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    NSMutableArray *parameters = @[].mutableCopy;
    if (in1 != NULL) {
        [parameters addObject:(__bridge id)in1];
        if (in2 != NULL) {
            [parameters addObject:(__bridge id)in2];
        }
    }
    __block id localResult;
    [[SecKeyProxy targetForKey:key error:error] performOperation:operation algorithm:(__bridge NSString *)algorithm parameters:parameters reply:^(NSArray *result, NSError *_error) {
        if (result.count > 0) {
            localResult = result[0];
        }
        else if (error != NULL) {
            *error = (__bridge_retained CFErrorRef)_error;
        }
    }];
    return CFBridgingRetain(localResult);
}

static const SecKeyDescriptor SecRemoteKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "RemoteKey",
    .init = SecRemoteKeyInit,
    .destroy = SecRemoteKeyDestroy,
    .blockSize = SecRemoteKeyBlockSize,
    .copyDictionary = SecRemoteKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecRemoteKeyCopyExternalRepresentation,
    .describe = SecRemoteKeyCopyDescription,
    .getAlgorithmID = SecRemoteKeyGetAlgorithmID,
    .copyPublicKey = SecRemoteKeyCopyPublicKey,
    .copyOperationResult = SecRemoteKeyCopyOperationResult,
};

+ (SecKeyRef)createItemFromEndpoint:(NSXPCListenerEndpoint *)endpoint certificate:(NSData **)certificate error:(NSError * _Nullable __autoreleasing *)error {
    // Connect to the server proxy object.
    NSXPCConnection *connection = [[NSXPCConnection alloc] initWithListenerEndpoint:endpoint];
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(SecKeyProxyProtocol)];
    [connection resume];

    // Initialize remote object.
    __block NSError *localError;
    __block NSData *localCertificate;
    [[connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        localError = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecItemNotFound userInfo:@{NSUnderlyingErrorKey: error}];
    }] initializeWithReply:^(NSData *_Nullable _certificate){
        localCertificate = _certificate;
    }];
    if (localError == nil) {
        if (certificate != nil) {
            *certificate = localCertificate;
        }
    } else {
        [connection invalidate];
        if (error != NULL) {
            *error = localError;
        }
        return NULL;
    }

    // Wrap returned connection in SecKeyRef instance.
    return SecKeyCreate(kCFAllocatorDefault, &SecRemoteKeyDescriptor, CFBridgingRetain(connection), 0, kSecKeyEncodingRaw);
}

+ (SecKeyRef)createKeyFromEndpoint:(NSXPCListenerEndpoint *)endpoint error:(NSError * _Nullable __autoreleasing *)error {
    return [self createItemFromEndpoint:endpoint certificate:nil error:error];
}

+ (SecIdentityRef)createIdentityFromEndpoint:(NSXPCListenerEndpoint *)endpoint error:(NSError * _Nullable __autoreleasing *)error {
    NSData *certificateData;
    id key = CFBridgingRelease([self createItemFromEndpoint:endpoint certificate:&certificateData error:error]);
    if (key == nil) {
        return NULL;
    }
    if (certificateData == nil) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:(NSString *)kSecErrorDomain code:errSecParam userInfo:@{(id)NSLocalizedDescriptionKey: @"Attempt to create remote identity from key-only proxy"}];
        }
        return NULL;
    }

    id certificate = CFBridgingRelease(SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certificateData));
    return SecIdentityCreate(kCFAllocatorDefault, (__bridge SecCertificateRef)certificate, (__bridge SecKeyRef)key);
}
@end
