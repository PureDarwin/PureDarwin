#include "PDArmGIC.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <arm/machine_routines.h>

#define PD_LOG(...) do { if (ml_get_interrupts_enabled()) { IOLog(__VA_ARGS__); } } while (0)

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
#define GICR_ICENABLER0          (GICR_SGI_BASE + 0x0180)
#define GICR_IPRIORITYR          (GICR_SGI_BASE + 0x0400)
#define GICR_IGRPMODR0           (GICR_SGI_BASE + 0x0D00)

#define GIC_MAX_CPUS         8            /* xnu's virt board config caps MAX_CPUS */
#define GICR_ISPENDR0        (GICR_SGI_BASE + 0x0200)
#define GIC_IPI_SGI          0            /* SGI INTID used for scheduler IPIs */

static IOMemoryMap *gGicdMap;
static IOMemoryMap *gGicrMap;
static volatile uint8_t *gGicd;
static volatile uint8_t *gGicr;

// Each PE has its own redistributor frame at base + cpu * frame size
static IOMemoryMap *gGicrCpuMap[GIC_MAX_CPUS];
static volatile uint8_t *gGicrCpu[GIC_MAX_CPUS];

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
		phys, size, kIODirectionOutIn);
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
#if defined(__arm__) && !defined(__arm64__)
	/* Pi Zero/BCM2835 is ARMv6 with a legacy interrupt controller, not GICv3. */
	PD_LOG("PDArmGIC: skipped on ARM32 BCM2835\n");
	return true;
#else
	if (gGicd != NULL) {
		return true;
	}

	gGicd = map_phys(GIC_GICD_BASE_PHYS, GIC_GICD_SIZE, &gGicdMap);
	gGicr = map_phys(GIC_GICR_BASE_PHYS, GIC_GICR_FRAME_SIZE, &gGicrMap);
	if (gGicd == NULL || gGicr == NULL) {
		PD_LOG("PDArmGIC: failed to map GIC (gicd=%p gicr=%p)\n", gGicd, gGicr);
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

	/* The generic timer must land in **Group 0**, i.e. be delivered as an FIQ:
	 * xnu handles it in sleh_fiq() (ml_get_timer_pending() -> rtclock_intr()),
	 * which acknowledges via ICC_IAR0_EL1 and EOIs via ICC_EOIR0_EL1. Putting
	 * it in Group 1 sends it to sleh_irq(), which dispatches to the IOCPU
	 * interrupt controller - that has no notion of the timer, so it never
	 * rearms and the CPU wedges in an interrupt storm.
	 *
	 * Everything stays masked here. Delivery is switched on by
	 * PDArmGIC_enable() once cpu_data->interrupt_handler is installed. */
	// The IPI SGI joins the timer in Group 0 so both arrive
	// as FIQs and are classified by INTID in sleh_fiq()
	r_write(GICR_ICENABLER0, 0xffffffffu);
	r_write(GICR_IGROUPR0, r_read(GICR_IGROUPR0) &
	    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	r_write(GICR_IGRPMODR0, r_read(GICR_IGRPMODR0) &
	    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	((volatile uint8_t *)(gGicr + GICR_IPRIORITYR))[GIC_TIMER_PPI] = 0x00;
	((volatile uint8_t *)(gGicr + GICR_IPRIORITYR))[GIC_IPI_SGI] = 0x00;

	/* System register access and priority mask are safe now; delivery is not. */
	__asm__ volatile (
	    "msr ICC_SRE_EL1, %0\n"
	    "isb\n"
	    "msr ICC_PMR_EL1, %1\n"
	    "msr ICC_IGRPEN0_EL1, %2\n"
	    "msr ICC_IGRPEN1_EL1, %2\n"
	    "isb\n"
	    :: "r"((uint64_t)0x1), "r"((uint64_t)0xff), "r"((uint64_t)0x0) : "memory");

	PD_LOG("PDArmGIC: configured (GICD_CTLR=0x%x GICR_WAKER=0x%x timer PPI %u Group0/masked)\n",
	    d_read(GICD_CTLR), r_read(GICR_WAKER), (unsigned)GIC_TIMER_PPI);
	return true;
#endif
}

bool
PDArmGIC_enable(void)
{
#if defined(__arm__) && !defined(__arm64__)
	return true;
#else
	if (gGicr == NULL) {
		return false;
	}

	r_write(GICR_ISENABLER0, (1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI));
	__asm__ volatile (
	    "msr ICC_IGRPEN0_EL1, %0\n"
	    "msr ICC_IGRPEN1_EL1, %0\n"
	    "isb\n"
	    :: "r"((uint64_t)0x1) : "memory");

	PD_LOG("PDArmGIC: delivery enabled (timer PPI %u as Group0/FIQ)\n",
	    (unsigned)GIC_TIMER_PPI);
	return true;
#endif
}

#if !defined(__arm__) || defined(__arm64__)
static inline uint32_t
rc_read(unsigned int cpu, uint32_t off)
{
	return *(volatile uint32_t *)(gGicrCpu[cpu] + off);
}

static inline void
rc_write(unsigned int cpu, uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(gGicrCpu[cpu] + off) = val;
}
#endif

// Map one CPU's redistributor frame, from the boot CPU. Idempotent
bool
PDArmGIC_map_cpu(unsigned int cpu)
{
#if defined(__arm__) && !defined(__arm64__)
	(void)cpu;
	return true;
#else
	if (cpu >= GIC_MAX_CPUS) {
		return false;
	}
	if (gGicrCpu[cpu] == NULL) {
		gGicrCpu[cpu] = map_phys(GIC_GICR_BASE_PHYS + (uint64_t)cpu * GIC_GICR_FRAME_SIZE,
		    GIC_GICR_FRAME_SIZE, &gGicrCpuMap[cpu]);
	}
	return gGicrCpu[cpu] != NULL;
#endif
}

// Secondary CPUs: same sequence as PDArmGIC_init(), on this CPU's own frame.
// The distributor is already programmed by the boot CPU
bool
PDArmGIC_init_cpu(unsigned int cpu)
{
#if defined(__arm__) && !defined(__arm64__)
	(void)cpu;
	return true;
#else
	// Mapped by PDArmGIC_map_cpu() on the boot CPU.
	// This runs on the secondary with interrupts masked, so it only touches registers
	if (cpu >= GIC_MAX_CPUS || gGicrCpu[cpu] == NULL) {
		return false;
	}

	rc_write(cpu, GICR_WAKER, rc_read(cpu, GICR_WAKER) & ~GICR_WAKER_PROCSLEEP);
	while (rc_read(cpu, GICR_WAKER) & GICR_WAKER_CHILDASLEEP) {
		;
	}

	rc_write(cpu, GICR_ICENABLER0, 0xffffffffu);
	rc_write(cpu, GICR_IGROUPR0, rc_read(cpu, GICR_IGROUPR0) &
	    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	rc_write(cpu, GICR_IGRPMODR0, rc_read(cpu, GICR_IGRPMODR0) &
	    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	((volatile uint8_t *)(gGicrCpu[cpu] + GICR_IPRIORITYR))[GIC_TIMER_PPI] = 0x00;
	((volatile uint8_t *)(gGicrCpu[cpu] + GICR_IPRIORITYR))[GIC_IPI_SGI] = 0x00;

	__asm__ volatile (
	    "msr ICC_SRE_EL1, %0\n"
	    "isb\n"
	    "msr ICC_PMR_EL1, %1\n"
	    "msr ICC_IGRPEN0_EL1, %2\n"
	    "msr ICC_IGRPEN1_EL1, %2\n"
	    "isb\n"
	    :: "r"((uint64_t)0x1), "r"((uint64_t)0xff), "r"((uint64_t)0x0) : "memory");
	return true;
#endif
}

bool
PDArmGIC_enable_cpu(unsigned int cpu)
{
#if defined(__arm__) && !defined(__arm64__)
	(void)cpu;
	return true;
#else
	if (cpu >= GIC_MAX_CPUS || gGicrCpu[cpu] == NULL) {
		return false;
	}
	rc_write(cpu, GICR_ISENABLER0, (1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI));
	__asm__ volatile (
	    "msr ICC_IGRPEN0_EL1, %0\n"
	    "msr ICC_IGRPEN1_EL1, %0\n"
	    "isb\n"
	    :: "r"((uint64_t)0x1) : "memory");
	return true;
#endif
}

// ICC_SGI1R_EL1: aff3[55:48] aff2[39:32] aff1[23:16], INTID[27:24], target list
void
PDArmGIC_send_ipi(uint64_t target_mpidr)
{
#if !defined(__arm__) || defined(__arm64__)
	uint64_t aff0 = target_mpidr & 0xffULL;
	uint64_t sgi = ((target_mpidr & 0xff00ULL) << 8) |
	    ((target_mpidr & 0xff0000ULL) << 16) |
	    (((target_mpidr >> 32) & 0xffULL) << 48) |
	    ((uint64_t)GIC_IPI_SGI << 24) |
	    (1ULL << (aff0 & 0xf));

	// Group 0 generation: the SGI is configured as Group 0 so it lands as
	// an FIQ alongside the timer, which is the only path wired up here
	__asm__ volatile ("msr ICC_SGI0R_EL1, %0\n" "isb\n" :: "r"(sgi) : "memory");
#else
	(void)target_mpidr;
#endif
}
