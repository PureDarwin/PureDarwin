#import <AppKit/NSStoryboard.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSControl.h>
#import <AppKit/NSSlider.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSImageView.h>
#import <AppKit/NSView.h>
#import <AppKit/NSViewController.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSWindowController.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSXMLDocument.h>
#import <Foundation/NSXMLElement.h>
#import <AppKit/NSNib.h>
#import <AppKit/NSNibLoading.h>
#import <objc/runtime.h>

// one instantiation: the objects made so far by Interface Builder id, and the connections still to make
@interface NSStoryboardInstantiation : NSObject {
@public
    NSMutableDictionary *_objects;
    NSMutableArray *_outlets;
    NSMutableArray *_actions;
}
@end

@implementation NSStoryboardInstantiation

-init {
    _objects=[NSMutableDictionary new];
    _outlets=[NSMutableArray new];
    _actions=[NSMutableArray new];
    return self;
}

-(void)dealloc {
    [_objects release];
    [_outlets release];
    [_actions release];
    [super dealloc];
}

@end

// a compiled scene's nib File's Owner: the nib hands it the scene's controller
@interface NSStoryboardSceneOwner : NSObject {
@public
    id _sceneController;
}
@end

@implementation NSStoryboardSceneOwner

-(void)setSceneController:controller {
    [controller retain];
    [_sceneController release];
    _sceneController=controller;
}

-(void)dealloc {
    [_sceneController release];
    [super dealloc];
}

@end

@implementation NSStoryboard {
    NSXMLDocument *_document;
    NSMutableDictionary *_elements;
    NSString *_name;
    NSBundle *_bundle;
    // compiled: the .storyboardc directory and its Info.plist
    NSString *_compiledPath;
    NSDictionary *_compiledInfo;
}

static NSStoryboard *mainStoryboard=nil;

+(NSStoryboard *)mainStoryboard {
    if(mainStoryboard==nil){
        NSBundle *bundle=[NSBundle mainBundle];
        NSString *name=[[bundle infoDictionary] objectForKey:@"NSMainStoryboardFile"];

        if(name!=nil)
            mainStoryboard=[[self storyboardWithName:name bundle:bundle] retain];
    }
    return mainStoryboard;
}

static NSString *attribute(NSXMLElement *element,NSString *name) {
    return [[element attributeForName:name] stringValue];
}

static BOOL boolAttribute(NSXMLElement *element,NSString *name) {
    return [attribute(element,name) isEqualToString:@"YES"];
}

// the child element with key="KEY"
static NSXMLElement *keyedChild(NSXMLElement *element,NSString *key) {
    for(NSXMLNode *child in [element children])
        if([child kind]==NSXMLElementKind && [attribute((NSXMLElement *)child,@"key") isEqualToString:key])
            return (NSXMLElement *)child;
    return nil;
}

static NSRect rectChild(NSXMLElement *element,NSString *key,NSRect fallback) {
    NSXMLElement *rect=keyedChild(element,key);

    if(rect==nil)
        return fallback;
    return NSMakeRect([attribute(rect,@"x") doubleValue],[attribute(rect,@"y") doubleValue],
                      [attribute(rect,@"width") doubleValue],[attribute(rect,@"height") doubleValue]);
}

static void collectElements(NSXMLElement *element,NSMutableDictionary *elements) {
    NSString *ident=attribute(element,@"id");

    if(ident!=nil)
        [elements setObject:element forKey:ident];
    for(NSXMLNode *child in [element children])
        if([child kind]==NSXMLElementKind)
            collectElements((NSXMLElement *)child,elements);
}

+(instancetype)storyboardWithName:(NSStoryboardName)name bundle:(NSBundle *)bundle {
    NSStoryboard *result=[[[self alloc] init] autorelease];
    NSString *path;
    NSData *data;
    NSError *error=nil;

    if(bundle==nil)
        bundle=[NSBundle mainBundle];
    // what Xcode ships: the compiled storyboard
    path=[bundle pathForResource:name ofType:@"storyboardc"];
    if(path==nil)
        path=[bundle pathForResource:name ofType:@"storyboardc" inDirectory:@"Base.lproj"];
    if(path!=nil){
        result->_compiledInfo=[[NSDictionary dictionaryWithContentsOfFile:[path stringByAppendingPathComponent:@"Info.plist"]] retain];
        if(result->_compiledInfo==nil){
            [NSException raise:NSInvalidArgumentException format:@"Compiled storyboard %@ has no Info.plist",path];
            return nil;
        }
        result->_compiledPath=[path copy];
        result->_name=[name copy];
        result->_bundle=[bundle retain];
        return result;
    }
    path=[bundle pathForResource:name ofType:@"storyboard"];
    if(path==nil)
        path=[bundle pathForResource:name ofType:@"storyboard" inDirectory:@"Base.lproj"];
    if(path==nil || (data=[NSData dataWithContentsOfFile:path])==nil){
        [NSException raise:NSInvalidArgumentException format:@"Could not find a storyboard named '%@' in bundle %@",name,bundle];
        return nil;
    }
    result->_document=[[NSXMLDocument alloc] initWithData:data options:0 error:&error];
    if(result->_document==nil){
        [NSException raise:NSInvalidArgumentException format:@"Storyboard %@ is not XML: %@",path,error];
        return nil;
    }
    result->_elements=[NSMutableDictionary new];
    collectElements([result->_document rootElement],result->_elements);
    result->_name=[name copy];
    result->_bundle=[bundle retain];
    return result;
}

-(void)dealloc {
    [_compiledPath release];
    [_compiledInfo release];
    [_document release];
    [_elements release];
    [_name release];
    [_bundle release];
    [super dealloc];
}

static Class classFor(NSXMLElement *element,Class fallback) {
    NSString *name=attribute(element,@"customClass");
    Class cls=name ? NSClassFromString(name) : Nil;

    if(name!=nil && cls==Nil)
        NSLog(@"NSStoryboard: unknown class %@, using %@",name,NSStringFromClass(fallback));
    return cls ? cls : fallback;
}

static Class viewClassForElement(NSXMLElement *element) {
    NSString *tag=[element name];
    Class fallback=[NSView class];

    if([tag isEqualToString:@"slider"])
        fallback=[NSSlider class];
    else if([tag isEqualToString:@"button"])
        fallback=[NSButton class];
    else if([tag isEqualToString:@"textField"])
        fallback=[NSTextField class];
    else if([tag isEqualToString:@"imageView"])
        fallback=[NSImageView class];
    else if(![tag isEqualToString:@"view"] && ![tag isEqualToString:@"customView"])
        NSLog(@"NSStoryboard: <%@> is made as a plain view",tag);
    return classFor(element,fallback);
}

// outlets and actions of an object, made once everything in the scene exists
static void noteConnections(NSXMLElement *element,id object,NSStoryboardInstantiation *inst) {
    NSXMLElement *connections=nil;

    for(NSXMLNode *child in [element children])
        if([child kind]==NSXMLElementKind && [[child name] isEqualToString:@"connections"])
            connections=(NSXMLElement *)child;
    for(NSXMLNode *child in [connections children]){
        NSXMLElement *c=(NSXMLElement *)child;

        if([child kind]!=NSXMLElementKind)
            continue;
        if([[c name] isEqualToString:@"outlet"])
            [inst->_outlets addObject:@[ object,attribute(c,@"property") ?: @"",attribute(c,@"destination") ?: @"" ]];
        else if([[c name] isEqualToString:@"action"])
            [inst->_actions addObject:@[ object,attribute(c,@"selector") ?: @"",attribute(c,@"target") ?: @"" ]];
    }
}

static void configureControl(NSView *view,NSXMLElement *element) {
    NSXMLElement *cell=keyedChild(element,@"cell");

    if([view isKindOfClass:[NSSlider class]] && cell!=nil){
        NSSlider *slider=(NSSlider *)view;

        if(attribute(cell,@"minValue"))
            [slider setMinValue:[attribute(cell,@"minValue") doubleValue]];
        if(attribute(cell,@"maxValue"))
            [slider setMaxValue:[attribute(cell,@"maxValue") doubleValue]];
        if(attribute(cell,@"doubleValue"))
            [slider setDoubleValue:[attribute(cell,@"doubleValue") doubleValue]];
        [slider setContinuous:boolAttribute(cell,@"continuous")];
    }
    else if([view isKindOfClass:[NSButton class]] && cell!=nil && attribute(cell,@"title"))
        [(NSButton *)view setTitle:attribute(cell,@"title")];
    else if([view isKindOfClass:[NSTextField class]] && cell!=nil && attribute(cell,@"title"))
        [(NSTextField *)view setStringValue:attribute(cell,@"title")];
}

-(NSView *)viewForElement:(NSXMLElement *)element instantiation:(NSStoryboardInstantiation *)inst {
    NSRect frame=rectChild(element,@"frame",NSMakeRect(0,0,100,100));
    NSView *view=[[viewClassForElement(element) alloc] initWithFrame:frame];
    NSXMLElement *mask=keyedChild(element,@"autoresizingMask");
    NSString *ident=attribute(element,@"id");

    if(ident!=nil)
        [inst->_objects setObject:view forKey:ident];
    if(mask!=nil){
        NSUInteger m=0;

        if(boolAttribute(mask,@"flexibleMinX")) m|=NSViewMinXMargin;
        if(boolAttribute(mask,@"widthSizable")) m|=NSViewWidthSizable;
        if(boolAttribute(mask,@"flexibleMaxX")) m|=NSViewMaxXMargin;
        if(boolAttribute(mask,@"flexibleMinY")) m|=NSViewMinYMargin;
        if(boolAttribute(mask,@"heightSizable")) m|=NSViewHeightSizable;
        if(boolAttribute(mask,@"flexibleMaxY")) m|=NSViewMaxYMargin;
        [view setAutoresizingMask:m];
    }
    configureControl(view,element);
    noteConnections(element,view,inst);
    for(NSXMLNode *child in [element children]){
        if([child kind]!=NSXMLElementKind || ![[child name] isEqualToString:@"subviews"])
            continue;
        for(NSXMLNode *sub in [child children]){
            if([sub kind]!=NSXMLElementKind)
                continue;
            NSView *subview=[self viewForElement:(NSXMLElement *)sub instantiation:inst];

            [view addSubview:subview];
            [subview release];
        }
    }
    return view;
}

-(NSViewController *)viewControllerForElement:(NSXMLElement *)element instantiation:(NSStoryboardInstantiation *)inst {
    NSViewController *controller=[[classFor(element,[NSViewController class]) alloc] initWithNibName:nil bundle:_bundle];
    NSXMLElement *viewElement=keyedChild(element,@"view");
    NSString *ident=attribute(element,@"id");

    if(ident!=nil)
        [inst->_objects setObject:controller forKey:ident];
    noteConnections(element,controller,inst);
    if(viewElement!=nil){
        NSView *view=[self viewForElement:viewElement instantiation:inst];

        [controller setView:view];
        [view release];
    }
    return controller;
}

-(NSWindowController *)windowControllerForElement:(NSXMLElement *)element instantiation:(NSStoryboardInstantiation *)inst {
    NSXMLElement *windowElement=keyedChild(element,@"window"), *style=windowElement ? keyedChild(windowElement,@"styleMask") : nil;
    NSRect content=rectChild(windowElement,@"contentRect",NSMakeRect(196,240,480,270));
    NSUInteger mask=NSBorderlessWindowMask;
    NSWindow *window;
    NSWindowController *controller;
    NSString *ident=attribute(element,@"id"), *segueTarget=nil;

    if(style==nil || boolAttribute(style,@"titled")) mask|=NSTitledWindowMask;
    if(style==nil || boolAttribute(style,@"closable")) mask|=NSClosableWindowMask;
    if(style==nil || boolAttribute(style,@"miniaturizable")) mask|=NSMiniaturizableWindowMask;
    if(style==nil || boolAttribute(style,@"resizable")) mask|=NSResizableWindowMask;
    window=[[classFor(windowElement,[NSWindow class]) alloc] initWithContentRect:content styleMask:mask
                                                                         backing:NSBackingStoreBuffered defer:NO];
    if(attribute(windowElement,@"title"))
        [window setTitle:attribute(windowElement,@"title")];
    if(attribute(windowElement,@"id"))
        [inst->_objects setObject:window forKey:attribute(windowElement,@"id")];
    noteConnections(windowElement,window,inst);

    controller=[[classFor(element,[NSWindowController class]) alloc] initWithWindow:window];
    [window release];
    if(ident!=nil)
        [inst->_objects setObject:controller forKey:ident];
    noteConnections(element,controller,inst);

    for(NSXMLNode *child in [element children]){
        if([child kind]!=NSXMLElementKind || ![[child name] isEqualToString:@"connections"])
            continue;
        for(NSXMLNode *c in [child children])
            if([c kind]==NSXMLElementKind && [[c name] isEqualToString:@"segue"] &&
               [attribute((NSXMLElement *)c,@"relationship") isEqualToString:@"window.shadowedContentViewController"])
                segueTarget=attribute((NSXMLElement *)c,@"destination");
    }
    if(segueTarget!=nil){
        NSXMLElement *target=[_elements objectForKey:segueTarget];
        NSViewController *content=target ? [self viewControllerForElement:target instantiation:inst] : nil;

        if(content!=nil){
            [window setContentViewController:content];
            [content release];
        }
    }
    return controller;
}

-(id)instantiateElement:(NSXMLElement *)element {
    NSStoryboardInstantiation *inst=[[NSStoryboardInstantiation alloc] init];
    id result=nil;
    NSString *tag=[element name];

    if([tag isEqualToString:@"windowController"])
        result=[self windowControllerForElement:element instantiation:inst];
    else if([tag hasSuffix:@"iewController"])
        result=[self viewControllerForElement:element instantiation:inst];
    else
        NSLog(@"NSStoryboard: cannot instantiate <%@>",tag);

    for(NSArray *o in inst->_outlets){
        id target=[inst->_objects objectForKey:[o objectAtIndex:2]];

        if(target==nil)
            continue;
        @try {
            [[o objectAtIndex:0] setValue:target forKey:[o objectAtIndex:1]];
        }
        @catch(NSException *e) {
            NSLog(@"NSStoryboard: outlet %@ of %@ not set: %@",[o objectAtIndex:1],[[o objectAtIndex:0] class],[e reason]);
        }
    }
    for(NSArray *a in inst->_actions){
        id control=[a objectAtIndex:0];

        if(![control respondsToSelector:@selector(setAction:)])
            continue;
        [control setTarget:[inst->_objects objectForKey:[a objectAtIndex:2]]];
        [control setAction:NSSelectorFromString([a objectAtIndex:1])];
    }
    [inst release];

    // a content view controller's view is loaded: like a nib, the controller hears it now
    if([result isKindOfClass:[NSWindowController class]])
        [[[result window] contentViewController] viewDidLoad];
    else if([result isKindOfClass:[NSViewController class]])
        [result viewDidLoad];
    return [result autorelease];
}

-(NSString *)_pathForNibNamed:(NSString *)name {
    return [[_compiledPath stringByAppendingPathComponent:name] stringByAppendingPathExtension:@"nib"];
}

// a compiled scene: its nib, with the storyboard for the nib's storyboard placeholder and an owner that is
// handed the scene's controller
-(id)instantiateCompiledControllerWithIdentifier:(NSString *)identifier {
    NSString *nibName=[[_compiledInfo objectForKey:@"NSViewControllerIdentifiersToNibNames"] objectForKey:identifier];
    NSNib *nib=[[[NSNib alloc] initWithContentsOfFile:[self _pathForNibNamed:nibName ? nibName : identifier]] autorelease];
    NSStoryboardSceneOwner *owner=[[[NSStoryboardSceneOwner alloc] init] autorelease];
    NSMutableArray *topLevel=[NSMutableArray array];
    NSDictionary *table=[NSDictionary dictionaryWithObjectsAndKeys:owner,NSNibOwner,self,@"NSStoryboardPlaceholder",
                                                                   topLevel,NSNibTopLevelObjects,nil];

    if(nib==nil || ![nib instantiateNibWithExternalNameTable:table]){
        [NSException raise:NSInvalidArgumentException format:@"Storyboard %@ could not load the scene '%@'",_name,identifier];
        return nil;
    }
    if(owner->_sceneController==nil)
        NSLog(@"NSStoryboard: scene '%@' of %@ has no controller",identifier,_name);
    return [[owner->_sceneController retain] autorelease];
}

-(id)instantiateInitialController {
    if(_compiledInfo!=nil)
        return [self instantiateCompiledControllerWithIdentifier:[_compiledInfo objectForKey:@"NSStoryboardDesignatedEntryPointIdentifier"]];

    NSString *initial=attribute([_document rootElement],@"initialViewController");
    NSXMLElement *element=initial ? [_elements objectForKey:initial] : nil;

    if(element==nil){
        NSLog(@"NSStoryboard: %@ has no initial controller",_name);
        return nil;
    }
    return [self instantiateElement:element];
}

-(id)instantiateControllerWithIdentifier:(NSStoryboardSceneIdentifier)identifier {
    if(_compiledInfo!=nil)
        return [self instantiateCompiledControllerWithIdentifier:identifier];
    for(NSXMLElement *element in [_elements allValues])
        if([attribute(element,@"storyboardIdentifier") isEqualToString:identifier])
            return [self instantiateElement:element];
    [NSException raise:NSInvalidArgumentException format:@"Storyboard %@ has no controller with identifier '%@'",_name,identifier];
    return nil;
}

@end

static char NSStoryboardKey;

@implementation NSViewController (NSStoryboard)

-(NSStoryboard *)storyboard {
    return objc_getAssociatedObject(self,&NSStoryboardKey);
}

-(void)setStoryboard:(NSStoryboard *)storyboard {
    objc_setAssociatedObject(self,&NSStoryboardKey,storyboard,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

@end

@implementation NSWindowController (NSStoryboard)

-(NSStoryboard *)storyboard {
    return objc_getAssociatedObject(self,&NSStoryboardKey);
}

-(void)setStoryboard:(NSStoryboard *)storyboard {
    objc_setAssociatedObject(self,&NSStoryboardKey,storyboard,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

@end
