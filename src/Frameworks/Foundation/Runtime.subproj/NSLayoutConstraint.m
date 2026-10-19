// NSLayoutConstraint.m - Auto Layout constraints, kept as data
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// a new constant asks the holding view to lay out again through _pdLayoutConstraints, which AppKit adds
#import <Foundation/NSLayoutConstraint.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#include <math.h>

@interface NSObject (NSLayoutConstraintOwner)
- (void)_pdLayoutConstraints;
@end

@implementation NSLayoutConstraint

@synthesize firstItem = _firstItem, secondItem = _secondItem, firstAttribute = _firstAttribute;
@synthesize secondAttribute = _secondAttribute, relation = _relation, multiplier = _multiplier;
@synthesize priority = _priority, active = _active, shouldBeArchived = _shouldBeArchived;

- (CGFloat)constant {
    return _constant;
}

+ (instancetype)constraintWithItem:(id)view1 attribute:(NSLayoutAttribute)attr1 relatedBy:(NSLayoutRelation)relation
                            toItem:(id)view2 attribute:(NSLayoutAttribute)attr2 multiplier:(CGFloat)multiplier
                          constant:(CGFloat)c {
    NSLayoutConstraint *result = [[[self alloc] init] autorelease];

    result->_firstItem = view1;
    result->_firstAttribute = attr1;
    result->_relation = relation;
    result->_secondItem = view2;
    result->_secondAttribute = attr2;
    result->_multiplier = multiplier;
    result->_constant = c;
    return result;
}

+ (void)activateConstraints:(NSArray *)constraints {
    for (NSLayoutConstraint *constraint in constraints)
        [constraint setActive:YES];
}

+ (void)deactivateConstraints:(NSArray *)constraints {
    for (NSLayoutConstraint *constraint in constraints)
        [constraint setActive:NO];
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _multiplier = 1;
        _priority = 1000;
        _shouldBeArchived = YES;
    }
    return self;
}

// the keys ibtool archives, newer files carry the V2 forms
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self init]) == nil)
        return nil;
    _firstItem = [coder decodeObjectForKey:@"NSFirstItem"];
    _secondItem = [coder decodeObjectForKey:@"NSSecondItem"];
    _firstAttribute = [coder containsValueForKey:@"NSFirstAttributeV2"] ? [coder decodeIntegerForKey:@"NSFirstAttributeV2"]
                                                                        : [coder decodeIntegerForKey:@"NSFirstAttribute"];
    _secondAttribute = [coder containsValueForKey:@"NSSecondAttributeV2"] ? [coder decodeIntegerForKey:@"NSSecondAttributeV2"]
                                                                          : [coder decodeIntegerForKey:@"NSSecondAttribute"];
    _relation = [coder decodeIntegerForKey:@"NSRelation"];
    if ([coder containsValueForKey:@"NSMultiplier"])
        _multiplier = [coder decodeDoubleForKey:@"NSMultiplier"];
    _constant = [coder containsValueForKey:@"NSConstantV2"] ? [coder decodeDoubleForKey:@"NSConstantV2"]
                                                            : [coder decodeDoubleForKey:@"NSConstant"];
    if ([coder containsValueForKey:@"NSPriority"])
        _priority = [coder decodeIntegerForKey:@"NSPriority"];
    _identifier = [[coder decodeObjectForKey:@"NSLayoutIdentifier"] copy];
    _active = YES;
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (void)dealloc {
    [_identifier release];
    [super dealloc];
}

- (NSString *)identifier {
    return _identifier;
}

// a new constant lays out the owner's subviews at once, there is no deferred layout pass
- (void)setConstant:(CGFloat)constant {
    if (constant == _constant)
        return;
    _constant = constant;
    [_owner _pdLayoutConstraints];
}

- (void)_setOwner:(id)owner {
    _owner = owner;
}

- (void)setIdentifier:(NSString *)identifier {
    NSString *old = _identifier;

    _identifier = [identifier copy];
    [old release];
}

// changes take effect at once, there is nothing to animate
- (id)animator {
    return self;
}

@end
