// CFObjCDispatch.h - what CF sends to Objective-C objects that are not CF instances
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// included by CFInternal.h for the files that define CF_OBJC_DISPATCH_ENABLED
// the receivers are Foundation's abstract classes or any subclass of them (Swift's bridged collections included)

#if !defined(__COREFOUNDATION_CFOBJCDISPATCH__)
#define __COREFOUNDATION_CFOBJCDISPATCH__ 1

#import <objc/NSObject.h>
#import <objc/runtime.h>
#import <Foundation/NSRange.h>

@class NSString, NSMutableString, NSArray, NSMutableArray, NSDictionary, NSMutableDictionary, NSSet, NSMutableSet;
@class NSData, NSMutableData, NSError;

typedef unsigned short unichar;
typedef NSUInteger NSStringCompareOptions;

// the selectors CF sends, with the types Foundation implements them with
@protocol __CFObjCDispatchTargets <NSObject>

// generic
- (CFTypeID)_cfTypeID;
- (id)copyWithZone:(void *)zone;
- (id)copy;
- (id)mutableCopy;

// NSString
- (NSUInteger)length;
- (unichar)characterAtIndex:(NSUInteger)index;
- (void)getCharacters:(unichar *)buffer range:(NSRange)range;
- (BOOL)_encodingCantBeStoredInEightBitCFString;
- (const char *)_fastCStringContents:(BOOL)nullTerminationRequired;
- (const unichar *)_fastCharacterContents;
- (BOOL)_getCString:(char *)buffer maxLength:(NSUInteger)maxLength encoding:(CFStringEncoding)encoding;
- (void)getLineStart:(NSUInteger *)start end:(NSUInteger *)end contentsEnd:(NSUInteger *)contentsEnd forRange:(NSRange)range;
- (void)getParagraphStart:(NSUInteger *)start end:(NSUInteger *)end contentsEnd:(NSUInteger *)contentsEnd forRange:(NSRange)range;
- (CFStringEncoding)_smallestEncodingInCFStringEncoding;
- (CFStringEncoding)_fastestEncodingInCFStringEncoding;
- (id)_createSubstringWithRange:(NSRange)range;

// NSMutableString
- (void)insertString:(NSString *)string atIndex:(NSUInteger)index;
- (void)deleteCharactersInRange:(NSRange)range;
- (void)replaceCharactersInRange:(NSRange)range withString:(NSString *)string;
- (void)setString:(NSString *)string;
- (void)appendString:(NSString *)string;
- (void)appendCharacters:(const unichar *)characters length:(NSUInteger)length;
- (void)_cfAppendCString:(const unsigned char *)cString length:(NSInteger)length;
- (NSUInteger)replaceOccurrencesOfString:(NSString *)target withString:(NSString *)replacement options:(NSStringCompareOptions)options range:(NSRange)range;
- (void)_cfPad:(CFStringRef)padString length:(uint32_t)length padIndex:(uint32_t)padIndex;
- (void)_cfTrim:(CFStringRef)trimString;
- (void)_cfTrimWS;
- (void)_cfLowercase:(const void *)locale;
- (void)_cfUppercase:(const void *)locale;
- (void)_cfCapitalize:(const void *)locale;
- (void)_cfNormalize:(CFStringNormalizationForm)form;

// NSArray and NSMutableArray
- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)index;
- (void)getObjects:(id *)objects range:(NSRange)range;
- (void)addObject:(id)object;
- (void)setObject:(id)object atIndex:(NSUInteger)index;
- (void)insertObject:(id)object atIndex:(NSUInteger)index;
- (void)exchangeObjectAtIndex:(NSUInteger)index1 withObjectAtIndex:(NSUInteger)index2;
- (void)removeObjectAtIndex:(NSUInteger)index;
- (void)removeAllObjects;
- (void)replaceObjectsInRange:(NSRange)range withObjects:(id *)objects count:(NSUInteger)count;

// NSDictionary, NSSet and their mutable forms
- (NSUInteger)countForKey:(id)key;
- (NSUInteger)countForObject:(id)object;
- (BOOL)containsKey:(id)key;
- (BOOL)containsObject:(id)object;
- (id)objectForKey:(id)key;
- (id)member:(id)object;
- (BOOL)__getValue:(id *)value forKey:(id)key;
- (BOOL)__getValue:(id *)value forObj:(id)object;
- (void)getObjects:(id *)objects andKeys:(id *)keys;
- (void)getObjects:(id *)objects;
- (void)__apply:(void (*)(const void *, const void *, void *))applier context:(void *)context;
- (void)__applyValues:(void (*)(const void *, void *))applier context:(void *)context;
- (void)__addObject:(id)object forKey:(id)key;
- (void)replaceObject:(id)object forKey:(id)key;
- (void)replaceObject:(id)object;
- (void)__setObject:(id)object forKey:(id)key;
- (void)setObject:(id)object;
- (void)removeObjectForKey:(id)key;
- (void)removeObject:(id)object;

// NSData and NSMutableData
- (const void *)bytes;
- (void *)mutableBytes;
- (void)getBytes:(void *)buffer range:(NSRange)range;
- (void)setLength:(NSUInteger)length;
- (void)increaseLengthBy:(NSUInteger)extraLength;
- (void)appendBytes:(const void *)bytes length:(NSUInteger)length;
- (void)replaceBytesInRange:(NSRange)range withBytes:(const void *)bytes length:(NSUInteger)length;

// NSError
- (id)userInfo;
- (id)domain;
- (NSInteger)code;
- (id)localizedDescription;
- (id)localizedFailureReason;
- (id)localizedRecoverySuggestion;

@end

#endif
