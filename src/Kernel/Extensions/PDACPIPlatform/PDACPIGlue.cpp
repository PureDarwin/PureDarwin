/*
 * The uacpi_kernel_* layer on XNU. Only the early-table subset so far: uACPI
 * calls just get_rsdp, map, unmap and log before AML starts executing.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSIterator.h>

extern "C" {
#include <uacpi/kernel_api.h>
}

#include "PDACPIPlatformExpert.h"

extern "C" void kprintf(const char *fmt, ...);

// unmap() gets only an address, so the map must be findable from it.
#define kMaxMappings 64

struct PDACPIMapping {
    void        *virt;
    uacpi_size   length;
    IOMemoryMap *map;
};

static PDACPIMapping gMappings[kMaxMappings];
static IOSimpleLock *gMappingLock;   // may be held at IRQ level

static uacpi_phys_addr gRsdpPhys;
static bool            gRsdpSearched;

// The bootloader flattens the EFI configuration table into the device tree.
static uacpi_phys_addr
pd_find_rsdp(void)
{
    IORegistryEntry *cfg =
        IORegistryEntry::fromPath("/efi/configuration-table", gIODTPlane);
    if (cfg == NULL) {
        IOLog("PDACPIPlatform: no /efi/configuration-table in the device tree\n");
        return 0;
    }

    uacpi_phys_addr phys = 0;
    bool haveV2 = false;

    OSIterator *children = cfg->getChildIterator(gIODTPlane);
    if (children != NULL) {
        while (OSObject *next = children->getNextObject()) {
            IORegistryEntry *child = OSDynamicCast(IORegistryEntry, next);
            if (child == NULL)
                continue;

            OSData *alias = OSDynamicCast(OSData, child->getProperty("alias"));
            OSData *table = OSDynamicCast(OSData, child->getProperty("table"));
            if (alias == NULL || table == NULL || table->getLength() < sizeof(UInt64))
                continue;

            const char *a = (const char *)alias->getBytesNoCopy();
            bool isV2 = (strncmp(a, "ACPI_20", 7) == 0);
            if (!isV2 && strncmp(a, "ACPI", 4) != 0)
                continue;

            // ACPI 2.0's RSDP has the 64-bit XSDT.
            if (haveV2 && !isV2)
                continue;

            phys = (uacpi_phys_addr)*(const UInt64 *)table->getBytesNoCopy();
            haveV2 = isV2;
        }
        children->release();
    }
    cfg->release();

    if (phys == 0)
        IOLog("PDACPIPlatform: no ACPI RSDP entry in the EFI configuration table\n");

    return phys;
}

// Diagnostic: a prelinked kext's __bss must arrive zeroed.
static uint64_t gBssCanary[4];

bool
PDACPIGlueInit(void)
{
    kprintf("PDACPIPlatform: bss canary %llx %llx %llx %llx (want all 0)\n",
            (unsigned long long)gBssCanary[0], (unsigned long long)gBssCanary[1],
            (unsigned long long)gBssCanary[2], (unsigned long long)gBssCanary[3]);

    if (gMappingLock == NULL) {
        gMappingLock = IOSimpleLockAlloc();
        if (gMappingLock == NULL)
            return false;
    }
    bzero(gMappings, sizeof(gMappings));
    return true;
}

void
PDACPIGlueFree(void)
{
    // Still-mapped entries are a uACPI leak; the mapping is ours to drop.
    for (unsigned i = 0; i < kMaxMappings; i++) {
        if (gMappings[i].map != NULL) {
            IOLog("PDACPIPlatform: releasing leaked mapping of %p (%llu bytes)\n",
                  gMappings[i].virt, (uint64_t)gMappings[i].length);
            gMappings[i].map->release();
            gMappings[i].map = NULL;
            gMappings[i].virt = NULL;
        }
    }

    if (gMappingLock != NULL) {
        IOSimpleLockFree(gMappingLock);
        gMappingLock = NULL;
    }
}

extern "C" {

uacpi_status
uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address)
{
    if (out_rsdp_address == UACPI_NULL)
        return UACPI_STATUS_INVALID_ARGUMENT;

    if (!gRsdpSearched) {
        gRsdpPhys = pd_find_rsdp();
        gRsdpSearched = true;
    }

    if (gRsdpPhys == 0)
        return UACPI_STATUS_NOT_FOUND;

    *out_rsdp_address = gRsdpPhys;
    return UACPI_STATUS_OK;
}

// Tables are firmware memory with no existing descriptor. Cacheable: plain RAM.
void *
uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    if (len == 0)
        return UACPI_NULL;

    // A table can start mid-page: map from the boundary, offset back.
    uacpi_phys_addr pageBase = addr & ~(uacpi_phys_addr)(PAGE_SIZE - 1);
    uacpi_size      offset   = (uacpi_size)(addr - pageBase);
    uacpi_size      mapLen   = round_page(offset + len);

    IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
        (IOPhysicalAddress)pageBase, (IOByteCount)mapLen, kIODirectionInOut);
    if (desc == NULL)
        return UACPI_NULL;

    IOMemoryMap *map = desc->map(kIOMapAnywhere);
    desc->release();   // the map holds its own reference
    if (map == NULL)
        return UACPI_NULL;

    void *virt = (void *)(map->getVirtualAddress() + offset);

    IOInterruptState is = IOSimpleLockLockDisableInterrupt(gMappingLock);
    unsigned slot;
    for (slot = 0; slot < kMaxMappings; slot++) {
        if (gMappings[slot].map == NULL) {
            gMappings[slot].virt   = virt;
            gMappings[slot].length = len;
            gMappings[slot].map    = map;
            break;
        }
    }
    IOSimpleLockUnlockEnableInterrupt(gMappingLock, is);

    if (slot == kMaxMappings) {
        // Better to fail than return a mapping unmap() cannot find.
        IOLog("PDACPIPlatform: out of mapping slots (max %u)\n", kMaxMappings);
        map->release();
        return UACPI_NULL;
    }

    return virt;
}

void
uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    if (addr == UACPI_NULL)
        return;

    IOMemoryMap *map = NULL;

    IOInterruptState is = IOSimpleLockLockDisableInterrupt(gMappingLock);
    for (unsigned i = 0; i < kMaxMappings; i++) {
        if (gMappings[i].map != NULL && gMappings[i].virt == addr) {
            map = gMappings[i].map;
            gMappings[i].map  = NULL;
            gMappings[i].virt = NULL;
            break;
        }
    }
    IOSimpleLockUnlockEnableInterrupt(gMappingLock, is);

    if (map == NULL) {
        IOLog("PDACPIPlatform: unmap of unknown address %p (%llu bytes)\n",
              addr, (uint64_t)len);
        return;
    }

    map->release();
}

// Already formatted by uACPI; "%s" so a stray % is not a conversion.
void
uacpi_kernel_log(uacpi_log_level level, const uacpi_char *text)
{
    const char *tag;

    switch (level) {
    case UACPI_LOG_DEBUG: tag = "debug"; break;
    case UACPI_LOG_TRACE: tag = "trace"; break;
    case UACPI_LOG_INFO:  tag = "info";  break;
    case UACPI_LOG_WARN:  tag = "warn";  break;
    case UACPI_LOG_ERROR: tag = "error"; break;
    default:              tag = "?";     break;
    }

    IOLog("PDACPIPlatform[%s]: %s", tag, text);
}

} // extern "C"
