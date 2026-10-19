// SPDX-License-Identifier: MIT
// FreeBSD's vm/vm.h types linuxkpi uses
#pragma once
// before include_next, fbio.h can come back here mid-way
typedef char vm_memattr_t;
#include_next <vm/vm.h>

typedef unsigned long vm_pindex_t;
typedef long long vm_ooffset_t;
typedef unsigned char vm_prot_t;
#define VM_PROT_NONE 0
#define VM_PROT_READ 1
#define VM_PROT_WRITE 2
#define VM_PROT_EXECUTE 4
#define VM_PROT_ALL (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE)
#define VM_PROT_RW (VM_PROT_READ | VM_PROT_WRITE)
#define VM_MEMATTR_UNCACHEABLE 0
#define VM_MEMATTR_WRITE_COMBINING 1
#define VM_MEMATTR_WRITE_BACK 6
#define VM_MEMATTR_DEFAULT VM_MEMATTR_WRITE_BACK
static inline int pmap_change_attr(vm_offset_t va, vm_size_t sz, int mode) { return 0; }
