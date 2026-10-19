// NSAppleEventDescriptor.m - Apple event descriptors: typed data, lists, records and events
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// there is no Apple event manager here, so events can be built and read but never sent
#import <Foundation/NSAppleEventDescriptor.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSError.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSProcessInfo.h>
#include <unistd.h>

enum {
    typeNull = 'null',
    typeAEList = 'list',
    typeAERecord = 'reco',
    typeAppleEvent = 'aevt',
    typeBoolean = 'bool',
    typeTrue = 'true',
    typeFalse = 'fals',
    typeSInt16 = 'shor',
    typeSInt32 = 'long',
    typeUInt32 = 'magn',
    typeSInt64 = 'comp',
    typeIEEE32BitFloatingPoint = 'sing',
    typeIEEE64BitFloatingPoint = 'doub',
    typeEnumerated = 'enum',
    typeType = 'type',
    typeKeyword = 'keyw',
    typeChar = 'TEXT',
    typeUnicodeText = 'utxt',
    typeUTF8Text = 'utf8',
    typeUTF16ExternalRepresentation = 'ut16',
    typeFileURL = 'furl',
    typeLongDateTime = 'ldt ',
    typeKernelProcessID = 'kpid',
    typeApplicationBundleID = 'bund',
    typeProcessSerialNumber = 'psn ',
    keyEventClassAttr = 'evcl',
    keyEventIDAttr = 'evid',
    keyReturnIDAttr = 'rtid',
    keyTransactionIDAttr = 'tran',
    keyAddressAttr = 'addr',
};

// seconds from 1904, the start of Mac time, to 2001, NSDate's reference date
static const double macEpochToReference = 3061152000.0;

@implementation NSAppleEventDescriptor

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (NSAppleEventDescriptor *)nullDescriptor {
    return [[[self alloc] initWithDescriptorType:typeNull data:nil] autorelease];
}

+ (NSAppleEventDescriptor *)listDescriptor {
    return [[[self alloc] initListDescriptor] autorelease];
}

+ (NSAppleEventDescriptor *)recordDescriptor {
    return [[[self alloc] initRecordDescriptor] autorelease];
}

+ (NSAppleEventDescriptor *)currentProcessDescriptor {
    return [self descriptorWithProcessIdentifier:getpid()];
}

+ (NSAppleEventDescriptor *)descriptorWithDescriptorType:(DescType)type bytes:(const void *)bytes length:(NSUInteger)length {
    return [[[self alloc] initWithDescriptorType:type bytes:bytes length:length] autorelease];
}

+ (NSAppleEventDescriptor *)descriptorWithDescriptorType:(DescType)type data:(NSData *)data {
    return [[[self alloc] initWithDescriptorType:type data:data] autorelease];
}

+ (NSAppleEventDescriptor *)descriptorWithBoolean:(Boolean)boolean {
    uint8_t value = boolean ? 1 : 0;
    return [self descriptorWithDescriptorType:typeBoolean bytes:&value length:1];
}

+ (NSAppleEventDescriptor *)descriptorWithEnumCode:(OSType)enumerator {
    return [self descriptorWithDescriptorType:typeEnumerated bytes:&enumerator length:sizeof(enumerator)];
}

+ (NSAppleEventDescriptor *)descriptorWithInt32:(SInt32)value {
    return [self descriptorWithDescriptorType:typeSInt32 bytes:&value length:sizeof(value)];
}

+ (NSAppleEventDescriptor *)descriptorWithDouble:(double)value {
    return [self descriptorWithDescriptorType:typeIEEE64BitFloatingPoint bytes:&value length:sizeof(value)];
}

+ (NSAppleEventDescriptor *)descriptorWithTypeCode:(OSType)typeCode {
    return [self descriptorWithDescriptorType:typeType bytes:&typeCode length:sizeof(typeCode)];
}

// strings are kept as host-order UTF-16, the way utxt is on macOS
+ (NSAppleEventDescriptor *)descriptorWithString:(NSString *)string {
    NSData *data = [string dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
    return [self descriptorWithDescriptorType:typeUnicodeText data:data ? data : [NSData data]];
}

+ (NSAppleEventDescriptor *)descriptorWithDate:(NSDate *)date {
    int64_t seconds = (int64_t)([date timeIntervalSinceReferenceDate] + macEpochToReference);
    return [self descriptorWithDescriptorType:typeLongDateTime bytes:&seconds length:sizeof(seconds)];
}

+ (NSAppleEventDescriptor *)descriptorWithFileURL:(NSURL *)fileURL {
    NSData *data = [[fileURL absoluteString] dataUsingEncoding:NSUTF8StringEncoding];
    return [self descriptorWithDescriptorType:typeFileURL data:data];
}

+ (NSAppleEventDescriptor *)descriptorWithProcessIdentifier:(pid_t)processIdentifier {
    return [self descriptorWithDescriptorType:typeKernelProcessID bytes:&processIdentifier length:sizeof(processIdentifier)];
}

+ (NSAppleEventDescriptor *)descriptorWithBundleIdentifier:(NSString *)bundleIdentifier {
    NSData *data = [bundleIdentifier dataUsingEncoding:NSUTF8StringEncoding];
    return [self descriptorWithDescriptorType:typeApplicationBundleID data:data];
}

+ (NSAppleEventDescriptor *)appleEventWithEventClass:(AEEventClass)eventClass eventID:(AEEventID)eventID
                                    targetDescriptor:(NSAppleEventDescriptor *)target returnID:(AEReturnID)returnID
                                       transactionID:(AETransactionID)transactionID {
    return [[[self alloc] initWithEventClass:eventClass eventID:eventID targetDescriptor:target returnID:returnID
                               transactionID:transactionID] autorelease];
}

- (instancetype)initWithDescriptorType:(DescType)type bytes:(const void *)bytes length:(NSUInteger)length {
    return [self initWithDescriptorType:type data:bytes ? [NSData dataWithBytes:bytes length:length] : nil];
}

- (instancetype)initWithDescriptorType:(DescType)type data:(NSData *)data {
    if ((self = [super init]) != nil) {
        _type = type;
        _data = [data copy];
        if (type == typeAEList || type == typeAERecord || type == typeAppleEvent) {
            _items = [[NSMutableArray alloc] init];
            _keywords = [[NSMutableArray alloc] init];
        }
        if (type == typeAppleEvent)
            _attributes = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (instancetype)init {
    return [self initWithDescriptorType:typeNull data:nil];
}

- (instancetype)initListDescriptor {
    return [self initWithDescriptorType:typeAEList data:nil];
}

- (instancetype)initRecordDescriptor {
    return [self initWithDescriptorType:typeAERecord data:nil];
}

- (instancetype)initWithEventClass:(AEEventClass)eventClass eventID:(AEEventID)eventID
                  targetDescriptor:(NSAppleEventDescriptor *)target returnID:(AEReturnID)returnID
                     transactionID:(AETransactionID)transactionID {
    if ((self = [self initWithDescriptorType:typeAppleEvent data:nil]) != nil) {
        SInt32 rtid = returnID;

        [self setAttributeDescriptor:[NSAppleEventDescriptor descriptorWithTypeCode:eventClass] forKeyword:keyEventClassAttr];
        [self setAttributeDescriptor:[NSAppleEventDescriptor descriptorWithTypeCode:eventID] forKeyword:keyEventIDAttr];
        [self setAttributeDescriptor:[NSAppleEventDescriptor descriptorWithDescriptorType:typeSInt32 bytes:&rtid length:4]
                          forKeyword:keyReturnIDAttr];
        [self setAttributeDescriptor:[NSAppleEventDescriptor descriptorWithInt32:transactionID] forKeyword:keyTransactionIDAttr];
        if (target != nil)
            [self setAttributeDescriptor:target forKeyword:keyAddressAttr];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    DescType type = (DescType)[coder decodeInt32ForKey:@"NS.type"];

    if ((self = [self initWithDescriptorType:type data:[coder decodeObjectForKey:@"NS.data"]]) != nil) {
        NSArray *items = [coder decodeObjectForKey:@"NS.items"];
        NSArray *keywords = [coder decodeObjectForKey:@"NS.keywords"];
        NSDictionary *attributes = [coder decodeObjectForKey:@"NS.attributes"];

        if (items != nil && _items != nil)
            [_items setArray:items];
        if (keywords != nil && _keywords != nil)
            [_keywords setArray:keywords];
        if (attributes != nil && _attributes != nil)
            [_attributes setDictionary:attributes];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeInt32:(int32_t)_type forKey:@"NS.type"];
    [coder encodeObject:_data forKey:@"NS.data"];
    [coder encodeObject:_items forKey:@"NS.items"];
    [coder encodeObject:_keywords forKey:@"NS.keywords"];
    [coder encodeObject:_attributes forKey:@"NS.attributes"];
}

- (void)dealloc {
    [_data release];
    [_items release];
    [_keywords release];
    [_attributes release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSAppleEventDescriptor *copy = [[NSAppleEventDescriptor alloc] initWithDescriptorType:_type data:_data];

    if (_items != nil) {
        [copy->_items setArray:_items];
        [copy->_keywords setArray:_keywords];
    }
    if (_attributes != nil)
        [copy->_attributes setDictionary:_attributes];
    return copy;
}

// no AEDesc exists behind these descriptors
- (const AEDesc *)aeDesc {
    return NULL;
}

- (DescType)descriptorType {
    return _type;
}

- (NSData *)data {
    return _data ? _data : [NSData data];
}

- (BOOL)_getBytes:(void *)buffer length:(NSUInteger)length {
    if ([_data length] < length)
        return NO;
    [_data getBytes:buffer length:length];
    return YES;
}

- (Boolean)booleanValue {
    uint8_t byte = 0;

    switch (_type) {
    case typeTrue: return true;
    case typeFalse: return false;
    case typeBoolean: return [self _getBytes:&byte length:1] && byte != 0;
    default: return [self int32Value] != 0;
    }
}

- (OSType)enumCodeValue {
    OSType code = 0;
    [self _getBytes:&code length:sizeof(code)];
    return code;
}

- (OSType)typeCodeValue {
    return [self enumCodeValue];
}

- (SInt32)int32Value {
    SInt16 shortValue;
    SInt32 longValue;
    SInt64 wideValue;
    float floatValue;
    double doubleValue;
    uint8_t byte;

    switch (_type) {
    case typeSInt16: return [self _getBytes:&shortValue length:2] ? shortValue : 0;
    case typeSInt32: case typeUInt32: return [self _getBytes:&longValue length:4] ? longValue : 0;
    case typeSInt64: return [self _getBytes:&wideValue length:8] ? (SInt32)wideValue : 0;
    case typeIEEE32BitFloatingPoint: return [self _getBytes:&floatValue length:4] ? (SInt32)floatValue : 0;
    case typeIEEE64BitFloatingPoint: return [self _getBytes:&doubleValue length:8] ? (SInt32)doubleValue : 0;
    case typeBoolean: return [self _getBytes:&byte length:1] ? byte : 0;
    case typeTrue: return 1;
    case typeUnicodeText: case typeUTF8Text: case typeChar: return [[self stringValue] intValue];
    default: return 0;
    }
}

- (double)doubleValue {
    double value;
    float floatValue;

    if (_type == typeIEEE64BitFloatingPoint && [self _getBytes:&value length:8])
        return value;
    if (_type == typeIEEE32BitFloatingPoint && [self _getBytes:&floatValue length:4])
        return floatValue;
    if (_type == typeUnicodeText || _type == typeUTF8Text || _type == typeChar)
        return [[self stringValue] doubleValue];
    return [self int32Value];
}

- (NSString *)stringValue {
    switch (_type) {
    case typeUnicodeText: return [[[NSString alloc] initWithData:[self data] encoding:NSUTF16LittleEndianStringEncoding] autorelease];
    case typeUTF16ExternalRepresentation: return [[[NSString alloc] initWithData:[self data] encoding:NSUTF16StringEncoding] autorelease];
    case typeUTF8Text: case typeFileURL: case typeApplicationBundleID:
        return [[[NSString alloc] initWithData:[self data] encoding:NSUTF8StringEncoding] autorelease];
    case typeChar: return [[[NSString alloc] initWithData:[self data] encoding:NSMacOSRomanStringEncoding] autorelease];
    case typeSInt16: case typeSInt32: case typeUInt32: case typeSInt64: return [NSString stringWithFormat:@"%d", [self int32Value]];
    case typeIEEE32BitFloatingPoint: case typeIEEE64BitFloatingPoint: return [NSString stringWithFormat:@"%g", [self doubleValue]];
    case typeTrue: return @"true";
    case typeFalse: return @"false";
    case typeBoolean: return [self booleanValue] ? @"true" : @"false";
    default: return nil;
    }
}

- (NSDate *)dateValue {
    int64_t seconds;

    if (_type != typeLongDateTime || ![self _getBytes:&seconds length:8])
        return nil;
    return [NSDate dateWithTimeIntervalSinceReferenceDate:seconds - macEpochToReference];
}

- (NSURL *)fileURLValue {
    NSString *string;

    if (_type != typeFileURL && _type != typeUnicodeText && _type != typeUTF8Text)
        return nil;
    string = [self stringValue];
    if ([string hasPrefix:@"/"])
        return [NSURL fileURLWithPath:string];
    return [NSURL URLWithString:string];
}

- (OSType)_attributeCode:(AEKeyword)keyword {
    return [[self attributeDescriptorForKeyword:keyword] typeCodeValue];
}

- (AEEventClass)eventClass { return [self _attributeCode:keyEventClassAttr]; }
- (AEEventID)eventID { return [self _attributeCode:keyEventIDAttr]; }
- (AEReturnID)returnID { return (AEReturnID)[[self attributeDescriptorForKeyword:keyReturnIDAttr] int32Value]; }
- (AETransactionID)transactionID { return [[self attributeDescriptorForKeyword:keyTransactionIDAttr] int32Value]; }

- (NSInteger)numberOfItems {
    return _items ? (NSInteger)[_items count] : 0;
}

- (BOOL)isRecordDescriptor {
    return _type == typeAERecord;
}

- (NSUInteger)_indexOfKeyword:(AEKeyword)keyword {
    return [_keywords indexOfObject:[NSNumber numberWithUnsignedInt:keyword]];
}

- (void)setDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword {
    NSUInteger index;

    if (_items == nil || _type == typeAEList || descriptor == nil)
        return;
    index = [self _indexOfKeyword:keyword];
    if (index != NSNotFound) {
        [_items replaceObjectAtIndex:index withObject:descriptor];
    } else {
        [_items addObject:descriptor];
        [_keywords addObject:[NSNumber numberWithUnsignedInt:keyword]];
    }
}

- (NSAppleEventDescriptor *)descriptorForKeyword:(AEKeyword)keyword {
    NSUInteger index = [self _indexOfKeyword:keyword];
    return (_items != nil && index != NSNotFound) ? [_items objectAtIndex:index] : nil;
}

- (void)removeDescriptorWithKeyword:(AEKeyword)keyword {
    NSUInteger index = [self _indexOfKeyword:keyword];

    if (_items == nil || index == NSNotFound)
        return;
    [_items removeObjectAtIndex:index];
    [_keywords removeObjectAtIndex:index];
}

- (AEKeyword)keywordForDescriptorAtIndex:(NSInteger)index {
    if (_type == typeAEList || index < 1 || index > (NSInteger)[_keywords count])
        return 0;
    return [[_keywords objectAtIndex:index - 1] unsignedIntValue];
}

- (void)setParamDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword {
    [self setDescriptor:descriptor forKeyword:keyword];
}

- (NSAppleEventDescriptor *)paramDescriptorForKeyword:(AEKeyword)keyword {
    return [self descriptorForKeyword:keyword];
}

- (void)removeParamDescriptorWithKeyword:(AEKeyword)keyword {
    [self removeDescriptorWithKeyword:keyword];
}

- (void)setAttributeDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword {
    if (_attributes != nil && descriptor != nil)
        [_attributes setObject:descriptor forKey:[NSNumber numberWithUnsignedInt:keyword]];
}

- (NSAppleEventDescriptor *)attributeDescriptorForKeyword:(AEKeyword)keyword {
    return [_attributes objectForKey:[NSNumber numberWithUnsignedInt:keyword]];
}

// list positions start at 1, and 0 or a position past the end appends
- (void)insertDescriptor:(NSAppleEventDescriptor *)descriptor atIndex:(NSInteger)index {
    if (_items == nil || descriptor == nil)
        return;
    if (_type != typeAEList) {
        [self setDescriptor:descriptor forKeyword:0];
        return;
    }
    if (index < 1 || index > (NSInteger)[_items count])
        [_items addObject:descriptor];
    else
        [_items replaceObjectAtIndex:index - 1 withObject:descriptor];
}

- (NSAppleEventDescriptor *)descriptorAtIndex:(NSInteger)index {
    if (_items == nil || index < 1 || index > (NSInteger)[_items count])
        return nil;
    return [_items objectAtIndex:index - 1];
}

- (void)removeDescriptorAtIndex:(NSInteger)index {
    if (_items == nil || index < 1 || index > (NSInteger)[_items count])
        return;
    [_items removeObjectAtIndex:index - 1];
    if (_type != typeAEList)
        [_keywords removeObjectAtIndex:index - 1];
}

- (NSAppleEventDescriptor *)coerceToDescriptorType:(DescType)type {
    NSString *string;

    if (type == _type || type == '****')
        return [[self copy] autorelease];
    switch (type) {
    case typeUnicodeText:
        string = [self stringValue];
        return string ? [NSAppleEventDescriptor descriptorWithString:string] : nil;
    case typeUTF8Text: case typeChar:
        string = [self stringValue];
        return string ? [NSAppleEventDescriptor descriptorWithDescriptorType:type
                                                                        data:[string dataUsingEncoding:type == typeChar ? NSMacOSRomanStringEncoding
                                                                                                                         : NSUTF8StringEncoding]]
                      : nil;
    case typeSInt32: return [NSAppleEventDescriptor descriptorWithInt32:[self int32Value]];
    case typeIEEE64BitFloatingPoint: return [NSAppleEventDescriptor descriptorWithDouble:[self doubleValue]];
    case typeBoolean: return [NSAppleEventDescriptor descriptorWithBoolean:[self booleanValue]];
    case typeFileURL: return [self fileURLValue] ? [NSAppleEventDescriptor descriptorWithFileURL:[self fileURLValue]] : nil;
    default: return nil;
    }
}

- (NSAppleEventDescriptor *)sendEventWithOptions:(NSAppleEventSendOptions)options timeout:(NSTimeInterval)timeout error:(NSError **)error {
    // procNotFound, there is no Apple event manager to deliver to
    if (error != NULL)
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:-600 userInfo:nil];
    return nil;
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSAppleEventDescriptor class]])
        return NO;
    NSAppleEventDescriptor *descriptor = other;
    return _type == descriptor->_type && [[self data] isEqual:[descriptor data]] &&
           (_items == descriptor->_items || [_items isEqual:descriptor->_items]) &&
           (_keywords == descriptor->_keywords || [_keywords isEqual:descriptor->_keywords]);
}

- (NSUInteger)hash {
    return _type ^ [_data hash];
}

- (NSString *)description {
    char code[5] = { (char)(_type >> 24), (char)(_type >> 16), (char)(_type >> 8), (char)_type, 0 };
    return [NSString stringWithFormat:@"<NSAppleEventDescriptor: '%s' %@>", code, _items ? (id)_items : (id)[self stringValue]];
}

@end
