# libtiny.S - tiny Linux ELF shared library for dynamic loading
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
.global greet
.type greet,@function
.text
greet:
    mov $1, %rax
    mov $1, %rdi
    lea message(%rip), %rsi
    mov $message_end-message, %rdx
    syscall
    ret

.section .rodata
message:
    .ascii "hello from Linux DSO on XNU\n"
message_end:
