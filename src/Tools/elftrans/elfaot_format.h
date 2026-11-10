// elfaot_format.h - the records elfaot appends to a converted x86-64 ELF
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef ELFAOT_FORMAT_H
#define ELFAOT_FORMAT_H

#include <stdint.h>

#define ELFAOT_MAGIC "PDAOT003"
#define ELFAOT_MAX_SYSCALLS 65536U

// after the ELF's own bytes: the syscall sites (ELF virtual addresses, each now int3 then nop), then this footer
typedef struct {
    char magic[8];
    uint64_t syscall_count;
    // the file it was made from, the cache uses it only while that file is unchanged
    uint64_t source_dev, source_ino, source_size;
    int64_t source_mtime_sec, source_mtime_nsec;
} AotFooter;

#endif
