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


#ifndef _SECIOFORMAT_H_
#define _SECIOFORMAT_H_
// TODO: Name this file SecType.h?  To match inttype.h?

#include <inttypes.h>

// MARK: Guidlines and Examples

/*  Tips for using printf and CFStringCreateWithFormat style functions.

    Avoid using casts in arguments to these functions like the plague.  If you
    have to use them, think again.  You are probably wrong and you are writing
    non portable code.  Instead try following this pattern:

    Type        Format String       Variants
    size_t      "%zu"
    ssize_t     "%zd"
    ptrdiff_t   "%td"               printf("array_len: %td", pos - begin)
    OSStatus    "%"PRIdOSStatus     printf("%"PRIxCFIndex" returned", status)
    CFIndex     "%"PRIdCFIndex      printf("ar[%"PRIdCFIndex"]=%p", ix, CFArrayGetValueAtIndex(ar, ix))
    sint64_t    "%"PRId64
    uint64_t    "%"PRIu64           printf("sqlite3_rowid: %"PRIu64, rowid)
    sint32_t    "%"PRId32
    uint32_t    "%"PRIu32
    uint8_t     "%"PRIx8

    All of the above examples also work inside a CFSTR(). For example:
    CFStringAppendFormat(ms, NULL, CFSTR("ar[%"PRIdCFIndex"]=%p"), ix, CFArrayGetValueAtIndex(ar, ix))
    Also you can use any of d i o u x X where appropriate for some of these types
    although u x X are for unsigned types only.

    Try to avoid using these types unless you know what you are doing because
    they can lead to portability issues on different flavors of 32 and 64 bit
    platforms:

    int         "%d"
    long        "%ld"
    long long   "%lld"
 */

// MARK: CFIndex printing support

// Note that CFIndex is signed so the u variants won't work.
#ifdef __LLP64__
#  define PRIdCFIndex    "lld"
#  define PRIiCFIndex    "lli"
#  define PRIoCFIndex    "llo"
#  define PRIuCFIndex    "llu"
#  define PRIxCFIndex    "llx"
#  define PRIXCFIndex    "llX"
#else
#  define PRIdCFIndex    "ld"
#  define PRIiCFIndex    "li"
#  define PRIoCFIndex    "lo"
#  define PRIuCFIndex    "lu"
#  define PRIxCFIndex    "lx"
#  define PRIXCFIndex    "lX"
#endif

// MARK: OSStatus printing support

// Note that OSStatus is signed so the u variants won't work.
#ifdef __LP64__
#  define PRIdOSStatus    "d"
#  define PRIiOSStatus    "i"
#  define PRIoOSStatus    "o"
#  define PRIuOSStatus    "u"
#  define PRIxOSStatus    "x"
#  define PRIXOSStatus    "X"
#else
#  define PRIdOSStatus    "ld"
#  define PRIiOSStatus    "li"
#  define PRIoOSStatus    "lo"
#  define PRIuOSStatus    "lu"
#  define PRIxOSStatus    "lx"
#  define PRIXOSStatus    "lX"
#endif

#endif
