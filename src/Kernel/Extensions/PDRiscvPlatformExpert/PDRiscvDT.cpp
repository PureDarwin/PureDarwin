#include "PDRiscvDT.h"
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <libkern/c++/OSData.h>

// the loader writes one native 64-bit word where the fdt had two cells
#define PD_DT_DEFAULT_CELLS	2
#define PD_DT_MAX_CELLS		4

static uint64_t
pd_dt_read_cells(const uint8_t *p, uint32_t cells)
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

static uint32_t
pd_dt_cells(IORegistryEntry *entry, const char *name)
{
	uint32_t cells;

	if (entry == NULL || !PDRiscvDT_getU32(entry, name, &cells) || cells > PD_DT_MAX_CELLS) {
		return PD_DT_DEFAULT_CELLS;
	}
	return cells;
}

bool
PDRiscvDT_isCompatible(IORegistryEntry *entry, const char *compatible)
{
	OSData *compat = OSDynamicCast(OSData, entry->getProperty("compatible"));
	size_t want = strlen(compatible);

	if (compat == NULL) {
		return false;
	}

	const char *str = (const char *)compat->getBytesNoCopy();
	unsigned int len = compat->getLength();

	// a list of nul terminated strings
	for (unsigned int off = 0; str != NULL && off < len;) {
		size_t n = strnlen(str + off, len - off);

		if (n == want && strncmp(str + off, compatible, want) == 0) {
			return true;
		}
		off += (unsigned int)n + 1;
	}
	return false;
}

bool
PDRiscvDT_isEnabled(IORegistryEntry *entry)
{
	OSData *status = OSDynamicCast(OSData, entry->getProperty("status"));

	if (status == NULL || status->getLength() == 0) {
		return true;
	}

	const char *s = (const char *)status->getBytesNoCopy();
	unsigned int len = status->getLength();

	return strncmp(s, "okay", len) == 0 || strncmp(s, "ok", len) == 0;
}

IORegistryEntry *
PDRiscvDT_findCompatible(const char * const *names, unsigned int count)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	IORegistryIterator *iter;
	IORegistryEntry *found = NULL;

	if (root == NULL) {
		return NULL;
	}
	iter = IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively);
	if (iter != NULL) {
		while (found == NULL) {
			IORegistryEntry *next = iter->getNextObject();

			if (next == NULL) {
				break;
			}
			if (!PDRiscvDT_isEnabled(next)) {
				continue;
			}
			for (unsigned int i = 0; i < count; i++) {
				if (PDRiscvDT_isCompatible(next, names[i])) {
					found = next;
					found->retain();
					break;
				}
			}
		}
		iter->release();
	}
	root->release();
	return found;
}

bool
PDRiscvDT_getU32(IORegistryEntry *entry, const char *name, uint32_t *value)
{
	OSData *data = OSDynamicCast(OSData, entry->getProperty(name));

	if (data == NULL || data->getLength() < sizeof(uint32_t)) {
		return false;
	}
	memcpy(value, data->getBytesNoCopy(), sizeof(*value));
	return true;
}

bool
PDRiscvDT_getCell(IORegistryEntry *entry, const char *name, uint64_t *value)
{
	OSData *data = OSDynamicCast(OSData, entry->getProperty(name));

	if (data == NULL) {
		return false;
	}
	if (data->getLength() >= sizeof(uint64_t)) {
		*value = pd_dt_read_cells((const uint8_t *)data->getBytesNoCopy(), 2);
		return true;
	}
	if (data->getLength() >= sizeof(uint32_t)) {
		*value = pd_dt_read_cells((const uint8_t *)data->getBytesNoCopy(), 1);
		return true;
	}
	return false;
}

bool
PDRiscvDT_getPHandle(IORegistryEntry *entry, uint32_t *phandle)
{
	return PDRiscvDT_getU32(entry, "AAPL,phandle", phandle) ||
	       PDRiscvDT_getU32(entry, "phandle", phandle) ||
	       PDRiscvDT_getU32(entry, "linux,phandle", phandle);
}

// the first ranges entry maps a bus onto its parent, an empty or missing ranges is identity
static uint64_t
pd_dt_bus_offset(IORegistryEntry *bus, IORegistryEntry *busParent)
{
	OSData *ranges = OSDynamicCast(OSData, bus->getProperty("ranges"));
	uint32_t childCells = pd_dt_cells(bus, "#address-cells");
	uint32_t parentCells = pd_dt_cells(busParent, "#address-cells");
	uint32_t sizeCells = pd_dt_cells(bus, "#size-cells");
	uint32_t entryLen = (childCells + parentCells + sizeCells) * sizeof(uint32_t);

	if (ranges == NULL || ranges->getLength() < entryLen || entryLen == 0) {
		return 0;
	}

	const uint8_t *r = (const uint8_t *)ranges->getBytesNoCopy();
	uint64_t child = pd_dt_read_cells(r, childCells);
	uint64_t parent = pd_dt_read_cells(r + childCells * sizeof(uint32_t), parentCells);

	return parent - child;
}

bool
PDRiscvDT_getReg(IORegistryEntry *entry, unsigned int index, uint64_t *phys, uint64_t *size)
{
	IORegistryEntry *bus = entry->getParentEntry(gIODTPlane);
	OSData *reg = OSDynamicCast(OSData, entry->getProperty("reg"));
	uint32_t addrCells = pd_dt_cells(bus, "#address-cells");
	uint32_t sizeCells = pd_dt_cells(bus, "#size-cells");
	uint32_t entryLen = (addrCells + sizeCells) * sizeof(uint32_t);
	uint64_t offset = 0;

	if (reg == NULL || entryLen == 0 || reg->getLength() < (index + 1) * entryLen) {
		return false;
	}

	const uint8_t *r = (const uint8_t *)reg->getBytesNoCopy() + index * entryLen;

	// every bus between the node and the root may translate its children
	while (bus != NULL) {
		IORegistryEntry *busParent = bus->getParentEntry(gIODTPlane);

		if (busParent == NULL) {
			break;
		}
		offset += pd_dt_bus_offset(bus, busParent);
		bus = busParent;
	}

	*phys = offset + pd_dt_read_cells(r, addrCells);
	if (size != NULL) {
		*size = pd_dt_read_cells(r + addrCells * sizeof(uint32_t), sizeCells);
	}
	return true;
}
