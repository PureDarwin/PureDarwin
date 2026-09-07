/* Copyright (c) 2009 Jens Ayton
   Copyright (C) 2024-2026 Zoe Knox <zoe@ravynsoft.com>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE. */

/* IMPORTANT: these are for internal use only. They are subject to change
   without notice. Use the appropriate functions (e.g. CFStringGetTypeID())
   rather than assuming specific values.

   Every name here is an alias for the corresponding_kCFRuntimeIDCF* in
   <CoreFoundation/CFRuntime_Internal.h> rather than a hard-coded number,
   so the two tables cannot drift apart.
*/

#ifndef _FOUNDATION_NSCFTYPEID_H_
#define _FOUNDATION_NSCFTYPEID_H_

#include <CoreFoundation/CFRuntime_Internal.h>

enum {
    kNSCFTypeNotAType         = _kCFRuntimeIDNotAType,
    kNSCFTypeTypeID           = _kCFRuntimeIDCFType,
    kNSCFTypeAllocator        = _kCFRuntimeIDCFAllocator,
    kNSCFTypeBasicHash        = _kCFRuntimeIDCFBasicHash,
    kNSCFTypeBag              = _kCFRuntimeIDCFBag,
    kNSCFTypeString           = _kCFRuntimeIDCFString,
    kNSCFTypeNull             = _kCFRuntimeIDCFNull,
    kNSCFTypeSet              = _kCFRuntimeIDCFSet,
    kNSCFTypeDictionary       = _kCFRuntimeIDCFDictionary,
    kNSCFTypeArray            = _kCFRuntimeIDCFArray,
    kNSCFTypeData             = _kCFRuntimeIDCFData,
    kNSCFTypeBoolean          = _kCFRuntimeIDCFBoolean,
    kNSCFTypeNumber           = _kCFRuntimeIDCFNumber,
    kNSCFTypeBinaryHeap       = _kCFRuntimeIDCFBinaryHeap,
    kNSCFTypeBitVector        = _kCFRuntimeIDCFBitVector,
    kNSCFTypeCharacterSet     = _kCFRuntimeIDCFCharacterSet,
    kNSCFTypeStorage          = _kCFRuntimeIDCFStorage,
    kNSCFTypeError            = _kCFRuntimeIDCFError,
    kNSCFTypeTree             = _kCFRuntimeIDCFTree,
    kNSCFTypeURL              = _kCFRuntimeIDCFURL,
    kNSCFTypeURLComponents    = _kCFRuntimeIDCFURLComponents,
    kNSCFTypeBundle           = _kCFRuntimeIDCFBundle,
    kNSCFTypePFactory         = _kCFRuntimeIDCFPFactory,
    kNSCFTypePlugInInstance   = _kCFRuntimeIDCFPlugInInstance,
    kNSCFTypeUUID             = _kCFRuntimeIDCFUUID,
    kNSCFTypeMessagePort      = _kCFRuntimeIDCFMessagePort,
#if TARGET_OS_MAC
    kNSCFTypeMachPort         = _kCFRuntimeIDCFMachPort,
#endif
    /* CF splits what ravynOS calls "Stream" into read and write halves. */
    kNSCFTypeReadStream       = _kCFRuntimeIDCFReadStream,
    kNSCFTypeWriteStream      = _kCFRuntimeIDCFWriteStream,
    kNSCFTypeKeyedArchiverUID = _kCFRuntimeIDCFKeyedArchiverUID,
    kNSCFTypeDate             = _kCFRuntimeIDCFDate,
    kNSCFTypeRunLoop          = _kCFRuntimeIDCFRunLoop,
    kNSCFTypeRunLoopMode      = _kCFRuntimeIDCFRunLoopMode,
    kNSCFTypeRunLoopObserver  = _kCFRuntimeIDCFRunLoopObserver,
    kNSCFTypeRunLoopSource    = _kCFRuntimeIDCFRunLoopSource,
    kNSCFTypeRunLoopTimer     = _kCFRuntimeIDCFRunLoopTimer,
    kNSCFTypeTimeZone         = _kCFRuntimeIDCFTimeZone,
    kNSCFTypeCalendar         = _kCFRuntimeIDCFCalendar,
    kNSCFTypeNotificationCenter = _kCFRuntimeIDCFNotificationCenter,
    kNSCFTypeLocale           = _kCFRuntimeIDCFLocale,
    kNSCFTypeSocket           = _kCFRuntimeIDCFSocket,
    kNSCFTypeAttributedString = _kCFRuntimeIDCFAttributedString
};

/* The -_cfTypeID accessor is declared with the rest of the bridge support,
   in CoreFoundation's Bridging.subproj/__NSCFType.h. */

#endif /* _FOUNDATION_NSCFTYPEID_H_ */
