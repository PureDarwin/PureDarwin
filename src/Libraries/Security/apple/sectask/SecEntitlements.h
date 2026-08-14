/*
 * Copyright (c) 2008-2010,2012,2014 Apple Inc. All Rights Reserved.
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


/* This file contains the names of all known entitlements currently
   in use on the system. */

#ifndef _SECURITY_SECENTITLEMENTS_H_
#define _SECURITY_SECENTITLEMENTS_H_

#include <CoreFoundation/CFString.h>

__BEGIN_DECLS

/* Allow other tasks to get this task's name port. This is needed so the app
   can be debugged. */
#define kSecEntitlementGetTaskAllow CFSTR("get-task-allow")

/* The identifier of this application, typically the same as the
 CFBundleIdentifier. On iOS, the identifier is prefixed with the team-id and
 for some uses, the same applies to macOS.

 This is used as the default access group for any keychain items this
 application creates and accesses unless there is a
 keychain-access-group-entitlement.

 Note that iOS and macOS uses different value for the same constant.
 */

#define kSecEntitlementAppleApplicationIdentifier CFSTR("com.apple.application-identifier")
#define kSecEntitlementBasicApplicationIdentifier CFSTR("application-identifier")
#if TARGET_OS_IPHONE
#define kSecEntitlementApplicationIdentifier kSecEntitlementBasicApplicationIdentifier
#else
#define kSecEntitlementApplicationIdentifier kSecEntitlementAppleApplicationIdentifier
#endif

/* Marzipan apps distributed through the App Store cannot share an application
   identifier with their iOS versions, so they have an associated application
   identifier which matches the iOS identifier. It will be preferred, when
   present, over the 'regular' application identifier. This avoids developers
   having to jump through hoops to port iOS apps to the Mac. */
#define kSecEntitlementAssociatedApplicationIdentifier CFSTR("com.apple.developer.associated-application-identifier")

/* The value should be an array of strings.  Each string is the name of an
   access group that the application has access to.  The
   application-identifier is implicitly added to this list.   When creating
   a new keychain item use the kSecAttrAccessGroup attribute (defined in
   <Security/SecItem.h>) to specify its access group.  If omitted, the
   access group defaults to the first access group in this list or the
   application-identifier if there is no keychain-access-groups entitlement. */
#define kSecEntitlementKeychainAccessGroups CFSTR("keychain-access-groups")

/* The value should be an array of strings.  Each string is the name of an
   access group that the application has access to.  The first of
   kSecEntitlementKeychainAccessGroups,
   kSecEntitlementApplicationIdentifier or
   kSecEntitlementAppleSecurityApplicationGroups to have a value becomes the default
   application group for keychain clients that don't specify an explicit one. */
#define kSecEntitlementAppleSecurityApplicationGroups CFSTR("com.apple.security.application-groups")

#define kSecEntitlementNetworkExtensionAccessGroups CFSTR("com.apple.networkextension.keychain")

/* Boolean entitlement, if present the application with the entitlement is
   allowed to modify the which certificates are trusted as anchors using
   the SecTrustStoreSetTrustSettings() and SecTrustStoreRemoveCertificate()
   SPIs. */
#define kSecEntitlementModifyAnchorCertificates CFSTR("modify-anchor-certificates")

#define kSecEntitlementDebugApplications CFSTR("com.apple.springboard.debugapplications")

#define kSecEntitlementOpenSensitiveURL CFSTR("com.apple.springboard.opensensitiveurl")

/* Boolean entitlement, if present allows the application to wipe the keychain
   and truststore. */
#define kSecEntitlementWipeDevice CFSTR("com.apple.springboard.wipedevice")

#define kSecEntitlementRemoteNotificationConfigure CFSTR("com.apple.remotenotification.configure")

#define kSecEntitlementMigrateKeychain CFSTR("migrate-keychain")

#define kSecEntitlementRestoreKeychain CFSTR("restore-keychain")

/* Entitlement needed to call SecKeychainSyncUpdate SPI. */
#define kSecEntitlementKeychainSyncUpdates CFSTR("keychain-sync-updates")

/* Boolean entitlement, if present you get access to the SPIs for keychain sync circle manipulation */
#define kSecEntitlementKeychainCloudCircle CFSTR("keychain-cloud-circle")

/* Boolean entitlement, if present you get access to the SPIs for keychain initial sync */
#define kSecEntitlementKeychainInitialSync CFSTR("com.apple.private.security.initial-sync")

/* Associated Domains entitlement (contains array of fully-qualified domain names) */
#define kSecEntitlementAssociatedDomains CFSTR("com.apple.developer.associated-domains")

/* Entitlement needed to call swcd and swcagent processes. */
#define kSecEntitlementPrivateAssociatedDomains CFSTR("com.apple.private.associated-domains")

/* Entitlement to control usage of system keychain */
#define kSecEntitlementPrivateSystemKeychain CFSTR("com.apple.private.system-keychain")

/* Entitlement to control usage of syncbubble keychain migration */
#define kSecEntitlementPrivateKeychainSyncBubble CFSTR("com.apple.private.syncbubble-keychain")

/* Entitlement to control usage of system keychain migration */
#define kSecEntitlementPrivateKeychainMigrateSystemKeychain CFSTR("com.apple.private.migrate-musr-system-keychain")

/* Entitlement to control usage of system keychain migration */
#define kSecEntitlementPrivateNetworkExtension CFSTR("com.apple.developer.networking.networkextension")

/* Entitlement to control usage of deletion of keychain items on app uninstallation */
#define kSecEntitlementPrivateUninstallDeletion CFSTR("com.apple.private.uninstall.deletion")

/* Entitlement to control usage of deletion of keychain items wholesale */
#define kSecEntitlementPrivateDeleteAll CFSTR("com.apple.private.security.delete.all")

/* Entitlement to allow access to circle joining APIs in SOSCC */
#define kSecEntitlementCircleJoin CFSTR("com.apple.private.keychain.circle.join")

/* Entitlement to deny use of keychain APIs, only effective on iOS keychain */
#define kSecEntitlementKeychainDeny CFSTR("com.apple.private.keychain.deny")

/* Entitlement to control use of keychain certificate fetching functions */
#define kSecEntitlementPrivateCertificateAllAccess CFSTR("com.apple.private.keychain.certificates")

/* Entitlement to control use of CKKS */
#define kSecEntitlementPrivateCKKS CFSTR("com.apple.private.ckks")

/* Entitlement to allow manipulation of backup keybags in keychain table */
#define kSecEntitlementBackupTableOperations CFSTR("com.apple.private.keychain.backuptableops")

/* Entitlement to allow use of CKKS plaintext fields */
#define kSecEntitlementPrivateCKKSPlaintextFields CFSTR("com.apple.private.ckks.plaintextfields")

/* Entitlement to allow use of inet expansion fields */
#define kSecEntitlementPrivateInetExpansionFields CFSTR("com.apple.private.keychain.inet_expansion_fields")

/* Entitlement to allow use of CKKS 'current item' changing SPI */
#define kSecEntitlementPrivateCKKSWriteCurrentItemPointers CFSTR("com.apple.private.ckks.currentitempointers_write")

/* Entitlement to allow use of CKKS 'current item' reading SPI */
#define kSecEntitlementPrivateCKKSReadCurrentItemPointers CFSTR("com.apple.private.ckks.currentitempointers_read")

/* Entitlement to allow use of sysbound field */
#define kSecEntitlementPrivateSysBound CFSTR("com.apple.private.keychain.sysbound")

#define kSecEntitlementBackupTableOperationsDeleteAll CFSTR("com.apple.private.keychain.backuptableops.deleteall")

/* Entitlement to allow executing keychain control actions */
#define kSecEntitlementKeychainControl CFSTR("com.apple.private.keychain.keychaincontrol")

/* Entitlement to allow deletion of app clip keychain items */
#define kSecEntitlementPrivateAppClipDeletion CFSTR("com.apple.private.keychain.appclipdeletion")

/* Entitlement to allow use of performance-impacting API */
#define kSecEntitlementPrivatePerformanceImpactingAPI CFSTR("com.apple.private.keychain.performance_impacting_api")

/* Entitlements to allow executing SecItemUpdateTokenItemsForAccessGroups SPI */
#define kSecEntitlementUpdateTokenItems CFSTR("com.apple.private.keychain.allow-update-tokens")

/* Entitlement to control access to login keychain master key stashing (loginwindow) */
#define kSecEntitlementPrivateStash CFSTR("com.apple.private.securityd.stash")

#if __OBJC__
/* Entitlement to control use of OT */
#define kSecEntitlementPrivateOctagon @"com.apple.private.octagon"

/* Entitlement to control use of Escrow Update */
#define kSecEntitlementPrivateEscrowRequest @"com.apple.private.escrow-update"

/* Entitlement for macOS securityd to connect to stash agent */
#define kSecEntitlementPrivateStashService @"com.apple.private.securityd.stash-agent-client"
#endif

__END_DECLS

#endif /* !_SECURITY_SECENTITLEMENTS_H_ */
