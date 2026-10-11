#import "NSNibArchiveUnarchiver.h"
#import <Foundation/Foundation.h>
#import <AppKit/NSNib.h>

#include <stdlib.h>
#include <string.h>

// the file: "NIBArchive", two format words, then count and offset of the objects, keys, values and class names
enum { NIB_INT8, NIB_INT16, NIB_INT32, NIB_INT64, NIB_TRUE, NIB_FALSE, NIB_FLOAT, NIB_DOUBLE, NIB_DATA, NIB_NIL, NIB_OBJECT };

typedef struct {
    uint32_t key;
    uint8_t type;
    int64_t i;
    double d;
    const uint8_t *bytes;
    uint32_t length;
} NibValue;

typedef struct {
    uint32_t cls, first, count;
} NibObject;

@interface NSObject (NSNibArchiveUnarchiverDelegate)
- unarchiver:(NSCoder *)unarchiver didDecodeObject:object;
- (void)unarchiver:(NSCoder *)unarchiver willReplaceObject:object withObject:replacement;
- (NSDictionary *)externalNameTable;
+ allocWithKeyedUnarchiver:(NSCoder *)coder;
@end

@implementation NSNibArchiveUnarchiver {
    NSData *_data;
    id _delegate;
    NSMutableDictionary *_classes;
    NSString **_keys;
    NSString **_classNames;
    NibValue *_values;
    NibObject *_objects;
    uint32_t _nkeys, _nclasses, _nvalues, _nobjects;
    id *_decoded;
    uint8_t *_state;
    uint32_t _stack[256];
    unsigned _depth;
    uint32_t _emptyKey;
}

+ (BOOL)isNibArchiveData:(NSData *)data {
    return [data length] >= 50 && memcmp([data bytes], "NIBArchive", 10) == 0;
}

static BOOL varint(const uint8_t *p, NSUInteger end, NSUInteger *at, uint32_t *out) {
    uint32_t v = 0;
    unsigned shift = 0;

    while (*at < end && shift < 35) {
        uint8_t c = p[(*at)++];

        v |= (uint32_t)(c & 0x7f) << shift;
        if (c & 0x80) {
            *out = v;
            return YES;
        }
        shift += 7;
    }
    return NO;
}

static uint32_t word(const uint8_t *p, NSUInteger at) {
    uint32_t v;

    memcpy(&v, p + at, 4);
    return v;
}

- (BOOL)parse {
    const uint8_t *p = [_data bytes];
    NSUInteger end = [_data length], at;
    uint32_t okeys, ovalues, oclasses, oobjects, i;

    _nobjects = word(p, 18), oobjects = word(p, 22), _nkeys = word(p, 26), okeys = word(p, 30);
    _nvalues = word(p, 34), ovalues = word(p, 38), _nclasses = word(p, 42), oclasses = word(p, 46);
    if (!_nobjects || oobjects >= end || okeys >= end || ovalues > end || oclasses >= end)
        return NO;

    _keys = calloc(_nkeys + 1, sizeof(NSString *));
    _emptyKey = UINT32_MAX;
    for (i = 0, at = okeys; i < _nkeys; i++) {
        uint32_t n;

        if (!varint(p, end, &at, &n) || at + n > end)
            return NO;
        _keys[i] = [[NSString alloc] initWithBytes:p + at length:n encoding:NSUTF8StringEncoding];
        if ([_keys[i] isEqualToString:@"UINibEncoderEmptyKey"])
            _emptyKey = i;
        at += n;
    }

    _classNames = calloc(_nclasses + 1, sizeof(NSString *));
    for (i = 0, at = oclasses; i < _nclasses; i++) {
        uint32_t n, extra;

        if (!varint(p, end, &at, &n) || !varint(p, end, &at, &extra) || at + 4 * extra + n > end)
            return NO;
        at += 4 * extra;
        // the name is NUL terminated inside its length
        _classNames[i] = [[NSString alloc] initWithBytes:p + at length:(n && !p[at + n - 1]) ? n - 1 : n encoding:NSUTF8StringEncoding];
        at += n;
    }

    _values = calloc(_nvalues + 1, sizeof(NibValue));
    for (i = 0, at = ovalues; i < _nvalues; i++) {
        NibValue *v = &_values[i];

        if (!varint(p, end, &at, &v->key) || at >= end || v->key >= _nkeys)
            return NO;
        v->type = p[at++];
        switch (v->type) {
        case NIB_INT8: v->i = (int8_t)p[at], at += 1; break;
        case NIB_INT16: { int16_t x; memcpy(&x, p + at, 2); v->i = x; at += 2; break; }
        case NIB_INT32: { int32_t x; memcpy(&x, p + at, 4); v->i = x; at += 4; break; }
        case NIB_INT64: memcpy(&v->i, p + at, 8), at += 8; break;
        case NIB_TRUE: v->i = 1; break;
        case NIB_FALSE: v->i = 0; break;
        case NIB_FLOAT: { float x; memcpy(&x, p + at, 4); v->d = x; at += 4; break; }
        case NIB_DOUBLE: memcpy(&v->d, p + at, 8), at += 8; break;
        case NIB_DATA:
            if (!varint(p, end, &at, &v->length) || at + v->length > end)
                return NO;
            v->bytes = p + at, at += v->length;
            break;
        case NIB_NIL: break;
        case NIB_OBJECT: v->i = word(p, at), at += 4; break;
        default:
            NSLog(@"NSNibArchiveUnarchiver: unknown value type %u", v->type);
            return NO;
        }
        if (at > end)
            return NO;
    }

    _objects = calloc(_nobjects, sizeof(NibObject));
    for (i = 0, at = oobjects; i < _nobjects; i++) {
        NibObject *o = &_objects[i];

        if (!varint(p, end, &at, &o->cls) || !varint(p, end, &at, &o->first) || !varint(p, end, &at, &o->count) ||
            o->cls >= _nclasses || o->first + o->count > _nvalues)
            return NO;
    }
    _decoded = calloc(_nobjects, sizeof(id));
    _state = calloc(_nobjects, 1);
    _stack[0] = 0, _depth = 1;
    return YES;
}

- initForReadingWithData:(NSData *)data {
    if (!(self = [super init]))
        return nil;
    _data = [data retain];
    _classes = [NSMutableDictionary new];
    if (![[self class] isNibArchiveData:data] || ![self parse]) {
        NSLog(@"NSNibArchiveUnarchiver: not a NIBArchive (or a damaged one)");
        [self release];
        return nil;
    }
    return self;
}

- (void)dealloc {
    uint32_t i;

    for (i = 0; i < _nkeys && _keys; i++)
        [_keys[i] release];
    for (i = 0; i < _nclasses && _classNames; i++)
        [_classNames[i] release];
    for (i = 0; i < _nobjects && _decoded; i++)
        [_decoded[i] release];
    free(_keys), free(_classNames), free(_values), free(_objects), free(_decoded), free(_state);
    [_classes release];
    [_data release];
    [super dealloc];
}

- delegate {
    return _delegate;
}

- (void)setDelegate:delegate {
    _delegate = delegate;
}

- (void)setClass:(Class)cls forClassName:(NSString *)className {
    [_classes setObject:cls forKey:className];
}

- (BOOL)allowsKeyedCoding {
    return YES;
}

- (unsigned)systemVersion {
    return 1000;
}

// the current object's value for key (the first one, as keyed archives have one per key)
- (NibValue *)valueForKey:(NSString *)key {
    NibObject *o = &_objects[_stack[_depth - 1]];

    for (uint32_t i = 0; i < o->count; i++) {
        NibValue *v = &_values[o->first + i];

        if ([_keys[v->key] isEqualToString:key])
            return v;
    }
    return NULL;
}

- (BOOL)containsValueForKey:(NSString *)key {
    return [self valueForKey:key] != NULL;
}

static int64_t intOf(NibValue *v) {
    if (!v)
        return 0;
    if (v->type == NIB_FLOAT || v->type == NIB_DOUBLE)
        return (int64_t)v->d;
    return v->type <= NIB_FALSE ? v->i : 0;
}

static double doubleOf(NibValue *v) {
    if (!v)
        return 0;
    if (v->type == NIB_FLOAT || v->type == NIB_DOUBLE)
        return v->d;
    return v->type <= NIB_FALSE ? (double)v->i : 0;
}

- (BOOL)decodeBoolForKey:(NSString *)key {
    return intOf([self valueForKey:key]) != 0;
}

- (int)decodeIntForKey:(NSString *)key {
    return (int)intOf([self valueForKey:key]);
}

- (int32_t)decodeInt32ForKey:(NSString *)key {
    return (int32_t)intOf([self valueForKey:key]);
}

- (int64_t)decodeInt64ForKey:(NSString *)key {
    return intOf([self valueForKey:key]);
}

- (NSInteger)decodeIntegerForKey:(NSString *)key {
    return (NSInteger)intOf([self valueForKey:key]);
}

- (float)decodeFloatForKey:(NSString *)key {
    return (float)doubleOf([self valueForKey:key]);
}

- (double)decodeDoubleForKey:(NSString *)key {
    return doubleOf([self valueForKey:key]);
}

- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)length {
    NibValue *v = [self valueForKey:key];

    if (!v || v->type != NIB_DATA) {
        if (length)
            *length = 0;
        return NULL;
    }
    if (length)
        *length = v->length;
    return v->bytes;
}

- decodeObject {
    return nil;
}

- (NSString *)stringForKey:(NSString *)key {
    id s = [self decodeObjectForKey:key];

    return [s isKindOfClass:[NSString class]] ? s : nil;
}

- (NSRect)decodeRectForKey:(NSString *)key {
    NSString *s = [self stringForKey:key];

    return s ? NSRectFromString(s) : NSZeroRect;
}

- (NSSize)decodeSizeForKey:(NSString *)key {
    NSString *s = [self stringForKey:key];

    return s ? NSSizeFromString(s) : NSZeroSize;
}

- (NSPoint)decodePointForKey:(NSString *)key {
    NSString *s = [self stringForKey:key];

    return s ? NSPointFromString(s) : NSZeroPoint;
}

// the objects an inlined collection lists under the empty key, in order
- (NSMutableArray *)elementsOf:(uint32_t)n {
    NibObject *o = &_objects[n];
    NSMutableArray *items = [NSMutableArray array];

    _stack[_depth++] = n;
    for (uint32_t i = 0; i < o->count; i++) {
        NibValue *v = &_values[o->first + i];

        if (v->key != _emptyKey)
            continue;
        if (v->type == NIB_OBJECT) {
            id item = [self objectAtIndex:(uint32_t)v->i];

            if (item)
                [items addObject:item];
        }
    }
    _depth--;
    return items;
}

// foundation's value classes as the archive inlines them, 0 when the class is an ordinary one
- (id)newValueObject:(uint32_t)n named:(NSString *)name {
    id result = nil;

    _stack[_depth++] = n;
    if ([name isEqualToString:@"NSString"] || [name isEqualToString:@"NSLocalizableString"] ||
        [name isEqualToString:@"NSMutableString"]) {
        NSUInteger len;
        const uint8_t *bytes = [self decodeBytesForKey:@"NS.bytes" returnedLength:&len];
        Class cls = [name isEqualToString:@"NSMutableString"] ? [NSMutableString class] : [NSString class];

        result = [[cls alloc] initWithBytes:bytes ? (const void *)bytes : "" length:len encoding:NSUTF8StringEncoding];
    } else if ([name isEqualToString:@"NSData"] || [name isEqualToString:@"NSMutableData"]) {
        NSUInteger len;
        const uint8_t *bytes = [self decodeBytesForKey:@"NS.bytes" returnedLength:&len];

        result = [[NSMutableData alloc] initWithBytes:bytes length:len];
    } else if ([name isEqualToString:@"NSNumber"]) {
        NibValue *v;

        if ((v = [self valueForKey:@"NS.dblval"]) || (v = [self valueForKey:@"NS.fltval"]))
            result = [[NSNumber alloc] initWithDouble:doubleOf(v)];
        else if ((v = [self valueForKey:@"NS.boolval"]))
            result = [[NSNumber alloc] initWithBool:intOf(v) != 0];
        else
            result = [[NSNumber alloc] initWithLongLong:intOf([self valueForKey:@"NS.intval"])];
    } else if ([name isEqualToString:@"NSNull"]) {
        result = [[NSNull null] retain];
    } else if ([name isEqualToString:@"NSNibExternalObjectPlaceholder"]) {
        // an object the loader passes in (the storyboard, say), found by its identifier in the name table
        NSString *ident = [self stringForKey:@"NSExternalObjectPlaceholderIdentifier"];
        NSDictionary *table = [_delegate respondsToSelector:@selector(externalNameTable)] ? [_delegate externalNameTable] : nil;

        result = [[table objectForKey:ident] retain];
        if (!result)
            NSLog(@"NSNibArchiveUnarchiver: no external object for %@", ident);
    }
    _depth--;
    if (result)
        return result;

    if ([name isEqualToString:@"NSArray"] || [name isEqualToString:@"NSMutableArray"])
        return [[self elementsOf:n] retain];
    if ([name isEqualToString:@"NSSet"] || [name isEqualToString:@"NSMutableSet"])
        return [[NSMutableSet alloc] initWithArray:[self elementsOf:n]];
    if ([name isEqualToString:@"NSOrderedSet"] || [name isEqualToString:@"NSMutableOrderedSet"])
        return [[self elementsOf:n] retain];
    if ([name isEqualToString:@"NSDictionary"] || [name isEqualToString:@"NSMutableDictionary"]) {
        NSArray *items = [self elementsOf:n];
        NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];

        // keys and objects alternate
        for (NSUInteger i = 0; i + 1 < [items count]; i += 2)
            [dict setObject:[items objectAtIndex:i + 1] forKey:[items objectAtIndex:i]];
        return dict;
    }
    return nil;
}

- objectAtIndex:(uint32_t)n {
    NSString *name;
    Class cls;
    id object, replacement;

    if (n >= _nobjects)
        return nil;
    if (_state[n])
        return _decoded[n];
    if (_depth >= 255) {
        NSLog(@"NSNibArchiveUnarchiver: objects nest too deeply");
        return nil;
    }
    name = _classNames[_objects[n].cls];
    _state[n] = 1;
    if ((object = [self newValueObject:n named:name])) {
        _decoded[n] = object, _state[n] = 2;
        return object;
    }

    if (!(cls = [_classes objectForKey:name]) && !(cls = NSClassFromString(name))) {
        NSLog(@"NSNibArchiveUnarchiver: unknown class %@", name);
        _state[n] = 2;
        return nil;
    }

    _stack[_depth++] = n;
    object = [cls respondsToSelector:@selector(allocWithKeyedUnarchiver:)] ? [cls allocWithKeyedUnarchiver:self] : [cls alloc];
    // registered before -initWithCoder: so references back to it (cycles) resolve
    _decoded[n] = object;
    object = [object initWithCoder:self];
    _decoded[n] = object;
    if (object && [object respondsToSelector:@selector(awakeAfterUsingCoder:)]) {
        // the delegate has not seen this object yet: only the replacement is reported, below
        replacement = [object awakeAfterUsingCoder:self];
        if (replacement != object)
            _decoded[n] = object = replacement;
    }
    _depth--;
    if (object && [_delegate respondsToSelector:@selector(unarchiver:didDecodeObject:)]) {
        replacement = [_delegate unarchiver:self didDecodeObject:object];
        if (replacement != object)
            _decoded[n] = object = [replacement retain];
    }
    _state[n] = 2;
    return object;
}

- decodeObjectForKey:(NSString *)key {
    NibValue *v = [self valueForKey:key];

    if (!v)
        return nil;
    if (v->type == NIB_OBJECT)
        return [self objectAtIndex:(uint32_t)v->i];
    if (v->type == NIB_DATA)
        return [NSData dataWithBytes:v->bytes length:v->length];
    return nil;
}

- (void)replaceObject:object withObject:replacement {
    for (uint32_t i = 0; i < _nobjects; i++) {
        if (_decoded[i] != object)
            continue;
        if ([_delegate respondsToSelector:@selector(unarchiver:willReplaceObject:withObject:)])
            [_delegate unarchiver:self willReplaceObject:object withObject:replacement];
        _decoded[i] = [replacement retain];
    }
}

- (void)finishDecoding {
}

@end
