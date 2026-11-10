// PDUtility.h - what the utility apps share: palette, unified toolbar, sidebars, status lines, glyphs and sheets
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/AppKit.h>

#define PD_TOOLBAR_H 44
#define PD_STATUS_H 24
// the toolbar's content starts right of the traffic lights
#define PD_LIGHTS_W 84

BOOL PDDark(void);
NSColor *PDFixed(unsigned rgb);
// one colour for the light look and one for the dark, picked when drawn
NSColor *PDColor(unsigned light, unsigned dark);
NSColor *PDAlpha(NSColor *color, CGFloat alpha);
NSFont *PDFont(NSString *name, CGFloat size);
NSFont *PDMono(CGFloat size);
NSString *PDFitted(NSString *text, NSDictionary *attrs, CGFloat width);
NSString *PDBytes(double n);
NSDictionary *PDText(NSFont *font, NSColor *color);
NSImage *PDResourceImage(NSString *name);
void PDFillRounded(NSRect r, CGFloat radius, NSColor *color);

// the named glyphs the toolbars draw: back, forward, stop, info, pause, play, clear, zoomin,
// zoomout, fit, rotl, rotr, search, check, partition, erase, eject, mount, plus
void PDDrawGlyph(NSString *name, NSRect r, NSColor *color);

void PDDrawToolbar(NSView *view, NSRect bar);
void PDDrawSidebar(NSRect side);
void PDDrawStatus(NSRect r, NSString *left, NSString *right);
void PDDrawTitle(NSString *title, NSString *subtitle, NSPoint at, CGFloat maxWidth, BOOL onDark);
void PDDrawSectionHeading(NSString *text, NSPoint at);

// a toolbar button: a glyph and an optional label, it remembers where it was drawn
@interface PDToolButton : NSObject {
@public
    NSString *glyph;
    NSString *label;
    SEL action;
    id target;
    NSRect frame;
    BOOL enabled;
    BOOL on;
    BOOL hidden;
}
+ (instancetype)buttonWithGlyph:(NSString *)glyph label:(NSString *)label target:(id)target action:(SEL)action;
- (CGFloat)width;
- (void)drawAt:(NSPoint)origin height:(CGFloat)h onDark:(BOOL)onDark;
@end

// lays toolbar buttons out right to left from an edge, returns the left edge reached
CGFloat PDLayoutButtons(NSArray *buttons, CGFloat right, CGFloat barH, BOOL onDark);
// the button under a point, enabled or not, or nil
PDToolButton *PDButtonAt(NSArray *buttons, NSPoint p);
BOOL PDClickButtons(NSArray *buttons, NSPoint p);

// a segmented control like the demo's CPU and Memory switch
@interface PDSegments : NSObject {
@public
    NSArray *labels;
    NSInteger selected;
    NSRect frame;
}
- (void)drawCenteredIn:(NSRect)r;
- (NSInteger)segmentAt:(NSPoint)p;
@end

// a sheet hung from the toolbar inside the window: a title, text, an optional field or
// monospace block, and buttons that call back with their index
@interface PDSheet : NSView {
    NSString *_icon;
    NSString *_title;
    NSString *_message;
    NSString *_mono;
    NSArray *_buttons;
    NSArray *_styles;
    NSTextField *_field;
    NSMutableArray *_rects;
    void (^_done)(NSInteger index, NSString *field);
}
+ (PDSheet *)showInView:(NSView *)host icon:(NSString *)icon title:(NSString *)title message:(NSString *)message
                  field:(NSString *)field mono:(NSString *)mono buttons:(NSArray *)buttons styles:(NSArray *)styles
                   done:(void (^)(NSInteger index, NSString *field))done;
- (void)dismiss;
@end

// the menus every app has, the app menu first
NSMenu *PDAddMenu(NSMenu *main, NSString *title);
NSMenuItem *PDAddItem(NSMenu *menu, NSString *title, SEL action, NSString *key, NSUInteger mods);
NSMenu *PDStandardMenus(NSString *appName, NSMenu **windowMenu);

// a window like the demo's: titled, transparent title bar, content under it
NSWindow *PDMakeWindow(NSSize size, NSSize minSize, CGFloat offset, id delegate);
