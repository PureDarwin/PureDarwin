/*
 * IOVirtIONet.cpp - virtio-net Ethernet over IOVirtIOTransport (VIRTIO 1.1,
 * section 5.1).
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
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

    // completions are polled, and an unacknowledged intx would hold a shared level line high
    fPCIDevice->configWrite16(kIOPCIConfigCommand,
        fPCIDevice->configRead16(kIOPCIConfigCommand) | kIOPCICommandInterruptDisable);

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
    fTxLock = IOSimpleLockAlloc();
    if (!fTxLock)
        return false;
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

    // kdp_match_name=en0 picks this interface for KDP and the network log
    if (!attachDebuggerClient(&fDebugger))
        IOLog("IOVirtIONet: no debugger client\n");

    registerService();
    return true;
}

void IOVirtIONet::stop(IOService *provider)
{
    if (fEnabled)
        disable(fInterface);

    if (fDebugger) {
        detachDebuggerClient(fDebugger);
        fDebugger = NULL;
    }
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
    if (fTxLock) { IOSimpleLockFree(fTxLock); fTxLock = NULL; }

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
    IOInterruptState is = IOSimpleLockLockDisableInterrupt(fTxLock);
    while (fTransport.pollForCompletion(&fTxQ, 0, 0, 0)) {
        if (fTxOutstanding) fTxOutstanding--;
    }
    IOSimpleLockUnlockEnableInterrupt(fTxLock, is);

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

    uint8_t frame[kVNetMaxPacket];
    mbuf_copydata(m, 0, pktLen, frame);
    freePacket(m);

    IOInterruptState is = IOSimpleLockLockDisableInterrupt(fTxLock);
    UInt32 ret = queueTx(frame, pktLen);
    IOSimpleLockUnlockEnableInterrupt(fTxLock, is);
    return ret;
}

// one frame onto the tx ring, the lock held; a full ring gets a bounded spin to drain, never a sleep
UInt32 IOVirtIONet::queueTx(const void *data, size_t len)
{
    for (unsigned spins = 0; ; spins++) {
        while (fTransport.pollForCompletion(&fTxQ, 0, 0, 0))
            if (fTxOutstanding) fTxOutstanding--;
        if (fTxOutstanding < kVNetTxCount || spins >= 50000)
            break;
        IODelay(1);
    }
    if (fTxOutstanding >= kVNetTxCount)
        return kIOReturnOutputDropped;

    unsigned slot = fTxNextSlot;
    uint8_t *buf = (uint8_t *)fTxBuf[slot]->getBytesNoCopy();
    bzero(buf, kVNetHdrSize); // zeroed virtio_net_hdr_v1: no offload/GSO
    bcopy(data, buf + kVNetHdrSize, len);

    VirtIOChainEntry entry = { fTxBuf[slot]->getPhysicalAddress(), (uint32_t)(kVNetHdrSize + len), false };
    fTransport.addDescChain(&fTxQ, &entry, 1);
    fTransport.notify(&fTxQ);

    fTxOutstanding++;
    fTxNextSlot = (fTxNextSlot + 1) % kVNetTxCount;
    fTxPackets++;
    return kIOReturnOutputSuccess;
}

// the debugger's transmit: reclaim slots, send, and spin until the frame is out so its buffer is free
void IOVirtIONet::sendPacket(void *pkt, UInt32 pktSize)
{
    if (!pkt || pktSize == 0 || pktSize > kVNetMaxPacket || !fTxLock)
        return;

    // a lock held by a cpu the debugger stopped would never come free: drop the frame instead
    if (!IOSimpleLockTryLock(fTxLock))
        return;
    queueTx(pkt, pktSize);
    IOSimpleLockUnlock(fTxLock);
}

// the debugger's receive: one frame within timeout ms, its buffer handed straight back to the device
void IOVirtIONet::receivePacket(void *pkt, UInt32 *pktSize, UInt32 timeout)
{
    uint64_t waitedUs = 0;

    *pktSize = 0;
    for (;;) {
        uint32_t len;
        uint16_t id;
        if (fTransport.pollForCompletion(&fRxQ, 0, &len, &id)) {
            if (id < kVNetRxCount && len > kVNetHdrSize && len <= kVNetHdrSize + kVNetMaxPacket) {
                UInt32 n = (UInt32)(len - kVNetHdrSize);
                bcopy((uint8_t *)fRxBuf[id]->getBytesNoCopy() + kVNetHdrSize, pkt, n);
                *pktSize = n;
            }
            if (id < kVNetRxCount)
                postRxBuffer(id);
            fTransport.notify(&fRxQ);
            if (*pktSize)
                return;
            continue;
        }
        if (waitedUs >= (uint64_t)timeout * 1000)
            return;
        IODelay(10);
        waitedUs += 10;
    }
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
