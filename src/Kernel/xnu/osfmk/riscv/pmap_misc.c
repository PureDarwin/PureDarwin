// the parts of the riscv pmap that work on physical pages, memory types, static and device
// mappings, the per cpu copy windows and the read only zone

#include <mach/vm_param.h>
#include <kern/kern_types.h>
#include <kern/misc_protos.h>
#include <kern/thread.h>
#include <kern/zalloc.h>
#include <kern/zalloc_internal.h>
#include <vm/pmap.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_page_internal.h>
#include <riscv/pmap_internal.h>
#include <riscv/proc_reg.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/caches_internal.h>
#include <riscv/misc_protos.h>
#include <string.h>

extern uint16_t pmap_page_attributes(unsigned int pai);
extern void pmap_page_update_memtype(unsigned int pai, unsigned int wimg);
extern void pmap_page_attributes_update(unsigned int pai, uint16_t clear, uint16_t set);

uint64_t pmap_query_page_info_retries;

// --- page contents

void
pmap_zero_page(ppnum_t pn)
{
	bzero((void *)phystokv(ptoa(pn)), PAGE_SIZE);
}

void
pmap_zero_page_with_options(ppnum_t pn, __unused int options)
{
	pmap_zero_page(pn);
}

void
pmap_zero_part_page(ppnum_t pn, vm_offset_t offset, vm_size_t len)
{
	assert(offset + len <= PAGE_SIZE);
	bzero((void *)(phystokv(ptoa(pn)) + offset), len);
}

void
pmap_copy_page(ppnum_t src, ppnum_t dst, __unused int options)
{
	memcpy((void *)phystokv(ptoa(dst)), (const void *)phystokv(ptoa(src)), PAGE_SIZE);
}

void
pmap_copy_part_page(ppnum_t src, vm_offset_t src_offset, ppnum_t dst, vm_offset_t dst_offset, vm_size_t len)
{
	assert(src_offset + len <= PAGE_SIZE && dst_offset + len <= PAGE_SIZE);
	memcpy((void *)(phystokv(ptoa(dst)) + dst_offset), (const void *)(phystokv(ptoa(src)) + src_offset), len);
}

void
pmap_copy_part_lpage(vm_offset_t src, ppnum_t dst, vm_offset_t dst_offset, vm_size_t len)
{
	memcpy((void *)(phystokv(ptoa(dst)) + dst_offset), (const void *)src, len);
}

void
pmap_copy_part_rpage(ppnum_t src, vm_offset_t src_offset, vm_offset_t dst, vm_size_t len)
{
	memcpy((void *)dst, (const void *)(phystokv(ptoa(src)) + src_offset), len);
}

void
fillPage(ppnum_t pn, unsigned int fill)
{
	unsigned int *addr = (unsigned int *)phystokv(ptoa(pn));
	for (unsigned int count = PAGE_SIZE / sizeof(unsigned int); count > 0; count--) {
		*addr++ = fill;
	}
}

void *
pmap_map_compressor_page(ppnum_t pn)
{
	// the aperture maps every page with the default memory type
	return (void *)phystokv(ptoa(pn));
}

void
pmap_unmap_compressor_page(__unused ppnum_t pn, __unused void *kva)
{
}

// --- memory types

unsigned int
pmap_cache_attributes(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	if (!pa_valid(pa)) {
		return VM_WIMG_IO;
	}
	unsigned int wimg = pmap_page_attributes(pa_index(pa)) & PP_ATTR_WIMG_MASK;
	return wimg ? wimg : VM_WIMG_DEFAULT;
}

void
pmap_set_cache_attributes(ppnum_t pn, unsigned int cacheattr)
{
	pmap_paddr_t pa = ptoa(pn);
	if (!pa_valid(pa)) {
		return;
	}
	unsigned int wimg = cacheattr & VM_WIMG_MASK;
	if (wimg == VM_WIMG_DEFAULT) {
		wimg = 0;
	}
	unsigned int old = pmap_page_attributes(pa_index(pa)) & PP_ATTR_WIMG_MASK;
	if (old == wimg) {
		return;
	}
	pmap_page_update_memtype(pa_index(pa), wimg ? wimg : VM_WIMG_DEFAULT);
	if (wimg != 0 && wimg != VM_WIMG_COPYBACK) {
		// dirty lines of the cacheable alias must not land on top of later uncached writes
		flush_dcache64((addr64_t)pa, PAGE_SIZE, TRUE);
	}
}

void
pmap_batch_set_cache_attributes(const unified_page_list_t *page_list, unsigned int cacheattr)
{
	unified_page_list_iterator_t iter;
	for (unified_page_list_iterator_init(page_list, &iter);
	    !unified_page_list_iterator_end(&iter);
	    unified_page_list_iterator_next(&iter)) {
		bool is_fictitious = false;
		ppnum_t pn = unified_page_list_iterator_page(&iter, &is_fictitious);
		if (!is_fictitious) {
			pmap_set_cache_attributes(pn, cacheattr);
		}
	}
}

kern_return_t
pmap_attribute_cache_sync(ppnum_t pn, vm_size_t size, __unused vm_machine_attribute_t attribute,
    __unused vm_machine_attribute_val_t *value)
{
	if (size > PAGE_SIZE) {
		panic("%s: size 0x%llx is larger than a page", __func__, (uint64_t)size);
	}
	pmap_sync_page_attributes_phys(pn);
	return KERN_SUCCESS;
}

void
pmap_sync_page_data_phys(ppnum_t pn)
{
	flush_dcache64((addr64_t)ptoa(pn), PAGE_SIZE, TRUE);
	invalidate_icache64((addr64_t)ptoa(pn), PAGE_SIZE, TRUE);
}

void
pmap_sync_page_attributes_phys(ppnum_t pn)
{
	flush_dcache64((addr64_t)ptoa(pn), PAGE_SIZE, TRUE);
}

boolean_t
pmap_is_noencrypt(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	return pa_valid(pa) && (pmap_page_attributes(pa_index(pa)) & PP_ATTR_NOENCRYPT);
}

void
pmap_set_noencrypt(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	if (pa_valid(pa)) {
		pmap_page_attributes_update(pa_index(pa), 0, PP_ATTR_NOENCRYPT);
	}
}

void
pmap_clear_noencrypt(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	if (pa_valid(pa)) {
		pmap_page_attributes_update(pa_index(pa), PP_ATTR_NOENCRYPT, 0);
	}
}

// --- static and device mappings in the kernel

static pt_entry_t
pmap_static_prot_bits(vm_prot_t prot)
{
	pt_entry_t bits = PTE_V | PTE_G | PTE_A | PTE_D;
	if (prot & VM_PROT_WRITE) {
		bits |= PTE_R | PTE_W;
	} else if (prot & VM_PROT_READ) {
		bits |= PTE_R;
	}
	if (prot & VM_PROT_EXECUTE) {
		bits |= PTE_X;
	}
	return bits;
}

// maps [start, end) physical at va through pmap_enter, returns the next free va
vm_map_address_t
pmap_map(vm_map_address_t virt, vm_offset_t start, vm_offset_t end, vm_prot_t prot, unsigned int flags)
{
	vm_map_address_t va = virt;
	for (vm_offset_t pa = start; pa < end; pa += PAGE_SIZE, va += PAGE_SIZE) {
		kern_return_t kr = pmap_enter_options_addr(kernel_pmap, va, pa, prot, VM_PROT_NONE, flags,
		    TRUE, 0, NULL, PMAP_MAPPING_TYPE_INFER);
		if (kr != KERN_SUCCESS) {
			panic("%s: failed pmap_enter, virt=%p, start_addr=%p, end_addr=%p, prot=%#x, kr=%d",
			    __func__, (void *)virt, (void *)start, (void *)end, prot, kr);
		}
	}
	return va;
}

// device mappings, entered straight into the kernel tables with no pv tracking
vm_map_address_t
pmap_map_bd_with_options(vm_map_address_t virt, vm_offset_t start, vm_offset_t end, vm_prot_t prot, int32_t options)
{
	unsigned int wimg;
	switch (options & PMAP_MAP_BD_MASK) {
	case PMAP_MAP_BD_WCOMB:
		wimg = VM_WIMG_WCOMB;
		break;
	case PMAP_MAP_BD_POSTED:
		wimg = VM_WIMG_POSTED;
		break;
	case PMAP_MAP_BD_POSTED_REORDERED:
		wimg = VM_WIMG_POSTED_REORDERED;
		break;
	case PMAP_MAP_BD_POSTED_COMBINED_REORDERED:
		wimg = VM_WIMG_POSTED_COMBINED_REORDERED;
		break;
	default:
		wimg = VM_WIMG_IO;
		break;
	}
	pt_entry_t bits = pmap_static_prot_bits(prot & ~VM_PROT_EXECUTE) | pmap_wimg_to_pte(wimg);
	riscv_vm_map_static(virt, start, round_page(end) - trunc_page(start), bits);
	return virt + (round_page(end) - trunc_page(start));
}

vm_map_address_t
pmap_map_bd(vm_map_address_t virt, vm_offset_t start, vm_offset_t end, vm_prot_t prot)
{
	return pmap_map_bd_with_options(virt, start, end, prot, PMAP_MAP_BD_DEVICE);
}

kern_return_t
pmap_map_block_addr(pmap_t pmap, addr64_t va, pmap_paddr_t pa, uint32_t size, vm_prot_t prot, int attr, unsigned int flags)
{
	(void)flags;
	for (uint32_t page = 0; page < size; page++) {
		kern_return_t kr = pmap_enter_options_addr(pmap, va + ptoa(page), pa + ptoa(page), prot,
		    VM_PROT_NONE, (unsigned int)attr, FALSE, 0, NULL, PMAP_MAPPING_TYPE_INFER);
		if (kr != KERN_SUCCESS) {
			// undo what was entered so a failed block leaves nothing behind
			pmap_remove(pmap, va, va + ptoa(page));
			return kr;
		}
	}
	return KERN_SUCCESS;
}

kern_return_t
pmap_map_block(pmap_t pmap, addr64_t va, ppnum_t pa, uint32_t size, vm_prot_t prot, int attr, unsigned int flags)
{
	return pmap_map_block_addr(pmap, va, ptoa(pa), size, prot, attr, flags);
}

// --- per cpu copy windows, for touching physical pages with a chosen memory type

vm_offset_t
pmap_cpu_windows_copy_addr(int cpu_num, unsigned int index)
{
	if (__improbable(index >= CPUWINDOWS_MAX)) {
		panic("%s: invalid index %u", __func__, index);
	}
	return (vm_offset_t)(CPUWINDOWS_BASE + (PAGE_SIZE * ((CPUWINDOWS_MAX * (unsigned int)cpu_num) + index)));
}

unsigned int
pmap_map_cpu_windows_copy(ppnum_t pn, vm_prot_t prot, unsigned int wimg_bits)
{
	pmap_cpu_data_t *pmap_cpu_data = pmap_get_cpu_data();
	unsigned int cpu_num = pmap_cpu_data->cpu_number;
	unsigned int i;

	for (i = 0; i < CPUWINDOWS_MAX; i++) {
		vm_offset_t va = pmap_cpu_windows_copy_addr((int)cpu_num, i);
		if (kvtophys(va) == 0) {
			break;
		}
	}
	if (i == CPUWINDOWS_MAX) {
		panic("%s: out of windows", __func__);
	}

	vm_offset_t va = pmap_cpu_windows_copy_addr((int)cpu_num, i);
	pt_entry_t bits = pmap_static_prot_bits(prot & ~VM_PROT_EXECUTE) | pmap_wimg_to_pte(wimg_bits ? wimg_bits : VM_WIMG_DEFAULT);
	riscv_vm_map_static(va, ptoa(pn), PAGE_SIZE, bits);
	// local only, a window is used by its own cpu with preemption off
	sfence_vma_va(va);
	pmap_cpu_data->copywindow_strong_sync[i] = (wimg_bits == VM_WIMG_IO);
	return i;
}

void
pmap_unmap_cpu_windows_copy(unsigned int index)
{
	pmap_cpu_data_t *pmap_cpu_data = pmap_get_cpu_data();
	vm_offset_t va = pmap_cpu_windows_copy_addr((int)pmap_cpu_data->cpu_number, index);
	riscv_vm_map_static(va, 0, PAGE_SIZE, 0);
	sfence_vma_va(va);
}

// --- the read only zone, written through the aperture since its kernel mapping is read only

void
pmap_ro_zone_memcpy(zone_id_t zid, vm_offset_t va, vm_offset_t offset, vm_offset_t new_data, vm_size_t new_data_size)
{
	(void)zid;
	vm_offset_t dst = phystokv(kvtophys_nofail(va + offset));
	assert(((va + offset) & PAGE_MASK) + new_data_size <= PAGE_SIZE);
	memcpy((void *)dst, (const void *)new_data, new_data_size);
}

uint64_t
pmap_ro_zone_atomic_op(zone_id_t zid, vm_offset_t va, vm_offset_t offset, uint32_t op, uint64_t value)
{
	(void)zid;
	vm_offset_t dst = phystokv(kvtophys_nofail(va + offset));
	return __zalloc_ro_mut_atomic(dst, (zro_atomic_op_t)op, value);
}

void
pmap_ro_zone_bzero(zone_id_t zid, vm_offset_t va, vm_offset_t offset, vm_size_t size)
{
	(void)zid;
	vm_offset_t dst = phystokv(kvtophys_nofail(va + offset));
	assert(((va + offset) & PAGE_MASK) + size <= PAGE_SIZE);
	bzero((void *)dst, size);
}

// --- policy queries

void
pmap_set_vm_map_cs_enforced(pmap_t pmap, bool new_value)
{
	pmap->pmap_vm_map_cs_enforced = new_value;
}

bool
pmap_get_vm_map_cs_enforced(pmap_t pmap)
{
	return pmap->pmap_vm_map_cs_enforced;
}

void
pmap_set_jit_entitled(pmap_t pmap)
{
	pmap->jit_entitled = true;
}

bool
pmap_get_jit_entitled(pmap_t pmap)
{
	return pmap->jit_entitled;
}

void
pmap_set_tpro(pmap_t pmap)
{
	pmap->tpro = true;
}

bool
pmap_get_tpro(pmap_t pmap)
{
	return pmap->tpro;
}

bool
pmap_has_prot_policy(__unused pmap_t pmap, __unused bool translated_allow_execute, __unused vm_prot_t prot)
{
	return false;
}

bool
pmap_in_ppl(void)
{
	return false;
}

bool
pmap_is_page_restricted(__unused ppnum_t pn)
{
	return false;
}

uint32_t
pmap_user_va_bits(__unused pmap_t pmap)
{
	// sv39 gives the user half 38 bits
	return RISCV_VA_BITS - 1;
}

uint32_t
pmap_kernel_va_bits(void)
{
	return RISCV_VA_BITS - 1;
}

vm_map_offset_t
pmap_max_64bit_offset(unsigned int option)
{
	switch (option) {
	case ARM_PMAP_MAX_OFFSET_DEFAULT:
	case ARM_PMAP_MAX_OFFSET_MAX:
	case ARM_PMAP_MAX_OFFSET_JUMBO:
	case ARM_PMAP_MAX_OFFSET_DEVICE:
		return MACH_VM_MAX_ADDRESS;
	case ARM_PMAP_MAX_OFFSET_MIN:
		// the end of the shared region and room after it
		return SHARED_REGION_BASE_RISCV64 + SHARED_REGION_SIZE_RISCV64 + (512ULL << 20);
	default:
		panic("%s: illegal option 0x%x", __func__, option);
	}
}

vm_map_offset_t
pmap_max_32bit_offset(__unused unsigned int option)
{
	panic("%s: riscv64 has no 32-bit address spaces", __func__);
}

vm_map_offset_t
pmap_max_offset(boolean_t is64, unsigned int option)
{
	return is64 ? pmap_max_64bit_offset(option) : pmap_max_32bit_offset(option);
}

void
pmap_advise_pagezero_range(__unused pmap_t p, __unused uint64_t a)
{
}

boolean_t
coredumpok(vm_map_t map, mach_vm_offset_t va)
{
	pmap_paddr_t pa = pmap_find_pa(map->pmap, va);
	if (pa == 0 || vm_map_entry_has_device_pager(map, va)) {
		return FALSE;
	}
	return pa_valid(pa) && pmap_cache_attributes((ppnum_t)atop(pa)) == VM_WIMG_DEFAULT;
}

// --- debugging

kern_return_t
pmap_dump_page_tables(pmap_t pmap, void *bufp, void *buf_end, unsigned int level_mask, size_t *bytes_copied)
{
	// writes every valid entry as its address and value, level by level as kdp asks
	uint64_t *out = (uint64_t *)bufp;
	uint64_t *end = (uint64_t *)buf_end;

	for (unsigned int i = 0; i < RISCV_TT_ENTRIES; i++) {
		tt_entry_t l1 = pmap->tte[i];
		if (!pte_is_valid(l1)) {
			continue;
		}
		if (level_mask & (1U << 1)) {
			if (out + 2 > end) {
				return KERN_INSUFFICIENT_BUFFER_SIZE;
			}
			*out++ = (uint64_t)&pmap->tte[i];
			*out++ = l1;
		}
		if (!pte_is_table(l1)) {
			continue;
		}
		tt_entry_t *l2 = (tt_entry_t *)phystokv(PTE_TO_PA(l1));
		for (unsigned int j = 0; j < RISCV_TT_ENTRIES; j++) {
			if (!pte_is_valid(l2[j])) {
				continue;
			}
			if (level_mask & (1U << 2)) {
				if (out + 2 > end) {
					return KERN_INSUFFICIENT_BUFFER_SIZE;
				}
				*out++ = (uint64_t)&l2[j];
				*out++ = l2[j];
			}
			if (!pte_is_table(l2[j]) || !(level_mask & (1U << 3))) {
				continue;
			}
			pt_entry_t *l3 = (pt_entry_t *)phystokv(PTE_TO_PA(l2[j]));
			for (unsigned int k = 0; k < RISCV_TT_ENTRIES; k++) {
				if (!pte_is_valid(l3[k])) {
					continue;
				}
				if (out + 2 > end) {
					return KERN_INSUFFICIENT_BUFFER_SIZE;
				}
				*out++ = (uint64_t)&l3[k];
				*out++ = l3[k];
			}
		}
	}
	*bytes_copied = (size_t)((uintptr_t)out - (uintptr_t)bufp);
	return KERN_SUCCESS;
}

#if DEVELOPMENT || DEBUG
// flips a bit in the kernel text page at pa through the aperture, used by vm tests
kern_return_t
pmap_test_text_corruption(pmap_paddr_t pa)
{
	if (!pa_valid(pa)) {
		return KERN_INVALID_ARGUMENT;
	}
	volatile uint8_t *byte = (volatile uint8_t *)phystokv(pa);
	*byte ^= 0x1;
	invalidate_icache64((addr64_t)pa, 1, TRUE);
	return KERN_SUCCESS;
}
#endif /* DEVELOPMENT || DEBUG */
