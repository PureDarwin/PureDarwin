// NSAppleEventDescriptor.h - Apple event descriptors: typed data, lists, records and events
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>
#include <MacTypes.h>

@class NSData, NSString, NSURL, NSDate, NSError, NSMutableArray, NSMutableDictionary;

#ifndef __AEDATAMODEL__
typedef FourCharCode DescType;
typedef FourCharCode AEKeyword;
typedef FourCharCode AEEventClass;
typedef FourCharCode AEEventID;
typedef SInt16 AEReturnID;
typedef SInt32 AETransactionID;
typedef struct AEDesc AEDesc;
#endif

typedef NS_OPTIONS(NSUInteger, NSAppleEventSendOptions) {
    NSAppleEventSendNoReply = 0x00000001,
    NSAppleEventSendQueueReply = 0x00000002,
    NSAppleEventSendWaitForReply = 0x00000003,
    NSAppleEventSendNeverInteract = 0x00000010,
    NSAppleEventSendCanInteract = 0x00000020,
    NSAppleEventSendAlwaysInteract = 0x00000030,
    NSAppleEventSendCanSwitchLayer = 0x00000040,
    NSAppleEventSendDontRecord = 0x00001000,
    NSAppleEventSendDontExecute = 0x00002000,
    NSAppleEventSendDontAnnotate = 0x00010000,
    NSAppleEventSendDefaultOptions = NSAppleEventSendWaitForReply | NSAppleEventSendCanInteract,
};

@interface NSAppleEventDescriptor : NSObject <NSCopying, NSSecureCoding> {
    DescType _type;
    NSData *_data;
    // list items, or the keywords and values of a record or an event's parameters
    NSMutableArray *_items;
    NSMutableArray *_keywords;
    NSMutableDictionary *_attributes;
}

+ (NSAppleEventDescriptor *)nullDescriptor;
+ (NSAppleEventDescriptor *)listDescriptor;
+ (NSAppleEventDescriptor *)recordDescriptor;
+ (NSAppleEventDescriptor *)currentProcessDescriptor;
+ (NSAppleEventDescriptor *)descriptorWithDescriptorType:(DescType)descriptorType bytes:(const void *)bytes length:(NSUInteger)byteCount;
+ (NSAppleEventDescriptor *)descriptorWithDescriptorType:(DescType)descriptorType data:(NSData *)data;
+ (NSAppleEventDescriptor *)descriptorWithBoolean:(Boolean)boolean;
+ (NSAppleEventDescriptor *)descriptorWithEnumCode:(OSType)enumerator;
+ (NSAppleEventDescriptor *)descriptorWithInt32:(SInt32)signedInt;
+ (NSAppleEventDescriptor *)descriptorWithDouble:(double)doubleValue;
+ (NSAppleEventDescriptor *)descriptorWithTypeCode:(OSType)typeCode;
+ (NSAppleEventDescriptor *)descriptorWithString:(NSString *)string;
+ (NSAppleEventDescriptor *)descriptorWithDate:(NSDate *)date;
+ (NSAppleEventDescriptor *)descriptorWithFileURL:(NSURL *)fileURL;
+ (NSAppleEventDescriptor *)descriptorWithProcessIdentifier:(pid_t)processIdentifier;
+ (NSAppleEventDescriptor *)descriptorWithBundleIdentifier:(NSString *)bundleIdentifier;
+ (NSAppleEventDescriptor *)appleEventWithEventClass:(AEEventClass)eventClass eventID:(AEEventID)eventID
                                    targetDescriptor:(NSAppleEventDescriptor *)targetDescriptor returnID:(AEReturnID)returnID
                                       transactionID:(AETransactionID)transactionID;

- (instancetype)initWithDescriptorType:(DescType)descriptorType bytes:(const void *)bytes length:(NSUInteger)byteCount;
- (instancetype)initWithDescriptorType:(DescType)descriptorType data:(NSData *)data;
- (instancetype)initListDescriptor;
- (instancetype)initRecordDescriptor;
- (instancetype)initWithEventClass:(AEEventClass)eventClass eventID:(AEEventID)eventID
                  targetDescriptor:(NSAppleEventDescriptor *)targetDescriptor returnID:(AEReturnID)returnID
                     transactionID:(AETransactionID)transactionID;

@property (readonly) const AEDesc *aeDesc;
@property (readonly) DescType descriptorType;
@property (readonly, copy) NSData *data;
@property (readonly) Boolean booleanValue;
@property (readonly) OSType enumCodeValue;
@property (readonly) SInt32 int32Value;
@property (readonly) double doubleValue;
@property (readonly) OSType typeCodeValue;
@property (readonly, copy) NSString *stringValue;
@property (readonly, copy) NSDate *dateValue;
@property (readonly, copy) NSURL *fileURLValue;
@property (readonly) AEEventClass eventClass;
@property (readonly) AEEventID eventID;
@property (readonly) AEReturnID returnID;
@property (readonly) AETransactionID transactionID;
@property (readonly) NSInteger numberOfItems;
@property (readonly) BOOL isRecordDescriptor;

- (void)setParamDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword;
- (NSAppleEventDescriptor *)paramDescriptorForKeyword:(AEKeyword)keyword;
- (void)removeParamDescriptorWithKeyword:(AEKeyword)keyword;
- (void)setAttributeDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword;
- (NSAppleEventDescriptor *)attributeDescriptorForKeyword:(AEKeyword)keyword;

- (void)insertDescriptor:(NSAppleEventDescriptor *)descriptor atIndex:(NSInteger)index;
- (NSAppleEventDescriptor *)descriptorAtIndex:(NSInteger)index;
- (void)removeDescriptorAtIndex:(NSInteger)index;

- (void)setDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword;
- (NSAppleEventDescriptor *)descriptorForKeyword:(AEKeyword)keyword;
- (void)removeDescriptorWithKeyword:(AEKeyword)keyword;
- (AEKeyword)keywordForDescriptorAtIndex:(NSInteger)index;

- (NSAppleEventDescriptor *)coerceToDescriptorType:(DescType)descriptorType;
- (NSAppleEventDescriptor *)sendEventWithOptions:(NSAppleEventSendOptions)sendOptions timeout:(NSTimeInterval)timeoutInSeconds
                                           error:(NSError **)error;

@end
