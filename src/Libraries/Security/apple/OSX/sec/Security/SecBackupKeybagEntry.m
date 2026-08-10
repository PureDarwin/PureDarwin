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

#include <AssertMacros.h>

#import <Foundation/Foundation.h>

#include <utilities/SecDb.h>
#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecItemSchema.h"

#if OCTAGON

#import "SecBackupKeybagEntry.h"

// from CKKSZoneStateEntry.m

@implementation SecBackupKeybagEntry

- (instancetype) initWithPublicKey: (NSData*)publicKey publickeyHash: (NSData*) publickeyHash user: (NSData*) user {
    if (self = [super init]) {
        _publickey = publicKey;
        _publickeyHash = publickeyHash;
        _musr = user;
    }
    return self;
}

- (BOOL)isEqual: (id) object {
    if(![object isKindOfClass:[SecBackupKeybagEntry class]]) {
        return NO;
    }

    SecBackupKeybagEntry* obj = (SecBackupKeybagEntry*) object;

    return ([self.publickeyHash isEqual: obj.publickeyHash]) ? YES : NO;
}

+ (instancetype) state: (NSData*) publickeyHash {
    NSError* error = nil;
    SecBackupKeybagEntry* ret = [SecBackupKeybagEntry tryFromDatabase:publickeyHash error:&error];

    if (error) {
        secerror("CKKS: error fetching SecBackupKeybagEntry(%@): %@", publickeyHash, error);
    }

    if(!ret) {
        ret = [[SecBackupKeybagEntry alloc] initWithPublicKey: nil publickeyHash: (NSData*) publickeyHash user: nil];
    }
    return ret;
}

#pragma mark - Database Operations

+ (instancetype) fromDatabase: (NSData*) publickeyHash error: (NSError * __autoreleasing *) error {
    return [self fromDatabaseWhere: @{@"publickeyHash": publickeyHash} error: error];
}

+ (instancetype) tryFromDatabase: (NSData*) publickeyHash error: (NSError * __autoreleasing *) error {
    return [self tryFromDatabaseWhere: @{@"publickeyHash": publickeyHash} error: error];
}

#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*) sqlTable {
    return @"backup_keybag";
}

+ (NSArray<NSString*>*) sqlColumns {
    return @[@"publickey", @"publickeyHash", @"musr"];
}

- (NSDictionary<NSString*,id>*) whereClauseToFindSelf {
    return @{@"publickeyHash": self.publickeyHash};
}

// used by saveToDatabaseWithConnection to write to db
- (NSDictionary<NSString*,id>*) sqlValues {
    return @{
        @"publickey":       [self.publickey base64EncodedStringWithOptions:0],
        @"publickeyHash":   [self.publickeyHash base64EncodedStringWithOptions:0],
        @"musr":            [self.musr base64EncodedStringWithOptions:0],
    };
}

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString*, CKKSSQLResult*>*)row {
    NSData *publicKey = row[@"publickey"].asBase64DecodedData;
    NSData *publickeyHash = row[@"publickeyHash"].asBase64DecodedData;
    NSData *musr = row[@"musr"].asBase64DecodedData;
    if (publicKey == NULL || publickeyHash == NULL || musr == NULL) {
        return nil;
    }

    return [[SecBackupKeybagEntry alloc] initWithPublicKey:publicKey publickeyHash:publickeyHash user:musr];
}

@end

#endif
