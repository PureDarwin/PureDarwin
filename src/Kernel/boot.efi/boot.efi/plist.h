/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef BOOT_EFI_PLIST_H_INCLUDED
#define BOOT_EFI_PLIST_H_INCLUDED

enum {
    kPlistTypeNone = 0,
    kPlistTypeKey,
    kPlistTypeString,
    kPlistTypeInteger,
    kPlistTypeData,
    kPlistTypeDate,
    kPlistTypeBooleanTrue,
    kPlistTypeBooleanFalse,
    kPlistTypeDictionary,
    kPlistTypeArray,
};

struct PlistElementStruct {
    long type;
    char *string;
    struct PlistElementStruct *element;
    struct PlistElementStruct *nextElement;
};
typedef struct PlistElementStruct PlistElement, *PlistElementPtr;

PlistElementPtr     PlistFindValue(PlistElementPtr dictionary, char *key);
long                PlistParseNextElement(char *buffer, PlistElementPtr *elementPtr);
void                PlistFreeElement(PlistElementPtr element);

long                PlistParse(char *buffer, PlistElementPtr *rootDictionary);

#endif
