/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
    @header SecItemPriv
    SecItemPriv defines private constants and SPI functions for access to
    Security items (certificates, identities, keys, and keychain items.)
*/

#ifndef _SECURITY_SECITEMPRIV_H_
#define _SECURITY_SECITEMPRIV_H_

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFError.h>
#include <TargetConditionals.h>
#include <Security/SecBase.h>
#include <xpc/xpc.h>

#if TARGET_OS_OSX
#include <Security/SecTask.h>
#endif

#if __OBJC__
#import <Foundation/Foundation.h>
#endif

__BEGIN_DECLS

/*!
    @enum Class Value Constants (Private)
    @discussion Predefined item class constants used to get or set values in
        a dictionary. The kSecClass constant is the key and its value is one
        of the constants defined here.
    @constant kSecClassAppleSharePassword Specifies AppleShare password items.
*/
extern const CFStringRef kSecClassAppleSharePassword;


/*!
    @enum Attribute Key Constants (Private)
    @discussion Predefined item attribute keys used to get or set values in a
        dictionary. Not all attributes apply to each item class. The table
        below lists the currently defined attributes for each item class:

    kSecClassGenericPassword item attributes:
        kSecAttrAccessGroup
        kSecAttrCreationDate
        kSecAttrModificationDate
        kSecAttrDescription
        kSecAttrComment
        kSecAttrCreator
        kSecAttrType
        kSecAttrScriptCode (private)
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrIsInvisible
        kSecAttrIsNegative
        kSecAttrHasCustomIcon (private)
        kSecAttrProtected (private)
        kSecAttrAccount
        kSecAttrService
        kSecAttrGeneric
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

    kSecClassInternetPassword item attributes:
        kSecAttrAccessGroup
        kSecAttrCreationDate
        kSecAttrModificationDate
        kSecAttrDescription
        kSecAttrComment
        kSecAttrCreator
        kSecAttrType
        kSecAttrScriptCode (private)
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrIsInvisible
        kSecAttrIsNegative
        kSecAttrHasCustomIcon (private)
        kSecAttrProtected (private)
        kSecAttrAccount
        kSecAttrSecurityDomain
        kSecAttrServer
        kSecAttrProtocol
        kSecAttrAuthenticationType
        kSecAttrPort
        kSecAttrPath
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

    kSecClassAppleSharePassword item attributes:
        kSecAttrAccessGroup
        kSecAttrCreationDate
        kSecAttrModificationDate
        kSecAttrDescription
        kSecAttrComment
        kSecAttrCreator
        kSecAttrType
        kSecAttrScriptCode (private)
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrIsInvisible
        kSecAttrIsNegative
        kSecAttrHasCustomIcon (private)
        kSecAttrProtected (private)
        kSecAttrAccount
        kSecAttrVolume
        kSecAttrAddress
        kSecAttrAFPServerSignature
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

    kSecClassCertificate item attributes:
        kSecAttrAccessGroup
        kSecAttrCertificateType
        kSecAttrCertificateEncoding
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrSubject
        kSecAttrIssuer
        kSecAttrSerialNumber
        kSecAttrSubjectKeyID
        kSecAttrPublicKeyHash
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

    kSecClassKey item attributes:
        kSecAttrAccessGroup
        kSecAttrKeyClass
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrApplicationLabel
        kSecAttrIsPermanent
        kSecAttrIsPrivate (private)
        kSecAttrIsModifiable (private)
        kSecAttrApplicationTag
        kSecAttrKeyCreator (private)
        kSecAttrKeyType
        kSecAttrKeySizeInBits
        kSecAttrEffectiveKeySize
        kSecAttrStartDate (private)
        kSecAttrEndDate (private)
        kSecAttrIsSensitive (private)
        kSecAttrWasAlwaysSensitive (private)
        kSecAttrIsExtractable (private)
        kSecAttrWasNeverExtractable (private)
        kSecAttrCanEncrypt
        kSecAttrCanDecrypt
        kSecAttrCanDerive
        kSecAttrCanSign
        kSecAttrCanVerify
        kSecAttrCanSignRecover (private)
        kSecAttrCanVerifyRecover (private)
        kSecAttrCanWrap
        kSecAttrCanUnwrap
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

    kSecClassIdentity item attributes:
        Since an identity is the combination of a private key and a
        certificate, this class shares attributes of both kSecClassKey and
        kSecClassCertificate.

    @constant kSecAttrScriptCode Specifies a dictionary key whose value is the
        item's script code attribute. You use this tag to set or get a value
        of type CFNumberRef that represents a script code for this item's
        strings. (Note: use of this attribute is deprecated; string attributes
        should always be stored in UTF-8 encoding. This is currently private
        for use by syncing; new code should not ever access this attribute.)
    @constant kSecAttrAlias Specifies a dictionary key whose value is the
        item's alias. You use this key to get or set a value of type CFDataRef
        which represents an alias. For certificate items, the alias is either
        a single email address, an array of email addresses, or the common
        name of the certificate if it does not contain any email address.
        (Items of class kSecClassCertificate have this attribute.)
    @constant kSecAttrHasCustomIcon Specifies a dictionary key whose value is the
        item's custom icon attribute. You use this tag to set or get a value
        of type CFBooleanRef that indicates whether the item should have an
        application-specific icon. (Note: use of this attribute is deprecated;
        custom item icons are not supported in Mac OS X. This is currently
        private for use by syncing; new code should not use this attribute.)
    @constant kSecAttrVolume Specifies a dictionary key whose value is the
        item's volume attribute. You use this key to set or get a CFStringRef
        value that represents an AppleShare volume name. (Items of class
        kSecClassAppleSharePassword have this attribute.)
    @constant kSecAttrAddress Specifies a dictionary key whose value is the
        item's address attribute. You use this key to set or get a CFStringRef
        value that contains the AppleTalk zone name, or the IP or domain name
        that represents the server address. (Items of class
        kSecClassAppleSharePassword have this attribute.)
    @constant kSecAttrAFPServerSignature Specifies a dictionary key whose value
        is the item's AFP server signature attribute. You use this key to set
        or get a CFDataRef value containing 16 bytes that represents the
        server's signature block. (Items of class kSecClassAppleSharePassword
        have this attribute.)
    @constant kSecAttrCRLType (read-only) Specifies a dictionary key whose
        value is the item's certificate revocation list type. You use this
        key to get a value of type CFNumberRef that denotes the CRL type (see
        the CSSM_CRL_TYPE enum in cssmtype.h). (Items of class
        kSecClassCertificate have this attribute.)
    @constant kSecAttrCRLEncoding (read-only) Specifies a dictionary key whose
        value is the item's certificate revocation list encoding.  You use
        this key to get a value of type CFNumberRef that denotes the CRL
        encoding (see the CSSM_CRL_ENCODING enum in cssmtype.h). (Items of
        class kSecClassCertificate have this attribute.)
    @constant kSecAttrKeyCreator Specifies a dictionary key whose value is a
        CFDataRef containing a CSSM_GUID structure representing the module ID of
        the CSP that owns this key.
    @constant kSecAttrIsPrivate Specifies a dictionary key whose value is a
        CFBooleanRef indicating whether the raw key material of the key in
        question is private.
    @constant kSecAttrIsModifiable Specifies a dictionary key whose value is a
        CFBooleanRef indicating whether any of the attributes of this key are
        modifiable.
    @constant kSecAttrStartDate Specifies a dictionary key whose value is a
        CFDateRef indicating the earliest date on which this key may be used.
        If kSecAttrStartDate is not present, the restriction does not apply.
    @constant kSecAttrEndDate Specifies a dictionary key whose value is a
        CFDateRef indicating the last date on which this key may be used.
        If kSecAttrEndDate is not present, the restriction does not apply.
    @constant kSecAttrIsSensitive Specifies a dictionary key whose value
        is a CFBooleanRef indicating whether the key in question must be wrapped
        with an algorithm other than CSSM_ALGID_NONE.
    @constant kSecAttrWasAlwaysSensitive Specifies a dictionary key whose value
        is a CFBooleanRef indicating that the key in question has always been
        marked as sensitive.
    @constant kSecAttrIsExtractable Specifies a dictionary key whose value
        is a CFBooleanRef indicating whether the key in question may be wrapped.
    @constant kSecAttrWasNeverExtractable Specifies a dictionary key whose value
        is a CFBooleanRef indicating that the key in question has never been
        marked as extractable.
    @constant kSecAttrCanSignRecover Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        perform sign recovery.
    @constant kSecAttrCanVerifyRecover Specifies a dictionary key whole value is
        a CFBooleanRef indicating whether the key in question can be used to
        perform verify recovery.
    @constant kSecAttrTombstone Specifies a dictionary key whose value is
        a CFBooleanRef indicating that the item in question is a tombstone.
    @constant kSecAttrNoLegacy Specifies a dictionary key whose
        value is a CFBooleanRef indicating that the query must be run on the
        syncable backend even for non syncable items. This attribute is deprecated
        in favor of the kSecUseDataProtectionKeychain API attribute.
*/
extern const CFStringRef kSecAttrScriptCode;
extern const CFStringRef kSecAttrAlias;
extern const CFStringRef kSecAttrHasCustomIcon;
extern const CFStringRef kSecAttrVolume;
extern const CFStringRef kSecAttrAddress;
extern const CFStringRef kSecAttrAFPServerSignature;
extern const CFStringRef kSecAttrCRLType;
extern const CFStringRef kSecAttrCRLEncoding;
extern const CFStringRef kSecAttrKeyCreator;
extern const CFStringRef kSecAttrIsPrivate;
extern const CFStringRef kSecAttrIsModifiable;
extern const CFStringRef kSecAttrStartDate;
extern const CFStringRef kSecAttrEndDate;
extern const CFStringRef kSecAttrIsSensitive;
extern const CFStringRef kSecAttrWasAlwaysSensitive;
extern const CFStringRef kSecAttrIsExtractable;
extern const CFStringRef kSecAttrWasNeverExtractable;
extern const CFStringRef kSecAttrCanSignRecover;
extern const CFStringRef kSecAttrCanVerifyRecover;
extern const CFStringRef kSecAttrTombstone;
extern const CFStringRef kSecAttrNoLegacy
    __API_DEPRECATED_WITH_REPLACEMENT("kSecUseDataProtectionKeychain", macos(10.11, 10.15), ios(9.3, 13.0), tvos(9.3, 13.0), watchos(2.3, 6.0));
extern const CFStringRef kSecAttrSyncViewHint
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecAttrMultiUser
    __OSX_AVAILABLE(10.11.5) __IOS_AVAILABLE(9.3) __TVOS_AVAILABLE(9.3) __WATCHOS_AVAILABLE(2.3);

/* This will force the syncing system to derive an item's plaintext synchronization id from its primary key.
 * This might leak primary key information, but will cause syncing devices to discover sync conflicts sooner.
 * Protected by the kSecEntitlementPrivateCKKSPlaintextFields entitlement.
 *
 * Will only be respected during a SecItemAdd.
 */
extern const CFStringRef kSecAttrDeriveSyncIDFromItemAttributes
__OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecAttrPCSPlaintextServiceIdentifier
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecAttrPCSPlaintextPublicKey
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecAttrPCSPlaintextPublicIdentity
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

extern const CFStringRef kSecDataInetExtraNotes
__OSX_AVAILABLE(10.16) __IOS_AVAILABLE(14.0) __TVOS_AVAILABLE(14.0) __WATCHOS_AVAILABLE(7.0);
extern const CFStringRef kSecDataInetExtraHistory
__OSX_AVAILABLE(10.16) __IOS_AVAILABLE(14.0) __TVOS_AVAILABLE(14.0) __WATCHOS_AVAILABLE(7.0);
extern const CFStringRef kSecDataInetExtraClientDefined0
__OSX_AVAILABLE(10.16) __IOS_AVAILABLE(14.0) __TVOS_AVAILABLE(14.0) __WATCHOS_AVAILABLE(7.0);
extern const CFStringRef kSecDataInetExtraClientDefined1
__OSX_AVAILABLE(10.16) __IOS_AVAILABLE(14.0) __TVOS_AVAILABLE(14.0) __WATCHOS_AVAILABLE(7.0);
extern const CFStringRef kSecDataInetExtraClientDefined2
__OSX_AVAILABLE(10.16) __IOS_AVAILABLE(14.0) __TVOS_AVAILABLE(14.0) __WATCHOS_AVAILABLE(7.0);
extern const CFStringRef kSecDataInetExtraClientDefined3
__OSX_AVAILABLE(10.16) __IOS_AVAILABLE(14.0) __TVOS_AVAILABLE(14.0) __WATCHOS_AVAILABLE(7.0);

// ObjectID of item stored on the token.  Token-type specific BLOB.
// For kSecAttrTokenIDSecureEnclave and kSecAttrTokenIDAppleKeyStore, ObjectID is libaks's blob representation of encoded key.
extern const CFStringRef kSecAttrTokenOID
     __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);
extern const CFStringRef kSecAttrUUID
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecAttrSysBound
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecAttrSHA1
__OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

#define kSecSecAttrSysBoundNot                    0
#define kSecSecAttrSysBoundPreserveDuringRestore  1


extern const CFStringRef kSecAttrKeyTypeECSECPrimeRandomPKA
     __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecAttrKeyTypeSecureEnclaveAttestation
     __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

// Should not be used, use kSecAttrTokenOID instead.
extern const CFStringRef kSecAttrSecureEnclaveKeyBlob
     __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
    @enum kSecAttrAccessible Value Constants (Private)
    @constant kSecAttrAccessibleAlwaysPrivate Private alias for kSecAttrAccessibleAlways,
        which is going to be deprecated for 3rd party use.
    @constant kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate for kSecAttrAccessibleAlwaysThisDeviceOnly,
        which is going to be deprecated for 3rd party use.
    @constant kSecAttrAccessibleUntilReboot Not usable for keychain item. Can be used only
        for generating non-permanent SEP-based SecKey. Such key does not need any keybag loaded and
        is valid only until next reboot. Also known as class F protection.
*/
extern const CFStringRef kSecAttrAccessibleAlwaysPrivate
;//%%%    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate
;//%%%    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecAttrAccessibleUntilReboot
API_AVAILABLE(macos(10.14.1), ios(12.1), tvos(12.1), watchos(5.1));

/*  View Hint Constants */

extern const CFStringRef kSecAttrViewHintPCSMasterKey;
extern const CFStringRef kSecAttrViewHintPCSiCloudDrive;
extern const CFStringRef kSecAttrViewHintPCSPhotos;
extern const CFStringRef kSecAttrViewHintPCSCloudKit;
extern const CFStringRef kSecAttrViewHintPCSEscrow;
extern const CFStringRef kSecAttrViewHintPCSFDE;
extern const CFStringRef kSecAttrViewHintPCSMailDrop;
extern const CFStringRef kSecAttrViewHintPCSiCloudBackup;
extern const CFStringRef kSecAttrViewHintPCSNotes;
extern const CFStringRef kSecAttrViewHintPCSiMessage;
extern const CFStringRef kSecAttrViewHintPCSFeldspar;
extern const CFStringRef kSecAttrViewHintPCSSharing;

extern const CFStringRef kSecAttrViewHintAppleTV;
extern const CFStringRef kSecAttrViewHintHomeKit;
extern const CFStringRef kSecAttrViewHintContinuityUnlock;
extern const CFStringRef kSecAttrViewHintAccessoryPairing;
extern const CFStringRef kSecAttrViewHintNanoRegistry;
extern const CFStringRef kSecAttrViewHintWatchMigration;
extern const CFStringRef kSecAttrViewHintEngram;
extern const CFStringRef kSecAttrViewHintManatee;
extern const CFStringRef kSecAttrViewHintAutoUnlock;
extern const CFStringRef kSecAttrViewHintHealth;
extern const CFStringRef kSecAttrViewHintApplePay;
extern const CFStringRef kSecAttrViewHintHome;
extern const CFStringRef kSecAttrViewHintLimitedPeersAllowed;


extern const CFStringRef kSecUseSystemKeychain
    __TVOS_AVAILABLE(9.2)
    __WATCHOS_AVAILABLE(3.0)
    __OSX_AVAILABLE(10.11.4)
    __IOS_AVAILABLE(9.3);

extern const CFStringRef kSecUseSyncBubbleKeychain
    __TVOS_AVAILABLE(9.2)
    __WATCHOS_AVAILABLE(3.0)
    __OSX_AVAILABLE(10.11.4)
    __IOS_AVAILABLE(9.3);

/*!
    @enum Other Constants (Private)
    @discussion Predefined constants used to set values in a dictionary.
    @constant kSecUseTombstones Specifies a dictionary key whose value is a
        CFBooleanRef if present this overrides the default behaviour for when
        we make tombstones.  The default being we create tombstones for
        synchronizable items unless we are explicitly deleting or updating a
        tombstone.  Setting this to false when calling SecItemDelete or
        SecItemUpdate will ensure no tombstones are created.  Setting it to
        true will ensure we create tombstones even when deleting or updating non
        synchronizable items.
    @constant kSecUseKeychain Specifies a dictionary key whose value is a
        keychain reference. You use this key to specify a value of type
        SecKeychainRef that indicates the keychain to which SecItemAdd
        will add the provided item(s).
    @constant kSecUseKeychainList Specifies a dictionary key whose value is
        either an array of keychains to search (CFArrayRef), or a single
        keychain (SecKeychainRef). If not provided, the user's default
        keychain list is searched. kSecUseKeychainList is ignored if an
        explicit kSecUseItemList is also provided.  This key can be used
        for the SecItemCopyMatching, SecItemUpdate and SecItemDelete calls.
    @constant kSecUseCredentialReference Specifies a CFDataRef containing
        AppleCredentialManager reference handle to be used when authorizing access
        to the item.
    @constant kSecUseCallerName Specifies a dictionary key whose value
        is a CFStringRef that represents a user-visible string describing
        the caller name for which the application is attempting to authenticate.
        The caller must have 'com.apple.private.LocalAuthentication.CallerName'
        entitlement set to YES to use this feature, otherwise it is ignored.
	@constant kSecUseTokenRawItems If set to true, token-based items (i.e. those
        which have non-empty kSecAttrTokenID are not going through client-side
        postprocessing, only raw form stored in the database is listed.  This
        flag is ignored in other operations than SecItemCopyMatching().
    @constant kSecUseCertificatesWithMatchIssuers If set to true,
        SecItemCopyMatching allows to return certificates when kSecMatchIssuers is specified.
*/
extern const CFStringRef kSecUseTombstones
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecUseCredentialReference
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);
extern const CFStringRef kSecUseCallerName
    __OSX_AVAILABLE(10.11.4) __IOS_AVAILABLE(9.3) __TVOS_AVAILABLE(9.3) __WATCHOS_AVAILABLE(2.3);
extern const CFStringRef kSecUseTokenRawItems
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecUseCertificatesWithMatchIssuers
    __OSX_AVAILABLE(10.14) API_UNAVAILABLE(ios, tvos, watchos, bridgeos, macCatalyst);

extern const CFStringRef kSOSInternalAccessGroup
    __OSX_AVAILABLE(10.9) __IOS_AVAILABLE(7.0) __TVOS_AVAILABLE(9.3) __WATCHOS_AVAILABLE(2.3);

/*!
 @enum kSecAttrTokenID Value Constants
 @discussion Predefined item attribute constant used to get or set values
 in a dictionary. The kSecAttrTokenID constant is the key and its value
 can be kSecAttrTokenIDSecureEnclave.
 @constant kSecAttrTokenIDKeyAppleStore Specifies well-known identifier of
 the token implemented using libaks (AppleKeyStore).  This token is identical to
 kSecAttrTokenIDSecureEnclave for devices which support Secure Enclave and
 silently falls back to in-kernel emulation for those devices which do not
 have Secure Enclave support.
 */
extern const CFStringRef kSecAttrTokenIDAppleKeyStore
	__OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(3.0);


extern const CFStringRef kSecNetworkExtensionAccessGroupSuffix;

/*!
    @function SecItemDeleteAll
    @abstract Removes all items from the keychain.
    @result A result code. See "Security Error Codes" (SecBase.h).
*/
OSStatus SecItemDeleteAll(void);

/*!
    @function SecItemPersistKeychainWritesAtHighPerformanceCost
    @abstract Attempts to ensure that all previous writes to the keychain are durably committed to disk.
            This overrides existing performance and device disk durability tradeoffs, so you should
            not use this SPI until you have a full understanding of how data loss resulting from power loss
            impacts your distributed system.

            Note: This SPI is not guaranteed to work on macOS, if the user is in a non-standard hardware configuration
            or is using network home folders.

            Note: on macOS, this will only affect the data protection keychain. Legacy macOS keychains are untouched.
    @param error An error returned, if any.
    @result A result code.
 */
OSStatus SecItemPersistKeychainWritesAtHighPerformanceCost(CFErrorRef* CF_RETURNS_RETAINED error)
    API_AVAILABLE(macos(11.3), ios(14.5), tvos(14.5), watchos(7.4));

/*!
    @function _SecItemAddAndNotifyOnSync
    @abstract Adds an item to the keychain, and calls syncCallback when the item has synced
    @param attributes Attributes dictionary to be passed to SecItemAdd
    @param result Result reference to be passed to SecItemAdd
    @param syncCallback Block to be executed after the item has synced or failed to sync
    @result The result code returned from SecItemAdd
 */
OSStatus _SecItemAddAndNotifyOnSync(CFDictionaryRef attributes, CFTypeRef * CF_RETURNS_RETAINED result, void (^syncCallback)(bool didSync, CFErrorRef error));

/*!
     @function SecItemSetCurrentItemAcrossAllDevices
     @abstract Sets 'new current item' to be the 'current' item in CloudKit for the given identifier.
 */
void SecItemSetCurrentItemAcrossAllDevices(CFStringRef accessGroup,
                                           CFStringRef identifier,
                                           CFStringRef viewHint,
                                           CFDataRef newCurrentItemReference,
                                           CFDataRef newCurrentItemHash,
                                           CFDataRef oldCurrentItemReference,
                                           CFDataRef oldCurrentItemHash,
                                           void (^complete)(CFErrorRef error));

/*!
     @function SecItemFetchCurrentItemAcrossAllDevices
     @abstract Fetches the locally cached idea of which keychain item is 'current' across this iCloud account
               for the given access group and identifier.
 @param accessGroup The accessGroup of your process and the expected current item
 @param identifier Which 'current' item you're interested in. Freeform, but should match the ID given to
 SecItemSetCurrentItemAcrossAllDevices.
 @param viewHint The keychain view hint for your items.
 @param fetchCloudValue If false, will return the local machine's cached idea of which item is current. If true,
 performs a CloudKit operation to determine the most up-to-date version.
 @param complete Called to return values: a persistent ref to the current item, if such an item exists. Otherwise, error.
 */
void SecItemFetchCurrentItemAcrossAllDevices(CFStringRef accessGroup,
                                             CFStringRef identifier,
                                             CFStringRef viewHint,
                                             bool fetchCloudValue,
                                             void (^complete)(CFDataRef persistentRef, CFErrorRef error));

#if __OBJC__
/*!
    @function SecItemVerifyBackupIntegrity
    @abstract Verifies the presence and integrity of all key material required
        to restore a backup of the keychain.
    @discussion This function performs a synchronous call to securityd, be prepared to wait for it to scan the keychain.
    @param lightweight Only verify the item keys wrapped by backup keys instead
        of the default rigorous pass. This mode can be run in any
        security class.
    @param completion Called to indicate results: a dictionary containing information about the the infrastructure
        and of the backup state of keychain items. Error is set when at least one failure occurred.
 */
void SecItemVerifyBackupIntegrity(BOOL lightweight,
                                  void(^completion)(NSDictionary* resultsPerKeyclass, NSError* error));
void _SecItemFetchDigests(NSString *itemClass, NSString *accessGroup, void (^complete)(NSArray *, NSError *));
void _SecKeychainDeleteMultiUser(NSString *musrUUID, void (^complete)(bool, NSError *));
#endif

/*!
 @function SecItemDeleteAllWithAccessGroups
 @abstract Deletes all items for each class for the given access groups
 @param accessGroups An array of access groups for the items
 @result A result code. See "Security Error Codes" (SecBase.h).
 @discussion Provided for use by MobileInstallation to allow cleanup after uninstall
    Requires entitlement "com.apple.private.uninstall.deletion"
 */
bool SecItemDeleteAllWithAccessGroups(CFArrayRef accessGroups, CFErrorRef *error);

/*
    Ensure the escrow keybag has been used to unlock the system keybag before
    calling either of these APIs.
    The password argument is optional, passing NULL implies no backup password
    was set.  We're assuming there will always be a backup keybag, except in
    the OTA case where the loaded OTA backup bag will be used.
 */
CFDataRef _SecKeychainCopyBackup(CFDataRef backupKeybag, CFDataRef password);
CFDataRef _SecKeychainCopyOTABackup(void);
OSStatus _SecKeychainRestoreBackup(CFDataRef backup, CFDataRef backupKeybag,
    CFDataRef password);
/*
    EMCS backups are similar to regular backups but we do not want to unlock the keybag
 */
CFDataRef _SecKeychainCopyEMCSBackup(CFDataRef backupKeybag);

bool
_SecKeychainWriteBackupToFileDescriptor(CFDataRef backupKeybag, CFDataRef password, int fd, CFErrorRef *error);

bool
_SecKeychainRestoreBackupFromFileDescriptor(int fd, CFDataRef backupKeybag, CFDataRef password, CFErrorRef *error);

CFStringRef
_SecKeychainCopyKeybagUUIDFromFileDescriptor(int fd, CFErrorRef *error);

OSStatus _SecKeychainBackupSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in, CFDictionaryRef *backup_out);
OSStatus _SecKeychainRestoreSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in);

/* Called by clients to push sync circle and message changes to us.
   Requires caller to have the kSecEntitlementKeychainSyncUpdates entitlement. */
CFArrayRef _SecKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error);

#if !TARGET_OS_IPHONE
CFDataRef _SecItemGetPersistentReference(CFTypeRef raw_item);
#endif

/* Returns an OSStatus value for the given CFErrorRef, returns errSecInternal if the
   domain of the provided error is not recognized.  Passing NULL returns errSecSuccess (0). */
OSStatus SecErrorGetOSStatus(CFErrorRef error);

bool _SecKeychainRollKeys(bool force, CFErrorRef *error);

CFDictionaryRef _SecSecuritydCopyWhoAmI(CFErrorRef *error);
XPC_RETURNS_RETAINED xpc_endpoint_t _SecSecuritydCopyCKKSEndpoint(CFErrorRef *error);
XPC_RETURNS_RETAINED xpc_endpoint_t _SecSecuritydCopySFKeychainEndpoint(CFErrorRef* error);
XPC_RETURNS_RETAINED xpc_endpoint_t _SecSecuritydCopyKeychainControlEndpoint(CFErrorRef* error);

bool _SecSyncBubbleTransfer(CFArrayRef services, uid_t uid, CFErrorRef *error);

bool _SecSystemKeychainTransfer(CFErrorRef *error);
bool _SecSyncDeleteUserViews(uid_t uid, CFErrorRef *error);


OSStatus SecItemUpdateTokenItemsForAccessGroups(CFTypeRef tokenID, CFArrayRef accessGroups, CFArrayRef tokenItemsAttributes);

#if SEC_OS_OSX
CFTypeRef SecItemCreateFromAttributeDictionary_osx(CFDictionaryRef refAttributes);
#endif

/*!
 * @function SecCopyLastError
 * @abstract return the last CFErrorRef for this thread
 * @param status the error code returned from the API call w/o CFErrorRef or 0
 * @result NULL or a retained CFError of the matching error code
 *
 * @discussion There are plenty of API calls in Security.framework that
 * doesn't return an CFError in case of an error, many of them actually have
 * a CFErrorRef internally, but throw it away at the last moment.
 * This might be your chance to get hold of it. The status code pass in is there
 * to avoid stale copies of CFErrorRef.

 * Note, not all interfaces support returning a CFErrorRef on the thread local
 * storage. This is especially true when going though old CDSA style API.
 */

CFErrorRef
SecCopyLastError(OSStatus status)
    __TVOS_AVAILABLE(10.0)
    __WATCHOS_AVAILABLE(3.0)
    __IOS_AVAILABLE(10.0);


bool
SecItemUpdateWithError(CFDictionaryRef inQuery,
                       CFDictionaryRef inAttributesToUpdate,
                       CFErrorRef *error)
    __TVOS_AVAILABLE(10.0)
    __WATCHOS_AVAILABLE(3.0)
    __IOS_AVAILABLE(10.0);

#if SEC_OS_OSX
/*!
 @function SecItemParentCachePurge
 @abstract Clear the cache of parent certificates used in SecItemCopyParentCertificates_osx.
 */
void SecItemParentCachePurge(void);
#endif


#if SEC_OS_OSX_INCLUDES
/*!
 @function SecItemCopyParentCertificates_osx
 @abstract Retrieve an array of possible issuing certificates for a given certificate.
 @param certificate A reference to a certificate whose issuers are being sought.
 @param context Pass NULL in this parameter to indicate that the default certificate
 source(s) should be searched. The default is to search all available keychains.
 Values of context other than NULL are currently ignored.
 @result An array of zero or more certificates whose normalized subject matches the
 normalized issuer of the provided certificate. Note that no cryptographic validation
 of the signature is performed by this function; its purpose is only to provide a list
 of candidate certificates.
 */
CFArrayRef SecItemCopyParentCertificates_osx(SecCertificateRef certificate, void *context)
__OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);

/*!
 @function SecItemCopyStoredCertificate
 @abstract Retrieve the first stored instance of a given certificate.
 @param certificate A reference to a certificate.
 @param context Pass NULL in this parameter to indicate that the default certificate
 source(s) should be searched. The default is to search all available keychains.
 Values of context other than NULL are currently ignored.
 @result Returns a certificate reference if the given certificate exists in a keychain,
 or NULL if the certificate cannot be found in any keychain. The caller is responsible
 for releasing the returned certificate reference when finished with it.
 */
SecCertificateRef SecItemCopyStoredCertificate(SecCertificateRef certificate, void *context)
__OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);
#endif /* SEC_OS_OSX */

/*!
 @enum kSecAttrTokenID Value Constants
 @discussion Predefined item attribute constant used to get or set values
 in a dictionary. The kSecAttrTokenID constant is the key and its value
 can be kSecAttrTokenIDSecureEnclave or kSecAttrTokenIDSecureElement.
 @constant kSecAttrTokenIDSecureElement Specifies well-known identifier of the
 token implemented using device's Secure Element. The only keychain items
 supported by the Secure Element token are 256-bit elliptic curve keys
 (kSecAttrKeyTypeECSecPrimeRandom). Keys must be generated on the secure element using
 SecKeyCreateRandomKey call with kSecAttrTokenID set to
 kSecAttrTokenIDSecureElement in the parameters dictionary, it is not
 possible to import pregenerated keys to kSecAttrTokenIDSecureElement token.
 */
extern const CFStringRef kSecAttrTokenIDSecureElement
SPI_AVAILABLE(ios(10.13));

/*!
 @function SecItemDeleteKeychainItemsForAppClip
 @abstract Remove all keychain items of specified App Clip's application identifier
 @discussion At uninstallation time an App Clip should not leave behind any data. This function deletes any keychain items it might have had.
 @param applicationIdentifier Name of the App Clip application identifier which is getting uninstalled.
 @result Returns errSecSuccess if zero or more items were successfully deleted, otherwise errSecInternal
 */
OSStatus
SecItemDeleteKeychainItemsForAppClip(CFStringRef applicationIdentifier)
SPI_AVAILABLE(ios(10.14));

__END_DECLS

#endif /* !_SECURITY_SECITEMPRIV_H_ */
