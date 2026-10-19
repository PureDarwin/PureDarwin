// SharedFileList.h - LSSharedFileList, the MDItem handle and UCKeyTranslate
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// LSSharedFileList, the MDItem handle and UCKeyTranslate, the remaining CoreServices calls apps use
#ifndef __CORESERVICES_SHAREDFILELIST__
#define __CORESERVICES_SHAREDFILELIST__

#include <CoreServices/LaunchServices.h>

CF_EXTERN_C_BEGIN

typedef struct OpaqueLSSharedFileListRef *LSSharedFileListRef;
typedef struct __LSSharedFileListItem *LSSharedFileListItemRef;
typedef struct OpaqueIconRef *IconRef;
typedef UInt32 LSSharedFileListResolutionFlags;

CF_EXPORT const CFStringRef kLSSharedFileListSessionLoginItems;
CF_EXPORT const CFStringRef kLSSharedFileListGlobalLoginItems;
CF_EXPORT const CFStringRef kLSSharedFileListRecentApplicationItems;
CF_EXPORT const CFStringRef kLSSharedFileListRecentDocumentItems;
CF_EXPORT const CFStringRef kLSSharedFileListFavoriteItems;
CF_EXPORT LSSharedFileListItemRef kLSSharedFileListItemLast;
CF_EXPORT LSSharedFileListItemRef kLSSharedFileListItemBeforeFirst;

CF_EXPORT LSSharedFileListRef LSSharedFileListCreate(CFAllocatorRef inAllocator, CFStringRef inListType, CFTypeRef listOptions);
CF_EXPORT CFArrayRef LSSharedFileListCopySnapshot(LSSharedFileListRef inList, UInt32 *outSnapshotSeed);
CF_EXPORT LSSharedFileListItemRef LSSharedFileListInsertItemURL(LSSharedFileListRef inList, LSSharedFileListItemRef insertAfterThisItem,
                                                                CFStringRef inDisplayName, IconRef inIconRef, CFURLRef inURL,
                                                                CFDictionaryRef inPropertiesToSet, CFArrayRef inPropertiesToClear);
CF_EXPORT OSStatus LSSharedFileListItemRemove(LSSharedFileListRef inList, LSSharedFileListItemRef inItem);
CF_EXPORT CFURLRef LSSharedFileListItemCopyResolvedURL(LSSharedFileListItemRef inItem, LSSharedFileListResolutionFlags inFlags,
                                                       CFErrorRef *outError);
CF_EXPORT CFStringRef LSSharedFileListItemCopyDisplayName(LSSharedFileListItemRef inItem);

typedef struct __MDItem *MDItemRef;
CF_EXPORT const CFStringRef kMDItemWhereFroms;
CF_EXPORT const CFStringRef kMDItemPath;
CF_EXPORT const CFStringRef kMDItemDisplayName;
CF_EXPORT MDItemRef MDItemCreate(CFAllocatorRef allocator, CFStringRef path);
CF_EXPORT CFTypeRef MDItemCopyAttribute(MDItemRef item, CFStringRef name);

// keyboard layout translation over 'uchr' data
typedef struct UCKeyboardLayout UCKeyboardLayout;
enum {
    kUCKeyActionDown = 0,
    kUCKeyActionUp = 1,
    kUCKeyActionAutoKey = 2,
    kUCKeyActionDisplay = 3,
};
enum {
    kUCKeyTranslateNoDeadKeysBit = 0,
    kUCKeyTranslateNoDeadKeysMask = 1UL << kUCKeyTranslateNoDeadKeysBit,
};
enum {
    kUCOutputBufferTooSmall = -25340,
};
CF_EXPORT OSStatus UCKeyTranslate(const UCKeyboardLayout *keyLayoutPtr, UInt16 virtualKeyCode, UInt16 keyAction,
                                  UInt32 modifierKeyState, UInt32 keyboardType, OptionBits keyTranslateOptions,
                                  UInt32 *deadKeyState, UniCharCount maxStringLength, UniCharCount *actualStringLength,
                                  UniChar unicodeString[]);

CF_EXTERN_C_END

#endif
