/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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


#include <utilities/SecCFWrappers.h>
#include <utilities/SecBuffer.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonRandom.h>

CFStringRef kSecDebugFormatOption = CFSTR("debug");

//
// Global singleton CopyDebugDescription format dictionary getter.
//
CFGiblisGetSingleton(CFDictionaryRef, SecGetDebugDescriptionFormatOptions, formatOption, ^{
    const void *k[] = { kSecDebugFormatOption };
    const void *v[] = { kCFBooleanTrue };
    *formatOption = CFDictionaryCreate(kCFAllocatorDefault, k, v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
})

//
// Smart comparator for strings that matches sorting functions
//

CFComparisonResult CFStringCompareSafe(const void *val1, const void *val2, void *context) {
    if (!isString(val1))
        return kCFCompareLessThan;
    if (!isString(val2))
        return kCFCompareGreaterThan;
    
    return CFStringCompare(val1, val2, 0);
}

void CFStringArrayPerformWithDelimiterWithDescription(CFArrayRef strings, CFStringRef start, CFStringRef end, void (^action)(CFStringRef description)) {
    if(!strings) {
        action(CFSTR("null"));
    } else {
        __block CFMutableStringRef description = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, start);
        __block CFStringRef separator = CFSTR("");
        
        CFArrayForEach(strings, ^(const void *value) {
            CFStringAppendFormat(description, NULL, CFSTR("%@%@"), separator, value);
            separator = CFSTR(", ");
        });
        
        CFStringAppend(description, end);
        
        action(description);
        
        CFReleaseNull(description);
    }
}


void CFStringArrayPerformWithDescription(CFArrayRef strings, void (^action)(CFStringRef description)) {
    CFStringArrayPerformWithDelimiterWithDescription(strings, CFSTR("["), CFSTR("]"), action);
}

static void
appendDescriptionToArray(const void *value, void *context)
{
    CFTypeRef obj = (CFTypeRef)value;
    CFMutableArrayRef array = (CFMutableArrayRef)context;
    CFStringRef desc;

    if (CFGetTypeID(obj) == CFStringGetTypeID()) {
        CFArrayAppendValue(array, obj);
    } else {
        desc = CFCopyDescription(obj);
        if (desc != NULL) {
            CFArrayAppendValue(array, desc);
            CFRelease(desc);
        } else {
            CFArrayAppendValue(array, CFSTR("null"));
        }
    }
}

void CFStringSetPerformWithDescription(CFSetRef set, void (^action)(CFStringRef description)) {
    if(!set) {
        action(CFSTR("null"));
    } else {
        CFMutableArrayRef keys = CFArrayCreateMutable(NULL, CFSetGetCount(set), &kCFTypeArrayCallBacks);
        
        CFSetApplyFunction(set, appendDescriptionToArray, keys);

        CFArraySortValues(keys, CFRangeMake(0, CFArrayGetCount(keys)), (CFComparatorFunction)&CFStringCompare, NULL);
        
        CFStringArrayPerformWithDelimiterWithDescription(keys, CFSTR("{("), CFSTR(")}"), action);
        
        CFReleaseNull(keys);
    }
}

//
// Global sigleton Zulu time. Must be serialized since it is really a CFMutableCalendarRef
//  <rdar://problem/16372688> CFCalendarDecomposeAbsoluteTime is not thread safe
//
static dispatch_queue_t fqueue_cf;
static CFCalendarRef sZuluCalendar = NULL;

static dispatch_queue_t SecCFCalendarGetZuluQueue() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        fqueue_cf = dispatch_queue_create("ZuluCalendar", DISPATCH_QUEUE_SERIAL);
    });
    return fqueue_cf;
}

static CFCalendarRef SecCFCalendarGetZulu() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sZuluCalendar = CFCalendarCreateWithIdentifier(kCFAllocatorDefault, kCFGregorianCalendar);
        CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(kCFAllocatorDefault, 0.0);
        CFCalendarSetTimeZone(sZuluCalendar, tz);
        if (tz)
            CFRelease(tz);
    });
    return sZuluCalendar;
}

void SecCFCalendarDoWithZuluCalendar(void(^action)(CFCalendarRef zuluCalendar)) {
    dispatch_sync(SecCFCalendarGetZuluQueue(), ^{
        action(SecCFCalendarGetZulu());
    });
}

void CFStringPerformWithCStringAndLength(CFStringRef inStr, void(^operation)(const char *utf8String, size_t utf8Length)) {
    const char *cstr = CFStringGetCStringPtr(inStr, kCFStringEncodingUTF8);
    if (cstr) {
        operation(cstr, strlen(cstr));
    } else {
        CFIndex neededLen = 0;
        CFRange range = { 0, CFStringGetLength(inStr) };

        CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                         0, FALSE, NULL, 0, &neededLen);
        
        // + 1 bytes for the '\0' we're adding.
        PerformWithBuffer(neededLen + 1, ^(size_t size, uint8_t *buf) {
            if (buf) {
                CFIndex usedLen;
                CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                                 0, FALSE, (UInt8 *)buf, neededLen, &usedLen);
                assert(usedLen == neededLen);
                buf[usedLen] = 0;
                operation((const char *) buf, (size_t)usedLen);
            }
        });
    }
}

void CFStringPerformWithCString(CFStringRef inStr, void(^operation)(const char *utf8String)) {
    CFStringPerformWithCStringAndLength(inStr, ^(const char *utf8String, size_t utf8Length) {
        operation(utf8String);
    });
}

void CFStringPerformWithUTF8CFData(CFStringRef inStr, void (^operation)(CFDataRef stringAsData)) {
    CFIndex neededLen = 0;
    CFRange range = { 0, CFStringGetLength(inStr) };

    CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                     0, FALSE, NULL, 0, &neededLen);

    CFMutableDataRef data = CFDataCreateMutableWithScratch(kCFAllocatorDefault, neededLen);

    CFIndex usedLen;
    CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                     0, FALSE, CFDataGetMutableBytePtr(data), neededLen, &usedLen);
    assert(usedLen == neededLen);

    operation(data);

    CFReleaseNull(data);
}


CFStringRef CFDictionaryCopyCompactDescription(CFDictionaryRef dictionary) {
    CFStringRef result = NULL;
    if (dictionary) {
        CFMutableStringRef compactDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("{"));
        __block CFStringRef separator = CFSTR("");

        CFDictionaryForEach(dictionary, ^(const void *key, const void *value) {
            CFMutableStringRef valueDescription = NULL;
            if (isData(value)) {
                valueDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("0x"));
                CFStringAppendHexData(valueDescription, (CFDataRef) value);
            }
            CFStringAppendFormat(compactDescription, NULL, CFSTR("%@%@:%@"), separator, key, valueDescription ? valueDescription : value);
            separator = CFSTR(", ");
            CFReleaseNull(valueDescription);
        });

        CFStringAppendFormat(compactDescription, NULL, CFSTR("}"));

        result = compactDescription;
    } else {
        result = CFCopyDescription(kCFNull);
    }

    return result;
}

CFStringRef CFDictionaryCopySuperCompactDescription(CFDictionaryRef dictionary) {
    CFStringRef result = NULL;
    if (dictionary) {
        CFMutableStringRef compactDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("{"));
        __block CFStringRef separator = CFSTR("");
        
        CFDictionaryForEach(dictionary, ^(const void *key, const void *value) {
            CFMutableStringRef valueDescription = NULL;
            
            if (isData(value)) {
                valueDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("0x"));
                CFStringAppendHexData(valueDescription, (CFDataRef) value);
                CFStringDelete(valueDescription, CFRangeMake(0, 5));
            }
            else if(isString(value)){
                CFStringRef stringValue = NULL;
                if(CFStringGetLength(value) < 6)
                    stringValue = CFStringCreateCopy(kCFAllocatorDefault, value);
                else
                    stringValue = CFStringCreateWithSubstring(kCFAllocatorDefault, value, CFRangeMake(0, 6));
                valueDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, CFStringGetLength(stringValue), stringValue);
                CFReleaseNull(stringValue);
            }
            else if(isNumber(value)){
                CFStringRef stringValue = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), (CFNumberRef)value);
                valueDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, CFStringGetLength(stringValue), stringValue);
                CFReleaseNull(stringValue);
            }
            
            UniChar firstCharOfKey = CFStringGetCharacterAtIndex(key, 0);
            CFStringAppendFormat(compactDescription, NULL, CFSTR("%c:%@ "), firstCharOfKey, valueDescription);
            separator = CFSTR(", ");
            CFReleaseNull(valueDescription);
        });
        
        CFStringAppendFormat(compactDescription, NULL, CFSTR("}"));
        
        result = compactDescription;
    } else {
        result = CFCopyDescription(kCFNull);
    }
    
    return result;
}


CFDataRef CFDataCreateWithRandomBytes(size_t len) {
    __block CFDataRef retval = NULL;
    PerformWithBufferAndClear(len, ^(size_t size, uint8_t *buffer) {
        CCRandomGenerateBytes(buffer, size);
        retval = CFDataCreate(NULL, buffer, size);
    });
    return retval;
}

CFDataRef CFDataCreateWithInitializer(CFAllocatorRef allocator, CFIndex size, bool (^operation)(size_t size, uint8_t *buffer)) {
    __block CFMutableDataRef result = NULL;
    if(!size) return NULL;
    if((result = CFDataCreateMutableWithScratch(allocator, size)) == NULL) return NULL;
    if (!operation(size, CFDataGetMutableBytePtr(result))) CFReleaseNull(result);
    return result;
}



CFGiblisGetSingleton(CFDateFormatterRef, GetShortDateFormatter, sDateFormatter, ^{
    CFLocaleRef locale = CFLocaleCopyCurrent();
    *sDateFormatter = CFDateFormatterCreate(kCFAllocatorDefault, locale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
    
    CFDateFormatterSetFormat(*sDateFormatter, CFSTR("yyyy-MM-dd HH:mm"));
    CFReleaseNull(locale);
})

CFGiblisGetSingleton(dispatch_queue_t, GetShortDateFormatterQueue, sDateFormatQueue, ^{
    *sDateFormatQueue = dispatch_queue_create("Date Formatting", DISPATCH_QUEUE_SERIAL);
})

//
// MARK: time formatters
//

static void withShortDateFormatter(void (^action)(CFDateFormatterRef formatter)) {
    dispatch_sync(GetShortDateFormatterQueue(), ^{
        action(GetShortDateFormatter());
    });
}

void withStringOfAbsoluteTime(CFAbsoluteTime at, void (^action)(CFStringRef decription)) {
    __block CFStringRef formattedString = NULL;
    
    withShortDateFormatter(^(CFDateFormatterRef formatter) {
        formattedString = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, formatter, at);
    });
    action(formattedString);
    
    CFReleaseNull(formattedString);
}
