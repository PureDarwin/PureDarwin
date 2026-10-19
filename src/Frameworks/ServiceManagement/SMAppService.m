// SMAppService.m - login items, agents and daemons an app registers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// nothing adds an app's services to launchd, so registering fails and every service reports not registered
#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, SMAppServiceStatus) {
    SMAppServiceStatusNotRegistered,
    SMAppServiceStatusEnabled,
    SMAppServiceStatusRequiresApproval,
    SMAppServiceStatusNotFound,
};

// ServiceManagement.c's domain and codes
extern const CFStringRef kSMErrorDomainFramework;
enum { kSMErrorServiceUnavailable = 7 };

@interface SMAppService : NSObject {
    NSString *_identifier;
    NSString *_plistName;
}
@end

@implementation SMAppService

+ (SMAppService *)_serviceWithIdentifier:(NSString *)identifier plistName:(NSString *)plistName {
    SMAppService *service = [[[self alloc] init] autorelease];

    service->_identifier = [identifier copy];
    service->_plistName = [plistName copy];
    return service;
}

+ (SMAppService *)mainAppService {
    return [self _serviceWithIdentifier:[[NSBundle mainBundle] bundleIdentifier] plistName:nil];
}

+ (SMAppService *)loginItemServiceWithIdentifier:(NSString *)identifier {
    return [self _serviceWithIdentifier:identifier plistName:nil];
}

+ (SMAppService *)agentServiceWithPlistName:(NSString *)plistName {
    return [self _serviceWithIdentifier:nil plistName:plistName];
}

+ (SMAppService *)daemonServiceWithPlistName:(NSString *)plistName {
    return [self _serviceWithIdentifier:nil plistName:plistName];
}

+ (SMAppServiceStatus)statusForLegacyURL:(NSURL *)url {
    return SMAppServiceStatusNotRegistered;
}

+ (void)openSystemSettingsLoginItems {
}

- (void)dealloc {
    [_identifier release];
    [_plistName release];
    [super dealloc];
}

- (SMAppServiceStatus)status {
    return SMAppServiceStatusNotRegistered;
}

- (NSError *)_unavailable {
    return [NSError errorWithDomain:(NSString *)kSMErrorDomainFramework code:kSMErrorServiceUnavailable
                           userInfo:@{ NSLocalizedDescriptionKey : @"Login items and app services are not available on this system." }];
}

- (BOOL)registerAndReturnError:(NSError **)error {
    if (error != NULL)
        *error = [self _unavailable];
    return NO;
}

- (BOOL)unregisterAndReturnError:(NSError **)error {
    if (error != NULL)
        *error = [self _unavailable];
    return NO;
}

- (void)unregisterWithCompletionHandler:(void (^)(NSError *error))handler {
    if (handler != nil)
        handler([self _unavailable]);
}

@end
