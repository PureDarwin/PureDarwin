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

// For now at least, we'll support backups only on iOS and macOS
#if (TARGET_OS_OSX || TARGET_OS_IOS || TARGET_OS_MACCATALYST) && !TARGET_OS_SIMULATOR && !(defined(TARGET_DARWINOS) && TARGET_DARWINOS)
#define SECDB_BACKUPS_ENABLED 1
#else
#define SECDB_BACKUPS_ENABLED 0
#endif

#if __OBJC2__
#import <Foundation/Foundation.h>
#import <SecurityFoundation/SFKey.h>
#import "SecAKSObjCWrappers.h"
#import "CheckV12DevEnabled.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SecDbBackupRecoveryType) {
    SecDbBackupRecoveryTypeInvalid = -1,
    SecDbBackupRecoveryTypeAKS = 1,
    SecDbBackupRecoveryTypeCylon = 2,
    SecDbBackupRecoveryTypeRecoveryKey = 3,
};

extern NSString* const KeychainBackupsErrorDomain;

typedef NS_ENUM(NSInteger, SecDbBackupErrorCode) {
    SecDbBackupUnknownError = -1,
    SecDbBackupSuccess = 0,
    SecDbBackupAKSFailure,
    SecDbBackupCryptoFailure,
    SecDbBackupWriteFailure,
    SecDbBackupDeserializationFailure,
    SecDbBackupSetupFailure,
    SecDbBackupNoBackupBagFound,
    SecDbBackupNoKCSKFound,
    SecDbBackupDuplicateBagFound,
    SecDbBackupMultipleDefaultBagsFound,
    SecDbBackupMalformedBagDataOnDisk,
    SecDbBackupMalformedKCSKDataOnDisk,
    SecDbBackupMalformedUUIDDataOnDisk,
    SecDbBackupUUIDMismatch,
    SecDbBackupDataMismatch,
    SecDbBackupUnknownOption,
    SecDbBackupKeychainLocked,
    SecDbBackupInvalidArgument,
    SecDbBackupNotSupported,
    SecDbBackupInternalError,

    SecDbBackupTestCodeFailure = 255,     // support code for testing is falling over somehow
};

@interface SecDbBackupWrappedKey : NSObject <NSSecureCoding>
@property (nonatomic) NSData* wrappedKey;
@property (nonatomic) NSData* baguuid;
@end

@interface SecDbBackupManager : NSObject

// Nullable to make analyzer not complain in the case where the stub returns nil
+ (instancetype _Nullable)manager;
- (instancetype)init NS_UNAVAILABLE;

- (NSData* _Nullable)currentBackupBagUUID;
- (SecDbBackupWrappedKey* _Nullable)wrapItemKey:(SFAESKey*)key forKeyclass:(keyclass_t)keyclass error:(NSError**)error;
- (SecDbBackupWrappedKey* _Nullable)wrapMetadataKey:(SFAESKey*)key forKeyclass:(keyclass_t)keyclass error:(NSError**)error;
- (void)verifyBackupIntegrity:(bool)lightweight
                   completion:(void (^)(NSDictionary<NSString*, NSString*>* results, NSError* _Nullable error))completion;

@end

NS_ASSUME_NONNULL_END
#endif      // __OBJC2__

// Declare C functions here

bool SecDbBackupCreateOrLoadBackupInfrastructure(CFErrorRef _Nullable * _Nonnull error);
void SecDbResetBackupManager(void);     // For testing. Here so SecKeychainDbReset can use it.
