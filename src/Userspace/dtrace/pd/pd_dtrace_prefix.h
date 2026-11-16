// pd_dtrace_prefix.h - declarations libdtrace's no-CoreSymbolication build otherwise gets through the CoreSymbolication headers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_DTRACE_PREFIX_H
#define PD_DTRACE_PREFIX_H

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <dispatch/dispatch.h>

// xnu syscall, declared in the kernel's private sys/proc_private.h
int pid_resume(int pid);

#endif
