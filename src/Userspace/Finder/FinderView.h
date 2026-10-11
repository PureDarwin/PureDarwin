/*
 * FinderView.h - one Finder window's contents: sidebar, toolbar, icon grid and status bar.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>

@class FinderView;

@protocol FinderViewDelegate
- (void)finderView:(FinderView *)view didChangePath:(NSString *)path;
- (void)finderView:(FinderView *)view openApplication:(NSString *)path;
@end

@interface FinderView : NSView {
    NSString *_path;
    NSArray *_entries;
    NSMutableIndexSet *_selection;
    NSMutableArray *_back;
    NSMutableArray *_forward;
    NSArray *_places;
    BOOL _sidebarHidden;
    CGFloat _scroll;
    id<FinderViewDelegate> _delegate;
}
- (instancetype)initWithFrame:(NSRect)frame path:(NSString *)path delegate:(id<FinderViewDelegate>)delegate;
- (NSString *)path;
- (void)goToPath:(NSString *)path;
- (BOOL)sidebarHidden;

- (IBAction)goBack:(id)sender;
- (IBAction)goForward:(id)sender;
- (IBAction)goEnclosingFolder:(id)sender;
- (IBAction)goToPlace:(id)sender;
- (IBAction)openSelection:(id)sender;
- (IBAction)newFolder:(id)sender;
- (IBAction)toggleSidebar:(id)sender;
@end
