// allwinner h616/h618 usb1 host bring-up, the type-a port on the orange pi zero 3
// u-boot tears usb down before booti, so clocks, resets, phy and vbus start from scratch

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include "PDSun50iUSB.h"

#define SUN50I_CCU_PHYS		0x03001000ULL
#define SUN50I_PIO_PHYS		0x0300b000ULL
#define SUN50I_EHCI1_PHYS	0x05200000ULL
#define SUN50I_OHCI1_PHYS	0x05200400ULL
#define SUN50I_PMU1_PHYS	0x05200800ULL
#define SUN50I_PMU2_PHYS	0x05310800ULL
#define SUN50I_HCI_SIZE		0x100
#define SUN50I_MMC0_PHYS	0x04020000ULL
#define SUN50I_MMC_SIZE		0x1000

// ccu, per-port clock registers and the shared usb bus gate/reset register
#define CCU_USB1_CLK		0xa74
#define CCU_USB2_CLK		0xa78
#define CCU_USB_OHCI_GATE	(1U << 31)
#define CCU_USB_PHY_RESET	(1U << 30)
#define CCU_USB_PHY_GATE	(1U << 29)
#define CCU_USB_BGR		0xa8c
#define CCU_BGR_OHCI1		(1U << 1)
#define CCU_BGR_EHCI1		(1U << 5)
#define CCU_BGR_EHCI2		(1U << 6)
#define CCU_BGR_OHCI1_RST	(1U << 17)
#define CCU_BGR_EHCI1_RST	(1U << 21)

// per-port "pmu" block that sits after each hci
#define PMU_HCI_ICR		0x00
#define PMU_PASSBY		((1U << 10) | (1U << 9) | (1U << 8) | (1U << 0))
#define PMU_PHY_CTL		0x10
#define PMU_PHY_CTL_SIDDQ	(1U << 3)

// vbus switch enable on pc16, bank c config 2 and data registers
#define PIO_BANK_C		0x48
#define PIO_CFG2		0x08
#define PIO_DATA		0x10
#define PIO_PC16		16

static volatile uint8_t *
map_phys(IOPhysicalAddress phys, IOByteCount size, IOMemoryMap **outMap)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		phys, size, kIODirectionOutIn);
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
rd(volatile uint8_t *base, uint32_t off)
{
	return *(volatile uint32_t *)(base + off);
}

static inline void
wr(volatile uint8_t *base, uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(base + off) = val;
	__asm__ volatile ("dsb sy" ::: "memory");
}

static inline void
set(volatile uint8_t *base, uint32_t off, uint32_t bits)
{
	wr(base, off, rd(base, off) | bits);
}

static inline void
clr(volatile uint8_t *base, uint32_t off, uint32_t bits)
{
	wr(base, off, rd(base, off) & ~bits);
}

bool
PDSun50i_isPlatform(void)
{
	IORegistryEntry *armio = IORegistryEntry::fromPath("/arm-io", gIODTPlane);
	bool sun50i = false;

	if (armio != NULL) {
		OSData *type = OSDynamicCast(OSData, armio->getProperty("device_type"));
		sun50i = type != NULL && type->getLength() >= 9 &&
		    strncmp((const char *)type->getBytesNoCopy(), "sun50i-io", 9) == 0;
		armio->release();
	}
	return sun50i;
}

// same order as linux ehci-platform, bus clocks then resets then phy then vbus
bool
PDSun50iUSB_init(void)
{
	IOMemoryMap *ccuMap = NULL, *pioMap = NULL, *pmu1Map = NULL, *pmu2Map = NULL;
	volatile uint8_t *ccu = map_phys(SUN50I_CCU_PHYS, 0x1000, &ccuMap);
	volatile uint8_t *pio = map_phys(SUN50I_PIO_PHYS, 0x1000, &pioMap);
	volatile uint8_t *pmu1 = map_phys(SUN50I_PMU1_PHYS, 0x1000, &pmu1Map);
	volatile uint8_t *pmu2 = map_phys(SUN50I_PMU2_PHYS, 0x1000, &pmu2Map);
	bool ok = ccu != NULL && pio != NULL && pmu1 != NULL && pmu2 != NULL;

	if (ok) {
		set(ccu, CCU_USB_BGR, CCU_BGR_OHCI1 | CCU_BGR_EHCI1);
		set(ccu, CCU_USB1_CLK, CCU_USB_OHCI_GATE);
		set(ccu, CCU_USB_BGR, CCU_BGR_OHCI1_RST | CCU_BGR_EHCI1_RST);

		set(ccu, CCU_USB1_CLK, CCU_USB_PHY_GATE | CCU_USB_PHY_RESET);

		// h616 phys only come out of siddq with phy2 clocked, its pmu needs the ehci2 bus clock
		set(ccu, CCU_USB2_CLK, CCU_USB_PHY_GATE | CCU_USB_PHY_RESET);
		set(ccu, CCU_USB_BGR, CCU_BGR_EHCI2);
		clr(pmu2, PMU_PHY_CTL, PMU_PHY_CTL_SIDDQ);

		clr(pmu1, PMU_PHY_CTL, PMU_PHY_CTL_SIDDQ);
		set(pmu1, PMU_HCI_ICR, PMU_PASSBY);

		// pc16 as output driven high switches the port's 5v on
		wr(pio, PIO_BANK_C + PIO_CFG2,
		    (rd(pio, PIO_BANK_C + PIO_CFG2) & ~0xfU) | 0x1U);
		set(pio, PIO_BANK_C + PIO_DATA, 1U << PIO_PC16);
		IOSleep(50);
	} else {
		IOLog("PDSun50iUSB: could not map usb control registers\n");
	}

	OSSafeReleaseNULL(ccuMap);
	OSSafeReleaseNULL(pioMap);
	OSSafeReleaseNULL(pmu1Map);
	OSSafeReleaseNULL(pmu2Map);
	return ok;
}

// service-plane nub carrying its register window as device memory 0
static IOService *
publish_nub(IOService *parent, const char *name, IOPhysicalAddress phys, IOByteCount size)
{
	IOService *nub = OSTypeAlloc(IOPlatformDevice);
	IODeviceMemory *range = IODeviceMemory::withRange(phys, size);
	OSArray *mem = OSArray::withCapacity(1);

	if (nub == NULL || range == NULL || mem == NULL || !nub->init())
		goto fail;
	mem->setObject(range);
	nub->setDeviceMemory(mem);
	nub->setName(name);
	if (!nub->attach(parent))
		goto fail;
	nub->registerService();
	OSSafeReleaseNULL(range);
	OSSafeReleaseNULL(mem);
	return nub;

fail:
	IOLog("PDSun50iUSB: could not publish %s\n", name);
	OSSafeReleaseNULL(range);
	OSSafeReleaseNULL(mem);
	OSSafeReleaseNULL(nub);
	return NULL;
}

// ehci first so it owns the ports and hands low and full speed ones to ohci
void
PDSun50iUSB_publish(IOService *parent)
{
	publish_nub(parent, "usb-ehci", SUN50I_EHCI1_PHYS, SUN50I_HCI_SIZE);
	publish_nub(parent, "usb-ohci", SUN50I_OHCI1_PHYS, SUN50I_HCI_SIZE);
}

// smhc0 behind the micro sd slot, PDSun50iMMC brings up its own pins and clocks
void
PDSun50iMMC_publish(IOService *parent)
{
	publish_nub(parent, "sunxi-mmc", SUN50I_MMC0_PHYS, SUN50I_MMC_SIZE);
}
