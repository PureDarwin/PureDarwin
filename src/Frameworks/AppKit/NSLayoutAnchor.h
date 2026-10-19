// NSLayoutAnchor.h - layout anchors and the visual format language, both making NSLayoutConstraints
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <AppKit/NSLayoutConstraint.h>

@class NSArray, NSDictionary, NSString;

typedef NS_OPTIONS(NSUInteger, NSLayoutFormatOptions) {
    NSLayoutFormatAlignAllLeft = (1 << NSLayoutAttributeLeft),
    NSLayoutFormatAlignAllRight = (1 << NSLayoutAttributeRight),
    NSLayoutFormatAlignAllTop = (1 << NSLayoutAttributeTop),
    NSLayoutFormatAlignAllBottom = (1 << NSLayoutAttributeBottom),
    NSLayoutFormatAlignAllLeading = (1 << NSLayoutAttributeLeading),
    NSLayoutFormatAlignAllTrailing = (1 << NSLayoutAttributeTrailing),
    NSLayoutFormatAlignAllCenterX = (1 << NSLayoutAttributeCenterX),
    NSLayoutFormatAlignAllCenterY = (1 << NSLayoutAttributeCenterY),
    NSLayoutFormatAlignAllLastBaseline = (1 << NSLayoutAttributeLastBaseline),
    NSLayoutFormatAlignAllFirstBaseline = (1 << NSLayoutAttributeFirstBaseline),
    NSLayoutFormatAlignmentMask = 0xFFFF,
    NSLayoutFormatDirectionLeadingToTrailing = 0 << 16,
    NSLayoutFormatDirectionLeftToRight = 1 << 16,
    NSLayoutFormatDirectionRightToLeft = 2 << 16,
    NSLayoutFormatDirectionMask = 0x3 << 16,
};

@interface NSLayoutAnchor : NSObject <NSCopying> {
    id _item;
    NSLayoutAttribute _attribute;
}
@property(readonly) NSString *name;
@property(readonly, assign) id item;
@property(readonly) BOOL hasAmbiguousLayout;
@property(readonly) NSArray *constraintsAffectingLayout;
- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutAnchor *)anchor;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutAnchor *)anchor;
- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutAnchor *)anchor;
- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c;
- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c;
@end

@interface NSLayoutXAxisAnchor : NSLayoutAnchor
@end

@interface NSLayoutYAxisAnchor : NSLayoutAnchor
@end

@interface NSLayoutDimension : NSLayoutAnchor
- (NSLayoutConstraint *)constraintEqualToConstant:(CGFloat)c;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToConstant:(CGFloat)c;
- (NSLayoutConstraint *)constraintLessThanOrEqualToConstant:(CGFloat)c;
- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m;
- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m;
- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c;
- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c;
@end

@interface NSView (NSLayoutAnchors)
@property(readonly, strong) NSLayoutXAxisAnchor *leadingAnchor;
@property(readonly, strong) NSLayoutXAxisAnchor *trailingAnchor;
@property(readonly, strong) NSLayoutXAxisAnchor *leftAnchor;
@property(readonly, strong) NSLayoutXAxisAnchor *rightAnchor;
@property(readonly, strong) NSLayoutYAxisAnchor *topAnchor;
@property(readonly, strong) NSLayoutYAxisAnchor *bottomAnchor;
@property(readonly, strong) NSLayoutDimension *widthAnchor;
@property(readonly, strong) NSLayoutDimension *heightAnchor;
@property(readonly, strong) NSLayoutXAxisAnchor *centerXAnchor;
@property(readonly, strong) NSLayoutYAxisAnchor *centerYAnchor;
@property(readonly, strong) NSLayoutYAxisAnchor *firstBaselineAnchor;
@property(readonly, strong) NSLayoutYAxisAnchor *lastBaselineAnchor;
@end

@interface NSLayoutConstraint (NSVisualFormat)
+ (NSArray *)constraintsWithVisualFormat:(NSString *)format options:(NSLayoutFormatOptions)opts
                                 metrics:(NSDictionary *)metrics views:(NSDictionary *)views;
@end
