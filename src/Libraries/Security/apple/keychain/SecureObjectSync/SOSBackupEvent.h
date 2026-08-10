/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

//
//  SOSBackupEvent.h
//  sec
//

#ifndef _sec_SOSBackupEvent_
#define _sec_SOSBackupEvent_

#include <stdbool.h>
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>

bool SOSBackupEventWriteReset(FILE *journalFile, CFDataRef keybag, CFErrorRef *error);
bool SOSBackupEventWriteDelete(FILE *journalFile, CFDataRef deletedDigest, CFErrorRef *error);
bool SOSBackupEventWriteAdd(FILE *journalFile, CFDictionaryRef backup_item, CFErrorRef *error);
bool SOSBackupEventWriteCompleteMarker(FILE *journalFile, uint64_t eventID, CFErrorRef *error);

#endif /* defined(_sec_SOSBackupEvent_) */
