// LocalAuthentication.m - LAContext with no biometric sensor and no system authentication dialog
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// policies cannot be evaluated, callers get LocalAuthentication's own errors

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

NSString *const LAErrorDomain = @"com.apple.LocalAuthentication";

enum {
    LAErrorAuthenticationFailed = -1,
    LAErrorPasscodeNotSet = -5,
    LAErrorBiometryNotAvailable = -6,
    LAErrorNotInteractive = -1004,
};

enum {
    LAPolicyDeviceOwnerAuthenticationWithBiometrics = 1,
    LAPolicyDeviceOwnerAuthentication = 2,
};

static NSError *laError(NSInteger policy)
{
    NSInteger code = policy == LAPolicyDeviceOwnerAuthenticationWithBiometrics ? LAErrorBiometryNotAvailable : LAErrorPasscodeNotSet;
    NSString *reason = code == LAErrorBiometryNotAvailable ? @"Biometry is not available on this system."
                                                           : @"No authentication method is available on this system.";
    return [NSError errorWithDomain:LAErrorDomain code:code userInfo:@{ NSLocalizedDescriptionKey : reason }];
}

@interface LAContext : NSObject {
    NSString *_reason, *_cancelTitle, *_fallbackTitle;
    BOOL _interactionNotAllowed;
}
@end

@implementation LAContext

- (void)dealloc
{
    [_reason release];
    [_cancelTitle release];
    [_fallbackTitle release];
    [super dealloc];
}

- (BOOL)canEvaluatePolicy:(NSInteger)policy error:(NSError **)outError
{
    if (outError)
        *outError = laError(policy);
    return NO;
}

- (void)evaluatePolicy:(NSInteger)policy localizedReason:(NSString *)localizedReason reply:(void (^)(BOOL success, NSError *error))reply
{
    if (reply == nil)
        [NSException raise:NSInvalidArgumentException format:@"-[LAContext evaluatePolicy:localizedReason:reply:] nil reply"];
    void (^copy)(BOOL, NSError *) = [reply copy];
    NSError *error = [laError(policy) retain];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        copy(NO, error);
        [error release];
        [copy release];
    });
}

- (void)invalidate { }
- (NSInteger)biometryType { return 0; }
- (NSData *)evaluatedPolicyDomainState { return nil; }
- (NSString *)localizedReason { return _reason; }
- (void)setLocalizedReason:(NSString *)reason
{
    [_reason autorelease];
    _reason = [reason copy];
}
- (NSString *)localizedCancelTitle { return _cancelTitle; }
- (void)setLocalizedCancelTitle:(NSString *)title
{
    [_cancelTitle autorelease];
    _cancelTitle = [title copy];
}
- (NSString *)localizedFallbackTitle { return _fallbackTitle; }
- (void)setLocalizedFallbackTitle:(NSString *)title
{
    [_fallbackTitle autorelease];
    _fallbackTitle = [title copy];
}
- (BOOL)interactionNotAllowed { return _interactionNotAllowed; }
- (void)setInteractionNotAllowed:(BOOL)notAllowed { _interactionNotAllowed = notAllowed; }

@end
