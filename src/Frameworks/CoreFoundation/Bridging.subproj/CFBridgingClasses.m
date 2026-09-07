#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>

/* CoreFoundation owns the toll-free-bridged NSNull class.  Foundation
 * consumes the public declaration but must not provide a second class. */
@interface NSNull : NSObject
+ (NSNull *)null;
- (NSString *)description;
@end

@implementation NSNull

+ (NSNull *)null {
    static NSNull *shared = nil;
    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

- (NSString *)description {
    return (NSString *)CFSTR("<null>");
}

@end

@interface __NSCFBoolean : NSObject
@end

@implementation __NSCFBoolean

/* A bridged kCFBooleanTrue/False reports this class, not NSNumber, so the
 * NSNumber accessors callers use on it have to be answered here. */
- (BOOL)boolValue {
    return CFBooleanGetValue((CFBooleanRef)self) ? YES : NO;
}

- (char)charValue {
    return (char)[self boolValue];
}

- (int)intValue {
    return (int)[self boolValue];
}

- (NSInteger)integerValue {
    return (NSInteger)[self boolValue];
}

- (unsigned int)unsignedIntValue {
    return (unsigned int)[self boolValue];
}

- (float)floatValue {
    return (float)[self boolValue];
}

- (double)doubleValue {
    return (double)[self boolValue];
}

- (NSString *)description {
    return (NSString *)([self boolValue] ? CFSTR("1") : CFSTR("0"));
}

@end

@interface __NSCFNumber : NSObject
@end

@implementation __NSCFNumber
@end
