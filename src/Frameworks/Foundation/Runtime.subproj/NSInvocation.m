/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSInvocation.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <string.h>
#include <stdlib.h>
#include <ffi.h>

/* Returns the position just past the type starting at `type`, descending into
 * struct/union/array bodies. */
static const char *_skipType(const char *type) {
    if (type == NULL || *type == '\0') {
        return type;
    }

    /* Leading qualifiers: const, in, out, inout, bycopy, byref, oneway. */
    while (*type == 'r' || *type == 'n' || *type == 'N' || *type == 'o' ||
           *type == 'O' || *type == 'R' || *type == 'V') {
        type++;
    }

    switch (*type) {
        case '{':
        case '(': {
            char open = *type;
            char close = (open == '{') ? '}' : ')';
            int depth = 0;

            for (; *type != '\0'; type++) {
                if (*type == open) {
                    depth++;
                } else if (*type == close) {
                    depth--;
                    if (depth == 0) {
                        return type + 1;
                    }
                }
            }
            return type;
        }
        case '[': {
            int depth = 0;

            for (; *type != '\0'; type++) {
                if (*type == '[') {
                    depth++;
                } else if (*type == ']') {
                    depth--;
                    if (depth == 0) {
                        return type + 1;
                    }
                }
            }
            return type;
        }
        case '^':
            return _skipType(type + 1);
        case 'b':
            type++;
            while (*type >= '0' && *type <= '9') {
                type++;
            }
            return type;
        default:
            return type + 1;
    }
}

static const char *_skipDigits(const char *type) {
    while (*type >= '0' && *type <= '9') {
        type++;
    }
    return type;
}

static ffi_type *_ffiTypeForEncoding(const char *type);

/* Struct types are built once and intentionally leaked: they must outlive
 * every cif that references them, and signatures are cached for the process
 * lifetime anyway. */
static ffi_type *_ffiStructForEncoding(const char *type) {
    const char *cursor = type + 1;

    while (*cursor != '\0' && *cursor != '=' && *cursor != '}') {
        cursor++;
    }
    if (*cursor == '=') {
        cursor++;
    }

    size_t capacity = 8;
    size_t count = 0;
    ffi_type **elements = calloc(capacity + 1, sizeof(ffi_type *));

    if (elements == NULL) {
        return &ffi_type_pointer;
    }

    while (*cursor != '\0' && *cursor != '}') {
        if (count == capacity) {
            capacity *= 2;
            ffi_type **grown = realloc(elements, (capacity + 1) * sizeof(ffi_type *));

            if (grown == NULL) {
                break;
            }
            elements = grown;
        }
        elements[count++] = _ffiTypeForEncoding(cursor);
        cursor = _skipType(cursor);
    }
    elements[count] = NULL;

    ffi_type *result = calloc(1, sizeof(ffi_type));

    if (result == NULL) {
        free(elements);
        return &ffi_type_pointer;
    }
    result->type = FFI_TYPE_STRUCT;
    result->elements = elements;
    return result;
}

static ffi_type *_ffiTypeForEncoding(const char *type) {
    while (*type == 'r' || *type == 'n' || *type == 'N' || *type == 'o' ||
           *type == 'O' || *type == 'R' || *type == 'V') {
        type++;
    }

    switch (*type) {
        case 'c': return &ffi_type_sint8;
        case 'C': return &ffi_type_uint8;
        case 's': return &ffi_type_sint16;
        case 'S': return &ffi_type_uint16;
        case 'i': return &ffi_type_sint32;
        case 'I': return &ffi_type_uint32;
        case 'l': return &ffi_type_sint32;
        case 'L': return &ffi_type_uint32;
        case 'q': return &ffi_type_sint64;
        case 'Q': return &ffi_type_uint64;
        case 'f': return &ffi_type_float;
        case 'd': return &ffi_type_double;
        case 'D': return &ffi_type_longdouble;
        case 'B': return &ffi_type_uint8;
        case 'v': return &ffi_type_void;
        case '*':
        case '@':
        case '#':
        case ':':
        case '^':
        case '?': return &ffi_type_pointer;
        case '{': return _ffiStructForEncoding(type);
        case '[': return &ffi_type_pointer;
        default:  return &ffi_type_pointer;
    }
}

static void _sizeAndAlignOfEncoding(const char *type, size_t *sizeOut, size_t *alignOut) {
    NSUInteger size = 0;
    NSUInteger align = 0;

    NSGetSizeAndAlignment(type, &size, &align);
    *sizeOut = (size_t)size;
    *alignOut = (size_t)align;
}

@implementation NSMethodSignature

+ (NSMethodSignature *)signatureWithObjCTypes:(const char *)types {
    if (types == NULL || *types == '\0') {
        return nil;
    }

    NSMethodSignature *signature = [[[self alloc] init] autorelease];

    signature->_types = strdup(types);

    /* First encoding is the return type; the rest are arguments, each possibly
     * followed by a stack offset we do not use. */
    const char *cursor = signature->_types;

    signature->_isOneway = (strchr(types, 'V') != NULL && *types == 'V');

    const char *returnStart = cursor;
    const char *returnEnd = _skipType(cursor);
    size_t returnBytes = (size_t)(returnEnd - returnStart);

    signature->_returnType = malloc(returnBytes + 1);
    memcpy(signature->_returnType, returnStart, returnBytes);
    signature->_returnType[returnBytes] = '\0';

    size_t returnSize = 0;
    size_t returnAlign = 0;

    _sizeAndAlignOfEncoding(signature->_returnType, &returnSize, &returnAlign);
    signature->_returnLength = (*signature->_returnType == 'v') ? 0 : returnSize;

    cursor = _skipDigits(returnEnd);

    /* Count arguments first so the tables can be sized exactly. */
    NSUInteger count = 0;
    const char *counting = cursor;

    while (*counting != '\0') {
        counting = _skipDigits(_skipType(counting));
        count++;
    }

    signature->_argumentCount = count;
    signature->_argumentTypes = calloc(count > 0 ? count : 1, sizeof(char *));
    signature->_argumentOffsets = calloc(count > 0 ? count : 1, sizeof(NSUInteger));
    signature->_argumentSizes = calloc(count > 0 ? count : 1, sizeof(NSUInteger));

    size_t offset = 0;

    for (NSUInteger i = 0; i < count; i++) {
        const char *start = cursor;
        const char *end = _skipType(cursor);
        size_t bytes = (size_t)(end - start);

        signature->_argumentTypes[i] = malloc(bytes + 1);
        memcpy(signature->_argumentTypes[i], start, bytes);
        signature->_argumentTypes[i][bytes] = '\0';

        size_t size = 0;
        size_t align = 0;

        _sizeAndAlignOfEncoding(signature->_argumentTypes[i], &size, &align);
        if (align == 0) {
            align = sizeof(void *);
        }
        if (size == 0) {
            size = sizeof(void *);
        }

        offset = (offset + align - 1) & ~(align - 1);
        signature->_argumentOffsets[i] = offset;
        signature->_argumentSizes[i] = size;
        offset += size;

        cursor = _skipDigits(end);
    }

    signature->_frameLength = (offset + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
    return signature;
}

- (void)dealloc {
    for (NSUInteger i = 0; i < _argumentCount; i++) {
        free(_argumentTypes[i]);
    }
    free(_argumentTypes);
    free(_argumentOffsets);
    free(_argumentSizes);
    free(_returnType);
    free(_types);
    [super dealloc];
}

- (NSUInteger)numberOfArguments {
    return _argumentCount;
}

- (const char *)getArgumentTypeAtIndex:(NSUInteger)index {
    if (index >= _argumentCount) {
        [NSException raise:NSInvalidArgumentException
                    format:@"argument index %lu out of range (%lu arguments)",
                           (unsigned long)index, (unsigned long)_argumentCount];
        return NULL;
    }
    return _argumentTypes[index];
}

- (const char *)methodReturnType {
    return _returnType;
}

- (NSUInteger)methodReturnLength {
    return _returnLength;
}

- (NSUInteger)frameLength {
    return _frameLength;
}

- (BOOL)isOneway {
    return _isOneway;
}

- (NSUInteger)_offsetOfArgumentAtIndex:(NSUInteger)index {
    return (index < _argumentCount) ? _argumentOffsets[index] : 0;
}

- (NSUInteger)_sizeOfArgumentAtIndex:(NSUInteger)index {
    return (index < _argumentCount) ? _argumentSizes[index] : 0;
}

- (const char *)_types {
    return _types;
}

@end

@implementation NSInvocation

+ (NSInvocation *)invocationWithMethodSignature:(NSMethodSignature *)signature {
    if (signature == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"NSInvocation requires a method signature"];
        return nil;
    }

    NSInvocation *invocation = [[[self alloc] init] autorelease];

    invocation->_signature = [signature retain];
    invocation->_frame = calloc(1, [signature frameLength] + sizeof(void *));

    NSUInteger returnLength = [signature methodReturnLength];

    invocation->_returnValue = calloc(1, returnLength > 0 ? returnLength : sizeof(void *));
    return invocation;
}

- (void)dealloc {
    if (_argumentsRetained) {
        NSUInteger count = [_signature numberOfArguments];

        for (NSUInteger i = 0; i < count; i++) {
            const char *type = [_signature getArgumentTypeAtIndex:i];

            if (*type == '@') {
                id object = nil;

                [self getArgument:&object atIndex:(NSInteger)i];
                [object release];
            }
        }
    }
    [_signature release];
    free(_frame);
    free(_returnValue);
    [super dealloc];
}

- (NSMethodSignature *)methodSignature {
    return _signature;
}

- (id)target {
    id target = nil;

    [self getArgument:&target atIndex:0];
    return target;
}

- (void)setTarget:(id)target {
    [self setArgument:&target atIndex:0];
}

- (SEL)selector {
    SEL selector = NULL;

    [self getArgument:&selector atIndex:1];
    return selector;
}

- (void)setSelector:(SEL)selector {
    [self setArgument:&selector atIndex:1];
}

- (void)getArgument:(void *)buffer atIndex:(NSInteger)index {
    if (index < 0 || (NSUInteger)index >= [_signature numberOfArguments]) {
        [NSException raise:NSInvalidArgumentException
                    format:@"argument index %ld out of range", (long)index];
        return;
    }
    memcpy(buffer, (char *)_frame + [_signature _offsetOfArgumentAtIndex:(NSUInteger)index],
           [_signature _sizeOfArgumentAtIndex:(NSUInteger)index]);
}

- (void)setArgument:(void *)buffer atIndex:(NSInteger)index {
    if (index < 0 || (NSUInteger)index >= [_signature numberOfArguments]) {
        [NSException raise:NSInvalidArgumentException
                    format:@"argument index %ld out of range", (long)index];
        return;
    }

    NSUInteger offset = [_signature _offsetOfArgumentAtIndex:(NSUInteger)index];
    NSUInteger size = [_signature _sizeOfArgumentAtIndex:(NSUInteger)index];

    if (_argumentsRetained && *[_signature getArgumentTypeAtIndex:(NSUInteger)index] == '@') {
        id existing = nil;
        id replacement = nil;

        memcpy(&existing, (char *)_frame + offset, sizeof(id));
        memcpy(&replacement, buffer, sizeof(id));
        [replacement retain];
        [existing release];
    }
    memcpy((char *)_frame + offset, buffer, size);
}

- (void)getReturnValue:(void *)buffer {
    NSUInteger length = [_signature methodReturnLength];

    if (length > 0) {
        memcpy(buffer, _returnValue, length);
    }
}

- (void)setReturnValue:(void *)buffer {
    NSUInteger length = [_signature methodReturnLength];

    if (length > 0) {
        memcpy(_returnValue, buffer, length);
    }
}

- (void)retainArguments {
    if (_argumentsRetained) {
        return;
    }
    _argumentsRetained = YES;

    NSUInteger count = [_signature numberOfArguments];

    for (NSUInteger i = 0; i < count; i++) {
        const char *type = [_signature getArgumentTypeAtIndex:i];

        if (*type == '@') {
            id object = nil;

            [self getArgument:&object atIndex:(NSInteger)i];
            [object retain];
        }
    }
}

- (BOOL)argumentsRetained {
    return _argumentsRetained;
}

- (void)invoke {
    id target = [self target];
    SEL selector = [self selector];

    if (target == nil) {
        /* Messaging nil yields a zeroed return, same as a direct send. */
        NSUInteger length = [_signature methodReturnLength];

        if (length > 0) {
            memset(_returnValue, 0, length);
        }
        return;
    }

    IMP implementation = class_getMethodImplementation(object_getClass(target), selector);

    if (implementation == NULL) {
        [NSException raise:NSInvalidArgumentException
                    format:@"no implementation for %s", sel_getName(selector)];
        return;
    }

    NSUInteger count = [_signature numberOfArguments];
    ffi_type **argumentTypes = calloc(count > 0 ? count : 1, sizeof(ffi_type *));
    void **argumentValues = calloc(count > 0 ? count : 1, sizeof(void *));

    for (NSUInteger i = 0; i < count; i++) {
        argumentTypes[i] = _ffiTypeForEncoding([_signature getArgumentTypeAtIndex:i]);
        argumentValues[i] = (char *)_frame + [_signature _offsetOfArgumentAtIndex:i];
    }

    ffi_type *returnType = _ffiTypeForEncoding([_signature methodReturnType]);
    ffi_cif cif;

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned)count, returnType,
                     argumentTypes) == FFI_OK) {
        /* libffi writes at least a full register for small returns. */
        long double scratch;
        void *destination = ([_signature methodReturnLength] > 0)
            ? ((returnType->size < sizeof(long double)) ? (void *)&scratch : _returnValue)
            : (void *)&scratch;

        ffi_call(&cif, FFI_FN(implementation), destination, argumentValues);

        if ([_signature methodReturnLength] > 0 && destination == (void *)&scratch) {
            memcpy(_returnValue, &scratch, [_signature methodReturnLength]);
        }
    }

    free(argumentTypes);
    free(argumentValues);
}

- (void)invokeWithTarget:(id)target {
    [self setTarget:target];
    [self invoke];
}

@end

@implementation NSObject (NSMethodSignatureLookup)

+ (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    Method method = class_getClassMethod(self, selector);

    if (method == NULL) {
        return nil;
    }
    return [NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    Method method = class_getInstanceMethod(object_getClass(self), selector);

    if (method == NULL) {
        return nil;
    }
    return [NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];
}

+ (NSMethodSignature *)instanceMethodSignatureForSelector:(SEL)selector {
    Method method = class_getInstanceMethod(self, selector);

    if (method == NULL) {
        return nil;
    }
    return [NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];
}

@end

@implementation NSObject (NSClassName)

- (NSString *)className {
    return NSStringFromClass([self class]);
}

@end
