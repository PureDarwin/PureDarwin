// NSDraggingItem.m - the items, image components and session of a drag
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSDraggingItem.h>
#import <AppKit/NSPasteboardItem.h>
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSDraggingManager.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSModernAppKit.h>
#import <AppKit/NSApplication.h>
#import <Foundation/Foundation.h>

@interface NSDraggingManager (PDDragLoop)
- (NSDragOperation)_runDragWithImage:(NSImage *)image imageOrigin:(NSPoint)origin event:(NSEvent *)event
                          pasteboard:(NSPasteboard *)pasteboard source:(id)source session:(NSDraggingSession *)session
                           slideBack:(BOOL)slideBack;
@end

NSString *const NSDraggingImageComponentIconKey = @"icon";
NSString *const NSDraggingImageComponentLabelKey = @"label";

@implementation NSDraggingImageComponent

@synthesize key = _key, contents = _contents, frame = _frame;

+ (instancetype)draggingImageComponentWithKey:(NSString *)key
{
    return [[[self alloc] initWithKey:key] autorelease];
}

- (instancetype)initWithKey:(NSString *)key
{
    if ((self = [super init]) == nil)
        return nil;
    _key = [key copy];
    return self;
}

- (void)dealloc
{
    [_key release];
    [_contents release];
    [super dealloc];
}

@end

@implementation NSDraggingItem

@synthesize item = _item, draggingFrame = _draggingFrame, imageComponentsProvider = _imageComponentsProvider;

- (instancetype)initWithPasteboardWriter:(id<NSPasteboardWriting>)pasteboardWriter
{
    if ((self = [super init]) == nil)
        return nil;
    _item = [pasteboardWriter retain];
    return self;
}

- (void)dealloc
{
    [_item release];
    [_contents release];
    [_imageComponentsProvider release];
    [super dealloc];
}

// a single icon component that fills the dragging frame, as setDraggingFrame:contents: describes
- (void)setDraggingFrame:(NSRect)frame contents:(id)contents
{
    _draggingFrame = frame;
    [_contents release];
    _contents = [contents retain];
    [self setImageComponentsProvider:nil];
}

- (NSArray *)imageComponents
{
    NSDraggingImageComponent *icon;

    if (_imageComponentsProvider != nil)
        return _imageComponentsProvider();
    if (_contents == nil)
        return nil;
    icon = [NSDraggingImageComponent draggingImageComponentWithKey:NSDraggingImageComponentIconKey];
    [icon setContents:_contents];
    [icon setFrame:NSMakeRect(0, 0, _draggingFrame.size.width, _draggingFrame.size.height)];
    return @[ icon ];
}

@end

@implementation NSDraggingSession

@synthesize draggingFormation = _draggingFormation, animatesToStartingPositionsOnCancelOrFail = _animates;
@synthesize draggingLeaderIndex = _draggingLeaderIndex;

- (instancetype)_initWithItems:(NSArray *)items source:(id)source pasteboard:(NSPasteboard *)pasteboard
{
    if ((self = [super init]) == nil)
        return nil;
    _items = [items copy];
    _source = source;
    _pasteboard = [pasteboard retain];
    _animates = YES;
    return self;
}

- (void)dealloc
{
    [_items release];
    [_pasteboard release];
    [super dealloc];
}

- (NSPasteboard *)draggingPasteboard
{
    return _pasteboard;
}

- (NSArray *)_items
{
    return _items;
}

- (id)_source
{
    return _source;
}

- (NSInteger)draggingSequenceNumber
{
    return (NSInteger)(uintptr_t)self;
}

- (NSPoint)draggingLocation
{
    return [NSEvent mouseLocation];
}

- (void)enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)options forView:(NSView *)view classes:(NSArray *)classArray
                            searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(NSDraggingItem *item, NSInteger index, BOOL *stop))block
{
    BOOL stop = NO;
    NSInteger index = 0;

    for (NSDraggingItem *item in _items) {
        BOOL wanted = classArray == nil;
        for (Class cls in classArray) {
            if ([[item item] isKindOfClass:cls])
                wanted = YES;
        }
        if (!wanted)
            continue;
        block(item, index++, &stop);
        if (stop)
            break;
    }
}

@end

// every item's image components drawn at their frames, in one image covering them all, with its origin in the view
static NSImage *PDDragImage(NSArray *items, BOOL flipped, NSRect *unionOut)
{
    NSRect bounds = NSZeroRect;

    for (NSDraggingItem *item in items)
        bounds = NSIsEmptyRect(bounds) ? [item draggingFrame] : NSUnionRect(bounds, [item draggingFrame]);
    *unionOut = bounds;
    if (NSIsEmptyRect(bounds))
        return nil;
    // drawn in the source view's orientation so frames given in a flipped view land the right way up
    return [NSImage imageWithSize:bounds.size flipped:flipped drawingHandler:^BOOL(NSRect dstRect) {
        for (NSDraggingItem *item in items) {
            NSRect frame = [item draggingFrame];

            for (NSDraggingImageComponent *component in [item imageComponents]) {
                NSRect rect = [component frame];
                id contents = [component contents];

                rect.origin.x += frame.origin.x - bounds.origin.x;
                rect.origin.y += frame.origin.y - bounds.origin.y;
                if ([contents isKindOfClass:[NSImage class]])
                    [contents drawInRect:rect fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0 respectFlipped:YES hints:nil];
            }
        }
        return YES;
    }];
}

@implementation NSView (DraggingSession)

// as on macOS the session starts once the current event has been handled, the call itself returns at once
- (NSDraggingSession *)beginDraggingSessionWithItems:(NSArray *)items event:(NSEvent *)event source:(id<NSDraggingSource>)source
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    NSMutableArray *writers = [NSMutableArray array];
    NSDraggingSession *session;
    NSRect frame;
    NSImage *image;
    NSPoint origin;

    for (NSDraggingItem *item in items) {
        if ([item item] != nil)
            [writers addObject:[item item]];
    }
    [pasteboard clearContents];
    [pasteboard writeObjects:writers];
    session = [[[NSDraggingSession alloc] _initWithItems:items source:source pasteboard:pasteboard] autorelease];
    image = PDDragImage(items, [self isFlipped], &frame);
    // the image origin is the frame's bottom-left corner on screen, flipped views keep it at the top
    origin = [self convertPoint:NSMakePoint(NSMinX(frame), [self isFlipped] ? NSMaxY(frame) : NSMinY(frame)) toView:nil];
    origin = [[self window] convertBaseToScreen:origin];

    NSDictionary *drag = @{ @"session" : session, @"event" : event ?: [NSApp currentEvent], @"origin" : [NSValue valueWithPoint:origin],
                            @"image" : image ?: [NSNull null] };
    [self performSelector:@selector(_pdRunDraggingSession:) withObject:drag afterDelay:0
                  inModes:@[ NSDefaultRunLoopMode, NSEventTrackingRunLoopMode, NSModalPanelRunLoopMode ]];
    return session;
}

- (void)_pdRunDraggingSession:(NSDictionary *)drag
{
    NSDraggingSession *session = [drag objectForKey:@"session"];
    id image = [drag objectForKey:@"image"];

    [[NSDraggingManager draggingManager] _runDragWithImage:image == [NSNull null] ? nil : image
                                               imageOrigin:[[drag objectForKey:@"origin"] pointValue]
                                                     event:[drag objectForKey:@"event"]
                                                pasteboard:[session draggingPasteboard]
                                                    source:[session _source]
                                                   session:session
                                                 slideBack:[session animatesToStartingPositionsOnCancelOrFail]];
}

@end
