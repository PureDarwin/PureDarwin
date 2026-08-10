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
 @header SecItemSchema.h - The thing that does the stuff with the gibli.
 */

#ifndef _SECURITYD_SECITEMSCHEMA_H_
#define _SECURITYD_SECITEMSCHEMA_H_

#include "keychain/securityd/SecDbItem.h"

__BEGIN_DECLS

const SecDbSchema* current_schema(void);
const SecDbSchema * const * all_schemas(void);

// class accessors for current schema
const SecDbClass* genp_class(void);
const SecDbClass* inet_class(void);
const SecDbClass* cert_class(void);
const SecDbClass* keys_class(void);

// Not really a class per-se
const SecDbClass* identity_class(void);

// Class with 1 element in it which is the database version.
const SecDbClass* tversion_class(void);

// Direct attribute accessors
// If you change one of these, update it here
extern const SecDbAttr v6v_Data;

extern const SecDbAttr v6agrp;
extern const SecDbAttr v6desc;
extern const SecDbAttr v6svce;
extern const SecDbAttr v7vwht;
extern const SecDbAttr v7tkid;
extern const SecDbAttr v7utomb;
extern const SecDbAttr v8musr;
extern const SecDbAttr v10itemuuid;
extern const SecDbAttr v10itempersistentref;

// TODO: Directly expose other important attributes like
// kSecItemSyncAttr, kSecItemTombAttr, kSecItemCdatAttr, kSecItemMdatAttr, kSecItemDataAttr
// This will prevent having to do lookups in SecDbItem for things by kind.

__END_DECLS

#endif /* _SECURITYD_SECITEMSCHEMA_H_ */
