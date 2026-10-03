/*
* Qualcomm TLMM GPIO interrupts as an IOInterruptController.
*/
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <kern/thread.h>
#include <pexpert/pexpert.h>

#include "tlmm_regs.h"

#define TLMM_MAX_PINS           1024
#define TLMM_WATCH_SECONDS      30
#define TLMM_UNCLAIMED_LIMIT    64
#define CONTROLLER_WAIT_NS      (5ULL * 1000 * 1000 * 1000)

// devicetree trigger types
#define DT_IRQ_EDGE_RISING      1
#define DT_IRQ_EDGE_FALLING     2
#define DT_IRQ_LEVEL_HIGH       4
#define DT_IRQ_LEVEL_LOW        8

class PDQcomTLMM : public IOInterruptController
{
    OSDeclareDefaultStructors(PDQcomTLMM)

private:
    IOMemoryMap         *fMap;
    volatile UInt8      *fRegs;
    UInt32               fPins;
    IOSimpleLock        *fLock;
    const OSSymbol      *fName;
    UInt32               fMode;
    UInt8                fFlags[TLMM_MAX_PINS];
    UInt32               fEnabled[TLMM_MAX_PINS / 32];
    UInt32               fReserved[TLMM_MAX_PINS / 32];
    IOService           *fProvider;
    UInt32               fUnclaimed;

    UInt32
    read32(UInt32 off) const
    {
        return *(volatile UInt32 *)(fRegs + off);
    }
    void
    write32(UInt32 off, UInt32 v) const
    {
        *(volatile UInt32 *)(fRegs + off) = v;
        __asm__ volatile ("dsb sy" ::: "memory");
    }
    void
    
    findReserved(IOService *provider);
    bool
    reserved(UInt32 pin) const
    {
        return (fReserved[pin / 32] >> (pin % 32)) & 1;
    }
    bool foreignEnabled(UInt32 pin, UInt32 cfg) const;
    UInt32 maskForeignPins(bool mask);
    bool maskLatchedForeign(void);
    void findConsumers(void);
    void configurePin(UInt32 pin);
    void logPin(UInt32 pin) const;
    bool attachToParent(IOService *provider);
    static void watchThread(void *arg, wait_result_t wr);
    void watch(void);

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;

    IOReturn    handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
    bool        vectorCanBeShared(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
    void        initVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
    int         getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
    void        disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
    void        enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
};

#define super IOInterruptController
OSDefineMetaClassAndStructors(PDQcomTLMM, IOInterruptController);

// every devicetree node whose interrupt parent is this controller
void
PDQcomTLMM::findConsumers(void)
{
    IORegistryIterator *iter = IORegistryIterator::iterateOver(gIODTPlane, kIORegistryIterateRecursively);
    IORegistryEntry *entry;

    while (iter != NULL && (entry = iter->getNextObject()) != NULL) {
        OSArray *controllers = OSDynamicCast(OSArray, entry->getProperty("IOInterruptControllers"));
        OSArray *specifiers = OSDynamicCast(OSArray, entry->getProperty("IOInterruptSpecifiers"));

        for (unsigned int i = 0; controllers != NULL && specifiers != NULL && i < controllers->getCount(); i++) {
            OSData *spec = OSDynamicCast(OSData, specifiers->getObject(i));
            if (!fName->isEqualTo(controllers->getObject(i)) || spec == NULL || spec->getLength() < 8) {
                continue;
            }
            const UInt32 *cells = (const UInt32 *)spec->getBytesNoCopy();
            if (cells[0] < fPins && reserved(cells[0])) {
                IOLog("PDQcomTLMM: pin %u (%s's interrupt) is reserved, refused\n", cells[0], entry->getName());
            } else if (cells[0] < fPins) {
                fFlags[cells[0]] = (UInt8)cells[1];
                IOLog("PDQcomTLMM: pin %u is %s's interrupt, type %u\n", cells[0], entry->getName(), cells[1]);
            }
        }
    }
    OSSafeReleaseNULL(iter);
}

// gpio-reserved-ranges: pairs of pins we must not touch
void
PDQcomTLMM::findReserved(IOService *provider)
{
    OSData *ranges = OSDynamicCast(OSData, provider->getProperty("gpio-reserved-ranges"));
    const UInt32 *cells = ranges != NULL ? (const UInt32 *)ranges->getBytesNoCopy() : NULL;
    unsigned int pairs = ranges != NULL ? ranges->getLength() / 8 : 0;

    for (unsigned int i = 0; i < pairs; i++) {
        for (UInt32 pin = cells[2 * i]; pin < cells[2 * i] + cells[2 * i + 1] && pin < fPins; pin++) {
            fReserved[pin / 32] |= 1u << (pin % 32);
        }
    }
    IOLog("PDQcomTLMM: %u reserved range(s)\n", pairs);
}

bool
PDQcomTLMM::foreignEnabled(UInt32 pin, UInt32 cfg) const
{
    return !fFlags[pin] && (cfg & kTlmmIrqEnable) &&
            ((cfg >> kTlmmIrqTargetShift) & 7) == kTlmmIrqTargetApps;
}

UInt32
PDQcomTLMM::maskForeignPins(bool mask)
{
    UInt32 found = 0;

    for (UInt32 pin = 0; pin < fPins; pin++) {
        if (reserved(pin)) {
            continue;
        }
        UInt32 cfg = read32(tlmmPin(pin, kTlmmPinIrqConfig));
        if (!foreignEnabled(pin, cfg)) {
            continue;
        }
        if (++found <= 16) {
            IOLog("PDQcomTLMM: pin %u left enabled by firmware, irq config 0x%08x status 0x%08x%s\n", pin,
                  cfg, read32(tlmmPin(pin, kTlmmPinIrqStatus)), mask ? ", masked" : "");
        }
        if (mask) {
            write32(tlmmPin(pin, kTlmmPinIrqConfig), cfg & ~(UInt32)kTlmmIrqEnable);
            write32(tlmmPin(pin, kTlmmPinIrqStatus), 0);
        }
    }
    IOLog("PDQcomTLMM: %u pin(s) left enabled by firmware%s\n", found, found && mask ? ", masked" : "");
    return found;
}

bool
PDQcomTLMM::maskLatchedForeign(void)
{
    bool found = false;

    for (UInt32 pin = 0; pin < fPins; pin++) {
        if (reserved(pin)) {
            continue;
        }
        UInt32 cfg = read32(tlmmPin(pin, kTlmmPinIrqConfig));
        if (foreignEnabled(pin, cfg) && (read32(tlmmPin(pin, kTlmmPinIrqStatus)) & 1)) {
            write32(tlmmPin(pin, kTlmmPinIrqConfig), cfg & ~(UInt32)kTlmmIrqEnable);
            write32(tlmmPin(pin, kTlmmPinIrqStatus), 0);
            found = true;
        }
    }
    return found;
}

void
PDQcomTLMM::logPin(UInt32 pin) const
{
    IOLog("PDQcomTLMM: pin %u control 0x%08x in/out 0x%08x irq config 0x%08x status 0x%08x\n", pin,
          read32(tlmmPin(pin, kTlmmPinControl)), read32(tlmmPin(pin, kTlmmPinInOut)),
          read32(tlmmPin(pin, kTlmmPinIrqConfig)), read32(tlmmPin(pin, kTlmmPinIrqStatus)));
}

void
PDQcomTLMM::configurePin(UInt32 pin)
{
    UInt32 flags = fFlags[pin];
    UInt32 cfg = read32(tlmmPin(pin, kTlmmPinIrqConfig));

    cfg &= ~(kTlmmIrqEnable | kTlmmIrqActiveHigh | (3u << kTlmmIrqTriggerShift) | (7u << kTlmmIrqTargetShift));
    cfg |= kTlmmIrqRawStatus | (kTlmmIrqTargetApps << kTlmmIrqTargetShift);
    if (flags & DT_IRQ_LEVEL_HIGH) {
        cfg |= kTlmmIrqActiveHigh;
    } else if (!(flags & DT_IRQ_LEVEL_LOW)) {
        UInt32 trigger = (flags & DT_IRQ_EDGE_RISING) && (flags & DT_IRQ_EDGE_FALLING) ? kTlmmTriggerBoth :
            (flags & DT_IRQ_EDGE_RISING) ? kTlmmTriggerRising : kTlmmTriggerFalling;
        cfg |= (trigger << kTlmmIrqTriggerShift) | kTlmmIrqActiveHigh;
    }
    write32(tlmmPin(pin, kTlmmPinIrqConfig), cfg);
    write32(tlmmPin(pin, kTlmmPinIrqStatus), 0);
}

void
PDQcomTLMM::watchThread(void *arg, wait_result_t wr)
{
    (void)wr;
    ((PDQcomTLMM *)arg)->watch();
    thread_terminate(current_thread());
}

// pdtlmm=1: report every change of level on the consumers' pins
// pdtlmm=2: report and clear every latch
void
PDQcomTLMM::watch(void)
{
    UInt32 last[TLMM_MAX_PINS / 32];
    UInt32 events = 0;

    bzero(last, sizeof(last));
    for (UInt32 pin = 0; pin < fPins; pin++) {
        if (fFlags[pin] && (read32(tlmmPin(pin, kTlmmPinInOut)) & 1)) {
            last[pin / 32] |= 1u << (pin % 32);
        }
    }
    for (UInt32 tick = 0; tick < TLMM_WATCH_SECONDS * 100; tick++) {
        for (UInt32 pin = 0; pin < fPins; pin++) {
            if (!fFlags[pin]) {
                continue;
            }
            UInt32 level = read32(tlmmPin(pin, kTlmmPinInOut)) & 1;
            UInt32 was = (last[pin / 32] >> (pin % 32)) & 1;
            if (fMode == 1 && level != was && ++events <= 40) {
                IOLog("PDQcomTLMM: pin %u %s\n", pin, level ? "high" : "low");
            }
            last[pin / 32] = (last[pin / 32] & ~(1u << (pin % 32))) | (level << (pin % 32));
            if (fMode == 2 && (read32(tlmmPin(pin, kTlmmPinIrqStatus)) & 1)) {
                if (++events <= 40) {
                    IOLog("PDQcomTLMM: pin %u latched (level now %s)\n", pin, level ? "high" : "low");
                }
                write32(tlmmPin(pin, kTlmmPinIrqStatus), 0);
            }
        }
        IOSleep(10);
    }
    IOLog("PDQcomTLMM: watch over, %u event(s)\n", events);
}

// the summary interrupt, through our own nub's specifier, from the parent controller (the GIC)
bool
PDQcomTLMM::attachToParent(IOService *provider)
{
    OSArray *names = OSDynamicCast(OSArray, provider->getProperty("IOInterruptControllers"));
    OSString *parent = names != NULL ? OSDynamicCast(OSString, names->getObject(0)) : NULL;
    OSDictionary *match;
    IOService *found = NULL;

    if (parent == NULL) {
        IOLog("PDQcomTLMM: no summary interrupt in the device tree\n");
        return false;
    }
    // registering blocks until the parent exists; wait for it as a resource with a deadline
    match = resourceMatching(parent);
    if (match != NULL) {
        found = waitForMatchingService(match, CONTROLLER_WAIT_NS);
        match->release();
    }
    if (found == NULL) {
        IOLog("PDQcomTLMM: parent controller %s never appeared\n", parent->getCStringNoCopy());
        return false;
    }
    found->release();

    IOInterruptAction handler = OSMemberFunctionCast(IOInterruptAction, this, &PDQcomTLMM::handleInterrupt);
    if (provider->registerInterrupt(0, this, handler, NULL) != kIOReturnSuccess) {
        IOLog("PDQcomTLMM: could not register the summary interrupt with %s\n", parent->getCStringNoCopy());
        return false;
    }
    provider->enableInterrupt(0);
    return true;
}

bool
PDQcomTLMM::start(IOService *provider)
{
    OSData *ngpios;

    if (!super::start(provider)) {
        return false;
    }
    fMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
    if (fMap == NULL) {
        IOLog("PDQcomTLMM: %s has no register window\n", provider->getName());
        return false;
    }
    fRegs = (volatile UInt8 *)fMap->getVirtualAddress();
    ngpios = OSDynamicCast(OSData, provider->getProperty("ngpios"));
    fPins = (ngpios != NULL && ngpios->getLength() >= 4) ? *(const UInt32 *)ngpios->getBytesNoCopy() :
        (UInt32)(fMap->getLength() / kTlmmPinStride);
    if (fPins > TLMM_MAX_PINS) {
        fPins = TLMM_MAX_PINS;
    }
    fProvider = provider;
    fLock = IOSimpleLockAlloc();
    fName = IODTInterruptControllerName(provider);
    if (fLock == NULL || fName == NULL) {
        IOLog("PDQcomTLMM: no AAPL,phandle on %s\n", provider->getName());
        return false;
    }

    findReserved(provider);
    findConsumers();
    for (UInt32 pin = 0; pin < fPins; pin++) {
        if (fFlags[pin]) {
            logPin(pin);
        }
    }

    UInt32 noscan = 0;
    fMode = 0;
    PE_parse_boot_argn("pdtlmm", &fMode, sizeof(fMode));
    PE_parse_boot_argn("pdtlmm_noscan", &noscan, sizeof(noscan));
    if (!noscan) {
        maskForeignPins(fMode != 1);
    }
    if (fMode == 1 || fMode == 2) {
        thread_t th;
        if (fMode == 2) {
            for (UInt32 pin = 0; pin < fPins; pin++) {
                if (fFlags[pin]) {
                    configurePin(pin);
                    write32(tlmmPin(pin, kTlmmPinIrqConfig), read32(tlmmPin(pin, kTlmmPinIrqConfig)) | kTlmmIrqEnable);
                    logPin(pin);
                }
            }
        }
        IOLog("PDQcomTLMM: %s mode for %u s, not an interrupt controller\n",
              fMode == 1 ? "observe" : "latch", TLMM_WATCH_SECONDS);
        if(kernel_thread_start(&PDQcomTLMM::watchThread, this, &th) == KERN_SUCCESS) {
            thread_deallocate(th);
        }
        return true;
    }

    vectors = (IOInterruptVector *)IOMalloc(fPins * sizeof(IOInterruptVector));
    if (vectors == NULL) {
        return false;
    }
    bzero(vectors, fPins * sizeof(IOInterruptVector));
    for (UInt32 i = 0; i < fPins; i++) {
        vectors[i].interruptLock = IOLockAlloc();
        if (vectors[i].interruptLock == NULL) {
            return false;
        }
    }
    if (!attachToParent(provider)) {
        return false;
    }
    getPlatform()->registerInterruptController((OSSymbol *)fName, this);
    publishResource(fName, this);
    registerService();
    IOLog("PDQcomTLMM: %s, %u pins\n", fName->getCStringNoCopy(), fPins);
    return true;
}

// the summary fired: dispatch every enabled pin that has latched
IOReturn
PDQcomTLMM::handleInterrupt(void * /*refCon*/, IOService * /*nub*/, int /*source*/)
{
    bool claimed = false;

    for (UInt32 word = 0; word < (fPins + 31) / 32; word++) {
        UInt32 bits = fEnabled[word];

        while (bits != 0) {
            UInt32 pin = word * 32 + (UInt32)__builtin_ctz(bits);
            bits &= bits - 1;
            if (!(read32(tlmmPin(pin, kTlmmPinIrqStatus)) & 1)) {
                continue;
            }
            write32(tlmmPin(pin, kTlmmPinIrqStatus), 0);
            claimed = true;

            IOInterruptVector * vector = &vectors[pin];
            vector->interruptActive = 1;
            if (!vector->interruptDisabledSoft && vector->interruptRegistered) {
                vector->handler(vector->target, vector->refCon, vector->nub, vector->source);
            } else {
                vector->interruptDisabledHard = 1;
                disableVectorHard((IOInterruptVectorNumber)pin, vector);
            }
            vector->interruptActive = 0;
        }
    }

    if (claimed || maskLatchedForeign()) {
        fUnclaimed = 0;
    } else if (++fUnclaimed == TLMM_UNCLAIMED_LIMIT) {
        fProvider->disableInterrupt(0);
    }
    return kIOReturnSuccess;
}

bool
PDQcomTLMM::vectorCanBeShared(IOInterruptVectorNumber /*vectorNumber*/, IOInterruptVector * /*vector*/)
{
    return false;
}

void
PDQcomTLMM::initVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector * /*vector*/)
{
    UInt32 pin = (UInt32)vectorNumber;

    if (pin < fPins) {
        IOInterruptState is = IOSimpleLockLockDisableInterrupt(fLock);
        configurePin(pin);
        IOSimpleLockUnlockEnableInterrupt(fLock, is);
    }
}

int
PDQcomTLMM::getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector * /*vector*/)
{
    UInt32 pin = (UInt32)vectorNumber;

    return (pin < fPins && !(fFlags[pin] & (DT_IRQ_LEVEL_HIGH | DT_IRQ_LEVEL_LOW))) ?
        kIOInterruptTypeEdge : kIOInterruptTypeLevel;
}

void
PDQcomTLMM::disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector * /*vector*/)
{
    UInt32 pin = (UInt32)vectorNumber;

    if (pin >= fPins) {
        return;
    }
    IOInterruptState is = IOSimpleLockLockDisableInterrupt(fLock);
    UInt32 off = tlmmPin(pin, kTlmmPinIrqConfig);
    write32(off, read32(off) & ~(UInt32)kTlmmIrqEnable);
    fEnabled[pin / 32] &= ~(1u << (pin %32));
    IOSimpleLockUnlockEnableInterrupt(fLock, is);
}

void
PDQcomTLMM::enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector * /*vector*/)
{
    UInt32 pin = (UInt32)vectorNumber;

    if (pin >= fPins) {
        return;
    }
    IOInterruptState is = IOSimpleLockLockDisableInterrupt(fLock);
    UInt32 off = tlmmPin(pin, kTlmmPinIrqConfig);
    fEnabled[pin / 32] |= 1u << (pin % 32);
    write32(off, read32(off) | kTlmmIrqEnable);
    IOSimpleLockUnlockEnableInterrupt(fLock, is);
}
