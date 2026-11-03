/*
 * Allwinner A20 (sun7i, e.g. Banana Pi): two Cortex-A7, GIC-400, DesignWare
 * 8250 UART. Built as the generic A7 board (VIRTA7) with these addresses.
 * U-Boot hands off through its own PSCI monitor with the core non-secure, so
 * the timer arrives as a group 1 IRQ rather than the FIQ QEMU gives it.
 */

#ifndef _PEXPERT_ARM_SUN7I_H
#define _PEXPERT_ARM_SUN7I_H

#define NO_MONITOR 1
#define NO_ECORE 1

/* The generic A7 paths (clock, topology, no Apple debug or arm-io nodes) */
#define QEMUVIRTA7

#define __ARM_TIME__              1

/* Non-secure A7: timer as a group 1 IRQ, DW 8250 console */
#define DW8250_NS_A7              1

/* UART0, left at 115200 by U-Boot; registers 4 bytes apart (reg-shift 2) */
#define DW8250_UART_BASE_PHYS     0x01c28000

#ifndef ASSEMBLER

#define DW8250_UART

#define DW8250_UART_SIZE          0x1000
/* GIC-400: the distributor and the cpu interface one page apart */
#define QEMUVIRTA7_GICD_BASE_PHYS 0x01c81000
#define QEMUVIRTA7_GICC_BASE_PHYS 0x01c82000
#define QEMUVIRTA7_GIC_SIZE       0x1000
#define QEMUVIRTA7_TIMER_PPI_VIRT 27

#endif /* ! ASSEMBLER */

#endif /* ! _PEXPERT_ARM_SUN7I_H */
