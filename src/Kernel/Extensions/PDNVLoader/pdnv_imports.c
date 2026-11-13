// pdnv_imports.c - resolves the resource manager's imports: ported functions, data cells, named traps
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <stdint.h>
#include <string.h>
#include <IOKit/IOLib.h>

#include "pd_elf_rel.h"
#include "pdnv_port.h"

// every ported function, by name
#define PDNV_DECLARE(name) extern void name(void);
PDNV_PORTED(PDNV_DECLARE)
#undef PDNV_DECLARE

static const struct {
	const char *name;
	void (*fn)(void);
} sPorted[] = {
#define PDNV_ENTRY(name) { #name, name },
	PDNV_PORTED(PDNV_ENTRY)
#undef PDNV_ENTRY
};

extern const void *pdnv_parms_table;
extern const unsigned long pdnv_parms_size;
static const unsigned char sTrue = 1;

// the variables os-interface.h declares, with the Linux layer's values for 4 KB pages
static const uint64_t sPageSize = 4096, sMaxPageSize = 4096ull << 10, sPageMask = ~4095ull;
static const unsigned char sPageShift = 12, sFalse = 0;

static const struct {
	const char *name;
	unsigned long size;
	const void *init;
} sVars[] = {
	{ "os_page_size", sizeof(sPageSize), &sPageSize },
	{ "os_max_page_size", sizeof(sMaxPageSize), &sMaxPageSize },
	{ "os_page_mask", sizeof(sPageMask), &sPageMask },
	{ "os_page_shift", 1, &sPageShift },
	// no confidential computing, dma-buf export or IMEX channels here
	{ "os_cc_enabled", 1, &sFalse },
	{ "os_cc_sev_snp_enabled", 1, &sFalse },
	{ "os_cc_sme_enabled", 1, &sFalse },
	{ "os_cc_snp_vtom_enabled", 1, &sFalse },
	{ "os_cc_tdx_enabled", 1, &sFalse },
	{ "os_dma_buf_enabled", 1, &sFalse },
	{ "os_imex_channel_is_supported", 1, &sFalse },
};

int
pdnv_resolve(void *ctx, const char *name, struct pdelf_target *t)
{
	void *trap;

	// storage the resource manager expects the interface layer to define, kept in the image it reaches
	if (strcmp(name, "nv_parms") == 0) {
		*t = (struct pdelf_target){ 0, 1, pdnv_parms_size, pdnv_parms_table };
		return 0;
	}
	if (strcmp(name, "nvidia_caps_root") == 0) {
		*t = (struct pdelf_target){ 0, 1, sizeof(void *), NULL };
		return 0;
	}
	if (strcmp(name, "nv_is_rm_firmware_supported_os") == 0) {
		*t = (struct pdelf_target){ 0, 1, 1, &sTrue };
		return 0;
	}

	for (unsigned i = 0; i < sizeof(sVars) / sizeof(sVars[0]); i++) {
		if (strcmp(name, sVars[i].name) == 0) {
			*t = (struct pdelf_target){ 0, 1, sVars[i].size, sVars[i].init };
			return 0;
		}
	}

	for (unsigned i = 0; i < sizeof(sPorted) / sizeof(sPorted[0]); i++) {
		if (strcmp(name, sPorted[i].name) == 0) {
			*t = (struct pdelf_target){ (uint64_t)(uintptr_t)sPorted[i].fn, 0, 0, NULL };
			return 0;
		}
	}

	trap = pdnv_trap_slot(name);
	if (trap == NULL)
		return -1;

	*t = (struct pdelf_target){ (uint64_t)(uintptr_t)trap, 0, 0, NULL };
	return 0;
}
