/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfaceTheme.h>
#import <AppKit/NSInterfacePart.h>
#import <AppKit/NSInterfacePartFill.h>
#import <AppKit/NSInterfacePartGradient.h>
#import <AppKit/NSInterfacePartImage.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSColor.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSString.h>
#import <Foundation/NSValue.h>

NSString *const NSInterfaceThemeMenuBarBackground = @"menuBarBackground";
NSString *const NSInterfaceThemeMenuBarItemSelected = @"menuBarItemSelected";
NSString *const NSInterfaceThemeMenuWindowBackground = @"menuWindowBackground";
NSString *const NSInterfaceThemeMenuSelection = @"menuSelection";
NSString *const NSInterfaceThemePushButtonNormal = @"pushButtonNormal";
NSString *const NSInterfaceThemePushButtonPressed = @"pushButtonPressed";
NSString *const NSInterfaceThemePushButtonHighlighted = @"pushButtonHighlighted";
NSString *const NSInterfaceThemeScrollerTrack = @"scrollerTrack";
NSString *const NSInterfaceThemeScrollerKnob = @"scrollerKnob";
NSString *const NSInterfaceThemeTableViewHeader = @"tableViewHeader";

static NSInterfaceTheme *sCurrentTheme = nil;

/* "r g b" or "r g b a", each component 0..1. Returns nil for anything else so
 * a malformed entry disables that one part instead of drawing a wrong colour. */
static NSColor *themeColor(id value)
{
    if (![value isKindOfClass:[NSString class]]) {
        return nil;
    }

    NSArray *fields = [(NSString *)value componentsSeparatedByString:@" "];
    CGFloat rgba[4] = { 0.0, 0.0, 0.0, 1.0 };
    NSUInteger given = 0;

    for (NSString *field in fields) {
        if ([field length] == 0) {
            continue;           /* tolerate runs of spaces */
        }
        if (given >= 4) {
            return nil;
        }
        rgba[given++] = (CGFloat)[field doubleValue];
    }
    if (given != 3 && given != 4) {
        return nil;
    }
    return [NSColor colorWithCalibratedRed:rgba[0] green:rgba[1] blue:rgba[2] alpha:rgba[3]];
}

static NSInterfacePart *themePart(NSDictionary *description, NSString *directory)
{
    if (![description isKindOfClass:[NSDictionary class]]) {
        return nil;
    }

    NSString *type = [description objectForKey:@"type"];
    NSColor *border = themeColor([description objectForKey:@"border"]);
    CGFloat radius = (CGFloat)[[description objectForKey:@"radius"] doubleValue];

    if ([type isEqualToString:@"gradient"]) {
        NSArray *values = [description objectForKey:@"colors"];
        NSMutableArray *colors = [NSMutableArray array];

        if (![values isKindOfClass:[NSArray class]]) {
            return nil;
        }
        for (id value in values) {
            NSColor *color = themeColor(value);

            if (color == nil) {
                return nil;
            }
            [colors addObject:color];
        }
        if ([colors count] < 2) {
            return nil;
        }

        id angle = [description objectForKey:@"angle"];
        /* Top-to-bottom unless asked otherwise, which is what nearly every
         * control background wants. */
        CGFloat degrees = (angle != nil) ? (CGFloat)[angle doubleValue] : 270.0;

        NSInterfacePartGradient *part =
            [[[NSInterfacePartGradient alloc] initWithColors:colors
                                                      angle:degrees
                                                borderColor:border
                                               cornerRadius:radius] autorelease];

        [part setTopHighlightColor:themeColor([description objectForKey:@"topHighlight"])
                 bottomBorderColor:themeColor([description objectForKey:@"bottomBorder"])];
        return part;
    }
    if ([type isEqualToString:@"image"]) {
        NSString *file = [description objectForKey:@"file"];

        if (![file isKindOfClass:[NSString class]] || [file length] == 0) {
            return nil;
        }
        /* Relative to the theme file, so a theme is a self-contained folder. */
        NSString *path = [file isAbsolutePath]
            ? file
            : [directory stringByAppendingPathComponent:file];
        NSImage *image = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];

        if (image == nil) {
            return nil;
        }

        NSArray *insets = [description objectForKey:@"insets"];
        CGFloat edge[4] = { 0.0, 0.0, 0.0, 0.0 };   /* left right top bottom */

        if ([insets isKindOfClass:[NSArray class]] && [insets count] == 4) {
            for (NSUInteger i = 0; i < 4; i++) {
                edge[i] = (CGFloat)[[insets objectAtIndex:i] doubleValue];
            }
        }
        return [[[NSInterfacePartImage alloc] initWithImage:image
                                                 leftInset:edge[0]
                                                rightInset:edge[1]
                                                  topInset:edge[2]
                                               bottomInset:edge[3]] autorelease];
    }
    if ([type isEqualToString:@"fill"]) {
        NSColor *fill = themeColor([description objectForKey:@"color"]);

        if (fill == nil && border == nil) {
            return nil;
        }
        return [[[NSInterfacePartFill alloc] initWithFillColor:fill
                                                   borderColor:border
                                                  cornerRadius:radius] autorelease];
    }
    return nil;
}

@implementation NSInterfaceTheme

+ (NSInterfaceTheme *)currentTheme
{
    if (sCurrentTheme == nil) {
        NSString *path = [[[NSProcessInfo processInfo] environment]
                          objectForKey:@"PUREDARWIN_THEME"];

        if ([path length] == 0) {
            path = @"/System/Library/Themes/Default.plist";
        }
        if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
            sCurrentTheme = [[NSInterfaceTheme alloc] initWithContentsOfFile:path];
        }
        if (sCurrentTheme == nil) {
            /* No theme is a valid state, not an error: an empty theme means
             * every control keeps drawing itself exactly as before. */
            sCurrentTheme = [[NSInterfaceTheme alloc] initWithDictionary:nil];
        }
    }
    return sCurrentTheme;
}

+ (void)setCurrentTheme:(NSInterfaceTheme *)theme
{
    if (theme == sCurrentTheme) {
        return;
    }
    [theme retain];
    [sCurrentTheme release];
    sCurrentTheme = theme;
}

- initWithDictionary:(NSDictionary *)description
{
    return [self initWithDictionary:description directory:nil];
}

- initWithDictionary:(NSDictionary *)description directory:(NSString *)directory
{
    if ((self = [super init]) == nil) {
        return nil;
    }
    _parts = [[NSMutableDictionary alloc] init];

    for (NSString *name in [description allKeys]) {
        NSInterfacePart *part = themePart([description objectForKey:name], directory);

        if (part != nil) {
            [_parts setObject:part forKey:name];
        }
    }
    return self;
}

- initWithContentsOfFile:(NSString *)path
{
    NSDictionary *description = [NSDictionary dictionaryWithContentsOfFile:path];

    if (![description isKindOfClass:[NSDictionary class]]) {
        [self release];
        return nil;
    }
    return [self initWithDictionary:description
                         directory:[path stringByDeletingLastPathComponent]];
}

- (void)dealloc
{
    [_parts release];
    [super dealloc];
}

- (NSInterfacePart *)partNamed:(NSString *)name
{
    return (name != nil) ? [_parts objectForKey:name] : nil;
}

- (BOOL)drawPartNamed:(NSString *)name inRect:(NSRect)rect
{
    NSInterfacePart *part = [self partNamed:name];

    if (part == nil) {
        return NO;
    }
    [part drawInRect:rect];
    return YES;
}

@end
