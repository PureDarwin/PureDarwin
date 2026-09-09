/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSHost_h
#define NSHost_h

#import <Foundation/NSObject.h>

@class NSString, NSArray;

@interface NSHost : NSObject {
    NSArray *_names;
    NSArray *_addresses;
}

+ (NSHost *)currentHost;
+ (NSHost *)hostWithName:(NSString *)name;
+ (NSHost *)hostWithAddress:(NSString *)address;

- (NSString *)name;
- (NSArray *)names;
- (NSString *)address;
- (NSArray *)addresses;

- (BOOL)isEqualToHost:(NSHost *)host;

@end

#endif /* NSHost_h */
