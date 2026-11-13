// pdnv_pci.c - the interface layer's PCI and mapping functions, and the probe that gives the resource manager a GPU
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include <kern/thread.h>
#include <kern/thread_call.h>

extern boolean_t ml_get_interrupts_enabled(void);

#include <nvtypes.h>
#include <nvstatus.h>
#include <os-interface.h>
#include <nv.h>
#include <os/nv_memory_type.h>

#include "pdnv_port.h"
#include "pdnv_pci.h"

int pdnv_stack_alloc(nvidia_stack_t **sp);
void pdnv_stack_free(nvidia_stack_t *sp);

typedef NV_STATUS (*rm_supported_fn)(nvidia_stack_t *, nv_state_t *);
typedef NvBool (*rm_private_fn)(nvidia_stack_t *, nv_state_t *);

// config space

void * NV_API_CALL
os_pci_init_handle(NvU32 domain, NvU8 bus, NvU8 slot, NvU8 function, NvU16 *vendor, NvU16 *device)
{
	return pdnv_pci_find(domain, bus, slot, function, vendor, device);
}

NV_STATUS NV_API_CALL
os_pci_read_byte(void *handle, NvU32 offset, NvU8 *value)
{
	uint32_t v;

	if (pdnv_cfg_read(handle, offset, 1, &v) != 0) {
		*value = 0xff;
		return NV_ERR_NOT_SUPPORTED;
	}
	*value = (NvU8)v;
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_pci_read_word(void *handle, NvU32 offset, NvU16 *value)
{
	uint32_t v;

	if (pdnv_cfg_read(handle, offset, 2, &v) != 0) {
		*value = 0xffff;
		return NV_ERR_NOT_SUPPORTED;
	}
	*value = (NvU16)v;
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_pci_read_dword(void *handle, NvU32 offset, NvU32 *value)
{
	uint32_t v;

	if (pdnv_cfg_read(handle, offset, 4, &v) != 0) {
		*value = 0xffffffff;
		return NV_ERR_NOT_SUPPORTED;
	}
	*value = v;
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_pci_write_byte(void *handle, NvU32 offset, NvU8 value)
{
	return pdnv_cfg_write(handle, offset, 1, value) == 0 ? NV_OK : NV_ERR_NOT_SUPPORTED;
}

NV_STATUS NV_API_CALL
os_pci_write_word(void *handle, NvU32 offset, NvU16 value)
{
	return pdnv_cfg_write(handle, offset, 2, value) == 0 ? NV_OK : NV_ERR_NOT_SUPPORTED;
}

NV_STATUS NV_API_CALL
os_pci_write_dword(void *handle, NvU32 offset, NvU32 value)
{
	return pdnv_cfg_write(handle, offset, 4, value) == 0 ? NV_OK : NV_ERR_NOT_SUPPORTED;
}

// no hot removal
NvBool NV_API_CALL
os_pci_remove_supported(void)
{
	return NV_FALSE;
}

void NV_API_CALL
os_pci_remove(void *handle)
{
	(void)handle;
}

// physical memory and BARs

void * NV_API_CALL
os_map_kernel_space(NvU64 start, NvU64 size, NvU32 mode)
{
	int cache = mode == NV_MEMORY_CACHED ? PDNV_MAP_CACHED :
	    mode == NV_MEMORY_WRITECOMBINED ? PDNV_MAP_WC : PDNV_MAP_UNCACHED;

	// as on Linux, page 0 only as ordinary cached memory
	if (start == 0 && mode != NV_MEMORY_CACHED) {
		IOLog("PDNVLoader: os_map_kernel_space(0x%llx, 0x%llx, %u) from image +0x%llx refused\n", start, size, mode,
		    (unsigned long long)((uintptr_t)__builtin_return_address(0) - pdnv_image_base));
		return NULL;
	}
	{
		void *va = pdnv_map_phys(start, size, cache);

		if (va == NULL)
			IOLog("PDNVLoader: os_map_kernel_space(0x%llx, 0x%llx, %u) from image +0x%llx failed\n", start, size,
			    mode, (unsigned long long)((uintptr_t)__builtin_return_address(0) - pdnv_image_base));
		return va;
	}
}

void NV_API_CALL
os_unmap_kernel_space(void *addr, NvU64 size)
{
	(void)size;
	pdnv_unmap_phys(addr);
}

// the per-device state behind nv->os_state, what nv_linux_state_t is to the Linux layer
struct pdnv_gpu {
	nv_state_t *nv;
	uint64_t dma_mask;
	nvidia_stack_t *isr_sp;     // the top half runs in the interrupt filter, the bottom half on a work loop
	nvidia_stack_t *bh_sp;
	nvidia_stack_t *timer_sp;   // the RC watchdog's, it runs on a thread call
	thread_call_t rc_timer;
	NvBool safe_to_mmap;
};

typedef NvBool (*rm_isr_fn)(nvidia_stack_t *, nv_state_t *, NvU32 *);
typedef void (*rm_bh_fn)(nvidia_stack_t *, nv_state_t *);

// DMA goes to physical addresses: no IOMMU translation, as with swiotlb on Linux
NvBool NV_API_CALL
nv_requires_dma_remap(nv_state_t *nv)
{
	(void)nv;
	return NV_FALSE;
}

void NV_API_CALL
nv_set_dma_address_size(nv_state_t *nv, NvU32 phys_addr_bits)
{
	struct pdnv_gpu *gpu = nv->os_state;

	gpu->dma_mask = phys_addr_bits >= 64 ? ~0ull : (1ull << phys_addr_bits) - 1;
}

// a record for a state with no device behind it, the control device's: DMA anywhere
void *
pdnv_gpu_record(void *state)
{
	struct pdnv_gpu *gpu;

	if (os_alloc_mem((void **)&gpu, sizeof(*gpu)) != NV_OK)
		return NULL;
	os_mem_set(gpu, 0, sizeof(*gpu));
	gpu->nv = state;
	gpu->dma_mask = ~0ull;
	gpu->safe_to_mmap = NV_TRUE;
	return gpu;
}

void *
pdnv_gpu_state(void *g)
{
	return ((struct pdnv_gpu *)g)->nv;
}

// the probe, as nv_pci_probe runs it: describe the device, then let the resource manager judge it

void *
pdnv_gpu_probe(const struct pdnv_pci_dev *info)
{
	nvidia_stack_t *sp = NULL;
	struct pdnv_gpu *gpu = NULL;
	nv_state_t *nv = NULL;
	NV_STATUS st;

	if (pdnv_rm.is_supported_device == NULL || pdnv_rm.init_private_state == NULL ||
	    pdnv_rm.init_event_locks == NULL)
		return NULL;
	if (os_alloc_mem((void **)&gpu, sizeof(*gpu)) != NV_OK)
		return NULL;
	os_mem_set(gpu, 0, sizeof(*gpu));
	if (os_alloc_mem((void **)&nv, sizeof(*nv)) != NV_OK)
		goto fail;
	os_mem_set(nv, 0, sizeof(*nv));
	gpu->nv = nv;
	gpu->safe_to_mmap = NV_TRUE;
	gpu->dma_mask = 0xffffffffull;  // 32-bit until the resource manager says otherwise, as on Linux

	for (int i = 0; i < info->nbars && i < NV_GPU_NUM_BARS; i++) {
		nv->bars[i].offset = info->bars[i].offset;
		nv->bars[i].cpu_address = info->bars[i].cpu_address;
		nv->bars[i].size = info->bars[i].size;
	}
	nv->regs = &nv->bars[NV_GPU_BAR_INDEX_REGS];
	nv->fb = &nv->bars[NV_GPU_BAR_INDEX_FB];
	nv->pci_info.domain = 0;
	nv->pci_info.bus = info->bus;
	nv->pci_info.slot = info->slot;
	nv->pci_info.function = info->func;
	nv->pci_info.vendor_id = info->vendor;
	nv->pci_info.device_id = info->device;
	nv->subsystem_vendor = info->subsystem_vendor;
	nv->subsystem_id = info->subsystem_id;
	nv->handle = info->handle;
	nv->os_state = gpu;
	nv->interrupt_line = info->irq;
	nv->cpu_numa_node_id = -1;

	IOLog("PDNVGPU: %02x:%02x.%x %04x:%04x, registers 0x%llx (%llu MB), framebuffer 0x%llx (%llu MB)\n",
	    info->bus, info->slot, info->func, info->vendor, info->device,
	    (unsigned long long)nv->regs->cpu_address, (unsigned long long)(nv->regs->size >> 20),
	    (unsigned long long)nv->fb->cpu_address, (unsigned long long)(nv->fb->size >> 20));

	// the chip's id register, all ones when the device does not decode or sleeps
	{
		volatile NvU32 *r = pdnv_map_phys(nv->regs->cpu_address, 4096, PDNV_MAP_UNCACHED);
		uint32_t cmd = 0, pm = 0, cap = 0;

		pdnv_cfg_read(info->handle, 0x04, 2, &cmd);
		pdnv_cfg_read(info->handle, 0x34, 1, &cap);
		while (cap != 0 && cap != 0xff) {
			uint32_t id = 0, next = 0;

			pdnv_cfg_read(info->handle, cap, 1, &id);
			pdnv_cfg_read(info->handle, cap + 1, 1, &next);
			if (id == 1) {
				pdnv_cfg_read(info->handle, cap + 4, 2, &pm);
				break;
			}
			cap = next;
		}
		IOLog("PDNVGPU: command 0x%x, power state D%u, PMC_BOOT_0 0x%08x\n", cmd, pm & 3, r != NULL ? r[0] : 0);
		if (r != NULL)
			pdnv_unmap_phys((void *)r);
	}

	if (pdnv_stack_alloc(&sp) != 0 || pdnv_stack_alloc(&gpu->isr_sp) != 0 || pdnv_stack_alloc(&gpu->bh_sp) != 0 ||
	    pdnv_stack_alloc(&gpu->timer_sp) != 0)
		goto fail;
	// as nv_lock_init_locks before the probe
	if (!((rm_private_fn)pdnv_rm.init_event_locks)(sp, nv)) {
		IOLog("PDNVGPU: rm_init_event_locks failed\n");
		goto fail;
	}
	st = ((rm_supported_fn)pdnv_rm.is_supported_device)(sp, nv);
	if (st != NV_OK) {
		IOLog("PDNVGPU: rm_is_supported_device 0x%x\n", st);
		goto fail;
	}
	if (!((rm_private_fn)pdnv_rm.init_private_state)(sp, nv)) {
		IOLog("PDNVGPU: rm_init_private_state failed\n");
		goto fail;
	}
	IOLog("PDNVGPU: supported, private state set up\n");
	pdnv_stack_free(sp);
	return gpu;

fail:
	if (sp != NULL)
		pdnv_stack_free(sp);
	if (gpu->isr_sp != NULL)
		pdnv_stack_free(gpu->isr_sp);
	if (gpu->bh_sp != NULL)
		pdnv_stack_free(gpu->bh_sp);
	if (gpu->timer_sp != NULL)
		pdnv_stack_free(gpu->timer_sp);
	if (nv != NULL)
		os_free_mem(nv);
	os_free_mem(gpu);
	return NULL;
}

// interrupts, as nvidia_isr and nvidia_isr_bh: 0 not ours, 1 handled, 2 handled with bottom half work
// interrupt counts, logged by the RC watchdog when they change

int
pdnv_gpu_isr(void *g)
{
	struct pdnv_gpu *gpu = g;
	NvU32 need_bh = 0;

	if (!((rm_isr_fn)pdnv_rm.isr)(gpu->isr_sp, gpu->nv, &need_bh))
		return 0;
	return need_bh ? 2 : 1;
}

void
pdnv_gpu_bh(void *g)
{
	struct pdnv_gpu *gpu = g;

	((rm_bh_fn)pdnv_rm.isr_bh)(gpu->bh_sp, gpu->nv);
}

// the adapter's start, as nv_start_device once the interrupt is in place
int
pdnv_gpu_start(void *g)
{
	struct pdnv_gpu *gpu = g;
	nvidia_stack_t *sp = NULL;
	NvBool ok;

	if (pdnv_rm.init_adapter == NULL || pdnv_stack_alloc(&sp) != 0)
		return -1;
	IOLog("PDNVGPU: rm_init_adapter\n");
	ok = ((rm_private_fn)pdnv_rm.init_adapter)(sp, gpu->nv);
	IOLog("PDNVGPU: rm_init_adapter %s\n", ok ? "done" : "failed");
	pdnv_stack_free(sp);
	return ok ? 0 : -1;
}

// system memory for the GPU, as nv_alloc_pages hands it out: one DMA address per 4 KB page, or one for a
// contiguous run. Non-contiguous requests come in chunks of the asked page size

#define PDNV_PAGE   4096ull

struct pdnv_chunk {
	void *handle;
	void *va;
	NvU64 pa;
};

struct pdnv_alloc {
	volatile SInt32 refs;       // the resource manager's, and one per user mapping
	NvU32 cache;
	NvU32 page_count;
	NvU32 nchunks;
	NvU64 chunk_size;
	struct pdnv_chunk c[];
};

void pdnv_alloc_release(void *a);

static int
cache_of(NvU32 cache_type)
{
	return cache_type == NV_MEMORY_CACHED ? PDNV_MAP_CACHED :
	    cache_type == NV_MEMORY_WRITECOMBINED ? PDNV_MAP_WC : PDNV_MAP_UNCACHED;
}

static void
free_alloc(struct pdnv_alloc *at)
{
	for (NvU32 i = 0; i < at->nchunks; i++)
		if (at->c[i].handle != NULL)
			pdnv_dma_free(at->c[i].handle);
	os_free_mem(at);
}

NV_STATUS NV_API_CALL
nv_alloc_pages(nv_state_t *nv, NvU32 page_count, NvU64 page_size, NvBool contiguous, NvU32 cache_type,
    NvBool zeroed, NvBool unencrypted, NvS32 node_id, NvU64 *pte_array, void **priv_data)
{
	NvU64 mask = nv != NULL ? ((struct pdnv_gpu *)nv->os_state)->dma_mask : ~0ull;
	NvU64 total = (NvU64)page_count * PDNV_PAGE;
	NvU64 chunk = contiguous ? total : (page_size > PDNV_PAGE ? page_size : PDNV_PAGE);
	NvU32 n = (NvU32)((total + chunk - 1) / chunk);
	struct pdnv_alloc *at;

	(void)zeroed;   // always zeroed
	(void)unencrypted;
	(void)node_id;
	if (page_count == 0)
		return NV_ERR_INVALID_ARGUMENT;
	if (os_alloc_mem((void **)&at, sizeof(*at) + n * sizeof(at->c[0])) != NV_OK)
		return NV_ERR_NO_MEMORY;
	os_mem_set(at, 0, sizeof(*at) + n * sizeof(at->c[0]));
	at->refs = 1;
	at->cache = cache_type;
	at->page_count = page_count;
	at->nchunks = n;
	at->chunk_size = chunk;

	for (NvU32 i = 0; i < n; i++) {
		at->c[i].handle = pdnv_dma_alloc(chunk, cache_of(cache_type), mask, &at->c[i].pa, &at->c[i].va);
		if (at->c[i].handle == NULL) {
			free_alloc(at);
			return NV_ERR_NO_MEMORY;
		}
	}
	if (contiguous) {
		pte_array[0] = at->c[0].pa;
	} else {
		for (NvU32 i = 0; i < page_count; i++) {
			NvU64 off = (NvU64)i * PDNV_PAGE;

			pte_array[i] = at->c[off / chunk].pa + off % chunk;
		}
	}
	*priv_data = at;
	return NV_OK;
}

NV_STATUS NV_API_CALL
nv_free_pages(nv_state_t *nv, NvU32 page_count, NvBool contiguous, NvU32 cache_type, void *priv_data)
{
	(void)nv;
	(void)page_count;
	(void)contiguous;
	(void)cache_type;
	// a user mapping keeps the pages until it goes, as at->usage_count on Linux
	if (priv_data != NULL)
		pdnv_alloc_release(priv_data);
	return NV_OK;
}

void
pdnv_alloc_retain(void *a)
{
	OSIncrementAtomic(&((struct pdnv_alloc *)a)->refs);
}

void
pdnv_alloc_release(void *a)
{
	if (OSDecrementAtomic(&((struct pdnv_alloc *)a)->refs) == 1)
		free_alloc(a);
}

NvU64
pdnv_alloc_pages(void *a)
{
	return ((struct pdnv_alloc *)a)->page_count;
}

NvU64
pdnv_alloc_page_pa(void *a, NvU64 i)
{
	struct pdnv_alloc *at = a;
	NvU64 off = i * PDNV_PAGE;

	return at->c[off / at->chunk_size].pa + off % at->chunk_size;
}

NvU32
pdnv_alloc_cache(void *a)
{
	return ((struct pdnv_alloc *)a)->cache;
}

// what a user mapping of system memory is told: the physical address, as on Linux
NV_STATUS NV_API_CALL
nv_alloc_user_mapping(nv_state_t *nv, void *pAllocPrivate, NvU64 pageIndex, NvU32 pageOffset, NvU64 size,
    NvU32 protect, NvU64 *pUserAddress, void **ppPrivate)
{
	(void)nv;
	(void)size;
	(void)protect;
	*ppPrivate = NULL;
	*pUserAddress = pdnv_alloc_page_pa(pAllocPrivate, pageIndex) + pageOffset;
	return NV_OK;
}

void NV_API_CALL
nv_free_user_mapping(nv_state_t *nv, void *pAllocPrivate, NvU64 userAddress, void *pPrivate)
{
	(void)nv;
	(void)pAllocPrivate;
	(void)userAddress;
	(void)pPrivate;
}

// the page of an allocation at a physical address handed out above
NV_STATUS NV_API_CALL
os_match_mmap_offset(void *pAllocPrivate, NvU64 offset, NvU64 *pPageIndex)
{
	for (NvU64 i = 0; i < pdnv_alloc_pages(pAllocPrivate); i++) {
		if (pdnv_alloc_page_pa(pAllocPrivate, i) == offset) {
			*pPageIndex = i;
			return NV_OK;
		}
	}
	return NV_ERR_OBJECT_NOT_FOUND;
}

// no 4 KB page isolation on these GPUs
NV_STATUS NV_API_CALL
nv_check_usermap_access_params(nv_state_t *nv, const nv_usermap_access_params_t *nvuap)
{
	(void)nv;
	(void)nvuap;
	return NV_OK;
}

// the mmap lock and whether mapping is allowed, the GPU is never powered down under a mapping here
static IOLock *sMmapLock;

void NV_API_CALL
nv_acquire_mmap_lock(nv_state_t *nv)
{
	(void)nv;
	IOLockLock(sMmapLock);
}

void NV_API_CALL
nv_release_mmap_lock(nv_state_t *nv)
{
	(void)nv;
	IOLockUnlock(sMmapLock);
}

void NV_API_CALL
nv_set_safe_to_mmap_locked(nv_state_t *nv, NvBool safe_to_mmap)
{
	((struct pdnv_gpu *)nv->os_state)->safe_to_mmap = safe_to_mmap;
}

NvBool
pdnv_gpu_safe_to_mmap(nv_state_t *nv)
{
	return ((struct pdnv_gpu *)nv->os_state)->safe_to_mmap;
}

// a kernel address for part of an allocation: inside one chunk directly, across chunks as a new mapping
void * NV_API_CALL
nv_alloc_kernel_mapping(nv_state_t *nv, void *pAllocPrivate, NvU64 pageIndex, NvU32 pageOffset, NvU64 size,
    void **pPrivate)
{
	struct pdnv_alloc *at = pAllocPrivate;
	NvU64 off = pageIndex * PDNV_PAGE + pageOffset;
	NvU64 first = off / at->chunk_size, last = (off + size - 1) / at->chunk_size;
	NvU32 n = (NvU32)(last - first + 1);
	NvU64 *pa;
	char *va;

	(void)nv;
	*pPrivate = NULL;
	if (size == 0 || last >= at->nchunks)
		return NULL;
	if (first == last)
		return (char *)at->c[first].va + off % at->chunk_size;
	pa = IOMalloc(n * sizeof(*pa));
	if (pa == NULL)
		return NULL;
	for (NvU32 i = 0; i < n; i++)
		pa[i] = at->c[first + i].pa;
	va = pdnv_map_runs(pa, n, at->chunk_size, cache_of(at->cache), pPrivate);
	IOFree(pa, n * sizeof(*pa));
	if (va == NULL) {
		nv_printf(NV_DBG_ERRORS, "NVRM: nv_alloc_kernel_mapping: 0x%llx bytes over %u chunks not mapped\n", size, n);
		return NULL;
	}
	return va + off % at->chunk_size;
}

void NV_API_CALL
nv_free_kernel_mapping(nv_state_t *nv, void *pAllocPrivate, void *address, void *pPrivate)
{
	(void)nv;
	(void)pAllocPrivate;
	(void)address;
	pdnv_unmap_runs(pPrivate);
}

NvU64 NV_API_CALL
nv_get_kern_phys_address(NvU64 address)
{
	return pdnv_kvtophys(address);
}

// the rest of what rm_init_adapter asks, with this system's answers

// one device node per GPU, numbered in probe order. One GPU for now
NvU32 NV_API_CALL
nv_get_dev_minor(nv_state_t *nv)
{
	(void)nv;
	return 0;
}

void NV_API_CALL
nv_get_disp_smmu_stream_ids(nv_state_t *nv, NvU32 *dispIsoStreamId, NvU32 *dispNisoStreamId)
{
	*dispIsoStreamId = nv->iommus.dispIsoStreamId;
	*dispNisoStreamId = nv->iommus.dispNisoStreamId;
}

// the console is not on this GPU
void NV_API_CALL
nv_get_screen_info(nv_state_t *nv, NvU64 *pPhysicalAddress, NvU32 *pFbWidth, NvU32 *pFbHeight, NvU32 *pFbDepth,
    NvU32 *pFbPitch, NvU64 *pFbSize)
{
	(void)nv;
	*pPhysicalAddress = 0;
	*pFbWidth = *pFbHeight = *pFbDepth = *pFbPitch = 0;
	*pFbSize = 0;
}

void NV_API_CALL
os_disable_console_access(void)
{
}

void NV_API_CALL
os_enable_console_access(void)
{
}

// no ACPI methods for the GPU yet: none present, every call unsupported
void NV_API_CALL
nv_acpi_methods_init(NvU32 *handlesPresent)
{
	if (handlesPresent != NULL)
		*handlesPresent = 0;
}

void NV_API_CALL
nv_acpi_methods_uninit(void)
{
}

NV_STATUS NV_API_CALL
nv_acpi_dsm_method(nv_state_t *nv, NvU8 *pAcpiDsmGuid, NvU32 acpiDsmRev, NvBool acpiNvpcfDsmFunction,
    NvU32 acpiDsmSubFunction, void *pInParams, NvU16 inParamSize, NvU32 *outStatus, void *pOutData,
    NvU16 *pSize)
{
	return NV_ERR_NOT_SUPPORTED;
}

NV_STATUS NV_API_CALL
nv_acpi_dod_method(nv_state_t *nv, NvU32 *pOutData, NvU32 *pSize)
{
	return NV_ERR_NOT_SUPPORTED;
}

NV_STATUS NV_API_CALL
nv_acpi_mux_method(nv_state_t *nv, NvU32 *pInOut, NvU32 muxAcpiId, const char *pMethodName)
{
	return NV_ERR_NOT_SUPPORTED;
}

// no coherent link or NUMA memory: the framebuffer is only behind the BAR
NV_STATUS NV_API_CALL
nv_get_device_memory_config(nv_state_t *nv, NvU64 *compr_addr_sys_phys, NvU64 *addr_guest_phys,
    NvU64 *rsvd_phys, NvU64 *addr_width, NvU32 *granularity, NvS32 *node_id)
{
	return NV_ERR_NOT_SUPPORTED;
}

// x86 has PAT, the BAR mappings above ask for write combining by it
NvBool NV_API_CALL
os_pat_supported(void)
{
	return NV_TRUE;
}

// no firmware files are handed out yet, so there is nothing to put back
void NV_API_CALL
nv_put_firmware(const void *fw_handle)
{
	(void)fw_handle;
}

// no pid namespaces, as the Linux layer's fallback: no reference to hand out
void * NV_API_CALL
os_get_pid_info(void)
{
	return NULL;
}

void NV_API_CALL
os_put_pid_info(void *pid_info)
{
	(void)pid_info;
}

// deferred work, as the Linux layer's nv_kthread_q: one thread runs the items in order, each on its own stack.
// Every queue the resource manager names shares it, there is one GPU

typedef void (*rm_work_fn)(nvidia_stack_t *, void *);

struct pdnv_work {
	struct pdnv_work *next;
	void *data;
};

static IOLock *sWorkLock;
static struct pdnv_work *sWorkHead, *sWorkTail;
static int sWorkBusy;

static void
work_thread(void *arg, wait_result_t wr)
{
	(void)arg;
	(void)wr;
	IOLockLock(sWorkLock);
	for (;;) {
		struct pdnv_work *w;
		nvidia_stack_t *sp = NULL;

		while (sWorkHead == NULL) {
			sWorkBusy = 0;
			IOLockWakeup(sWorkLock, &sWorkBusy, false);
			IOLockSleep(sWorkLock, &sWorkHead, THREAD_UNINT);
		}
		w = sWorkHead;
		sWorkHead = w->next;
		if (sWorkHead == NULL)
			sWorkTail = NULL;
		sWorkBusy = 1;
		IOLockUnlock(sWorkLock);

		if (pdnv_stack_alloc(&sp) == 0) {
			((rm_work_fn)pdnv_rm.execute_work_item)(sp, w->data);
			pdnv_stack_free(sp);
		}
		IOFree(w, sizeof(*w));
		IOLockLock(sWorkLock);
	}
}

void
pdnv_work_init(void)
{
	thread_t t;

	if (sWorkLock != NULL)
		return;
	sWorkLock = IOLockAlloc();
	sMmapLock = IOLockAlloc();
	if (kernel_thread_start(work_thread, NULL, &t) == KERN_SUCCESS)
		thread_deallocate(t);
}

NV_STATUS NV_API_CALL
os_queue_work_item(struct os_work_queue *queue, void *data)
{
	struct pdnv_work *w;

	(void)queue;
	if (sWorkLock == NULL || pdnv_rm.execute_work_item == NULL)
		return NV_ERR_NOT_READY;
	w = IOMalloc(sizeof(*w));
	if (w == NULL)
		return NV_ERR_NO_MEMORY;
	w->next = NULL;
	w->data = data;
	IOLockLock(sWorkLock);
	if (sWorkTail != NULL)
		sWorkTail->next = w;
	else
		sWorkHead = w;
	sWorkTail = w;
	IOLockWakeup(sWorkLock, &sWorkHead, true);
	IOLockUnlock(sWorkLock);
	return NV_OK;
}

// waits until the queue is empty and its thread idle
NV_STATUS NV_API_CALL
os_flush_work_queue(struct os_work_queue *queue, NvBool is_unload)
{
	(void)queue;
	(void)is_unload;
	if (sWorkLock == NULL)
		return NV_OK;
	if (!ml_get_interrupts_enabled())
		return NV_ERR_ILLEGAL_ACTION;
	IOLockLock(sWorkLock);
	while (sWorkHead != NULL || sWorkBusy)
		IOLockSleep(sWorkLock, &sWorkBusy, THREAD_UNINT);
	IOLockUnlock(sWorkLock);
	return NV_OK;
}

// DDC over ACPI is for laptop panels, none here
NV_STATUS NV_API_CALL
nv_acpi_ddc_method(nv_state_t *nv, void *pEdidBuffer, NvU32 *pSize, NvBool bReadMultiBlock)
{
	return NV_ERR_NOT_SUPPORTED;
}

// a yield, as schedule_timeout(1): sleeping is only allowed where the caller may block
NV_STATUS NV_API_CALL
os_schedule(void)
{
	if (!ml_get_interrupts_enabled()) {
		nv_printf(NV_DBG_ERRORS, "NVRM: os_schedule: attempted to yield the CPU in atomic context\n");
		return NV_ERR_ILLEGAL_ACTION;
	}
	IOSleep(1);
	return NV_OK;
}

// the RC watchdog, as nvidia_rc_timer_callback: once a second while the resource manager answers NV_OK

typedef NV_STATUS (*rm_rc_fn)(nvidia_stack_t *, nv_state_t *);

static void
rc_timer_fire(thread_call_param_t p0, thread_call_param_t p1)
{
	struct pdnv_gpu *gpu = p0;
	uint64_t deadline;

	(void)p1;
	if (!gpu->nv->rc_timer_enabled)
		return;
	if (((rm_rc_fn)pdnv_rm.run_rc_callback)(gpu->timer_sp, gpu->nv) == NV_OK) {
		clock_interval_to_deadline(1, NSEC_PER_SEC, &deadline);
		thread_call_enter_delayed(gpu->rc_timer, deadline);
	}
}

NvS32 NV_API_CALL
nv_start_rc_timer(nv_state_t *nv)
{
	struct pdnv_gpu *gpu = nv->os_state;
	uint64_t deadline;

	if (nv->rc_timer_enabled || pdnv_rm.run_rc_callback == NULL)
		return -1;
	if (gpu->rc_timer == NULL)
		gpu->rc_timer = thread_call_allocate(rc_timer_fire, gpu);
	if (gpu->rc_timer == NULL)
		return -1;
	nv->rc_timer_enabled = 1;
	clock_interval_to_deadline(1, NSEC_PER_SEC, &deadline);
	thread_call_enter_delayed(gpu->rc_timer, deadline);
	return 0;
}

NvS32 NV_API_CALL
nv_stop_rc_timer(nv_state_t *nv)
{
	struct pdnv_gpu *gpu = nv->os_state;

	if (!nv->rc_timer_enabled)
		return -1;
	nv->rc_timer_enabled = 0;
	thread_call_cancel_wait(gpu->rc_timer);
	return 0;
}

// no I2C adapters are registered yet, as the Linux layer built without I2C support
void * NV_API_CALL
nv_i2c_add_adapter(nv_state_t *nv, NvU32 port)
{
	(void)nv;
	(void)port;
	return NULL;
}

void NV_API_CALL
nv_i2c_del_adapter(nv_state_t *nv, void *data)
{
	(void)nv;
	(void)data;
}
