// NSConstantString.m - the class behind @"..." literals under libobjc2, for Argon on Linux
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// on PureDarwin clang emits CFString constants and this file is empty

#import <Foundation/NSString.h>

#if ARGON_LIBOBJC2
#include <string.h>

// clang's gnustep-2 literal: flags give the encoding, length counts UTF-16 units, size counts bytes
enum { NSConstantStringASCII = 0, NSConstantStringUTF8 = 1, NSConstantStringUTF16 = 2 };

@interface NSConstantString : NSString {
    uint32_t _flags;
    uint32_t _length;
    uint32_t _size;
    uint32_t _hash;
    const char *_data;
}
@end

@implementation NSConstantString

- (BOOL)_isUTF16
{
    return (_flags & 3) == NSConstantStringUTF16;
}

- (NSUInteger)length
{
    return _length;
}

- (unichar)characterAtIndex:(NSUInteger)index
{
    if ([self _isUTF16])
        return ((const unichar *)_data)[index];
    if ((_flags & 3) == NSConstantStringASCII)
        return (unsigned char)_data[index];
    return [[NSString stringWithUTF8String:_data] characterAtIndex:index];
}

- (void)getCharacters:(unichar *)buffer range:(NSRange)range
{
    if ([self _isUTF16]) {
        memcpy(buffer, (const unichar *)_data + range.location, range.length * sizeof(unichar));
        return;
    }
    if ((_flags & 3) == NSConstantStringASCII) {
        for (NSUInteger i = 0; i < range.length; i++)
            buffer[i] = (unsigned char)_data[range.location + i];
        return;
    }
    [[NSString stringWithUTF8String:_data] getCharacters:buffer range:range];
}

// ASCII and UTF-8 literals are already NUL-terminated UTF-8
- (const char *)UTF8String
{
    if (![self _isUTF16])
        return _data;
    return [super UTF8String];
}

// literals live in the binary for the life of the process
- (id)retain { return self; }
- (oneway void)release {}
- (id)autorelease { return self; }
- (NSUInteger)retainCount { return (NSUInteger)-1; }
- (id)copyWithZone:(NSZone *)zone { return self; }

@end

// clang packs ASCII literals shorter than 9 characters into the pointer: tag 4 in bits 0-2,
// the length in bits 3-6, then 7 bits per character from bit 57 down
BOOL objc_registerSmallObjectClass_np(Class cls, uintptr_t mask);

@interface NSTinyString : NSString
@end

@implementation NSTinyString

+ (void)load
{
    objc_registerSmallObjectClass_np(self, 4);
}

static inline uintptr_t tinyBits(id string)
{
    return (uintptr_t)string;
}

- (NSUInteger)length
{
    return (tinyBits(self) >> 3) & 0xf;
}

- (unichar)characterAtIndex:(NSUInteger)index
{
    return (unichar)((tinyBits(self) >> (57 - index * 7)) & 0x7f);
}

- (void)getCharacters:(unichar *)buffer range:(NSRange)range
{
    for (NSUInteger i = 0; i < range.length; i++)
        buffer[i] = [self characterAtIndex:range.location + i];
}

// tagged pointers are not objects in memory, retaining one is a no-op
- (id)retain { return self; }
- (oneway void)release {}
- (id)autorelease { return self; }
- (NSUInteger)retainCount { return (NSUInteger)-1; }
- (id)copyWithZone:(NSZone *)zone { return self; }

@end
#endif
