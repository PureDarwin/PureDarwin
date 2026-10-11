#import <Foundation/NSObject.h>
#include <uuid/uuid.h>

@class NSString;

// a 128 bit identifier: random (version 4) from -init and +UUID
@interface NSUUID : NSObject <NSCopying, NSSecureCoding> {
    uuid_t _bytes;
}

+ (instancetype)UUID;
- (instancetype)init;
- (instancetype)initWithUUIDString:(NSString *)string;
- (instancetype)initWithUUIDBytes:(const uuid_t)bytes;
- (void)getUUIDBytes:(uuid_t)bytes;
- (NSComparisonResult)compare:(NSUUID *)other;
@property(readonly, copy) NSString *UUIDString;

@end
