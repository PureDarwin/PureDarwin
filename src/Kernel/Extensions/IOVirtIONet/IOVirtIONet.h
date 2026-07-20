#pragma once

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include "IOVirtIOTransport.h"

#define kVNetRxCount     32
#define kVNetTxCount     32
#define kVNetMaxPacket   2048
#define kVNetHdrSize     12   // struct virtio_net_hdr_v1 (VERSION_1 always negotiated)

class IOVirtIONet : public IOEthernetController
{
    OSDeclareDefaultStructors(IOVirtIONet);

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
    IOPCIDevice       *fPCIDevice;
    IOVirtIOTransport  fTransport;
    IOWorkLoop        *fWorkLoop;
    IOTimerEventSource *fPollTimer;
    IOEthernetInterface *fInterface;

    VirtQueue fRxQ; // queue index 0
    VirtQueue fTxQ; // queue index 1

    IOBufferMemoryDescriptor *fRxBuf[kVNetRxCount]; // hdr+packet, one descriptor each
    IOBufferMemoryDescriptor *fTxBuf[kVNetTxCount];
    uint32_t fTxNextSlot;
    uint32_t fTxOutstanding;

    IOEthernetAddress fMACAddress;
    bool     fEnabled;
    uint32_t fRxPackets;
    uint32_t fTxPackets;

    bool     readMACAddress();
    bool     initRxRing();
    void     postRxBuffer(unsigned slot);
    void     pollReceive();
    bool     publishLinkMedium();
    static void pollTimerAction(OSObject *owner, IOTimerEventSource *sender);
};
