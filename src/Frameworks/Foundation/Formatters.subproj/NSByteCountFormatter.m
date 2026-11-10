// NSByteCountFormatter.m - byte counts as text in the units an app allows
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

#import <Foundation/NSByteCountFormatter.h>
#import <Foundation/NSString.h>
#import <Foundation/NSValue.h>

static NSString *const unitNames[] = { @"bytes", @"KB", @"MB", @"GB", @"TB", @"PB", @"EB", @"ZB", @"YB" };

@implementation NSByteCountFormatter

@synthesize allowedUnits = _allowedUnits;
@synthesize countStyle = _countStyle;
@synthesize allowsNonnumericFormatting = _allowsNonnumericFormatting;
@synthesize includesUnit = _includesUnit;
@synthesize includesCount = _includesCount;
@synthesize includesActualByteCount = _includesActualByteCount;
@synthesize adaptive = _adaptive;
@synthesize zeroPadsFractionDigits = _zeroPadsFractionDigits;

+ (NSString *)stringFromByteCount:(long long)byteCount countStyle:(NSByteCountFormatterCountStyle)countStyle {
    NSByteCountFormatter *formatter = [[[self alloc] init] autorelease];

    formatter.countStyle = countStyle;
    return [formatter stringFromByteCount:byteCount];
}

- (instancetype)init {
    if ((self = [super init])) {
        _allowsNonnumericFormatting = YES;
        _includesUnit = YES;
        _includesCount = YES;
        _adaptive = YES;
    }
    return self;
}

- (NSString *)stringFromByteCount:(long long)byteCount {
    double base = (_countStyle == NSByteCountFormatterCountStyleMemory || _countStyle == NSByteCountFormatterCountStyleBinary) ? 1024 : 1000;
    NSByteCountFormatterUnits allowed = _allowedUnits ? _allowedUnits : NSByteCountFormatterUseAll;
    double value = (double)(byteCount < 0 ? -byteCount : byteCount);
    int unit = 0, lowest = 0, highest = 0, digits;
    NSString *count, *name, *result;

    lowest = -1;
    for (int i = 0; i < 9; i++) {
        BOOL ok = i < 8 ? (allowed & (1UL << i)) != 0 : (allowed & NSByteCountFormatterUseYBOrHigher) != 0;

        if (!ok)
            continue;
        if (lowest < 0)
            lowest = i;
        highest = i;
    }
    if (lowest < 0)
        lowest = 0;
    for (unit = 0; unit < lowest; unit++)
        value /= base;
    // step up while the number is a full unit of the next size, and that unit is allowed
    while (unit < highest && value >= base) {
        value /= base;
        unit++;
    }

    if (byteCount == 0 && _allowsNonnumericFormatting && _includesCount && _includesUnit)
        return @"Zero KB";

    // bytes and KB print whole, MB one fraction digit and larger units two, as macOS's adaptive mode does
    digits = !_adaptive ? 1 : unit <= 1 ? 0 : unit == 2 ? 1 : 2;
    count = [NSString stringWithFormat:@"%.*f", digits, byteCount < 0 ? -value : value];
    if (!_zeroPadsFractionDigits && digits > 0 && [count rangeOfString:@"."].location != NSNotFound) {
        while ([count hasSuffix:@"0"])
            count = [count substringToIndex:[count length] - 1];
        if ([count hasSuffix:@"."])
            count = [count substringToIndex:[count length] - 1];
    }
    name = unitNames[unit];
    if (unit == 0 && value == 1)
        name = @"byte";

    if (_includesCount && _includesUnit)
        result = [NSString stringWithFormat:@"%@ %@", count, name];
    else if (_includesCount)
        result = count;
    else
        result = _includesUnit ? name : @"";
    if (_includesActualByteCount && unit > 0)
        result = [result stringByAppendingFormat:@" (%lld bytes)", byteCount];
    return result;
}

- (NSString *)stringForObjectValue:(id)object {
    if (![object respondsToSelector:@selector(longLongValue)])
        return nil;
    return [self stringFromByteCount:[object longLongValue]];
}

@end
