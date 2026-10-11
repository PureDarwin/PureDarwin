#import <AppKit/NSViewController.h>
#import <AppKit/NSNibLoading.h>
#import <AppKit/NSNib.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSStoryboard.h>
#import <objc/runtime.h>

@interface NSStoryboard(private)
-(NSString *)_pathForNibNamed:(NSString *)name;
@end

@implementation NSViewController

-initWithNibName:(NSString *)name bundle:(NSBundle *)bundle {
   _nibName=[name copy];
   _nibBundle=[bundle retain];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    _nibName=[[coder decodeObjectForKey:@"NSNibName"] copy];
    _title=[[coder decodeObjectForKey:@"NSTitle"] copy];
    NSString *bundleIdentifier=[coder decodeObjectForKey:@"NSNibBundleIdentifier"];
    if(bundleIdentifier!=nil)
     _nibBundle=[NSBundle bundleWithIdentifier:bundleIdentifier];
   }

   return self;
}

-(NSString *)nibName {
   return _nibName;
}

-(NSBundle *)nibBundle {
   return _nibBundle;
}

-(NSView *)view {
   if(_view==nil){
    [self loadView];
    if(_view!=nil)
     [self viewDidLoad];
   }

   return _view;
}

-(void)viewDidLoad {
}

-(void)viewWillAppear {
}

-(void)viewDidAppear {
}

-(void)viewWillDisappear {
}

-(void)viewDidDisappear {
}

-(NSString *)title {
   return _title;
}

-representedObject {
   return _representedObject;
}

-(void)setRepresentedObject:object {
   object=[object retain];
   [_representedObject release];
   _representedObject=object;
}

-(void)setTitle:(NSString *)value {
   value=[value retain];
   [_title release];
   _title=value;
}

-(void)setView:(NSView *)value {
   value=[value retain];
   [_view release];
   _view=value;
}

-(void)loadView {
   NSString *name=[self nibName];
   NSBundle *bundle=[self nibBundle];

   if(name==nil){
   // should pathForResource assert name for non-nil?
    [NSException raise:NSInvalidArgumentException format:@"-[%@ %s] nibName is nil",[self class],_cmd];
    return;
   }

   if(bundle==nil)
    bundle=[NSBundle mainBundle];

   // a storyboard's view controller loads its view from the compiled storyboard's nibs
   NSString     *path=[self storyboard] ? [[self storyboard] _pathForNibNamed:name] : [bundle pathForResource:name ofType:@"nib"];
   NSDictionary *nameTable=[NSDictionary dictionaryWithObject:self forKey:NSNibOwner];

   if(path==nil)
    NSLog(@"NSViewController unable to find nib named %@, bundle=%@",name,bundle);

   [bundle loadNibFile:path externalNameTable:nameTable withZone:NULL];
}

-(void)discardEditing {
   NSUnimplementedMethod();
}

-(BOOL)commitEditing {
   NSUnimplementedMethod();
   return NO;
}

-(void)commitEditingWithDelegate:delegate didCommitSelector:(SEL)didCommitSelector contextInfo:(void *)contextInfo {
   NSUnimplementedMethod();
}

@end

@implementation NSWindow (NSViewController)

static char NSWindowContentViewControllerKey;

-(NSViewController *)contentViewController {
   return objc_getAssociatedObject(self,&NSWindowContentViewControllerKey);
}

-(void)setContentViewController:(NSViewController *)controller {
   NSView *view=[controller view];

   objc_setAssociatedObject(self,&NSWindowContentViewControllerKey,controller,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   if(view==nil)
    return;
   [self setContentSize:[view frame].size];
   [self setContentView:view];
}

@end
