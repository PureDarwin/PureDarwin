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
 */

#ifndef SecItemRateLimit_tests_h
#define SecItemRateLimit_tests_h

#import "SecItemRateLimit.h"
#import <Foundation/Foundation.h>

// Broken out into header for testing convenience.
// If you need this, why?
@interface SecItemRateLimit : NSObject

@property (nonatomic, readonly) int roCapacity;
@property (nonatomic, readonly) double roRate;
@property (nonatomic, readonly) int rwCapacity;
@property (nonatomic, readonly) double rwRate;
@property (nonatomic, readonly) double limitMultiplier;

@property (nonatomic, readonly) NSDate* roBucket;
@property (nonatomic, readonly) NSDate* rwBucket;

- (bool)shouldCountAPICalls;
- (bool)isEnabled;
- (void)forceEnabled:(bool)force;

+ (instancetype)getStaticRateLimit;
+ (void)resetStaticRateLimit;

@end

#endif /* SecItemRateLimit_tests_h */
