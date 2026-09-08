/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSByteOrder_h
#define NSByteOrder_h

#import <Foundation/NSObjCRuntime.h>
#include <stdint.h>

typedef enum {
    NS_UnknownByteOrder,
    NS_LittleEndian,
    NS_BigEndian,
} NSByteOrder;

/* A float/double carried as its bit pattern, so a swapped value cannot be
 * accidentally used as a number. */
typedef struct { uint32_t v; } NSSwappedFloat;
typedef struct { uint64_t v; } NSSwappedDouble;

NS_INLINE long NSHostByteOrder(void) {
    union { uint32_t word; uint8_t bytes[4]; } probe = { 0x01020304 };

    return (probe.bytes[0] == 0x04) ? NS_LittleEndian : NS_BigEndian;
}

NS_INLINE uint16_t NSSwapShort(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

NS_INLINE uint32_t NSSwapInt(uint32_t value) {
    return __builtin_bswap32(value);
}

NS_INLINE uint64_t NSSwapLongLong(uint64_t value) {
    return __builtin_bswap64(value);
}

NS_INLINE NSSwappedFloat NSConvertHostFloatToSwapped(float value) {
    union { float f; uint32_t i; } convert = { value };
    NSSwappedFloat result = { convert.i };

    return result;
}

NS_INLINE float NSConvertSwappedFloatToHost(NSSwappedFloat value) {
    union { uint32_t i; float f; } convert = { value.v };

    return convert.f;
}

NS_INLINE NSSwappedDouble NSConvertHostDoubleToSwapped(double value) {
    union { double d; uint64_t i; } convert = { value };
    NSSwappedDouble result = { convert.i };

    return result;
}

NS_INLINE double NSConvertSwappedDoubleToHost(NSSwappedDouble value) {
    union { uint64_t i; double d; } convert = { value.v };

    return convert.d;
}

NS_INLINE NSSwappedFloat NSSwapFloat(NSSwappedFloat value) {
    NSSwappedFloat result = { NSSwapInt(value.v) };

    return result;
}

NS_INLINE NSSwappedDouble NSSwapDouble(NSSwappedDouble value) {
    NSSwappedDouble result = { NSSwapLongLong(value.v) };

    return result;
}

#if defined(__BIG_ENDIAN__)
 #define NS_HOST_IS_BIG_ENDIAN 1
#else
 #define NS_HOST_IS_BIG_ENDIAN 0
#endif

NS_INLINE float NSSwapBigFloatToHost(NSSwappedFloat value) {
    return NS_HOST_IS_BIG_ENDIAN ? NSConvertSwappedFloatToHost(value)
                                 : NSConvertSwappedFloatToHost(NSSwapFloat(value));
}

NS_INLINE double NSSwapBigDoubleToHost(NSSwappedDouble value) {
    return NS_HOST_IS_BIG_ENDIAN ? NSConvertSwappedDoubleToHost(value)
                                 : NSConvertSwappedDoubleToHost(NSSwapDouble(value));
}

NS_INLINE float NSSwapLittleFloatToHost(NSSwappedFloat value) {
    return NS_HOST_IS_BIG_ENDIAN ? NSConvertSwappedFloatToHost(NSSwapFloat(value))
                                 : NSConvertSwappedFloatToHost(value);
}

NS_INLINE double NSSwapLittleDoubleToHost(NSSwappedDouble value) {
    return NS_HOST_IS_BIG_ENDIAN ? NSConvertSwappedDoubleToHost(NSSwapDouble(value))
                                 : NSConvertSwappedDoubleToHost(value);
}

NS_INLINE NSSwappedFloat NSSwapHostFloatToBig(float value) {
    NSSwappedFloat swapped = NSConvertHostFloatToSwapped(value);

    return NS_HOST_IS_BIG_ENDIAN ? swapped : NSSwapFloat(swapped);
}

NS_INLINE NSSwappedDouble NSSwapHostDoubleToBig(double value) {
    NSSwappedDouble swapped = NSConvertHostDoubleToSwapped(value);

    return NS_HOST_IS_BIG_ENDIAN ? swapped : NSSwapDouble(swapped);
}

NS_INLINE uint16_t NSSwapBigShortToHost(uint16_t value) {
    return NS_HOST_IS_BIG_ENDIAN ? value : NSSwapShort(value);
}

NS_INLINE uint32_t NSSwapBigIntToHost(uint32_t value) {
    return NS_HOST_IS_BIG_ENDIAN ? value : NSSwapInt(value);
}

NS_INLINE uint16_t NSSwapHostShortToBig(uint16_t value) {
    return NSSwapBigShortToHost(value);
}

NS_INLINE uint32_t NSSwapHostIntToBig(uint32_t value) {
    return NSSwapBigIntToHost(value);
}

#endif /* NSByteOrder_h */
