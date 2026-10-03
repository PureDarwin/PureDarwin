#ifndef _PEXPERT_ARM_SC8280XP_H
#define _PEXPERT_ARM_SC8280XP_H

#define SC8280XP

#define NO_MONITOR 1
#define NO_ECORE 1

#define ARM_ARCH_TIMER

#define __ARM_16K_PG__            1
#define __ARM_ARCH__              8
#define __ARM_VMSA__              8
#define __ARM_VFP__               4
#define __ARM_COHERENT_CACHE__    1
#define __ARM_DEBUG__             7
#if !ARM_LARGE_MEMORY
#define __ARM64_PMAP_SUBPAGE_L1__ 1
#endif
#define __ARM_PAN_AVAILABLE__     1

#ifndef ASSEMBLER

#define HAS_GICV3_FIQ             1
#define GIC_SPURIOUS_IRQ          1023

#define QEMUVIRT_IPI_SGI          0     /* sleh.c's name; must match GIC_IPI_SGI in PDArmGIC.cpp */
#define QEMUVIRT_GICD_BASE_PHYS   0x17A00000ULL
#define QEMUVIRT_GICD_SIZE        0x10000ULL
#define QEMUVIRT_GICR_BASE_PHYS   0x17A60000ULL
#define QEMUVIRT_GICR_SIZE        0x100000ULL

/* SC8280XP timer PPIs */
#define QEMUVIRT_TIMER_PPI_SEC    29
#define QEMUVIRT_TIMER_PPI_NONSEC 30
#define QEMUVIRT_TIMER_PPI_VIRT   27
#define QEMUVIRT_TIMER_PPI_HYP    26

#define QEMUVIRT_RAM_BASE_PHYS    0x80000000ULL

#endif /* ! ASSEMBLER */

#endif /* ! _PEXPERT_ARM_SC8280XP_H */
