// SPDX-License-Identifier: MIT
// FreeBSD's sys/stdarg.h on the compiler builtins
#pragma once
#ifndef _VA_LIST_DECLARED
#define _VA_LIST_DECLARED
typedef __builtin_va_list va_list;
#endif
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_copy(d, s) __builtin_va_copy(d, s)
#define __va_copy(d, s) __builtin_va_copy(d, s)
