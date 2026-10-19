// SPDX-License-Identifier: MIT
// FreeBSD's vm/vm_phys.h, fake pages for device apertures
#pragma once
#include <vm/vm.h>
#include <vm/vm_page.h>

// device apertures (vram bars) get fake pages so their pfns can be mapped
int vm_phys_fictitious_reg_range(vm_paddr_t start, vm_paddr_t end, vm_memattr_t memattr);
void vm_phys_fictitious_unreg_range(vm_paddr_t start, vm_paddr_t end);
vm_page_t vm_phys_fictitious_to_vm_page(vm_paddr_t pa);
