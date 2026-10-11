#import <Foundation/NSUUID.h>
#import <Foundation/NSString.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSData.h>
#include <CoreFoundation/CFUUID.h>
#include <string.h>

@implementation NSUUID

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)UUID {
    return [[[self alloc] init] autorelease];
}

- (instancetype)init {
    if ((self = [super init])) {
        CFUUIDRef u = CFUUIDCreate(NULL);
        CFUUIDBytes b = CFUUIDGetUUIDBytes(u);

        memcpy(_bytes, &b, sizeof(_bytes));
        CFRelease(u);
    }
    return self;
}

- (instancetype)initWithUUIDBytes:(const uuid_t)bytes {
    if ((self = [super init]))
        memcpy(_bytes, bytes, sizeof(_bytes));
    return self;
}

// nil unless the string is the 8-4-4-4-12 hex form
- (instancetype)initWithUUIDString:(NSString *)string {
    if (!(self = [super init]))
        return nil;
    if (!string || uuid_parse([string UTF8String], _bytes) != 0) {
        [self release];
        return nil;
    }
    return self;
}

- (void)getUUIDBytes:(uuid_t)bytes {
    memcpy(bytes, _bytes, sizeof(_bytes));
}

- (NSString *)UUIDString {
    uuid_string_t s;

    uuid_unparse_upper(_bytes, s);
    return [NSString stringWithUTF8String:s];
}

- (NSString *)description {
    return [self UUIDString];
}

- (NSComparisonResult)compare:(NSUUID *)other {
    int c = memcmp(_bytes, other->_bytes, sizeof(_bytes));

    return c < 0 ? NSOrderedAscending : c > 0 ? NSOrderedDescending : NSOrderedSame;
}

- (BOOL)isEqual:(id)other {
    return other == self || ([other isKindOfClass:[NSUUID class]] && memcmp(_bytes, ((NSUUID *)other)->_bytes, sizeof(_bytes)) == 0);
}

- (NSUInteger)hash {
    NSUInteger h;

    memcpy(&h, _bytes, sizeof(h));
    return h;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeBytes:_bytes length:sizeof(_bytes) forKey:@"NS.uuidbytes"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSUInteger len = 0;
    const uint8_t *b = [coder decodeBytesForKey:@"NS.uuidbytes" returnedLength:&len];

    if (!(self = [super init]))
        return nil;
    if (b && len == sizeof(_bytes))
        memcpy(_bytes, b, sizeof(_bytes));
    return self;
}

@end
