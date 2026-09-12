#import <AppKit/NSMenuView.h>
#import <AppKit/NSInterfaceStyle.h>
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
    [_presentedItems release];
    [self _invalidatePresentation];
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

/* Mac-style menu bars put an application's own commands under one application
 * menu instead of spreading them across the bar. GNUstep's AppKit does this in
 * its Macintosh interface style; ours has to, or an app like Workspace - whose
 * main menu is a flat list of commands, as is normal for GNUstep - fills the
 * whole bar and overflows the screen.
 *
 * Items are only ever *referenced* here. An NSMenuItem belongs to exactly one
 * NSMenu, so moving or copying them would either mutate the application's real
 * menu or desynchronise from it. */
- (BOOL)_groupsIntoApplicationMenu
{
    if (!_horizontal || _menu == nil) {
        return NO;
    }
    /* Asked once per item per redraw through -presentedItems, so the answer is
     * kept rather than recomputed. */
    if (_macStyleChecked == 0) {
        _macStyleChecked = (NSInterfaceStyleForKey(@"NSMenuInterfaceStyle", nil)
                            == NSMacintoshInterfaceStyle) ? 1 : -1;
    }
    return _macStyleChecked > 0;
}

- (void)_invalidatePresentation
{
    [_macBarItems release];
    _macBarItems = nil;
    [_macAppItems release];
    _macAppItems = nil;
    [_macAppItem release];
    _macAppItem = nil;
}

- (void)_buildMacPresentation
{
    NSMutableArray *bar = [[NSMutableArray alloc] init];
    NSMutableArray *app = [[NSMutableArray alloc] init];

    for (NSMenuItem *item in [_menu itemArray]) {
        if ([item hasSubmenu]) {
            [bar addObject:item];
        }
        else {
            [app addObject:item];
        }
    }

    _macAppItems = app;
    if ([app count] != 0) {
        /* The menu's own title names the application it belongs to. Do NOT
         * fall back to the process name: a menu bar shows *other*
         * applications' menus, so the process here is the menu bar itself -
         * which is how an application menu ended up labelled "Menu". With no
         * title there is nothing truthful to call it, so leave it blank and
         * let the item read as an anonymous application menu. */
        NSString *title = [_menu title];

        if (title == nil) {
            title = @"";
        }
        /* Stands in for the application menu. Deliberately given no submenu:
         * its contents are _macAppItems, which still belong to _menu, and
         * -openSubmenuAtIndex: hands them straight to the submenu view. */
        _macAppItem = [[NSMenuItem alloc] initWithTitle:(title ?: @"")
                                                action:NULL
                                         keyEquivalent:@""];
        [bar insertObject:_macAppItem atIndex:0];
    }
    _macBarItems = bar;
}

- (NSArray *)presentedItems
{
    if (_presentedItems != nil) {
        return _presentedItems;
    }
    if (![self _groupsIntoApplicationMenu]) {
        return [_menu itemArray];
    }
    if (_macBarItems == nil) {
        [self _buildMacPresentation];
    }
    return _macBarItems;
}

- (void)setPresentedItems:(NSArray *)items
{
    if (items == _presentedItems) {
        return;
    }
    [_presentedItems release];
    _presentedItems = [items retain];
    [self setNeedsDisplay:YES];
}

- (Margins)menuItemTextMargins
{
    Margins margins = {0};
    return margins;
}

- (void)setMenu:(NSMenu *)menu
{
    if (menu == _menu) {
        return;
    }
    [self closeSubmenu];

    [super setMenu:menu];
    [self _invalidatePresentation];
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

    /* -itemAttributes is the single source of truth for menu text metrics:
     * whoever draws the title must measure with the same dictionary, or the
     * item comes out narrower than its own text and the title is clipped. */
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
    NSArray *items = [self presentedItems];

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
    NSInteger count = (NSInteger)[[self presentedItems] count];

    for (NSInteger i = 0; i < count; i++) {
        if (NSPointInRect(point, [self rectOfItemAtIndex:i])) {
            return i;
        }
    }
    return -1;
}

- (void)sizeToFit
{
    NSArray *items = [self presentedItems];
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
    NSArray *items = [self presentedItems];
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

    NSMenuItem *item = [[self presentedItems] objectAtIndex:index];
    NSMenu *submenu = [item submenu];
    NSArray *loose = (item == _macAppItem) ? _macAppItems : nil;

    if (submenu == nil && loose == nil) {
        return;
    }

    NSMenuView *view = [[NSMenuView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 10.0, 10.0)];

    [view setFont:_font];
    /* The application menu's items still belong to _menu, so the view is given
     * the menu they live in and told which of them to show. */
    [view setMenu:(submenu != nil) ? submenu : _menu];
    if (loose != nil) {
        [view setPresentedItems:loose];
    }
    [view sizeToFit];
    view->_supermenuView = self;

    /* An empty menu sizes to nothing, and a zero-sized window gets no surface:
     * it would look exactly like a menu that failed to open. */
    if (NSIsEmptyRect([view frame])) {
        [view release];
        return;
    }

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
    BOOL sticky = NO;

    for (;;) {
        /* Once a submenu is open the pointer is over its window, so the event's
         * coordinates are relative to that one, not to this view's. */
        NSWindow *eventWindow = [event window];

        if (eventWindow == nil) {
            eventWindow = [self window];
        }

        NSPoint screenPoint = [eventWindow convertBaseToScreen:[event locationInWindow]];
        NSInteger index = -1;
        NSMenuView *view = [self viewForScreenPoint:screenPoint index:&index];

        [view setHighlightedItemIndex:index];

        if (view == self) {
            if (index >= 0) {
                NSMenuItem *item = [[self presentedItems] objectAtIndex:index];

                if ([item hasSubmenu] && [item isEnabled]) {
                    [self openSubmenuAtIndex:index];
                }
                else {
                    [self closeSubmenu];
                }
            }
        }

        NSEventType type = [event type];

        if (type == NSLeftMouseUp || (sticky && type == NSLeftMouseDown)) {
            NSMenuItem *hit = nil;

            if (index >= 0) {
                NSArray *items = [view presentedItems];

                if (index < (NSInteger)[items count]) {
                    NSMenuItem *item = [items objectAtIndex:index];

                    if ([item isEnabled] && ![item isSeparatorItem] && ![item hasSubmenu]) {
                        hit = item;
                    }
                }
            }
            if (hit != nil) {
                chosen = hit;
                break;
            }
            /* Nothing actionable under the cursor: the first release opens the
             * menu for good, anything after that dismisses it. */
            if (type == NSLeftMouseUp && !sticky) {
                sticky = YES;
            }
            else {
                break;
            }
        }

        NSUInteger mask = NSLeftMouseUpMask | NSLeftMouseDraggedMask | NSMouseMovedMask;

        if (sticky) {
            mask |= NSLeftMouseDownMask;
        }
        event = [NSApp nextEventMatchingMask:mask
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
