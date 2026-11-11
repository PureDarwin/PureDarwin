// NSStackView.h - a view that lines its arranged subviews up in a row or a column
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSView.h>
#import <AppKit/NSModernAppKit.h>
#import <AppKit/NSLayoutConstraint.h>

#ifndef NSUserInterfaceLayoutOrientationDefined
#define NSUserInterfaceLayoutOrientationDefined 1
typedef NS_ENUM(NSInteger, NSUserInterfaceLayoutOrientation) {
    NSUserInterfaceLayoutOrientationHorizontal = 0,
    NSUserInterfaceLayoutOrientationVertical = 1,
};
#endif

typedef NS_ENUM(NSInteger, NSStackViewGravity) {
    NSStackViewGravityTop = 1,
    NSStackViewGravityLeading = 1,
    NSStackViewGravityCenter = 2,
    NSStackViewGravityBottom = 3,
    NSStackViewGravityTrailing = 3,
};

typedef NS_ENUM(NSInteger, NSStackViewDistribution) {
    NSStackViewDistributionGravityAreas = -1,
    NSStackViewDistributionFill = 0,
    NSStackViewDistributionFillEqually,
    NSStackViewDistributionFillProportionally,
    NSStackViewDistributionEqualSpacing,
    NSStackViewDistributionEqualCentering,
};

static const CGFloat NSStackViewSpacingUseDefault = -3.4028234663852886e38;

@interface NSStackView : NSView {
    NSMutableArray *_arrangedSubviews;
    NSMutableDictionary *_gravities;
    NSMutableDictionary *_customSpacing;
    NSMutableDictionary *_naturalSizes;
    NSUserInterfaceLayoutOrientation _orientation;
    NSLayoutAttribute _alignment;
    NSStackViewDistribution _distribution;
    NSEdgeInsets _edgeInsets;
    CGFloat _spacing;
    BOOL _detachesHiddenViews;
    BOOL _arranging;
}

+ (instancetype)stackViewWithViews:(NSArray *)views;

- (NSArray *)arrangedSubviews;
- (void)addArrangedSubview:(NSView *)view;
- (void)insertArrangedSubview:(NSView *)view atIndex:(NSInteger)index;
- (void)removeArrangedSubview:(NSView *)view;

- (NSArray *)views;
- (void)setViews:(NSArray *)views inGravity:(NSStackViewGravity)gravity;
- (void)addView:(NSView *)view inGravity:(NSStackViewGravity)gravity;
- (void)insertView:(NSView *)view atIndex:(NSUInteger)index inGravity:(NSStackViewGravity)gravity;
- (void)removeView:(NSView *)view;
- (NSArray *)viewsInGravity:(NSStackViewGravity)gravity;

- (NSUserInterfaceLayoutOrientation)orientation;
- (void)setOrientation:(NSUserInterfaceLayoutOrientation)orientation;
- (NSLayoutAttribute)alignment;
- (void)setAlignment:(NSLayoutAttribute)alignment;
- (NSStackViewDistribution)distribution;
- (void)setDistribution:(NSStackViewDistribution)distribution;
- (NSEdgeInsets)edgeInsets;
- (void)setEdgeInsets:(NSEdgeInsets)insets;
- (CGFloat)spacing;
- (void)setSpacing:(CGFloat)spacing;
- (CGFloat)customSpacingAfterView:(NSView *)view;
- (void)setCustomSpacing:(CGFloat)spacing afterView:(NSView *)view;
- (BOOL)detachesHiddenViews;
- (void)setDetachesHiddenViews:(BOOL)flag;
- (NSLayoutPriority)huggingPriorityForOrientation:(NSUserInterfaceLayoutOrientation)orientation;
- (void)setHuggingPriority:(NSLayoutPriority)priority forOrientation:(NSUserInterfaceLayoutOrientation)orientation;
- (NSLayoutPriority)clippingResistancePriorityForOrientation:(NSUserInterfaceLayoutOrientation)orientation;
- (void)setClippingResistancePriority:(NSLayoutPriority)priority forOrientation:(NSUserInterfaceLayoutOrientation)orientation;
- (NSSize)fittingSize;

@end
