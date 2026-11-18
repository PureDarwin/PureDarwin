// execinfo.h - backtrace(3) for the browser build, where there is no stack to walk from C
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef _PD_WEB_EXECINFO_H
#define _PD_WEB_EXECINFO_H

#ifdef __cplusplus
extern "C" {
#endif

// no frames: callers print an empty trace
int backtrace(void **frames, int max);
char **backtrace_symbols(void *const *frames, int count);
void backtrace_symbols_fd(void *const *frames, int count, int fd);

#ifdef __cplusplus
}
#endif

#endif
