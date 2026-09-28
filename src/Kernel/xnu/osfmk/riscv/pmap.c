// sv39 pmap for riscv64, the kernel half is shared by copying its 256 level 1 entries into every
// user root, so those entries are all created at boot and never change afterwards

#include <mach_assert.h>
#include <mach_kdp.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine/vm_types.h>
#include <kern/kern_types.h>
#include <kern/ledger.h>
#include <kern/locks.h>
#include <kern/misc_protos.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/zalloc.h>
#include <kern/zalloc_internal.h>
#include <kern/cpu_number.h>
#include <vm/pmap.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_page_internal.h>
#include <vm/vm_object_internal.h>
#include <vm/vm_protos.h>
#include <machine/commpage.h>
#include <riscv/cpu_capabilities.h>
#include <riscv/pmap_internal.h>
#include <riscv/proc_reg.h>
#include <riscv/sbi.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/cpu_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/caches_internal.h>
#include <riscv/trap_internal.h>
#include <riscv/lowglobals.h>
#include <sys/kdebug.h>

// --- global state

struct pmap     kernel_pmap_store;
const pmap_t    kernel_pmap = &kernel_pmap_store;

SECURITY_READ_ONLY_LATE(tt_entry_t *) cpu_tte;
SECURITY_READ_ONLY_LATE(pmap_paddr_t) cpu_ttep;
SECURITY_READ_ONLY_LATE(tt_entry_t *) invalid_tte;
SECURITY_READ_ONLY_LATE(pmap_paddr_t) invalid_ttep;

int nx_enabled = 1;
int allow_data_exec  = 0;
int allow_stack_exec = 0;

SECURITY_READ_ONLY_LATE(pmap_paddr_t) vm_first_phys;
SECURITY_READ_ONLY_LATE(pmap_paddr_t) vm_last_phys;
SECURITY_READ_ONLY_LATE(riscv_memtype_mode_t) riscv_memtype_mode;

// managed pages still free for the vm to take during bootstrap
pmap_paddr_t    avail_start;
pmap_paddr_t    avail_end;
pmap_paddr_t    first_avail_phys;
static unsigned int avail_remaining;

// vm_resident's, io_map hands out va from here before kernel_map exists
extern vm_offset_t virtual_space_start;
extern vm_offset_t virtual_space_end;

static SECURITY_READ_ONLY_LATE(uintptr_t *) pv_head_table;
static SECURITY_READ_ONLY_LATE(uint16_t *) pp_attr_table;
static SECURITY_READ_ONLY_LATE(unsigned int) pmap_npages;

static boolean_t pmap_initialized = FALSE;

SECURITY_READ_ONLY_LATE(zone_t) pmap_zone;

struct vm_object pmap_object_store VM_PAGE_PACKED_ALIGNED;
SECURITY_READ_ONLY_LATE(vm_object_t) pmap_object = &pmap_object_store;

static SECURITY_READ_ONLY_LATE(pmap_t) commpage_pmap;

LCK_GRP_DECLARE(pmap_lck_grp, "pmap");

#if DEVELOPMENT || DEBUG
// level 1 lifecycle, 2 mappings, 3 attributes and fast faults
SECURITY_READ_ONLY_LATE(unsigned int) pmap_trace_mask = 0;

#define PMAP_TRACE(level, ...) \
	if (__improbable((1 << (level)) & pmap_trace_mask)) { \
	        KDBG_RELEASE(__VA_ARGS__); \
	}
#else /* DEVELOPMENT || DEBUG */
#define PMAP_TRACE(level, ...)
#endif /* DEVELOPMENT || DEBUG */

// riscv_vm_init turns this on once the physical aperture is live
static bool pmap_physmap_active = false;

static inline bool
physmap_ready(void)
{
	return pmap_physmap_active;
}

void
pmap_set_physmap_active(void)
{
	pmap_physmap_active = true;
}

// all user pmaps, for pmap_dump and debugging
static queue_head_t map_pmap_list;
static hw_lock_data_t pmaps_lock;

static unsigned int inuse_pmap_pages_count;

// pv heads are protected by a hashed set of spinlocks, taken after the pmap lock
#define PV_LOCK_COUNT   512
static hw_lock_data_t pv_locks[PV_LOCK_COUNT];

static inline void
pvh_lock(unsigned int pai)
{
	hw_lock_lock(&pv_locks[pai % PV_LOCK_COUNT], &pmap_lck_grp);
}

static inline void
pvh_unlock(unsigned int pai)
{
	hw_lock_unlock(&pv_locks[pai % PV_LOCK_COUNT]);
}

static inline uintptr_t *
pai_to_pvh(unsigned int pai)
{
	return &pv_head_table[pai];
}

static inline unsigned int
pvh_type(uintptr_t pvh)
{
	return (unsigned int)(pvh & PVH_TYPE_MASK);
}

static inline pv_entry_t *
pvh_list(uintptr_t pvh)
{
	return (pvh_type(pvh) == PVH_TYPE_PVEP) ? (pv_entry_t *)(pvh & ~(uintptr_t)PVH_TYPE_MASK) : NULL;
}

static inline pt_desc_t *
pvh_ptd(uintptr_t pvh)
{
	return (pvh_type(pvh) == PVH_TYPE_PTDP) ? (pt_desc_t *)(pvh & ~(uintptr_t)PVH_TYPE_MASK) : NULL;
}

static inline void
pvh_set_list(unsigned int pai, pv_entry_t *head)
{
	pv_head_table[pai] = head ? ((uintptr_t)head | PVH_TYPE_PVEP) : PVH_TYPE_NULL;
}

static inline void
ppattr_set_bits(unsigned int pai, uint16_t bits)
{
	os_atomic_or(&pp_attr_table[pai], bits, relaxed);
}

static inline void
ppattr_clear_bits(unsigned int pai, uint16_t bits)
{
	os_atomic_andnot(&pp_attr_table[pai], bits, relaxed);
}

static inline bool
ppattr_test(unsigned int pai, uint16_t bits)
{
	return (os_atomic_load(&pp_attr_table[pai], relaxed) & bits) != 0;
}

// --- pmap locks, a reader writer lock per pmap

static inline void
pmap_lock_exclusive(pmap_t pmap)
{
	lck_rw_lock_exclusive(&pmap->rwlock);
}

static inline void
pmap_unlock_exclusive(pmap_t pmap)
{
	lck_rw_unlock_exclusive(&pmap->rwlock);
}

static inline void
pmap_lock_shared(pmap_t pmap)
{
	lck_rw_lock_shared(&pmap->rwlock);
}

static inline void
pmap_unlock_shared(pmap_t pmap)
{
	lck_rw_unlock_shared(&pmap->rwlock);
}

// --- memory types

pt_entry_t
pmap_wimg_to_pte(unsigned int wimg)
{
	switch (riscv_memtype_mode) {
	case RISCV_MEMTYPE_SVPBMT:
		switch (wimg & VM_WIMG_MASK) {
		case VM_WIMG_IO:
			return PTE_PBMT_IO;
		case VM_WIMG_WCOMB:
			return PTE_PBMT_NC;
		default:
			return PTE_PBMT_PMA;
		}
	case RISCV_MEMTYPE_THEAD:
		// strong order for devices, bufferable only for write combining, cached and shareable otherwise
		// the posted and real time types alias these two
		switch (wimg & VM_WIMG_MASK) {
		case VM_WIMG_IO:
			return PTE_THEAD_SO | PTE_THEAD_SH;
		case VM_WIMG_WCOMB:
			return PTE_THEAD_B | PTE_THEAD_SH;
		default:
			return PTE_THEAD_C | PTE_THEAD_B | PTE_THEAD_SH;
		}
	default:
		return 0;
	}
}

static inline pt_entry_t
pte_memtype_mask(void)
{
	return PTE_PBMT_MASK | PTE_THEAD_MASK;
}

// --- tlb maintenance

// beyond this many pages a whole address space flush is cheaper
#define PMAP_FLUSH_PAGES_MAX    64

static inline bool
pmap_multi_hart(void)
{
	return real_ncpus > 1;
}

static void
pmap_flush_tlb_region(pmap_t pmap, vm_map_address_t va, vm_size_t size)
{
	if (size == 0) {
		return;
	}
	if (pmap == kernel_pmap) {
		if (pmap_multi_hart()) {
			// the base of -1 covers every hart, this one included
			(void)sbi_remote_sfence_vma(0, SBI_HART_MASK_BASE_ALL, va,
			    (atop(size) > PMAP_FLUSH_PAGES_MAX) ? (unsigned long)-1 : size);
		} else if (atop(size) > PMAP_FLUSH_PAGES_MAX) {
			sfence_vma_all();
		} else {
			for (vm_map_address_t cur = va; cur < va + size; cur += PAGE_SIZE) {
				sfence_vma_va(cur);
			}
		}
		return;
	}

	uint64_t asid = pmap->hw_asid;
	if (pmap_multi_hart()) {
		(void)sbi_remote_sfence_vma_asid(0, SBI_HART_MASK_BASE_ALL, va,
		    (atop(size) > PMAP_FLUSH_PAGES_MAX) ? (unsigned long)-1 : size, asid);
	} else if (atop(size) > PMAP_FLUSH_PAGES_MAX) {
		sfence_vma_asid(asid);
	} else {
		for (vm_map_address_t cur = va; cur < va + size; cur += PAGE_SIZE) {
			sfence_vma_va_asid(cur, asid);
		}
	}
}

static void
pmap_flush_tlb_asid(pmap_t pmap)
{
	if (pmap == kernel_pmap) {
		if (pmap_multi_hart()) {
			(void)sbi_remote_sfence_vma(0, SBI_HART_MASK_BASE_ALL, 0, (unsigned long)-1);
		} else {
			sfence_vma_all();
		}
		return;
	}
	if (pmap_multi_hart()) {
		(void)sbi_remote_sfence_vma_asid(0, SBI_HART_MASK_BASE_ALL, 0, (unsigned long)-1, pmap->hw_asid);
	} else {
		sfence_vma_asid(pmap->hw_asid);
	}
}

// --- asids, allocated per generation, a hart flushes everything when it first sees a new generation

static SECURITY_READ_ONLY_LATE(uint32_t) pmap_asid_count;
static uint64_t pmap_asid_generation = 1;
static uint64_t pmap_asid_bitmap[(1 << 16) / 64];
static uint32_t pmap_asid_hint = 1;
static hw_lock_data_t pmap_asid_lock;

static void
pmap_asid_probe(void)
{
	// satp keeps only the asid bits the hart implements
	uint64_t old = csr_read(satp);
	csr_write(satp, old | SATP_ASID_MASK);
	uint64_t readback = csr_read(satp);
	csr_write(satp, old);
	sfence_vma_all();

	uint32_t bits = (uint32_t)__builtin_popcountll((readback & SATP_ASID_MASK) >> SATP_ASID_SHIFT);
	pmap_asid_count = bits ? (1U << bits) : 1;
	bitmap_set((bitmap_t *)pmap_asid_bitmap, 0);
}

static void
pmap_asid_activate(pmap_t pmap)
{
	pmap_cpu_data_t *cpu = pmap_get_cpu_data();

	if (pmap_asid_count <= 1) {
		// no usable asids, every switch starts from an empty tlb
		pmap->hw_asid = 0;
		sfence_vma_all();
		return;
	}

	hw_lock_lock(&pmap_asid_lock, &pmap_lck_grp);
	if (pmap->asid_gen != pmap_asid_generation) {
		uint32_t asid = 0;
		for (uint32_t i = 0; i < pmap_asid_count - 1; i++) {
			uint32_t candidate = 1 + ((pmap_asid_hint - 1 + i) % (pmap_asid_count - 1));
			if (!bitmap_test((bitmap_t *)pmap_asid_bitmap, candidate)) {
				asid = candidate;
				break;
			}
		}
		if (asid == 0) {
			// out of asids, start a new generation, harts flush before using it
			pmap_asid_generation++;
			bzero(pmap_asid_bitmap, sizeof(pmap_asid_bitmap));
			bitmap_set((bitmap_t *)pmap_asid_bitmap, 0);
			asid = 1;
		}
		bitmap_set((bitmap_t *)pmap_asid_bitmap, asid);
		pmap_asid_hint = asid + 1;
		pmap->hw_asid = (uint16_t)asid;
		pmap->asid_gen = pmap_asid_generation;
	}
	uint64_t gen = pmap_asid_generation;
	hw_lock_unlock(&pmap_asid_lock);

	if (cpu->cpu_asid_gen != gen) {
		cpu->cpu_asid_gen = gen;
		sfence_vma_all();
	}
}

static void
pmap_asid_release(pmap_t pmap)
{
	if (pmap_asid_count <= 1 || pmap->hw_asid == 0) {
		return;
	}
	hw_lock_lock(&pmap_asid_lock, &pmap_lck_grp);
	if (pmap->asid_gen == pmap_asid_generation) {
		bitmap_clear((bitmap_t *)pmap_asid_bitmap, pmap->hw_asid);
	}
	hw_lock_unlock(&pmap_asid_lock);
	pmap->hw_asid = 0;
	pmap->asid_gen = 0;
}

// --- per cpu data

pmap_cpu_data_t *
pmap_get_cpu_data(void)
{
	return &getCpuDatap()->cpu_pmap_cpu_data;
}

pmap_cpu_data_t *
pmap_get_remote_cpu_data(unsigned int cpu)
{
	cpu_data_t *cdp = cpu_datap((int)cpu);
	return cdp ? &cdp->cpu_pmap_cpu_data : NULL;
}

void
pmap_cpu_data_init(void)
{
	pmap_cpu_data_t *pmap_cpu_data = pmap_get_cpu_data();

	pmap_cpu_data->cpu_number = (unsigned int)cpu_number();
	pmap_cpu_data->cpu_user_pmap = PMAP_NULL;
	pmap_cpu_data->cpu_nested_pmap = PMAP_NULL;
	pmap_cpu_data->cpu_asid_gen = 0;
}

// --- page allocation for page tables and pmap metadata

// bootstrap allocations come straight from the free range after the kernel
static pmap_paddr_t
pmap_steal_page(void)
{
	if (avail_start >= avail_end) {
		panic("%s: out of bootstrap memory", __func__);
	}
	pmap_paddr_t pa = avail_start;
	avail_start += PAGE_SIZE;
	avail_remaining--;
	bzero((void *)phystokv(pa), PAGE_SIZE);
	return pa;
}

#define PMAP_PAGES_ALLOCATE_NOWAIT      0x1

static kern_return_t
pmap_pages_alloc_zeroed(pmap_paddr_t *pa, unsigned int options)
{
	if (!pmap_initialized) {
		*pa = pmap_steal_page();
		return KERN_SUCCESS;
	}

	const boolean_t privileged = set_vm_privilege(true);
	vm_page_t mem = vm_page_grab_options((options & PMAP_PAGES_ALLOCATE_NOWAIT) ?
	    VM_PAGE_GRAB_NOPAGEWAIT : VM_PAGE_GRAB_OPTIONS_NONE);
	set_vm_privilege(privileged);

	if (mem == VM_PAGE_NULL) {
		return KERN_RESOURCE_SHORTAGE;
	}

	vm_page_lock_queues();
	vm_page_wire(mem, VM_KERN_MEMORY_PTE, TRUE);
	vm_page_unlock_queues();

	*pa = ptoa(VM_PAGE_GET_PHYS_PAGE(mem));

	vm_object_lock(pmap_object);
	vm_page_insert_wired(mem, pmap_object, (vm_object_offset_t)(*pa - gPhysBase), VM_KERN_MEMORY_PTE);
	vm_object_unlock(pmap_object);

	os_atomic_inc(&inuse_pmap_pages_count, relaxed);
	bzero((void *)phystokv(*pa), PAGE_SIZE);
	return KERN_SUCCESS;
}

static void
pmap_pages_free(pmap_paddr_t pa)
{
	vm_object_lock(pmap_object);
	vm_page_t mem = vm_page_lookup(pmap_object, (vm_object_offset_t)(pa - gPhysBase));
	if (mem == VM_PAGE_NULL) {
		// a bootstrap page, the vm never owned it
		vm_object_unlock(pmap_object);
		return;
	}
	vm_page_lock_queues();
	vm_page_free(mem);
	vm_page_unlock_queues();
	vm_object_unlock(pmap_object);
	os_atomic_dec(&inuse_pmap_pages_count, relaxed);
}

// --- small object pools for pv entries and page table descriptors, carved from pages

typedef struct pmap_pool {
	void            *free;
	unsigned int    count;
	unsigned int    elem_size;
	unsigned int    low_water;
	hw_lock_data_t  lock;
} pmap_pool_t;

static pmap_pool_t pv_pool = { .elem_size = sizeof(pv_entry_t), .low_water = 128 };
static pmap_pool_t ptd_pool = { .elem_size = sizeof(pt_desc_t), .low_water = 16 };

static void *
pmap_pool_get(pmap_pool_t *pool)
{
	hw_lock_lock(&pool->lock, &pmap_lck_grp);
	void *elem = pool->free;
	if (elem != NULL) {
		pool->free = *(void **)elem;
		pool->count--;
	}
	hw_lock_unlock(&pool->lock);
	if (elem != NULL) {
		bzero(elem, pool->elem_size);
	}
	return elem;
}

static void
pmap_pool_put(pmap_pool_t *pool, void *elem)
{
	hw_lock_lock(&pool->lock, &pmap_lck_grp);
	*(void **)elem = pool->free;
	pool->free = elem;
	pool->count++;
	hw_lock_unlock(&pool->lock);
}

// refills a pool with one page worth of elements, called with no pmap locks held
static kern_return_t
pmap_pool_replenish(pmap_pool_t *pool, unsigned int options)
{
	if (os_atomic_load(&pool->count, relaxed) >= pool->low_water) {
		return KERN_SUCCESS;
	}
	pmap_paddr_t pa;
	kern_return_t kr = pmap_pages_alloc_zeroed(&pa, options);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	uint8_t *page = (uint8_t *)phystokv(pa);
	for (unsigned int off = 0; off + pool->elem_size <= PAGE_SIZE; off += pool->elem_size) {
		pmap_pool_put(pool, page + off);
	}
	return KERN_SUCCESS;
}

// --- page table walks

static inline tt_entry_t *
pmap_tt1e(pmap_t pmap, vm_map_address_t va)
{
	return &pmap->tte[RISCV_TT_L1_INDEX(va)];
}

static inline tt_entry_t *
pmap_tt2e(pmap_t pmap, vm_map_address_t va)
{
	tt_entry_t l1 = *pmap_tt1e(pmap, va);
	if (!pte_is_table(l1)) {
		return NULL;
	}
	return &((tt_entry_t *)phystokv(PTE_TO_PA(l1)))[RISCV_TT_L2_INDEX(va)];
}

static inline pt_entry_t *
pmap_pte(pmap_t pmap, vm_map_address_t va)
{
	tt_entry_t *l2p = pmap_tt2e(pmap, va);
	if (l2p == NULL || !pte_is_table(*l2p)) {
		return NULL;
	}
	return &((pt_entry_t *)phystokv(PTE_TO_PA(*l2p)))[RISCV_TT_L3_INDEX(va)];
}

static inline pt_desc_t *
ptd_for_table(pmap_paddr_t table_pa)
{
	if (!pa_valid(table_pa)) {
		return NULL;
	}
	return pvh_ptd(pv_head_table[pa_index(table_pa)]);
}

static inline pt_desc_t *
ptd_for_ptep(pt_entry_t *ptep)
{
	return ptd_for_table(kvtophys_nofail((vm_offset_t)ptep) & ~(pmap_paddr_t)RISCV_PGMASK);
}

// the page table this pmap owns at the given level 1 slot, NULL when it is shared (nested or kernel)
static inline bool
pmap_owns_l2(pmap_t pmap, tt_entry_t l1)
{
	if (!pte_is_table(l1)) {
		return false;
	}
	pt_desc_t *ptd = ptd_for_table(PTE_TO_PA(l1));
	return ptd != NULL && ptd->pmap == pmap;
}

static void
pmap_install_table(pmap_t pmap, tt_entry_t *ttep, pmap_paddr_t table_pa, vm_offset_t va, uint8_t level, pt_desc_t *ptd)
{
	if (ptd != NULL) {
		ptd->pmap = pmap;
		ptd->va = va;
		ptd->level = level;
		ptd->refcnt = 0;
		ptd->wiredcnt = 0;
		pv_head_table[pa_index(table_pa)] = (uintptr_t)ptd | PVH_TYPE_PTDP;
	}
	os_atomic_store(ttep, PA_TO_PTE(table_pa) | PTE_V, release);
}

// makes sure the level 2 and level 3 tables for va exist, returns with no locks held
static kern_return_t
pmap_expand(pmap_t pmap, vm_map_address_t va, unsigned int options)
{
	for (;;) {
		pmap_lock_shared(pmap);
		tt_entry_t *l1p = pmap_tt1e(pmap, va);
		bool need_l2 = !pte_is_valid(*l1p);
		bool need_l3 = false;
		if (!need_l2) {
			if (pte_is_leaf(*l1p)) {
				pmap_unlock_shared(pmap);
				panic("%s: va 0x%llx in pmap %p is inside a gigapage", __func__, (uint64_t)va, pmap);
			}
			tt_entry_t *l2p = pmap_tt2e(pmap, va);
			need_l3 = !pte_is_valid(*l2p);
			if (!need_l3 && pte_is_leaf(*l2p)) {
				pmap_unlock_shared(pmap);
				panic("%s: va 0x%llx in pmap %p is inside a megapage", __func__, (uint64_t)va, pmap);
			}
		}
		pmap_unlock_shared(pmap);

		if (!need_l2 && !need_l3) {
			return KERN_SUCCESS;
		}
		if (pmap == kernel_pmap && need_l2) {
			panic("%s: kernel va 0x%llx outside the preallocated kernel tables", __func__, (uint64_t)va);
		}

		pmap_paddr_t table_pa;
		kern_return_t kr = pmap_pool_replenish(&ptd_pool, (options & PMAP_OPTIONS_NOWAIT) ? PMAP_PAGES_ALLOCATE_NOWAIT : 0);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		kr = pmap_pages_alloc_zeroed(&table_pa, (options & PMAP_OPTIONS_NOWAIT) ? PMAP_PAGES_ALLOCATE_NOWAIT : 0);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		// every table made after pmap_bootstrap gets a descriptor, the boot tables never map managed pages
		pt_desc_t *ptd = pmap_pool_get(&ptd_pool);
		if (ptd == NULL) {
			pmap_pages_free(table_pa);
			continue;
		}

		bool used = false;
		pmap_lock_exclusive(pmap);
		tt_entry_t *l1p2 = pmap_tt1e(pmap, va);
		if (!pte_is_valid(*l1p2)) {
			pmap_install_table(pmap, l1p2, table_pa, va & ~(vm_map_address_t)(RISCV_TT_L1_SIZE - 1), 2, ptd);
			used = true;
		} else {
			tt_entry_t *l2p = pmap_tt2e(pmap, va);
			if (!pte_is_valid(*l2p)) {
				pmap_install_table(pmap, l2p, table_pa, va & ~(vm_map_address_t)(RISCV_TT_L2_SIZE - 1), 3, ptd);
				pt_desc_t *parent = ptd_for_ptep(l2p);
				if (parent != NULL) {
					parent->refcnt++;
				}
				used = true;
			}
		}
		pmap_unlock_exclusive(pmap);

		if (!used) {
			if (ptd != NULL) {
				pmap_pool_put(&ptd_pool, ptd);
			}
			pmap_pages_free(table_pa);
		}
	}
}

// the static mapper's way to grow a kernel table once the pools exist
kern_return_t
pmap_expand_kernel(vm_map_address_t va)
{
	return pmap_expand(kernel_pmap, va, 0);
}

// --- translation

// the kernel window, the physical aperture and any other kernel mapping, superpages included
static pmap_paddr_t
pmap_walk_pa(pmap_t pmap, vm_map_address_t va)
{
	tt_entry_t l1 = *pmap_tt1e(pmap, va);
	if (!pte_is_valid(l1)) {
		return 0;
	}
	if (pte_is_leaf(l1)) {
		return PTE_TO_PA(l1) + (va & (RISCV_TT_L1_SIZE - 1));
	}
	tt_entry_t l2 = ((tt_entry_t *)phystokv(PTE_TO_PA(l1)))[RISCV_TT_L2_INDEX(va)];
	if (!pte_is_valid(l2)) {
		return 0;
	}
	if (pte_is_leaf(l2)) {
		return PTE_TO_PA(l2) + (va & (RISCV_TT_L2_SIZE - 1));
	}
	pt_entry_t l3 = ((pt_entry_t *)phystokv(PTE_TO_PA(l2)))[RISCV_TT_L3_INDEX(va)];
	if (!pte_is_valid(l3)) {
		return 0;
	}
	return PTE_TO_PA(l3) + (va & RISCV_PGMASK);
}

pmap_paddr_t
kvtophys(vm_offset_t va)
{
	if (va >= PHYSMAP_BASE && va < PHYSMAP_BASE + (gDramSize ? gDramSize : PHYSMAP_MAX_SIZE)) {
		return (va - PHYSMAP_BASE) + gDramBase;
	}
	if (!physmap_ready()) {
		// until riscv_vm_init switches tables the loader's linear window is what's live
		return va - gVirtBase + gPhysBase;
	}
	return pmap_walk_pa(kernel_pmap, va);
}

pmap_paddr_t
kvtophys_nofail(vm_offset_t va)
{
	pmap_paddr_t pa = kvtophys(va);
	if (pa == 0) {
		panic("%s: no translation for kernel va 0x%llx", __func__, (uint64_t)va);
	}
	return pa;
}

vm_map_address_t
phystokv(pmap_paddr_t pa)
{
	if (gDramSize != 0 && pa >= gDramBase && pa < gDramBase + gDramSize && physmap_ready()) {
		return (vm_map_address_t)(pa - gDramBase + PHYSMAP_BASE);
	}
	// before the aperture exists the loader's window covers the kernel's own memory
	return (vm_map_address_t)(pa - gPhysBase + gVirtBase);
}

vm_map_address_t
phystokv_range(pmap_paddr_t pa, vm_size_t *max_len)
{
	vm_size_t len = *max_len;
	if (gDramSize != 0 && pa >= gDramBase && pa < gDramBase + gDramSize) {
		vm_size_t remaining = (vm_size_t)(gDramBase + gDramSize - pa);
		if (len > remaining) {
			*max_len = remaining;
		}
	}
	return phystokv(pa);
}

pmap_paddr_t
pmap_find_pa(pmap_t pmap, addr64_t va)
{
	if (pmap == kernel_pmap) {
		return kvtophys((vm_offset_t)va);
	}
	pmap_lock_shared(pmap);
	pmap_paddr_t pa = pmap_walk_pa(pmap, (vm_map_address_t)va);
	pmap_unlock_shared(pmap);
	return pa;
}

pmap_paddr_t
pmap_find_pa_nofault(pmap_t pmap, addr64_t va)
{
	// the walk only touches page tables through the aperture, it can't fault
	if (pmap == kernel_pmap) {
		return kvtophys((vm_offset_t)va);
	}
	return pmap_walk_pa(pmap, (vm_map_address_t)va);
}

ppnum_t
pmap_find_phys(pmap_t pmap, addr64_t va)
{
	return (ppnum_t)atop(pmap_find_pa(pmap, va));
}

ppnum_t
pmap_find_phys_nofault(pmap_t pmap, addr64_t va)
{
	return (ppnum_t)atop(pmap_find_pa_nofault(pmap, va));
}

#if !MACH_KDP
// kdp_core.c has the full version when kdp is built in
ppnum_t
kernel_pmap_present_mapping(uint64_t vaddr, uint64_t *pvincr, uintptr_t *pvphysaddr)
{
	pmap_paddr_t pa = kvtophys((vm_offset_t)vaddr);
	*pvincr = PAGE_SIZE;
	if (pa == 0) {
		return 0;
	}
	if (pvphysaddr != NULL) {
		*pvphysaddr = (uintptr_t)phystokv(pa);
	}
	return (ppnum_t)atop(pa);
}
#endif /* !MACH_KDP */

vm_map_address_t
pmap_ptov(pmap_t pmap, ppnum_t pn)
{
	(void)pmap;
	return phystokv(ptoa(pn));
}

// --- pte construction

static pt_entry_t
pmap_construct_pte(pmap_t pmap, vm_map_address_t va, pmap_paddr_t pa, vm_prot_t prot,
    vm_prot_t fault_type, unsigned int wimg, uint16_t *pp_attr_bits)
{
	(void)va;
	pt_entry_t pte = PA_TO_PTE(pa) | PTE_V;

	if (prot & VM_PROT_WRITE) {
		// write without read is reserved in sv39
		pte |= PTE_R | PTE_W;
	} else if (prot & VM_PROT_READ) {
		pte |= PTE_R;
	}
	if ((prot & VM_PROT_EXECUTE) && !(pmap->nx_enabled && pmap == kernel_pmap)) {
		pte |= PTE_X;
	}

	if (pmap == kernel_pmap) {
		// kernel pages never take accessed or dirty faults
		pte |= PTE_G | PTE_A | PTE_D;
	} else {
		pte |= PTE_U;
		*pp_attr_bits = 0;
		if (fault_type != VM_PROT_NONE) {
			pte |= PTE_A;
			*pp_attr_bits |= PP_ATTR_REFERENCED;
			if ((fault_type & VM_PROT_WRITE) && (prot & VM_PROT_WRITE)) {
				pte |= PTE_D;
				*pp_attr_bits |= PP_ATTR_MODIFIED;
			}
		}
	}

	pte |= pmap_wimg_to_pte(wimg);
	return pte;
}

// --- pv lists

static void
pv_list_insert(unsigned int pai, pv_entry_t *pve, pt_entry_t *ptep, uintptr_t flags)
{
	pve->ptep_flags = (uintptr_t)ptep | flags;
	pve->next = pvh_list(pv_head_table[pai]);
	pvh_set_list(pai, pve);
}

static pv_entry_t *
pv_list_remove(unsigned int pai, pt_entry_t *ptep)
{
	pv_entry_t *prev = NULL;
	for (pv_entry_t *pve = pvh_list(pv_head_table[pai]); pve != NULL; prev = pve, pve = pve->next) {
		if (pve_ptep(pve) == ptep) {
			if (prev == NULL) {
				pvh_set_list(pai, pve->next);
			} else {
				prev->next = pve->next;
			}
			return pve;
		}
	}
	return NULL;
}

// the pmap and va a pte belongs to, from its page table's descriptor
static bool
ptep_owner(pt_entry_t *ptep, pmap_t *pmapp, vm_map_address_t *vap)
{
	pt_desc_t *ptd = ptd_for_ptep(ptep);
	if (ptd == NULL) {
		panic("%s: pte %p of a managed page sits in a table without a descriptor", __func__, ptep);
	}
	if (pmapp) {
		*pmapp = ptd->pmap;
	}
	if (vap) {
		*vap = ptd->va + (((vm_offset_t)ptep & RISCV_PGMASK) / sizeof(pt_entry_t)) * PAGE_SIZE;
	}
	return true;
}

// --- ledgers

static inline void
pmap_ledger_credit(pmap_t pmap, int entry, ledger_amount_t amount)
{
	if (pmap != kernel_pmap && pmap->ledger != NULL) {
		ledger_credit(pmap->ledger, entry, amount);
	}
}

static inline void
pmap_ledger_debit(pmap_t pmap, int entry, ledger_amount_t amount)
{
	if (pmap != kernel_pmap && pmap->ledger != NULL) {
		ledger_debit(pmap->ledger, entry, amount);
	}
}

#if DEVELOPMENT || DEBUG
#define PMAP_FOOTPRINT_SUSPENDED(pmap) ((pmap)->footprint_suspended)
#else
#define PMAP_FOOTPRINT_SUSPENDED(pmap) (FALSE)
#endif

// --- entering mappings

kern_return_t
pmap_enter_options_addr(
	pmap_t pmap,
	vm_map_offset_t v,
	pmap_paddr_t pa,
	vm_prot_t prot,
	vm_prot_t fault_type,
	unsigned int flags,
	boolean_t wired,
	unsigned int options,
	__unused void *arg,
	__unused pmap_mapping_type_t mapping_type)
{
	if (pmap == PMAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	if ((v & PAGE_MASK) || (pa & PAGE_MASK)) {
		panic("%s: unaligned va 0x%llx or pa 0x%llx", __func__, (uint64_t)v, (uint64_t)pa);
	}
	if (v < pmap->min || v >= pmap->max) {
		panic("%s: va 0x%llx outside pmap %p", __func__, (uint64_t)v, pmap);
	}
	if (pmap == kernel_pmap && v >= CPUWINDOWS_BASE && v < CPUWINDOWS_TOP) {
		panic("%s: kernel va 0x%llx is a cpu copy window", __func__, (uint64_t)v);
	}
	if ((prot & VM_PROT_EXECUTE) && pmap == kernel_pmap) {
		panic("%s: executable mapping in the kernel pmap at 0x%llx", __func__, (uint64_t)v);
	}

	PMAP_TRACE(2, PMAP_CODE(PMAP__ENTER) | DBG_FUNC_START,
	    VM_KERNEL_ADDRHIDE(pmap), VM_KERNEL_ADDRHIDE(v), pa, prot);

	kern_return_t kr;
	const unsigned int alloc_options = (options & PMAP_OPTIONS_NOWAIT) ? PMAP_PAGES_ALLOCATE_NOWAIT : 0;

	for (;;) {
		kr = pmap_expand(pmap, v, options);
		if (kr != KERN_SUCCESS) {
			goto out;
		}
		if (options & PMAP_OPTIONS_NOENTER) {
			goto out;
		}
		if (pmap_initialized) {
			kr = pmap_pool_replenish(&pv_pool, alloc_options);
			if (kr != KERN_SUCCESS) {
				goto out;
			}
		}

		pmap_lock_exclusive(pmap);
		pt_entry_t *ptep = pmap_pte(pmap, v);
		if (ptep == NULL) {
			// a racing remove freed the table, expand again
			pmap_unlock_exclusive(pmap);
			continue;
		}

		pt_entry_t spte = *ptep;
		bool had_valid = pte_is_valid(spte);
		bool had_compressed = pte_is_compressed(spte);
		pt_desc_t *ptd = (pmap != kernel_pmap) ? ptd_for_ptep(ptep) : NULL;

		if (had_valid && PTE_TO_PA(spte) != pa) {
			// a different page is mapped here, drop it first
			pmap_unlock_exclusive(pmap);
			pmap_remove_options(pmap, v, v + PAGE_SIZE, PMAP_OPTIONS_REMOVE);
			continue;
		}

		uint16_t pp_attr_bits = 0;
		unsigned int wimg;
		if (flags & (VM_WIMG_MASK | VM_WIMG_USE_DEFAULT)) {
			wimg = flags & VM_WIMG_MASK;
			if (flags & VM_WIMG_USE_DEFAULT) {
				wimg = pa_valid(pa) ? (pp_attr_table[pa_index(pa)] & PP_ATTR_WIMG_MASK) : VM_WIMG_IO;
				if (wimg == 0) {
					wimg = VM_WIMG_DEFAULT;
				}
			}
		} else {
			wimg = pa_valid(pa) ? (pp_attr_table[pa_index(pa)] & PP_ATTR_WIMG_MASK) : VM_WIMG_IO;
			if (wimg == 0) {
				wimg = VM_WIMG_DEFAULT;
			}
		}

		pt_entry_t pte = pmap_construct_pte(pmap, v, pa, prot, fault_type, wimg, &pp_attr_bits);
		if (wired) {
			pte |= PTE_SW_WIRED;
		}

		bool is_internal = false, is_altacct = false, is_reusable = false, is_external = false;

		if (pa_valid(pa)) {
			unsigned int pai = pa_index(pa);
			pvh_lock(pai);
			if (!had_valid) {
				pv_entry_t *pve = pmap_pool_get(&pv_pool);
				if (pve == NULL) {
					pvh_unlock(pai);
					pmap_unlock_exclusive(pmap);
					if (pmap_pool_replenish(&pv_pool, alloc_options) != KERN_SUCCESS) {
						kr = KERN_RESOURCE_SHORTAGE;
						goto out;
					}
					continue;
				}
				uintptr_t pve_flags = 0;
				if (pmap != kernel_pmap) {
					if (options & PMAP_OPTIONS_INTERNAL) {
						pve_flags |= PVE_INTERNAL;
						if ((options & PMAP_OPTIONS_ALT_ACCT) || PMAP_FOOTPRINT_SUSPENDED(pmap)) {
							pve_flags |= PVE_ALTACCT;
							is_altacct = true;
						}
					}
					if (ppattr_test(pai, PP_ATTR_REUSABLE) && !is_altacct) {
						is_reusable = true;
					} else if (options & PMAP_OPTIONS_INTERNAL) {
						is_internal = true;
					} else {
						is_external = true;
					}
				}
				pv_list_insert(pai, pve, ptep, pve_flags);
			} else {
				// a protection or wiring change on the same page keeps the ref and mod state
				pte |= spte & (PTE_A | PTE_D);
			}
			if (pp_attr_bits) {
				ppattr_set_bits(pai, pp_attr_bits);
			}
			os_atomic_store(ptep, pte, release);
			pvh_unlock(pai);
		} else {
			if (prot & VM_PROT_EXECUTE) {
				pmap_unlock_exclusive(pmap);
				kr = KERN_FAILURE;
				goto out;
			}
			os_atomic_store(ptep, pte, release);
		}
		if (!had_valid) {
			// harts may cache invalid entries, this one retries at once instead of taking a spurious fault
			if (pmap == kernel_pmap) {
				sfence_vma_va(v);
			} else {
				sfence_vma_va_asid(v, pmap->hw_asid);
			}
		}

		if (ptd != NULL) {
			if (!had_valid && !had_compressed) {
				ptd->refcnt++;
			}
			if (wired && !(had_valid && (spte & PTE_SW_WIRED))) {
				ptd->wiredcnt++;
				pmap_ledger_credit(pmap, task_ledgers.wired_mem, PAGE_SIZE);
			} else if (!wired && had_valid && (spte & PTE_SW_WIRED)) {
				ptd->wiredcnt--;
				pmap_ledger_debit(pmap, task_ledgers.wired_mem, PAGE_SIZE);
			}
		}

		if (!had_valid && pmap != kernel_pmap && pa_valid(pa)) {
			pmap_ledger_credit(pmap, task_ledgers.phys_mem, PAGE_SIZE);
			if (is_internal) {
				pmap_ledger_credit(pmap, task_ledgers.internal, PAGE_SIZE);
				if (!(had_compressed && !(spte & PTE_COMPRESSED_ALT))) {
					pmap_ledger_credit(pmap, task_ledgers.phys_footprint, PAGE_SIZE);
				}
			}
			if (is_altacct) {
				pmap_ledger_credit(pmap, task_ledgers.internal, PAGE_SIZE);
				pmap_ledger_credit(pmap, task_ledgers.alternate_accounting, PAGE_SIZE);
			}
			if (is_reusable) {
				pmap_ledger_credit(pmap, task_ledgers.reusable, PAGE_SIZE);
			} else if (is_external) {
				pmap_ledger_credit(pmap, task_ledgers.external, PAGE_SIZE);
			}
		}
		if (had_compressed) {
			// the page came back from the compressor
			pmap_ledger_debit(pmap, task_ledgers.internal_compressed, PAGE_SIZE);
			if (spte & PTE_COMPRESSED_ALT) {
				pmap_ledger_debit(pmap, task_ledgers.alternate_accounting_compressed, PAGE_SIZE);
			} else if (!is_internal) {
				pmap_ledger_debit(pmap, task_ledgers.phys_footprint, PAGE_SIZE);
			}
		}

		if (had_valid) {
			pmap_flush_tlb_region(pmap, v, PAGE_SIZE);
		}
		pmap_unlock_exclusive(pmap);
		kr = KERN_SUCCESS;
		break;
	}

out:
	PMAP_TRACE(2, PMAP_CODE(PMAP__ENTER) | DBG_FUNC_END, kr);
	return kr;
}

kern_return_t
pmap_enter_options(
	pmap_t pmap,
	vm_map_offset_t v,
	ppnum_t pn,
	vm_prot_t prot,
	vm_prot_t fault_type,
	unsigned int flags,
	boolean_t wired,
	unsigned int options,
	void *arg,
	pmap_mapping_type_t mapping_type)
{
	return pmap_enter_options_addr(pmap, v, ptoa(pn), prot, fault_type, flags, wired, options, arg, mapping_type);
}

kern_return_t
pmap_enter(
	pmap_t pmap,
	vm_map_offset_t v,
	ppnum_t pn,
	vm_prot_t prot,
	vm_prot_t fault_type,
	unsigned int flags,
	boolean_t wired,
	pmap_mapping_type_t mapping_type)
{
	return pmap_enter_options(pmap, v, pn, prot, fault_type, flags, wired, 0, NULL, mapping_type);
}

// --- removing mappings

// drops a level 3 table (and an emptied level 2 above it) once nothing is left in it
static void
pmap_free_empty_tables(pmap_t pmap, vm_map_address_t va)
{
	if (pmap == kernel_pmap) {
		return;
	}
	tt_entry_t *l1p = pmap_tt1e(pmap, va);
	if (!pmap_owns_l2(pmap, *l1p)) {
		return;
	}
	tt_entry_t *l2p = pmap_tt2e(pmap, va);
	if (!pte_is_table(*l2p)) {
		return;
	}
	pmap_paddr_t l3_pa = PTE_TO_PA(*l2p);
	pt_desc_t *l3_ptd = ptd_for_table(l3_pa);
	if (l3_ptd == NULL || l3_ptd->refcnt != 0) {
		return;
	}
	os_atomic_store(l2p, 0, release);
	pmap_flush_tlb_region(pmap, va & ~(vm_map_address_t)(RISCV_TT_L2_SIZE - 1), RISCV_TT_L2_SIZE);
	pv_head_table[pa_index(l3_pa)] = PVH_TYPE_NULL;
	pmap_pool_put(&ptd_pool, l3_ptd);
	pmap_pages_free(l3_pa);

	pmap_paddr_t l2_pa = PTE_TO_PA(*l1p);
	pt_desc_t *l2_ptd = ptd_for_table(l2_pa);
	if (l2_ptd != NULL && --l2_ptd->refcnt == 0) {
		os_atomic_store(l1p, 0, release);
		pmap_flush_tlb_asid(pmap);
		pv_head_table[pa_index(l2_pa)] = PVH_TYPE_NULL;
		pmap_pool_put(&ptd_pool, l2_ptd);
		pmap_pages_free(l2_pa);
	}
}

// removes the mappings in [va, end) of one level 3 table, the pmap lock is held exclusive
static void
pmap_remove_range(pmap_t pmap, vm_map_address_t va, vm_map_address_t end, pt_entry_t *ptep, int options)
{
	pt_desc_t *ptd = (pmap != kernel_pmap) ? ptd_for_ptep(ptep) : NULL;
	unsigned int num_removed = 0, num_unwired = 0, num_internal = 0, num_alt_internal = 0;
	unsigned int num_external = 0, num_reusable = 0, num_compressed = 0, num_alt_compressed = 0;
	unsigned int num_pte_changed = 0;

	for (vm_map_address_t cur = va; cur < end; cur += PAGE_SIZE, ptep++) {
		pt_entry_t spte = *ptep;

		if (pte_is_compressed(spte)) {
			if (options & PMAP_OPTIONS_REMOVE) {
				os_atomic_store(ptep, 0, relaxed);
				num_compressed++;
				if (spte & PTE_COMPRESSED_ALT) {
					num_alt_compressed++;
				}
				if (ptd != NULL) {
					ptd->refcnt--;
				}
			}
			continue;
		}
		if (!pte_is_valid(spte)) {
			continue;
		}

		pmap_paddr_t pa = PTE_TO_PA(spte);
		if (pa_valid(pa)) {
			unsigned int pai = pa_index(pa);
			pvh_lock(pai);
			// the entry may have changed while the pv lock was taken
			spte = *ptep;
			if (!pte_is_valid(spte) || PTE_TO_PA(spte) != pa) {
				pvh_unlock(pai);
				continue;
			}
			pv_entry_t *pve = pv_list_remove(pai, ptep);
			uintptr_t pve_flags = pve ? (pve->ptep_flags & PVE_FLAGS_MASK) : 0;
			if (spte & PTE_A) {
				ppattr_set_bits(pai, PP_ATTR_REFERENCED);
			}
			if (spte & PTE_D) {
				ppattr_set_bits(pai, PP_ATTR_MODIFIED);
			}
			os_atomic_store(ptep, 0, relaxed);
			if (pmap != kernel_pmap) {
				if (pve_flags & PVE_ALTACCT) {
					num_alt_internal++;
				} else if (pve_flags & PVE_INTERNAL) {
					if (ppattr_test(pai, PP_ATTR_REUSABLE)) {
						num_reusable++;
					} else {
						num_internal++;
					}
				} else {
					num_external++;
				}
			}
			pvh_unlock(pai);
			if (pve != NULL) {
				pmap_pool_put(&pv_pool, pve);
			}
		} else {
			os_atomic_store(ptep, 0, relaxed);
		}

		num_removed++;
		num_pte_changed++;
		if (spte & PTE_SW_WIRED) {
			num_unwired++;
			if (ptd != NULL) {
				ptd->wiredcnt--;
			}
		}
		if (ptd != NULL) {
			ptd->refcnt--;
		}
	}

	if (num_pte_changed) {
		pmap_flush_tlb_region(pmap, va, end - va);
	}

	if (pmap != kernel_pmap) {
		ledger_amount_t unit = PAGE_SIZE;
		pmap_ledger_debit(pmap, task_ledgers.phys_mem, num_removed * unit);
		pmap_ledger_debit(pmap, task_ledgers.external, num_external * unit);
		pmap_ledger_debit(pmap, task_ledgers.reusable, num_reusable * unit);
		pmap_ledger_debit(pmap, task_ledgers.wired_mem, num_unwired * unit);
		pmap_ledger_debit(pmap, task_ledgers.internal, (num_internal + num_alt_internal) * unit);
		pmap_ledger_debit(pmap, task_ledgers.alternate_accounting, num_alt_internal * unit);
		pmap_ledger_debit(pmap, task_ledgers.alternate_accounting_compressed, num_alt_compressed * unit);
		pmap_ledger_debit(pmap, task_ledgers.internal_compressed, num_compressed * unit);
		pmap_ledger_debit(pmap, task_ledgers.phys_footprint,
		    (num_internal + num_compressed - num_alt_compressed) * unit);
	}
}

void
pmap_remove_options(pmap_t pmap, vm_map_offset_t start, vm_map_offset_t end, int options)
{
	if (pmap == PMAP_NULL || start >= end) {
		return;
	}

	PMAP_TRACE(2, PMAP_CODE(PMAP__REMOVE) | DBG_FUNC_START,
	    VM_KERNEL_ADDRHIDE(pmap), VM_KERNEL_ADDRHIDE(start), VM_KERNEL_ADDRHIDE(end));

	pmap_lock_exclusive(pmap);
	vm_map_address_t va = start;
	while (va < end) {
		vm_map_address_t l1_end = (va & ~(vm_map_address_t)(RISCV_TT_L1_SIZE - 1)) + RISCV_TT_L1_SIZE;
		tt_entry_t l1 = *pmap_tt1e(pmap, va);
		if (!pte_is_table(l1) || (pmap != kernel_pmap && !pmap_owns_l2(pmap, l1))) {
			// nothing here, or a nested region the pmap doesn't own
			va = l1_end;
			continue;
		}
		vm_map_address_t l2_end = (va & ~(vm_map_address_t)(RISCV_TT_L2_SIZE - 1)) + RISCV_TT_L2_SIZE;
		if (l2_end > end) {
			l2_end = end;
		}
		pt_entry_t *ptep = pmap_pte(pmap, va);
		if (ptep != NULL) {
			pmap_remove_range(pmap, va, l2_end, ptep, options);
			pmap_free_empty_tables(pmap, va);
		}
		va = l2_end;
	}
	pmap_unlock_exclusive(pmap);

	PMAP_TRACE(2, PMAP_CODE(PMAP__REMOVE) | DBG_FUNC_END);
}

void
pmap_remove(pmap_t pmap, vm_map_offset_t start, vm_map_offset_t end)
{
	pmap_remove_options(pmap, start, end, PMAP_OPTIONS_REMOVE);
}

// --- protection

void
pmap_protect_options(pmap_t pmap, vm_map_offset_t start, vm_map_offset_t end,
    vm_prot_t prot, unsigned int options, __unused void *arg)
{
	if (pmap == PMAP_NULL || start >= end) {
		return;
	}
	if ((prot & VM_PROT_ALL) == VM_PROT_NONE) {
		pmap_remove_options(pmap, start, end, options);
		return;
	}

	pmap_lock_exclusive(pmap);
	vm_map_address_t va = start;
	while (va < end) {
		vm_map_address_t l1_end = (va & ~(vm_map_address_t)(RISCV_TT_L1_SIZE - 1)) + RISCV_TT_L1_SIZE;
		tt_entry_t l1 = *pmap_tt1e(pmap, va);
		if (!pte_is_table(l1) || (pmap != kernel_pmap && !pmap_owns_l2(pmap, l1))) {
			va = l1_end;
			continue;
		}
		vm_map_address_t l2_end = (va & ~(vm_map_address_t)(RISCV_TT_L2_SIZE - 1)) + RISCV_TT_L2_SIZE;
		if (l2_end > end) {
			l2_end = end;
		}
		pt_entry_t *ptep = pmap_pte(pmap, va);
		bool changed = false;
		for (vm_map_address_t cur = va; ptep != NULL && cur < l2_end; cur += PAGE_SIZE, ptep++) {
			pt_entry_t spte = *ptep;
			if (!pte_is_valid(spte)) {
				continue;
			}
			// only ever narrows, pmap_enter is the one place permissions grow
			pt_entry_t npte = spte;
			if (!(prot & VM_PROT_WRITE)) {
				npte &= ~PTE_W;
			}
			if (!(prot & VM_PROT_EXECUTE)) {
				npte &= ~PTE_X;
			}
			if (!(prot & VM_PROT_READ) && (prot & VM_PROT_EXECUTE)) {
				npte &= ~PTE_R;
			}
			if (npte != spte) {
				pmap_paddr_t pa = PTE_TO_PA(spte);
				if (pa_valid(pa)) {
					unsigned int pai = pa_index(pa);
					pvh_lock(pai);
					if (spte & PTE_D) {
						ppattr_set_bits(pai, PP_ATTR_MODIFIED);
					}
					os_atomic_store(ptep, npte, relaxed);
					pvh_unlock(pai);
				} else {
					os_atomic_store(ptep, npte, relaxed);
				}
				changed = true;
			}
		}
		if (changed) {
			pmap_flush_tlb_region(pmap, va, l2_end - va);
		}
		va = l2_end;
	}
	pmap_unlock_exclusive(pmap);
}

void
pmap_protect(pmap_t pmap, vm_map_offset_t start, vm_map_offset_t end, vm_prot_t prot)
{
	pmap_protect_options(pmap, start, end, prot, 0, NULL);
}

// --- physical page operations

// restricts or removes every mapping of a page, the pmap locks are not taken
void
pmap_page_protect_options(ppnum_t pn, vm_prot_t prot, unsigned int options, __unused void *arg)
{
	pmap_paddr_t pa = ptoa(pn);
	if (!pa_valid(pa)) {
		return;
	}
	unsigned int pai = pa_index(pa);
	bool remove = (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) == 0;

	if (prot & VM_PROT_WRITE) {
		// nothing to restrict
		return;
	}

	pvh_lock(pai);
	pv_entry_t *pve = pvh_list(pv_head_table[pai]);
	pv_entry_t *remaining = NULL;
	while (pve != NULL) {
		pv_entry_t *next = pve->next;
		pt_entry_t *ptep = pve_ptep(pve);
		pmap_t pmap;
		vm_map_address_t va;
		ptep_owner(ptep, &pmap, &va);
		pt_entry_t spte = *ptep;

		if (spte & PTE_A) {
			ppattr_set_bits(pai, PP_ATTR_REFERENCED);
		}
		if (spte & PTE_D) {
			ppattr_set_bits(pai, PP_ATTR_MODIFIED);
		}

		if (remove) {
			bool internal = (pve->ptep_flags & PVE_INTERNAL) != 0;
			bool altacct = (pve->ptep_flags & PVE_ALTACCT) != 0;
			pt_desc_t *ptd = (pmap != kernel_pmap) ? ptd_for_ptep(ptep) : NULL;
			pt_entry_t npte = 0;

			// a page going to the compressor leaves a marker behind for accounting
			if ((options & PMAP_OPTIONS_COMPRESSOR) && pmap != kernel_pmap && internal) {
				npte = PTE_COMPRESSED | (altacct ? PTE_COMPRESSED_ALT : 0);
			}
			os_atomic_store(ptep, npte, relaxed);
			pmap_flush_tlb_region(pmap, va, PAGE_SIZE);

			if (pmap != kernel_pmap) {
				pmap_ledger_debit(pmap, task_ledgers.phys_mem, PAGE_SIZE);
				if (spte & PTE_SW_WIRED) {
					pmap_ledger_debit(pmap, task_ledgers.wired_mem, PAGE_SIZE);
					if (ptd) {
						ptd->wiredcnt--;
					}
				}
				if (altacct) {
					pmap_ledger_debit(pmap, task_ledgers.internal, PAGE_SIZE);
					pmap_ledger_debit(pmap, task_ledgers.alternate_accounting, PAGE_SIZE);
					if (npte != 0) {
						pmap_ledger_credit(pmap, task_ledgers.internal_compressed, PAGE_SIZE);
						pmap_ledger_credit(pmap, task_ledgers.alternate_accounting_compressed, PAGE_SIZE);
					}
				} else if (internal) {
					if (ppattr_test(pai, PP_ATTR_REUSABLE)) {
						pmap_ledger_debit(pmap, task_ledgers.reusable, PAGE_SIZE);
						if (npte != 0) {
							pmap_ledger_credit(pmap, task_ledgers.internal_compressed, PAGE_SIZE);
							pmap_ledger_credit(pmap, task_ledgers.phys_footprint, PAGE_SIZE);
						}
					} else {
						pmap_ledger_debit(pmap, task_ledgers.internal, PAGE_SIZE);
						if (npte != 0) {
							pmap_ledger_credit(pmap, task_ledgers.internal_compressed, PAGE_SIZE);
						} else {
							pmap_ledger_debit(pmap, task_ledgers.phys_footprint, PAGE_SIZE);
						}
					}
				} else {
					pmap_ledger_debit(pmap, task_ledgers.external, PAGE_SIZE);
				}
				if (ptd != NULL && npte == 0) {
					ptd->refcnt--;
				}
			}
			pmap_pool_put(&pv_pool, pve);
		} else {
			pt_entry_t npte = spte & ~PTE_W;
			if (!(prot & VM_PROT_EXECUTE)) {
				npte &= ~PTE_X;
			}
			if (npte != spte) {
				os_atomic_store(ptep, npte, relaxed);
				pmap_flush_tlb_region(pmap, va, PAGE_SIZE);
			}
			pve->next = remaining;
			remaining = pve;
		}
		pve = next;
	}
	pvh_set_list(pai, remaining);
	pvh_unlock(pai);
}

void
pmap_page_protect(ppnum_t pn, vm_prot_t prot)
{
	pmap_page_protect_options(pn, prot, 0, NULL);
}

unsigned int
pmap_disconnect_options(ppnum_t pn, unsigned int options, void *arg)
{
	pmap_page_protect_options(pn, VM_PROT_NONE, options, arg);
	return pmap_get_refmod(pn);
}

unsigned int
pmap_disconnect(ppnum_t pn)
{
	return pmap_disconnect_options(pn, 0, NULL);
}

void
pmap_change_wiring(pmap_t pmap, vm_map_offset_t va, boolean_t wired)
{
	pmap_lock_exclusive(pmap);
	pt_entry_t *ptep = pmap_pte(pmap, va);
	if (ptep != NULL && pte_is_valid(*ptep)) {
		pt_entry_t spte = *ptep;
		bool was_wired = (spte & PTE_SW_WIRED) != 0;
		if (was_wired != (bool)wired) {
			os_atomic_store(ptep, wired ? (spte | PTE_SW_WIRED) : (spte & ~PTE_SW_WIRED), relaxed);
			pt_desc_t *ptd = (pmap != kernel_pmap) ? ptd_for_ptep(ptep) : NULL;
			if (ptd != NULL) {
				if (wired) {
					ptd->wiredcnt++;
					pmap_ledger_credit(pmap, task_ledgers.wired_mem, PAGE_SIZE);
				} else {
					ptd->wiredcnt--;
					pmap_ledger_debit(pmap, task_ledgers.wired_mem, PAGE_SIZE);
				}
			}
		}
	}
	pmap_unlock_exclusive(pmap);
}

// --- reference and modify state

// folds the hardware accessed and dirty bits of every mapping into the page attributes
static void
pmap_sync_refmod(unsigned int pai)
{
	for (pv_entry_t *pve = pvh_list(pv_head_table[pai]); pve != NULL; pve = pve->next) {
		pt_entry_t spte = *pve_ptep(pve);
		if (spte & PTE_A) {
			ppattr_set_bits(pai, PP_ATTR_REFERENCED);
		}
		if (spte & PTE_D) {
			ppattr_set_bits(pai, PP_ATTR_MODIFIED);
		}
	}
}

static uint16_t
phys_attribute_get(ppnum_t pn, uint16_t bits)
{
	pmap_paddr_t pa = ptoa(pn);
	if (!pa_valid(pa)) {
		return 0;
	}
	unsigned int pai = pa_index(pa);
	pvh_lock(pai);
	pmap_sync_refmod(pai);
	uint16_t attr = pp_attr_table[pai] & bits;
	pvh_unlock(pai);
	return attr;
}

// clears the bits in every mapping too, so the next access faults or is recorded by the hart again
static void
phys_attribute_clear(ppnum_t pn, uint16_t bits, unsigned int options)
{
	pmap_paddr_t pa = ptoa(pn);
	if (!pa_valid(pa)) {
		return;
	}
	unsigned int pai = pa_index(pa);
	pt_entry_t pte_bits = ((bits & PP_ATTR_REFERENCED) ? PTE_A : 0) | ((bits & PP_ATTR_MODIFIED) ? PTE_D : 0);

	pvh_lock(pai);
	for (pv_entry_t *pve = pvh_list(pv_head_table[pai]); pve != NULL; pve = pve->next) {
		pt_entry_t *ptep = pve_ptep(pve);
		pmap_t pmap;
		vm_map_address_t va;
		ptep_owner(ptep, &pmap, &va);
		if (pmap == kernel_pmap) {
			// kernel mappings keep a and d set, their state lives in the attributes only
			continue;
		}
		pt_entry_t spte = *ptep;
		if (spte & pte_bits) {
			os_atomic_andnot(ptep, pte_bits, relaxed);
			if (!(options & PMAP_OPTIONS_NOFLUSH)) {
				pmap_flush_tlb_region(pmap, va, PAGE_SIZE);
			}
		}
	}
	ppattr_clear_bits(pai, bits);
	pvh_unlock(pai);
}

boolean_t
pmap_is_referenced(ppnum_t pn)
{
	return phys_attribute_get(pn, PP_ATTR_REFERENCED) != 0;
}

boolean_t
pmap_is_modified(ppnum_t pn)
{
	return phys_attribute_get(pn, PP_ATTR_MODIFIED) != 0;
}

void
pmap_clear_reference(ppnum_t pn)
{
	phys_attribute_clear(pn, PP_ATTR_REFERENCED, 0);
}

void
pmap_clear_modify(ppnum_t pn)
{
	phys_attribute_clear(pn, PP_ATTR_MODIFIED, 0);
}

void
pmap_set_modify(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	if (pa_valid(pa)) {
		ppattr_set_bits(pa_index(pa), PP_ATTR_MODIFIED);
	}
}

void
pmap_set_reference(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	if (pa_valid(pa)) {
		ppattr_set_bits(pa_index(pa), PP_ATTR_REFERENCED);
	}
}

unsigned int
pmap_get_refmod(ppnum_t pn)
{
	uint16_t attr = phys_attribute_get(pn, PP_ATTR_REFERENCED | PP_ATTR_MODIFIED);
	return ((attr & PP_ATTR_REFERENCED) ? VM_MEM_REFERENCED : 0) |
	       ((attr & PP_ATTR_MODIFIED) ? VM_MEM_MODIFIED : 0);
}

void
pmap_clear_refmod_options(ppnum_t pn, unsigned int mask, unsigned int options, __unused void *arg)
{
	uint16_t bits = ((mask & VM_MEM_REFERENCED) ? PP_ATTR_REFERENCED : 0) |
	    ((mask & VM_MEM_MODIFIED) ? PP_ATTR_MODIFIED : 0);

	pmap_paddr_t pa = ptoa(pn);
	if (pa_valid(pa) && (options & (PMAP_OPTIONS_SET_REUSABLE | PMAP_OPTIONS_CLEAR_REUSABLE))) {
		unsigned int pai = pa_index(pa);
		pvh_lock(pai);
		bool set = (options & PMAP_OPTIONS_SET_REUSABLE) != 0;
		bool was = ppattr_test(pai, PP_ATTR_REUSABLE);
		if (set != was) {
			// reusable internal pages move between the internal and reusable ledgers
			for (pv_entry_t *pve = pvh_list(pv_head_table[pai]); pve != NULL; pve = pve->next) {
				if (!(pve->ptep_flags & PVE_INTERNAL) || (pve->ptep_flags & PVE_ALTACCT)) {
					continue;
				}
				pmap_t pmap;
				ptep_owner(pve_ptep(pve), &pmap, NULL);
				if (set) {
					pmap_ledger_credit(pmap, task_ledgers.reusable, PAGE_SIZE);
					pmap_ledger_debit(pmap, task_ledgers.internal, PAGE_SIZE);
					pmap_ledger_debit(pmap, task_ledgers.phys_footprint, PAGE_SIZE);
				} else {
					pmap_ledger_debit(pmap, task_ledgers.reusable, PAGE_SIZE);
					pmap_ledger_credit(pmap, task_ledgers.internal, PAGE_SIZE);
					pmap_ledger_credit(pmap, task_ledgers.phys_footprint, PAGE_SIZE);
				}
			}
			if (set) {
				ppattr_set_bits(pai, PP_ATTR_REUSABLE);
			} else {
				ppattr_clear_bits(pai, PP_ATTR_REUSABLE);
			}
		}
		pvh_unlock(pai);
	}
	if (bits) {
		phys_attribute_clear(pn, bits, options);
	}
}

void
pmap_clear_refmod(ppnum_t pn, unsigned int mask)
{
	pmap_clear_refmod_options(pn, mask, 0, NULL);
}

bool
pmap_clear_refmod_range_options(pmap_t pmap, vm_map_address_t start, vm_map_address_t end,
    unsigned int mask, unsigned int options)
{
	// done page by page through the physical pages the range maps
	for (vm_map_address_t va = start; va < end; va += PAGE_SIZE) {
		ppnum_t pn = pmap_find_phys(pmap, va);
		if (pn != 0) {
			pmap_clear_refmod_options(pn, mask, options, NULL);
		}
	}
	return true;
}

void
pmap_flush_context_init(pmap_flush_context *pfc)
{
	pfc->pfc_cpus = 0;
	pfc->pfc_invalid_global = 0;
}

void
pmap_flush(pmap_flush_context *pfc)
{
	// deferred flushes aren't batched here, every change was flushed as it was made
	(void)pfc;
}

void
mapping_set_mod(ppnum_t pn)
{
	pmap_set_modify(pn);
}

void
mapping_set_ref(ppnum_t pn)
{
	pmap_set_reference(pn);
}

// resolves faults on harts that don't update the accessed and dirty bits themselves
kern_return_t
riscv_fast_fault(pmap_t pmap, vm_map_address_t va, vm_prot_t fault_type,
    __unused bool was_af_fault, __unused bool from_user)
{
	if (pmap == PMAP_NULL || va < pmap->min || va >= pmap->max) {
		return KERN_FAILURE;
	}

	if (!ml_get_interrupts_enabled() || get_preemption_level() != 0) {
		if (!lck_rw_try_lock_shared(&pmap->rwlock)) {
			return KERN_FAILURE;
		}
	} else {
		pmap_lock_shared(pmap);
	}

	kern_return_t kr = KERN_FAILURE;
	pt_entry_t *ptep = pmap_pte(pmap, va);
	if (ptep != NULL) {
		pt_entry_t spte = *ptep;
		pmap_paddr_t pa = PTE_TO_PA(spte);
		if (pte_is_valid(spte)) {
			bool allowed = true;
			pt_entry_t set = PTE_A;
			if (fault_type & VM_PROT_WRITE) {
				allowed = (spte & PTE_W) != 0;
				set |= PTE_D;
			} else if (fault_type & VM_PROT_EXECUTE) {
				allowed = (spte & PTE_X) != 0;
			} else {
				allowed = (spte & (PTE_R | PTE_X)) != 0;
			}
			if (allowed && (spte & set) != set && pa_valid(pa)) {
				unsigned int pai = pa_index(pa);
				pvh_lock(pai);
				if (*ptep == spte) {
					os_atomic_or(ptep, set, relaxed);
					ppattr_set_bits(pai, PP_ATTR_REFERENCED | ((set & PTE_D) ? PP_ATTR_MODIFIED : 0));
					if (pmap == kernel_pmap) {
						sfence_vma_va(va);
					} else {
						sfence_vma_va_asid(va, pmap->hw_asid);
					}
					kr = KERN_SUCCESS;
				}
				pvh_unlock(pai);
			} else if (allowed && (spte & set) == set) {
				// another hart already fixed it or the mapping is new, the stale tlb entry was the fault
				if (pmap == kernel_pmap) {
					sfence_vma_va(va);
				} else {
					sfence_vma_va_asid(va, pmap->hw_asid);
				}
				kr = KERN_SUCCESS;
			}
		}
	}
	pmap_unlock_shared(pmap);
	return kr;
}

// --- pmap lifecycle

pmap_t
pmap_create_options(ledger_t ledger, vm_map_size_t size, unsigned int flags)
{
	if (size != 0) {
		return PMAP_NULL;
	}
	if (flags & ~(PMAP_CREATE_KNOWN_FLAGS)) {
		return PMAP_NULL;
	}

	pmap_t p = zalloc_flags(pmap_zone, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	pmap_paddr_t root_pa;
	if (pmap_pages_alloc_zeroed(&root_pa, 0) != KERN_SUCCESS) {
		zfree(pmap_zone, p);
		return PMAP_NULL;
	}

	p->tte = (tt_entry_t *)phystokv(root_pa);
	p->ttep = root_pa;
	// every root shares the kernel half
	for (unsigned int i = RISCV_L1_INDEX_KERNEL_FIRST; i < RISCV_TT_ENTRIES; i++) {
		p->tte[i] = cpu_tte[i];
	}

	p->min = MACH_VM_MIN_ADDRESS;
	p->max = MACH_VM_MAX_ADDRESS;
	p->ledger = ledger;
	if (ledger != NULL) {
		ledger_reference(ledger);
	}
	os_ref_init_count_raw(&p->ref_count, NULL, 1);
	lck_rw_init(&p->rwlock, &pmap_lck_grp, LCK_ATTR_NULL);
	p->nx_enabled = true;
	p->is_64bit = true;
	p->type = (flags & PMAP_CREATE_NESTED) ? PMAP_TYPE_NESTED : PMAP_TYPE_USER;
	p->hw_asid = 0;
	p->asid_gen = 0;

	hw_lock_lock(&pmaps_lock, &pmap_lck_grp);
	queue_enter(&map_pmap_list, p, pmap_t, pmaps);
	hw_lock_unlock(&pmaps_lock);

	return p;
}

void
pmap_reference(pmap_t pmap)
{
	if (pmap != PMAP_NULL && pmap != kernel_pmap) {
		os_ref_retain_raw(&pmap->ref_count, NULL);
	}
}

// frees every table the pmap owns, nested and kernel ones stay
static void
pmap_free_tables(pmap_t pmap)
{
	for (unsigned int i = 0; i < RISCV_L1_INDEX_KERNEL_FIRST; i++) {
		tt_entry_t l1 = pmap->tte[i];
		if (!pmap_owns_l2(pmap, l1)) {
			continue;
		}
		pmap_paddr_t l2_pa = PTE_TO_PA(l1);
		tt_entry_t *l2 = (tt_entry_t *)phystokv(l2_pa);
		for (unsigned int j = 0; j < RISCV_TT_ENTRIES; j++) {
			if (!pte_is_table(l2[j])) {
				continue;
			}
			pmap_paddr_t l3_pa = PTE_TO_PA(l2[j]);
			pt_desc_t *l3_ptd = ptd_for_table(l3_pa);
			if (l3_ptd != NULL) {
				pv_head_table[pa_index(l3_pa)] = PVH_TYPE_NULL;
				pmap_pool_put(&ptd_pool, l3_ptd);
			}
			pmap_pages_free(l3_pa);
		}
		pt_desc_t *l2_ptd = ptd_for_table(l2_pa);
		if (l2_ptd != NULL) {
			pv_head_table[pa_index(l2_pa)] = PVH_TYPE_NULL;
			pmap_pool_put(&ptd_pool, l2_ptd);
		}
		pmap_pages_free(l2_pa);
		pmap->tte[i] = 0;
	}
}

void
pmap_destroy(pmap_t pmap)
{
	if (pmap == PMAP_NULL || pmap == kernel_pmap) {
		return;
	}
	if (os_ref_release_raw(&pmap->ref_count, NULL) > 0) {
		return;
	}

	hw_lock_lock(&pmaps_lock, &pmap_lck_grp);
	queue_remove(&map_pmap_list, pmap, pmap_t, pmaps);
	hw_lock_unlock(&pmaps_lock);

	// whatever is still mapped goes, the vm normally removed it all already
	pmap_remove_options(pmap, pmap->min, pmap->max, PMAP_OPTIONS_REMOVE);
	pmap_flush_tlb_asid(pmap);
	pmap_free_tables(pmap);
	pmap_asid_release(pmap);
	pmap_pages_free(pmap->ttep);

	if (pmap->nested_region_unnested_table_bitmap != NULL) {
		kfree_data(pmap->nested_region_unnested_table_bitmap,
		    pmap->nested_region_unnested_table_bitmap_size * sizeof(unsigned int));
	}
	if (pmap->nested_pmap != PMAP_NULL) {
		pmap_destroy(pmap->nested_pmap);
	}
	if (pmap->ledger != NULL) {
		ledger_dereference(pmap->ledger);
	}
	lck_rw_destroy(&pmap->rwlock, &pmap_lck_grp);
	zfree(pmap_zone, pmap);
}

void
pmap_require(pmap_t pmap)
{
	if (pmap != kernel_pmap) {
		zone_id_require(ZONE_ID_PMAP, sizeof(struct pmap), pmap);
	}
}

void
pmap_set_process(pmap_t pmap, int pid, char *procname)
{
#if MACH_ASSERT
	if (pmap == PMAP_NULL || pmap == kernel_pmap) {
		return;
	}
	pmap->pmap_pid = pid;
	strlcpy(pmap->pmap_procname, procname, sizeof(pmap->pmap_procname));
#else
	(void)pmap;
	(void)pid;
	(void)procname;
#endif
}

void
pmap_disable_NX(pmap_t pmap)
{
	pmap->nx_enabled = false;
}

void
pmap_set_nested(pmap_t pmap)
{
	pmap->type = PMAP_TYPE_NESTED;
}

bool
pmap_is_nested(pmap_t pmap)
{
	return pmap->type == PMAP_TYPE_NESTED;
}

// --- switching address spaces

void
pmap_switch(pmap_t pmap, __unused thread_t thread)
{
	boolean_t istate = ml_set_interrupts_enabled(FALSE);
	pmap_cpu_data_t *cpu = pmap_get_cpu_data();

	if (pmap == kernel_pmap) {
		csr_write(satp, SATP_MAKE(0, cpu_ttep));
		cpu->cpu_user_pmap = PMAP_NULL;
	} else {
		pmap_asid_activate(pmap);
		csr_write(satp, SATP_MAKE(pmap->hw_asid, pmap->ttep));
		cpu->cpu_user_pmap = pmap;
		cpu->cpu_nested_pmap = pmap->nested_pmap;
	}
	if (pmap_asid_count <= 1) {
		sfence_vma_all();
	}
	ml_set_interrupts_enabled(istate);
}

void
pmap_set_pmap(pmap_t pmap, thread_t thread)
{
	pmap_switch(pmap, thread);
}

void
pmap_switch_user(thread_t thread, vm_map_t map)
{
	// vm_map_switch_to relies on this to move the thread onto the new map
	thread->map = map;
	pmap_set_pmap(map->pmap, thread);
}

void
pmap_clear_user_ttb(void)
{
	// the kernel root holds no user mappings
	csr_write(satp, SATP_MAKE(0, cpu_ttep));
	sfence_vma_all();
	pmap_get_cpu_data()->cpu_user_pmap = PMAP_NULL;
}

// --- nesting the shared region, whole level 1 entries at a time

uint64_t
pmap_shared_region_size_min(__unused pmap_t pmap)
{
	return RISCV_TT_L1_SIZE;
}

kern_return_t
pmap_nest(pmap_t grand, pmap_t subord, addr64_t vstart, uint64_t size)
{
	if ((vstart | size) & (RISCV_TT_L1_SIZE - 1)) {
		panic("%s: 0x%llx+0x%llx is not level 1 aligned", __func__, vstart, size);
	}
	if (subord->type != PMAP_TYPE_NESTED) {
		panic("%s: pmap %p is not nested", __func__, subord);
	}

	// the shared region's level 2 tables exist for its whole range before anyone points at them
	for (addr64_t va = vstart; va < vstart + size; va += RISCV_TT_L1_SIZE) {
		for (;;) {
			pmap_lock_shared(subord);
			bool present = pte_is_valid(*pmap_tt1e(subord, va));
			pmap_unlock_shared(subord);
			if (present) {
				break;
			}
			pmap_paddr_t table_pa;
			if (pmap_pages_alloc_zeroed(&table_pa, 0) != KERN_SUCCESS ||
			    pmap_pool_replenish(&ptd_pool, 0) != KERN_SUCCESS) {
				return KERN_RESOURCE_SHORTAGE;
			}
			pt_desc_t *ptd = pmap_pool_get(&ptd_pool);
			pmap_lock_exclusive(subord);
			tt_entry_t *l1p = pmap_tt1e(subord, va);
			if (!pte_is_valid(*l1p) && ptd != NULL) {
				pmap_install_table(subord, l1p, table_pa, va, 2, ptd);
				ptd = NULL;
				table_pa = 0;
			}
			pmap_unlock_exclusive(subord);
			if (ptd != NULL) {
				pmap_pool_put(&ptd_pool, ptd);
			}
			if (table_pa != 0) {
				pmap_pages_free(table_pa);
			}
		}
	}

	pmap_lock_exclusive(grand);
	if (grand->nested_pmap == PMAP_NULL) {
		pmap_reference(subord);
		grand->nested_pmap = subord;
		grand->nested_region_addr = vstart;
		grand->nested_region_size = size;
		grand->nested_region_true_start = vstart;
		grand->nested_region_true_end = vstart + size;
		grand->nested_bounds_set = true;
	}
	for (addr64_t va = vstart; va < vstart + size; va += RISCV_TT_L1_SIZE) {
		tt_entry_t *l1p = pmap_tt1e(grand, va);
		if (pmap_owns_l2(grand, *l1p)) {
			panic("%s: pmap %p already has its own tables at 0x%llx", __func__, grand, va);
		}
		os_atomic_store(l1p, *pmap_tt1e(subord, va), release);
	}
	pmap_unlock_exclusive(grand);
	pmap_flush_tlb_asid(grand);
	return KERN_SUCCESS;
}

kern_return_t
pmap_unnest_options(pmap_t grand, addr64_t vaddr, uint64_t size, __unused unsigned int option)
{
	addr64_t start = vaddr & ~(addr64_t)(RISCV_TT_L1_SIZE - 1);
	addr64_t end = (vaddr + size + RISCV_TT_L1_SIZE - 1) & ~(addr64_t)(RISCV_TT_L1_SIZE - 1);

	pmap_lock_exclusive(grand);
	for (addr64_t va = start; va < end; va += RISCV_TT_L1_SIZE) {
		tt_entry_t *l1p = pmap_tt1e(grand, va);
		if (pte_is_table(*l1p) && !pmap_owns_l2(grand, *l1p)) {
			os_atomic_store(l1p, 0, release);
		}
	}
	pmap_unlock_exclusive(grand);
	pmap_flush_tlb_asid(grand);
	return KERN_SUCCESS;
}

kern_return_t
pmap_unnest(pmap_t grand, addr64_t vaddr, uint64_t size)
{
	return pmap_unnest_options(grand, vaddr, size, 0);
}

boolean_t
pmap_adjust_unnest_parameters(__unused pmap_t p, vm_map_offset_t *s, vm_map_offset_t *e)
{
	*s &= ~(vm_map_offset_t)(RISCV_TT_L1_SIZE - 1);
	*e = (*e + RISCV_TT_L1_SIZE - 1) & ~(vm_map_offset_t)(RISCV_TT_L1_SIZE - 1);
	// report the range as adjusted so the vm logs unnests that widen
	return TRUE;
}

kern_return_t
pmap_fork_nest(pmap_t old_pmap, pmap_t new_pmap)
{
	if (old_pmap == NULL || new_pmap == NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	if (old_pmap->nested_pmap == NULL) {
		return KERN_SUCCESS;
	}
	pmap_t subord = old_pmap->nested_pmap;
	addr64_t start = old_pmap->nested_region_addr;
	uint64_t size = old_pmap->nested_region_size;

	kern_return_t kr = pmap_nest(new_pmap, subord, start, size);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	// the child shares only what the parent still had nested
	pmap_lock_exclusive(new_pmap);
	pmap_lock_shared(old_pmap);
	for (addr64_t va = start; va < start + size; va += RISCV_TT_L1_SIZE) {
		if (*pmap_tt1e(old_pmap, va) != *pmap_tt1e(subord, va)) {
			os_atomic_store(pmap_tt1e(new_pmap, va), 0, release);
		}
	}
	pmap_unlock_shared(old_pmap);
	pmap_unlock_exclusive(new_pmap);
	pmap_flush_tlb_asid(new_pmap);
	return KERN_SUCCESS;
}

void
pmap_trim(__unused pmap_t grand, __unused pmap_t subord, __unused addr64_t vstart, __unused uint64_t size)
{
	// nesting shares whole level 2 tables, there is nothing below them to trim
}

void
pmap_set_shared_region(pmap_t grand, pmap_t subord, addr64_t vstart, uint64_t size)
{
	pmap_lock_exclusive(grand);
	if (grand->nested_pmap == PMAP_NULL) {
		pmap_reference(subord);
		grand->nested_pmap = subord;
		grand->nested_region_addr = vstart;
		grand->nested_region_size = size;
		grand->nested_region_true_start = vstart;
		grand->nested_region_true_end = vstart + size;
	}
	pmap_unlock_exclusive(grand);
}

// --- the commpage, shared through one level 1 entry at the top of the user half

void
pmap_create_commpages(vm_map_address_t *kernel_data_addr, vm_map_address_t *kernel_text_addr,
    vm_map_address_t *kernel_ro_data_addr, vm_map_address_t *user_text_addr)
{
	pmap_paddr_t data_pa, ro_data_pa;
	kern_return_t kr;

	(void)pmap_pages_alloc_zeroed(&data_pa, 0);
	(void)pmap_pages_alloc_zeroed(&ro_data_pa, 0);

	commpage_pmap = pmap_create_options(NULL, 0, 0);
	assert(commpage_pmap != PMAP_NULL);
	commpage_pmap->type = PMAP_TYPE_COMMPAGE;
	// the commpage lives in the top gigabyte of the user half, above the task address space
	commpage_pmap->max = 1ULL << (RISCV_TT_L1_SHIFT + 8);

	kr = pmap_enter_options_addr(commpage_pmap, _COMM_PAGE64_BASE_ADDRESS, data_pa, VM_PROT_READ,
	    VM_PROT_READ, VM_WIMG_USE_DEFAULT, TRUE, 0, NULL, PMAP_MAPPING_TYPE_INFER);
	assert(kr == KERN_SUCCESS);
	kr = pmap_enter_options_addr(commpage_pmap, _COMM_PAGE64_RO_ADDRESS, ro_data_pa, VM_PROT_READ,
	    VM_PROT_READ, VM_WIMG_USE_DEFAULT, TRUE, 0, NULL, PMAP_MAPPING_TYPE_INFER);
	assert(kr == KERN_SUCCESS);

	// global, every process sees the same page under any asid
	pt_entry_t *ptep = pmap_pte(commpage_pmap, _COMM_PAGE64_BASE_ADDRESS);
	os_atomic_or(ptep, PTE_G, relaxed);
	ptep = pmap_pte(commpage_pmap, _COMM_PAGE64_RO_ADDRESS);
	os_atomic_or(ptep, PTE_G, relaxed);

	*kernel_data_addr = phystokv(data_pa);
	*kernel_ro_data_addr = phystokv(ro_data_pa);
	*kernel_text_addr = 0;
	*user_text_addr = 0;
}

void
pmap_insert_commpage(pmap_t pmap)
{
	if (commpage_pmap == PMAP_NULL) {
		return;
	}
	pmap_lock_exclusive(pmap);
	os_atomic_store(pmap_tt1e(pmap, _COMM_PAGE64_BASE_ADDRESS),
	    *pmap_tt1e(commpage_pmap, _COMM_PAGE64_BASE_ADDRESS), release);
	pmap_unlock_exclusive(pmap);
}

// --- queries

kern_return_t
pmap_query_page_info(pmap_t pmap, vm_map_offset_t va, int *disp_p)
{
	int disp = 0;

	if (pmap == PMAP_NULL || pmap == kernel_pmap) {
		*disp_p = 0;
		return KERN_INVALID_ARGUMENT;
	}

	pmap_lock_shared(pmap);
	pt_entry_t *ptep = pmap_pte(pmap, va);
	if (ptep != NULL) {
		pt_entry_t pte = *ptep;
		if (pte_is_compressed(pte)) {
			disp |= PMAP_QUERY_PAGE_COMPRESSED;
			if (pte & PTE_COMPRESSED_ALT) {
				disp |= PMAP_QUERY_PAGE_COMPRESSED_ALTACCT;
			}
		} else if (pte_is_valid(pte)) {
			disp |= PMAP_QUERY_PAGE_PRESENT;
			pmap_paddr_t pa = PTE_TO_PA(pte);
			if (pa_valid(pa)) {
				unsigned int pai = pa_index(pa);
				pvh_lock(pai);
				for (pv_entry_t *pve = pvh_list(pv_head_table[pai]); pve != NULL; pve = pve->next) {
					if (pve_ptep(pve) != ptep) {
						continue;
					}
					if (pve->ptep_flags & PVE_ALTACCT) {
						disp |= PMAP_QUERY_PAGE_ALTACCT;
					} else if (ppattr_test(pai, PP_ATTR_REUSABLE)) {
						disp |= PMAP_QUERY_PAGE_REUSABLE;
					} else if (pve->ptep_flags & PVE_INTERNAL) {
						disp |= PMAP_QUERY_PAGE_INTERNAL;
					}
					break;
				}
				pvh_unlock(pai);
			}
		}
	}
	pmap_unlock_shared(pmap);
	*disp_p = disp;
	return KERN_SUCCESS;
}

mach_vm_size_t
pmap_query_resident(pmap_t pmap, vm_map_offset_t start, vm_map_offset_t end, mach_vm_size_t *compressed_bytes_p)
{
	mach_vm_size_t resident = 0, compressed = 0;

	if (pmap == PMAP_NULL || start >= end) {
		if (compressed_bytes_p) {
			*compressed_bytes_p = 0;
		}
		return 0;
	}
	pmap_lock_shared(pmap);
	for (vm_map_offset_t va = start; va < end; va += PAGE_SIZE) {
		pt_entry_t *ptep = pmap_pte(pmap, va);
		if (ptep == NULL) {
			// skip the rest of an absent level 3 table
			va = ((va & ~(vm_map_offset_t)(RISCV_TT_L2_SIZE - 1)) + RISCV_TT_L2_SIZE) - PAGE_SIZE;
			continue;
		}
		if (pte_is_valid(*ptep)) {
			resident += PAGE_SIZE;
		} else if (pte_is_compressed(*ptep)) {
			compressed += PAGE_SIZE;
		}
	}
	pmap_unlock_shared(pmap);
	if (compressed_bytes_p) {
		*compressed_bytes_p = compressed;
	}
	return resident;
}

boolean_t
pmap_is_empty(pmap_t pmap, vm_map_offset_t start, vm_map_offset_t end)
{
	if (pmap == PMAP_NULL) {
		return TRUE;
	}
	pmap_lock_shared(pmap);
	boolean_t empty = TRUE;
	for (vm_map_offset_t va = start; va < end && empty; va += PAGE_SIZE) {
		pt_entry_t *ptep = pmap_pte(pmap, va);
		if (ptep == NULL) {
			va = ((va & ~(vm_map_offset_t)(RISCV_TT_L2_SIZE - 1)) + RISCV_TT_L2_SIZE) - PAGE_SIZE;
			continue;
		}
		// as on arm, compressed markers count and nested or commpage tables belong to someone else
		if (*ptep != 0 && (pmap == kernel_pmap || ptd_for_ptep(ptep) == NULL ||
		    ptd_for_ptep(ptep)->pmap == pmap)) {
			empty = FALSE;
		}
	}
	pmap_unlock_shared(pmap);
	return empty;
}

// --- bootstrap

void
pmap_early_init(pmap_paddr_t first_free)
{
	avail_start = first_avail_phys = round_page(first_free);
	// the loader's memSize (clamped by maxmem) ends the managed range, firmware may own what follows
	avail_end = trunc_page(gPhysBase + mem_size);
	avail_remaining = (unsigned int)atop(avail_end - avail_start);
}

void
pmap_bootstrap(vm_offset_t dynamic_memory_begin)
{
	kernel_pmap->tte = cpu_tte;
	kernel_pmap->ttep = cpu_ttep;
	kernel_pmap->min = VM_MIN_KERNEL_AND_KEXT_ADDRESS;
	kernel_pmap->max = UINTPTR_MAX;
	os_ref_init_count_raw(&kernel_pmap->ref_count, NULL, 1);
	kernel_pmap->nx_enabled = true;
	kernel_pmap->is_64bit = true;
	kernel_pmap->type = PMAP_TYPE_KERNEL;
	kernel_pmap->hw_asid = 0;
	lck_rw_init(&kernel_pmap->rwlock, &pmap_lck_grp, LCK_ATTR_NULL);

	for (unsigned int i = 0; i < PV_LOCK_COUNT; i++) {
		hw_lock_init(&pv_locks[i]);
	}
	hw_lock_init(&pv_pool.lock);
	hw_lock_init(&ptd_pool.lock);
	hw_lock_init(&pmap_asid_lock);
	hw_lock_init(&pmaps_lock);
	queue_init(&map_pmap_list);

	pmap_asid_probe();

	// every page from the start of managed memory to the end of dram gets a pv head and attributes
	vm_first_phys = gPhysBase;
	vm_last_phys = avail_end;
	pmap_npages = (unsigned int)atop(vm_last_phys - vm_first_phys);

	vm_size_t pv_size = round_page(pmap_npages * sizeof(uintptr_t));
	vm_size_t attr_size = round_page(pmap_npages * sizeof(uint16_t));
	pmap_paddr_t tables_pa = avail_start;
	avail_start += pv_size + attr_size;
	avail_remaining -= (unsigned int)atop(pv_size + attr_size);
	pv_head_table = (uintptr_t *)phystokv(tables_pa);
	pp_attr_table = (uint16_t *)phystokv(tables_pa + pv_size);
	bzero(pv_head_table, pv_size + attr_size);

	virtual_space_start = dynamic_memory_begin;
	virtual_space_end = KERNEL_DYN_END;

	pmap_cpu_data_init();
}

void
pmap_virtual_space(vm_offset_t *startp, vm_offset_t *endp)
{
	*startp = virtual_space_start;
	*endp = virtual_space_end;
}

uint_t
pmap_free_pages(void)
{
	return avail_remaining;
}

uint_t
pmap_free_pages_span(void)
{
	return (uint_t)atop(avail_end - avail_start);
}

boolean_t
pmap_next_page(ppnum_t *pnum)
{
	if (avail_start < avail_end) {
		*pnum = (ppnum_t)atop(avail_start);
		avail_start += PAGE_SIZE;
		avail_remaining--;
		return TRUE;
	}
	return FALSE;
}

boolean_t
pmap_next_page_hi(ppnum_t *pnum, __unused boolean_t might_free)
{
	return pmap_next_page(pnum);
}

boolean_t
pmap_valid_page(ppnum_t pn)
{
	return pa_valid(ptoa(pn));
}

boolean_t
pmap_valid_address(pmap_paddr_t addr)
{
	return pa_valid(addr);
}

boolean_t
pmap_has_managed_page(ppnum_t first, ppnum_t last)
{
	for (ppnum_t pn = first; pn <= last && pn >= first; pn++) {
		if (pa_valid(ptoa(pn))) {
			return TRUE;
		}
	}
	return FALSE;
}

boolean_t
pmap_bootloader_page(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	return pa < first_avail_phys && pa >= gDramBase;
}

void
pmap_init(void)
{
	pmap_zone = zone_create_ext("pmap", sizeof(struct pmap),
	    ZC_ZFREE_CLEARMEM, ZONE_ID_PMAP, NULL);

	_vm_object_allocate(mem_size, pmap_object, VM_MAP_SERIAL_SPECIAL);
	pmap_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;

	pmap_initialized = TRUE;
}

bool
pmap_is_initialized(void)
{
	return pmap_initialized;
}

void
pmap_gc(void)
{
}

void
compute_pmap_gc_throttle(__unused void *arg)
{
}

void
mapping_adjust(void)
{
	(void)pmap_pool_replenish(&pv_pool, PMAP_PAGES_ALLOCATE_NOWAIT);
}

void
mapping_free_prime(void)
{
	(void)pmap_pool_replenish(&pv_pool, 0);
	(void)pmap_pool_replenish(&ptd_pool, 0);
}

uint64_t
pmap_release_pages_fast(void)
{
	return 0;
}

void
pmap_lock_phys_page(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	if (pa_valid(pa)) {
		pvh_lock(pa_index(pa));
	}
}

void
pmap_unlock_phys_page(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	if (pa_valid(pa)) {
		pvh_unlock(pa_index(pa));
	}
}

bool
pmap_verify_free(ppnum_t pn)
{
	pmap_paddr_t pa = ptoa(pn);
	if (!pa_valid(pa)) {
		return true;
	}
	return pvh_type(pv_head_table[pa_index(pa)]) == PVH_TYPE_NULL;
}

void
pmap_recycle_page(ppnum_t pn)
{
	if (!pmap_verify_free(pn)) {
		panic("%s: page 0x%llx is referenced", __func__, (unsigned long long)ptoa(pn));
	}
}

uint16_t
pmap_page_attributes(unsigned int pai)
{
	return pp_attr_table[pai];
}

void
pmap_page_attributes_update(unsigned int pai, uint16_t clear, uint16_t set)
{
	pvh_lock(pai);
	pp_attr_table[pai] = (uint16_t)((pp_attr_table[pai] & ~clear) | set);
	pvh_unlock(pai);
}

// walks every mapping of a page, used when its memory type changes
void
pmap_page_update_memtype(unsigned int pai, unsigned int wimg)
{
	pvh_lock(pai);
	pp_attr_table[pai] = (uint16_t)((pp_attr_table[pai] & ~PP_ATTR_WIMG_MASK) | (wimg & PP_ATTR_WIMG_MASK));
	pt_entry_t memtype = pmap_wimg_to_pte(wimg);
	for (pv_entry_t *pve = pvh_list(pv_head_table[pai]); pve != NULL; pve = pve->next) {
		pt_entry_t *ptep = pve_ptep(pve);
		pmap_t pmap;
		vm_map_address_t va;
		ptep_owner(ptep, &pmap, &va);
		pt_entry_t spte = *ptep;
		os_atomic_store(ptep, (spte & ~pte_memtype_mask()) | memtype, relaxed);
		pmap_flush_tlb_region(pmap, va, PAGE_SIZE);
	}
	pvh_unlock(pai);
}

// debuggers find lowGlo at a fixed address in the cpu windows' 2MB
void
pmap_map_globals(void)
{
	pt_entry_t bits = PTE_V | PTE_R | PTE_A | PTE_G | pmap_wimg_to_pte(VM_WIMG_DEFAULT);
	riscv_vm_map_static(LOWGLOBAL_ALIAS, ml_static_vtop((vm_offset_t)&lowGlo), PAGE_SIZE, bits);
}
