/*
 * Copyright (c) 2007-2021, 2023 Apple Inc. All rights reserved.
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
#ifndef _RISCV_PMAP_H_
#define _RISCV_PMAP_H_

#include <mach_assert.h>
#include <riscv/proc_reg.h>

#ifndef ASSEMBLER

#include <stdatomic.h>
#include <stdbool.h>
#include <libkern/section_keywords.h>
#include <mach/kern_return.h>
#include <mach/machine/vm_types.h>
#include <riscv/pmap_public.h>
#include <kern/ast.h>
#include <mach/riscv/thread_status.h>
#include <os/refcnt.h>

// satp carries up to 16 asid bits, harts report how many they keep
#define MAX_HW_ASIDS            (1 << 16)

#define NBBY                    (8)

#define CPUWINDOWS_MAX          4

// root, one l2 and a handful of l3 tables to map the kernel before pmap_bootstrap
#define BOOTSTRAP_TABLE_SIZE    (RISCV_PGBYTES * 8)

typedef uint64_t tt_entry_t; /* translation table entry type */
typedef uint64_t pt_entry_t; /* page table entry type */

#define PT_ENTRY_NULL ((pt_entry_t *) 0)
#define TT_ENTRY_NULL ((tt_entry_t *) 0)

struct pmap_cpu_data {
	pmap_t          cpu_nested_pmap;
	pmap_t          cpu_user_pmap;
	unsigned int    cpu_number;
	uint64_t        cpu_asid_gen;           /* asid generation this hart last flushed for */
	bool            copywindow_strong_sync[CPUWINDOWS_MAX];
};
typedef struct pmap_cpu_data pmap_cpu_data_t;

#include <mach/vm_prot.h>
#include <mach/vm_statistics.h>
#include <mach/machine/vm_param.h>
#include <kern/kern_types.h>
#include <kern/thread.h>
#include <kern/queue.h>

#include <sys/cdefs.h>

#define KERNEL_PMAP_HEAP_RANGE_START VM_MIN_KERNEL_AND_KEXT_ADDRESS

#define PAGE_RATIO        1
#define TEST_PAGE_RATIO_4 (0)

#define SUPERPAGE_NBASEPAGES 1 /* No superpages support */

#define pa_to_pte(a)        PA_TO_PTE(a)
#define pte_to_pa(p)        PTE_TO_PA(p)
#define pte_increment_pa(p) ((p) += (1ULL << PTE_PPN_SHIFT))

#define pmap_cs_log(level, fmt, args...)
#define pmap_cs_log_debug(fmt, args...)
#define pmap_cs_log_info(fmt, args...)
#define pmap_cs_log_error(fmt, args...)
#define pmap_cs_log_force(level, fmt, args...)

#define ttetokv(a) (phystokv(pte_to_pa(a)))
#define ptetokv(a) (phystokv(pte_to_pa(a)))

struct pmap {
	// the sv39 root table and its physical address, which goes into satp
	tt_entry_t *            tte;
	pmap_paddr_t            ttep;

	// lowest supported va (inclusive) and highest (exclusive)
	vm_map_address_t        min;
	vm_map_address_t        max;

	ledger_t                ledger;

	lck_rw_t                rwlock;

	// global list of pmaps
	queue_chain_t           pmaps;

	// the shared region nested in this pmap, shared at l3 table granularity
	struct pmap *           nested_pmap;
	vm_map_address_t        nested_region_addr;
	vm_map_offset_t         nested_region_size;
	vm_map_offset_t         nested_region_true_start;
	vm_map_offset_t         nested_region_true_end;
	unsigned int *          nested_region_unnested_table_bitmap;
	unsigned int            nested_region_unnested_table_bitmap_size;

	os_ref_atomic_t         ref_count;

	uint32_t                nested_no_bounds_refcnt;

	// the hardware asid and the allocation generation it belongs to
	uint16_t                hw_asid;
	uint8_t                 sw_asid;
	uint64_t                asid_gen;

#if MACH_ASSERT
	int                     pmap_pid;
	char                    pmap_procname[17];
#endif /* MACH_ASSERT */

	bool                    pmap_vm_map_cs_enforced;

#if DEVELOPMENT || DEBUG
	bool                    footprint_suspended;
	bool                    footprint_was_suspended;
#endif /* DEVELOPMENT || DEBUG */

	bool                    nx_enabled;
	bool                    is_64bit;

	enum : uint8_t {
		NESTED_NO_BOUNDS_REF_NONE = 0,
		NESTED_NO_BOUNDS_REF_SUBORD,
		NESTED_NO_BOUNDS_REF_AFTER,
		NESTED_NO_BOUNDS_REF_BEFORE_AND_AFTER,
	} nested_no_bounds_ref_state;

	bool                    nested_bounds_set;

	uint8_t                 type;
	bool                    jit_entitled;
	bool                    tpro;

	uint64_t                associated_vm_map_serial_id;
};

#define PMAP_TYPE_USER 0 /* ordinary pmap */
#define PMAP_TYPE_KERNEL 1 /* kernel pmap */
#define PMAP_TYPE_COMMPAGE 2 /* commpage pmap */
#define PMAP_TYPE_NESTED 3 /* pmap nested within another pmap */

#define PMAP_VASID(pmap) (((uint32_t)((pmap)->sw_asid) << 16) | pmap->hw_asid)

extern int copysafe(vm_map_address_t from, vm_map_address_t to, uint32_t cnt, int type, uint32_t *bytes_copied);

extern tt_entry_t *cpu_tte;   /* the kernel root table, shared by every hart */
extern pmap_paddr_t cpu_ttep; /* its physical address */

extern tt_entry_t *invalid_tte; /* root table with only the kernel half, for harts with no user pmap */
extern pmap_paddr_t invalid_ttep;

#define PMAP_CONTEXT(pmap, thread)

extern void pmap_clear_user_ttb(void);
extern void pmap_bootstrap(vm_offset_t);
extern vm_map_address_t pmap_ptov(pmap_t, ppnum_t);
extern pmap_paddr_t pmap_find_pa(pmap_t map, addr64_t va);
extern pmap_paddr_t pmap_find_pa_nofault(pmap_t map, addr64_t va);
extern ppnum_t pmap_find_phys(pmap_t map, addr64_t va);
extern ppnum_t pmap_find_phys_nofault(pmap_t map, addr64_t va);
extern void pmap_switch_user(thread_t th, vm_map_t map);
extern void pmap_set_pmap(pmap_t pmap, thread_t thread);
extern void pmap_gc(void);

#define PMAP_SWITCH_USER(th, new_map, my_cpu) pmap_switch_user((th), (new_map))

#define pmap_kernel() (kernel_pmap)

#define pmap_kernel_va(VA) \
	(((VA) >= VM_MIN_KERNEL_ADDRESS) && ((VA) <= VM_MAX_KERNEL_ADDRESS))

#define copyinmsg(from, to, cnt) copyin(from, to, cnt)
#define copyoutmsg(from, to, cnt) copyout(from, to, cnt)

#define MACRO_NOOP
#define pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr) MACRO_NOOP
#define pmap_pageable(pmap, start, end, pageable) MACRO_NOOP

extern pmap_paddr_t kvtophys(vm_offset_t va);
extern pmap_paddr_t kvtophys_nofail(vm_offset_t va);
extern vm_map_address_t phystokv(pmap_paddr_t pa);
extern vm_map_address_t phystokv_range(pmap_paddr_t pa, vm_size_t *max_len);

extern vm_map_address_t pmap_map(vm_map_address_t va, vm_offset_t sa, vm_offset_t ea, vm_prot_t prot, unsigned int flags);
extern kern_return_t pmap_map_block(pmap_t pmap, addr64_t va, ppnum_t pa, uint32_t size, vm_prot_t prot, int attr, unsigned int flags);
extern kern_return_t pmap_map_block_addr(pmap_t pmap, addr64_t va, pmap_paddr_t pa, uint32_t size, vm_prot_t prot, int attr, unsigned int flags);
extern void pmap_map_globals(void);

#define PMAP_MAP_BD_DEVICE                    0x0
#define PMAP_MAP_BD_WCOMB                     0x1
#define PMAP_MAP_BD_POSTED                    0x2
#define PMAP_MAP_BD_POSTED_REORDERED          0x3
#define PMAP_MAP_BD_POSTED_COMBINED_REORDERED 0x4
#define PMAP_MAP_BD_MASK                      0x7

extern vm_map_address_t pmap_map_bd_with_options(vm_map_address_t va, vm_offset_t sa, vm_offset_t ea, vm_prot_t prot, int32_t options);
extern vm_map_address_t pmap_map_bd(vm_map_address_t va, vm_offset_t sa, vm_offset_t ea, vm_prot_t prot);

extern boolean_t pmap_valid_address(pmap_paddr_t addr);
extern void pmap_disable_NX(pmap_t pmap);
extern void pmap_set_nested(pmap_t pmap);
extern void pmap_create_commpages(vm_map_address_t *kernel_data_addr, vm_map_address_t *kernel_text_addr,
    vm_map_address_t *kernel_ro_data_addr, vm_map_address_t *user_text_addr);
extern void pmap_insert_commpage(pmap_t pmap);

extern vm_offset_t pmap_cpu_windows_copy_addr(int cpu_num, unsigned int index);
extern unsigned int pmap_map_cpu_windows_copy(ppnum_t pn, vm_prot_t prot, unsigned int wimg_bits);
extern void pmap_unmap_cpu_windows_copy(unsigned int index);

// read only zone elements need no extra alignment, sv39 protects at page granularity
static inline vm_offset_t
pmap_ro_zone_align(vm_offset_t value)
{
	return value;
}

extern void pmap_ro_zone_memcpy(zone_id_t zid, vm_offset_t va, vm_offset_t offset,
    vm_offset_t new_data, vm_size_t new_data_size);
extern uint64_t pmap_ro_zone_atomic_op(zone_id_t zid, vm_offset_t va, vm_offset_t offset,
    uint32_t op, uint64_t value);
extern void pmap_ro_zone_bzero(zone_id_t zid, vm_offset_t va, vm_offset_t offset, vm_size_t size);

extern boolean_t pmap_valid_page(ppnum_t pn);
extern boolean_t pmap_bootloader_page(ppnum_t pn);

extern boolean_t pmap_is_empty(pmap_t pmap, vm_map_offset_t start, vm_map_offset_t end);

#if DEVELOPMENT || DEBUG
extern kern_return_t pmap_test_text_corruption(pmap_paddr_t);
#endif /* DEVELOPMENT || DEBUG */

#define ARM_PMAP_MAX_OFFSET_DEFAULT 0x01
#define ARM_PMAP_MAX_OFFSET_MIN     0x02
#define ARM_PMAP_MAX_OFFSET_MAX     0x04
#define ARM_PMAP_MAX_OFFSET_DEVICE  0x08
#define ARM_PMAP_MAX_OFFSET_JUMBO   0x10

extern vm_map_offset_t pmap_max_offset(boolean_t is64, unsigned int option);
extern vm_map_offset_t pmap_max_64bit_offset(unsigned int option);
extern vm_map_offset_t pmap_max_32bit_offset(unsigned int option);

#define PMAP_INVALID_CPU_NUM (~0U)

extern void pmap_cpu_data_init(void);
extern pmap_cpu_data_t *pmap_get_cpu_data(void);
extern pmap_cpu_data_t *pmap_get_remote_cpu_data(unsigned int cpu);

#endif /* #ifndef ASSEMBLER */

#endif /* #ifndef _RISCV_PMAP_H_ */
