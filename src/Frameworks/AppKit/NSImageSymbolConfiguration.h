// NSImageSymbolConfiguration.h - how a symbol image is drawn: size, weight, scale and colors
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSFontDescriptor.h>

@class NSArray, NSColor, NSString;

typedef NSInteger NSImageSymbolScale;
enum {
    NSImageSymbolScaleSmall = 1,
    NSImageSymbolScaleMedium = 2,
    NSImageSymbolScaleLarge = 3,
};

// how a symbol image is drawn: size, weight, scale and colors
@interface NSImageSymbolConfiguration : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)configurationWithPointSize:(CGFloat)pointSize weight:(NSFontWeight)weight;
+ (instancetype)configurationWithPointSize:(CGFloat)pointSize weight:(NSFontWeight)weight scale:(NSImageSymbolScale)scale;
+ (instancetype)configurationWithTextStyle:(NSString *)style;
+ (instancetype)configurationWithTextStyle:(NSString *)style scale:(NSImageSymbolScale)scale;
+ (instancetype)configurationWithScale:(NSImageSymbolScale)scale;
+ (instancetype)configurationWithHierarchicalColor:(NSColor *)hierarchicalColor;
+ (instancetype)configurationWithPaletteColors:(NSArray *)paletteColors;
+ (instancetype)configurationPreferringMulticolor;
+ (instancetype)configurationPreferringMonochrome;
+ (instancetype)configurationPreferringHierarchical;
- (NSImageSymbolConfiguration *)configurationByApplyingConfiguration:(NSImageSymbolConfiguration *)configuration;
@end
