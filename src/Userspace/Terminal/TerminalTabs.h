/*
 * TerminalTabs.h - a Terminal window's content: the tab bar and one terminal per tab.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#import "TerminalView.h"

@class TerminalTabs;

@protocol TerminalTabsOwner
- (void)terminalTabsDidClose:(TerminalTabs *)tabs;
- (IBAction)newWindow:(id)sender;
@end

@interface TerminalTabs : NSView <TerminalViewDelegate> {
    NSMutableArray *_terms;
    NSUInteger _current;
    NSInteger _hoverClose;
    id<TerminalTabsOwner> _owner;
}
- (instancetype)initWithOwner:(id<TerminalTabsOwner>)owner;
- (NSSize)preferredContentSize;
- (BOOL)addTab;

- (IBAction)newTab:(id)sender;
- (IBAction)closeTab:(id)sender;
- (IBAction)closeWindow:(id)sender;
- (IBAction)newWindow:(id)sender;
- (IBAction)selectNextTab:(id)sender;
- (IBAction)selectPreviousTab:(id)sender;
@end
