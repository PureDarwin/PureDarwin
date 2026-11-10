// NSAccessibilityElement.m - an accessibility element that is not a view, for UI an app draws itself
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSAccessibilityElement.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>

@implementation NSAccessibilityElement

+ (id)accessibilityElementWithRole:(NSString *)role frame:(NSRect)frame label:(NSString *)label parent:(id)parent {
    NSAccessibilityElement *element = [[[self alloc] init] autorelease];

    [element setAccessibilityRole:role];
    [element setAccessibilityFrame:frame];
    [element setAccessibilityLabel:label];
    [element setAccessibilityParent:parent];
    return element;
}

- (void)dealloc {
    [_accessibilityRole release];
    [_accessibilityLabel release];
    [_accessibilityChildren release];
    [super dealloc];
}

- (void)accessibilityAddChildElement:(NSAccessibilityElement *)childElement {
    if (_accessibilityChildren == nil)
        _accessibilityChildren = [[NSMutableArray alloc] init];
    [_accessibilityChildren addObject:childElement];
    [childElement setAccessibilityParent:self];
}

- (NSString *)accessibilityRole {
    return _accessibilityRole;
}

- (void)setAccessibilityRole:(NSString *)role {
    role = [role copy];
    [_accessibilityRole release];
    _accessibilityRole = role;
}

- (NSString *)accessibilityLabel {
    return _accessibilityLabel;
}

- (void)setAccessibilityLabel:(NSString *)label {
    label = [label copy];
    [_accessibilityLabel release];
    _accessibilityLabel = label;
}

- (NSRect)accessibilityFrame {
    return _accessibilityFrame;
}

- (void)setAccessibilityFrame:(NSRect)frame {
    _accessibilityFrame = frame;
}

- (NSRect)accessibilityFrameInParentSpace {
    return _accessibilityFrameInParentSpace;
}

- (void)setAccessibilityFrameInParentSpace:(NSRect)frame {
    _accessibilityFrameInParentSpace = frame;
}

- (id)accessibilityParent {
    return _accessibilityParent;
}

- (void)setAccessibilityParent:(id)parent {
    _accessibilityParent = parent;
}

- (NSArray *)accessibilityChildren {
    return _accessibilityChildren;
}

- (void)setAccessibilityChildren:(NSArray *)children {
    NSMutableArray *copy = [children mutableCopy];

    [_accessibilityChildren release];
    _accessibilityChildren = copy;
}

- (BOOL)isAccessibilityElement {
    return YES;
}

@end
