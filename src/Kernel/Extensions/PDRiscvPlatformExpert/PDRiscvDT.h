#ifndef _PUREDARWIN_PDRISCVDT_H
#define _PUREDARWIN_PDRISCVDT_H

#include <IOKit/IOTypes.h>

class IORegistryEntry;

// device tree helpers for the riscv platform, cells are native endian as the loader writes them
// a two cell value is one native 64-bit word, the same format pexpert's pe_riscv_dt reads

// true when the string list in "compatible" holds the given entry
bool PDRiscvDT_isCompatible(IORegistryEntry *entry, const char *compatible);

// first enabled node anywhere under the root whose compatible holds one of the names, retained
IORegistryEntry *PDRiscvDT_findCompatible(const char * const *names, unsigned int count);

// a 32-bit cell property, false when missing or short
bool PDRiscvDT_getU32(IORegistryEntry *entry, const char *name, uint32_t *value);

// a 1 or 2 cell value, which covers a riscv cpu node's reg
bool PDRiscvDT_getCell(IORegistryEntry *entry, const char *name, uint64_t *value);

// the node's phandle from AAPL,phandle, phandle or linux,phandle
bool PDRiscvDT_getPHandle(IORegistryEntry *entry, uint32_t *phandle);

// a missing status means the node is usable
bool PDRiscvDT_isEnabled(IORegistryEntry *entry);

// physical address and size of reg entry index, through the ranges of every ancestor bus
bool PDRiscvDT_getReg(IORegistryEntry *entry, unsigned int index, uint64_t *phys, uint64_t *size);

#endif /* _PUREDARWIN_PDRISCVDT_H */
