/*
 * Copyright (c) 2000-2004,2011,2013-2014 Apple Inc. All Rights Reserved.
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
 *
 * cssmapplePriv.h -- Private CSSM features specific to Apple's Implementation
 */

#ifndef _CSSMAPPLE_PRIV_H_
#define _CSSMAPPLE_PRIV_H_  1

#include <TargetConditionals.h>

#if TARGET_OS_OSX

#include <Security/cssmtype.h>
#include <Security/cssmapple.h>

#ifdef __cplusplus
extern "C" {
#endif
 
/* 
 * Options for X509TP's CSSM_TP_CertGroupVerify for policy 
 * CSSMOID_APPLE_TP_REVOCATION_OCSP. A pointer to, and length of, one 
 * of these is optionally placed in 
 * CSSM_TP_VERIFY_CONTEXT.Cred->Policy.PolicyIds[n].FieldValue.
 */

#define CSSM_APPLE_TP_OCSP_OPTS_VERSION		0

typedef uint32 CSSM_APPLE_TP_OCSP_OPT_FLAGS;
enum {
	// require OCSP verification for each cert; default is "try"
	CSSM_TP_ACTION_OCSP_REQUIRE_PER_CERT			= 0x00000001,
	// require OCSP verification for certs which claim an OCSP responder
	CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT 	= 0x00000002,
	// disable network OCSP transactions
	CSSM_TP_ACTION_OCSP_DISABLE_NET					= 0x00000004,
	// disable reads from local OCSP cache
	CSSM_TP_ACTION_OCSP_CACHE_READ_DISABLE			= 0x00000008,
	// disable reads from local OCSP cache
	CSSM_TP_ACTION_OCSP_CACHE_WRITE_DISABLE			= 0x00000010,
	// if set and positive OCSP verify for given cert, no further revocation
	// checking need be done on that cert
	CSSM_TP_ACTION_OCSP_SUFFICIENT					= 0x00000020,
	// generate nonce in OCSP request
	CSSM_TP_OCSP_GEN_NONCE							= 0x00000040,
	// when generating nonce, require matching nonce in response
	CSSM_TP_OCSP_REQUIRE_RESP_NONCE					= 0x00000080
};

typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER {
	uint32							Version;	
	CSSM_APPLE_TP_OCSP_OPT_FLAGS	Flags;
	CSSM_DATA_PTR					LocalResponder;		/* URI */
	CSSM_DATA_PTR					LocalResponderCert;	/* X509 DER encoded cert */
} CSSM_APPLE_TP_OCSP_OPTIONS DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

enum
{
	/* Given a "master" and dependent keychain, make the dependent "syncable" to the master's secrets */
	CSSM_APPLECSPDL_CSP_RECODE = CSSM_APPLE_PRIVATE_CSPDL_CODE_8,
	
	/* Given a keychain item, return a record identifier */
	CSSM_APPLECSPDL_DB_GET_RECORD_IDENTIFIER = CSSM_APPLE_PRIVATE_CSPDL_CODE_9,
	
	// Get the blob for a database
	CSSM_APPLECSPDL_DB_COPY_BLOB = CSSM_APPLE_PRIVATE_CSPDL_CODE_10,
	
	// enforce a bypass of crypto operations on insert
	CSSM_APPLECSPDL_DB_INSERT_WITHOUT_ENCRYPTION = CSSM_APPLE_PRIVATE_CSPDL_CODE_11,
	
	// enforce a bypass of crypto operations on modify
	CSSM_APPLECSPDL_DB_MODIFY_WITHOUT_ENCRYPTION = CSSM_APPLE_PRIVATE_CSPDL_CODE_12,
	
	// enforce a bypass of crypto operations on get
	CSSM_APPLECSPDL_DB_GET_WITHOUT_ENCRYPTION = CSSM_APPLE_PRIVATE_CSPDL_CODE_13,
	
	// convert a record identifier to a CSSM_DB_RECORD_IDENTIFIER for the CSP/DL
	CSSM_APPLECSPDL_DB_CONVERT_RECORD_IDENTIFIER = CSSM_APPLE_PRIVATE_CSPDL_CODE_14,
	
	// create the default records in a "blank" database
	CSSM_APPLECSPDL_DB_CREATE_WITH_BLOB = CSSM_APPLE_PRIVATE_CSPDL_CODE_15,

	// query a DB to see if a relation exists
	CSSM_APPLECSPDL_DB_RELATION_EXISTS = CSSM_APPLE_PRIVATE_CSPDL_CODE_16,
    
    // stash a DB key
    CSSM_APPLECSPDL_DB_STASH = CSSM_APPLE_PRIVATE_CSPDL_CODE_17,
    CSSM_APPLECSPDL_DB_STASH_CHECK = CSSM_APPLE_PRIVATE_CSPDL_CODE_18,

    // Ask the CSPDL for its database blob version number.
    CSSM_APPLECSPDL_DB_GET_BLOB_VERSION = CSSM_APPLE_PRIVATE_CSPDL_CODE_19,

    // Recode this database blob into a new blob version
    CSSM_APPLECSPDL_DB_RECODE_TO_BLOB_VERSION = CSSM_APPLE_PRIVATE_CSPDL_CODE_20,

    // Try to take the file lock on the underlying database
    // You should probably be sure that autocommit is off before taking this
    CSSM_APPLECSPDL_DB_TAKE_FILE_LOCK = CSSM_APPLE_PRIVATE_CSPDL_CODE_21,
    CSSM_APPLECSPDL_DB_RELEASE_FILE_LOCK = CSSM_APPLE_PRIVATE_CSPDL_CODE_22,

    // Make a backup of this database on the filesystem
    CSSM_APPLECSPDL_DB_MAKE_BACKUP = CSSM_APPLE_PRIVATE_CSPDL_CODE_23,

    // Make a copy of this database on the filesystem
    CSSM_APPLECSPDL_DB_MAKE_COPY = CSSM_APPLE_PRIVATE_CSPDL_CODE_24,

    // Make a clone of this database on the filesystem, and tell securityd about it
    CSSM_APPLECSPDL_DB_CLONE = CSSM_APPLE_PRIVATE_CSPDL_CODE_25,

    // Delete the file underlying this database
    CSSM_APPLECSPDL_DB_DELETE_FILE = CSSM_APPLE_PRIVATE_CSPDL_CODE_26,

    // Recoding of this database is complete
    CSSM_APPLECSPDL_DB_RECODE_FINISHED = CSSM_APPLE_PRIVATE_CSPDL_CODE_27,
};

/* AppleCSPDL passthrough parameters */
typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER cssm_applecspdl_db_recode_parameters
{
	CSSM_DATA dbBlob;
	CSSM_DATA extraData;
} CSSM_APPLECSPDL_RECODE_PARAMETERS DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER, *CSSM_APPLECSPDL_RECODE_PARAMETERS_PTR DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER cssm_applecspdl_db_copy_blob_parameters
{
	CSSM_DATA blob;
} CSSM_APPLECSPDL_DB_COPY_BLOB_PARAMETERS DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER cssm_applecspdl_db_insert_without_encryption_parameters
{
	CSSM_DB_RECORDTYPE recordType;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes;
	CSSM_DATA data;
} CSSM_APPLECSPDL_DB_INSERT_WITHOUT_ENCRYPTION_PARAMETERS DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER cssm_applecspdl_db_modify_without_encryption_parameters
{
	CSSM_DB_RECORDTYPE recordType;
	CSSM_DB_UNIQUE_RECORD_PTR uniqueID;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes;
	CSSM_DATA *data;
	CSSM_DB_MODIFY_MODE modifyMode;
} CSSM_APPLECSPDL_DB_MODIFY_WITHOUT_ENCRYPTION_PARAMETERS DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER cssm_applecspdl_db_get_without_encryption_parameters
{
	CSSM_DB_UNIQUE_RECORD_PTR uniqueID;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes;
} CSSM_APPLECSPDL_DB_GET_WITHOUT_ENCRYPTION_PARAMETERS DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER cssm_applecspdl_db_create_with_blob_parameters
{
	const char *dbName;
	const CSSM_NET_ADDRESS *dbLocation;
	const CSSM_DBINFO *dbInfo;
	CSSM_DB_ACCESS_TYPE accessRequest;
	const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry;
	const void *openParameters;
	const CSSM_DATA *blob;
} CSSM_APPLE_CSPDL_DB_CREATE_WITH_BLOB_PARAMETERS DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

#ifdef __cplusplus
}
#endif

#endif /* TARGET_OS_OSX */

#endif	/* _CSSMAPPLE_PRIV_H_ */
