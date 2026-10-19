// NSLayoutConstraint.h - Auto Layout constraints, kept as data
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// Foundation owns the class as CoreAutoLayout does on macOS, AppKit lays views out with it
#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>

@class NSArray, NSString;

#ifndef NSLAYOUTPRIORITY_DEFINED
#define NSLAYOUTPRIORITY_DEFINED 1
typedef float NSLayoutPriority;
#endif

typedef NS_ENUM(NSInteger, NSLayoutRelation) {
    NSLayoutRelationLessThanOrEqual = -1,
    NSLayoutRelationEqual = 0,
    NSLayoutRelationGreaterThanOrEqual = 1,
};

typedef NS_ENUM(NSInteger, NSLayoutAttribute) {
    NSLayoutAttributeLeft = 1,
    NSLayoutAttributeRight,
    NSLayoutAttributeTop,
    NSLayoutAttributeBottom,
    NSLayoutAttributeLeading,
    NSLayoutAttributeTrailing,
    NSLayoutAttributeWidth,
    NSLayoutAttributeHeight,
    NSLayoutAttributeCenterX,
    NSLayoutAttributeCenterY,
    NSLayoutAttributeLastBaseline,
    NSLayoutAttributeFirstBaseline,
    NSLayoutAttributeNotAnAttribute = 0,
};

// there is no constraint solver here: views keep their archived frames, constraints only hold their values
@interface NSLayoutConstraint : NSObject <NSCoding> {
    id _firstItem;
    id _secondItem;
    NSLayoutAttribute _firstAttribute;
    NSLayoutAttribute _secondAttribute;
    NSLayoutRelation _relation;
    CGFloat _multiplier;
    CGFloat _constant;
    NSLayoutPriority _priority;
    BOOL _active;
    BOOL _shouldBeArchived;
    NSString *_identifier;
    // the view the constraint was added to, which lays its items out again when the constant changes
    id _owner;
}

+ (instancetype)constraintWithItem:(id)view1 attribute:(NSLayoutAttribute)attr1 relatedBy:(NSLayoutRelation)relation
                            toItem:(id)view2 attribute:(NSLayoutAttribute)attr2 multiplier:(CGFloat)multiplier
                          constant:(CGFloat)c;
+ (void)activateConstraints:(NSArray *)constraints;
+ (void)deactivateConstraints:(NSArray *)constraints;

@property (readonly, assign) id firstItem;
@property (readonly, assign) id secondItem;
@property (readonly) NSLayoutAttribute firstAttribute;
@property (readonly) NSLayoutAttribute secondAttribute;
@property (readonly) NSLayoutRelation relation;
@property (readonly) CGFloat multiplier;
@property CGFloat constant;
@property NSLayoutPriority priority;
@property (getter=isActive) BOOL active;
@property BOOL shouldBeArchived;
@property (copy) NSString *identifier;

- (id)animator;

@end
