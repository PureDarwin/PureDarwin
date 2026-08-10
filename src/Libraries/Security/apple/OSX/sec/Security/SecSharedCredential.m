/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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
 *
 * SecSharedCredential.m - Retrieve shared credentials with AuthenticationServices.
 *
 */

#include <Security/SecSharedCredential.h>
#include <Security/SecBasePriv.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include "SecItemInternal.h"
#include <dlfcn.h>

#import <Foundation/Foundation.h>
#import <AuthenticationServices/AuthenticationServices.h>

// Forward declaration of the primary function implemented in this file
OSStatus SecCopySharedWebCredentialSyncUsingAuthSvcs(CFStringRef fqdn, CFStringRef account, CFArrayRef *credentials, CFErrorRef *error);

// Classes we will load dynamically
static Class kASAuthorizationClass = NULL;
static Class kASAuthorizationControllerClass = NULL;
static Class kASAuthorizationPasswordProviderClass = NULL;
static Class kASPasswordCredentialClass = NULL;
static Class kUIApplicationClass = NULL;
static Class kNSApplicationClass = NULL;

static void loadAuthenticationServices(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const char *path = "/System/Library/Frameworks/AuthenticationServices.framework/AuthenticationServices";
        if ( [NSProcessInfo processInfo].macCatalystApp == YES ) {
            path = "/System/iOSSupport/System/Library/Frameworks/AuthenticationServices.framework/AuthenticationServices";
        }
        void* lib_handle = dlopen(path, RTLD_LAZY);
        if (lib_handle != NULL) {
            kASAuthorizationClass = NSClassFromString(@"ASAuthorization");
            kASAuthorizationControllerClass = NSClassFromString(@"ASAuthorizationController");
            kASAuthorizationPasswordProviderClass = NSClassFromString(@"ASAuthorizationPasswordProvider");
            kASPasswordCredentialClass = NSClassFromString(@"ASPasswordCredential");
        }
    });
}

static void loadUIKit(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const char *path = "/System/Library/Frameworks/UIKit.framework/UIKit";
        if ( [NSProcessInfo processInfo].macCatalystApp == YES ) {
            path = "/System/Library/iOSSupport/System/Library/Frameworks/UIKit.framework/UIKit";
        }
        void* lib_handle = dlopen(path, RTLD_LAZY);
        if (lib_handle != NULL) {
            kUIApplicationClass = NSClassFromString(@"UIApplication");
        }
    });
}

static void loadAppKit(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const char *path = "/System/Library/Frameworks/AppKit.framework/AppKit";
        void* lib_handle = dlopen(path, RTLD_LAZY);
        if (lib_handle != NULL) {
            kNSApplicationClass = NSClassFromString(@"NSApplication");
        }
    });
}

static Class ASAuthorizationClass() {
    loadAuthenticationServices();
    return kASAuthorizationClass;
}

static Class ASAuthorizationControllerClass() {
    loadAuthenticationServices();
    return kASAuthorizationControllerClass;
}

static Class ASAuthorizationPasswordProviderClass() {
    loadAuthenticationServices();
    return kASAuthorizationPasswordProviderClass;
}

static Class ASPasswordCredentialClass() {
    loadAuthenticationServices();
    return kASPasswordCredentialClass;
}

static Class UIApplicationClass() {
    loadUIKit();
    return kUIApplicationClass;
}

static Class NSApplicationClass() {
    loadAppKit();
    return kNSApplicationClass;
}

@interface SharedCredentialController : NSObject
    <ASAuthorizationControllerDelegate,
     ASAuthorizationControllerPresentationContextProviding>

-(ASPasswordCredential *)passwordCredential;

@end

@implementation SharedCredentialController {
    ASAuthorizationPasswordProvider *_provider;
    ASAuthorizationController *_controller;
    ASPasswordCredential *_passwordCredential;
    dispatch_semaphore_t _semaphore;
    NSError *_error;
    OSStatus _result;
}

- (void)dealloc {
    // Don't want any further callbacks since we are going away
    _controller.delegate = nil;
    _controller.presentationContextProvider = nil;
}

- (void)_requestCredential {
    if (!_provider) {
        _provider = [[ASAuthorizationPasswordProviderClass() alloc] init];
    }
    if (!_controller) {
        _controller = [[ASAuthorizationControllerClass() alloc] initWithAuthorizationRequests:@[ [_provider createRequest] ]];
    }
    _controller.delegate = self;
    _controller.presentationContextProvider = self;
    _semaphore = dispatch_semaphore_create(0);
    _result = errSecItemNotFound;
    _error = nil;

    [_controller performRequests];
}

- (ASPasswordCredential *)passwordCredential {
    if (_passwordCredential) {
        return _passwordCredential;
    }
    BOOL shouldRequest = YES; // ( [NSProcessInfo processInfo].macCatalystApp == YES );
    if (shouldRequest) {
        [self _requestCredential];
        // wait synchronously until user picks a credential or cancels
        dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    } else {
        // unable to return a shared credential: <rdar://problem/59958701>
        _result = errSecItemNotFound;
        _error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:_result userInfo:NULL];
    }
    return _passwordCredential;
}

- (NSError *)error {
    return _error;
}

- (OSStatus)result {
    return _result;
}

- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithAuthorization:(ASAuthorization *)authorization {
    secinfo("swcagent", "SWC received didCompleteWithAuthorization");
    ASPasswordCredential *passwordCredential = authorization.credential;
    if (![passwordCredential isKindOfClass:[ASPasswordCredentialClass() class]]) {
        _passwordCredential = nil;
        _result = errSecItemNotFound;
    } else {
        _passwordCredential = passwordCredential;
        _result = errSecSuccess;
    }
    dispatch_semaphore_signal(_semaphore);
}

- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithError:(NSError *)error {
    secinfo("swcagent", "SWC received didCompleteWithError");
    _passwordCredential = nil;
    _error = error;
    _result = errSecItemNotFound;
    dispatch_semaphore_signal(_semaphore);
}

- (ASPresentationAnchor)presentationAnchorForAuthorizationController:(ASAuthorizationController *)controller
{
    ASPresentationAnchor anchorWindow = nil;
#if TARGET_OS_OSX
    if ( [NSProcessInfo processInfo].macCatalystApp == NO ) {
        anchorWindow = [[NSApplicationClass() sharedApplication] keyWindow];
    }
#endif
    if (!anchorWindow) {
        anchorWindow = [[UIApplicationClass() sharedApplication] keyWindow];
    }
    return anchorWindow;
}

@end

OSStatus SecCopySharedWebCredentialSyncUsingAuthSvcs(CFStringRef fqdn, CFStringRef account, CFArrayRef *credentials, CFErrorRef *error) {
    SharedCredentialController *controller = [[SharedCredentialController alloc] init];
    ASPasswordCredential *passwordCredential = [controller passwordCredential];
    OSStatus status = [controller result];
    NSArray *returnedCredentials = @[];
    if (status != errSecSuccess) {
        secinfo("swcagent", "SecCopySharedWebCredentialSyncUsingAuthSvcs received result %d", (int)status);
        if (error) {
            *error = (CFErrorRef)CFBridgingRetain([controller error]);
        }
    } else if (passwordCredential) {
        // Use the .user and .password of the passwordCredential to satisfy the SWC interface.
        NSDictionary *credential = @{
            (id)kSecAttrServer : (__bridge NSString*)fqdn,
            (id)kSecAttrAccount : passwordCredential.user,
            (id)kSecSharedPassword : passwordCredential.password,
        };
        returnedCredentials = @[ credential ];
    } else {
        secinfo("swcagent", "SecCopySharedWebCredentialSyncUsingAuthSvcs found no credential");
        status = errSecItemNotFound;
    }
    if (credentials) {
        *credentials = (CFArrayRef)CFBridgingRetain(returnedCredentials);
    }
    return status;
}
