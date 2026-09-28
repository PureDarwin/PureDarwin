/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
 */
#include <pexpert/pexpert.h>
#include <pexpert/boot.h>
#include <pexpert/protos.h>
#include <pexpert/device_tree.h>
#include <pexpert/riscv/board_config.h>

#include <machine/machine_routines.h>

#include <kern/clock.h>
#include <kern/locks.h>

#include "pe_riscv_dt.h"

/* Local declarations */
void pe_identify_machine(boot_args * bootArgs);

// deep enough for root, soc, a bridge and its children
#define PE_RISCV_DT_MAX_DEPTH   8

// the loader writes two cells per address and size unless a bus says otherwise
#define PE_RISCV_DT_DEFAULT_CELLS 2

// used when neither /cpus nor a cpu node carries timebase-frequency
#define PE_RISCV_DEFAULT_TIMEBASE_HZ 10000000UL

static vm_offset_t gPESoCBasePhys;
static bool gPESoCBaseFound;

static uint64_t
pe_riscv_dt_read_cells(const uint8_t *p, uint32_t cells)
{
	uint32_t v32;
	uint64_t v64;

	switch (cells) {
	case 0:
		return 0;
	case 1:
		memcpy(&v32, p, sizeof(v32));
		return v32;
	default:
		// wider values keep their low 64 bits in the last two cells
		memcpy(&v64, p + (cells - 2) * sizeof(uint32_t), sizeof(v64));
		return v64;
	}
}

bool
pe_riscv_dt_get_u32(DTEntry entry, const char *name, uint32_t *value)
{
	void const *prop;
	unsigned int size;

	if (SecureDTGetProperty(entry, name, &prop, &size) != kSuccess || size < sizeof(uint32_t)) {
		return false;
	}
	memcpy(value, prop, sizeof(*value));
	return true;
}

static uint64_t
pe_riscv_dt_get_freq(DTEntry entry, const char *name, uint64_t dflt)
{
	void const *prop;
	unsigned int size;

	if (SecureDTGetProperty(entry, name, &prop, &size) != kSuccess) {
		return dflt;
	}
	if (size == sizeof(uint64_t)) {
		return pe_riscv_dt_read_cells(prop, 2);
	}
	if (size == sizeof(uint32_t)) {
		return pe_riscv_dt_read_cells(prop, 1);
	}
	return dflt;
}

bool
pe_riscv_dt_is_compatible(DTEntry entry, const char *compatible)
{
	char const *prop;
	unsigned int size;
	size_t want = strlen(compatible);

	if (SecureDTGetProperty(entry, "compatible", (void const **)&prop, &size) != kSuccess) {
		return false;
	}

	// a string list, each entry nul terminated
	for (unsigned int off = 0; off < size;) {
		size_t len = strnlen(prop + off, size - off);
		if (len == want && strncmp(prop + off, compatible, want) == 0) {
			return true;
		}
		off += (unsigned int)len + 1;
	}
	return false;
}

bool
pe_riscv_dt_is_enabled(DTEntry entry)
{
	char const *status;
	unsigned int size;

	if (SecureDTGetProperty(entry, "status", (void const **)&status, &size) != kSuccess) {
		return true;
	}
	return strncmp(status, "okay", size) == 0 || strncmp(status, "ok", size) == 0;
}

static uint32_t
pe_riscv_dt_cells(DTEntry entry, const char *name, uint32_t dflt)
{
	uint32_t cells;

	if (!pe_riscv_dt_get_u32(entry, name, &cells) || cells > 4) {
		return dflt;
	}
	return cells;
}

// the first ranges entry maps the child bus onto its parent, an empty ranges is identity
static uint64_t
pe_riscv_dt_bus_offset(DTEntry bus, uint32_t parent_addr_cells)
{
	uint8_t const *ranges;
	unsigned int size;
	uint32_t child_cells = pe_riscv_dt_cells(bus, "#address-cells", PE_RISCV_DT_DEFAULT_CELLS);
	uint32_t size_cells = pe_riscv_dt_cells(bus, "#size-cells", PE_RISCV_DT_DEFAULT_CELLS);
	uint32_t entry_len = (child_cells + parent_addr_cells + size_cells) * sizeof(uint32_t);

	if (SecureDTGetProperty(bus, "ranges", (void const **)&ranges, &size) != kSuccess ||
	    size == 0 || size < entry_len) {
		return 0;
	}

	uint64_t child = pe_riscv_dt_read_cells(ranges, child_cells);
	uint64_t parent = pe_riscv_dt_read_cells(ranges + child_cells * sizeof(uint32_t), parent_addr_cells);
	return parent - child;
}

static bool
pe_riscv_dt_walk(DTEntry scope, uint32_t depth, uint32_t addr_cells, uint32_t size_cells,
    uint64_t bus_to_phys, pe_riscv_dt_match_t match, const void *ctx, struct pe_riscv_dt_node *out)
{
	OpaqueDTEntryIterator iter;
	DTEntry child;

	// fixed size iterators on the stack, the scope helpers allocate and this runs before zalloc
	if (SecureDTInitEntryIterator(scope, &iter) != kSuccess) {
		return false;
	}

	while (SecureDTIterateEntries(&iter, &child) == kSuccess) {
		if (match(child, ctx)) {
			out->entry = child;
			out->addr_cells = addr_cells;
			out->size_cells = size_cells;
			out->bus_to_phys = bus_to_phys;
			return true;
		}
		if (depth + 1 >= PE_RISCV_DT_MAX_DEPTH || !pe_riscv_dt_is_enabled(child)) {
			continue;
		}
		if (pe_riscv_dt_walk(child, depth + 1,
		    pe_riscv_dt_cells(child, "#address-cells", PE_RISCV_DT_DEFAULT_CELLS),
		    pe_riscv_dt_cells(child, "#size-cells", PE_RISCV_DT_DEFAULT_CELLS),
		    bus_to_phys + pe_riscv_dt_bus_offset(child, addr_cells),
		    match, ctx, out)) {
			return true;
		}
	}
	return false;
}

bool
pe_riscv_dt_find(pe_riscv_dt_match_t match, const void *ctx, struct pe_riscv_dt_node *out)
{
	DTEntry root;

	if (SecureDTLookupEntry(NULL, "/", &root) != kSuccess) {
		return false;
	}
	return pe_riscv_dt_walk(root, 0,
	           pe_riscv_dt_cells(root, "#address-cells", PE_RISCV_DT_DEFAULT_CELLS),
	           pe_riscv_dt_cells(root, "#size-cells", PE_RISCV_DT_DEFAULT_CELLS),
	           0, match, ctx, out);
}

static bool
pe_riscv_dt_match_compatible(DTEntry entry, const void *ctx)
{
	return pe_riscv_dt_is_enabled(entry) && pe_riscv_dt_is_compatible(entry, (const char *)ctx);
}

bool
pe_riscv_dt_find_compatible(const char *compatible, struct pe_riscv_dt_node *out)
{
	return pe_riscv_dt_find(pe_riscv_dt_match_compatible, compatible, out);
}

static bool
pe_riscv_dt_match_entry(DTEntry entry, const void *ctx)
{
	return SecureDTEntryIsEqual(entry, (const DTEntry)ctx);
}

bool
pe_riscv_dt_find_entry(DTEntry entry, struct pe_riscv_dt_node *out)
{
	return pe_riscv_dt_find(pe_riscv_dt_match_entry, entry, out);
}

bool
pe_riscv_dt_get_reg(const struct pe_riscv_dt_node *node, unsigned int index,
    uint64_t *phys, uint64_t *size)
{
	uint8_t const *reg;
	unsigned int reg_size;
	uint32_t addr_len = node->addr_cells * sizeof(uint32_t);
	uint32_t entry_len = addr_len + node->size_cells * sizeof(uint32_t);

	if (entry_len == 0 ||
	    SecureDTGetProperty(node->entry, "reg", (void const **)&reg, &reg_size) != kSuccess ||
	    reg_size < (index + 1) * entry_len) {
		return false;
	}

	reg += index * entry_len;
	*phys = node->bus_to_phys + pe_riscv_dt_read_cells(reg, node->addr_cells);
	if (size != NULL) {
		*size = pe_riscv_dt_read_cells(reg + addr_len, node->size_cells);
	}
	return true;
}

bool
pe_riscv_get_device_reg(const char *compatible, unsigned int index, uint64_t *phys, uint64_t *size)
{
	struct pe_riscv_dt_node node;

	if (!pe_riscv_dt_find_compatible(compatible, &node)) {
		return false;
	}
	return pe_riscv_dt_get_reg(&node, index, phys, size);
}

// pe_identify_machine: sets up platform parameters. Returns: nothing
void
pe_identify_machine(boot_args * bootArgs)
{
	OpaqueDTEntryIterator iter;
	DTEntry         cpus, cpu;
	void const     *value;
	unsigned int    size;
	bool            have_cpu = false;

	(void)bootArgs;

	/* Clear the gPEClockFrequencyInfo struct */
	bzero((void *)&gPEClockFrequencyInfo, sizeof(clock_frequency_info_t));

	/* Start with default values. */
	gPEClockFrequencyInfo.timebase_frequency_hz = 0;
	gPEClockFrequencyInfo.bus_clock_rate_hz = 100000000;
	gPEClockFrequencyInfo.cpu_clock_rate_hz = 400000000;

	if (SecureDTLookupEntry(NULL, "/cpus", &cpus) != kSuccess) {
		cpus = NULL;
	}

	// the riscv binding puts the timebase on /cpus, a cpu node may still carry its own
	if (cpus != NULL) {
		gPEClockFrequencyInfo.timebase_frequency_hz =
		    (unsigned long)pe_riscv_dt_get_freq(cpus, "timebase-frequency", 0);
	}

	if (cpus != NULL && SecureDTInitEntryIterator(cpus, &iter) == kSuccess) {
		while (!have_cpu && kSuccess == SecureDTIterateEntries(&iter, &cpu)) {
			if (SecureDTGetProperty(cpu, "device_type", &value, &size) != kSuccess ||
			    strncmp((char const *)value, "cpu", size) != 0 ||
			    !pe_riscv_dt_is_enabled(cpu)) {
				continue;
			}
			// the loader marks the boot hart running, a plain fdt conversion has no state
			if (SecureDTGetProperty(cpu, "state", &value, &size) == kSuccess &&
			    strncmp((char const *)value, "running", size) != 0) {
				continue;
			}
			have_cpu = true;

			if (gPEClockFrequencyInfo.timebase_frequency_hz == 0) {
				gPEClockFrequencyInfo.timebase_frequency_hz =
				    (unsigned long)pe_riscv_dt_get_freq(cpu, "timebase-frequency", 0);
			}

			gPEClockFrequencyInfo.bus_frequency_hz = pe_riscv_dt_get_freq(cpu, "bus-frequency", 0);
			gPEClockFrequencyInfo.mem_frequency_hz = pe_riscv_dt_get_freq(cpu, "memory-frequency", 0);
			gPEClockFrequencyInfo.prf_frequency_hz = pe_riscv_dt_get_freq(cpu, "peripheral-frequency", 0);
			gPEClockFrequencyInfo.fix_frequency_hz = pe_riscv_dt_get_freq(cpu, "fixed-frequency", 0);
			gPEClockFrequencyInfo.cpu_frequency_hz = pe_riscv_dt_get_freq(cpu, "clock-frequency", 0);
		}
	}

	if (gPEClockFrequencyInfo.timebase_frequency_hz == 0) {
		gPEClockFrequencyInfo.timebase_frequency_hz = PE_RISCV_DEFAULT_TIMEBASE_HZ;
	}
	gPEClockFrequencyInfo.dec_clock_rate_hz = gPEClockFrequencyInfo.timebase_frequency_hz;
	gPEClockFrequencyInfo.timebase_frequency_num = gPEClockFrequencyInfo.timebase_frequency_hz;
	gPEClockFrequencyInfo.timebase_frequency_den = 1;

	gPEClockFrequencyInfo.bus_frequency_min_hz = gPEClockFrequencyInfo.bus_frequency_hz;
	gPEClockFrequencyInfo.bus_frequency_max_hz = gPEClockFrequencyInfo.bus_frequency_hz;
	if (gPEClockFrequencyInfo.bus_frequency_hz != 0) {
		if (gPEClockFrequencyInfo.bus_frequency_hz < 0x100000000ULL) {
			gPEClockFrequencyInfo.bus_clock_rate_hz = (unsigned long)gPEClockFrequencyInfo.bus_frequency_hz;
		} else {
			gPEClockFrequencyInfo.bus_clock_rate_hz = 0xFFFFFFFF;
		}
	}

	gPEClockFrequencyInfo.mem_frequency_min_hz = gPEClockFrequencyInfo.mem_frequency_hz;
	gPEClockFrequencyInfo.mem_frequency_max_hz = gPEClockFrequencyInfo.mem_frequency_hz;
	gPEClockFrequencyInfo.prf_frequency_min_hz = gPEClockFrequencyInfo.prf_frequency_hz;
	gPEClockFrequencyInfo.prf_frequency_max_hz = gPEClockFrequencyInfo.prf_frequency_hz;

	gPEClockFrequencyInfo.cpu_frequency_min_hz = gPEClockFrequencyInfo.cpu_frequency_hz;
	gPEClockFrequencyInfo.cpu_frequency_max_hz = gPEClockFrequencyInfo.cpu_frequency_hz;
	if (gPEClockFrequencyInfo.cpu_frequency_hz != 0) {
		if (gPEClockFrequencyInfo.cpu_frequency_hz < 0x100000000ULL) {
			gPEClockFrequencyInfo.cpu_clock_rate_hz = (unsigned long)gPEClockFrequencyInfo.cpu_frequency_hz;
		} else {
			gPEClockFrequencyInfo.cpu_clock_rate_hz = 0xFFFFFFFF;
		}
	}

	/* Set the num / den pairs form the hz values. */
	gPEClockFrequencyInfo.bus_clock_rate_num = gPEClockFrequencyInfo.bus_clock_rate_hz;
	gPEClockFrequencyInfo.bus_clock_rate_den = 1;

	gPEClockFrequencyInfo.bus_to_cpu_rate_num =
	    (2 * gPEClockFrequencyInfo.cpu_clock_rate_hz) / gPEClockFrequencyInfo.bus_clock_rate_hz;
	gPEClockFrequencyInfo.bus_to_cpu_rate_den = 2;

	gPEClockFrequencyInfo.bus_to_dec_rate_num = 1;
	gPEClockFrequencyInfo.bus_to_dec_rate_den =
	    gPEClockFrequencyInfo.bus_clock_rate_hz / gPEClockFrequencyInfo.dec_clock_rate_hz;
}

static bool
pe_riscv_dt_match_soc(DTEntry entry, const void *ctx __unused)
{
	char const *name;
	unsigned int size;

	if (SecureDTGetProperty(entry, "name", (void const **)&name, &size) != kSuccess) {
		return false;
	}
	// riscv-io is what the loader builds, soc is the usual fdt name for the peripheral bus
	return strncmp(name, "riscv-io", size) == 0 || strncmp(name, "soc", size) == 0;
}

// the offset from soc bus addresses to physical ones, 0 when the soc bus is identity mapped
vm_offset_t
pe_riscv_get_soc_base_phys(void)
{
	struct pe_riscv_dt_node soc;

	if (!gPESoCBaseFound && pe_riscv_dt_find(pe_riscv_dt_match_soc, NULL, &soc)) {
		gPESoCBasePhys = (vm_offset_t)(soc.bus_to_phys +
		    pe_riscv_dt_bus_offset(soc.entry, soc.addr_cells));
		gPESoCBaseFound = true;
	}
	return gPESoCBasePhys;
}

static uint32_t
pe_riscv_init_timer(void *args)
{
	// the timer is sbi or sstc driven from osfmk, no fiq and no decrementer hooks
	struct tbd_ops  empty_funcs = {NULL, NULL, NULL};

	if (args != NULL) {
		ml_init_timebase(args, &empty_funcs, 0, 0);
	}

	return 1;
}

// the plic and its per hart contexts belong to osfmk and the platform kext
uint32_t
pe_riscv_init_interrupts(void *args)
{
	kprintf("pe_riscv_init_interrupts: args: %p timebase %lu Hz\n", args,
	    gPEClockFrequencyInfo.timebase_frequency_hz);

	return pe_riscv_init_timer(args);
}
