/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 @header SecItemDb.h - A Database full of SecDbItems.
 */

#ifndef _SECURITYD_SECITEMDB_H_
#define _SECURITYD_SECITEMDB_H_

#include "keychain/securityd/SecDbQuery.h"

struct SecurityClient;

__BEGIN_DECLS

bool SecItemDbCreateSchema(SecDbConnectionRef dbt, const SecDbSchema *schema, CFArrayRef classIndexesForNewTables, bool includeVersion, CFErrorRef *error);

bool SecItemDbDeleteSchema(SecDbConnectionRef dbt, const SecDbSchema *schema, CFErrorRef *error);

CFTypeRef SecDbItemCopyResult(SecDbItemRef item, ReturnTypeMask return_type, CFErrorRef *error);

bool SecDbItemSelect(SecDbQueryRef query, SecDbConnectionRef dbconn, CFErrorRef *error,
                     bool (^return_attr)(const SecDbAttr *attr),
                     bool (^use_attr_in_where)(const SecDbAttr *attr),
                     bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere),
                     bool (^bind_added_where)(sqlite3_stmt *stmt, int col),
                     void (^handle_row)(SecDbItemRef item, bool *stop));

CFStringRef SecDbItemCopySelectSQL(SecDbQueryRef query,
                                   bool (^return_attr)(const SecDbAttr *attr),
                                   bool (^use_attr_in_where)(const SecDbAttr *attr),
                                   bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere));
bool SecDbItemSelectBind(SecDbQueryRef query, sqlite3_stmt *stmt, CFErrorRef *error,
                         bool (^use_attr_in_where)(const SecDbAttr *attr),
                         bool (^bind_added_where)(sqlite3_stmt *stmt, int col));

bool SecDbItemQuery(SecDbQueryRef query, CFArrayRef accessGroups, SecDbConnectionRef dbconn, CFErrorRef *error,
                    void (^handle_row)(SecDbItemRef item, bool *stop));

void query_pre_add(Query *q, bool force_date);

bool SecItemIsSystemBound(CFDictionaryRef item, const SecDbClass *cls, bool multiUser);

//
// MARK: backup restore stuff
//

/* Forward declaration of import export SPIs. */
enum SecItemFilter {
    kSecNoItemFilter,
    kSecSysBoundItemFilter,
    kSecBackupableItemFilter,
};

CFDictionaryRef SecServerCopyKeychainPlist(SecDbConnectionRef dbt,
                                           struct SecurityClient *client,
                                           keybag_handle_t src_keybag,
                                           keybag_handle_t dest_keybag,
                                           enum SecItemFilter filter,
                                           CFErrorRef *error);
bool SecServerImportKeychainInPlist(SecDbConnectionRef dbt,
                                    struct SecurityClient *client,
                                    keybag_handle_t src_keybag,
                                    keybag_handle_t dest_keybag,
                                    CFDictionaryRef keychain,
                                    enum SecItemFilter filter,
                                    bool removeKeychainContent,
                                    CFErrorRef *error);

CFStringRef
SecServerBackupGetKeybagUUID(CFDictionaryRef keychain, CFErrorRef *error);


#if TARGET_OS_IPHONE
bool SecServerDeleteAllForUser(SecDbConnectionRef dbt, CFDataRef musrView, bool keepU, CFErrorRef *error);
#endif
OSStatus SecServerDeleteForAppClipApplicationIdentifier(CFStringRef identifier);

bool kc_transaction(SecDbConnectionRef dbt, CFErrorRef *error, bool(^perform)(void));
bool kc_transaction_type(SecDbConnectionRef dbt, SecDbTransactionType type, CFErrorRef *error, bool(^perform)(void));
bool s3dl_copy_matching(SecDbConnectionRef dbt, Query *q, CFTypeRef *result,
                        CFArrayRef accessGroups, CFErrorRef *error);
bool s3dl_query_add(SecDbConnectionRef dbt, Query *q, CFTypeRef *result, CFErrorRef *error);
bool s3dl_query_update(SecDbConnectionRef dbt, Query *q,
                  CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups, CFErrorRef *error);
bool s3dl_query_delete(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFErrorRef *error);
bool s3dl_copy_digest(SecDbConnectionRef dbt, Query *q, CFArrayRef *result, CFArrayRef accessGroups, CFErrorRef *error);

const SecDbAttr *SecDbAttrWithKey(const SecDbClass *c, CFTypeRef key, CFErrorRef *error);

bool s3dl_dbt_keys_current(SecDbConnectionRef dbt, uint32_t current_generation, CFErrorRef *error);
bool s3dl_dbt_update_keys(SecDbConnectionRef dbt, struct SecurityClient *client, CFErrorRef *error);

// We'd love to take a query here, but switching layers at the callsite means we don't have it
bool s3dl_item_make_new_uuid(SecDbItemRef item, bool uuid_from_primary_key, CFErrorRef* error);
        
__END_DECLS

#endif /* _SECURITYD_SECITEMDB_H_ */
