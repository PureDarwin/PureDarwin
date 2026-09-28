// builds the kernel's own sv39 tables to replace the gigapages start.s used, then hands the
// rest of memory to the pmap

#include <mach/vm_param.h>
#include <mach/machine/vm_param.h>
#include <kern/misc_protos.h>
#include <kern/debug.h>
#include <vm/pmap.h>
#include <vm/vm_page_internal.h>
#include <libkern/kernel_mach_header.h>
#include <libkern/section_keywords.h>
#include <pexpert/pexpert.h>
#include <pexpert/device_tree.h>
#include <pexpert/riscv/boot.h>
#include <riscv/pmap_internal.h>
#include <riscv/proc_reg.h>
#include <riscv/sbi.h>
#include <riscv/misc_protos.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/cpu_internal.h>
#include <riscv/lowglobals.h>
#include <string.h>

extern void *last_kernel_symbol;
extern vm_offset_t intstack_low_guard;
extern vm_offset_t intstack_high_guard;
extern vm_offset_t excepstack_high_guard;
extern uint64_t riscv_kernel_satp;
extern uint64_t riscv_physmap_offset;

SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_base;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_top;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kext_base;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kext_top;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_stext;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_etext;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_slide;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_slid_base;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_slid_top;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_stext;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_etext;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_sdata;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_edata;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_sinfo;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_einfo;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_slinkedit;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_elinkedit;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_builtinkmod_text;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_builtinkmod_text_end;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernelcache_base;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernelcache_top;

SECURITY_READ_ONLY_LATE(unsigned long) gVirtBase;
SECURITY_READ_ONLY_LATE(unsigned long) gPhysBase;
SECURITY_READ_ONLY_LATE(unsigned long) gPhysSize;

vm_offset_t     mem_size;               /* size of the physical memory the kernel manages */
uint64_t        mem_actual;             /* all of it, before any maxmem */
uint64_t        max_mem;                /* the size the vm sizes itself for */
uint64_t        max_mem_actual;
uint64_t        sane_size;              /* memory size used for defaults */

SECURITY_READ_ONLY_LATE(vm_map_address_t) physmap_base;
SECURITY_READ_ONLY_LATE(vm_map_address_t) physmap_end;
SECURITY_READ_ONLY_LATE(vm_map_address_t) physmap_max;
SECURITY_READ_ONLY_LATE(vm_offset_t) static_memory_end;
SECURITY_READ_ONLY_LATE(vm_offset_t) end_kern;
SECURITY_READ_ONLY_LATE(vm_offset_t) etext;
SECURITY_READ_ONLY_LATE(vm_offset_t) sdata;
SECURITY_READ_ONLY_LATE(vm_offset_t) edata;

SECURITY_READ_ONLY_LATE(vm_offset_t) segLOWESTTEXT;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLOWEST;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLOWESTRO;
SECURITY_READ_ONLY_LATE(vm_offset_t) segHIGHESTRO;

SECURITY_READ_ONLY_LATE(vm_offset_t) segLOWESTKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segHIGHESTKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLOWESTROKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segHIGHESTROKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLOWESTAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segHIGHESTAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLOWESTROAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segHIGHESTROAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLOWESTRXAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segHIGHESTRXAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t) segHIGHESTNLEAuxKC;

SECURITY_READ_ONLY_LATE(static vm_offset_t) segTEXTB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeTEXT;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segDATACONSTB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeDATACONST;
SECURITY_READ_ONLY_LATE(vm_offset_t) segTEXTEXECB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizeTEXTEXEC;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segDATAB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeDATA;
SECURITY_READ_ONLY_LATE(vm_offset_t) segBOOTDATAB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizeBOOTDATA;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLINKB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeLINK;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segKLDB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizeKLD;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segKLDDATAB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeKLDDATA;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLASTB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizeLAST;
SECURITY_READ_ONLY_LATE(vm_offset_t) segLASTDATACONSTB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizeLASTDATACONST;
SECURITY_READ_ONLY_LATE(vm_offset_t) segHIBDATAB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizeHIBDATA;
SECURITY_READ_ONLY_LATE(vm_offset_t) segPRELINKTEXTB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizePRELINKTEXT;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segPLKTEXTEXECB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePLKTEXTEXEC;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segPLKDATACONSTB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePLKDATACONST;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segPRELINKDATAB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePRELINKDATA;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segPRELINKINFOB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePRELINKINFO;
SECURITY_READ_ONLY_LATE(vm_offset_t) segKCTEXTEXECB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizeKCTEXTEXEC;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segKCDATACONSTB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeKCDATACONST;
SECURITY_READ_ONLY_LATE(static vm_offset_t) segKCDATAB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeKCDATA;


// the page table pages riscv_vm_init allocates live in the free memory after the kernel
static bool riscv_vm_tables_live = false;
static bool riscv_vm_physmap_live = false;

vm_offset_t
riscv_vm_alloc_ptpage(void)
{
	if (avail_start >= avail_end) {
		panic("%s: out of memory for page tables", __func__);
	}
	pmap_paddr_t pa = avail_start;
	if (!riscv_vm_physmap_live && pa >= first_avail_phys + RISCV_BOOT_ALLOC_MARGIN) {
		panic("%s: early page tables outgrew the boot mapping", __func__);
	}
	avail_start += PAGE_SIZE;
	vm_offset_t va = phystokv(pa);
	bzero((void *)va, PAGE_SIZE);
	return va;
}

// the level 3 entry for a kernel va, making level 2 and 3 tables on the way unless only looking
static pt_entry_t *
riscv_vm_kernel_pte(vm_offset_t va, bool create)
{
	tt_entry_t *l1p = &cpu_tte[RISCV_TT_L1_INDEX(va)];
	if (!pte_is_valid(*l1p)) {
		if (!create) {
			return NULL;
		}
		if (riscv_vm_tables_live) {
			panic("%s: kernel level 1 entry for 0x%llx created after boot", __func__, (uint64_t)va);
		}
		vm_offset_t table = riscv_vm_alloc_ptpage();
		*l1p = PA_TO_PTE(kvtophys(table)) | PTE_V;
	} else if (pte_is_leaf(*l1p)) {
		panic("%s: 0x%llx is inside a gigapage", __func__, (uint64_t)va);
	}
	tt_entry_t *l2p = &((tt_entry_t *)phystokv(PTE_TO_PA(*l1p)))[RISCV_TT_L2_INDEX(va)];
	if (!pte_is_valid(*l2p)) {
		if (!create) {
			return NULL;
		}
		if (riscv_vm_tables_live) {
			// after boot the kernel pmap grows its own tables with descriptors
			if (pmap_expand_kernel(va) != KERN_SUCCESS) {
				panic("%s: can't make a table for 0x%llx", __func__, (uint64_t)va);
			}
		} else {
			vm_offset_t table = riscv_vm_alloc_ptpage();
			*l2p = PA_TO_PTE(kvtophys(table)) | PTE_V;
		}
	} else if (pte_is_leaf(*l2p)) {
		panic("%s: 0x%llx is inside a megapage", __func__, (uint64_t)va);
	}
	return &((pt_entry_t *)phystokv(PTE_TO_PA(*l2p)))[RISCV_TT_L3_INDEX(va)];
}

// maps physical [pa, pa + size) at va with the given leaf bits, a zero bits value unmaps
void
riscv_vm_map_static(vm_offset_t va, pmap_paddr_t pa, vm_size_t size, pt_entry_t bits)
{
	for (vm_size_t off = 0; off < size; off += PAGE_SIZE) {
		pt_entry_t *ptep = riscv_vm_kernel_pte(va + off, bits != 0);
		if (ptep == NULL) {
			continue;
		}
		pt_entry_t pte = bits ? (PA_TO_PTE(pa + off) | bits) : 0;
		os_atomic_store(ptep, pte, release);
		if (riscv_vm_tables_live) {
			sfence_vma_va(va + off);
		}
	}
}

// large aligned stretches of the aperture use 2MB megapages
static void
riscv_vm_map_physmap(pmap_paddr_t pa_start, pmap_paddr_t pa_end, pt_entry_t bits)
{
	pmap_paddr_t pa = pa_start;
	while (pa < pa_end) {
		vm_offset_t va = (vm_offset_t)(pa - gDramBase + PHYSMAP_BASE);
		if ((pa & (RISCV_TT_L2_SIZE - 1)) == 0 && pa + RISCV_TT_L2_SIZE <= pa_end) {
			tt_entry_t *l1p = &cpu_tte[RISCV_TT_L1_INDEX(va)];
			if (!pte_is_valid(*l1p)) {
				vm_offset_t table = riscv_vm_alloc_ptpage();
				*l1p = PA_TO_PTE(kvtophys(table)) | PTE_V;
			}
			tt_entry_t *l2p = &((tt_entry_t *)phystokv(PTE_TO_PA(*l1p)))[RISCV_TT_L2_INDEX(va)];
			*l2p = PA_TO_PTE(pa) | bits;
			pa += RISCV_TT_L2_SIZE;
		} else {
			riscv_vm_map_static(va, pa, PAGE_SIZE, bits);
			pa += PAGE_SIZE;
		}
	}
}

// picks the pte memory type encoding from the isa the device tree describes
static void
riscv_vm_detect_memtype(void)
{
	riscv_memtype_mode = RISCV_MEMTYPE_NONE;

	DTEntry cpus = NULL;
	if (SecureDTLookupEntry(NULL, "/cpus", &cpus) == kSuccess) {
		DTEntryIterator iter;
		DTEntry cpu;
		if (SecureDTInitEntryIterator(cpus, &iter) == kSuccess &&
		    SecureDTIterateEntries(&iter, &cpu) == kSuccess) {
			void const *prop = NULL;
			unsigned int size = 0;
			if (SecureDTGetProperty(cpu, "riscv,isa", &prop, &size) == kSuccess && size > 0) {
				if (strnstr((const char *)prop, "svpbmt", size) != NULL) {
					riscv_memtype_mode = RISCV_MEMTYPE_SVPBMT;
				}
			}
			if (riscv_memtype_mode == RISCV_MEMTYPE_NONE &&
			    SecureDTGetProperty(cpu, "riscv,isa-extensions", &prop, &size) == kSuccess) {
				// a list of nul separated names
				const char *name = (const char *)prop;
				for (unsigned int off = 0; off < size; off += (unsigned int)strnlen(name + off, size - off) + 1) {
					if (strncmp(name + off, "svpbmt", size - off) == 0) {
						riscv_memtype_mode = RISCV_MEMTYPE_SVPBMT;
						break;
					}
				}
			}
		}
	}

	if (riscv_memtype_mode == RISCV_MEMTYPE_NONE) {
		// t-head cores predate svpbmt and carry their own attribute bits
		struct sbiret ret = sbi_ecall(SBI_EXT_BASE, SBI_BASE_GET_MVENDORID, 0, 0, 0, 0, 0, 0);
		if (ret.error == SBI_SUCCESS && ret.value == 0x5b7) {
			riscv_memtype_mode = RISCV_MEMTYPE_THEAD;
		}
	}
}

static pt_entry_t
riscv_vm_prot_bits(vm_prot_t prot)
{
	pt_entry_t bits = PTE_V | PTE_G | PTE_A | PTE_D | pmap_wimg_to_pte(VM_WIMG_DEFAULT);
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

// how a kernel collection segment is mapped until lockdown, read only data stays writable until then
static vm_prot_t
riscv_vm_segment_prot(kernel_segment_command_t *seg)
{
	const char *segname = seg->segname;
	vm_prot_t initprot = seg->initprot;

	if (strcmp(segname, "__TEXT") == 0 || strcmp(segname, "__LINKEDIT") == 0 ||
	    strcmp(segname, "__PRELINK_INFO") == 0 || strcmp(segname, "__PLK_LINKEDIT") == 0) {
		return VM_PROT_READ;
	}
	// the kexts inside get their own protections from their entries, the gaps stay data
	if (strcmp(segname, "__PRELINK_TEXT") == 0) {
		return VM_PROT_READ | VM_PROT_WRITE;
	}
	if (initprot & VM_PROT_EXECUTE) {
		return VM_PROT_READ | VM_PROT_EXECUTE;
	}
	// ld64 leaves some code segments like __KLD at rw, their sections still say they hold code
	kernel_section_t *sect = firstsect(seg);
	for (uint32_t i = 0; i < seg->nsects; i++, sect++) {
		if (sect->flags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)) {
			return VM_PROT_READ | VM_PROT_EXECUTE;
		}
	}
	return VM_PROT_READ | VM_PROT_WRITE;
}

static void
riscv_vm_map_header_segments(kernel_mach_header_t *mh)
{
	kernel_segment_command_t *seg;
	for (seg = firstsegfromheader(mh); seg != NULL; seg = nextsegfromheader(mh, seg)) {
		if (seg->vmsize == 0) {
			continue;
		}
		vm_offset_t start = trunc_page(seg->vmaddr);
		vm_offset_t end = round_page(seg->vmaddr + seg->vmsize);
		vm_prot_t prot = riscv_vm_segment_prot(seg);
		riscv_vm_map_static(start, start - gVirtBase + gPhysBase, end - start, riscv_vm_prot_bits(prot));
	}
}

// entry headers were slid with the collection by riscv_slide_rebase_image
static void
riscv_vm_map_fileset_entries(kernel_mach_header_t *kc_mh)
{
	struct load_command *lc = (struct load_command *)((uintptr_t)kc_mh + sizeof(*kc_mh));
	for (uint32_t i = 0; i < kc_mh->ncmds; i++, lc = (struct load_command *)((uintptr_t)lc + lc->cmdsize)) {
		if (lc->cmd == LC_FILESET_ENTRY) {
			struct fileset_entry_command *fse = (struct fileset_entry_command *)lc;
			riscv_vm_map_header_segments((kernel_mach_header_t *)fse->vmaddr);
		}
	}
}

// the arm64 fileset layout, kext segments merged into the collection's own
static void
riscv_vm_find_segments(void)
{
	segPRELINKTEXTB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PRELINK_TEXT", &segSizePRELINKTEXT);
	segPLKDATACONSTB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PLK_DATA_CONST", &segSizePLKDATACONST);
	segPLKTEXTEXECB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PLK_TEXT_EXEC", &segSizePLKTEXTEXEC);
	segTEXTB         = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__TEXT", &segSizeTEXT);
	segDATACONSTB    = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__DATA_CONST", &segSizeDATACONST);
	segTEXTEXECB     = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__TEXT_EXEC", &segSizeTEXTEXEC);
	segDATAB         = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__DATA", &segSizeDATA);
	segBOOTDATAB     = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__BOOTDATA", &segSizeBOOTDATA);
	segLINKB         = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__LINKEDIT", &segSizeLINK);
	segKLDB          = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__KLD", &segSizeKLD);
	segKLDDATAB      = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__KLDDATA", &segSizeKLDDATA);
	segPRELINKDATAB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PRELINK_DATA", &segSizePRELINKDATA);
	segPRELINKINFOB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PRELINK_INFO", &segSizePRELINKINFO);
	segLASTB         = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__LAST", &segSizeLAST);
	segLASTDATACONSTB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__LASTDATA_CONST", &segSizeLASTDATACONST);
	segHIBDATAB      = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__HIBDATA", &segSizeHIBDATA);

	if (kernel_mach_header_is_in_fileset(&_mh_execute_header)) {
		kernel_mach_header_t *kc_mh = PE_get_kc_header(KCKindPrimary);

		segKCTEXTEXECB = (vm_offset_t) getsegdatafromheader(kc_mh, "__TEXT_EXEC", &segSizeKCTEXTEXEC);
		if (segTEXTEXECB == segKCTEXTEXECB) {
			segPLKTEXTEXECB = segLASTB + segSizeLAST;
			segSizePLKTEXTEXEC = segSizeKCTEXTEXEC - (segPLKTEXTEXECB - segKCTEXTEXECB);
		} else {
			segPLKTEXTEXECB = segKCTEXTEXECB;
			segSizePLKTEXTEXEC = segTEXTEXECB - segKCTEXTEXECB;
		}

		segKCDATACONSTB = (vm_offset_t) getsegdatafromheader(kc_mh, "__DATA_CONST", &segSizeKCDATACONST);
		segPLKDATACONSTB = segLASTDATACONSTB + segSizeLASTDATACONST;
		segSizePLKDATACONST = segSizeKCDATACONST - (segPLKDATACONSTB - segKCDATACONSTB);

		segKCDATAB = (vm_offset_t) getsegdatafromheader(kc_mh, "__DATA", &segSizeKCDATA);
		segSizePRELINKDATA = segSizeKCDATA - (segPRELINKDATAB - segKCDATAB);

		segPRELINKTEXTB = (vm_offset_t) getsegdatafromheader(kc_mh, "__PRELINK_TEXT", &segSizePRELINKTEXT);
		segPRELINKINFOB = (vm_offset_t) getsegdatafromheader(kc_mh, "__PRELINK_INFO", &segSizePRELINKINFO);
		segLINKB        = (vm_offset_t) getsegdatafromheader(kc_mh, "__LINKEDIT", &segSizeLINK);
	}

	etext = (vm_offset_t) segTEXTB + segSizeTEXT;
	sdata = (vm_offset_t) segDATAB;
	edata = (vm_offset_t) segDATAB + segSizeDATA;
	end_kern = round_page(segHIGHESTKC ? segHIGHESTKC : getlastkerneladdr());

	vm_kernel_base = segTEXTB;
	vm_kernel_top = (vm_offset_t) &last_kernel_symbol;
	vm_kext_base = segPRELINKTEXTB;
	if (!segSizePLKTEXTEXEC && !segSizePLKDATACONST) {
		vm_kext_top = vm_kext_base + segSizePRELINKTEXT;
		vm_prelink_etext = segPRELINKTEXTB + segSizePRELINKTEXT;
	} else {
		vm_kext_top = segPLKTEXTEXECB + segSizePLKTEXTEXEC;
		vm_prelink_etext = segPLKTEXTEXECB + segSizePLKTEXTEXEC;
	}
	vm_prelink_stext = segPRELINKTEXTB;
	vm_prelink_sinfo = segPRELINKINFOB;
	vm_prelink_einfo = segPRELINKINFOB + segSizePRELINKINFO;
	vm_slinkedit = segLINKB;
	vm_elinkedit = segLINKB + segSizeLINK;
	vm_prelink_sdata = segPRELINKDATAB;
	vm_prelink_edata = segPRELINKDATAB + segSizePRELINKDATA;

	segLOWEST = segLOWESTKC ? segLOWESTKC : segTEXTB;
	segLOWESTTEXT = segLOWEST;
	segLOWESTRO = segLOWESTROKC ? segLOWESTROKC : segTEXTB;
	segHIGHESTRO = segHIGHESTROKC ? segHIGHESTROKC : segTEXTB + segSizeTEXT;
}

vm_offset_t
ml_static_vtop(vm_offset_t va)
{
	if (va >= PHYSMAP_BASE && va < physmap_end) {
		return va - PHYSMAP_BASE + gDramBase;
	}
	if (__improbable((va < gVirtBase) || ((va - gVirtBase) >= gPhysSize))) {
		panic("%s: illegal VA: %p; virt base 0x%llx, size 0x%llx", __func__,
		    (void *)va, (unsigned long long)gVirtBase, (unsigned long long)gPhysSize);
	}
	return va - gVirtBase + gPhysBase;
}

bool
kernel_text_contains(vm_offset_t addr)
{
	if (segTEXTB <= addr && addr < (segTEXTB + segSizeTEXT)) {
		return true;
	}
	if (segTEXTEXECB <= addr && addr < (segTEXTEXECB + segSizeTEXTEXEC)) {
		return true;
	}
	return segDATACONSTB <= addr && addr < (segDATACONSTB + segSizeDATACONST);
}

void
riscv_vm_init(uint64_t memory_size, boot_args *args)
{
	// page_size and page_shift stay zero on riscv until this runs, the commpage publishes the shift
	vm_set_page_size();

	gVirtBase = args->virtBase;
	gPhysBase = args->physBase;
	gPhysSize = mem_size = ((gPhysBase + args->memSize) & ~PAGE_MASK) - gPhysBase;
	mem_actual = args->memSizeActual ? args->memSizeActual : mem_size;

	if ((memory_size != 0) && (mem_size > memory_size)) {
		mem_size = memory_size;
		max_mem_actual = memory_size;
	} else {
		max_mem_actual = mem_actual;
	}
	if (gDramSize > PHYSMAP_MAX_SIZE) {
		panic("%s: 0x%llx bytes of dram don't fit the physical aperture", __func__, gDramSize);
	}

	pmap_early_init(args->topOfKernelData);
	riscv_vm_detect_memtype();
	riscv_vm_find_segments();

	cpu_tte = (tt_entry_t *)riscv_vm_alloc_ptpage();
	cpu_ttep = kvtophys((vm_offset_t)cpu_tte);
	invalid_tte = cpu_tte;
	invalid_ttep = cpu_ttep;

	// everything the loader placed in the window, boot args and the device tree included
	vm_offset_t window_end = round_page(args->topOfKernelData - gPhysBase + gVirtBase);
	riscv_vm_map_static(gVirtBase, gPhysBase, window_end - gVirtBase, riscv_vm_prot_bits(VM_PROT_READ | VM_PROT_WRITE));

	// then the collection, and over it every fileset entry (kernel and kexts) with its own protections
	if (kernel_mach_header_is_in_fileset(&_mh_execute_header)) {
		kernel_mach_header_t *kc_mh = PE_get_kc_header(KCKindPrimary);
		riscv_vm_map_header_segments(kc_mh);
		riscv_vm_map_fileset_entries(kc_mh);
	} else {
		riscv_vm_map_header_segments(&_mh_execute_header);
	}

	// the stacks' guard pages stay unmapped
	riscv_vm_map_static((vm_offset_t)&intstack_low_guard, 0, PAGE_SIZE, 0);
	riscv_vm_map_static((vm_offset_t)&intstack_high_guard, 0, PAGE_SIZE, 0);
	riscv_vm_map_static((vm_offset_t)&excepstack_high_guard, 0, PAGE_SIZE, 0);

	// dram linear from physBase, firmware below it may be pmp protected and stays unmapped
	physmap_base = PHYSMAP_BASE;
	physmap_end = physmap_max = PHYSMAP_BASE + round_page(gDramSize);
	riscv_vm_map_physmap(trunc_page(gPhysBase), gDramBase + round_page(gDramSize),
	    riscv_vm_prot_bits(VM_PROT_READ | VM_PROT_WRITE));
	static_memory_end = physmap_end;

	// every level 2 table the kernel map will ever need, so level 1 is final from here on
	for (vm_offset_t va = KERNEL_DYN_BASE; va < KERNEL_DYN_END; va += RISCV_TT_L1_SIZE) {
		tt_entry_t *l1p = &cpu_tte[RISCV_TT_L1_INDEX(va)];
		if (!pte_is_valid(*l1p)) {
			vm_offset_t table = riscv_vm_alloc_ptpage();
			*l1p = PA_TO_PTE(kvtophys(table)) | PTE_V;
		}
	}

	// the per cpu copy windows get their table now, entries are filled in as windows open
	for (vm_offset_t va = CPUWINDOWS_BASE; va < CPUWINDOWS_TOP; va += PAGE_SIZE) {
		(void)riscv_vm_kernel_pte(va, true);
	}
	(void)riscv_vm_kernel_pte(LOWGLOBAL_ALIAS, true);
	static_assert(LOWGLOBAL_ALIAS >= CPUWINDOWS_TOP && LOWGLOBAL_ALIAS < VM_MAX_KERNEL_ADDRESS);

	// the level 1 entries of the kernel half are final, user roots copy them from here on
	csr_write(satp, SATP_MAKE(0, cpu_ttep));
	sfence_vma_all();
	pmap_set_physmap_active();
	riscv_vm_physmap_live = true;
	cpu_tte = (tt_entry_t *)phystokv(cpu_ttep);
	invalid_tte = cpu_tte;
	// kvtophys walks the kernel pmap from here on, before pmap_bootstrap fills in the rest
	kernel_pmap->tte = cpu_tte;
	kernel_pmap->ttep = cpu_ttep;

	// secondary harts come up through start_cpu straight onto these tables
	riscv_kernel_satp = SATP_MAKE(0, cpu_ttep);
	riscv_physmap_offset = PHYSMAP_BASE - gDramBase;

	vm_kernelcache_base = segLOWEST;
	vm_kernelcache_top = end_kern;
	vm_page_kernelcache_count = (unsigned int)atop_64(end_kern - segLOWEST);

	sane_size = mem_size - (avail_start - gPhysBase);
	max_mem = mem_size;
	vm_kernel_slid_base = segLOWESTTEXT;
	vm_kernel_stext = segTEXTB;
	if (kernel_mach_header_is_in_fileset(&_mh_execute_header)) {
		vm_kernel_etext = segTEXTEXECB + segSizeTEXTEXEC;
		vm_kernel_slid_top = vm_slinkedit;
	} else {
		vm_kernel_etext = segTEXTB + segSizeTEXT + segSizeDATACONST + segSizeTEXTEXEC;
		vm_kernel_slid_top = vm_prelink_einfo;
	}

	pmap_bootstrap(KERNEL_DYN_BASE);
	riscv_vm_tables_live = true;

	avail_start = round_page(avail_start);
	patch_low_glo_static_region(args->topOfKernelData, avail_start - args->topOfKernelData);
}

// kernel read only data stays writable through boot, lockdown takes the write bit away
void
riscv_vm_prot_finalize(__unused boot_args *args)
{
	vm_offset_t ranges[][2] = {
		{ segDATACONSTB, segDATACONSTB + segSizeDATACONST },
		{ segKCDATACONSTB, segKCDATACONSTB + segSizeKCDATACONST },
		{ segLASTDATACONSTB, segLASTDATACONSTB + segSizeLASTDATACONST },
	};
	for (unsigned int i = 0; i < sizeof(ranges) / sizeof(ranges[0]); i++) {
		vm_offset_t start = trunc_page(ranges[i][0]);
		vm_offset_t end = round_page(ranges[i][1]);
		if (start >= end) {
			continue;
		}
		riscv_vm_map_static(start, start - gVirtBase + gPhysBase, end - start, riscv_vm_prot_bits(VM_PROT_READ));
	}
	// every hart drops its writable copies
	if (real_ncpus > 1) {
		(void)sbi_remote_sfence_vma(0, SBI_HART_MASK_BASE_ALL, 0, (unsigned long)-1);
	} else {
		sfence_vma_all();
	}
}

void
riscv_vm_prot_init(__unused boot_args *args)
{
	// the protections were applied when riscv_vm_init built the window
}
