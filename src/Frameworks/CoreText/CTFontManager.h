// CTFontManager.h - the installed fonts and fonts registered at run time
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTFONTMANAGER__
#define __CORETEXT_CTFONTMANAGER__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGFont.h>
#import <CoreText/CoreTextExport.h>
#import <CoreText/CTFontDescriptor.h>

CORETEXT_EXPORT const CFStringRef kCTFontManagerErrorDomain;
CORETEXT_EXPORT const CFStringRef kCTFontManagerErrorFontURLsKey;
CORETEXT_EXPORT const CFStringRef kCTFontManagerRegisteredFontsChangedNotification;

typedef CFIndex CTFontManagerError;
enum {
    kCTFontManagerErrorFileNotFound = 101,
    kCTFontManagerErrorInsufficientPermissions = 102,
    kCTFontManagerErrorUnrecognizedFormat = 103,
    kCTFontManagerErrorInvalidFontData = 104,
    kCTFontManagerErrorAlreadyRegistered = 105,
    kCTFontManagerErrorExceededResourceLimit = 106,
    kCTFontManagerErrorAssetNotFound = 107,
    kCTFontManagerErrorNotRegistered = 201,
    kCTFontManagerErrorInUse = 202,
    kCTFontManagerErrorSystemRequired = 203,
    kCTFontManagerErrorRegistrationFailed = 301,
    kCTFontManagerErrorMissingEntitlement = 302,
    kCTFontManagerErrorInsufficientInfo = 303,
    kCTFontManagerErrorCancelledByUser = 304,
    kCTFontManagerErrorDuplicatedName = 305,
    kCTFontManagerErrorInvalidFilePath = 306,
    kCTFontManagerErrorUnsupportedScope = 307
};

typedef uint32_t CTFontManagerScope;
enum {
    kCTFontManagerScopeNone = 0,
    kCTFontManagerScopeProcess = 1,
    kCTFontManagerScopePersistent = 2,
    kCTFontManagerScopeSession = 3,
    kCTFontManagerScopeUser = 2
};

CORETEXT_EXPORT CFArrayRef CTFontManagerCopyAvailablePostScriptNames(void);
CORETEXT_EXPORT CFArrayRef CTFontManagerCopyAvailableFontFamilyNames(void);
CORETEXT_EXPORT CFArrayRef CTFontManagerCopyAvailableFontURLs(void);
CORETEXT_EXPORT CFComparisonResult CTFontManagerCompareFontFamilyNames(const void *family1, const void *family2, void *context);
CORETEXT_EXPORT CFArrayRef CTFontManagerCreateFontDescriptorsFromURL(CFURLRef fileURL);
CORETEXT_EXPORT CTFontDescriptorRef CTFontManagerCreateFontDescriptorFromData(CFDataRef data);
CORETEXT_EXPORT CFArrayRef CTFontManagerCreateFontDescriptorsFromData(CFDataRef data);
CORETEXT_EXPORT bool CTFontManagerRegisterFontsForURL(CFURLRef fontURL, CTFontManagerScope scope, CFErrorRef *error);
CORETEXT_EXPORT bool CTFontManagerUnregisterFontsForURL(CFURLRef fontURL, CTFontManagerScope scope, CFErrorRef *error);
CORETEXT_EXPORT bool CTFontManagerRegisterFontsForURLs(CFArrayRef fontURLs, CTFontManagerScope scope, CFArrayRef *errors);
CORETEXT_EXPORT bool CTFontManagerUnregisterFontsForURLs(CFArrayRef fontURLs, CTFontManagerScope scope, CFArrayRef *errors);
CORETEXT_EXPORT bool CTFontManagerRegisterGraphicsFont(CGFontRef font, CFErrorRef *error);
CORETEXT_EXPORT bool CTFontManagerUnregisterGraphicsFont(CGFontRef font, CFErrorRef *error);
#if __BLOCKS__
CORETEXT_EXPORT void CTFontManagerRegisterFontURLs(CFArrayRef fontURLs, CTFontManagerScope scope, bool enabled, bool (^registrationHandler)(CFArrayRef errors, bool done));
CORETEXT_EXPORT void CTFontManagerUnregisterFontURLs(CFArrayRef fontURLs, CTFontManagerScope scope, bool (^registrationHandler)(CFArrayRef errors, bool done));
#endif
CORETEXT_EXPORT CTFontManagerScope CTFontManagerGetScopeForURL(CFURLRef fontURL);
CORETEXT_EXPORT bool CTFontManagerIsSupportedFont(CFURLRef fontURL);

#endif
