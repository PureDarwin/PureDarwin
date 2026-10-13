#import <AppKit/NSAppearance.h>
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
#import <AppKit/NSBezierPath.h>

NSString *const NSMenuDidBeginTrackingNotification = @"NSMenuDidBeginTrackingNotification";
NSString *const NSMenuDidEndTrackingNotification = @"NSMenuDidEndTrackingNotification";

#define ITEM_PAD_X   10.0
#define ITEM_PAD_Y    3.0
#define ARROW_WIDTH  12.0
#define SEPARATOR_H  11.0

// a vertical menu is a light panel with a 5px inset, rounded accent highlights and a shortcut column
#define PANEL_INSET   5.0
#define KEY_GAP      24.0
#define HIGHLIGHT_R   4.0
#define PANEL_R       8.0
// a clear margin round the panel holds its drop shadow, the demo's 0 10px 30px at a quarter black
#define SHADOW_M     16.0
#define MENU_MIN_W  210.0

// the Mac glyphs for a shortcut: control, option, shift, command, then the key
static NSString *pdKeyEquivalentGlyphs(NSMenuItem *item)
{
    NSString *key = [item keyEquivalent];
    NSUInteger mask = [item keyEquivalentModifierMask];
    NSMutableString *out = [NSMutableString string];
    unichar c;

    if ([key length] == 0)
        return @"";
    c = [key characterAtIndex:0];
    if (![[key lowercaseString] isEqualToString:key])
        mask |= NSShiftKeyMask;
    if (mask & NSControlKeyMask)
        [out appendString:@"\u2303"];
    if (mask & NSAlternateKeyMask)
        [out appendString:@"\u2325"];
    if (mask & NSShiftKeyMask)
        [out appendString:@"\u21e7"];
    if (mask & NSCommandKeyMask)
        [out appendString:@"\u2318"];
    switch (c) {
    case '\r': [out appendString:@"\u21a9"]; break;
    case '\t': [out appendString:@"\u21e5"]; break;
    case 0x1b: [out appendString:@"\u238b"]; break;
    case 0x7f: case '\b': [out appendString:@"\u232b"]; break;
    case ' ': [out appendString:@"Space"]; break;
    case NSUpArrowFunctionKey: [out appendString:@"\u2191"]; break;
    case NSDownArrowFunctionKey: [out appendString:@"\u2193"]; break;
    case NSLeftArrowFunctionKey: [out appendString:@"\u2190"]; break;
    case NSRightArrowFunctionKey: [out appendString:@"\u2192"]; break;
    default: [out appendString:[key uppercaseString]]; break;
    }
    return out;
}

static BOOL pdCancelTracking;
static NSArray *pdExitRects;
static NSInteger pdExitIndex = -1;

@implementation NSMenu (PDCancelTracking)

// closes whatever menu is being tracked, for the menu bar when another app takes the front
+ (void)_pdCancelTracking
{
    pdCancelTracking = YES;
}

// screen rects that end the next tracking when the pointer enters one, the menu bar's other titles
+ (void)_pdSetTrackingExitRects:(NSArray *)rects
{
    [pdExitRects release];
    pdExitRects = [rects copy];
    pdExitIndex = -1;
}

// which exit rect ended the last tracking, -1 when none did
+ (NSInteger)_pdTrackingExitIndex
{
    return pdExitIndex;
}

@end

@implementation NSMenuView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        // Inter at 13 like the rest of the desktop, the menu font type alone gives 14pt
        _font = [[NSFont fontWithName:@"Inter-Regular" size:13] retain];
        if (_font == nil)
            _font = [[NSFont menuFontOfSize:13] retain];
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
    if (!_horizontal)
        return NO;
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
    [_macSourceFirst release];
    _macSourceFirst = nil;
}

- (void)_buildMacPresentation
{
    NSMutableArray *bar = [[NSMutableArray alloc] init];
    NSMutableArray *app = [[NSMutableArray alloc] init];
    NSArray *items = [_menu itemArray];
    NSInteger count = (NSInteger)[items count];
    NSMenuItem *owner = nil;
    NSInteger firstLoose = count;
    NSInteger tailStart = count;
    NSInteger i;

    /* The application menu's contents sit inline between its own title and the
     * run of titles that ends the menu, so the split is positional: the
     * trailing run of submenu items is the rest of the bar, and everything
     * from the first loose item up to it belongs to the application - even
     * items that have submenus of their own, such as Services. */
    for (i = 0; i < count; i++) {
        if (![[items objectAtIndex:i] hasSubmenu]) {
            firstLoose = i;
            break;
        }
    }
    for (i = count - 1; i >= 0 && [[items objectAtIndex:i] hasSubmenu]; i--) {
        tailStart = i;
    }
    if (tailStart < firstLoose) {
        tailStart = count;
    }
    if (firstLoose > 0 && firstLoose < count) {
        owner = [items objectAtIndex:firstLoose - 1];
    }

    for (i = 0; i < count; i++) {
        NSMenuItem *item = [items objectAtIndex:i];

        if (i < firstLoose || i >= tailStart) {
            [bar addObject:item];
        }
        else {
            [app addObject:item];
        }
    }

    _macAppItems = app;
    if ([app count] != 0 && owner != nil) {
        /* An existing title already stands for the application, so adding one
         * would both duplicate it and leave a blank slot in the bar. */
        _macAppItem = [owner retain];
    }
    else if ([app count] != 0) {
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
    _macSourceCount = (NSUInteger)count;
    _macSourceFirst = (count > 0) ? [[items objectAtIndex:0] retain] : nil;
}

- (NSArray *)presentedItems
{
    if (_presentedItems != nil) {
        return _presentedItems;
    }
    if (![self _groupsIntoApplicationMenu]) {
        return [_menu itemArray];
    }
    // apps may fill the menu in after setMainMenu:, so rebuild when the item count changes
    NSArray *source = [_menu itemArray];

    if (_macBarItems != nil &&
        (_macSourceCount != [source count] ||
         ([source count] != 0 && [source objectAtIndex:0] != _macSourceFirst))) {
        [self _invalidatePresentation];
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
    if (!_horizontal) {
        NSString *keys = pdKeyEquivalentGlyphs(item);

        if ([keys length] > 0)
            size.width += KEY_GAP + [keys sizeWithAttributes:[self itemAttributes]].width;
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
    return NSMakeRect(SHADOW_M + PANEL_INSET, SHADOW_M + PANEL_INSET + offset,
                      bounds.size.width - 2 * (SHADOW_M + PANEL_INSET), size.height);
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

    if (!_horizontal) {
        total.width = MAX(total.width + 2 * PANEL_INSET, MENU_MIN_W);
        total.width += 2 * SHADOW_M;
        total.height += 2 * PANEL_INSET + 2 * SHADOW_M;
    }
    // round up to whole pixels so the window's truncation never clips the panel edge
    total.width = ceil(total.width);
    total.height = ceil(total.height);
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
        // the window is clear, so the panel's rounded corners and shadow show what is behind it
        NSRect box = NSInsetRect([self bounds], SHADOW_M, SHADOW_M);

        // back to transparent first, NSRectFill copies, or every redraw would stack the translucent layers
        [[NSColor clearColor] setFill];
        NSRectFill([self bounds]);
        NSBezierPath *panel = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(box, 0.5, 0.5)
                                                              xRadius:PANEL_R yRadius:PANEL_R];

        // stacked faint rounded rects make the soft shadow, dropped a few pixels as in the demo
        for (int k = 8; k >= 1; k--) {
            NSRect r = NSOffsetRect(NSInsetRect(box, -k * 1.5, -k * 1.5), 0, 5);

            [[NSColor colorWithCalibratedWhite:0 alpha:0.018] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:r xRadius:PANEL_R + k yRadius:PANEL_R + k] fill];
        }
        // the demo's 92%, the compositor blurs what shows through
        BOOL dark = [[NSAppearance currentDrawingAppearance] _isDark];

        [(dark ? [NSColor colorWithCalibratedRed:0.172 green:0.172 blue:0.18 alpha:0.92]
               : [NSColor colorWithCalibratedRed:0.98 green:0.969 blue:0.961 alpha:0.92]) setFill];
        [panel fill];
        [[NSColor colorWithCalibratedWhite:dark ? 1 : 0 alpha:0.12] setStroke];
        [panel setLineWidth:1];
        [panel stroke];
    }

    for (NSInteger i = 0; i < count; i++) {
        NSMenuItem *item = [items objectAtIndex:i];
        NSRect rect = [self rectOfItemAtIndex:i];

        if (!NSIntersectsRect(dirtyRect, rect)) {
            continue;
        }

        if (_horizontal) {
            [self _drawBarItem:item inRect:rect index:i style:style];
            continue;
        }
        [self _drawMenuItem:item inRect:rect selected:(i == _highlightedIndex)];
    }
}

- (void)_drawBarItem:(NSMenuItem *)item inRect:(NSRect)rect index:(NSInteger)i style:(NSGraphicsStyle *)style
{
    BOOL selected = (i == _highlightedIndex);
    BOOL enabled = [item isEnabled];
    NSString *title = [item title];

    if ([item isSeparatorItem]) {
        [style drawMenuSeparatorInRect:rect];
        return;
    }
    if (selected) {
        [style drawMenuBarItemBorderInRect:rect hover:YES selected:YES];
    }
    [style drawMenuItemText:(title != nil ? title : @"")
                     inRect:NSInsetRect(rect, ITEM_PAD_X, ITEM_PAD_Y)
                    enabled:enabled
                   selected:selected];
}

// one row of a vertical menu: highlight, title, shortcut and submenu arrow
- (void)_drawMenuItem:(NSMenuItem *)item inRect:(NSRect)rect selected:(BOOL)selected
{
    NSColor *ink, *keyInk;
    NSDictionary *attrs, *keyAttrs;
    NSString *title = [item title], *keys;
    NSRect textRect;
    BOOL enabled = [item isEnabled];
    BOOL dark = [[NSAppearance currentDrawingAppearance] _isDark];
    CGFloat w;

    if ([item isSeparatorItem]) {
        // solid, NSRectFill copies pixels, so an alpha colour would cut a see-through line in the panel
        [(dark ? [NSColor colorWithCalibratedWhite:0.3 alpha:1] : [NSColor colorWithCalibratedRed:0.862 green:0.853 blue:0.846 alpha:1]) setFill];
        NSRectFill(NSMakeRect(NSMinX(rect) + 3, floor(NSMidY(rect)), NSWidth(rect) - 6, 1));
        return;
    }
    selected = selected && enabled;
    if (selected) {
        [[NSColor controlAccentColor] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:rect xRadius:HIGHLIGHT_R yRadius:HIGHLIGHT_R] fill];
    }

    // the demo's #eee text and #777 disabled in the dark
    CGFloat text = dark ? 0.93 : 0.1;

    ink = selected ? [NSColor whiteColor] : enabled ? [NSColor colorWithCalibratedWhite:text alpha:1] : [NSColor colorWithCalibratedWhite:dark ? 0.47 : 0.67 alpha:1];
    keyInk = selected ? [NSColor colorWithCalibratedWhite:1 alpha:0.75] : [NSColor colorWithCalibratedWhite:text alpha:enabled ? 0.5 : 0.3];
    attrs = @{ NSFontAttributeName : _font, NSForegroundColorAttributeName : ink };
    keyAttrs = @{ NSFontAttributeName : _font, NSForegroundColorAttributeName : keyInk };

    textRect = NSInsetRect(rect, ITEM_PAD_X, ITEM_PAD_Y);
    // a checked item carries its mark in the left padding, the title stays in line with the rest
    if ([item state] == NSOnState) {
        w = [@"\u2713" sizeWithAttributes:attrs].width;
        [@"\u2713" drawInRect:NSMakeRect(NSMinX(textRect) - w - 2, NSMinY(textRect), w + 1, NSHeight(textRect)) withAttributes:attrs];
    }
    [(title != nil ? title : @"") drawInRect:textRect withAttributes:attrs];

    keys = pdKeyEquivalentGlyphs(item);
    if ([keys length] > 0) {
        w = [keys sizeWithAttributes:keyAttrs].width;
        [keys drawInRect:NSMakeRect(NSMaxX(textRect) - w, NSMinY(textRect), w + 1, NSHeight(textRect)) withAttributes:keyAttrs];
    }
    if ([item hasSubmenu]) {
        [@"\u203a" drawInRect:NSMakeRect(NSMaxX(rect) - ARROW_WIDTH, NSMinY(textRect), ARROW_WIDTH, NSHeight(textRect))
               withAttributes:attrs];
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
        /* The application menu shows its own items followed by the ones that
         * were listed loose under its title. */
        NSMutableArray *combined = [NSMutableArray array];

        if (submenu != nil) {
            [combined addObjectsFromArray:[submenu itemArray]];
        }
        [combined addObjectsFromArray:loose];
        [view setPresentedItems:combined];
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
    NSMenuItem *chosen;

    if (_menu == nil || _tracking) {
        return;
    }
    chosen = [self _trackFromEvent:event];

    // sent after tracking ends so the action sees a settled menu
    if (chosen != nil && [chosen action] != NULL) {
        [NSApp sendAction:[chosen action] to:[chosen target] from:chosen];
    }
}

// runs the menu until a pick, a click on nothing or the app losing focus, and returns the pick
- (NSMenuItem *)_trackFromEvent:(NSEvent *)event
{
    NSMenuItem *chosen = nil;
    // a menu opened by sliding onto its title is already open, the next click anywhere else closes it
    BOOL sticky = ([event type] != NSLeftMouseDown);
    // a quick click leaves the menu open, only a drag-and-release picks on the way up
    NSTimeInterval opened = [event timestamp];
    BOOL dragged = NO;

    // only an activation that arrives while this menu is open cancels it
    pdCancelTracking = NO;
    _tracking = YES;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSMenuDidBeginTrackingNotification
                      object:_menu];

    for (;;) {
        // once a submenu is open the pointer is over its window, so the event's
        // coordinates are relative to that one, not to this view's
        NSWindow *eventWindow = [event window];

        // motion over another of our windows can arrive without its window set, find it by number
        if (eventWindow == nil && [event windowNumber] != 0) {
            eventWindow = [NSApp windowWithWindowNumber:[event windowNumber]];
        }
        if (eventWindow == nil) {
            eventWindow = [self window];
        }

        NSPoint screenPoint = [eventWindow convertBaseToScreen:[event locationInWindow]];
        NSInteger index = -1;
        NSUInteger exit;

        for (exit = 0; exit < [pdExitRects count]; exit++) {
            if (NSPointInRect(screenPoint, [[pdExitRects objectAtIndex:exit] rectValue]))
                break;
        }
        if (exit < [pdExitRects count]) {
            pdExitIndex = exit;
            break;
        }
        NSMenuView *view = [self viewForScreenPoint:screenPoint index:&index];

        [view setHighlightedItemIndex:index];
        // the run loop is not redrawing while we track, so menus repaint themselves here
        [[self window] displayIfNeeded];
        [_submenuWindow displayIfNeeded];

        if (view == self) {
            if (index >= 0) {
                NSMenuItem *item = [[self presentedItems] objectAtIndex:index];

                // the application menu's slot has no submenu of its own, its
                // contents are the loose items
                BOOL opens = ([item hasSubmenu] || item == _macAppItem);

                if (opens && [item isEnabled]) {
                    [self openSubmenuAtIndex:index];
                }
                else {
                    [self closeSubmenu];
                }
            }
        }

        NSEventType type = [event type];

        if (type == NSLeftMouseDragged)
            dragged = YES;
        if (type == NSLeftMouseUp && !sticky && !dragged && [event timestamp] - opened < 0.4) {
            sticky = YES;
            type = NSMouseMoved;
        }
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
            // nothing actionable under the cursor: the first release opens the
            // menu for good, anything after that dismisses it
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
        // a click in another app reaches us only as lost focus, so wake up to check for it
        for (event = nil; event == nil;) {
            event = [NSApp nextEventMatchingMask:mask
                                       untilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]
                                          inMode:NSEventTrackingRunLoopMode
                                         dequeue:YES];
            if (event == nil) {
                // a click in another app never reaches us, its owner says so instead through +_pdCancelTracking
                [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate date]];
                if (pdCancelTracking)
                    break;
            }
        }
        if (event == nil) {
            break;
        }
    }

    [self closeSubmenu];
    [self setHighlightedItemIndex:-1];
    _tracking = NO;
    pdCancelTracking = NO;
    [pdExitRects release];
    pdExitRects = nil;

    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSMenuDidEndTrackingNotification
                      object:_menu];
    return chosen;
}

- (void)mouseUp:(NSEvent *)event
{
}

@end
