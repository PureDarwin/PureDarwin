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
/*
 * 7, not 6, and deliberately so. XNU spells its 32-bit page-table support
 * "VMSA7", and tests for it as __ARM_VMSA__ == 7 in 39 places and > 7 (meaning
 * arm64) in 45 more - there is no VMSA6 branch anywhere in the tree. Declaring
 * 6 does not select a v6 path, it selects the arm64 one, which is why every
 * arm32 file collapsed.
 *
 * It is also the truth: with SCTLR.XP set, ARM1176 uses the extended
 * (subpages-disabled) short-descriptor format, which is the same layout ARMv7
 * made mandatory. start.s must set SCTLR.XP for this to hold.
 */
#define __ARM_VMSA__              7
#define __ARM_VFP__               2
#define __ARM_DEBUG__             6

#ifndef ASSEMBLER

/*
 * Use UART0 (the ARM PL011 at peripheral offset 0x201000) exclusively.
 * PI3_UART selects the AUX mini-UART driver and would remux GPIO14/15 to
 * ALT5 when serial_init() runs, disconnecting the PL011 console inherited
 * from LK.  VMAPPLE_UART is XNU's polled PL011 implementation; the device
 * tree supplied by LK exposes this block as uart0.
 */
#define VMAPPLE_UART

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

/*
 * System Timer and ARMCTRL interrupt controller. Bus addresses 0x7E003000 and
 * 0x7E00B000 are ARM physical 0x20003000 and 0x2000B000; start.s maps the
 * first megabyte of the peripheral window V=P, which covers both.
 *
 * These are outside the ASSEMBLER guard because the decrementer routines in
 * machine_routines_asm.s use the register offsets.
 */
#define BCM2835_ST_BASE_V               0x20003000
#define BCM2835_ST_CS                   0x00    /* control/status, match bits */
#define BCM2835_ST_CLO                  0x04    /* counter, low 32 bits */
#define BCM2835_ST_CHI                  0x08    /* counter, high 32 bits */
#define BCM2835_ST_C3                   0x18    /* compare channel 3 */
#define BCM2835_ST_M3                   (1 << 3) /* channel 3 match, write to clear */

/*
 * The compare is an equality match against the low 32 bits of the counter, not
 * a "greater or equal", so a deadline written even one tick late is missed
 * until the counter wraps ~71 minutes later. Deadlines are floored this far
 * ahead to stay clear of that.
 */
#define BCM2835_DEC_MIN                 16      /* ticks, i.e. microseconds */

#define BCM2835_ARMCTRL_BASE_V          0x2000B000
#define BCM2835_ARMCTRL_FIQ_CONTROL     0x20C
#define BCM2835_FIQ_ENABLE              (1 << 7)
#define BCM2835_FIQ_SRC_SYSTEM_TIMER_3  3       /* GPU IRQ 3 = System Timer match 3 */

#endif /* ! _PEXPERT_ARM_BCM2835_H */
