#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>

extern "C" {
#include <lil/imports.h>
}

// PCI config space
// lil offsets are raw byte offsets into PCI config space.

extern "C" void lil_pci_writeb(void *device, uint16_t offset, uint8_t val) {
    ((IOPCIDevice *)device)->configWrite8(offset, val);
}
extern "C" uint8_t lil_pci_readb(void *device, uint16_t offset) {
    return ((IOPCIDevice *)device)->configRead8(offset);
}
extern "C" void lil_pci_writew(void *device, uint16_t offset, uint16_t val) {
    ((IOPCIDevice *)device)->configWrite16(offset, val);
}
extern "C" uint16_t lil_pci_readw(void *device, uint16_t offset) {
    return ((IOPCIDevice *)device)->configRead16(offset);
}
extern "C" void lil_pci_writed(void *device, uint16_t offset, uint32_t val) {
    ((IOPCIDevice *)device)->configWrite32(offset, val);
}
extern "C" uint32_t lil_pci_readd(void *device, uint16_t offset) {
    return ((IOPCIDevice *)device)->configRead32(offset);
}

// BAR mapping
// lil BAR index N == PCI BAR N. IOPCIDevice memory index maps 1:1 to BAR index
// for the memory BARs we care about (BAR0 = MMIO+GTT, BAR2 = stolen aperture).
// We map write-combined and leak the mapping (driver-lifetime); a future
// revision can track these per-device for lil_unmap teardown.

extern "C" void lil_get_bar(void *device, int bar, uintptr_t *obase, uintptr_t *len) {
    IOPCIDevice *pci = (IOPCIDevice *)device;
    *obase = 0;
    *len = 0;

    // This platform's IOPCIFamily doesn't size BARs (getDeviceMemoryWithIndex
    // returns a zero-length IODeviceMemory, so its map() fails - the same quirk
    // RavynXHCIPort works around). Read the base + size straight from config
    // space and map by physical address instead. The iGPU BAR0/BAR2 are 64-bit.
    const uint8_t off = (uint8_t)(0x10 + bar * 4);
    uint32_t lo = pci->configRead32(off);
    const bool is64 = (lo & 0x6) == 0x4;
    uint64_t base = (uint64_t)(lo & ~0xFU);
    uint32_t hi = 0;
    if (is64) { hi = pci->configRead32(off + 4); base |= ((uint64_t)hi << 32); }

    // Size it: disable memory decode, write all-1s, read back the mask, restore.
    const uint16_t cmd = pci->configRead16(kIOPCIConfigCommand);
    pci->configWrite16(kIOPCIConfigCommand, cmd & ~(uint16_t)0x2);
    pci->configWrite32(off, 0xFFFFFFFFU);
    uint32_t maskLo = pci->configRead32(off);
    uint32_t maskHi = 0xFFFFFFFFU;
    if (is64) { pci->configWrite32(off + 4, 0xFFFFFFFFU); maskHi = pci->configRead32(off + 4); }
    pci->configWrite32(off, lo);
    if (is64) pci->configWrite32(off + 4, hi);
    pci->configWrite16(kIOPCIConfigCommand, cmd);

    const uint64_t mask = ((uint64_t)maskHi << 32) | (maskLo & ~0xFU);
    const uint64_t size = mask ? (~mask + 1) : 0;
    if (!base || !size) {
        IOLog("lil_get_bar: BAR%d unassigned (base=0x%llx size=0x%llx)\n",
              bar, (unsigned long long)base, (unsigned long long)size);
        return;
    }

    // BAR0 = MMIO+GTT (must be uncached for register ordering); BAR2 = the
    // stolen/framebuffer aperture (write-combine). A single cache mode - the
    // old code OR'd InhibitCache|WriteCombineCache, an invalid combination that
    // made map() fail.
    const IOOptionBits cache = (bar == 2) ? kIOMapWriteCombineCache
                                          : kIOMapInhibitCache;
    IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(
        (IOPhysicalAddress)base, (IOByteCount)size, kIODirectionInOut);
    if (!md) {
        IOLog("lil_get_bar: withPhysicalAddress BAR%d (0x%llx/0x%llx) failed\n",
              bar, (unsigned long long)base, (unsigned long long)size);
        return;
    }
    IOMemoryMap *map = md->map(cache);
    if (!map) {
        IOLog("lil_get_bar: map BAR%d failed (size=0x%llx)\n",
              bar, (unsigned long long)size);
        md->release();
        return;
    }

    // Intentionally retained for the driver's lifetime.
    *obase = (uintptr_t)map->getVirtualAddress();
    *len = (uintptr_t)size;
    IOLog("lil_get_bar: BAR%d base=0x%llx size=0x%llx -> va=0x%lx\n",
          bar, (unsigned long long)base, (unsigned long long)size,
          (unsigned long)*obase);
}

// lil_map/lil_unmap: map an arbitrary PHYSICAL address range into the kernel so
// lil can read it. The Intel OpRegion and VBT (Video BIOS Table) live at
// firmware physical addresses read from the ASLS/RVDA PCI config registers
// (see lil/src/vbt/vbt.cpp); without these, lil dereferences a raw physical
// address and the kernel faults (the 0x77fb4018 page fault seen on GLK). A
// small table lets lil_unmap release the transient VBT mapping.
#define LIL_MAP_MAX 8
static struct { void *va; IOMemoryMap *map; IOMemoryDescriptor *md; } gLilMaps[LIL_MAP_MAX];

extern "C" void *lil_map(size_t loc, size_t len) {
    IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(
        (IOPhysicalAddress)loc, (IOByteCount)len, kIODirectionInOut);
    if (!md) {
        IOLog("lil_map: withPhysicalAddress(0x%lx, 0x%lx) failed\n",
              (unsigned long)loc, (unsigned long)len);
        return NULL;
    }
    IOMemoryMap *map = md->map(kIOMapInhibitCache);
    if (!map) {
        IOLog("lil_map: map(0x%lx) failed\n", (unsigned long)loc);
        md->release();
        return NULL;
    }
    void *va = (void *)map->getVirtualAddress();
    for (int i = 0; i < LIL_MAP_MAX; i++) {
        if (!gLilMaps[i].va) {
            gLilMaps[i].va = va;
            gLilMaps[i].map = map;
            gLilMaps[i].md = md;
            return va;
        }
    }
    // Table full: retain the mapping for the driver's lifetime (leak; only the
    // one-time opregion/vbt init maps ever land here).
    return va;
}

extern "C" void lil_unmap(void *loc, size_t len) {
    (void)len;
    for (int i = 0; i < LIL_MAP_MAX; i++) {
        if (gLilMaps[i].va == loc) {
            gLilMaps[i].map->release();
            gLilMaps[i].md->release();
            gLilMaps[i].va = NULL;
            gLilMaps[i].map = NULL;
            gLilMaps[i].md = NULL;
            return;
        }
    }
}

// timing

extern "C" void lil_sleep(uint64_t ms) {
    IOSleep((unsigned)ms);          // may block; lil calls this from start()
}
extern "C" void lil_usleep(uint64_t us) {
    IODelay((unsigned)us);          // busy-wait; safe at any context
}

// allocation
// IOFree needs the size, which lil_free() does not carry, so we stash the
// allocation size in an 8-byte header preceding the returned pointer.

extern "C" void *lil_malloc(size_t s) {
    size_t total = s + sizeof(size_t);
    void *raw = IOMalloc(total);
    if (!raw) {
        panic("lil_malloc: IOMalloc(%zu) failed", total);
    }
    *(size_t *)raw = total;
    return (void *)((uint8_t *)raw + sizeof(size_t));
}

extern "C" void lil_free(void *p) {
    void *raw = (void *)((uint8_t *)p - sizeof(size_t));
    size_t total = *(size_t *)raw;
    IOFree(raw, total);
}

// logging

extern "C" void lil_log(enum LilLogType type, const char *fmt, ...) {
    const char *tag;
    switch (type) {
    case ERROR:   tag = "lil ERROR: "; break;
    case WARNING: tag = "lil WARN: ";  break;
    case INFO:    tag = "lil: ";       break;
    default:      tag = "lil dbg: ";   break;
    }
    // IOLog has no vprintf variant exposed here; prefix then format.
    va_list ap;
    va_start(ap, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    IOLog("%s%s", tag, buf);
}

// Recovery hook: lilBringup() arms this with __builtin_setjmp before calling
// into lil. lil is riddled with lil_assert()/lil_panic() on hardware bringup
// steps; without recovery any single failed assertion (e.g. a Gemini Lake power
// well that doesn't match lil's SKL sequence) panics the whole machine. When
// armed, lil_panic longjmps back to lilBringup so it returns false and IOKit
// falls back to IOGOPFramebuffer - the box still boots, and gen9 can be iterated
// without bricking every attempt.
extern "C" { void *gLilPanicBuf[5]; int gLilPanicArmed = 0; }

extern "C" void lil_panic(const char *msg) {
    IOLog("IOIntelGen9Framebuffer: lil_panic: %s\n", msg);
    if (gLilPanicArmed) {
        gLilPanicArmed = 0;
        __builtin_longjmp(gLilPanicBuf, 1);
    }
    panic("lil_panic: %s", msg);
}
