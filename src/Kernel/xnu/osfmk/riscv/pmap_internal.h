#ifndef _RISCV_PMAP_INTERNAL_H_
#define _RISCV_PMAP_INTERNAL_H_

// private to the pmap and riscv_vm_init

#include <riscv/pmap.h>
#include <riscv/proc_reg.h>
#include <kern/locks.h>

// the kernel half of sv39, fixed so the level 1 entries never change after boot
// everything sits above VM_MIN_KERNEL_AND_KEXT_ADDRESS so vm_page and zone packing can reach it
#define PHYSMAP_BASE            0xffffffe000000000ULL   /* all of dram, linear */
#define PHYSMAP_MAX_SIZE        (32ULL << 30)
#define KERNEL_DYN_BASE         0xffffffe800000000ULL   /* kernel_map */
#define KERNEL_DYN_END          0xffffffff00000000ULL
#define KERNEL_WINDOW_BASE      0xffffffff80000000ULL   /* the kernel collection, top 2GB */

// the low globals alias, in the 2MB the cpu windows' level 3 table already covers
#define LOWGLOBAL_ALIAS         0xffffffffffff0000ULL

#define RISCV_L1_INDEX_KERNEL_FIRST     256     /* first level 1 slot of the kernel half */

// how memory types are encoded in leaf ptes on this hart
typedef enum {
	RISCV_MEMTYPE_NONE = 0,         /* platform pma attributes only */
	RISCV_MEMTYPE_SVPBMT,           /* standard pbmt field */
	RISCV_MEMTYPE_THEAD,            /* c906 extended attributes */
} riscv_memtype_mode_t;

extern riscv_memtype_mode_t riscv_memtype_mode;

// an invalid pte can carry a compressed marker for page accounting
#define PTE_COMPRESSED          (1ULL << 62)
#define PTE_COMPRESSED_ALT      (1ULL << 61)
#define PTE_COMPRESSED_MASK     (PTE_COMPRESSED | PTE_COMPRESSED_ALT)

static inline bool
pte_is_valid(pt_entry_t pte)
{
	return (pte & PTE_V) != 0;
}

static inline bool
pte_is_leaf(pt_entry_t pte)
{
	return (pte & (PTE_V | PTE_LEAF_MASK)) > PTE_V;
}

static inline bool
pte_is_table(pt_entry_t pte)
{
	return (pte & (PTE_V | PTE_LEAF_MASK)) == PTE_V;
}

static inline bool
pte_is_compressed(pt_entry_t pte)
{
	return !pte_is_valid(pte) && (pte & PTE_COMPRESSED) != 0;
}

// managed physical memory, every page here has a pv head and attributes
extern pmap_paddr_t vm_first_phys;
extern pmap_paddr_t vm_last_phys;

static inline bool
pa_valid(pmap_paddr_t pa)
{
	return pa >= vm_first_phys && pa < vm_last_phys;
}

static inline unsigned int
pa_index(pmap_paddr_t pa)
{
	return (unsigned int)atop(pa - vm_first_phys);
}

// page attributes, one halfword per managed page
#define PP_ATTR_WIMG_MASK       0x00FF
#define PP_ATTR_REFERENCED      0x0100
#define PP_ATTR_MODIFIED        0x0200
#define PP_ATTR_REUSABLE        0x0400
#define PP_ATTR_NOENCRYPT       0x0800
#define PP_ATTR_REFFAULT        0x1000
#define PP_ATTR_MODFAULT        0x2000

// a page table page's owner, the va it covers and how many entries are live
typedef struct pt_desc {
	pmap_t          pmap;
	vm_offset_t     va;
	uint16_t        refcnt;
	uint16_t        wiredcnt;
	uint8_t         level;
} pt_desc_t;

// one mapping of a physical page, the low bits of ptep say how it is accounted
typedef struct pv_entry {
	struct pv_entry *next;
	uintptr_t       ptep_flags;
} pv_entry_t;

#define PVE_INTERNAL            0x1
#define PVE_ALTACCT             0x2
#define PVE_FLAGS_MASK          0x7

static inline pt_entry_t *
pve_ptep(const pv_entry_t *pve)
{
	return (pt_entry_t *)(pve->ptep_flags & ~(uintptr_t)PVE_FLAGS_MASK);
}

// a pv head is either empty, a mapping list or, for page table pages, their descriptor
#define PVH_TYPE_NULL           0x0
#define PVH_TYPE_PVEP           0x1
#define PVH_TYPE_PTDP           0x2
#define PVH_TYPE_MASK           0x3

extern void riscv_vm_map_static(vm_offset_t va, pmap_paddr_t pa, vm_size_t size, pt_entry_t prot_bits);
extern vm_offset_t riscv_vm_alloc_ptpage(void);
extern pt_entry_t pmap_wimg_to_pte(unsigned int wimg);
extern void pmap_early_init(pmap_paddr_t first_free);
extern void pmap_set_physmap_active(void);
extern kern_return_t pmap_expand_kernel(vm_map_address_t va);

extern pmap_paddr_t avail_start;
extern pmap_paddr_t avail_end;
extern pmap_paddr_t first_avail_phys;

#endif /* _RISCV_PMAP_INTERNAL_H_ */
