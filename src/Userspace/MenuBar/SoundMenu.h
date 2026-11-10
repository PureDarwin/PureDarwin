// SoundMenu.h - the Sound menu under the menu bar's speaker: the default output's level and the output devices
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/AppKit.h>

// posted on the main thread whenever a device, the default or a level changes
extern NSString *const SoundChangedNotification;

@interface SoundMenu : NSObject
+ (SoundMenu *)shared;
// opens under the glyph whose left edge is at x in screen points, or closes when it is already open
- (void)toggleAtX:(CGFloat)x;
- (BOOL)isOpen;
// the default output is muted or at no level, the bar draws the speaker faint then
- (BOOL)silent;
- (BOOL)hasOutput;
@end
