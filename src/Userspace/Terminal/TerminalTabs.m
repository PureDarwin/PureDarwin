/*
 * TerminalTabs.m - a Terminal window's content: the tab bar and one terminal per tab.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "TerminalTabs.h"

// the demo's tab bar, shown only while a window has more than one tab
#define BAR_H 26
#define CLOSE_X 8
#define CLOSE_W 14

static NSColor *grey(int v)
{
    return [NSColor colorWithCalibratedWhite:v / 255.0 alpha:1];
}

static NSFont *tabFont(void)
{
    NSFont *f = [NSFont fontWithName:@"Inter-Regular" size:12];

    return f != nil ? f : [NSFont systemFontOfSize:12];
}

@implementation TerminalTabs

- (instancetype)initWithOwner:(id<TerminalTabsOwner>)owner
{
    if ((self = [super initWithFrame:NSMakeRect(0, 0, 100, 100)]) == nil)
        return nil;
    _owner = owner;
    _terms = [[NSMutableArray alloc] init];
    _hoverClose = -1;
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    return self;
}

- (void)dealloc
{
    for (TerminalView *t in _terms)
        [t setDelegate:nil];
    [_terms release];
    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}

- (CGFloat)barHeight
{
    return [_terms count] > 1 ? BAR_H + 1 : 0;
}

- (TerminalView *)currentTerm
{
    return _current < [_terms count] ? _terms[_current] : nil;
}

// 80 by 24 like the first window Terminal opens, plus the bar when there is one
- (NSSize)preferredContentSize
{
    TerminalView *t = [self currentTerm];
    NSSize s = t != nil ? [t sizeForRows:24 cols:80] : NSMakeSize(640, 400);

    return NSMakeSize(s.width, s.height + [self barHeight]);
}

- (void)layoutTerms
{
    NSRect b = [self bounds];
    CGFloat bar = [self barHeight];

    [[self currentTerm] setFrame:NSMakeRect(0, bar, NSWidth(b), MAX(0, NSHeight(b) - bar))];
    [self setNeedsDisplay:YES];
}

- (void)resizeSubviewsWithOldSize:(NSSize)old
{
    [self layoutTerms];
}

- (void)updateTitle
{
    NSString *title = [[self currentTerm] title];

    [[self window] setTitle:[title length] > 0 ? title : @"Terminal"];
}

- (void)selectTab:(NSUInteger)i
{
    TerminalView *old = [self currentTerm];

    if (i >= [_terms count])
        return;
    if (old != nil && old != _terms[i])
        [old removeFromSuperview];
    _current = i;
    if ([[self currentTerm] superview] != self)
        [self addSubview:[self currentTerm]];
    [self layoutTerms];
    [[self window] makeFirstResponder:[self currentTerm]];
    [self updateTitle];
}

// first responder and title wait for the window the tabs land in
- (void)viewDidMoveToWindow
{
    if ([self window] != nil && [_terms count] > 0)
        [self selectTab:_current];
}

- (BOOL)addTab
{
    TerminalView *t = [[[TerminalView alloc] initWithRows:24 cols:80] autorelease];

    [t setDelegate:self];
    if (![t startShell])
        return NO;
    [_terms addObject:t];
    [self selectTab:[_terms count] - 1];
    return YES;
}

- (void)removeTerm:(TerminalView *)t
{
    NSUInteger i = [_terms indexOfObject:t];

    if (i == NSNotFound)
        return;
    [[t retain] autorelease];
    [t setDelegate:nil];
    [t removeFromSuperview];
    [_terms removeObjectAtIndex:i];
    if ([_terms count] == 0) {
        [_owner terminalTabsDidClose:self];
        return;
    }
    // the tab to the left takes over, as in the demo
    _current = i > 0 ? i - 1 : 0;
    [self selectTab:_current];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    NSUInteger n = [_terms count], i;
    CGFloat w;

    if (n < 2)
        return;
    w = NSWidth(b) / n;
    [grey(0xd4) setFill];
    NSRectFill(NSMakeRect(0, 0, NSWidth(b), BAR_H));
    [grey(0xb4) setFill];
    NSRectFill(NSMakeRect(0, BAR_H, NSWidth(b), 1));
    for (i = 0; i < n; i++) {
        NSRect r = NSMakeRect(floor(i * w), 0, floor((i + 1) * w) - floor(i * w), BAR_H);
        BOOL active = i == _current;
        NSString *title = [_terms[i] title] ?: @"Terminal";
        NSDictionary *a = @{ NSFontAttributeName: tabFont(), NSForegroundColorAttributeName: active ? grey(0x11) : grey(0x44) };
        NSSize s;

        if (active) {
            [grey(0xec) setFill];
            NSRectFill(r);
        }
        if (i + 1 < n) {
            [grey(0xb4) setFill];
            NSRectFill(NSMakeRect(NSMaxX(r) - 1, 0, 1, BAR_H));
        }
        // long titles keep their end, where the directory is
        while ([title length] > 1 && [title sizeWithAttributes:a].width > NSWidth(r) - 2 * (CLOSE_X + CLOSE_W)) {
            title = [@"…" stringByAppendingString:[title substringFromIndex:2]];
        }
        s = [title sizeWithAttributes:a];
        [title drawAtPoint:NSMakePoint(floor(NSMidX(r) - s.width / 2), floor((BAR_H - s.height) / 2)) withAttributes:a];
        if ((NSInteger)i == _hoverClose)
            [@"✕" drawAtPoint:NSMakePoint(NSMinX(r) + CLOSE_X, floor((BAR_H - s.height) / 2))
                    withAttributes:@{ NSFontAttributeName: [NSFont fontWithName:@"Inter-Regular" size:11] ?: tabFont(),
                                      NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0 alpha:0.6] }];
    }
}

- (NSInteger)tabAtPoint:(NSPoint)p
{
    NSUInteger n = [_terms count];

    if (n < 2 || p.y >= BAR_H || p.y < 0)
        return -1;
    return MIN((NSInteger)(p.x / (NSWidth([self bounds]) / n)), (NSInteger)n - 1);
}

- (void)mouseMoved:(NSEvent *)event
{
    NSInteger i = [self tabAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    if (i != _hoverClose) {
        _hoverClose = i;
        [self setNeedsDisplayInRect:NSMakeRect(0, 0, NSWidth([self bounds]), BAR_H)];
    }
}

- (void)mouseExited:(NSEvent *)event
{
    _hoverClose = -1;
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSInteger i = [self tabAtPoint:p];
    CGFloat w;

    if (i < 0)
        return;
    w = NSWidth([self bounds]) / [_terms count];
    if (p.x - i * w >= CLOSE_X - 2 && p.x - i * w < CLOSE_X + CLOSE_W) {
        TerminalView *t = _terms[i];

        [t hangUp];
        [self removeTerm:t];
        return;
    }
    [self selectTab:i];
}

- (void)terminalViewDidExit:(TerminalView *)view
{
    [self removeTerm:view];
}

- (void)terminalViewTitleChanged:(TerminalView *)view
{
    if (view == [self currentTerm])
        [self updateTitle];
    [self setNeedsDisplayInRect:NSMakeRect(0, 0, NSWidth([self bounds]), BAR_H)];
}

- (void)terminalView:(TerminalView *)view wantsSize:(NSSize)size
{
    [[self window] setContentSize:NSMakeSize(size.width, size.height + [self barHeight])];
}

// a new tab opens at the window's current size, so the first one appearing takes room from the terminal
- (IBAction)newTab:(id)sender
{
    [self addTab];
    [self layoutTerms];
}

- (IBAction)closeTab:(id)sender
{
    TerminalView *t = [self currentTerm];

    [t hangUp];
    [self removeTerm:t];
}

- (IBAction)closeWindow:(id)sender
{
    for (TerminalView *t in [[_terms copy] autorelease])
        [t hangUp];
    [_owner terminalTabsDidClose:self];
}

- (IBAction)newWindow:(id)sender
{
    [_owner newWindow:sender];
}

- (IBAction)selectNextTab:(id)sender
{
    if ([_terms count] > 1)
        [self selectTab:(_current + 1) % [_terms count]];
}

- (IBAction)selectPreviousTab:(id)sender
{
    if ([_terms count] > 1)
        [self selectTab:(_current + [_terms count] - 1) % [_terms count]];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] == @selector(selectNextTab:) || [item action] == @selector(selectPreviousTab:))
        return [_terms count] > 1;
    return YES;
}

@end
