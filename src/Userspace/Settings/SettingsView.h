/*
 * SettingsView.h - the Settings window: home tiles, panes, breadcrumbs and search.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>

@interface SettingsView : NSView {
    NSString *_pane;
    NSMutableArray *_history;
    NSMutableString *_search;
    BOOL _searchActive;
    CGFloat _scroll;
    CGFloat _contentHeight;
    NSMutableArray *_hits;
    id _drag;
    NSString *_message;
    NSArray *_daemons;
    NSArray *_kexts;
    NSMutableArray *_bootArgs;
    NSArray *_displayModes;
    NSRect _dirty;
}
- (void)showPane:(NSString *)pane;
- (IBAction)goBack:(id)sender;
- (IBAction)goHome:(id)sender;
- (IBAction)find:(id)sender;
@end
