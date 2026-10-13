/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "PDDesignWareI2C.h"

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>

OSDefineMetaClassAndStructors(PDDesignWareI2C, PDI2CController);

// register offsets
enum {
    kCon            = 0x00,
    kTar            = 0x04,
    kDataCmd        = 0x10,
    kSsHcnt         = 0x14,
    kSsLcnt         = 0x18,
    kFsHcnt         = 0x1c,
    kFsLcnt         = 0x20,
    kIntrMask       = 0x30,
    kRawIntrStat    = 0x34,
    kRxTl           = 0x38,
    kTxTl           = 0x3c,
    kClrIntr        = 0x40,
    kEnable         = 0x6c,
    kStatus         = 0x70,
    kTxFlr          = 0x74,
    kRxFlr          = 0x78,
    kSdaHold        = 0x7c,
    kTxAbrtSource   = 0x80,
    kEnableStatus   = 0x9c,
    kCompParam1     = 0xf4,
    kCompType       = 0xfc,
};

// IC_CON
enum {
    kConMaster      = 1u << 0,
    kConSpeedStd    = 1u << 1,
    kConSpeedFast   = 2u << 1,
    kConRestartEn   = 1u << 5,
    kConSlaveDis    = 1u << 6,
};

// IC_DATA_CMD
enum {
    kCmdRead        = 1u << 8,
    kCmdStop        = 1u << 9,
    kCmdRestart     = 1u << 10,
};

// IC_RAW_INTR_STAT
enum {
    kIntrTxAbrt     = 1u << 6,
    kIntrStopDet    = 1u << 9,
};

// IC_STATUS
enum {
    kStatusActivity = 1u << 0,
    kStatusTfnf     = 1u << 1,
    kStatusMstAct   = 1u << 5,
};

#define kCompTypeValue  0x44570140      // "DW" and the component number

bool
PDDesignWareI2C::start(IOService *provider)
{
    if (!PDI2CController::start(provider)) {
        return false;
    }
    fMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
    if (fMap == NULL) {
        IOLog("PDDesignWareI2C: %s has no register window\n", provider->getName());
        return false;
    }
    fRegs = (volatile UInt8 *)fMap->getVirtualAddress();

    UInt32 type = rd(kCompType);
    if (type != kCompTypeValue) {
        IOLog("PDDesignWareI2C: %s is not a DesignWare I2C block (0x%08x)\n", provider->getName(), type);
        return false;
    }
    UInt32 param = rd(kCompParam1);
    fTxDepth = ((param >> 16) & 0xff) + 1;
    fRxDepth = ((param >> 8) & 0xff) + 1;
    // some blocks leave the parameter register unpopulated
    if (param == 0 || fTxDepth < 2 || fRxDepth < 2) {
        fTxDepth = fRxDepth = 16;
    }

    UInt32 hz = 400000;
    OSIterator *it = provider->getChildIterator(gIODTPlane);
    IORegistryEntry *child;
    while (it != NULL && (child = (IORegistryEntry *)it->getNextObject()) != NULL) {
        OSData *d = OSDynamicCast(OSData, child->getProperty("clock-frequency"));
        if (d != NULL && d->getLength() >= sizeof(UInt32) && *(const UInt32 *)d->getBytesNoCopy() < hz) {
            hz = *(const UInt32 *)d->getBytesNoCopy();
        }
    }
    OSSafeReleaseNULL(it);

    setEnabled(false);
    setupTiming(hz);
    wr(kIntrMask, 0);
    (void)rd(kClrIntr);
    IOLog("PDDesignWareI2C: %s fifo %u/%u, %u Hz, ss %u/%u fs %u/%u\n", provider->getName(), fTxDepth, fRxDepth,
          hz, rd(kSsHcnt), rd(kSsLcnt), rd(kFsHcnt), rd(kFsLcnt));

    registerService();
    IOLog("PDDesignWareI2C: %u device(s) on the bus\n", publishDevices(provider));
    return true;
}

void
PDDesignWareI2C::free(void)
{
    if (fRegs != NULL) {
        setEnabled(false);
    }
    OSSafeReleaseNULL(fMap);
    PDI2CController::free();
}

// enable and disable take effect once the block confirms them in IC_ENABLE_STATUS
bool
PDDesignWareI2C::setEnabled(bool on)
{
    for (int i = 0; i < 500; i++) {
        wr(kEnable, on ? 1 : 0);
        if ((rd(kEnableStatus) & 1) == (on ? 1u : 0u)) {
            return true;
        }
        IODelay(25);
    }
    return false;
}

// firmware has usually programmed the SCL counts for the board, those are kept. Otherwise
// counts for a 133 MHz input clock, what AMD's controllers run at
void
PDDesignWareI2C::setupTiming(UInt32 hz)
{
    UInt32 con = kConMaster | kConRestartEn | kConSlaveDis;

    if (hz > 100000) {
        con |= kConSpeedFast;
        if (rd(kFsHcnt) == 0 || rd(kFsLcnt) == 0) {
            wr(kFsHcnt, 0x72);
            wr(kFsLcnt, 0xd8);
        }
    } else {
        con |= kConSpeedStd;
        if (rd(kSsHcnt) == 0 || rd(kSsLcnt) == 0) {
            wr(kSsHcnt, 0x22c);
            wr(kSsLcnt, 0x28f);
        }
    }
    wr(kCon, con);
    wr(kRxTl, 0);
    wr(kTxTl, 0);
}

// a write of tx, then a read of rx after a repeated START, all queued through IC_DATA_CMD
IOReturn
PDDesignWareI2C::transferLocked(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx, UInt32 rxLen,
                                UInt32 timeoutUs, IOOptionBits options)
{
    UInt32 total = txLen + rxLen, queued = 0, got = 0, waited = 0;
    IOReturn ret = kIOReturnSuccess;

    if (total == 0) {
        return kIOReturnBadArgument;
    }
    if (!setEnabled(false)) {
        return kIOReturnBusy;
    }
    wr(kTar, addr);
    (void)rd(kClrIntr);
    if (!setEnabled(true)) {
        return kIOReturnBusy;
    }

    while (got < rxLen || queued < total) {
        UInt32 raw = rd(kRawIntrStat);

        if (raw & kIntrTxAbrt) {
            UInt32 why = rd(kTxAbrtSource);
            (void)rd(kClrIntr);
            // bits 0-3 are an address byte nobody acknowledged
            ret = (why & 0xf) ? kIOReturnNotResponding : kIOReturnIOError;
            if (!(options & kPDI2CTransferQuiet)) {
                IOLog("PDDesignWareI2C: 0x%02x aborted, source 0x%x\n", addr, why);
            }
            break;
        }
        // keep the rx fifo from overflowing: no more reads in flight than it holds
        while (queued < total && rd(kTxFlr) < fTxDepth && (queued < txLen || queued - txLen - got < fRxDepth)) {
            UInt32 cmd = queued < txLen ? tx[queued] : kCmdRead;
            if (queued == txLen && txLen != 0) {
                cmd |= kCmdRestart;
            }
            if (queued == total - 1) {
                cmd |= kCmdStop;
            }
            wr(kDataCmd, cmd);
            queued++;
        }
        while (got < rxLen && rd(kRxFlr) > 0) {
            rx[got++] = (UInt8)rd(kDataCmd);
        }
        if (got >= rxLen && queued >= total) {
            break;
        }
        if (waited >= timeoutUs) {
            ret = kIOReturnTimeout;
            break;
        }
        IODelay(10);
        waited += 10;
    }

    // the STOP has to go out before the next transfer retargets the block
    while (ret == kIOReturnSuccess && !(rd(kRawIntrStat) & kIntrStopDet) && waited < timeoutUs) {
        IODelay(10);
        waited += 10;
    }
    (void)rd(kClrIntr);
    if (ret == kIOReturnSuccess && waited >= timeoutUs) {
        ret = kIOReturnTimeout;
    }
    if (ret == kIOReturnTimeout && !(options & kPDI2CTransferQuiet)) {
        IOLog("PDDesignWareI2C: 0x%02x timed out, %u/%u queued, %u/%u read, status 0x%x\n", addr, queued, total,
              got, rxLen, rd(kStatus));
    }
    return ret;
}
