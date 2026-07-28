#include "PDArmGIC.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

#define GIC_GICD_BASE_PHYS   0x08000000ULL
#define GIC_GICD_SIZE        0x10000ULL
#define GIC_GICR_BASE_PHYS   0x080a0000ULL
#define GIC_GICR_FRAME_SIZE  0x20000ULL   /* this PE's RD + SGI frame only */
#define GIC_TIMER_PPI        27           /* CNTV virtual-timer INTID */

/* GICD (distributor) */
#define GICD_CTLR            0x0000
#define GICD_CTLR_ENGRP0     (1u << 0)
#define GICD_CTLR_ENGRP1     (1u << 1)
#define GICD_CTLR_ARE        (1u << 4)
#define GICD_CTLR_RWP        (1u << 31)

/* GICR (redistributor): RD frame, SGI frame at +0x10000 */
#define GICR_WAKER               0x0014
#define GICR_WAKER_PROCSLEEP     (1u << 1)
#define GICR_WAKER_CHILDASLEEP   (1u << 2)
#define GICR_SGI_BASE            0x10000
#define GICR_IGROUPR0            (GICR_SGI_BASE + 0x0080)
#define GICR_ISENABLER0          (GICR_SGI_BASE + 0x0100)
#define GICR_IPRIORITYR          (GICR_SGI_BASE + 0x0400)
#define GICR_IGRPMODR0           (GICR_SGI_BASE + 0x0D00)

static IOMemoryMap *gGicdMap;
static IOMemoryMap *gGicrMap;
static volatile uint8_t *gGicd;
static volatile uint8_t *gGicr;

static inline uint32_t
d_read(uint32_t off)
{
	return *(volatile uint32_t *)(gGicd + off);
}
static inline void
d_write(uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(gGicd + off) = val;
}
static inline uint32_t
r_read(uint32_t off)
{
	return *(volatile uint32_t *)(gGicr + off);
}
static inline void
r_write(uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(gGicr + off) = val;
}

static volatile uint8_t *
map_phys(IOPhysicalAddress phys, IOByteCount size, IOMemoryMap **outMap)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		phys, size, kIODirectionOutIn | kIOMemoryMapperNone);
	if (!desc) {
		return NULL;
	}
	IOMemoryMap *map = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (!map) {
		return NULL;
	}
	*outMap = map;
	return (volatile uint8_t *)map->getVirtualAddress();
}

bool
PDArmGIC_init(void)
{
	if (gGicd != NULL) {
		return true;
	}

	gGicd = map_phys(GIC_GICD_BASE_PHYS, GIC_GICD_SIZE, &gGicdMap);
	gGicr = map_phys(GIC_GICR_BASE_PHYS, GIC_GICR_FRAME_SIZE, &gGicrMap);
	if (gGicd == NULL || gGicr == NULL) {
		IOLog("PDArmGIC: failed to map GIC (gicd=%p gicr=%p)\n", gGicd, gGicr);
		return false;
	}

	/* Distributor: enable affinity routing, then Group 0 + Group 1. */
	d_write(GICD_CTLR, GICD_CTLR_ARE);
	while (d_read(GICD_CTLR) & GICD_CTLR_RWP) {
		;
	}
	d_write(GICD_CTLR, GICD_CTLR_ARE | GICD_CTLR_ENGRP1 | GICD_CTLR_ENGRP0);
	while (d_read(GICD_CTLR) & GICD_CTLR_RWP) {
		;
	}

	/* Redistributor for this CPU: wake it and wait for it to power up. */
	r_write(GICR_WAKER, r_read(GICR_WAKER) & ~GICR_WAKER_PROCSLEEP);
	while (r_read(GICR_WAKER) & GICR_WAKER_CHILDASLEEP) {
		;
	}

	r_write(GICR_IGROUPR0, r_read(GICR_IGROUPR0) | (1u << GIC_TIMER_PPI));
	r_write(GICR_IGRPMODR0, r_read(GICR_IGRPMODR0) & ~(1u << GIC_TIMER_PPI));
	((volatile uint8_t *)(gGicr + GICR_IPRIORITYR))[GIC_TIMER_PPI] = 0x00;
	r_write(GICR_ISENABLER0, (1u << GIC_TIMER_PPI));

	__asm__ volatile (
	    "msr ICC_SRE_EL1, %0\n"
	    "isb\n"
	    "msr ICC_PMR_EL1, %1\n"
	    "msr ICC_IGRPEN1_EL1, %2\n"
	    "isb\n"
	    :: "r"((uint64_t)0x1), "r"((uint64_t)0xff), "r"((uint64_t)0x1) : "memory");

	IOLog("PDArmGIC: up (GICD_CTLR=0x%x GICR_WAKER=0x%x timer PPI %u Group1 enabled)\n",
	    d_read(GICD_CTLR), r_read(GICR_WAKER), (unsigned)GIC_TIMER_PPI);
	return true;
}
