/*
 * main.m - Calculator: the basic four-function calculator, laid out as macOS's dark keypad.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#include <math.h>

#define CALC_W 232
#define TITLE_H 28
#define DISPLAY_H 72
#define KEY_H 48
#define GAP 1
#define MAX_DIGITS 9

static NSColor *rgb(int r, int g, int b)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1];
}

static NSFont *font(NSString *name, CGFloat size)
{
    return [NSFont fontWithName:name size:size] ?: [NSFont systemFontOfSize:size];
}

// the keypad row by row, kind is what colours the key
typedef enum { KeyDigit, KeyFunction, KeyOperator } KeyKind;

static const struct { NSString *label; KeyKind kind; int span; } keys[] = {
    { @"AC", KeyFunction, 1 }, { @"±", KeyFunction, 1 }, { @"%", KeyFunction, 1 }, { @"÷", KeyOperator, 1 },
    { @"7", KeyDigit, 1 }, { @"8", KeyDigit, 1 }, { @"9", KeyDigit, 1 }, { @"×", KeyOperator, 1 },
    { @"4", KeyDigit, 1 }, { @"5", KeyDigit, 1 }, { @"6", KeyDigit, 1 }, { @"−", KeyOperator, 1 },
    { @"1", KeyDigit, 1 }, { @"2", KeyDigit, 1 }, { @"3", KeyDigit, 1 }, { @"+", KeyOperator, 1 },
    { @"0", KeyDigit, 2 }, { @".", KeyDigit, 1 }, { @"=", KeyOperator, 1 },
};
#define KEY_COUNT (sizeof(keys) / sizeof(keys[0]))

@interface CalculatorView : NSView {
    NSString *_current;
    double _accumulator;
    BOOL _haveAccumulator;
    NSString *_operator;
    BOOL _fresh;
    NSInteger _pressed;
}
- (void)press:(NSString *)key;
- (NSString *)displayValue;
- (void)setDisplayValue:(NSString *)value;
@end

@implementation CalculatorView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _current = @"0";
    _fresh = YES;
    _pressed = -1;
    return self;
}

- (void)dealloc
{
    [_current release];
    [_operator release];
    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (NSRect)rectOfKey:(NSUInteger)index
{
    CGFloat w = (CALC_W - 3 * GAP) / 4.0, top = TITLE_H + DISPLAY_H + GAP;
    NSUInteger column = 0, row = 0;

    for (NSUInteger i = 0; i < index; i++) {
        column += keys[i].span;
        if (column >= 4)
            column = 0, row++;
    }
    return NSMakeRect(floor(column * (w + GAP)), top + row * (KEY_H + GAP),
                      ceil(keys[index].span * w + (keys[index].span - 1) * GAP), KEY_H);
}

- (NSString *)labelOfKey:(NSUInteger)index
{
    // AC clears everything until something is typed, then it only clears the entry
    if (index == 0)
        return [_current isEqualToString:@"0"] && _fresh ? @"AC" : @"C";
    return keys[index].label;
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    NSDictionary *digit = @{ NSFontAttributeName: font(@"Inter-Regular", 20), NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSDictionary *op = @{ NSFontAttributeName: font(@"Inter-Regular", 24), NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSDictionary *opOn = @{ NSFontAttributeName: font(@"Inter-Regular", 24), NSForegroundColorAttributeName: rgb(0xff, 0x9f, 0x0a) };
    CGFloat size = [_current length] > 7 ? MAX(24, 48 - ([_current length] - 7) * 5) : 48;
    NSDictionary *display = @{ NSFontAttributeName: font(@"Inter-Light", size), NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSSize ds = [_current sizeWithAttributes:display];

    [rgb(0x2b, 0x2b, 0x2d) setFill];
    NSRectFill(b);
    [_current drawAtPoint:NSMakePoint(CALC_W - 16 - ds.width, TITLE_H + DISPLAY_H - 4 - ds.height) withAttributes:display];

    [rgb(0x1f, 0x1f, 0x20) setFill];
    NSRectFill(NSMakeRect(0, TITLE_H + DISPLAY_H, CALC_W, NSHeight(b) - TITLE_H - DISPLAY_H));
    for (NSUInteger i = 0; i < KEY_COUNT; i++) {
        NSRect r = [self rectOfKey:i];
        NSString *label = [self labelOfKey:i];
        BOOL on = keys[i].kind == KeyOperator && _fresh && [_operator isEqualToString:label];
        NSDictionary *attrs = keys[i].kind == KeyOperator ? (on ? opOn : op) : digit;
        NSColor *fill = keys[i].kind == KeyOperator ? (on ? [NSColor whiteColor] : rgb(0xff, 0x9f, 0x0a))
                      : keys[i].kind == KeyFunction ? rgb(0x44, 0x44, 0x46) : rgb(0x5a, 0x5a, 0x5c);
        NSSize ls = [label sizeWithAttributes:attrs];

        if ((NSInteger)i == _pressed)
            fill = [fill blendedColorWithFraction:0.3 ofColor:[NSColor whiteColor]];
        [fill setFill];
        NSRectFill(r);
        // the wide zero keeps its label at the left like the others in its column
        [label drawAtPoint:NSMakePoint(keys[i].span > 1 ? NSMinX(r) + 22 : floor(NSMidX(r) - ls.width / 2),
                                       floor(NSMidY(r) - ls.height / 2)) withAttributes:attrs];
    }
}

- (NSString *)format:(double)value
{
    NSString *text;

    if (!isfinite(value))
        return @"Error";
    text = [NSString stringWithFormat:@"%.*g", MAX_DIGITS, value];
    if ([text length] > MAX_DIGITS)
        text = [NSString stringWithFormat:@"%.3e", value];
    return text;
}

- (void)setCurrent:(NSString *)value
{
    [_current release];
    _current = [value copy];
}

- (void)apply
{
    double a = _accumulator, b = [_current doubleValue], r;

    if (_operator == nil || !_haveAccumulator)
        return;
    if ([_operator isEqualToString:@"÷"])
        r = a / b;
    else if ([_operator isEqualToString:@"×"])
        r = a * b;
    else if ([_operator isEqualToString:@"−"])
        r = a - b;
    else
        r = a + b;
    [self setCurrent:[self format:r]];
}

- (void)press:(NSString *)key
{
    unichar c = [key length] == 1 ? [key characterAtIndex:0] : 0;

    if (c >= '0' && c <= '9') {
        if (_fresh || [_current isEqualToString:@"0"])
            [self setCurrent:key];
        else if ([[_current stringByReplacingOccurrencesOfString:@"-" withString:@""] length] < MAX_DIGITS)
            [self setCurrent:[_current stringByAppendingString:key]];
        _fresh = NO;
    } else if ([key isEqualToString:@"."]) {
        if (_fresh)
            [self setCurrent:@"0"], _fresh = NO;
        if ([_current rangeOfString:@"."].location == NSNotFound)
            [self setCurrent:[_current stringByAppendingString:@"."]];
    } else if ([key isEqualToString:@"AC"]) {
        [self setCurrent:@"0"];
        _haveAccumulator = NO;
        [_operator release];
        _operator = nil;
        _fresh = YES;
    } else if ([key isEqualToString:@"C"]) {
        [self setCurrent:@"0"];
        _fresh = YES;
    } else if ([key isEqualToString:@"±"]) {
        if ([_current hasPrefix:@"-"])
            [self setCurrent:[_current substringFromIndex:1]];
        else if (![_current isEqualToString:@"0"])
            [self setCurrent:[@"-" stringByAppendingString:_current]];
    } else if ([key isEqualToString:@"%"]) {
        [self setCurrent:[self format:[_current doubleValue] / 100]];
    } else if ([@[ @"÷", @"×", @"−", @"+" ] containsObject:key]) {
        if (!_fresh)
            [self apply];
        _accumulator = [_current doubleValue];
        _haveAccumulator = YES;
        [_operator release];
        _operator = [key copy];
        _fresh = YES;
    } else if ([key isEqualToString:@"="]) {
        [self apply];
        _haveAccumulator = NO;
        [_operator release];
        _operator = nil;
        _fresh = YES;
    }
    [self setNeedsDisplay:YES];
}

- (NSString *)displayValue
{
    return _current;
}

- (void)setDisplayValue:(NSString *)value
{
    double v = [value doubleValue];

    if ([[value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] length] == 0)
        return;
    [self setCurrent:[self format:v]];
    _fresh = NO;
    [self setNeedsDisplay:YES];
}

- (NSInteger)keyAtPoint:(NSPoint)p
{
    for (NSUInteger i = 0; i < KEY_COUNT; i++) {
        if (NSPointInRect(p, [self rectOfKey:i]))
            return (NSInteger)i;
    }
    return -1;
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    // the display and the title strip move the window, the title bar is see-through
    if (p.y < TITLE_H + DISPLAY_H) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    _pressed = [self keyAtPoint:p];
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    NSInteger key = [self keyAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    if (key >= 0 && key == _pressed)
        [self press:[self labelOfKey:key]];
    _pressed = -1;
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event characters];
    NSDictionary *map = @{ @"/": @"÷", @"*": @"×", @"x": @"×", @"-": @"−", @"+": @"+", @"=": @"=", @"\r": @"=",
                           @"%": @"%", @".": @".", @",": @"." };
    unichar c = [chars length] ? [chars characterAtIndex:0] : 0;

    if (c >= '0' && c <= '9')
        [self press:chars];
    else if (c == 27)
        [self press:[_current isEqualToString:@"0"] && _fresh ? @"AC" : @"C"];
    else if (c == 127 || c == 8)
        [self press:@"C"];
    else if (map[chars] != nil)
        [self press:map[chars]];
    else
        [super keyDown:event];
}

@end

@interface CalculatorController : NSObject {
    NSWindow *_window;
    CalculatorView *_view;
}
@end

@implementation CalculatorController

static NSMenuItem *addItem(NSMenu *menu, NSString *title, SEL action, NSString *key)
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];

    [menu addItem:item];
    [item release];
    return item;
}

static NSMenu *addMenu(NSMenu *mainMenu, NSString *title)
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:title];

    [addItem(mainMenu, title, NULL, @"") setSubmenu:menu];
    [menu release];
    return menu;
}

- (void)buildMenu
{
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSMenu *app = addMenu(mainMenu, @"Calculator");
    NSMenu *edit = addMenu(mainMenu, @"Edit");
    NSMenu *view = addMenu(mainMenu, @"View");
    NSMenu *window = addMenu(mainMenu, @"Window");
    NSMenu *help = addMenu(mainMenu, @"Help");

    [addItem(app, @"About Calculator", NULL, @"") setEnabled:NO];
    [app addItem:[NSMenuItem separatorItem]];
    addItem(app, @"Quit Calculator", @selector(terminate:), @"q");
    [addItem(edit, @"Copy", @selector(copy:), @"c") setTarget:self];
    [addItem(edit, @"Paste", @selector(paste:), @"v") setTarget:self];
    [addItem(edit, @"All Clear", @selector(allClear:), @"") setTarget:self];
    [addItem(view, @"Basic", NULL, @"1") setState:NSOnState];
    addItem(window, @"Minimize", @selector(performMiniaturize:), @"m");
    [addItem(help, @"Calculator Help", NULL, @"") setEnabled:NO];
    [NSApp setMainMenu:mainMenu];
    [mainMenu release];
}

- (void)copy:(id)sender
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];

    [pb declareTypes:@[ NSStringPboardType ] owner:nil];
    [pb setString:[_view displayValue] forType:NSStringPboardType];
}

- (void)paste:(id)sender
{
    NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType];

    if (text != nil)
        [_view setDisplayValue:text];
}

- (void)allClear:(id)sender
{
    [_view press:@"AC"];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect screen = [[NSScreen mainScreen] visibleFrame];
    CGFloat h = TITLE_H + DISPLAY_H + GAP + 5 * KEY_H + 4 * GAP;
    NSRect frame = NSMakeRect(NSMaxX(screen) - CALC_W - 120, NSMaxY(screen) - h - 64, CALC_W, h);

    [self buildMenu];
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask |
                                                    NSFullSizeContentViewWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"Calculator"];
    [_window setTitlebarAppearsTransparent:YES];
    [_window setTitleVisibility:NSWindowTitleHidden];
    [_window setBackgroundColor:rgb(0x2b, 0x2b, 0x2d)];
    [_window setDelegate:self];
    _view = [[CalculatorView alloc] initWithFrame:NSMakeRect(0, 0, CALC_W, h)];
    [_window setContentView:_view];
    [_window makeFirstResponder:_view];
    [_window makeKeyAndOrderFront:nil];
}

- (void)windowWillClose:(NSNotification *)note
{
    [NSApp terminate:self];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    CalculatorController *controller = [[CalculatorController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
