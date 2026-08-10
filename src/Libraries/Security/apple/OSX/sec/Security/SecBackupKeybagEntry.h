/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#import "CKKSSQLDatabaseObject.h"
#include <utilities/SecDb.h>
#include "keychain/securityd/SecDbItem.h"

#ifndef SecBackupKeybagEntry_h
#define SecBackupKeybagEntry_h

#if OCTAGON

@interface SecBackupKeybagEntry : CKKSSQLDatabaseObject {

}

//@property (getter=getChangeToken,setter=setChangeToken:) CKServerChangeToken* changeToken;
@property NSData* publickeyHash;
@property NSData* publickey;
@property NSData* musr;         // musr

+ (instancetype) state: (NSString*) ckzone;

+ (instancetype) fromDatabase: (NSData*) publickeyHash error: (NSError * __autoreleasing *) error;
+ (instancetype) tryFromDatabase: (NSData*) publickeyHash error: (NSError * __autoreleasing *) error;

- (instancetype) initWithPublicKey: (NSData*)publicKey publickeyHash: (NSData*) publickeyHash user: (NSData*) user;

- (BOOL)isEqual: (id) object;
@end

#endif
#endif /* SecBackupKeybagEntry_h */
