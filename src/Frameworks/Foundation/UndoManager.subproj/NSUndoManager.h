/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSUndoManager_h
#define NSUndoManager_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray, NSMutableArray;

FOUNDATION_EXPORT NSString *const NSUndoManagerCheckpointNotification;
FOUNDATION_EXPORT NSString *const NSUndoManagerDidOpenUndoGroupNotification;
FOUNDATION_EXPORT NSString *const NSUndoManagerDidRedoChangeNotification;
FOUNDATION_EXPORT NSString *const NSUndoManagerDidUndoChangeNotification;
FOUNDATION_EXPORT NSString *const NSUndoManagerWillCloseUndoGroupNotification;
FOUNDATION_EXPORT NSString *const NSUndoManagerWillRedoChangeNotification;
FOUNDATION_EXPORT NSString *const NSUndoManagerWillUndoChangeNotification;

@interface NSUndoManager : NSObject {
    NSMutableArray *_undoStack;
    NSMutableArray *_redoStack;
    NSMutableArray *_openGroups;
    NSArray *_runLoopModes;
    /* Not named _nextUndoManager: AppKit's NSCellUndoManager subclass declares
     * an ivar by that name, and a duplicate member is an error. */
    NSUndoManager *_chainedUndoManager;
    NSString *_actionName;
    NSInteger _disableCount;
    NSUInteger _levelsOfUndo;
    BOOL _isUndoing;
    BOOL _isRedoing;
    BOOL _groupsByEvent;
}

- (void)registerUndoWithTarget:(id)target selector:(SEL)selector object:(id)object;
- (id)prepareWithInvocationTarget:(id)target;
- (void)removeAllActions;
- (void)removeAllActionsWithTarget:(id)target;

- (void)beginUndoGrouping;
- (void)endUndoGrouping;
- (NSInteger)groupingLevel;
- (BOOL)groupsByEvent;
- (void)setGroupsByEvent:(BOOL)groupsByEvent;

- (void)disableUndoRegistration;
- (void)enableUndoRegistration;
- (BOOL)isUndoRegistrationEnabled;

- (BOOL)canUndo;
- (BOOL)canRedo;
- (BOOL)isUndoing;
- (BOOL)isRedoing;
- (void)undo;
- (void)undoNestedGroup;
- (void)redo;

- (NSUInteger)levelsOfUndo;
- (void)setLevelsOfUndo:(NSUInteger)levels;
- (NSArray *)runLoopModes;
- (void)setRunLoopModes:(NSArray *)modes;

- (void)setActionName:(NSString *)name;
- (NSString *)undoActionName;
- (NSString *)redoActionName;
- (NSString *)undoMenuItemTitle;
- (NSString *)redoMenuItemTitle;

- (NSUndoManager *)nextUndoManager;
- (void)setNextUndoManager:(NSUndoManager *)undoManager;

@end

#endif /* NSUndoManager_h */
