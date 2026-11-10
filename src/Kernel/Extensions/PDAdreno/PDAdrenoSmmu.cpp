// SPDX-License-Identifier: MIT
// arm smmu context bank programming and lpae page tables for the adreno
#include "PDAdrenoSmmu.h"

// stage-1 context bank registers [ARM SMMU v2, spec 3.2]
#define CB_SCTLR	0x000
#define CB_TCR2		0x010
#define CB_TTBR0	0x020
#define CB_TTBR1	0x028
#define CB_TCR		0x030
#define CB_MAIR0	0x038
#define CB_MAIR1	0x03c
#define CB_FSR		0x058
#define CB_FAR		0x060
#define CB_FSYNR0	0x068
#define CB_TLBIASID	0x610
#define CB_TLBSYNC	0x7f0
#define CB_TLBSTATUS	0x7f4
#define TLBSTATUS_SACTIVE	(1u << 0)

// values the bank is programmed with [spec 3.2]
#define SCTLR_ON	0x000010e7u
#define TCR_VAL		0x80802020u	// 32-bit input range, non-cacheable outer-shareable walks
#define TCR2_VAL	0x00038002u
#define MAIR0_VAL	0xf404ff44u	// attr0 = normal non-cacheable
#define MAIR1_VAL	0xffffffffu
#define FSR_ALL		0xc00001feu

// VMSAv8-64 descriptors, 4 KB granule [Arm ARM D8]
#define DESC_VALID_TABLE	0x3ull		// table (levels 1, 2) or page (level 3)
#define DESC_ATTRINDX(n)	((uint64_t)(n) << 2)
#define DESC_AP_RW_ANY		(1ull << 6)
#define DESC_SH_OUTER		(2ull << 8)
#define DESC_AF			(1ull << 10)
#define DESC_NG			(1ull << 11)
#define DESC_ADDR_MASK		0x0000fffffffff000ull

uint32_t
PDAdrenoSmmu::cb32(uint32_t bank, uint32_t off) const
{
	return *(volatile uint32_t *)(fBase + bank * 0x1000 + off);
}

void
PDAdrenoSmmu::setCb32(uint32_t bank, uint32_t off, uint32_t v) const
{
	*(volatile uint32_t *)(fBase + bank * 0x1000 + off) = v;
	__asm__ volatile ("dsb sy" ::: "memory");
}

void
PDAdrenoSmmu::setCb64(uint32_t bank, uint32_t off, uint64_t v) const
{
	*(volatile uint64_t *)(fBase + bank * 0x1000 + off) = v;
	__asm__ volatile ("dsb sy" ::: "memory");
}

bool
PDAdrenoSmmu::init(volatile uint8_t *base, AllocFn alloc, void *ctx)
{
	fBase = base;
	fAlloc = alloc;
	fCtx = ctx;
	fBankCount = 0;
	fTabCount = 0;
	fL1 = (uint64_t *)fAlloc(fCtx, 4096, 4096, &fL1Phys);
	if (fL1 == NULL)
		return false;
	fTabs[fTabCount++] = (Tab){ fL1, fL1Phys };
	return true;
}

// the next-level table under parent[index], created on first use
uint64_t *
PDAdrenoSmmu::table(uint64_t *parent, uint32_t index)
{
	uint64_t d = parent[index];

	if (d & 1) {
		uint64_t phys = d & DESC_ADDR_MASK;
		for (uint32_t i = 0; i < fTabCount; i++) {
			if (fTabs[i].phys == phys)
				return fTabs[i].va;
		}
		return NULL;
	}
	if (fTabCount == sizeof(fTabs) / sizeof(fTabs[0]))
		return NULL;
	uint64_t phys;
	uint64_t *t = (uint64_t *)fAlloc(fCtx, 4096, 4096, &phys);
	if (t == NULL)
		return NULL;
	fTabs[fTabCount++] = (Tab){ t, phys };
	parent[index] = (phys & DESC_ADDR_MASK) | DESC_VALID_TABLE;
	return t;
}

bool
PDAdrenoSmmu::map(uint32_t iova, uint64_t phys, uint32_t size)
{
	for (uint32_t off = 0; off < size; off += 4096) {
		uint32_t va = iova + off;
		uint64_t *l2 = table(fL1, (va >> 30) & 0x3);
		uint64_t *l3 = l2 != NULL ? table(l2, (va >> 21) & 0x1ff) : NULL;
		if (l3 == NULL)
			return false;
		l3[(va >> 12) & 0x1ff] = ((phys + off) & DESC_ADDR_MASK) | DESC_VALID_TABLE |
		    DESC_ATTRINDX(0) | DESC_AP_RW_ANY | DESC_SH_OUTER | DESC_AF | DESC_NG;
	}
	__asm__ volatile ("dsb sy" ::: "memory");
	flush();
	return true;
}

void
PDAdrenoSmmu::flush(void)
{
	for (uint32_t i = 0; i < fBankCount; i++) {
		setCb32(fBanks[i], CB_TLBIASID, fAsid[i]);
		setCb32(fBanks[i], CB_TLBSYNC, 0);
		for (int us = 0; us < 10000 && (cb32(fBanks[i], CB_TLBSTATUS) & TLBSTATUS_SACTIVE); us++)
			IODelay(1);
	}
}

bool
PDAdrenoSmmu::attach(uint32_t bank, uint16_t asid)
{
	if (fBankCount == sizeof(fBanks) / sizeof(fBanks[0]))
		return false;
	setCb32(bank, CB_SCTLR, 0);
	setCb64(bank, CB_FAR, 0);
	setCb32(bank, CB_FSR, FSR_ALL);
	setCb64(bank, CB_TTBR0, ((uint64_t)asid << 48) | fL1Phys);
	setCb64(bank, CB_TTBR1, 0);
	setCb32(bank, CB_TCR2, TCR2_VAL);
	setCb32(bank, CB_TCR, TCR_VAL);
	setCb32(bank, CB_MAIR0, MAIR0_VAL);
	setCb32(bank, CB_MAIR1, MAIR1_VAL);
	setCb32(bank, CB_SCTLR, SCTLR_ON);
	fBanks[fBankCount] = bank;
	fAsid[fBankCount] = asid;
	fBankCount++;
	IOLog("PDAdreno: smmu bank %u on, asid %u, table 0x%llx, sctlr 0x%x tcr 0x%x\n",
	    bank, asid, fL1Phys, cb32(bank, CB_SCTLR), cb32(bank, CB_TCR));
	return true;
}

void
PDAdrenoSmmu::dumpFault(void)
{
	for (uint32_t i = 0; i < fBankCount; i++) {
		uint32_t b = fBanks[i];
		IOLog("PDAdreno: smmu bank %u fsr 0x%x far 0x%08x%08x fsynr0 0x%x\n", b,
		    cb32(b, CB_FSR), cb32(b, CB_FAR + 4), cb32(b, CB_FAR), cb32(b, CB_FSYNR0));
	}
}
