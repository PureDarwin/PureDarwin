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
    NSArray *_presentedItems;
    NSArray *_macBarItems;
    NSArray *_macAppItems;
    NSMenuItem *_macAppItem;
    int _macStyleChecked;
    NSUInteger _macSourceCount;
    NSMenuItem *_macSourceFirst;
}

- (void)setMenu:(NSMenu *)menu;
- (NSMenu *)menu;

- (void)setHorizontal:(BOOL)flag;
- (BOOL)isHorizontal;

/* The attributes item widths are measured with. Draw titles with these, or the
 * text will not fit the rect that was measured for it. */
- (NSDictionary *)itemAttributes;

/* The items this view lays out. Normally the menu's own, but a Mac-style menu
 * bar groups an application's loose commands under an application menu, and
 * that menu's view is handed its items directly. Items are referenced, never
 * moved: an NSMenuItem belongs to exactly one NSMenu and this must not
 * disturb the menu it came from. */
- (NSArray *)presentedItems;
- (void)setPresentedItems:(NSArray *)items;

- (void)setFont:(NSFont *)font;
- (NSFont *)font;

- (void)sizeToFit;
- (NSRect)rectOfItemAtIndex:(NSInteger)index;
- (NSInteger)indexOfItemAtPoint:(NSPoint)point;

- (void)setHighlightedItemIndex:(NSInteger)index;
- (NSInteger)highlightedItemIndex;

@end
