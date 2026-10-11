// CMTime.h - rational time values
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef CMTIME_H
#define CMTIME_H

#include <stdint.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t CMTimeValue;
typedef int32_t CMTimeScale;
typedef int64_t CMTimeEpoch;

typedef uint32_t CMTimeFlags;
enum {
    kCMTimeFlags_Valid = 1UL << 0,
    kCMTimeFlags_HasBeenRounded = 1UL << 1,
    kCMTimeFlags_PositiveInfinity = 1UL << 2,
    kCMTimeFlags_NegativeInfinity = 1UL << 3,
    kCMTimeFlags_Indefinite = 1UL << 4,
    kCMTimeFlags_ImpliedValueFlagsMask = kCMTimeFlags_PositiveInfinity | kCMTimeFlags_NegativeInfinity | kCMTimeFlags_Indefinite
};

typedef uint32_t CMTimeRoundingMethod;
enum {
    kCMTimeRoundingMethod_RoundHalfAwayFromZero = 1,
    kCMTimeRoundingMethod_RoundTowardZero = 2,
    kCMTimeRoundingMethod_RoundAwayFromZero = 3,
    kCMTimeRoundingMethod_QuickTime = 4,
    kCMTimeRoundingMethod_RoundTowardPositiveInfinity = 5,
    kCMTimeRoundingMethod_RoundTowardNegativeInfinity = 6,
    kCMTimeRoundingMethod_Default = kCMTimeRoundingMethod_RoundHalfAwayFromZero
};

#define kCMTimeMaxTimescale 0x7fffffffL

#pragma pack(push, 4)
typedef struct {
    CMTimeValue value;
    CMTimeScale timescale;
    CMTimeFlags flags;
    CMTimeEpoch epoch;
} CMTime;
#pragma pack(pop)

extern const CMTime kCMTimeInvalid;
extern const CMTime kCMTimeIndefinite;
extern const CMTime kCMTimePositiveInfinity;
extern const CMTime kCMTimeNegativeInfinity;
extern const CMTime kCMTimeZero;

#define CMTIME_IS_VALID(time) ((Boolean)(((time).flags & kCMTimeFlags_Valid) != 0))
#define CMTIME_IS_INVALID(time) (!CMTIME_IS_VALID(time))
#define CMTIME_IS_POSITIVE_INFINITY(time) ((Boolean)(CMTIME_IS_VALID(time) && (((time).flags & kCMTimeFlags_PositiveInfinity) != 0)))
#define CMTIME_IS_NEGATIVE_INFINITY(time) ((Boolean)(CMTIME_IS_VALID(time) && (((time).flags & kCMTimeFlags_NegativeInfinity) != 0)))
#define CMTIME_IS_INDEFINITE(time) ((Boolean)(CMTIME_IS_VALID(time) && (((time).flags & kCMTimeFlags_Indefinite) != 0)))
#define CMTIME_IS_NUMERIC(time) ((Boolean)(((time).flags & (kCMTimeFlags_Valid | kCMTimeFlags_ImpliedValueFlagsMask)) == kCMTimeFlags_Valid))

CMTime CMTimeMake(int64_t value, int32_t timescale);
CMTime CMTimeMakeWithEpoch(int64_t value, int32_t timescale, int64_t epoch);
CMTime CMTimeMakeWithSeconds(double seconds, int32_t preferredTimescale);
double CMTimeGetSeconds(CMTime time);
CMTime CMTimeConvertScale(CMTime time, int32_t newTimescale, CMTimeRoundingMethod method);
CMTime CMTimeAdd(CMTime lhs, CMTime rhs);
CMTime CMTimeSubtract(CMTime lhs, CMTime rhs);
CMTime CMTimeMultiply(CMTime time, int32_t multiplier);
CMTime CMTimeMultiplyByFloat64(CMTime time, double multiplier);
int32_t CMTimeCompare(CMTime time1, CMTime time2);
CMTime CMTimeMinimum(CMTime time1, CMTime time2);
CMTime CMTimeMaximum(CMTime time1, CMTime time2);
CMTime CMTimeAbsoluteValue(CMTime time);
void CMTimeShow(CMTime time);

#ifdef __cplusplus
}
#endif

#endif
