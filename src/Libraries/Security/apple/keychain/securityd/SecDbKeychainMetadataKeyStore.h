/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import "SecKeybagSupport.h"
#import <SecurityFoundation/SFKey.h>

NS_ASSUME_NONNULL_BEGIN

// This class is intended for SecDbKeychainItemV7, db resets and SecDbKeychainManager _only_

@interface SecDbKeychainMetadataKeyStore : NSObject

+ (bool)cachingEnabled;

+ (void)resetSharedStore;
+ (instancetype)sharedStore;

- (instancetype)init NS_UNAVAILABLE;

- (void)dropClassAKeys;

- (SFAESKey* _Nullable)keyForKeyclass:(keyclass_t)keyClass
                               keybag:(keybag_handle_t)keybag
                         keySpecifier:(SFAESKeySpecifier*)keySpecifier
                          allowWrites:(BOOL)allowWrites		// (re)create keys if missing, corrupt or outdated format
                                error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
