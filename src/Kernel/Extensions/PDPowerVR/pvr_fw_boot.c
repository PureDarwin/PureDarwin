// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// the mips firmware boot path of the imagination powervr driver: pvr_fw.c, pvr_fw_mips.c,
// pvr_vm_mips.c, pvr_fw_startstop.c, pvr_ccb.c, pvr_fw_trace.c and pvr_fw_util.c, on iokit memory

#include "pvr_port.h"
#include "pvr_rogue_fwif_dev_info.h"

#define FW_MAX_SUPPORTED_MAJOR_VERSION 1

#define FW_BOOT_TIMEOUT_USEC 5000000

#define POLL_TIMEOUT_USEC 1000000

/* Config heap occupies top 192k of the firmware heap. */
#define PVR_ROGUE_FW_CONFIG_HEAP_GRANULARITY SZ_64K
#define PVR_ROGUE_FW_CONFIG_HEAP_SIZE (3 * PVR_ROGUE_FW_CONFIG_HEAP_GRANULARITY)

/* Main firmware allocations should come from the remainder of the heap. */
#define PVR_ROGUE_FW_MAIN_HEAP_BASE ROGUE_FW_HEAP_BASE

/* Offsets from start of configuration area of FW heap. */
#define PVR_ROGUE_FWIF_CONNECTION_CTL_OFFSET 0
#define PVR_ROGUE_FWIF_OSINIT_OFFSET \
	(PVR_ROGUE_FWIF_CONNECTION_CTL_OFFSET + PVR_ROGUE_FW_CONFIG_HEAP_GRANULARITY)
#define PVR_ROGUE_FWIF_SYSINIT_OFFSET \
	(PVR_ROGUE_FWIF_OSINIT_OFFSET + PVR_ROGUE_FW_CONFIG_HEAP_GRANULARITY)

#define PVR_ROGUE_FAULT_PAGE_SIZE SZ_4K

#define PVR_SYNC_OBJ_SIZE sizeof(u32)

#define ROGUE_FW_HEAP_MIPS_BASE 0xC0000000
#define ROGUE_FW_HEAP_MIPS_SHIFT 24 /* 16 MB */
#define ROGUE_FW_HEAP_MIPS_RESERVED_SIZE SZ_1M

#define DEV_INFO_MASK_SIZE(x) (((x) + 63) / 64)

#define WRITE_ONCE(x, v) (*(volatile __typeof__(x) *)&(x) = (v))
#define READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))

// the parts of linux/elf.h the command stream walk reads
#define PT_LOAD 1

struct elf32_hdr {
	u8 e_ident[16];
	u16 e_type;
	u16 e_machine;
	u32 e_version;
	u32 e_entry;
	u32 e_phoff;
	u32 e_shoff;
	u32 e_flags;
	u16 e_ehsize;
	u16 e_phentsize;
	u16 e_phnum;
	u16 e_shentsize;
	u16 e_shnum;
	u16 e_shstrndx;
};

struct elf32_phdr {
	u32 p_type;
	u32 p_offset;
	u32 p_vaddr;
	u32 p_paddr;
	u32 p_filesz;
	u32 p_memsz;
	u32 p_flags;
	u32 p_align;
};

int
pvr_cr_poll_reg32(struct pvr_device *pvr_dev, u32 reg_addr, u32 reg_value,
		  u32 reg_mask, u64 timeout_usec)
{
	u64 deadline = pd_pvr_now_us() + timeout_usec;

	for (;;) {
		if ((pvr_cr_read32(pvr_dev, reg_addr) & reg_mask) == reg_value)
			return 0;
		if (pd_pvr_now_us() > deadline)
			return -ETIMEDOUT;
		pd_pvr_udelay(1);
	}
}

int
pvr_cr_poll_reg64(struct pvr_device *pvr_dev, u32 reg_addr, u64 reg_value,
		  u64 reg_mask, u64 timeout_usec)
{
	u64 deadline = pd_pvr_now_us() + timeout_usec;

	for (;;) {
		if ((pvr_cr_read64(pvr_dev, reg_addr) & reg_mask) == reg_value)
			return 0;
		if (pd_pvr_now_us() > deadline)
			return -ETIMEDOUT;
		pd_pvr_udelay(1);
	}
}

void
pvr_load_gpu_id(struct pvr_device *pvr_dev)
{
	struct pvr_gpu_id *gpu_id = &pvr_dev->gpu_id;
	u64 bvnc;

	/*
	 * Try reading the BVNC using the newer (cleaner) method first. If the
	 * B value is zero, fall back to the older method.
	 */
	bvnc = pvr_cr_read64(pvr_dev, ROGUE_CR_CORE_ID__PBVNC);

	gpu_id->b = PVR_CR_FIELD_GET(bvnc, CORE_ID__PBVNC__BRANCH_ID);
	if (gpu_id->b != 0) {
		gpu_id->v = PVR_CR_FIELD_GET(bvnc, CORE_ID__PBVNC__VERSION_ID);
		gpu_id->n = PVR_CR_FIELD_GET(bvnc, CORE_ID__PBVNC__NUMBER_OF_SCALABLE_UNITS);
		gpu_id->c = PVR_CR_FIELD_GET(bvnc, CORE_ID__PBVNC__CONFIG_ID);
	} else {
		u32 core_rev = pvr_cr_read32(pvr_dev, ROGUE_CR_CORE_REVISION);
		u32 core_id = pvr_cr_read32(pvr_dev, ROGUE_CR_CORE_ID);
		u16 core_id_config = PVR_CR_FIELD_GET(core_id, CORE_ID_CONFIG);

		gpu_id->b = PVR_CR_FIELD_GET(core_rev, CORE_REVISION_MAJOR);
		gpu_id->v = PVR_CR_FIELD_GET(core_rev, CORE_REVISION_MINOR);
		gpu_id->n = (core_id_config & 0xFF00) >> 8;
		gpu_id->c = core_id_config & 0x00FF;
	}

	pd_pvr_log("BVNC %u.%u.%u.%u (raw pbvnc 0x%016llx)\n",
		   gpu_id->b, gpu_id->v, gpu_id->n, gpu_id->c, bvnc);
}

static const struct pvr_fw_layout_entry *
pvr_fw_find_layout_entry(struct pvr_device *pvr_dev, enum pvr_fw_section_id id)
{
	const struct pvr_fw_layout_entry *layout_entries = pvr_dev->fw_dev.layout_entries;
	u32 num_layout_entries = pvr_dev->fw_dev.header->layout_entry_num;

	for (u32 entry = 0; entry < num_layout_entries; entry++) {
		if (layout_entries[entry].id == id)
			return &layout_entries[entry];
	}

	return NULL;
}

static const struct pvr_fw_layout_entry *
pvr_fw_find_private_data(struct pvr_device *pvr_dev)
{
	const struct pvr_fw_layout_entry *layout_entries = pvr_dev->fw_dev.layout_entries;
	u32 num_layout_entries = pvr_dev->fw_dev.header->layout_entry_num;

	for (u32 entry = 0; entry < num_layout_entries; entry++) {
		if (layout_entries[entry].id == META_PRIVATE_DATA ||
		    layout_entries[entry].id == MIPS_PRIVATE_DATA ||
		    layout_entries[entry].id == RISCV_PRIVATE_DATA)
			return &layout_entries[entry];
	}

	return NULL;
}

static int
pvr_fw_validate(struct pvr_device *pvr_dev)
{
	const struct pvr_fw_layout_entry *layout_entries;
	const struct pvr_fw_info_header *header;
	const u8 *fw = pvr_dev->fw_dev.image;
	u32 fw_size = pvr_dev->fw_dev.image_size;
	u32 fw_offset = fw_size - SZ_4K;
	u32 layout_table_size, entry;

	if (fw_size < SZ_4K || (fw_size % FW_BLOCK_SIZE))
		return -EINVAL;

	header = (const struct pvr_fw_info_header *)&fw[fw_offset];

	if (header->info_version != PVR_FW_INFO_VERSION) {
		pd_pvr_log("unsupported fw info version %u\n", header->info_version);
		return -EINVAL;
	}

	if (header->header_len != sizeof(struct pvr_fw_info_header) ||
	    header->layout_entry_size != sizeof(struct pvr_fw_layout_entry) ||
	    header->layout_entry_num > PVR_FW_INFO_MAX_NUM_ENTRIES) {
		pd_pvr_log("fw info format mismatch\n");
		return -EINVAL;
	}

	if (!(header->flags & PVR_FW_FLAGS_OPEN_SOURCE) ||
	    header->fw_version_major > FW_MAX_SUPPORTED_MAJOR_VERSION ||
	    header->fw_version_major == 0) {
		pd_pvr_log("unsupported fw version %u.%u (build: %u%s)\n",
			   header->fw_version_major, header->fw_version_minor,
			   header->fw_version_build,
			   (header->flags & PVR_FW_FLAGS_OPEN_SOURCE) ? " OS" : "");
		return -EINVAL;
	}

	if (pvr_gpu_id_to_packed_bvnc(&pvr_dev->gpu_id) != header->bvnc) {
		struct pvr_gpu_id fw_gpu_id;

		packed_bvnc_to_pvr_gpu_id(header->bvnc, &fw_gpu_id);
		pd_pvr_log("fw built for gpu id %u.%u.%u.%u, this is %u.%u.%u.%u\n",
			   fw_gpu_id.b, fw_gpu_id.v, fw_gpu_id.n, fw_gpu_id.c,
			   pvr_dev->gpu_id.b, pvr_dev->gpu_id.v, pvr_dev->gpu_id.n,
			   pvr_dev->gpu_id.c);
		return -EINVAL;
	}

	fw_offset += header->header_len;
	layout_table_size = header->layout_entry_size * header->layout_entry_num;
	if ((fw_offset + layout_table_size) > fw_size)
		return -EINVAL;

	layout_entries = (const struct pvr_fw_layout_entry *)&fw[fw_offset];
	for (entry = 0; entry < header->layout_entry_num; entry++) {
		u32 start_addr = layout_entries[entry].base_addr;
		u32 end_addr = start_addr + layout_entries[entry].alloc_size;

		if (start_addr >= end_addr)
			return -EINVAL;
	}

	pd_pvr_log("fw version v%u.%u (build %u OS)\n", header->fw_version_major,
		   header->fw_version_minor, header->fw_version_build);

	pvr_dev->fw_version.major = header->fw_version_major;
	pvr_dev->fw_version.minor = header->fw_version_minor;

	pvr_dev->fw_dev.header = header;
	pvr_dev->fw_dev.layout_entries = layout_entries;

	return 0;
}

static int
pvr_fw_get_device_info(struct pvr_device *pvr_dev)
{
	const u8 *fw = pvr_dev->fw_dev.image;
	const struct pvr_fw_device_info_header *header;
	const u64 *dev_info;
	u32 fw_offset;

	fw_offset = (pvr_dev->fw_dev.image_size - SZ_4K) - pvr_dev->fw_dev.header->device_info_size;

	header = (const struct pvr_fw_device_info_header *)&fw[fw_offset];
	dev_info = (const u64 *)(header + 1);

	pvr_device_info_set_quirks(pvr_dev, dev_info, header->brn_mask_size);
	dev_info += header->brn_mask_size;

	pvr_device_info_set_enhancements(pvr_dev, dev_info, header->ern_mask_size);
	dev_info += header->ern_mask_size;

	return pvr_device_info_set_features(pvr_dev, dev_info, header->feature_mask_size,
					    header->feature_param_size);
}

int
pvr_fw_validate_init_device_info(struct pvr_device *pvr_dev)
{
	int err;

	err = pvr_fw_validate(pvr_dev);
	if (err)
		return err;

	return pvr_fw_get_device_info(pvr_dev);
}

static void
layout_get_sizes(struct pvr_device *pvr_dev)
{
	const struct pvr_fw_layout_entry *layout_entries = pvr_dev->fw_dev.layout_entries;
	u32 num_layout_entries = pvr_dev->fw_dev.header->layout_entry_num;
	struct pvr_fw_mem *fw_mem = &pvr_dev->fw_dev.mem;

	fw_mem->code_alloc_size = 0;
	fw_mem->data_alloc_size = 0;
	fw_mem->core_code_alloc_size = 0;
	fw_mem->core_data_alloc_size = 0;

	/* Extract section sizes from FW layout table. */
	for (u32 entry = 0; entry < num_layout_entries; entry++) {
		switch (layout_entries[entry].type) {
		case FW_CODE:
			fw_mem->code_alloc_size += layout_entries[entry].alloc_size;
			break;
		case FW_DATA:
			fw_mem->data_alloc_size += layout_entries[entry].alloc_size;
			break;
		case FW_COREMEM_CODE:
			fw_mem->core_code_alloc_size += layout_entries[entry].alloc_size;
			break;
		case FW_COREMEM_DATA:
			fw_mem->core_data_alloc_size += layout_entries[entry].alloc_size;
			break;
		case NONE:
			break;
		}
	}
}

static int
pvr_fw_find_mmu_segment(struct pvr_device *pvr_dev, u32 addr, u32 size, void *fw_code_ptr,
			void *fw_data_ptr, void *fw_core_code_ptr, void *fw_core_data_ptr,
			void **host_addr_out)
{
	const struct pvr_fw_layout_entry *layout_entries = pvr_dev->fw_dev.layout_entries;
	u32 num_layout_entries = pvr_dev->fw_dev.header->layout_entry_num;
	u32 end_addr = addr + size;

	/* Ensure requested range is not zero, and size is not causing addr to overflow. */
	if (end_addr <= addr)
		return -EINVAL;

	for (u32 entry = 0; entry < num_layout_entries; entry++) {
		u32 entry_start_addr = layout_entries[entry].base_addr;
		u32 entry_end_addr = entry_start_addr + layout_entries[entry].alloc_size;

		if (addr >= entry_start_addr && addr < entry_end_addr &&
		    end_addr > entry_start_addr && end_addr <= entry_end_addr) {
			switch (layout_entries[entry].type) {
			case FW_CODE:
				*host_addr_out = fw_code_ptr;
				break;

			case FW_DATA:
				*host_addr_out = fw_data_ptr;
				break;

			case FW_COREMEM_CODE:
				*host_addr_out = fw_core_code_ptr;
				break;

			case FW_COREMEM_DATA:
				*host_addr_out = fw_core_data_ptr;
				break;

			default:
				return -EINVAL;
			}
			/* Direct Mem write to mapped memory */
			addr -= layout_entries[entry].base_addr;
			addr += layout_entries[entry].alloc_offset;

			/*
			 * Add offset to pointer to FW allocation only if that
			 * allocation is available
			 */
			if (*host_addr_out)
				*(u8 **)host_addr_out += addr;
			return 0;
		}
	}

	return -EINVAL;
}

static int
pvr_fw_process_elf_command_stream(struct pvr_device *pvr_dev, const u8 *fw,
				  u8 *fw_code_ptr, u8 *fw_data_ptr,
				  u8 *fw_core_code_ptr, u8 *fw_core_data_ptr)
{
	const struct elf32_hdr *header = (const struct elf32_hdr *)fw;
	const struct elf32_phdr *program_header = (const struct elf32_phdr *)(fw + header->e_phoff);
	int err;

	for (u32 entry = 0; entry < header->e_phnum; entry++, program_header++) {
		void *write_addr;

		/* Only consider loadable entries in the ELF segment table */
		if (program_header->p_type != PT_LOAD)
			continue;

		err = pvr_fw_find_mmu_segment(pvr_dev, program_header->p_vaddr,
					      program_header->p_memsz, fw_code_ptr, fw_data_ptr,
					      fw_core_code_ptr, fw_core_data_ptr, &write_addr);
		if (err) {
			pd_pvr_log("fw addr 0x%x (size: %u) not found in any firmware segment\n",
				   program_header->p_vaddr, program_header->p_memsz);
			return err;
		}

		/* Write to FW allocation only if available */
		if (write_addr) {
			__builtin_memcpy(write_addr, fw + program_header->p_offset,
					 program_header->p_filesz);

			__builtin_memset((u8 *)write_addr + program_header->p_filesz, 0,
					 program_header->p_memsz - program_header->p_filesz);
		}
	}

	return 0;
}

static void
pvr_fw_heap_info_init(struct pvr_device *pvr_dev, u32 log2_size, u32 reserved_size)
{
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;

	fw_dev->fw_heap_info.gpu_addr = PVR_ROGUE_FW_MAIN_HEAP_BASE;
	fw_dev->fw_heap_info.log2_size = log2_size;
	fw_dev->fw_heap_info.reserved_size = reserved_size;
	fw_dev->fw_heap_info.raw_size = 1 << fw_dev->fw_heap_info.log2_size;
	fw_dev->fw_heap_info.offset_mask = fw_dev->fw_heap_info.raw_size - 1;
	fw_dev->fw_heap_info.config_offset = fw_dev->fw_heap_info.raw_size -
					     PVR_ROGUE_FW_CONFIG_HEAP_SIZE;
	fw_dev->fw_heap_info.size = fw_dev->fw_heap_info.raw_size -
				    (PVR_ROGUE_FW_CONFIG_HEAP_SIZE + reserved_size);
}

// pvr_vm_mips.c: the mips page table, four 4k pages in one contiguous allocation
static int
pvr_vm_mips_init(struct pvr_device *pvr_dev)
{
	u32 pt_size = 1 << (pvr_dev->fw_dev.fw_heap_info.log2_size - ROGUE_MIPSFW_LOG2_PAGE_SIZE_4K +
			    ROGUE_MIPSFW_LOG2_PTE_ENTRY_SIZE);
	struct pvr_fw_mips_data *mips_data = &pvr_dev->fw_dev.mips_data;
	struct pvr_fw_object *pt = &pvr_dev->fw_dev.objs[0];
	u32 phys_bus_width;

	/* Page table size must be at most ROGUE_MIPSFW_MAX_NUM_PAGETABLE_PAGES * 4k pages. */
	if (pt_size > ROGUE_MIPSFW_MAX_NUM_PAGETABLE_PAGES * SZ_4K)
		return -EINVAL;

	if (PVR_FEATURE_VALUE(pvr_dev, phys_bus_width, &phys_bus_width))
		return -EINVAL;

	if (pt->in_use)
		return -EINVAL;
	if (pd_pvr_alloc(ROGUE_MIPSFW_MAX_NUM_PAGETABLE_PAGES * SZ_4K, &pt->phys, &pt->cpu,
			 &pt->bmd, &pt->map))
		return -ENOMEM;
	pt->size = ROGUE_MIPSFW_MAX_NUM_PAGETABLE_PAGES * SZ_4K;
	pt->pvr_dev = pvr_dev;
	pt->in_use = true;
	pvr_dev->fw_dev.num_objs = 1;

	mips_data->pt_obj = pt;
	mips_data->pt = (u32 *)pt->cpu;
	for (u32 page_nr = 0; page_nr < PVR_MIPS_PT_PAGE_COUNT; page_nr++)
		mips_data->pt_dma_addr[page_nr] = pt->phys + page_nr * SZ_4K;

	mips_data->pfn_mask = (phys_bus_width > 32) ? ROGUE_MIPSFW_ENTRYLO_PFN_MASK_ABOVE_32BIT :
						      ROGUE_MIPSFW_ENTRYLO_PFN_MASK;

	mips_data->cache_policy = (phys_bus_width > 32) ? ROGUE_MIPSFW_CACHED_POLICY_ABOVE_32BIT :
							  ROGUE_MIPSFW_CACHED_POLICY;

	return 0;
}

static u32
get_mips_pte_flags(bool read, bool write, u32 cache_policy)
{
	u32 flags = 0;

	if (read && write) /* Read/write. */
		flags |= ROGUE_MIPSFW_ENTRYLO_DIRTY_EN;
	else if (write)    /* Write only. */
		flags |= ROGUE_MIPSFW_ENTRYLO_READ_INHIBIT_EN;

	flags |= cache_policy << ROGUE_MIPSFW_ENTRYLO_CACHE_POLICY_SHIFT;

	flags |= ROGUE_MIPSFW_ENTRYLO_VALID_EN | ROGUE_MIPSFW_ENTRYLO_GLOBAL_EN;

	return flags;
}

static int
pvr_vm_mips_map(struct pvr_device *pvr_dev, struct pvr_fw_object *fw_obj)
{
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;
	struct pvr_fw_mips_data *mips_data = &fw_dev->mips_data;
	const u64 start = fw_obj->fw_start;
	const u64 size = fw_obj->size;
	u64 end = start + size - 1;
	u32 cache_policy;
	u32 pte_flags;
	s32 start_pfn;
	s32 end_pfn;
	s32 pfn;

	if (start < ROGUE_FW_HEAP_BASE ||
	    start >= ROGUE_FW_HEAP_BASE + fw_dev->fw_heap_info.raw_size ||
	    end < ROGUE_FW_HEAP_BASE ||
	    end >= ROGUE_FW_HEAP_BASE + fw_dev->fw_heap_info.raw_size ||
	    (start & ROGUE_MIPSFW_PAGE_MASK_4K) ||
	    ((end + 1) & ROGUE_MIPSFW_PAGE_MASK_4K))
		return -EINVAL;

	start_pfn = (start & fw_dev->fw_heap_info.offset_mask) >> ROGUE_MIPSFW_LOG2_PAGE_SIZE_4K;
	end_pfn = (end & fw_dev->fw_heap_info.offset_mask) >> ROGUE_MIPSFW_LOG2_PAGE_SIZE_4K;

	if (fw_obj->flags & PVR_BO_FW_FLAGS_DEVICE_UNCACHED)
		cache_policy = ROGUE_MIPSFW_UNCACHED_CACHE_POLICY;
	else
		cache_policy = mips_data->cache_policy;

	pte_flags = get_mips_pte_flags(true, true, cache_policy);

	for (pfn = start_pfn; pfn <= end_pfn; pfn++) {
		u64 dma_addr = fw_obj->phys + ((u64)(pfn - start_pfn) << ROGUE_MIPSFW_LOG2_PAGE_SIZE_4K);
		u32 pte;

		pte = ((dma_addr >> ROGUE_MIPSFW_LOG2_PAGE_SIZE_4K)
		       << ROGUE_MIPSFW_ENTRYLO_PFN_SHIFT) & mips_data->pfn_mask;
		pte |= pte_flags;

		WRITE_ONCE(mips_data->pt[pfn], pte);
	}

	return 0;
}

// the heap allocator: a range taken at a fixed address, or the first gap in the main heap
static bool
fw_heap_overlaps(struct pvr_fw_device *fw_dev, u64 start, u32 size)
{
	for (u32 i = 0; i < fw_dev->num_objs; i++) {
		struct pvr_fw_object *o = &fw_dev->objs[i];

		if (!o->in_use || o->fw_start == 0)
			continue;
		if (start < o->fw_start + o->size && o->fw_start < start + size)
			return true;
	}
	return false;
}

static int
fw_heap_place(struct pvr_fw_device *fw_dev, u64 dev_addr, u32 size, u64 *start_out)
{
	u64 heap_end = fw_dev->fw_heap_info.gpu_addr + fw_dev->fw_heap_info.size;
	u64 cand;
	bool moved;

	if (dev_addr) {
		if (dev_addr < fw_dev->fw_mm_base ||
		    dev_addr + size > fw_dev->fw_mm_base + fw_dev->fw_heap_info.raw_size ||
		    fw_heap_overlaps(fw_dev, dev_addr, size))
			return -EINVAL;
		*start_out = dev_addr;
		return 0;
	}

	cand = fw_dev->fw_heap_info.gpu_addr;
	do {
		moved = false;
		for (u32 i = 0; i < fw_dev->num_objs; i++) {
			struct pvr_fw_object *o = &fw_dev->objs[i];

			if (!o->in_use || o->fw_start == 0)
				continue;
			if (cand < o->fw_start + o->size && o->fw_start < cand + size) {
				cand = o->fw_start + o->size;
				moved = true;
			}
		}
	} while (moved);

	if (cand + size > heap_end)
		return -ENOSPC;
	*start_out = cand;
	return 0;
}

// each owner (call site and size) of the slots in use, once, when the table first fills
static void
fw_slots_dump(struct pvr_fw_device *fw_dev)
{
	static bool dumped;
	static bool counted[PVR_FW_MAX_OBJECTS];

	if (dumped)
		return;
	dumped = true;
	pd_pvr_log("fw slots full, owners as call site offset from pvr_fw_object_create:\n");
	for (u32 i = 0; i < fw_dev->num_objs; i++) {
		struct pvr_fw_object *o = &fw_dev->objs[i];
		u32 n = 0;

		if (!o->in_use || counted[i])
			continue;
		for (u32 j = i; j < fw_dev->num_objs; j++) {
			struct pvr_fw_object *p = &fw_dev->objs[j];

			if (p->in_use && !counted[j] && p->caller == o->caller && p->size == o->size) {
				counted[j] = true;
				n++;
			}
		}
		pd_pvr_log("  %u objects of 0x%x bytes from %+lld init %+lld\n", n, o->size,
			   (long long)((uintptr_t)o->caller - (uintptr_t)pvr_fw_object_create),
			   o->init ? (long long)((uintptr_t)o->init - (uintptr_t)pvr_fw_object_create) : 0LL);
	}
}

// slots in use, logged at every 128 more than the last report
static void
fw_slots_growth(struct pvr_fw_device *fw_dev)
{
	static u32 reported;
	u32 used = 0;

	for (u32 i = 0; i < fw_dev->num_objs; i++)
		used += fw_dev->objs[i].in_use;
	if (used >= reported + 128) {
		reported = used;
		pd_pvr_log("fw slots in use %u, hwrt datasets created %u destroy ioctls %u released %u, jobs created %u released %u, "
			   "irqs %u, irq processing started %u finished %u\n",
			   used, pd_diag_hwrt_created, pd_diag_hwrt_destroyed, pd_diag_hwrt_released,
			   pd_diag_jobs_created, pd_diag_jobs_released,
			   container_of(fw_dev, struct pvr_device, fw_dev)->irq_count,
			   pd_diag_process_started, pd_diag_process_finished);
	}
}

static void *
pvr_fw_object_create_and_map_common(struct pvr_device *pvr_dev, size_t size,
				    u32 flags, u64 dev_addr,
				    void (*init)(void *cpu_ptr, void *priv),
				    void *init_priv, struct pvr_fw_object **fw_obj_out, void *caller)
{
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;
	struct pvr_fw_object *fw_obj;
	u32 alloc_size = ALIGN((u32)size, (u32)SZ_4K);
	u64 start;
	int err;

	err = fw_heap_place(fw_dev, dev_addr, alloc_size, &start);
	if (err) {
		pd_pvr_log("no fw heap room for 0x%x bytes at 0x%llx (%d)\n", alloc_size, dev_addr, err);
		return NULL;
	}

	fw_obj = NULL;
	for (u32 i = 0; i < PVR_FW_MAX_OBJECTS; i++) {
		if (!fw_dev->objs[i].in_use) {
			fw_obj = &fw_dev->objs[i];
			if (i >= fw_dev->num_objs)
				fw_dev->num_objs = i + 1;
			break;
		}
	}
	if (fw_obj == NULL) {
		pd_pvr_log("out of fw object slots\n");
		if (pd_pvr_slot_diag())
			fw_slots_dump(fw_dev);
		return NULL;
	}
	__builtin_memset(fw_obj, 0, sizeof(*fw_obj));
	if (pd_pvr_alloc(alloc_size, &fw_obj->phys, &fw_obj->cpu, &fw_obj->bmd, &fw_obj->map))
		return NULL;
	fw_obj->in_use = true;
	fw_obj->pvr_dev = pvr_dev;
	fw_obj->size = alloc_size;
	fw_obj->flags = flags;
	fw_obj->init = init;
	fw_obj->init_priv = init_priv;
	fw_obj->fw_start = start;
	fw_obj->caller = caller;

	/* Map object on GPU. */
	err = pvr_vm_mips_map(pvr_dev, fw_obj);
	if (err) {
		pd_pvr_log("mips map of 0x%llx failed (%d)\n", start, err);
		return NULL;
	}

	fw_obj->fw_addr_offset = (u32)(fw_obj->fw_start - fw_dev->fw_mm_base);

	*fw_obj_out = fw_obj;
	if (pd_pvr_slot_diag())
		fw_slots_growth(fw_dev);

	if (fw_obj->init)
		fw_obj->init(fw_obj->cpu, fw_obj->init_priv);

	return fw_obj->cpu;
}

__attribute__((noinline)) void *
pvr_fw_object_create_and_map(struct pvr_device *pvr_dev, size_t size, u32 flags,
			     void (*init)(void *cpu_ptr, void *priv),
			     void *init_priv, struct pvr_fw_object **fw_obj_out)
{
	return pvr_fw_object_create_and_map_common(pvr_dev, size, flags, 0, init, init_priv,
						   fw_obj_out, __builtin_return_address(0));
}

__attribute__((noinline)) static void *
pvr_fw_object_create_and_map_offset(struct pvr_device *pvr_dev,
				    u32 dev_offset, size_t size, u32 flags,
				    void (*init)(void *cpu_ptr, void *priv),
				    void *init_priv, struct pvr_fw_object **fw_obj_out)
{
	u64 dev_addr = pvr_dev->fw_dev.fw_mm_base + dev_offset;

	return pvr_fw_object_create_and_map_common(pvr_dev, size, flags, dev_addr, init, init_priv,
						   fw_obj_out, __builtin_return_address(0));
}

__attribute__((noinline)) int
pvr_fw_object_create(struct pvr_device *pvr_dev, size_t size, u32 flags,
		     void (*init)(void *cpu_ptr, void *priv), void *init_priv,
		     struct pvr_fw_object **fw_obj_out)
{
	return pvr_fw_object_create_and_map_common(pvr_dev, size, flags, 0, init, init_priv, fw_obj_out,
						   __builtin_return_address(0)) ? 0 : -ENOMEM;
}

// pvr_mips_get_fw_addr_with_offset(): mips cacheability is determined by page table
void
pvr_fw_object_get_fw_addr_offset(struct pvr_fw_object *fw_obj, u32 offset, u32 *fw_addr_out)
{
	struct pvr_device *pvr_dev = fw_obj->pvr_dev;

	*fw_addr_out = ((fw_obj->fw_addr_offset + offset) & pvr_dev->fw_dev.fw_heap_info.offset_mask) |
		       ROGUE_FW_HEAP_MIPS_BASE;
}

u64
pvr_fw_obj_get_gpu_addr(struct pvr_fw_object *fw_obj)
{
	return fw_obj->pvr_dev->fw_dev.fw_heap_info.gpu_addr + fw_obj->fw_addr_offset;
}

// pvr_vm_mips_unmap() and the release of the heap range and memory
void
pvr_fw_object_destroy(struct pvr_fw_object *fw_obj)
{
	struct pvr_device *pvr_dev;
	struct pvr_fw_device *fw_dev;

	if (fw_obj == NULL || !fw_obj->in_use)
		return;
	pvr_dev = fw_obj->pvr_dev;
	fw_dev = &pvr_dev->fw_dev;
	if (fw_obj->fw_start != 0) {
		u32 start_pfn = (fw_obj->fw_start & fw_dev->fw_heap_info.offset_mask) >>
				ROGUE_MIPSFW_LOG2_PAGE_SIZE_4K;
		u32 end_pfn = start_pfn + (fw_obj->size >> ROGUE_MIPSFW_LOG2_PAGE_SIZE_4K);

		for (u32 pfn = start_pfn; pfn < end_pfn; pfn++)
			WRITE_ONCE(fw_dev->mips_data.pt[pfn], 0);
		pvr_mmu_flush_request_all(pvr_dev);
		pvr_mmu_flush_exec(pvr_dev, true);
	}
	pd_pvr_free(fw_obj->bmd, fw_obj->map);
	__builtin_memset(fw_obj, 0, sizeof(*fw_obj));
}

static int
pvr_mips_fw_process(struct pvr_device *pvr_dev, const u8 *fw,
		    u8 *fw_code_ptr, u8 *fw_data_ptr, u8 *fw_core_code_ptr, u8 *fw_core_data_ptr)
{
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;
	struct pvr_fw_mips_data *mips_data = &fw_dev->mips_data;
	const struct pvr_fw_layout_entry *boot_code_entry;
	const struct pvr_fw_layout_entry *boot_data_entry;
	const struct pvr_fw_layout_entry *exception_code_entry;
	const struct pvr_fw_layout_entry *stack_entry;
	struct rogue_mipsfw_boot_data *boot_data;
	int err;

	err = pvr_fw_process_elf_command_stream(pvr_dev, fw, fw_code_ptr, fw_data_ptr,
						fw_core_code_ptr, fw_core_data_ptr);
	if (err)
		return err;

	boot_code_entry = pvr_fw_find_layout_entry(pvr_dev, MIPS_BOOT_CODE);
	boot_data_entry = pvr_fw_find_layout_entry(pvr_dev, MIPS_BOOT_DATA);
	exception_code_entry = pvr_fw_find_layout_entry(pvr_dev, MIPS_EXCEPTIONS_CODE);
	if (!boot_code_entry || !boot_data_entry || !exception_code_entry)
		return -EINVAL;

	mips_data->boot_code_dma_addr = fw_dev->mem.code_obj->phys + boot_code_entry->alloc_offset;
	mips_data->boot_data_dma_addr = fw_dev->mem.data_obj->phys + boot_data_entry->alloc_offset;
	mips_data->exception_code_dma_addr = fw_dev->mem.code_obj->phys +
					     exception_code_entry->alloc_offset;

	stack_entry = pvr_fw_find_layout_entry(pvr_dev, MIPS_STACK);
	if (!stack_entry)
		return -EINVAL;

	boot_data = (struct rogue_mipsfw_boot_data *)(fw_data_ptr + boot_data_entry->alloc_offset +
						      ROGUE_MIPSFW_BOOTLDR_CONF_OFFSET);

	boot_data->stack_phys_addr = fw_dev->mem.data_obj->phys + stack_entry->alloc_offset;

	boot_data->reg_base = pvr_dev->regs_phys;

	// the host page is 4k-aligned contiguous memory, so each mips page is just the next 4k
	for (u32 page_nr = 0; page_nr < ROGUE_MIPSFW_MAX_NUM_PAGETABLE_PAGES; page_nr++)
		boot_data->pt_phys_addr[page_nr] = mips_data->pt_dma_addr[page_nr];

	boot_data->pt_log2_page_size = ROGUE_MIPSFW_LOG2_PAGE_SIZE_4K;
	boot_data->pt_num_pages = ROGUE_MIPSFW_MAX_NUM_PAGETABLE_PAGES;
	boot_data->reserved1 = 0;
	boot_data->reserved2 = 0;

	pd_pvr_log("mips boot code 0x%llx, boot data 0x%llx, exceptions 0x%llx, stack 0x%llx, pt 0x%llx\n",
		   mips_data->boot_code_dma_addr, mips_data->boot_data_dma_addr,
		   mips_data->exception_code_dma_addr, (u64)boot_data->stack_phys_addr,
		   mips_data->pt_dma_addr[0]);

	return 0;
}

static int
pvr_mips_wrapper_init(struct pvr_device *pvr_dev)
{
	struct pvr_fw_mips_data *mips_data = &pvr_dev->fw_dev.mips_data;
	const u64 remap_settings = ROGUE_MIPSFW_BOOT_REMAP_LOG2_SEGMENT_SIZE;
	u32 phys_bus_width;

	int err = PVR_FEATURE_VALUE(pvr_dev, phys_bus_width, &phys_bus_width);

	if (err)
		return err;

	/* Currently MIPS FW only supported with physical bus width > 32 bits. */
	if (phys_bus_width <= 32)
		return -EINVAL;

	pvr_cr_write32(pvr_dev, ROGUE_CR_MIPS_WRAPPER_CONFIG,
		       (ROGUE_MIPSFW_REGISTERS_VIRTUAL_BASE >>
			ROGUE_MIPSFW_WRAPPER_CONFIG_REGBANK_ADDR_ALIGN) |
		       ROGUE_CR_MIPS_WRAPPER_CONFIG_BOOT_ISA_MODE_MICROMIPS);

	/* Configure remap for boot code, boot data and exceptions code areas. */
	pvr_cr_write64(pvr_dev, ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG1,
		       ROGUE_MIPSFW_BOOT_REMAP_PHYS_ADDR_IN |
		       ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG1_MODE_ENABLE_EN);
	pvr_cr_write64(pvr_dev, ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG2,
		       (mips_data->boot_code_dma_addr &
			~ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG2_ADDR_OUT_CLRMSK) | remap_settings);

	if (PVR_HAS_QUIRK(pvr_dev, 63553)) {
		/*
		 * WA always required on 36 bit cores, to avoid continuous unmapped memory accesses
		 * to address 0x0.
		 */
		pvr_cr_write64(pvr_dev, ROGUE_CR_MIPS_ADDR_REMAP5_CONFIG1,
			       ROGUE_CR_MIPS_ADDR_REMAP5_CONFIG1_MODE_ENABLE_EN);
		pvr_cr_write64(pvr_dev, ROGUE_CR_MIPS_ADDR_REMAP5_CONFIG2,
			       (mips_data->boot_code_dma_addr &
				~ROGUE_CR_MIPS_ADDR_REMAP5_CONFIG2_ADDR_OUT_CLRMSK) |
			       remap_settings);
	}

	pvr_cr_write64(pvr_dev, ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG1,
		       ROGUE_MIPSFW_DATA_REMAP_PHYS_ADDR_IN |
		       ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG1_MODE_ENABLE_EN);
	pvr_cr_write64(pvr_dev, ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG2,
		       (mips_data->boot_data_dma_addr &
			~ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG2_ADDR_OUT_CLRMSK) | remap_settings);

	pvr_cr_write64(pvr_dev, ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG1,
		       ROGUE_MIPSFW_CODE_REMAP_PHYS_ADDR_IN |
		       ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG1_MODE_ENABLE_EN);
	pvr_cr_write64(pvr_dev, ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG2,
		       (mips_data->exception_code_dma_addr &
			~ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG2_ADDR_OUT_CLRMSK) | remap_settings);

	/* Garten IDLE bit controlled by MIPS. */
	pvr_cr_write64(pvr_dev, ROGUE_CR_MTS_GARTEN_WRAPPER_CONFIG,
		       ROGUE_CR_MTS_GARTEN_WRAPPER_CONFIG_IDLE_CTRL_META);

	/* Turn on the EJTAG probe. */
	pvr_cr_write32(pvr_dev, ROGUE_CR_MIPS_DEBUG_CONFIG, 0);

	return 0;
}

static int
pvr_fw_process(struct pvr_device *pvr_dev)
{
	struct pvr_fw_mem *fw_mem = &pvr_dev->fw_dev.mem;
	const u8 *fw = pvr_dev->fw_dev.image;
	const struct pvr_fw_layout_entry *private_data;
	u8 *fw_code_ptr;
	u8 *fw_data_ptr;
	u8 *fw_core_code_ptr = NULL;
	u8 *fw_core_data_ptr = NULL;
	u32 base_addr;
	int err;

	layout_get_sizes(pvr_dev);

	private_data = pvr_fw_find_private_data(pvr_dev);
	if (!private_data)
		return -EINVAL;

	/*
	 * Code allocation must be at the start of the firmware heap, otherwise
	 * firmware processor will be unable to boot.
	 *
	 * This has the useful side-effect that for every other object in the
	 * driver, a firmware address of 0 is invalid.
	 */
	fw_code_ptr = pvr_fw_object_create_and_map_offset(pvr_dev, 0, fw_mem->code_alloc_size,
							  PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
							  NULL, NULL, &fw_mem->code_obj);
	if (!fw_code_ptr) {
		pd_pvr_log("unable to allocate fw code memory\n");
		return -ENOMEM;
	}

	// mips has a fixed data address
	base_addr = private_data->base_addr & pvr_dev->fw_dev.fw_heap_info.offset_mask;
	fw_data_ptr = pvr_fw_object_create_and_map_offset(pvr_dev, base_addr,
							  fw_mem->data_alloc_size,
							  PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
							  NULL, NULL, &fw_mem->data_obj);
	if (!fw_data_ptr) {
		pd_pvr_log("unable to allocate fw data memory\n");
		return -ENOMEM;
	}

	/* Core code and data sections are optional. */
	if (fw_mem->core_code_alloc_size) {
		fw_core_code_ptr =
			pvr_fw_object_create_and_map(pvr_dev, fw_mem->core_code_alloc_size,
						     PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
						     NULL, NULL, &fw_mem->core_code_obj);
		if (!fw_core_code_ptr)
			return -ENOMEM;
	}

	if (fw_mem->core_data_alloc_size) {
		fw_core_data_ptr =
			pvr_fw_object_create_and_map(pvr_dev, fw_mem->core_data_alloc_size,
						     PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
						     NULL, NULL, &fw_mem->core_data_obj);
		if (!fw_core_data_ptr)
			return -ENOMEM;
	}

	pd_pvr_log("fw code 0x%x bytes at 0x%llx, data 0x%x bytes at 0x%llx, coremem %u/%u\n",
		   fw_mem->code_alloc_size, fw_mem->code_obj->phys,
		   fw_mem->data_alloc_size, fw_mem->data_obj->phys,
		   fw_mem->core_code_alloc_size, fw_mem->core_data_alloc_size);

	// the sections are written straight into the zeroed device memory, no host shadow copy
	err = pvr_mips_fw_process(pvr_dev, fw, fw_code_ptr, fw_data_ptr, fw_core_code_ptr,
				  fw_core_data_ptr);
	if (err)
		return err;

	pvr_dev->fw_dev.fwif_connection_ctl =
		pvr_fw_object_create_and_map_offset(pvr_dev,
						    pvr_dev->fw_dev.fw_heap_info.config_offset +
						    PVR_ROGUE_FWIF_CONNECTION_CTL_OFFSET,
						    sizeof(*pvr_dev->fw_dev.fwif_connection_ctl),
						    PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
						    NULL, NULL,
						    &fw_mem->fwif_connection_ctl_obj);
	if (!pvr_dev->fw_dev.fwif_connection_ctl) {
		pd_pvr_log("unable to allocate fwif connection control memory\n");
		return -ENOMEM;
	}

	return 0;
}

// pvr_ccb.c
static void
ccb_ctrl_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_ccb_ctl *ctrl = cpu_ptr;
	struct pvr_ccb *pvr_ccb = priv;

	ctrl->write_offset = 0;
	ctrl->read_offset = 0;
	ctrl->wrap_mask = pvr_ccb->num_cmds - 1;
	ctrl->cmd_size = pvr_ccb->cmd_size;
}

static int
pvr_ccb_init(struct pvr_device *pvr_dev, struct pvr_ccb *pvr_ccb,
	     u32 num_cmds_log2, size_t cmd_size)
{
	u32 num_cmds = 1 << num_cmds_log2;
	u32 ccb_size = num_cmds * cmd_size;

	pvr_ccb->num_cmds = num_cmds;
	pvr_ccb->cmd_size = cmd_size;

	/*
	 * Map CCB and control structure as uncached, so we don't have to flush
	 * CPU cache repeatedly when polling for space.
	 */
	pvr_ccb->ctrl = pvr_fw_object_create_and_map(pvr_dev, sizeof(*pvr_ccb->ctrl),
						     PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
						     ccb_ctrl_init, pvr_ccb, &pvr_ccb->ctrl_obj);
	if (!pvr_ccb->ctrl)
		return -ENOMEM;

	pvr_ccb->ccb = pvr_fw_object_create_and_map(pvr_dev, ccb_size,
						    PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
						    NULL, NULL, &pvr_ccb->ccb_obj);
	if (!pvr_ccb->ccb)
		return -ENOMEM;

	pvr_fw_object_get_fw_addr(pvr_ccb->ctrl_obj, &pvr_ccb->ctrl_fw_addr);
	pvr_fw_object_get_fw_addr(pvr_ccb->ccb_obj, &pvr_ccb->ccb_fw_addr);

	WRITE_ONCE(pvr_ccb->ctrl->write_offset, 0);
	WRITE_ONCE(pvr_ccb->ctrl->read_offset, 0);
	WRITE_ONCE(pvr_ccb->ctrl->wrap_mask, num_cmds - 1);
	WRITE_ONCE(pvr_ccb->ctrl->cmd_size, cmd_size);

	return 0;
}

// pvr_fw_trace.c
static void
tracebuf_ctrl_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_tracebuf *tracebuf_ctrl = cpu_ptr;
	struct pvr_device *pvr_dev = priv;
	struct pvr_fw_trace *fw_trace = &pvr_dev->fw_dev.fw_trace;

	tracebuf_ctrl->tracebuf_size_in_dwords = ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS;
	tracebuf_ctrl->tracebuf_flags = 0;

	if (fw_trace->group_mask)
		tracebuf_ctrl->log_type = fw_trace->group_mask | ROGUE_FWIF_LOG_TYPE_TRACE;
	else
		tracebuf_ctrl->log_type = ROGUE_FWIF_LOG_TYPE_NONE;

	for (u32 thread_nr = 0; thread_nr < ARRAY_SIZE(fw_trace->buffers); thread_nr++) {
		struct rogue_fwif_tracebuf_space *tracebuf_space =
			&tracebuf_ctrl->tracebuf[thread_nr];
		struct pvr_fw_trace_buffer *trace_buffer = &fw_trace->buffers[thread_nr];

		pvr_fw_object_get_fw_addr(trace_buffer->buf_obj,
					  &tracebuf_space->trace_buffer_fw_addr);

		tracebuf_space->trace_buffer = trace_buffer->buf;
		tracebuf_space->trace_pointer = 0;
	}
}

static int
pvr_fw_trace_init(struct pvr_device *pvr_dev)
{
	struct pvr_fw_trace *fw_trace = &pvr_dev->fw_dev.fw_trace;

	for (u32 thread_nr = 0; thread_nr < ARRAY_SIZE(fw_trace->buffers); thread_nr++) {
		struct pvr_fw_trace_buffer *trace_buffer = &fw_trace->buffers[thread_nr];

		trace_buffer->buf =
			pvr_fw_object_create_and_map(pvr_dev,
						     ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS *
						     sizeof(*trace_buffer->buf),
						     PVR_BO_FW_FLAGS_DEVICE_UNCACHED |
						     PVR_BO_FW_NO_CLEAR_ON_RESET,
						     NULL, NULL, &trace_buffer->buf_obj);
		if (!trace_buffer->buf) {
			pd_pvr_log("unable to allocate trace buffer\n");
			return -ENOMEM;
		}
	}

	/* TODO: Provide control of group mask. */
	fw_trace->group_mask = 0;

	fw_trace->tracebuf_ctrl =
		pvr_fw_object_create_and_map(pvr_dev,
					     sizeof(*fw_trace->tracebuf_ctrl),
					     PVR_BO_FW_FLAGS_DEVICE_UNCACHED |
					     PVR_BO_FW_NO_CLEAR_ON_RESET,
					     tracebuf_ctrl_init, pvr_dev,
					     &fw_trace->tracebuf_ctrl_obj);
	if (!fw_trace->tracebuf_ctrl) {
		pd_pvr_log("unable to allocate trace buffer control structure\n");
		return -ENOMEM;
	}

	for (u32 thread_nr = 0; thread_nr < ARRAY_SIZE(fw_trace->buffers); thread_nr++)
		fw_trace->buffers[thread_nr].tracebuf_space = &fw_trace->tracebuf_ctrl->tracebuf[thread_nr];

	return 0;
}

static void
fw_osinit_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_osinit *fwif_osinit = cpu_ptr;
	struct pvr_device *pvr_dev = priv;
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;
	struct pvr_fw_mem *fw_mem = &fw_dev->mem;

	fwif_osinit->kernel_ccbctl_fw_addr = pvr_dev->kccb.ccb.ctrl_fw_addr;
	fwif_osinit->kernel_ccb_fw_addr = pvr_dev->kccb.ccb.ccb_fw_addr;
	pvr_fw_object_get_fw_addr(pvr_dev->kccb.rtn_obj,
				  &fwif_osinit->kernel_ccb_rtn_slots_fw_addr);

	fwif_osinit->firmware_ccbctl_fw_addr = pvr_dev->fwccb.ctrl_fw_addr;
	fwif_osinit->firmware_ccb_fw_addr = pvr_dev->fwccb.ccb_fw_addr;

	fwif_osinit->work_est_firmware_ccbctl_fw_addr = 0;
	fwif_osinit->work_est_firmware_ccb_fw_addr = 0;

	pvr_fw_object_get_fw_addr(fw_mem->hwrinfobuf_obj,
				  &fwif_osinit->rogue_fwif_hwr_info_buf_ctl_fw_addr);
	pvr_fw_object_get_fw_addr(fw_mem->osdata_obj, &fwif_osinit->fw_os_data_fw_addr);

	fwif_osinit->hwr_debug_dump_limit = 0;

	rogue_fwif_compchecks_bvnc_init(&fwif_osinit->rogue_comp_checks.hw_bvnc);
	rogue_fwif_compchecks_bvnc_init(&fwif_osinit->rogue_comp_checks.fw_bvnc);
}

static void
fw_osdata_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_osdata *fwif_osdata = cpu_ptr;
	struct pvr_device *pvr_dev = priv;
	struct pvr_fw_mem *fw_mem = &pvr_dev->fw_dev.mem;

	pvr_fw_object_get_fw_addr(fw_mem->power_sync_obj, &fwif_osdata->power_sync_fw_addr);
}

static void
fw_fault_page_init(void *cpu_ptr, void *priv)
{
	u32 *fault_page = cpu_ptr;

	(void)priv;
	for (u32 i = 0; i < PVR_ROGUE_FAULT_PAGE_SIZE / sizeof(*fault_page); i++)
		fault_page[i] = 0xdeadbee0;
}

static void
fw_sysinit_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_sysinit *fwif_sysinit = cpu_ptr;
	struct pvr_device *pvr_dev = priv;
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;
	struct pvr_fw_mem *fw_mem = &fw_dev->mem;
	u32 clock_speed_hz = pvr_dev->core_clock_hz;

	fwif_sysinit->fault_phys_addr = fw_mem->fault_page_obj->phys;

	fwif_sysinit->pds_exec_base = ROGUE_PDSCODEDATA_HEAP_BASE;
	fwif_sysinit->usc_exec_base = ROGUE_USCCODE_HEAP_BASE;

	pvr_fw_object_get_fw_addr(fw_mem->runtime_cfg_obj, &fwif_sysinit->runtime_cfg_fw_addr);
	pvr_fw_object_get_fw_addr(fw_dev->fw_trace.tracebuf_ctrl_obj,
				  &fwif_sysinit->trace_buf_ctl_fw_addr);
	pvr_fw_object_get_fw_addr(fw_mem->sysdata_obj, &fwif_sysinit->fw_sys_data_fw_addr);
	pvr_fw_object_get_fw_addr(fw_mem->gpu_util_fwcb_obj,
				  &fwif_sysinit->gpu_util_fw_cb_ctl_fw_addr);
	if (fw_mem->core_data_obj) {
		pvr_fw_object_get_fw_addr(fw_mem->core_data_obj,
					  &fwif_sysinit->coremem_data_store.fw_addr);
	}

	/* Currently unsupported. */
	fwif_sysinit->counter_dump_ctl.buffer_fw_addr = 0;
	fwif_sysinit->counter_dump_ctl.size_in_dwords = 0;

	/* Skip alignment checks. */
	fwif_sysinit->align_checks = 0;

	fwif_sysinit->filter_flags = 0;
	fwif_sysinit->hw_perf_filter = 0;
	fwif_sysinit->firmware_perf = FW_PERF_CONF_NONE;
	fwif_sysinit->initial_core_clock_speed = clock_speed_hz;
	fwif_sysinit->active_pm_latency_ms = 0;
	fwif_sysinit->gpio_validation_mode = ROGUE_FWIF_GPIO_VAL_OFF;
	fwif_sysinit->firmware_started = false;
	fwif_sysinit->marker_val = 1;

	__builtin_memset(&fwif_sysinit->bvnc_km_feature_flags, 0,
			 sizeof(fwif_sysinit->bvnc_km_feature_flags));
}

#define ROGUE_FWIF_SLC_MIN_SIZE_FOR_DM_OVERLAP_KB 4

static void
fw_sysdata_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_sysdata *fwif_sysdata = cpu_ptr;
	struct pvr_device *pvr_dev = priv;
	u32 slc_size_in_kilobytes = 0;
	u32 config_flags = 0;

	(void)PVR_FEATURE_VALUE(pvr_dev, slc_size_in_kilobytes, &slc_size_in_kilobytes);

	if (slc_size_in_kilobytes < ROGUE_FWIF_SLC_MIN_SIZE_FOR_DM_OVERLAP_KB)
		config_flags |= ROGUE_FWIF_INICFG_DISABLE_DM_OVERLAP;

	fwif_sysdata->config_flags = config_flags;
}

static void
fw_runtime_cfg_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_runtime_cfg *runtime_cfg = cpu_ptr;
	struct pvr_device *pvr_dev = priv;

	runtime_cfg->core_clock_speed = pvr_dev->core_clock_hz;
	runtime_cfg->active_pm_latency_ms = 0;
	runtime_cfg->active_pm_latency_persistant = true;
	(void)PVR_FEATURE_VALUE(pvr_dev, num_clusters, &runtime_cfg->default_dusts_num_init);

	/* Keep watchdog timer disabled. */
	runtime_cfg->wdg_period_us = 0;
}

static void
fw_gpu_util_fwcb_init(void *cpu_ptr, void *priv)
{
	struct rogue_fwif_gpu_util_fwcb *gpu_util_fwcb = cpu_ptr;

	(void)priv;
	gpu_util_fwcb->last_word = PVR_FWIF_GPU_UTIL_STATE_IDLE;
}

static int
pvr_fw_create_structures(struct pvr_device *pvr_dev)
{
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;
	struct pvr_fw_mem *fw_mem = &fw_dev->mem;
	int err;

	fw_dev->power_sync = pvr_fw_object_create_and_map(pvr_dev, sizeof(*fw_dev->power_sync),
							  PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
							  NULL, NULL, &fw_mem->power_sync_obj);
	if (!fw_dev->power_sync) {
		pd_pvr_log("unable to allocate fw power_sync structure\n");
		return -ENOMEM;
	}

	fw_dev->hwrinfobuf = pvr_fw_object_create_and_map(pvr_dev, sizeof(*fw_dev->hwrinfobuf),
							  PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
							  NULL, NULL, &fw_mem->hwrinfobuf_obj);
	if (!fw_dev->hwrinfobuf) {
		pd_pvr_log("unable to allocate fw hwrinfobuf structure\n");
		return -ENOMEM;
	}

	err = pvr_fw_object_create(pvr_dev, PVR_SYNC_OBJ_SIZE,
				   PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
				   NULL, NULL, &fw_mem->mmucache_sync_obj);
	if (err) {
		pd_pvr_log("unable to allocate mmu cache sync object\n");
		return err;
	}

	fw_dev->fwif_sysdata = pvr_fw_object_create_and_map(pvr_dev,
							    sizeof(*fw_dev->fwif_sysdata),
							    PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
							    fw_sysdata_init, pvr_dev,
							    &fw_mem->sysdata_obj);
	if (!fw_dev->fwif_sysdata) {
		pd_pvr_log("unable to allocate fw sysdata structure\n");
		return -ENOMEM;
	}

	err = pvr_fw_object_create(pvr_dev, PVR_ROGUE_FAULT_PAGE_SIZE,
				   PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
				   fw_fault_page_init, NULL, &fw_mem->fault_page_obj);
	if (err) {
		pd_pvr_log("unable to allocate fw fault page\n");
		return err;
	}

	err = pvr_fw_object_create(pvr_dev, sizeof(struct rogue_fwif_gpu_util_fwcb),
				   PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
				   fw_gpu_util_fwcb_init, pvr_dev, &fw_mem->gpu_util_fwcb_obj);
	if (err) {
		pd_pvr_log("unable to allocate gpu util fwcb\n");
		return err;
	}

	err = pvr_fw_object_create(pvr_dev, sizeof(struct rogue_fwif_runtime_cfg),
				   PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
				   fw_runtime_cfg_init, pvr_dev, &fw_mem->runtime_cfg_obj);
	if (err) {
		pd_pvr_log("unable to allocate fw runtime config\n");
		return err;
	}

	err = pvr_fw_trace_init(pvr_dev);
	if (err)
		return err;

	fw_dev->fwif_osdata = pvr_fw_object_create_and_map(pvr_dev,
							   sizeof(*fw_dev->fwif_osdata),
							   PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
							   fw_osdata_init, pvr_dev,
							   &fw_mem->osdata_obj);
	if (!fw_dev->fwif_osdata) {
		pd_pvr_log("unable to allocate fw osdata structure\n");
		return -ENOMEM;
	}

	fw_dev->fwif_osinit =
		pvr_fw_object_create_and_map_offset(pvr_dev,
						    fw_dev->fw_heap_info.config_offset +
						    PVR_ROGUE_FWIF_OSINIT_OFFSET,
						    sizeof(*fw_dev->fwif_osinit),
						    PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
						    fw_osinit_init, pvr_dev, &fw_mem->osinit_obj);
	if (!fw_dev->fwif_osinit) {
		pd_pvr_log("unable to allocate fw osinit structure\n");
		return -ENOMEM;
	}

	fw_dev->fwif_sysinit =
		pvr_fw_object_create_and_map_offset(pvr_dev,
						    fw_dev->fw_heap_info.config_offset +
						    PVR_ROGUE_FWIF_SYSINIT_OFFSET,
						    sizeof(*fw_dev->fwif_sysinit),
						    PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
						    fw_sysinit_init, pvr_dev, &fw_mem->sysinit_obj);
	if (!fw_dev->fwif_sysinit) {
		pd_pvr_log("unable to allocate fw sysinit structure\n");
		return -ENOMEM;
	}

	return 0;
}

// pvr_fw_startstop.c
static void
rogue_axi_ace_list_init(struct pvr_device *pvr_dev)
{
	/* Setup AXI-ACE config. Set everything to outer cache. */
	u64 reg_val =
		(3U << ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_NON_SNOOPING_SHIFT) |
		(3U << ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_NON_SNOOPING_SHIFT) |
		(2U << ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_CACHE_MAINTENANCE_SHIFT) |
		(2U << ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_COHERENT_SHIFT) |
		(2U << ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_COHERENT_SHIFT) |
		(2U << ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_AWCACHE_COHERENT_SHIFT) |
		(2U << ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_COHERENT_SHIFT) |
		(2U << ROGUE_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_CACHE_MAINTENANCE_SHIFT);

	pvr_cr_write64(pvr_dev, ROGUE_CR_AXI_ACE_LITE_CONFIGURATION, reg_val);
}

static int
rogue_slc_init(struct pvr_device *pvr_dev)
{
	u16 slc_cache_line_size_bits;
	u32 reg_val;
	int err;

	/*
	 * SLC Misc control.
	 *
	 * Note: This is a 64bit register and we set only the lower 32bits
	 *       leaving the top 32bits (ROGUE_CR_SLC_CTRL_MISC_SCRAMBLE_BITS)
	 *       unchanged from the HW default.
	 */
	reg_val = (pvr_cr_read32(pvr_dev, ROGUE_CR_SLC_CTRL_MISC) &
		      ROGUE_CR_SLC_CTRL_MISC_ENABLE_PSG_HAZARD_CHECK_EN) |
		     ROGUE_CR_SLC_CTRL_MISC_ADDR_DECODE_MODE_PVR_HASH1;

	err = PVR_FEATURE_VALUE(pvr_dev, slc_cache_line_size_bits, &slc_cache_line_size_bits);
	if (err)
		return err;

	/* Bypass burst combiner if SLC line size is smaller than 1024 bits. */
	if (slc_cache_line_size_bits < 1024)
		reg_val |= ROGUE_CR_SLC_CTRL_MISC_BYPASS_BURST_COMBINER_EN;

	if (PVR_HAS_QUIRK(pvr_dev, 71242) && !PVR_HAS_FEATURE(pvr_dev, gpu_multicore_support))
		reg_val |= ROGUE_CR_SLC_CTRL_MISC_LAZYWB_OVERRIDE_EN;

	pvr_cr_write32(pvr_dev, ROGUE_CR_SLC_CTRL_MISC, reg_val);

	return 0;
}

int
pvr_fw_start(struct pvr_device *pvr_dev)
{
	bool has_reset2 = PVR_HAS_FEATURE(pvr_dev, xe_tpu2);
	u64 soft_reset_mask;
	int err;

	if (PVR_HAS_FEATURE(pvr_dev, pbe2_in_xe))
		soft_reset_mask = ROGUE_CR_SOFT_RESET__PBE2_XE__MASKFULL;
	else
		soft_reset_mask = ROGUE_CR_SOFT_RESET_MASKFULL;

	if (PVR_HAS_FEATURE(pvr_dev, sys_bus_secure_reset)) {
		/*
		 * Disable the default sys_bus_secure protection to perform
		 * minimal setup.
		 */
		pvr_cr_write32(pvr_dev, ROGUE_CR_SYS_BUS_SECURE, 0);
		(void)pvr_cr_read32(pvr_dev, ROGUE_CR_SYS_BUS_SECURE); /* Fence write */
	}

	/* Set Rogue in soft-reset. */
	pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET, soft_reset_mask);
	if (has_reset2)
		pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET2, ROGUE_CR_SOFT_RESET2_MASKFULL);

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline. */
	(void)pvr_cr_read64(pvr_dev, ROGUE_CR_SOFT_RESET);
	if (has_reset2)
		(void)pvr_cr_read64(pvr_dev, ROGUE_CR_SOFT_RESET2);

	/* Take Rascal and Dust out of reset. */
	pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET,
		       soft_reset_mask ^ ROGUE_CR_SOFT_RESET_RASCALDUSTS_EN);
	if (has_reset2)
		pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET2, 0);

	(void)pvr_cr_read64(pvr_dev, ROGUE_CR_SOFT_RESET);
	if (has_reset2)
		(void)pvr_cr_read64(pvr_dev, ROGUE_CR_SOFT_RESET2);

	/* Take everything out of reset but the FW processor. */
	pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET, ROGUE_CR_SOFT_RESET_GARTEN_EN);
	if (has_reset2)
		pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET2, 0);

	(void)pvr_cr_read64(pvr_dev, ROGUE_CR_SOFT_RESET);
	if (has_reset2)
		(void)pvr_cr_read64(pvr_dev, ROGUE_CR_SOFT_RESET2);

	err = rogue_slc_init(pvr_dev);
	if (err)
		goto err_reset;

	/* Initialise Firmware wrapper. */
	err = pvr_mips_wrapper_init(pvr_dev);
	if (err)
		goto err_reset;

	/* We must init the AXI-ACE interface before first BIF transaction. */
	rogue_axi_ace_list_init(pvr_dev);

	/* Need to wait for at least 16 cycles before taking the FW processor out of reset ... */
	pd_pvr_udelay(3);

	pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET, 0x0);
	(void)pvr_cr_read64(pvr_dev, ROGUE_CR_SOFT_RESET);

	/* ... and afterwards. */
	pd_pvr_udelay(3);

	return 0;

err_reset:
	/* Put everything back into soft-reset. */
	pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET, soft_reset_mask);

	return err;
}

int
pvr_fw_stop(struct pvr_device *pvr_dev)
{
	const u32 sidekick_idle_mask = ROGUE_CR_SIDEKICK_IDLE_MASKFULL &
				       ~(ROGUE_CR_SIDEKICK_IDLE_GARTEN_EN |
					 ROGUE_CR_SIDEKICK_IDLE_SOCIF_EN |
					 ROGUE_CR_SIDEKICK_IDLE_HOSTIF_EN);
	int err;

	/*
	 * Wait for Sidekick/Jones to signal IDLE except for the Garten Wrapper.
	 * For cores with the LAYOUT_MARS feature, SIDEKICK would have been
	 * powered down by the FW.
	 */
	err = pvr_cr_poll_reg32(pvr_dev, ROGUE_CR_SIDEKICK_IDLE, sidekick_idle_mask,
				sidekick_idle_mask, POLL_TIMEOUT_USEC);
	if (err)
		return err;

	/* Unset MTS DM association with threads. */
	pvr_cr_write32(pvr_dev, ROGUE_CR_MTS_INTCTX_THREAD0_DM_ASSOC,
		       ROGUE_CR_MTS_INTCTX_THREAD0_DM_ASSOC_MASKFULL &
		       ROGUE_CR_MTS_INTCTX_THREAD0_DM_ASSOC_DM_ASSOC_CLRMSK);
	pvr_cr_write32(pvr_dev, ROGUE_CR_MTS_BGCTX_THREAD0_DM_ASSOC,
		       ROGUE_CR_MTS_BGCTX_THREAD0_DM_ASSOC_MASKFULL &
		       ROGUE_CR_MTS_BGCTX_THREAD0_DM_ASSOC_DM_ASSOC_CLRMSK);
	pvr_cr_write32(pvr_dev, ROGUE_CR_MTS_INTCTX_THREAD1_DM_ASSOC,
		       ROGUE_CR_MTS_INTCTX_THREAD1_DM_ASSOC_MASKFULL &
		       ROGUE_CR_MTS_INTCTX_THREAD1_DM_ASSOC_DM_ASSOC_CLRMSK);
	pvr_cr_write32(pvr_dev, ROGUE_CR_MTS_BGCTX_THREAD1_DM_ASSOC,
		       ROGUE_CR_MTS_BGCTX_THREAD1_DM_ASSOC_MASKFULL &
		       ROGUE_CR_MTS_BGCTX_THREAD1_DM_ASSOC_DM_ASSOC_CLRMSK);

	/* Extra Idle checks. */
	err = pvr_cr_poll_reg32(pvr_dev, ROGUE_CR_BIF_STATUS_MMU, 0,
				ROGUE_CR_BIF_STATUS_MMU_MASKFULL,
				POLL_TIMEOUT_USEC);
	if (err)
		return err;

	err = pvr_cr_poll_reg32(pvr_dev, ROGUE_CR_BIFPM_STATUS_MMU, 0,
				ROGUE_CR_BIFPM_STATUS_MMU_MASKFULL,
				POLL_TIMEOUT_USEC);
	if (err)
		return err;

	if (!PVR_HAS_FEATURE(pvr_dev, xt_top_infrastructure)) {
		err = pvr_cr_poll_reg32(pvr_dev, ROGUE_CR_BIF_READS_EXT_STATUS, 0,
					ROGUE_CR_BIF_READS_EXT_STATUS_MASKFULL,
					POLL_TIMEOUT_USEC);
		if (err)
			return err;
	}

	err = pvr_cr_poll_reg32(pvr_dev, ROGUE_CR_BIFPM_READS_EXT_STATUS, 0,
				ROGUE_CR_BIFPM_READS_EXT_STATUS_MASKFULL,
				POLL_TIMEOUT_USEC);
	if (err)
		return err;

	err = pvr_cr_poll_reg64(pvr_dev, ROGUE_CR_SLC_STATUS1, 0,
				ROGUE_CR_SLC_STATUS1_MASKFULL,
				POLL_TIMEOUT_USEC);
	if (err)
		return err;

	/*
	 * Wait for SLC to signal IDLE.
	 * For cores with the LAYOUT_MARS feature, SLC would have been powered
	 * down by the FW.
	 */
	err = pvr_cr_poll_reg32(pvr_dev, ROGUE_CR_SLC_IDLE,
				ROGUE_CR_SLC_IDLE_MASKFULL,
				ROGUE_CR_SLC_IDLE_MASKFULL, POLL_TIMEOUT_USEC);
	if (err)
		return err;

	err = pvr_cr_poll_reg32(pvr_dev, ROGUE_CR_SIDEKICK_IDLE, sidekick_idle_mask,
				sidekick_idle_mask, POLL_TIMEOUT_USEC);
	if (err)
		return err;

	err = pvr_cr_poll_reg32(pvr_dev, ROGUE_CR_SIDEKICK_IDLE,
				ROGUE_CR_SIDEKICK_IDLE_GARTEN_EN,
				ROGUE_CR_SIDEKICK_IDLE_GARTEN_EN,
				POLL_TIMEOUT_USEC);
	if (err)
		return err;

	if (PVR_HAS_FEATURE(pvr_dev, pbe2_in_xe))
		pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET,
			       ROGUE_CR_SOFT_RESET__PBE2_XE__MASKFULL);
	else
		pvr_cr_write64(pvr_dev, ROGUE_CR_SOFT_RESET, ROGUE_CR_SOFT_RESET_MASKFULL);

	return 0;
}

static void
pvr_fw_log_boot_state(struct pvr_device *pvr_dev, const char *what)
{
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;

	pd_pvr_log("%s: firmware_started %u marker 0x%x connection fw %u os %u, mips exception 0x%x "
		   "wrapper status 0x%x irq status 0x%x, soft reset 0x%llx, event status 0x%x\n",
		   what, (u32)READ_ONCE(fw_dev->fwif_sysinit->firmware_started),
		   READ_ONCE(fw_dev->fwif_sysinit->marker_val),
		   (u32)READ_ONCE(fw_dev->fwif_connection_ctl->connection_fw_state),
		   (u32)READ_ONCE(fw_dev->fwif_connection_ctl->connection_os_state),
		   pvr_cr_read32(pvr_dev, ROGUE_CR_MIPS_EXCEPTION_STATUS),
		   pvr_cr_read32(pvr_dev, ROGUE_CR_MIPS_WRAPPER_STATUS),
		   pvr_cr_read32(pvr_dev, ROGUE_CR_MIPS_WRAPPER_IRQ_STATUS),
		   pvr_cr_read64(pvr_dev, ROGUE_CR_SOFT_RESET),
		   pvr_cr_read32(pvr_dev, ROGUE_CR_EVENT_STATUS));
}

int
pvr_wait_for_fw_boot(struct pvr_device *pvr_dev)
{
	u64 deadline = pd_pvr_now_us() + FW_BOOT_TIMEOUT_USEC;
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;

	while (pd_pvr_now_us() < deadline) {
		if (READ_ONCE(fw_dev->fwif_sysinit->firmware_started))
			return 0;
		pd_pvr_udelay(10);
	}

	return -ETIMEDOUT;
}

int
pvr_fw_init(struct pvr_device *pvr_dev)
{
	u32 kccb_size_log2 = ROGUE_FWIF_KCCB_NUMCMDS_LOG2_DEFAULT;
	u32 kccb_rtn_size = (1 << kccb_size_log2) * sizeof(*pvr_dev->kccb.rtn);
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;
	u64 t0;
	int err;

	if (!PVR_HAS_FEATURE(pvr_dev, mips)) {
		pd_pvr_log("not a mips firmware core, only mips is ported\n");
		return -EINVAL;
	}

	err = pvr_kccb_init_locks(pvr_dev);
	if (err)
		return err;

	pvr_fw_heap_info_init(pvr_dev, ROGUE_FW_HEAP_MIPS_SHIFT, ROGUE_FW_HEAP_MIPS_RESERVED_SIZE);
	fw_dev->fw_mm_base = ROGUE_FW_HEAP_BASE;

	err = pvr_vm_mips_init(pvr_dev);
	if (err)
		return err;

	err = pvr_fw_process(pvr_dev);
	if (err)
		return err;

	/* Initialise KCCB and FWCCB. */
	err = pvr_ccb_init(pvr_dev, &pvr_dev->kccb.ccb, ROGUE_FWIF_KCCB_NUMCMDS_LOG2_DEFAULT,
			   sizeof(struct rogue_fwif_kccb_cmd));
	if (err)
		return err;

	err = pvr_ccb_init(pvr_dev, &pvr_dev->fwccb, ROGUE_FWIF_FWCCB_NUMCMDS_LOG2,
			   sizeof(struct rogue_fwif_fwccb_cmd));
	if (err)
		return err;

	/* Allocate memory for KCCB return slots. */
	pvr_dev->kccb.rtn = pvr_fw_object_create_and_map(pvr_dev, kccb_rtn_size,
							 PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
							 NULL, NULL, &pvr_dev->kccb.rtn_obj);
	if (!pvr_dev->kccb.rtn)
		return -ENOMEM;

	err = pvr_fw_create_structures(pvr_dev);
	if (err)
		return err;

	pd_pvr_log("fw heap: %u objects, kccb 0x%x fwccb 0x%x osinit 0x%x sysinit 0x%x\n",
		   fw_dev->num_objs, pvr_dev->kccb.ccb.ccb_fw_addr, pvr_dev->fwccb.ccb_fw_addr,
		   (fw_dev->mem.osinit_obj->fw_addr_offset & fw_dev->fw_heap_info.offset_mask) |
		   ROGUE_FW_HEAP_MIPS_BASE,
		   (fw_dev->mem.sysinit_obj->fw_addr_offset & fw_dev->fw_heap_info.offset_mask) |
		   ROGUE_FW_HEAP_MIPS_BASE);

	// make every write through the write-combined mappings visible before the core starts
	__asm__ volatile ("dsb sy" ::: "memory");

	err = pvr_fw_start(pvr_dev);
	if (err) {
		pd_pvr_log("fw start failed (%d)\n", err);
		return err;
	}

	t0 = pd_pvr_now_us();
	err = pvr_wait_for_fw_boot(pvr_dev);
	if (err) {
		pvr_fw_log_boot_state(pvr_dev, "firmware failed to boot");
		pvr_fw_stop(pvr_dev);
		return err;
	}

	fw_dev->booted = true;
	pvr_fw_log_boot_state(pvr_dev, "fw booted");
	pd_pvr_log("fw booted in %llu us\n", pd_pvr_now_us() - t0);

	return 0;
}

void
pvr_fw_fini(struct pvr_device *pvr_dev)
{
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;

	fw_dev->booted = false;
	for (u32 i = 0; i < fw_dev->num_objs; i++) {
		if (fw_dev->objs[i].in_use)
			pd_pvr_free(fw_dev->objs[i].bmd, fw_dev->objs[i].map);
		fw_dev->objs[i].in_use = false;
	}
	fw_dev->num_objs = 0;
}
