/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
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

#ifndef __DISKARBITRATIOND_DAMAIN__
#define __DISKARBITRATIOND_DAMAIN__

#include <sys/types.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const char *           kDAMainMountPointFolder;
extern const char *           kDAMainMountPointFolderCookieFile;
extern const char *           kDAMainDataVolumeMountPointFolder;

extern CFURLRef               gDABundlePath;
extern CFStringRef            gDAConsoleUser;
extern gid_t                  gDAConsoleUserGID;
extern uid_t                  gDAConsoleUserUID;
extern CFArrayRef             gDAConsoleUserList;
extern CFMutableArrayRef      gDADiskList;
extern Boolean                gDAExit;
extern CFMutableArrayRef      gDAFileSystemList;
extern CFMutableArrayRef      gDAFileSystemProbeList;
extern Boolean                gDAIdle;
extern io_iterator_t          gDAMediaAppearedNotification;
extern io_iterator_t          gDAMediaDisappearedNotification;
extern IONotificationPortRef  gDAMediaPort;
extern CFMutableArrayRef      gDAMountMapList1;
extern CFMutableArrayRef      gDAMountMapList2;
extern CFMutableDictionaryRef gDAPreferenceList;
extern pid_t                  gDAProcessID;
extern char *                 gDAProcessName;
extern char *                 gDAProcessNameID;
extern CFMutableArrayRef      gDARequestList;
extern CFMutableArrayRef      gDAResponseList;
extern CFMutableArrayRef      gDASessionList;
extern CFMutableDictionaryRef gDAUnitList;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DAMAIN__ */
