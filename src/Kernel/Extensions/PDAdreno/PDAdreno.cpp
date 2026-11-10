// SPDX-License-Identifier: MIT
// adreno core: firmware load, a3xx register init, cp start, ring and fences
#include "PDAdreno.h"
#include "PDAdrenoSmmu.h"

#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/fcntl.h>
#include <sys/vfs_context.h>

#define super IOService
OSDefineMetaClassAndStructors(PDAdreno, IOService);

// register window fallbacks when the platform publishes no memory for a node [spec 1]
#define GPU_PHYS	0x01c00000ull
#define GPU_SIZE	0x20000u
#define GCC_PHYS	0x01800000ull
#define GCC_SIZE	0x80000u
#define SMMU_PHYS	0x01f08000ull
#define SMMU_SIZE	0x10000u

// GPU registers, dword offsets [freedreno a3xx.xml / adreno_common.xml]
#define RBBM_HW_VERSION			0x0000
#define RBBM_CLOCK_CTL			0x0010
#define RBBM_SP_HYST_CNT		0x0012
#define RBBM_AHB_CTL0			0x0020
#define RBBM_AHB_CTL1			0x0021
#define RBBM_STATUS			0x0030
#define RBBM_STATUS_GPU_BUSY		(1u << 31)
#define RBBM_WAIT_IDLE_CLOCKS_CTL	0x0033
#define RBBM_INTERFACE_HANG_INT_CTL	0x0050
#define RBBM_INT_CLEAR_CMD		0x0061
#define RBBM_INT_0_MASK			0x0063
#define RBBM_INT_0_STATUS		0x0064
#define INT0_CACHE_FLUSH_TS		(1u << 20)
#define RBBM_PERFCTR_CTL		0x0080
#define RBBM_GPU_BUSY_MASKED		0x0088
#define RBBM_RBBM_CTL			0x0100
#define CP_RB_BASE			0x01c0
#define CP_RB_CNTL			0x01c1
#define CP_RB_RPTR			0x01c4
#define CP_RB_WPTR			0x01c5
#define CP_PFP_UCODE_ADDR		0x01c9
#define CP_PFP_UCODE_DATA		0x01ca
#define CP_QUEUE_THRESHOLDS		0x01d5
#define CP_ME_CNTL			0x01f6
#define CP_ME_RAM_WADDR			0x01f8
#define CP_ME_RAM_DATA			0x01fa
#define CP_DEBUG			0x01fc
#define CP_PROTECT_CTRL			0x045e
#define CP_PROTECT0			0x0460
#define CP_SCRATCH_REG2			0x057a
#define UCHE_CACHE_MODE_CONTROL_REG	0x0e82
#define SP_PERFCOUNTER6_SELECT		0x0eca
#define SP_PERFCOUNTER7_SELECT		0x0ecb
#define VBIF_OUT_RD_LIM_CONF0		0x3034
#define VBIF_OUT_WR_LIM_CONF0		0x3035
#define VBIF_ROUND_ROBIN_QOS_ARB	0x3049

// packets [freedreno adreno_pm4.xml]
#define PKT0(reg, cnt)	((((uint32_t)(cnt) - 1) << 16) | ((reg) & 0x7fff))
#define PKT2		0x80000000u
#define PKT3(op, cnt)	((3u << 30) | (((uint32_t)(cnt) - 1) << 16) | ((uint32_t)(op) << 8))
#define CP_NOP			0x10
#define CP_WAIT_FOR_IDLE	0x26
#define CP_INDIRECT_BUFFER_PFD	0x37
#define CP_MEM_WRITE		0x3d
#define CP_EVENT_WRITE		0x46
#define CP_ME_INIT		0x48
#define EV_CACHE_FLUSH_TS	0x04
#define EV_HLSQ_FLUSH		0x07
#define EV_IRQ			(1u << 31)

// observed init values [spec 4, 5]
static const uint32_t kProtect[13] = {
	0x63000040, 0x62000080, 0x600000cc, 0x60000108, 0x64000140, 0x66000400, 0x65000700,
	0x610007d8, 0x620007e0, 0x61001178, 0x64001180, 0x60003300, 0x6b00c000,
};
static const uint32_t kMeInit[17] = {
	0x000003f7, 0, 0, 0, 0x00000080, 0x00000100, 0x00000180, 0x00006600,
	0x00000150, 0x0000014e, 0x00000154, 0x00000001, 0, 0, 0, 0, 0,
};

#define FW_DIR "/Library/Firmware/adreno/"

static void *
smmuAlloc(void *ctx, uint32_t size, uint32_t align, uint64_t *phys)
{
	return ((PDAdreno *)ctx)->poolAllocPhys(size, align, phys);
}

bool
PDAdreno::mapPhys(uint64_t phys, uint32_t size, IOMemoryMap **map, volatile uint8_t **va)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(phys, size, kIODirectionInOut);

	*map = md != NULL ? md->map(kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(md);
	if (*map == NULL)
		return false;
	*va = (volatile uint8_t *)(*map)->getVirtualAddress();
	return true;
}

void *
PDAdreno::poolAlloc(uint32_t size, uint32_t align)
{
	uint32_t off = (fPoolUsed + align - 1) & ~(align - 1);

	if (off + size > kPoolSize)
		return NULL;
	fPoolUsed = off + size;
	bzero(fPool + off, size);
	return fPool + off;
}

void *
PDAdreno::poolAllocPhys(uint32_t size, uint32_t align, uint64_t *phys)
{
	void *p = poolAlloc(size, align);

	if (p != NULL)
		*phys = fPoolPhys + ((uint8_t *)p - fPool);
	return p;
}

bool
PDAdreno::start(IOService *provider)
{
	uint32_t enable = 0;
	thread_t th;

	if (!super::start(provider))
		return false;
	PE_parse_boot_argn("pdadreno", &enable, sizeof(enable));
	if (!enable) {
		IOLog("PDAdreno: off (boot-arg pdadreno=1 to start it)\n");
		return false;
	}

	fGpuMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	if (fGpuMap != NULL)
		fGpu = (volatile uint8_t *)fGpuMap->getVirtualAddress();
	else if (!mapPhys(GPU_PHYS, GPU_SIZE, &fGpuMap, &fGpu))
		return false;
	if (!mapPhys(GCC_PHYS, GCC_SIZE, &fGccMap, &fGcc))
		return false;
	volatile uint8_t *smmuBase;
	IOMemoryMap *smmuMap;
	if (!mapPhys(SMMU_PHYS, SMMU_SIZE, &smmuMap, &smmuBase))
		return false;
	// kept for the life of the driver, as the bank registers are
	(void)smmuMap;

	// one physically contiguous pool for everything the GPU sees, mapped uncached
	fPoolMD = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous, kPoolSize, 0xfffff000ULL);
	if (fPoolMD == NULL)
		return false;
	fPoolPhys = fPoolMD->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
	fPoolMD->performOperation(kIOMemoryIncoherentIOFlush, 0, kPoolSize);
	fPoolPhysMD = IOMemoryDescriptor::withPhysicalAddress(fPoolPhys, kPoolSize, kIODirectionInOut);
	fPoolMap = fPoolPhysMD != NULL ? fPoolPhysMD->map(kIOMapWriteCombineCache) : NULL;
	if (fPoolMap == NULL)
		return false;
	fPool = (uint8_t *)fPoolMap->getVirtualAddress();

	fSmmu = new PDAdrenoSmmu;
	if (fSmmu == NULL || !fSmmu->init(smmuBase, smmuAlloc, this))
		return false;

	if (kernel_thread_start(&PDAdreno::bringUpThread, this, &th) != KERN_SUCCESS)
		return false;
	thread_deallocate(th);
	registerService();
	return true;
}

void
PDAdreno::free(void)
{
	// the GPU may still be reading the pool, so a stopped driver leaks it
	super::free();
}

void
PDAdreno::bringUpThread(void *arg, wait_result_t wr)
{
	(void)wr;
	((PDAdreno *)arg)->bringUp();
	thread_terminate(current_thread());
}

bool
PDAdreno::loadFirmware(const char *path, uint32_t **words, uint32_t *count)
{
	vfs_context_t ctx = vfs_context_create(NULL);
	vnode_t vp = NULLVP;
	int err = ENOENT;

	// the root filesystem may not be mounted yet when this runs from start
	for (int tries = 0; tries < 120; tries++) {
		err = vnode_open(path, FREAD, 0, 0, &vp, ctx);
		if (err == 0)
			break;
		IOSleep(1000);
	}
	if (err != 0) {
		IOLog("PDAdreno: %s: error %d\n", path, err);
		vfs_context_rele(ctx);
		return false;
	}

	struct vnode_attr va;
	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	uint64_t size = 0;
	if (vnode_getattr(vp, &va, ctx) == 0)
		size = va.va_data_size;
	if (size < 8 || size > 64 * 1024 || (size & 3) != 0) {
		IOLog("PDAdreno: %s: unexpected size %llu\n", path, size);
		vnode_close(vp, FREAD, ctx);
		vfs_context_rele(ctx);
		return false;
	}

	uint32_t *buf = (uint32_t *)IOMalloc((vm_size_t)size);
	uio_t uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
	uio_addiov(uio, CAST_USER_ADDR_T(buf), (user_size_t)size);
	err = VNOP_READ(vp, uio, 0, ctx);
	if (err == 0 && uio_resid(uio) != 0)
		err = EIO;
	uio_free(uio);
	vnode_close(vp, FREAD, ctx);
	vfs_context_rele(ctx);
	if (err != 0) {
		IOFree(buf, (vm_size_t)size);
		IOLog("PDAdreno: %s: read error %d\n", path, err);
		return false;
	}
	*words = buf;
	*count = (uint32_t)(size / 4);
	IOLog("PDAdreno: %s: %u words, header 0x%08x\n", path, *count, buf[0]);
	return true;
}

// spec 4
bool
PDAdreno::hwInit(void)
{
	setGpu(VBIF_ROUND_ROBIN_QOS_ARB, 0x3);
	setGpu(VBIF_OUT_RD_LIM_CONF0, 0xa);
	setGpu(VBIF_OUT_WR_LIM_CONF0, 0xa);
	setGpu(RBBM_GPU_BUSY_MASKED, 0xffffffff);
	setGpu(RBBM_SP_HYST_CNT, 0x10);
	setGpu(RBBM_WAIT_IDLE_CLOCKS_CTL, 0x10);
	setGpu(RBBM_AHB_CTL0, 0x1);
	setGpu(RBBM_AHB_CTL1, 0xa6ffffff);
	setGpu(RBBM_RBBM_CTL, 0x30000);
	setGpu(RBBM_INTERFACE_HANG_INT_CTL, 0x10fff);
	setGpu(UCHE_CACHE_MODE_CONTROL_REG, 0x1);
	setGpu(RBBM_CLOCK_CTL, 0xaaaaaaaa);
	setGpu(RBBM_PERFCTR_CTL, 0x1);
	setGpu(SP_PERFCOUNTER6_SELECT, 0x1d);
	setGpu(SP_PERFCOUNTER7_SELECT, 0xe);
	setGpu(RBBM_INT_0_MASK, 0x0231ef22);

	fRing = (volatile uint32_t *)poolAlloc(kRingBytes, 4096);
	if (fRing == NULL)
		return false;
	fWptr = 0;
	setGpu(CP_RB_CNTL, 0x0800020c);		// 32 KB, rptr read from the register only
	setGpu(CP_RB_BASE, iova((const void *)fRing));

	setGpu(CP_PROTECT_CTRL, 0x7);
	for (int i = 0; i < 13; i++)
		setGpu(CP_PROTECT0 + i, kProtect[i]);
	setGpu(CP_DEBUG, 0x0a000000);

	// microcode: word 0 of each file is a header and is not uploaded [spec 4, FW]
	setGpu(CP_ME_RAM_WADDR, 0);
	for (uint32_t i = 1; i < fPm4Count; i++)
		*(volatile uint32_t *)(fGpu + CP_ME_RAM_DATA * 4) = fPm4[i];
	setGpu(CP_PFP_UCODE_ADDR, 0);
	for (uint32_t i = 1; i < fPfpCount; i++)
		*(volatile uint32_t *)(fGpu + CP_PFP_UCODE_DATA * 4) = fPfp[i];
	__asm__ volatile ("dsb sy" ::: "memory");

	setGpu(CP_QUEUE_THRESHOLDS, 0x000e0602);
	return true;
}

void
PDAdreno::ringEmit(uint32_t w)
{
	fRing[fWptr] = w;
	fWptr = (fWptr + 1) % (kRingBytes / 4);
}

bool
PDAdreno::ringKick(uint32_t timeoutMs)
{
	__asm__ volatile ("dsb sy" ::: "memory");
	setGpu(CP_RB_WPTR, fWptr);
	for (uint32_t us = 0; us < timeoutMs * 1000; us += 10) {
		if (gpu(CP_RB_RPTR) == fWptr)
			return true;
		IODelay(10);
	}
	IOLog("PDAdreno: ring stuck: rptr %u wptr %u rbbm_status 0x%08x int0 0x%08x\n",
	    gpu(CP_RB_RPTR), fWptr, gpu(RBBM_STATUS), gpu(RBBM_INT_0_STATUS));
	fSmmu->dumpFault();
	return false;
}

// spec 5
bool
PDAdreno::cpStart(void)
{
	setGpu(CP_ME_CNTL, 0);
	ringEmit(PKT3(CP_ME_INIT, 17));
	for (int i = 0; i < 17; i++)
		ringEmit(kMeInit[i]);
	if (!ringKick(1000))
		return false;
	for (int i = 0; i < 1000 && (gpu(RBBM_STATUS) & RBBM_STATUS_GPU_BUSY); i++)
		IODelay(10);
	IOLog("PDAdreno: CP running: rptr %u, rbbm_status 0x%08x\n", gpu(CP_RB_RPTR), gpu(RBBM_STATUS));
	return true;
}

// spec 6: an IB that writes a word to memory, then the fence sequence
bool
PDAdreno::testSubmit(void)
{
	uint32_t *ib = (uint32_t *)poolAlloc(4096, 4096);
	uint32_t n = 0, seq = ++fSeqno;

	if (ib == NULL)
		return false;
	*fScratch = 0;
	*fFence = 0;
	ib[n++] = PKT3(CP_MEM_WRITE, 2);
	ib[n++] = iova((const void *)fScratch);
	ib[n++] = 0xcafe1234;
	ib[n++] = PKT3(CP_NOP, 1);
	ib[n++] = 0;

	ringEmit(PKT3(CP_INDIRECT_BUFFER_PFD, 2));
	ringEmit(iova(ib));
	ringEmit(n);
	ringEmit(PKT2);
	ringEmit(PKT0(CP_SCRATCH_REG2, 1));
	ringEmit(seq);
	ringEmit(PKT3(CP_EVENT_WRITE, 1));
	ringEmit(EV_HLSQ_FLUSH);
	ringEmit(PKT3(CP_WAIT_FOR_IDLE, 1));
	ringEmit(0);
	ringEmit(PKT3(CP_EVENT_WRITE, 3));
	ringEmit(EV_IRQ | EV_CACHE_FLUSH_TS);
	ringEmit(iova((const void *)fFence));
	ringEmit(seq);
	if (!ringKick(1000))
		return false;

	for (int i = 0; i < 100000 && *fFence != seq; i++)
		IODelay(10);
	uint32_t st = gpu(RBBM_INT_0_STATUS);
	IOLog("PDAdreno: test submit: fence %u (want %u), scratch mem 0x%08x, scratch reg2 %u, int0 0x%08x\n",
	    *fFence, seq, *fScratch, gpu(CP_SCRATCH_REG2), st);
	setGpu(RBBM_INT_CLEAR_CMD, st);
	return *fFence == seq && *fScratch == 0xcafe1234;
}

void
PDAdreno::bringUp(void)
{
	if (!socSmmuClocksOn()) {
		IOLog("PDAdreno: smmu clocks failed\n");
		return;
	}
	if (!socSecureSetup()) {
		IOLog("PDAdreno: secure smmu setup failed\n");
		return;
	}
	// the whole pool at kPoolIova, then both GPU context banks on the one table
	if (!fSmmu->map(kPoolIova, fPoolPhys, kPoolSize) || !fSmmu->attach(1, 1) || !fSmmu->attach(2, 2)) {
		IOLog("PDAdreno: smmu setup failed\n");
		return;
	}
	if (!socPowerOn()) {
		IOLog("PDAdreno: power on failed\n");
		return;
	}
	IOLog("PDAdreno: powered, hw version 0x%08x\n", gpu(RBBM_HW_VERSION));

	fFence = (volatile uint32_t *)poolAlloc(4096, 4096);
	fScratch = fFence + 16;
	if (fFence == NULL)
		return;
	if (!loadFirmware(FW_DIR "a300_pm4.fw", &fPm4, &fPm4Count) ||
	    !loadFirmware(FW_DIR "a300_pfp.fw", &fPfp, &fPfpCount))
		return;
	if (!hwInit() || !cpStart()) {
		IOLog("PDAdreno: CP did not start\n");
		return;
	}
	bool ok = testSubmit();
	IOLog("PDAdreno: bring-up %s\n", ok ? "OK" : "FAILED");
	setProperty("PDAdrenoBringUp", ok ? "ok" : "failed");
}
