/*	CFICUConverters.c
	Copyright (c) 2004-2018, Apple Inc. and the Swift project authors

	Portions Copyright (c) 2014-2018, Apple Inc. and the Swift project authors
	Licensed under Apache License v2.0 with Runtime Library Exception
	See http://swift.org/LICENSE.txt for license information
	See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
	Responsibility: Foundation Team

	PureDarwin: no real ICU runtime library exists here (only the SDK's
	link-time-only libicucore.tbd stub, with no dylib behind it), so any
	code path that actually reaches into ICU fails at runtime - some
	eagerly (data-symbol references like &UCNV_FROM_U_CALLBACK_STOP need
	load-time binding and crash dyld immediately), some lazily (plain
	ICU function calls only fail once invoked). This file's job is ICU-
	backed encoding conversion for encodings CF's own fast-path converters
	(UTF-8, ASCII, MacRoman, ISOLatin1, etc. - see CFStringEncodingConverter.c)
	don't handle directly - i.e. already a fallback path for uncommon
	encodings, not something CF's core string operations depend on. Stub
	it out entirely rather than chase individual ICU symbols one at a time.
*/

#include "CFStringEncodingDatabase.h"
#include "CFStringEncodingConverterPriv.h"
#include "CFICUConverters.h"
#include <CoreFoundation/CFStringEncodingExt.h>
#include <CoreFoundation/CFUniChar.h>
#include "CFInternal.h"

CF_PRIVATE const char *__CFStringEncodingGetICUName(CFStringEncoding encoding) {
    return NULL;
}

CF_PRIVATE CFStringEncoding __CFStringEncodingGetFromICUName(const char *icuName) {
    return kCFStringEncodingInvalidId;
}

CF_PRIVATE CFIndex __CFStringEncodingICUToBytes(const char *icuName, uint32_t flags, const UniChar *characters, CFIndex numChars, CFIndex *usedCharLen, uint8_t *bytes, CFIndex maxByteLen, CFIndex *usedByteLen) {
    if (usedCharLen) *usedCharLen = 0;
    if (usedByteLen) *usedByteLen = 0;
    return kCFStringEncodingConverterUnavailable;
}

CF_PRIVATE CFIndex __CFStringEncodingICUToUnicode(const char *icuName, uint32_t flags, const uint8_t *bytes, CFIndex numBytes, CFIndex *usedByteLen, UniChar *characters, CFIndex maxCharLen, CFIndex *usedCharLen) {
    if (usedByteLen) *usedByteLen = 0;
    if (usedCharLen) *usedCharLen = 0;
    return kCFStringEncodingConverterUnavailable;
}

CF_PRIVATE CFIndex __CFStringEncodingICUCharLength(const char *icuName, uint32_t flags, const uint8_t *bytes, CFIndex numBytes) {
    return 0;
}

CF_PRIVATE CFIndex __CFStringEncodingICUByteLength(const char *icuName, uint32_t flags, const UniChar *characters, CFIndex numChars) {
    return 0;
}

CF_PRIVATE CFStringEncoding *__CFStringEncodingCreateICUEncodings(CFAllocatorRef allocator, CFIndex *numberOfIndex) {
    *numberOfIndex = 0;
    return NULL;
}
