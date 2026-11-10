// SPDX-License-Identifier: MIT
// tegra210 wdt0 keep-alive for a watchdog the loader armed

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>
#include "PDTegraWatchdog.h"

// wdt0 sits 0x100 into the timer block, l4t trees name it directly as nvidia,tegra-wdt
#define WDT_CFG			0x00
#define WDT_CMD			0x08
#define WDT_CFG_SYS_RST_EN	(1U << 14)
#define WDT_CFG_PMC2CAR_RST_EN	(1U << 15)
#define WDT_CMD_START		(1U << 0)
#define WDT_PET_MS		5000

extern "C" void pd_dump_threads(void);

static IOMemoryMap *gTegraWdtMap;
static volatile uint32_t *gTegraWdt;
static thread_call_t gTegraWdtCall;
static uint64_t gTegraWdtDeadline;

static void
tegra_wdt_pet(thread_call_param_t, thread_call_param_t)
{
	uint64_t next;

	// past a pdtegrawdt deadline the petting stops and the armed reset follows
	if (gTegraWdtDeadline != 0 && mach_absolute_time() >= gTegraWdtDeadline) {
		IOLog("PDTegraWatchdog: pdtegrawdt deadline, no more pets\n");
		// what every thread waits on, read back from the ram log after the reset
		pd_dump_threads();
		return;
	}
	// a start command on a running counter reloads it
	gTegraWdt[WDT_CMD / 4] = WDT_CMD_START;
	__asm__ volatile ("dsb sy" ::: "memory");
	clock_interval_to_deadline(WDT_PET_MS, kMillisecondScale, &next);
	thread_call_enter_delayed(gTegraWdtCall, next);
}

// true when a nul-separated compatible list names want
static bool
tegra_compat_has(OSData *compat, const char *want)
{
	const char *s = compat != NULL ? (const char *)compat->getBytesNoCopy() : NULL;
	unsigned int len = compat != NULL ? compat->getLength() : 0;

	for (unsigned int o = 0; s != NULL && o < len; o += strnlen(s + o, len - o) + 1) {
		if (strncmp(s + o, want, len - o) == 0) return true;
	}
	return false;
}

// pmc words through the firmware's sip call, the pmc is secure on this board
static uint32_t
tegra_pmc_read(uint32_t off)
{
	register uint64_t x0 __asm__("x0") = 0xc2fffe00ull;
	register uint64_t x1 __asm__("x1") = 0xaa;
	register uint64_t x2 __asm__("x2") = off;
	register uint64_t x3 __asm__("x3") = 0;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
	return x0 == 0 ? (uint32_t)x1 : 0xdeadbeef;
}

// read-only gpu bring-up state: car offsets from freebsd tegra210_car.h, pmc from tegra210_pmc.c
static void
tegra_gpu_state(void)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(0x60006000, 0x1000, kIODirectionIn);
	IOMemoryMap *map = desc != NULL ? desc->map(kIOMapAnywhere | kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(desc);
	if (map == NULL) return;
	volatile uint32_t *car = (volatile uint32_t *)map->getVirtualAddress();

	uint32_t enbL = car[0x010 / 4], rstL = car[0x004 / 4], enbH = car[0x014 / 4], rstH = car[0x008 / 4];
	IOLog("PDTegraGPU: car osc_ctrl 0x%08x spare0 0x%08x pllp_base 0x%08x pllp_outc 0x%08x\n",
	    car[0x050 / 4], car[0x55c / 4], car[0x0a0 / 4], car[0x67c / 4]);
	IOLog("PDTegraGPU: car enb_l 0x%08x rst_l 0x%08x enb_h 0x%08x rst_h 0x%08x enb_x 0x%08x rst_x 0x%08x\n",
	    enbL, rstL, enbH, rstH, car[0x280 / 4], car[0x28c / 4]);
	IOLog("PDTegraGPU: car src_pwm 0x%08x src_i2c5 0x%08x\n", car[0x110 / 4], car[0x128 / 4]);
	IOLog("PDTegraGPU: pmc pwrgate_status 0x%08x clamp_status 0x%08x gpu_rg_cntrl 0x%08x\n",
	    tegra_pmc_read(0x38), tegra_pmc_read(0x2c), tegra_pmc_read(0x2d4));
	OSSafeReleaseNULL(map);

	// the pwm block is read only with its clock on (L17) and out of reset
	if ((enbL & (1U << 17)) == 0 || (rstL & (1U << 17)) != 0) {
		IOLog("PDTegraGPU: pwm clock off or in reset, not read\n");
		return;
	}
	desc = IOMemoryDescriptor::withPhysicalAddress(0x7000a000, 0x1000, kIODirectionIn);
	map = desc != NULL ? desc->map(kIOMapAnywhere | kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(desc);
	if (map == NULL) return;
	volatile uint32_t *pwm = (volatile uint32_t *)map->getVirtualAddress();
	IOLog("PDTegraGPU: pwm ch0 0x%08x ch1 0x%08x\n", pwm[0], pwm[0x10 / 4]);
	OSSafeReleaseNULL(map);
}

void
PDTegraWatchdog_startFromDeviceTree(void)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	OSIterator *iter = root != NULL ? IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively) : NULL;
	IORegistryEntry *entry;
	uint64_t phys = 0;
	uint32_t on = 1, cfg;

	while (phys == 0 && iter != NULL && (entry = (IORegistryEntry *)iter->getNextObject()) != NULL) {
		OSData *compat = OSDynamicCast(OSData, entry->getProperty("compatible"));
		OSData *reg = OSDynamicCast(OSData, entry->getProperty("reg"));
		uint64_t off;

		if (tegra_compat_has(compat, "nvidia,tegra-wdt")) off = 0;
		else if (tegra_compat_has(compat, "nvidia,tegra210-timer")) off = 0x100;
		else continue;
		if (reg != NULL && reg->getLength() >= sizeof(uint64_t))
			phys = *(const uint64_t *)reg->getBytesNoCopy() + off;
	}
	OSSafeReleaseNULL(iter);
	OSSafeReleaseNULL(root);
	if (phys == 0) return;
	tegra_gpu_state();

	// pdtegrawdt=0 leaves the loader's watchdog to fire, pdtegrawdt=SECONDS above 1 pets that long
	if (PE_parse_boot_argn("pdtegrawdt", &on, sizeof(on)) && on == 0) {
		IOLog("PDTegraWatchdog: wdt0 at 0x%llx left alone by pdtegrawdt=0\n", (unsigned long long)phys);
		return;
	}

	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(phys & ~0xfffULL, 0x1000, kIODirectionOutIn);
	if (desc == NULL) return;
	gTegraWdtMap = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (gTegraWdtMap == NULL) return;
	gTegraWdt = (volatile uint32_t *)(gTegraWdtMap->getVirtualAddress() + (phys & 0xfffULL));

	// a reset enable means someone armed it, petting an idle wdt0 would start it
	cfg = gTegraWdt[WDT_CFG / 4];
	if ((cfg & (WDT_CFG_SYS_RST_EN | WDT_CFG_PMC2CAR_RST_EN)) == 0) {
		IOLog("PDTegraWatchdog: wdt0 at 0x%llx not armed (cfg 0x%x)\n", (unsigned long long)phys, cfg);
		OSSafeReleaseNULL(gTegraWdtMap);
		gTegraWdt = NULL;
		return;
	}
	gTegraWdtCall = thread_call_allocate(tegra_wdt_pet, NULL);
	if (gTegraWdtCall == NULL) return;
	if (on > 1) clock_interval_to_deadline(on, kSecondScale, &gTegraWdtDeadline);
	IOLog("PDTegraWatchdog: wdt0 at 0x%llx armed (cfg 0x%x), petting every %u ms for %u s (1: always)\n",
	    (unsigned long long)phys, cfg, WDT_PET_MS, on);
	tegra_wdt_pet(NULL, NULL);
}
