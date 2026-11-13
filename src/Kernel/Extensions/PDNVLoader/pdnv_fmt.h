// pdnv_fmt.h - a vsnprintf for the resource manager's format strings, written to the Linux kernel's rules
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PDNV_FMT_H
#define PDNV_FMT_H

#include <stdarg.h>
#include <stddef.h>

// as vsnprintf: always terminated when size > 0, returns the length it would have written
int pdnv_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

#endif
