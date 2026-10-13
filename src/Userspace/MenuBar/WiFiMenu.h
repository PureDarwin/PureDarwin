/*
 * WiFiMenu.h - the Wi-Fi menu under the menu bar's Wi-Fi glyph.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>

@interface WiFiMenu : NSObject
+ (WiFiMenu *)shared;
// opens under the glyph whose left edge is at x in screen points, or closes when it is already open
- (void)toggleAtX:(CGFloat)x;
- (BOOL)isOpen;
@end
