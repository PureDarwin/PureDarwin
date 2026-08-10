/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "SecKeybagSupport.h"
#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBCodable.h>
#import "CheckV12DevEnabled.h"

NS_ASSUME_NONNULL_BEGIN

@interface SecDbKeychainItemV7 : NSObject

@property (nonatomic, readonly) keyclass_t keyclass;
@property (nonatomic, readonly) NSData* backupUUID;

- (nullable instancetype)initWithData:(NSData*)data decryptionKeybag:(keybag_handle_t)decryptionKeybag error:(NSError**)error;
- (instancetype)initWithSecretAttributes:(NSDictionary*)secretAttributes metadataAttributes:(NSDictionary*)metadataAttributes tamperCheck:(NSString*)tamperCheck keyclass:(keyclass_t)keyclass;

- (nullable NSDictionary*)metadataAttributesWithError:(NSError**)error;
- (nullable NSDictionary*)secretAttributesWithAcmContext:(NSData*)acmContext accessControl:(SecAccessControlRef)accessControl callerAccessGroups:(NSArray*)callerAccessGroups error:(NSError**)error;
- (BOOL)deleteWithAcmContext:(NSData*)acmContext accessControl:(SecAccessControlRef)accessControl callerAccessGroups:(NSArray*)callerAccessGroups error:(NSError**)error;

- (nullable NSData*)encryptedBlobWithKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(nullable NSData*)acmContext error:(NSError**)error;

@end

extern NSString* const SecDbKeychainErrorDomain;
extern const NSInteger SecDbKeychainErrorDeserializationFailed;


@class SecDbKeychainSerializedMetadata;
@class SecDbKeychainSerializedSecretData;

@interface SecDbKeychainItemV7 (UnitTesting)

+ (bool)isKeychainUnlocked;

@property (readonly) NSData* encryptedMetadataBlob;
@property (readonly) NSData* encryptedSecretDataBlob;

- (BOOL)encryptMetadataWithKeybag:(keybag_handle_t)keybag error:(NSError**)error;
- (BOOL)encryptSecretDataWithKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(nullable NSData*)acmContext error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
