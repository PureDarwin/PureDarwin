/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <mach_assert.h>
#include <mach/vm_types.h>
#include <mach/mach_time.h>
#include <kern/timer.h>
#include <kern/clock.h>
#include <kern/machine.h>
#include <mach/machine.h>
#include <mach/machine/vm_param.h>
#include <riscv/caches_internal.h>
#include <riscv/cpu_data.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/cpu_internal.h>
#include <riscv/sbi.h>

#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/pmap.h>

#include <riscv/misc_protos.h>

// dcache_incoherent_io_flush64() dcache_incoherent_io_store64() result info
#define LWOpDone 1
#define BWOpDone 3

// from machine_routines.c and machine_routines_common.c
extern bool ml_riscv_isa_has(const char *ext);
extern uint32_t ml_riscv_dcache_line_size(void);
extern uint32_t ml_riscv_dcache_size(void);

// dma on the c906 bypasses its dcache, the t-head cmo instructions maintain it
#define THEAD_MVENDORID         0x5b7

// xtheadcmo, i-type with opcode custom-0 and the operation in imm[11:0]
#define TH_DCACHE_CVA(va)       __asm__ volatile (".insn i 0x0b, 0, x0, %0, 0x025" :: "r"(va) : "memory")
#define TH_DCACHE_CIVA(va)      __asm__ volatile (".insn i 0x0b, 0, x0, %0, 0x027" :: "r"(va) : "memory")
#define TH_DCACHE_CPA(pa)       __asm__ volatile (".insn i 0x0b, 0, x0, %0, 0x029" :: "r"(pa) : "memory")
#define TH_DCACHE_CIPA(pa)      __asm__ volatile (".insn i 0x0b, 0, x0, %0, 0x02b" :: "r"(pa) : "memory")
#define TH_DCACHE_CALL()        __asm__ volatile (".word 0x0010000b" ::: "memory")
#define TH_DCACHE_CIALL()       __asm__ volatile (".word 0x0030000b" ::: "memory")
#define TH_SYNC_S()             __asm__ volatile (".word 0x0190000b" ::: "memory")

__enum_decl(riscv_cmo_t, uint8_t, {
	RISCV_CMO_UNKNOWN,
	RISCV_CMO_COHERENT,
	RISCV_CMO_THEAD,
});

static riscv_cmo_t riscv_cmo = RISCV_CMO_UNKNOWN;

TUNABLE(bool, up_style_idle_exit, "up_style_idle_exit", false);

// decided on first use, the sbi and device tree answers never change after boot
static riscv_cmo_t
riscv_cmo_kind(void)
{
	riscv_cmo_t kind = os_atomic_load(&riscv_cmo, relaxed);

	if (__probable(kind != RISCV_CMO_UNKNOWN)) {
		return kind;
	}

	struct sbiret ret = sbi_ecall(SBI_EXT_BASE, SBI_BASE_GET_MVENDORID, 0, 0, 0, 0, 0, 0);
	if ((ret.error == SBI_SUCCESS && ret.value == THEAD_MVENDORID) ||
	    ml_riscv_isa_has("xtheadcmo")) {
		kind = RISCV_CMO_THEAD;
	} else {
		kind = RISCV_CMO_COHERENT;
	}
	os_atomic_store(&riscv_cmo, kind, relaxed);
	return kind;
}

static void
riscv_dcache_region(vm_offset_t va, vm_size_t length, bool invalidate)
{
	if (riscv_cmo_kind() != RISCV_CMO_THEAD) {
		// coherent harts and dma, ordering the stores is all that is left
		__asm__ volatile ("fence iorw, iorw" ::: "memory");
		return;
	}

	uint32_t line = ml_riscv_dcache_line_size();
	vm_offset_t end = va + length;

	for (va &= ~((vm_offset_t)line - 1); va < end; va += line) {
		if (invalidate) {
			TH_DCACHE_CIVA(va);
		} else {
			TH_DCACHE_CVA(va);
		}
	}
	TH_SYNC_S();
}

static void
riscv_dcache_region_phys(pmap_paddr_t pa, vm_size_t length, bool invalidate)
{
	if (riscv_cmo_kind() != RISCV_CMO_THEAD) {
		__asm__ volatile ("fence iorw, iorw" ::: "memory");
		return;
	}

	uint32_t line = ml_riscv_dcache_line_size();
	pmap_paddr_t end = pa + length;

	for (pa &= ~((pmap_paddr_t)line - 1); pa < end; pa += line) {
		if (invalidate) {
			TH_DCACHE_CIPA(pa);
		} else {
			TH_DCACHE_CPA(pa);
		}
	}
	TH_SYNC_S();
}

static void
riscv_dcache_all(bool invalidate)
{
	if (riscv_cmo_kind() != RISCV_CMO_THEAD) {
		__asm__ volatile ("fence iorw, iorw" ::: "memory");
		return;
	}

	if (invalidate) {
		TH_DCACHE_CIALL();
	} else {
		TH_DCACHE_CALL();
	}
	TH_SYNC_S();
}

// the size above which a whole cache operation beats walking the lines
static vm_size_t
riscv_dcache_bulk_size(void)
{
	if (riscv_cmo_kind() != RISCV_CMO_THEAD) {
		return 0;
	}
	return ml_riscv_dcache_size();
}

// fence.i covers this hart, the other harts get it through sbi
static void
riscv_icache_invalidate_all_harts(void)
{
	__asm__ volatile ("fence.i" ::: "memory");
	if (real_ncpus > 1) {
		(void)sbi_remote_fence_i(0, SBI_HART_MASK_BASE_ALL);
	}
}

void
flush_dcache(
	vm_offset_t addr,
	unsigned length,
	boolean_t phys)
{
	cpu_data_t      *cpu_data_ptr = getCpuDatap();
	vm_offset_t     vaddr;
	addr64_t        paddr;
	vm_size_t       count;

	while (length > 0) {
		if (phys) {
			count = length;
			paddr = CAST_DOWN(pmap_paddr_t, addr);
			vaddr = phystokv_range(paddr, &count);
		} else {
			paddr = kvtophys(addr);
			vaddr = addr;
			count = PAGE_SIZE - (addr & PAGE_MASK);
			if (count > length) {
				count = length;
			}
		}
		riscv_dcache_region(vaddr, count, true);
		if (paddr && (cpu_data_ptr->cpu_cache_dispatch != NULL)) {
			cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheCleanFlushRegion, (unsigned int) paddr, (unsigned)count);
		}
		addr += count;
		length -= count;
	}
	return;
}

void
flush_dcache64(
	addr64_t addr,
	unsigned count,
	int phys)
{
	flush_dcache((vm_offset_t)addr, count, phys);
}

void
clean_dcache(
	vm_offset_t addr,
	unsigned length,
	boolean_t phys)
{
	cpu_data_t      *cpu_data_ptr = getCpuDatap();
	vm_offset_t     vaddr;
	addr64_t        paddr;
	vm_size_t       count;

	while (length > 0) {
		if (phys) {
			count = length;
			paddr = CAST_DOWN(pmap_paddr_t, addr);
			vaddr = phystokv_range(paddr, &count);
		} else {
			paddr = kvtophys(addr);
			vaddr = addr;
			count = PAGE_SIZE - (addr & PAGE_MASK);
			if (count > length) {
				count = length;
			}
		}
		riscv_dcache_region(vaddr, count, false);
		if (paddr && (cpu_data_ptr->cpu_cache_dispatch != NULL)) {
			cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheCleanRegion, (unsigned int) paddr, (unsigned)count);
		}
		addr += count;
		length -= count;
	}
	return;
}

void
clean_dcache64(
	addr64_t addr,
	unsigned count,
	int phys)
{
	clean_dcache((vm_offset_t)addr, count, phys);
}

void
invalidate_icache(
	__unused vm_offset_t addr,
	__unused unsigned cnt,
	__unused int phys)
{
	riscv_icache_invalidate_all_harts();
}

void
invalidate_icache64(
	__unused addr64_t addr,
	__unused unsigned cnt,
	__unused int phys)
{
	riscv_icache_invalidate_all_harts();
}

void
flush_dcache_syscall(
	vm_offset_t va,
	unsigned length)
{
	vm_size_t bulk = riscv_dcache_bulk_size();

	if ((bulk != 0) && (length >= bulk)) {
		riscv_dcache_all(true);
		if (getCpuDatap()->cpu_cache_dispatch != NULL) {
			getCpuDatap()->cpu_cache_dispatch(getCpuDatap()->cpu_id, CacheCleanFlush, 0x0UL, 0x0UL);
		}
	} else {
		riscv_dcache_region((vm_offset_t) va, length, true);
	}
	return;
}

void
dcache_incoherent_io_flush64(
	addr64_t pa,
	unsigned int size,
	unsigned int remaining,
	unsigned int *res)
{
	cpu_data_t *cpu_data_ptr = getCpuDatap();
	vm_size_t bulk = riscv_dcache_bulk_size();

	if (riscv_cmo_kind() != RISCV_CMO_THEAD) {
		__asm__ volatile ("fence iorw, iorw" ::: "memory");
		*res = LWOpDone;
		return;
	}

	if ((bulk != 0) && (remaining >= bulk)) {
		riscv_dcache_all(true);
		if (cpu_data_ptr->cpu_cache_dispatch != NULL) {
			cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheCleanFlush, 0x0UL, 0x0UL);
		}
		*res = BWOpDone;
	} else {
		// the physical forms need no mapping, so io ranges outside the aperture work as well
		pmap_paddr_t paddr = CAST_DOWN(pmap_paddr_t, pa);

		riscv_dcache_region_phys(paddr, size, true);
		if (pmap_valid_address(paddr) && (cpu_data_ptr->cpu_cache_dispatch != NULL)) {
			cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheCleanFlushRegion, (unsigned int) paddr, size);
		}
	}

	return;
}

void
dcache_incoherent_io_store64(
	addr64_t pa,
	unsigned int size,
	unsigned int remaining,
	unsigned int *res)
{
	pmap_paddr_t paddr = CAST_DOWN(pmap_paddr_t, pa);
	cpu_data_t *cpu_data_ptr = getCpuDatap();
	vm_size_t bulk = riscv_dcache_bulk_size();

	if (riscv_cmo_kind() != RISCV_CMO_THEAD) {
		__asm__ volatile ("fence iorw, iorw" ::: "memory");
		*res = LWOpDone;
		return;
	}

	if (pmap_valid_address(paddr)) {
		unsigned int wimg_bits = pmap_cache_attributes((ppnum_t) (paddr >> PAGE_SHIFT));
		if ((wimg_bits == VM_WIMG_IO) || (wimg_bits == VM_WIMG_WCOMB) || (wimg_bits == VM_WIMG_RT)) {
			return;
		}
	}

	if ((bulk != 0) && (remaining >= bulk)) {
		riscv_dcache_all(false);
		if (cpu_data_ptr->cpu_cache_dispatch != NULL) {
			cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheClean, 0x0UL, 0x0UL);
		}
		*res = BWOpDone;
	} else {
		riscv_dcache_region_phys(paddr, size, false);
		if (pmap_valid_address(paddr) && (cpu_data_ptr->cpu_cache_dispatch != NULL)) {
			cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheCleanRegion, (unsigned int) paddr, size);
		}
	}

	return;
}

void
cache_sync_page(
	ppnum_t pp
	)
{
	pmap_paddr_t    paddr = ptoa(pp);

	// the page was written through the data side, push it out before anyone fetches from it
	if (pmap_valid_address(paddr)) {
		riscv_dcache_region(phystokv(paddr), PAGE_SIZE, false);
	} else {
		riscv_dcache_all(false);
	}
	riscv_icache_invalidate_all_harts();
}

void
platform_cache_init(
	void)
{
	cpu_data_t      *cpu_data_ptr = getCpuDatap();

	(void)riscv_cmo_kind();

	if (cpu_data_ptr->cpu_cache_dispatch != NULL) {
		cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheControl, CacheControlEnable, 0x0UL);
	}
}

void
platform_cache_flush(
	void)
{
	cpu_data_t      *cpu_data_ptr = getCpuDatap();

	riscv_dcache_all(true);

	if (cpu_data_ptr->cpu_cache_dispatch != NULL) {
		cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheCleanFlush, 0x0UL, 0x0UL);
	}
}

void
platform_cache_clean(
	void)
{
	cpu_data_t      *cpu_data_ptr = getCpuDatap();

	riscv_dcache_all(false);

	if (cpu_data_ptr->cpu_cache_dispatch != NULL) {
		cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheClean, 0x0UL, 0x0UL);
	}
}

void
platform_cache_shutdown(
	void)
{
	cpu_data_t      *cpu_data_ptr = getCpuDatap();

	riscv_dcache_all(false);

	if (cpu_data_ptr->cpu_cache_dispatch != NULL) {
		cpu_data_ptr->cpu_cache_dispatch(cpu_data_ptr->cpu_id, CacheShutdown, 0x0UL, 0x0UL);
	}
}

void
platform_cache_disable(void)
{
}

void
platform_cache_idle_enter(
	void)
{
	platform_cache_disable();

	// wfi keeps the caches powered, only a lone non-coherent hart writes back first
	if (up_style_idle_exit && (real_ncpus == 1)) {
		riscv_dcache_all(false);
	}
}

boolean_t
platform_cache_batch_wimg(
	__unused unsigned int new_wimg,
	__unused unsigned int size
	)
{
	boolean_t       do_cache_op = FALSE;
	vm_size_t       bulk = riscv_dcache_bulk_size();

	// coherent harts make the batched flush a single fence
	if (riscv_cmo_kind() != RISCV_CMO_THEAD) {
		return TRUE;
	}

	if ((bulk != 0) && (size >= bulk)) {
		do_cache_op = TRUE;
	}

	return do_cache_op;
}

void
platform_cache_flush_wimg(
	__unused unsigned int new_wimg
	)
{
	riscv_dcache_all(true);
	if (getCpuDatap()->cpu_cache_dispatch != NULL) {
		getCpuDatap()->cpu_cache_dispatch(getCpuDatap()->cpu_id, CacheCleanFlush, 0x0UL, 0x0UL);
	}
}
