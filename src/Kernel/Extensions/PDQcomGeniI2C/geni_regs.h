// Qualcomm GENI serial engine
#ifndef PD_GENI_REGS_H
#define PD_GENI_REGS_H

#include <libkern/OSTypes.h>

// register offsets from the engine's base
enum {
    kGeniFirmwareRevision   = 0x068,    // which protocol image firmware loaded
    kGeniDmaEnable          = 0x258,    // 0: FIFO mode
    kGeniI2CWriteLength     = 0x26c,    // bytes in the next write command
    kGeniI2CReadLength      = 0x270,    // bytes in the next read command
    kGeniI2CClockCounters   = 0x278,    // bus timing, as firmware programmed it
    kGeniCommand            = 0x600,    // starts a command: opcode + parameters
    kGeniCommandControl     = 0x604,    // aborts a command
    kGeniEventStatus        = 0x610,    // latched command events and bus errors
    kGeniEventEnable        = 0x614,    // which of them raise the engine's interrupt
    kGeniEventClear         = 0x618,    // write 1 to clear
    kGeniTxFifo             = 0x700,    // one 32bit word per write
    kGeniRxFifo             = 0x780,    // one 32bit word per read
    kGeniRxFifoStatus       = 0x804,    // words waiting and the last words byte count
    kGeniTxWatermark        = 0x80c,    // TX refill threshold
    kGeniHardwareParams0    = 0xe24,    // FIFO depth and width (TX)
    kGeniHardwareParams1    = 0xe28,    // FIFO depth and width (RX)
};

// kGeniFirmwareRevision
enum { kGeniProtocolI2C = 3 };

static inline UInt32
geniProtocol(UInt32 revision)
{
    return (revision >> 8) & 0xff;
}

// kGeniCommand
enum {
    kGeniI2CWrite           = 1,
    kGeniI2CRead            = 2,
};

static inline UInt32
geniCommand(UInt32 opcode, UInt32 parameters)
{
    return (opcode << 27) | (parameters & 0x07ffffffu);
}

static inline UInt32
geniI2CTarget(UInt8 addr)
{
    return ((UInt32)addr & 0x7f) << 9;
}

enum { kGeniI2CHoldBus = 1u << 2 };

// kGeniCommandControl
enum { kGeniAbort = 1u << 1 };

// kGeniEventStatus, kGeniEventEnable, kGeniEventClear
enum {
    kGeniEventDone              = 1u << 0,
    kGeniEventOverrun           = 1u << 1,
    kGeniEventIllegal           = 1u << 2,
    kGeniEventFailed            = 1u << 3,
    kGeniEventCancelled         = 1u << 4,
    kGeniEventAborted           = 1u << 5,
    kGeniEventI2CNack           = 1u << 10,
    kGeniEventI2CBusError       = 1u << 12,
    kGeniEventI2CArbitration    = 1u << 13,
    kGeniEventFinished          = kGeniEventDone | kGeniEventOverrun | kGeniEventIllegal | kGeniEventFailed | kGeniEventCancelled | kGeniEventAborted,
    kGeniEventI2CErrors         = kGeniEventI2CNack | kGeniEventI2CBusError | kGeniEventI2CArbitration,
};

enum { kGeniRxLastWord = 1u << 31 };

static inline UInt32
geniRxWords(UInt32 status)
{
    return status & 0xffffffu;
}

static inline UInt32
geniRxLastBytes(UInt32 status)
{
    UInt32 n = (status >> 28) & 0x7;
    return n != 0 ? n : 4;
}

#endif
