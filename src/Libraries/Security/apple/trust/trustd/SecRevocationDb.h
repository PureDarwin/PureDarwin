/*
 * Copyright (c) 2016-2019 Apple Inc. All Rights Reserved.
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
 */

/*!
 @header SecRevocationDb
 The functions in SecRevocationDb.h provide an interface to look up
 revocation information, and refresh that information periodically.
 */

#ifndef _SECURITY_SECREVOCATIONDB_H_
#define _SECURITY_SECREVOCATIONDB_H_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFError.h>
#include <dispatch/dispatch.h>
#include <Security/SecBase.h>

__BEGIN_DECLS

/* issuer group data format */
typedef CF_ENUM(uint32_t, SecValidInfoFormat) {
    kSecValidInfoFormatUnknown      = 0,
    kSecValidInfoFormatSerial       = 1,
    kSecValidInfoFormatSHA256       = 2,
    kSecValidInfoFormatNto1         = 3
};

/* policy types */
typedef CF_ENUM(int8_t, SecValidPolicy) {
    kSecValidPolicyNone = -1,
    kSecValidPolicyAny = 0,
    kSecValidPolicyServerAuthentication = 1,
    kSecValidPolicyClientAuthentication = 2,
    kSecValidPolicyEmailProtection = 3,
    kSecValidPolicyCodeSigning = 4,
    kSecValidPolicyTimeStamping = 5,
};

/*!
    @typedef SecValidInfoRef
    @abstract CFType used to return valid info lookup results.
 */
typedef struct __SecValidInfo *SecValidInfoRef;

struct __SecValidInfo {
    CFRuntimeBase _base;

    SecValidInfoFormat  format;               // format of per-issuer validity data
    CFDataRef           certHash;             // SHA-256 hash of cert to which the following info applies
    CFDataRef           issuerHash;           // SHA-256 hash of issuing CA certificate
    CFDataRef           anchorHash;           // SHA-256 hash of anchor certificate (optional)
    bool                isOnList;             // true if this cert was found on allow list or block list
    bool                valid;                // true if this is an allow list, false if a block list
    bool                complete;             // true if list is complete (i.e. status is definitive)
    bool                checkOCSP;            // true if complete is false and OCSP check is required
    bool                knownOnly;            // true if intermediate CAs under issuer must be found in database
    bool                requireCT;            // true if this cert must have CT proof
    bool                noCACheck;            // true if an entry does not require an OCSP check to accept
    bool                overridable;          // true if the trust status is recoverable and can be overridden
    bool                hasDateConstraints;   // true if this issuer has supplemental date constraints
    bool                hasNameConstraints;   // true if this issuer has supplemental name constraints
    bool                hasPolicyConstraints; // true if this issuer has policy constraints
    CFDateRef           notBeforeDate;        // minimum notBefore for this certificate (if hasDateConstraints is true)
    CFDateRef           notAfterDate;         // maximum notAfter for this certificate (if hasDateConstraints is true)
    CFDataRef           nameConstraints;      // name constraints blob (if hasNameConstraints is true)
    CFDataRef           policyConstraints;    // policy constraints blob (if policyConstraints is true)
};

/*!
	@function SecValidInfoSetAnchor
	@abstract Updates a SecValidInfo reference with info about the anchor certificate in a chain.
	@param validInfo The SecValidInfo reference to be updated.
	@param anchor The certificate which anchors the chain for the certificate in this SecValidInfo reference.
	@discussion A SecValidInfo reference contains information about a single certificate and its issuer. In some cases, it may be necessary to additionally examine the anchor of the certificate chain to determine validity.
 */
void SecValidInfoSetAnchor(SecValidInfoRef validInfo, SecCertificateRef anchor);

/*!
	@function SecRevocationDbCheckNextUpdate
	@abstract Periodic hook to poll for updates.
 */
void SecRevocationDbCheckNextUpdate(void);

/*!
    @function SecRevocationDbUpdate
    @abstract Trigger update now. For use in testing and tools.
 */
bool SecRevocationDbUpdate(CFErrorRef *error);

/*!
	@function SecRevocationDbCopyMatching
	@abstract Returns a SecValidInfo reference if matching revocation (or allow list) info was found.
	@param certificate The certificate whose validity status is being requested.
	@param issuer The issuing CA certificate. If the cert is self-signed, the same reference should be passed in both certificate and issuer parameters. Omitting either cert parameter is an error and NULL will be returned.
	@result A SecValidInfoRef if there was matching revocation info. Caller must release this reference when finished by calling CFRelease. NULL is returned if no matching info was found in the database.
 */
SecValidInfoRef SecRevocationDbCopyMatching(SecCertificateRef certificate,
                                            SecCertificateRef issuer);

/*!
 @function SecRevocationDbContainsIssuer
 @abstract Returns true if the database contains an entry for the specified CA certificate.
 @param issuer The certificate being checked.
 @result If a matching issuer group was found, returns true, otherwise false.
*/
bool SecRevocationDbContainsIssuer(SecCertificateRef issuer);

/*!
	@function SecRevocationDbGetVersion
	@abstract Returns a CFIndex containing the version number of the database.
	@result On success, the returned version will be a value greater than or equal to zero. A version of 0 indicates an empty database which has yet to be populated. If the version cannot be obtained, -1 is returned.
 */
CFIndex SecRevocationDbGetVersion(void);

/*!
	@function SecRevocationDbGetSchemaVersion
	@abstract Returns a CFIndex containing the schema version number of the database.
	@result On success, the returned version will be a value greater than or equal to zero. A version of 0 indicates an empty database which has yet to be populated. If the version cannot be obtained, -1 is returned.
 */
CFIndex SecRevocationDbGetSchemaVersion(void);

/*!
 @function SecValidUpdateVerifyAndIngest
 @abstract Callback for receiving update data.
 @param updateData The decompressed update data.
 @param updateServer The source server for this data.
 @param fullUpdate If true, a full update was requested.
 */
void SecValidUpdateVerifyAndIngest(CFDataRef updateData, CFStringRef updateServer, bool fullUpdate);

/*!
 @function readValidFile
 @abstract Reads data into a CFDataRef using mmap.
 @param fileName The file to read.
 @param bytes The data read from the file.
 @result An integer indicating failure (non-zero) or success.
 @discussion This function mmaps the file and then makes a no-copy CFData for use of that mmapped file.  This data MUST be munmapped when the caller has finished with the data.
 */
int readValidFile(const char *fileName, CFDataRef  *bytes);

/*!
 @function SecRevocationDbComputeAndSetNextUpdateTime
 @abstract Callback to push forward next update.
 */
void SecRevocationDbComputeAndSetNextUpdateTime(void);

/*!
 @function SecRevocationDbInitialize
 @abstract Initializes revocation database if it doesn't exist or needs to be replaced. This should only be called once at process startup, before any database connections are established.
 */
void SecRevocationDbInitialize(void);

extern const CFStringRef kValidUpdateProdServer;
extern const CFStringRef kValidUpdateSeedServer;
extern const CFStringRef kValidUpdateCarryServer;

/*!
 @function SecRevocationDbCopyUpdateSource
 @abstract Returns the server source for updates of the revocation database.
 @result The base string of the server URI.
 */
CF_RETURNS_RETAINED CFStringRef SecRevocationDbCopyUpdateSource(void);


__END_DECLS

#endif /* _SECURITY_SECREVOCATIONDB_H_ */
