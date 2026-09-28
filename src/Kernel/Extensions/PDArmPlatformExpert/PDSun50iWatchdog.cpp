// h616 watchdog as a test harness, pdwdt=<seconds> keeps feeding it until the deadline
// then lets the soc reset, a hang or panic stops the feeding early

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>
#include "PDSun50iWatchdog.h"

#define SUN50I_WDT_PHYS		0x030090a0ULL
#define WDT_CTRL		0x10
#define WDT_CTRL_RESTART	((0xa57U << 1) | 1U)
#define WDT_CFG			0x14
#define WDT_CFG_SYSTEM_RESET	1U
#define WDT_MODE		0x18
#define WDT_MODE_16S		((11U << 4) | 1U)
#define WDT_FEED_MS		4000

static IOMemoryMap *gWdtMap;
static volatile uint8_t *gWdt;
static thread_call_t gWdtCall;
static uint64_t gWdtDeadline;

static inline void
wdt_wr(uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(gWdt + off) = val;
	__asm__ volatile ("dsb sy" ::: "memory");
}

static void
wdt_feed(thread_call_param_t, thread_call_param_t)
{
	uint64_t next;

	// past the deadline the feeding stops and the watchdog resets the board 16s later
	if (mach_absolute_time() >= gWdtDeadline)
		return;
	wdt_wr(WDT_CTRL, WDT_CTRL_RESTART);
	clock_interval_to_deadline(WDT_FEED_MS, kMillisecondScale, &next);
	thread_call_enter_delayed(gWdtCall, next);
}

void
PDSun50iWatchdog_start(void)
{
	uint32_t secs = 0;
	IOMemoryDescriptor *desc;

	if (!PE_parse_boot_argn("pdwdt", &secs, sizeof(secs)) || secs == 0)
		return;

	desc = IOMemoryDescriptor::withPhysicalAddress(SUN50I_WDT_PHYS & ~0xfffULL,
	    0x1000, kIODirectionOutIn);
	if (desc == NULL)
		return;
	gWdtMap = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (gWdtMap == NULL)
		return;
	gWdt = (volatile uint8_t *)gWdtMap->getVirtualAddress() + (SUN50I_WDT_PHYS & 0xfffULL);

	gWdtCall = thread_call_allocate(wdt_feed, NULL);
	if (gWdtCall == NULL)
		return;
	clock_interval_to_deadline(secs, kSecondScale, &gWdtDeadline);

	wdt_wr(WDT_CFG, WDT_CFG_SYSTEM_RESET);
	wdt_wr(WDT_MODE, WDT_MODE_16S);
	wdt_wr(WDT_CTRL, WDT_CTRL_RESTART);
	wdt_feed(NULL, NULL);
}
