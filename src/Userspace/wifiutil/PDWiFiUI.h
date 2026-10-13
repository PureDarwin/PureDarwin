/*
 * PDWiFiUI.h - the pieces of the Wi-Fi design the menu bar, Settings and the installer share:
 * the glyph, the lock, the live state and the join sheet.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#import <AppKit/AppKit.h>

// posted on the main queue whenever PDWiFi has new state, in this app or another one
extern NSString *const PDWiFiChangedNotification;

// one network as the screens show it
@interface PDWiFiNetwork : NSObject
@property (copy) NSString *ssid;
@property (copy) NSString *security;
@property int rssi;
@property unsigned channel;
@property BOOL known;
- (int)bars;
@end

// the radio, the joined network, what is in range and what is remembered, refreshed off the main thread
@interface PDWiFi : NSObject
+ (PDWiFi *)shared;
@property (readonly) BOOL hardware;
@property (readonly) BOOL powered;
@property (readonly, retain) PDWiFiNetwork *current;
@property (readonly, copy) NSArray *inRange;
@property (readonly, copy) NSArray *known;
@property (readonly, copy) NSString *busy;
// why the last join from a list failed, cleared by the next join
@property (readonly, copy) NSString *lastError;
@property (readonly) BOOL scanning;
@property (readonly, copy) NSString *interface;
@property (readonly, copy) NSString *hardwareAddress;
- (void)refresh;
- (void)scan;
- (void)setPower:(BOOL)on;
- (void)leave;
- (void)forget:(NSString *)ssid;
// a known or open network joins straight away, a new secured one opens the join sheet
- (void)pick:(PDWiFiNetwork *)network accent:(NSColor *)accent;
- (void)joinOther:(NSColor *)accent;
@end

// the accent chosen in Settings (AppleAccentColor), blue when none is
NSColor *PDSystemAccent(void);

// the glyph: a dot and three arcs, the arcs past the level drawn faint, level 0 is all faint
void PDDrawWiFiGlyph(NSRect r, int level, NSColor *colour, BOOL flipped);
void PDDrawWiFiLock(NSRect r, NSColor *colour, BOOL flipped);
// a ring turning with the clock, for a network being joined
void PDDrawWiFiSpinner(NSRect r, NSColor *colour);

// the password sheet, or the other network sheet when ssid is nil
@interface PDWiFiJoinPanel : NSObject
+ (void)showForNetwork:(PDWiFiNetwork *)network accent:(NSColor *)accent;
@end
