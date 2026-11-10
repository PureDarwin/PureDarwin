# hello-dynamic.S - call a function from a Linux ELF shared library
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
.global _start
.text
_start:
    call greet@PLT
    mov $60, %rax
    xor %rdi, %rdi
    syscall
