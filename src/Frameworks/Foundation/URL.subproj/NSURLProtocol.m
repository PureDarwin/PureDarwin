// NSURLProtocol.m - the base class URL loading hands custom schemes to
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSURLProtocol.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <objc/runtime.h>
#import <pthread.h>

static pthread_mutex_t registryLock = PTHREAD_MUTEX_INITIALIZER;
static NSMutableArray *registeredClasses;
static char propertiesKey;

@implementation NSURLProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest *)request {
    [NSException raise:NSInternalInconsistencyException format:@"%@ must override +canInitWithRequest:", self];
    return NO;
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request {
    [NSException raise:NSInternalInconsistencyException format:@"%@ must override +canonicalRequestForRequest:", self];
    return request;
}

+ (BOOL)requestIsCacheEquivalent:(NSURLRequest *)a toRequest:(NSURLRequest *)b {
    return [[a URL] isEqual:[b URL]];
}

// the last class registered is asked first, as on macOS
+ (BOOL)registerClass:(Class)protocolClass {
    if (![protocolClass isSubclassOfClass:[NSURLProtocol class]])
        return NO;
    pthread_mutex_lock(&registryLock);
    if (registeredClasses == nil)
        registeredClasses = [[NSMutableArray alloc] init];
    [registeredClasses removeObject:protocolClass];
    [registeredClasses insertObject:protocolClass atIndex:0];
    pthread_mutex_unlock(&registryLock);
    return YES;
}

+ (void)unregisterClass:(Class)protocolClass {
    pthread_mutex_lock(&registryLock);
    [registeredClasses removeObject:protocolClass];
    pthread_mutex_unlock(&registryLock);
}

// protocol properties ride on the request itself
+ (id)propertyForKey:(NSString *)key inRequest:(NSURLRequest *)request {
    return [objc_getAssociatedObject(request, &propertiesKey) objectForKey:key];
}

+ (void)setProperty:(id)value forKey:(NSString *)key inRequest:(NSMutableURLRequest *)request {
    NSMutableDictionary *properties = objc_getAssociatedObject(request, &propertiesKey);

    if (properties == nil) {
        properties = [NSMutableDictionary dictionary];
        objc_setAssociatedObject(request, &propertiesKey, properties, OBJC_ASSOCIATION_RETAIN);
    }
    [properties setObject:value forKey:key];
}

+ (void)removePropertyForKey:(NSString *)key inRequest:(NSMutableURLRequest *)request {
    [objc_getAssociatedObject(request, &propertiesKey) removeObjectForKey:key];
}

- (instancetype)initWithRequest:(NSURLRequest *)request cachedResponse:(NSCachedURLResponse *)cachedResponse client:(id<NSURLProtocolClient>)client {
    if ((self = [super init]) != nil) {
        _request = [request copy];
        _cachedResponse = [cachedResponse retain];
        _client = [client retain];
    }
    return self;
}

- (void)dealloc {
    [_request release];
    [_cachedResponse release];
    [_client release];
    [super dealloc];
}

- (void)startLoading {
    [NSException raise:NSInternalInconsistencyException format:@"%@ must override -startLoading", [self class]];
}

- (void)stopLoading {
    [NSException raise:NSInternalInconsistencyException format:@"%@ must override -stopLoading", [self class]];
}

- (NSURLRequest *)request {
    return _request;
}

- (NSCachedURLResponse *)cachedResponse {
    return _cachedResponse;
}

- (id<NSURLProtocolClient>)client {
    return _client;
}

@end
