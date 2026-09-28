#ifndef _PEXPERT_RISCV_PE_RISCV_DT_H_
#define _PEXPERT_RISCV_PE_RISCV_DT_H_

#include <stdint.h>
#include <stdbool.h>
#include <pexpert/device_tree.h>

// a device tree node plus what its parent says about decoding its reg property
// cells are native endian and a two cell value is one native 64-bit word, as the loader writes them
struct pe_riscv_dt_node {
	DTEntry         entry;
	uint32_t        addr_cells;
	uint32_t        size_cells;
	// added to a bus address to get the physical address, from the ranges of every ancestor
	uint64_t        bus_to_phys;
};

typedef bool (*pe_riscv_dt_match_t)(DTEntry entry, const void *ctx);

bool pe_riscv_dt_find(pe_riscv_dt_match_t match, const void *ctx, struct pe_riscv_dt_node *out);
bool pe_riscv_dt_find_compatible(const char *compatible, struct pe_riscv_dt_node *out);
bool pe_riscv_dt_find_entry(DTEntry entry, struct pe_riscv_dt_node *out);

bool pe_riscv_dt_is_compatible(DTEntry entry, const char *compatible);
bool pe_riscv_dt_is_enabled(DTEntry entry);
bool pe_riscv_dt_get_u32(DTEntry entry, const char *name, uint32_t *value);
bool pe_riscv_dt_get_reg(const struct pe_riscv_dt_node *node, unsigned int index,
    uint64_t *phys, uint64_t *size);

#endif /* _PEXPERT_RISCV_PE_RISCV_DT_H_ */
