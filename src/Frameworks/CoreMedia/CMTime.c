// CMTime.c - rational time values, a count of 1/timescale seconds or a flagged non-numeric time
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "CMTime.h"
#include <math.h>
#include <stdio.h>

const CMTime kCMTimeInvalid = { 0, 0, 0, 0 };
const CMTime kCMTimeIndefinite = { 0, 0, kCMTimeFlags_Valid | kCMTimeFlags_Indefinite, 0 };
const CMTime kCMTimePositiveInfinity = { 0, 0, kCMTimeFlags_Valid | kCMTimeFlags_PositiveInfinity, 0 };
const CMTime kCMTimeNegativeInfinity = { 0, 0, kCMTimeFlags_Valid | kCMTimeFlags_NegativeInfinity, 0 };
const CMTime kCMTimeZero = { 0, 1, kCMTimeFlags_Valid, 0 };

CMTime CMTimeMakeWithEpoch(int64_t value, int32_t timescale, int64_t epoch)
{
    if (timescale <= 0)
        return kCMTimeInvalid;
    return (CMTime){ value, timescale, kCMTimeFlags_Valid, epoch };
}

CMTime CMTimeMake(int64_t value, int32_t timescale)
{
    return CMTimeMakeWithEpoch(value, timescale, 0);
}

CMTime CMTimeMakeWithSeconds(double seconds, int32_t preferredTimescale)
{
    if (isnan(seconds))
        return kCMTimeInvalid;
    if (isinf(seconds))
        return seconds > 0 ? kCMTimePositiveInfinity : kCMTimeNegativeInfinity;
    if (preferredTimescale <= 0)
        return kCMTimeInvalid;

    double units = seconds * preferredTimescale;
    CMTime time = CMTimeMake((int64_t)llround(units), preferredTimescale);
    if ((double)time.value != units)
        time.flags |= kCMTimeFlags_HasBeenRounded;
    return time;
}

double CMTimeGetSeconds(CMTime time)
{
    if (CMTIME_IS_INVALID(time) || CMTIME_IS_INDEFINITE(time))
        return NAN;
    if (CMTIME_IS_POSITIVE_INFINITY(time))
        return INFINITY;
    if (CMTIME_IS_NEGATIVE_INFINITY(time))
        return -INFINITY;
    return (double)time.value / (double)time.timescale;
}

static int64_t roundQuotient(__int128 numerator, __int128 denominator, CMTimeRoundingMethod method, Boolean *rounded)
{
    __int128 quotient = numerator / denominator, remainder = numerator % denominator;

    *rounded = remainder != 0;
    if (remainder == 0)
        return (int64_t)quotient;

    Boolean negative = (numerator < 0) != (denominator < 0);
    __int128 twice = remainder < 0 ? -2 * remainder : 2 * remainder;
    __int128 magnitude = denominator < 0 ? -denominator : denominator;
    switch (method) {
    case kCMTimeRoundingMethod_RoundTowardZero:
        return (int64_t)quotient;
    case kCMTimeRoundingMethod_RoundAwayFromZero:
        return (int64_t)(negative ? quotient - 1 : quotient + 1);
    case kCMTimeRoundingMethod_RoundTowardPositiveInfinity:
        return (int64_t)(negative ? quotient : quotient + 1);
    case kCMTimeRoundingMethod_RoundTowardNegativeInfinity:
        return (int64_t)(negative ? quotient - 1 : quotient);
    default:
        if (twice >= magnitude)
            return (int64_t)(negative ? quotient - 1 : quotient + 1);
        return (int64_t)quotient;
    }
}

CMTime CMTimeConvertScale(CMTime time, int32_t newTimescale, CMTimeRoundingMethod method)
{
    if (!CMTIME_IS_NUMERIC(time) || newTimescale <= 0)
        return newTimescale <= 0 ? kCMTimeInvalid : time;
    if (time.timescale == newTimescale)
        return time;

    Boolean rounded;
    CMTime result = time;
    result.value = roundQuotient((__int128)time.value * newTimescale, time.timescale, method, &rounded);
    result.timescale = newTimescale;
    if (rounded)
        result.flags |= kCMTimeFlags_HasBeenRounded;
    return result;
}

static int64_t gcd64(int64_t a, int64_t b)
{
    while (b != 0) {
        int64_t t = a % b;
        a = b;
        b = t;
    }
    return a < 0 ? -a : a;
}

// sums go in the smallest timescale both operands divide into, if it fits, else the larger one
static int32_t commonTimescale(int32_t a, int32_t b)
{
    int64_t lcm = (int64_t)a / gcd64(a, b) * b;

    return lcm <= kCMTimeMaxTimescale ? (int32_t)lcm : (a > b ? a : b);
}

static CMTime combine(CMTime lhs, CMTime rhs, int sign)
{
    if (CMTIME_IS_INVALID(lhs) || CMTIME_IS_INVALID(rhs))
        return kCMTimeInvalid;
    if (CMTIME_IS_INDEFINITE(lhs) || CMTIME_IS_INDEFINITE(rhs))
        return kCMTimeIndefinite;

    Boolean lpos = CMTIME_IS_POSITIVE_INFINITY(lhs), lneg = CMTIME_IS_NEGATIVE_INFINITY(lhs);
    Boolean rpos = CMTIME_IS_POSITIVE_INFINITY(rhs), rneg = CMTIME_IS_NEGATIVE_INFINITY(rhs);
    if (sign < 0) {
        Boolean t = rpos;
        rpos = rneg;
        rneg = t;
    }
    if ((lpos && rneg) || (lneg && rpos))
        return kCMTimeInvalid;
    if (lpos || rpos)
        return kCMTimePositiveInfinity;
    if (lneg || rneg)
        return kCMTimeNegativeInfinity;

    int32_t scale = commonTimescale(lhs.timescale, rhs.timescale);
    CMTime a = CMTimeConvertScale(lhs, scale, kCMTimeRoundingMethod_Default);
    CMTime b = CMTimeConvertScale(rhs, scale, kCMTimeRoundingMethod_Default);
    __int128 value = (__int128)a.value + sign * (__int128)b.value;
    if (value > INT64_MAX)
        return kCMTimePositiveInfinity;
    if (value < INT64_MIN)
        return kCMTimeNegativeInfinity;

    CMTime result = CMTimeMakeWithEpoch((int64_t)value, scale, lhs.epoch);
    result.flags |= (a.flags | b.flags) & kCMTimeFlags_HasBeenRounded;
    return result;
}

CMTime CMTimeAdd(CMTime lhs, CMTime rhs)
{
    return combine(lhs, rhs, 1);
}

CMTime CMTimeSubtract(CMTime lhs, CMTime rhs)
{
    return combine(lhs, rhs, -1);
}

CMTime CMTimeMultiply(CMTime time, int32_t multiplier)
{
    if (!CMTIME_IS_NUMERIC(time)) {
        if (CMTIME_IS_POSITIVE_INFINITY(time) || CMTIME_IS_NEGATIVE_INFINITY(time)) {
            if (multiplier == 0)
                return kCMTimeInvalid;
            if (multiplier < 0)
                return CMTIME_IS_POSITIVE_INFINITY(time) ? kCMTimeNegativeInfinity : kCMTimePositiveInfinity;
        }
        return time;
    }
    __int128 value = (__int128)time.value * multiplier;
    if (value > INT64_MAX)
        return kCMTimePositiveInfinity;
    if (value < INT64_MIN)
        return kCMTimeNegativeInfinity;
    time.value = (int64_t)value;
    return time;
}

CMTime CMTimeMultiplyByFloat64(CMTime time, double multiplier)
{
    if (!CMTIME_IS_NUMERIC(time))
        return CMTimeMultiply(time, multiplier > 0 ? 1 : (multiplier < 0 ? -1 : 0));
    double value = (double)time.value * multiplier;
    if (value >= 9.2233720368547758e18)
        return kCMTimePositiveInfinity;
    if (value <= -9.2233720368547758e18)
        return kCMTimeNegativeInfinity;
    time.value = (int64_t)llround(value);
    if ((double)time.value != value)
        time.flags |= kCMTimeFlags_HasBeenRounded;
    return time;
}

// invalid sorts below everything, then negative infinity, numbers, positive infinity, indefinite
static int rank(CMTime time)
{
    if (CMTIME_IS_INVALID(time))
        return 0;
    if (CMTIME_IS_NEGATIVE_INFINITY(time))
        return 1;
    if (CMTIME_IS_POSITIVE_INFINITY(time))
        return 3;
    if (CMTIME_IS_INDEFINITE(time))
        return 4;
    return 2;
}

int32_t CMTimeCompare(CMTime time1, CMTime time2)
{
    int r1 = rank(time1), r2 = rank(time2);

    if (r1 != r2)
        return r1 < r2 ? -1 : 1;
    if (r1 != 2)
        return 0;
    if (time1.epoch != time2.epoch)
        return time1.epoch < time2.epoch ? -1 : 1;

    __int128 a = (__int128)time1.value * time2.timescale, b = (__int128)time2.value * time1.timescale;
    return a < b ? -1 : (a > b ? 1 : 0);
}

CMTime CMTimeMinimum(CMTime time1, CMTime time2)
{
    if (CMTIME_IS_INVALID(time1) || CMTIME_IS_INVALID(time2))
        return kCMTimeInvalid;
    return CMTimeCompare(time1, time2) <= 0 ? time1 : time2;
}

CMTime CMTimeMaximum(CMTime time1, CMTime time2)
{
    if (CMTIME_IS_INVALID(time1) || CMTIME_IS_INVALID(time2))
        return kCMTimeInvalid;
    return CMTimeCompare(time1, time2) >= 0 ? time1 : time2;
}

CMTime CMTimeAbsoluteValue(CMTime time)
{
    if (CMTIME_IS_NEGATIVE_INFINITY(time))
        return kCMTimePositiveInfinity;
    if (CMTIME_IS_NUMERIC(time) && time.value < 0)
        time.value = -time.value;
    return time;
}

void CMTimeShow(CMTime time)
{
    if (CMTIME_IS_INVALID(time))
        fprintf(stderr, "{INVALID}\n");
    else if (CMTIME_IS_INDEFINITE(time))
        fprintf(stderr, "{INDEFINITE}\n");
    else if (CMTIME_IS_POSITIVE_INFINITY(time))
        fprintf(stderr, "{+INFINITY}\n");
    else if (CMTIME_IS_NEGATIVE_INFINITY(time))
        fprintf(stderr, "{-INFINITY}\n");
    else
        fprintf(stderr, "{%lld/%d = %.3f}\n", (long long)time.value, time.timescale, CMTimeGetSeconds(time));
}
