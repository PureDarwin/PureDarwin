#ifndef _PEXPERT_ARM_SUN50I_H
#define _PEXPERT_ARM_SUN50I_H

// Allwinner H616/H618 (Orange Pi Zero 3) and Sophgo SG2002 (LicheeRV Nano), Cortex-A53s whose
// uart and gic come from the device tree. No 16KB granule on the A53, so 4KB pages, no __ARM_16K_PG__

#define NO_MONITOR 1
#define NO_ECORE 1

#define SUN50I
#define ARM_ARCH_TIMER

// 4KB pages, see above
#define __ARM_ARCH__              8
#define __ARM_VMSA__              8
#define __ARM_VFP__               4
#define __ARM_COHERENT_CACHE__    1
#define __ARM_DEBUG__             7
#if !ARM_LARGE_MEMORY
#define __ARM64_PMAP_SUBPAGE_L1__ 1
#endif
// No __ARM_PAN_AVAILABLE__: PAN is ARMv8.1 and Cortex-A53 is ARMv8.0

#ifndef ASSEMBLER

// pe_serial.c's DesignWare APB UART, picked by "snps,dw-apb-uart" on /arm-io/uart0,
// 16550-compatible with reg-shift 2 and a 24MHz clock
#define DW_APB_UART

#define SUN50I_UART0_BASE_PHYS    0x05000000ULL
#define SUN50I_UART0_SIZE         0x400ULL
#define SUN50I_UART0_IRQ          32  // SPI 0 -> GIC INTID 32+0

// GIC-400 (GICv2), not QEMU virt's GICv3, so no HAS_GICV3_FIQ: the timer is an ordinary IRQ
// and sleh.c drives the GIC directly
#define SUN50I_GICD_BASE_PHYS     0x03021000ULL
#define SUN50I_GICD_SIZE          0x1000ULL
#define SUN50I_GICC_BASE_PHYS     0x03022000ULL
#define SUN50I_GICC_SIZE          0x2000ULL

// Generic timer PPIs, the standard ARMv8 assignment as in the H616 DT
#define SUN50I_TIMER_PPI_SEC      29
#define SUN50I_TIMER_PPI_NONSEC   30
#define SUN50I_TIMER_PPI_VIRT     27
#define SUN50I_TIMER_PPI_HYP      26
#define GIC_SPURIOUS_IRQ          1023    // INTID read from GICC_IAR when nothing is pending

// RAM base, as in the H616 DT (memory@40000000)
#define SUN50I_RAM_BASE_PHYS      0x40000000ULL

#define SUN50I_PUT32(addr, value) do { *((volatile uint32_t *) addr) = value; } while(0)
#define SUN50I_GET32(addr) *((volatile uint32_t *) addr)

// no fixed panic log: 0x47000000 is plain ram on the h616 and not ram at all on the sg2002
#endif // ! ASSEMBLER

#endif // ! _PEXPERT_ARM_SUN50I_H
