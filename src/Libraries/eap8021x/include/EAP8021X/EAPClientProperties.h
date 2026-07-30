/*
 * Copyright (c) 2002-2020 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPCLIENTPROPERTIES_H
#define _EAP8021X_EAPCLIENTPROPERTIES_H

#include <TargetConditionals.h>
#include <CoreFoundation/CFString.h>

/*
 * The type of the value corresponding to the following keys are CFString's 
 * unless otherwise noted
 */

/*
 * kEAPClientProp*
 * - properties used to configure the EAPClient, and for the client to report
 *   its configuration needs
 * Note: default values shown in parenthesis (when applicable)
 */

/**
 ** Properties applicable to most protocols
 **/
#define kEAPClientPropUserName	       		CFSTR("UserName")
#define kEAPClientPropUserPassword		CFSTR("UserPassword")
#define kEAPClientPropUserPasswordKeychainItemID CFSTR("UserPasswordKeychainItemID")
#define kEAPClientPropOneTimeUserPassword CFSTR("OneTimeUserPassword") /* boolean (false) */
#define kEAPClientPropAcceptEAPTypes		CFSTR("AcceptEAPTypes") /* array[integer] */
#define kEAPClientPropInnerAcceptEAPTypes	CFSTR("InnerAcceptEAPTypes") /* array[integer] */

/**
 ** Properties for TLS-based authentication (EAP-TLS, EAP-TTLS, PEAP, EAP-FAST)
 **/

/*
 * kEAPClientPropTLSCertificateIsRequired
 * - TLS-based authentication protocol requires a certificate to authenticate
 * - the default value is TRUE for EAP-TLS, FALSE otherwise
 * - allows for two-factor authentication (certificate + name/password)  
 *   when set to TRUE for EAP-TTLS, PEAP, EAP-FAST
 * - allows for zero-factor authentication when set to FALSE for EAP-TLS
 */
#define kEAPClientPropTLSCertificateIsRequired \
	CFSTR("TLSCertificateIsRequired") 		/* boolean */
/*
 * kEAPClientPropTLSTrustedCertificates
 * - which certificates we should trust for this authentication session
 * - may contain root, leaf, or intermediate certificates
 */
#define kEAPClientPropTLSTrustedCertificates \
	CFSTR("TLSTrustedCertificates") 		/* array[data] */

/*
 * kEAPClientPropTLSTrustedServerNames
 * - which server names we should trust for this authentication session
 */
#define kEAPClientPropTLSTrustedServerNames \
	CFSTR("TLSTrustedServerNames") 		/* array[string] */

/*
 * kEAPClientPropProfileID
 * - the profile identifier of the configuration, if the configuration came
 *   from an EAPOLClientProfileRef
 */
#define kEAPClientPropProfileID 	CFSTR("ProfileID")	/* string */

/*
 * kEAPClientPropEAPSIMAKAEncryptedIdentityEnabled
 * - when true tells EAP client to use encrypted Identity for EAP-AKA/EAP-SIM protocols
 */
#define kEAPClientPropEAPSIMAKAEncryptedIdentityEnabled \
	CFSTR("EAPSIMAKAEncryptedIdentityEnabled")

/*
 * kEAPClientPropTLSTrustExceptionsDomain 
 * kEAPClientPropTLSTrustExceptionsID
 * - properties used to locate the appropriate trust exception for the
 *   current authentication session
 */
#define kEAPClientPropTLSTrustExceptionsDomain \
	CFSTR("TLSTrustExceptionsDomain")
#define kEAPClientPropTLSTrustExceptionsID \
	CFSTR("TLSTrustExceptionsID")

/*
 * kEAPClientPropTLSSaveTrustExceptions
 * - tells the client to save trust exceptions for the current server
 *   certificate chain, kEAPClientPropTLSUserTrustProceedCertificateChain
 */
#define kEAPClientPropTLSSaveTrustExceptions \
	CFSTR("TLSSaveTrustExceptions")			/* boolean (false) */

/*
 * kEAPTLSTrustExceptionsDomain*
 *
 * Values for the kEAPClientPropTLSTrustExceptionsDomain property
 *
 * kEAPTrustExceptionsDomainWirelessSSID
 * - used when the desired trust domain is the wireless SSID to which we
 *   are authenticating
 *
 * kEAPTrustExceptionsDomainProfileID
 * - used when the desired trust domain is the UUID of the configuration profile
 *
 * kEAPTLSTrustExceptionsDomainNetworkInterfaceName
 * - used when the desired trust domain is the unique network interface name
 */
#define kEAPTLSTrustExceptionsDomainWirelessSSID \
    	CFSTR("WirelessSSID")
#define kEAPTLSTrustExceptionsDomainProfileID \
    	CFSTR("ProfileID")
#define kEAPTLSTrustExceptionsDomainNetworkInterfaceName \
    	CFSTR("NetworkInterfaceName")

#if TARGET_OS_OSX

/*
 * kEAPClientPropSaveCredentialsOnSuccessfulAuthentication
 * - when set to TRUE and the authentication is successful,
 *   the credentials/identity preference are saved in the keychain
 */
#define kEAPClientPropSaveCredentialsOnSuccessfulAuthentication \
    CFSTR("SaveCredentialsOnSuccessfulAuthentication")

/*
 * kEAPClientPropDisableUserInteraction
 * - when set to TRUE, disables all UI prompts
 */
#define kEAPClientPropDisableUserInteraction	CFSTR("DisableUserInteraction") /* boolean (false) */

#endif /* TARGET_OS_OSX */

#define kEAPClientPropTLSVerifyServerCertificate \
	CFSTR("TLSVerifyServerCertificate") 		/* boolean (true) */
#define kEAPClientPropTLSEnableSessionResumption \
	CFSTR("TLSEnableSessionResumption") 		/* boolean (true) */
#define kEAPClientPropTLSUserTrustProceedCertificateChain \
	CFSTR("TLSUserTrustProceedCertificateChain")	/* array[data] */

/*
 * kEAPClientPropSystemModeUseOpenDirectoryCredentials 
 * - when true, tells the EAP client to use OpenDirectory machine credentials
 *   when running in System mode
 * - supercedes kEAPClientPropSystemModeCredentialsSource
 */
#define kEAPClientPropSystemModeUseOpenDirectoryCredentials \
    CFSTR("SystemModeUseOpenDirectoryCredentials") /* boolean (false) */ 

/*
 * kEAPClientPropSystemModeOpenDirectoryNodeName
 * - if kEAPClientPropSystemModeUseOpenDirectoryCredentials is true,
 *   tells the EAP client to specify a particular node name to retrieve
 *   OpenDirectory machine credentials
 */ 
#define kEAPClientPropSystemModeOpenDirectoryNodeName \
    CFSTR("SystemModeOpenDirectoryNodeName")
/*
 * kEAPClientPropSystemModeCredentialsSource
 * - tells the EAP client to use an alternate source for credentials when
 *   running in System mode
 * - when set to kEAPClientCredentialsSourceActiveDirectory, the EAP client
 *   will attempt to use the machine name/password used by Active Directory;
 *   if those credentials are missing, the authentication will fail
 * - superceded by kEAPClientPropSystemModeUseOpenDirectoryCredentials
 */ 
#define kEAPClientPropSystemModeCredentialsSource	CFSTR("SystemModeCredentialsSource")
#define kEAPClientCredentialsSourceActiveDirectory	CFSTR("ActiveDirectory")

/**
 ** Properties for TTLS
 **/
#define kEAPClientPropTTLSInnerAuthentication	CFSTR("TTLSInnerAuthentication")
#define kEAPTTLSInnerAuthenticationPAP		CFSTR("PAP")
#define kEAPTTLSInnerAuthenticationCHAP		CFSTR("CHAP")
#define kEAPTTLSInnerAuthenticationMSCHAP	CFSTR("MSCHAP")
#define kEAPTTLSInnerAuthenticationMSCHAPv2	CFSTR("MSCHAPv2")
#define kEAPTTLSInnerAuthenticationEAP		CFSTR("EAP")

#define kEAPClientPropNewPassword		CFSTR("NewPassword")
/* for TTLS, PEAP, EAP-FAST: */
#define kEAPClientPropOuterIdentity		CFSTR("OuterIdentity")

/* for TLS: */
#define kEAPClientPropTLSIdentityHandle		    CFSTR("TLSIdentityHandle") /* EAPSecIdentityHandle */
#define kEAPClientPropTLSClientIdentityData	    CFSTR("TLSClientIdentityData") /* persistent reference */
#define kEAPClientPropTLSClientIdentityTrustChain   CFSTR("TLSClientIdentityTrustChain") /* array */
#define kEAPClientPropTLSMinimumVersion		    CFSTR("TLSMinimumVersion") /* string (kEAPTLSVersion*) */
#define kEAPClientPropTLSMaximumVersion		    CFSTR("TLSMaximumVersion") /* string (kEAPTLSVersion*) */
#define kEAPClientPropTLSShareableIdentityInfo	    CFSTR("TLSShareableIdentityInfo") /* Dictionary */

/* acceptable values for TLs version */
#define kEAPTLSVersion1_0			CFSTR("1.0")
#define kEAPTLSVersion1_1			CFSTR("1.1")
#define kEAPTLSVersion1_2			CFSTR("1.2")

/* for EAP-FAST */
#define kEAPClientPropEAPFASTUsePAC		CFSTR("EAPFASTUsePAC") /* boolean (false) */
#define kEAPClientPropEAPFASTProvisionPAC	CFSTR("EAPFASTProvisionPAC") /* boolean (false) */
#define kEAPClientPropEAPFASTProvisionPACAnonymously	CFSTR("EAPFASTProvisionPACAnonymously") /* boolean (false) */

/*
 * for EAP-MSCHAPv2
 *
 * Note: these are only used as an internal communication mechanism between the 
 * outer authentication and EAP-MSCHAPv2.
 */
#define kEAPClientPropEAPMSCHAPv2ServerChallenge CFSTR("EAPMSCHAPv2ServerChallenge") /* data */
#define kEAPClientPropEAPMSCHAPv2ClientChallenge CFSTR("EAPMSCHAPv2ClientChallenge") /* data */

/*
 * Properties supplied by the client as published/additional properties
 */
#define kEAPClientInnerEAPType		CFSTR("InnerEAPType")	/* integer (EAPType) */
#define kEAPClientInnerEAPTypeName	CFSTR("InnerEAPTypeName")
#define kEAPClientPropTLSServerCertificateChain	\
	CFSTR("TLSServerCertificateChain") /* array[data] */
#define kEAPClientPropTLSTrustClientStatus	CFSTR("TLSTrustClientStatus") /* integer (EAPClientStatus) */
#define kEAPClientPropTLSSessionWasResumed \
	CFSTR("TLSSessionWasResumed")	/* boolean */
#define kEAPClientPropTLSNegotiatedCipher \
	CFSTR("TLSNegotiatedCipher")	/* integer (UInt32) */

#define kEAPClientPropEAPFASTPACWasProvisioned	CFSTR("EAPFASTPACWasProvisioned") /* boolean */

/* 
 * Deprecated/unused properties
 */
#define kEAPClientPropIdentity			CFSTR("Identity")
#define kEAPClientPropTLSReplaceTrustedRootCertificates \
	CFSTR("TLSReplaceTrustedRootCertificates")	/* boolean (false) */
#define kEAPClientPropTLSTrustedRootCertificates \
	CFSTR("TLSTrustedRootCertificates") 		/* array[data] */
#define kEAPClientPropTLSAllowAnyRoot \
	CFSTR("TLSAllowAnyRoot") 			/* boolean (false) */

#if TARGET_OS_IPHONE

/*
 * kEAPClientPropTLSAllowTrustExceptions
 * - this property is no longer consulted
 * - if trust is explicitly configured using 
 *   kEAPClientPropTLSTrustedCertificate and/or
 *   kEAPClientPropTLSTrustedServerNames), trust exceptions are not allowed
 */
#define kEAPClientPropTLSAllowTrustExceptions \
	CFSTR("TLSAllowTrustExceptions") 		/* boolean (see above) */

#else /* TARGET_OS_IPHONE */
/*
 * kEAPClientPropTLSAllowTrustDecisions
 * - this property is no longer consulted
 * - if trust is explicitly configured using 
 *   kEAPClientPropTLSTrustedCertificate and/or
 *   kEAPClientPropTLSTrustedServerNames), trust decisions are not allowed
 */
#define kEAPClientPropTLSAllowTrustDecisions \
	CFSTR("TLSAllowTrustDecisions")		/* boolean (see above) */

#endif /* TARGET_OS_IPHONE */

#endif /* _EAP8021X_EAPCLIENTPROPERTIES_H */
