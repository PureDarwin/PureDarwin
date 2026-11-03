/*
 * Rockchip RV1106/RV1103 (Luckfox Pico): one Cortex-A7, GIC-400, DesignWare 8250 UART.
 * Built as the generic A7 board (VIRTA7) with these addresses. The kernel runs non-secure,
 * so the timer is a group 1 interrupt on the IRQ vector, not the FIQ QEMU gives it.
 */

#ifndef _PEXPERT_ARM_RV1106_H
#define _PEXPERT_ARM_RV1106_H

#define NO_MONITOR 1
#define NO_ECORE 1

/* The generic A7 paths (clock, topology, no Apple debug or arm-io nodes) */
#define QEMUVIRTA7

#define __ARM_TIME__              1

/* Non-secure A7: timer as a group 1 IRQ, DW 8250 console (shared with RK3506, A20) */
#define DW8250_NS_A7            1

/* UART2, left at 115200 by U-Boot; registers 4 bytes apart */
#define DW8250_UART_BASE_PHYS     0xff4c0000

#ifndef ASSEMBLER

#define DW8250_UART

#define DW8250_UART_SIZE          0x1000
#define QEMUVIRTA7_GICD_BASE_PHYS 0xff1f1000
#define QEMUVIRTA7_GICC_BASE_PHYS 0xff1f2000
#define QEMUVIRTA7_GIC_SIZE       0x1000
#define QEMUVIRTA7_TIMER_PPI_VIRT 27

#endif /* ! ASSEMBLER */

#endif /* ! _PEXPERT_ARM_RV1106_H */
