// I2C on a Qualcomm GENI serial engine
#include "PDQcomGeniI2C.h"
#include "geni_regs.h"

#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>

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

    registerService();
    IOLog("PDQcomGeniI2C: %u device(s) on the bus\n", publishDevices(provider));
    return true;
}

void
PDQcomGeniI2C::free(void)
{
    OSSafeReleaseNULL(fMap);
    PDI2CController::free();
}

void
PDQcomGeniI2C::abortCommand(void)
{
    wr(fRegs, kGeniCommandControl, kGeniAbort);
    barrier();
    for (UInt32 i = 0; i < 1000 && !(rd(fRegs, kGeniEventStatus) & kGeniEventAborted); i++) {
        IODelay(1);
    }
    wr(fRegs, kGeniEventClear, 0xffffffffu);
    barrier();
}

UInt32
PDQcomGeniI2C::waitTerminal(UInt32 timeoutUs)
{
    UInt32 st = 0;

    for (UInt32 waited = 0; waited < timeoutUs; waited++) {
        st = rd(fRegs, kGeniEventStatus);
        if (st & (kGeniEventFinished | kGeniEventI2CErrors)) {
            break;
        }
        IODelay(1);
    }
    return st;
}

IOReturn
PDQcomGeniI2C::transferLocked(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx, UInt32 rxLen,
                              UInt32 timeoutUs, IOOptionBits options)
{
    const UInt32 target = geniI2CTarget(addr);
    const bool quiet = (options & kPDI2CTransferQuiet) != 0;
    UInt32 st;

    // the write must fit the TX FIFO (16 words)
    if (fRegs == NULL || txLen > 32) {
        return fRegs == NULL ? kIOReturnNotReady : kIOReturnUnsupported;
    }

    // polled: the engine's interrupt stays masked, its event status latches either way
    wr(fRegs, kGeniEventEnable, 0);
    wr(fRegs, kGeniEventClear, 0xffffffffu);
    wr(fRegs, kGeniDmaEnable, 0);
    wr(fRegs, kGeniTxWatermark, 0);
    barrier();

    if (txLen > 0) {
        // holding the bus makes the read below start with a repeated START
        wr(fRegs, kGeniI2CWriteLength, txLen);
        barrier();
        wr(fRegs, kGeniCommand, geniCommand(kGeniI2CWrite, target | (rxLen > 0 ? kGeniI2CHoldBus : 0)));
        barrier();
        for (UInt32 off = 0; off < txLen;) {
            UInt32 w = 0;
            for (UInt32 b = 0; b < 4 && off < txLen; b++, off++) {
                w |= (UInt32)tx[off] << (8 * b);
            }
            wr(fRegs, kGeniTxFifo, w);
        }
        barrier();
        st = waitTerminal(timeoutUs);
        if (!(st & kGeniEventDone) || (st & kGeniEventI2CErrors)) {
            if (!quiet) {
                IOLog("PDQcomGeniI2C: write to 0x%02x failed, status=0x%08x%s\n", addr, st,
                      (st & kGeniEventI2CNack) ? " (NACK)" : "");
            }
            abortCommand();
            return (st & kGeniEventI2CNack) ? kIOReturnNotResponding : kIOReturnTimeout;
        }
        wr(fRegs, kGeniEventClear, 0xffffffffu);
        barrier();
    }

    if (rxLen > 0) {
        UInt32 got = 0;

        wr(fRegs, kGeniI2CReadLength, rxLen);
        barrier();
        wr(fRegs, kGeniCommand, geniCommand(kGeniI2CRead, target));
        barrier();
        st = 0;
        for (UInt32 waited = 0; waited < timeoutUs;) {
            UInt32 fs = rd(fRegs, kGeniRxFifoStatus);
            UInt32 words = geniRxWords(fs);

            if (words > 0) {
                // the last word of the read may be only partly data
                for (UInt32 i = 0; i < words; i++) {
                    UInt32 w = rd(fRegs, kGeniRxFifo);
                    UInt32 valid = (i == words - 1 && (fs & kGeniRxLastWord)) ? geniRxLastBytes(fs) : 4;
                    for (UInt32 b = 0; b < valid && got < rxLen; b++, got++) {
                        rx[got] = (UInt8)(w >> (8 * b));
                    }
                }
                continue;
            }
            st = rd(fRegs, kGeniEventStatus);
            if ((st & kGeniEventI2CErrors) || ((st & kGeniEventDone) && got >= rxLen) ||
                ((st & kGeniEventFinished) && !(st & kGeniEventDone))) {
                break;
            }
            IODelay(1);
            waited++;
        }
        if (got < rxLen) {
            if (!quiet) {
                IOLog("PDQcomGeniI2C: read from 0x%02x short, %u of %u, status=0x%08x%s\n", addr,
                      got, rxLen, st, (st & kGeniEventI2CNack) ? " (NACK)" : "");
            }
            abortCommand();
            return (st & kGeniEventI2CNack) ? kIOReturnNotResponding : kIOReturnUnderrun;
        }
        wr(fRegs, kGeniEventClear, 0xffffffffu);
        barrier();
    }
    return kIOReturnSuccess;
}
