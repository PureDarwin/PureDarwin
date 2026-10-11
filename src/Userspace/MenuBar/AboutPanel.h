/*
 * AboutPanel.h - the About PureDarwin window opened from the beak menu.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>

@interface AboutPanel : NSObject {
    NSWindow *_window;
}
+ (AboutPanel *)sharedPanel;
- (void)show;
@end
