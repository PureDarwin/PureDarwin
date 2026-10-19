// NSAccessibilityCustomAction.m - an extra action an element offers to assistive technology
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSAccessibilityCustomAction.h>
#import <Foundation/Foundation.h>
#import <objc/message.h>

@implementation NSAccessibilityCustomAction

@synthesize name = _name, handler = _handler, target = _target, selector = _selector;

- (instancetype)initWithName:(NSString *)name handler:(BOOL (^)(void))handler
{
    if ((self = [super init]) == nil)
        return nil;
    _name = [name copy];
    _handler = [handler copy];
    return self;
}

- (instancetype)initWithName:(NSString *)name target:(id)target selector:(SEL)selector
{
    if ((self = [super init]) == nil)
        return nil;
    _name = [name copy];
    _target = target;
    _selector = selector;
    return self;
}

- (void)dealloc
{
    [_name release];
    [_handler release];
    [super dealloc];
}

// what assistive technology calls to run the action, the handler or the target's method
- (BOOL)_perform
{
    if (_handler)
        return _handler();
    if (_target && _selector && [_target respondsToSelector:_selector])
        return ((BOOL (*)(id, SEL, id))objc_msgSend)(_target, _selector, self);
    return NO;
}

@end
