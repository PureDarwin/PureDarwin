// NSByteCountFormatter.h - byte counts as text in the units an app allows
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

#import <Foundation/NSFormatter.h>

typedef NS_OPTIONS(NSUInteger, NSByteCountFormatterUnits) {
    NSByteCountFormatterUseDefault = 0,
    NSByteCountFormatterUseBytes = 1UL << 0,
    NSByteCountFormatterUseKB = 1UL << 1,
    NSByteCountFormatterUseMB = 1UL << 2,
    NSByteCountFormatterUseGB = 1UL << 3,
    NSByteCountFormatterUseTB = 1UL << 4,
    NSByteCountFormatterUsePB = 1UL << 5,
    NSByteCountFormatterUseEB = 1UL << 6,
    NSByteCountFormatterUseZB = 1UL << 7,
    NSByteCountFormatterUseYBOrHigher = 0x0FFUL << 8,
    NSByteCountFormatterUseAll = 0x0FFFFUL,
};

typedef NS_ENUM(NSInteger, NSByteCountFormatterCountStyle) {
    NSByteCountFormatterCountStyleFile = 0,
    NSByteCountFormatterCountStyleMemory = 1,
    NSByteCountFormatterCountStyleDecimal = 2,
    NSByteCountFormatterCountStyleBinary = 3,
};

// sizes as people read them, "1.5 MB", decimal for files and binary for memory
@interface NSByteCountFormatter : NSFormatter {
    NSByteCountFormatterUnits _allowedUnits;
    NSByteCountFormatterCountStyle _countStyle;
    BOOL _allowsNonnumericFormatting;
    BOOL _includesUnit;
    BOOL _includesCount;
    BOOL _includesActualByteCount;
    BOOL _adaptive;
    BOOL _zeroPadsFractionDigits;
}

+ (NSString *)stringFromByteCount:(long long)byteCount countStyle:(NSByteCountFormatterCountStyle)countStyle;
- (NSString *)stringFromByteCount:(long long)byteCount;

@property NSByteCountFormatterUnits allowedUnits;
@property NSByteCountFormatterCountStyle countStyle;
@property BOOL allowsNonnumericFormatting;
@property BOOL includesUnit;
@property BOOL includesCount;
@property BOOL includesActualByteCount;
@property (getter=isAdaptive) BOOL adaptive;
@property BOOL zeroPadsFractionDigits;

@end
