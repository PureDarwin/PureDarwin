// NSAccessibilityCustomAction.h - an extra action an element offers to assistive technology
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>

@class NSString;

@interface NSAccessibilityCustomAction : NSObject
- (instancetype)initWithName:(NSString *)name handler:(BOOL (^)(void))handler;
- (instancetype)initWithName:(NSString *)name target:(id)target selector:(SEL)selector;
@property(copy) NSString *name;
@property(copy) BOOL (^handler)(void);
@property(assign) id target;
@property SEL selector;
@end
