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
#include <objc/runtime.h>
#include <objc/message.h>
#include <string.h>

struct NSForwardFrame {
    uint64_t ints[6];       /* rdi rsi rdx rcx r8 r9 */
    double floats[8];       /* xmm0..xmm7 */
    void *stackArguments;   /* first argument passed on the stack */
    uint64_t returnInt;
    double returnFloat;
};

void NSForwardingHandler(struct NSForwardFrame *frame);

#if defined(__x86_64__)

__asm__(
"   .text                                   \n"
"   .globl _NSForwardingTrampoline          \n"
"   .p2align 4                              \n"
"_NSForwardingTrampoline:                   \n"
"   pushq %rbp                              \n"
"   movq  %rsp, %rbp                        \n"
"   subq  $144, %rsp                        \n"
"   movq  %rdi,    0(%rsp)                  \n"
"   movq  %rsi,    8(%rsp)                  \n"
"   movq  %rdx,   16(%rsp)                  \n"
"   movq  %rcx,   24(%rsp)                  \n"
"   movq  %r8,    32(%rsp)                  \n"
"   movq  %r9,    40(%rsp)                  \n"
"   movsd %xmm0,  48(%rsp)                  \n"
"   movsd %xmm1,  56(%rsp)                  \n"
"   movsd %xmm2,  64(%rsp)                  \n"
"   movsd %xmm3,  72(%rsp)                  \n"
"   movsd %xmm4,  80(%rsp)                  \n"
"   movsd %xmm5,  88(%rsp)                  \n"
"   movsd %xmm6,  96(%rsp)                  \n"
"   movsd %xmm7, 104(%rsp)                  \n"
"   leaq  16(%rbp), %rax                    \n"
"   movq  %rax,  112(%rsp)                  \n"
"   movq  %rsp, %rdi                        \n"
"   callq _NSForwardingHandler              \n"
"   movq  120(%rsp), %rax                   \n"
"   movsd 128(%rsp), %xmm0                  \n"
"   movq  %rbp, %rsp                        \n"
"   popq  %rbp                              \n"
"   retq                                    \n"
);

extern void NSForwardingTrampoline(void);

#define NS_HAVE_FORWARDING_TRAMPOLINE 1

#endif /* __x86_64__ */

/* SysV classifies each argument as INTEGER or SSE; the two classes draw from
 * separate register banks, then spill to the stack in declaration order. */
static BOOL _isFloatEncoding(const char *type) {
    while (*type == 'r' || *type == 'n' || *type == 'N' || *type == 'o' ||
           *type == 'O' || *type == 'R' || *type == 'V') {
        type++;
    }
    return (*type == 'f' || *type == 'd');
}

static BOOL _isStructEncoding(const char *type) {
    while (*type == 'r' || *type == 'n' || *type == 'N' || *type == 'o' ||
           *type == 'O' || *type == 'R' || *type == 'V') {
        type++;
    }
    return (*type == '{' || *type == '(' || *type == '[');
}

void NSForwardingHandler(struct NSForwardFrame *frame) {
    id target = (id)(uintptr_t)frame->ints[0];
    SEL selector = (SEL)(uintptr_t)frame->ints[1];

    NSMethodSignature *signature = [target methodSignatureForSelector:selector];

    if (signature == nil) {
        [target doesNotRecognizeSelector:selector];
        return;
    }

    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    NSUInteger count = [signature numberOfArguments];
    unsigned intIndex = 0;
    unsigned floatIndex = 0;
    char *stackCursor = (char *)frame->stackArguments;

    for (NSUInteger i = 0; i < count; i++) {
        const char *type = [signature getArgumentTypeAtIndex:i];

        if (_isStructEncoding(type)) {
            [NSException raise:NSInvalidArgumentException
                        format:@"forwarding %s: struct arguments are not supported yet",
                               sel_getName(selector)];
            return;
        }

        if (_isFloatEncoding(type)) {
            if (floatIndex < 8) {
                double value = frame->floats[floatIndex++];

                if (*type == 'f') {
                    float narrowed = (float)value;

                    [invocation setArgument:&narrowed atIndex:(NSInteger)i];
                } else {
                    [invocation setArgument:&value atIndex:(NSInteger)i];
                }
                continue;
            }
        } else if (intIndex < 6) {
            uint64_t value = frame->ints[intIndex++];

            [invocation setArgument:&value atIndex:(NSInteger)i];
            continue;
        }

        /* Spilled: stack slots are eightbyte-aligned in declaration order. */
        [invocation setArgument:stackCursor atIndex:(NSInteger)i];
        stackCursor += 8;
    }

    [target forwardInvocation:invocation];

    NSUInteger returnLength = [signature methodReturnLength];

    if (returnLength > 0) {
        const char *returnType = [signature methodReturnType];

        if (_isStructEncoding(returnType)) {
            [NSException raise:NSInvalidArgumentException
                        format:@"forwarding %s: struct returns are not supported yet",
                               sel_getName(selector)];
            return;
        }
        if (_isFloatEncoding(returnType)) {
            if (*returnType == 'f') {
                float narrow = 0;

                [invocation getReturnValue:&narrow];
                frame->returnFloat = narrow;
            } else {
                double wide = 0;

                [invocation getReturnValue:&wide];
                frame->returnFloat = wide;
            }
        } else {
            uint64_t value = 0;

            [invocation getReturnValue:&value];
            frame->returnInt = value;
        }
    }
}

/* A struct return puts a hidden pointer in the first register, shifting self
 * and _cmd along; the non-stret trampoline would misread the frame, so this
 * reports the gap rather than corrupting the call. */
static void _NSForwardingStretHandler(void *returnBuffer, id target, SEL selector) {
    [NSException raise:NSInvalidArgumentException
                format:@"forwarding %s: struct returns are not supported yet",
                       sel_getName(selector)];
}

@implementation NSObject (NSForwarding)

/* objc4's NSObject has doesNotRecognizeSelector: but no -forwardInvocation:.
 * Without this default the handler would message a selector that is itself
 * unimplemented and re-enter forwarding forever. */
- (void)forwardInvocation:(NSInvocation *)invocation {
    [self doesNotRecognizeSelector:[invocation selector]];
}

+ (void)forwardInvocation:(NSInvocation *)invocation {
    [self doesNotRecognizeSelector:[invocation selector]];
}

@end

__attribute__((constructor))
static void _NSInstallForwardHandler(void) {
#if defined(NS_HAVE_FORWARDING_TRAMPOLINE)
    objc_setForwardHandler((void *)NSForwardingTrampoline,
                           (void *)_NSForwardingStretHandler);
#endif
}
