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

@class NSString;

/* NSErrorDomain and NSErrorUserInfoKey are typed NSString aliases; code that
 * declares an error domain constant spells it with the alias. */
typedef NSString *NSErrorDomain;
typedef NSString *NSErrorUserInfoKey;

@interface NSError : NSObject

+ (instancetype)errorWithDomain:(NSString *)domain code:(NSInteger)code;

- (NSInteger)code;
- (NSString *)domain;
- (NSString *)localizedDescription;

@end

#endif /* NSError_h */
