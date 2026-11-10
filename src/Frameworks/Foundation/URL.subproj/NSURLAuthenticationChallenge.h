// NSURLAuthenticationChallenge.h - a server's request for credentials, what it protects, and the answer
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>

@class NSString, NSError, NSArray, NSURLResponse;

// Security is built after Foundation, so Foundation's own build declares the two types itself
#if __has_include(<Security/Security.h>)
#include <Security/Security.h>
#else
typedef struct __SecTrust *SecTrustRef;
typedef struct __SecIdentity *SecIdentityRef;
#endif

FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodDefault;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodHTTPBasic;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodHTTPDigest;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodHTMLForm;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodNTLM;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodNegotiate;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodClientCertificate;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodServerTrust;

FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceHTTP;
FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceHTTPS;

typedef NS_ENUM(NSUInteger, NSURLCredentialPersistence) {
    NSURLCredentialPersistenceNone,
    NSURLCredentialPersistenceForSession,
    NSURLCredentialPersistencePermanent,
    NSURLCredentialPersistenceSynchronizable,
};

@interface NSURLProtectionSpace : NSObject <NSCopying> {
    NSString *_host;
    NSInteger _port;
    NSString *_protocol;
    NSString *_realm;
    NSString *_authenticationMethod;
    SecTrustRef _serverTrust;
}

- (instancetype)initWithHost:(NSString *)host port:(NSInteger)port protocol:(NSString *)protocol
    realm:(NSString *)realm authenticationMethod:(NSString *)authenticationMethod;
// SPI: a server trust space carries the server's trust object
- (instancetype)_initWithHost:(NSString *)host port:(NSInteger)port protocol:(NSString *)protocol
    serverTrust:(SecTrustRef)serverTrust;

@property (readonly, copy) NSString *host;
@property (readonly) NSInteger port;
@property (readonly, copy) NSString *protocol;
@property (readonly, copy) NSString *realm;
@property (readonly, copy) NSString *authenticationMethod;
@property (readonly) SecTrustRef serverTrust;
@property (readonly) BOOL receivesCredentialSecurely;

@end

@interface NSURLCredential : NSObject <NSCopying> {
    NSString *_user;
    NSString *_password;
    NSURLCredentialPersistence _persistence;
    SecTrustRef _trust;
    SecIdentityRef _identity;
    NSArray *_certificates;
}

+ (NSURLCredential *)credentialWithUser:(NSString *)user password:(NSString *)password
    persistence:(NSURLCredentialPersistence)persistence;
+ (NSURLCredential *)credentialForTrust:(SecTrustRef)trust;
+ (NSURLCredential *)credentialWithIdentity:(SecIdentityRef)identity certificates:(NSArray *)certificates
    persistence:(NSURLCredentialPersistence)persistence;

@property (readonly, copy) NSString *user;
@property (readonly, copy) NSString *password;
@property (readonly) BOOL hasPassword;
@property (readonly) NSURLCredentialPersistence persistence;
@property (readonly) SecIdentityRef identity;
@property (readonly, copy) NSArray *certificates;

@end

@protocol NSURLAuthenticationChallengeSender;

@interface NSURLAuthenticationChallenge : NSObject {
    NSURLProtectionSpace *_protectionSpace;
    NSURLCredential *_proposedCredential;
    NSInteger _previousFailureCount;
    NSURLResponse *_failureResponse;
    NSError *_error;
    id _sender;
}

- (instancetype)initWithProtectionSpace:(NSURLProtectionSpace *)space proposedCredential:(NSURLCredential *)credential
    previousFailureCount:(NSInteger)previousFailureCount failureResponse:(NSURLResponse *)response
    error:(NSError *)error sender:(id<NSURLAuthenticationChallengeSender>)sender;

@property (readonly, copy) NSURLProtectionSpace *protectionSpace;
@property (readonly, copy) NSURLCredential *proposedCredential;
@property (readonly) NSInteger previousFailureCount;
@property (readonly, copy) NSURLResponse *failureResponse;
@property (readonly, copy) NSError *error;
@property (readonly, retain) id<NSURLAuthenticationChallengeSender> sender;

@end

@protocol NSURLAuthenticationChallengeSender <NSObject>
- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;
- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;
- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;
@end
