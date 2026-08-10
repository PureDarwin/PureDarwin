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


#include "utilities/SecCFRelease.h"
#include "utilities/SecCFWrappers.h"
#include "utilities/der_date.h"
#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include <corecrypto/ccder.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFCalendar.h>
#include <math.h>

#define NULL_TIME       NAN
#define IS_NULL_TIME(x) isnan(x)

/* Cumulative number of days in the year for months up to month i.  */
static int mdays[13] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

static bool validateDateComponents(int year, int month, int day, int hour, int minute, int second, int *is_leap_year, CFErrorRef* error)
{
    int leapyear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 1 : 0;
    if (is_leap_year) {
        *is_leap_year = leapyear;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour >= 24 || minute >= 60 || second > 61
        || (month == 2 && day > mdays[month] - mdays[month - 1] + leapyear)
        || (month != 2 && day > mdays[month] - mdays[month - 1]))
    {
        SecError(kSecDERErrorUnknownEncoding, error, CFSTR("Invalid date: %i, %i, %i, %i, %i, %i, %i"), year, month, day, hour, minute, second, leapyear);
        return false;
    }

    return true;
}

static CFAbsoluteTime SecGregorianDateGetAbsoluteTime(int year, int month, int day, int hour, int minute, int second, CFTimeInterval timeZoneOffset, CFErrorRef *error) {
    int is_leap_year = 0;
    if (!validateDateComponents(year, month, day, hour, minute, second, &is_leap_year, error)) {
        return NULL_TIME;
    }

    int dy = year - 2001;
    if (dy < 0) {
        dy += 1;
        day -= 1;
    }

    int leap_days = dy / 4 - dy / 100 + dy / 400;
    day += ((year - 2001) * 365 + leap_days) + mdays[month - 1] - 1;
    if (month > 2)
        day += is_leap_year;

    CFAbsoluteTime absTime = (CFAbsoluteTime)((day * 24 + hour) * 60 + minute) * 60 + second;
	return absTime - timeZoneOffset;
}

static bool SecAbsoluteTimeGetGregorianDate(CFTimeInterval at, int *year, int *month, int *day, int *hour, int *minute, int *second, CFErrorRef *error) {
    // TODO: Remove CFCalendarDecomposeAbsoluteTime dependancy because
    // CFTimeZoneCreateWithTimeIntervalFromGMT is expensive and requires
    // filesystem access to timezone files when we are only doing zulu time anyway
    __block bool result;
    SecCFCalendarDoWithZuluCalendar(^(CFCalendarRef zuluCalendar) {
        result = CFCalendarDecomposeAbsoluteTime(zuluCalendar, at, "yMdHms", year, month, day, hour, minute, second);
    });
    if (!result) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Failed to encode date."), 0, error);
    }

    return result;
}

static int der_get_char(const uint8_t **der_p, const uint8_t *der_end,
                         CFErrorRef *error) {
    const uint8_t *der = *der_p;
    if (!der) {
        /* Don't create a new error in this case. */
        return -1;
    }

    if (der >= der_end) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding,
                            CFSTR("Unexpected end of datetime"), 0, error);
        *der_p = NULL;
        return -1;
    }

    int ch = *der++;
    *der_p = der;
    return ch;
}


static int der_decode_decimal(const uint8_t **der_p, const uint8_t *der_end,
                             CFErrorRef *error) {
    char ch = der_get_char(der_p, der_end, error);
    if (ch < '0' || ch > '9') {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding,
                            CFSTR("Not a decimal digit"), 0, error);
        *der_p = NULL;
        return -1;
    }
    return ch - '0';
}

static int der_decode_decimal_pair(const uint8_t **der_p, const uint8_t *der_end,
                            CFErrorRef *error) {
    return (10 * der_decode_decimal(der_p, der_end, error))
        + der_decode_decimal(der_p, der_end, error);
}

static int der_peek_byte(const uint8_t *der, const uint8_t *der_end) {
    if (!der || der >= der_end)
        return -1;

    return *der;
}

static const uint8_t *der_decode_decimal_fraction(double *fraction, CFErrorRef *error,
                                                  const uint8_t* der, const uint8_t *der_end) {
    int ch = der_peek_byte(der, der_end);
    if (ch == -1) {
        der = NULL;
    } else if (ch == '.') {
        uint64_t divisor = 1;
        uint64_t value = 0;
        int last = -1;
        while (++der < der_end) {
            last = ch;
            ch = *der;
            if (ch < '0' || ch > '9') {
                break;
            }
            if (divisor < UINT64_MAX / 10) {
                divisor *= 10;
                value *= 10;
                value += (ch - '0');
            }
        }
        if (der >= der_end) {
            SecCFDERCreateError(kSecDERErrorOverflow,
                                CFSTR("overflow"), 0, error);
            der = NULL;
        } else if (last == '0') {
            SecCFDERCreateError(kSecDERErrorUnknownEncoding,
                                CFSTR("fraction ends in 0"), 0, error);
            der = NULL;
        } else if (last == '.') {
            SecCFDERCreateError(kSecDERErrorUnknownEncoding,
                                CFSTR("fraction without digits"), 0, error);
            der = NULL;
        } else {
            *fraction = (double)value / divisor;
        }
    } else {
        *fraction = 0.0;
    }

    return der;
}

static CFTimeInterval der_decode_timezone_offset(const uint8_t **der_p,
                                                       const uint8_t *der_end,
                                                       CFErrorRef *error) {
    CFTimeInterval timeZoneOffset;
    int ch = der_get_char(der_p, der_end, error);
    if (ch == 'Z') {
        /* Zulu time. */
        timeZoneOffset = 0.0;
    } else {
		/* ZONE INDICATOR */
        int multiplier;
        if (ch == '-')
            multiplier = -60;
        else if (ch == '+')
            multiplier = +60;
        else {
            SecCFDERCreateError(kSecDERErrorUnknownEncoding,
                                CFSTR("Invalid datetime character"), 0, error);
            return NULL_TIME;
        }
        
        timeZoneOffset = multiplier *
            (der_decode_decimal_pair(der_p, der_end, error)
             * 60 + der_decode_decimal_pair(der_p, der_end, error));
    }
    return timeZoneOffset;
}

static const uint8_t* der_decode_commontime_body(CFAbsoluteTime *at, CFErrorRef *error, int year,
                                                 const uint8_t* der, const uint8_t *der_end)
{
	int month = der_decode_decimal_pair(&der, der_end, error);
	int day = der_decode_decimal_pair(&der, der_end, error);
	int hour = der_decode_decimal_pair(&der, der_end, error);
	int minute = der_decode_decimal_pair(&der, der_end, error);
    int second = der_decode_decimal_pair(&der, der_end, error);
    double fraction;
    der = der_decode_decimal_fraction(&fraction, error, der, der_end);

	CFTimeInterval timeZoneOffset = der_decode_timezone_offset(&der, der_end, error);

    if (der) {
        if (der != der_end) {
            SecCFDERCreateError(kSecDERErrorUnknownEncoding,
                                CFSTR("trailing garbage at end of datetime"), 0, error);
            return NULL;
        }

        *at = SecGregorianDateGetAbsoluteTime(year, month, day, hour, minute, second, timeZoneOffset, error);
        if (IS_NULL_TIME(*at))
            return NULL;

        *at += fraction;
    }

    return der;
}

const uint8_t* der_decode_generalizedtime_body(CFAbsoluteTime *at, CFErrorRef *error,
                                               const uint8_t* der, const uint8_t *der_end)
{
    int year = 100 * der_decode_decimal_pair(&der, der_end, error) + der_decode_decimal_pair(&der, der_end, error);
    return der_decode_commontime_body(at, error, year, der, der_end);
}

const uint8_t* der_decode_universaltime_body(CFAbsoluteTime *at, CFErrorRef *error,
                                             const uint8_t* der, const uint8_t *der_end)
{
    SInt32 year = der_decode_decimal_pair(&der, der_end, error);
    if (year < 50) {
        /* 0  <= year <  50 : assume century 21 */
        year += 2000;
    } else if (year < 70) {
        /* 50 <= year <  70 : illegal per PKIX */
        SecCFDERCreateError(kSecDERErrorUnknownEncoding,
                            CFSTR("Invalid universal time year between 50 and 70"), 0, error);
        der = NULL;
    } else {
        /* 70 <  year <= 99 : assume century 20 */
        year += 1900;
    }

    return der_decode_commontime_body(at, error, year, der, der_end);
}

const uint8_t* der_decode_date(CFAllocatorRef allocator,
                               CFDateRef* date, CFErrorRef *error,
                               const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorNullInput, CFSTR("null input"), NULL, error);
        return NULL;
    }

    der = ccder_decode_constructed_tl(CCDER_GENERALIZED_TIME, &der_end, der, der_end);
    CFAbsoluteTime at = 0;
    der = der_decode_generalizedtime_body(&at, error, der, der_end);
    if (der) {
        *date = CFDateCreate(allocator, at);
        if (NULL == *date) {
            SecCFDERCreateError(kSecDERErrorAllocationFailure, CFSTR("Failed to create date"), NULL, error);
            return NULL;
        }
    }
    return der;
}

extern char *__dtoa(double _d, int mode, int ndigits, int *decpt, int *sign, char **rve);
extern void  __freedtoa(char *);

static size_t ccder_sizeof_nanoseconds(CFAbsoluteTime at) {
    int dotoff;
    int sign;
    char *end;
    char *str = __dtoa(at, 0, 0, &dotoff, &sign, &end);
    ptrdiff_t len = end - str;
    __freedtoa(str);
    return len < dotoff ? 0 : len - dotoff;
    //return len < dotoff ? 0 : len - dotoff > 9 ? 9 : len - dotoff;
}

size_t der_sizeof_generalizedtime_body(CFAbsoluteTime at, CFErrorRef *error)
{
    size_t subsec_digits = ccder_sizeof_nanoseconds(at);

    /* Generalized zulu time YYYYMMDDhhmmss[.ssss]Z */
    return subsec_digits ? 16 + subsec_digits : 15;
}

size_t der_sizeof_generalizedtime(CFAbsoluteTime at, CFErrorRef *error)
{
    return ccder_sizeof(CCDER_GENERALIZED_TIME,
                        der_sizeof_generalizedtime_body(at, error));
}

size_t der_sizeof_date(CFDateRef date, CFErrorRef *error)
{
    return der_sizeof_generalizedtime(CFDateGetAbsoluteTime(date), error);
}


static uint8_t *ccder_encode_byte(uint8_t byte,
                                  const uint8_t *der, uint8_t *der_end) {
    if (der + 1 > der_end) {
        return NULL;
    }
    *--der_end = byte;
    return der_end;
}

static uint8_t *ccder_encode_decimal_pair(int v, const uint8_t *der,
                                          uint8_t *der_end) {
    if (der_end == NULL || der + 2 > der_end) {
        return NULL;
    }
    assert(v < 100);
    *--der_end = '0' + v % 10;
    *--der_end = '0' + v / 10;
    return der_end;
}

static uint8_t *ccder_encode_decimal_quad(int v, const uint8_t *der,
                                          uint8_t *der_end) {
    return ccder_encode_decimal_pair(v / 100, der,
           ccder_encode_decimal_pair(v % 100, der, der_end));
}

static uint8_t *ccder_encode_nanoseconds(CFAbsoluteTime at, const uint8_t *der,
                                         uint8_t *der_end) {
    int dotoff;
    int sign;
    char *end;
    char *str = __dtoa(at, 0, 0, &dotoff, &sign, &end);
    char *begin = str + (dotoff < 0 ? 0 : dotoff);
    // Compute 1.0000000 - fraction in ascii space
    if (at < 0.0 && begin < end) {
        char *p = end - 1;
        // Borrow for last digit
        *p = ('9' + 1) - (*p - '0');
        while (p-- > begin) {
            // Every other digit is a 9 since we borrowed from the last one
            *p = '9' - (*p - '0');
        }
    }

    ptrdiff_t len = end - str;
    if (len > dotoff) {
        if (dotoff < 0) {
            assert(-1.0 < at && at < 1.0);
            der_end = ccder_encode_body(len, (const uint8_t *)str, der, der_end);
            der_end = ccder_encode_body_nocopy(-dotoff, der, der_end);
            if (der_end)
                memset(der_end, at < 0.0 ? '9' : '0', -dotoff);
        } else {
            der_end = ccder_encode_body(len - dotoff, (const uint8_t *)(str + dotoff), der, der_end);
        }
        der_end = ccder_encode_byte('.', der, der_end);
    }
    __freedtoa(str);

    return der_end;
}

uint8_t* der_encode_generalizedtime_body(CFAbsoluteTime at, CFErrorRef *error,
                                         const uint8_t *der, uint8_t *der_end)
{
    return der_encode_generalizedtime_body_repair(at, error, false, der, der_end);
}

/* Encode generalized zulu time YYYYMMDDhhmmss[.ssss]Z */
uint8_t* der_encode_generalizedtime_body_repair(CFAbsoluteTime at, CFErrorRef *error, bool repair,
                                         const uint8_t *der, uint8_t *der_end)
{
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!SecAbsoluteTimeGetGregorianDate(at, &year, &month, &day, &hour, &minute, &second, error)) {
        secerror("der: unable to encode date: %@", error ? *error : NULL);
        return NULL;
    }

    // Without repair flag, do not let validation change result so we do not change existing behavior
    CFErrorRef localError = NULL;
    if (!validateDateComponents(year, month, day, hour, minute, second, 0, &localError)) {
        CFStringRef desc = CFErrorCopyDescription(localError);
        __security_simulatecrash(desc, __sec_exception_code_CorruptItem);
        CFReleaseNull(desc);
        secerror("der: invalid date: %@; %s", localError, repair ? "setting default value" : "continuing");
        CFReleaseNull(localError);
        if (repair) {
            year = 2001;
            month = 1;
            day = 1;
            // I don't think this is required but I don't want any funny business with exactly midnight in any timezone
            minute = 1;
        }
    }

    uint8_t * result = ccder_encode_decimal_quad(year, der,
           ccder_encode_decimal_pair(month, der,
           ccder_encode_decimal_pair(day, der,
           ccder_encode_decimal_pair(hour, der,
           ccder_encode_decimal_pair(minute, der,
           ccder_encode_decimal_pair(second, der,
           ccder_encode_nanoseconds(at, der,    // Uses original "at" even after repair because DER length calculations depend on its exact value
           ccder_encode_byte('Z', der, der_end))))))));

    return SecCCDEREncodeHandleResult(result, error);
}

uint8_t* der_encode_generalizedtime(CFAbsoluteTime at, CFErrorRef *error,
                                    const uint8_t *der, uint8_t *der_end)
{
    return SecCCDEREncodeHandleResult(ccder_encode_constructed_tl(CCDER_GENERALIZED_TIME, der_end, der,
                                                                  der_encode_generalizedtime_body(at, error, der, der_end)),
                                      error);
}


uint8_t* der_encode_date(CFDateRef date, CFErrorRef *error,
                         const uint8_t *der, uint8_t *der_end)
{
    return der_encode_generalizedtime(CFDateGetAbsoluteTime(date), error,
                                      der, der_end);
}

uint8_t* der_encode_date_repair(CFDateRef date, CFErrorRef *error,
                                bool repair, const uint8_t *der, uint8_t *der_end)
{
    return SecCCDEREncodeHandleResult(ccder_encode_constructed_tl(CCDER_GENERALIZED_TIME, der_end, der,
                                                                  der_encode_generalizedtime_body_repair(CFDateGetAbsoluteTime(date), error, repair, der, der_end)),
                                      error);
}
