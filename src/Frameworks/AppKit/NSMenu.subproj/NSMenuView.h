#import <AppKit/NSView.h>

@class NSMenu, NSMenuItem, NSFont, NSWindow;

APPKIT_EXPORT NSString *const NSMenuDidBeginTrackingNotification;
APPKIT_EXPORT NSString *const NSMenuDidEndTrackingNotification;

/* The menu being displayed is NSView's own _menu ivar: -setMenu:/-menu are
 * overridden below to mean "the menu this view draws" rather than a contextual
 * menu, which is the sense a menu view is written against. */
@interface NSMenuView : NSView {
    NSFont *_font;
    BOOL _horizontal;
    NSInteger _highlightedIndex;
    NSInteger _openSubmenuIndex;
    NSWindow *_submenuWindow;
    NSMenuView *_submenuView;
    NSMenuView *_supermenuView;
    BOOL _tracking;
}

- (void)setMenu:(NSMenu *)menu;
- (NSMenu *)menu;

- (void)setHorizontal:(BOOL)flag;
- (BOOL)isHorizontal;

- (void)setFont:(NSFont *)font;
- (NSFont *)font;

- (void)sizeToFit;
- (NSRect)rectOfItemAtIndex:(NSInteger)index;
- (NSInteger)indexOfItemAtPoint:(NSPoint)point;

- (void)setHighlightedItemIndex:(NSInteger)index;
- (NSInteger)highlightedItemIndex;

@end
