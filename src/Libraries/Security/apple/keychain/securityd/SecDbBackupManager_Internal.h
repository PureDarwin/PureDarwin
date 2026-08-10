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

// DO NOT INCLUDE ME (unless you're SecDbBackupManager.m or a unit test)
// These are for internal use and testing only

#ifndef SecDbBackupManager_Internal_h
#define SecDbBackupManager_Internal_h

// Need these things in tests, too
#import "SecDbBackupManager.h"

#if SECDB_BACKUPS_ENABLED

#import "SecDbBackupBag.h"
#import "SecDbBackupBagIdentity.h"
#import "SecDbBackupKeyClassSigningKey.h"
#import "SecDbBackupMetadataClassKey.h"
#import "SecDbBackupRecoverySet.h"

#include <utilities/SecDb.h>

#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFCryptoServicesErrors.h>

@interface SecDbBackupManager (Internal)
@property (nonatomic) SecDbBackupBagIdentity* bagIdentity;

#define BACKUPBAG_PASSPHRASE_LENGTH 32
#define UUIDBYTESLENGTH 16

+ (void)resetManager;
- (NSData*)createBackupBagSecret:(NSError**)error;
- (keybag_handle_t)createBackupBagWithSecret:(NSData*)secret error:(NSError**)error;
- (BOOL)saveBackupBag:(keybag_handle_t)handle asDefault:(BOOL)asDefault error:(NSError**)error;
- (keybag_handle_t)loadBackupBag:(NSUUID*)uuid error:(NSError**)error;
- (BOOL)createOrLoadBackupInfrastructure:(NSError**)error;
- (SecDbBackupKeyClassSigningKey*)createKCSKForKeyClass:(keyclass_t)keyclass withWrapper:(SFAESKey*)wrapper error:(NSError**)error;
- (SecDbBackupRecoverySet*)createRecoverySetWithBagSecret:(NSData*)secret forType:(SecDbBackupRecoveryType)type error:(NSError**)error;
- (SFECKeyPair*)fetchKCSKForKeyclass:(keyclass_t)keyclass error:(NSError**)error;

// Pure utilities
- (NSData*)getSHA256OfData:(NSData*)data;
- (SFECKeyPair*)getECKeyPairFromDERBytes:(void*)bytes length:(size_t)len error:(NSError**)error;

@end

#endif  // SECDB_BACKUPS_ENABLED

#endif  /* SecDbBackupManager_Internal_h */
