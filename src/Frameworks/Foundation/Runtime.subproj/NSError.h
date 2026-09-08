/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSError_h
#define NSError_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSDictionary, NSArray;

/* NSErrorDomain and NSErrorUserInfoKey are typed NSString aliases; code that
 * declares an error domain constant spells it with the alias. */
typedef NSString *NSErrorDomain;
typedef NSString *NSErrorUserInfoKey;

FOUNDATION_EXPORT NSErrorDomain const NSCocoaErrorDomain;
FOUNDATION_EXPORT NSErrorDomain const NSPOSIXErrorDomain;
FOUNDATION_EXPORT NSErrorDomain const NSOSStatusErrorDomain;
FOUNDATION_EXPORT NSErrorDomain const NSMachErrorDomain;

FOUNDATION_EXPORT NSErrorUserInfoKey const NSLocalizedDescriptionKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSLocalizedFailureReasonErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSLocalizedRecoverySuggestionErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSLocalizedRecoveryOptionsErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSRecoveryAttempterErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSUnderlyingErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSFilePathErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSURLErrorKey;

@interface NSError : NSObject

+ (instancetype)errorWithDomain:(NSString *)domain code:(NSInteger)code;
+ (instancetype)errorWithDomain:(NSString *)domain
                           code:(NSInteger)code
                       userInfo:(NSDictionary *)userInfo;

- (NSDictionary *)userInfo;
- (NSString *)localizedFailureReason;
- (NSString *)localizedRecoverySuggestion;
- (NSArray *)localizedRecoveryOptions;

- (NSInteger)code;
- (NSString *)domain;
- (NSString *)localizedDescription;

@end

#endif /* NSError_h */
