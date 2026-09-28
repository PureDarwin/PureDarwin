/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * This file contains the low-level serial drivers used on RISC-V devices.
 * The generic serial console code in osfmk/console/serial_console.c will call
 * into this code to transmit and receive serial data.
 */
#include <kern/clock.h>
#include <kern/debug.h>
#include <libkern/OSBase.h>
#include <libkern/section_keywords.h>
#include <mach/mach_time.h>
#include <machine/atomic.h>
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include <pexpert/protos.h>
#include <pexpert/device_tree.h>
#include <pexpert/riscv/board_config.h>
#include <pexpert/riscv/protos.h>

#include "pe_riscv_dt.h"
#include "pe_sbi.h"

struct pe_serial_functions {
	/* Initialize the underlying serial hardware. */
	void (*init) (void);

	/* Return a non-zero value if the serial interface is ready to send more data. */
	unsigned int (*transmit_ready) (void);

	/* Write a single byte of data to serial. */
	void (*transmit_data) (uint8_t c);

	/* Return a non-zero value if there's a byte of data available. */
	unsigned int (*receive_ready) (void);

	/* Read a single byte from serial. */
	uint8_t (*receive_data) (void);

	/* Pointer to the next serial interface in the linked-list. */
	struct pe_serial_functions *next;
};

static struct pe_serial_functions *gPESF = NULL;

// Whether uart has been initialized already. Kept across sleep/wake so
// serial_init knows to reinitialize after wake.
static bool uart_initted = false;

// a wedged or absent transmitter drops the character instead of hanging the boot
#define PE_SERIAL_TX_SPIN_LIMIT 1000000u

// sv39 base page, device windows are mapped in whole pages
#define PE_SERIAL_IO_PAGE       0x1000ULL

static void
register_serial_functions(struct pe_serial_functions *fns)
{
	fns->next = gPESF;
	gPESF = fns;
}

/*****************************************************************************/

// sbi console, the fallback when the device tree names no uart
// dbcn is preferred, the legacy putchar and getchar calls cover older firmware

static bool pe_sbi_console_probed;
static bool pe_sbi_has_dbcn;
static bool pe_sbi_has_legacy_putchar;
static bool pe_sbi_has_legacy_getchar;
static int pe_sbi_pending_char = -1;
static uint8_t pe_sbi_read_buf;

static void
pe_sbi_console_probe(void)
{
	if (pe_sbi_console_probed) {
		return;
	}

	if (pe_sbi_get_spec_version() == 0) {
		// v0.1 firmware only has the legacy calls
		pe_sbi_has_legacy_putchar = true;
		pe_sbi_has_legacy_getchar = true;
	} else {
		pe_sbi_has_dbcn = pe_sbi_probe_extension(PE_SBI_EXT_DBCN);
		pe_sbi_has_legacy_putchar = pe_sbi_probe_extension(PE_SBI_EXT_LEGACY_PUTCHAR);
		pe_sbi_has_legacy_getchar = pe_sbi_probe_extension(PE_SBI_EXT_LEGACY_GETCHAR);
	}
	pe_sbi_console_probed = true;
}

static bool
pe_sbi_console_available(void)
{
	pe_sbi_console_probe();
	return pe_sbi_has_dbcn || pe_sbi_has_legacy_putchar;
}

static void
pe_sbi_console_init(void)
{
	pe_sbi_console_probe();
}

static unsigned int
pe_sbi_console_transmit_ready(void)
{
	return 1;
}

static void
pe_sbi_console_transmit_data(uint8_t c)
{
	if (pe_sbi_has_dbcn) {
		(void)pe_sbi_dbcn_write_byte((char)c);
	} else if (pe_sbi_has_legacy_putchar) {
		pe_sbi_legacy_putchar((char)c);
	}
}

static int
pe_sbi_console_poll(void)
{
	if (pe_sbi_has_legacy_getchar) {
		return pe_sbi_legacy_getchar();
	}

	if (pe_sbi_has_dbcn) {
		// dbcn reads into physical memory, so this only works once the kernel is mapped
		vm_offset_t phys = ml_vtophys((vm_offset_t)&pe_sbi_read_buf);
		if (phys == 0) {
			return -1;
		}
		struct pe_sbi_ret ret = pe_sbi_dbcn_read(1, phys);
		if (ret.error == PE_SBI_SUCCESS && ret.value == 1) {
			return pe_sbi_read_buf;
		}
	}
	return -1;
}

static unsigned int
pe_sbi_console_receive_ready(void)
{
	// the firmware has no peek, hold on to what a poll returns
	if (pe_sbi_pending_char < 0) {
		pe_sbi_pending_char = pe_sbi_console_poll();
	}
	return pe_sbi_pending_char >= 0;
}

static uint8_t
pe_sbi_console_receive_data(void)
{
	uint8_t c = (uint8_t)pe_sbi_pending_char;

	pe_sbi_pending_char = -1;
	return c;
}

SECURITY_READ_ONLY_LATE(static struct pe_serial_functions) pe_sbi_console_serial_functions =
{
	.init = pe_sbi_console_init,
	.transmit_ready = pe_sbi_console_transmit_ready,
	.transmit_data = pe_sbi_console_transmit_data,
	.receive_ready = pe_sbi_console_receive_ready,
	.receive_data = pe_sbi_console_receive_data,
};

/*****************************************************************************/

// ns16550 compatible uart, register numbers follow the 16550 datasheet
// the byte offset of a register is its number shifted by reg-shift

#define NS16550_RBR_THR_DLL     0       // rx buffer, tx holding, divisor low
#define NS16550_IER_DLM         1       // interrupt enable, divisor high
#define NS16550_FCR             2       // fifo control, write only
#define NS16550_LCR             3       // line control
#define NS16550_MCR             4       // modem control
#define NS16550_LSR             5       // line status
#define DW_APB_UART_USR         31      // designware status, reading it clears busy detect

#define NS16550_FCR_ENABLE      0x01
#define NS16550_FCR_CLEAR_RX    0x02
#define NS16550_FCR_CLEAR_TX    0x04

#define NS16550_LCR_8N1         0x03
#define NS16550_LCR_DLAB        0x80

#define NS16550_MCR_DTR         0x01
#define NS16550_MCR_RTS         0x02

#define NS16550_LSR_DR          0x01
#define NS16550_LSR_THRE        0x20
#define NS16550_LSR_TEMT        0x40

static vm_offset_t ns16550_base = 0;
static uint32_t ns16550_reg_shift = 0;
static uint32_t ns16550_io_width = 1;
static uint32_t ns16550_clock_hz = 0;
static uint32_t ns16550_baud = 0;
static bool ns16550_is_dw = false;

static const char *const ns16550_compatibles[] = {
	"ns16550a",
	"ns16550",
	"ns16450",
	"ns8250",
	"snps,dw-apb-uart",
};

static inline uint32_t
ns16550_read(unsigned int reg)
{
	vm_offset_t addr = ns16550_base + (reg << ns16550_reg_shift);

	switch (ns16550_io_width) {
	case 4:
		return *(volatile uint32_t *)addr;
	case 2:
		return *(volatile uint16_t *)addr;
	default:
		return *(volatile uint8_t *)addr;
	}
}

static inline void
ns16550_write(unsigned int reg, uint32_t value)
{
	vm_offset_t addr = ns16550_base + (reg << ns16550_reg_shift);

	switch (ns16550_io_width) {
	case 4:
		*(volatile uint32_t *)addr = value;
		break;
	case 2:
		*(volatile uint16_t *)addr = (uint16_t)value;
		break;
	default:
		*(volatile uint8_t *)addr = (uint8_t)value;
		break;
	}
}

static unsigned int
ns16550_transmit_ready(void)
{
	return (ns16550_read(NS16550_LSR) & NS16550_LSR_THRE) != 0;
}

static void
ns16550_transmit_data(uint8_t c)
{
	ns16550_write(NS16550_RBR_THR_DLL, c);
}

static unsigned int
ns16550_receive_ready(void)
{
	return (ns16550_read(NS16550_LSR) & NS16550_LSR_DR) != 0;
}

static uint8_t
ns16550_receive_data(void)
{
	return (uint8_t)ns16550_read(NS16550_RBR_THR_DLL);
}

static void
ns16550_init(void)
{
	// polled, the device raises nothing
	ns16550_write(NS16550_IER_DLM, 0);
	ns16550_write(NS16550_FCR, NS16550_FCR_ENABLE | NS16550_FCR_CLEAR_RX | NS16550_FCR_CLEAR_TX);
	ns16550_write(NS16550_MCR, NS16550_MCR_DTR | NS16550_MCR_RTS);

	// without a clock and a speed in the device tree the firmware's line settings stay
	if (ns16550_clock_hz == 0 || ns16550_baud == 0) {
		return;
	}

	// a designware uart drops lcr writes while busy, drain the transmitter first
	for (uint32_t spins = 0; spins < PE_SERIAL_TX_SPIN_LIMIT; spins++) {
		if (ns16550_read(NS16550_LSR) & NS16550_LSR_TEMT) {
			break;
		}
	}
	if (ns16550_is_dw) {
		(void)ns16550_read(DW_APB_UART_USR);
	}

	const uint32_t divisor = (ns16550_clock_hz + 8 * ns16550_baud) / (16 * ns16550_baud);

	ns16550_write(NS16550_LCR, NS16550_LCR_DLAB);
	ns16550_write(NS16550_RBR_THR_DLL, divisor & 0xff);
	ns16550_write(NS16550_IER_DLM, (divisor >> 8) & 0xff);
	ns16550_write(NS16550_LCR, NS16550_LCR_8N1);

	if (ns16550_is_dw) {
		(void)ns16550_read(DW_APB_UART_USR);
	}
}

SECURITY_READ_ONLY_LATE(static struct pe_serial_functions) ns16550_serial_functions =
{
	.init = ns16550_init,
	.transmit_ready = ns16550_transmit_ready,
	.transmit_data = ns16550_transmit_data,
	.receive_ready = ns16550_receive_ready,
	.receive_data = ns16550_receive_data,
};

static bool
ns16550_is_supported(DTEntry entry)
{
	for (size_t i = 0; i < sizeof(ns16550_compatibles) / sizeof(ns16550_compatibles[0]); i++) {
		if (pe_riscv_dt_is_compatible(entry, ns16550_compatibles[i])) {
			return true;
		}
	}
	return false;
}

static bool
ns16550_setup(const struct pe_riscv_dt_node *node)
{
	uint64_t phys, size, page;
	uint32_t prop, reg_offset = 0;

	if (!pe_riscv_dt_get_reg(node, 0, &phys, &size)) {
		return false;
	}

	// reg-shift, reg-io-width, reg-offset, clock-frequency and current-speed are optional
	if (pe_riscv_dt_get_u32(node->entry, "reg-shift", &prop) && prop < 8) {
		ns16550_reg_shift = prop;
	}
	if (pe_riscv_dt_get_u32(node->entry, "reg-io-width", &prop) &&
	    (prop == 1 || prop == 2 || prop == 4)) {
		ns16550_io_width = prop;
	}
	(void)pe_riscv_dt_get_u32(node->entry, "reg-offset", &reg_offset);
	(void)pe_riscv_dt_get_u32(node->entry, "clock-frequency", &ns16550_clock_hz);
	(void)pe_riscv_dt_get_u32(node->entry, "current-speed", &ns16550_baud);
	ns16550_is_dw = pe_riscv_dt_is_compatible(node->entry, "snps,dw-apb-uart");

	phys += reg_offset;
	if (size < (8ULL << ns16550_reg_shift)) {
		size = 8ULL << ns16550_reg_shift;
	}

	page = phys & ~(PE_SERIAL_IO_PAGE - 1);
	size = (phys - page + size + PE_SERIAL_IO_PAGE - 1) & ~(PE_SERIAL_IO_PAGE - 1);
	ns16550_base = ml_io_map((vm_offset_t)page, (vm_size_t)size) + (vm_offset_t)(phys - page);

	register_serial_functions(&ns16550_serial_functions);
	return true;
}

/*****************************************************************************/

// Gets the phandle of the devicetree node that represents the serial device
// XNU has been configured (either via devicetree or bootarg) to use.
static bool
get_serial_device_phandle(uint32_t * const phandle)
{
	bool serial_device_phandle_specified = false;
	DTEntry defaults;

	if (SecureDTLookupEntry(NULL, "/defaults", &defaults) == kSuccess &&
	    pe_riscv_dt_get_u32(defaults, "serial-device", phandle)) {
		serial_device_phandle_specified = true;
	}

	uint32_t phandle_bootarg;
	if (PE_parse_boot_argn("serial-device", &phandle_bootarg, sizeof(phandle_bootarg))) {
		*phandle = phandle_bootarg;
		serial_device_phandle_specified = true;
	}

	char serial_device_name[32];
	if (PE_parse_boot_arg_str("serial-device-name", serial_device_name, sizeof(serial_device_name))) {
		const DeviceTreeNode *serial_device_node;
		if (SecureDTFindNodeWithStringProperty("name", serial_device_name, &serial_device_node) == kSuccess &&
		    pe_riscv_dt_get_u32((DTEntry)serial_device_node, "AAPL,phandle", phandle)) {
			serial_device_phandle_specified = true;
		}
	}

	return serial_device_phandle_specified;
}

// /chosen stdout-path is a node path or an alias, optionally followed by ":options"
static bool
get_stdout_path_entry(DTEntry *entry)
{
	DTEntry chosen, aliases;
	char const *prop;
	unsigned int size;
	char path[128];
	size_t len;

	if (SecureDTLookupEntry(NULL, "/chosen", &chosen) != kSuccess ||
	    SecureDTGetProperty(chosen, "stdout-path", (void const **)&prop, &size) != kSuccess) {
		return false;
	}

	len = strnlen(prop, size);
	for (size_t i = 0; i < len; i++) {
		if (prop[i] == ':') {
			len = i;
			break;
		}
	}
	if (len == 0 || len >= sizeof(path)) {
		return false;
	}
	memcpy(path, prop, len);
	path[len] = '\0';

	if (path[0] != '/') {
		if (SecureDTLookupEntry(NULL, "/aliases", &aliases) != kSuccess ||
		    SecureDTGetProperty(aliases, path, (void const **)&prop, &size) != kSuccess) {
			return false;
		}
		len = strnlen(prop, size);
		if (len == 0 || len >= sizeof(path)) {
			return false;
		}
		memcpy(path, prop, len);
		path[len] = '\0';
	}

	return SecureDTLookupEntry(NULL, path, entry) == kSuccess;
}

static bool
find_serial_device(struct pe_riscv_dt_node *node)
{
	uint32_t phandle;
	DTEntry entry;

	if (get_serial_device_phandle(&phandle)) {
		const DeviceTreeNode *dt_node;
		if (SecureDTFindNodeWithPhandle(phandle, &dt_node) == kSuccess &&
		    pe_riscv_dt_find_entry((DTEntry)dt_node, node)) {
			return true;
		}
	}

	if (get_stdout_path_entry(&entry) && pe_riscv_dt_find_entry(entry, node)) {
		return true;
	}

	for (size_t i = 0; i < sizeof(ns16550_compatibles) / sizeof(ns16550_compatibles[0]); i++) {
		if (pe_riscv_dt_find_compatible(ns16550_compatibles[i], node)) {
			return true;
		}
	}
	return false;
}

int
serial_init(void)
{
	struct pe_serial_functions *fns = gPESF;
	struct pe_riscv_dt_node node;
	bool sbi_console = false;

	if (uart_initted) {
		while (fns != NULL) {
			fns->init();
			fns = fns->next;
		}
		return gPESF != NULL;
	}

	// sbi-console=1 skips the uart and prints through the firmware
	PE_parse_boot_argn("sbi-console", &sbi_console, sizeof(sbi_console));

	if (!sbi_console && find_serial_device(&node) && ns16550_is_supported(node.entry)) {
		(void)ns16550_setup(&node);
	}

	if (gPESF == NULL && pe_sbi_console_available()) {
		register_serial_functions(&pe_sbi_console_serial_functions);
	}

	fns = gPESF;
	while (fns != NULL) {
		fns->init();
		fns = fns->next;
	}

	/* Complete. */
	uart_initted = true;
	return gPESF != NULL;
}

static inline void
uart_putc_device(char c, struct pe_serial_functions *fns)
{
	for (uint32_t spins = 0; !fns->transmit_ready(); spins++) {
		if (spins >= PE_SERIAL_TX_SPIN_LIMIT) {
			return;
		}
	}
	fns->transmit_data((uint8_t)c);
}

// Output a character onto every registered serial interface.
void
uart_putc_options(char c, __unused bool poll)
{
	struct pe_serial_functions *fns = gPESF;

	while (fns != NULL) {
		uart_putc_device(c, fns);
		fns = fns->next;
	}
}

void
uart_putc(char c)
{
	uart_putc_options(c, true);
}

// Read a character from the first registered serial interface that has data
// available. Returns the character, or -1 if no interface has data.
int
uart_getc(void)
{
	struct pe_serial_functions *fns = gPESF;
	while (fns != NULL) {
		if (fns->receive_ready()) {
			return (int)fns->receive_data();
		}
		fns = fns->next;
	}
	return -1;
}

// no lock and no console state, usable from trap handlers and before serial_init
void
pe_riscv_early_putc(char c)
{
	if (ns16550_base != 0) {
		uart_putc_device(c, &ns16550_serial_functions);
		return;
	}
	if (pe_sbi_console_available()) {
		pe_sbi_console_transmit_data((uint8_t)c);
	}
}
