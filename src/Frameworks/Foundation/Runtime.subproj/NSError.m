/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSError.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>

NSErrorDomain const NSCocoaErrorDomain = @"NSCocoaErrorDomain";
NSErrorDomain const NSPOSIXErrorDomain = @"NSPOSIXErrorDomain";
NSErrorDomain const NSOSStatusErrorDomain = @"NSOSStatusErrorDomain";
NSErrorDomain const NSMachErrorDomain = @"NSMachErrorDomain";

NSErrorUserInfoKey const NSLocalizedDescriptionKey = @"NSLocalizedDescription";
NSErrorUserInfoKey const NSLocalizedFailureReasonErrorKey = @"NSLocalizedFailureReason";
NSErrorUserInfoKey const NSLocalizedRecoverySuggestionErrorKey = @"NSLocalizedRecoverySuggestion";
NSErrorUserInfoKey const NSLocalizedRecoveryOptionsErrorKey = @"NSLocalizedRecoveryOptions";
NSErrorUserInfoKey const NSRecoveryAttempterErrorKey = @"NSRecoveryAttempter";
NSErrorUserInfoKey const NSUnderlyingErrorKey = @"NSUnderlyingError";
NSErrorUserInfoKey const NSFilePathErrorKey = @"NSFilePathErrorKey";
NSErrorUserInfoKey const NSURLErrorKey = @"NSURL";

@implementation NSError {
    NSString *_domain;
    NSInteger _code;
    NSDictionary *_userInfo;
}

+ (instancetype)errorWithDomain:(NSString *)domain code:(NSInteger)code {
    return [self errorWithDomain:domain code:code userInfo:nil];
}

+ (instancetype)errorWithDomain:(NSString *)domain
                           code:(NSInteger)code
                       userInfo:(NSDictionary *)userInfo {
    NSError *err = [[self alloc] init];
    if (err != nil) {
        err->_domain = [domain copy];
        err->_code = code;
        err->_userInfo = [userInfo retain];
    }
    return err;
}

- (NSDictionary *)userInfo {
    return _userInfo;
}

- (NSInteger)code {
    return _code;
}

- (NSString *)domain {
    return _domain;
}

- (NSString *)localizedDescription {
    NSString *provided = [_userInfo objectForKey:NSLocalizedDescriptionKey];

    if (provided != nil) {
        return provided;
    }
    return (NSString *)CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                CFSTR("%@ error %ld"),
                                                (CFStringRef)_domain, (long)_code);
}

- (NSString *)localizedFailureReason {
    return [_userInfo objectForKey:NSLocalizedFailureReasonErrorKey];
}

- (NSString *)localizedRecoverySuggestion {
    return [_userInfo objectForKey:NSLocalizedRecoverySuggestionErrorKey];
}

- (NSArray *)localizedRecoveryOptions {
    return [_userInfo objectForKey:NSLocalizedRecoveryOptionsErrorKey];
}

@end
