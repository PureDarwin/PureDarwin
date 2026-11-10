// SPDX-License-Identifier: MIT
// gpu stage-1 translation through the soc arm smmu context banks
#ifndef _PUREDARWIN_PDADRENOSMMU_H
#define _PUREDARWIN_PDADRENOSMMU_H

#include <IOKit/IOLib.h>

// vmsav8-64 4 kb granule, 32-bit input so the walk starts at level 1, one table shared by all banks
// table memory is mapped uncached, the walker is non-cacheable (spec 3.2)
class PDAdrenoSmmu
{
public:
	typedef void *(*AllocFn)(void *ctx, uint32_t size, uint32_t align, uint64_t *phys);

	bool init(volatile uint8_t *base, AllocFn alloc, void *ctx);
	// program and enable context bank n (n * 0x1000 into the SMMU window)
	bool attach(uint32_t bank, uint16_t asid);
	// map [iova, iova + size) to [phys, phys + size), 4 KB pages
	bool map(uint32_t iova, uint64_t phys, uint32_t size);
	void flush(void);
	void dumpFault(void);

private:
	uint64_t *table(uint64_t *parent, uint32_t index);
	uint32_t cb32(uint32_t bank, uint32_t off) const;
	void setCb32(uint32_t bank, uint32_t off, uint32_t v) const;
	void setCb64(uint32_t bank, uint32_t off, uint64_t v) const;

	volatile uint8_t *fBase;
	AllocFn fAlloc;
	void *fCtx;
	uint64_t *fL1;
	uint64_t fL1Phys;
	uint32_t fBanks[4];
	uint16_t fAsid[4];
	uint32_t fBankCount;
	// table virtual -> physical bookkeeping, a handful of tables at most
	struct Tab { uint64_t *va; uint64_t phys; };
	Tab fTabs[32];
	uint32_t fTabCount;
};

#endif
