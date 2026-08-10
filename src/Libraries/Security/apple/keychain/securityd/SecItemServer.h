/*
 * Copyright (c) 2007-2009,2012-2014 Apple Inc. All Rights Reserved.
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

/*!
    @header SecItemServer
    The functions provided in SecItemServer.h provide an interface to
    the backend for SecItem APIs in the server.
*/

#ifndef _SECURITYD_SECITEMSERVER_H_
#define _SECURITYD_SECITEMSERVER_H_

#include <CoreFoundation/CoreFoundation.h>
#include "keychain/SecureObjectSync/SOSCircle.h"
#include "keychain/securityd/SecDbQuery.h"
#include "utilities/SecDb.h"
#include <TargetConditionals.h>
#include "sec/ipc/securityd_client.h"


__BEGIN_DECLS

bool _SecItemAdd(CFDictionaryRef attributes, SecurityClient *client, CFTypeRef *result, CFErrorRef *error);
bool _SecItemCopyMatching(CFDictionaryRef query, SecurityClient *client, CFTypeRef *result, CFErrorRef *error);
bool _SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate, SecurityClient *client, CFErrorRef *error);
bool _SecItemDelete(CFDictionaryRef query, SecurityClient *client, CFErrorRef *error);
bool _SecItemDeleteAll(CFErrorRef *error);
bool _SecItemServerDeleteAllWithAccessGroups(CFArrayRef accessGroups, SecurityClient *client, CFErrorRef *error);

bool _SecServerRestoreKeychain(CFErrorRef *error);
bool _SecServerMigrateKeychain(int32_t handle_in, CFDataRef data_in, int32_t *handle_out, CFDataRef *data_out, CFErrorRef *error);
CFDataRef _SecServerKeychainCreateBackup(SecurityClient *client, CFDataRef keybag, CFDataRef passcode, bool emcs, CFErrorRef *error);
bool _SecServerKeychainRestore(CFDataRef backup, SecurityClient *client, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error);
CFStringRef _SecServerBackupCopyUUID(CFDataRef backup, CFErrorRef *error);

bool _SecServerBackupKeybagAdd(SecurityClient *client, CFDataRef passcode, CFDataRef *identifier, CFDataRef *pathinfo, CFErrorRef *error);
bool _SecServerBackupKeybagDelete(CFDictionaryRef attributes, bool deleteAll, CFErrorRef *error);

bool _SecItemUpdateTokenItemsForAccessGroups(CFStringRef tokenID, CFArrayRef accessGroups, CFArrayRef items, SecurityClient *client, CFErrorRef *error);

CF_RETURNS_RETAINED CFArrayRef _SecServerKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error);
CF_RETURNS_RETAINED CFDictionaryRef _SecServerBackupSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error);

int SecServerKeychainTakeOverBackupFD(CFStringRef backupName, CFErrorRef *error);

bool _SecServerRestoreSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error);

#if TARGET_OS_IOS
bool _SecServerTransmogrifyToSystemKeychain(SecurityClient *client, CFErrorRef *error);
bool _SecServerTransmogrifyToSyncBubble(CFArrayRef services, uid_t uid, SecurityClient *client, CFErrorRef *error);
bool _SecServerDeleteMUSERViews(SecurityClient *client, uid_t uid, CFErrorRef *error);
#endif

#if SHAREDWEBCREDENTIALS
bool _SecAddSharedWebCredential(CFDictionaryRef attributes, SecurityClient *client, const audit_token_t *clientAuditToken, CFStringRef appID, CFArrayRef domains, CFTypeRef *result, CFErrorRef *error);
bool _SecCopySharedWebCredential(CFDictionaryRef query, SecurityClient *client, const audit_token_t *clientAuditToken, CFStringRef appID, CFArrayRef domains, CFTypeRef *result, CFErrorRef *error);
#endif /* SHAREDWEBCREDENTIALS */

// Hack to log objects from inside SOS code
void SecItemServerAppendItemDescription(CFMutableStringRef desc, CFDictionaryRef object);

SecDbRef SecKeychainDbCreate(CFStringRef path, CFErrorRef* error);
SecDbRef SecKeychainDbInitialize(SecDbRef db);

bool kc_with_dbt(bool writeAndRead, CFErrorRef *error, bool (^perform)(SecDbConnectionRef dbt));
bool kc_with_dbt_non_item_tables(bool writeAndRead, CFErrorRef* error, bool (^perform)(SecDbConnectionRef dbt)); // can be used when only tables which don't store 'items' are accessed - avoids invoking SecItemDataSourceFactoryGetDefault()
bool kc_with_custom_db(bool writeAndRead, bool usesItemTables, SecDbRef db, CFErrorRef *error, bool (^perform)(SecDbConnectionRef dbt));



/* For whitebox testing only */
void SecKeychainDbForceClose(void);
void SecKeychainDbReset(dispatch_block_t inbetween);

SOSDataSourceFactoryRef SecItemDataSourceFactoryGetDefault(void);

/* FIXME: there is a specific type for keybag handle (keybag_handle_t)
   but it's not defined for simulator so we just use an int32_t */
void SecItemServerSetKeychainKeybag(int32_t keybag);
void SecItemServerResetKeychainKeybag(void);

void SecItemServerSetKeychainChangedNotification(const char *notification_name);

CFStringRef __SecKeychainCopyPath(void);

bool _SecServerRollKeys(bool force, SecurityClient *client, CFErrorRef *error);
bool _SecServerRollKeysGlue(bool force, CFErrorRef *error);


/* initial sync */
#define SecServerInitialSyncCredentialFlagTLK                (1 << 0)
#define SecServerInitialSyncCredentialFlagPCS                (1 << 1)
#define SecServerInitialSyncCredentialFlagPCSNonCurrent      (1 << 2)
#define SecServerInitialSyncCredentialFlagBluetoothMigration (1 << 3)

CFArrayRef _SecServerCopyInitialSyncCredentials(uint32_t flags, CFErrorRef *error);
bool _SecServerImportInitialSyncCredentials(CFArrayRef array, CFErrorRef *error);

CF_RETURNS_RETAINED CFArrayRef _SecItemCopyParentCertificates(CFDataRef normalizedIssuer, CFArrayRef accessGroups, CFErrorRef *error);
bool _SecItemCertificateExists(CFDataRef normalizedIssuer, CFDataRef serialNumber, CFArrayRef accessGroups, CFErrorRef *error);

bool SecKeychainDbGetVersion(SecDbConnectionRef dbt, int *version, CFErrorRef *error);


// Should all be blocks called from SecItemDb
bool match_item(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFDictionaryRef item);
bool accessGroupsAllows(CFArrayRef accessGroups, CFStringRef accessGroup, SecurityClient* client);
bool itemInAccessGroup(CFDictionaryRef item, CFArrayRef accessGroups);
void SecKeychainChanged(void);

void deleteCorruptedItemAsync(SecDbConnectionRef dbt, CFStringRef tablename, sqlite_int64 rowid);

__END_DECLS

#endif /* _SECURITYD_SECITEMSERVER_H_ */
