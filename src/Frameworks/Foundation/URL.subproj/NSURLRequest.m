// NSURLRequest.m - URL load requests and the responses that answer them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSString.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSCharacterSet.h>

// header names compare without case, as HTTP says
static NSString *headerKey(NSDictionary *headers, NSString *field) {
    for (NSString *key in headers) {
        if ([key caseInsensitiveCompare:field] == NSOrderedSame)
            return key;
    }
    return nil;
}

static NSString *headerValue(NSDictionary *headers, NSString *field) {
    NSString *key = headerKey(headers, field);
    return key ? [headers objectForKey:key] : nil;
}

@implementation NSURLRequest

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)requestWithURL:(NSURL *)URL {
    return [[[self alloc] initWithURL:URL] autorelease];
}

+ (instancetype)requestWithURL:(NSURL *)URL cachePolicy:(NSURLRequestCachePolicy)cachePolicy timeoutInterval:(NSTimeInterval)timeoutInterval {
    return [[[self alloc] initWithURL:URL cachePolicy:cachePolicy timeoutInterval:timeoutInterval] autorelease];
}

- (instancetype)init {
    return [self initWithURL:nil];
}

- (instancetype)initWithURL:(NSURL *)URL {
    return [self initWithURL:URL cachePolicy:NSURLRequestUseProtocolCachePolicy timeoutInterval:60];
}

- (instancetype)initWithURL:(NSURL *)URL cachePolicy:(NSURLRequestCachePolicy)cachePolicy timeoutInterval:(NSTimeInterval)timeoutInterval {
    if ((self = [super init]) != nil) {
        _URL = [URL copy];
        _cachePolicy = cachePolicy;
        _timeoutInterval = timeoutInterval;
        _HTTPMethod = @"GET";
        _headers = [[NSMutableDictionary alloc] init];
        _shouldHandleCookies = YES;
        _allowsCellularAccess = YES;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self initWithURL:[coder decodeObjectForKey:@"NS.URL"]]) != nil) {
        NSString *method = [coder decodeObjectForKey:@"NS.method"];
        NSDictionary *headers = [coder decodeObjectForKey:@"NS.headers"];

        if (method != nil) {
            [_HTTPMethod release];
            _HTTPMethod = [method copy];
        }
        if (headers != nil)
            [_headers setDictionary:headers];
        _HTTPBody = [[coder decodeObjectForKey:@"NS.body"] copy];
        _cachePolicy = [coder decodeIntegerForKey:@"NS.cachePolicy"];
        if ([coder containsValueForKey:@"NS.timeout"])
            _timeoutInterval = [coder decodeDoubleForKey:@"NS.timeout"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_URL forKey:@"NS.URL"];
    [coder encodeObject:_HTTPMethod forKey:@"NS.method"];
    [coder encodeObject:_headers forKey:@"NS.headers"];
    [coder encodeObject:_HTTPBody forKey:@"NS.body"];
    [coder encodeInteger:_cachePolicy forKey:@"NS.cachePolicy"];
    [coder encodeDouble:_timeoutInterval forKey:@"NS.timeout"];
}

- (void)dealloc {
    [_URL release];
    [_mainDocumentURL release];
    [_HTTPMethod release];
    [_headers release];
    [_HTTPBody release];
    [super dealloc];
}

- (id)_copyAsClass:(Class)cls {
    NSURLRequest *copy = [[cls alloc] initWithURL:_URL cachePolicy:_cachePolicy timeoutInterval:_timeoutInterval];

    copy->_mainDocumentURL = [_mainDocumentURL copy];
    copy->_networkServiceType = _networkServiceType;
    [copy->_HTTPMethod release];
    copy->_HTTPMethod = [_HTTPMethod copy];
    [copy->_headers setDictionary:_headers];
    copy->_HTTPBody = [_HTTPBody copy];
    copy->_shouldHandleCookies = _shouldHandleCookies;
    copy->_allowsCellularAccess = _allowsCellularAccess;
    return copy;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self _copyAsClass:[NSURLRequest class]];
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return [self _copyAsClass:[NSMutableURLRequest class]];
}

- (NSURL *)URL {
    return _URL;
}

- (NSURL *)mainDocumentURL {
    return _mainDocumentURL;
}

- (NSURLRequestCachePolicy)cachePolicy {
    return _cachePolicy;
}

- (NSTimeInterval)timeoutInterval {
    return _timeoutInterval;
}

- (NSURLRequestNetworkServiceType)networkServiceType {
    return _networkServiceType;
}

- (NSString *)HTTPMethod {
    return _HTTPMethod;
}

- (NSDictionary *)allHTTPHeaderFields {
    return [_headers count] ? [[_headers copy] autorelease] : nil;
}

- (NSData *)HTTPBody {
    return _HTTPBody;
}

- (NSInputStream *)HTTPBodyStream {
    return nil;
}

- (BOOL)HTTPShouldHandleCookies {
    return _shouldHandleCookies;
}

- (BOOL)allowsCellularAccess {
    return _allowsCellularAccess;
}

- (NSString *)valueForHTTPHeaderField:(NSString *)field {
    return headerValue(_headers, field);
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSURLRequest class]])
        return NO;
    NSURLRequest *request = other;
    return (_URL == request->_URL || [_URL isEqual:request->_URL]) && [_HTTPMethod isEqualToString:request->_HTTPMethod] &&
           [_headers isEqual:request->_headers] && (_HTTPBody == request->_HTTPBody || [_HTTPBody isEqual:request->_HTTPBody]);
}

- (NSUInteger)hash {
    return [_URL hash];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p> { URL: %@ }", [self class], self, _URL];
}

@end

@implementation NSMutableURLRequest

@dynamic URL, mainDocumentURL, cachePolicy, timeoutInterval, networkServiceType, HTTPMethod, allHTTPHeaderFields, HTTPBody;
@dynamic HTTPShouldHandleCookies, allowsCellularAccess;

- (void)setURL:(NSURL *)URL {
    NSURL *old = _URL;
    _URL = [URL copy];
    [old release];
}

- (void)setMainDocumentURL:(NSURL *)URL {
    NSURL *old = _mainDocumentURL;
    _mainDocumentURL = [URL copy];
    [old release];
}

- (void)setCachePolicy:(NSURLRequestCachePolicy)cachePolicy {
    _cachePolicy = cachePolicy;
}

- (void)setTimeoutInterval:(NSTimeInterval)timeoutInterval {
    _timeoutInterval = timeoutInterval;
}

- (void)setNetworkServiceType:(NSURLRequestNetworkServiceType)type {
    _networkServiceType = type;
}

- (void)setHTTPMethod:(NSString *)method {
    NSString *old = _HTTPMethod;
    _HTTPMethod = method ? [method copy] : @"GET";
    [old release];
}

- (void)setAllHTTPHeaderFields:(NSDictionary *)fields {
    for (NSString *field in fields)
        [self setValue:[fields objectForKey:field] forHTTPHeaderField:field];
}

- (void)setHTTPBody:(NSData *)body {
    NSData *old = _HTTPBody;
    _HTTPBody = [body copy];
    [old release];
}

- (void)setHTTPShouldHandleCookies:(BOOL)flag {
    _shouldHandleCookies = flag;
}

- (void)setAllowsCellularAccess:(BOOL)flag {
    _allowsCellularAccess = flag;
}

- (void)setValue:(NSString *)value forHTTPHeaderField:(NSString *)field {
    NSString *key = headerKey(_headers, field);

    if (key != nil)
        [_headers removeObjectForKey:key];
    if (value != nil)
        [_headers setObject:[[value copy] autorelease] forKey:[[field copy] autorelease]];
}

// a repeated header joins its values with commas
- (void)addValue:(NSString *)value forHTTPHeaderField:(NSString *)field {
    NSString *old = [self valueForHTTPHeaderField:field];
    [self setValue:old ? [NSString stringWithFormat:@"%@,%@", old, value] : value forHTTPHeaderField:field];
}

@end

@implementation NSURLResponse

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithURL:(NSURL *)URL MIMEType:(NSString *)MIMEType expectedContentLength:(NSInteger)length
           textEncodingName:(NSString *)name {
    if ((self = [super init]) != nil) {
        _URL = [URL copy];
        _MIMEType = [MIMEType copy];
        _expectedContentLength = length;
        _textEncodingName = [name copy];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithURL:[coder decodeObjectForKey:@"NS.URL"] MIMEType:[coder decodeObjectForKey:@"NS.MIMEType"]
          expectedContentLength:[coder decodeInt64ForKey:@"NS.length"] textEncodingName:[coder decodeObjectForKey:@"NS.encoding"]];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_URL forKey:@"NS.URL"];
    [coder encodeObject:_MIMEType forKey:@"NS.MIMEType"];
    [coder encodeInt64:_expectedContentLength forKey:@"NS.length"];
    [coder encodeObject:_textEncodingName forKey:@"NS.encoding"];
}

- (void)dealloc {
    [_URL release];
    [_MIMEType release];
    [_textEncodingName release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (NSURL *)URL {
    return _URL;
}

- (NSString *)MIMEType {
    return _MIMEType;
}

- (long long)expectedContentLength {
    return _expectedContentLength;
}

- (NSString *)textEncodingName {
    return _textEncodingName;
}

- (NSString *)suggestedFilename {
    NSString *name = [[_URL path] lastPathComponent];
    return [name length] && ![name isEqualToString:@"/"] ? name : @"Unknown";
}

@end

@implementation NSHTTPURLResponse

- (instancetype)initWithURL:(NSURL *)url statusCode:(NSInteger)statusCode HTTPVersion:(NSString *)HTTPVersion
               headerFields:(NSDictionary *)headerFields {
    NSString *type = headerValue(headerFields, @"Content-Type");
    NSString *length = headerValue(headerFields, @"Content-Length");
    NSString *mime = nil, *charset = nil;

    for (NSString *part in [type componentsSeparatedByString:@";"]) {
        NSString *trimmed = [part stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if (mime == nil)
            mime = trimmed;
        else if ([[trimmed lowercaseString] hasPrefix:@"charset="])
            charset = [trimmed substringFromIndex:8];
    }
    if ((self = [super initWithURL:url MIMEType:mime expectedContentLength:length ? [length longLongValue] : -1
                  textEncodingName:charset]) != nil) {
        _statusCode = statusCode;
        _headerFields = [headerFields copy];
    }
    return self;
}

- (void)dealloc {
    [_headerFields release];
    [super dealloc];
}

+ (NSString *)localizedStringForStatusCode:(NSInteger)statusCode {
    switch (statusCode) {
    case 200: return @"no error";
    case 201: return @"created";
    case 204: return @"no content";
    case 301: return @"moved permanently";
    case 302: return @"found";
    case 304: return @"not modified";
    case 400: return @"bad request";
    case 401: return @"unauthorized";
    case 403: return @"forbidden";
    case 404: return @"not found";
    case 500: return @"internal server error";
    case 503: return @"service unavailable";
    default:
        if (statusCode >= 100 && statusCode < 200) return @"informational";
        if (statusCode < 300) return @"success";
        if (statusCode < 400) return @"redirected";
        if (statusCode < 500) return @"client error";
        return @"server error";
    }
}

- (NSInteger)statusCode {
    return _statusCode;
}

- (NSDictionary *)allHeaderFields {
    return _headerFields;
}

- (NSString *)valueForHTTPHeaderField:(NSString *)field {
    return headerValue(_headerFields, field);
}

@end
