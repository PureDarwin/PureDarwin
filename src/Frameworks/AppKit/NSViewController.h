#import <AppKit/NSResponder.h>

#import <AppKit/NSWindow.h>

@class NSView;

@interface NSViewController : NSResponder {
    NSString *_nibName;
    NSBundle *_nibBundle;
    id _representedObject;
    NSString *_title;
    NSView *_view;
}

- initWithNibName:(NSString *)name bundle:(NSBundle *)bundle;

- (NSString *)nibName;
- (NSBundle *)nibBundle;

- (NSView *)view;
- (NSString *)title;
- representedObject;

- (void)setRepresentedObject:object;

- (void)setTitle:(NSString *)value;

- (void)setView:(NSView *)value;

- (void)loadView;

// called once the view is loaded (by -view through -loadView, or by a storyboard)
- (void)viewDidLoad;
- (void)viewWillAppear;
- (void)viewDidAppear;
- (void)viewWillDisappear;
- (void)viewDidDisappear;

- (void)discardEditing;

- (BOOL)commitEditing;
- (void)commitEditingWithDelegate:delegate didCommitSelector:(SEL)didCommitSelector contextInfo:(void *)contextInfo;

@end

// a window whose content view is a view controller's view, the window takes the view's size
@interface NSWindow (NSViewController)
- (NSViewController *)contentViewController;
- (void)setContentViewController:(NSViewController *)controller;
@end
