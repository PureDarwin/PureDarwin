/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include <xpc/xpc.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CoreFoundation.h>

#ifndef _UTILITIES_SECURITYDXPC_H_
#define _UTILITIES_SECURITYDXPC_H_

bool SecXPCDictionarySetData(xpc_object_t message, const char *key, CFDataRef data, CFErrorRef *error);
bool SecXPCDictionarySetDataOptional(xpc_object_t message, const char *key, CFDataRef data, CFErrorRef *error);

bool SecXPCDictionarySetBool(xpc_object_t message, const char *key, bool value, CFErrorRef *error);

bool SecXPCDictionarySetPList(xpc_object_t message, const char *key, CFTypeRef object, CFErrorRef *error);
bool SecXPCDictionarySetPListWithRepair(xpc_object_t message, const char *key, CFTypeRef object, bool repair, CFErrorRef *error);
bool SecXPCDictionarySetPListOptional(xpc_object_t message, const char *key, CFTypeRef object, CFErrorRef *error);

bool SecXPCDictionarySetString(xpc_object_t message, const char *key, CFStringRef string, CFErrorRef *error);
bool SecXPCDictionarySetStringOptional(xpc_object_t message, const char *key, CFStringRef string, CFErrorRef *error);

bool SecXPCDictionarySetInt64(xpc_object_t message, const char *key, int64_t value, CFErrorRef *error);

bool SecXPCDictionarySetFileDescriptor(xpc_object_t message, const char *key, int fd, CFErrorRef *error);
int SecXPCDictionaryDupFileDescriptor(xpc_object_t message, const char *key, CFErrorRef *error);

CFTypeRef SecXPCDictionaryCopyPList(xpc_object_t message, const char *key, CFErrorRef *error);
bool SecXPCDictionaryCopyPListOptional(xpc_object_t message, const char *key, CFTypeRef *pobject, CFErrorRef *error);

CFArrayRef SecXPCDictionaryCopyArray(xpc_object_t message, const char *key, CFErrorRef *error);
bool SecXPCDictionaryCopyArrayOptional(xpc_object_t message, const char *key, CFArrayRef *parray, CFErrorRef *error);

CFDataRef SecXPCDictionaryCopyData(xpc_object_t message, const char *key, CFErrorRef *error);
bool SecXPCDictionaryCopyDataOptional(xpc_object_t message, const char *key, CFDataRef *pdata, CFErrorRef *error);

bool SecXPCDictionaryCopyURLOptional(xpc_object_t message, const char *key, CFURLRef *purl, CFErrorRef *error);

bool SecXPCDictionaryGetBool(xpc_object_t message, const char *key, CFErrorRef *error);

bool SecXPCDictionaryGetDouble(xpc_object_t message, const char *key, double *pvalue, CFErrorRef *error) ;

CFDictionaryRef SecXPCDictionaryCopyDictionary(xpc_object_t message, const char *key, CFErrorRef *error);

bool SecXPCDictionaryCopyDictionaryOptional(xpc_object_t message, const char *key, CFDictionaryRef *pdictionary, CFErrorRef *error);

CFStringRef SecXPCDictionaryCopyString(xpc_object_t message, const char *key, CFErrorRef *error);

bool SecXPCDictionaryCopyStringOptional(xpc_object_t message, const char *key, CFStringRef *pstring, CFErrorRef *error);

CFSetRef SecXPCDictionaryCopySet(xpc_object_t message, const char *key, CFErrorRef *error);

/* For an xpc_array of datas */
bool SecXPCDictionaryCopyCFDataArrayOptional(xpc_object_t message, const char *key, CFArrayRef *data_array, CFErrorRef *error);

#endif
