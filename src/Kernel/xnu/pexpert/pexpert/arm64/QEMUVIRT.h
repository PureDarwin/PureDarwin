#ifndef _PEXPERT_ARM_QEMUVIRT_H
#define _PEXPERT_ARM_QEMUVIRT_H

#define NO_MONITOR 1
#define NO_ECORE 1

#define QEMUVIRT
#define QEMUVIRT_BRINGUP
#define ARM_ARCH_TIMER

#define __ARM_16K_PG__            1
#define __ARM_ARCH__              8
#define __ARM_VMSA__              8
#define __ARM_VFP__               4
#define __ARM_COHERENT_CACHE__    1
#define __ARM_DEBUG__             7
#define __ARM64_PMAP_SUBPAGE_L1__ 1
#define __ARM_PAN_AVAILABLE__ 1

#ifndef ASSEMBLER

#define QEMUVIRT_UART

/* PL011 UART, matched against the real QEMU virt DTB */
#define QEMUVIRT_UART_BASE_PHYS   0x09000000ULL
#define QEMUVIRT_UART_SIZE        0x1000ULL
#define QEMUVIRT_UART_IRQ         33  /* SPI 1 -> GIC IRQ 32+1 */

#define QEMUVIRT_UART_DR          (qemuvirt_uart_base_vaddr + 0x00)
#define QEMUVIRT_UART_FR          (qemuvirt_uart_base_vaddr + 0x18)
#define QEMUVIRT_UART_FR_TXFF     (1U << 5)  /* transmit FIFO full */

/* GICv3, matched against the real QEMU virt DTB */
#define GIC_SPURIOUS_IRQ          1023    /* INTID returned by ICC_IAR when no interrupt is pending */
#define QEMUVIRT_GICD_BASE_PHYS   0x08000000ULL
#define QEMUVIRT_GICD_SIZE        0x10000ULL
#define QEMUVIRT_GICR_BASE_PHYS   0x080a0000ULL
#define QEMUVIRT_GICR_SIZE        0x00f60000ULL

/* Generic timer PPIs (standard ARMv8 assignment, confirmed via DTB) */
#define QEMUVIRT_TIMER_PPI_SEC    29
#define QEMUVIRT_TIMER_PPI_NONSEC 30
#define QEMUVIRT_TIMER_PPI_VIRT   27
#define QEMUVIRT_TIMER_PPI_HYP    26

/* RAM base, matched against the real QEMU virt DTB (memory@40000000) */
#define QEMUVIRT_RAM_BASE_PHYS    0x40000000ULL

#define QEMUVIRT_PUT32(addr, value) do { *((volatile uint32_t *) addr) = value; } while(0)
#define QEMUVIRT_GET32(addr) *((volatile uint32_t *) addr)

#define PLATFORM_PANIC_LOG_PADDR  0x47000000ULL
#define PLATFORM_PANIC_LOG_SIZE   16384        // 16kb
#endif /* ! ASSEMBLER */

#endif /* ! _PEXPERT_ARM_QEMUVIRT_H */
