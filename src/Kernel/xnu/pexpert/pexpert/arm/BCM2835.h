/*
 * Raspberry Pi Zero / Zero W: BCM2835, ARM1176JZF-S (ARMv6Z), single core.
 *
 * The peripheral block is the same VideoCore IP as the BCM2837 supported on
 * the arm64 side - identical GPIO and AUX mini-UART register offsets - at a
 * different base: 0x20000000 here, 0x3F000000 there. pe_serial.c takes the
 * base from the device tree, so the driver itself is shared unchanged; the
 * register names below are what it expects to find.
 */

#ifndef _PEXPERT_ARM_BCM2835_H
#define _PEXPERT_ARM_BCM2835_H

#define NO_MONITOR 1
#define NO_ECORE 1

#define BCM2835
#define BCM2835_BRINGUP

/*
 * ARM1176JZF-S: ARMv6 with the v6K extensions and TrustZone, VMSAv6 short
 * descriptors, VFPv2. Notably NO generic timer - the board has to use the
 * BCM2835 System Timer - so ARM_ARCH_TIMER is deliberately absent.
 */
#define __ARM_ARCH__              6
#define __ARM_VMSA__              6
#define __ARM_VFP__               2
#define __ARM_DEBUG__             6

#ifndef ASSEMBLER

#define PI3_UART

#define PI3_BREAK                               asm volatile("bkpt #0");

#define BCM2835_GPFSEL0_V               (pi3_gpio_base_vaddr + 0x0)
#define BCM2835_GPSET0_V                (pi3_gpio_base_vaddr + 0x1C)
#define BCM2835_GPCLR0_V                (pi3_gpio_base_vaddr + 0x28)
#define BCM2835_GPPUD_V                 (pi3_gpio_base_vaddr + 0x94)
#define BCM2835_GPPUDCLK0_V             (pi3_gpio_base_vaddr + 0x98)

#define BCM2835_FSEL_INPUT              0x0
#define BCM2835_FSEL_OUTPUT             0x1
#define BCM2835_FSEL_ALT0               0x4
#define BCM2835_FSEL_ALT1               0x5
#define BCM2835_FSEL_ALT2               0x6
#define BCM2835_FSEL_ALT3               0x7
#define BCM2835_FSEL_ALT4               0x3
#define BCM2835_FSEL_ALT5               0x2

#define BCM2835_FSEL_NFUNCS             54
#define BCM2835_FSEL_REG(func)          (BCM2835_GPFSEL0_V + (4 * ((func) / 10)))
#define BCM2835_FSEL_OFFS(func)         (((func) % 10) * 3)
#define BCM2835_FSEL_MASK(func)         (0x7 << BCM2835_FSEL_OFFS(func))

#define BCM2835_AUX_ENABLES_V           (pi3_aux_base_vaddr + 0x4)
#define BCM2835_AUX_MU_IO_REG_V         (pi3_aux_base_vaddr + 0x40)
#define BCM2835_AUX_MU_IER_REG_V        (pi3_aux_base_vaddr + 0x44)
#define BCM2835_AUX_MU_IIR_REG_V        (pi3_aux_base_vaddr + 0x48)
#define BCM2835_AUX_MU_LCR_REG_V        (pi3_aux_base_vaddr + 0x4C)
#define BCM2835_AUX_MU_MCR_REG_V        (pi3_aux_base_vaddr + 0x50)
#define BCM2835_AUX_MU_LSR_REG_V        (pi3_aux_base_vaddr + 0x54)
#define BCM2835_AUX_MU_MSR_REG_V        (pi3_aux_base_vaddr + 0x58)
#define BCM2835_AUX_MU_SCRATCH_V        (pi3_aux_base_vaddr + 0x5C)
#define BCM2835_AUX_MU_CNTL_REG_V       (pi3_aux_base_vaddr + 0x60)
#define BCM2835_AUX_MU_STAT_REG_V       (pi3_aux_base_vaddr + 0x64)
#define BCM2835_AUX_MU_BAUD_REG_V       (pi3_aux_base_vaddr + 0x68)
#define BCM2835_PUT32(addr, value) do { *((volatile uint32_t *) addr) = value; } while(0)
#define BCM2835_GET32(addr) *((volatile uint32_t *) addr)

/*
 * pe_serial.c's Pi driver is written against the BCM2837 spelling of these
 * names. The registers are identical, so alias rather than fork the driver.
 */
#define BCM2837_GPFSEL0_V               BCM2835_GPFSEL0_V
#define BCM2837_GPSET0_V                BCM2835_GPSET0_V
#define BCM2837_GPCLR0_V                BCM2835_GPCLR0_V
#define BCM2837_GPPUD_V                 BCM2835_GPPUD_V
#define BCM2837_GPPUDCLK0_V             BCM2835_GPPUDCLK0_V
#define BCM2837_FSEL_ALT5               BCM2835_FSEL_ALT5
#define BCM2837_FSEL_REG(func)          BCM2835_FSEL_REG(func)
#define BCM2837_FSEL_OFFS(func)         BCM2835_FSEL_OFFS(func)
#define BCM2837_FSEL_MASK(func)         BCM2835_FSEL_MASK(func)
#define BCM2837_AUX_ENABLES_V           BCM2835_AUX_ENABLES_V
#define BCM2837_AUX_MU_IO_REG_V         BCM2835_AUX_MU_IO_REG_V
#define BCM2837_AUX_MU_IER_REG_V        BCM2835_AUX_MU_IER_REG_V
#define BCM2837_AUX_MU_IIR_REG_V        BCM2835_AUX_MU_IIR_REG_V
#define BCM2837_AUX_MU_LCR_REG_V        BCM2835_AUX_MU_LCR_REG_V
#define BCM2837_AUX_MU_MCR_REG_V        BCM2835_AUX_MU_MCR_REG_V
#define BCM2837_AUX_MU_LSR_REG_V        BCM2835_AUX_MU_LSR_REG_V
#define BCM2837_AUX_MU_MSR_REG_V        BCM2835_AUX_MU_MSR_REG_V
#define BCM2837_AUX_MU_SCRATCH_V        BCM2835_AUX_MU_SCRATCH_V
#define BCM2837_AUX_MU_CNTL_REG_V       BCM2835_AUX_MU_CNTL_REG_V
#define BCM2837_AUX_MU_STAT_REG_V       BCM2835_AUX_MU_STAT_REG_V
#define BCM2837_AUX_MU_BAUD_REG_V       BCM2835_AUX_MU_BAUD_REG_V
#define BCM2837_PUT32(addr, value)      BCM2835_PUT32(addr, value)
#define BCM2837_GET32(addr)             BCM2835_GET32(addr)

#endif /* ! ASSEMBLER */

#endif /* ! _PEXPERT_ARM_BCM2835_H */
