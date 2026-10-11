// a minimal stack switch for the compute fibers: callee-saved registers on the old stack, then the
// new stack's. ucontext is deprecated on Darwin and missing from PD's libSystem export list
#include "fiber.h"

#include <stdint.h>
#include <string.h>

#ifdef __APPLE__
#define SYM(n) "_" #n
#else
#define SYM(n) #n
#endif

#if defined(__aarch64__)
__asm__(".text\n.globl " SYM(pd_fiber_switch) "\n.p2align 2\n" SYM(pd_fiber_switch) ":\n"
        "  sub sp, sp, #160\n"
        "  stp x19, x20, [sp, #0]\n  stp x21, x22, [sp, #16]\n  stp x23, x24, [sp, #32]\n"
        "  stp x25, x26, [sp, #48]\n  stp x27, x28, [sp, #64]\n  stp x29, x30, [sp, #80]\n"
        "  stp d8, d9, [sp, #96]\n  stp d10, d11, [sp, #112]\n  stp d12, d13, [sp, #128]\n  stp d14, d15, [sp, #144]\n"
        "  mov x2, sp\n  str x2, [x0]\n  mov sp, x1\n"
        "  ldp x19, x20, [sp, #0]\n  ldp x21, x22, [sp, #16]\n  ldp x23, x24, [sp, #32]\n"
        "  ldp x25, x26, [sp, #48]\n  ldp x27, x28, [sp, #64]\n  ldp x29, x30, [sp, #80]\n"
        "  ldp d8, d9, [sp, #96]\n  ldp d10, d11, [sp, #112]\n  ldp d12, d13, [sp, #128]\n  ldp d14, d15, [sp, #144]\n"
        "  add sp, sp, #160\n  ret\n");

// a new fiber's first switch returns into the start routine (x30) with its argument in x19
__asm__(".text\n.p2align 2\n" SYM(pd_fiber_start) ":\n  mov x0, x19\n  blr x20\n  brk #1\n");
extern void pd_fiber_start(void);

void *pd_fiber_init(void *stack, size_t size, void (*fn)(void *), void *arg) {
  uint64_t *sp = (uint64_t *)(((uintptr_t)stack + size) & ~(uintptr_t)15) - 20;

  memset(sp, 0, 160);
  sp[0] = (uint64_t)(uintptr_t)arg;
  sp[1] = (uint64_t)(uintptr_t)fn;
  sp[11] = (uint64_t)(uintptr_t)pd_fiber_start;
  return sp;
}
#elif defined(__x86_64__)
__asm__(".text\n.globl " SYM(pd_fiber_switch) "\n" SYM(pd_fiber_switch) ":\n"
        "  pushq %rbp\n  pushq %rbx\n  pushq %r12\n  pushq %r13\n  pushq %r14\n  pushq %r15\n"
        "  movq %rsp, (%rdi)\n  movq %rsi, %rsp\n"
        "  popq %r15\n  popq %r14\n  popq %r13\n  popq %r12\n  popq %rbx\n  popq %rbp\n  ret\n");

// a new fiber's first switch returns here with fn in r12 and its argument in r13
__asm__(".text\n" SYM(pd_fiber_start) ":\n  subq $8, %rsp\n  movq %r13, %rdi\n  callq *%r12\n  ud2\n");
extern void pd_fiber_start(void);

void *pd_fiber_init(void *stack, size_t size, void (*fn)(void *), void *arg) {
  uint64_t *sp = (uint64_t *)(((uintptr_t)stack + size) & ~(uintptr_t)15) - 8;

  // r15 r14 r13 r12 rbx rbp, then the return address, the start routine is entered 16-byte misaligned
  // by one push like a call
  memset(sp, 0, 64);
  sp[2] = (uint64_t)(uintptr_t)arg;
  sp[3] = (uint64_t)(uintptr_t)fn;
  sp[6] = (uint64_t)(uintptr_t)pd_fiber_start;
  return sp;
}
#else
#error "pd_fiber_switch: no stack switch for this architecture"
#endif
