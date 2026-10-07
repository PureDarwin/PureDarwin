// I2C on a Qualcomm GENI serial engine
#include "PDQcomGeniI2C.h"
#include "geni_regs.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

#define CONTROLLER_WAIT_NS      (5ULL * 1000 * 1000 * 1000)
#define MISSED_IRQ_SLICE_US     1000

static const UInt32 kEventsWanted = kGeniEventFinished | kGeniEventI2CErrors | kGeniEventRxWatermark |
    kGeniEventRxLast;

OSDefineMetaClassAndStructors(PDQcomGeniI2C, PDI2CController);

static inline UInt32
rd(volatile UInt8 *base, UInt32 off)
{
    return *(volatile UInt32 *)(base + off);
}

static inline void
wr(volatile UInt8 *base, UInt32 off, UInt32 v)
{
    *(volatile UInt32 *)(base + off) = v;
}

static inline void
barrier(void)
{
    __asm__ volatile ("dsb sy" ::: "memory");
}

bool
PDQcomGeniI2C::start(IOService *provider)
{
    UInt32 off = 0;

    if (PE_parse_boot_argn("pdgeni_disable", &off, sizeof(off)) && off) {
        IOLog("PDQcomGeniI2C: disabled by pdgeni_disable\n");
        return false;
    }
    if (!PDI2CController::start(provider)) {
        return false;
    }
    fMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
    if (fMap == NULL) {
        IOLog("PDQcomGeniI2C: %s has no register window\n", provider->getName());
        return false;
    }
    fRegs = (volatile UInt8 *)fMap->getVirtualAddress();

    // the first read. if its clock were off this would stall the core (x13s observed)
    // firmware leaves the keyboard engine's clocks on, so it is not checked
    UInt32 fw = rd(fRegs, kGeniFirmwareRevision);
    UInt32 proto = geniProtocol(fw);
    if (proto != kGeniProtocolI2C) {
        IOLog("PDQcomGeniI2C: the engine is not running I2C, leaving it alone\n");
        return false;
    }

    if (!setupInterrupt(provider)) {
        teardownInterrupt();
        IOLog("PDQcomGeniI2C: polled\n");
    }

    registerService();
    IOLog("PDQcomGeniI2C: %u device(s) on the bus\n", publishDevices(provider));
    return true;
}

void
PDQcomGeniI2C::free(void)
{
    teardownInterrupt();
    OSSafeReleaseNULL(fMap);
    PDI2CController::free();
}

bool
PDQcomGeniI2C::setupInterrupt(IOService *provider)
{
    OSArray *names = OSDynamicCast(OSArray, provider->getProperty("IOInterruptControllers"));
    OSString *parent = names != NULL ? OSDynamicCast(OSString, names->getObject(0)) : NULL;
    OSDictionary *match;
    IOService *found = NULL;
    UInt32 poll = 0;
    int type = 0;

    if (PE_parse_boot_argn("pdgeni_poll", &poll, sizeof(poll)) && poll) {
        IOLog("PDQcomGeniI2C: pdgeni_poll set\n");
        return false;
    }
    if (parent == NULL) {
        IOLog("PDQcomGeniI2C: no interrupt in the device tree\n");
        return false;
    }
    match = resourceMatching(parent);
    if (match != NULL) {
        found = waitForMatchingService(match, CONTROLLER_WAIT_NS);
        match->release();
    }
    if (found == NULL) {
        IOLog("PDQcomGeniI2C: %s did not resolve the interrupt\n", parent->getCStringNoCopy());
        return false;
    }

    fEngineLoop = IOWorkLoop::workLoop();
    fGate = IOCommandGate::commandGate(this);
    fInterrupt = IOInterruptEventSource::interruptEventSource(this,
        OSMemberFunctionCast(IOInterruptEventAction, this, &PDQcomGeniI2C::interruptOccurred), provider, 0);
    if (fEngineLoop == NULL || fGate == NULL || fInterrupt == NULL ||
        fEngineLoop->addEventSource(fGate) != kIOReturnSuccess ||
        fEngineLoop->addEventSource(fInterrupt) != kIOReturnSuccess) {
        IOLog("PDQcomGeniI2C: could not set up the interrupt\n");
        return false;
    }

    UInt32 irqEnable = rd(fRegs, kGeniEngineIrqEnable);
    UInt32 depth = geniRxFifoDepth(rd(fRegs, kGeniHardwareParams1));
    wr(fRegs, kGeniEventEnable, 0);
    wr(fRegs, kGeniEventClear, 0xffffffffu);
    wr(fRegs, kGeniEngineIrqEnable, irqEnable | kGeniEngineIrqMain);
    wr(fRegs, kGeniRxWatermark, depth >= 2 ? depth / 2 : 1);
    barrier();

    fInterrupt->enable();
    return true;
}

void
PDQcomGeniI2C::teardownInterrupt(void)
{
    if (fInterrupt != NULL) {
        fInterrupt->disable();
        if (fEngineLoop != NULL) {
            fEngineLoop->removeEventSource(fInterrupt);
        }
    }
    if (fGate != NULL && fEngineLoop != NULL) {
        fEngineLoop->removeEventSource(fGate);
    }
    OSSafeReleaseNULL(fInterrupt);
    OSSafeReleaseNULL(fGate);
    OSSafeReleaseNULL(fEngineLoop);
}

void
PDQcomGeniI2C::interruptOccurred(IOInterruptEventSource * /*sender*/, int /*count*/)
{
    UInt32 st = rd(fRegs, kGeniEventStatus);

    drainRx();
    wr(fRegs, kGeniEventClear, st);
    barrier();
    if (st & (kGeniEventFinished | kGeniEventI2CErrors)) {
        drainRx();
        wr(fRegs, kGeniEventEnable, 0);
        barrier();
        fEvents |= st;
        fGate->commandWakeup(&fEvents);
    }
}

void
PDQcomGeniI2C::abortCommand(void)
{
    wr(fRegs, kGeniEventEnable, 0);
    wr(fRegs, kGeniCommandControl, kGeniAbort);
    barrier();
    for (UInt32 i = 0; i < 1000 && !(rd(fRegs, kGeniEventStatus) & kGeniEventAborted); i++) {
        IODelay(1);
    }
    wr(fRegs, kGeniEventClear, 0xffffffffu);
    barrier();
}

void
PDQcomGeniI2C::armEvents(void)
{
    fEvents = 0;
    wr(fRegs, kGeniEventClear, 0xffffffffu);
    wr(fRegs, kGeniEventEnable, fGate != NULL ? kEventsWanted : 0);
    barrier();
}

void
PDQcomGeniI2C::drainRx(void)
{
    if (fRx == NULL) {
        return;
    }
    UInt32 fs = rd(fRegs, kGeniRxFifoStatus);
    UInt32 words = geniRxWords(fs);
    for (UInt32 i = 0; i < words; i++) {
        UInt32 w = rd(fRegs, kGeniRxFifo);
        UInt32 valid = (i == words - 1 && (fs & kGeniRxLastWord)) ? geniRxLastBytes(fs) : 4;
        for (UInt32 b = 0; b < valid && fRxGot < fRxLen; b++, fRxGot++) {
            fRx[fRxGot] = (UInt8)(w >> (8 * b));
        }
    }
}

UInt32
PDQcomGeniI2C::waitEvents(UInt32 timeoutUs)
{
    return fGate != NULL ? sleepEvents(timeoutUs) : pollEvents(timeoutUs);
}

UInt32
PDQcomGeniI2C::pollEvents(UInt32 timeoutUs)
{
    UInt32 st = 0;

    for (UInt32 waited = 0; waited < timeoutUs; waited++) {
        drainRx();
        st = rd(fRegs, kGeniEventStatus);
        if (st & (kGeniEventFinished | kGeniEventI2CErrors)) {
            break;
        }
        IODelay(1);
    }
    drainRx();
    return st;
}

UInt32
PDQcomGeniI2C::sleepEvents(UInt32 timeoutUs)
{
    uint64_t deadline, slice;
    UInt32 st = 0;

    clock_interval_to_deadline(timeoutUs, kMicrosecondScale, &deadline);
    while (!(fEvents & (kGeniEventFinished | kGeniEventI2CErrors))) {
        clock_interval_to_deadline(MISSED_IRQ_SLICE_US, kMicrosecondScale, &slice);
        if (slice > deadline) {
            slice = deadline;
        }
        // sleep in slices so rx FIFO doesn't fill up before we drain
        if (fGate->commandSleep(&fEvents, slice, THREAD_UNINT) != THREAD_TIMED_OUT) {
            continue;
        }
        st = rd(fRegs, kGeniEventStatus);
        drainRx();
        if (st & (kGeniEventFinished | kGeniEventI2CErrors)) {
            if (fMissed++ < 8) {
                IOLog("PDQcomGeniI2C: events 0x%08x latched, no interrupt\n", st);
            }
            return fEvents | st;
        }
        if (mach_absolute_time() >= deadline) {
            return fEvents | st;
        }
    }
    return fEvents;
}

IOReturn
PDQcomGeniI2C::transferLocked(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx, UInt32 rxLen,
                              UInt32 timeoutUs, IOOptionBits options)
{
    const Transfer t = { addr, tx, txLen, rx, rxLen, timeoutUs, options };

    if (fRegs == NULL || txLen > 32) {
        return fRegs == NULL ? kIOReturnNotReady : kIOReturnUnsupported;
    }
    if (fGate != NULL) {
        return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &PDQcomGeniI2C::gatedTransfer),
                                (void *)&t);
    }
    return runTransfer(&t);
}

IOReturn
PDQcomGeniI2C::gatedTransfer(void *arg0, void * /*arg1*/, void * /*arg2*/, void * /*arg3*/)
{
    return runTransfer((const Transfer *)arg0);
}

IOReturn
PDQcomGeniI2C::runTransfer(const Transfer *t)
{
    const UInt32 target = geniI2CTarget(t->addr);
    const bool quiet = (t->options & kPDI2CTransferQuiet) != 0;
    UInt32 st;

    wr(fRegs, kGeniDmaEnable, 0);
    wr(fRegs, kGeniTxWatermark, 0);
    barrier();
    
    if (t->txLen > 0) {
        // holding the bus makes the read below start with a repeated START
        armEvents();
        wr(fRegs, kGeniI2CWriteLength, t->txLen);
        barrier();
        wr(fRegs, kGeniCommand, geniCommand(kGeniI2CWrite, target | (t->rxLen > 0 ? kGeniI2CHoldBus : 0)));
        barrier();
        for (UInt32 off = 0; off < t->txLen;) {
            UInt32 w = 0;
            for (UInt32 b = 0; b < 4 && off < t->txLen; b++, off++) {
                w |= (UInt32)t->tx[off] << (8 * b);
            }
            wr(fRegs, kGeniTxFifo, w);
        }
        barrier();
        st = waitEvents(t->timeoutUs);
        if (!(st & kGeniEventDone) || (st & kGeniEventI2CErrors)) {
            if (!quiet) {
                IOLog("PDQcomGeniI2C: write to 0x%02x failed, status=0x%08x%s\n", t->addr, st,
                      (st & kGeniEventI2CNack) ? " (NACK)" : "");
            }
            abortCommand();
            return (st & kGeniEventI2CNack) ? kIOReturnNotResponding : kIOReturnTimeout;
        }
    }

    if (t->rxLen > 0) {
        fRx = t->rx;
        fRxLen = t->rxLen;
        fRxGot = 0;
        armEvents();
        wr(fRegs, kGeniI2CReadLength, t->rxLen);
        barrier();
        wr(fRegs, kGeniCommand, geniCommand(kGeniI2CRead, target));
        barrier();
        st = waitEvents(t->timeoutUs);
        UInt32 got = fRxGot;
        fRx = NULL;
        if (got < t->rxLen || (st & kGeniEventI2CErrors)) {
            if (!quiet) {
                IOLog("PDQcomGeniI2C: read from 0x%02x short, %u of %u, status=0x%08x%s\n", t->addr,
                      got, t->rxLen, st, (st & kGeniEventI2CNack) ? " (NACK)" : "");
            }
            abortCommand();
            return (st & kGeniEventI2CNack) ? kIOReturnNotResponding : kIOReturnUnderrun;
        }
    }

    wr(fRegs, kGeniEventEnable, 0);
    wr(fRegs, kGeniEventClear, 0xffffffffu);
    barrier();
    return kIOReturnSuccess;
}
