// NSPasteboardItem.h - one item on a pasteboard, and the modern pasteboard reading and writing API
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <AppKit/NSPasteboard.h>

@class NSArray, NSData, NSString, NSDictionary, NSMutableDictionary, NSMutableArray;
@protocol NSPasteboardItemDataProvider;

typedef NS_OPTIONS(NSUInteger, NSPasteboardWritingOptions) {
    NSPasteboardWritingPromised = 1 << 9,
};

typedef NS_OPTIONS(NSUInteger, NSPasteboardReadingOptions) {
    NSPasteboardReadingAsData = 0,
    NSPasteboardReadingAsString = 1 << 0,
    NSPasteboardReadingAsPropertyList = 1 << 1,
    NSPasteboardReadingAsKeyedArchive = 1 << 2,
};

@protocol NSPasteboardWriting <NSObject>
- (NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard;
- (id)pasteboardPropertyListForType:(NSString *)type;
@optional
- (NSPasteboardWritingOptions)writingOptionsForType:(NSString *)type pasteboard:(NSPasteboard *)pasteboard;
@end

@protocol NSPasteboardReading <NSObject>
+ (NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard;
@optional
+ (NSPasteboardReadingOptions)readingOptionsForType:(NSString *)type pasteboard:(NSPasteboard *)pasteboard;
- (id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSString *)type;
@end

@interface NSPasteboardItem : NSObject <NSPasteboardWriting, NSPasteboardReading> {
    NSMutableDictionary *_values;
    NSMutableDictionary *_providers;
    NSMutableArray *_types;
}

@property(readonly, copy) NSArray *types;

- (NSString *)availableTypeFromArray:(NSArray *)types;
- (BOOL)setDataProvider:(id<NSPasteboardItemDataProvider>)dataProvider forTypes:(NSArray *)types;
- (BOOL)setData:(NSData *)data forType:(NSString *)type;
- (BOOL)setString:(NSString *)string forType:(NSString *)type;
- (BOOL)setPropertyList:(id)propertyList forType:(NSString *)type;
- (NSData *)dataForType:(NSString *)type;
- (NSString *)stringForType:(NSString *)type;
- (id)propertyListForType:(NSString *)type;

@end

@protocol NSPasteboardItemDataProvider <NSObject>
- (void)pasteboard:(NSPasteboard *)pasteboard item:(NSPasteboardItem *)item provideDataForType:(NSString *)type;
@optional
- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard;
@end

@interface NSPasteboard (PasteboardItems)
@property(readonly, copy) NSArray *pasteboardItems;
- (BOOL)writeObjects:(NSArray *)objects;
- (NSArray *)readObjectsForClasses:(NSArray *)classArray options:(NSDictionary *)options;
- (BOOL)canReadObjectForClasses:(NSArray *)classArray options:(NSDictionary *)options;
- (BOOL)canReadItemWithDataConformingToTypes:(NSArray *)types;
@end
