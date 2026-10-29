/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>
   Copyright (c) 2026 The PureDarwin Project

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSDraggingManager.h>
#import <AppKit/NSDraggingItem.h>
#import <AppKit/NSDragging.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSView.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSPasteboardItem.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSColor.h>
#import <CoreGraphics/CGWindowLevel.h>
#import <CoreGraphics/CGBitmapContext.h>
#import <Foundation/Foundation.h>
#import <WindowServer/rpc.h>
#import <WindowServer/message.h>
#import <dispatch/dispatch.h>
#import <pthread.h>

@interface NSDraggingSession (PDDragLoop)
- (NSArray *)_items;
- (id)_source;
@end

@interface NSDraggingManager (PDDragLoop)
- (NSDragOperation)_runDragWithImage:(NSImage *)image imageOrigin:(NSPoint)origin event:(NSEvent *)event
                          pasteboard:(NSPasteboard *)pasteboard source:(id)source session:(NSDraggingSession *)session
                           slideBack:(BOOL)slideBack;
@end

// the image under the pointer, in a window that takes no input so hit testing sees through it
@interface PDDragImageView : NSView {
    NSImage *_image;
}
- (void)setImage:(NSImage *)image;
@end

@implementation PDDragImageView

- (void)dealloc
{
    [_image release];
    [super dealloc];
}

- (void)setImage:(NSImage *)image
{
    [_image release];
    _image = [image retain];
    [self setNeedsDisplay:YES];
}

- (BOOL)isOpaque
{
    return NO;
}

- (void)drawRect:(NSRect)rect
{
    [[NSColor clearColor] set];
    NSRectFill([self bounds]);
    [_image drawInRect:[self bounds] fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:0.75];
}

@end

// what a destination sees of the drag in progress
@interface PDDraggingInfo : NSObject <NSDraggingInfo> {
@public
    NSPasteboard *_pasteboard;
    id _source;
    NSDraggingSession *_session;
    NSImage *_image;
    NSPoint _location;
    NSPoint _imageLocation;
    NSSize _imageOffset;
    NSWindow *_destinationWindow;
    NSDragOperation _sourceMask;
    NSInteger _sequence;
    NSInteger _validItems;
    BOOL _animatesToDestination;
    NSInteger _formation;
}
@end

@implementation PDDraggingInfo

- (void)dealloc
{
    [_pasteboard release];
    [_session release];
    [_image release];
    [super dealloc];
}

- (NSPasteboard *)draggingPasteboard
{
    return _pasteboard;
}

- (NSDragOperation)draggingSourceOperationMask
{
    return _sourceMask;
}

// window coordinates of the destination window, as on macOS
- (NSPoint)draggingLocation
{
    return _destinationWindow ? [_destinationWindow convertScreenToBase:_location] : _location;
}

- (NSImage *)draggedImage
{
    return _image;
}

- (NSPoint)draggedImageLocation
{
    return _destinationWindow ? [_destinationWindow convertScreenToBase:_imageLocation] : _imageLocation;
}

- (NSWindow *)draggingDestinationWindow
{
    return _destinationWindow;
}

- (id)draggingSource
{
    return _source;
}

// the image stays under the pointer until the drop, so there is nothing to slide
- (void)slideDraggedImageTo:(NSPoint)point
{
}

- (int)draggingSequenceNumber
{
    return (int)_sequence;
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)destination
{
    if ([_source respondsToSelector:@selector(namesOfPromisedFilesDroppedAtDestination:)])
        return [_source namesOfPromisedFilesDroppedAtDestination:destination];
    return nil;
}

- (NSDraggingFormation)draggingFormation
{
    return (NSDraggingFormation)_formation;
}

- (void)setDraggingFormation:(NSDraggingFormation)formation
{
    _formation = formation;
}

- (BOOL)animatesToDestination
{
    return _animatesToDestination;
}

- (void)setAnimatesToDestination:(BOOL)flag
{
    _animatesToDestination = flag;
}

- (NSInteger)numberOfValidItemsForDrop
{
    return _validItems;
}

- (void)setNumberOfValidItemsForDrop:(NSInteger)count
{
    _validItems = count;
}

- (NSSpringLoadingHighlight)springLoadingHighlight
{
    return 0;
}

- (void)resetSpringLoading
{
}

// legacy drags have no items, the pasteboard stands in as one
- (void)enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)options forView:(NSView *)view classes:(NSArray *)classArray
                            searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(NSDraggingItem *item, NSInteger index, BOOL *stop))block
{
    NSArray *objects;
    BOOL stop = NO;
    NSInteger index = 0;

    if (_session != nil) {
        [_session enumerateDraggingItemsWithOptions:options forView:view classes:classArray searchOptions:searchOptions usingBlock:block];
        return;
    }
    objects = [_pasteboard readObjectsForClasses:classArray ?: @[ [NSPasteboardItem class] ] options:searchOptions];
    for (id object in objects) {
        NSDraggingItem *item = [[[NSDraggingItem alloc] initWithPasteboardWriter:object] autorelease];
        block(item, index++, &stop);
        if (stop)
            break;
    }
}

@end

static NSDraggingManager *sharedManager;
static NSInteger dragSequence;

// the operations the modifier keys ask for, out of what the source offers
static NSDragOperation PDOperationForModifiers(NSDragOperation mask, NSUInteger modifiers, BOOL ignore)
{
    BOOL option = (modifiers & NSAlternateKeyMask) != 0;
    BOOL command = (modifiers & NSCommandKeyMask) != 0;
    BOOL control = (modifiers & NSControlKeyMask) != 0;

    if (ignore)
        return mask;
    if (option && command)
        return mask & NSDragOperationLink;
    if (option)
        return mask & NSDragOperationCopy;
    if (command)
        return mask & (NSDragOperationGeneric | NSDragOperationMove);
    if (control)
        return mask & NSDragOperationLink;
    return mask;
}

// the destination takes one of the offered operations, preferring the usual order
static NSDragOperation PDSingleOperation(NSDragOperation operation, NSDragOperation mask)
{
    static const NSDragOperation order[] = { NSDragOperationMove, NSDragOperationCopy, NSDragOperationGeneric,
                                             NSDragOperationLink, NSDragOperationPrivate, NSDragOperationDelete };
    NSDragOperation allowed = operation & mask;

    if (allowed == NSDragOperationNone)
        return NSDragOperationNone;
    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if (allowed & order[i])
            return order[i];
    }
    return allowed;
}

// wl_data_device_manager dnd actions
enum { PDWaylandCopy = 1, PDWaylandMove = 2, PDWaylandAsk = 4 };

static uint32_t PDWaylandActions(NSDragOperation operations)
{
    uint32_t actions = 0;

    if (operations & (NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationLink))
        actions |= PDWaylandCopy;
    if (operations & (NSDragOperationMove | NSDragOperationDelete))
        actions |= PDWaylandMove;
    return actions;
}

static NSDragOperation PDOperationsForWayland(uint32_t actions)
{
    NSDragOperation operations = NSDragOperationNone;

    if (actions & PDWaylandCopy)
        operations |= NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationLink;
    if (actions & PDWaylandMove)
        operations |= NSDragOperationMove;
    if (actions & PDWaylandAsk)
        operations |= NSDragOperationGeneric;
    return operations;
}

@interface NSPasteboard (PDDragOffer)
+ (NSPasteboard *)_pdDragOfferPasteboard;
+ (NSArray *)_pdMimeTypesForType:(NSString *)type;
+ (NSString *)_pdTypeForMimeType:(NSString *)mime;
@end

// what the drag this process handed to the compositor sends, MIME type to bytes, read on the event thread
static NSDictionary *outgoingData;
static pthread_mutex_t outgoingLock = PTHREAD_MUTEX_INITIALIZER;

static void PDSendDragData(const char *mimeType, int fd)
{
    NSData *data;

    @autoreleasepool {
        pthread_mutex_lock(&outgoingLock);
        data = [[outgoingData objectForKey:[NSString stringWithUTF8String:mimeType]] retain];
        pthread_mutex_unlock(&outgoingLock);
    }
    // a reader in this same process reads on the main thread, so the write must not wait for it here
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        const char *bytes = [data bytes];
        size_t left = [data length];

        while (left > 0) {
            ssize_t wrote = write(fd, bytes, left);
            if (wrote <= 0)
                break;
            bytes += wrote;
            left -= (size_t)wrote;
        }
        close(fd);
        [data release];
    });
}

// every type on the board as the MIME types other clients read, files also as one uri-list
static NSDictionary *PDDragDataForPasteboard(NSPasteboard *pasteboard, NSMutableArray *mimeTypes)
{
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    NSArray *files = [[pasteboard types] containsObject:NSFilenamesPboardType] ? [pasteboard propertyListForType:NSFilenamesPboardType] : nil;

    if ([files isKindOfClass:[NSArray class]] && [files count] > 0) {
        NSMutableString *list = [NSMutableString string];

        for (NSString *path in files)
            [list appendFormat:@"%@\r\n", [[NSURL fileURLWithPath:path] absoluteString]];
        [result setObject:[list dataUsingEncoding:NSUTF8StringEncoding] forKey:@"text/uri-list"];
        [mimeTypes addObject:@"text/uri-list"];
    }
    for (NSString *type in [pasteboard types]) {
        NSData *data = nil;

        for (NSString *mime in [NSPasteboard _pdMimeTypesForType:type]) {
            if ([result objectForKey:mime] != nil)
                continue;
            if (data == nil && (data = [pasteboard dataForType:type]) == nil)
                break;
            [result setObject:data forKey:mime];
            [mimeTypes addObject:mime];
        }
    }
    return result;
}

// the drag image as premultiplied ARGB rows from the top, for the compositor to show under the pointer
static NSData *PDDragImagePixels(NSImage *image, int *width, int *height)
{
    NSSize size = [image size];
    int w = (int)ceil(size.width), h = (int)ceil(size.height);
    NSMutableData *pixels;
    CGColorSpaceRef space;
    CGContextRef context;

    if (image == nil || w <= 0 || h <= 0 || w > 1024 || h > 1024)
        return nil;
    pixels = [NSMutableData dataWithLength:(NSUInteger)w * 4 * (NSUInteger)h];
    space = CGColorSpaceCreateDeviceRGB();
    context = CGBitmapContextCreate([pixels mutableBytes], w, h, 8, w * 4, space,
                                    kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    if (context == NULL)
        return nil;
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO]];
    [image drawInRect:NSMakeRect(0, 0, w, h) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:0.8];
    [NSGraphicsContext restoreGraphicsState];
    CGContextRelease(context);
    *width = w;
    *height = h;
    return pixels;
}

static BOOL PDTypesIntersect(NSArray *registered, NSArray *offered)
{
    for (NSString *type in registered) {
        if ([offered containsObject:type])
            return YES;
    }
    return NO;
}

@implementation NSDraggingManager

+ (NSDraggingManager *)draggingManager
{
    if (sharedManager == nil)
        sharedManager = [[NSDraggingManager alloc] init];
    return sharedManager;
}

// registration lives on the window and its views, the manager reads it when it hit tests
- (void)registerWindow:(NSWindow *)window dragTypes:(NSArray *)types
{
}

- (void)unregisterWindow:(NSWindow *)window
{
}

- (id)localDraggingSource
{
    return _localSource;
}

// the app's own visible windows, key and main first, then the rest by level
- (NSArray *)_candidateWindows:(NSWindow *)except
{
    NSMutableArray *windows = [NSMutableArray array];
    NSWindow *key = [NSApp keyWindow], *main = [NSApp mainWindow];

    if (key != nil && key != except && [key isVisible])
        [windows addObject:key];
    if (main != nil && main != key && main != except && [main isVisible])
        [windows addObject:main];
    for (NSWindow *window in [[NSApp windows] sortedArrayUsingComparator:^NSComparisonResult(NSWindow *a, NSWindow *b) {
             return [a level] > [b level] ? NSOrderedAscending : [a level] < [b level] ? NSOrderedDescending : NSOrderedSame;
         }]) {
        if (window != except && [window isVisible] && ![windows containsObject:window])
            [windows addObject:window];
    }
    return windows;
}

// the innermost view under the point that takes one of the dragged types, or the window itself
- (id)_destinationAtScreenPoint:(NSPoint)point types:(NSArray *)types window:(NSWindow **)windowOut except:(NSWindow *)except
{
    for (NSWindow *window in [self _candidateWindows:except]) {
        NSView *frameView;
        NSView *view;

        if (!NSPointInRect(point, [window frame]))
            continue;
        *windowOut = window;
        frameView = [[window contentView] superview] ?: [window contentView];
        view = [frameView hitTest:[frameView convertPoint:[window convertScreenToBase:point] fromView:nil]];
        for (; view != nil; view = [view superview]) {
            if (PDTypesIntersect([view registeredDraggedTypes], types))
                return view;
        }
        if (PDTypesIntersect([window _draggedTypes], types))
            return window;
        return nil;
    }
    *windowOut = nil;
    return nil;
}

- (NSPoint)_screenLocationOfEvent:(NSEvent *)event fallback:(NSPoint)fallback
{
    NSWindow *window = [event window];

    if (window == nil && [event windowNumber] != 0)
        window = [NSApp windowWithWindowNumber:[event windowNumber]];
    if (window == nil)
        return [NSEvent mouseLocation];
    return [window convertBaseToScreen:[event locationInWindow]];
}

// legacy entry, image location is in the window coordinates of the event's window
- (void)dragImage:(NSImage *)image at:(NSPoint)location offset:(NSSize)offset event:(NSEvent *)event pasteboard:(NSPasteboard *)pasteboard source:(id)source slideBack:(BOOL)slideBack
{
    NSWindow *window = [event window];
    NSPoint origin = window ? [window convertBaseToScreen:location] : location;

    origin.x += offset.width;
    origin.y += offset.height;
    [self _runDragWithImage:image imageOrigin:origin event:event pasteboard:pasteboard source:source session:nil slideBack:slideBack];
}

// asks the destination under the point what it would do, and tells the source client through the offer
- (void)_remoteDragUpdate:(NSPoint)point modifiers:(NSUInteger)modifiers
{
    PDDraggingInfo *info = _remoteInfo;
    NSArray *types = [info->_pasteboard types];
    NSWindow *window = nil;
    id target;
    NSDragOperation operation = NSDragOperationNone;
    const char *mime = NULL;

    info->_location = point;
    info->_imageLocation = point;
    target = [self _destinationAtScreenPoint:point types:types window:&window except:nil];
    info->_destinationWindow = window;
    if (target != _remoteDestination) {
        if ([_remoteDestination respondsToSelector:@selector(draggingExited:)])
            [_remoteDestination draggingExited:info];
        _remoteDestination = target;
        if ([target respondsToSelector:@selector(draggingEntered:)])
            operation = [target draggingEntered:info];
    } else if ([target respondsToSelector:@selector(draggingUpdated:)]) {
        operation = [target draggingUpdated:info];
    } else {
        operation = _remoteOperation;
    }
    _remoteOperation = target ? PDSingleOperation(operation, info->_sourceMask) : NSDragOperationNone;

    // the MIME type of the first board type the destination takes, any offered one will do for the source
    if (_remoteOperation != NSDragOperationNone) {
        char **mimeTypes = NULL;
        int count = _windowServerCopyDragMimeTypes(&mimeTypes, NULL);

        if (count > 0)
            mime = [[NSString stringWithUTF8String:mimeTypes[0]] UTF8String];
        for (int i = 0; i < count; i++)
            free(mimeTypes[i]);
        free(mimeTypes);
    }
    _windowServerDragAccept(mime, PDWaylandActions(_remoteOperation), PDWaylandActions(_remoteOperation) & PDWaylandMove ? PDWaylandMove : PDWaylandCopy);
}

- (void)_remoteDragEnd
{
    PDDraggingInfo *info = _remoteInfo;

    if ([_remoteDestination respondsToSelector:@selector(draggingEnded:)])
        [_remoteDestination draggingEnded:info];
    _remoteDestination = nil;
    _remoteOperation = NSDragOperationNone;
    [_remoteInfo release];
    _remoteInfo = nil;
}

- (void)_remoteDragEvent:(NSArray *)event
{
    int code = [[event objectAtIndex:0] intValue];
    NSWindow *window = [NSApp windowWithWindowNumber:[[event objectAtIndex:1] intValue]];
    NSPoint point = NSMakePoint([[event objectAtIndex:2] doubleValue], [[event objectAtIndex:3] doubleValue]);
    NSUInteger modifiers = [[event objectAtIndex:4] unsignedIntValue];
    PDDraggingInfo *info;

    if (window != nil)
        point = [window convertBaseToScreen:point];
    if (getenv("PD_DRAG_TRACE") != NULL)
        NSLog(@"drag: remote event %d window %@ at %@", code, window, NSStringFromPoint(point));

    switch (code) {
    case WS_EVENT_DRAG_ENTER: {
        uint32_t actions = 0;
        char **mimeTypes = NULL;
        int count;

        if (_remoteInfo != nil)
            [self _remoteDragEnd];
        info = [[PDDraggingInfo alloc] init];
        info->_pasteboard = [[NSPasteboard _pdDragOfferPasteboard] retain];
        count = _windowServerCopyDragMimeTypes(&mimeTypes, &actions);
        for (int i = 0; i < count; i++)
            free(mimeTypes[i]);
        free(mimeTypes);
        info->_sourceMask = PDOperationsForWayland(actions);
        // a source that names no actions predates them, copying is what it means
        if (info->_sourceMask == NSDragOperationNone)
            info->_sourceMask = NSDragOperationCopy | NSDragOperationGeneric;
        info->_source = _localSource;
        info->_sequence = ++dragSequence;
        info->_validItems = 1;
        _remoteInfo = info;
        _remoteDestination = nil;
        _remoteOperation = NSDragOperationNone;
        [self _remoteDragUpdate:point modifiers:modifiers];
        break;
    }
    case WS_EVENT_DRAG_MOTION:
        if (_remoteInfo != nil)
            [self _remoteDragUpdate:point modifiers:modifiers];
        break;
    case WS_EVENT_DRAG_LEAVE:
        if (_remoteInfo == nil)
            break;
        if ([_remoteDestination respondsToSelector:@selector(draggingExited:)])
            [_remoteDestination draggingExited:_remoteInfo];
        [self _remoteDragEnd];
        break;
    case WS_EVENT_DRAG_DROP: {
        id destination = _remoteDestination;
        BOOL accepted = NO;

        info = _remoteInfo;
        if (info == nil) {
            _windowServerDragFinish();
            break;
        }
        if (destination != nil && _remoteOperation != NSDragOperationNone) {
            accepted = YES;
            if ([destination respondsToSelector:@selector(prepareForDragOperation:)])
                accepted = [destination prepareForDragOperation:info];
            if (accepted && [destination respondsToSelector:@selector(performDragOperation:)])
                accepted = [destination performDragOperation:info];
            if (accepted && [destination respondsToSelector:@selector(concludeDragOperation:)])
                [destination concludeDragOperation:info];
        }
        if (!accepted && [destination respondsToSelector:@selector(draggingExited:)])
            [destination draggingExited:info];
        _windowServerDragFinish();
        [self _remoteDragEnd];
        break;
    }
    }
}

@end

@implementation NSDraggingManager (PDDragLoop)

- (NSDragOperation)_sourceMaskFor:(id)source session:(NSDraggingSession *)session local:(BOOL)local
{
    if (session != nil && [source respondsToSelector:@selector(draggingSession:sourceOperationMaskForDraggingContext:)])
        return [source draggingSession:session
            sourceOperationMaskForDraggingContext:local ? NSDraggingContextWithinApplication : NSDraggingContextOutsideApplication];
    if ([source respondsToSelector:@selector(draggingSourceOperationMaskForLocal:)])
        return [source draggingSourceOperationMaskForLocal:local];
    return local ? NSDragOperationEvery : NSDragOperationCopy;
}

- (BOOL)_ignoresModifiers:(id)source session:(NSDraggingSession *)session
{
    if (session != nil && [source respondsToSelector:@selector(ignoreModifierKeysForDraggingSession:)])
        return [source ignoreModifierKeysForDraggingSession:session];
    if ([source respondsToSelector:@selector(ignoreModifierKeysWhileDragging)])
        return [source ignoreModifierKeysWhileDragging];
    return NO;
}

- (void)_notifySource:(id)source session:(NSDraggingSession *)session image:(NSImage *)image movedTo:(NSPoint)point
{
    if (session != nil && [source respondsToSelector:@selector(draggingSession:movedToPoint:)])
        [source draggingSession:session movedToPoint:point];
    else if ([source respondsToSelector:@selector(draggedImage:movedTo:)])
        [source draggedImage:image movedTo:point];
}

// runs until the button comes up or Escape, sending the NSDraggingDestination messages as the pointer moves
// gives the drag to the compositor with every type as MIME types and the image as its icon
- (BOOL)_handOffDrag:(PDDraggingInfo *)info image:(NSImage *)image grab:(NSSize)grab event:(NSEvent *)event offered:(NSDragOperation)offered
{
    NSMutableArray *mimeTypes = [NSMutableArray array];
    NSDictionary *data = PDDragDataForPasteboard(info->_pasteboard, mimeTypes);
    NSUInteger count = [mimeTypes count];
    const char **list;
    NSData *pixels;
    int width = 0, height = 0, hotX, hotY, started;

    if (count == 0 || [event windowNumber] == 0)
        return NO;
    pthread_mutex_lock(&outgoingLock);
    [outgoingData release];
    outgoingData = [data retain];
    pthread_mutex_unlock(&outgoingLock);

    list = malloc(sizeof(char *) * count);
    for (NSUInteger i = 0; i < count; i++)
        list[i] = [[mimeTypes objectAtIndex:i] UTF8String];
    pixels = PDDragImagePixels(image, &width, &height);
    // the pointer holds the image where it grabbed it, or by its middle when the grab lies outside
    hotX = (int)grab.width;
    hotY = height - (int)grab.height;
    if (hotX < 0 || hotX > width || hotY < 0 || hotY > height) {
        hotX = width / 2;
        hotY = height / 2;
    }
    started = _windowServerDragStart((int)[event windowNumber], list, (int)count, PDWaylandActions(offered),
                                     [pixels bytes], width, height, hotX, hotY, PDSendDragData);
    free(list);
    if (getenv("PD_DRAG_TRACE") != NULL)
        NSLog(@"drag: handed to the compositor %d with %@", started, mimeTypes);
    return started != 0;
}

- (NSDragOperation)_runDragWithImage:(NSImage *)image imageOrigin:(NSPoint)origin event:(NSEvent *)event
                          pasteboard:(NSPasteboard *)pasteboard source:(id)source session:(NSDraggingSession *)session
                           slideBack:(BOOL)slideBack
{
    PDDraggingInfo *info = [[PDDraggingInfo alloc] init];
    NSPoint start = [self _screenLocationOfEvent:event fallback:origin];
    NSSize grab = NSMakeSize(start.x - origin.x, start.y - origin.y);
    NSSize imageSize = image ? [image size] : NSZeroSize;
    NSArray *types = [pasteboard types] ?: @[];
    BOOL ignoreModifiers = [self _ignoresModifiers:source session:session];
    NSDragOperation offered = [self _sourceMaskFor:source session:session local:YES];
    NSDragOperation current = NSDragOperationNone;
    NSUInteger modifiers = [event modifierFlags];
    NSWindow *imageWindow = nil;
    id destination = nil;
    BOOL dropped = NO, cancelled = NO, periodic = NO;
    BOOL handedOff = NO, handOffDone = NO;
    NSTimeInterval droppedAt = 0;
    NSDragOperation result = NSDragOperationNone;
    NSPoint point = start;

    _localSource = source;
    info->_pasteboard = [pasteboard retain];
    info->_source = source;
    info->_session = [session retain];
    info->_image = [image retain];
    info->_sequence = ++dragSequence;
    info->_validItems = session ? (NSInteger)[[session _items] count] : 1;
    info->_animatesToDestination = NO;

    if (imageSize.width > 0 && imageSize.height > 0) {
        PDDragImageView *view;

        imageWindow = [[NSPanel alloc] initWithContentRect:NSMakeRect(origin.x, origin.y, imageSize.width, imageSize.height)
                                                 styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
        [imageWindow setLevel:kCGDraggingWindowLevel];
        [imageWindow setOpaque:NO];
        [imageWindow setBackgroundColor:[NSColor clearColor]];
        [imageWindow setHasShadow:NO];
        [imageWindow setIgnoresMouseEvents:YES];
        view = [[[PDDragImageView alloc] initWithFrame:NSMakeRect(0, 0, imageSize.width, imageSize.height)] autorelease];
        [view setImage:image];
        [imageWindow setContentView:view];
        [imageWindow orderFront:nil];
        [imageWindow display];
    }

    if (session != nil && [source respondsToSelector:@selector(draggingSession:willBeginAtPoint:)])
        [source draggingSession:session willBeginAtPoint:start];
    else if ([source respondsToSelector:@selector(draggedImage:beganAt:)])
        [source draggedImage:image beganAt:origin];

    for (;;) {
        NSWindow *window = nil;
        id target;
        NSEvent *next;

        // the compositor runs the drag once it left the app, this only waits to hear how it ended
        if (handedOff) {
            uint32_t action = 0;
            int state = _windowServerDragState(&action);

            if (state == WS_DRAG_DROPPED && droppedAt == 0)
                droppedAt = [NSDate timeIntervalSinceReferenceDate];
            // a target that never says it finished still took the drop after a while
            if (state == WS_DRAG_FINISHED || state == WS_DRAG_CANCELLED || state == WS_DRAG_NONE ||
                (state == WS_DRAG_DROPPED && [NSDate timeIntervalSinceReferenceDate] - droppedAt > 3)) {
                if (state != WS_DRAG_CANCELLED && state != WS_DRAG_NONE)
                    result = PDSingleOperation(PDOperationsForWayland(action), info->_sourceMask);
                if (getenv("PD_DRAG_TRACE") != NULL)
                    NSLog(@"drag: compositor drag ended state %d action %u", state, action);
                handOffDone = YES;
                break;
            }
            [NSApp nextEventMatchingMask:NSLeftMouseUpMask | NSKeyDownMask untilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]
                                  inMode:NSEventTrackingRunLoopMode dequeue:YES];
            continue;
        }

        info->_location = point;
        info->_imageLocation = NSMakePoint(point.x - grab.width, point.y - grab.height);
        info->_sourceMask = PDOperationForModifiers(offered, modifiers, ignoreModifiers);
        target = [self _destinationAtScreenPoint:point types:types window:&window except:imageWindow];
        info->_destinationWindow = window;

        // outside every window of this app other clients may take it, the compositor carries it from here
        if (window == nil && !dropped && !cancelled && [types count] > 0 &&
            [self _handOffDrag:info image:image grab:grab event:event offered:[self _sourceMaskFor:source session:session local:NO]]) {
            handedOff = YES;
            if ([destination respondsToSelector:@selector(draggingExited:)])
                [destination draggingExited:info];
            destination = nil;
            current = NSDragOperationNone;
            if (imageWindow != nil)
                [imageWindow orderOut:nil];
            continue;
        }

        if (target != destination) {
            if ([destination respondsToSelector:@selector(draggingExited:)])
                [destination draggingExited:info];
            destination = target;
            current = NSDragOperationNone;
            if ([destination respondsToSelector:@selector(draggingEntered:)])
                current = [destination draggingEntered:info];
        } else if (destination != nil) {
            BOOL wantsPeriodic = ![destination respondsToSelector:@selector(wantsPeriodicDraggingUpdates)] ||
                                 [destination wantsPeriodicDraggingUpdates];
            if ((!periodic || wantsPeriodic) && [destination respondsToSelector:@selector(draggingUpdated:)])
                current = [destination draggingUpdated:info];
        }
        current = PDSingleOperation(current, info->_sourceMask);

        if (imageWindow != nil)
            [imageWindow setFrameOrigin:info->_imageLocation];
        [self _notifySource:source session:session image:image movedTo:info->_imageLocation];

        if (dropped || cancelled)
            break;

        // periodic updates arrive when the pointer rests, unless the destination opts out
        next = [NSApp nextEventMatchingMask:NSLeftMouseDraggedMask | NSLeftMouseUpMask | NSMouseMovedMask | NSFlagsChangedMask | NSKeyDownMask
                                  untilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]
                                     inMode:NSEventTrackingRunLoopMode
                                    dequeue:YES];
        periodic = next == nil;
        if (periodic)
            continue;
        if (getenv("PD_DRAG_TRACE") != NULL)
            NSLog(@"drag: event %d at %@ destination %@ operation %u", (int)[next type], NSStringFromPoint([next locationInWindow]),
                  destination, (unsigned)current);
        modifiers = [next modifierFlags];
        switch ([next type]) {
        case NSLeftMouseUp:
            point = [self _screenLocationOfEvent:next fallback:point];
            dropped = YES;
            break;
        case NSKeyDown:
            if ([[next charactersIgnoringModifiers] isEqualToString:@"\e"])
                cancelled = YES;
            break;
        case NSFlagsChanged:
            break;
        default:
            point = [self _screenLocationOfEvent:next fallback:point];
            break;
        }
        if (dropped || cancelled) {
            info->_location = point;
            info->_imageLocation = NSMakePoint(point.x - grab.width, point.y - grab.height);
            break;
        }
    }

    if (getenv("PD_DRAG_TRACE") != NULL)
        NSLog(@"drag: ended dropped %d cancelled %d destination %@ operation %u", dropped, cancelled, destination, (unsigned)current);
    if (handedOff) {
        _windowServerDragEnd();
        pthread_mutex_lock(&outgoingLock);
        [outgoingData release];
        outgoingData = nil;
        pthread_mutex_unlock(&outgoingLock);
    }
    // a drop goes through prepare, perform and conclude only when the destination accepted an operation
    if (handOffDone) {
    } else if (dropped && destination != nil && current != NSDragOperationNone) {
        BOOL accepted = YES;

        if ([destination respondsToSelector:@selector(prepareForDragOperation:)])
            accepted = [destination prepareForDragOperation:info];
        if (accepted && [destination respondsToSelector:@selector(performDragOperation:)])
            accepted = [destination performDragOperation:info];
        if (accepted) {
            result = current;
            if ([destination respondsToSelector:@selector(concludeDragOperation:)])
                [destination concludeDragOperation:info];
        }
    } else if (destination != nil && [destination respondsToSelector:@selector(draggingExited:)]) {
        [destination draggingExited:info];
    }

    if (destination != nil && [destination respondsToSelector:@selector(draggingEnded:)])
        [destination draggingEnded:info];

    if (imageWindow != nil) {
        BOOL slide = result == NSDragOperationNone &&
                     (session ? [session animatesToStartingPositionsOnCancelOrFail] : slideBack);

        if (slide) {
            NSPoint from = [imageWindow frame].origin;
            for (int step = 1; step <= 8; step++) {
                CGFloat t = step / 8.0;
                [imageWindow setFrameOrigin:NSMakePoint(from.x + (origin.x - from.x) * t, from.y + (origin.y - from.y) * t)];
                [imageWindow displayIfNeeded];
                [[NSRunLoop currentRunLoop] runMode:NSEventTrackingRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.016]];
            }
        }
        [imageWindow orderOut:nil];
        [imageWindow close];
        [imageWindow release];
    }

    if (session != nil && [source respondsToSelector:@selector(draggingSession:endedAtPoint:operation:)])
        [source draggingSession:session endedAtPoint:info->_imageLocation operation:result];
    else if ([source respondsToSelector:@selector(draggedImage:endedAt:operation:)])
        [source draggedImage:image endedAt:info->_imageLocation operation:result];

    _localSource = nil;
    [info release];
    return result;
}

@end
