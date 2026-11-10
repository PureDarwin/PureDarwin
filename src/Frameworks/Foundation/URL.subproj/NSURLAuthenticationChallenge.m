// NSURLAuthenticationChallenge.m - a server's request for credentials, what it protects, and the answer
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSURLAuthenticationChallenge.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSError.h>
#include <CoreFoundation/CoreFoundation.h>

NSString *const NSURLAuthenticationMethodDefault = @"NSURLAuthenticationMethodDefault";
NSString *const NSURLAuthenticationMethodHTTPBasic = @"NSURLAuthenticationMethodHTTPBasic";
NSString *const NSURLAuthenticationMethodHTTPDigest = @"NSURLAuthenticationMethodHTTPDigest";
NSString *const NSURLAuthenticationMethodHTMLForm = @"NSURLAuthenticationMethodHTMLForm";
NSString *const NSURLAuthenticationMethodNTLM = @"NSURLAuthenticationMethodNTLM";
NSString *const NSURLAuthenticationMethodNegotiate = @"NSURLAuthenticationMethodNegotiate";
NSString *const NSURLAuthenticationMethodClientCertificate = @"NSURLAuthenticationMethodClientCertificate";
NSString *const NSURLAuthenticationMethodServerTrust = @"NSURLAuthenticationMethodServerTrust";

NSString *const NSURLProtectionSpaceHTTP = @"http";
NSString *const NSURLProtectionSpaceHTTPS = @"https";

@implementation NSURLProtectionSpace

- (instancetype)initWithHost:(NSString *)host port:(NSInteger)port protocol:(NSString *)protocol
    realm:(NSString *)realm authenticationMethod:(NSString *)authenticationMethod {
    if ((self = [super init])) {
        _host = [host copy];
        _port = port;
        _protocol = [protocol copy];
        _realm = [realm copy];
        _authenticationMethod = [(authenticationMethod ?: NSURLAuthenticationMethodDefault) copy];
    }
    return self;
}

- (instancetype)_initWithHost:(NSString *)host port:(NSInteger)port protocol:(NSString *)protocol
    serverTrust:(SecTrustRef)serverTrust {
    if ((self = [self initWithHost:host port:port protocol:protocol realm:nil
                authenticationMethod:NSURLAuthenticationMethodServerTrust]) && serverTrust)
        _serverTrust = (SecTrustRef)CFRetain(serverTrust);
    return self;
}

- (void)dealloc {
    [_host release];
    [_protocol release];
    [_realm release];
    [_authenticationMethod release];
    if (_serverTrust)
        CFRelease(_serverTrust);
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (NSString *)host { return _host; }
- (NSInteger)port { return _port; }
- (NSString *)protocol { return _protocol; }
- (NSString *)realm { return _realm; }
- (NSString *)authenticationMethod { return _authenticationMethod; }
- (SecTrustRef)serverTrust { return _serverTrust; }

- (BOOL)receivesCredentialSecurely {
    return [_protocol isEqualToString:NSURLProtectionSpaceHTTPS] ||
        [_authenticationMethod isEqualToString:NSURLAuthenticationMethodHTTPDigest];
}

@end

@implementation NSURLCredential

+ (NSURLCredential *)credentialWithUser:(NSString *)user password:(NSString *)password
    persistence:(NSURLCredentialPersistence)persistence {
    NSURLCredential *credential = [[[self alloc] init] autorelease];

    credential->_user = [user copy];
    credential->_password = [password copy];
    credential->_persistence = persistence;
    return credential;
}

+ (NSURLCredential *)credentialForTrust:(SecTrustRef)trust {
    NSURLCredential *credential = [[[self alloc] init] autorelease];

    credential->_trust = trust ? (SecTrustRef)CFRetain(trust) : NULL;
    credential->_persistence = NSURLCredentialPersistenceForSession;
    return credential;
}

+ (NSURLCredential *)credentialWithIdentity:(SecIdentityRef)identity certificates:(NSArray *)certificates
    persistence:(NSURLCredentialPersistence)persistence {
    NSURLCredential *credential = [[[self alloc] init] autorelease];

    credential->_identity = identity ? (SecIdentityRef)CFRetain(identity) : NULL;
    credential->_certificates = [certificates copy];
    credential->_persistence = persistence;
    return credential;
}

- (void)dealloc {
    [_user release];
    [_password release];
    [_certificates release];
    if (_trust)
        CFRelease(_trust);
    if (_identity)
        CFRelease(_identity);
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (NSString *)user { return _user; }
- (NSString *)password { return _password; }
- (BOOL)hasPassword { return _password != nil; }
- (NSURLCredentialPersistence)persistence { return _persistence; }
- (SecIdentityRef)identity { return _identity; }
- (NSArray *)certificates { return _certificates; }

@end

@implementation NSURLAuthenticationChallenge

- (instancetype)initWithProtectionSpace:(NSURLProtectionSpace *)space proposedCredential:(NSURLCredential *)credential
    previousFailureCount:(NSInteger)previousFailureCount failureResponse:(NSURLResponse *)response
    error:(NSError *)error sender:(id<NSURLAuthenticationChallengeSender>)sender {
    if ((self = [super init])) {
        _protectionSpace = [space retain];
        _proposedCredential = [credential retain];
        _previousFailureCount = previousFailureCount;
        _failureResponse = [(id)response retain];
        _error = [error retain];
        _sender = [sender retain];
    }
    return self;
}

- (void)dealloc {
    [_protectionSpace release];
    [_proposedCredential release];
    [(id)_failureResponse release];
    [_error release];
    [_sender release];
    [super dealloc];
}

- (NSURLProtectionSpace *)protectionSpace { return _protectionSpace; }
- (NSURLCredential *)proposedCredential { return _proposedCredential; }
- (NSInteger)previousFailureCount { return _previousFailureCount; }
- (NSURLResponse *)failureResponse { return _failureResponse; }
- (NSError *)error { return _error; }
- (id<NSURLAuthenticationChallengeSender>)sender { return _sender; }

@end
