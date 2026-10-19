// NSDraggingItem.h - the items, image components and session of a drag
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSView.h>
#import <AppKit/NSDragging.h>

@class NSArray, NSString, NSImage, NSPasteboard, NSEvent;
@protocol NSPasteboardWriting;

APPKIT_EXPORT NSString *const NSDraggingImageComponentIconKey;
APPKIT_EXPORT NSString *const NSDraggingImageComponentLabelKey;

typedef NS_ENUM(NSInteger, NSDraggingFormation) {
    NSDraggingFormationDefault = 0,
    NSDraggingFormationNone,
    NSDraggingFormationPile,
    NSDraggingFormationList,
    NSDraggingFormationStack,
};

typedef NS_ENUM(NSInteger, NSDraggingContext) {
    NSDraggingContextOutsideApplication = 0,
    NSDraggingContextWithinApplication,
};

typedef NS_OPTIONS(NSUInteger, NSDraggingItemEnumerationOptions) {
    NSDraggingItemEnumerationConcurrent = 1 << 0,
    NSDraggingItemEnumerationClearNonenumeratedImages = 1 << 16,
};

@interface NSDraggingImageComponent : NSObject
+ (instancetype)draggingImageComponentWithKey:(NSString *)key;
- (instancetype)initWithKey:(NSString *)key;
@property(copy) NSString *key;
@property(retain) id contents;
@property NSRect frame;
@end

@interface NSDraggingItem : NSObject {
    id _item;
    NSRect _draggingFrame;
    id _contents;
    NSArray *(^_imageComponentsProvider)(void);
}
- (instancetype)initWithPasteboardWriter:(id<NSPasteboardWriting>)pasteboardWriter;
@property(readonly, retain) id item;
@property NSRect draggingFrame;
@property(copy) NSArray *(^imageComponentsProvider)(void);
@property(readonly, copy) NSArray *imageComponents;
- (void)setDraggingFrame:(NSRect)frame contents:(id)contents;
@end

@interface NSDraggingSession : NSObject {
    NSArray *_items;
    id _source;
    NSPasteboard *_pasteboard;
}
@property NSDraggingFormation draggingFormation;
@property BOOL animatesToStartingPositionsOnCancelOrFail;
@property NSInteger draggingLeaderIndex;
@property(readonly) NSPasteboard *draggingPasteboard;
@property(readonly) NSInteger draggingSequenceNumber;
@property(readonly) NSPoint draggingLocation;
- (void)enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)options forView:(NSView *)view classes:(NSArray *)classArray
                            searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(NSDraggingItem *item, NSInteger index, BOOL *stop))block;
@end

@protocol NSDraggingSource <NSObject>
- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context;
@optional
- (void)draggingSession:(NSDraggingSession *)session willBeginAtPoint:(NSPoint)screenPoint;
- (void)draggingSession:(NSDraggingSession *)session movedToPoint:(NSPoint)screenPoint;
- (void)draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation;
- (BOOL)ignoreModifierKeysForDraggingSession:(NSDraggingSession *)session;
@end

@interface NSView (DraggingSession)
- (NSDraggingSession *)beginDraggingSessionWithItems:(NSArray *)items event:(NSEvent *)event source:(id<NSDraggingSource>)source;
@end
