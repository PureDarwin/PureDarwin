/*
 * Copyright (c) 2007-2017 Apple Inc. All Rights Reserved.
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
 * SecPolicy.c - Implementation of various X.509 certificate trust policies
 */

#include <Security/SecPolicyInternal.h>
#include <Security/SecPolicyPriv.h>
#include <AssertMacros.h>
#include <pthread.h>
#include <utilities/debugging.h>
#include <Security/SecInternal.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFTimeZone.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecItem.h>
#include <libDER/oids.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <ipc/securityd_client.h>
#include <os/variant_private.h>
#include <MobileGestalt.h>

#include <utilities/SecInternalReleasePriv.h>

#include <Security/SecBase64.h>

#undef POLICYCHECKMACRO
#define POLICYCHECKMACRO(NAME, TRUSTRESULT, SUBTYPE, LEAFCHECK, PATHCHECK, LEAFONLY, CSSMERR, OSSTATUS) \
    const CFStringRef kSecPolicyCheck##NAME = CFSTR(#NAME);
#include "SecPolicyChecks.list"

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

/********************************************************
 ******************* Feature toggles ********************
 ********************************************************/
/* Option for AnchorApple */
SEC_CONST_DECL (kSecPolicyAppleAnchorIncludeTestRoots, "AnchorAppleTestRoots");

/* options for kSecPolicyCheckLeafMarkersProdAndQA */
SEC_CONST_DECL (kSecPolicyLeafMarkerProd, "ProdMarker");
SEC_CONST_DECL (kSecPolicyLeafMarkerQA, "QAMarker");

/* Revocation toggles */
SEC_CONST_DECL (kSecPolicyCheckRevocationOCSP, "OCSP");
SEC_CONST_DECL (kSecPolicyCheckRevocationCRL, "CRL");
SEC_CONST_DECL (kSecPolicyCheckRevocationAny, "AnyRevocationMethod");

/* Public policy oids. */
#define POLICYMACRO(NAME, OID, ISPUBLIC, INTNAME, IN_NAME, IN_PROPERTIES, FUNCTION) \
const CFStringRef kSecPolicyApple##NAME = CFSTR("1.2.840.113635.100.1."#OID);
#include "SecPolicy.list"
//Some naming exceptions
SEC_CONST_DECL(kSecPolicyMacAppStoreReceipt, "1.2.840.113635.100.1.19")

SEC_CONST_DECL (kSecPolicyOid, "SecPolicyOid");
SEC_CONST_DECL (kSecPolicyName, "SecPolicyName");
SEC_CONST_DECL (kSecPolicyClient, "SecPolicyClient");
SEC_CONST_DECL (kSecPolicyRevocationFlags, "SecPolicyRevocationFlags");
SEC_CONST_DECL (kSecPolicyTeamIdentifier, "SecPolicyTeamIdentifier");
SEC_CONST_DECL (kSecPolicyContext, "SecPolicyContext");
SEC_CONST_DECL (kSecPolicyPolicyName, "SecPolicyPolicyName");
SEC_CONST_DECL (kSecPolicyIntermediateMarkerOid, "SecPolicyIntermediateMarkerOid");
SEC_CONST_DECL (kSecPolicyLeafMarkerOid, "SecPolicyLeafMarkerOid");
SEC_CONST_DECL (kSecPolicyRootDigest, "SecPolicyRootDigest");

SEC_CONST_DECL (kSecPolicyKU_DigitalSignature, "CE_KU_DigitalSignature");
SEC_CONST_DECL (kSecPolicyKU_NonRepudiation, "CE_KU_NonRepudiation");
SEC_CONST_DECL (kSecPolicyKU_KeyEncipherment, "CE_KU_KeyEncipherment");
SEC_CONST_DECL (kSecPolicyKU_DataEncipherment, "CE_KU_DataEncipherment");
SEC_CONST_DECL (kSecPolicyKU_KeyAgreement, "CE_KU_KeyAgreement");
SEC_CONST_DECL (kSecPolicyKU_KeyCertSign, "CE_KU_KeyCertSign");
SEC_CONST_DECL (kSecPolicyKU_CRLSign, "CE_KU_CRLSign");
SEC_CONST_DECL (kSecPolicyKU_EncipherOnly, "CE_KU_EncipherOnly");
SEC_CONST_DECL (kSecPolicyKU_DecipherOnly, "CE_KU_DecipherOnly");

/* Internal policy names */
#undef POLICYMACRO
#define __P_DO_DECLARE_(NAME, INTNAME) static CFStringRef kSecPolicyName##NAME = CFSTR(#INTNAME);
#define __P_DO_DECLARE_E(NAME, INTNAME) static CFStringRef kSecPolicyName##NAME = CFSTR(#INTNAME);
#define __P_DO_DECLARE_P(NAME, INTNAME) const CFStringRef kSecPolicyNameApple##NAME = CFSTR(#INTNAME);
#define __P_DO_DECLARE_I(NAME, INTNAME) const CFStringRef kSecPolicyName##NAME = CFSTR(#INTNAME);
#define POLICYMACRO(NAME, OID, ISPUBLIC, INTNAME, IN_NAME, IN_PROPERTIES, FUNCTION) \
__P_DO_DECLARE_##ISPUBLIC(NAME, INTNAME)
#include "SecPolicy.list"
//Some naming exceptions
static CFStringRef kSecPolicyNameAppleIDSBag = CFSTR("IDSBag");

/* External Policy Names
 * These correspond to the names defined in CertificatePinning.plist
 * in security_certificates */
SEC_CONST_DECL (kSecPolicyNameSSLServer, "sslServer");
SEC_CONST_DECL (kSecPolicyNameSSLClient, "sslClient");
SEC_CONST_DECL (kSecPolicyNameEAPServer, "eapServer");
SEC_CONST_DECL (kSecPolicyNameEAPClient, "eapClient");
SEC_CONST_DECL (kSecPolicyNameIPSecServer, "ipsecServer");
SEC_CONST_DECL (kSecPolicyNameIPSecClient, "ipsecClient");
SEC_CONST_DECL (kSecPolicyNameAppleiCloudSetupService, "iCloudSetup");
SEC_CONST_DECL (kSecPolicyNameAppleMMCSService, "MMCS");
SEC_CONST_DECL (kSecPolicyNameAppleAST2Service, "AST2");
SEC_CONST_DECL (kSecPolicyNameAppleEscrowProxyService, "Escrow");
SEC_CONST_DECL (kSecPolicyNameAppleFMiPService, "FMiP");
SEC_CONST_DECL (kSecPolicyNameAppleHomeKitService, "HomeKit");
SEC_CONST_DECL (kSecPolicyNameAppleAIDCService, "AIDC");
SEC_CONST_DECL (kSecPolicyNameAppleMapsService, "Maps");
SEC_CONST_DECL (kSecPolicyNameAppleHealthProviderService, "HealthProvider");
SEC_CONST_DECL (kSecPolicyNameAppleParsecService, "Parsec");
SEC_CONST_DECL (kSecPolicyNameAppleAMPService, "AMP");
SEC_CONST_DECL (kSecPolicyNameAppleSiriService, "Siri");
SEC_CONST_DECL (kSecPolicyNameAppleHomeAppClipUploadService, "HomeAppClipUploadService");
SEC_CONST_DECL (kSecPolicyNameAppleUpdatesService, "Updates");
SEC_CONST_DECL (kSecPolicyNameApplePushCertPortal, "PushCertPortal");

#define kSecPolicySHA256Size CC_SHA256_DIGEST_LENGTH

// MARK: -
// MARK: SecPolicy
/********************************************************
 ****************** SecPolicy object ********************
 ********************************************************/

static void SecPolicyDestroy(CFTypeRef cf) {
	SecPolicyRef policy = (SecPolicyRef) cf;
	CFRelease(policy->_oid);
	CFReleaseSafe(policy->_name);
	CFRelease(policy->_options);
}

static Boolean SecPolicyCompare(CFTypeRef cf1, CFTypeRef cf2) {
	SecPolicyRef policy1 = (SecPolicyRef) cf1;
	SecPolicyRef policy2 = (SecPolicyRef) cf2;
    if (policy1->_name && policy2->_name) {
        return CFEqual(policy1->_oid, policy2->_oid) &&
            CFEqual(policy1->_name, policy2->_name) &&
            CFEqual(policy1->_options, policy2->_options);
    } else {
        return CFEqual(policy1->_oid, policy2->_oid) &&
            CFEqual(policy1->_options, policy2->_options);
    }
}

static CFHashCode SecPolicyHash(CFTypeRef cf) {
	SecPolicyRef policy = (SecPolicyRef) cf;
    if (policy->_name) {
        return CFHash(policy->_oid) + CFHash(policy->_name) + CFHash(policy->_options);
    } else {
        return CFHash(policy->_oid) + CFHash(policy->_options);
    }
}

static CFStringRef SecPolicyCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
	SecPolicyRef policy = (SecPolicyRef) cf;
    CFMutableStringRef desc = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef typeStr = CFCopyTypeIDDescription(CFGetTypeID(cf));
    CFStringAppendFormat(desc, NULL,
        CFSTR("<%@: oid: %@ name: %@ options %@"), typeStr,
        policy->_oid, (policy->_name) ? policy->_name : CFSTR(""),
                         policy->_options);
    CFRelease(typeStr);
    CFStringAppend(desc, CFSTR(" >"));

    return desc;
}

/* SecPolicy API functions. */
CFGiblisWithHashFor(SecPolicy);

/* AUDIT[securityd](done):
   oid (ok) is a caller provided string, only its cf type has been checked.
   options is a caller provided dictionary, only its cf type has been checked.
 */
SecPolicyRef SecPolicyCreate(CFStringRef oid, CFStringRef name, CFDictionaryRef options) {
	SecPolicyRef result = NULL;

	require(oid, errOut);
	require(options, errOut);
    require(result =
		(SecPolicyRef)_CFRuntimeCreateInstance(kCFAllocatorDefault,
		SecPolicyGetTypeID(),
		sizeof(struct __SecPolicy) - sizeof(CFRuntimeBase), 0), errOut);

	CFRetain(oid);
	result->_oid = oid;
	CFRetainSafe(name);
	result->_name = name;
	CFRetain(options);
	result->_options = options;

errOut:
    return result;
}

#if TARGET_OS_OSX
static void set_ku_from_properties(SecPolicyRef policy, CFDictionaryRef properties);
#endif

SecPolicyRef SecPolicyCreateWithProperties(CFTypeRef policyIdentifier,
	CFDictionaryRef properties) {
	// Creates a policy reference for a given policy object identifier.
	// If policy-specific parameters can be supplied (e.g. hostname),
	// attempt to obtain from input properties dictionary.
	// Returns NULL if the given identifier is unsupported.

	SecPolicyRef policy = NULL;
	CFTypeRef name = NULL;
	CFStringRef teamID = NULL;
	Boolean client = false;
	CFDictionaryRef context = NULL;
	CFStringRef policyName = NULL, intermediateMarkerOid = NULL, leafMarkerOid = NULL;
	CFDataRef rootDigest = NULL;
	require(policyIdentifier && (CFStringGetTypeID() == CFGetTypeID(policyIdentifier)), errOut);

	if (properties) {
		name = CFDictionaryGetValue(properties, kSecPolicyName);
		teamID = CFDictionaryGetValue(properties, kSecPolicyTeamIdentifier);

		CFBooleanRef dictionaryClientValue;
		client = (CFDictionaryGetValueIfPresent(properties, kSecPolicyClient, (const void **)&dictionaryClientValue) &&
				(dictionaryClientValue != NULL) && CFEqual(kCFBooleanTrue, dictionaryClientValue));
		context = CFDictionaryGetValue(properties, kSecPolicyContext);
		policyName = CFDictionaryGetValue(properties, kSecPolicyPolicyName);
		intermediateMarkerOid = CFDictionaryGetValue(properties, kSecPolicyIntermediateMarkerOid);
		leafMarkerOid = CFDictionaryGetValue(properties, kSecPolicyLeafMarkerOid);
		rootDigest = CFDictionaryGetValue(properties, kSecPolicyRootDigest);
	}

	/* only the EAP policy allows a non-string name */
	if (name && !isString(name) && !CFEqual(policyIdentifier, kSecPolicyAppleEAP)) {
		secerror("policy \"%@\" requires a string value for the %@ key", policyIdentifier, kSecPolicyName);
		goto errOut;
	}

    /* What follows are all the exceptional functions that do not match the macro below */
    if (CFEqual(policyIdentifier, kSecPolicyAppleSSL)) {
        policy = SecPolicyCreateSSL(!client, name);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleSMIME)) {
        policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage | kSecAnyEncryptSMIME, name);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleEAP)) {
        CFArrayRef array = NULL;
        if (isString(name)) {
            array = CFArrayCreate(kCFAllocatorDefault, (const void **)&name, 1, &kCFTypeArrayCallBacks);
        } else if (isArray(name)) {
            array = CFArrayCreateCopy(NULL, name);
        }
        policy = SecPolicyCreateEAP(!client, array);
        CFReleaseSafe(array);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleIPsec)) {
        policy = SecPolicyCreateIPSec(!client, name);
    } else if (CFEqual(policyIdentifier, kSecPolicyMacAppStoreReceipt)) {
        policy = SecPolicyCreateMacAppStoreReceipt();
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleRevocation)) {
        policy = SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod);
    } else if (CFEqual(policyIdentifier, kSecPolicyApplePassbookSigning)) {
        policy = SecPolicyCreatePassbookCardSigner(name, teamID);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleAST2DiagnosticsServerAuth)) {
        if (name) {
            policy = SecPolicyCreateAppleAST2Service(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleEscrowProxyServerAuth)) {
        if (name) {
            policy = SecPolicyCreateAppleEscrowProxyService(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleFMiPServerAuth)) {
        if (name) {
            policy = SecPolicyCreateAppleFMiPService(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleMMCService)) {
        if (name) {
            policy = SecPolicyCreateAppleMMCSService(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleGSService)) {
        if (name) {
            policy = SecPolicyCreateAppleGSService(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyApplePPQService)) {
        if (name) {
            policy = SecPolicyCreateApplePPQService(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleGenericApplePinned)) {
        if (policyName) {
            policy = SecPolicyCreateApplePinned(policyName, intermediateMarkerOid, leafMarkerOid);
        } else {
            secerror("policy \"%@\" requires kSecPolicyPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleGenericAppleSSLPinned)) {
        if (policyName) {
            policy = SecPolicyCreateAppleSSLPinned(policyName, name, intermediateMarkerOid, leafMarkerOid);
        } else {
            secerror("policy \"%@\" requires kSecPolicyPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleIDSServiceContext)) {
        if (name) {
            policy = SecPolicyCreateAppleIDSServiceContext(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyApplePushService)) {
        if (name) {
            policy = SecPolicyCreateApplePushService(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleUniqueDeviceIdentifierCertificate)) {
        policy = SecPolicyCreateAppleUniqueDeviceCertificate(rootDigest);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleiCloudSetupServerAuth)) {
        if (name) {
            policy = SecPolicyCreateAppleiCloudSetupService(name, context);
        } else {
            secerror("policy \"%@\" requires kSecPolicyName input", policyIdentifier);
        }
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleBasicAttestationSystem)) {
        policy = SecPolicyCreateAppleBasicAttestationSystem(rootDigest);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleBasicAttestationUser)) {
        policy = SecPolicyCreateAppleBasicAttestationUser(rootDigest);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleComponentCertificate)) {
        policy = SecPolicyCreateAppleComponentCertificate(rootDigest);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleAggregateMetricTransparency)) {
        policy = SecPolicyCreateAggregateMetricTransparency(!client);
    } else if (CFEqual(policyIdentifier, kSecPolicyAppleAggregateMetricEncryption)) {
        policy = SecPolicyCreateAggregateMetricEncryption(!client);
    } else if (CFEqual(policyIdentifier, kSecPolicyApplePayModelSigning)) {
        policy = SecPolicyCreateApplePayModelSigning(true);
    }
    /* For a couple of common patterns we use the macro, but some of the
     * policies are deprecated (or not yet available), so we need to ignore the warning. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunguarded-availability"
#define _P_OPTION_
#define _P_OPTION_N name
#define _P_PROPERTIES_(NAME, IN_NAME, FUNCTION)
#define _P_PROPERTIES_Y(NAME, IN_NAME, FUNCTION)  else if (CFEqual(policyIdentifier, kSecPolicyApple##NAME)) { \
    policy = SecPolicyCreate##FUNCTION(_P_OPTION_##IN_NAME); \
}
#undef POLICYMACRO
#define POLICYMACRO(NAME, OID, ISPUBLIC, INTNAME, IN_NAME, IN_PROPERTIES, FUNCTION) \
_P_PROPERTIES_##IN_PROPERTIES(NAME, IN_NAME, FUNCTION)
#include "SecPolicy.list"
	else {
		secerror("ERROR: policy \"%@\" is unsupported", policyIdentifier);
	}
#pragma clang diagnostic pop // ignored "-Wdeprecated-declarations"

    if (!policy) {
        return NULL;
    }

#if TARGET_OS_OSX
    set_ku_from_properties(policy, properties);
#endif

    if (policyName) {
        SecPolicySetName(policy, policyName);
    }

errOut:
	return policy;
}

CFDictionaryRef SecPolicyCopyProperties(SecPolicyRef policyRef) {
	// Builds and returns a dictionary which the caller must release.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // After introducing nullability annotations, policyRef is supposed to be nonnull, suppress the warning
	if (!policyRef) return NULL;
#pragma clang diagnostic pop
	CFMutableDictionaryRef properties = CFDictionaryCreateMutable(NULL, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // 'properties' is nonnull in reality suppress the warning
	if (!properties) return NULL;
#pragma clang diagnostic pop
	CFStringRef oid = (CFStringRef) CFRetain(policyRef->_oid);
	CFTypeRef nameKey = NULL;

	// Determine name key
    if (policyRef->_options) {
        if (CFDictionaryContainsKey(policyRef->_options, kSecPolicyCheckSSLHostname)) {
            nameKey = kSecPolicyCheckSSLHostname;
        } else if (CFDictionaryContainsKey(policyRef->_options, kSecPolicyCheckEAPTrustedServerNames)) {
            nameKey = kSecPolicyCheckEAPTrustedServerNames;
        } else if (CFDictionaryContainsKey(policyRef->_options, kSecPolicyCheckEmail)) {
            nameKey = kSecPolicyCheckEmail;
        }
    }

	// Set kSecPolicyOid
	CFDictionarySetValue(properties, (const void *)kSecPolicyOid,
		(const void *)oid);

	// Set kSecPolicyName if we have one
	if (nameKey && policyRef->_options) {
		CFTypeRef name = (CFTypeRef) CFDictionaryGetValue(policyRef->_options,
			nameKey);
		if (name) {
			CFDictionarySetValue(properties, (const void *)kSecPolicyName,
				(const void *)name);
		}
	}

	// Set kSecPolicyClient
    CFStringRef policyName = (CFStringRef) CFRetainSafe(policyRef->_name);
	if (policyName && (CFEqual(policyName, kSecPolicyNameSSLClient) ||
		CFEqual(policyName, kSecPolicyNameIPSecClient) ||
		CFEqual(policyName, kSecPolicyNameEAPClient))) {
		CFDictionarySetValue(properties, (const void *)kSecPolicyClient,
			(const void *)kCFBooleanTrue);
	}

	CFRelease(oid);
	return properties;
}

static void SecPolicySetOid(SecPolicyRef policy, CFStringRef oid) {
	if (!policy || !oid) return;
	CFStringRef temp = policy->_oid;
	CFRetain(oid);
	policy->_oid = oid;
	CFReleaseSafe(temp);
}

void SecPolicySetName(SecPolicyRef policy, CFStringRef policyName) {
    if (!policy || !policyName) return;
    CFStringRef temp = policy->_name;
    CFRetain(policyName);
    policy->_name= policyName;
    CFReleaseSafe(temp);
}

CFStringRef SecPolicyGetOidString(SecPolicyRef policy) {
	return policy->_oid;
}

CFStringRef SecPolicyGetName(SecPolicyRef policy) {
	return policy->_name;
}

CFDictionaryRef SecPolicyGetOptions(SecPolicyRef policy) {
	return policy->_options;
}

void SecPolicySetOptionsValue(SecPolicyRef policy, CFStringRef key, CFTypeRef value) {
	if (!policy || !key) return;
	CFMutableDictionaryRef options = (CFMutableDictionaryRef) policy->_options;
	if (!options) {
		options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!options) return;
		policy->_options = options;
	}
	CFDictionarySetValue(options, key, value);
}

/* Local forward declaration */
static void set_ssl_ekus(CFMutableDictionaryRef options, bool server);

#if TARGET_OS_IPHONE
// this is declared as NA for iPhone in SecPolicy.h, so declare here
OSStatus SecPolicySetProperties(SecPolicyRef policyRef, CFDictionaryRef properties);
#endif

OSStatus SecPolicySetProperties(SecPolicyRef policyRef, CFDictionaryRef properties) {
	// Set policy options based on the provided dictionary keys.

	if (!(policyRef && properties && (CFDictionaryGetTypeID() == CFGetTypeID(properties)))) {
		return errSecParam;
	}
	CFStringRef oid = (CFStringRef) CFRetain(policyRef->_oid);
	OSStatus result = errSecSuccess;

	// kSecPolicyName
	CFTypeRef name = NULL;
	if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyName,
		(const void **)&name) && name) {
		CFTypeID typeID = CFGetTypeID(name);
		if (CFEqual(oid, kSecPolicyAppleSSL) ||
			CFEqual(oid, kSecPolicyAppleIPsec)) {
			if (CFStringGetTypeID() == typeID) {
				SecPolicySetOptionsValue(policyRef, kSecPolicyCheckSSLHostname, name);
			}
			else result = errSecParam;
		}
		else if (CFEqual(oid, kSecPolicyAppleEAP)) {
			if ((CFStringGetTypeID() == typeID) ||
				(CFArrayGetTypeID() == typeID)) {
				SecPolicySetOptionsValue(policyRef, kSecPolicyCheckEAPTrustedServerNames, name);
			}
			else result = errSecParam;
		}
		else if (CFEqual(oid, kSecPolicyAppleSMIME)) {
			if (CFStringGetTypeID() == typeID) {
				SecPolicySetOptionsValue(policyRef, kSecPolicyCheckEmail, name);
			}
			else result = errSecParam;
		}
	}

	// kSecPolicyClient
	CFTypeRef client = NULL;
	if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyClient,
		(const void **)&client) && client) {
		if (!(CFBooleanGetTypeID() == CFGetTypeID(client))) {
			result = errSecParam;
		}
		else if (CFEqual(client, kCFBooleanTrue)) {
			if (CFEqual(oid, kSecPolicyAppleSSL)) {
				SecPolicySetName(policyRef, kSecPolicyNameSSLClient);
				/* Set EKU checks for clients */
				CFMutableDictionaryRef newOptions = CFDictionaryCreateMutableCopy(NULL, 0, policyRef->_options);
				set_ssl_ekus(newOptions, false);
				CFReleaseSafe(policyRef->_options);
				policyRef->_options = newOptions;
			}
			else if (CFEqual(oid, kSecPolicyAppleIPsec)) {
				SecPolicySetName(policyRef, kSecPolicyNameIPSecClient);
			}
			else if (CFEqual(oid, kSecPolicyNameEAPServer)) {
				SecPolicySetName(policyRef, kSecPolicyNameEAPClient);
				/* Set EKU checks for clients */
				CFMutableDictionaryRef newOptions = CFDictionaryCreateMutableCopy(NULL, 0, policyRef->_options);
				set_ssl_ekus(newOptions, false);
				CFReleaseSafe(policyRef->_options);
				policyRef->_options = newOptions;
			}
		}
		else {
			if (CFEqual(oid, kSecPolicyAppleSSL)) {
				SecPolicySetName(policyRef, kSecPolicyNameSSLServer);
				/* Set EKU checks for servers */
				CFMutableDictionaryRef newOptions = CFDictionaryCreateMutableCopy(NULL, 0, policyRef->_options);
				set_ssl_ekus(newOptions, true);
				CFReleaseSafe(policyRef->_options);
				policyRef->_options = newOptions;
			}
			else if (CFEqual(oid, kSecPolicyAppleIPsec)) {
				SecPolicySetName(policyRef, kSecPolicyNameIPSecServer);
			}
			else if (CFEqual(oid, kSecPolicyAppleEAP)) {
				SecPolicySetName(policyRef, kSecPolicyNameEAPServer);
				/* Set EKU checks for servers */
				CFMutableDictionaryRef newOptions = CFDictionaryCreateMutableCopy(NULL, 0, policyRef->_options);
				set_ssl_ekus(newOptions, true);
				CFReleaseSafe(policyRef->_options);
				policyRef->_options = newOptions;
			}
		}
	}

#if TARGET_OS_OSX
    set_ku_from_properties(policyRef, properties);
#endif
	CFRelease(oid);
	return result;
}

static xpc_object_t copy_xpc_policy_object(SecPolicyRef policy);
static bool append_policy_to_xpc_array(SecPolicyRef policy, xpc_object_t xpc_policies);
extern xpc_object_t copy_xpc_policies_array(CFArrayRef policies);
extern OSStatus validate_array_of_items(CFArrayRef array, CFStringRef arrayItemType, CFTypeID itemTypeID, bool required);

static xpc_object_t copy_xpc_policy_object(SecPolicyRef policy) {
    xpc_object_t xpc_policy = NULL;
    xpc_object_t data[2] = { NULL, NULL };
	if (policy->_oid && (CFGetTypeID(policy->_oid) == CFStringGetTypeID()) &&
        policy->_name && (CFGetTypeID(policy->_name) == CFStringGetTypeID())) {
        /* These should really be different elements of the xpc array. But
         * SecPolicyCreateWithXPCObject previously checked the size via ==, which prevents
         * us from appending new information while maintaining backward compatibility.
         * Doing this makes the builders happy. */
        CFMutableStringRef oidAndName = NULL;
        oidAndName = CFStringCreateMutableCopy(NULL, 0, policy->_oid);
        if (oidAndName) {
            CFStringAppend(oidAndName, CFSTR("++"));
            CFStringAppend(oidAndName, policy->_name);
            data[0] = _CFXPCCreateXPCObjectFromCFObject(oidAndName);
            CFReleaseNull(oidAndName);
        } else {
            data[0] = _CFXPCCreateXPCObjectFromCFObject(policy->_oid);
        }
    } else if (policy->_oid && (CFGetTypeID(policy->_oid) == CFStringGetTypeID())) {
        data[0] = _CFXPCCreateXPCObjectFromCFObject(policy->_oid);
	} else {
		secerror("policy 0x%lX has no _oid", (uintptr_t)policy);
	}
	if (policy->_options && (CFGetTypeID(policy->_options) == CFDictionaryGetTypeID())) {
		data[1] = _CFXPCCreateXPCObjectFromCFObject(policy->_options);
	} else {
		secerror("policy 0x%lX has no _options", (uintptr_t)policy);
	}
	xpc_policy = xpc_array_create(data, array_size(data));
    if (data[0]) xpc_release(data[0]);
    if (data[1]) xpc_release(data[1]);
    return xpc_policy;
}

static bool append_policy_to_xpc_array(SecPolicyRef policy, xpc_object_t xpc_policies) {
    if (!policy) {
        return true; // NOOP
	}
    xpc_object_t xpc_policy = copy_xpc_policy_object(policy);
    if (!xpc_policy) {
        return false;
	}
    xpc_array_append_value(xpc_policies, xpc_policy);
    xpc_release(xpc_policy);
    return true;
}

xpc_object_t copy_xpc_policies_array(CFArrayRef policies) {
	xpc_object_t xpc_policies = xpc_array_create(NULL, 0);
	if (!xpc_policies) {
		return NULL;
	}
	validate_array_of_items(policies, CFSTR("policy"), SecPolicyGetTypeID(), true);
	CFIndex ix, count = CFArrayGetCount(policies);
	for (ix = 0; ix < count; ++ix) {
		SecPolicyRef policy = (SecPolicyRef) CFArrayGetValueAtIndex(policies, ix);
    #if SECTRUST_VERBOSE_DEBUG
		CFDictionaryRef props = SecPolicyCopyProperties(policy);
		secerror("idx=%d of %d; policy=0x%lX properties=%@", (int)ix, (int)count, (uintptr_t)policy, props);
		CFReleaseSafe(props);
    #endif
		if (!append_policy_to_xpc_array(policy, xpc_policies)) {
			xpc_release(xpc_policies);
			xpc_policies = NULL;
			break;
		}
	}
	return xpc_policies;
}

static xpc_object_t SecPolicyCopyXPCObject(SecPolicyRef policy, CFErrorRef *error) {
    xpc_object_t xpc_policy = NULL;
    xpc_object_t data[2] = {};
    CFMutableStringRef oidAndName = NULL;
    oidAndName = CFStringCreateMutableCopy(NULL, 0, policy->_oid);
    if (oidAndName) {
        if (policy->_name) {
            CFStringAppend(oidAndName, CFSTR("++"));
            CFStringAppend(oidAndName, policy->_name);
        }

        require_action_quiet(data[0] = _CFXPCCreateXPCObjectFromCFObject(oidAndName), exit,
                             SecError(errSecParam, error,
                                      CFSTR("failed to create xpc_object from policy oid and name")));
    } else {
        require_action_quiet(data[0] = _CFXPCCreateXPCObjectFromCFObject(policy->_oid), exit,
                             SecError(errSecParam, error, CFSTR("failed to create xpc_object from policy oid")));
    }
    require_action_quiet(data[1] = _CFXPCCreateXPCObjectFromCFObject(policy->_options), exit,
                         SecError(errSecParam, error, CFSTR("failed to create xpc_object from policy options")));
    require_action_quiet(xpc_policy = xpc_array_create(data, array_size(data)), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create xpc_array for policy")));

exit:
    if (data[0]) xpc_release(data[0]);
    if (data[1]) xpc_release(data[1]);
    CFReleaseNull(oidAndName);
    return xpc_policy;
}

static bool SecPolicyAppendToXPCArray(SecPolicyRef policy, xpc_object_t policies, CFErrorRef *error) {
    if (!policy)
        return true; // NOOP

    xpc_object_t xpc_policy = SecPolicyCopyXPCObject(policy, error);
    if (!xpc_policy)
        return false;

    xpc_array_append_value(policies, xpc_policy);
    xpc_release(xpc_policy);
    return true;
}

xpc_object_t SecPolicyArrayCopyXPCArray(CFArrayRef policies, CFErrorRef *error) {
    xpc_object_t xpc_policies;
    require_action_quiet(xpc_policies = xpc_array_create(NULL, 0), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create xpc_array")));
    CFIndex ix, count = CFArrayGetCount(policies);
    for (ix = 0; ix < count; ++ix) {
        if (!SecPolicyAppendToXPCArray((SecPolicyRef)CFArrayGetValueAtIndex(policies, ix), xpc_policies, error)) {
            xpc_release(xpc_policies);
            return NULL;
        }
    }
exit:
    return xpc_policies;
}

static OSStatus parseOidAndName(CFStringRef oidAndName, CFStringRef *oid, CFStringRef *name) {
    OSStatus result = errSecSuccess;
    CFStringRef partial = NULL;

    CFRange delimiter = CFStringFind(oidAndName, CFSTR("++"), 0);
    if (delimiter.length != 2) {
        return errSecParam;
    }

    /* get first half: oid */
    partial = CFStringCreateWithSubstring(NULL, oidAndName, CFRangeMake(0, delimiter.location));
    if (oid) { *oid = CFRetainSafe(partial); }
    CFReleaseNull(partial);

    /* get second half: name */
    if (delimiter.location + 2 >= CFStringGetLength(oidAndName)) {
        return errSecSuccess;  // name is optional
    }
    CFRange nameRange = CFRangeMake(delimiter.location+2,
                                    CFStringGetLength(oidAndName) - delimiter.location - 2);
    partial = CFStringCreateWithSubstring(NULL, oidAndName, nameRange);
    if (name) { *name = CFRetainSafe(partial); }
    CFReleaseNull(partial);
    return result;
}

static SecPolicyRef SecPolicyCreateWithXPCObject(xpc_object_t xpc_policy, CFErrorRef *error) {
    SecPolicyRef policy = NULL;
    CFTypeRef oidAndName = NULL;
    CFStringRef oid = NULL;
    CFStringRef name = NULL;
    CFTypeRef options = NULL;

    require_action_quiet(xpc_policy, exit, SecError(errSecParam, error, CFSTR("policy xpc value is NULL")));
    require_action_quiet(xpc_get_type(xpc_policy) == XPC_TYPE_ARRAY, exit, SecError(errSecDecode, error, CFSTR("policy xpc value is not an array")));
    require_action_quiet(xpc_array_get_count(xpc_policy) >= 2, exit, SecError(errSecDecode, error, CFSTR("policy xpc array count < 2")));
    oidAndName = _CFXPCCreateCFObjectFromXPCObject(xpc_array_get_value(xpc_policy, 0));
    require_action_quiet(isString(oidAndName), exit,
                         SecError(errSecParam, error, CFSTR("failed to convert xpc policy[0]=%@ to CFString"), oidAndName));
    options = _CFXPCCreateCFObjectFromXPCObject(xpc_array_get_value(xpc_policy, 1));
    require_action_quiet(isDictionary(options), exit,
                         SecError(errSecParam, error, CFSTR("failed to convert xpc policy[1]=%@ to CFDictionary"), options));
    require_noerr_action_quiet(parseOidAndName(oidAndName, &oid, &name), exit,
                               SecError(errSecParam, error, CFSTR("failed to convert combined %@ to name and oid"), oidAndName));
    require_action_quiet(policy = SecPolicyCreate(oid, name, options), exit,
                         SecError(errSecDecode, error, CFSTR("Failed to create policy")));

exit:
    CFReleaseSafe(oidAndName);
    CFReleaseSafe(oid);
    CFReleaseSafe(name);
    CFReleaseSafe(options);
    return policy;
}

CFArrayRef SecPolicyXPCArrayCopyArray(xpc_object_t xpc_policies, CFErrorRef *error) {
    CFMutableArrayRef policies = NULL;
    require_action_quiet(xpc_get_type(xpc_policies) == XPC_TYPE_ARRAY, exit,
                         SecError(errSecParam, error, CFSTR("policies xpc value is not an array")));
    size_t count = xpc_array_get_count(xpc_policies);
    require_action_quiet(policies = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create CFArray of capacity %zu"), count));

    size_t ix;
    for (ix = 0; ix < count; ++ix) {
        SecPolicyRef policy = SecPolicyCreateWithXPCObject(xpc_array_get_value(xpc_policies, ix), error);
        if (!policy) {
            CFRelease(policies);
            return NULL;
        }
        CFArraySetValueAtIndex(policies, ix, policy);
        CFRelease(policy);
    }

exit:
    return policies;

}

static SEC_CONST_DECL (kSecPolicyOptions, "policyOptions");

static SecPolicyRef SecPolicyCreateWithDictionary(CFDictionaryRef dict) {
    SecPolicyRef policy = NULL;
    CFStringRef oid = (CFStringRef)CFDictionaryGetValue(dict, kSecPolicyOid);
    require_quiet(isString(oid), errOut);
    CFDictionaryRef options = (CFDictionaryRef)CFDictionaryGetValue(dict, kSecPolicyOptions);
    require_quiet(isDictionary(options), errOut);
    CFStringRef name = (CFStringRef)CFDictionaryGetValue(dict, kSecPolicyPolicyName);
    policy = SecPolicyCreate(oid, name, options);
errOut:
    return policy;
}

static void deserializePolicy(const void *value, void *context) {
    CFDictionaryRef policyDict = (CFDictionaryRef)value;
    if (isDictionary(policyDict)) {
        CFTypeRef deserializedPolicy = SecPolicyCreateWithDictionary(policyDict);
        if (deserializedPolicy) {
            CFArrayAppendValue((CFMutableArrayRef)context, deserializedPolicy);
            CFRelease(deserializedPolicy);
        }
    }
}

CFArrayRef SecPolicyArrayCreateDeserialized(CFArrayRef serializedPolicies) {
    CFMutableArrayRef result = NULL;
    require_quiet(isArray(serializedPolicies), errOut);
    CFIndex count = CFArrayGetCount(serializedPolicies);
    result = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    CFRange all_policies = { 0, count };
    CFArrayApplyFunction(serializedPolicies, all_policies, deserializePolicy, result);
errOut:
    return result;
}

static CFDictionaryRef SecPolicyCreateDictionary(SecPolicyRef policy) {
    CFMutableDictionaryRef dict = NULL;
    dict = CFDictionaryCreateMutable(NULL, 3, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(dict, kSecPolicyOid, policy->_oid);
    CFDictionaryAddValue(dict, kSecPolicyOptions, policy->_options);
    if (policy->_name) {
        CFDictionaryAddValue(dict, kSecPolicyPolicyName, policy->_name);
    }
    return dict;
}

static void serializePolicy(const void *value, void *context) {
    SecPolicyRef policy = (SecPolicyRef)value;
    if (policy && SecPolicyGetTypeID() == CFGetTypeID(policy)) {
        CFDictionaryRef serializedPolicy = SecPolicyCreateDictionary(policy);
        if (serializedPolicy) {
            CFArrayAppendValue((CFMutableArrayRef)context, serializedPolicy);
            CFRelease(serializedPolicy);
        }
    }
}

CFArrayRef SecPolicyArrayCreateSerialized(CFArrayRef policies) {
    CFMutableArrayRef result = NULL;
    require_quiet(isArray(policies), errOut);
    CFIndex count = CFArrayGetCount(policies);
    result = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    CFRange all_policies = { 0, count};
    CFArrayApplyFunction(policies, all_policies, serializePolicy, result);
errOut:
    return result;
}

static void add_element(CFMutableDictionaryRef options, CFStringRef key,
    CFTypeRef value) {
    CFTypeRef old_value = CFDictionaryGetValue(options, key);
    if (old_value) {
        CFMutableArrayRef array;
        if (CFGetTypeID(old_value) == CFArrayGetTypeID()) {
            array = (CFMutableArrayRef)old_value;
        } else {
            array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                       &kCFTypeArrayCallBacks);
            CFArrayAppendValue(array, old_value);
            CFDictionarySetValue(options, key, array);
            CFRelease(array);
        }
        CFArrayAppendValue(array, value);
    } else {
        CFDictionaryAddValue(options, key, value);
    }
}

static void add_eku(CFMutableDictionaryRef options, const DERItem *ekuOid) {
    CFDataRef eku = CFDataCreate(kCFAllocatorDefault,
                                 ekuOid ? ekuOid->data : NULL,
                                 ekuOid ? ekuOid->length : 0);
    if (eku) {
        add_element(options, kSecPolicyCheckExtendedKeyUsage, eku);
        CFRelease(eku);
    }
}

static void add_eku_string(CFMutableDictionaryRef options, CFStringRef ekuOid) {
    if (ekuOid) {
        add_element(options, kSecPolicyCheckExtendedKeyUsage, ekuOid);
    }
}

static void set_ssl_ekus(CFMutableDictionaryRef options, bool server) {
    CFDictionaryRemoveValue(options, kSecPolicyCheckExtendedKeyUsage);

    /* If server and EKU ext present then EKU ext should contain one of
     ServerAuth or ExtendedKeyUsageAny or NetscapeSGC or MicrosoftSGC.
     else if !server and EKU ext present then EKU ext should contain one of
     ClientAuth or ExtendedKeyUsageAny. */

    /* We always allow certificates that specify oidAnyExtendedKeyUsage. */
    add_eku(options, NULL); /* eku extension is optional */
    add_eku(options, &oidAnyExtendedKeyUsage);
    if (server) {
        add_eku(options, &oidExtendedKeyUsageServerAuth);
        add_eku(options, &oidExtendedKeyUsageMicrosoftSGC);
        add_eku(options, &oidExtendedKeyUsageNetscapeSGC);
    } else {
        add_eku(options, &oidExtendedKeyUsageClientAuth);
    }
}

static void add_ku(CFMutableDictionaryRef options, SecKeyUsage keyUsage) {
    SInt32 dku = keyUsage;
    CFNumberRef ku = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
        &dku);
    if (ku) {
        add_element(options, kSecPolicyCheckKeyUsage, ku);
        CFRelease(ku);
    }
}

static void add_ku_report(CFMutableDictionaryRef options, SecKeyUsage keyUsage) {
    SInt32 dku = keyUsage;
    CFNumberRef ku = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
        &dku);
    if (ku) {
        add_element(options, kSecPolicyCheckKeyUsageReportOnly, ku);
        CFRelease(ku);
    }
}

#if TARGET_OS_OSX
static void set_ku_from_properties(SecPolicyRef policy, CFDictionaryRef properties) {
    if (!policy || !properties) {
        return;
    }

    CFStringRef keyNames[] = { kSecPolicyKU_DigitalSignature, kSecPolicyKU_NonRepudiation, kSecPolicyKU_KeyEncipherment, kSecPolicyKU_DataEncipherment,
        kSecPolicyKU_KeyAgreement, kSecPolicyKU_KeyCertSign, kSecPolicyKU_CRLSign, kSecPolicyKU_EncipherOnly, kSecPolicyKU_DecipherOnly };

    uint32_t keyUsageValues[] = { kSecKeyUsageDigitalSignature, kSecKeyUsageNonRepudiation, kSecKeyUsageKeyEncipherment, kSecKeyUsageDataEncipherment,
        kSecKeyUsageKeyAgreement, kSecKeyUsageKeyCertSign, kSecKeyUsageCRLSign, kSecKeyUsageEncipherOnly, kSecKeyUsageDecipherOnly };

    bool haveKeyUsage = false;
    CFTypeRef keyUsageBoolean;
    for (uint32_t i = 0; i < sizeof(keyNames) / sizeof(CFStringRef); ++i) {
        if (CFDictionaryGetValueIfPresent(properties, keyNames[i], (const void**)&keyUsageBoolean)) {
            if (CFEqual(keyUsageBoolean, kCFBooleanTrue)) {
                haveKeyUsage = true;
                break;
            }
        }
    }

    if (!haveKeyUsage) {
        return;
    }

    CFMutableDictionaryRef options = (CFMutableDictionaryRef) policy->_options;
    if (!options) {
        options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!options) return;
        policy->_options = options;
    } else {
        CFDictionaryRemoveValue(options, kSecPolicyCheckKeyUsage);
    }

    for (uint32_t i = 0; i < sizeof(keyNames) / sizeof(CFStringRef); ++i) {
        if (CFDictionaryGetValueIfPresent(properties, keyNames[i], (const void**)&keyUsageBoolean)) {
            if (CFEqual(keyUsageBoolean, kCFBooleanTrue)) {
                add_ku(options, keyUsageValues[i]);
            }
        }
    }
}
#endif

static void add_oid(CFMutableDictionaryRef options, CFStringRef policy_key, const DERItem *oid) {
    CFDataRef oid_data = CFDataCreate(kCFAllocatorDefault,
                                 oid ? oid->data : NULL,
                                 oid ? oid->length : 0);
    if (oid_data) {
        add_element(options, policy_key, oid_data);
        CFRelease(oid_data);
    }
}

static void add_leaf_marker_value(CFMutableDictionaryRef options, const DERItem *markerOid, CFStringRef string_value) {

    CFTypeRef policyData = NULL;

    if (NULL == string_value) {
        policyData = CFDataCreate(kCFAllocatorDefault,
                                markerOid ? markerOid->data : NULL,
                                markerOid ? markerOid->length : 0);
    } else {
        CFStringRef oid_as_string = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, markerOid);

        const void *key[1]   = { oid_as_string };
        const void *value[1] = { string_value };
        policyData = CFDictionaryCreate(kCFAllocatorDefault,
                                        key, value, 1,
                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFReleaseNull(oid_as_string);
    }

    add_element(options, kSecPolicyCheckLeafMarkerOid, policyData);

    CFReleaseNull(policyData);

}

static void add_leaf_marker(CFMutableDictionaryRef options, const DERItem *markerOid) {
    add_leaf_marker_value(options, markerOid, NULL);
}

static void add_leaf_marker_value_string(CFMutableDictionaryRef options, CFStringRef markerOid, CFStringRef string_value) {
    if (NULL == string_value) {
        add_element(options, kSecPolicyCheckLeafMarkerOid, markerOid);
    } else {
        CFDictionaryRef policyData = NULL;
        const void *key[1]   = { markerOid };
        const void *value[1] = { string_value };
        policyData = CFDictionaryCreate(kCFAllocatorDefault,
                                        key, value, 1,
                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        add_element(options, kSecPolicyCheckLeafMarkerOid, policyData);

        CFReleaseNull(policyData);
    }
}

static void add_leaf_marker_string(CFMutableDictionaryRef options, CFStringRef markerOid) {
    add_leaf_marker_value_string(options, markerOid, NULL);
}

static void add_leaf_prod_qa_element(CFMutableDictionaryRef options, CFTypeRef prodValue, CFTypeRef qaValue)
{
    CFMutableDictionaryRef prodAndQADictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionaryRef old_value = CFDictionaryGetValue(options, kSecPolicyCheckLeafMarkersProdAndQA);
    if (old_value) {
        CFMutableArrayRef prodArray = NULL, qaArray = NULL;
        CFTypeRef old_prod_value = CFDictionaryGetValue(old_value, kSecPolicyLeafMarkerProd);
        CFTypeRef old_qa_value = CFDictionaryGetValue(old_value, kSecPolicyLeafMarkerQA);
        if (isArray(old_prod_value) && isArray(old_qa_value)) {
            prodArray = (CFMutableArrayRef)CFRetainSafe(old_prod_value);
            qaArray = (CFMutableArrayRef)CFRetainSafe(old_qa_value);
        } else {
            prodArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            qaArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            CFArrayAppendValue(prodArray, old_prod_value);
            CFArrayAppendValue(qaArray, old_qa_value);
        }
        CFArrayAppendValue(prodArray, prodValue);
        CFArrayAppendValue(qaArray, qaValue);
        CFDictionaryAddValue(prodAndQADictionary, kSecPolicyLeafMarkerProd, prodArray);
        CFDictionaryAddValue(prodAndQADictionary, kSecPolicyLeafMarkerQA, qaArray);
        CFReleaseNull(prodArray);
        CFReleaseNull(qaArray);

    } else {
        CFDictionaryAddValue(prodAndQADictionary, kSecPolicyLeafMarkerProd, prodValue);
        CFDictionaryAddValue(prodAndQADictionary, kSecPolicyLeafMarkerQA, qaValue);
    }
    CFDictionarySetValue(options, kSecPolicyCheckLeafMarkersProdAndQA, prodAndQADictionary);
    CFReleaseNull(prodAndQADictionary);

}

static void add_leaf_prod_qa_markers(CFMutableDictionaryRef options, const DERItem *prodMarkerOid, const DERItem *qaMarkerOid)
{
    CFDataRef prodData = NULL, qaData = NULL;
    prodData = CFDataCreate(NULL, prodMarkerOid ? prodMarkerOid->data : NULL,
                            prodMarkerOid ? prodMarkerOid->length : 0);
    qaData = CFDataCreate(NULL, qaMarkerOid ? qaMarkerOid->data : NULL,
                          qaMarkerOid ? qaMarkerOid->length : 0);
    add_leaf_prod_qa_element(options, prodData, qaData);
    CFReleaseNull(prodData);
    CFReleaseNull(qaData);
}

static void add_leaf_prod_qa_markers_string(CFMutableDictionaryRef options, CFStringRef prodMarkerOid, CFStringRef qaMarkerOid)
{
    add_leaf_prod_qa_element(options, prodMarkerOid, qaMarkerOid);
}

static void add_leaf_prod_qa_markers_value_string(CFMutableDictionaryRef options,
                                                  CFStringRef prodMarkerOid, CFStringRef prod_value,
                                                  CFStringRef qaMarkerOid, CFStringRef qa_value)
{
    if (!prod_value && !qa_value) {
        add_leaf_prod_qa_element(options, prodMarkerOid, qaMarkerOid);
    } else {
        CFDictionaryRef prodData = NULL, qaData = NULL;
        const void *prodKey[1] = { prodMarkerOid }, *qaKey[1] = { qaMarkerOid };
        const void *prodValue[1] = { prod_value }, *qaValue[1] = { qa_value };
        prodData = CFDictionaryCreate(NULL, prodKey, prodValue, 1, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        qaData = CFDictionaryCreate(NULL, qaKey, qaValue, 1, &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks);
        add_leaf_prod_qa_element(options, prodData, qaData);
        CFReleaseNull(prodData);
        CFReleaseNull(qaData);
    }
}

static void add_intermediate_marker_value_string(CFMutableDictionaryRef options, CFStringRef markerOid, CFStringRef string_value) {
    if (NULL == string_value) {
        add_element(options, kSecPolicyCheckIntermediateMarkerOid, markerOid);
    } else {
        CFDictionaryRef policyData = NULL;
        const void *key[1]   = { markerOid };
        const void *value[1] = { string_value };
        policyData = CFDictionaryCreate(kCFAllocatorDefault,
                                        key, value, 1,
                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        add_element(options, kSecPolicyCheckIntermediateMarkerOid, policyData);

        CFReleaseNull(policyData);
    }
}

static void add_certificate_policy_oid(CFMutableDictionaryRef options, const DERItem *certificatePolicyOid) {
	 CFTypeRef certificatePolicyData = NULL;
     certificatePolicyData = CFDataCreate(kCFAllocatorDefault,
                                certificatePolicyOid ? certificatePolicyOid->data : NULL,
                                certificatePolicyOid ? certificatePolicyOid->length : 0);
    if (certificatePolicyData) {
        add_element(options, kSecPolicyCheckCertificatePolicy, certificatePolicyData);
        CFRelease(certificatePolicyData);
    }
}

static void add_certificate_policy_oid_string(CFMutableDictionaryRef options, CFStringRef certificatePolicyOid) {
    if (certificatePolicyOid) {
        add_element(options, kSecPolicyCheckCertificatePolicy, certificatePolicyOid);
    }
}

//
// Routines for adding dictionary entries for policies.
//

// X.509, but missing validity requirements.
static void SecPolicyAddBasicCertOptions(CFMutableDictionaryRef options)
{
    //CFDictionaryAddValue(options, kSecPolicyCheckBasicCertificateProcessing, kCFBooleanTrue);
        // Happens automatically in SecPVCPathChecks
    CFDictionaryAddValue(options, kSecPolicyCheckCriticalExtensions, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckIdLinkage, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckBasicConstraints, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckNonEmptySubject, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckWeakKeySize, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckWeakSignature, kCFBooleanTrue);
}

static void SecPolicyAddBasicX509Options(CFMutableDictionaryRef options)
{
    SecPolicyAddBasicCertOptions(options);
    CFDictionaryAddValue(options, kSecPolicyCheckTemporalValidity, kCFBooleanTrue);

	// Make sure that black and gray leaf checks are performed for basic X509 chain building
    CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);
}

static bool SecPolicyAddChainLengthOptions(CFMutableDictionaryRef options, CFIndex length)
{
    bool result = false;
    CFNumberRef lengthAsCF = NULL;

    require(lengthAsCF = CFNumberCreate(kCFAllocatorDefault,
                                         kCFNumberCFIndexType, &length), errOut);
    CFDictionaryAddValue(options, kSecPolicyCheckChainLength, lengthAsCF);

    result = true;

errOut:
	CFReleaseSafe(lengthAsCF);
    return result;
}

static bool SecPolicyAddAnchorSHA256Options(CFMutableDictionaryRef options,
                                            const UInt8 anchorSha256[kSecPolicySHA256Size])
{
    bool success = false;
    CFDataRef anchorData = NULL;

    require(anchorData = CFDataCreate(kCFAllocatorDefault, anchorSha256, kSecPolicySHA256Size), errOut);
    add_element(options, kSecPolicyCheckAnchorSHA256, anchorData);

    success = true;

errOut:
    CFReleaseSafe(anchorData);
    return success;
}

static bool SecPolicyAddStrongKeySizeOptions(CFMutableDictionaryRef options) {
    bool success = false;
    CFDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), errOut);
    require(ecSize = CFNumberCreateWithCFIndex(NULL, 256), errOut);
    const void *keys[] = { kSecAttrKeyTypeRSA, kSecAttrKeyTypeEC };
    const void *values[] = { rsaSize, ecSize };
    require(keySizes = CFDictionaryCreate(NULL, keys, values, 2,
                                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);
    add_element(options, kSecPolicyCheckKeySize, keySizes);

    success = true;

errOut:
    CFReleaseSafe(keySizes);
    CFReleaseSafe(rsaSize);
    CFReleaseSafe(ecSize);
    return success;
}

static bool SecPolicyRemoveWeakHashOptions(CFMutableDictionaryRef options) {
    CFMutableArrayRef disallowedHashes = CFArrayCreateMutable(NULL, 5, &kCFTypeArrayCallBacks);
    if (!disallowedHashes) {
        return false;
    }
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD2);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD4);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD5);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmSHA1);

    add_element(options, kSecPolicyCheckSignatureHashAlgorithms, disallowedHashes);
    CFReleaseNull(disallowedHashes);
    return true;
}

static bool isAppleOid(CFStringRef oid) {
    if (!SecCertificateIsOidString(oid)) {
        return false;
    }
    if (CFStringHasPrefix(oid, CFSTR("1.2.840.113635"))) {
        return true;
    }
    return false;
}

static bool isCFPreferenceInSecurityDomain(CFStringRef setting) {
    return (CFPreferencesGetAppBooleanValue(setting, CFSTR("com.apple.security"), NULL));
}

static bool SecPolicyAddAppleAnchorOptions(CFMutableDictionaryRef options, CFStringRef __unused policyName)
{
    CFMutableDictionaryRef appleAnchorOptions = NULL;
    appleAnchorOptions = CFDictionaryCreateMutableForCFTypes(NULL);
    if (!appleAnchorOptions) {
        return false;
    }

    /* Currently no Apple Anchor options */
    add_element(options, kSecPolicyCheckAnchorApple, appleAnchorOptions);
    CFReleaseSafe(appleAnchorOptions);
    return true;
}

CFDataRef CreateCFDataFromBase64CFString(CFStringRef base64string)
{
    __block CFDataRef cfData = NULL;

    require_quiet(base64string, errOut);

    CFStringPerformWithCStringAndLength(base64string, ^(const char *base64string_buf, size_t base64string_buf_length) {
        void *data = NULL;

        require_quiet(base64string_buf != NULL, errOut);
        require_quiet(base64string_buf_length != 0, errOut);

        size_t expected_data_length = SecBase64Decode(base64string_buf, base64string_buf_length, NULL, 0);
        require_quiet(expected_data_length != 0, errOut);

        data = malloc(expected_data_length);
        require(data != NULL, errOut);

        size_t actual_data_length = SecBase64Decode(base64string_buf, base64string_buf_length, data, expected_data_length);
        require_quiet(actual_data_length != 0, errOut);

        cfData = CFDataCreate(kCFAllocatorDefault, (const uint8_t *)data, actual_data_length);

    errOut:
        free(data);
        return;
    });

errOut:
    return cfData;
}

static CFStringRef CopyParentDomainNameFromHostName(CFStringRef hostName)
{
    CFStringRef parentDomainName = NULL;

    require_quiet(hostName, errOut);

    CFIndex hostNameLength = CFStringGetLength(hostName);
    require_quiet(hostNameLength != 0, errOut);

    CFRange nextLabel = CFStringFind(hostName, CFSTR("."), 0);
    require_quiet(nextLabel.location != kCFNotFound && nextLabel.location < (hostNameLength - 1), errOut);

    CFRange parentDomainNameRange = CFRangeMake(nextLabel.location + 1, hostNameLength - nextLabel.location - 1);
    parentDomainName =  CFStringCreateWithSubstring(NULL, hostName, parentDomainNameRange);

errOut:
    return parentDomainName;
}

CFArrayRef parseNSPinnedDomains(CFDictionaryRef nsPinnedDomainsDict, CFStringRef hostName, CFStringRef nsPinnedIdentityType)
{
    CFMutableArrayRef targetSPKISHA256 = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    __block bool hostNamePinned = false;

    // Strip the trailing dot if any.
    CFIndex hostNameLength = CFStringGetLength(hostName);
    if (hostNameLength > 0 && '.' == CFStringGetCharacterAtIndex(hostName, hostNameLength - 1)) {
        hostName = CFStringCreateWithSubstring(NULL, hostName, CFRangeMake(0, hostNameLength - 1));
        require_quiet(hostName, errOut);
    } else {
        CFRetainSafe(hostName);
    }

    CFDictionaryForEach(nsPinnedDomainsDict, ^(const void *key, const void *value) {
        CFStringRef parentDomainName = NULL;

        require_quiet(isString(key), errOutNSPinnedDomainsDict);
        require_quiet(isDictionary(value), errOutNSPinnedDomainsDict);

        // Match one of the pinned domains to the current endpoint's hostname.
        CFStringRef domainName = (CFStringRef)key;
        bool hostNameMatched = (CFStringCompare(domainName, hostName, kCFCompareCaseInsensitive) == kCFCompareEqualTo);

        // Match one of the pinned domains to the current endpoint's parent domain if allowed.
        if (hostNameMatched == false) {
            CFTypeRef nsIncludesSubdomains = CFDictionaryGetValue(value, CFSTR("NSIncludesSubdomains"));
            require_quiet(nsIncludesSubdomains == kCFBooleanTrue, errOutNSPinnedDomainsDict);

            parentDomainName = CopyParentDomainNameFromHostName(hostName);
            require_quiet(parentDomainName != NULL, errOutNSPinnedDomainsDict);

            hostNameMatched = (CFStringCompare(domainName, parentDomainName, kCFCompareCaseInsensitive) == kCFCompareEqualTo);
        }
        require_quiet(hostNameMatched, errOutNSPinnedDomainsDict);

        CFTypeRef nsPinnedIdentities = CFDictionaryGetValue(value, nsPinnedIdentityType);
        require_quiet(nsPinnedIdentities, errOutNSPinnedDomainsDict);
        hostNamePinned = true;

        require_quiet(isArray(nsPinnedIdentities), errOutNSPinnedDomainsDict);
        CFArrayForEach(nsPinnedIdentities, ^(const void *v) {
            CFDataRef spkiSHA256 = NULL;

            require_quiet(isDictionary(v), errOutNSPinnedIdentities);

            CFTypeRef spkiSHA256base64 = CFDictionaryGetValue(v, CFSTR("SPKI-SHA256-BASE64"));
            require_quiet(isString(spkiSHA256base64), errOutNSPinnedIdentities);

            spkiSHA256 = CreateCFDataFromBase64CFString(spkiSHA256base64);
            require_quiet(spkiSHA256, errOutNSPinnedIdentities);

            CFArrayAppendValue(targetSPKISHA256, spkiSHA256);

        errOutNSPinnedIdentities:
            CFReleaseSafe(spkiSHA256);
        });

    errOutNSPinnedDomainsDict:
        CFReleaseSafe(parentDomainName);
        return;
    });

errOut:
    CFReleaseSafe(hostName);
    if (hostNamePinned == false) {
        CFReleaseNull(targetSPKISHA256);
    }
    return targetSPKISHA256;
}

static CFArrayRef getNSPinnedIdentitiesForHostName(CFStringRef hostName, CFStringRef nsPinnedIdentityType)
{
    CFMutableArrayRef targetSPKISHA256 = NULL;

    static CFDictionaryRef nsPinnedDomainsDict = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        CFBundleRef bundle = CFBundleGetMainBundle();
        require(bundle, initializationIncomplete);

        CFTypeRef nsAppTransportSecurityDict = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("NSAppTransportSecurity"));
        require_quiet(isDictionary(nsAppTransportSecurityDict), initializationIncomplete);

        nsPinnedDomainsDict = CFDictionaryGetValue(nsAppTransportSecurityDict, CFSTR("NSPinnedDomains"));
        require(isDictionary(nsPinnedDomainsDict), initializationIncomplete);
        return;

    initializationIncomplete:
        nsPinnedDomainsDict = NULL;
    });
    // To proceed, this or a previous call must have found NSPinnedDomains in the info dictionary.
    require_quiet(nsPinnedDomainsDict, errOut);

    targetSPKISHA256 = (CFMutableArrayRef)parseNSPinnedDomains(nsPinnedDomainsDict, hostName, nsPinnedIdentityType);

    // Return NULL if the hostname (or its parent domain name) is not among the pinned domains.
    // Otherwise return an array of zero or more SPKI SHA256 identities.
errOut:
    return targetSPKISHA256;
}

static void SecPolicyAddATSpinningIfInfoSpecified(CFMutableDictionaryRef options)
{
    CFStringRef hostname = CFDictionaryGetValue(options, kSecPolicyCheckSSLHostname);
    require_quiet(isString(hostname), errOut);

    CFArrayRef leafSPKISHA256 = getNSPinnedIdentitiesForHostName(hostname, CFSTR("NSPinnedLeafIdentities"));
    if (leafSPKISHA256) {
        add_element(options, kSecPolicyCheckLeafSPKISHA256, leafSPKISHA256);
    }

    CFArrayRef caSPKISHA256 = getNSPinnedIdentitiesForHostName(hostname, CFSTR("NSPinnedCAIdentities"));
    if (caSPKISHA256) {
        add_element(options, kSecPolicyCheckCAspkiSHA256, caSPKISHA256);
    }

errOut:
    return;
}

void SecPolicyReconcilePinningRequiredIfInfoSpecified(CFMutableDictionaryRef options)
{
    bool hasPinningRequiredKey = false;
    CFArrayRef leafSPKISHA256 = NULL;
    CFArrayRef caSPKISHA256 = NULL;

    hasPinningRequiredKey = CFDictionaryContainsKey(options, kSecPolicyCheckPinningRequired);
    require_quiet(hasPinningRequiredKey, errOut);

    // A non-NULL, empty, leafSPKISHA256 array allows all leaves and thus excludes this hostname from pinning.
    leafSPKISHA256 = CFDictionaryGetValue(options, kSecPolicyCheckLeafSPKISHA256);
    caSPKISHA256 = CFDictionaryGetValue(options, kSecPolicyCheckCAspkiSHA256);
    if (isArray(leafSPKISHA256) && CFArrayGetCount(leafSPKISHA256) == 0 &&
        isArray(caSPKISHA256) && CFArrayGetCount(caSPKISHA256) == 0) {
        CFDictionaryRemoveValue(options, kSecPolicyCheckPinningRequired);
    }

    // kSecPolicyCheckPinningRequired and (kSecPolicyCheckLeafSPKISHA256, kSecPolicyCheckCAspkiSHA256) are mutually exclusive.
    CFDictionaryRemoveValue(options, kSecPolicyCheckLeafSPKISHA256);
    CFDictionaryRemoveValue(options, kSecPolicyCheckCAspkiSHA256);

errOut:
    return;
}

static bool SecPolicyAddPinningRequiredIfInfoSpecified(CFMutableDictionaryRef options)
{
    static bool result = false;
    static bool hasPinningRequiredKey = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        CFBundleRef bundle = CFBundleGetMainBundle();
        if (bundle) {
            CFTypeRef value = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("SecTrustPinningRequired"));
            if (isBoolean(value) && CFBooleanGetValue(value)) {
                hasPinningRequiredKey = true;
            }
            result = true;
        }
    });
    if (result && hasPinningRequiredKey) {
        add_element(options, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    }
    return result;
}

void SecPolicySetSHA256Pins(SecPolicyRef policy, CFArrayRef _Nullable leafSPKISHA256, CFArrayRef _Nullable caSPKISHA256)
{
    if (!policy) {
        return;
    }
    CFMutableDictionaryRef options = (CFMutableDictionaryRef) policy->_options;
    if (!options) {
        options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!options) return;
        policy->_options = options;
    }

    if (leafSPKISHA256) {
        CFDictionaryRemoveValue(options, kSecPolicyCheckLeafSPKISHA256);
        add_element(options, kSecPolicyCheckLeafSPKISHA256, leafSPKISHA256);
    } else {
        CFDictionaryRemoveValue(options, kSecPolicyCheckLeafSPKISHA256);
    }

    if (caSPKISHA256) {
        CFDictionaryRemoveValue(options, kSecPolicyCheckCAspkiSHA256);
        add_element(options, kSecPolicyCheckCAspkiSHA256, caSPKISHA256);
    } else {
        CFDictionaryRemoveValue(options, kSecPolicyCheckCAspkiSHA256);
    }
}

//
// MARK: Policy Creation Functions
//
SecPolicyRef SecPolicyCreateBasicX509(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);
	CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess,
                         kCFBooleanTrue);

	require(result = SecPolicyCreate(kSecPolicyAppleX509Basic, kSecPolicyNameX509Basic, options), errOut);

errOut:
	CFReleaseSafe(options);
	return (SecPolicyRef _Nonnull)result;
}

SecPolicyRef SecPolicyCreateSSLWithKeyUsage(Boolean server, CFStringRef hostname, uint32_t keyUsage) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

	SecPolicyAddBasicX509Options(options);

	if (hostname) {
		CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);
	}

	CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);

    if (server) {
        require_quiet(SecPolicyRemoveWeakHashOptions(options), errOut);
        require_quiet(SecPolicyAddStrongKeySizeOptions(options), errOut);
        require_quiet(SecPolicyAddPinningRequiredIfInfoSpecified(options), errOut);
        SecPolicyAddATSpinningIfInfoSpecified(options);
        SecPolicyReconcilePinningRequiredIfInfoSpecified(options);
        CFDictionaryAddValue(options, kSecPolicyCheckValidityPeriodMaximums, kCFBooleanTrue);
        CFDictionaryAddValue(options, kSecPolicyCheckServerAuthEKU, kCFBooleanTrue); // enforces stricter EKU rules than set_ssl_ekus below for certain anchor types
#if !TARGET_OS_BRIDGE
        CFDictionaryAddValue(options, kSecPolicyCheckSystemTrustedCTRequired, kCFBooleanTrue);
#endif
    }

    if (keyUsage) {
        add_ku_report(options, keyUsage);
    }

	set_ssl_ekus(options, server);

	require(result = SecPolicyCreate(kSecPolicyAppleSSL,
				server ? kSecPolicyNameSSLServer : kSecPolicyNameSSLClient,
				options), errOut);

errOut:
	CFReleaseSafe(options);
	return (SecPolicyRef _Nonnull)result;
}

SecPolicyRef SecPolicyCreateSSL(Boolean server, CFStringRef hostname) {
    return SecPolicyCreateSSLWithKeyUsage(server, hostname, kSecKeyUsageUnspecified);
}

SecPolicyRef SecPolicyCreateLegacySSL(Boolean server, CFStringRef hostname) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    if (hostname) {
        CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);
    }

    CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);

    if (server) {
        // fewer requirements than the standard SSL policy
        require_quiet(SecPolicyAddPinningRequiredIfInfoSpecified(options), errOut);
        SecPolicyAddATSpinningIfInfoSpecified(options);
        SecPolicyReconcilePinningRequiredIfInfoSpecified(options);
        CFDictionaryAddValue(options, kSecPolicyCheckValidityPeriodMaximums, kCFBooleanTrue);
#if !TARGET_OS_BRIDGE
        CFDictionaryAddValue(options, kSecPolicyCheckSystemTrustedCTRequired, kCFBooleanTrue);
#endif
    }

    set_ssl_ekus(options, server);

    require(result = SecPolicyCreate(kSecPolicyAppleLegacySSL, kSecPolicyNameLegacySSL, options), errOut);

errOut:
    CFReleaseSafe(options);
    return (SecPolicyRef _Nonnull)result;
}

SecPolicyRef SecPolicyCreateApplePinned(CFStringRef policyName, CFStringRef intermediateMarkerOID, CFStringRef leafMarkerOID) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    if (!policyName || !intermediateMarkerOID || !leafMarkerOID) {
        goto errOut;
    }

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* Anchored to the Apple Roots */
    require(SecPolicyAddAppleAnchorOptions(options, policyName), errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID matches input OID */
    if (!isAppleOid(intermediateMarkerOID)) {
        secwarning("creating an Apple pinning policy with a non-Apple OID: %@", intermediateMarkerOID);
    }
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, intermediateMarkerOID);

    /* Leaf marker OID matches input OID */
    if (!isAppleOid(leafMarkerOID)) {
        secwarning("creating an Apple pinning policy with a non-Apple OID: %@", leafMarkerOID);
    }
    add_leaf_marker_string(options, leafMarkerOID);

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    /* Check for weak hashes */
    // require(SecPolicyRemoveWeakHashOptions(options), errOut); // the current WWDR CA cert is signed with SHA1
    require(result = SecPolicyCreate(kSecPolicyAppleGenericApplePinned,
                                     policyName, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

static bool
requireUATPinning(CFStringRef service)
{
    bool pinningRequired = true;

    if (SecIsInternalRelease()) {
        CFStringRef setting = CFStringCreateWithFormat(NULL, NULL, CFSTR("AppleServerAuthenticationNoPinning%@"), service);
        require(setting, fail);
        if(isCFPreferenceInSecurityDomain(setting)) {
            pinningRequired = false;
        } else {
            secnotice("pinningQA", "could not disable pinning: %@ not true", setting);
        }
        CFRelease(setting);

        if (!pinningRequired) {
            goto fail;
        }

        if(isCFPreferenceInSecurityDomain(CFSTR("AppleServerAuthenticationNoPinning"))) {
            pinningRequired = false;
        } else {
            secnotice("pinningQA", "could not disable pinning: AppleServerAuthenticationNoPinning not true");
        }
    } else {
        secnotice("pinningQA", "could not disable pinning: not an internal release");
    }
fail:
    return pinningRequired;
}

SecPolicyRef SecPolicyCreateAppleSSLPinned(CFStringRef policyName, CFStringRef hostname,
                                          CFStringRef intermediateMarkerOID, CFStringRef leafMarkerOID) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    if (!policyName || !hostname || !leafMarkerOID) {
        goto errOut;
    }

    if (requireUATPinning(policyName)) {
        require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                    &kCFTypeDictionaryKeyCallBacks,
                                                    &kCFTypeDictionaryValueCallBacks), errOut);

        SecPolicyAddBasicX509Options(options);

        /* Anchored to the Apple Roots */
        require(SecPolicyAddAppleAnchorOptions(options, policyName), errOut);

        /* Exactly 3 certs in the chain */
        require(SecPolicyAddChainLengthOptions(options, 3), errOut);

        if (intermediateMarkerOID) {
            /* Intermediate marker OID matches input OID */
            if (!isAppleOid(intermediateMarkerOID)) {
                secwarning("creating an Apple pinning policy with a non-Apple OID: %@", intermediateMarkerOID);
            }
            add_element(options, kSecPolicyCheckIntermediateMarkerOid, intermediateMarkerOID);
        } else {
            add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.12"));
        }

        /* Leaf marker OID matches input OID */
        if (!isAppleOid(leafMarkerOID)) {
            secwarning("creating an Apple pinning policy with a non-Apple OID: %@", leafMarkerOID);
        }
        add_leaf_marker_string(options, leafMarkerOID);

        /* New leaf marker OID format */
        add_leaf_marker_value_string(options, CFSTR("1.2.840.113635.100.6.48.1"), leafMarkerOID);

        /* ServerAuth EKU is in leaf cert */
        add_eku_string(options, CFSTR("1.3.6.1.5.5.7.3.1"));

        /* Hostname is in leaf cert */
        add_element(options, kSecPolicyCheckSSLHostname, hostname);

        /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
        require(SecPolicyAddStrongKeySizeOptions(options), errOut);

        /* Check for weak hashes */
        require(SecPolicyRemoveWeakHashOptions(options), errOut);

        /* Check revocation using any available method */
        add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

        require(result = SecPolicyCreate(kSecPolicyAppleGenericAppleSSLPinned,
                                         policyName, options), errOut);

    } else {
        result = SecPolicyCreateSSL(true, hostname);
        SecPolicySetOid(result, kSecPolicyAppleGenericAppleSSLPinned);
        SecPolicySetName(result, policyName);
    }

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateiPhoneActivation(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

#if 0
	CFDictionaryAddValue(options, kSecPolicyCheckKeyUsage,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckExtendedKeyUsage,
		kCFBooleanTrue);
#endif

    /* Basic X.509 policy with the additional requirements that the chain
       length is 3, it's anchored at the AppleCA and the leaf certificate
       has issuer "Apple iPhone Certification Authority" and
       subject "Apple iPhone Activation" for the common name. */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
        CFSTR("Apple iPhone Certification Authority"));
    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
        CFSTR("Apple iPhone Activation"));

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameiPhoneActivation), errOut);

	require(result = SecPolicyCreate(kSecPolicyAppleiPhoneActivation,
                                     kSecPolicyNameiPhoneActivation, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiPhoneDeviceCertificate(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

#if 0
	CFDictionaryAddValue(options, kSecPolicyCheckKeyUsage,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckExtendedKeyUsage,
		kCFBooleanTrue);
#endif

    /* Basic X.509 policy with the additional requirements that the chain
       length is 4, it's anchored at the AppleCA and the first intermediate
       has the subject "Apple iPhone Device CA". */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
        CFSTR("Apple iPhone Device CA"));

    require(SecPolicyAddChainLengthOptions(options, 4), errOut);
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameiPhoneDeviceCertificate), errOut);

	require(result = SecPolicyCreate(kSecPolicyAppleiPhoneDeviceCertificate,
                                     kSecPolicyNameiPhoneDeviceCertificate, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

/* subject:/C=US/O=Apple Inc./OU=Apple iPhone/CN=[TEST] Apple iPhone Device CA */
/* issuer :/C=US/O=Apple Inc./OU=Apple Certification Authority/CN=[TEST] Apple iPhone Certification Authority */
const uint8_t kFactoryDeviceCASHA256[CC_SHA256_DIGEST_LENGTH] = {
    0x7b, 0x8e, 0xc8, 0x78, 0xff, 0x3a, 0xcf, 0x61, 0xdd, 0xe6, 0x53, 0x77, 0x2b, 0xe7, 0x32, 0xc5,
    0x97, 0xf4, 0x6b, 0x9c, 0xa6, 0x00, 0xc5, 0x2c, 0xc1, 0x25, 0x85, 0x02, 0x03, 0x06, 0x97, 0x96
};

SecPolicyRef SecPolicyCreateFactoryDeviceCertificate(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    /* Basic X.509 policy with the additional requirements that the chain
       is anchored at the factory device certificate issuer. */
    require(SecPolicyAddAnchorSHA256Options(options, kFactoryDeviceCASHA256), errOut);

	require(result = SecPolicyCreate(kSecPolicyAppleFactoryDeviceCertificate,
                                     kSecPolicyNameFactoryDeviceCertificate, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiAP(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;
	CFTimeZoneRef tz = NULL;
	CFDateRef date = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNamePrefix,
        CFSTR("IPA_"));

    date = CFDateCreateForGregorianZuluDay(NULL, 2006, 5, 31);
    CFDictionaryAddValue(options, kSecPolicyCheckNotValidBefore, date);

	require(result = SecPolicyCreate(kSecPolicyAppleiAP,
                                     kSecPolicyNameiAP, options),
        errOut);

errOut:
	CFReleaseSafe(date);
	CFReleaseSafe(tz);
	CFReleaseSafe(options);
	return result;
}

/* subject:/O=Apple Inc./OU=iTunes Store/CN=iTunes Store Root/C=US/ST=California/L=Cupertino */
/* issuer :/O=Apple Inc./OU=iTunes Store/CN=iTunes Store Root/C=US/ST=California/L=Cupertino */
const uint8_t kITMS_CA_SHA256[CC_SHA256_DIGEST_LENGTH] = {
    0xa1, 0xdc, 0x36, 0x23, 0x84, 0xb4, 0xba, 0x0f, 0xaf, 0xea, 0x2a, 0xd4, 0xac, 0xc4, 0x86, 0x8f,
    0xfb, 0xae, 0x57, 0x21, 0x4d, 0x20, 0x88, 0xc8, 0x82, 0xe7, 0x65, 0x13, 0x47, 0xab, 0x81, 0xd7
};

SecPolicyRef SecPolicyCreateiTunesStoreURLBag(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;


	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

	CFDictionaryAddValue(options, kSecPolicyCheckSubjectOrganization,
		CFSTR("Apple Inc."));
	CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
		CFSTR("iTunes Store URL Bag"));

    require(SecPolicyAddChainLengthOptions(options, 2), errOut);
    require(SecPolicyAddAnchorSHA256Options(options, kITMS_CA_SHA256), errOut);

	require(result = SecPolicyCreate(kSecPolicyAppleiTunesStoreURLBag,
                                     kSecPolicyNameiTunesStoreURLBag, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateEAP(Boolean server, CFArrayRef trustedServerNames) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

	SecPolicyAddBasicX509Options(options);

	/* Since EAP is used to setup the network we don't want evaluation
	   using this policy to access the network. */
	CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess,
			kCFBooleanTrue);

	if (trustedServerNames) {
		CFDictionaryAddValue(options, kSecPolicyCheckEAPTrustedServerNames, trustedServerNames);
    }

    if (server) {
        /* Check for weak hashes and keys, but only on system-trusted roots, because enterprise
         * PKIs are the absolute worst. */
        CFDictionaryAddValue(options, kSecPolicyCheckSystemTrustedWeakHash, kCFBooleanTrue);
        CFDictionaryAddValue(options, kSecPolicyCheckSystemTrustedWeakKey, kCFBooleanTrue);
    }

    /* We need to check for EKU per rdar://22206018 */
    set_ssl_ekus(options, server);

	require(result = SecPolicyCreate(kSecPolicyAppleEAP,
				server ? kSecPolicyNameEAPServer : kSecPolicyNameEAPClient,
				options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateIPSec(Boolean server, CFStringRef hostname) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

	if (hostname) {
		CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);
	}

    /* Require oidExtendedKeyUsageIPSec if Extended Keyusage Extention is
       present. */
    /* Per <rdar://problem/6843827> Cisco VPN Certificate compatibility issue.
       We don't check the EKU for IPSec certs for now.  If we do add eku
       checking back in the future, we should probably also accept the
       following EKUs:
           ipsecEndSystem   1.3.6.1.5.5.7.3.5
       and possibly even
           ipsecTunnel      1.3.6.1.5.5.7.3.6
           ipsecUser        1.3.6.1.5.5.7.3.7
     */
    //add_eku(options, NULL); /* eku extension is optional */
    //add_eku(options, &oidAnyExtendedKeyUsage);
    //add_eku(options, &oidExtendedKeyUsageIPSec);

	require(result = SecPolicyCreate(kSecPolicyAppleIPsec,
		server ? kSecPolicyNameIPSecServer : kSecPolicyNameIPSecClient,
		options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiPhoneApplicationSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    /* Anchored to the Apple Roots */
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameiPhoneApplicationSigning), errOut);

    /* Leaf checks */
    if (SecIsInternalRelease()) {
        /* Allow a prod hierarchy-signed test cert */
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNameTEST,
                             CFSTR("Apple iPhone OS Application Signing"));
        add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.3.1"));
    }
    else {
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                             CFSTR("Apple iPhone OS Application Signing"));
    }
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.3"));

    add_eku(options, NULL); /* eku extension is optional */
    add_eku(options, &oidAnyExtendedKeyUsage);
    add_eku(options, &oidExtendedKeyUsageCodeSigning);

    /* Intermediate check */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple iPhone Certification Authority"));

    /* Chain length check */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Skip networked revocation checks */
    CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess, kCFBooleanTrue);

	require(result = SecPolicyCreate(kSecPolicyAppleiPhoneApplicationSigning,
                                     kSecPolicyNameiPhoneApplicationSigning, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiPhoneVPNApplicationSigning(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    /* Anchored to the Apple Roots */
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameiPhoneVPNApplicationSigning), errOut);

    /* Leaf checks */
    if (SecIsInternalRelease()) {
        /* Allow a prod hierarchy-signed test cert */
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNameTEST,
                             CFSTR("Apple iPhone OS Application Signing"));
        add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.6.1"));
    }
    else {
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                             CFSTR("Apple iPhone OS Application Signing"));
    }
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.6"));

    add_eku(options, NULL); /* eku extension is optional */
    add_eku(options, &oidAnyExtendedKeyUsage);
    add_eku(options, &oidExtendedKeyUsageCodeSigning);

    /* Intermediate check */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple iPhone Certification Authority"));

    /* Chain length check */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Skip networked revocation checks */
    CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess, kCFBooleanTrue);

    require(result = SecPolicyCreate(kSecPolicyAppleiPhoneVPNApplicationSigning,
                                     kSecPolicyNameiPhoneVPNApplicationSigning, options),
            errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateiPhoneProfileApplicationSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options); // With expiration checking

    /* Apple Anchor */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameiPhoneProfileApplicationSigning),
                  errOut);

    /* Chain Len: 3 */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Leaf has CodeSigning EKU */
    add_eku_string(options, CFSTR("1.3.6.1.5.5.7.3.3"));

    /* On iOS, the cert in the provisioning profile may be one of:
                                leaf OID            intermediate OID
        iPhone Developer        <ADS>.6.1.2         <ADS>.6.2.1
        iPhone Distribution     <ADS>.6.1.4         <ADS>.6.2.1
        TestFlight (Prod)       <ADS>.6.1.25.1      <ADS>.6.2.1
        TestFlight (QA)         <ADS>.6.1.25.2      <ADS>.6.2.1
     */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.1"));
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.2"));
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.4"));
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.25.1"));
    if (SecIsInternalRelease()) {
        add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.25.2"));
    }

    /* Revocation via any available method */
    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

	require(result = SecPolicyCreate(kSecPolicyAppleiPhoneProfileApplicationSigning,
                                     kSecPolicyNameiPhoneProfileApplicationSigning,
                                     options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateMacOSProfileApplicationSigning(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options); // Without expiration checking

    /* Apple Anchor */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameiPhoneProfileApplicationSigning),
                  errOut);

    /* Chain Len: 3 */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Leaf has CodeSigning EKU */
    add_eku_string(options, CFSTR("1.3.6.1.5.5.7.3.3"));


    /* On macOS, the cert in the provisioning profile may be one of:
                            leaf OID            intermediate OID
     MAS Development         <ADS>.6.1.12        <ADS>.6.2.1
     MAS Submission          <ADS>.6.1.7         <ADS>.6.2.1
     Developer ID            <ADS>.6.1.13        <ADS>.6.2.6
     B&I                     <ADS>.6.22          None - "Apple Code Signing Certification Authority"
     */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.12"));
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.7"));
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.13"));
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.22"));

    /* Revocation via any available method */
    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    require(result = SecPolicyCreate(kSecPolicyAppleMacOSProfileApplicationSigning,
                                     kSecPolicyNameMacOSProfileApplicationSigning,
                                     options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateiPhoneProvisioningProfileSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    /* Basic X.509 policy with the additional requirements that the chain
       length is 3, it's anchored at the AppleCA and the leaf certificate
       has issuer "Apple iPhone Certification Authority" and
       subject "Apple iPhone OS Provisioning Profile Signing" for the common name. */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
        CFSTR("Apple iPhone Certification Authority"));
    if (SecIsInternalRelease()) {
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNameTEST,
                             CFSTR("Apple iPhone OS Provisioning Profile Signing"));
    }
    else {
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                             CFSTR("Apple iPhone OS Provisioning Profile Signing"));
    }

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameiPhoneProvisioningProfileSigning), errOut);

    /* Skip networked revocation checks */
    CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess, kCFBooleanTrue);

	require(result = SecPolicyCreate(kSecPolicyAppleiPhoneProvisioningProfileSigning,
                                     kSecPolicyNameiPhoneProvisioningProfileSigning, options),
        errOut);

    /* 1.2.840.113635.100.6.2.2.1, non-critical: DER:05:00 - provisioning profile */

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateAppleTVOSApplicationSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;
	CFDataRef atvProdOid = NULL;
	CFDataRef atvTestOid = NULL;
	CFArrayRef oids = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameTVOSApplicationSigning),
                  errOut);

    /* Check for intermediate: Apple Worldwide Developer Relations */
    /* 1.2.840.113635.100.6.2.1 */
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleWWDR);

    add_ku(options, kSecKeyUsageDigitalSignature);

    /* Check for prod or test AppleTV Application Signing OIDs */
    /* Prod: 1.2.840.113635.100.6.1.24 */
    /* ProdQA: 1.2.840.113635.100.6.1.24.1 */
    add_leaf_marker(options, &oidAppleTVOSApplicationSigningProd);
    add_leaf_marker(options, &oidAppleTVOSApplicationSigningProdQA);

    /* Skip networked revocation checks */
    CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess, kCFBooleanTrue);

	require(result = SecPolicyCreate(kSecPolicyAppleTVOSApplicationSigning,
                                     kSecPolicyNameTVOSApplicationSigning, options),
			errOut);

errOut:
	CFReleaseSafe(options);
	CFReleaseSafe(oids);
	CFReleaseSafe(atvProdOid);
	CFReleaseSafe(atvTestOid);
	return result;
}

SecPolicyRef SecPolicyCreateOCSPSigner(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* Require id-kp-OCSPSigning extendedKeyUsage to be present, not optional. */
    add_eku(options, &oidExtendedKeyUsageOCSPSigning);

    /* Check for digitalSignature KU and CA:FALSE. See <rdar://problem/65354714> */
    add_ku(options, kSecKeyUsageDigitalSignature);
    CFDictionarySetValue(options, kSecPolicyCheckNotCA, kCFBooleanTrue);

    require(result = SecPolicyCreate(kSecPolicyAppleOCSPSigner,
                                     kSecPolicyNameOCSPSigner, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateRevocation(CFOptionFlags revocationFlags) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

    require(revocationFlags != 0, errOut);

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    if (revocationFlags & kSecRevocationCheckIfTrusted) {
        CFDictionaryAddValue(options, kSecPolicyCheckRevocationIfTrusted, kCFBooleanTrue);
        /* Set method, but allow caller to override with later checks */
        CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);
    }

    if (revocationFlags & kSecRevocationOnlineCheck) {
        CFDictionaryAddValue(options, kSecPolicyCheckRevocationOnline, kCFBooleanTrue);
        /* Set method, but allow caller to override with later checks */
        CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);
    }

	if (revocationFlags & kSecRevocationOCSPMethod && revocationFlags & kSecRevocationCRLMethod) {
		CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);
	}
	else if (revocationFlags & kSecRevocationOCSPMethod) {
		CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationOCSP);
	}
	else if (revocationFlags & kSecRevocationCRLMethod) {
		CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationCRL);
	}

	if (revocationFlags & kSecRevocationRequirePositiveResponse) {
		CFDictionaryAddValue(options, kSecPolicyCheckRevocationResponseRequired, kCFBooleanTrue);
	}

	if (revocationFlags & kSecRevocationNetworkAccessDisabled) {
		CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess, kCFBooleanTrue);
    } else {
        /* If the caller didn't explicitly disable network access, the revocation policy
         * should override any other policy's network setting.
         * In particular, pairing a revocation policy with BasicX509 should result in
         * allowing network access for revocation unless explicitly disabled.
         * Note that SecTrustSetNetworkFetchAllowed can override even this. */
        CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess, kCFBooleanFalse);
    }

	/* Only flag bits 0-6 are currently defined */
	require(((revocationFlags >> 7) == 0), errOut);

	require(result = SecPolicyCreate(kSecPolicyAppleRevocation,
                                     kSecPolicyNameRevocation, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateSMIME(CFIndex smimeUsage, CFStringRef email) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    if (smimeUsage & kSecIgnoreExpirationSMIMEUsage) {
        SecPolicyAddBasicCertOptions(options);
    } else {
        SecPolicyAddBasicX509Options(options);
    }

    /* We call add_ku for each combination of bits we are willing to allow. */
    if (smimeUsage & kSecSignSMIMEUsage) {
        add_ku(options, kSecKeyUsageUnspecified);
        add_ku(options, kSecKeyUsageDigitalSignature);
    }
    if (smimeUsage & kSecKeyEncryptSMIMEUsage) {
        add_ku(options, kSecKeyUsageKeyEncipherment);
    }
    if (smimeUsage & kSecDataEncryptSMIMEUsage) {
        add_ku(options, kSecKeyUsageDataEncipherment);
    }
    if (smimeUsage & kSecKeyExchangeDecryptSMIMEUsage ||
        smimeUsage & kSecKeyExchangeEncryptSMIMEUsage ||
        smimeUsage & kSecKeyExchangeBothSMIMEUsage) {
        /* <rdar://57130017> */
        add_ku(options, kSecKeyUsageKeyAgreement);
    }

	if (email) {
		CFDictionaryAddValue(options, kSecPolicyCheckEmail, email);
	}

    add_eku(options, NULL); /* eku extension is optional */
    add_eku(options, &oidExtendedKeyUsageEmailProtection);

#if !TARGET_OS_IPHONE
    // Check revocation on OS X
    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);
#endif

	require(result = SecPolicyCreate(kSecPolicyAppleSMIME, kSecPolicyNameSMIME, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateApplePackageSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

	SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNamePackageSigning), errOut);

    add_ku(options, kSecKeyUsageDigitalSignature);
    add_eku(options, &oidExtendedKeyUsageCodeSigning);

	require(result = SecPolicyCreate(kSecPolicyApplePackageSigning,
                                     kSecPolicyNamePackageSigning, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;

}

SecPolicyRef SecPolicyCreateAppleSWUpdateSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;
/*
 * OS X rules for this policy:
 * -- Must have one intermediate cert
 * -- intermediate must have basic constraints with path length 0
 * -- intermediate has CSSMOID_APPLE_EKU_CODE_SIGNING EKU
 * -- leaf cert has either CODE_SIGNING or CODE_SIGN_DEVELOPMENT EKU (the latter of
 *    which triggers a CSSMERR_APPLETP_CODE_SIGN_DEVELOPMENT error)
 */
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

	SecPolicyAddBasicX509Options(options);

	require(SecPolicyAddChainLengthOptions(options, 3), errOut);
	require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameSWUpdateSigning), errOut);

	add_eku(options, &oidAppleExtendedKeyUsageCodeSigning);
	add_oid(options, kSecPolicyCheckIntermediateEKU, &oidAppleExtendedKeyUsageCodeSigning);

	require(result = SecPolicyCreate(kSecPolicyAppleSWUpdateSigning,
                                     kSecPolicyNameSWUpdateSigning, options),
			errOut);

errOut:
	CFReleaseSafe(options);
	return result;

}

SecPolicyRef SecPolicyCreateCodeSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

	SecPolicyAddBasicX509Options(options);

	/* If the key usage extension is present, we accept it having either of
	   these values. */
	add_ku(options, kSecKeyUsageDigitalSignature);
	add_ku(options, kSecKeyUsageNonRepudiation);

	/* We require an extended key usage extension with the codesigning
	   eku purpose. (The Apple codesigning eku is not accepted here
	   since it's valid only for SecPolicyCreateAppleSWUpdateSigning.) */
	add_eku(options, &oidExtendedKeyUsageCodeSigning);
#if TARGET_OS_IPHONE
	/* Accept the 'any' eku on iOS only to match prior behavior.
	   This may be further restricted in future releases. */
	add_eku(options, &oidAnyExtendedKeyUsage);
#endif

	require(result = SecPolicyCreate(kSecPolicyAppleCodeSigning,
                                     kSecPolicyNameCodeSigning, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

/* Explicitly leave out empty subject/subjectaltname check */
SecPolicyRef SecPolicyCreateLockdownPairing(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);
	//CFDictionaryAddValue(options, kSecPolicyCheckBasicCertificateProcessing,
    //    kCFBooleanTrue); // Happens automatically in SecPVCPathChecks
	CFDictionaryAddValue(options, kSecPolicyCheckCriticalExtensions,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckIdLinkage,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckBasicConstraints,
		kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckWeakKeySize, kCFBooleanTrue);

	require(result = SecPolicyCreate(kSecPolicyAppleLockdownPairing,
                                     kSecPolicyNameLockdownPairing, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateURLBag(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    add_eku(options, &oidExtendedKeyUsageCodeSigning);

	require(result = SecPolicyCreate(kSecPolicyAppleURLBag,
                                     kSecPolicyNameURLBag, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateOTATasking(void)
{
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* Apple Anchor */
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameMobileAsset), errOut);

    /* Chain length of 3 */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate has common name "Apple iPhone Certification Authority". */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple iPhone Certification Authority"));

    /* Leaf has common name "Asset Manifest Signing" */
    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName, CFSTR("OTA Task Signing"));

    require(result = SecPolicyCreate(kSecPolicyAppleOTATasking, kSecPolicyNameOTATasking, options),
            errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateMobileAsset(void)
{
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    /* No expiration check */
    SecPolicyAddBasicCertOptions(options);

    /* Apple Anchor */
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameMobileAsset), errOut);

    /* Chain length of 3 */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate has common name "Apple iPhone Certification Authority". */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple iPhone Certification Authority"));

    /* Leaf has common name "Asset Manifest Signing" */
    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName, CFSTR("Asset Manifest Signing"));

    require(result = SecPolicyCreate(kSecPolicyAppleMobileAsset, kSecPolicyNameMobileAsset, options),
            errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateMobileAssetDevelopment(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    /* No expiration check */
    SecPolicyAddBasicCertOptions(options);

    /* Apple Anchor */
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameMobileAsset), errOut);

    /* Chain length of 3 */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate has the iPhone CA Marker extension */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.18"));

    /* Leaf has ProdQA Mobile Asset Marker extension */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.55.1"));

    require(result = SecPolicyCreate(kSecPolicyAppleMobileAssetDevelopment, kSecPolicyNameMobileAsset, options),
            errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleIDAuthorityPolicy(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), out);

    //Leaf appears to be a SSL only cert, so policy should expand on that policy
    SecPolicyAddBasicX509Options(options);

    // Apple CA anchored
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameIDAuthority), out);

    // with the addition of the existence check of an extension with "Apple ID Sharing Certificate" oid (1.2.840.113635.100.4.7)
    // NOTE: this obviously intended to have gone into Extended Key Usage, but evidence of existing certs proves the contrary.
    add_leaf_marker(options, &oidAppleExtendedKeyUsageAppleID);

    // and validate that intermediate has extension with CSSMOID_APPLE_EXTENSION_AAI_INTERMEDIATE  oid (1.2.840.113635.100.6.2.3) and goes back to the Apple Root CA.
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleID);
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleID2);

	require(result = SecPolicyCreate(kSecPolicyAppleIDAuthority,
                                     kSecPolicyNameIDAuthority, options), out);

out:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateMacAppStoreReceipt(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), out);

    SecPolicyAddBasicX509Options(options);

    // Apple CA anchored
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameMacAppStoreReceipt), out);

    // Chain length of 3
    require(SecPolicyAddChainLengthOptions(options, 3), out);

    // MacAppStoreReceipt policy OID
    add_certificate_policy_oid_string(options, CFSTR("1.2.840.113635.100.5.6.1"));

    // Intermediate marker OID
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.1"));

    // Leaf marker OID
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.11.1"));

    // Check revocation
    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

	require(result = SecPolicyCreate(kSecPolicyMacAppStoreReceipt,
                                     kSecPolicyNameMacAppStoreReceipt, options), out);

out:
    CFReleaseSafe(options);
    return result;

}

static SecPolicyRef _SecPolicyCreatePassbookCardSigner(CFStringRef cardIssuer, CFStringRef teamIdentifier, bool requireTeamID)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks), out);

	SecPolicyAddBasicX509Options(options);
	require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNamePassbookSigning), out);

    // Chain length of 3
    require(SecPolicyAddChainLengthOptions(options, 3), out);

	if (teamIdentifier) {
		// If supplied, teamIdentifier must match subject OU field
		CFDictionaryAddValue(options, kSecPolicyCheckSubjectOrganizationalUnit, teamIdentifier);
	}
	else {
		// If not supplied, and it was required, fail
		require(!requireTeamID, out);
	}

    // Must be both push and 3rd party package signing
    add_leaf_marker_value(options, &oidAppleInstallerPackagingSigningExternal, cardIssuer);

	// We should check that it also has push marker, but we don't support requiring both, only either.
	// add_independent_oid(options, kSecPolicyCheckLeafMarkerOid, &oidApplePushServiceClient);

    //WWDR Intermediate marker OID
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.1"));

	// And Passbook signing eku
	add_eku(options, &oidAppleExtendedKeyUsagePassbook);

	require(result = SecPolicyCreate(kSecPolicyApplePassbookSigning,
                                     kSecPolicyNamePassbookSigning, options), out);

out:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreatePassbookCardSigner(CFStringRef cardIssuer, CFStringRef teamIdentifier)
{
    return _SecPolicyCreatePassbookCardSigner(cardIssuer, teamIdentifier, true);
}


static SecPolicyRef CreateMobileStoreSigner(Boolean forTest)
{

    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicX509Options(options);
    require(SecPolicyAddAppleAnchorOptions(options,
                                   ((forTest) ? kSecPolicyNameTestMobileStore :
                                   kSecPolicyNameMobileStore)), errOut);

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple System Integration 2 Certification Authority"));

    add_ku(options, kSecKeyUsageDigitalSignature);

    const DERItem* pOID = (forTest) ? &oidApplePolicyMobileStoreProdQA : &oidApplePolicyMobileStore;

    add_certificate_policy_oid(options, pOID);

    require(result = SecPolicyCreate((forTest) ? kSecPolicyAppleTestMobileStore : kSecPolicyAppleMobileStore,
                                     (forTest) ? kSecPolicyNameTestMobileStore : kSecPolicyNameMobileStore,
                                     options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateMobileStoreSigner(void)
{
	return CreateMobileStoreSigner(false);
}

SecPolicyRef SecPolicyCreateTestMobileStoreSigner(void)
{
    return CreateMobileStoreSigner(true);
}


CF_RETURNS_RETAINED SecPolicyRef SecPolicyCreateEscrowServiceSigner(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    // X509, ignoring date validity
    SecPolicyAddBasicCertOptions(options);

    add_ku(options, kSecKeyUsageKeyEncipherment);

    /* Leaf has marker OID with value that can't be pre-determined */
    add_element(options, kSecPolicyCheckLeafMarkerOidWithoutValueCheck, CFSTR("1.2.840.113635.100.6.23.1"));
    require(SecPolicyAddChainLengthOptions(options, 2), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleEscrowService,
                                     kSecPolicyNameEscrowService, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

CF_RETURNS_RETAINED SecPolicyRef SecPolicyCreatePCSEscrowServiceSigner(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);
    add_ku(options, kSecKeyUsageKeyEncipherment);

    /* Leaf has marker OID with value that can't be pre-determined */
    add_element(options, kSecPolicyCheckLeafMarkerOidWithoutValueCheck, CFSTR("1.2.840.113635.100.6.23.1"));
    require(SecPolicyAddChainLengthOptions(options, 2), errOut);

    require(result = SecPolicyCreate(kSecPolicyApplePCSEscrowService,
                                     kSecPolicyNamePCSEscrowService, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateConfigurationProfileSigner(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameProfileSigner), errOut);

    //Chain length 3
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    // Require the profile signing EKU
    add_eku(options, &oidAppleExtendedKeyUsageProfileSigning);

    CFStringRef releaseType = MGCopyAnswer(kMGQReleaseType, NULL);
    if (releaseType != NULL) {
        // all non-GM variants (beta, carrier, internal, etc) allow the QA signer as well
        add_eku(options, &oidAppleExtendedKeyUsageQAProfileSigning);
    }
    CFReleaseNull(releaseType);

    // Require the Apple Application Integration CA marker OID
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.3"));

    require(result = SecPolicyCreate(kSecPolicyAppleProfileSigner,
                                     kSecPolicyNameProfileSigner,
                                     options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateQAConfigurationProfileSigner(void)
{
    return SecPolicyCreateConfigurationProfileSigner();
}

SecPolicyRef SecPolicyCreateOSXProvisioningProfileSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    // Require valid chain from the Apple root
    SecPolicyAddBasicX509Options(options);
    SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameOSXProvisioningProfileSigning);

    // Require provisioning profile leaf marker OID (1.2.840.113635.100.4.11)
    add_leaf_marker(options, &oidAppleCertExtOSXProvisioningProfileSigning);

    // Require intermediate marker OID (1.2.840.113635.100.6.2.1)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleWWDR);

    // Require key usage that allows signing
    add_ku(options, kSecKeyUsageDigitalSignature);

    // Ensure that revocation is checked (OCSP)
    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationOCSP);

    require(result = SecPolicyCreate(kSecPolicyAppleOSXProvisioningProfileSigning,
                                     kSecPolicyNameOSXProvisioningProfileSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateAppleSMPEncryption
 @abstract Check for intermediate certificate 'Apple System Integration CA - G3' by name,
    and root certificate 'Apple Root CA - G3' by hash.
    Leaf cert must have Key Encipherment usage.
    Leaf cert must have Apple SMP Encryption marker OID (1.2.840.113635.100.6.30).
    Intermediate must have marker OID (1.2.840.113635.100.6.2.13).
 */
SecPolicyRef SecPolicyCreateAppleSMPEncryption(void)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	                                              &kCFTypeDictionaryKeyCallBacks,
	                                              &kCFTypeDictionaryValueCallBacks), errOut);
	SecPolicyAddBasicCertOptions(options);

	require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameSMPEncryption),
            errOut);
	require(SecPolicyAddChainLengthOptions(options, 3), errOut);

	CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
					CFSTR("Apple System Integration CA - G3"));

	// Check that leaf has extension with "Apple SMP Encryption" oid (1.2.840.113635.100.6.30)
	add_leaf_marker(options, &oidAppleCertExtAppleSMPEncryption);

	// Check that intermediate has extension (1.2.840.113635.100.6.2.13)
	add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntgG3);

	add_ku(options, kSecKeyUsageKeyEncipherment);

	// Ensure that revocation is checked (OCSP)
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationOCSP);

	require(result = SecPolicyCreate(kSecPolicyAppleSMPEncryption,
                                     kSecPolicyNameSMPEncryption, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

/* subject:/CN=Test Apple Root CA - ECC/OU=Certification Authority/O=Apple Inc./C=US */
/* issuer :/CN=Test Apple Root CA - ECC/OU=Certification Authority/O=Apple Inc./C=US */
const uint8_t kTestAppleRootCA_ECC_SHA256[CC_SHA256_DIGEST_LENGTH] = {
    0xe8, 0x6a, 0xd6, 0x5c, 0x74, 0x60, 0x21, 0x14, 0x47, 0xc6, 0x6a, 0xd7, 0x5f, 0xf8, 0x06, 0x7b,
    0xec, 0xb5, 0x52, 0x7e, 0x4e, 0xa1, 0xac, 0x48, 0xcf, 0x3c, 0x53, 0x8f, 0x4d, 0x2b, 0x20, 0xa9
};

/*!
 @function SecPolicyCreateTestAppleSMPEncryption
 @abstract Check for intermediate certificate 'Test Apple System Integration CA - ECC' by name,
    and root certificate 'Test Apple Root CA - ECC' by hash.
    Leaf cert must have Key Encipherment usage. Other checks TBD.
 */
SecPolicyRef SecPolicyCreateTestAppleSMPEncryption(void)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	                                              &kCFTypeDictionaryKeyCallBacks,
	                                              &kCFTypeDictionaryValueCallBacks), errOut);
	SecPolicyAddBasicCertOptions(options);

	SecPolicyAddAnchorSHA256Options(options, kTestAppleRootCA_ECC_SHA256);
	require(SecPolicyAddChainLengthOptions(options, 3), errOut);

	CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
			CFSTR("Test Apple System Integration CA - ECC"));

	add_ku(options, kSecKeyUsageKeyEncipherment);

	// Ensure that revocation is checked (OCSP)
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationOCSP);

	require(result = SecPolicyCreate(kSecPolicyAppleTestSMPEncryption,
                                     kSecPolicyNameTestSMPEncryption, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}


SecPolicyRef SecPolicyCreateAppleIDValidationRecordSigningPolicy(void)
{
	SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    //Leaf appears to be a SSL only cert, so policy should expand on that policy
    SecPolicyAddBasicX509Options(options);

    // Apple CA anchored
    require(SecPolicyAddAppleAnchorOptions(options,
                                           kSecPolicyNameIDValidationRecordSigning),
            errOut);

    // Check for an extension with " Apple ID Validation Record Signing" oid (1.2.840.113635.100.6.25)
    add_leaf_marker(options, &oidAppleCertExtensionAppleIDRecordValidationSigning);

    // and validate that intermediate has extension
	// Application Integration Intermediate Certificate (1.2.840.113635.100.6.2.3)
	// and also validate that intermediate has extension
	// System Integration 2 Intermediate Certificate (1.2.840.113635.100.6.2.10)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleID);
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntg2);

	// Ensure that revocation is checked (OCSP)
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationOCSP);

	require(result = SecPolicyCreate(kSecPolicyAppleIDValidationRecordSigning,
                                     kSecPolicyNameIDValidationRecordSigning, options), errOut);

errOut:
  CFReleaseSafe(options);
  return result;
}

/*!
 @function SecPolicyCreateAppleServerAuthCommon
 @abstract Generic policy for server authentication Sub CAs

 Allows control for both if pinning is required at all and if UAT environments should be added
 to the trust policy.

 No pinning is for developer/QA that needs to use proxy to debug the protocol, while UAT
 environment is for QA/internal developer that have no need allow fake servers.

 Both the noPinning and UAT are gated on that you run on internal hardware.

 */

static SecPolicyRef
SecPolicyCreateAppleServerAuthCommon(CFStringRef hostname,
                                     CFDictionaryRef __unused context,
                                     CFStringRef policyOID, CFStringRef service,
                                     const DERItem *leafMarkerOID,
                                     const DERItem *UATLeafMarkerOID)
{
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;
    CFDataRef oid = NULL, uatoid = NULL;

    options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(options, errOut);

    SecPolicyAddBasicX509Options(options);

    require(hostname, errOut);
    CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);

    CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);

    add_eku(options, &oidExtendedKeyUsageServerAuth);

    if (requireUATPinning(service)) {

        /*
         * Require pinning to the Apple CA's.
         */
        SecPolicyAddAppleAnchorOptions(options, service);

        /* old-style leaf marker OIDs */
        if (UATLeafMarkerOID) {
            add_leaf_prod_qa_markers(options, leafMarkerOID, UATLeafMarkerOID);
        } else {
            add_leaf_marker(options, leafMarkerOID);
        }

        /* new-style leaf marker OIDs */
        CFStringRef leafMarkerOIDStr = NULL, UATLeafMarkerOIDStr = NULL;
        leafMarkerOIDStr = SecDERItemCopyOIDDecimalRepresentation(NULL, leafMarkerOID);
        if (UATLeafMarkerOID) {
            UATLeafMarkerOIDStr = SecDERItemCopyOIDDecimalRepresentation(NULL, UATLeafMarkerOID);
        }

        if (leafMarkerOIDStr && UATLeafMarkerOIDStr) {
            add_leaf_prod_qa_markers_value_string(options,
                                                  CFSTR("1.2.840.113635.100.6.48.1"), leafMarkerOIDStr,
                                                  CFSTR("1.2.840.113635.100.6.48.1"), UATLeafMarkerOIDStr);
        } else if (leafMarkerOIDStr) {
            add_leaf_marker_value_string(options, CFSTR("1.2.840.113635.100.6.48.1"), leafMarkerOIDStr);
        }

        CFReleaseNull(leafMarkerOIDStr);
        CFReleaseNull(UATLeafMarkerOIDStr);

        add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleServerAuthentication);
    }

    /* Check for weak hashes and keys */
    require(SecPolicyRemoveWeakHashOptions(options), errOut);
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    result = SecPolicyCreate(policyOID, service, options);
    require(result, errOut);

errOut:
    CFReleaseSafe(options);
    CFReleaseSafe(oid);
    CFReleaseSafe(uatoid);
    return result;
}

/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleIDSService(CFStringRef hostname)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, NULL, kSecPolicyAppleIDSService,
                                                kSecPolicyNameAppleIDSBag,
                                                &oidAppleCertExtAppleServerAuthenticationIDSProd,
                                                &oidAppleCertExtAppleServerAuthenticationIDSProdQA);
}

/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleIDSServiceContext(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyAppleIDSServiceContext,
                                                kSecPolicyNameAppleIDSService,
                                                &oidAppleCertExtAppleServerAuthenticationIDSProd,
                                                &oidAppleCertExtAppleServerAuthenticationIDSProdQA);
}

/*!
 @function SecPolicyCreateAppleGSService
 @abstract Ensure we're appropriately pinned to the GS service
 */
SecPolicyRef SecPolicyCreateAppleGSService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyAppleGSService,
                                                kSecPolicyNameAppleGSService,
                                                &oidAppleCertExtAppleServerAuthenticationGS,
                                                NULL);
}

/*!
 @function SecPolicyCreateApplePushService
 @abstract Ensure we're appropriately pinned to the Push service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateApplePushService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyApplePushService,
                                                kSecPolicyNameApplePushService,
                                                &oidAppleCertExtAppleServerAuthenticationAPNProd,
                                                &oidAppleCertExtAppleServerAuthenticationAPNProdQA);
}

/*!
 @function SecPolicyCreateApplePPQService
 @abstract Ensure we're appropriately pinned to the PPQ service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateApplePPQService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyApplePPQService,
                                                kSecPolicyNameApplePPQService,
                                                &oidAppleCertExtAppleServerAuthenticationPPQProd ,
                                                &oidAppleCertExtAppleServerAuthenticationPPQProdQA);
}

/*!
 @function SecPolicyCreateAppleAST2Service
 @abstract Ensure we're appropriately pinned to the AST2 Diagnostic service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleAST2Service(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyAppleAST2DiagnosticsServerAuth,
                                                kSecPolicyNameAppleAST2Service,
                                                &oidAppleCertExtAST2DiagnosticsServerAuthProd,
                                                &oidAppleCertExtAST2DiagnosticsServerAuthProdQA);
}

/*!
 @function SecPolicyCreateAppleEscrowProxyService
 @abstract Ensure we're appropriately pinned to the iCloud Escrow Proxy service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleEscrowProxyService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyAppleEscrowProxyServerAuth,
                                                  kSecPolicyNameAppleEscrowProxyService,
                                                    &oidAppleCertExtEscrowProxyServerAuthProd,
                                                    &oidAppleCertExtEscrowProxyServerAuthProdQA);
}

/* subject:/C=US/O=GeoTrust Inc./CN=GeoTrust Global CA */
/* SKID: C0:7A:98:68:8D:89:FB:AB:05:64:0C:11:7D:AA:7D:65:B8:CA:CC:4E */
/* Not Before: May 21 04:00:00 2002 GMT, Not After : May 21 04:00:00 2022 GMT */
/* Signature Algorithm: sha1WithRSAEncryption */
unsigned char GeoTrust_Global_CA_sha256[kSecPolicySHA256Size] = {
    0xff, 0x85, 0x6a, 0x2d, 0x25, 0x1d, 0xcd, 0x88, 0xd3, 0x66, 0x56, 0xf4, 0x50, 0x12, 0x67, 0x98,
    0xcf, 0xab, 0xaa, 0xde, 0x40, 0x79, 0x9c, 0x72, 0x2d, 0xe4, 0xd2, 0xb5, 0xdb, 0x36, 0xa7, 0x3a
};

static SecPolicyRef SecPolicyCreateAppleGeoTrustServerAuthCommon(CFStringRef hostname, CFStringRef policyOid,
                                                                 CFStringRef policyName,
                                                                 CFStringRef leafMarkerOid,
                                                                 CFStringRef qaLeafMarkerOid) {
    CFMutableDictionaryRef options = NULL;
    CFDataRef spkiDigest = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* basic SSL */
    SecPolicyAddBasicX509Options(options);

    require(hostname, errOut);
    CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);

    add_eku(options, &oidExtendedKeyUsageServerAuth);

    /* pinning */
    if (requireUATPinning(policyName)) {
        /* GeoTrust root */
        SecPolicyAddAnchorSHA256Options(options, GeoTrust_Global_CA_sha256);

        /* Issued to Apple Inc. in the US */
        add_element(options, kSecPolicyCheckIntermediateCountry, CFSTR("US"));
        add_element(options, kSecPolicyCheckIntermediateOrganization, CFSTR("Apple Inc."));

        require_action(SecPolicyAddChainLengthOptions(options, 3), errOut, CFReleaseNull(result));

        /* Marker OIDs in both formats */
        if (qaLeafMarkerOid) {
            add_leaf_prod_qa_markers_string(options, leafMarkerOid, qaLeafMarkerOid);
            add_leaf_prod_qa_markers_value_string(options,
                                                  CFSTR("1.2.840.113635.100.6.48.1"), leafMarkerOid,
                                                  CFSTR("1.2.840.113635.100.6.48.1"), qaLeafMarkerOid);
        } else {
            add_leaf_marker_string(options, leafMarkerOid);
            add_leaf_marker_value_string(options, CFSTR("1.2.840.113635.100.6.48.1"), leafMarkerOid);
        }
    }

    /* Check for weak hashes */
    require(SecPolicyRemoveWeakHashOptions(options), errOut);
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    /* See <rdar://25344801> for more details */

    result = SecPolicyCreate(policyOid, policyName, options);

errOut:
    CFReleaseSafe(options);
    CFReleaseSafe(spkiDigest);
    return result;
}

SecPolicyRef SecPolicyCreateAppleCompatibilityEscrowProxyService(CFStringRef hostname) {
    return SecPolicyCreateAppleGeoTrustServerAuthCommon(hostname, kSecPolicyAppleEscrowProxyCompatibilityServerAuth,
                                                        kSecPolicyNameAppleEscrowProxyService,
                                                        CFSTR("1.2.840.113635.100.6.27.7.2"),
                                                        CFSTR("1.2.840.113635.100.6.27.7.1"));
}

/*!
 @function SecPolicyCreateAppleFMiPService
 @abstract Ensure we're appropriately pinned to the Find My iPhone service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleFMiPService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyAppleFMiPServerAuth,
                                                  kSecPolicyNameAppleFMiPService,
                                                  &oidAppleCertExtFMiPServerAuthProd,
                                                  &oidAppleCertExtFMiPServerAuthProdQA);
}


/* should use verbatim copy, but since this is the deprecated way, don't care right now */
static const UInt8 entrustSPKIL1C[kSecPolicySHA256Size] = {
    0x54, 0x5b, 0xf9, 0x35, 0xe9, 0xad, 0xa1, 0xda,
    0x11, 0x7e, 0xdc, 0x3c, 0x2a, 0xcb, 0xc5, 0x6f,
    0xc0, 0x28, 0x09, 0x6c, 0x0e, 0x24, 0xbe, 0x9b,
    0x38, 0x94, 0xbe, 0x52, 0x2d, 0x1b, 0x43, 0xde
};

/*!
 @function SecPolicyCreateApplePushServiceLegacy
 @abstract Ensure we're appropriately pinned to the Push service (via Entrust)
 */
SecPolicyRef SecPolicyCreateApplePushServiceLegacy(CFStringRef hostname)
{
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;
    CFDataRef digest = NULL;

    digest = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, entrustSPKIL1C, sizeof(entrustSPKIL1C), kCFAllocatorNull);
    require(digest, errOut);

    options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(options, errOut);

    SecPolicyAddBasicX509Options(options);

    CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);

    CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);

    CFDictionaryAddValue(options, kSecPolicyCheckIntermediateSPKISHA256, digest);

    add_eku(options, &oidExtendedKeyUsageServerAuth);

    /* Check for weak hashes and keys */
    require(SecPolicyRemoveWeakHashOptions(options), errOut);
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    result = SecPolicyCreate(kSecPolicyAppleLegacyPushService,
                             kSecPolicyNameLegacyPushService, options);
    require(result, errOut);

errOut:
    CFReleaseSafe(digest);
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateAppleMMCSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleMMCSService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyAppleMMCService,
                                                kSecPolicyNameAppleMMCSService,
                                                &oidAppleCertExtAppleServerAuthenticationMMCSProd,
                                                &oidAppleCertExtAppleServerAuthenticationMMCSProdQA);
}

SecPolicyRef SecPolicyCreateAppleCompatibilityMMCSService(CFStringRef hostname) {
    return SecPolicyCreateAppleGeoTrustServerAuthCommon(hostname, kSecPolicyAppleMMCSCompatibilityServerAuth,
                                                        kSecPolicyNameAppleMMCSService,
                                                        CFSTR("1.2.840.113635.100.6.27.11.2"),
                                                        CFSTR("1.2.840.113635.100.6.27.11.1"));
}


SecPolicyRef SecPolicyCreateAppleiCloudSetupService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, kSecPolicyAppleiCloudSetupServerAuth,
                                                kSecPolicyNameAppleiCloudSetupService,
                                                &oidAppleCertExtAppleServerAuthenticationiCloudSetupProd,
                                                &oidAppleCertExtAppleServerAuthenticationiCloudSetupProdQA);
}

SecPolicyRef SecPolicyCreateAppleCompatibilityiCloudSetupService(CFStringRef hostname)
{
    return SecPolicyCreateAppleGeoTrustServerAuthCommon(hostname, kSecPolicyAppleiCloudSetupCompatibilityServerAuth,
                                                        kSecPolicyNameAppleiCloudSetupService,
                                                        CFSTR("1.2.840.113635.100.6.27.15.2"),
                                                        CFSTR("1.2.840.113635.100.6.27.15.1"));
}

/*!
 @function SecPolicyCreateAppleSSLService
 @abstract Ensure we're appropriately pinned to an Apple server (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleSSLService(CFStringRef hostname)
{
	// SSL server, pinned to an Apple intermediate
	SecPolicyRef policy = SecPolicyCreateSSL(true, hostname);
	CFMutableDictionaryRef options = NULL;
	require(policy, errOut);

	// change options for SSL policy evaluation
	require((options=(CFMutableDictionaryRef)policy->_options) != NULL, errOut);

	// Apple CA anchored
	require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameServerAuthentication), errOut);

	// Check leaf for Apple Server Authentication marker oid (1.2.840.113635.100.6.27.1)
	add_leaf_marker(options, &oidAppleCertExtAppleServerAuthentication);

	// Check intermediate for Apple Server Authentication intermediate marker (1.2.840.113635.100.6.2.12)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleServerAuthentication);

    /* Check for weak hashes */
    require(SecPolicyRemoveWeakHashOptions(options), errOut);

	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    SecPolicySetOid(policy, kSecPolicyAppleServerAuthentication);
    SecPolicySetName(policy, kSecPolicyNameServerAuthentication);

	return policy;

errOut:
	CFReleaseSafe(options);
	CFReleaseSafe(policy);
	return NULL;
}

/*!
 @function SecPolicyCreateApplePPQSigning
 @abstract Check for intermediate certificate 'Apple System Integration 2 Certification Authority' by name,
 and apple anchor.
 Leaf cert must have Digital Signature usage.
 Leaf cert must have Apple PPQ Signing marker OID (1.2.840.113635.100.6.38.2).
 Intermediate must have marker OID (1.2.840.113635.100.6.2.10).
 */
SecPolicyRef SecPolicyCreateApplePPQSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicCertOptions(options);

    SecPolicyAddAppleAnchorOptions(options, kSecPolicyNamePPQSigning);
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple System Integration 2 Certification Authority"));

    // Check that leaf has extension with "Apple PPQ Signing" prod oid (1.2.840.113635.100.6.38.2)
    add_leaf_marker(options, &oidAppleCertExtApplePPQSigningProd);

    // Check that intermediate has extension (1.2.840.113635.100.6.2.10)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntg2);

    add_ku(options, kSecKeyUsageDigitalSignature);

    require(result = SecPolicyCreate(kSecPolicyApplePPQSigning,
                                     kSecPolicyNamePPQSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateTestApplePPQSigning
 @abstract Check for intermediate certificate 'Apple System Integration 2 Certification Authority' by name,
 and apple anchor.
 Leaf cert must have Digital Signature usage.
 Leaf cert must have Apple PPQ Signing Test marker OID (1.2.840.113635.100.6.38.1).
 Intermediate must have marker OID (1.2.840.113635.100.6.2.10).
 */
SecPolicyRef SecPolicyCreateTestApplePPQSigning(void)
{
    /* Guard against use of test policy on production devices */
    if (!SecIsInternalRelease()) {
        return SecPolicyCreateApplePPQSigning();
    }

    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicCertOptions(options);

    SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameTestPPQSigning);
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple System Integration 2 Certification Authority"));

    // Check that leaf has extension with "Apple PPQ Signing" test oid (1.2.840.113635.100.6.38.1)
    add_leaf_marker(options, &oidAppleCertExtApplePPQSigningProdQA);

    // Check that intermediate has extension (1.2.840.113635.100.6.2.10)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntg2);

    add_ku(options, kSecKeyUsageDigitalSignature);

    require(result = SecPolicyCreate(kSecPolicyAppleTestPPQSigning,
                                     kSecPolicyNameTestPPQSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}
/*!
 @function SecPolicyCreateAppleTimeStamping
 @abstract Check for RFC3161 timestamping EKU.
 */
SecPolicyRef SecPolicyCreateAppleTimeStamping(void)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

	SecPolicyAddBasicX509Options(options);

	/* Require id-kp-timeStamping extendedKeyUsage to be present. */
	add_eku(options, &oidExtendedKeyUsageTimeStamping);

	require(result = SecPolicyCreate(kSecPolicyAppleTimeStamping,
                                     kSecPolicyNameTimeStamping, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

/*!
 @function SecPolicyCreateApplePayIssuerEncryption
 @abstract Check for intermediate certificate 'Apple Worldwide Developer Relations CA - G2' by name,
 and ECC apple anchor.
 Leaf cert must have Key Encipherment and Key Agreement usage.
 Leaf cert must have Apple Pay Issuer Encryption marker OID (1.2.840.113635.100.6.39).
 */
SecPolicyRef SecPolicyCreateApplePayIssuerEncryption(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNamePayIssuerEncryption),
            errOut);
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple Worldwide Developer Relations CA - G2"));

    // Check that leaf has extension with "Apple Pay Issuer Encryption" oid (1.2.840.113635.100.6.39)
    add_leaf_marker(options, &oidAppleCertExtCryptoServicesExtEncryption);

    add_ku(options, kSecKeyUsageKeyEncipherment);

    require(result = SecPolicyCreate(kSecPolicyApplePayIssuerEncryption,
                                     kSecPolicyNamePayIssuerEncryption, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateAppleATVVPNProfileSigning
 @abstract Check for leaf marker OID 1.2.840.113635.100.6.43,
 intermediate marker OID 1.2.840.113635.100.6.2.10,
 chains to Apple Root CA, path length 3
 */
SecPolicyRef SecPolicyCreateAppleATVVPNProfileSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    CFMutableDictionaryRef appleAnchorOptions = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    
    SecPolicyAddBasicCertOptions(options);
    
    // Require pinning to the Apple CAs (including test CA for internal releases)
    appleAnchorOptions = CFDictionaryCreateMutableForCFTypes(NULL);
    require(appleAnchorOptions, errOut);
    
    if (SecIsInternalRelease()) {
        CFDictionarySetValue(appleAnchorOptions,
                             kSecPolicyAppleAnchorIncludeTestRoots, kCFBooleanTrue);
    }
    
    add_element(options, kSecPolicyCheckAnchorApple, appleAnchorOptions);
    
    // Cert chain length 3
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    
    // Check leaf for Apple ATV VPN Profile Signing OID (1.2.840.113635.100.6.43)
    add_leaf_marker(options, &oidAppleCertExtATVVPNProfileSigning);
    
    // Check intermediate for Apple System Integration 2 CA intermediate marker (1.2.840.113635.100.6.2.10)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntg2);
    
    // Ensure that revocation is checked (OCSP only)
    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationOCSP);
    
    require(result = SecPolicyCreate(kSecPolicyAppleATVVPNProfileSigning,
                                     kSecPolicyNameATVVPNProfileSigning, options), errOut);
    
errOut:
    CFReleaseSafe(options);
    CFReleaseSafe(appleAnchorOptions);
    return result;
}

SecPolicyRef SecPolicyCreateAppleHomeKitServerAuth(CFStringRef hostname) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;
    CFDataRef oid = NULL;

    options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(options, errOut);

    SecPolicyAddBasicX509Options(options);

    CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);

    add_eku(options, &oidExtendedKeyUsageServerAuth);

    if (requireUATPinning(kSecPolicyNameAppleHomeKitService)) {

        // Cert chain length 3
        require(SecPolicyAddChainLengthOptions(options, 3), errOut);

        // Apple anchors, allowing test anchors for internal release
        SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameAppleHomeKitService);

        add_leaf_marker(options, &oidAppleCertExtHomeKitServerAuth);

        add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleHomeKitServerCA);
    }

    /* Check for weak hashes */
    require(SecPolicyRemoveWeakHashOptions(options), errOut);
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    result = SecPolicyCreate(kSecPolicyAppleHomeKitServerAuth,
                             kSecPolicyNameAppleHomeKitService, options);
    require(result, errOut);

errOut:
    CFReleaseSafe(options);
    CFReleaseSafe(oid);
    return result;
}

SecPolicyRef SecPolicyCreateAppleExternalDeveloper(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    /* Create basic Apple pinned policy */
    require(result = SecPolicyCreateApplePinned(kSecPolicyNameExternalDeveloper,
                                                CFSTR("1.2.840.113635.100.6.2.1"),  // WWDR Intermediate OID
                                                CFSTR("1.2.840.113635.100.6.1.2")), // "iPhone Developer" leaf OID
            errOut);

    require_action(options = CFDictionaryCreateMutableCopy(NULL, 0, result->_options), errOut, CFReleaseNull(result));

    /* Additional intermediate OIDs */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid,
                CFSTR("1.2.840.113635.100.6.2.6")); // "Developer ID" Intermediate OID

    /* Addtional leaf OIDS */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.4"));  // "iPhone Distribution" leaf OID
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.5"));  // "Safari Developer" leaf OID
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.7"));  // "3rd Party Mac Developer Application" leaf OID
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.8"));  // "3rd Party Mac Developer Installer" leaf OID
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.12")); // "Mac Developer" leaf OID
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.13")); // "Developer ID Application" leaf OID
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.14")); // "Developer ID Installer" leaf OID

    /* Restrict EKUs */
    add_eku_string(options, CFSTR("1.3.6.1.5.5.7.3.3"));       // CodeSigning EKU
    add_eku_string(options, CFSTR("1.2.840.113635.100.4.8"));  // "Safari Developer" EKU
    add_eku_string(options, CFSTR("1.2.840.113635.100.4.9"));  // "3rd Party Mac Developer Installer" EKU
    add_eku_string(options, CFSTR("1.2.840.113635.100.4.13")); // "Developer ID Installer" EKU

    CFReleaseSafe(result->_options);
    result->_options = CFRetainSafe(options);

    SecPolicySetOid(result, kSecPolicyAppleExternalDeveloper);

errOut:
    CFReleaseSafe(options);
    return result;
}

/* This one is special because the intermediate has no marker OID */
SecPolicyRef SecPolicyCreateAppleSoftwareSigning(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    /* Anchored to the Apple Roots */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameSoftwareSigning),
                  errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate Common Name matches */
    add_element(options, kSecPolicyCheckIssuerCommonName, CFSTR("Apple Code Signing Certification Authority"));

    /* Leaf marker OID matches */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.22"));

    /* Leaf has CodeSigning EKU */
    add_eku_string(options, CFSTR("1.3.6.1.5.5.7.3.3"));

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleSoftwareSigning,
                                     kSecPolicyNameSoftwareSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/* subject:/CN=SEP Root CA/O=Apple Inc./ST=California */
/* SKID: 58:EF:D6:BE:C5:82:B0:54:CD:18:A6:84:AD:A2:F6:7B:7B:3A:7F:CF */
/* Not Before: Jun 24 21:43:24 2014 GMT, Not After : Jun 24 21:43:24 2029 GMT */
/* Signature Algorithm: ecdsa-with-SHA384 */
const uint8_t SEPRootCA_SHA256[kSecPolicySHA256Size] = {
    0xd1, 0xdf, 0x82, 0x00, 0xf3, 0x89, 0x4e, 0xe9, 0x96, 0xf3, 0x77, 0xdf, 0x76, 0x3b, 0x0a, 0x16,
    0x8f, 0xd9, 0x6c, 0x58, 0xc0, 0x3e, 0xc9, 0xb0, 0x5f, 0xa5, 0x64, 0x79, 0xc0, 0xe8, 0xc9, 0xe7
};

SecPolicyRef SecPolicyCreateAppleUniqueDeviceCertificate(CFDataRef testRootHash) {
    CFMutableDictionaryRef options = NULL;
    CFDictionaryRef keySizes = NULL;
    CFNumberRef ecSize = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* Device certificate should never expire */
    SecPolicyAddBasicCertOptions(options);

    /* Anchored to the SEP Root CA. Allow alternative root for developers */
    require(SecPolicyAddAnchorSHA256Options(options, SEPRootCA_SHA256),errOut);
    if (testRootHash && SecIsInternalRelease() && (kSecPolicySHA256Size == CFDataGetLength(testRootHash))) {
        add_element(options, kSecPolicyCheckAnchorSHA256, testRootHash);
    }

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate has marker OID with value */
    add_intermediate_marker_value_string(options, CFSTR("1.2.840.113635.100.6.44"), CFSTR("ucrt"));

    /* Leaf has marker OID with varying value that can't be pre-determined */
    add_element(options, kSecPolicyCheckLeafMarkerOidWithoutValueCheck, CFSTR("1.2.840.113635.100.10.1"));

    /* RSA key sizes are disallowed. EC key sizes are P-256 or larger. */
    require(ecSize = CFNumberCreateWithCFIndex(NULL, 256), errOut);
    require(keySizes = CFDictionaryCreate(NULL, (const void**)&kSecAttrKeyTypeEC,
                                          (const void**)&ecSize, 1,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks), errOut);
    add_element(options, kSecPolicyCheckKeySize, keySizes);


    require(result = SecPolicyCreate(kSecPolicyAppleUniqueDeviceIdentifierCertificate,
                                     kSecPolicyNameUniqueDeviceIdentifierCertificate, options), errOut);

errOut:
    CFReleaseSafe(options);
    CFReleaseSafe(keySizes);
    CFReleaseSafe(ecSize);
    return result;
}

SecPolicyRef SecPolicyCreateAppleWarsaw(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* Anchored to the Apple Roots. */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameWarsaw),
                  errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID matches input OID */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.14"));

    /* Leaf marker OID matches input OID */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.29"));

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleWarsaw,
                                     kSecPolicyNameWarsaw, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleSecureIOStaticAsset(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;
#if TARGET_OS_BRIDGE
    CFMutableDictionaryRef appleAnchorOptions = NULL;
#endif

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* This certificate cannot expire so that assets always load */
    SecPolicyAddBasicCertOptions(options);

    /* Anchored to the Apple Roots. */
#if TARGET_OS_BRIDGE
    /* On the bridge, test roots are gated in the trust and policy servers. */
    require_quiet(appleAnchorOptions = CFDictionaryCreateMutableForCFTypes(NULL), errOut);
    CFDictionarySetValue(appleAnchorOptions,
                         kSecPolicyAppleAnchorIncludeTestRoots, kCFBooleanTrue);
    add_element(options, kSecPolicyCheckAnchorApple, appleAnchorOptions);
    CFReleaseSafe(appleAnchorOptions);
#else
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameSecureIOStaticAsset),
                  errOut);
#endif

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID matches ASI CA */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.10"));

    /* Leaf marker OID matches static IO OID */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.50"));

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleSecureIOStaticAsset,
                                     kSecPolicyNameSecureIOStaticAsset, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleAppTransportSecurity(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;
    CFMutableArrayRef disallowedHashes = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    /* Hash algorithm is SHA-256 or better */
    require(disallowedHashes = CFArrayCreateMutable(NULL, 5, &kCFTypeArrayCallBacks), errOut);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD2);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD4);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD5);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmSHA1);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmSHA224);

    add_element(options, kSecPolicyCheckSignatureHashAlgorithms, disallowedHashes);

    require_quiet(result = SecPolicyCreate(kSecPolicyAppleAppTransportSecurity,
                                           kSecPolicyNameAppTransportSecurity,
                                           options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateMobileSoftwareUpdate(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    /* No expiration check. */
    SecPolicyAddBasicCertOptions(options);

    /* Apple Anchor */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameMobileSoftwareUpdate),
                  errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID is iPhone CA OID */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.18"));

    /* Leaf marker OID is either the prod MSU OID, or, on internal builds, the prodQA OID */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.57.2"));
    if (SecIsInternalRelease()) {
        add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.57.1"));
    }

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleMobileSoftwareUpdate,
                                     kSecPolicyNameMobileSoftwareUpdate, options), errOut);

errOut:
    CFReleaseNull(options);
    return result;
}

/* subject:/CN=Basic Attestation System Root CA/O=Apple Inc./ST=California */
/* SKID: FE:D1:D1:C2:08:07:03:D5:B9:3C:34:B2:BB:FD:7C:3A:99:25:1B:8F */
/* Not Before: Apr 20 00:22:09 2017 GMT, Not After : Mar 22 00:00:00 2032 GMT */
/* Signature Algorithm: ecdsa-with-SHA384 */
const uint8_t BASystemRootCA_SHA256[kSecPolicySHA256Size] = {
    0x10, 0xD3, 0xC8, 0x67, 0xF0, 0xAE, 0xFB, 0x24, 0x31, 0xA0, 0xA9, 0x7A, 0x88, 0x18, 0xBD, 0x64,
    0xF7, 0xF9, 0x0F, 0xFE, 0x11, 0x94, 0x48, 0x4F, 0xCA, 0x97, 0xF0, 0xF2, 0x9E, 0xCA, 0x00, 0x47
};

/* subject:/CN=Basic Attestation User Root CA/O=Apple Inc./ST=California */
/* SKID: 83:E5:A3:21:9E:B0:74:C3:F9:61:90:FD:97:4E:23:10:76:A4:A3:F2 */
/* Not Before: Apr 19 21:41:56 2017 GMT, Not After : Mar 22 00:00:00 2032 GMT */
/* Signature Algorithm: ecdsa-with-SHA384 */
const uint8_t BAUserRootCA_SHA256[kSecPolicySHA256Size] = {
    0x03, 0x75, 0x1c, 0x80, 0xfc, 0xbe, 0x58, 0x19, 0xd1, 0x70, 0xd2, 0x67, 0xce, 0x1a, 0xd6, 0xd0,
    0x94, 0x40, 0x7c, 0x91, 0xd8, 0x73, 0xd7, 0xa6, 0x56, 0x2d, 0xe3, 0x66, 0x6d, 0x35, 0x94, 0xc6
};

SecPolicyRef SecPolicyCreateAppleBasicAttestationSystem(CFDataRef testRootHash) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    /* BAA certs expire */
    SecPolicyAddBasicX509Options(options);

    /* Anchored to one of the Basic Attestation roots. Allow alternative root for developers */
    SecPolicyAddAnchorSHA256Options(options, BASystemRootCA_SHA256);
    if (testRootHash && SecIsInternalRelease() && (kSecPolicySHA256Size == CFDataGetLength(testRootHash))) {
        add_element(options, kSecPolicyCheckAnchorSHA256, testRootHash);
    }

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleBasicAttestationSystem,
                                     kSecPolicyNameBasicAttestationSystem, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleBasicAttestationUser(CFDataRef testRootHash) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    /* BAA certs expire */
    SecPolicyAddBasicX509Options(options);

    /* Anchored to one of the Basic Attestation roots. Allow alternative root for developers */
    SecPolicyAddAnchorSHA256Options(options, BAUserRootCA_SHA256);
    if (testRootHash && SecIsInternalRelease() && (kSecPolicySHA256Size == CFDataGetLength(testRootHash))) {
        add_element(options, kSecPolicyCheckAnchorSHA256, testRootHash);
    }

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleBasicAttestationUser,
                                     kSecPolicyNameBasicAttestationUser, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateiAPSWAuthWithExpiration(bool checkExpiration) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* iAP checks expiration on developement certs, but not on production certs */
    if (checkExpiration) {
        SecPolicyAddBasicX509Options(options);
    } else {
        SecPolicyAddBasicCertOptions(options);
    }

    /* Exactly 2 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 2), errOut);

    /* iAP SW Auth General Capabilities Extension present */
    add_element(options, kSecPolicyCheckLeafMarkerOidWithoutValueCheck, CFSTR("1.2.840.113635.100.6.59.1"));

    require(result = SecPolicyCreate(kSecPolicyAppleiAPSWAuth,
                                     kSecPolicyNameiAPSWAuth, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateiAPSWAuth(void) {
    /* By default, iAP SW Auth certs don't expire */
    return SecPolicyCreateiAPSWAuthWithExpiration(false);
}

SecPolicyRef SecPolicyCreateDemoDigitalCatalogSigning(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicX509Options(options);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Demo Signing Extension present in leaf */
    add_element(options, kSecPolicyCheckLeafMarkerOid, CFSTR("1.2.840.113635.100.6.60"));

    /* Issuer common name is "DemoUnit CA" */
    add_element(options, kSecPolicyCheckIssuerCommonName, CFSTR("DemoUnit CA"));

    require(result = SecPolicyCreate(kSecPolicyAppleDemoDigitalCatalog,
                                     kSecPolicyNameDemoDigitalCatalog, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleAssetReceipt(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* No expiration check. */
    SecPolicyAddBasicCertOptions(options);

    /* Apple Anchor */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameAssetReceipt), errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID is Apple System Integration 2 CA */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.10"));

    /* Leaf marker OID is the Asset Receipt OID */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.61"));

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleAssetReceipt,
                                     kSecPolicyNameAssetReceipt, options), errOut);

errOut:
    CFReleaseNull(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleDeveloperIDPlusTicket(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* No expiration check. */
    SecPolicyAddBasicCertOptions(options);

    /* Apple Anchor */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameDeveloperIDPlusTicket), errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID is Apple System Integration CA 4 */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.17"));

    /* Leaf marker OID is the Developer ID+ Ticket OID */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.1.30"));

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleDeveloperIDPlusTicket,
                                     kSecPolicyNameDeveloperIDPlusTicket, options), errOut);

errOut:
    CFReleaseNull(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleFDRProvisioning(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* No expiration check. */
    SecPolicyAddBasicCertOptions(options);

    require(result = SecPolicyCreate(kSecPolicyAppleFDRProvisioning,
                                     kSecPolicyNameFDRProvisioning, options), errOut);
errOut:
    CFReleaseNull(options);
    return result;
}

/* subject:/CN=Component Root CA/O=Apple Inc./ST=California */
/* SKID: 90:D1:56:A9:3E:B4:EE:8C:D0:10:4B:9F:17:1C:5B:55:F2:12:F6:4C */
/* Not Before: Dec 19 19:31:54 2018 GMT, Not After : Dec 16 00:00:00 2043 GMT */
/* Signature Algorithm: ecdsa-with-SHA384 */
const uint8_t ComponentRootCA_SHA256[kSecPolicySHA256Size] = {
    0x2f, 0x71, 0x9d, 0xbf, 0x3c, 0xcd, 0xe7, 0xc2, 0xb2, 0x59, 0x3f, 0x32, 0x1f, 0x90, 0xf3, 0x42,
    0x42, 0xaa, 0x84, 0xa1, 0xb2, 0x0d, 0xca, 0xcc, 0x10, 0xc0, 0x5b, 0x26, 0xd6, 0x23, 0xb8, 0x47,
};
SecPolicyRef SecPolicyCreateAppleComponentCertificate(CFDataRef testRootHash) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* Component certificates don't expire */
    SecPolicyAddBasicCertOptions(options);

    /* Anchored to one of the Component roots. Allow alternative root for developers */
    SecPolicyAddAnchorSHA256Options(options, ComponentRootCA_SHA256);
    if (testRootHash && SecIsInternalRelease() && (kSecPolicySHA256Size == CFDataGetLength(testRootHash))) {
        add_element(options, kSecPolicyCheckAnchorSHA256, testRootHash);
    }

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Leaf and intermediate must contain Component Type OID */
    add_element(options, kSecPolicyCheckLeafMarkerOidWithoutValueCheck, CFSTR("1.2.840.113635.100.11.1"));
    add_element(options, kSecPolicyCheckIntermediateMarkerOidWithoutValueCheck, CFSTR("1.2.840.113635.100.11.1"));

    require(result = SecPolicyCreate(kSecPolicyAppleComponentCertificate,
                                     kSecPolicyNameComponentCertificate, options), errOut);
errOut:
    CFReleaseNull(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleKeyTransparency(CFStringRef applicationId) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* KT signing certs don't expire */
    SecPolicyAddBasicCertOptions(options);

    /* Apple Anchor */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameKeyTransparency), errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID matches AAI CA 5 */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.3"));

    /* Leaf marker extension matches input applicationId */
    add_leaf_marker_value_string(options, CFSTR("1.2.840.113635.100.12.4"), applicationId);

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    /* Check for weak hashes */
    require(SecPolicyRemoveWeakHashOptions(options), errOut);

    /* Future CT requirement */

    require(result = SecPolicyCreate(kSecPolicyAppleKeyTransparency,
                                     kSecPolicyNameKeyTransparency, options), errOut);
errOut:
    CFReleaseNull(options);
    return result;
}

SecPolicyRef SecPolicyCreateAlisha(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;
    CFDictionaryRef keySizes = NULL;
    CFNumberRef ecSize = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* Alisha certs don't expire */
    SecPolicyAddBasicCertOptions(options);

    /* RSA key sizes are disallowed. EC key sizes are P-256 or larger. */
    require(ecSize = CFNumberCreateWithCFIndex(NULL, 256), errOut);
    require(keySizes = CFDictionaryCreate(NULL, (const void**)&kSecAttrKeyTypeEC,
                                          (const void**)&ecSize, 1,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks), errOut);
    add_element(options, kSecPolicyCheckKeySize, keySizes);

    /* Check for weak hashes */
    require(SecPolicyRemoveWeakHashOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleAlisha,
                                     kSecPolicyNameAlisha, options), errOut);
errOut:
    CFReleaseNull(options);
    CFReleaseNull(keySizes);
    CFReleaseNull(ecSize);
    return result;
}

SecPolicyRef SecPolicyCreateMeasuredBootPolicySigning(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* No expiration check. */
    SecPolicyAddBasicCertOptions(options);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Corporate Signing subCA */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.24.17"));

    /* Measured Boot Policy Signing Leaf OID */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.6.26.6.1"));

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleMeasuredBootPolicySigning,
                                     kSecPolicyNameMeasuredBootPolicySigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/* subject:/CN=Apple External EC Root/O=Apple Inc./C=US */
/* SKID: 3F:A4:C0:94:20:70:CB:3B:DD:A8:54:E6:14:1E:29:CC:4D:14:38:53 */
/* Not Before: Jan 23 00:46:48 2020 GMT, Not After : Jan 18 00:00:00 2045 GMT */
/* Signature Algorithm: ecdsa-with-SHA384 */
const uint8_t AppleExternalECRoot_SHA256[kSecPolicySHA256Size] = {
    0x72, 0x56, 0x6e, 0x6f, 0x66, 0x30, 0x0c, 0xfd, 0x24, 0xe5, 0xe6, 0x85, 0xa2, 0xf1, 0x5a, 0x74,
    0x9d, 0xe0, 0x4b, 0xb0, 0x38, 0x50, 0x77, 0x91, 0x96, 0x63, 0x6e, 0x07, 0x23, 0x0f, 0x91, 0x1e
};
/* subject:/CN=Test Apple External EC Root/O=Apple Inc./C=US */
/* SKID: 07:6B:07:47:33:E4:96:B4:FC:6F:FA:32:2C:8E:BE:70:C2:8F:80:3C */
/* Not Before: Nov  5 18:00:46 2019 GMT, Not After : Oct 29 18:00:46 2044 GMT */
/* Signature Algorithm: ecdsa-with-SHA384 */
const uint8_t TestAppleExternalECRoot_SHA256[kSecPolicySHA256Size] = {
    0xf3, 0x98, 0x39, 0xdc, 0x6a, 0x64, 0xf6, 0xe3, 0xa0, 0xdc, 0x97, 0xd7, 0x83, 0x61, 0x6b, 0x84,
    0x9f, 0xdf, 0xa1, 0x70, 0x54, 0x59, 0xae, 0x96, 0x0f, 0x41, 0xe1, 0x16, 0xa3, 0xb4, 0x8b, 0xb5
};
SecPolicyRef SecPolicyCreateApplePayQRCodeEncryption(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* Check expiration */
    SecPolicyAddBasicX509Options(options);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Apple External EC CA 1 - G1 */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.22"));

    /* ApplePay QR Code Encryption */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.13.3"));

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(SecPolicyAddAnchorSHA256Options(options, AppleExternalECRoot_SHA256),errOut);
    if (SecIsInternalRelease()) {
        require(SecPolicyAddAnchorSHA256Options(options, TestAppleExternalECRoot_SHA256),errOut);
    }

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    require(result = SecPolicyCreate(kSecPolicyApplePayQRCodeEncryption,
                                     kSecPolicyNamePayQRCodeEncryption, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateApplePayQRCodeSigning(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* Check expiration */
    SecPolicyAddBasicX509Options(options);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Apple External EC CA 1 - G1 */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.22"));

    /* ApplePay QR Code Signing */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.12.12"));

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(SecPolicyAddAnchorSHA256Options(options, AppleExternalECRoot_SHA256),errOut);
    if (SecIsInternalRelease()) {
        require(SecPolicyAddAnchorSHA256Options(options, TestAppleExternalECRoot_SHA256),errOut);
    }

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    require(result = SecPolicyCreate(kSecPolicyApplePayQRCodeSigning,
                                     kSecPolicyNamePayQRCodeSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateAppleAccessoryUpdateSigning(void) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    /* No expiration check */
    SecPolicyAddBasicCertOptions(options);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Apple Anchor */
    require_quiet(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameAccessoryUpdateSigning), errOut);

    /* Apple External EC CA 1 - G1 */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.17"));

    /* Accessory Manufacturer Firmware Signing Prod */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.12.9"));
    if (isCFPreferenceInSecurityDomain(CFSTR("AllowAccessoryUpdateSigningBeta"))) {
        add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.12.10")); // ProdQA
    }

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    require(result = SecPolicyCreate(kSecPolicyAppleAccessoryUpdateSigning,
                                     kSecPolicyNameAccessoryUpdateSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

CF_RETURNS_RETAINED SecPolicyRef SecPolicyCreateEscrowServiceIdKeySigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    // X509, ignoring date validity
    SecPolicyAddBasicCertOptions(options);

    add_ku(options, kSecKeyUsageDigitalSignature);

    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                         CFSTR("Escrow Service ID Key"));

    /* Exactly 2 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 2), errOut);

    require(result = SecPolicyCreate(kSecPolicyAppleEscrowServiceIdKeySigning,
                                     kSecPolicyNameEscrowServiceIdKeySigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

CF_RETURNS_RETAINED SecPolicyRef SecPolicyCreatePCSEscrowServiceIdKeySigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);
    add_ku(options, kSecKeyUsageDigitalSignature);

    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                         CFSTR("Effaceable Service ID Key"));

    /* Exactly 2 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 2), errOut);

    require(result = SecPolicyCreate(kSecPolicyApplePCSEscrowServiceIdKeySigning,
                                     kSecPolicyNamePCSEscrowServiceIdKeySigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateAggregateMetricTransparency(bool facilitator)
{
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* Anchored to the Apple Roots */
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameAggregateMetricTransparency), errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID matches AAICA 6 */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.26"));

    /* Leaf marker OID matches expected OID for either Facilitator or Partner */
    if (facilitator) {
        add_element(options, kSecPolicyCheckLeafMarkerOidWithoutValueCheck, CFSTR("1.2.840.113635.100.12.17"));
    } else {
        add_element(options, kSecPolicyCheckLeafMarkerOidWithoutValueCheck, CFSTR("1.2.840.113635.100.12.18"));
    }

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    /* Require CT */
    if (!SecIsInternalRelease() || !isCFPreferenceInSecurityDomain(CFSTR("disableAggregateMetricsCTCheck"))) {
        add_element(options, kSecPolicyCheckCTRequired, kCFBooleanTrue);
    }

    /* Check for weak hashes */
    // require(SecPolicyRemoveWeakHashOptions(options), errOut); // the current WWDR CA cert is signed with SHA1
    require(result = SecPolicyCreate(kSecPolicyAppleAggregateMetricTransparency,
                                     kSecPolicyNameAggregateMetricTransparency, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateAggregateMetricEncryption(bool facilitator)
{
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* Anchored to the Apple Roots */
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNameAggregateMetricEncryption), errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID matches AAICA 6 */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.26"));

    /* Leaf marker OID matches expected OID for either Facilitator or Partner */
    if (facilitator) {
        add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.15.2"));
    } else {
        add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.15.3"));
    }

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    /* Require CT */
    if (!SecIsInternalRelease() || !isCFPreferenceInSecurityDomain(CFSTR("disableAggregateMetricsCTCheck"))) {
        add_element(options, kSecPolicyCheckNonTlsCTRequired, kCFBooleanTrue);
    }

    require(result = SecPolicyCreate(kSecPolicyAppleAggregateMetricEncryption,
                                     kSecPolicyNameAggregateMetricEncryption, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateApplePayModelSigning(bool checkExpiration) {
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    if (checkExpiration) {
        SecPolicyAddBasicX509Options(options);
    } else {
        SecPolicyAddBasicCertOptions(options);
    }

    /* Anchored to the Apple Roots */
    require(SecPolicyAddAppleAnchorOptions(options, kSecPolicyNamePayModelSigning), errOut);

    /* Exactly 3 certs in the chain */
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    /* Intermediate marker OID is Apple System Integration CA 4 */
    add_element(options, kSecPolicyCheckIntermediateMarkerOid, CFSTR("1.2.840.113635.100.6.2.17"));

    /* Leaf marker OID for ApplePay Model Signing */
    add_leaf_marker_string(options, CFSTR("1.2.840.113635.100.12.20"));

    /* Check revocation using any available method */
    add_element(options, kSecPolicyCheckRevocation, kSecPolicyCheckRevocationAny);

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger. */
    require(SecPolicyAddStrongKeySizeOptions(options), errOut);

    require(result = SecPolicyCreate(kSecPolicyApplePayModelSigning,
                                     kSecPolicyNamePayModelSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}
