/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <fcntl.h>
#include <unistd.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <Foundation/NSObjCRuntime.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#if defined(__x86_64__)
#define NS_FORWARD_INT_REGS 6
struct NSForwardFrame {
    uint64_t ints[6];       /* rdi rsi rdx rcx r8 r9 */
    double floats[8];       /* xmm0..xmm7 */
    void *stackArguments;   /* first argument passed on the stack */
    uint64_t returnInt;
    double returnFloat;
};
#define NS_FORWARD_CALLER(frame) (((void **)(frame)->stackArguments)[-1])
#else
// arm64: x0..x7 and d0..d7, the caller's lr, stack arguments at the entry sp
#define NS_FORWARD_INT_REGS 8
struct NSForwardFrame {
    uint64_t ints[8];
    double floats[8];
    void *stackArguments;
    uint64_t returnInt;
    double returnFloat;
    void *caller;
};
#define NS_FORWARD_CALLER(frame) ((frame)->caller)
#endif

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

#elif defined(__arm64__) || defined(__aarch64__)

// the same frame for the C handler, filled from the arm64 argument registers
__asm__(
"   .text                                   \n"
"   .globl _NSForwardingTrampoline          \n"
"   .p2align 2                              \n"
"_NSForwardingTrampoline:                   \n"
"   stp   x29, x30, [sp, #-16]!             \n"
"   mov   x29, sp                           \n"
"   sub   sp, sp, #160                      \n"
"   stp   x0, x1, [sp, #0]                  \n"
"   stp   x2, x3, [sp, #16]                 \n"
"   stp   x4, x5, [sp, #32]                 \n"
"   stp   x6, x7, [sp, #48]                 \n"
"   stp   d0, d1, [sp, #64]                 \n"
"   stp   d2, d3, [sp, #80]                 \n"
"   stp   d4, d5, [sp, #96]                 \n"
"   stp   d6, d7, [sp, #112]                \n"
"   add   x9, x29, #16                      \n"
"   str   x9, [sp, #128]                    \n"
"   str   x30, [sp, #152]                   \n"
"   mov   x0, sp                            \n"
"   bl    _NSForwardingHandler              \n"
"   ldr   x0, [sp, #136]                    \n"
"   ldr   d0, [sp, #144]                    \n"
"   mov   sp, x29                           \n"
"   ldp   x29, x30, [sp], #16               \n"
"   ret                                     \n"
);

extern void NSForwardingTrampoline(void);

#define NS_HAVE_FORWARDING_TRAMPOLINE 1

#endif // __x86_64__, arm64

// SysV classifies each argument as INTEGER or SSE, the two classes draw from
// separate register banks, then spill to the stack in declaration order
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

// PD_OBJC_UNRECOGNIZED=continue logs unrecognized selectors and returns zero
static BOOL _unrecognizedContinue(void) {
    static int state = -1;

    if (state < 0) {
        const char *value = getenv("PD_OBJC_UNRECOGNIZED");
        state = value != NULL && strcmp(value, "continue") == 0;
    }
    return state;
}

// names the gap and its caller on stderr and the console, before any abort
static void _logUnrecognized(id target, SEL selector, void *caller) {
    Dl_info info;
    char line[512];
    BOOL meta = class_isMetaClass(object_getClass(target));
    int length, fd;

    if (dladdr(caller, &info) == 0)
        info.dli_fname = "?", info.dli_fbase = NULL;
    length = snprintf(line, sizeof(line), "PD-UNRECOGNIZED %c[%s %s] from %s+0x%lx in %s[%d]\n", meta ? '+' : '-',
                      object_getClassName(target), sel_getName(selector), info.dli_fname,
                      (unsigned long)((char *)caller - (char *)info.dli_fbase), getprogname(), getpid());
    if (length <= 0)
        return;
    if (length >= (int)sizeof(line))
        length = sizeof(line) - 1;
    write(STDERR_FILENO, line, length);
    if ((fd = open("/dev/console", O_WRONLY | O_NOCTTY | O_NONBLOCK)) >= 0) {
        write(fd, line, length);
        close(fd);
    }
}

void NSForwardingHandler(struct NSForwardFrame *frame) {
    id target = (id)(uintptr_t)frame->ints[0];
    SEL selector = (SEL)(uintptr_t)frame->ints[1];

    NSMethodSignature *signature = [target methodSignatureForSelector:selector];

    if (signature == nil) {
        _logUnrecognized(target, selector, NS_FORWARD_CALLER(frame));
        if (_unrecognizedContinue()) {
            // debug aid for porting: carry on with zero instead of aborting
            frame->returnInt = 0;
            frame->returnFloat = 0;
            return;
        }
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
                // a float travels single precision in the low half of the register, both arches
                [invocation setArgument:&frame->floats[floatIndex++] atIndex:(NSInteger)i];
                continue;
            }
        } else if (intIndex < NS_FORWARD_INT_REGS) {
            uint64_t value = frame->ints[intIndex++];

            [invocation setArgument:&value atIndex:(NSInteger)i];
            continue;
        }

#if defined(__x86_64__)
        /* Spilled: stack slots are eightbyte-aligned in declaration order. */
        [invocation setArgument:stackCursor atIndex:(NSInteger)i];
        stackCursor += 8;
#else
        // darwin arm64 packs stack arguments at their natural alignment, with no 8-byte slots
        NSUInteger size = 0, align = 1;
        NSGetSizeAndAlignment(type, &size, &align);
        if (align > 1)
            stackCursor = (char *)(((uintptr_t)stackCursor + align - 1) & ~(uintptr_t)(align - 1));
        [invocation setArgument:stackCursor atIndex:(NSInteger)i];
        stackCursor += size;
#endif
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
            // a float result goes back in the low half of the register, as the caller reads it
            frame->returnFloat = 0;
            [invocation getReturnValue:&frame->returnFloat];
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
