// pdaccounts.h - local accounts stored the macOS way, one dslocal plist per user
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PDACCOUNTS_H
#define PDACCOUNTS_H

#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>

#define PDA_NODE "/var/db/dslocal/nodes/Default"
#define PDA_USERS PDA_NODE "/users"
#define PDA_GROUPS PDA_NODE "/groups"
#define PDA_INDEX PDA_NODE "/users.passwd"
#define PDA_FIRST_UID 501
#define PDA_STAFF_GID 20
#define PDA_ADMIN_GID 80
#define PDA_FIRST_GID 501

// a user record as a mutable dictionary of attribute arrays, NULL when there is none
CFMutableDictionaryRef pda_copy_user(const char *name);

// the first string of an attribute, copied into buf
int pda_attr(CFDictionaryRef rec, const char *key, char *buf, size_t len);
void pda_set_attr(CFMutableDictionaryRef rec, const char *key, const char *value);

// writes the record root-only and refreshes the public index
int pda_write_user(CFDictionaryRef rec);
int pda_delete_user(const char *name);

// every user name, sorted, the caller releases the array
CFArrayRef pda_copy_user_names(void);
uid_t pda_next_uid(void);

// SALTED-SHA512-PBKDF2 in ShadowHashData, as macOS stores it
int pda_set_password(CFMutableDictionaryRef rec, const char *password);
int pda_check_password(CFDictionaryRef rec, const char *password);

// the hash-free passwd-format index libinfo reads for processes that are not root
int pda_write_index(void);

// the user whose uid this is, NULL when dslocal has none
CFMutableDictionaryRef pda_copy_user_by_uid(uid_t uid);

// group records sit beside the users, readable by everyone since they hold no secrets
CFMutableDictionaryRef pda_copy_group(const char *name);
int pda_write_group(CFDictionaryRef rec);
int pda_delete_group(const char *name);
CFArrayRef pda_copy_group_names(void);
gid_t pda_next_gid(void);

// a new group record, admin keeps macOS's gid 80
CFMutableDictionaryRef pda_new_group(const char *name, gid_t gid);

// membership by short name in the users attribute, creating admin when it is missing
int pda_group_add_member(const char *group, const char *user);
int pda_group_remove_member(const char *group, const char *user);
int pda_group_has_member(const char *group, const char *user);

// takes a user out of every group, for when the account goes
void pda_remove_from_all_groups(const char *user);

// a whole new account: record, home folder and admin membership, 0 or -1 with errno
// real and shell may be NULL, uid 0 picks the next free one
int pda_create_user(const char *name, const char *real, uid_t uid, const char *shell, const char *password, int admin);

// the record and its group memberships, and the home under /Users when remove_home is set
int pda_remove_user(const char *name, int remove_home);

// a short name dslocal takes as a record: letters, digits, '_', '-' and '.', with no leading '-' or '.'
int pda_valid_name(const char *name);

#endif
