/*
 * AppleUSBOHCI - a real (if minimal) OHCI host controller driver for the
 * reconstructed PureDarwin IOUSBFamily.
 *
 * This is not Apple's original UIM (that shipped as 0-byte files). It drives
 * the OHCI hardware directly and satisfies the reconstructed IOUSBController's
 * simple synchronous UIM contract (UIMDeviceRequest / UIMOpenPipe /
 * UIMReadWrite), the same interface RavynXHCIUSBBus uses for xHCI. The
 * controller owns root-hub enumeration itself: a kernel thread polls the root
 * hub ports and, on connect, resets + addresses the device and hands it to
 * IOUSBController::CreateAndConfigureDevice so the normal
 * IOUSBDevice/IOUSBInterface/IOUSBCompositeDriver stack lights up.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <kern/thread.h>
#include "AppleUSBOHCI.h"

#define super IOUSBControllerV3
OSDefineMetaClassAndStructors(AppleUSBOHCI, IOUSBControllerV3)

#define OHCI_Log(fmt, ...) IOLog("AppleUSBOHCI: " fmt "\n", ##__VA_ARGS__)

// root hub port status bits (OHCI spec, not symbolically in USBOHCI.h)
enum {
    kRhPortCCS  = (1 << 0),   // current connect status
    kRhPortPES  = (1 << 1),   // port enable status
    kRhPortPSS  = (1 << 2),   // suspend
    kRhPortPRS  = (1 << 4),   // port reset status / set-reset
    kRhPortPPS  = (1 << 8),   // port power status / set-power
    kRhPortLSDA = (1 << 9),   // low speed device attached
    kRhPortCSC  = (1 << 16),  // connect status change
    kRhPortPESC = (1 << 17),
    kRhPortPSSC = (1 << 18),
    kRhPortOCIC = (1 << 19),
    kRhPortPRSC = (1 << 20),  // reset status change
};
enum {
    kRhStatusLPSC = (1 << 16), // set global power
};

// file-static controller state (single OHCI instance)
static AppleUSBOHCI *        gOHCI;
static IOBufferMemoryDescriptor * gEDBuf;
static IOBufferMemoryDescriptor * gTDBuf;
static IOBufferMemoryDescriptor * gBounceBuf;
static volatile UInt8 *      gEDVirt;
static UInt32                gEDPhys;
static volatile UInt8 *      gTDVirt;
static UInt32                gTDPhys;
static volatile UInt8 *      gBounceVirt;
static UInt32                gBouncePhys;
static UInt8                 gAddrMaxPkt[128];
static bool                  gAddrLow[128];
static UInt8                 gNextAddr;
static IOUSBDevice *         gPortDevice[16];
static bool                  gEnumRunning;

// OHCI shared descriptor layout (16 bytes each) inside our DMA pages.
#define ED_STRIDE   16
#define TD_STRIDE   16
#define ED_CONTROL  0            // ED index 0 reserved for control transfers
#define ED_INTR_BASE 1           // interrupt EDs start here
#define TD_SETUP    0
#define TD_DATA     1
#define TD_STATUS   2
#define TD_TAIL     3
#define TD_INTR     4            // TDs used for interrupt reads
#define TD_INTR_TAIL 5

#define BOUNCE_SETUP_OFF 0
#define BOUNCE_DATA_OFF  16

static inline void ohciW(volatile UInt32 *r, UInt32 v)
{
    *r = v;
    /* Full barrier after an MMIO write. __sync_synchronize() rather than
     * a bare mfence so this controller also builds for arm64, where
     * EHCI/OHCI are common on-SoC. */
    __sync_synchronize();
}
static inline UInt32 ohciR(volatile UInt32 *r) { return *r; }

static inline volatile UInt32 *edAt(UInt32 idx) { return (volatile UInt32 *)(gEDVirt + idx * ED_STRIDE); }
static inline UInt32           edPhys(UInt32 idx) { return gEDPhys + idx * ED_STRIDE; }
static inline volatile UInt32 *tdAt(UInt32 idx) { return (volatile UInt32 *)(gTDVirt + idx * TD_STRIDE); }
static inline UInt32           tdPhys(UInt32 idx) { return gTDPhys + idx * TD_STRIDE; }

//
// IOKit lifecycle
//

bool AppleUSBOHCI::init(OSDictionary *propTable)
{
    if (!super::init(propTable))
        return false;

    _deviceBase = NULL;
    _barDesc = NULL;
    _pOHCIRegisters = NULL;
    _hccaBuffer = NULL;
    _uimInitialized = false;
    _wdhLock = IOSimpleLockAlloc();
    return _wdhLock != NULL;
}

bool AppleUSBOHCI::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) {
        OHCI_Log("provider is not a PCI device");
        return false;
    }

    pci->setMemoryEnable(true);
    pci->setBusMasterEnable(true);
    _vendorID = pci->configRead16(kIOPCIConfigVendorID);
    _deviceID = pci->configRead16(kIOPCIConfigDeviceID);

    IODeviceMemory *bar0 = pci->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (bar0 && bar0->getLength())
        _deviceBase = bar0->map(kIOMapAnywhere);
    if (!_deviceBase)
        _deviceBase = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (!_deviceBase) {
        /* IOPCIFamily didn't hand us an IODeviceMemory range - read BAR0 out
         * of config space and map its physical window directly (OHCI BAR0 is
         * a 32-bit MMIO region QEMU assigns at POST). */
        UInt32 raw = pci->configRead32(kIOPCIConfigBaseAddress0);
        if (!(raw & 0x1) && raw != 0xFFFFFFFFU && (raw & ~0xFU)) {
            IOPhysicalAddress phys = (IOPhysicalAddress)(raw & ~0xFU);
            _barDesc = IOMemoryDescriptor::withPhysicalAddress(
                phys, 0x1000, kIODirectionNone | kIOMemoryMapperNone);
            if (_barDesc)
                _deviceBase = _barDesc->map(kIOMapAnywhere);
            OHCI_Log("BAR0 config fallback raw=%08x phys=%p map=%p", raw,
                     (void *)phys, _deviceBase);
        }
    }
    if (!_deviceBase) {
        OHCI_Log("failed to map BAR0");
        return false;
    }
    _pOHCIRegisters = (OHCIRegistersPtr)_deviceBase->getVirtualAddress();
    OHCI_Log("pci %04x:%04x BAR0=%p rev=%08x", _vendorID, _deviceID,
             _pOHCIRegisters, ohciR(&_pOHCIRegisters->hcRevision));

    if (UIMInitialize(provider) != kIOReturnSuccess) {
        OHCI_Log("UIMInitialize failed");
        return false;
    }

    gOHCI = this;
    gEnumRunning = true;
    thread_t thread = THREAD_NULL;
    if (kernel_thread_start((thread_continue_t)&AppleUSBOHCI::EnumThreadEntry,
                            this, &thread) == KERN_SUCCESS) {
        thread_deallocate(thread);
    } else {
        OHCI_Log("failed to start enumeration thread");
    }

    registerService();
    OHCI_Log("started");
    return true;
}

void AppleUSBOHCI::stop(IOService *provider)
{
    gEnumRunning = false;
    UIMFinalize();
    super::stop(provider);
}

bool AppleUSBOHCI::finalize(IOOptionBits options)   { return super::finalize(options); }
IOReturn AppleUSBOHCI::message(UInt32 t, IOService *p, void *a) { return super::message(t, p, a); }
void AppleUSBOHCI::powerChangeDone(unsigned long) {}
bool AppleUSBOHCI::willTerminate(IOService *p, IOOptionBits o) { return super::willTerminate(p, o); }
bool AppleUSBOHCI::didTerminate(IOService *p, IOOptionBits o, bool *d) { return super::didTerminate(p, o, d); }

void AppleUSBOHCI::free()
{
    if (_wdhLock) { IOSimpleLockFree(_wdhLock); _wdhLock = NULL; }
    if (_hccaBuffer) { _hccaBuffer->release(); _hccaBuffer = NULL; }
    if (_deviceBase) { _deviceBase->release(); _deviceBase = NULL; }
    if (_barDesc) { _barDesc->release(); _barDesc = NULL; }
    super::free();
}

//
// Hardware bring-up
//

IOReturn AppleUSBOHCI::UIMInitialize(IOService *provider)
{
    OHCIRegistersPtr regs = _pOHCIRegisters;

    // Allocate HCCA (256 bytes, must be 256-aligned; a page satisfies that).
    _hccaBuffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        kOHCIPageSize, 0xFFFFF000ULL);
    if (!_hccaBuffer) return kIOReturnNoMemory;
    bzero(_hccaBuffer->getBytesNoCopy(), kOHCIPageSize);
    _hccaPhysAddr = (IOPhysicalAddress)_hccaBuffer->getPhysicalAddress();

    // Allocate ED / TD / bounce DMA regions (one page each is plenty).
    gEDBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        kOHCIPageSize, 0xFFFFF000ULL);
    gTDBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        kOHCIPageSize, 0xFFFFF000ULL);
    gBounceBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        kOHCIPageSize, 0xFFFFF000ULL);
    if (!gEDBuf || !gTDBuf || !gBounceBuf) return kIOReturnNoMemory;
    gEDVirt = (volatile UInt8 *)gEDBuf->getBytesNoCopy();
    gTDVirt = (volatile UInt8 *)gTDBuf->getBytesNoCopy();
    gBounceVirt = (volatile UInt8 *)gBounceBuf->getBytesNoCopy();
    gEDPhys = (UInt32)gEDBuf->getPhysicalAddress();
    gTDPhys = (UInt32)gTDBuf->getPhysicalAddress();
    gBouncePhys = (UInt32)gBounceBuf->getPhysicalAddress();
    bzero((void *)gEDVirt, kOHCIPageSize);
    bzero((void *)gTDVirt, kOHCIPageSize);
    bzero((void *)gBounceVirt, kOHCIPageSize);

    for (int i = 0; i < 128; i++) { gAddrMaxPkt[i] = 8; gAddrLow[i] = false; }
    gNextAddr = 1;
    bzero(gPortDevice, sizeof(gPortDevice));

    // Save fmInterval before reset (contains FSMPS the HW computed).
    UInt32 fmInterval = ohciR(&regs->hcFmInterval);
    UInt32 fi = fmInterval & kOHCIHcFmInterval_FI;
    if (fi == 0) fi = 0x2EDF;   // spec default

    // Reset the host controller.
    ohciW(&regs->hcCommandStatus, kOHCIHcCommandStatus_HCR);
    for (int i = 0; i < 10; i++) {
        if ((ohciR(&regs->hcCommandStatus) & kOHCIHcCommandStatus_HCR) == 0) break;
        IODelay(10);
    }
    // After reset the HC is in USBReset; we have 2ms to reach operational.

    // Program operational registers.
    ohciW(&regs->hcHCCA, (UInt32)_hccaPhysAddr);
    ohciW(&regs->hcControlHeadED, 0);
    ohciW(&regs->hcControlCurrentED, 0);
    ohciW(&regs->hcBulkHeadED, 0);
    ohciW(&regs->hcBulkCurrentED, 0);

    UInt32 fsmps = ((fi - 210) * 6) / 7;
    UInt32 fit = (fmInterval & kOHCIHcFmInterval_FIT) ^ kOHCIHcFmInterval_FIT;
    ohciW(&regs->hcFmInterval, fi | (fsmps << 16) | fit);
    ohciW(&regs->hcPeriodicStart, (fi * 9) / 10);
    ohciW(&regs->hcLSThreshold, 0x0628);

    // No hardware interrupts - we poll. Disable everything.
    ohciW(&regs->hcInterruptDisable, kOHCIHcInterrupt_MIE);
    ohciW(&regs->hcInterruptStatus, 0xFFFFFFFF);

    // Go operational, enable control + bulk lists.
    UInt32 ctrl = ohciR(&regs->hcControl);
    ctrl &= ~kOHCIHcControl_HCFS;
    ctrl |= (kOHCIFunctionalState_Operational << OHCIBitRangePhase(6, 7));
    ctrl |= kOHCIHcControl_CLE | kOHCIHcControl_BLE;
    ctrl &= ~kOHCIHcControl_IR;   // interrupts route to us, not SMM
    ohciW(&regs->hcControl, ctrl);

    // Power on the root hub ports.
    UInt32 rhDescA = ohciR(&regs->hcRhDescriptorA);
    _rootHubNumPorts = rhDescA & kOHCIHcRhDescriptorA_NDP;
    if (_rootHubNumPorts == 0 || _rootHubNumPorts > 15) _rootHubNumPorts = 15;
    ohciW(&regs->hcRhStatus, kRhStatusLPSC);        // global power on
    for (UInt32 p = 0; p < _rootHubNumPorts; p++)
        ohciW(&regs->hcRhPortStatus[p], kRhPortPPS); // per-port power on
    IOSleep(20);   // POTPGT settle

    _uimInitialized = true;
    OHCI_Log("UIMInitialize ok: %u root ports, ctrl=%08x", _rootHubNumPorts,
             ohciR(&regs->hcControl));
    return kIOReturnSuccess;
}

IOReturn AppleUSBOHCI::UIMFinalize()
{
    if (_pOHCIRegisters)
        ohciW(&_pOHCIRegisters->hcControl,
              (kOHCIFunctionalState_Reset << OHCIBitRangePhase(6, 7)));
    _uimInitialized = false;
    return kIOReturnSuccess;
}

//
// Control transfers (polled)
//

// Perform one synchronous control transfer. setup points to the 8-byte setup
// packet. data (may be NULL) is the caller's flat buffer. Returns bytes moved
// in *outLen. In-hardware toggles are forced per stage, so no carry tracking.
IOReturn AppleUSBOHCI::DoControlTransfer(UInt8 address, const UInt8 *setup,
                                         void *data, UInt32 dataLen, bool isIn,
                                         UInt32 *outLen)
{
    OHCIRegistersPtr regs = _pOHCIRegisters;
    UInt8 mps = gAddrMaxPkt[address] ? gAddrMaxPkt[address] : 8;
    bool  low = gAddrLow[address];
    if (dataLen > (kOHCIPageSize - BOUNCE_DATA_OFF)) dataLen = kOHCIPageSize - BOUNCE_DATA_OFF;

    // Copy setup packet + (for OUT) data into the bounce buffer.
    for (int i = 0; i < 8; i++) gBounceVirt[BOUNCE_SETUP_OFF + i] = setup[i];
    if (dataLen && !isIn && data)
        bcopy(data, (void *)(gBounceVirt + BOUNCE_DATA_OFF), dataLen);

    // Build TD chain: setup -> [data] -> status -> tail(dummy).
    UInt32 nextAfterSetup = dataLen ? tdPhys(TD_DATA) : tdPhys(TD_STATUS);
    volatile UInt32 *st = tdAt(TD_SETUP);
    st[0] = (15u << 28) | (0u << 19) | (7u << 21) | (2u << 24); // CC=NA,DP=SETUP,DI=none,T=DATA0
    st[1] = gBouncePhys + BOUNCE_SETUP_OFF;
    st[2] = nextAfterSetup;
    st[3] = gBouncePhys + BOUNCE_SETUP_OFF + 7;

    if (dataLen) {
        volatile UInt32 *dt = tdAt(TD_DATA);
        UInt32 dp = isIn ? 2u : 1u;            // IN : OUT
        UInt32 r  = isIn ? (1u << 18) : 0;      // buffer rounding for IN
        dt[0] = (15u << 28) | r | (dp << 19) | (7u << 21) | (3u << 24); // T=DATA1
        dt[1] = gBouncePhys + BOUNCE_DATA_OFF;
        dt[2] = tdPhys(TD_STATUS);
        dt[3] = gBouncePhys + BOUNCE_DATA_OFF + dataLen - 1;
    }

    volatile UInt32 *stat = tdAt(TD_STATUS);
    UInt32 sdp = isIn ? 1u : 2u;               // status is opposite direction; no-data => IN
    if (dataLen == 0) sdp = 2u;                // IN status for no-data control
    stat[0] = (15u << 28) | (sdp << 19) | (7u << 21) | (3u << 24); // T=DATA1
    stat[1] = 0;
    stat[2] = tdPhys(TD_TAIL);
    stat[3] = 0;

    volatile UInt32 *tail = tdAt(TD_TAIL);
    tail[0] = tail[1] = tail[2] = tail[3] = 0;

    // Program the control ED.
    volatile UInt32 *ed = edAt(ED_CONTROL);
    ed[0] = (address & 0x7F) | (0u << 7) | (0u << 11) | ((low ? 1u : 0u) << 13) | ((UInt32)mps << 16);
    ed[1] = tdPhys(TD_TAIL);       // tdQueueTailPtr
    ed[2] = tdPhys(TD_SETUP);      // tdQueueHeadPtr (halt/carry = 0)
    ed[3] = 0;                     // nextED

    __sync_synchronize();

    // Kick the control list.
    ohciW(&regs->hcControlHeadED, edPhys(ED_CONTROL));
    ohciW(&regs->hcControlCurrentED, 0);
    ohciW(&regs->hcCommandStatus, kOHCIHcCommandStatus_CLF);

    // Poll until the ED's TD queue drains (head==tail) or halts, with timeout.
    bool done = false, halted = false;
    for (int i = 0; i < 1000; i++) {          // ~1000 * 100us = 100ms
        __sync_synchronize();
        UInt32 head = ed[2];
        if (head & 1) { halted = true; break; }         // H bit set on error
        if ((head & ~0xFu) == (ed[1] & ~0xFu)) { done = true; break; }
        IODelay(100);
    }

    UInt32 moved = 0;
    IOReturn ret = kIOReturnSuccess;
    if (halted) {
        UInt32 cc = 0;
        volatile UInt32 *badTD = dataLen ? tdAt(TD_DATA) : tdAt(TD_SETUP);
        cc = (badTD[0] >> 28) & 0xF;
        ret = (cc == 4) ? kIOReturnNotResponding : kIOReturnIOError;
    } else if (!done) {
        ret = kIOReturnTimeout;
    } else if (dataLen && isIn) {
        volatile UInt32 *dt = tdAt(TD_DATA);
        UInt32 cbp = dt[1];    // currentBufferPtr: 0 when fully consumed
        moved = (cbp == 0) ? dataLen : (cbp - (gBouncePhys + BOUNCE_DATA_OFF));
        if (moved > dataLen) moved = dataLen;
        if (data) bcopy((void *)(gBounceVirt + BOUNCE_DATA_OFF), data, moved);
    } else if (dataLen && !isIn) {
        moved = dataLen;
    }

    // Idle the control list so the HC doesn't walk a stale ED next round.
    ohciW(&regs->hcControlHeadED, 0);
    if (outLen) *outLen = moved;
    return ret;
}

IOReturn AppleUSBOHCI::UIMDeviceRequest(IOUSBDevRequest *request, USBDeviceAddress address)
{
    if (!request) return kIOReturnBadArgument;

    UInt8 setup[8];
    setup[0] = request->bmRequestType;
    setup[1] = request->bRequest;
    setup[2] = request->wValue & 0xFF;
    setup[3] = (request->wValue >> 8) & 0xFF;
    setup[4] = request->wIndex & 0xFF;
    setup[5] = (request->wIndex >> 8) & 0xFF;
    setup[6] = request->wLength & 0xFF;
    setup[7] = (request->wLength >> 8) & 0xFF;

    bool isIn = (request->bmRequestType & 0x80) != 0;
    UInt32 moved = 0;
    IOReturn ret = DoControlTransfer((UInt8)address, setup, request->pData,
                                     request->wLength, isIn, &moved);
    request->wLenDone = moved;
    return ret;
}

//
// Root-hub enumeration thread
//

void AppleUSBOHCI::EnumThreadEntry(void *arg, wait_result_t)
{
    AppleUSBOHCI *me = (AppleUSBOHCI *)arg;
    me->EnumThreadLoop();
}

void AppleUSBOHCI::EnumThreadLoop()
{
    OHCIRegistersPtr regs = _pOHCIRegisters;
    IOSleep(300);   // let the rest of the boot settle

    while (gEnumRunning) {
        for (UInt32 p = 1; p <= _rootHubNumPorts; p++) {
            UInt32 status = ohciR(&regs->hcRhPortStatus[p - 1]);

            if (status & kRhPortCSC)
                ohciW(&regs->hcRhPortStatus[p - 1], kRhPortCSC);   // ack change

            bool connected = (status & kRhPortCCS) != 0;
            if (connected && gPortDevice[p] == NULL)
                EnumeratePort(p);
            else if (!connected && gPortDevice[p] != NULL) {
                OHCI_Log("port %u disconnect", p);
                gPortDevice[p]->terminate();
                gPortDevice[p]->release();
                gPortDevice[p] = NULL;
            }
        }
        IOSleep(256);
    }
}

void AppleUSBOHCI::EnumeratePort(UInt32 port)
{
    OHCIRegistersPtr regs = _pOHCIRegisters;

    // Reset the port.
    ohciW(&regs->hcRhPortStatus[port - 1], kRhPortPRS);
    bool reset = false;
    for (int i = 0; i < 50; i++) {
        UInt32 s = ohciR(&regs->hcRhPortStatus[port - 1]);
        if (s & kRhPortPRSC) { reset = true; break; }
        IOSleep(2);
    }
    ohciW(&regs->hcRhPortStatus[port - 1], kRhPortPRSC);   // ack reset change
    IOSleep(10);

    UInt32 status = ohciR(&regs->hcRhPortStatus[port - 1]);
    if (!reset || !(status & kRhPortCCS)) {
        OHCI_Log("port %u reset failed (status=%08x)", port, status);
        return;
    }
    bool low = (status & kRhPortLSDA) != 0;
    UInt8 speed = low ? kUSBDeviceSpeedLow : kUSBDeviceSpeedFull;
    OHCI_Log("port %u reset ok, %s speed, status=%08x", port,
             low ? "low" : "full", status);

    // Address 0: read the first 8 bytes of the device descriptor to learn
    // bMaxPacketSize0.
    gAddrMaxPkt[0] = 8; gAddrLow[0] = low;
    UInt8 devdesc[18];
    bzero(devdesc, sizeof(devdesc));
    UInt8 setup[8] = { 0x80, kUSBRqGetDescriptor, 0x00, kUSBDeviceDesc, 0x00, 0x00, 0x08, 0x00 };
    UInt32 moved = 0;
    if (DoControlTransfer(0, setup, devdesc, 8, true, &moved) != kIOReturnSuccess || moved < 8) {
        OHCI_Log("port %u: GET_DESCRIPTOR(8) failed", port);
        return;
    }
    // Re-read the full 18-byte descriptor now that we know bMaxPacketSize0
    // (also exercises multi-packet control IN).
    gAddrMaxPkt[0] = devdesc[7] ? devdesc[7] : 8;
    setup[6] = 18;
    DoControlTransfer(0, setup, devdesc, 18, true, &moved);
    UInt8 maxPkt0 = devdesc[7] ? devdesc[7] : 8;

    // Assign an address.
    UInt8 newAddr = gNextAddr++;
    if (gNextAddr > 127) gNextAddr = 1;
    UInt8 setAddr[8] = { 0x00, kUSBRqSetAddress, newAddr, 0x00, 0x00, 0x00, 0x00, 0x00 };
    if (DoControlTransfer(0, setAddr, NULL, 0, false, NULL) != kIOReturnSuccess) {
        OHCI_Log("port %u: SET_ADDRESS(%u) failed", port, newAddr);
        return;
    }
    IOSleep(2);   // SET_ADDRESS recovery
    gAddrMaxPkt[newAddr] = maxPkt0;
    gAddrLow[newAddr] = low;
    OHCI_Log("port %u: addressed %u, maxPkt0=%u", port, newAddr, maxPkt0);

    // Hand off to the generic device-creation path.
    IOUSBDevice *dev = CreateAndConfigureDevice(newAddr, speed, maxPkt0);
    if (!dev) {
        OHCI_Log("port %u: CreateAndConfigureDevice failed", port);
        return;
    }
    if (!dev->attach(this)) { dev->release(); return; }
    if (!dev->start(this)) { dev->detach(this); dev->release(); return; }
    dev->registerService();

    UInt8 cfgValue = 1;   // configuration 1 (typical for HID)
    if (dev->SetConfiguration(dev, cfgValue) != kIOReturnSuccess)
        OHCI_Log("port %u: SetConfiguration failed (continuing)", port);

    gPortDevice[port] = dev;
    OHCI_Log("port %u: device %u published (vid=%04x pid=%04x)", port, newAddr,
             (devdesc[9] << 8) | devdesc[8], (devdesc[11] << 8) | devdesc[10]);
}

//
// Interrupt-IN pipes (polled, best-effort)
//

IOReturn AppleUSBOHCI::UIMOpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint)
{
    // Nothing to pre-allocate: interrupt reads build their own ED/TD on the
    // fly in UIMReadWrite. Just validate.
    if (!endpoint) return kIOReturnBadArgument;
    return kIOReturnSuccess;
}

IOReturn AppleUSBOHCI::UIMClosePipe(USBDeviceAddress, Endpoint *) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::UIMAbortPipe(USBDeviceAddress, Endpoint *) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::UIMClearPipeStall(USBDeviceAddress, Endpoint *) { return kIOReturnSuccess; }

IOReturn AppleUSBOHCI::UIMReadWrite(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                                    Endpoint *endpoint, bool isWrite)
{
    // A single polled interrupt-IN transfer with a short timeout. Used by the
    // synchronous Read() path; returns kIOUSBTransactionTimeout when the device has
    // nothing to report (e.g. an idle mouse), which callers treat as "no data".
    if (!buffer || !endpoint || isWrite)
        return kIOReturnUnsupported;

    OHCIRegistersPtr regs = _pOHCIRegisters;
    UInt32 len = (UInt32)buffer->getLength();
    if (len == 0 || len > (kOHCIPageSize - BOUNCE_DATA_OFF)) return kIOReturnBadArgument;

    UInt8 epNum = endpoint->number & 0x0F;
    UInt8 mps = endpoint->maxPacketSize ? endpoint->maxPacketSize
                                         : (gAddrMaxPkt[address] ? gAddrMaxPkt[address] : 8);
    bool  low = gAddrLow[address];

    volatile UInt32 *dt = tdAt(TD_INTR);
    dt[0] = (15u << 28) | (1u << 18) | (2u << 19) | (7u << 21) | (0u << 24); // IN, rounding, T=carry
    dt[1] = gBouncePhys + BOUNCE_DATA_OFF;
    dt[2] = tdPhys(TD_INTR_TAIL);
    dt[3] = gBouncePhys + BOUNCE_DATA_OFF + len - 1;

    volatile UInt32 *tail = tdAt(TD_INTR_TAIL);
    tail[0] = tail[1] = tail[2] = tail[3] = 0;

    volatile UInt32 *ed = edAt(ED_INTR_BASE);
    ed[0] = (address & 0x7F) | ((UInt32)epNum << 7) | (2u << 11) /* IN */
            | ((low ? 1u : 0u) << 13) | ((UInt32)mps << 16);
    ed[1] = tdPhys(TD_INTR_TAIL);
    ed[2] = tdPhys(TD_INTR);
    ed[3] = 0;
    __sync_synchronize();

    // Link into the HCCA interrupt table (all 32 slots) and enable periodic.
    volatile UInt32 *hcca = (volatile UInt32 *)_hccaBuffer->getBytesNoCopy();
    for (int i = 0; i < 32; i++) hcca[i] = edPhys(ED_INTR_BASE);
    UInt32 ctrl = ohciR(&regs->hcControl);
    ohciW(&regs->hcControl, ctrl | kOHCIHcControl_PLE);

    bool done = false, halted = false;
    for (int i = 0; i < 100; i++) {         // ~100 * 100us = 10ms poll window
        __sync_synchronize();
        UInt32 head = ed[2];
        if (head & 1) { halted = true; break; }
        if ((head & ~0xFu) == (ed[1] & ~0xFu)) { done = true; break; }
        IODelay(100);
    }

    // Unlink so the HC stops touching this ED.
    for (int i = 0; i < 32; i++) hcca[i] = 0;

    if (halted) return kIOReturnIOError;
    if (!done)  return kIOUSBTransactionTimeout;

    UInt32 cbp = dt[1];
    UInt32 moved = (cbp == 0) ? len : (cbp - (gBouncePhys + BOUNCE_DATA_OFF));
    if (moved > len) moved = len;
    buffer->writeBytes(0, (void *)(gBounceVirt + BOUNCE_DATA_OFF), moved);
    return kIOReturnSuccess;
}

//
// Remaining IOUSBControllerV3 surface - not used by this minimal UIM.
//

IOReturn AppleUSBOHCI::AllocatePowerStateArray() { return kIOReturnSuccess; }
void AppleUSBOHCI::ResumeUSBBus(bool) {}
void AppleUSBOHCI::SuspendUSBBus(bool) {}
void AppleUSBOHCI::SetVendorInfo(void) {}
void AppleUSBOHCI::finishPending(void) {}
IOReturn AppleUSBOHCI::ControlInitialize(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::BulkInitialize(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::IsochronousInitialize(void) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::InterruptInitialize(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::InitializeOperationalRegisters(void) { return kIOReturnSuccess; }
void AppleUSBOHCI::showRegisters(UInt32, const char *) {}

void AppleUSBOHCI::doCallback(AppleOHCIGeneralTransferDescriptorPtr, UInt32, UInt32) {}
UInt32 AppleUSBOHCI::findBufferRemaining(AppleOHCIGeneralTransferDescriptorPtr) { return 0; }
AppleOHCIIsochTransferDescriptorPtr AppleUSBOHCI::AllocateITD(void) { return NULL; }
AppleOHCIGeneralTransferDescriptorPtr AppleUSBOHCI::AllocateTD(void) { return NULL; }
AppleOHCIEndpointDescriptorPtr AppleUSBOHCI::AllocateED(void) { return NULL; }
IOReturn AppleUSBOHCI::TranslateStatusToUSBError(UInt32 status) { return status ? kIOReturnIOError : kIOReturnSuccess; }
void AppleUSBOHCI::ProcessCompletedITD(AppleOHCIIsochTransferDescriptorPtr, IOReturn) {}
IOReturn AppleUSBOHCI::DeallocateITD(AppleOHCIIsochTransferDescriptorPtr) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::DeallocateTD(AppleOHCIGeneralTransferDescriptorPtr) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::DeallocateED(AppleOHCIEndpointDescriptorPtr) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::RemoveAllTDs(AppleOHCIEndpointDescriptorPtr) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::RemoveTDs(AppleOHCIEndpointDescriptorPtr, bool) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::DoDoneQueueProcessing(IOPhysicalAddress, UInt32, IOUSBCompletionAction) { return kIOReturnSuccess; }
void AppleUSBOHCI::UIMProcessDoneQueue(IOUSBCompletionAction) {}
void AppleUSBOHCI::UIMRootHubStatusChange(void) {}
void AppleUSBOHCI::UIMRootHubStatusChange(bool) {}
void AppleUSBOHCI::ReturnTransactions(AppleOHCIGeneralTransferDescriptorPtr, UInt32) {}
void AppleUSBOHCI::ReturnOneTransaction(AppleOHCIGeneralTransferDescriptorPtr, AppleOHCIEndpointDescriptorPtr, IOReturn) {}

IOReturn AppleUSBOHCI::UIMCreateControlEndpoint(UInt8, UInt8, UInt16, UInt8) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateControlEndpoint(UInt8, UInt8, UInt16, UInt8, USBDeviceAddress, int) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateControlTransfer(short, short, IOUSBCompletion, void *, bool, UInt32, short) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateControlTransfer(short, short, IOUSBCommand *, void *, bool, UInt32, short) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateControlTransfer(short, short, IOUSBCompletion, IOMemoryDescriptor *, bool, UInt32, short) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateControlTransfer(short, short, IOUSBCommand *, IOMemoryDescriptor *, bool, UInt32, short) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateBulkEndpoint(UInt8, UInt8, UInt8, UInt8, UInt8) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateBulkEndpoint(UInt8, UInt8, UInt8, UInt8, UInt16, USBDeviceAddress, int) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateBulkTransfer(short, short, IOUSBCompletion, IOMemoryDescriptor *, bool, UInt32, short) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateBulkTransfer(IOUSBCommand *) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::CreateGeneralTransfer(AppleOHCIEndpointDescriptorPtr, IOUSBCommand *, IOMemoryDescriptor *, UInt32, UInt32, UInt32, UInt32) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateInterruptEndpoint(short, short, UInt8, short, UInt16, short) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateInterruptEndpoint(short, short, UInt8, short, UInt16, short, USBDeviceAddress, int) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateInterruptTransfer(short, short, IOUSBCompletion, IOMemoryDescriptor *, bool, UInt32, short) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateInterruptTransfer(IOUSBCommand *) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateIsochEndpoint(short, short, UInt32, UInt8) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateIsochEndpoint(short, short, UInt32, UInt8, USBDeviceAddress, int) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateIsochTransfer(short, short, IOUSBIsocCompletion, UInt8, UInt64, IOMemoryDescriptor *, UInt32, IOUSBIsocFrame *) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateIsochTransfer(short, short, IOUSBIsocCompletion, UInt8, UInt64, IOMemoryDescriptor *, UInt32, IOUSBLowLatencyIsocFrame *, UInt32) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMCreateIsochTransfer(IOUSBIsocCommand *) { return kIOReturnUnsupported; }
IOReturn AppleUSBOHCI::UIMAbortEndpoint(short, short, short) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::UIMDeleteEndpoint(short, short, short) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::UIMClearEndpointStall(short, short, short) { return kIOReturnSuccess; }

UInt32 AppleUSBOHCI::GetBandwidthAvailable() { return _isochBandwidthAvail; }
UInt64 AppleUSBOHCI::GetFrameNumber() { return _frameNumber; }
UInt32 AppleUSBOHCI::GetFrameNumber32() { return (UInt32)_frameNumber; }
IOReturn AppleUSBOHCI::callPlatformFunction(const OSSymbol *, bool, void *, void *, void *, void *) { return kIOReturnUnsupported; }
void AppleUSBOHCI::PollInterrupts(IOUSBCompletionAction) {}
void AppleUSBOHCI::UIMCheckForTimeouts(void) {}

IOReturn AppleUSBOHCI::GetFrameNumberWithTime(UInt64 *frameNumber, AbsoluteTime *theTime)
{
    if (frameNumber) *frameNumber = GetFrameNumber();
    if (theTime) clock_get_uptime(theTime);
    return kIOReturnSuccess;
}
IOReturn AppleUSBOHCI::GatedGetFrameNumberWithTime(OSObject *owner, void *arg0, void *arg1, void *, void *)
{
    AppleUSBOHCI *me = OSDynamicCast(AppleUSBOHCI, owner);
    if (!me) return kIOReturnBadArgument;
    return me->GetFrameNumberWithTime((UInt64 *)arg0, (AbsoluteTime *)arg1);
}
IODMACommand *AppleUSBOHCI::GetNewDMACommand()
{
    return IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, PAGE_SIZE,
        (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
}
void AppleUSBOHCI::CheckSleepCapability(void) {}
IOReturn AppleUSBOHCI::ResetControllerState(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::RestartControllerFromReset(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::SaveControllerStateForSleep(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::RestoreControllerStateFromSleep(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::DozeController(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::WakeControllerFromDoze(void) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::UIMEnableAddressEndpoints(USBDeviceAddress, bool) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::UIMEnableAllEndpoints(bool) { return kIOReturnSuccess; }
IOReturn AppleUSBOHCI::EnableInterruptsFromController(bool) { return kIOReturnSuccess; }
