/*
 * Copyright (c) 2009,2012-2014,2018-2019 Apple Inc. All Rights Reserved.
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

/*
    @header SecFrameworkStrings
    Defines localized strings using a macro that genstrings will recognize
    for each localizable string used by SecCertificate and SecTrust.
*/

#ifndef _SECURITY_SECFRAMEWORKSTRINGS_H_
#define _SECURITY_SECFRAMEWORKSTRINGS_H_

#include <Security/SecFramework.h>

__BEGIN_DECLS

#define SecCopyCertString(KEY) SecFrameworkCopyLocalizedString(KEY, CFSTR("Certificate"))
#define SecCopyCKString(KEY) SecFrameworkCopyLocalizedString(KEY, CFSTR("CloudKeychain"))

/* SecCertificate Strings */
#define SEC_NULL_KEY                SecStringWithDefaultValue("<NULL>", "Certificate", 0, "<NULL>", "Value of a field if its length is 0")
#define SEC_OID_TOO_LONG_KEY		SecStringWithDefaultValue("OID too long", "Certificate", 0, "OID too long", "value of an OID field if its length is more than what we allow for OIDs")
#define SEC_UNPARSED_KEY			SecStringWithDefaultValue("Unparsed %@", "Certificate", 0, "Unparsed %@", "Label of a value is printed into this string if the data can not been parsed according to its type")
#define SEC_INVALID_KEY             SecStringWithDefaultValue("Invalid %@", "Certificate", 0, "Invalid %@", "Label of a value is printed into this string if the data is not valid")
#define SEC_ALGORITHM_KEY			SecStringWithDefaultValue("Algorithm", "Certificate", 0, "Algorithm", "Label of the algorithm sub-field of an AlgorithmIdentifier")
#define SEC_PARAMETERS_KEY			SecStringWithDefaultValue("Parameters", "Certificate", 0, "Parameters", "Label of the parameters sub-field of an AlgorithmIdentifier")
#define SEC_NONE_KEY                SecStringWithDefaultValue("none", "Certificate", 0, "none", "field value of parameters field when no parameters are present")
#define SEC_BLOB_KEY                SecStringWithDefaultValue("%@; %d %@; data = %@", "Certificate", 0, "%@; %d %@; data = %@", "Format string for encoded field data (e.g. Sequence; 128 bytes; data = 00 00 ...)")
#define SEC_BYTE_STRING_KEY         SecStringWithDefaultValue("Byte string", "Certificate", 0, "Byte string", "First argument to SEC_BLOB_KEY format string for a Byte string")
#define SEC_BYTES_KEY               SecStringWithDefaultValue("bytes", "Certificate", 0, "bytes", "Third argument to SEC_BLOB_KEY format string for a byte string")
#define SEC_BIT_STRING_KEY          SecStringWithDefaultValue("Bit string", "Certificate", 0, "Bit string", "First argument to SEC_BLOB_KEY format string for a bit string")
#define SEC_BITS_KEY                SecStringWithDefaultValue("bits", "Certificate", 0, "bits", "")
#define SEC_SEQUENCE_KEY            SecStringWithDefaultValue("Sequence", "Certificate", 0, "Sequence", "First argument to SEC_BLOB_KEY format string for a Sequence")
#define SEC_SET_KEY                 SecStringWithDefaultValue("Set", "Certificate", 0, "Set", "First argument to SEC_BLOB_KEY format string for a Set")
#define SEC_NOT_DISPLAYED_KEY		SecStringWithDefaultValue("not displayed (tag = %ld; length %ld)", "Certificate", 0, "not displayed (tag = %ld; length %ld)", "format string for undisplayed field data with a given DER tag and length")
#define SEC_RDN_KEY                 SecStringWithDefaultValue("RDN", "Certificate", 0, "RDN", "Label of a RDN")
#define SEC_X501_NAME_KEY			SecStringWithDefaultValue("X.501 Name", "Certificate", 0, "X.501 Name", "Label of a X.501 Name")
#define SEC_YES_KEY                 SecStringWithDefaultValue("Yes", "Certificate", 0, "Yes", "Value for a boolean property when it's value is true (example critical: yes)")
#define SEC_NO_KEY                  SecStringWithDefaultValue("No", "Certificate", 0, "No", "Value for a boolean property when it's value is false (example critical: no)")
#define SEC_STRING_LIST_KEY         SecStringWithDefaultValue("%@, %@", "Certificate", 0, "%@, %@", "Format string used to build a list of values, first argument is list second argument is to be appended element")
#define SEC_DIGITAL_SIGNATURE_KEY	SecStringWithDefaultValue("Digital Signature", "Certificate", 0, "Digital Signature", "X.509 key usage bit-field name")
#define SEC_NON_REPUDIATION_KEY		SecStringWithDefaultValue("Non-Repudiation", "Certificate", 0, "Non-Repudiation", "X.509 key usage bit-field name")
#define SEC_KEY_ENCIPHERMENT_KEY	SecStringWithDefaultValue("Key Encipherment", "Certificate", 0, "Key Encipherment", "X.509 key usage bit-field name")
#define SEC_DATA_ENCIPHERMENT_KEY	SecStringWithDefaultValue("Data Encipherment", "Certificate", 0, "Data Encipherment", "X.509 key usage bit-field name")
#define SEC_KEY_AGREEMENT_KEY		SecStringWithDefaultValue("Key Agreement", "Certificate", 0, "Key Agreement", "X.509 key usage bit-field name")
#define SEC_CERT_SIGN_KEY			SecStringWithDefaultValue("Cert Sign", "Certificate", 0, "Cert Sign", "X.509 key usage bit-field name")
#define SEC_CRL_SIGN_KEY			SecStringWithDefaultValue("CRL Sign", "Certificate", 0, "CRL Sign", "X.509 key usage bit-field name")
#define SEC_ENCIPHER_ONLY_KEY		SecStringWithDefaultValue("Encipher Only", "Certificate", 0, "Encipher Only", "X.509 key usage bit-field name")
#define SEC_DECIPHER_ONLY_KEY		SecStringWithDefaultValue("Decipher Only", "Certificate", 0, "Decipher Only", "X.509 key usage bit-field name")
#define SEC_USAGE_KEY               SecStringWithDefaultValue("Usage", "Certificate", 0, "Usage", "Label for Key Usage bit-field values")
#define SEC_NOT_VALID_BEFORE_KEY	SecStringWithDefaultValue("Not Valid Before", "Certificate", 0, "Not Valid Before", "label indicating the soonest date at which something is valid")
#define SEC_NOT_VALID_AFTER_KEY		SecStringWithDefaultValue("Not Valid After", "Certificate", 0, "Not Valid After", "label indicating the date after which something is no longer valid")
#define SEC_VALIDITY_PERIOD_KEY     SecStringWithDefaultValue("Validity Period", "Certificate", 0, "Validity Period", "")
#define SEC_PRIVATE_KU_PERIOD_KEY   SecStringWithDefaultValue("Private Key Usage Period", "Certificate", 0, "Private Key Usage Period", "Label for an invalid private key usage period value")
#define SEC_OTHER_NAME_KEY			SecStringWithDefaultValue("Other Name", "Certificate", 0, "Other Name", "Label used for Other Name RDN when value is invalid")
#define SEC_EMAIL_ADDRESS_KEY		SecStringWithDefaultValue("Email Address", "Certificate", 0, "Email Address", "label for general name field value")
#define SEC_DNS_NAME_KEY			SecStringWithDefaultValue("DNS Name", "Certificate", 0, "DNS Name", "label for general name field value")
#define SEC_X400_ADDRESS_KEY		SecStringWithDefaultValue("X.400 Address", "Certificate", 0, "X.400 Address", "label for general name field value")
#define SEC_DIRECTORY_NAME_KEY		SecStringWithDefaultValue("Directory Name", "Certificate", 0, "Directory Name", "label for general name field value")
#define SEC_EDI_PARTY_NAME_KEY		SecStringWithDefaultValue("EDI Party Name", "Certificate", 0, "EDI Party Name", "label for general name field value")
#define SEC_URI_KEY                 SecStringWithDefaultValue("URI", "Certificate", 0, "URI", "label for general name field value")
#define SEC_IP_ADDRESS_KEY			SecStringWithDefaultValue("IP Address", "Certificate", 0, "IP Address", "label for general name field value")
#define SEC_REGISTERED_ID_KEY		SecStringWithDefaultValue("Registered ID", "Certificate", 0, "Registered ID", "label for general name field value")
#define SEC_GENERAL_NAME_KEY		SecStringWithDefaultValue("General Name", "Certificate", 0, "General Name", "Label used for General Name entry when value is invalid")
#define SEC_GENERAL_NAMES_KEY		SecStringWithDefaultValue("General Names", "Certificate", 0, "General Names", "Label used for General Names when value is invalid")
#define SEC_CERT_AUTHORITY_KEY		SecStringWithDefaultValue("Certificate Authority", "Certificate", 0, "Certificate Authority", "Label for boolean is_ca property of a basic constraints extension")
#define SEC_PATH_LEN_CONSTRAINT_KEY	SecStringWithDefaultValue("Path Length Constraint", "Certificate", 0, "Path Length Constraint", "Label for path length constraint property of a basic constraints extension")
#define SEC_BASIC_CONSTRAINTS_KEY	SecStringWithDefaultValue("Basic Constraints", "Certificate", 0, "Basic Constraints", "Label used for Basic Constraints when value is invalid")

/* Name Constraints extension */
#define SEC_NAME_CONSTRAINTS_KEY	SecStringWithDefaultValue("Name Constraints", "Certificate", 0, "Name Constraints", "Label used for Name Constraints when value is invalid")
#define SEC_PERMITTED_MINIMUM_KEY	SecStringWithDefaultValue("Permitted Subtree Minimum", "Certificate", 0, "Permitted Subtree Minimum", "Label for minimum base distance property of a permitted subtree in name constraints extension.")
#define SEC_PERMITTED_MAXIMUM_KEY	SecStringWithDefaultValue("Permitted Subtree Maximum", "Certificate", 0, "Permitted Subtree Maximum", "Label for maximum base distance property of a permitted subtree in name constraints extension.")
#define SEC_PERMITTED_NAME_KEY		SecStringWithDefaultValue("Permitted Subtree General Name", "Certificate", 0, "Permitted Subtree General Name", "Label for general name of a permitted subtree in name constraints extension.")
#define SEC_EXCLUDED_MINIMUM_KEY	SecStringWithDefaultValue("Excluded Subtree Minimum", "Certificate", 0, "Excluded Subtree Minimum", "Label for minimum base distance property of an excluded subtree in name constraints extension.")
#define SEC_EXCLUDED_MAXIMUM_KEY	SecStringWithDefaultValue("Excluded Subtree Maximum", "Certificate", 0, "Excluded Subtree Maximum", "Label for maximum base distance property of an excluded subtree in name constraints extension.")
#define SEC_EXCLUDED_NAME_KEY		SecStringWithDefaultValue("Excluded Subtree General Name", "Certificate", 0, "Excluded Subtree General Name", "Label for general name of an excluded subtree in name constraints extension.")

/* CRL Distribution Points extension */
#define SEC_NAME_REL_CRL_ISSUER_KEY	SecStringWithDefaultValue("Name Relative To CRL Issuer", "Certificate", 0, "Name Relative To CRL Issuer", "Subsection label in CRL Distribution Points extension.")
#define SEC_UNUSED_KEY              SecStringWithDefaultValue("Unused", "Certificate", 0, "Unused", "CRL Distribution Points extension supported reason name")
#define SEC_KEY_COMPROMISE_KEY		SecStringWithDefaultValue("Key Compromise", "Certificate", 0, "Key Compromise", "CRL Distribution Points extension supported reason name")
#define SEC_CA_COMPROMISE_KEY		SecStringWithDefaultValue("CA Compromise", "Certificate", 0, "CA Compromise", "CRL Distribution Points extension supported reason name")
#define SEC_AFFILIATION_CHANGED_KEY	SecStringWithDefaultValue("Affiliation Changed", "Certificate", 0, "Affiliation Changed", "CRL Distribution Points extension supported reason name")
#define SEC_SUPERSEDED_KEY			SecStringWithDefaultValue("Superseded", "Certificate", 0, "Superseded", "CRL Distribution Points extension supported reason name")
#define SEC_CESSATION_OF_OPER_KEY	SecStringWithDefaultValue("Cessation Of Operation", "Certificate", 0, "Cessation Of Operation", "CRL Distribution Points extension supported reason name")
#define SEC_CERTIFICATE_HOLD_KEY	SecStringWithDefaultValue("Certificate Hold", "Certificate", 0, "Certificate Hold", "CRL Distribution Points extension supported reason name")
#define SEC_PRIV_WITHDRAWN_KEY		SecStringWithDefaultValue("Privilege Withdrawn", "Certificate", 0, "Privilege Withdrawn", "CRL Distribution Points extension supported reason name")
#define SEC_AA_COMPROMISE_KEY		SecStringWithDefaultValue("AA Compromise", "Certificate", 0, "AA Compromise", "CRL Distribution Points extension supported reason name")
#define SEC_REASONS_KEY             SecStringWithDefaultValue("Reasons", "Certificate", 0, "Reasons", "CRL Distribution Points extension supported reasons bit-field label")
#define SEC_CRL_ISSUER_KEY			SecStringWithDefaultValue("CRL Issuer", "Certificate", 0, "CRL Issuer", "Label for CRL issuer field of CRL Distribution Points extension")
#define SEC_CRL_DISTR_POINTS_KEY	SecStringWithDefaultValue("CRL Distribution Points", "Certificate", 0, "CRL Distribution Points", "CRL Distribution Points extension label")

/* Certificate Policies extension */
#define SEC_POLICY_IDENTIFIER_KEY	SecStringWithDefaultValue("Policy Identifier #%d", "Certificate", 0, "Policy Identifier #%d", "Format string for label of field in Certificate Policies extension, %d is a monotonic increasing counter starting at 1")
#define SEC_POLICY_QUALIFIER_KEY	SecStringWithDefaultValue("Policy Qualifier #%d", "Certificate", 0, "Policy Qualifier #%d", "Format string for label of field in Certificate Policies extension, %d is a monotonic increasing counter starting at 1")
#define SEC_CPS_URI_KEY             SecStringWithDefaultValue("CPS URI", "Certificate", 0, "CPS URI", "Label of field in Certificate Policies extension")
#define SEC_ORGANIZATION_KEY		SecStringWithDefaultValue("Organization", "Certificate", 0, "Organization", "Label of field in Certificate Policies extension")
#define SEC_NOTICE_NUMBERS_KEY		SecStringWithDefaultValue("Notice Numbers", "Certificate", 0, "Notice Numbers", "Label of field in Certificate Policies extension")
#define SEC_EXPLICIT_TEXT_KEY		SecStringWithDefaultValue("Explicit Text", "Certificate", 0, "Explicit Text", "Label of field in Certificate Policies extension")
#define SEC_QUALIFIER_KEY			SecStringWithDefaultValue("Qualifier", "Certificate", 0, "Qualifier", "Label of field in Certificate Policies extension")
#define SEC_CERT_POLICIES_KEY       SecStringWithDefaultValue("Certificate Policies", "Certificate", 0, "Certificate Policies", "Certificate Policies extension label")

/* Subject and Authority Key Identifier extensions */
#define SEC_KEY_IDENTIFIER_KEY		SecStringWithDefaultValue("Key Identifier", "Certificate", 0, "Key Identifier", "Label of field in Subject or Authority Key Identifier extension")
#define SEC_SUBJ_KEY_ID_KEY         SecStringWithDefaultValue("Subject Key Identifier", "Certificate", 0, "Subject Key Identifier", "Subject Key Identifier extension label")
#define SEC_AUTH_CERT_SERIAL_KEY	SecStringWithDefaultValue("Authority Certificate Serial Number", "Certificate", 0, "Authority Certificate Serial Number", "Label of field in Authority Key Identifier extension")
#define SEC_AUTHORITY_KEY_ID_KEY	SecStringWithDefaultValue("Authority Key Identifier", "Certificate", 0, "Authority Key Identifier", "Authority Key Identifier extension label")

/* Policy constraints extension */
#define SEC_REQUIRE_EXPL_POLICY_KEY	SecStringWithDefaultValue("Require Explicit Policy", "Certificate", 0, "Require Explicit Policy", "Label of field in policy constraints extension")
#define SEC_INHIBIT_POLICY_MAP_KEY	SecStringWithDefaultValue("Inhibit Policy Mapping", "Certificate", 0, "Inhibit Policy Mapping", "Label of field in policy constraints extension")
#define SEC_POLICY_CONSTRAINTS_KEY	SecStringWithDefaultValue("Policy Constraints", "Certificate", 0, "Policy Constraints", "Policy constraints extension label")

/* Extended key usage extension */
#define SEC_PURPOSE_KEY             SecStringWithDefaultValue("Purpose", "Certificate", 0, "Purpose", "Label of field in extended key usage extension")
#define SEC_EXTENDED_KEY_USAGE_KEY	SecStringWithDefaultValue("Extended Key Usage", "Certificate", 0, "Extended Key Usage", "Extended key usage extension label")

/* Authority info access extension */
#define SEC_ACCESS_METHOD_KEY		SecStringWithDefaultValue("Access Method", "Certificate", 0, "Access Method", "Label of field in authority info access extension")
//#define SEC_ACCESS_LOCATION_KEY		SecStringWithDefaultValue("Access Location", "Certificate", 0, "Access Location", "Label of field in authority info access extension")
#define SEC_AUTH_INFO_ACCESS_KEY	SecStringWithDefaultValue("Authority Information Access", "Certificate", 0, "Authority Information Access", "Authority info access extension label")

/* Netscape cert type extension */
#define SEC_SSL_CLIENT_KEY			SecStringWithDefaultValue("SSL client", "Certificate", 0, "SSL client", "Netscape certificate type usage value")
#define SEC_SSL_SERVER_KEY			SecStringWithDefaultValue("SSL server", "Certificate", 0, "SSL server", "Netscape certificate type usage value")
#define SEC_SMIME_KEY               SecStringWithDefaultValue("S/MIME", "Certificate", 0, "S/MIME", "Netscape certificate type usage value")
#define SEC_OBJECT_SIGNING_KEY		SecStringWithDefaultValue("Object Signing", "Certificate", 0, "Object Signing", "Netscape certificate type usage value")
#define SEC_RESERVED_KEY			SecStringWithDefaultValue("Reserved", "Certificate", 0, "Reserved", "Netscape certificate type usage value")
#define SEC_SSL_CA_KEY              SecStringWithDefaultValue("SSL CA", "Certificate", 0, "SSL CA", "Netscape certificate type usage value")
#define SEC_SMIME_CA_KEY			SecStringWithDefaultValue("S/MIME CA", "Certificate", 0, "S/MIME CA", "Netscape certificate type usage value")
#define SEC_OBJECT_SIGNING_CA_KEY	SecStringWithDefaultValue("Object Signing CA", "Certificate", 0, "Object Signing CA", "Netscape certificate type usage value")

/* Generic extension strings. */
#define SEC_CRITICAL_KEY			SecStringWithDefaultValue("Critical", "Certificate", 0, "Critical", "Label of field in extension that indicates whether this extension is critical")
#define SEC_DATA_KEY                SecStringWithDefaultValue("Data", "Certificate", 0, "Data", "Label for raw data of extension (used for unknown extensions)")

#define SEC_COMMON_NAME_DESC_KEY	SecStringWithDefaultValue("%@ (%@)", "Certificate", 0, "%@ (%@)", "If a X.500 name has a description and a common name we display Common Name (Description) using this format string")

//#define SEC_ISSUER_SUMMARY_KEY		SecStringWithDefaultValue("Issuer Summary", "Certificate", 0, "Issuer Summary", "")
//#define SEC_ISSUED_BY_KEY			SecStringWithDefaultValue("Issued By", "Certificate", 0, "Issued By", "")
#define SEC_EXPIRED_KEY             SecStringWithDefaultValue("Expired", "Certificate", 0, "Expired", "")
#define SEC_CERT_EXPIRED_KEY		SecStringWithDefaultValue("This certificate has expired", "Certificate", 0, "This certificate has expired", "")
#define SEC_VALID_FROM_KEY			SecStringWithDefaultValue("Valid from", "Certificate", 0, "Valid from", "")
#define SEC_CERT_NOT_YET_VALID_KEY	SecStringWithDefaultValue("This certificate is not yet valid", "Certificate", 0, "This certificate is not yet valid", "")
#define SEC_ISSUER_EXPIRED_KEY		SecStringWithDefaultValue("This certificate has an issuer that has expired", "Certificate", 0, "This certificate has an issuer that has expired", "")
#define SEC_ISSR_NOT_YET_VALID_KEY  SecStringWithDefaultValue("This certificate has an issuer that is not yet valid", "Certificate", 0, "This certificate has an issuer that is not yet valid", "")
#define SEC_EXPIRES_KEY             SecStringWithDefaultValue("Expires", "Certificate", 0, "Expires", "Label of expiration date value when certificate is temporally valid")
#define SEC_CERT_VALID_KEY			SecStringWithDefaultValue("This certificate is valid", "Certificate", 0, "This certificate is valid", "The certificate is temporally valid")

#define SEC_SUBJECT_NAME_KEY		SecStringWithDefaultValue("Subject Name", "Certificate", 0, "Subject Name", "")
#define SEC_ISSUER_NAME_KEY			SecStringWithDefaultValue("Issuer Name", "Certificate", 0, "Issuer Name", "")

#define SEC_CERT_VERSION_VALUE_KEY  SecStringWithDefaultValue("%d", "Certificate", 0, "%d", "format string to turn version number into a string")
#define SEC_VERSION_KEY             SecStringWithDefaultValue("Version", "Certificate", 0, "Version", "")
#define SEC_SERIAL_NUMBER_KEY		SecStringWithDefaultValue("Serial Number", "Certificate", 0, "Serial Number", "")
#define SEC_SUBJECT_UNIQUE_ID_KEY	SecStringWithDefaultValue("Subject Unique ID", "Certificate", 0, "Subject Unique ID", "")
#define SEC_ISSUER_UNIQUE_ID_KEY	SecStringWithDefaultValue("Issuer Unique ID", "Certificate", 0, "Issuer Unique ID", "")

#define SEC_PUBLIC_KEY_KEY          SecStringWithDefaultValue("Public Key Info", "Certificate", 0, "Public Key Info", "")
#define SEC_PUBLIC_KEY_ALG_KEY		SecStringWithDefaultValue("Public Key Algorithm", "Certificate", 0, "Public Key Algorithm", "")
#define SEC_PUBLIC_KEY_DATA_KEY		SecStringWithDefaultValue("Public Key Data", "Certificate", 0, "Public Key Data", "")
#define SEC_PUBLIC_KEY_SIZE_KEY     SecStringWithDefaultValue("Public Key Size", "Certificate", 0, "Public Key Size", "")

#define SEC_SIGNATURE_KEY			SecStringWithDefaultValue("Signature", "Certificate", 0, "Signature", "")
#define SEC_SIGNATURE_ALGORITHM_KEY SecStringWithDefaultValue("Signature Algorithm", "Certificate", 0, "Signature Algorithm", "")
#define SEC_SIGNATURE_DATA_KEY      SecStringWithDefaultValue("Signature Data", "Certificate", 0, "Signature Data", "")

#define SEC_FINGERPRINTS_KEY         SecStringWithDefaultValue("Fingerprints", "Certificate", 0, "Fingerprints", "")
#define SEC_SHA1_FINGERPRINT_KEY    SecStringWithDefaultValue("SHA-1", "Certificate", 0, "SHA-1", "")
#define SEC_SHA2_FINGERPRINT_KEY    SecStringWithDefaultValue("SHA-256", "Certificate", 0, "SHA-256", "")

/* Cloud Keychain Strings */
#define SEC_CK_PASSWORD_INCORRECT   SecStringWithDefaultValue("Incorrect Password For “%@”", "CloudKeychain", 0, "Incorrect Password For “%@”", "Title for alert when password has been entered incorrectly")
#define SEC_CK_TRY_AGAIN            SecStringWithDefaultValue("Try Again", "CloudKeychain", 0, "Try Again", "Button for try again after incorrect password")
#define SEC_CK_ALLOW                SecStringWithDefaultValue("Allow", "CloudKeychain", 0, "Allow", "Allow button")
#define SEC_CK_DONT_ALLOW           SecStringWithDefaultValue("Don’t Allow", "CloudKeychain", 0, "Don’t Allow", "Don’t Allow button")
#define SEC_CK_ICLOUD_PASSWORD      SecStringWithDefaultValue("Password", "CloudKeychain", 0, "password", "Password prompt text")

#define SEC_CK_TID_FUTURE           SecStringWithDefaultValue("the future", "CloudKeychain", 0, "the future", "the future")
#define SEC_CK_TID_NOW              SecStringWithDefaultValue("now", "CloudKeychain", 0, "now", "now")
#define SEC_CK_TID_SUBSECOND        SecStringWithDefaultValue("less than a second", "CloudKeychain", 0, "less than a second", "Less then then one second")
#define SEC_CK_TID_SECONDS          SecStringWithDefaultValue("seconds", "CloudKeychain", 0, "seconds", "More than one second")
#define SEC_CK_TID_MINUTES          SecStringWithDefaultValue("minutes", "CloudKeychain", 0, "minutes", "More than one minute")
#define SEC_CK_TID_HOURS            SecStringWithDefaultValue("hours", "CloudKeychain", 0, "hours", "More than one hour")
#define SEC_CK_TID_DAY              SecStringWithDefaultValue("day", "CloudKeychain", 0, "day", "One day")
#define SEC_CK_TID_DAYS             SecStringWithDefaultValue("days", "CloudKeychain", 0, "days", "More than one day")

#define SEC_CK_PWD_REQUIRED_TITLE		SecStringWithDefaultValue("Apple ID Password Required", "CloudKeychain", 0, "Apple ID Password Required", "Title for alert when iCloud keychain was disabled or reset")
#define SEC_CK_PWD_REQUIRED_BODY_OSX	SecStringWithDefaultValue("Enter your password in Apple ID Preferences.", "CloudKeychain", 0, "Enter your password in Apple ID Preferences.", "macOS alert text when iCloud keychain was disabled or reset")
#define SEC_CK_PWD_REQUIRED_BODY_IOS	SecStringWithDefaultValue("Enter your password in iCloud Settings.", "CloudKeychain", 0, "Enter your password in iCloud Settings.", "iOS alert text when iCloud keychain was disabled or reset")
#define SEC_CK_CR_REASON_INTERNAL		SecStringWithDefaultValue(" (AppleInternal: departure reason %s)", "CloudKeychain", 0, " (AppleInternal: departure reason %s)", "Display departure reason code on internal devices")
#define SEC_CK_CONTINUE					SecStringWithDefaultValue("Continue", "CloudKeychain", 0, "Continue", "Button text to continue to iCloud settings (iOS)")
#define SEC_CK_NOT_NOW					SecStringWithDefaultValue("Not Now", "CloudKeychain", 0, "Not Now", "Button text to dismiss alert")

#define SEC_CK_APPROVAL_TITLE           SecStringWithDefaultValue("Approve “%@”?", "CloudKeychain", 0, "Approve “%@”?", "Title for alert when approving another device")
#define SEC_CK_APPROVAL_BODY_OSX_IPAD   SecStringWithDefaultValue("This iPad wants to use your iCloud account.", "CloudKeychain", 0, "This iPad wants to use your iCloud account.", "Body text when approving an iPad on Mac")
#define SEC_CK_APPROVAL_BODY_OSX_IPHONE SecStringWithDefaultValue("This iPhone wants to use your iCloud account.", "CloudKeychain", 0, "This iPhone wants to use your iCloud account.", "Body text when approving an iPhone on Mac")
#define SEC_CK_APPROVAL_BODY_OSX_IPOD   SecStringWithDefaultValue("This iPod wants to use your iCloud account.", "CloudKeychain", 0, "This iPod wants to use your iCloud account.", "Body text when approving an iPod on Mac")
#define SEC_CK_APPROVAL_BODY_OSX_MAC    SecStringWithDefaultValue("This Mac wants to use your iCloud account.", "CloudKeychain", 0, "This Mac wants to use your iCloud account.", "Body text when approving a Mac on Mac")
#define SEC_CK_APPROVAL_BODY_OSX_GENERIC SecStringWithDefaultValue("This device wants to use your iCloud account.", "CloudKeychain", 0, "This device wants to use your iCloud account.", "Body text when approving a device on Mac")
#define SEC_CK_APPROVE                  SecStringWithDefaultValue("Approve", "CloudKeychain", 0, "Approve", "Button text to approve iCloud sign in request")
#define SEC_CK_DECLINE                  SecStringWithDefaultValue("Decline", "CloudKeychain", 0, "Decline", "Button text to decline iCloud sign in request")

#define SEC_CK_APPROVAL_BODY_IOS_IPAD	SecStringWithDefaultValue("Enter the password for the Apple ID “%@” to allow this new iPad to use your iCloud account.", "CloudKeychain", 0, "Enter the password for the Apple ID “%@” to allow this new iPad to use your iCloud account.", "Body text when approving an iPad")
#define SEC_CK_APPROVAL_BODY_IOS_IPHONE	SecStringWithDefaultValue("Enter the password for the Apple ID “%@” to allow this new iPhone to use your iCloud account.", "CloudKeychain", 0, "Enter the password for the Apple ID “%@” to allow this new iPhone to use your iCloud account.", "Body text when approving an iPhone")
#define SEC_CK_APPROVAL_BODY_IOS_IPOD	SecStringWithDefaultValue("Enter the password for the Apple ID “%@” to allow this new iPod to use your iCloud account.", "CloudKeychain", 0, "Enter the password for the Apple ID “%@” to allow this new iPod to use your iCloud account.", "Body text when approving an iPod")
#define SEC_CK_APPROVAL_BODY_IOS_MAC	SecStringWithDefaultValue("Enter the password for the Apple ID “%@” to allow this new Mac to use your iCloud account.", "CloudKeychain", 0, "Enter the password for the Apple ID “%@” to allow this new Mac to use your iCloud account.", "Body text when approving another Mac")
#define SEC_CK_APPROVAL_BODY_IOS_GENERIC	SecStringWithDefaultValue("Enter the password for the Apple ID “%@” to allow this new device to use your iCloud account.", "CloudKeychain", 0, "Enter the password for the Apple ID “%@” to allow this new device to use your iCloud account.", "Body text when approving another (generic) device")

#define SEC_CK_REMINDER_TITLE_OSX		SecStringWithDefaultValue("iCloud Approval Required", "CloudKeychain", 0, "iCloud Approval Required", "Title for reminder that iCloud Keychain Application (from this device) is still pending")
#define SEC_CK_REMINDER_BODY_OSX		SecStringWithDefaultValue("This Mac is still waiting for approval by another device.", "CloudKeychain", 0, "This Mac is still waiting for approval by another device.", "Body text for reminder that iCloud Keychain Application (from this device) is still pending")
#define SEC_CK_REMINDER_TITLE_IOS		SecStringWithDefaultValue("Approval Request Sent", "CloudKeychain", 0, "Approval Request Sent", "Title for reminder that iCloud Keychain Application (from this device) is still pending")
#define SEC_CK_REMINDER_BODY_IOS_IPAD	SecStringWithDefaultValue("To continue using iCloud on this iPad, approve it from one of your other devices using iCloud or enter your iCloud Security Code.", "CloudKeychain", 0, "To continue using iCloud on this iPad, approve it from one of your other devices using iCloud or enter your iCloud Security Code.", "Body of reminder text that the iCloud keychain application for this iPad is still pending")
#define SEC_CK_REMINDER_BODY_IOS_IPHONE	SecStringWithDefaultValue("To continue using iCloud on this iPhone, approve it from one of your other devices using iCloud or enter your iCloud Security Code.", "CloudKeychain", 0, "To continue using iCloud on this iPhone, approve it from one of your other devices using iCloud or enter your iCloud Security Code.", "Body of reminder text that the iCloud keychain application for this iPhone is still pending")
#define SEC_CK_REMINDER_BODY_IOS_IPOD	SecStringWithDefaultValue("To continue using iCloud on this iPod, approve it from one of your other devices using iCloud or enter your iCloud Security Code.", "CloudKeychain", 0, "To continue using iCloud on this iPod, approve it from one of your other devices using iCloud or enter your iCloud Security Code.", "Body of reminder text that the iCloud keychain application for this iPod is still pending")
#define SEC_CK_REMINDER_BODY_IOS_GENERIC	SecStringWithDefaultValue("To continue using iCloud on this device, approve it from one of your other devices using iCloud or enter your iCloud Security Code.", "CloudKeychain", 0, "To continue using iCloud on this device, approve it from one of your other devices using iCloud or enter your iCloud Security Code.", "Body of reminder text that the iCloud keychain application for this device is still pending")
#define SEC_CK_REMINDER_BUTTON_ICSC		SecStringWithDefaultValue("Use Security Code", "CloudKeychain", 0, "Use Security Code", "Button label to approve via iCSC")
#define SEC_CK_REMINDER_BUTTON_OK		SecStringWithDefaultValue("OK", "CloudKeychain", 0, "OK", "Button label to acknowledge/dismiss reminder alert without further action")

/* Trust errors */
#define SEC_INVALID_LINKAGE_KEY         SecStringWithDefaultValue("Invalid certificate chain linkage.", "Certificate", 0, "Invalid certificate chain linkage.", "")
#define SEC_BAD_CRIT_EXTN_KEY           SecStringWithDefaultValue("One or more unsupported critical extensions found.", "Certificate", 0, "One or more unsupported critical extensions found.", "")
#define SEC_ROOT_UNTRUSTED_KEY          SecStringWithDefaultValue("Root certificate is not trusted.", "Certificate", 0, "Root certificate is not trusted.", "")
#define SEC_HOSTNAME_MISMATCH_KEY       SecStringWithDefaultValue("Hostname mismatch.", "Certificate", 0, "Hostname mismatch.", "")
#define SEC_POLICY__REQ_NOT_MET_KEY     SecStringWithDefaultValue("Policy requirements not met.", "Certificate", 0, "Policy requirements not met.", "")
#define SEC_CHAIN_VALIDITY_ERR_KEY      SecStringWithDefaultValue("One or more certificates have expired or are not valid yet.", "Certificate", 0, "One or more certificates have expired or are not valid yet.", "")
#define SEC_WEAK_KEY_ERR_KEY            SecStringWithDefaultValue("One or more certificates is using a weak key size.", "Certificate", 0, "One or more certificates is using a weak key size.", "")
#define SEC_MISSING_INTERMEDIATE_KEY    SecStringWithDefaultValue("Unable to build chain to root certificate.", "Certificate", 0, "Unable to build chain to root certificate.", "")

#define SEC_TRUST_CERTIFICATE_ERROR         SecStringWithDefaultValue("Certificate %ld “%@” has errors: ", "Trust", 0, "Certificate %ld “%@” has errors: ", "Preface for per-certificate errors")

#define SEC_TRUST_ERROR_SUBTYPE_BLOCKED     SecStringWithDefaultValue("“%@” certificate is blocked", "Trust", 0, "“%@” certificate is blocked", "Error for blocked certificates")
#define SEC_TRUST_ERROR_SUBTYPE_REVOKED     SecStringWithDefaultValue("“%@” certificate is revoked", "Trust", 0, "“%@” certificate is revoked", "Error for revoked certificates")
#define SEC_TRUST_ERROR_SUBTYPE_KEYSIZE     SecStringWithDefaultValue("“%@” certificate is using a broken key size", "Trust", 0, "“%@” certificate is using a broken key size", "Error for certificates with weak key sizes")
#define SEC_TRUST_ERROR_SUBTYPE_WEAKHASH    SecStringWithDefaultValue("“%@” certificate is using a broken signature algorithm", "Trust", 0, "“%@” certificate is using a broken signature algorithm", "Error for certificates with weak signature algorithms")
#define SEC_TRUST_ERROR_SUBTYPE_DENIED      SecStringWithDefaultValue("User or administrator set “%@” certificate as distrusted", "Trust", 0, "User or administrator set “%@” certificate as distrusted", "Error for certificates with deny trust settings")
#define SEC_TRUST_ERROR_SUBTYPE_COMPLIANCE  SecStringWithDefaultValue("“%@” certificate is not standards compliant", "Trust", 0, "“%@” certificate is not standards compliant", "Error for certificates that violate standards")
#define SEC_TRUST_ERROR_SUBTYPE_EXPIRED     SecStringWithDefaultValue("“%@” certificate is expired", "Trust", 0, "“%@” certificate is expired", "Error for certificates that are expired")
#define SEC_TRUST_ERROR_SUBTYPE_TRUST       SecStringWithDefaultValue("“%@” certificate is not trusted", "Trust", 0, "“%@” certificate is not trusted", "Error for certificates that are not trusted")
#define SEC_TRUST_ERROR_SUBTYPE_NAME        SecStringWithDefaultValue("“%@” certificate name does not match input", "Trust", 0, "“%@” certificate name does not match input", "Error for certificates whose names do not match the policy")
#define SEC_TRUST_ERROR_SUBTYPE_USAGE       SecStringWithDefaultValue("“%@” certificate is not permitted for this usage", "Trust", 0, "“%@” certificate is not permitted for this usage", "Error for certificates whose usages do not match the policy")
#define SEC_TRUST_ERROR_SUBTYPE_PINNING     SecStringWithDefaultValue("%@ certificates do not meet pinning requirements", "Trust", 0, "%@ certificates do not meet pinning requirements", "Error for certificates that do not meet pinning requirements")
#define SEC_TRUST_ERROR_SUBTYPE_ISSUER      SecStringWithDefaultValue("“%@” certificate does not meet issuer constraints", "Trust", 0, "“%@” certificate does not meet issuer constraints", "Error for certificates which violate constraints set on their issuer")
#define SEC_TRUST_ERROR_SUBTYPE_INVALID     SecStringWithDefaultValue("Unknown trust error for “%@” certificate", "Trust", 0, "Unknown trust error for “%@” certificate", "Error for unknown error")

//Note the the following errors do not follow the casing conventions of the above so that they can be used with POLICYCHECKMACRO
#define SEC_TRUST_ERROR_SSLHostname                 SecStringWithDefaultValue("SSL hostname does not match name(s) in certificate", "Trust", 0, "SSL hostname does not match name(s) in certificate", "Error for SSL hostname mismatch")
#define SEC_TRUST_ERROR_Email                       SecStringWithDefaultValue("Email address does not match name(s) in certificate", "Trust", 0, "Email address does not match name(s) in certificate", "Error for email mismatch")
#define SEC_TRUST_ERROR_TemporalValidity            SecStringWithDefaultValue("Certificate is not temporally valid", "Trust", 0, "Certificate is not temporally valid", "Error for temporal validity")
#define SEC_TRUST_ERROR_WeakKeySize                 SecStringWithDefaultValue("Certificate is using a broken key size", "Trust", 0, "Certificate is using a broken key size", "Error for weak keys")
#define SEC_TRUST_ERROR_WeakSignature               SecStringWithDefaultValue("Certificate is using a broken signature algorithm", "Trust", 0, "Certificate is using a broken signature algorithm", "Error for weak signatures")
#define SEC_TRUST_ERROR_KeyUsage                    SecStringWithDefaultValue("Key usage does not match certificate usage", "Trust", 0, "Key usage does not match certificate usage", "Error for key usage mismatch")
#define SEC_TRUST_ERROR_KeyUsageReportOnly          SecStringWithDefaultValue("Key usage does not match certificate usage", "Trust", 0, "Key usage does not match certificate usage", "Error for key usage mismatch")
#define SEC_TRUST_ERROR_ExtendedKeyUsage            SecStringWithDefaultValue("Extended key usage does not match certificate usage", "Trust", 0, "Extended key usage does not match certificate usage", "Error for extended key usage mismatch")
#define SEC_TRUST_ERROR_SubjectCommonName           SecStringWithDefaultValue("Common Name does not match expected name", "Trust", 0, "Common Name does not match expected name", "Error for subject common name mismatch")
#define SEC_TRUST_ERROR_SubjectCommonNamePrefix     SecStringWithDefaultValue("Common Name does not match expected name", "Trust", 0, "Common Name does not match expected name", "Error for subject common name prefix mismatch")
#define SEC_TRUST_ERROR_SubjectCommonNameTEST       SecStringWithDefaultValue("Common Name does not match expected name", "Trust", 0, "Common Name does not match expected name", "Error for subject common name mismatch, allowing test")
#define SEC_TRUST_ERROR_SubjectOrganization         SecStringWithDefaultValue("Organization does not match expected name", "Trust", 0, "Organization does not match expected name", "Error for subject organization mismatch")
#define SEC_TRUST_ERROR_SubjectOrganizationalUnit   SecStringWithDefaultValue("Organizational Unit does not match expected name", "Trust", 0, "Certificate Organizational Unit does not match expected name", "Error for subject organizational unit mismatch")
#define SEC_TRUST_ERROR_NotValidBefore              SecStringWithDefaultValue("Certificate issued before allowed time", "Trust", 0, "Certificate issued before allowed time", "Error for not before date")
#define SEC_TRUST_ERROR_EAPTrustedServerNames       SecStringWithDefaultValue("Trusted EAP hostname does not match name(s) in certificate", "Trust", 0, "Trusted EAP hostname does not match name(s) in certificate", "Error for EAP hostname mismatch")
#define SEC_TRUST_ERROR_LeafMarkerOid               SecStringWithDefaultValue("Missing project-specific extension OID", "Trust", 0, "Missing project-specific extension OID", "Error for leaf marker OID")
#define SEC_TRUST_ERROR_LeafMarkerOidWithoutValueCheck SecStringWithDefaultValue("Missing project-specific extension OID", "Trust", 0, "Missing project-specific extension OID", "Error for leaf marker OID without value check")
#define SEC_TRUST_ERROR_LeafMarkersProdAndQA        SecStringWithDefaultValue("Missing project-specific extension OID", "Trust", 0, "Missing project-specific extension OID", "Error for leaf marker OID allowing prod or QA")
#define SEC_TRUST_ERROR_BlackListedLeaf             SecStringWithDefaultValue("Certificate is blocked", "Trust", 0, "Certificate is blocked", "Error for blocklisted certificates")
#define SEC_TRUST_ERROR_GrayListedLeaf              SecStringWithDefaultValue("Certificate is listed as untrusted", "Trust", 0, "Certificate is listed as untrusted", "Error for graylisted certificates")
#define SEC_TRUST_ERROR_LeafSPKISHA256              SecStringWithDefaultValue("Public key does not match pinned value", "Trust", 0, "Public key does not match pinned value", "Error for leaf public key pin")
#define SEC_TRUST_ERROR_NotCA                      SecStringWithDefaultValue("Leaf certificate is a CA", "Trust", 0, "Leaf certificate is a CA", "Error for leaf CA")
#define SEC_TRUST_ERROR_IssuerCommonName            SecStringWithDefaultValue("Common Name does not match expected name", "Trust", 0, "Common Name does not match expected name", "Error for issuer common name mismatch")
#define SEC_TRUST_ERROR_BasicConstraints            SecStringWithDefaultValue("Basic constraints are required but missing", "Trust", 0, "Basic constraints are required but missing", "Error for missing basic constraints")
#define SEC_TRUST_ERROR_BasicConstraintsCA          SecStringWithDefaultValue("Non-CA certificate used as a CA", "Trust", 0, "Non-CA certificate used as a CA", "Error for CA basic constraints")
#define SEC_TRUST_ERROR_BasicConstraintsPathLen     SecStringWithDefaultValue("Chain exceeded constrained path length", "Trust", 0, "Chain exceeded constrained path length", "Error for path length basic constraints")
#define SEC_TRUST_ERROR_IntermediateSPKISHA256      SecStringWithDefaultValue("Public key does not match pinned value", "Trust", 0, "Public key does not match pinned value", "Error for intermediate public key pin")
#define SEC_TRUST_ERROR_CAspkiSHA256                SecStringWithDefaultValue("Public key does not match pinned value", "Trust", 0, "Public key does not match pinned value", "Error for CA public key pin")
#define SEC_TRUST_ERROR_IntermediateEKU             SecStringWithDefaultValue("Extended key usage does not match pinned value", "Trust", 0, "Extended key usage does not match pinned value", "Error for intermediate extended key usage pin")
#define SEC_TRUST_ERROR_IntermediateMarkerOid       SecStringWithDefaultValue("Missing issuer-specific extension OID", "Trust", 0, "Missing issuer-specific extension OID", "Error for intermediate marker OID")
#define SEC_TRUST_ERROR_IntermediateMarkerOidWithoutValueCheck SecStringWithDefaultValue("Missing issuer-specific extension OID", "Trust", 0, "Missing issuer-specific extension OID", "Error for intermediate marker OID")
#define SEC_TRUST_ERROR_IntermediateOrganization    SecStringWithDefaultValue("Organization does not match expected name", "Trust", 0, "Organization does not match expected name", "Error for issuer organization mismatch")
#define SEC_TRUST_ERROR_IntermediateCountry         SecStringWithDefaultValue("Country or Region does not match expected name", "Trust", 0, "Country or Region does not match expected name", "Error for issuer country mismatch")
#define SEC_TRUST_ERROR_AnchorSHA256                SecStringWithDefaultValue("Anchor does not match pinned fingerprint", "Trust", 0, "Anchor does not match pinned fingerprint", "Error for anchor SHA-256 fingerprint pin")
#define SEC_TRUST_ERROR_AnchorTrusted               SecStringWithDefaultValue("Root is not trusted", "Trust", 0, "Root is not trusted", "Error for untrusted root")
#define SEC_TRUST_ERROR_MissingIntermediate         SecStringWithDefaultValue("Unable to build chain to root (possible missing intermediate)", "Trust", 0, "Unable to build chain to root (possible missing intermediate)", "Error for missing intermediates")
#define SEC_TRUST_ERROR_AnchorApple                 SecStringWithDefaultValue("Anchor is not an Apple root", "Trust", 0, "Anchor is not an Apple root", "Error for Apple anchor pin")
#define SEC_TRUST_ERROR_NonEmptySubject             SecStringWithDefaultValue("Certificate missing a name", "Trust", 0, "Certificate missing a name", "Error for empty subject name")
#define SEC_TRUST_ERROR_IdLinkage                   SecStringWithDefaultValue("SubjectKeyID/AuthorityKeyID mismatch in chain", "Trust", 0, "SubjectKeyID/AuthorityKeyID mismatch in chain", "Error for bad key ID linkage")
#define SEC_TRUST_ERROR_KeySize                     SecStringWithDefaultValue("Key size is not permitted for this use", "Trust", 0, "Key size is not permitted for this use", "Error for pinned key size")
#define SEC_TRUST_ERROR_SignatureHashAlgorithms     SecStringWithDefaultValue("Signature hash algorithm is not permitted for this use", "Trust", 0, "Signature hash algorithm is not permitted for this use", "Error for pinned hash algorithm")
#define SEC_TRUST_ERROR_CertificatePolicy           SecStringWithDefaultValue("Missing project-specific Certificate Policy OID", "Trust", 0, "Missing project-specific Certificate Policy OID", "Error for certificate policy marker OID")
#define SEC_TRUST_ERROR_ValidRoot                   SecStringWithDefaultValue("Root is not temporally valid", "Trust", 0, "Root is not temporally valid", "Error for root temporal validity")
#define SEC_TRUST_ERROR_CriticalExtensions          SecStringWithDefaultValue("Found unknown critical extensions", "Trust", 0, "Found unknown critical extensions", "Error for unknown critical extensions")
#define SEC_TRUST_ERROR_ChainLength                 SecStringWithDefaultValue("Chain does not match expected path length", "Trust", 0, "Chain does not match expected path length", "Error for pinned chain length")
#define SEC_TRUST_ERROR_BasicCertificateProcessing  SecStringWithDefaultValue("Certificate is not standards compliant", "Trust", 0, "Certificate is not standards compliant", "Error for certificates that violate standards")
#define SEC_TRUST_ERROR_NameConstraints             SecStringWithDefaultValue("Name constraints violated", "Trust", 0, "Name constraints violated", "Error for name constraints")
#define SEC_TRUST_ERROR_PolicyConstraints           SecStringWithDefaultValue("Policy constraints violated", "Trust", 0, "Policy constraints violated", "Error for policy constraints")
#define SEC_TRUST_ERROR_GrayListedKey               SecStringWithDefaultValue("Key is listed as untrusted", "Trust", 0, "Key is listed as untrusted", "Error for graylisted keys")
#define SEC_TRUST_ERROR_BlackListedKey              SecStringWithDefaultValue("Key is blocked", "Trust", 0, "Key is blocked", "Error for blocklisted keys")
#define SEC_TRUST_ERROR_UsageConstraints            SecStringWithDefaultValue("User or administrator set certificate as distrusted", "Trust", 0, "User or administrator set certificate as distrusted", "Error for certificates with deny trust settings")
#define SEC_TRUST_ERROR_SystemTrustedWeakHash       SecStringWithDefaultValue("Signature hash algorithm is not permitted for this use", "Trust", 0, "Signature hash algorithm is not permitted for this use", "Error for system-trust hash algorithm")
#define SEC_TRUST_ERROR_SystemTrustedWeakKey        SecStringWithDefaultValue("Key size is not permitted for this use", "Trust", 0, "Key size is not permitted for this use", "Error for system-trust key size")
#define SEC_TRUST_ERROR_SystemTrustedCTRequired     SecStringWithDefaultValue("Certificate Transparency validation required for this use", "Trust", 0, "Certificate Transparency validation required for this use", "Error for system-trust CT requirement")
#define SEC_TRUST_ERROR_PinningRequired             SecStringWithDefaultValue("Pinning required but not used", "Trust", 0, "Pinning required but not used", "Error for required pinning")
#define SEC_TRUST_ERROR_Revocation                  SecStringWithDefaultValue("Certificate is revoked", "Trust", 0, "Certificate is revoked", "Error for revocation")
#define SEC_TRUST_ERROR_RevocationResponseRequired  SecStringWithDefaultValue("Failed to check revocation", "Trust", 0, "Failed to check revocation", "Error for revocation required")
#define SEC_TRUST_ERROR_CTRequired                  SecStringWithDefaultValue("Certificate Transparency validation required but missing", "Trust", 0, "Certificate Transparency validation required but missing", "Error for missing Certificate Transparency validation")
#define SEC_TRUST_ERROR_NoNetworkAccess             SecStringWithDefaultValue("Unexpected error detail", "Trust", 0, "Unexpected error detail", "Error for unexpected error details")
#define SEC_TRUST_ERROR_ExtendedValidation          SecStringWithDefaultValue("Unexpected error detail", "Trust", 0, "Unexpected error detail", "Error for unexpected error details")
#define SEC_TRUST_ERROR_RevocationOnline            SecStringWithDefaultValue("Unexpected error detail", "Trust", 0, "Unexpected error detail", "Error for unexpected error details")
#define SEC_TRUST_ERROR_RevocationIfTrusted         SecStringWithDefaultValue("Unexpected error detail", "Trust", 0, "Unexpected error detail", "Error for unexpected error details")
#define SEC_TRUST_ERROR_IssuerPolicyConstraints     SecStringWithDefaultValue("Certificate violates issuer policy constraints", "Trust", 0, "Certificate violates issuer policy constraints", "Error for certificates which violate policy constraints set on their issuer")
#define SEC_TRUST_ERROR_IssuerNameConstraints       SecStringWithDefaultValue("Certificate violates issuer name constraints", "Trust", 0, "Certificate violates issuer name constraints", "Error for certificates which violate name constraints set on their issuer")
#define SEC_TRUST_ERROR_ValidityPeriodMaximums      SecStringWithDefaultValue("Certificate exceeds maximum temporal validity period", "Trust", 0, "Certificate exceeds maximum temporal validity period", "Error for certificates that exceed the system's maximum temporal validity")
#define SEC_TRUST_ERROR_ServerAuthEKU               SecStringWithDefaultValue("Extended key usage does not match certificate usage", "Trust", 0, "Extended key usage does not match certificate usage", "Error for extended key usage mismatch")
#define SEC_TRUST_ERROR_UnparseableExtension        SecStringWithDefaultValue("Unable to parse known extension", "Trust", 0, "Unable to parse known extension", "Error for unparseable known extensions")
#define SEC_TRUST_ERROR_NonTlsCTRequired            SecStringWithDefaultValue("Certificate Transparency validation required but missing", "Trust", 0, "Certificate Transparency validation required but missing", "Error for missing Certificate Transparency validation")

__END_DECLS

#endif /* !_SECURITY_SECFRAMEWORKSTRINGS_H_ */
