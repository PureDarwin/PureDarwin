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

CF_EXTERN_C_BEGIN

typedef OSStatus LSStatus;

enum {
    kLSUnknownErr                = -10810,
    kLSApplicationNotFoundErr    = -10814,
    kLSDataErr                   = -10817,
};

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

CF_EXTERN_C_END

#endif /* __CORESERVICES_LAUNCHSERVICES__ */
