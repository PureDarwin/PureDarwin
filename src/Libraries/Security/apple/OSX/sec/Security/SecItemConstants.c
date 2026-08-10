/*
 * Copyright (c) 2006-2008,2010-2014 Apple Inc. All Rights Reserved.
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

#define __CONSTANT_CFSTRINGS__  1
#include <CoreFoundation/CFString.h>

/* String constant declarations */

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

/* Class Key Constant */
SEC_CONST_DECL (kSecClass, "class");

/* Class Value Constants */
SEC_CONST_DECL (kSecClassGenericPassword, "genp");
SEC_CONST_DECL (kSecClassInternetPassword, "inet");
SEC_CONST_DECL (kSecClassAppleSharePassword, "apls");
SEC_CONST_DECL (kSecClassCertificate, "cert");
SEC_CONST_DECL (kSecClassKey, "keys");
SEC_CONST_DECL (kSecClassIdentity, "idnt");

/* Attribute Key Constants */
SEC_CONST_DECL (kSecAttrAccessible, "pdmn");
SEC_CONST_DECL (kSecAttrAccessControl, "accc");
SEC_CONST_DECL (kSecAttrAccessGroup, "agrp");
SEC_CONST_DECL (kSecAttrCreationDate, "cdat");
SEC_CONST_DECL (kSecAttrModificationDate, "mdat");
SEC_CONST_DECL (kSecAttrDescription, "desc");
SEC_CONST_DECL (kSecAttrComment, "icmt");
SEC_CONST_DECL (kSecAttrCreator, "crtr");
SEC_CONST_DECL (kSecAttrType, "type");
SEC_CONST_DECL (kSecAttrLabel, "labl");
SEC_CONST_DECL (kSecAttrIsInvisible, "invi");
SEC_CONST_DECL (kSecAttrIsNegative, "nega");
SEC_CONST_DECL (kSecAttrAccount, "acct");
SEC_CONST_DECL (kSecAttrService, "svce");
SEC_CONST_DECL (kSecAttrGeneric, "gena");
SEC_CONST_DECL (kSecAttrSecurityDomain, "sdmn");
SEC_CONST_DECL (kSecAttrServer, "srvr");
SEC_CONST_DECL (kSecAttrProtocol, "ptcl");
SEC_CONST_DECL (kSecAttrAuthenticationType, "atyp");
SEC_CONST_DECL (kSecAttrPort, "port");
SEC_CONST_DECL (kSecAttrPath, "path");
SEC_CONST_DECL (kSecAttrVolume, "volm");
SEC_CONST_DECL (kSecAttrAddress, "addr");
SEC_CONST_DECL (kSecAttrAFPServerSignature, "afps");
SEC_CONST_DECL (kSecAttrAlias, "alis");
SEC_CONST_DECL (kSecAttrSubject, "subj");
SEC_CONST_DECL (kSecAttrIssuer, "issr");
SEC_CONST_DECL (kSecAttrSerialNumber, "slnr");
SEC_CONST_DECL (kSecAttrSubjectKeyID, "skid");
SEC_CONST_DECL (kSecAttrPublicKeyHash, "pkhh");
SEC_CONST_DECL (kSecAttrCertificateType, "ctyp");
SEC_CONST_DECL (kSecAttrCertificateEncoding, "cenc");
SEC_CONST_DECL (kSecAttrKeyClass, "kcls");
SEC_CONST_DECL (kSecAttrApplicationLabel, "klbl");
SEC_CONST_DECL (kSecAttrIsPermanent, "perm");
SEC_CONST_DECL (kSecAttrIsPrivate, "priv");
SEC_CONST_DECL (kSecAttrIsModifiable, "modi");
SEC_CONST_DECL (kSecAttrApplicationTag, "atag");
SEC_CONST_DECL (kSecAttrKeyCreator, "crtr");
SEC_CONST_DECL (kSecAttrKeyType, "type");
SEC_CONST_DECL (kSecAttrKeySizeInBits, "bsiz");
SEC_CONST_DECL (kSecAttrEffectiveKeySize, "esiz");
SEC_CONST_DECL (kSecAttrStartDate, "sdat");
SEC_CONST_DECL (kSecAttrEndDate, "edat");
SEC_CONST_DECL (kSecAttrIsSensitive, "sens");
SEC_CONST_DECL (kSecAttrWasAlwaysSensitive, "asen");
SEC_CONST_DECL (kSecAttrIsExtractable, "extr");
SEC_CONST_DECL (kSecAttrWasNeverExtractable, "next");
SEC_CONST_DECL (kSecAttrCanEncrypt, "encr");
SEC_CONST_DECL (kSecAttrCanDecrypt, "decr");
SEC_CONST_DECL (kSecAttrCanDerive, "drve");
SEC_CONST_DECL (kSecAttrCanSign, "sign");
SEC_CONST_DECL (kSecAttrCanVerify, "vrfy");
SEC_CONST_DECL (kSecAttrCanSignRecover, "snrc");
SEC_CONST_DECL (kSecAttrCanVerifyRecover, "vyrc");
SEC_CONST_DECL (kSecAttrCanWrap, "wrap");
SEC_CONST_DECL (kSecAttrCanUnwrap, "unwp");
SEC_CONST_DECL (kSecAttrSyncViewHint, "vwht");
SEC_CONST_DECL (kSecAttrTokenID, "tkid");
/* Attribute Constants (Private) */
SEC_CONST_DECL (kSecAttrScriptCode, "scrp");
SEC_CONST_DECL (kSecAttrHasCustomIcon, "cusi");
SEC_CONST_DECL (kSecAttrCRLType, "crlt");
SEC_CONST_DECL (kSecAttrCRLEncoding, "crle");
SEC_CONST_DECL (kSecAttrSynchronizable, "sync");
SEC_CONST_DECL (kSecAttrTombstone, "tomb");
SEC_CONST_DECL (kSecAttrMultiUser, "musr");
SEC_CONST_DECL (kSecAttrNoLegacy, "nleg");      // Deprecated SPI version of kSecUseDataProtectionKeychain
SEC_CONST_DECL (kSecAttrTokenOID, "toid");
SEC_CONST_DECL (kSecAttrUUID, "UUID");
SEC_CONST_DECL (kSecAttrPersistantReference, "persistref");
SEC_CONST_DECL (kSecAttrPersistentReference, "persistref");
SEC_CONST_DECL (kSecAttrSysBound, "sysb");
SEC_CONST_DECL (kSecAttrSHA1, "sha1");

SEC_CONST_DECL (kSecAttrDeriveSyncIDFromItemAttributes, "dspk");
SEC_CONST_DECL (kSecAttrPCSPlaintextServiceIdentifier, "pcss");
SEC_CONST_DECL (kSecAttrPCSPlaintextPublicKey, "pcsk");
SEC_CONST_DECL (kSecAttrPCSPlaintextPublicIdentity, "pcsi");

SEC_CONST_DECL (kSecDataInetExtraNotes, "binn");
SEC_CONST_DECL (kSecDataInetExtraHistory, "bini");
SEC_CONST_DECL (kSecDataInetExtraClientDefined0, "bin0");
SEC_CONST_DECL (kSecDataInetExtraClientDefined1, "bin1");
SEC_CONST_DECL (kSecDataInetExtraClientDefined2, "bin2");
SEC_CONST_DECL (kSecDataInetExtraClientDefined3, "bin3");

/* Predefined access groups constants */
SEC_CONST_DECL (kSecAttrAccessGroupToken, "com.apple.token");

/* Search Constants */
SEC_CONST_DECL (kSecMatchPolicy, "m_Policy");
SEC_CONST_DECL (kSecMatchItemList, "m_ItemList");
SEC_CONST_DECL (kSecMatchSearchList, "m_SearchList");
SEC_CONST_DECL (kSecMatchIssuers, "m_Issuers");
SEC_CONST_DECL (kSecMatchEmailAddressIfPresent, "m_EmailAddressIfPresent");
SEC_CONST_DECL (kSecMatchSubjectContains, "m_SubjectContains");
SEC_CONST_DECL (kSecMatchCaseInsensitive, "m_CaseInsensitive");
SEC_CONST_DECL (kSecMatchTrustedOnly, "m_TrustedOnly");
SEC_CONST_DECL (kSecMatchValidOnDate, "m_ValidOnDate");
SEC_CONST_DECL (kSecMatchLimit, "m_Limit");
/* Could just use kCFBooleanTrue and kCFBooleanFalse for these 2. */
SEC_CONST_DECL (kSecMatchLimitOne, "m_LimitOne");
SEC_CONST_DECL (kSecMatchLimitAll, "m_LimitAll");

/* Return Type Key Constants */
SEC_CONST_DECL (kSecReturnData, "r_Data");
SEC_CONST_DECL (kSecReturnAttributes, "r_Attributes");
SEC_CONST_DECL (kSecReturnRef, "r_Ref");
SEC_CONST_DECL (kSecReturnPersistentRef, "r_PersistentRef");

/* Value Type Key Constants */
SEC_CONST_DECL (kSecValueData, "v_Data");
SEC_CONST_DECL (kSecValueRef, "v_Ref");
SEC_CONST_DECL (kSecValuePersistentRef, "v_PersistentRef");

/* kSecUse Constants */
SEC_CONST_DECL (kSecUseItemList, "u_ItemList");
SEC_CONST_DECL (kSecUseTombstones, "u_Tomb");
SEC_CONST_DECL (kSecUseCredentialReference, "u_CredRef");
SEC_CONST_DECL (kSecUseOperationPrompt, "u_OpPrompt");
SEC_CONST_DECL (kSecUseNoAuthenticationUI, "u_NoAuthUI");
SEC_CONST_DECL (kSecUseAuthenticationUI, "u_AuthUI");
SEC_CONST_DECL (kSecUseSystemKeychain, "u_SystemKeychain");
SEC_CONST_DECL (kSecUseSyncBubbleKeychain, "u_SyncBubbleKeychain");
SEC_CONST_DECL (kSecUseCallerName, "u_CallerName");
SEC_CONST_DECL (kSecUseTokenRawItems, "u_TokenRawItems");
SEC_CONST_DECL (kSecUseCertificatesWithMatchIssuers, "u_CertWithIssuers");
SEC_CONST_DECL (kSecUseDataProtectionKeychain, "nleg");     // API version replacing kSecAttrNoLegacy

/* kSecAttrAccessible Value Constants. */
SEC_CONST_DECL (kSecAttrAccessibleWhenUnlocked, "ak");
SEC_CONST_DECL (kSecAttrAccessibleAfterFirstUnlock, "ck");
SEC_CONST_DECL (kSecAttrAccessibleAlways, "dk");
SEC_CONST_DECL (kSecAttrAccessibleWhenUnlockedThisDeviceOnly, "aku");
SEC_CONST_DECL (kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly, "cku");
SEC_CONST_DECL (kSecAttrAccessibleAlwaysThisDeviceOnly, "dku");
SEC_CONST_DECL (kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, "akpu");
/* kSecAttrAccessible Value Constants (Private). */
SEC_CONST_DECL (kSecAttrAccessibleAlwaysPrivate, "dk");
SEC_CONST_DECL (kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate, "dku");
/* _kSecAttrAccessibleUntilReboot for class F. */
SEC_CONST_DECL (kSecAttrAccessibleUntilReboot, "f");

/* kSecAttrProtocol Value Constants. */
SEC_CONST_DECL (kSecAttrProtocolFTP, "ftp ");
SEC_CONST_DECL (kSecAttrProtocolFTPAccount, "ftpa");
SEC_CONST_DECL (kSecAttrProtocolHTTP, "http");
SEC_CONST_DECL (kSecAttrProtocolIRC, "irc ");
SEC_CONST_DECL (kSecAttrProtocolNNTP, "nntp");
SEC_CONST_DECL (kSecAttrProtocolPOP3, "pop3");
SEC_CONST_DECL (kSecAttrProtocolSMTP, "smtp");
SEC_CONST_DECL (kSecAttrProtocolSOCKS, "sox ");
SEC_CONST_DECL (kSecAttrProtocolIMAP, "imap");
SEC_CONST_DECL (kSecAttrProtocolLDAP, "ldap");
SEC_CONST_DECL (kSecAttrProtocolAppleTalk, "atlk");
SEC_CONST_DECL (kSecAttrProtocolAFP, "afp ");
SEC_CONST_DECL (kSecAttrProtocolTelnet, "teln");
SEC_CONST_DECL (kSecAttrProtocolSSH, "ssh ");
SEC_CONST_DECL (kSecAttrProtocolFTPS, "ftps");
SEC_CONST_DECL (kSecAttrProtocolHTTPS, "htps");
SEC_CONST_DECL (kSecAttrProtocolHTTPProxy, "htpx");
SEC_CONST_DECL (kSecAttrProtocolHTTPSProxy, "htsx");
SEC_CONST_DECL (kSecAttrProtocolFTPProxy, "ftpx");
SEC_CONST_DECL (kSecAttrProtocolSMB, "smb ");
SEC_CONST_DECL (kSecAttrProtocolRTSP, "rtsp");
SEC_CONST_DECL (kSecAttrProtocolRTSPProxy, "rtsx");
SEC_CONST_DECL (kSecAttrProtocolDAAP, "daap");
SEC_CONST_DECL (kSecAttrProtocolEPPC, "eppc");
SEC_CONST_DECL (kSecAttrProtocolIPP, "ipp ");
SEC_CONST_DECL (kSecAttrProtocolNNTPS, "ntps");
SEC_CONST_DECL (kSecAttrProtocolLDAPS, "ldps");
SEC_CONST_DECL (kSecAttrProtocolTelnetS, "tels");
SEC_CONST_DECL (kSecAttrProtocolIMAPS, "imps");
SEC_CONST_DECL (kSecAttrProtocolIRCS, "ircs");
SEC_CONST_DECL (kSecAttrProtocolPOP3S, "pops");

/* kSecAttrAuthenticationType Value Constants. */
SEC_CONST_DECL (kSecAttrAuthenticationTypeNTLM, "ntlm");
SEC_CONST_DECL (kSecAttrAuthenticationTypeMSN, "msna");
SEC_CONST_DECL (kSecAttrAuthenticationTypeDPA, "dpaa");
SEC_CONST_DECL (kSecAttrAuthenticationTypeRPA, "rpaa");
SEC_CONST_DECL (kSecAttrAuthenticationTypeHTTPBasic, "http");
SEC_CONST_DECL (kSecAttrAuthenticationTypeHTTPDigest, "httd");
SEC_CONST_DECL (kSecAttrAuthenticationTypeHTMLForm, "form");
SEC_CONST_DECL (kSecAttrAuthenticationTypeDefault, "dflt");

/* kSecAttrKeyClass Value Constants.  Based on <Security/cssmtype.h>
	CSSM_KEYCLASS_PUBLIC_KEY =		0,
	CSSM_KEYCLASS_PRIVATE_KEY =		1,
	CSSM_KEYCLASS_SESSION_KEY =		2,
 */
SEC_CONST_DECL (kSecAttrKeyClassPublic, "0");
SEC_CONST_DECL (kSecAttrKeyClassPrivate, "1");
SEC_CONST_DECL (kSecAttrKeyClassSymmetric, "2");

/* kSecAttrKeyType Value Constants.  Based on CSSM_ALGORITHMS. */
SEC_CONST_DECL (kSecAttrKeyTypeRSA, "42");
SEC_CONST_DECL (kSecAttrKeyTypeEC, "73");  /* rdar://10755886 */
SEC_CONST_DECL (kSecAttrKeyTypeECSECPrimeRandom, "73");
SEC_CONST_DECL (kSecAttrKeyTypeECSECPrimeRandomPKA, "2147483678");  /* <Security/cssmapple.h> CSSM_ALGID__FIRST_UNUSED */
SEC_CONST_DECL (kSecAttrKeyTypeSecureEnclaveAttestation, "2147483679"); /* <Security/cssmapple.h> CSSM_ALGID__FIRST_UNUSED + 1 */

/* kSecAttrSynchronizable Value Constants. */
SEC_CONST_DECL (kSecAttrSynchronizableAny, "syna");

/* Constants used by SecKeyGeneratePair() - in SecKey.h.  Never used in
   any SecItem apis directly. */
SEC_CONST_DECL (kSecPrivateKeyAttrs, "private");
SEC_CONST_DECL (kSecPublicKeyAttrs, "public");
SEC_CONST_DECL (kSecKeyApplePayEnabled, "applepay");

/* This is here only temporarily until MobileActivation starts using kSecAttrTokenOID instead of this specific attribute. */
SEC_CONST_DECL (kSecAttrSecureEnclaveKeyBlob, "toid");

/* Constants used by SecPassword - in SecPasswordStrength */
SEC_CONST_DECL (kSecPasswordMaxLength, "PasswordMaxLength");
SEC_CONST_DECL (kSecPasswordMinLength, "PasswordMaxLength");
SEC_CONST_DECL (kSecPasswordAllowedCharacters, "PasswordAllowedCharacters");
SEC_CONST_DECL (kSecPasswordRequiredCharacters, "PasswordRequiredCharacters");

/* Constants used by SecSharedCredential - in <Security/SecSharedCredential.h> */
SEC_CONST_DECL (kSecSharedPassword, "spwd");

SEC_CONST_DECL (kSecUseAuthenticationUIAllow, "u_AuthUIA");
SEC_CONST_DECL (kSecUseAuthenticationUIFail, "u_AuthUIF");
SEC_CONST_DECL (kSecUseAuthenticationUISkip, "u_AuthUIS");
SEC_CONST_DECL (kSecUseAuthenticationContext, "u_AuthCtx");

/* kSecAttrTokenID Value Constants. */
SEC_CONST_DECL (kSecAttrTokenIDSecureEnclave, "com.apple.setoken");
SEC_CONST_DECL (kSecAttrTokenIDAppleKeyStore, "com.apple.setoken:aks");
SEC_CONST_DECL (kSecAttrTokenIDSecureElement, "com.apple.secelemtoken");

/* Internal kSecAttrAccessGroup for syncing */
SEC_CONST_DECL (kSOSInternalAccessGroup, "com.apple.security.sos");

/*  View Hint Constants */

#undef DOVIEWMACRO
#define DO_SEC_CONST_DECL_(VIEWNAME, DEFSTRING) const CFTypeRef kSecAttrViewHint##VIEWNAME = CFSTR(DEFSTRING);
#define DO_SEC_CONST_DECL_V(VIEWNAME, DEFSTRING)

#define DOVIEWMACRO(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULTSETTING, INITIALSYNCSETTING, ALWAYSONSETTING, BACKUPSETTING, V0SETTING) DO_SEC_CONST_DECL_##V0SETTING(VIEWNAME, DEFSTRING)
#include "keychain/SecureObjectSync/ViewList.list"
#undef DOVIEWMACRO

