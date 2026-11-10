# hello-absolute.S - exercise AOT relocation of an absolute address
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
.global _start
.text
_start:
    mov $1, %rax
    mov $1, %rdi
    movabs $message, %rsi
    mov $message_end-message, %rdx
    syscall
    mov $60, %rax
    xor %rdi, %rdi
    syscall

.section .rodata
message:
    .ascii "hello from Linux AOT absolute address on XNU\n"
message_end:
