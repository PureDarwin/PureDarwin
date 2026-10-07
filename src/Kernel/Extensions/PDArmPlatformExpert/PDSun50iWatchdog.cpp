// allwinner watchdog as a test harness, pdwdt=<seconds> keeps feeding it until the deadline
// then lets the soc reset, a hang or panic stops the feeding early

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <pexpert/pexpert.h>
#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>
#include "PDSun50iWatchdog.h"

#define SUN50I_WDT_PHYS		0x030090a0ULL
#define WDT_CTRL_RESTART	((0xa57U << 1) | 1U)
#define WDT_CFG_SYSTEM_RESET	1U
#define WDT_MODE_16S		((11U << 4) | 1U)
#define WDT_FEED_MS		4000

// where the control, config and mode registers sit, and the key config and mode writes carry
struct PDWdtLayout {
	uint32_t ctrl, cfg, mode, key;
};

static const PDWdtLayout kH616Layout = { 0x10, 0x14, 0x18, 0 };
// the a733's wdt-v103, read back on the board: four bytes lower, and cfg and mode drop writes
// without 0x16aa in the upper half
static const PDWdtLayout kV103Layout = { 0x0c, 0x10, 0x14, 0x16aa0000U };

static IOMemoryMap *gWdtMap;
static volatile uint8_t *gWdt;
static const PDWdtLayout *gWdtLayout = &kH616Layout;
static thread_call_t gWdtCall;
static uint64_t gWdtDeadline;

static inline void
wdt_wr(uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(gWdt + off) = val;
	__asm__ volatile ("dsb sy" ::: "memory");
}

// a watchdog reset may not reach every bus master, so drivers marked pd-quiesce stop their dma
// first, as for a restart. pdnoquiesce=1 skips it, to see what a reset leaves running
static void
wdt_quiesce_drivers(void)
{
	const OSSymbol *key = OSSymbol::withCString("pd-quiesce");
	OSDictionary *match = key != NULL ? IOService::propertyMatching(key, kOSBooleanTrue) : NULL;
	OSIterator *iter = match != NULL ? IOService::getMatchingServices(match) : NULL;
	IOService *svc;
	uint32_t skip = 0, n = 0;

	if (PE_parse_boot_argn("pdnoquiesce", &skip, sizeof(skip)) && skip != 0) {
		IOLog("PDSun50iWatchdog: deadline, quiesce skipped by pdnoquiesce\n");
	} else {
		while (iter != NULL && (svc = OSDynamicCast(IOService, iter->getNextObject())) != NULL) {
			svc->message(kIOMessageSystemWillRestart, NULL, NULL);
			n++;
		}
		IOLog("PDSun50iWatchdog: deadline, %u drivers quiesced, reset in 16 s\n", n);
	}
	OSSafeReleaseNULL(iter);
	OSSafeReleaseNULL(match);
	OSSafeReleaseNULL(key);
}

static void
wdt_feed(thread_call_param_t, thread_call_param_t)
{
	uint64_t next;

	// past the deadline the feeding stops and the watchdog resets the board 16s later
	if (mach_absolute_time() >= gWdtDeadline) {
		wdt_quiesce_drivers();
		return;
	}
	wdt_wr(gWdtLayout->ctrl, WDT_CTRL_RESTART);
	clock_interval_to_deadline(WDT_FEED_MS, kMillisecondScale, &next);
	thread_call_enter_delayed(gWdtCall, next);
}

static void
wdt_start(uint64_t phys, const PDWdtLayout *layout)
{
	uint32_t secs = 0;
	IOMemoryDescriptor *desc;

	if (!PE_parse_boot_argn("pdwdt", &secs, sizeof(secs)) || secs == 0)
		return;

	gWdtLayout = layout;
	desc = IOMemoryDescriptor::withPhysicalAddress(phys & ~0xfffULL,
	    0x1000, kIODirectionOutIn);
	if (desc == NULL)
		return;
	gWdtMap = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (gWdtMap == NULL)
		return;
	gWdt = (volatile uint8_t *)gWdtMap->getVirtualAddress() + (phys & 0xfffULL);

	gWdtCall = thread_call_allocate(wdt_feed, NULL);
	if (gWdtCall == NULL)
		return;
	clock_interval_to_deadline(secs, kSecondScale, &gWdtDeadline);

	wdt_wr(layout->cfg, layout->key | WDT_CFG_SYSTEM_RESET);
	wdt_wr(layout->mode, layout->key | WDT_MODE_16S);
	wdt_wr(layout->ctrl, WDT_CTRL_RESTART);
	IOLog("PDSun50iWatchdog: armed at 0x%llx, fed for %u s\n", (unsigned long long)phys, secs);
	wdt_feed(NULL, NULL);
}

void
PDSun50iWatchdog_start(void)
{
	wdt_start(SUN50I_WDT_PHYS, &kH616Layout);
}

// true when a nul-separated compatible list names want
static bool
wdt_compat_has(OSData *compat, const char *want)
{
	const char *s = compat != NULL ? (const char *)compat->getBytesNoCopy() : NULL;
	unsigned int len = compat != NULL ? compat->getLength() : 0;

	for (unsigned int o = 0; s != NULL && o < len; o += strnlen(s + o, len - o) + 1) {
		if (strncmp(s + o, want, len - o) == 0)
			return true;
	}
	return false;
}

// a board described by its own tree: the watchdog is whichever node says wdt-v103
void
PDSun50iWatchdog_startFromDeviceTree(void)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	OSIterator *iter = root != NULL ? IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively) : NULL;
	IORegistryEntry *entry;
	uint64_t phys = 0;

	while (phys == 0 && iter != NULL && (entry = (IORegistryEntry *)iter->getNextObject()) != NULL) {
		OSData *reg = OSDynamicCast(OSData, entry->getProperty("reg"));
		if (!wdt_compat_has(OSDynamicCast(OSData, entry->getProperty("compatible")), "allwinner,wdt-v103"))
			continue;
		if (reg != NULL && reg->getLength() >= sizeof(uint64_t))
			phys = *(const uint64_t *)reg->getBytesNoCopy();
	}
	OSSafeReleaseNULL(iter);
	OSSafeReleaseNULL(root);
	if (phys != 0)
		wdt_start(phys, &kV103Layout);
}
