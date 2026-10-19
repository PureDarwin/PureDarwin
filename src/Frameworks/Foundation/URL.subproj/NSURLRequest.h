// NSURLRequest.h - URL load requests and the responses that answer them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>

@class NSURL, NSString, NSData, NSDictionary, NSMutableDictionary, NSInputStream;

typedef NS_ENUM(NSUInteger, NSURLRequestCachePolicy) {
    NSURLRequestUseProtocolCachePolicy = 0,
    NSURLRequestReloadIgnoringLocalCacheData = 1,
    NSURLRequestReloadIgnoringLocalAndRemoteCacheData = 4,
    NSURLRequestReloadIgnoringCacheData = NSURLRequestReloadIgnoringLocalCacheData,
    NSURLRequestReturnCacheDataElseLoad = 2,
    NSURLRequestReturnCacheDataDontLoad = 3,
    NSURLRequestReloadRevalidatingCacheData = 5,
};

typedef NS_ENUM(NSUInteger, NSURLRequestNetworkServiceType) {
    NSURLNetworkServiceTypeDefault = 0,
    NSURLNetworkServiceTypeVoIP = 1,
    NSURLNetworkServiceTypeVideo = 2,
    NSURLNetworkServiceTypeBackground = 3,
    NSURLNetworkServiceTypeVoice = 4,
    NSURLNetworkServiceTypeResponsiveData = 6,
    NSURLNetworkServiceTypeAVStreaming = 8,
    NSURLNetworkServiceTypeResponsiveAV = 9,
    NSURLNetworkServiceTypeCallSignaling = 11,
};

@interface NSURLRequest : NSObject <NSSecureCoding, NSCopying, NSMutableCopying> {
    NSURL *_URL;
    NSURL *_mainDocumentURL;
    NSURLRequestCachePolicy _cachePolicy;
    NSTimeInterval _timeoutInterval;
    NSURLRequestNetworkServiceType _networkServiceType;
    NSString *_HTTPMethod;
    NSMutableDictionary *_headers;
    NSData *_HTTPBody;
    BOOL _shouldHandleCookies;
    BOOL _allowsCellularAccess;
}

+ (instancetype)requestWithURL:(NSURL *)URL;
+ (instancetype)requestWithURL:(NSURL *)URL cachePolicy:(NSURLRequestCachePolicy)cachePolicy timeoutInterval:(NSTimeInterval)timeoutInterval;
- (instancetype)initWithURL:(NSURL *)URL;
- (instancetype)initWithURL:(NSURL *)URL cachePolicy:(NSURLRequestCachePolicy)cachePolicy timeoutInterval:(NSTimeInterval)timeoutInterval;

@property (readonly, copy) NSURL *URL;
@property (readonly, copy) NSURL *mainDocumentURL;
@property (readonly) NSURLRequestCachePolicy cachePolicy;
@property (readonly) NSTimeInterval timeoutInterval;
@property (readonly) NSURLRequestNetworkServiceType networkServiceType;
@property (readonly, copy) NSString *HTTPMethod;
@property (readonly, copy) NSDictionary *allHTTPHeaderFields;
@property (readonly, copy) NSData *HTTPBody;
@property (readonly) NSInputStream *HTTPBodyStream;
@property (readonly) BOOL HTTPShouldHandleCookies;
@property (readonly) BOOL allowsCellularAccess;

- (NSString *)valueForHTTPHeaderField:(NSString *)field;

@end

@interface NSMutableURLRequest : NSURLRequest

@property (copy) NSURL *URL;
@property (copy) NSURL *mainDocumentURL;
@property NSURLRequestCachePolicy cachePolicy;
@property NSTimeInterval timeoutInterval;
@property NSURLRequestNetworkServiceType networkServiceType;
@property (copy) NSString *HTTPMethod;
@property (copy) NSDictionary *allHTTPHeaderFields;
@property (copy) NSData *HTTPBody;
@property BOOL HTTPShouldHandleCookies;
@property BOOL allowsCellularAccess;

- (void)setValue:(NSString *)value forHTTPHeaderField:(NSString *)field;
- (void)addValue:(NSString *)value forHTTPHeaderField:(NSString *)field;

@end

@interface NSURLResponse : NSObject <NSSecureCoding, NSCopying> {
    NSURL *_URL;
    NSString *_MIMEType;
    long long _expectedContentLength;
    NSString *_textEncodingName;
}

- (instancetype)initWithURL:(NSURL *)URL MIMEType:(NSString *)MIMEType expectedContentLength:(NSInteger)length
           textEncodingName:(NSString *)name;

@property (readonly, copy) NSURL *URL;
@property (readonly, copy) NSString *MIMEType;
@property (readonly) long long expectedContentLength;
@property (readonly, copy) NSString *textEncodingName;
@property (readonly, copy) NSString *suggestedFilename;

@end

@interface NSHTTPURLResponse : NSURLResponse {
    NSInteger _statusCode;
    NSDictionary *_headerFields;
}

- (instancetype)initWithURL:(NSURL *)url statusCode:(NSInteger)statusCode HTTPVersion:(NSString *)HTTPVersion
               headerFields:(NSDictionary *)headerFields;
+ (NSString *)localizedStringForStatusCode:(NSInteger)statusCode;

@property (readonly) NSInteger statusCode;
@property (readonly, copy) NSDictionary *allHeaderFields;

- (NSString *)valueForHTTPHeaderField:(NSString *)field;

@end
