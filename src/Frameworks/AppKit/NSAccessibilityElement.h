// NSAccessibilityElement.h - an accessibility element that is not a view, for UI an app draws itself
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/NSAccessibility.h>

@class NSArray, NSMutableArray, NSString;

@interface NSAccessibilityElement : NSObject {
    NSString *_accessibilityRole;
    NSString *_accessibilityLabel;
    NSRect _accessibilityFrame;
    NSRect _accessibilityFrameInParentSpace;
    id _accessibilityParent;
    NSMutableArray *_accessibilityChildren;
}

+ (id)accessibilityElementWithRole:(NSString *)role frame:(NSRect)frame label:(NSString *)label parent:(id)parent;

- (void)accessibilityAddChildElement:(NSAccessibilityElement *)childElement;

@property NSRect accessibilityFrameInParentSpace;
@property(copy) NSString *accessibilityRole;
@property(copy) NSString *accessibilityLabel;
@property NSRect accessibilityFrame;
@property(assign) id accessibilityParent;
@property(copy) NSArray *accessibilityChildren;

@end
