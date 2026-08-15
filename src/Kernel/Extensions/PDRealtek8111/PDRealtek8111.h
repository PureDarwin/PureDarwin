/*
 * PDRealtek8111: IOEthernetController for the Realtek RTL8111/8168/8411 PCIe
 * gigabit family - the usual onboard NIC on consumer x86 hardware.
 *
 * The register layout, descriptor format and initialisation sequence are taken
 * from NetBSD's re(4) (sys/dev/ic/rtl8169.c, sys/dev/ic/rtl81x9reg.h), which is
 * BSD-licensed. NetBSD keeps the chip logic in sys/dev/ic/ and the bus
 * attachment in sys/dev/pci/if_re_pci.c; this file replaces that attachment
 * layer with IOKit, so the chip programming below follows re(4) closely while
 * the service/DMA/queue plumbing follows PDE1000 beside it.
 *
 * Info.plist matches the IDs sharing this C+ programming model; 0x8125 (2.5GbE)
 * is deliberately excluded, as it uses a different descriptor layout.
 *
 * Original driver code is Copyright (c) 1997, 1998-2003 Bill Paul; see
 * LICENSE.re in this directory for the full notice, which must be retained in
 * source and reproduced in binary distributions.
 */

#ifndef _PDREALTEK8111_H
#define _PDREALTEK8111_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOGatedOutputQueue.h>

#define kRTLRxDescCount   64
#define kRTLTxDescCount   64
#define kRTLRxBufferSize  2048

/* re_desc from rtl81x9reg.h: identical layout for RX and TX, 16 bytes, and the
 * ring base must be 256-byte aligned. */
struct RTLDesc {
    volatile uint32_t cmdstat;
    volatile uint32_t vlanctl;
    volatile uint32_t bufaddr_lo;
    volatile uint32_t bufaddr_hi;
} __attribute__((packed));

class PDRealtek8111 : public IOEthernetController
{
    OSDeclareDefaultStructors(PDRealtek8111);

public:
    bool init(OSDictionary *properties) override;
    void free() override;
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;

    IOReturn enable(IONetworkInterface *interface) override;
    IOReturn disable(IONetworkInterface *interface) override;

    UInt32 outputPacket(mbuf_t m, void *param) override;

    IOReturn getHardwareAddress(IOEthernetAddress *addr) override;
    IOReturn setPromiscuousMode(bool active) override;
    IOReturn setMulticastMode(bool active) override;
    IOReturn setMulticastList(IOEthernetAddress *addrs, UInt32 count) override;

    const OSString *newVendorString() const override;
    const OSString *newModelString() const override;

    IOOutputQueue *createOutputQueue() override;

private:
    IOPCIDevice          *fPCIDevice;
    IOMemoryMap           *fRegMap;
    IOMemoryDescriptor    *fRegDesc;   // set only when the BAR is mapped via config fallback
    volatile uint8_t      *fRegs;      // register space; needs 8/16/32-bit access
    IOWorkLoop            *fWorkLoop;
    IOTimerEventSource     *fPollTimer;
    IOInterruptEventSource *fInterruptSource;
    IOEthernetInterface    *fInterface;

    IOBufferMemoryDescriptor *fRxDescBuf;
    IOBufferMemoryDescriptor *fRxPacketBuf[kRTLRxDescCount];
    RTLDesc                  *fRxDesc;
    uint32_t                  fRxHead;
    uint32_t                  fRxPackets;

    IOBufferMemoryDescriptor *fTxDescBuf;
    IOBufferMemoryDescriptor *fTxPacketBuf[kRTLTxDescCount];
    RTLDesc                  *fTxDesc;
    uint32_t                  fTxTail;
    uint32_t                  fTxPackets;

    IOEthernetAddress fMACAddress;
    bool              fEnabled;
    bool              fPromiscuous;
    bool              fMulticastAll;
    uint32_t          fLastLinkStatus;

    uint8_t  reg8(uint32_t off);
    uint16_t reg16(uint32_t off);
    uint32_t reg32(uint32_t off);
    void     write8(uint32_t off, uint8_t v);
    void     write16(uint32_t off, uint16_t v);
    void     write32(uint32_t off, uint32_t v);

    void     checkBridgeDecode();
    void     forcePowerStateD0();
    bool     chipReset();
    bool     readMACAddress();
    bool     initRxRing();
    bool     initTxRing();
    void     applyRxFilter();
    void     startChip();
    void     stopChip();
    void     pollReceive();
    void     updateLinkStatus();
    bool     publishLinkMedium();
    static void pollTimerAction(OSObject *owner, IOTimerEventSource *sender);
    void     interruptOccurred(IOInterruptEventSource *sender, int count);
    static void interruptOccurredStatic(OSObject *owner, IOInterruptEventSource *sender, int count);
};

#endif /* !_PDREALTEK8111_H */
