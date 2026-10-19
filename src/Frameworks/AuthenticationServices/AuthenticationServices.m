// AuthenticationServices.m - authorization requests with no providers behind them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// no single sign-on extensions or passkeys exist here, so every request completes as not handled

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

NSString *const ASAuthorizationErrorDomain = @"com.apple.AuthenticationServices.AuthorizationError";

typedef NS_ENUM(NSInteger, ASAuthorizationError) {
    ASAuthorizationErrorUnknown = 1000,
    ASAuthorizationErrorCanceled = 1001,
    ASAuthorizationErrorInvalidResponse = 1002,
    ASAuthorizationErrorNotHandled = 1003,
    ASAuthorizationErrorFailed = 1004,
    ASAuthorizationErrorNotInteractive = 1005,
};

@interface ASAuthorizationRequest : NSObject {
    id _provider;
}
@end

@implementation ASAuthorizationRequest
- (void)dealloc
{
    [_provider release];
    [super dealloc];
}
- (id)provider { return _provider; }
- (void)setProvider:(id)provider
{
    [_provider autorelease];
    _provider = [provider retain];
}
@end

@interface ASAuthorizationOpenIDRequest : ASAuthorizationRequest
@end

@implementation ASAuthorizationOpenIDRequest
@end

@interface ASAuthorizationSingleSignOnRequest : ASAuthorizationOpenIDRequest {
    NSArray *_headers;
    NSArray *_items;
    BOOL _userInterfaceEnabled;
}
@end

@implementation ASAuthorizationSingleSignOnRequest
- (void)dealloc
{
    [_headers release];
    [_items release];
    [super dealloc];
}
- (NSArray *)authorizationOptions { return _items; }
- (void)setAuthorizationOptions:(NSArray *)items
{
    [_items autorelease];
    _items = [items copy];
}
- (BOOL)isUserInterfaceEnabled { return _userInterfaceEnabled; }
- (void)setUserInterfaceEnabled:(BOOL)enabled { _userInterfaceEnabled = enabled; }
- (NSString *)requestedOperation { return @"login"; }
- (void)setRequestedOperation:(NSString *)operation { }
@end

@interface ASAuthorizationSingleSignOnProvider : NSObject {
    NSURL *_url;
}
@end

@implementation ASAuthorizationSingleSignOnProvider

+ (instancetype)authorizationProviderWithIdentityProviderURL:(NSURL *)url
{
    ASAuthorizationSingleSignOnProvider *p = [[[self alloc] init] autorelease];
    p->_url = [url copy];
    return p;
}

- (void)dealloc
{
    [_url release];
    [super dealloc];
}

- (NSURL *)url { return _url; }
// no extension is installed to answer for any identity provider
- (BOOL)canPerformAuthorization { return NO; }

- (ASAuthorizationSingleSignOnRequest *)createRequest
{
    ASAuthorizationSingleSignOnRequest *r = [[[ASAuthorizationSingleSignOnRequest alloc] init] autorelease];
    [r setProvider:self];
    return r;
}

@end

@interface ASAuthorizationSingleSignOnCredential : NSObject
@end

@implementation ASAuthorizationSingleSignOnCredential
- (NSString *)state { return nil; }
- (NSData *)accessToken { return nil; }
- (NSData *)identityToken { return nil; }
- (NSArray *)authorizedScopes { return @[]; }
- (id)authenticatedResponse { return nil; }
- (NSArray *)privateKeys { return @[]; }
@end

@interface ASAuthorization : NSObject
@end

@implementation ASAuthorization
- (id)provider { return nil; }
- (id)credential { return nil; }
@end

@interface ASAuthorizationController : NSObject {
    NSArray *_requests;
    id _delegate;
    id _presentationContextProvider;
}
@end

@interface NSObject (ASAuthorizationControllerDelegate)
- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithError:(NSError *)error;
@end

@implementation ASAuthorizationController

- (instancetype)initWithAuthorizationRequests:(NSArray *)requests
{
    if ((self = [super init]))
        _requests = [requests copy];
    return self;
}

- (void)dealloc
{
    [_requests release];
    [super dealloc];
}

- (NSArray *)authorizationRequests { return _requests; }
- (id)delegate { return _delegate; }
- (void)setDelegate:(id)delegate { _delegate = delegate; }
- (id)presentationContextProvider { return _presentationContextProvider; }
- (void)setPresentationContextProvider:(id)provider { _presentationContextProvider = provider; }
- (NSArray *)customAuthorizationMethods { return @[]; }
- (void)setCustomAuthorizationMethods:(NSArray *)methods { }

- (void)performRequests
{
    [self retain];
    dispatch_async(dispatch_get_main_queue(), ^{
        NSError *error = [NSError errorWithDomain:ASAuthorizationErrorDomain code:ASAuthorizationErrorNotHandled
                                         userInfo:@{ NSLocalizedFailureReasonErrorKey : @"No authorization provider is available." }];
        if ([_delegate respondsToSelector:@selector(authorizationController:didCompleteWithError:)])
            [_delegate authorizationController:self didCompleteWithError:error];
        [self release];
    });
}

- (void)performAutoFillAssistedRequests { [self performRequests]; }
- (void)performRequestsWithOptions:(NSUInteger)options { [self performRequests]; }
- (void)cancel { }

@end

// WebAuthn: the providers build requests, but no authenticator is reachable here, so performing
// them ends in ASAuthorizationErrorNotHandled through the controller above

typedef NSString *ASAuthorizationPublicKeyCredentialAttestationKind;
typedef NSString *ASAuthorizationPublicKeyCredentialResidentKeyPreference;
typedef NSString *ASAuthorizationPublicKeyCredentialUserVerificationPreference;
typedef NSString *ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport;

NSString *const ASAuthorizationPublicKeyCredentialAttestationKindNone = @"none";
NSString *const ASAuthorizationPublicKeyCredentialAttestationKindDirect = @"direct";
NSString *const ASAuthorizationPublicKeyCredentialAttestationKindIndirect = @"indirect";
NSString *const ASAuthorizationPublicKeyCredentialAttestationKindEnterprise = @"enterprise";
NSString *const ASAuthorizationPublicKeyCredentialResidentKeyPreferenceDiscouraged = @"discouraged";
NSString *const ASAuthorizationPublicKeyCredentialResidentKeyPreferencePreferred = @"preferred";
NSString *const ASAuthorizationPublicKeyCredentialResidentKeyPreferenceRequired = @"required";
NSString *const ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged = @"discouraged";
NSString *const ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred = @"preferred";
NSString *const ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired = @"required";
NSString *const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportUSB = @"usb";
NSString *const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportNFC = @"nfc";
NSString *const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportBluetooth = @"ble";

// requests and inputs keep what they are given as plain properties
@interface _ASPropertyBag : NSObject {
    NSMutableDictionary *_values;
}
@end

@implementation _ASPropertyBag

- (instancetype)init {
    if ((self = [super init]))
        _values = [[NSMutableDictionary alloc] init];
    return self;
}

- (void)dealloc {
    [_values release];
    [super dealloc];
}

- (id)_get:(NSString *)key { return [_values objectForKey:key]; }

- (void)_set:(id)value key:(NSString *)key {
    if (value)
        [_values setObject:value forKey:key];
    else
        [_values removeObjectForKey:key];
}

- (id)copyWithZone:(NSZone *)zone {
    _ASPropertyBag *copy = [[[self class] alloc] init];
    [copy->_values setDictionary:_values];
    return copy;
}

@end

#define AS_PROPERTY(getter, setter, name) \
    - (id)getter { return [self _get:name]; } \
    - (void)setter(id)value { [self _set:[[value copy] autorelease] key:name]; }

#define AS_FLAG(getter, setter, name) \
    - (BOOL)getter { return [[self _get:name] boolValue]; } \
    - (void)setter(BOOL)value { [self _set:[NSNumber numberWithBool:value] key:name]; }

@interface ASAuthorizationPublicKeyCredentialParameters : NSObject <NSCopying> {
    NSInteger _algorithm;
}
@end

@implementation ASAuthorizationPublicKeyCredentialParameters
+ (BOOL)supportsSecureCoding { return YES; }

- (instancetype)initWithAlgorithm:(NSInteger)algorithm {
    if ((self = [super init]))
        _algorithm = algorithm;
    return self;
}

- (NSInteger)algorithm { return _algorithm; }
- (id)copyWithZone:(NSZone *)zone { return [self retain]; }
@end

@interface ASAuthorizationPlatformPublicKeyCredentialDescriptor : _ASPropertyBag
@end

@implementation ASAuthorizationPlatformPublicKeyCredentialDescriptor
- (instancetype)initWithCredentialID:(NSData *)credentialID {
    if ((self = [super init]))
        [self _set:[[credentialID copy] autorelease] key:@"credentialID"];
    return self;
}
AS_PROPERTY(credentialID, setCredentialID:, @"credentialID")
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor : _ASPropertyBag
@end

@implementation ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor
- (instancetype)initWithCredentialID:(NSData *)credentialID transports:(NSArray *)transports {
    if ((self = [super init])) {
        [self _set:[[credentialID copy] autorelease] key:@"credentialID"];
        [self _set:[[transports copy] autorelease] key:@"transports"];
    }
    return self;
}
AS_PROPERTY(credentialID, setCredentialID:, @"credentialID")
AS_PROPERTY(transports, setTransports:, @"transports")
@end

typedef NS_ENUM(NSInteger, ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement) {
    ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementRequired,
    ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementPreferred,
};

typedef NS_ENUM(NSInteger, ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation) {
    ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperationRead,
    ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperationWrite,
};

@interface ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput : _ASPropertyBag
@end

@implementation ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput
- (instancetype)initWithSupportRequirement:(ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement)requirement {
    if ((self = [super init]))
        [self _set:[NSNumber numberWithInteger:requirement] key:@"supportRequirement"];
    return self;
}
- (ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement)supportRequirement { return [[self _get:@"supportRequirement"] integerValue]; }
@end

@interface ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput : _ASPropertyBag
@end

@implementation ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput
- (instancetype)initWithOperation:(ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation)operation {
    if ((self = [super init]))
        [self _set:[NSNumber numberWithInteger:operation] key:@"operation"];
    return self;
}
- (ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation)operation { return [[self _get:@"operation"] integerValue]; }
AS_PROPERTY(dataToWrite, setDataToWrite:, @"dataToWrite")
@end

@interface ASAuthorizationPublicKeyCredentialPRFAssertionInputValues : _ASPropertyBag
@end

@implementation ASAuthorizationPublicKeyCredentialPRFAssertionInputValues
- (instancetype)initWithSaltInput1:(NSData *)saltInput1 saltInput2:(NSData *)saltInput2 {
    if ((self = [super init])) {
        [self _set:[[saltInput1 copy] autorelease] key:@"saltInput1"];
        [self _set:[[saltInput2 copy] autorelease] key:@"saltInput2"];
    }
    return self;
}
- (NSData *)saltInput1 { return [self _get:@"saltInput1"]; }
- (NSData *)saltInput2 { return [self _get:@"saltInput2"]; }
@end

@interface ASAuthorizationPublicKeyCredentialPRFAssertionInput : _ASPropertyBag
@end

@implementation ASAuthorizationPublicKeyCredentialPRFAssertionInput
- (instancetype)initWithInputValues:(ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *)values
            perCredentialInputValues:(NSDictionary *)perCredentialValues {
    if ((self = [super init])) {
        [self _set:values key:@"inputValues"];
        [self _set:[[perCredentialValues copy] autorelease] key:@"perCredentialInputValues"];
    }
    return self;
}
- (id)inputValues { return [self _get:@"inputValues"]; }
- (NSDictionary *)perCredentialInputValues { return [self _get:@"perCredentialInputValues"]; }
@end

@interface ASAuthorizationPublicKeyCredentialPRFRegistrationInput : _ASPropertyBag
@end

@implementation ASAuthorizationPublicKeyCredentialPRFRegistrationInput
+ (instancetype)checkForSupport { return [[[self alloc] init] autorelease]; }
- (instancetype)initWithInputValues:(ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *)values {
    if ((self = [super init])) {
        [self _set:values key:@"inputValues"];
        [self _set:[NSNumber numberWithBool:YES] key:@"shouldCheckForSupport"];
    }
    return self;
}
- (id)inputValues { return [self _get:@"inputValues"]; }
AS_FLAG(shouldCheckForSupport, setShouldCheckForSupport:, @"shouldCheckForSupport")
@end

// what requests from both providers carry
@interface _ASPublicKeyCredentialRequest : ASAuthorizationRequest {
    NSMutableDictionary *_values;
}
@end

@implementation _ASPublicKeyCredentialRequest

- (instancetype)init {
    if ((self = [super init]))
        _values = [[NSMutableDictionary alloc] init];
    return self;
}

- (void)dealloc {
    [_values release];
    [super dealloc];
}

- (id)_get:(NSString *)key { return [_values objectForKey:key]; }

- (void)_set:(id)value key:(NSString *)key {
    if (value)
        [_values setObject:value forKey:key];
    else
        [_values removeObjectForKey:key];
}

AS_PROPERTY(relyingPartyIdentifier, setRelyingPartyIdentifier:, @"relyingPartyIdentifier")
AS_PROPERTY(challenge, setChallenge:, @"challenge")
AS_PROPERTY(clientData, setClientData:, @"clientData")
AS_PROPERTY(clientDataHash, setClientDataHash:, @"clientDataHash")
AS_PROPERTY(userVerificationPreference, setUserVerificationPreference:, @"userVerificationPreference")
AS_PROPERTY(attestationPreference, setAttestationPreference:, @"attestationPreference")
AS_PROPERTY(residentKeyPreference, setResidentKeyPreference:, @"residentKeyPreference")
AS_PROPERTY(credentialParameters, setCredentialParameters:, @"credentialParameters")
AS_PROPERTY(excludedCredentials, setExcludedCredentials:, @"excludedCredentials")
AS_PROPERTY(allowedCredentials, setAllowedCredentials:, @"allowedCredentials")
AS_PROPERTY(userID, setUserID:, @"userID")
AS_PROPERTY(name, setName:, @"name")
AS_PROPERTY(displayName, setDisplayName:, @"displayName")
AS_PROPERTY(largeBlob, setLargeBlob:, @"largeBlob")
AS_PROPERTY(prf, setPrf:, @"prf")
AS_PROPERTY(appID, setAppID:, @"appID")
AS_FLAG(shouldShowHybridTransport, setShouldShowHybridTransport:, @"shouldShowHybridTransport")

@end

@interface ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest : _ASPublicKeyCredentialRequest
@end
@implementation ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest
@end

@interface ASAuthorizationPlatformPublicKeyCredentialAssertionRequest : _ASPublicKeyCredentialRequest
@end
@implementation ASAuthorizationPlatformPublicKeyCredentialAssertionRequest
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest : _ASPublicKeyCredentialRequest
@end
@implementation ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest : _ASPublicKeyCredentialRequest
@end
@implementation ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest
@end

static id makeRequest(Class cls, id provider, NSString *relyingParty, NSData *challenge) {
    _ASPublicKeyCredentialRequest *request = [[[cls alloc] init] autorelease];

    [request setProvider:provider];
    [request setRelyingPartyIdentifier:relyingParty];
    [request setChallenge:challenge];
    [request setUserVerificationPreference:ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred];
    return request;
}

@interface ASAuthorizationPlatformPublicKeyCredentialProvider : NSObject {
    NSString *_relyingPartyIdentifier;
}
@end

@implementation ASAuthorizationPlatformPublicKeyCredentialProvider

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier {
    if ((self = [super init]))
        _relyingPartyIdentifier = [relyingPartyIdentifier copy];
    return self;
}

- (void)dealloc {
    [_relyingPartyIdentifier release];
    [super dealloc];
}

- (NSString *)relyingPartyIdentifier { return _relyingPartyIdentifier; }

- (id)createCredentialRegistrationRequestWithChallenge:(NSData *)challenge name:(NSString *)name userID:(NSData *)userID {
    id request = makeRequest([ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest class], self, _relyingPartyIdentifier, challenge);
    [request setName:name];
    [request setUserID:userID];
    return request;
}

- (id)createCredentialAssertionRequestWithChallenge:(NSData *)challenge {
    return makeRequest([ASAuthorizationPlatformPublicKeyCredentialAssertionRequest class], self, _relyingPartyIdentifier, challenge);
}

@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialProvider : ASAuthorizationPlatformPublicKeyCredentialProvider
@end

@implementation ASAuthorizationSecurityKeyPublicKeyCredentialProvider

- (id)createCredentialRegistrationRequestWithChallenge:(NSData *)challenge displayName:(NSString *)displayName name:(NSString *)name
                                                userID:(NSData *)userID {
    id request = makeRequest([ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest class], self,
                             [self relyingPartyIdentifier], challenge);
    [request setDisplayName:displayName];
    [request setName:name];
    [request setUserID:userID];
    [request setAttestationPreference:ASAuthorizationPublicKeyCredentialAttestationKindNone];
    return request;
}

- (id)createCredentialAssertionRequestWithChallenge:(NSData *)challenge {
    return makeRequest([ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest class], self, [self relyingPartyIdentifier], challenge);
}

@end

// credentials only ever come back from an authenticator, so none are made here
@interface _ASPublicKeyCredential : NSObject
@end

@implementation _ASPublicKeyCredential
- (NSData *)rawClientDataJSON { return nil; }
- (NSData *)credentialID { return nil; }
- (NSData *)rawAttestationObject { return nil; }
- (NSData *)rawAuthenticatorData { return nil; }
- (NSData *)signature { return nil; }
- (NSData *)userID { return nil; }
- (id)largeBlob { return nil; }
- (id)prf { return nil; }
- (NSInteger)attachment { return 0; }
- (NSArray *)transports { return @[]; }
@end

@interface ASAuthorizationPlatformPublicKeyCredentialRegistration : _ASPublicKeyCredential
@end
@implementation ASAuthorizationPlatformPublicKeyCredentialRegistration
@end

@interface ASAuthorizationPlatformPublicKeyCredentialAssertion : _ASPublicKeyCredential
@end
@implementation ASAuthorizationPlatformPublicKeyCredentialAssertion
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialRegistration : _ASPublicKeyCredential
@end
@implementation ASAuthorizationSecurityKeyPublicKeyCredentialRegistration
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialAssertion : _ASPublicKeyCredential
@end
@implementation ASAuthorizationSecurityKeyPublicKeyCredentialAssertion
@end

typedef NS_ENUM(NSInteger, ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState) {
    ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateAuthorized,
    ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateDenied,
    ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateNotDetermined,
};

// a browser's view of the passkeys on the system: there are none, and no prompt can grant access
@interface ASAuthorizationWebBrowserPublicKeyCredentialManager : NSObject
@end

@implementation ASAuthorizationWebBrowserPublicKeyCredentialManager

- (ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState)authorizationStateForPlatformCredentials {
    return ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateDenied;
}

- (void)requestAuthorizationForPublicKeyCredentials:(void (^)(ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState))handler {
    if (handler == nil)
        return;
    handler = [[handler copy] autorelease];
    dispatch_async(dispatch_get_main_queue(), ^{
        handler(ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateDenied);
    });
}

- (void)platformCredentialsForRelyingParty:(NSString *)relyingParty completionHandler:(void (^)(NSArray *))handler {
    if (handler == nil)
        return;
    handler = [[handler copy] autorelease];
    dispatch_async(dispatch_get_main_queue(), ^{
        handler(@[]);
    });
}

@end
