/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
 */
#ifndef _PEXPERT_RISCV_PROTOS_H
#define _PEXPERT_RISCV_PROTOS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern vm_offset_t pe_riscv_get_soc_base_phys(void);
extern uint32_t pe_riscv_init_interrupts(void *args);

// physical reg entry index of the first enabled node whose compatible list has the string
extern bool pe_riscv_get_device_reg(const char *compatible, unsigned int index,
    uint64_t *phys, uint64_t *size);

// unlocked putc for trap handlers and early boot, the uart once mapped and the sbi console before
extern void pe_riscv_early_putc(char c);

#ifdef  PEXPERT_KERNEL_PRIVATE
extern void console_write_unbuffered(char);
#endif
int serial_init(void);
int serial_getc(void);
void serial_putc(char);
void uart_putc(char);
#ifdef PRIVATE
void serial_putc_options(char, bool);
void uart_putc_options(char, bool);
#endif /* PRIVATE */
int uart_getc(void);

int switch_to_serial_console(void);
void switch_to_old_console(int);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _PEXPERT_RISCV_PROTOS_H */
