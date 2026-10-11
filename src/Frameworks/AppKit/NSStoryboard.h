#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSViewController.h>
#import <AppKit/NSWindowController.h>

@class NSBundle, NSString, NSViewController, NSWindowController;

typedef NSString *NSStoryboardName;
typedef NSString *NSStoryboardSceneIdentifier;

// a storyboard compiled by ibtool (NAME.storyboardc, an Info.plist naming each scene's nib) or read from Interface Builder XML (NAME.storyboard):
// window and view controllers, window and view trees, content relationships and outlets. XML menus and other scene objects are skipped
@interface NSStoryboard : NSObject

+ (NSStoryboard *)mainStoryboard;
+ (instancetype)storyboardWithName:(NSStoryboardName)name bundle:(NSBundle *)bundle;

- (id)instantiateInitialController;
- (id)instantiateControllerWithIdentifier:(NSStoryboardSceneIdentifier)identifier;

@end

// the storyboard a controller came from (set by the scene's nib)
@interface NSViewController (NSStoryboard)
- (NSStoryboard *)storyboard;
- (void)setStoryboard:(NSStoryboard *)storyboard;
@end

@interface NSWindowController (NSStoryboard)
- (NSStoryboard *)storyboard;
- (void)setStoryboard:(NSStoryboard *)storyboard;
@end
