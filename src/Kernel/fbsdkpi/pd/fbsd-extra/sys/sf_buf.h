// SPDX-License-Identifier: MIT
// FreeBSD's sf_buf(9) over always mapped pages
#pragma once
#include <vm/vm_page.h>

// every linuxkpi page keeps a kernel mapping, so an sf_buf is the page itself
struct sf_buf;
#define SFB_CPUPRIVATE 0x0001
#define SFB_NOWAIT 0x0004
static inline struct sf_buf *sf_buf_alloc(vm_page_t m, int flags) { return (struct sf_buf *)m; }
static inline void sf_buf_free(struct sf_buf *sf) { }
static inline vm_offset_t sf_buf_kva(struct sf_buf *sf) { return (vm_offset_t)((vm_page_t)sf)->pd_kva; }
static inline vm_page_t sf_buf_page(struct sf_buf *sf) { return (vm_page_t)sf; }
