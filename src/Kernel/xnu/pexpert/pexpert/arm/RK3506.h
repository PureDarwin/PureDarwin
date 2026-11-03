// rockchip rk3506 (luckfox lyra): three cortex-a7s, gic-400, designware 8250 uart
// built as the generic a7 board (VIRTA7) with these addresses, like the rv1106

#ifndef _PEXPERT_ARM_RK3506_H
#define _PEXPERT_ARM_RK3506_H

#define NO_MONITOR 1
#define NO_ECORE 1

// the generic a7 paths: clock, topology, no apple debug or arm-io nodes
#define QEMUVIRTA7

#define __ARM_TIME__              1

// the rockchip a7 class shared with the rv1106, secure or under op-tee
#define DW8250_NS_A7            1

// uart0 as u-boot left it, registers 4 bytes apart
#define DW8250_UART_BASE_PHYS     0xff0a0000

#ifndef ASSEMBLER

#define DW8250_UART

#define DW8250_UART_SIZE          0x1000
#define QEMUVIRTA7_GICD_BASE_PHYS 0xff581000
#define QEMUVIRTA7_GICC_BASE_PHYS 0xff582000
#define QEMUVIRTA7_GIC_SIZE       0x1000
#define QEMUVIRTA7_TIMER_PPI_VIRT 27

#endif /* ! ASSEMBLER */

#endif /* ! _PEXPERT_ARM_RK3506_H */
