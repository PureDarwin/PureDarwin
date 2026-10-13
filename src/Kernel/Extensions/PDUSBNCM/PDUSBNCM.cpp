/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// USB CDC NCM Ethernet, written from the USB-IF CDC 1.2 and NCM 1.0 specifications

#include "PDUSBNCM.h"

#include <IOKit/IOLib.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/network/IOOutputQueue.h>

extern "C" {
#include <sys/kpi_mbuf.h>
}

OSDefineMetaClassAndStructors(PDUSBNCMConfig, IOService);
OSDefineMetaClassAndStructors(PDUSBNCM, IOEthernetController);

enum {
    kDescInterface      = 4,
    kDescCSInterface    = 0x24,
    kSubtypeUnion       = 0x06,
    kSubtypeEthernet    = 0x0f,
    kClassComm          = 0x02,
    kClassData          = 0x0a,
    kSubclassNCM        = 0x0d,
};

// class requests
enum {
    kReqSetPacketFilter = 0x43,
    kReqGetNtbParams    = 0x80,
    kReqSetNtbInputSize = 0x86,
};

// SET_ETHERNET_PACKET_FILTER bits
enum {
    kFilterPromisc      = 1u << 0,
    kFilterAllMulti     = 1u << 1,
    kFilterDirected     = 1u << 2,
    kFilterBroadcast    = 1u << 3,
};

// notifications on the interrupt endpoint
enum {
    kNotifyConnection   = 0x00,
    kNotifySpeed        = 0x2a,
};

#define kNth16Sig       0x484d434e      // "NCMH"
#define kNdp16Sig       0x304d434e      // "NCM0", no CRC
#define kNthLen         12
#define kNdpLen         16              // header plus one datagram and the terminating entry
#define kMaxNtb         32768

static inline UInt16 le16(const UInt8 *p) { return (UInt16)(p[0] | (p[1] << 8)); }
static inline UInt32 le32(const UInt8 *p) { return (UInt32)p[0] | ((UInt32)p[1] << 8) | ((UInt32)p[2] << 16) | ((UInt32)p[3] << 24); }
static inline void put16(UInt8 *p, UInt16 v) { p[0] = (UInt8)v; p[1] = (UInt8)(v >> 8); }
static inline void put32(UInt8 *p, UInt32 v) { put16(p, (UInt16)v); put16(p + 2, (UInt16)(v >> 16)); }
static uint64_t deadline(uint32_t ms);

// the configuration value whose descriptor has an NCM communication interface, 0 when none does
static UInt8 ncmConfigValue(IOUSBDevice *dev)
{
    for (UInt8 i = 0; i < dev->GetNumConfigurations(); i++) {
        const UInt8 *cd = (const UInt8 *)dev->GetFullConfigurationDescriptor(i);
        if (cd == NULL)
            continue;
        UInt16 total = le16(cd + 2);
        for (UInt16 off = 0; off + 2 <= total && cd[off] != 0; off += cd[off]) {
            if (cd[off + 1] == kDescInterface && off + 7 <= total && cd[off + 5] == kClassComm && cd[off + 6] == kSubclassNCM)
                return cd[5];
        }
    }
    return 0;
}

IOService *
PDUSBNCMConfig::probe(IOService *provider, SInt32 *score)
{
    IOUSBDevice *dev = OSDynamicCast(IOUSBDevice, provider);

    if (dev == NULL || (fConfigValue = ncmConfigValue(dev)) == 0)
        return NULL;
    return IOService::probe(provider, score);
}

bool
PDUSBNCMConfig::start(IOService *provider)
{
    IOUSBDevice *dev = OSDynamicCast(IOUSBDevice, provider);

    if (dev == NULL || !IOService::start(provider) || !dev->open(this))
        return false;
    IOReturn r = dev->SetConfiguration(this, fConfigValue, true);
    IOLog("PDUSBNCM: %04x:%04x configuration %u for NCM: 0x%x\n", dev->GetVendorID(), dev->GetProductID(),
          fConfigValue, r);
    dev->close(this);
    return r == kIOReturnSuccess;
}

bool
PDUSBNCM::createWorkLoop(void)
{
    fWorkLoop = IOWorkLoop::workLoop();
    return fWorkLoop != NULL;
}

IOWorkLoop *
PDUSBNCM::getWorkLoop(void) const
{
    return fWorkLoop;
}

IOOutputQueue *
PDUSBNCM::createOutputQueue(void)
{
    // the direct output handler, as PDRealtek8111 and PDE1000 use
    return NULL;
}

const OSString *
PDUSBNCM::newVendorString(void) const
{
    return OSString::withCString("USB");
}

const OSString *
PDUSBNCM::newModelString(void) const
{
    return OSString::withCString("CDC NCM Ethernet");
}

IOReturn
PDUSBNCM::classRequest(UInt8 dirIn, UInt8 request, UInt16 value, void *data, UInt16 len)
{
    IOUSBDevRequest r;

    r.bmRequestType = dirIn ? 0xa1 : 0x21;
    r.bRequest = request;
    r.wValue = value;
    r.wIndex = fControl->GetInterfaceNumber();
    r.wLength = len;
    r.pData = data;
    r.wLenDone = 0;
    return fDevice->DeviceRequest(&r);
}

// the data interface named by the union descriptor, set to the alternate setting that has endpoints
bool
PDUSBNCM::findDataInterface(void)
{
    const UInt8 *d = NULL;
    UInt8 dataNum = 0xff;

    while ((d = (const UInt8 *)fControl->FindNextAssociatedDescriptor(d, kDescCSInterface)) != NULL) {
        if (d[2] == kSubtypeUnion && d[0] >= 5) {
            dataNum = d[4];
            break;
        }
    }
    if (dataNum == 0xff)
        dataNum = fControl->GetInterfaceNumber() + 1;

    // the interfaces of a configuration are published one by one, so the data interface may still be on its way
    IOUSBInterface *it = NULL;
    for (int tries = 0; tries < 50 && it == NULL; tries++) {
        OSIterator *iter = fDevice->getChildIterator(gIOServicePlane);
        OSObject *o;
        while (iter != NULL && (o = iter->getNextObject()) != NULL) {
            IOUSBInterface *c = OSDynamicCast(IOUSBInterface, o);
            if (c != NULL && c->GetInterfaceNumber() == dataNum) {
                it = c;
                break;
            }
        }
        OSSafeReleaseNULL(iter);
        if (it == NULL)
            IOSleep(100);
    }
    if (it == NULL) {
        IOLog("PDUSBNCM: no data interface %u\n", dataNum);
        return false;
    }
    fData = it;
    fData->retain();
    if (!fData->open(this)) {
        IOLog("PDUSBNCM: cannot open the data interface\n");
        return false;
    }
    // alternate setting 0 has no endpoints, the next one carries traffic
    fDataAlt = 1;
    IOReturn r = fData->SetAlternateInterface(this, fDataAlt);
    if (r != kIOReturnSuccess) {
        IOLog("PDUSBNCM: alternate setting %u failed: 0x%x\n", fDataAlt, r);
        return false;
    }

    for (UInt8 i = 0; i < 32; i++) {
        IOUSBPipe *p = fData->GetPipeObj(i);
        const IOUSBEndpointDescriptor *ed = p ? p->GetEndpointDescriptor() : NULL;
        if (ed == NULL || (ed->bmAttributes & 3) != kUSBBulk)
            continue;
        if (ed->bEndpointAddress & 0x80)
            fBulkIn = fBulkIn ? fBulkIn : p;
        else
            fBulkOut = fBulkOut ? fBulkOut : p;
    }
    for (UInt8 i = 0; i < 32; i++) {
        IOUSBPipe *p = fControl->GetPipeObj(i);
        const IOUSBEndpointDescriptor *ed = p ? p->GetEndpointDescriptor() : NULL;
        if (ed != NULL && (ed->bmAttributes & 3) == kUSBInterrupt && (ed->bEndpointAddress & 0x80)) {
            fIntrIn = p;
            break;
        }
    }
    return fBulkIn != NULL && fBulkOut != NULL;
}

// the MAC is a string descriptor of twelve hex digits, named by the Ethernet functional descriptor
bool
PDUSBNCM::readMACAddress(void)
{
    const UInt8 *d = NULL;
    char s[32];

    while ((d = (const UInt8 *)fControl->FindNextAssociatedDescriptor(d, kDescCSInterface)) != NULL) {
        if (d[2] == kSubtypeEthernet && d[0] >= 4)
            break;
    }
    if (d == NULL || d[3] == 0 || fDevice->GetStringDescriptor(d[3], s, sizeof(s)) != kIOReturnSuccess)
        return false;
    for (int i = 0; i < 6; i++) {
        UInt8 b = 0;
        for (int j = 0; j < 2; j++) {
            char c = s[i * 2 + j];
            UInt8 v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                    : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 0xff;
            if (v == 0xff)
                return false;
            b = (UInt8)(b << 4 | v);
        }
        fMac.bytes[i] = b;
    }
    return true;
}

bool
PDUSBNCM::readNtbParameters(void)
{
    UInt8 p[28];

    bzero(p, sizeof(p));
    if (classRequest(1, kReqGetNtbParams, 0, p, sizeof(p)) != kIOReturnSuccess) {
        IOLog("PDUSBNCM: GET_NTB_PARAMETERS failed\n");
        return false;
    }
    fNtbInMax = le32(p + 4);
    fOutDivisor = le16(p + 18);
    fOutRemainder = le16(p + 20);
    fOutAlign = le16(p + 22);
    fNtbOutMax = le32(p + 16);
    if (fOutDivisor == 0)
        fOutDivisor = 4;
    if (fNtbInMax > kMaxNtb) {
        UInt32 want = kMaxNtb;
        UInt8 b[4];
        put32(b, want);
        if (classRequest(0, kReqSetNtbInputSize, 0, b, sizeof(b)) == kIOReturnSuccess)
            fNtbInMax = want;
    }
    if (fNtbOutMax > kMaxNtb)
        fNtbOutMax = kMaxNtb;
    IOLog("PDUSBNCM: NTB in %u out %u, out divisor %u remainder %u\n", fNtbInMax, fNtbOutMax, fOutDivisor,
          fOutRemainder);
    return fNtbInMax >= 2048 && fNtbOutMax >= 2048;
}

void
PDUSBNCM::publishMedia(void)
{
    OSDictionary *dict = OSDictionary::withCapacity(1);
    IONetworkMedium *m = IONetworkMedium::medium(kIOMediumEthernetAuto, 0);

    if (dict && m && IONetworkMedium::addMedium(dict, m))
        publishMediumDictionary(dict);
    OSSafeReleaseNULL(m);
    OSSafeReleaseNULL(dict);
}

bool
PDUSBNCM::start(IOService *provider)
{
    if (!IOEthernetController::start(provider))
        return false;
    fControl = OSDynamicCast(IOUSBInterface, provider);
    if (fControl == NULL || !fControl->open(this))
        return false;
    fDevice = fControl->GetDevice();

    if (!findDataInterface() || !readNtbParameters())
        return false;
    if (!readMACAddress()) {
        // a locally administered address so the interface still comes up
        static const UInt8 fallback[6] = { 0x02, 0x50, 0x44, 0x4e, 0x43, 0x4d };
        bcopy(fallback, fMac.bytes, 6);
        IOLog("PDUSBNCM: no MAC string, using a local address\n");
    }
    IOLog("PDUSBNCM: %04x:%04x MAC %02x:%02x:%02x:%02x:%02x:%02x\n", fDevice->GetVendorID(), fDevice->GetProductID(),
          fMac.bytes[0], fMac.bytes[1], fMac.bytes[2], fMac.bytes[3], fMac.bytes[4], fMac.bytes[5]);

    for (int i = 0; i < kRxBuffers; i++) {
        fRx[i].owner = this;
        fRx[i].mem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionIn, fNtbInMax);
        fRx[i].again = thread_call_allocate(&PDUSBNCM::resubmit, this);
        if (fRx[i].mem == NULL || fRx[i].mem->prepare() != kIOReturnSuccess)
            return false;
    }
    for (int i = 0; i < kTxBuffers; i++) {
        fTx[i].owner = this;
        fTx[i].mem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionOut, fNtbOutMax);
        if (fTx[i].mem == NULL || fTx[i].mem->prepare() != kIOReturnSuccess)
            return false;
    }
    fStatus.owner = this;
    fStatus.mem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionIn, 64);
    fStatus.again = thread_call_allocate(&PDUSBNCM::resubmit, this);
    if (fStatus.mem == NULL || fStatus.mem->prepare() != kIOReturnSuccess)
        return false;

    fFilter = kFilterDirected | kFilterBroadcast | kFilterAllMulti;
    publishMedia();
    if (!attachInterface((IONetworkInterface **)&fNetif, true))
        return false;
    registerService();
    return true;
}

void
PDUSBNCM::stop(IOService *provider)
{
    disable(fNetif);
    if (fNetif) {
        detachInterface(fNetif, true);
        fNetif = NULL;
    }
    if (fData && fData->isOpen(this))
        fData->close(this);
    if (fControl && fControl->isOpen(this))
        fControl->close(this);
    IOEthernetController::stop(provider);
}

void
PDUSBNCM::free(void)
{
    for (int i = 0; i < kRxBuffers; i++) {
        if (fRx[i].mem) {
            fRx[i].mem->complete();
            OSSafeReleaseNULL(fRx[i].mem);
        }
    }
    for (int i = 0; i < kTxBuffers; i++) {
        if (fTx[i].mem) {
            fTx[i].mem->complete();
            OSSafeReleaseNULL(fTx[i].mem);
        }
    }
    for (int i = 0; i < kRxBuffers; i++) {
        if (fRx[i].again) {
            thread_call_cancel_wait(fRx[i].again);
            thread_call_free(fRx[i].again);
        }
    }
    if (fStatus.again) {
        thread_call_cancel_wait(fStatus.again);
        thread_call_free(fStatus.again);
    }
    if (fStatus.mem) {
        fStatus.mem->complete();
        OSSafeReleaseNULL(fStatus.mem);
    }
    OSSafeReleaseNULL(fData);
    OSSafeReleaseNULL(fWorkLoop);
    IOEthernetController::free();
}

void
PDUSBNCM::setFilter(void)
{
    IOReturn r = classRequest(0, kReqSetPacketFilter, fFilter, NULL, 0);
    if (r != kIOReturnSuccess)
        IOLog("PDUSBNCM: packet filter 0x%x: 0x%x\n", fFilter, r);
}

IOReturn
PDUSBNCM::enable(IONetworkInterface *)
{
    if (fRunning)
        return kIOReturnSuccess;
    fRunning = true;
    setFilter();
    for (int i = 0; i < kRxBuffers; i++)
        submitRx(&fRx[i]);
    submitStatus();
    // not every device sends a connection notification after the filter is set, so start as up
    fLinkUp = true;
    setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, NULL, fSpeed);
    return kIOReturnSuccess;
}

IOReturn
PDUSBNCM::disable(IONetworkInterface *)
{
    if (!fRunning)
        return kIOReturnSuccess;
    fRunning = false;
    if (fBulkIn)
        fBulkIn->Abort();
    if (fBulkOut)
        fBulkOut->Abort();
    if (fIntrIn)
        fIntrIn->Abort();
    setLinkStatus(kIONetworkLinkValid, NULL, 0);
    return kIOReturnSuccess;
}

IOReturn
PDUSBNCM::getHardwareAddress(IOEthernetAddress *addr)
{
    bcopy(fMac.bytes, addr->bytes, 6);
    return kIOReturnSuccess;
}

IOReturn
PDUSBNCM::setPromiscuousMode(bool active)
{
    fFilter = active ? (fFilter | kFilterPromisc) : (fFilter & ~kFilterPromisc);
    if (fRunning)
        setFilter();
    return kIOReturnSuccess;
}

// all multicast stays on, the device has no filter list worth programming
IOReturn
PDUSBNCM::setMulticastMode(bool)
{
    return kIOReturnSuccess;
}

IOReturn
PDUSBNCM::setMulticastList(IOEthernetAddress *, UInt32)
{
    return kIOReturnSuccess;
}

void
PDUSBNCM::submitRx(Xfer *x)
{
    if (!fRunning)
        return;
    x->completion.target = x;
    x->completion.action = &PDUSBNCM::rxDone;
    x->completion.parameter = NULL;
    x->mem->setLength(fNtbInMax);
    x->busy = true;
    IOReturn r = fBulkIn->Read(x->mem, 0, 0, fNtbInMax, &x->completion);
    if (r != kIOReturnSuccess) {
        x->busy = false;
        IOLog("PDUSBNCM: rx submit 0x%x\n", r);
    }
}

void
PDUSBNCM::rxDone(void *target, void *, IOReturn status, UInt32 remaining)
{
    Xfer *x = (Xfer *)target;
    PDUSBNCM *me = x->owner;

    x->busy = false;
    if (!me->fRunning || status == kIOReturnAborted)
        return;
    if (status == kIOReturnSuccess && remaining < me->fNtbInMax)
        me->parseNtb((const UInt8 *)x->mem->getBytesNoCopy(), me->fNtbInMax - remaining);
    else if (status == kIOUSBPipeStalled)
        me->fBulkIn->ClearPipeStall(true);
    // a completion can run inside Read(), so the next read goes out from a thread, later after an error
    if (status == kIOReturnSuccess)
        thread_call_enter1(x->again, x);
    else
        thread_call_enter1_delayed(x->again, x, deadline(10));
}

// an NTB16: a header, then one or more datagram pointer tables, each listing datagrams until a zero entry
void
PDUSBNCM::parseNtb(const UInt8 *buf, UInt32 len)
{
    if (len < kNthLen || le32(buf) != kNth16Sig)
        return;
    UInt32 ndp = le16(buf + 10);
    int tables = 0;

    while (ndp != 0 && ndp + 8 <= len && tables++ < 8) {
        const UInt8 *t = buf + ndp;
        if ((le32(t) & 0x00ffffff) != (kNdp16Sig & 0x00ffffff))
            break;
        UInt32 tlen = le16(t + 4);
        for (UInt32 e = 8; e + 4 <= tlen && ndp + e + 4 <= len; e += 4) {
            UInt32 off = le16(t + e), dlen = le16(t + e + 2);
            if (off == 0 || dlen == 0)
                break;
            if (off + dlen > len || dlen < 14)
                continue;
            mbuf_t m = allocatePacket(dlen);
            if (m == NULL)
                continue;
            mbuf_copyback(m, 0, dlen, buf + off, MBUF_DONTWAIT);
            fNetif->inputPacket(m, dlen, IONetworkInterface::kInputOptionQueuePacket);
        }
        ndp = le16(t + 6);
    }
    fNetif->flushInputQueue();
}

static uint64_t
deadline(uint32_t ms)
{
    uint64_t d;
    clock_interval_to_deadline(ms, kMillisecondScale, &d);
    return d;
}

void
PDUSBNCM::resubmit(thread_call_param_t self, thread_call_param_t xfer)
{
    PDUSBNCM *me = (PDUSBNCM *)self;
    Xfer *x = (Xfer *)xfer;

    if (x == &me->fStatus)
        me->submitStatus();
    else
        me->submitRx(x);
}

void
PDUSBNCM::submitStatus(void)
{
    if (!fRunning || fIntrIn == NULL)
        return;
    fStatus.completion.target = &fStatus;
    fStatus.completion.action = &PDUSBNCM::statusDone;
    fStatus.completion.parameter = NULL;
    fStatus.mem->setLength(64);
    if (fIntrIn->Read(fStatus.mem, 0, 0, 64, &fStatus.completion) == kIOReturnSuccess)
        fStatus.busy = true;
}

void
PDUSBNCM::statusDone(void *target, void *, IOReturn status, UInt32 remaining)
{
    Xfer *x = (Xfer *)target;
    PDUSBNCM *me = x->owner;

    x->busy = false;
    if (!me->fRunning || status == kIOReturnAborted)
        return;
    if (status == kIOReturnSuccess && remaining < 64)
        me->handleStatus((const UInt8 *)x->mem->getBytesNoCopy(), 64 - remaining);
    else if (status == kIOUSBPipeStalled)
        me->fIntrIn->ClearPipeStall(true);
    if (status == kIOReturnSuccess)
        thread_call_enter1(x->again, x);
    else
        thread_call_enter1_delayed(x->again, x, deadline(100));
}

void
PDUSBNCM::handleStatus(const UInt8 *b, UInt32 len)
{
    if (len < 8 || b[0] != 0xa1)
        return;
    // devices repeat these every few seconds, only a change is reported
    if (b[1] == kNotifyConnection) {
        bool up = le16(b + 2) != 0;
        if (up == fLinkUp)
            return;
        fLinkUp = up;
        IOLog("PDUSBNCM: link %s\n", fLinkUp ? "up" : "down");
    } else if (b[1] == kNotifySpeed && len >= 16) {
        UInt64 speed = le32(b + 8);
        if (speed == fSpeed)
            return;
        fSpeed = speed;
        IOLog("PDUSBNCM: %llu Mb/s\n", fSpeed / 1000000);
    } else {
        return;
    }
    setLinkStatus(fLinkUp ? (kIONetworkLinkValid | kIONetworkLinkActive) : kIONetworkLinkValid, NULL,
                  fLinkUp ? fSpeed : 0);
}

void
PDUSBNCM::txDone(void *target, void *, IOReturn status, UInt32)
{
    Xfer *x = (Xfer *)target;

    if (status == kIOUSBPipeStalled)
        x->owner->fBulkOut->ClearPipeStall(true);
    x->busy = false;
}

// one datagram per NTB, which every NCM device accepts
UInt32
PDUSBNCM::outputPacket(mbuf_t m, void *)
{
    size_t len = mbuf_pkthdr_len(m);
    Xfer *x = NULL;

    if (!fRunning) {
        freePacket(m);
        return kIOReturnOutputDropped;
    }
    for (int i = 0; i < kTxBuffers; i++) {
        if (!fTx[i].busy) {
            x = &fTx[i];
            break;
        }
    }
    UInt32 off = kNthLen + kNdpLen;
    while ((off % fOutDivisor) != fOutRemainder % fOutDivisor)
        off++;
    UInt32 total = off + (UInt32)len;
    if (x == NULL || total > fNtbOutMax) {
        freePacket(m);
        return kIOReturnOutputDropped;
    }
    // a transfer of whole packets needs a zero-length packet to end it, a padding byte avoids that
    UInt16 mps = fBulkOut->GetMaxPacketSize();
    if (mps != 0 && (total % mps) == 0 && total < fNtbOutMax)
        total++;

    UInt8 *b = (UInt8 *)x->mem->getBytesNoCopy();
    bzero(b, off);
    put32(b, kNth16Sig);
    put16(b + 4, kNthLen);
    put16(b + 6, fTxSeq++);
    put16(b + 8, (UInt16)total);
    put16(b + 10, kNthLen);
    put32(b + kNthLen, kNdp16Sig);
    put16(b + kNthLen + 4, kNdpLen);
    put16(b + kNthLen + 6, 0);
    put16(b + kNthLen + 8, (UInt16)off);
    put16(b + kNthLen + 10, (UInt16)len);
    mbuf_copydata(m, 0, len, b + off);
    if (total > off + len)
        b[total - 1] = 0;
    freePacket(m);

    x->completion.target = x;
    x->completion.action = &PDUSBNCM::txDone;
    x->completion.parameter = NULL;
    x->mem->setLength(total);
    x->busy = true;
    IOReturn r = fBulkOut->Write(x->mem, 0, 2000, total, &x->completion);
    if (r != kIOReturnSuccess) {
        x->busy = false;
        return kIOReturnOutputDropped;
    }
    return kIOReturnOutputSuccess;
}
