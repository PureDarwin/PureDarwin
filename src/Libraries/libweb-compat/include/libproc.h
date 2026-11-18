// libproc.h - the process queries AppKit makes, answered for a page, which has no process table
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef _LIBPROC_H_
#define _LIBPROC_H_

#include <stdint.h>
#include <sys/types.h>

#define PROC_PIDPATHINFO_MAXSIZE 4096

// no other process to find, and this one has no executable path
static inline int proc_pidpath(int pid, void *buffer, uint32_t buffersize) { (void)pid; (void)buffer; (void)buffersize; return 0; }
// this page is the only process
static inline int proc_listallpids(void *buffer, int buffersize) { if (buffer != NULL && buffersize >= (int)sizeof(int)) *(int *)buffer = 1; return 1; }
static inline int proc_name(int pid, void *buffer, uint32_t buffersize) { (void)pid; (void)buffer; (void)buffersize; return 0; }

#endif
