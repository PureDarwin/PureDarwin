// NSImageSymbolConfiguration.m - how a symbol image is drawn: size, weight, scale and colors
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSImageSymbolConfiguration.h>
#import <AppKit/NSColor.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSString.h>

// symbol images need the SF Symbols catalog this system lacks, the configuration is still kept and combined
@implementation NSImageSymbolConfiguration {
    CGFloat _pointSize;
    NSFontWeight _weight;
    NSImageSymbolScale _scale;
    NSString *_textStyle;
    NSArray *_colors;
    BOOL _hasWeight;
}

+ (instancetype)configurationWithPointSize:(CGFloat)pointSize weight:(NSFontWeight)weight {
    return [self configurationWithPointSize:pointSize weight:weight scale:0];
}

+ (instancetype)configurationWithPointSize:(CGFloat)pointSize weight:(NSFontWeight)weight scale:(NSImageSymbolScale)scale {
    NSImageSymbolConfiguration *config = [[self new] autorelease];

    config->_pointSize = pointSize;
    config->_weight = weight;
    config->_hasWeight = YES;
    config->_scale = scale;
    return config;
}

+ (instancetype)configurationWithTextStyle:(NSString *)style {
    return [self configurationWithTextStyle:style scale:0];
}

+ (instancetype)configurationWithTextStyle:(NSString *)style scale:(NSImageSymbolScale)scale {
    NSImageSymbolConfiguration *config = [[self new] autorelease];

    config->_textStyle = [style copy];
    config->_scale = scale;
    return config;
}

+ (instancetype)configurationWithScale:(NSImageSymbolScale)scale {
    NSImageSymbolConfiguration *config = [[self new] autorelease];

    config->_scale = scale;
    return config;
}

+ (instancetype)configurationWithHierarchicalColor:(NSColor *)hierarchicalColor {
    return [self configurationWithPaletteColors:hierarchicalColor ? [NSArray arrayWithObject:hierarchicalColor] : nil];
}

+ (instancetype)configurationWithPaletteColors:(NSArray *)paletteColors {
    NSImageSymbolConfiguration *config = [[self new] autorelease];

    config->_colors = [paletteColors copy];
    return config;
}

+ (instancetype)configurationPreferringMulticolor {
    return [[self new] autorelease];
}

+ (instancetype)configurationPreferringMonochrome {
    return [[self new] autorelease];
}

+ (instancetype)configurationPreferringHierarchical {
    return [[self new] autorelease];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (void)dealloc {
    [_textStyle release];
    [_colors release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (id)initWithCoder:(NSCoder *)coder {
    return [self init];
}

// values set in the argument win over this one's
- (NSImageSymbolConfiguration *)configurationByApplyingConfiguration:(NSImageSymbolConfiguration *)other {
    NSImageSymbolConfiguration *config = [[NSImageSymbolConfiguration new] autorelease];

    config->_pointSize = other && other->_pointSize ? other->_pointSize : _pointSize;
    config->_hasWeight = (other && other->_hasWeight) || _hasWeight;
    config->_weight = other && other->_hasWeight ? other->_weight : _weight;
    config->_scale = other && other->_scale ? other->_scale : _scale;
    config->_textStyle = [(other && other->_textStyle ? other->_textStyle : _textStyle) copy];
    config->_colors = [(other && other->_colors ? other->_colors : _colors) copy];
    return config;
}

@end
