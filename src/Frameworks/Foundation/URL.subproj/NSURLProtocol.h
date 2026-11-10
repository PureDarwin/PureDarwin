// NSURLProtocol.h - the base class URL loading hands custom schemes to
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <Foundation/NSURLRequest.h>

@class NSURLProtocol, NSError, NSData, NSString, NSCachedURLResponse, NSURLAuthenticationChallenge;

typedef NS_ENUM(NSUInteger, NSURLCacheStoragePolicy) {
    NSURLCacheStorageAllowed = 0,
    NSURLCacheStorageAllowedInMemoryOnly = 1,
    NSURLCacheStorageNotAllowed = 2,
};

@protocol NSURLProtocolClient <NSObject>
- (void)URLProtocol:(NSURLProtocol *)protocol wasRedirectedToRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse;
- (void)URLProtocol:(NSURLProtocol *)protocol cachedResponseIsValid:(NSCachedURLResponse *)cachedResponse;
- (void)URLProtocol:(NSURLProtocol *)protocol didReceiveResponse:(NSURLResponse *)response cacheStoragePolicy:(NSURLCacheStoragePolicy)policy;
- (void)URLProtocol:(NSURLProtocol *)protocol didLoadData:(NSData *)data;
- (void)URLProtocolDidFinishLoading:(NSURLProtocol *)protocol;
- (void)URLProtocol:(NSURLProtocol *)protocol didFailWithError:(NSError *)error;
@end

@interface NSURLProtocol : NSObject {
    NSURLRequest *_request;
    NSCachedURLResponse *_cachedResponse;
    id<NSURLProtocolClient> _client;
}

+ (BOOL)canInitWithRequest:(NSURLRequest *)request;
+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request;
+ (BOOL)requestIsCacheEquivalent:(NSURLRequest *)a toRequest:(NSURLRequest *)b;
+ (BOOL)registerClass:(Class)protocolClass;
+ (void)unregisterClass:(Class)protocolClass;
+ (id)propertyForKey:(NSString *)key inRequest:(NSURLRequest *)request;
+ (void)setProperty:(id)value forKey:(NSString *)key inRequest:(NSMutableURLRequest *)request;
+ (void)removePropertyForKey:(NSString *)key inRequest:(NSMutableURLRequest *)request;

- (instancetype)initWithRequest:(NSURLRequest *)request cachedResponse:(NSCachedURLResponse *)cachedResponse client:(id<NSURLProtocolClient>)client;
- (void)startLoading;
- (void)stopLoading;

@property (readonly) NSURLRequest *request;
@property (readonly) NSCachedURLResponse *cachedResponse;
@property (readonly) id<NSURLProtocolClient> client;

@end
