// SPDX-License-Identifier: MIT
// msm8916 glue for the adreno: gcc clocks, oxili gdsc and secure smmu setup, from spec sections 2 and 3.1
#include "PDAdreno.h"
#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>

// clock controller registers [spec 2]: CBCR bit 0 enable and bit 31 clock off, RCG command bit 0 update,
// GDSC bit 0 collapse and bit 31 power on
#define CBCR_EN		(1u << 0)
#define CBCR_OFF	(1u << 31)
#define RCG_UPDATE	(1u << 0)
#define GDSC_COLLAPSE	(1u << 0)
#define GDSC_PWR_ON	(1u << 31)

#define GCC_APCS_CLOCK_VOTE	0x4500c
#define VOTE_SMMU_CFG		(1u << 12)
#define VOTE_GFX_TCU		(1u << 2)
#define GCC_SMMU_CFG_CBCR	0x12038
#define GCC_GFX_TCU_CBCR	0x12020
#define GCC_OXILI_GDSC		0x5901c
#define GCC_GFX3D_CMD_RCGR	0x59000
#define GCC_GFX3D_CFG_RCGR	0x59004
#define GFX3D_CFG_400MHZ	0x303		// source 3 (800 MHz), divide by 2
#define GCC_OXILI_GFX3D_CBCR	0x59020
#define GCC_OXILI_GMEM_CBCR	0x59024
#define GCC_OXILI_AHB_CBCR	0x59028
#define GCC_BIMC_GFX_CBCR	0x31024
#define GCC_BIMC_GPU_CBCR	0x31040

// secure calls, SMC32: x0 function, x1 argument descriptor, x2.. arguments [spec 3.1]
#define SCM_IOMMU_SECURE_PTBL_SIZE	0x02000c03u
#define SCM_IOMMU_SECURE_PTBL_INIT	0x02000c04u
#define SCM_RESTORE_SEC_CFG		0x02000c02u
#define GPU_SMMU_SECURE_ID		0x12
#define SECURE_PTBL_SIZE		0x17000u
// a no-map carve-out in the device tree, never touched by the OS once handed over
#define SECURE_PTBL_PHYS		0x9bf00000ull

struct ScmResult {
	uint64_t x0, x1, x2, x3;
};

static ScmResult
scmCall(uint64_t fn, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
{
	register uint64_t x0 __asm__ ("x0") = fn;
	register uint64_t x1 __asm__ ("x1") = a1;
	register uint64_t x2 __asm__ ("x2") = a2;
	register uint64_t x3 __asm__ ("x3") = a3;
	register uint64_t x4 __asm__ ("x4") = a4;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4) : :
	    "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory");
	return (ScmResult){ x0, x1, x2, x3 };
}

bool
PDAdreno::gccPoll(uint32_t off, uint32_t mask, uint32_t want, uint32_t timeoutUs)
{
	for (uint32_t us = 0; us < timeoutUs; us += 10) {
		if ((gcc(off) & mask) == want)
			return true;
		IODelay(10);
	}
	IOLog("PDAdreno: gcc 0x%05x stuck at 0x%08x (want 0x%x under 0x%x)\n", off, gcc(off), want, mask);
	return false;
}

bool
PDAdreno::socSecureSetup(void)
{
	uint64_t phys = SECURE_PTBL_PHYS;
	ScmResult r;

	PE_parse_boot_argn("pdadrenosec", &phys, sizeof(phys));
	r = scmCall(SCM_IOMMU_SECURE_PTBL_SIZE, 1, 0, 0, 0);
	IOLog("PDAdreno: secure ptbl size call: %llx %llx %llx\n", r.x0, r.x1, r.x2);
	// which result register carries the size was not observed, so this only reports it
	if (r.x0 != 0)
		return false;
	r = scmCall(SCM_IOMMU_SECURE_PTBL_INIT, 0x23, phys, SECURE_PTBL_SIZE, 0);
	IOLog("PDAdreno: secure ptbl init at 0x%llx: %llx %llx\n", phys, r.x0, r.x1);
	if (r.x0 != 0)
		return false;
	for (uint32_t bank = 1; bank <= 2; bank++) {
		r = scmCall(SCM_RESTORE_SEC_CFG, 2, GPU_SMMU_SECURE_ID, bank, 0);
		IOLog("PDAdreno: restore sec cfg 0x%x bank %u: %llx %llx\n", GPU_SMMU_SECURE_ID, bank, r.x0, r.x1);
		if (r.x0 != 0)
			return false;
	}
	return true;
}

static bool
cbcrOn(volatile uint8_t *gccBase, uint32_t off)
{
	volatile uint32_t *r = (volatile uint32_t *)(gccBase + off);

	*r = *r | CBCR_EN;
	__asm__ volatile ("dsb sy" ::: "memory");
	for (int us = 0; us < 2000; us += 10) {
		if ((*r & CBCR_OFF) == 0)
			return true;
		IODelay(10);
	}
	IOLog("PDAdreno: clock 0x%05x did not start (0x%08x)\n", off, *r);
	return false;
}

// spec 2 step 1: the SMMU's clocks, needed before its secure setup and bank programming
bool
PDAdreno::socSmmuClocksOn(void)
{
	setGcc(GCC_APCS_CLOCK_VOTE, gcc(GCC_APCS_CLOCK_VOTE) | VOTE_SMMU_CFG | VOTE_GFX_TCU);
	return gccPoll(GCC_SMMU_CFG_CBCR, CBCR_OFF, 0, 2000) && gccPoll(GCC_GFX_TCU_CBCR, CBCR_OFF, 0, 2000);
}

// spec 2 steps 2-4
bool
PDAdreno::socPowerOn(void)
{

	IOLog("PDAdreno: oxili gdsc 0x%08x\n", gcc(GCC_OXILI_GDSC));
	setGcc(GCC_OXILI_GDSC, gcc(GCC_OXILI_GDSC) & ~GDSC_COLLAPSE);
	if (!gccPoll(GCC_OXILI_GDSC, GDSC_PWR_ON, GDSC_PWR_ON, 10000))
		return false;

	setGcc(GCC_GFX3D_CFG_RCGR, GFX3D_CFG_400MHZ);
	setGcc(GCC_GFX3D_CMD_RCGR, gcc(GCC_GFX3D_CMD_RCGR) | RCG_UPDATE);
	if (!gccPoll(GCC_GFX3D_CMD_RCGR, RCG_UPDATE, 0, 2000))
		return false;

	return cbcrOn(fGcc, GCC_OXILI_GFX3D_CBCR) && cbcrOn(fGcc, GCC_OXILI_AHB_CBCR) &&
	    cbcrOn(fGcc, GCC_OXILI_GMEM_CBCR) && cbcrOn(fGcc, GCC_BIMC_GFX_CBCR) &&
	    cbcrOn(fGcc, GCC_BIMC_GPU_CBCR);
}
