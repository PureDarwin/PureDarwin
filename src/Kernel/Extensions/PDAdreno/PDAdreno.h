// SPDX-License-Identifier: MIT
// qualcomm adreno gpu core, clean room from the adreno-cleanroom spec (no gpl source)
#ifndef _PUREDARWIN_PDADRENO_H
#define _PUREDARWIN_PDADRENO_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

// power, smmu, cp microcode and one polled, fenced test submission, the msm uapi goes on top
// gated by the boot-arg pdadreno=1

class PDAdrenoSmmu;

class PDAdreno : public IOService
{
	OSDeclareDefaultStructors(PDAdreno);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;

	// GPU VA of a pool address (the pool is mapped once, contiguously, at kPoolIova)
	uint32_t iova(const void *p) const
	{
		return kPoolIova + (uint32_t)((const uint8_t *)p - fPool);
	}

	void *poolAllocPhys(uint32_t size, uint32_t align, uint64_t *phys);

	enum {
		kPoolSize = 1024 * 1024,
		kPoolIova = 0x01000000,
		kRingBytes = 32 * 1024,
	};

private:
	static void bringUpThread(void *arg, wait_result_t wr);
	void bringUp(void);

	// SoC glue (PDAdrenoMsm8916.cpp)
	bool socSmmuClocksOn(void);
	bool socSecureSetup(void);
	bool socPowerOn(void);

	// GPU (this file)
	bool loadFirmware(const char *path, uint32_t **words, uint32_t *count);
	bool hwInit(void);
	bool cpStart(void);
	bool testSubmit(void);
	void ringEmit(uint32_t w);
	bool ringKick(uint32_t timeoutMs);

	void *poolAlloc(uint32_t size, uint32_t align);
	bool mapPhys(uint64_t phys, uint32_t size, IOMemoryMap **map, volatile uint8_t **va);

	uint32_t gpu(uint32_t dw) const { return *(volatile uint32_t *)(fGpu + dw * 4); }
	void setGpu(uint32_t dw, uint32_t v) const
	{
		*(volatile uint32_t *)(fGpu + dw * 4) = v;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	uint32_t gcc(uint32_t off) const { return *(volatile uint32_t *)(fGcc + off); }
	void setGcc(uint32_t off, uint32_t v) const
	{
		*(volatile uint32_t *)(fGcc + off) = v;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	bool gccPoll(uint32_t off, uint32_t mask, uint32_t want, uint32_t timeoutUs);

	IOMemoryMap *fGpuMap, *fGccMap;
	volatile uint8_t *fGpu, *fGcc;
	PDAdrenoSmmu *fSmmu;

	IOBufferMemoryDescriptor *fPoolMD;
	IOMemoryDescriptor *fPoolPhysMD;
	IOMemoryMap *fPoolMap;
	uint8_t *fPool;
	uint64_t fPoolPhys;
	uint32_t fPoolUsed;

	uint32_t *fPm4, *fPfp;
	uint32_t fPm4Count, fPfpCount;

	volatile uint32_t *fRing;
	uint32_t fWptr;
	volatile uint32_t *fFence;	// written by CACHE_FLUSH_TS events
	volatile uint32_t *fScratch;	// target of the test IB's memory write
	uint32_t fSeqno;
};

#endif
