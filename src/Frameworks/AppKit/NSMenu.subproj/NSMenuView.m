#import <AppKit/NSMenuView.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSGraphicsStyle.h>
#import <AppKit/NSAttributedString.h>

NSString *const NSMenuDidBeginTrackingNotification = @"NSMenuDidBeginTrackingNotification";
NSString *const NSMenuDidEndTrackingNotification = @"NSMenuDidEndTrackingNotification";

#define ITEM_PAD_X   10.0
#define ITEM_PAD_Y    3.0
#define ARROW_WIDTH  12.0
#define SEPARATOR_H   6.0

@implementation NSMenuView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        _font = [[NSFont menuFontOfSize:0] retain];
        _highlightedIndex = -1;
        _openSubmenuIndex = -1;
    }
    return self;
}

- (void)dealloc
{
    [_submenuWindow release];
    [_submenuView release];
    [_font release];
    /* _menu belongs to NSView, which releases it. */
    [super dealloc];
}

/* Both orientations lay out from the top-left, which is what the item rect
 * arithmetic below assumes. */
- (BOOL)isFlipped
{
    return YES;
}

- (NSWindow *)window
{
    return [super window];
}

- (BOOL)isOpaque
{
    return [super isOpaque];
}

- (void)setMenu:(NSMenu *)menu
{
    if (menu == _menu) {
        return;
    }
    [self closeSubmenu];

    [super setMenu:menu];
    _highlightedIndex = -1;

    [self setNeedsDisplay:YES];
}

- (NSMenu *)menu
{
    return [super menu];
}

- (void)setHorizontal:(BOOL)flag
{
    _horizontal = flag;
    [self setNeedsDisplay:YES];
}

- (BOOL)isHorizontal
{
    return _horizontal;
}

- (void)setFont:(NSFont *)font
{
    [font retain];
    [_font release];
    _font = font;
    [self setNeedsDisplay:YES];
}

- (NSFont *)font
{
    return _font;
}

- (NSInteger)highlightedItemIndex
{
    return _highlightedIndex;
}

- (void)setHighlightedItemIndex:(NSInteger)index
{
    if (index != _highlightedIndex) {
        _highlightedIndex = index;
        [self setNeedsDisplay:YES];
    }
}

- (NSDictionary *)itemAttributes
{
    return [NSDictionary dictionaryWithObject:(_font != nil ? _font : [NSFont menuFontOfSize:0])
                                       forKey:NSFontAttributeName];
}

- (NSSize)sizeOfItem:(NSMenuItem *)item
{
    NSSize size;

    if ([item isSeparatorItem]) {
        size.width = SEPARATOR_H;
        size.height = SEPARATOR_H;
        return size;
    }

    NSString *title = [item title];

    size = [(title != nil ? title : @"") sizeWithAttributes:[self itemAttributes]];
    size.width += ITEM_PAD_X * 2.0;
    size.height += ITEM_PAD_Y * 2.0;

    if (!_horizontal && [item hasSubmenu]) {
        size.width += ARROW_WIDTH;
    }
    return size;
}

- (NSRect)rectOfItemAtIndex:(NSInteger)index
{
    NSArray *items = [_menu itemArray];

    if (_menu == nil || index < 0 || index >= (NSInteger)[items count]) {
        return NSZeroRect;
    }

    NSRect bounds = [self bounds];
    CGFloat offset = 0.0;

    for (NSInteger i = 0; i < index; i++) {
        NSSize size = [self sizeOfItem:[items objectAtIndex:i]];

        offset += _horizontal ? size.width : size.height;
    }

    NSSize size = [self sizeOfItem:[items objectAtIndex:index]];

    if (_horizontal) {
        return NSMakeRect(offset, 0.0, size.width, bounds.size.height);
    }
    return NSMakeRect(0.0, offset, bounds.size.width, size.height);
}

- (NSInteger)indexOfItemAtPoint:(NSPoint)point
{
    NSInteger count = (NSInteger)[[_menu itemArray] count];

    for (NSInteger i = 0; i < count; i++) {
        if (NSPointInRect(point, [self rectOfItemAtIndex:i])) {
            return i;
        }
    }
    return -1;
}

- (void)sizeToFit
{
    NSArray *items = [_menu itemArray];
    NSSize total = NSMakeSize(0.0, 0.0);

    for (NSMenuItem *item in items) {
        NSSize size = [self sizeOfItem:item];

        if (_horizontal) {
            total.width += size.width;
            total.height = MAX(total.height, size.height);
        }
        else {
            total.width = MAX(total.width, size.width);
            total.height += size.height;
        }
    }

    NSRect frame = [self frame];

    frame.size = total;
    [self setFrame:frame];
}

- (void)drawRect:(NSRect)dirtyRect
{
    NSGraphicsStyle *style = [self graphicsStyle];
    NSArray *items = [_menu itemArray];
    NSInteger count = (NSInteger)[items count];

    if (_horizontal) {
        [style drawMenuBarBackgroundInRect:[self bounds]];
    }
    else {
        [style drawMenuWindowBackgroundInRect:[self bounds]];
    }

    for (NSInteger i = 0; i < count; i++) {
        NSMenuItem *item = [items objectAtIndex:i];
        NSRect rect = [self rectOfItemAtIndex:i];

        if (!NSIntersectsRect(dirtyRect, rect)) {
            continue;
        }

        if ([item isSeparatorItem]) {
            [style drawMenuSeparatorInRect:rect];
            continue;
        }

        BOOL selected = (i == _highlightedIndex);
        BOOL enabled = [item isEnabled];

        if (selected) {
            if (_horizontal) {
                [style drawMenuBarItemBorderInRect:rect hover:YES selected:YES];
            }
            else {
                [style drawMenuSelectionInRect:rect enabled:enabled];
            }
        }

        NSRect textRect = NSInsetRect(rect, ITEM_PAD_X, ITEM_PAD_Y);
        NSString *title = [item title];

        [style drawMenuItemText:(title != nil ? title : @"")
                         inRect:textRect
                        enabled:enabled
                       selected:selected];

        if (!_horizontal && [item hasSubmenu]) {
            NSRect arrow = rect;

            arrow.origin.x = NSMaxX(rect) - ARROW_WIDTH;
            arrow.size.width = ARROW_WIDTH;
            [style drawMenuBranchArrowInRect:arrow enabled:enabled selected:selected];
        }
    }
}

/* A submenu is a vertical view in its own borderless window above everything
 * else; the menu bar itself is a layer-shell surface and cannot host it. */
- (void)openSubmenuAtIndex:(NSInteger)index
{
    if (index == _openSubmenuIndex) {
        return;
    }
    [self closeSubmenu];

    NSMenuItem *item = [[_menu itemArray] objectAtIndex:index];
    NSMenu *submenu = [item submenu];

    if (submenu == nil) {
        return;
    }

    NSMenuView *view = [[NSMenuView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 10.0, 10.0)];

    [view setFont:_font];
    [view setMenu:submenu];
    [view sizeToFit];
    view->_supermenuView = self;

    NSRect itemRect = [self rectOfItemAtIndex:index];
    NSPoint origin = [self convertPoint:NSMakePoint(NSMinX(itemRect), NSMaxY(itemRect))
                                 toView:nil];

    origin = [[self window] convertBaseToScreen:origin];

    NSRect frame = [view frame];

    frame.origin = origin;
    frame.origin.y -= frame.size.height;

    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:NSBorderlessWindowMask
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];

    [window setLevel:NSPopUpMenuWindowLevel];
    [[window contentView] addSubview:view];
    [window orderFront:nil];

    _submenuWindow = window;
    _submenuView = view;
    _openSubmenuIndex = index;
}

- (void)closeSubmenu
{
    if (_submenuWindow != nil) {
        [_submenuView closeSubmenu];
        [_submenuWindow orderOut:nil];
        [_submenuWindow release];
        _submenuWindow = nil;
    }
    [_submenuView release];
    _submenuView = nil;
    _openSubmenuIndex = -1;
}

/* Maps a screen point into whichever view currently owns it: the open submenu
 * if the pointer is inside it, otherwise this view. */
- (NSMenuView *)viewForScreenPoint:(NSPoint)screenPoint index:(NSInteger *)indexOut
{
    if (_submenuView != nil) {
        NSPoint base = [_submenuWindow convertScreenToBase:screenPoint];
        NSPoint local = [_submenuView convertPoint:base fromView:nil];

        if (NSPointInRect(local, [_submenuView bounds])) {
            *indexOut = [_submenuView indexOfItemAtPoint:local];
            return _submenuView;
        }
    }

    NSPoint base = [[self window] convertScreenToBase:screenPoint];
    NSPoint local = [self convertPoint:base fromView:nil];

    *indexOut = [self indexOfItemAtPoint:local];
    return self;
}

- (void)mouseDown:(NSEvent *)event
{
    if (_menu == nil || _tracking) {
        return;
    }

    _tracking = YES;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSMenuDidBeginTrackingNotification
                      object:_menu];

    NSMenuItem *chosen = nil;

    for (;;) {
        NSPoint screenPoint = [[self window] convertBaseToScreen:[event locationInWindow]];
        NSInteger index = -1;
        NSMenuView *view = [self viewForScreenPoint:screenPoint index:&index];

        [view setHighlightedItemIndex:index];

        if (view == self) {
            if (index >= 0) {
                NSMenuItem *item = [[_menu itemArray] objectAtIndex:index];

                if ([item hasSubmenu] && [item isEnabled]) {
                    [self openSubmenuAtIndex:index];
                }
                else {
                    [self closeSubmenu];
                }
            }
        }

        if ([event type] == NSLeftMouseUp) {
            if (index >= 0) {
                NSArray *items = [[view menu] itemArray];

                if (index < (NSInteger)[items count]) {
                    NSMenuItem *item = [items objectAtIndex:index];

                    if ([item isEnabled] && ![item isSeparatorItem] && ![item hasSubmenu]) {
                        chosen = item;
                    }
                }
            }
            break;
        }

        event = [NSApp nextEventMatchingMask:NSLeftMouseUpMask | NSLeftMouseDraggedMask |
                                             NSMouseMovedMask
                                   untilDate:[NSDate distantFuture]
                                      inMode:NSEventTrackingRunLoopMode
                                     dequeue:YES];
        if (event == nil) {
            break;
        }
    }

    [self closeSubmenu];
    [self setHighlightedItemIndex:-1];
    _tracking = NO;

    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSMenuDidEndTrackingNotification
                      object:_menu];

    /* Sent after tracking ends so the action sees a settled menu. */
    if (chosen != nil && [chosen action] != NULL) {
        [NSApp sendAction:[chosen action] to:[chosen target] from:chosen];
    }
}

- (void)mouseUp:(NSEvent *)event
{
}

@end
