// UnicodeUtilities.h - Unicode collators over locales, and the old system activity call
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// Unicode collators over locales, and the old system activity call, as CarbonCore declares them
#ifndef __CORESERVICES_UNICODEUTILITIES__
#define __CORESERVICES_UNICODEUTILITIES__

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

typedef struct OpaqueLocaleRef *LocaleRef;
typedef UInt32 LocaleOperationVariant;
typedef struct OpaqueCollatorRef *CollatorRef;
typedef UInt32 UCCollateOptions;

enum {
    kUCCollateComposeInsensitiveMask = 1UL << 1,
    kUCCollateWidthInsensitiveMask = 1UL << 2,
    kUCCollateCaseInsensitiveMask = 1UL << 3,
    kUCCollateDiacritInsensitiveMask = 1UL << 4,
    kUCCollatePunctuationSignificantMask = 1UL << 15,
    kUCCollateDigitsOverrideMask = 1UL << 16,
    kUCCollateDigitsAsNumberMask = 1UL << 17,
};

CF_EXPORT OSStatus LocaleRefFromLocaleString(const char *localeString, LocaleRef *locale);
CF_EXPORT OSStatus UCCreateCollator(LocaleRef locale, LocaleOperationVariant opVariant, UCCollateOptions options,
                                    CollatorRef *collatorRef);
CF_EXPORT OSStatus UCDisposeCollator(CollatorRef *collatorRef);
CF_EXPORT OSStatus UCCompareText(CollatorRef collatorRef, const UniChar *text1Ptr, UniCharCount text1Length,
                                 const UniChar *text2Ptr, UniCharCount text2Length, Boolean *equivalent, SInt32 *order);

// UpdateSystemActivity tells the system the user is active, which resets the idle time
enum {
    OverallAct = 0,
    UsrActivity = 1,
    NetActivity = 2,
    HDActivity = 3,
    IdleActivity = 4,
};
CF_EXPORT OSErr UpdateSystemActivity(UInt8 activity);

CF_EXTERN_C_END

#endif
