/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSProcessInfo_h
#define NSProcessInfo_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>

@interface NSProcessInfo : NSObject

+ (NSProcessInfo *)processInfo;

- (NSArray<NSString *> *)arguments;
- (NSDictionary<NSString *, NSString *> *)environment;
- (NSString *)processName;
- (int)processIdentifier;
- (NSString *)hostName;
- (NSUInteger)processorCount;
- (unsigned long long)physicalMemory;

@end

#endif /* NSProcessInfo_h */
