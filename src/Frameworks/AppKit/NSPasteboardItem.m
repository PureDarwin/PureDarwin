// NSPasteboardItem.m - one item on a pasteboard, and the modern pasteboard reading and writing API
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSPasteboardItem.h>
#import <Foundation/Foundation.h>

@implementation NSPasteboardItem

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    _values = [[NSMutableDictionary alloc] init];
    _providers = [[NSMutableDictionary alloc] init];
    _types = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc
{
    [_values release];
    [_providers release];
    [_types release];
    [super dealloc];
}

- (NSArray *)types
{
    return [[_types copy] autorelease];
}

- (void)_noteType:(NSString *)type
{
    if (![_types containsObject:type])
        [_types addObject:type];
}

- (NSString *)availableTypeFromArray:(NSArray *)types
{
    for (NSString *type in types) {
        if ([_types containsObject:type])
            return type;
    }
    return nil;
}

// the provider is asked for the data the first time a type is read
- (BOOL)setDataProvider:(id<NSPasteboardItemDataProvider>)dataProvider forTypes:(NSArray *)types
{
    for (NSString *type in types) {
        [_providers setObject:dataProvider forKey:type];
        [self _noteType:type];
    }
    return YES;
}

- (BOOL)setData:(NSData *)data forType:(NSString *)type
{
    if (data == nil || type == nil)
        return NO;
    [_values setObject:data forKey:type];
    [self _noteType:type];
    return YES;
}

- (BOOL)setString:(NSString *)string forType:(NSString *)type
{
    return [self setData:[string dataUsingEncoding:NSUTF8StringEncoding] forType:type];
}

- (BOOL)setPropertyList:(id)propertyList forType:(NSString *)type
{
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:propertyList format:NSPropertyListXMLFormat_v1_0 options:0 error:NULL];

    return [self setData:data forType:type];
}

- (NSData *)dataForType:(NSString *)type
{
    NSData *data = [_values objectForKey:type];
    id provider;

    if (data == nil && (provider = [_providers objectForKey:type]) != nil) {
        [provider pasteboard:nil item:self provideDataForType:type];
        data = [_values objectForKey:type];
    }
    return data;
}

- (NSString *)stringForType:(NSString *)type
{
    NSData *data = [self dataForType:type];

    return data ? [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease] : nil;
}

- (id)propertyListForType:(NSString *)type
{
    NSData *data = [self dataForType:type];

    return data ? [NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL] : nil;
}

- (NSArray *)writableTypesForPasteboard:(NSPasteboard *)pasteboard
{
    return [self types];
}

- (id)pasteboardPropertyListForType:(NSString *)type
{
    return [self dataForType:type];
}

+ (NSArray *)readableTypesForPasteboard:(NSPasteboard *)pasteboard
{
    return [pasteboard types];
}

- (id)initWithPasteboardPropertyList:(id)propertyList ofType:(NSString *)type
{
    if ((self = [self init]) == nil)
        return nil;
    if ([propertyList isKindOfClass:[NSData class]])
        [self setData:propertyList forType:type];
    else if ([propertyList isKindOfClass:[NSString class]])
        [self setString:propertyList forType:type];
    else if (propertyList != nil)
        [self setPropertyList:propertyList forType:type];
    return self;
}

@end

// the types and values an object puts on a pasteboard, including the classes that predate NSPasteboardWriting
static NSDictionary *PDWritableValues(id object, NSPasteboard *pasteboard)
{
    NSMutableDictionary *values = [NSMutableDictionary dictionary];

    if ([object isKindOfClass:[NSString class]]) {
        [values setObject:object forKey:NSPasteboardTypeString];
    } else if ([object isKindOfClass:[NSURL class]]) {
        [values setObject:[object absoluteString] forKey:[object isFileURL] ? NSPasteboardTypeFileURL : NSPasteboardTypeURL];
        if ([object isFileURL])
            [values setObject:@[ [object path] ] forKey:NSFilenamesPboardType];
        [values setObject:[object absoluteString] forKey:NSPasteboardTypeString];
    } else if ([object conformsToProtocol:@protocol(NSPasteboardWriting)]) {
        for (NSString *type in [object writableTypesForPasteboard:pasteboard]) {
            id value = [object pasteboardPropertyListForType:type];
            if (value != nil)
                [values setObject:value forKey:type];
        }
    }
    return values;
}

@implementation NSPasteboard (PasteboardItems)

// this pasteboard holds one set of typed data, shown as one item
- (NSArray *)pasteboardItems
{
    NSPasteboardItem *item = [[[NSPasteboardItem alloc] init] autorelease];

    for (NSString *type in [self types]) {
        NSData *data = [self dataForType:type];
        if (data != nil)
            [item setData:data forType:type];
    }
    return [[item types] count] ? @[ item ] : @[];
}

- (BOOL)writeObjects:(NSArray *)objects
{
    NSMutableDictionary *merged = [NSMutableDictionary dictionary];
    NSMutableArray *order = [NSMutableArray array];

    for (id object in objects) {
        NSDictionary *values = PDWritableValues(object, self);
        for (NSString *type in values) {
            id existing = [merged objectForKey:type];

            // every file of a multiple selection goes into the one filenames list
            if (existing != nil && [type isEqualToString:NSFilenamesPboardType])
                [merged setObject:[existing arrayByAddingObjectsFromArray:[values objectForKey:type]] forKey:type];
            else if (existing == nil) {
                [merged setObject:[values objectForKey:type] forKey:type];
                [order addObject:type];
            }
        }
    }
    if ([order count] == 0)
        return NO;
    [self addTypes:order owner:nil];
    for (NSString *type in order) {
        id value = [merged objectForKey:type];
        if ([value isKindOfClass:[NSData class]])
            [self setData:value forType:type];
        else if ([value isKindOfClass:[NSString class]])
            [self setString:value forType:type];
        else
            [self setPropertyList:value forType:type];
    }
    return YES;
}

- (id)_objectOfClass:(Class)cls
{
    if (cls == [NSString class])
        return [self stringForType:NSPasteboardTypeString];
    if (cls == [NSURL class]) {
        NSArray *files = [self propertyListForType:NSFilenamesPboardType];
        NSString *url = [self stringForType:NSPasteboardTypeFileURL] ?: [self stringForType:NSPasteboardTypeURL];
        if ([files count])
            return [NSURL fileURLWithPath:[files objectAtIndex:0]];
        return url ? [NSURL URLWithString:url] : nil;
    }
    if (cls == [NSPasteboardItem class])
        return [[self pasteboardItems] firstObject];
    if ([cls conformsToProtocol:@protocol(NSPasteboardReading)]) {
        for (NSString *type in [cls readableTypesForPasteboard:self]) {
            NSData *data = [self dataForType:type];
            if (data != nil)
                return [[[cls alloc] initWithPasteboardPropertyList:data ofType:type] autorelease];
        }
    }
    return nil;
}

- (NSArray *)readObjectsForClasses:(NSArray *)classArray options:(NSDictionary *)options
{
    // a filenames list gives one file URL per file
    if ([classArray containsObject:[NSURL class]]) {
        NSArray *files = [[self types] containsObject:NSFilenamesPboardType] ? [self propertyListForType:NSFilenamesPboardType] : nil;
        if ([files count] > 1) {
            NSMutableArray *urls = [NSMutableArray array];
            for (NSString *path in files)
                [urls addObject:[NSURL fileURLWithPath:path]];
            return urls;
        }
    }
    for (Class cls in classArray) {
        id object = [self _objectOfClass:cls];
        if (object != nil)
            return @[ object ];
    }
    return @[];
}

- (BOOL)canReadObjectForClasses:(NSArray *)classArray options:(NSDictionary *)options
{
    return [[self readObjectsForClasses:classArray options:options] count] > 0;
}

- (BOOL)canReadItemWithDataConformingToTypes:(NSArray *)types
{
    return [self availableTypeFromArray:types] != nil;
}

@end
