#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>

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
@end

@interface __NSCFNumber : NSObject
@end

@implementation __NSCFNumber
@end
