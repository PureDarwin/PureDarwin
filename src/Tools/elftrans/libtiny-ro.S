# libtiny-ro.S - test a dynamic relocation in a read-only segment
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
.global greet
.type greet,@function
.text
greet:
    mov $1, %rax
    mov $1, %rdi
    mov ptr(%rip), %rsi
    mov $message_end-message, %rdx
    syscall
    ret

.section .rodata
ptr:
    .quad message
message:
    .ascii "hello from read-only relocation on XNU\n"
message_end:
