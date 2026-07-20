#include "IOVirtIONet.h"
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/network/IOOutputQueue.h>
#include <libkern/OSByteOrder.h>

#define super IOEthernetController
OSDefineMetaClassAndStructors(IOVirtIONet, IOEthernetController);

enum {
    // VIRTIO_NET_F_MAC (bit 5): device config's mac[] field is valid and
    // should be used as the interface's persistent address.
    VIRTIO_NET_F_MAC = (1u << 5),
};

bool IOVirtIONet::init(OSDictionary *properties)
{
    if (!super::init(properties))
        return false;

    fPCIDevice = NULL;
    fWorkLoop = NULL;
    fPollTimer = NULL;
    fInterface = NULL;
    bzero(&fRxQ, sizeof(fRxQ));
    bzero(&fTxQ, sizeof(fTxQ));
    bzero(fRxBuf, sizeof(fRxBuf));
    bzero(fTxBuf, sizeof(fTxBuf));
    fTxNextSlot = 0;
    fTxOutstanding = 0;
    fEnabled = false;
    fRxPackets = fTxPackets = 0;
    bzero(&fMACAddress, sizeof(fMACAddress));
    return true;
}

IOService *IOVirtIONet::probe(IOService *provider, SInt32 *score)
{
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci)
        return NULL;

    UInt16 vendor = pci->configRead16(kIOPCIConfigVendorID);
    UInt16 device = pci->configRead16(kIOPCIConfigDeviceID);
    IOLog("IOVirtIONet: probe vendor=0x%04x device=0x%04x\n", vendor, device);

    // Red Hat/Virtio vendor. Modern virtio-net is device id 0x1041;
    // legacy/transitional is 0x1000 - match both.
    if (vendor != 0x1af4)
        return NULL;
    if (device != 0x1041 && device != 0x1000)
        return NULL;

    if (score)
        *score = 5000;
    return this;
}

bool IOVirtIONet::readMACAddress()
{
    volatile uint8_t *cfg = fTransport.deviceConfig();
    if (!cfg)
        return false;

    for (int i = 0; i < 6; i++)
        fMACAddress.bytes[i] = cfg[i];
    return true;
}

void IOVirtIONet::postRxBuffer(unsigned slot)
{
    VirtIOChainEntry entry = { fRxBuf[slot]->getPhysicalAddress(),
                              (uint32_t)(kVNetHdrSize + kVNetMaxPacket), true };
    fTransport.addDescChain(&fRxQ, &entry, 1);
}

bool IOVirtIONet::initRxRing()
{
    for (int i = 0; i < kVNetRxCount; i++) {
        fRxBuf[i] = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
            kVNetHdrSize + kVNetMaxPacket, 0xFFFFFFFFULL);
        if (!fRxBuf[i])
            return false;
        bzero(fRxBuf[i]->getBytesNoCopy(), kVNetHdrSize + kVNetMaxPacket);
        postRxBuffer(i); // addDescChain's cyclic head index == i here (fresh queue)
    }
    return true;
}

bool IOVirtIONet::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;

    fPCIDevice->retain();
    if (!fPCIDevice->open(this)) {
        IOLog("IOVirtIONet: failed to open PCI device\n");
        return false;
    }

    if (!fTransport.attach(fPCIDevice)) {
        IOLog("IOVirtIONet: failed to find/map virtio-pci capabilities\n");
        return false;
    }
    if (!fTransport.negotiateFeatures(VIRTIO_NET_F_MAC)) {
        IOLog("IOVirtIONet: device rejected feature negotiation\n");
        return false;
    }

    if (!readMACAddress()) {
        IOLog("IOVirtIONet: failed to read MAC from device config\n");
        return false;
    }
    IOLog("IOVirtIONet: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
        fMACAddress.bytes[0], fMACAddress.bytes[1], fMACAddress.bytes[2],
        fMACAddress.bytes[3], fMACAddress.bytes[4], fMACAddress.bytes[5]);

    if (!fTransport.initQueue(&fRxQ, 0, kVNetRxCount) ||
        !fTransport.initQueue(&fTxQ, 1, kVNetTxCount)) {
        IOLog("IOVirtIONet: failed to init virtqueues\n");
        return false;
    }
    for (int i = 0; i < kVNetTxCount; i++) {
        fTxBuf[i] = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
            kVNetHdrSize + kVNetMaxPacket, 0xFFFFFFFFULL);
        if (!fTxBuf[i])
            return false;
    }
    if (!initRxRing()) {
        IOLog("IOVirtIONet: failed to allocate RX ring\n");
        return false;
    }

    fTransport.setDriverOk();
    fTransport.notify(&fRxQ); // kick the device to start filling posted RX buffers

    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
        return false;

    fPollTimer = IOTimerEventSource::timerEventSource(this, &IOVirtIONet::pollTimerAction);
    if (!fPollTimer || fWorkLoop->addEventSource(fPollTimer) != kIOReturnSuccess) {
        IOLog("IOVirtIONet: failed to create poll timer\n");
        return false;
    }

    if (!publishLinkMedium())
        return false;

    if (!attachInterface((IONetworkInterface **)&fInterface, true)) {
        IOLog("IOVirtIONet: attachInterface failed\n");
        return false;
    }
    setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, getCurrentMedium());

    registerService();
    return true;
}

void IOVirtIONet::stop(IOService *provider)
{
    if (fEnabled)
        disable(fInterface);

    if (fInterface) {
        detachInterface(fInterface, true);
        fInterface->release();
        fInterface = NULL;
    }
    if (fWorkLoop && fPollTimer)
        fWorkLoop->removeEventSource(fPollTimer);

    super::stop(provider);
}

void IOVirtIONet::free()
{
    for (int i = 0; i < kVNetRxCount; i++)
        if (fRxBuf[i]) { fRxBuf[i]->release(); fRxBuf[i] = NULL; }
    for (int i = 0; i < kVNetTxCount; i++)
        if (fTxBuf[i]) { fTxBuf[i]->release(); fTxBuf[i] = NULL; }

    fTransport.freeQueue(&fRxQ);
    fTransport.freeQueue(&fTxQ);
    fTransport.detach();

    if (fPollTimer) { fPollTimer->release(); fPollTimer = NULL; }

    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
        fPCIDevice = NULL;
    }

    super::free();
}

bool IOVirtIONet::publishLinkMedium()
{
    OSDictionary *mediumDict = OSDictionary::withCapacity(2);
    IONetworkMedium *autoMedium = NULL;
    IONetworkMedium *gigMedium = NULL;
    bool ok = false;

    if (!mediumDict)
        return false;

    autoMedium = IONetworkMedium::medium(
        kIOMediumEthernetAuto | kIOMediumOptionFullDuplex, 1000000000ULL);
    gigMedium = IONetworkMedium::medium(
        kIOMediumEthernet1000BaseT | kIOMediumOptionFullDuplex, 1000000000ULL);

    if (autoMedium && gigMedium &&
        IONetworkMedium::addMedium(mediumDict, autoMedium) &&
        IONetworkMedium::addMedium(mediumDict, gigMedium) &&
        publishMediumDictionary(mediumDict) &&
        setCurrentMedium(autoMedium)) {
        ok = true;
    }

    if (gigMedium) gigMedium->release();
    if (autoMedium) autoMedium->release();
    mediumDict->release();

    if (!ok)
        IOLog("IOVirtIONet: failed to publish link medium\n");
    return ok;
}

IOReturn IOVirtIONet::enable(IONetworkInterface *interface)
{
    if (fEnabled)
        return kIOReturnSuccess;
    IOLog("IOVirtIONet: enable interface=%p\n", interface);
    if (fPollTimer)
        fPollTimer->setTimeoutMS(5);
    fEnabled = true;
    return kIOReturnSuccess;
}

IOReturn IOVirtIONet::disable(IONetworkInterface *interface)
{
    if (!fEnabled)
        return kIOReturnSuccess;
    IOLog("IOVirtIONet: disable interface=%p\n", interface);
    if (fPollTimer)
        fPollTimer->cancelTimeout();
    fEnabled = false;
    return kIOReturnSuccess;
}

void IOVirtIONet::pollTimerAction(OSObject *owner, IOTimerEventSource *sender)
{
    IOVirtIONet *self = OSDynamicCast(IOVirtIONet, owner);
    if (!self)
        return;
    self->pollReceive();
    if (self->fEnabled)
        sender->setTimeoutMS(5);
}

void IOVirtIONet::pollReceive()
{
    bool reposted = false;
    uint32_t len; uint16_t id;

    while (fTransport.pollForCompletion(&fRxQ, 0, &len, &id)) {
        if (id < kVNetRxCount && len > kVNetHdrSize && len <= kVNetHdrSize + kVNetMaxPacket) {
            size_t pktLen = len - kVNetHdrSize;
            uint8_t *payload = (uint8_t *)fRxBuf[id]->getBytesNoCopy() + kVNetHdrSize;

            mbuf_t m = allocatePacket((UInt32)pktLen);
            if (m) {
                if (mbuf_copyback(m, 0, pktLen, payload, MBUF_WAITOK) == 0) {
                    mbuf_pkthdr_setlen(m, pktLen);
                    mbuf_setlen(m, pktLen);
                }
                fRxPackets++;
                if (fInterface)
                    fInterface->inputPacket(m, (UInt32)pktLen, IONetworkInterface::kInputOptionQueuePacket);
            }
        }
        if (id < kVNetRxCount)
            postRxBuffer(id); // hand the same buffer back to the device
        reposted = true;
    }

    // Also reclaim any finished TX slots so outputPacket() doesn't stall
    // waiting for a free one under sustained send load.
    while (fTransport.pollForCompletion(&fTxQ, 0, 0, 0)) {
        if (fTxOutstanding) fTxOutstanding--;
    }

    if (reposted)
        fTransport.notify(&fRxQ);
    if (fInterface)
        fInterface->flushInputQueue();
}

UInt32 IOVirtIONet::outputPacket(mbuf_t m, void *param)
{
    size_t pktLen = mbuf_pkthdr_len(m);
    if (pktLen == 0 || pktLen > kVNetMaxPacket) {
        freePacket(m);
        return kIOReturnOutputDropped;
    }

    if (fTxOutstanding >= kVNetTxCount) {
        // Ring full - give the device a brief window to drain before
        // dropping (no backpressure signaling to the output queue yet).
        uint32_t len; uint16_t id;
        if (fTransport.pollForCompletion(&fTxQ, 50, &len, &id) && fTxOutstanding)
            fTxOutstanding--;
        if (fTxOutstanding >= kVNetTxCount) {
            freePacket(m);
            return kIOReturnOutputDropped;
        }
    }

    unsigned slot = fTxNextSlot;
    uint8_t *buf = (uint8_t *)fTxBuf[slot]->getBytesNoCopy();
    bzero(buf, kVNetHdrSize); // zeroed virtio_net_hdr_v1: no offload/GSO
    mbuf_copydata(m, 0, pktLen, buf + kVNetHdrSize);

    VirtIOChainEntry entry = { fTxBuf[slot]->getPhysicalAddress(),
                              (uint32_t)(kVNetHdrSize + pktLen), false };
    fTransport.addDescChain(&fTxQ, &entry, 1);
    fTransport.notify(&fTxQ);

    fTxOutstanding++;
    fTxNextSlot = (fTxNextSlot + 1) % kVNetTxCount;
    fTxPackets++;

    freePacket(m);
    return kIOReturnOutputSuccess;
}

IOReturn IOVirtIONet::getHardwareAddress(IOEthernetAddress *addr)
{
    if (!addr)
        return kIOReturnBadArgument;
    *addr = fMACAddress;
    return kIOReturnSuccess;
}

IOReturn IOVirtIONet::setPromiscuousMode(bool active)
{
    // No VIRTIO_NET_F_CTRL_VQ/CTRL_RX negotiated - promiscuous/multicast
    // filtering isn't controllable; the device already delivers what it
    // sees on QEMU's default usermode/tap backends.
    return kIOReturnSuccess;
}

IOReturn IOVirtIONet::setMulticastMode(bool active)
{
    return kIOReturnSuccess;
}

IOReturn IOVirtIONet::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
    return kIOReturnSuccess;
}

const OSString *IOVirtIONet::newVendorString() const
{
    return OSString::withCString("Red Hat / Virtio");
}

const OSString *IOVirtIONet::newModelString() const
{
    return OSString::withCString("virtio-net (IOVirtIONet)");
}

IOOutputQueue *IOVirtIONet::createOutputQueue()
{
    // Matches PDE1000: this tree's IONetworkController doesn't start the
    // optional IOOutputQueue during doEnable(), so use the legacy direct
    // output handler until the generic queue lifecycle is wired.
    return NULL;
}
