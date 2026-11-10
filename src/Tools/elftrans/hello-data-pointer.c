// hello-data-pointer.c - exercise AOT relocation of a stored pointer
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
static const char message[] = "hello from Linux data pointer on XNU\n";
static const char *ptr = message;

__attribute__((noreturn)) void
_start(void)
{
    const char *p = ptr;
    register long nr __asm__("rax") = 1;
    register long fd __asm__("rdi") = 1;
    register const char *buf __asm__("rsi") = p;
    register long len __asm__("rdx") = sizeof(message) - 1;

    __asm__ volatile("syscall" : "+a"(nr) : "D"(fd), "S"(buf), "d"(len)
        : "rcx", "r11", "memory");
    nr = 60;
    fd = 0;
    __asm__ volatile("syscall" : "+a"(nr) : "D"(fd)
        : "rcx", "r11", "memory");
    __builtin_unreachable();
}
