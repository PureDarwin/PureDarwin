/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>

@class NSString, NSDictionary, NSMutableDictionary, NSInterfacePart;

/* Names NSGraphicsStyle looks up. A theme need not define all of them: any it
 * leaves out falls back to the style's own drawing, so a partial theme is
 * valid and an empty one is exactly the untouched appearance. */
extern NSString *const NSInterfaceThemeMenuBarBackground;
extern NSString *const NSInterfaceThemeMenuBarItemSelected;
extern NSString *const NSInterfaceThemeMenuWindowBackground;
extern NSString *const NSInterfaceThemeMenuSelection;
extern NSString *const NSInterfaceThemePushButtonNormal;
extern NSString *const NSInterfaceThemePushButtonPressed;
extern NSString *const NSInterfaceThemePushButtonHighlighted;
extern NSString *const NSInterfaceThemeScrollerTrack;
extern NSString *const NSInterfaceThemeScrollerKnob;
extern NSString *const NSInterfaceThemeTableViewHeader;

@interface NSInterfaceTheme : NSObject {
    NSMutableDictionary *_parts;
}

+ (NSInterfaceTheme *)currentTheme;
+ (void)setCurrentTheme:(NSInterfaceTheme *)theme;

- initWithContentsOfFile:(NSString *)path;
- initWithDictionary:(NSDictionary *)description;
/* Relative "file" paths in image parts resolve against this directory. */
- initWithDictionary:(NSDictionary *)description directory:(NSString *)directory;

- (NSInterfacePart *)partNamed:(NSString *)name;

/* Draws the named part and returns YES, or returns NO without drawing when the
 * theme does not define it - which is the caller's signal to draw its own. */
- (BOOL)drawPartNamed:(NSString *)name inRect:(NSRect)rect;

@end
