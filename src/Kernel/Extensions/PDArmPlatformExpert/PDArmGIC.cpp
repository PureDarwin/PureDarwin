#include "PDArmGIC.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <arm/machine_routines.h>

#define PD_LOG(...) do { if (ml_get_interrupts_enabled()) { IOLog(__VA_ARGS__); } } while (0)

#define GIC_GICD_BASE_PHYS   0x08000000ULL
#define GIC_GICD_SIZE        0x10000ULL
#define GIC_GICR_BASE_PHYS   0x080a0000ULL
#define GIC_GICR_FRAME_SIZE  0x20000ULL   /* one PE's RD + SGI frame */
#define GIC_GICR_TYPER       0x0008
#define GIC_GICR_TYPER_VLPIS (1ULL << 1)  /* gicv4: two more frames per PE */
#define GIC_GICR_TYPER_LAST  (1ULL << 4)
#define GIC_TIMER_PPI        27           /* CNTV virtual-timer INTID */

/* GICD (distributor) */
#define GICD_CTLR            0x0000
#define GICD_CTLR_ENGRP0     (1u << 0)
#define GICD_CTLR_ENGRP1     (1u << 1)
#define GICD_CTLR_ARE        (1u << 4)
#define GICD_CTLR_DS         (1u << 6)   /* single security state: group 0 is ours */
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

#define GIC_MAX_CPUS         32           /* matches MAX_CPUS in xnu's virt board config */
#define GICR_ISPENDR0        (GICR_SGI_BASE + 0x0200)
#define GIC_IPI_SGI          0            /* SGI INTID used for scheduler IPIs */

/* GICv2 (KVM on a GIC-400 host): distributor at the same base, CPU interface MMIO */
#define GIC2_GICC_BASE_PHYS  0x08010000ULL
#define GIC2_GICC_SIZE       0x2000ULL
#define GIC2_D_IGROUPR0      0x0080
#define GIC2_D_ISENABLER0    0x0100
#define GIC2_D_ICENABLER0    0x0180
#define GIC2_D_IPRIORITYR    0x0400
#define GIC2_D_SGIR          0x0F00
#define SUN50I_GICD_PHYS     0x03021000ULL
#define SUN50I_GICC_PHYS     0x03022000ULL
#define GIC2_C_CTLR          0x0000
#define GIC2_C_PMR           0x0004
/* EnableGrp0 | EnableGrp1 | FIQEn: Group 0 (timer, IPI) arrives as FIQ */
#define GIC2_C_CTLR_VAL      0xBu

static bool gGicV2;
// KVM's GICv3 keeps Group 0 disabled, and under TF-A group 0 belongs to the secure world:
// then the timer and IPI go in Group 1 and arrive as IRQs
static bool gGicGrp1;
// two security states: group 0 registers are the secure world's, leave them alone
static bool gGicSecure;
// sun50i: the kernel owns the GIC-400, only its SGIR is mapped here for IPIs
static bool gSun50iSgi;
static IOMemoryMap *gGiccMap;
static volatile uint8_t *gGicc;

static bool
gic_is_v2(void)
{
#if !defined(__arm__) || defined(__arm64__)
	uint64_t pfr0;
	__asm__ volatile ("mrs %0, ID_AA64PFR0_EL1" : "=r"(pfr0));
	return ((pfr0 >> 24) & 0xf) == 0;
#else
	return false;
#endif
}

static IOMemoryMap *gGicdMap;
static volatile uint8_t *gGicd;
static volatile uint8_t *gGicr;

// the redistributor region, mapped once; each PE's frame is found by its affinity
static uint64_t gGicdPhys = GIC_GICD_BASE_PHYS;
static uint64_t gGicrPhys = GIC_GICR_BASE_PHYS;
static uint64_t gGicrSize = GIC_MAX_CPUS * GIC_GICR_FRAME_SIZE;
static IOMemoryMap *gGicrAllMap;
static volatile uint8_t *gGicrAll;
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

static inline void
c_write(uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(gGicc + off) = val;
}

// SGI/PPI enables, groups and priorities are banked per CPU in a GICv2 distributor
static void
gic2_cpu_setup(void)
{
	d_write(GIC2_D_ICENABLER0, 0xffffffffu);
	d_write(GIC2_D_IGROUPR0, d_read(GIC2_D_IGROUPR0) &
	    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	((volatile uint8_t *)(gGicd + GIC2_D_IPRIORITYR))[GIC_TIMER_PPI] = 0x00;
	((volatile uint8_t *)(gGicd + GIC2_D_IPRIORITYR))[GIC_IPI_SGI] = 0x00;
	c_write(GIC2_C_PMR, 0xff);
	c_write(GIC2_C_CTLR, GIC2_C_CTLR_VAL);
}

// banked gic-400 state for the calling cpu, the same setup the kernel gives the boot cpu
// sgi 0 carries ipis, ppi 27 the virtual timer, both as non-secure irq
static bool
sun50i_cpu_setup(void)
{
	d_write(GIC2_D_ISENABLER0, (1u << GIC_IPI_SGI) | (1u << GIC_TIMER_PPI));
	((volatile uint8_t *)(gGicd + GIC2_D_IPRIORITYR))[GIC_TIMER_PPI] = 0x80;
	((volatile uint8_t *)(gGicd + GIC2_D_IPRIORITYR))[GIC_IPI_SGI] = 0x80;
	c_write(GIC2_C_PMR, 0xf0);
	c_write(GIC2_C_CTLR, 1);
	return true;
}

#if !defined(__arm__) || defined(__arm64__)
// /arm-io/gic from the loader names a gic-400 the kernel drives, reg holds the distributor
// and cpu interface relative to the arm-io window. older sun50i loaders only set device_type
static bool
kernel_owned_gic(uint64_t *gicd, uint64_t *gicc)
{
	IORegistryEntry *armio = IORegistryEntry::fromPath("/arm-io", gIODTPlane);
	IORegistryEntry *gic = IORegistryEntry::fromPath("/arm-io/gic", gIODTPlane);
	bool found = false;

	if (gic != NULL && armio != NULL) {
		OSData *compat = OSDynamicCast(OSData, gic->getProperty("compatible"));
		OSData *reg = OSDynamicCast(OSData, gic->getProperty("reg"));
		OSData *ranges = OSDynamicCast(OSData, armio->getProperty("ranges"));
		if (compat != NULL && compat->getLength() >= 11 &&
		    strncmp((const char *)compat->getBytesNoCopy(), "arm,gic-400", 11) == 0 &&
		    reg != NULL && reg->getLength() >= 4 * sizeof(uint64_t) &&
		    ranges != NULL && ranges->getLength() >= 2 * sizeof(uint64_t)) {
			const uint64_t *r = (const uint64_t *)reg->getBytesNoCopy();
			uint64_t base = ((const uint64_t *)ranges->getBytesNoCopy())[1];
			*gicd = base + r[0];
			*gicc = base + r[2];
			found = true;
		}
	}
	if (!found && armio != NULL) {
		OSData *type = OSDynamicCast(OSData, armio->getProperty("device_type"));
		if (type != NULL && type->getLength() >= 9 &&
		    strncmp((const char *)type->getBytesNoCopy(), "sun50i-io", 9) == 0) {
			*gicd = SUN50I_GICD_PHYS;
			*gicc = SUN50I_GICC_PHYS;
			found = true;
		}
	}
	OSSafeReleaseNULL(gic);
	OSSafeReleaseNULL(armio);
	return found;
}

// /arm-io/gic from the loader, when the board's tree has a gic-v3: distributor and
// redistributor region relative to the arm-io window. Without it, qemu virt's addresses
static void
gic_v3_locate(void)
{
	IORegistryEntry *armio = IORegistryEntry::fromPath("/arm-io", gIODTPlane);
	IORegistryEntry *gic = IORegistryEntry::fromPath("/arm-io/gic", gIODTPlane);

	if (gic != NULL && armio != NULL) {
		OSData *compat = OSDynamicCast(OSData, gic->getProperty("compatible"));
		OSData *reg = OSDynamicCast(OSData, gic->getProperty("reg"));
		OSData *ranges = OSDynamicCast(OSData, armio->getProperty("ranges"));
		if (compat != NULL && compat->getLength() >= 10 &&
		    strncmp((const char *)compat->getBytesNoCopy(), "arm,gic-v3", 10) == 0 &&
		    reg != NULL && reg->getLength() >= 4 * sizeof(uint64_t) &&
		    ranges != NULL && ranges->getLength() >= 2 * sizeof(uint64_t)) {
			const uint64_t *r = (const uint64_t *)reg->getBytesNoCopy();
			uint64_t base = ((const uint64_t *)ranges->getBytesNoCopy())[1];
			gGicdPhys = base + r[0];
			gGicrPhys = base + r[2];
			if (r[3] != 0) {
				gGicrSize = r[3];
			}
		}
	}
	OSSafeReleaseNULL(gic);
	OSSafeReleaseNULL(armio);
}

// MPIDR to the Aff3.Aff2.Aff1.Aff0 layout GICR_TYPER reports
static uint32_t
mpidr_affinity(uint64_t mpidr)
{
	return (uint32_t)(((mpidr >> 32) & 0xffULL) << 24 | (mpidr & 0xffffffULL));
}

// the redistributor frame whose GICR_TYPER affinity is this PE's
static volatile uint8_t *
gicr_frame(uint64_t mpidr)
{
	uint32_t want = mpidr_affinity(mpidr);
	uint64_t off = 0;

	if (gGicrAll == NULL) {
		return NULL;
	}
	while (off + GIC_GICR_FRAME_SIZE <= gGicrSize) {
		uint64_t typer = *(volatile uint64_t *)(gGicrAll + off + GIC_GICR_TYPER);
		if ((uint32_t)(typer >> 32) == want) {
			return gGicrAll + off;
		}
		if (typer & GIC_GICR_TYPER_LAST) {
			break;
		}
		off += (typer & GIC_GICR_TYPER_VLPIS) ? 2 * GIC_GICR_FRAME_SIZE : GIC_GICR_FRAME_SIZE;
	}
	return NULL;
}
#endif

#if !defined(__arm__) || defined(__arm64__)
// the cpu interface through system registers: groups off while configuring, on to deliver.
// group 0 is left to the secure world when there is one
static void
gic_cpuif_setup(bool deliver)
{
	uint64_t on = deliver ? 1 : 0;

	if (!deliver) {
		__asm__ volatile (
		    "msr ICC_SRE_EL1, %0\n"
		    "isb\n"
		    "msr ICC_PMR_EL1, %1\n"
		    :: "r"((uint64_t)0x1), "r"((uint64_t)0xff) : "memory");
	}
	if (!gGicSecure) {
		__asm__ volatile ("msr ICC_IGRPEN0_EL1, %0\n" :: "r"(on) : "memory");
	}
	__asm__ volatile (
	    "msr ICC_IGRPEN1_EL1, %0\n"
	    "isb\n"
	    :: "r"(on) : "memory");
}

#endif

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

	// The H616/H618 and SG2002 GIC-400 is driven by the kernel itself (sleh.c): the timer
	// arrives as a non-secure IRQ long before this kext loads, and virt's GIC addresses are not there
	uint64_t gicd_phys, gicc_phys;
	if (kernel_owned_gic(&gicd_phys, &gicc_phys)) {
		// only the distributor's SGIR is used here, for IPIs: a GIC-400 has no ICC_SGI*R_EL1
		gGicd = map_phys(gicd_phys, 0x1000, &gGicdMap);
		// secondaries set up their own banked cpu interface through this mapping
		gGicc = map_phys(gicc_phys, GIC2_GICC_SIZE, &gGiccMap);
		gSun50iSgi = gGicd != NULL && gGicc != NULL;
		PD_LOG("PDArmGIC: GIC-400 at 0x%llx owned by the kernel, IPIs through GICD_SGIR %s\n",
		    (unsigned long long)gicd_phys, gSun50iSgi ? "mapped" : "unmapped");
		return true;
	}

	if (gic_is_v2()) {
		gGicd = map_phys(GIC_GICD_BASE_PHYS, GIC_GICD_SIZE, &gGicdMap);
		gGicc = map_phys(GIC2_GICC_BASE_PHYS, GIC2_GICC_SIZE, &gGiccMap);
		if (gGicd == NULL || gGicc == NULL) {
			PD_LOG("PDArmGIC: failed to map GICv2 (gicd=%p gicc=%p)\n", gGicd, gGicc);
			gGicd = NULL;
			return false;
		}
		gGicV2 = true;
		d_write(GICD_CTLR, 0);
		gic2_cpu_setup();
		d_write(GICD_CTLR, GICD_CTLR_ENGRP0 | GICD_CTLR_ENGRP1);
		PD_LOG("PDArmGIC: GICv2 configured (timer PPI %u, IPI SGI %u as Group0/FIQ, masked)\n",
		    (unsigned)GIC_TIMER_PPI, (unsigned)GIC_IPI_SGI);
		return true;
	}

	gic_v3_locate();
	gGicd = map_phys(gGicdPhys, GIC_GICD_SIZE, &gGicdMap);
	gGicrAll = map_phys(gGicrPhys, round_page_64(gGicrSize), &gGicrAllMap);
	{
		uint64_t mpidr;
		__asm__ volatile ("mrs %0, MPIDR_EL1" : "=r"(mpidr));
		gGicr = gicr_frame(mpidr);
	}
	if (gGicd == NULL || gGicr == NULL) {
		PD_LOG("PDArmGIC: failed to map GIC at 0x%llx/0x%llx (gicd=%p gicr=%p)\n",
		    (unsigned long long)gGicdPhys, (unsigned long long)gGicrPhys, gGicd, gGicr);
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
	// with two security states this is the non-secure view, where bit 0 is EnableGrp1
	gGicSecure = (d_read(GICD_CTLR) & GICD_CTLR_DS) == 0;
	gGicGrp1 = gGicSecure || (d_read(GICD_CTLR) & GICD_CTLR_ENGRP0) == 0;

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
	if (gGicGrp1) {
		r_write(GICR_IGROUPR0, r_read(GICR_IGROUPR0) | (1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI));
	} else {
		r_write(GICR_IGROUPR0, r_read(GICR_IGROUPR0) &
		    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	}
	r_write(GICR_IGRPMODR0, r_read(GICR_IGRPMODR0) &
	    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	((volatile uint8_t *)(gGicr + GICR_IPRIORITYR))[GIC_TIMER_PPI] = 0x00;
	((volatile uint8_t *)(gGicr + GICR_IPRIORITYR))[GIC_IPI_SGI] = 0x00;

	/* System register access and priority mask are safe now; delivery is not. */
	gic_cpuif_setup(false);

	PD_LOG("PDArmGIC: configured (GICD_CTLR=0x%x GICR_WAKER=0x%x timer PPI %u %s/masked)\n",
	    d_read(GICD_CTLR), r_read(GICR_WAKER), (unsigned)GIC_TIMER_PPI, gGicGrp1 ? "Group1" : "Group0");
	return true;
#endif
}

bool
PDArmGIC_enable(void)
{
#if defined(__arm__) && !defined(__arm64__)
	return true;
#else
	if (gGicV2) {
		d_write(GIC2_D_ISENABLER0, (1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI));
		PD_LOG("PDArmGIC: GICv2 delivery enabled\n");
		return true;
	}
	if (gGicr == NULL) {
		return false;
	}

	r_write(GICR_ISENABLER0, (1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI));
	gic_cpuif_setup(true);

	PD_LOG("PDArmGIC: delivery enabled (timer PPI %u as %s)\n",
	    (unsigned)GIC_TIMER_PPI, gGicGrp1 ? "Group1/IRQ" : "Group0/FIQ");
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
PDArmGIC_map_cpu(unsigned int cpu, uint64_t mpidr)
{
#if defined(__arm__) && !defined(__arm64__)
	(void)cpu;
	(void)mpidr;
	return true;
#else
	if (cpu >= GIC_MAX_CPUS) {
		return false;
	}
	if (gGicV2) {
		return true;
	}
	if (gGicrCpu[cpu] == NULL) {
		gGicrCpu[cpu] = gicr_frame(mpidr);
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
	if (gSun50iSgi) {
		return sun50i_cpu_setup();
	}
	if (gGicV2) {
		gic2_cpu_setup();
		return true;
	}
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
	if (gGicGrp1) {
		rc_write(cpu, GICR_IGROUPR0, rc_read(cpu, GICR_IGROUPR0) | (1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI));
	} else {
		rc_write(cpu, GICR_IGROUPR0, rc_read(cpu, GICR_IGROUPR0) &
		    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	}
	rc_write(cpu, GICR_IGRPMODR0, rc_read(cpu, GICR_IGRPMODR0) &
	    ~((1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI)));
	((volatile uint8_t *)(gGicrCpu[cpu] + GICR_IPRIORITYR))[GIC_TIMER_PPI] = 0x00;
	((volatile uint8_t *)(gGicrCpu[cpu] + GICR_IPRIORITYR))[GIC_IPI_SGI] = 0x00;

	gic_cpuif_setup(false);
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
	if (gSun50iSgi) {
		// sun50i_cpu_setup() already enabled the ipi and timer lines
		return true;
	}
	if (gGicV2) {
		d_write(GIC2_D_ISENABLER0, (1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI));
		return true;
	}
	if (cpu >= GIC_MAX_CPUS || gGicrCpu[cpu] == NULL) {
		return false;
	}
	rc_write(cpu, GICR_ISENABLER0, (1u << GIC_TIMER_PPI) | (1u << GIC_IPI_SGI));
	gic_cpuif_setup(true);
	return true;
#endif
}

// ICC_SGI1R_EL1: aff3[55:48] aff2[39:32] aff1[23:16], INTID[27:24], target list
void
PDArmGIC_send_ipi(uint64_t target_mpidr)
{
#if !defined(__arm__) || defined(__arm64__)
	if (gGicV2 || gSun50iSgi) {
		// Target list is CPU interface numbers, which QEMU virt and the H618 number by Aff0
		d_write(GIC2_D_SGIR, (1u << (16 + (target_mpidr & 0x7))) | GIC_IPI_SGI);
		return;
	}
	uint64_t aff0 = target_mpidr & 0xffULL;
	uint64_t sgi = ((target_mpidr & 0xff00ULL) << 8) |
	    ((target_mpidr & 0xff0000ULL) << 16) |
	    (((target_mpidr >> 32) & 0xffULL) << 48) |
	    ((uint64_t)GIC_IPI_SGI << 24) |
	    (1ULL << (aff0 & 0xf));

	// Group 0 generation: the SGI is configured as Group 0 so it lands as
	// an FIQ alongside the timer, unless the GIC only offers Group 1
	if (gGicGrp1) {
		__asm__ volatile ("msr ICC_SGI1R_EL1, %0\n" "isb\n" :: "r"(sgi) : "memory");
	} else {
		__asm__ volatile ("msr ICC_SGI0R_EL1, %0\n" "isb\n" :: "r"(sgi) : "memory");
	}
#else
	(void)target_mpidr;
#endif
}
