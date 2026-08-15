/*
 * PDRealtek8111: RTL8111/8168/8411 PCIe gigabit driver. See PDRealtek8111.h.
 *
 * Register offsets, descriptor layout and the init sequence follow NetBSD's
 * re(4) (sys/dev/ic/rtl81x9reg.h, sys/dev/ic/rtl8169.c), Copyright (c) 1997,
 * 1998-2003 Bill Paul. See LICENSE.re in this directory.
 */

#include "PDRealtek8111.h"
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/network/IONetworkMedium.h>
#include <libkern/OSByteOrder.h>

#define super IOEthernetController

OSDefineMetaClassAndStructors(PDRealtek8111, IOEthernetController);

/* Register offsets (rtl81x9reg.h). The Realtek register file is a mix of 8-,
 * 16- and 32-bit registers, unlike e1000's uniform 32-bit space. */
enum {
    RTK_IDR0            = 0x0000,   /* station address, 6 bytes */
    RTK_TXLIST_ADDR_LO  = 0x0020,
    RTK_TXLIST_ADDR_HI  = 0x0024,
    RTK_COMMAND         = 0x0037,   /* 8-bit */
    RTK_GTXSTART        = 0x0038,   /* 8-bit */
    RTK_IMR             = 0x003C,   /* 16-bit */
    RTK_ISR             = 0x003E,   /* 16-bit */
    RTK_TXCFG           = 0x0040,
    RTK_RXCFG           = 0x0044,
    RTK_MISSEDPKT       = 0x004C,
    RTK_EECMD           = 0x0050,   /* 8-bit */
    RTK_PHYAR           = 0x0060,
    RTK_GMEDIASTAT      = 0x006C,   /* 8-bit */
    RTK_MAR0            = 0x0008,   /* multicast hash, 8 bytes */
    RTK_MAXRXPKTLEN     = 0x00DA,   /* 16-bit, raw byte count */
    RTK_CPLUS_CMD       = 0x00E0,   /* 16-bit */
    RTK_RXLIST_ADDR_LO  = 0x00E4,
    RTK_RXLIST_ADDR_HI  = 0x00E8,
    RTK_EARLY_TX_THRESH = 0x00EC,   /* 8-bit */
    RTK_MISC            = 0x00F0,
};

enum {
    RTK_CMD_EMPTY_RXBUF = 0x01,
    RTK_CMD_TX_ENB      = 0x04,
    RTK_CMD_RX_ENB      = 0x08,
    RTK_CMD_RESET       = 0x10,
};

enum {
    RTK_EEMODE_OFF      = 0x00,
    RTK_EEMODE_WRITECFG = 0xC0,
};

enum {
    RTK_TXSTART_START   = 0x40,
};

enum {
    RTK_RXCFG_RX_ALLPHYS = 0x00000001,
    RTK_RXCFG_RX_INDIV   = 0x00000002,
    RTK_RXCFG_RX_MULTI   = 0x00000004,
    RTK_RXCFG_RX_BROAD   = 0x00000008,
    RTK_RXCFG_MAXDMA_UNL = 0x00000700,   /* unlimited DMA burst */
    RTK_RXCFG_FIFO_UNL   = 0x0000E000,   /* no FIFO threshold */
};

enum {
    RTK_TXCFG_MAXDMA_UNL = 0x00000700,
    RTK_TXCFG_IFG_STD    = 0x03000000,
};

enum {
    RE_CPLUSCMD_TXENB       = 0x0001,
    RE_CPLUSCMD_RXENB       = 0x0002,
    RE_CPLUSCMD_PCI_MRW     = 0x0008,
    RE_CPLUSCMD_MACSTAT_DIS = 0x0080,
};

enum {
    RTK_ISR_RX_OK           = 0x0001,
    RTK_ISR_RX_ERR          = 0x0002,
    RTK_ISR_TX_OK           = 0x0004,
    RTK_ISR_TX_ERR          = 0x0008,
    RTK_ISR_RX_OVERRUN      = 0x0010,
    RTK_ISR_LINKCHG         = 0x0020,
    RTK_ISR_TX_DESC_UNAVAIL = 0x0080,
    RTK_ISR_TIMEOUT_EXPIRED = 0x4000,
    RTK_ISR_SYSTEM_ERR      = 0x8000,
};

#define RTK_INTRS (RTK_ISR_RX_OK | RTK_ISR_RX_ERR | RTK_ISR_TX_ERR | \
                   RTK_ISR_RX_OVERRUN | RTK_ISR_LINKCHG | RTK_ISR_SYSTEM_ERR)

enum {
    RTK_MISC_RXDV_GATED_EN = (1u << 19),
};

/* Descriptor cmdstat bits, shared by RX and TX (rtl81x9reg.h). */
enum {
    RE_DESC_OWN         = 0x80000000,
    RE_DESC_EOR         = 0x40000000,
    RE_TDESC_CMD_SOF    = 0x20000000,
    RE_TDESC_CMD_EOF    = 0x10000000,
    RE_RDESC_CMD_BUFLEN = 0x00001FFF,
    RE_RDESC_STAT_EOF   = 0x10000000,
    RE_RDESC_STAT_RXERRSUM = 0x00100000,
    RE_RDESC_STAT_GFRAGLEN = 0x00003FFF,
};

enum {
    RTK_GMEDIASTAT_FDX      = 0x01,
    RTK_GMEDIASTAT_LINK     = 0x02,
    RTK_GMEDIASTAT_10MBPS   = 0x04,
    RTK_GMEDIASTAT_100MBPS  = 0x08,
    RTK_GMEDIASTAT_1000MBPS = 0x10,
};

#define RTK_RESET_TIMEOUT 1000

uint8_t  PDRealtek8111::reg8(uint32_t off)  { return *(volatile uint8_t *)(fRegs + off); }
uint16_t PDRealtek8111::reg16(uint32_t off) { return OSReadLittleInt16(fRegs, off); }
uint32_t PDRealtek8111::reg32(uint32_t off) { return OSReadLittleInt32(fRegs, off); }
void PDRealtek8111::write8(uint32_t off, uint8_t v)   { *(volatile uint8_t *)(fRegs + off) = v; }
void PDRealtek8111::write16(uint32_t off, uint16_t v) { OSWriteLittleInt16(fRegs, off, v); }
void PDRealtek8111::write32(uint32_t off, uint32_t v) { OSWriteLittleInt32(fRegs, off, v); }

bool PDRealtek8111::init(OSDictionary *properties)
{
    if (!super::init(properties))
        return false;

    fPCIDevice = NULL;
    fRegMap = NULL;
    fRegDesc = NULL;
    fRegs = NULL;
    fWorkLoop = NULL;
    fPollTimer = NULL;
    fInterruptSource = NULL;
    fInterface = NULL;
    fRxDescBuf = NULL;
    fRxDesc = NULL;
    fRxHead = 0;
    fRxPackets = 0;
    fTxDescBuf = NULL;
    fTxDesc = NULL;
    fTxTail = 0;
    fTxPackets = 0;
    fEnabled = false;
    fPromiscuous = false;
    fMulticastAll = false;
    fLastLinkStatus = 0;
    bzero(&fMACAddress, sizeof(fMACAddress));
    for (int i = 0; i < kRTLRxDescCount; i++)
        fRxPacketBuf[i] = NULL;
    for (int i = 0; i < kRTLTxDescCount; i++)
        fTxPacketBuf[i] = NULL;

    return true;
}

IOService *PDRealtek8111::probe(IOService *provider, SInt32 *score)
{
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci)
        return NULL;

    UInt16 vendor = pci->configRead16(kIOPCIConfigVendorID);
    UInt16 device = pci->configRead16(kIOPCIConfigDeviceID);

    IOLog("PDRealtek8111: probe vendor=0x%04x device=0x%04x\n", vendor, device);

    if (vendor != 0x10EC)
        return NULL;

    /* 0x8168 covers the whole RTL8111/8168/8411 line; 0x8161/0x8136 are the
     * other IDs the same C+ programming model appears under. 0x8125 (2.5GbE)
     * is deliberately NOT claimed - it has a different descriptor layout. */
    if (device != 0x8168 && device != 0x8161 && device != 0x8136 &&
        device != 0x8167 && device != 0x8169)
        return NULL;

    if (score)
        *score = 5000;
    return this;
}

/* Same config-space BAR sizing dance as PDE1000: the provider's IODeviceMemory
 * ranges are not dependable, so fall back to reading the BAR directly. */
static bool readMemoryBAR(IOPCIDevice *pci, UInt8 reg, uint64_t *outBase, uint64_t *outSize)
{
    if (!pci || !outBase || !outSize) return false;

    const uint16_t savedCmd = pci->configRead16(kIOPCIConfigCommand);
    const uint32_t savedLo  = pci->configRead32(reg);
    uint32_t savedHi = 0;

    if (savedLo & 0x1)
        return false;

    const bool is64 = ((savedLo & 0x6) == 0x4);
    if (is64) {
        if (reg > kIOPCIConfigBaseAddress4) return false;
        savedHi = pci->configRead32(reg + 4);
    }

    pci->configWrite16(kIOPCIConfigCommand, savedCmd & ~(uint16_t)0x3);
    pci->configWrite32(reg, 0xffffffffU);
    if (is64) pci->configWrite32(reg + 4, 0xffffffffU);

    const uint32_t maskLo = pci->configRead32(reg);
    const uint32_t maskHi = is64 ? pci->configRead32(reg + 4) : 0xffffffffU;

    pci->configWrite32(reg, savedLo);
    if (is64) pci->configWrite32(reg + 4, savedHi);
    pci->configWrite16(kIOPCIConfigCommand, savedCmd);

    uint64_t base = savedLo & ~0x0fULL;
    uint64_t sizeMask = maskLo & ~0x0fULL;
    if (is64) {
        base |= ((uint64_t)savedHi << 32);
        sizeMask |= ((uint64_t)maskHi << 32);
    }
    if (!base || !sizeMask) return false;

    uint64_t size = (~sizeMask) + 1;
    if (!is64) size &= 0xffffffffULL;
    if (size < 0x1000) size = 0x1000;
    if (size > 0x1000000ULL) return false;

    *outBase = base;
    *outSize = size;
    return true;
}

/* This NIC sits behind a PCIe port bridge whose IOKit "ranges" come through with
 * zero length, and the nub gets no assigned-addresses. If the bridge is not
 * decoding the window its child's BAR lives in, every MMIO read returns 0xff.
 * Report what the bridge is actually programmed with, and enable its decode. */
void PDRealtek8111::checkBridgeDecode()
{
    IOService *pp = fPCIDevice->getProvider();
    IOPCIDevice *bridge = pp ? OSDynamicCast(IOPCIDevice, pp->getProvider()) : NULL;
    if (!bridge)
        return;

    /* Identify it before trusting anything it reports: header type must be 1,
     * or getProvider()->getProvider() landed somewhere other than the port. */
    const uint32_t id = bridge->configRead32(kIOPCIConfigVendorID);
    const uint8_t hdr = bridge->configRead8(kIOPCIConfigHeaderType) & 0x7f;
    const uint32_t buses = bridge->configRead32(0x18);
    IOLog("PDRealtek8111: bridge %04x:%04x hdr=%u bus %u->%u..%u\n",
        (uint16_t)(id & 0xffff), (uint16_t)(id >> 16), hdr,
        (uint8_t)buses, (uint8_t)(buses >> 8), (uint8_t)(buses >> 16));
    if (hdr != 1)
        return;

    uint16_t cmd = bridge->configRead16(kIOPCIConfigCommand);
    uint32_t winBase = (uint32_t)(bridge->configRead16(0x20) & 0xfff0) << 16;
    uint32_t winLim = ((uint32_t)(bridge->configRead16(0x22) & 0xfff0) << 16) | 0xfffff;

    IOLog("PDRealtek8111: bridge cmd=0x%04x memwin=0x%08x-0x%08x\n", cmd, winBase, winLim);

    /* If firmware left the window closed (base > limit) or it does not cover our
     * BAR, MMIO to the NIC is never forwarded and every read returns 0xff. */
    const uint32_t bar = fPCIDevice->configRead32(kIOPCIConfigBaseAddress2) & ~0xfU;
    if (bar && (winBase > winLim || bar < winBase || bar > winLim)) {
        const uint32_t newBase = bar & 0xfff00000;
        IOLog("PDRealtek8111: bridge window does not cover BAR 0x%08x, opening 0x%08x\n",
            bar, newBase);
        bridge->configWrite16(0x20, (uint16_t)(newBase >> 16));
        bridge->configWrite16(0x22, (uint16_t)(newBase >> 16));
    }

    /* bit 1 = memory space, bit 2 = bus master (needed to forward our DMA). */
    if ((cmd & 0x6) != 0x6) {
        IOLog("PDRealtek8111: enabling bridge memory decode\n");
        bridge->configWrite16(kIOPCIConfigCommand, cmd | 0x6);
    }
}

/* Many 8168 boards are left in D3hot by firmware: config space still answers,
 * but every MMIO read returns 0xff and the reset bit never clears. re(4) gets
 * this from its PCI attachment (pci_set_powerstate), which IOKit does not do
 * for us here. */
void PDRealtek8111::forcePowerStateD0()
{
    UInt8 pmCap = 0;
    if (!fPCIDevice->findPCICapability(kIOPCIPowerManagementCapability, &pmCap) || !pmCap)
        return;

    const uint16_t pmcsr = fPCIDevice->configRead16(pmCap + 4);
    if ((pmcsr & 0x3) == 0)
        return;

    IOLog("PDRealtek8111: device in D%u, forcing D0\n", pmcsr & 0x3);
    fPCIDevice->configWrite16(pmCap + 4, (uint16_t)(pmcsr & ~(uint16_t)0x3));
    IOSleep(10);
}

bool PDRealtek8111::chipReset()
{
    write8(RTK_COMMAND, RTK_CMD_RESET);
    for (int i = 0; i < RTK_RESET_TIMEOUT; i++) {
        IODelay(10);
        if ((reg8(RTK_COMMAND) & RTK_CMD_RESET) == 0)
            return true;
    }
    IOLog("PDRealtek8111: reset never completed\n");
    return false;
}

/* The chip auto-loads the station address from EEPROM into IDR0..5 at reset,
 * so unlike e1000 there is no EEPROM read protocol to drive here. */
bool PDRealtek8111::readMACAddress()
{
    for (int i = 0; i < kIOEthernetAddressSize; i++)
        fMACAddress.bytes[i] = reg8(RTK_IDR0 + i);

    bool allZero = true, allOnes = true;
    for (int i = 0; i < kIOEthernetAddressSize; i++) {
        if (fMACAddress.bytes[i] != 0x00) allZero = false;
        if (fMACAddress.bytes[i] != 0xFF) allOnes = false;
    }
    if (allZero || allOnes || (fMACAddress.bytes[0] & 0x01)) {
        IOLog("PDRealtek8111: implausible station address in IDR0\n");
        return false;
    }
    return true;
}

/* physicalMask does double duty: the low zero bits force 256-byte alignment,
 * which the chip requires of both ring bases, and the 32-bit ceiling keeps the
 * rings addressable without relying on the HI address registers. */
bool PDRealtek8111::initRxRing()
{
    size_t descSize = sizeof(RTLDesc) * kRTLRxDescCount;
    fRxDescBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        descSize, 0x00000000FFFFFF00ULL);
    if (!fRxDescBuf)
        return false;
    fRxDescBuf->prepare();
    fRxDesc = (RTLDesc *)fRxDescBuf->getBytesNoCopy();
    bzero((void *)fRxDesc, descSize);

    for (int i = 0; i < kRTLRxDescCount; i++) {
        IOBufferMemoryDescriptor *buf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
            kRTLRxBufferSize, 0x00000000FFFFFFFFULL);
        if (!buf)
            return false;
        buf->prepare();
        fRxPacketBuf[i] = buf;

        uint64_t phys = buf->getPhysicalAddress();
        fRxDesc[i].bufaddr_lo = (uint32_t)(phys & 0xFFFFFFFF);
        fRxDesc[i].bufaddr_hi = (uint32_t)(phys >> 32);
        fRxDesc[i].vlanctl = 0;
        /* Hand every descriptor to the chip; EOR marks the ring wrap. */
        fRxDesc[i].cmdstat = RE_DESC_OWN | (kRTLRxBufferSize & RE_RDESC_CMD_BUFLEN) |
            ((i == kRTLRxDescCount - 1) ? RE_DESC_EOR : 0);
    }
    fRxHead = 0;

    uint64_t descPhys = fRxDescBuf->getPhysicalAddress();
    write32(RTK_RXLIST_ADDR_LO, (uint32_t)(descPhys & 0xFFFFFFFF));
    write32(RTK_RXLIST_ADDR_HI, (uint32_t)(descPhys >> 32));
    return true;
}

bool PDRealtek8111::initTxRing()
{
    size_t descSize = sizeof(RTLDesc) * kRTLTxDescCount;
    fTxDescBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        descSize, 0x00000000FFFFFF00ULL);
    if (!fTxDescBuf)
        return false;
    fTxDescBuf->prepare();
    fTxDesc = (RTLDesc *)fTxDescBuf->getBytesNoCopy();
    bzero((void *)fTxDesc, descSize);

    for (int i = 0; i < kRTLTxDescCount; i++) {
        IOBufferMemoryDescriptor *buf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
            kRTLRxBufferSize, 0x00000000FFFFFFFFULL);
        if (!buf)
            return false;
        buf->prepare();
        fTxPacketBuf[i] = buf;

        uint64_t phys = buf->getPhysicalAddress();
        fTxDesc[i].bufaddr_lo = (uint32_t)(phys & 0xFFFFFFFF);
        fTxDesc[i].bufaddr_hi = (uint32_t)(phys >> 32);
        fTxDesc[i].vlanctl = 0;
        /* Driver-owned until a packet is queued; only EOR is set up front. */
        fTxDesc[i].cmdstat = (i == kRTLTxDescCount - 1) ? RE_DESC_EOR : 0;
    }
    fTxTail = 0;

    uint64_t descPhys = fTxDescBuf->getPhysicalAddress();
    write32(RTK_TXLIST_ADDR_LO, (uint32_t)(descPhys & 0xFFFFFFFF));
    write32(RTK_TXLIST_ADDR_HI, (uint32_t)(descPhys >> 32));
    return true;
}

void PDRealtek8111::applyRxFilter()
{
    uint32_t rxcfg = RTK_RXCFG_RX_INDIV | RTK_RXCFG_RX_BROAD |
        RTK_RXCFG_MAXDMA_UNL | RTK_RXCFG_FIFO_UNL;

    if (fPromiscuous)
        rxcfg |= RTK_RXCFG_RX_ALLPHYS | RTK_RXCFG_RX_MULTI;
    if (fMulticastAll)
        rxcfg |= RTK_RXCFG_RX_MULTI;

    write32(RTK_RXCFG, rxcfg);

    /* Accept-all hash while the multicast list is not filtered precisely;
     * an exact filter would program the CRC-derived bits here instead. */
    uint32_t mar = (fPromiscuous || fMulticastAll) ? 0xFFFFFFFF : 0x00000000;
    write32(RTK_MAR0, mar);
    write32(RTK_MAR0 + 4, mar);
}

void PDRealtek8111::startChip()
{
    /* C+ mode must be configured before anything else (re_init_locked). */
    write16(RTK_CPLUS_CMD, RE_CPLUSCMD_PCI_MRW | RE_CPLUSCMD_MACSTAT_DIS |
        RE_CPLUSCMD_TXENB);
    write16(0x00E2 /* RTK_IM */, 0x0000);
    IODelay(10000);

    /* Some 8168 revisions gate RXDV until this is cleared; harmless elsewhere. */
    write32(RTK_MISC, reg32(RTK_MISC) & ~RTK_MISC_RXDV_GATED_EN);

    write8(RTK_EARLY_TX_THRESH, 0x3F);
    /* RxMaxSize is a raw byte count (r8169/re(4) both write it unshifted); the
     * old >>3 capped RX at 256 bytes, so ARP passed and DHCP OFFER did not. */
    write16(RTK_MAXRXPKTLEN, kRTLRxBufferSize);

    write32(RTK_TXCFG, RTK_TXCFG_MAXDMA_UNL | RTK_TXCFG_IFG_STD);
    applyRxFilter();

    /* Enable the engines only after RX/TX config is programmed - several
     * 8168 revisions require that ordering (RTKQ_TXRXEN_LATER in re(4)). */
    write8(RTK_COMMAND, RTK_CMD_TX_ENB | RTK_CMD_RX_ENB);
    write32(RTK_MISSEDPKT, 0);
}

void PDRealtek8111::stopChip()
{
    write16(RTK_IMR, 0);
    write16(RTK_ISR, 0xFFFF);
    write8(RTK_COMMAND, 0);
}

bool PDRealtek8111::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;

    fPCIDevice->retain();
    if (!fPCIDevice->open(this)) {
        IOLog("PDRealtek8111: failed to open PCI device\n");
        return false;
    }

    checkBridgeDecode();
    forcePowerStateD0();
    fPCIDevice->setBusMasterEnable(true);
    fPCIDevice->setMemoryEnable(true);
    IOLog("PDRealtek8111: device cmd=0x%04x bar2=0x%08x\n",
        fPCIDevice->configRead16(kIOPCIConfigCommand),
        fPCIDevice->configRead32(kIOPCIConfigBaseAddress2));

    /* The 8168 exposes its registers at BAR2 (BAR0 is the legacy I/O port
     * range inherited from the 8139), so map BAR2 and fall back to BAR1/BAR0
     * only if that is absent. */
    static const UInt8 barOrder[] = {
        kIOPCIConfigBaseAddress2, kIOPCIConfigBaseAddress1, kIOPCIConfigBaseAddress0
    };
    for (unsigned b = 0; b < sizeof(barOrder) / sizeof(barOrder[0]) && !fRegMap; b++) {
        IODeviceMemory *bar = fPCIDevice->getDeviceMemoryWithRegister(barOrder[b]);
        if (bar && bar->getLength())
            fRegMap = bar->map(kIOMapAnywhere);
        if (!fRegMap) {
            uint64_t base = 0, size = 0;
            if (readMemoryBAR(fPCIDevice, barOrder[b], &base, &size)) {
                fRegDesc = IOMemoryDescriptor::withPhysicalAddress(
                    (IOPhysicalAddress)base, (IOByteCount)size,
                    kIODirectionNone | kIOMemoryMapperNone);
                if (fRegDesc)
                    fRegMap = fRegDesc->map(kIOMapAnywhere);
                if (fRegMap)
                    IOLog("PDRealtek8111: BAR@0x%02x config fallback base=0x%llx size=0x%llx\n",
                        barOrder[b], (unsigned long long)base, (unsigned long long)size);
                else if (fRegDesc) {
                    fRegDesc->release();
                    fRegDesc = NULL;
                }
            }
        }
    }
    if (!fRegMap) {
        IOLog("PDRealtek8111: failed to map register BAR\n");
        return false;
    }
    fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();

    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
        return false;

    fInterruptSource = IOInterruptEventSource::interruptEventSource(
        this, &PDRealtek8111::interruptOccurredStatic, fPCIDevice, 0);
    if (fInterruptSource && fWorkLoop->addEventSource(fInterruptSource) == kIOReturnSuccess) {
        IOLog("PDRealtek8111: using MSI interrupt (source 0)\n");
    } else {
        if (fInterruptSource) { fInterruptSource->release(); fInterruptSource = NULL; }
        IOLog("PDRealtek8111: MSI registration failed, using polling only\n");
    }

    fPollTimer = IOTimerEventSource::timerEventSource(this, &PDRealtek8111::pollTimerAction);
    if (!fPollTimer || fWorkLoop->addEventSource(fPollTimer) != kIOReturnSuccess) {
        IOLog("PDRealtek8111: failed to create poll timer\n");
        return false;
    }

    /* All-0xff here means the window is dead (wrong BAR, or still powered
     * down) rather than the chip refusing to reset. */
    IOLog("PDRealtek8111: cmd=0x%02x txcfg=0x%08x mediastat=0x%02x idr=%08x%04x\n",
        reg8(RTK_COMMAND), reg32(RTK_TXCFG), reg8(RTK_GMEDIASTAT),
        reg32(RTK_IDR0), reg16(RTK_IDR0 + 4));

    if (!chipReset())
        return false;

    write16(RTK_IMR, 0);
    write16(RTK_ISR, 0xFFFF);

    if (!readMACAddress()) {
        IOLog("PDRealtek8111: failed to read station address\n");
        return false;
    }

    IOLog("PDRealtek8111: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
        fMACAddress.bytes[0], fMACAddress.bytes[1], fMACAddress.bytes[2],
        fMACAddress.bytes[3], fMACAddress.bytes[4], fMACAddress.bytes[5]);

    if (!initRxRing() || !initTxRing())
        return false;

    startChip();

    if (!publishLinkMedium())
        return false;

    if (!attachInterface((IONetworkInterface **)&fInterface, true)) {
        IOLog("PDRealtek8111: attachInterface failed\n");
        return false;
    }

    updateLinkStatus();
    registerService();
    return true;
}

void PDRealtek8111::stop(IOService *provider)
{
    if (fEnabled)
        disable(fInterface);

    if (fRegs)
        stopChip();

    if (fInterface) {
        detachInterface(fInterface, true);
        fInterface = NULL;
    }
    if (fPCIDevice && fPCIDevice->isOpen(this))
        fPCIDevice->close(this);

    super::stop(provider);
}

void PDRealtek8111::free()
{
    if (fPollTimer) {
        if (fWorkLoop) fWorkLoop->removeEventSource(fPollTimer);
        fPollTimer->release();
        fPollTimer = NULL;
    }
    if (fInterruptSource) {
        if (fWorkLoop) fWorkLoop->removeEventSource(fInterruptSource);
        fInterruptSource->release();
        fInterruptSource = NULL;
    }
    for (int i = 0; i < kRTLRxDescCount; i++) {
        if (fRxPacketBuf[i]) {
            fRxPacketBuf[i]->complete();
            fRxPacketBuf[i]->release();
            fRxPacketBuf[i] = NULL;
        }
    }
    for (int i = 0; i < kRTLTxDescCount; i++) {
        if (fTxPacketBuf[i]) {
            fTxPacketBuf[i]->complete();
            fTxPacketBuf[i]->release();
            fTxPacketBuf[i] = NULL;
        }
    }
    if (fRxDescBuf) { fRxDescBuf->complete(); fRxDescBuf->release(); fRxDescBuf = NULL; }
    if (fTxDescBuf) { fTxDescBuf->complete(); fTxDescBuf->release(); fTxDescBuf = NULL; }
    if (fRegMap)  { fRegMap->release();  fRegMap = NULL; }
    if (fRegDesc) { fRegDesc->release(); fRegDesc = NULL; }
    if (fPCIDevice) { fPCIDevice->release(); fPCIDevice = NULL; }

    super::free();
}

bool PDRealtek8111::publishLinkMedium()
{
    OSDictionary *mediumDict = OSDictionary::withCapacity(4);
    if (!mediumDict)
        return false;

    IONetworkMedium *autoMedium = IONetworkMedium::medium(
        kIOMediumEthernetAuto | kIOMediumOptionFullDuplex, 1000000000ULL);
    IONetworkMedium *gigMedium = IONetworkMedium::medium(
        kIOMediumEthernet1000BaseT | kIOMediumOptionFullDuplex, 1000000000ULL);
    IONetworkMedium *fastMedium = IONetworkMedium::medium(
        kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex, 100000000ULL);
    IONetworkMedium *tenMedium = IONetworkMedium::medium(
        kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex, 10000000ULL);

    bool ok = autoMedium && gigMedium && fastMedium && tenMedium &&
        IONetworkMedium::addMedium(mediumDict, autoMedium) &&
        IONetworkMedium::addMedium(mediumDict, gigMedium) &&
        IONetworkMedium::addMedium(mediumDict, fastMedium) &&
        IONetworkMedium::addMedium(mediumDict, tenMedium) &&
        publishMediumDictionary(mediumDict) &&
        setCurrentMedium(autoMedium);

    if (tenMedium)  tenMedium->release();
    if (fastMedium) fastMedium->release();
    if (gigMedium)  gigMedium->release();
    if (autoMedium) autoMedium->release();
    mediumDict->release();

    if (!ok)
        IOLog("PDRealtek8111: failed to publish link medium\n");
    return ok;
}

/* RTK_GMEDIASTAT reports the resolved link directly, so the PHY does not have
 * to be interrogated over MDIO just to learn speed and duplex. */
void PDRealtek8111::updateLinkStatus()
{
    uint8_t st = reg8(RTK_GMEDIASTAT);
    uint32_t status = kIONetworkLinkValid;
    uint64_t speed = 0;
    uint32_t mediumType = kIOMediumEthernetNone;

    if (st & RTK_GMEDIASTAT_LINK) {
        status |= kIONetworkLinkActive;
        if (st & RTK_GMEDIASTAT_1000MBPS) {
            speed = 1000000000ULL; mediumType = kIOMediumEthernet1000BaseT;
        } else if (st & RTK_GMEDIASTAT_100MBPS) {
            speed = 100000000ULL;  mediumType = kIOMediumEthernet100BaseTX;
        } else if (st & RTK_GMEDIASTAT_10MBPS) {
            speed = 10000000ULL;   mediumType = kIOMediumEthernet10BaseT;
        }
        if (st & RTK_GMEDIASTAT_FDX)
            mediumType |= kIOMediumOptionFullDuplex;
    }

    if (status == fLastLinkStatus)
        return;
    fLastLinkStatus = status;

    const IONetworkMedium *medium = IONetworkMedium::getMediumWithType(
        getMediumDictionary(), mediumType);
    setLinkStatus(status, medium, speed);
    IOLog("PDRealtek8111: link %s\n",
        (status & kIONetworkLinkActive) ? "up" : "down");
}

IOReturn PDRealtek8111::enable(IONetworkInterface *interface)
{
    if (fEnabled)
        return kIOReturnSuccess;

    IOLog("PDRealtek8111: enable interface=%p\n", interface);

    if (fInterruptSource) {
        write16(RTK_IMR, RTK_INTRS);
        fInterruptSource->enable();
    }
    if (fPollTimer)
        fPollTimer->setTimeoutMS(5);

    /* IOGatedOutputQueue is created with capacity 0 and stopped, so without
     * this nothing is ever handed to outputPacket. */
    IOOutputQueue *queue = getOutputQueue();
    if (queue) {
        queue->setCapacity(kRTLTxDescCount);
        queue->start();
    }

    fEnabled = true;
    updateLinkStatus();
    return kIOReturnSuccess;
}

IOReturn PDRealtek8111::disable(IONetworkInterface *interface)
{
    if (!fEnabled)
        return kIOReturnSuccess;

    IOLog("PDRealtek8111: disable interface=%p\n", interface);

    if (fInterruptSource) {
        write16(RTK_IMR, 0);
        fInterruptSource->disable();
    }
    if (fPollTimer)
        fPollTimer->cancelTimeout();

    IOOutputQueue *queue = getOutputQueue();
    if (queue) {
        queue->stop();
        queue->setCapacity(0);
        queue->flush();
    }

    fEnabled = false;
    return kIOReturnSuccess;
}

void PDRealtek8111::pollTimerAction(OSObject *owner, IOTimerEventSource *sender)
{
    PDRealtek8111 *self = OSDynamicCast(PDRealtek8111, owner);
    if (!self)
        return;
    self->pollReceive();
    self->updateLinkStatus();
    if (self->fEnabled)
        sender->setTimeoutMS(5);
}

void PDRealtek8111::interruptOccurredStatic(OSObject *owner, IOInterruptEventSource *sender, int count)
{
    PDRealtek8111 *self = OSDynamicCast(PDRealtek8111, owner);
    if (!self)
        return;
    self->interruptOccurred(sender, count);
}

void PDRealtek8111::interruptOccurred(IOInterruptEventSource *sender, int count)
{
    /* ISR is write-1-to-clear, unlike e1000's clear-on-read ICR. */
    uint16_t isr = reg16(RTK_ISR);
    if (!isr)
        return;
    write16(RTK_ISR, isr);

    if (isr & (RTK_ISR_RX_OK | RTK_ISR_RX_ERR | RTK_ISR_RX_OVERRUN))
        pollReceive();
    if (isr & RTK_ISR_LINKCHG)
        updateLinkStatus();
}

void PDRealtek8111::pollReceive()
{
    /* The chip walks the ring itself; ownership is per-descriptor rather than
     * head/tail registers as on e1000, so follow OWN from where we left off. */
    for (uint32_t n = 0; n < kRTLRxDescCount; n++) {
        uint32_t idx = fRxHead;
        RTLDesc *desc = &fRxDesc[idx];
        uint32_t cmdstat = desc->cmdstat;

        if (cmdstat & RE_DESC_OWN)
            break;

        uint32_t len = cmdstat & RE_RDESC_STAT_GFRAGLEN;
        bool ok = (cmdstat & RE_RDESC_STAT_EOF) &&
                  !(cmdstat & RE_RDESC_STAT_RXERRSUM) &&
                  len > 4 && len <= kRTLRxBufferSize;

        if (ok) {
            /* The chip includes the 4-byte FCS in the reported length. */
            len -= 4;
            fRxPacketBuf[idx]->complete();
            fRxPackets++;
            mbuf_t m = allocatePacket(len);
            if (m) {
                if (mbuf_copyback(m, 0, len, fRxPacketBuf[idx]->getBytesNoCopy(),
                        MBUF_WAITOK) == 0) {
                    mbuf_pkthdr_setlen(m, len);
                    mbuf_setlen(m, len);
                }
                fInterface->inputPacket(m, len,
                    IONetworkInterface::kInputOptionQueuePacket);
            }
            fRxPacketBuf[idx]->prepare();
        }

        /* Hand the descriptor back, preserving EOR on the last slot. */
        desc->vlanctl = 0;
        desc->cmdstat = RE_DESC_OWN | (kRTLRxBufferSize & RE_RDESC_CMD_BUFLEN) |
            ((idx == kRTLRxDescCount - 1) ? RE_DESC_EOR : 0);

        fRxHead = (idx + 1) % kRTLRxDescCount;
    }

    if (fInterface)
        fInterface->flushInputQueue();
}

UInt32 PDRealtek8111::outputPacket(mbuf_t m, void *param)
{
    size_t pktLen = mbuf_pkthdr_len(m);
    if (pktLen == 0 || pktLen > kRTLRxBufferSize) {
        freePacket(m);
        return kIOReturnOutputDropped;
    }

    uint32_t idx = fTxTail;
    RTLDesc *desc = &fTxDesc[idx];

    /* Descriptor is ours again once the chip clears OWN. */
    for (int spin = 0; spin < 100000 && (desc->cmdstat & RE_DESC_OWN); spin++)
        ;
    if (desc->cmdstat & RE_DESC_OWN) {
        freePacket(m);
        return kIOReturnOutputDropped;
    }

    /* The buffer was prepared once in initTxRing and stays wired; the old
     * complete()/prepare() pair here unwired it across the chip's DMA. */
    mbuf_copydata(m, 0, pktLen, fTxPacketBuf[idx]->getBytesNoCopy());

    /* Single-fragment frame: SOF and EOF both set. OWN must be written last -
     * the chip may start on the descriptor the moment it sees it. */
    desc->vlanctl = 0;
    uint32_t cmd = RE_TDESC_CMD_SOF | RE_TDESC_CMD_EOF | (uint32_t)pktLen |
        ((idx == kRTLTxDescCount - 1) ? RE_DESC_EOR : 0);
    desc->cmdstat = cmd;
    OSSynchronizeIO();
    desc->cmdstat = cmd | RE_DESC_OWN;
    OSSynchronizeIO();

    fTxPackets++;
    fTxTail = (idx + 1) % kRTLTxDescCount;

    write8(RTK_GTXSTART, RTK_TXSTART_START);

    /* First few frames only: did the chip actually consume the descriptor?
     * OWN still set means it is not processing the TX ring at all. */
    if (fTxPackets <= 3) {
        int spin = 0;
        while (spin < 10000 && (desc->cmdstat & RE_DESC_OWN)) {
            IODelay(10);
            spin++;
        }
        IOLog("PDRealtek8111: tx %u len=%u own=%s cmdstat=0x%08x isr=0x%04x "
              "cmd=0x%02x txcfg=0x%08x missed=%u\n",
            fTxPackets, (unsigned)pktLen,
            (desc->cmdstat & RE_DESC_OWN) ? "STUCK" : "cleared",
            desc->cmdstat, reg16(RTK_ISR), reg8(RTK_COMMAND),
            reg32(RTK_TXCFG), reg32(RTK_MISSEDPKT));
    }

    freePacket(m);
    return kIOReturnOutputSuccess;
}

IOReturn PDRealtek8111::getHardwareAddress(IOEthernetAddress *addr)
{
    if (!addr)
        return kIOReturnBadArgument;
    *addr = fMACAddress;
    return kIOReturnSuccess;
}

IOReturn PDRealtek8111::setPromiscuousMode(bool active)
{
    fPromiscuous = active;
    if (fRegs)
        applyRxFilter();
    return kIOReturnSuccess;
}

IOReturn PDRealtek8111::setMulticastMode(bool active)
{
    fMulticastAll = active;
    if (fRegs)
        applyRxFilter();
    return kIOReturnSuccess;
}

IOReturn PDRealtek8111::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
    /* No exact filter yet: setMulticastMode() already opened the hash when the
     * stack asked for multicast, so the list itself needs no action. */
    return kIOReturnSuccess;
}

const OSString *PDRealtek8111::newVendorString() const
{
    return OSString::withCString("Realtek");
}

const OSString *PDRealtek8111::newModelString() const
{
    return OSString::withCString("RTL8111/8168 PCIe Gigabit Ethernet");
}

IOOutputQueue *PDRealtek8111::createOutputQueue()
{
    /* Same as PDE1000: this tree's IONetworkController never starts the
     * optional IOOutputQueue in doEnable(), so a gated queue swallows every
     * packet before outputPacket() sees it. Use the direct output handler.
     * enable()/disable() below still drive the queue if one is restored. */
    return NULL;
}
