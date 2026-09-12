/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSUndoManager.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSInvocation.h>
#include <objc/message.h>

NSString *const NSUndoManagerCheckpointNotification = @"NSUndoManagerCheckpointNotification";
NSString *const NSUndoManagerDidOpenUndoGroupNotification = @"NSUndoManagerDidOpenUndoGroupNotification";
NSString *const NSUndoManagerDidRedoChangeNotification = @"NSUndoManagerDidRedoChangeNotification";
NSString *const NSUndoManagerDidUndoChangeNotification = @"NSUndoManagerDidUndoChangeNotification";
NSString *const NSUndoManagerWillCloseUndoGroupNotification = @"NSUndoManagerWillCloseUndoGroupNotification";
NSString *const NSUndoManagerWillRedoChangeNotification = @"NSUndoManagerWillRedoChangeNotification";
NSString *const NSUndoManagerWillUndoChangeNotification = @"NSUndoManagerWillUndoChangeNotification";

/* One registered action. Target is held weakly, matching AppKit's contract
 * that a target is expected to outlive its registrations or call
 * -removeAllActionsWithTarget:. */
@interface NSUndoAction : NSObject {
@public
    id _target;
    SEL _selector;
    id _object;
    NSInvocation *_invocation;
}
@end

@implementation NSUndoAction

- (void)dealloc {
    [_object release];
    [_invocation release];
    [super dealloc];
}

- (void)invoke {
    /* Registered through -prepareWithInvocationTarget:, so the message shape
     * is whatever the caller sent rather than the single-object form. */
    if (_invocation != nil) {
        [_invocation invoke];
        return;
    }
    ((void (*)(id, SEL, id))objc_msgSend)(_target, _selector, _object);
}

@end

@interface NSUndoManager (PureDarwinUndoProxy)
- (void)_registerUndoInvocation:(NSInvocation *)invocation target:(id)target;
@end

/* Returned by -prepareWithInvocationTarget:. It records the next message sent
 * to it and registers that as the undo action. */
@interface NSUndoProxy : NSObject {
@public
    NSUndoManager *_manager;
    id _target;
}
@end

@implementation NSUndoProxy

- (void)forwardInvocation:(NSInvocation *)invocation {
    [invocation setTarget:_target];
    /* The arguments outlive this message, so the invocation has to own them. */
    [invocation retainArguments];
    [_manager _registerUndoInvocation:invocation target:_target];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    /* The proxy answers for its target: returning nil here makes the runtime
     * report an unrecognised selector instead of forwarding, which aborted on
     * every -prepareWithInvocationTarget: message. */
    NSMethodSignature *signature = [_target methodSignatureForSelector:selector];

    if (signature != nil) {
        return signature;
    }
    return [super methodSignatureForSelector:selector];
}

- (BOOL)respondsToSelector:(SEL)selector {
    return [_target respondsToSelector:selector];
}

@end

@implementation NSUndoManager

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _undoStack = [[NSMutableArray alloc] init];
    _redoStack = [[NSMutableArray alloc] init];
    _openGroups = [[NSMutableArray alloc] init];
    _groupsByEvent = YES;
    return self;
}

- (void)dealloc {
    [_undoStack release];
    [_redoStack release];
    [_openGroups release];
    [_runLoopModes release];
    [_actionName release];
    [super dealloc];
}

- (void)_pushAction:(NSUndoAction *)action {
    /* A registration made while undoing is the redo action, and vice versa. */
    if (_isUndoing) {
        [_redoStack addObject:action];
    } else {
        [_undoStack addObject:action];
        if (!_isRedoing) {
            [_redoStack removeAllObjects];
        }
    }

    if (_levelsOfUndo > 0) {
        while ([_undoStack count] > _levelsOfUndo) {
            [_undoStack removeObjectAtIndex:0];
        }
    }
}

- (void)registerUndoWithTarget:(id)target selector:(SEL)selector object:(id)object {
    if (![self isUndoRegistrationEnabled]) {
        return;
    }

    NSUndoAction *action = [[NSUndoAction alloc] init];

    action->_target = target;
    action->_selector = selector;
    action->_object = [object retain];
    [self _pushAction:action];
    [action release];
}

- (void)_registerUndoInvocation:(NSInvocation *)invocation target:(id)target {
    if (![self isUndoRegistrationEnabled]) {
        return;
    }

    NSUndoAction *action = [[NSUndoAction alloc] init];

    action->_target = target;
    action->_selector = [invocation selector];
    action->_invocation = [invocation retain];
    [self _pushAction:action];
    [action release];
}

- (id)prepareWithInvocationTarget:(id)target {
    NSUndoProxy *proxy = [[NSUndoProxy alloc] init];

    proxy->_manager = self;
    proxy->_target = target;
    return [proxy autorelease];
}

- (void)removeAllActions {
    [_undoStack removeAllObjects];
    [_redoStack removeAllObjects];
}

- (void)removeAllActionsWithTarget:(id)target {
    NSMutableArray *stacks[2] = { _undoStack, _redoStack };

    for (int s = 0; s < 2; s++) {
        NSUInteger i = [stacks[s] count];
        while (i-- > 0) {
            NSUndoAction *action = [stacks[s] objectAtIndex:i];
            if (action->_target == target) {
                [stacks[s] removeObjectAtIndex:i];
            }
        }
    }
}

- (void)beginUndoGrouping {
    [_openGroups addObject:[NSNumber numberWithUnsignedInteger:[_undoStack count]]];
}

- (void)endUndoGrouping {
    if ([_openGroups count] > 0) {
        [_openGroups removeObjectAtIndex:[_openGroups count] - 1];
    }
}

- (NSInteger)groupingLevel {
    return (NSInteger)[_openGroups count];
}

- (BOOL)groupsByEvent {
    return _groupsByEvent;
}

- (void)setGroupsByEvent:(BOOL)groupsByEvent {
    _groupsByEvent = groupsByEvent;
}

- (void)disableUndoRegistration {
    _disableCount++;
}

- (void)enableUndoRegistration {
    if (_disableCount > 0) {
        _disableCount--;
    }
}

- (BOOL)isUndoRegistrationEnabled {
    return _disableCount == 0;
}

- (BOOL)canUndo {
    return [_undoStack count] > 0;
}

- (BOOL)canRedo {
    return [_redoStack count] > 0;
}

- (BOOL)isUndoing {
    return _isUndoing;
}

- (BOOL)isRedoing {
    return _isRedoing;
}

- (void)undo {
    if (![self canUndo]) {
        return;
    }

    NSUndoAction *action = [[_undoStack lastObject] retain];
    [_undoStack removeObjectAtIndex:[_undoStack count] - 1];

    _isUndoing = YES;
    [action invoke];
    _isUndoing = NO;
    [action release];
}

- (void)undoNestedGroup {
    [self undo];
}

- (void)redo {
    if (![self canRedo]) {
        return;
    }

    NSUndoAction *action = [[_redoStack lastObject] retain];
    [_redoStack removeObjectAtIndex:[_redoStack count] - 1];

    _isRedoing = YES;
    [action invoke];
    _isRedoing = NO;
    [action release];
}

- (NSUInteger)levelsOfUndo {
    return _levelsOfUndo;
}

- (void)setLevelsOfUndo:(NSUInteger)levels {
    _levelsOfUndo = levels;
}

- (NSArray *)runLoopModes {
    return _runLoopModes;
}

- (void)setRunLoopModes:(NSArray *)modes {
    modes = [modes copy];
    [_runLoopModes release];
    _runLoopModes = modes;
}

- (void)setActionName:(NSString *)name {
    name = [name copy];
    [_actionName release];
    _actionName = name;
}

- (NSString *)undoActionName {
    return _actionName;
}

- (NSString *)redoActionName {
    return _actionName;
}

- (NSString *)undoMenuItemTitle {
    return (_actionName != nil) ? _actionName : @"Undo";
}

- (NSString *)redoMenuItemTitle {
    return (_actionName != nil) ? _actionName : @"Redo";
}

- (NSUndoManager *)nextUndoManager {
    return _chainedUndoManager;
}

- (void)setNextUndoManager:(NSUndoManager *)undoManager {
    _chainedUndoManager = undoManager;
}

@end
