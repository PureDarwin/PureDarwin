//
//  SecNSAdditions.h
//  Security
//

#ifndef _SECNSADDITIONS_H_
#define _SECNSADDITIONS_H_

#import <Foundation/Foundation.h>

static inline BOOL NSIsEqualSafe(NSObject* obj1, NSObject* obj2) {
    return obj1 == nil ? (obj2 == nil) : [obj1 isEqual:obj2];
}


// MARK: NSArray

@interface NSArray (compactDescription)
- (NSMutableString*) concatenateWithSeparator: (NSString*) separator;
@end

@interface NSDictionary (SOSDictionaryFormat)
- (NSString*) compactDescription;
@end

@interface NSMutableDictionary (FindAndRemove)
-(NSObject*)extractObjectForKey:(NSString*)key;
@end

// MARK: NSSet

@interface NSSet (Emptiness)
- (bool) isEmpty;
@end

@interface NSSet (HasElements)
- (bool) containsElementsNotIn: (NSSet*) other;
@end

@interface NSSet (compactDescription)
- (NSString*) shortDescription;
@end

@interface NSSet (Stringizing)
- (NSString*) sortedElementsJoinedByString: (NSString*) separator;
- (NSString*) sortedElementsTruncated: (NSUInteger) length JoinedByString: (NSString*) separator;
@end



// MARK: NSString

static inline NSString* asNSString(NSObject* object) {
    return [object isKindOfClass:[NSString class]] ? (NSString*) object : nil;
}

@interface NSString (FileOutput)
- (void) writeTo: (FILE*) file;
- (void) writeToStdOut;
- (void) writeToStdErr;
@end

// MARK: NSData

@interface NSData (Hexinization)
- (NSString*) asHexString;
@end

@interface NSMutableData (filledAndClipped)
+ (instancetype) dataWithSpace: (NSUInteger) initialSize DEREncode: (uint8_t*(^)(size_t size, uint8_t *buffer)) initialization;
@end

#endif /* SecNSAdditions_h */
