/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef __CORESERVICES_LAUNCHSERVICES__
#define __CORESERVICES_LAUNCHSERVICES__

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/FileManager.h>
#include <CoreServices/UTType.h>

CF_EXTERN_C_BEGIN

typedef OSStatus LSStatus;

enum {
    kLSAttributeNotFoundErr      = -10662,
    kLSUnknownErr                = -10810,
    kLSNotAnApplicationErr       = -10811,
    kLSDataUnavailableErr        = -10813,
    kLSApplicationNotFoundErr    = -10814,
    kLSUnknownTypeErr            = -10815,
    kLSDataErr                   = -10817,
};

typedef CF_OPTIONS(OptionBits, LSLaunchFlags) {
    kLSLaunchDefaults            = 0x00000001,
    kLSLaunchAndPrint            = 0x00000002,
    kLSLaunchAndDisplayErrors    = 0x00000040,
    kLSLaunchDontAddToRecents    = 0x00000100,
    kLSLaunchDontSwitch          = 0x00000200,
    kLSLaunchAsync               = 0x00010000,
    kLSLaunchNewInstance         = 0x00080000,
    kLSLaunchAndHide             = 0x00100000,
    kLSLaunchAndHideOthers       = 0x00200000,
};

typedef struct AEDesc AEDesc;

typedef struct LSLaunchURLSpec {
    CFURLRef appURL;
    CFArrayRef itemURLs;
    const AEDesc *passThruParams;
    LSLaunchFlags launchFlags;
    void *asyncRefCon;
} LSLaunchURLSpec;

typedef struct LSLaunchFSRefSpec {
    const FSRef *appRef;
    ItemCount numDocs;
    const FSRef *itemRefs;
    const AEDesc *passThruParams;
    LSLaunchFlags launchFlags;
    void *asyncRefCon;
} LSLaunchFSRefSpec;

CF_EXPORT const CFStringRef kLSItemDisplayName;
CF_EXPORT const CFStringRef kLSItemContentType;
CF_EXPORT const CFStringRef kLSItemExtension;
CF_EXPORT const CFStringRef kLSQuarantineTypeKey;
CF_EXPORT const CFStringRef kLSQuarantineOriginURLKey;
CF_EXPORT const CFStringRef kLSQuarantineDataURLKey;
CF_EXPORT const CFStringRef kLSQuarantineAgentNameKey;
CF_EXPORT const CFStringRef kLSQuarantineAgentBundleIdentifierKey;
CF_EXPORT const CFStringRef kLSQuarantineTimeStampKey;
CF_EXPORT const CFStringRef kLSQuarantineTypeWebDownload;
CF_EXPORT const CFStringRef kLSQuarantineTypeOtherDownload;

typedef CF_OPTIONS(CFOptionFlags, LSRolesMask) {
    kLSRolesNone    = 0x00000001,
    kLSRolesViewer  = 0x00000002,
    kLSRolesEditor  = 0x00000004,
    kLSRolesShell   = 0x00000008,
    kLSRolesAll     = 0xFFFFFFFF
};

/* Applications able to open the item, most preferred first. NULL if none. */
CF_EXPORT CFArrayRef LSCopyApplicationURLsForURL(CFURLRef inURL, LSRolesMask inRoleMask);

/* The single application that would be used, or NULL. */
CF_EXPORT CFURLRef LSCopyDefaultApplicationURLForURL(CFURLRef inURL,
                                                     LSRolesMask inRoleMask,
                                                     CFErrorRef *outError);

CF_EXPORT CFURLRef LSCopyDefaultApplicationURLForContentType(CFStringRef inContentType,
                                                             LSRolesMask inRoleMask,
                                                             CFErrorRef *outError);

CF_EXPORT CFArrayRef LSCopyApplicationURLsForBundleIdentifier(CFStringRef inBundleIdentifier,
                                                              CFErrorRef *outError);

/* Open a document (launching its handler) or an application bundle. */
CF_EXPORT OSStatus LSOpenCFURLRef(CFURLRef inURL, CFURLRef *outLaunchedURL);

/* Launch an application bundle, optionally passing it items to open. */
CF_EXPORT OSStatus LSOpenApplicationAtURL(CFURLRef inAppURL, CFArrayRef inItemURLs,
                                          CFURLRef *outLaunchedURL);

/* Display name and kind, from the bundle's Info.plist where available. */
CF_EXPORT CFStringRef LSCopyDisplayNameForURL(CFURLRef inURL);
CF_EXPORT CFStringRef LSCopyKindStringForURL(CFURLRef inURL);

/* Every registered application bundle. PureDarwin extension: the search is
 * a directory scan, so there is no separate registration database to query. */
CF_EXPORT CFArrayRef LSCopyAllApplicationURLs(void);

/* Drop the cached scan; the next lookup re-reads the application folders. */
CF_EXPORT void LSRefreshApplicationRegistry(void);

CF_EXPORT OSStatus LSCopyApplicationForMIMEType(CFStringRef inMIMEType, LSRolesMask inRoleMask, CFURLRef *outAppURL);
CF_EXPORT CFStringRef LSCopyDefaultHandlerForURLScheme(CFStringRef inURLScheme);
CF_EXPORT OSStatus LSSetDefaultHandlerForURLScheme(CFStringRef inURLScheme, CFStringRef inHandlerBundleID);
CF_EXPORT OSStatus LSSetDefaultRoleHandlerForContentType(CFStringRef inContentType, LSRolesMask inRole, CFStringRef inHandlerBundleID);
CF_EXPORT OSStatus LSGetApplicationForItem(const FSRef *inItemRef, LSRolesMask inRoleMask, FSRef *outAppRef, CFURLRef *outAppURL);
CF_EXPORT OSStatus LSGetApplicationForInfo(OSType inType, OSType inCreator, CFStringRef inExtension, LSRolesMask inRoleMask,
                                           FSRef *outAppRef, CFURLRef *outAppURL);
CF_EXPORT OSStatus LSCopyItemAttribute(const FSRef *inItem, LSRolesMask inRoles, CFStringRef inAttributeName, CFTypeRef *outValue);
CF_EXPORT OSStatus LSRegisterURL(CFURLRef inURL, Boolean inUpdate);
CF_EXPORT OSStatus LSOpenFromURLSpec(const LSLaunchURLSpec *inLaunchSpec, CFURLRef *outLaunchedURL);
CF_EXPORT OSStatus LSOpenFromRefSpec(const LSLaunchFSRefSpec *inLaunchSpec, FSRef *outLaunchedRef);

CF_EXTERN_C_END

#endif /* __CORESERVICES_LAUNCHSERVICES__ */
