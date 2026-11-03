/*
 * QEMU "virt" with a Cortex-A7 (ARMv7-A, VMSAv7, generic timer, GICv2).
 * The generic 32-bit board the RK3506 and other A7 SoCs are brought up against.
 */

#ifndef _PEXPERT_ARM_VIRTA7_H
#define _PEXPERT_ARM_VIRTA7_H

#define NO_MONITOR 1
#define NO_ECORE 1

#define QEMUVIRTA7

/* Timebase and decrementer both on the generic timer: CNTV fires on the FIQ vector */
#define __ARM_TIME__              1

#ifndef ASSEMBLER

#define QEMUVIRT_UART

/* PL011 and GICv2 at the QEMU virt addresses */
#define QEMUVIRT_UART_BASE_PHYS   0x09000000
#define QEMUVIRT_UART_SIZE        0x1000
#define QEMUVIRTA7_GICD_BASE_PHYS 0x08000000
#define QEMUVIRTA7_GICC_BASE_PHYS 0x08010000
#define QEMUVIRTA7_GIC_SIZE       0x1000
#define QEMUVIRTA7_TIMER_PPI_VIRT 27

#endif /* ! ASSEMBLER */

#endif /* ! _PEXPERT_ARM_VIRTA7_H */
