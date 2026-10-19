// SPDX-License-Identifier: MIT
// FreeBSD's vm/vm_page.h, the page fields linuxkpi uses
#pragma once
#include <vm/vm.h>
#include <sys/queue.h>

struct vm_object;

// linuxkpi's struct page is a vm_page, here backed by wired kernel memory
struct vm_page {
	vm_paddr_t phys_addr;
	void *pd_kva;
	// the cached alias while pd_kva is a write-combined or uncached view
	void *pd_kva_cached;
	int ref_count;
	int flags;
	struct vm_page *pd_hnext;
	void *pd_run;
	// ttm keeps pool pages on these lists
	union {
		TAILQ_ENTRY(vm_page) q;
	} plinks;
	struct vm_object *object;
	vm_pindex_t pindex;
	unsigned char oflags;
	vm_memattr_t pd_memattr;
};
typedef struct vm_page *vm_page_t;
TAILQ_HEAD(pglist, vm_page);

// flags
#define PG_FICTITIOUS 0x0004
#define PG_ZERO 0x0008
#define PG_MARKER 0x0010
// oflags
#define VPO_UNMANAGED 0x04

#define VM_PAGE_TO_PHYS(m) ((m)->phys_addr)
vm_page_t PHYS_TO_VM_PAGE(vm_paddr_t pa);
void pmap_zero_page(vm_page_t m);
void pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma);
