// hello-jit.S - a syscall in code made at run time, which only the EXC_SYSCALL fallback can serve
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
    .text
    .globl _start
_start:
    // an anonymous RWX page: mmap(0, 4096, 7, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
    mov $9, %eax
    xor %edi, %edi
    mov $4096, %esi
    mov $7, %edx
    mov $0x22, %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    syscall
    mov %rax, %rbx
    lea code(%rip), %rsi
    mov %rbx, %rdi
    mov $(code_end - code), %ecx
    rep movsb
    lea message(%rip), %r12
    call *%rbx
    // exit with the copied write's result, the message length when the fallback served it
    mov %eax, %edi
    sub $(message_end - message), %edi
    mov $60, %eax
    syscall

    .data
// write(1, r12, length); ret, as bytes so the converter never sees a syscall in them
code:
    .byte 0xb8, 0x01, 0x00, 0x00, 0x00
    .byte 0xbf, 0x01, 0x00, 0x00, 0x00
    .byte 0x4c, 0x89, 0xe6
    .byte 0xba, message_end - message, 0x00, 0x00, 0x00
    .byte 0x0f, 0x05
    .byte 0xc3
code_end:
message:
    .ascii "hello from a run-time syscall\n"
message_end:
