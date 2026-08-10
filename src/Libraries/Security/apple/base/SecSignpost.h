//
/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#ifndef _SECSIGNPOST_H_
#define _SECSIGNPOST_H_


#include <CoreFoundation/CoreFoundation.h>

#if !TARGET_IPHONE_SIMULATOR
#import <System/sys/kdebug.h>
#endif

/*
 If you update this file, please also update SecurityCustomSignposts.plist.
 */

static unsigned int SecSignpostComponent = 82;

typedef CF_ENUM(unsigned int, SecSignpostType) {
    /* between 0 and SecSignpostImpulse, use every even number
     * After SecSignpostImpulse, its free for all for custom impulse points
     * Remeber to update SecurityCustomSignposts.plist
     */
    SecSignpostRestoreKeychain              = 0,
    SecSignpostRestoreOpenKeybag            = 2,
    SecSignpostUnlockKeybag                 = 4,
    SecSignpostBackupKeychain               = 6,
    SecSignpostBackupOpenKeybag             = 8,
    SecSignpostUpgradePhase1                = 10,
    SecSignpostUpgradePhase2                = 12,
    SecSignpostBackupKeychainBackupable     = 14,
    SecSignpostRestoreKeychainBackupable    = 16,

    SecSignpostSecItemAdd                   = 18,
    SecSignpostSecItemUpdate                = 20,
    SecSignpostSecItemDelete                = 22,
    SecSignpostSecItemCopyMatching          = 24,


    SecSignpostImpulse                         = 0x1000,
    SecSignpostImpulseBackupClassCount         = 0x1001,
    SecSignpostImpulseRestoreClassCount        = 0x1002,
};


static inline void SecSignpostStart(SecSignpostType type) {
#if !TARGET_IPHONE_SIMULATOR
    kdebug_trace(ARIADNEDBG_CODE(SecSignpostComponent, type + 0), 0, 0, 0, 0);
#endif
}

static inline void SecSignpostStop(SecSignpostType type) {
#if !TARGET_IPHONE_SIMULATOR
    kdebug_trace(ARIADNEDBG_CODE(SecSignpostComponent, type + 1), 0, 0, 0, 0);
#endif
}

static inline void SecSignpostBackupCount(SecSignpostType type,
                                          CFStringRef cls,
                                          CFIndex count,
                                          unsigned filter) {
#if !TARGET_IPHONE_SIMULATOR
    if (CFStringGetLength(cls) != 4)
        return;
    unsigned char ucls[5];
    if (!CFStringGetCString(cls, (char *)ucls, sizeof(ucls), kCFStringEncodingUTF8))
        return;
    uint32_t c = (ucls[0] & 0xff) | (ucls[1] << 8) | (ucls[2] << 16) | (ucls[3] << 24);
    kdebug_trace(ARIADNEDBG_CODE(SecSignpostComponent, type), c, count, filter, 0);
#endif
}


#endif /* _SECSIGNPOST_H_ */
