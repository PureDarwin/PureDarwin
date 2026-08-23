/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <objc/runtime.h>

static NSString *_stringFromCString(const char *cString) {
    if (cString == NULL) {
        return nil;
    }
    return (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, cString,
                                                 kCFStringEncodingUTF8);
}

/* CFStringGetCStringPtr can refuse, so go through a bounded copy. */
static BOOL _cStringFromString(NSString *string, char *buffer, size_t size) {
    if (string == nil) {
        return NO;
    }
    return CFStringGetCString((CFStringRef)string, buffer, (CFIndex)size,
                              kCFStringEncodingUTF8) ? YES : NO;
}

NSString *NSStringFromSelector(SEL selector) {
    return selector == NULL ? nil : _stringFromCString(sel_getName(selector));
}

SEL NSSelectorFromString(NSString *name) {
    char buffer[1024];
    if (!_cStringFromString(name, buffer, sizeof(buffer))) {
        return NULL;
    }
    return sel_registerName(buffer);
}

NSString *NSStringFromClass(Class aClass) {
    return aClass == Nil ? nil : _stringFromCString(class_getName(aClass));
}

Class NSClassFromString(NSString *name) {
    char buffer[1024];
    if (!_cStringFromString(name, buffer, sizeof(buffer))) {
        return Nil;
    }
    return objc_lookUpClass(buffer);
}

NSString *NSStringFromProtocol(Protocol *protocol) {
    return protocol == NULL ? nil : _stringFromCString(protocol_getName(protocol));
}

Protocol *NSProtocolFromString(NSString *name) {
    char buffer[1024];
    if (!_cStringFromString(name, buffer, sizeof(buffer))) {
        return NULL;
    }
    return objc_getProtocol(buffer);
}

/* Type-encoding walker. libobjc does not export one, so Foundation carries it.
 * Returns the position just past the type it measured. */

#define NS_SCALAR_CASE(code, type) \
    case code: size = sizeof(type); align = __alignof__(type); typePtr++; break

static const char *_sizeAndAlignment(const char *typePtr, NSUInteger *sizep, NSUInteger *alignp);

/* Struct/union bodies are "{name=field...}"; skip to just past the '='. */
static const char *_skipAggregateName(const char *typePtr) {
    while (*typePtr != '\0' && *typePtr != '=' && *typePtr != '}' && *typePtr != ')') {
        typePtr++;
    }
    if (*typePtr == '=') {
        typePtr++;
    }
    return typePtr;
}

static const char *_aggregate(const char *typePtr, char close, BOOL isUnion,
                              NSUInteger *sizep, NSUInteger *alignp) {
    NSUInteger size = 0;
    NSUInteger align = 1;

    typePtr = _skipAggregateName(typePtr);

    while (*typePtr != '\0' && *typePtr != close) {
        /* Field names appear as "name" before each member; drop them. */
        if (*typePtr == '"') {
            typePtr++;
            while (*typePtr != '\0' && *typePtr != '"') {
                typePtr++;
            }
            if (*typePtr == '"') {
                typePtr++;
            }
            continue;
        }

        NSUInteger fieldSize = 0;
        NSUInteger fieldAlign = 1;
        typePtr = _sizeAndAlignment(typePtr, &fieldSize, &fieldAlign);

        if (fieldAlign > align) {
            align = fieldAlign;
        }
        if (isUnion) {
            if (fieldSize > size) {
                size = fieldSize;
            }
        } else {
            if (fieldAlign != 0 && (size % fieldAlign) != 0) {
                size += fieldAlign - (size % fieldAlign);
            }
            size += fieldSize;
        }
    }

    if (*typePtr == close) {
        typePtr++;
    }
    if (align != 0 && (size % align) != 0) {
        size += align - (size % align);
    }

    *sizep = size;
    *alignp = align;
    return typePtr;
}

static const char *_sizeAndAlignment(const char *typePtr, NSUInteger *sizep, NSUInteger *alignp) {
    NSUInteger size = 0;
    NSUInteger align = 1;

    /* Method-encoding qualifiers carry no storage of their own. */
    while (*typePtr == 'r' || *typePtr == 'n' || *typePtr == 'N' || *typePtr == 'o'
           || *typePtr == 'O' || *typePtr == 'R' || *typePtr == 'V') {
        typePtr++;
    }

    switch (*typePtr) {
        NS_SCALAR_CASE('c', char);
        NS_SCALAR_CASE('C', unsigned char);
        NS_SCALAR_CASE('s', short);
        NS_SCALAR_CASE('S', unsigned short);
        NS_SCALAR_CASE('i', int);
        NS_SCALAR_CASE('I', unsigned int);
        NS_SCALAR_CASE('l', long);
        NS_SCALAR_CASE('L', unsigned long);
        NS_SCALAR_CASE('q', long long);
        NS_SCALAR_CASE('Q', unsigned long long);
        NS_SCALAR_CASE('f', float);
        NS_SCALAR_CASE('d', double);
        NS_SCALAR_CASE('D', long double);
        NS_SCALAR_CASE('B', _Bool);
        NS_SCALAR_CASE('*', char *);
        NS_SCALAR_CASE('#', Class);
        NS_SCALAR_CASE(':', SEL);

        case 'v':
            size = 0;
            align = 1;
            typePtr++;
            break;

        case '@':
            size = sizeof(id);
            align = __alignof__(id);
            typePtr++;
            /* Block encodings are "@?"; the '?' is part of the type. */
            if (*typePtr == '?') {
                typePtr++;
            }
            break;

        case '^':
            size = sizeof(void *);
            align = __alignof__(void *);
            typePtr++;
            typePtr = _sizeAndAlignment(typePtr, NULL, NULL);
            break;

        case '?':
            size = sizeof(void *);
            align = __alignof__(void *);
            typePtr++;
            break;

        case 'b': {
            /* Bitfield: "b" followed by a width in bits. */
            typePtr++;
            NSUInteger bits = 0;
            while (*typePtr >= '0' && *typePtr <= '9') {
                bits = bits * 10 + (NSUInteger)(*typePtr - '0');
                typePtr++;
            }
            size = (bits + 7) / 8;
            align = 1;
            break;
        }

        case '[': {
            typePtr++;
            NSUInteger count = 0;
            while (*typePtr >= '0' && *typePtr <= '9') {
                count = count * 10 + (NSUInteger)(*typePtr - '0');
                typePtr++;
            }

            NSUInteger elementSize = 0;
            typePtr = _sizeAndAlignment(typePtr, &elementSize, &align);
            size = elementSize * count;

            if (*typePtr == ']') {
                typePtr++;
            }
            break;
        }

        case '{':
            typePtr = _aggregate(typePtr + 1, '}', NO, &size, &align);
            break;

        case '(':
            typePtr = _aggregate(typePtr + 1, ')', YES, &size, &align);
            break;

        default:
            /* Unknown code: consume it so the caller cannot spin. */
            if (*typePtr != '\0') {
                typePtr++;
            }
            break;
    }

    if (sizep != NULL) {
        *sizep = size;
    }
    if (alignp != NULL) {
        *alignp = align;
    }
    return typePtr;
}

#undef NS_SCALAR_CASE

const char *NSGetSizeAndAlignment(const char *typePtr, NSUInteger *sizep, NSUInteger *alignp) {
    if (typePtr == NULL) {
        return NULL;
    }
    return _sizeAndAlignment(typePtr, sizep, alignp);
}
