/*
 * MenuBarView.h - draws the bar and pops up the menus.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>

#define MenuBarHeight 26

@class MenuBarView;

@protocol MenuBarViewDelegate
- (void)menuBarView:(MenuBarView *)view pickedPath:(NSArray *)path;
@end

@interface MenuBarView : NSView {
    id<MenuBarViewDelegate> _delegate;
    NSDictionary *_menuData;
    NSMutableArray *_titleRects;
    NSImage *_beak;
    NSArray *_status;
    NSInteger _open;
    NSPanel *_lookoutWindow;
    NSTextField *_lookoutField;
    NSMutableArray *_lookoutResults;
    NSInteger _lookoutSelection;
}

- (void)setDelegate:(id<MenuBarViewDelegate>)delegate;
- (void)setMenuData:(NSDictionary *)menuData;
- (void)tick:(NSTimer *)timer;

@end
