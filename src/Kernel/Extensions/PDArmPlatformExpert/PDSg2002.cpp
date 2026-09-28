// sophgo sg2002 platform bits for the a53 port, the licheerv nano

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>
#include "PDSg2002.h"

// designware watchdog, its reset reaches the soc through bit 2 of top+0x8
#define SG2002_TOP_PHYS		0x03000000ULL
#define TOP_WDT_RST_EN_REG	0x8
#define TOP_WDT_RST_EN		(1U << 2)
#define SG2002_WDT_PHYS		0x03010000ULL
#define WDT_CR			0x00
#define WDT_CR_EN		(1U << 0)
#define WDT_TORR		0x04
#define WDT_TORR_21S		0xddU	// 2^29 cycles of the 25 MHz clock
#define WDT_CRR			0x0c
#define WDT_CRR_KICK		0x76U
#define WDT_FEED_MS		4000

// sd0, the micro sd slot, PDSg2002SD matches the nub by name
#define SG2002_SD0_PHYS		0x04310000ULL
#define SG2002_SD0_SIZE		0x1000ULL
// gmac0 behind the rj45 on the nano e, PDSg2002Eth matches it
#define SG2002_GMAC0_PHYS	0x04070000ULL
#define SG2002_GMAC0_SIZE	0x10000ULL

static IOMemoryMap *gWdtMap;
static volatile uint8_t *gWdt;
static thread_call_t gWdtCall;
static uint64_t gWdtDeadline;
static bool gWdtForever;

bool
PDSg2002_isPlatform(void)
{
	IORegistryEntry *armio = IORegistryEntry::fromPath("/arm-io", gIODTPlane);
	bool sg2002 = false;

	if (armio != NULL) {
		OSData *type = OSDynamicCast(OSData, armio->getProperty("device_type"));
		sg2002 = type != NULL && type->getLength() >= 9 &&
		    strncmp((const char *)type->getBytesNoCopy(), "sg2002-io", 9) == 0;
		armio->release();
	}
	return sg2002;
}

static volatile uint8_t *
map_page(IOPhysicalAddress phys, IOMemoryMap **outMap)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(phys, 0x1000,
	    kIODirectionOutIn);
	IOMemoryMap *map;

	if (desc == NULL)
		return NULL;
	map = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (map == NULL)
		return NULL;
	*outMap = map;
	return (volatile uint8_t *)map->getVirtualAddress();
}

static inline uint32_t
wdt_rd(uint32_t off)
{
	return *(volatile uint32_t *)(gWdt + off);
}

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

	// past the deadline the feeding stops and the watchdog resets the board 21s later
	if (!gWdtForever && mach_absolute_time() >= gWdtDeadline)
		return;
	wdt_wr(WDT_CRR, WDT_CRR_KICK);
	clock_interval_to_deadline(WDT_FEED_MS, kMillisecondScale, &next);
	thread_call_enter_delayed(gWdtCall, next);
}

void
PDSg2002Watchdog_start(void)
{
	IOMemoryMap *topMap = NULL;
	volatile uint8_t *top;
	uint32_t secs = 0;
	bool armed;

	gWdt = map_page(SG2002_WDT_PHYS, &gWdtMap);
	if (gWdt == NULL)
		return;
	armed = (wdt_rd(WDT_CR) & WDT_CR_EN) != 0;
	gWdtForever = !PE_parse_boot_argn("pdwdt", &secs, sizeof(secs)) || secs == 0;
	if (gWdtForever && !armed)
		return;

	gWdtCall = thread_call_allocate(wdt_feed, NULL);
	if (gWdtCall == NULL)
		return;
	if (!gWdtForever)
		clock_interval_to_deadline(secs, kSecondScale, &gWdtDeadline);

	// once enabled it cannot be turned off, only kicked, so a timeout change needs a kick first
	top = map_page(SG2002_TOP_PHYS, &topMap);
	if (top != NULL) {
		*(volatile uint32_t *)(top + TOP_WDT_RST_EN_REG) |= TOP_WDT_RST_EN;
		OSSafeReleaseNULL(topMap);
	}
	wdt_wr(WDT_TORR, WDT_TORR_21S);
	wdt_wr(WDT_CRR, WDT_CRR_KICK);
	wdt_wr(WDT_CR, wdt_rd(WDT_CR) | WDT_CR_EN);
	wdt_feed(NULL, NULL);
}

// service-plane nub carrying its register window as device memory 0
static void
publish_nub(IOService *parent, const char *name, IOPhysicalAddress phys, IOByteCount size)
{
	IOService *nub = OSTypeAlloc(IOPlatformDevice);
	IODeviceMemory *range = IODeviceMemory::withRange(phys, size);
	OSArray *mem = OSArray::withCapacity(1);

	if (nub != NULL && range != NULL && mem != NULL && nub->init()) {
		mem->setObject(range);
		nub->setDeviceMemory(mem);
		nub->setName(name);
		if (nub->attach(parent))
			nub->registerService();
	} else {
		IOLog("PDSg2002: could not publish %s\n", name);
	}
	OSSafeReleaseNULL(range);
	OSSafeReleaseNULL(mem);
	OSSafeReleaseNULL(nub);
}

void
PDSg2002_publish(IOService *parent)
{
	publish_nub(parent, "sg2002-sdhci", SG2002_SD0_PHYS, SG2002_SD0_SIZE);
	publish_nub(parent, "sg2002-gmac", SG2002_GMAC0_PHYS, SG2002_GMAC0_SIZE);
}
