// SPDX-License-Identifier: MIT
// linuxkpi pages: wired contiguous memory with a vm_page per physical page
// pages are found again by physical address through a hash

#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/page.h>

#include "../../pd_fbsd_os.h"

#define PD_PAGE_HASH	1024
// the offset bits in a page. linux/page.h makes PAGE_MASK the linux one, the page bits
#define PD_PAGE_OFFSET	((vm_paddr_t)PAGE_SIZE - 1)

struct pd_page_run {
	vm_paddr_t phys;
	void *kva;
	unsigned long npages;
	bool owned;
	// joined kernel mappings still reaching these pages, and a free that waits for them
	int vmaps;
	bool freed;
	struct vm_page pages[];
};

// a joined mapping and the page runs it keeps alive
struct pd_vmap {
	struct pd_vmap *next;
	void *va;
	unsigned int count;
	struct pd_page_run *runs[];
};

static struct pd_vmap *pd_vmaps;

static void pd_page_flush(struct page *page);

static struct vm_page *pd_page_hash[PD_PAGE_HASH];
static struct mtx pd_page_lock;

static inline unsigned
pd_page_bucket(vm_paddr_t phys)
{
	return (unsigned)((phys >> PAGE_SHIFT) % PD_PAGE_HASH);
}

static vm_page_t
pd_page_find(vm_paddr_t phys)
{
	vm_page_t m;

	phys &= ~PD_PAGE_OFFSET;
	for (m = pd_page_hash[pd_page_bucket(phys)]; m != NULL; m = m->pd_hnext) {
		if (m->phys_addr == phys)
			return m;
	}
	return NULL;
}

static void
pd_page_run_unhash(struct pd_page_run *run)
{
	unsigned long i;

	for (i = 0; i < run->npages; i++) {
		vm_page_t *pp = &pd_page_hash[pd_page_bucket(run->pages[i].phys_addr)];
		while (*pp != NULL && *pp != &run->pages[i])
			pp = &(*pp)->pd_hnext;
		if (*pp != NULL)
			*pp = run->pages[i].pd_hnext;
	}
}

static struct pd_page_run *
pd_page_run_new(vm_paddr_t phys, void *kva, unsigned long npages, bool owned)
{
	struct pd_page_run *run;
	unsigned long i;

	run = malloc(sizeof(*run) + npages * sizeof(struct vm_page), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (run == NULL)
		return NULL;

	run->phys = phys;
	run->kva = kva;
	run->npages = npages;
	run->owned = owned;

	mtx_lock(&pd_page_lock);
	for (i = 0; i < npages; i++) {
		vm_page_t m = &run->pages[i];
		unsigned b;

		m->phys_addr = phys + ((vm_paddr_t)i << PAGE_SHIFT);
		m->pd_kva = (char *)kva + (i << PAGE_SHIFT);
		m->ref_count = 1;
		m->pd_run = run;
		b = pd_page_bucket(m->phys_addr);
		m->pd_hnext = pd_page_hash[b];
		pd_page_hash[b] = m;
	}
	mtx_unlock(&pd_page_lock);
	return run;
}

vm_page_t
PHYS_TO_VM_PAGE(vm_paddr_t pa)
{
	vm_page_t m;

	mtx_lock(&pd_page_lock);
	m = pd_page_find(pa);
	mtx_unlock(&pd_page_lock);
	return m;
}

struct vm_page *
pd_lkpi_virt_to_page(const void *va)
{
	vm_paddr_t pa = pdos_vtophys(va);
	vm_page_t page = PHYS_TO_VM_PAGE(pa);
	if (page != NULL)
		return page;

	// heap memory gets a one-page record the first time it is asked for
	struct pd_page_run *run = pd_page_run_new(pa & ~PD_PAGE_OFFSET,
		(void *)((uintptr_t)va & ~(uintptr_t)PD_PAGE_OFFSET), 1, false);
	return run != NULL ? &run->pages[0] : NULL;
}

void
si_meminfo(struct sysinfo *si)
{
	si->totalram = physmem;
	si->freeram = physmem / 2;
	si->totalhigh = 0;
	si->freehigh = 0;
	si->mem_unit = PAGE_SIZE;
}

void *
linux_page_address(const struct page *page)
{
	return page->pd_kva;
}

struct page *
linux_alloc_pages(gfp_t flags, unsigned int order)
{
	unsigned long npages = 1UL << order;
	size_t size = (size_t)PAGE_SIZE << order;
	uint64_t phys;
	void *kva;

	kva = pdos_malloc_contig(size, PAGE_SIZE,
		(flags & GFP_DMA32) != 0 ? 0xffffffffULL : ~0ULL, &phys);
	if (kva == NULL)
		return NULL;
	if ((flags & M_ZERO) != 0)
		memset(kva, 0, size);

	struct pd_page_run *run = pd_page_run_new(phys, kva, npages, true);
	if (run == NULL) {
		pdos_free_contig(kva, size);
		return NULL;
	}
	return &run->pages[0];
}

void
linux_free_pages(struct page *page, unsigned int order)
{
	struct pd_page_run *run;

	if (page == NULL)
		return;
	// a page someone took with get_page stays until its last holder lets go
	if (order == 0 && __atomic_load_n(&page->ref_count, __ATOMIC_ACQUIRE) > 1) {
		__atomic_sub_fetch(&page->ref_count, 1, __ATOMIC_ACQ_REL);
		return;
	}

	run = page->pd_run;
	for (unsigned long i = 0; i < run->npages; i++) {
		if (run->pages[i].pd_kva_cached != NULL) {
			pdos_unmap_pages(run->pages[i].pd_kva);
			run->pages[i].pd_kva = run->pages[i].pd_kva_cached;
			run->pages[i].pd_kva_cached = NULL;
		}
	}
	mtx_lock(&pd_page_lock);
	pd_page_run_unhash(run);
	if (run->vmaps > 0) {
		// xnu refuses to recycle a page that is still mapped, the last vunmap frees it
		run->freed = true;
		mtx_unlock(&pd_page_lock);
		return;
	}
	mtx_unlock(&pd_page_lock);

	if (run->owned)
		pdos_free_contig(run->kva, (size_t)run->npages << PAGE_SHIFT);
	free(run, M_DEVBUF);
}

void
linux_release_pages(release_pages_arg arg, int nr)
{
	int i;

	for (i = 0; i < nr; i++)
		__free_page(arg.pages[i]);
}

vm_offset_t
linux_alloc_kmem(gfp_t flags, unsigned int order)
{
	struct page *page = linux_alloc_pages(flags, order);
	return page != NULL ? (vm_offset_t)page->pd_kva : 0;
}

void
linux_free_kmem(vm_offset_t addr, unsigned int order)
{
	struct page *page = virt_to_page((void *)addr);
	if (page != NULL)
		linux_free_pages(page, order);
}

int
is_vmalloc_addr(const void *addr)
{
	return false;
}

void *
_ioremap_attr(vm_paddr_t phys_addr, unsigned long size, int attr)
{
	// write-back maps cached, write-combining as such (2), anything else uncached
	if (attr == VM_MEMATTR_WRITE_COMBINING)
		return pdos_map_phys(phys_addr, size, 2);
	return pdos_map_phys(phys_addr, size, attr != VM_MEMATTR_WRITE_BACK);
}

void
iounmap(void *addr)
{
	// the size is not tracked, the seam unmaps by address
	pdos_unmap_phys(addr, 0);
}

void *
vmap(struct page **pages, unsigned int count, unsigned long flags, int prot)
{
	unsigned int i;
	uint64_t *phys;
	void *va;
	bool uncached;

	if (count == 0)
		return NULL;
	for (i = 0; i < count; i++) {
		if (pages[i] == NULL) {
			printf("vmap: page %u of %u is missing\n", i, count);
			return NULL;
		}
	}
	uncached = prot != 0 && pgprot2cachemode(prot) != VM_MEMATTR_WRITE_BACK;
	// physically adjacent pages from one allocation already have one mapping,
	// which is the cached alias, so only cached requests may use it
	for (i = uncached ? count + 1 : 1; i < count; i++) {
		if (pages[0]->pd_kva == NULL || pages[i]->pd_kva !=
		    (char *)pages[0]->pd_kva + ((size_t)i << PAGE_SHIFT))
			break;
	}
	if (i == count)
		return pages[0]->pd_kva;

	phys = malloc(count * sizeof(*phys), M_DEVBUF, M_WAITOK);
	if (phys == NULL)
		return NULL;
	for (i = 0; i < count; i++)
		phys[i] = pages[i]->phys_addr;
	if (uncached) {
		for (i = 0; i < count; i++)
			pd_page_flush(pages[i]);
	}
	va = pdos_map_pages(phys, count, uncached);
	free(phys, M_DEVBUF);
	if (va == NULL)
		return NULL;

	struct pd_vmap *vm = malloc(sizeof(*vm) + count * sizeof(vm->runs[0]), M_DEVBUF, M_WAITOK);
	if (vm == NULL) {
		pdos_unmap_pages(va);
		return NULL;
	}
	vm->va = va;
	vm->count = count;
	mtx_lock(&pd_page_lock);
	for (i = 0; i < count; i++) {
		vm->runs[i] = pages[i]->pd_run;
		vm->runs[i]->vmaps++;
	}
	vm->next = pd_vmaps;
	pd_vmaps = vm;
	mtx_unlock(&pd_page_lock);
	return va;
}

// pinning user memory into struct pages needs a vm_map wiring path PD lacks, radeon's userptr
// ioctl gets the fault a bad address would
long
lkpi_get_user_pages(unsigned long start, unsigned long nr_pages, unsigned int gup_flags,
    struct page **pages)
{
	return -EFAULT;
}

void
vunmap(void *addr)
{
	struct pd_vmap **link, *vm = NULL;
	unsigned int i;

	mtx_lock(&pd_page_lock);
	for (link = &pd_vmaps; *link != NULL; link = &(*link)->next) {
		if ((*link)->va == addr) {
			vm = *link;
			*link = vm->next;
			break;
		}
	}
	mtx_unlock(&pd_page_lock);
	// a single allocation's own address was never joined
	if (vm == NULL)
		return;

	pdos_unmap_pages(addr);
	for (i = 0; i < vm->count; i++) {
		struct pd_page_run *run = vm->runs[i];
		bool release;

		mtx_lock(&pd_page_lock);
		release = --run->vmaps == 0 && run->freed;
		mtx_unlock(&pd_page_lock);
		if (release) {
			if (run->owned)
				pdos_free_contig(run->kva, (size_t)run->npages << PAGE_SHIFT);
			free(run, M_DEVBUF);
		}
	}
	free(vm, M_DEVBUF);
}

// write back and drop a page's lines from the cached kernel alias, so a later
// eviction cannot overwrite what a write-combined or uncached view stored
static void
pd_page_flush(struct page *page)
{
	char *va = page->pd_kva;
	size_t off;

	if (va == NULL)
		return;
#if defined(__x86_64__)
	for (off = 0; off < PAGE_SIZE; off += 64)
		__asm__ volatile("clflush %0" : "+m"(*(volatile char *)(va + off)));
	__asm__ volatile("mfence" ::: "memory");
#endif
}

int
lkpi_set_pages_attr(struct page *page, int numpages, vm_memattr_t ma)
{
	int i;

	for (i = 0; i < numpages; i++)
		pmap_page_set_memattr(&page[i], ma);
	return 0;
}

int
lkpi_arch_phys_wc_add(unsigned long base, unsigned long size)
{
	return 0;
}

void
lkpi_arch_phys_wc_del(int reg)
{
}

void *
linuxkpi_page_frag_alloc(struct page_frag_cache *pfc, size_t fragsz, gfp_t gfp)
{
	struct page *pages;

	if (fragsz == 0 || fragsz > PAGE_SIZE)
		return NULL;

	pages = alloc_pages(gfp, 0);
	if (pages == NULL)
		return NULL;

	pfc->va = linux_page_address(pages);
	pfc->pagecnt_bias = 0;
	return pfc->va;
}

void
linuxkpi_page_frag_free(void *addr)
{
	linux_free_pages(virt_to_page(addr), 0);
}

void
linuxkpi__page_frag_cache_drain(struct page *page, size_t count __unused)
{
	linux_free_pages(page, 0);
}

static void
lkpi_page_init(void *arg)
{
	mtx_init(&pd_page_lock, "lkpi pages", NULL, MTX_DEF);
}
SYSINIT(lkpi_page, SI_SUB_LOCK, SI_ORDER_ANY, lkpi_page_init, NULL);

void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{
	memcpy(dst->pd_kva, src->pd_kva, PAGE_SIZE);
}

void
pmap_zero_page(vm_page_t m)
{
	memset(m->pd_kva, 0, PAGE_SIZE);
}

// a non write-back attribute gets its own physical mapping in that cache mode, since
// kernel_task's cached alias serves stale lines. page_address, kmap and vmap follow pd_kva.
void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{
	uint64_t phys = m->phys_addr;
	void *va;

	if (m->pd_memattr == ma)
		return;
	if (ma == VM_MEMATTR_WRITE_BACK) {
		if (m->pd_kva_cached != NULL) {
			pdos_unmap_pages(m->pd_kva);
			m->pd_kva = m->pd_kva_cached;
			m->pd_kva_cached = NULL;
		}
		m->pd_memattr = ma;
		return;
	}
	if (m->pd_kva == NULL) {
		m->pd_memattr = ma;
		return;
	}
	if (m->pd_kva_cached == NULL) {
		pd_page_flush(m);
		va = pdos_map_pages(&phys, 1, 1);
		if (va == NULL)
			return;
		m->pd_kva_cached = m->pd_kva;
		m->pd_kva = va;
	}
	m->pd_memattr = ma;
}

#ifdef PD_LKPI_DRM
// device apertures get page records with no kernel address, so ttm can name
// vram pages by physical address
#include <vm/vm_phys.h>

int
vm_phys_fictitious_reg_range(vm_paddr_t start, vm_paddr_t end, vm_memattr_t memattr)
{
	struct pd_page_run *run;
	unsigned long i;

	run = pd_page_run_new(start, NULL, (end - start) >> PAGE_SHIFT, false);
	if (run == NULL)
		return ENOMEM;
	for (i = 0; i < run->npages; i++) {
		run->pages[i].pd_kva = NULL;
		run->pages[i].pd_memattr = memattr;
		run->pages[i].flags |= PG_FICTITIOUS;
	}
	return 0;
}

void
vm_phys_fictitious_unreg_range(vm_paddr_t start, vm_paddr_t end)
{
	vm_page_t m = PHYS_TO_VM_PAGE(start);

	struct pd_page_run *run = m != NULL ? m->pd_run : NULL;

	if (run == NULL || run->phys != start)
		return;
	mtx_lock(&pd_page_lock);
	pd_page_run_unhash(run);
	mtx_unlock(&pd_page_lock);
	free(run, M_DEVBUF);
}

vm_page_t
vm_phys_fictitious_to_vm_page(vm_paddr_t pa)
{
	return PHYS_TO_VM_PAGE(pa);
}
#endif
