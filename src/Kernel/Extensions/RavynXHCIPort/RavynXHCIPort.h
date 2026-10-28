/*
 * RavynXHCIPort: minimal xHCI host controller driver + USB Mass Storage
 * (bulk-only transport) enumeration, for booting off a USB stick where no
 * AHCI/SATA disk is present. One host controller covers USB 1.1/2.0/3.x.
 *
 * Copyright (C) 2026 ravynOS Project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _RAVYN_XHCI_PORT_H
#define _RAVYN_XHCI_PORT_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/usb/USB.h>
#include "XHCI.h"

extern void XHCI_Log(const char *fmt, ...);

class RavynXHCIMassStorageDisk;
class RavynXHCIUSBBus;
class IOUSBDevice;

class RavynXHCIPort : public IOService
{
    OSDeclareDefaultStructors(RavynXHCIPort);
    friend class RavynXHCIMassStorageDisk;
    friend class RavynXHCIUSBBus;

public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;
    IOReturn message(UInt32 type, IOService *provider, void *argument = NULL) override;

    // called by RavynXHCIMassStorageDisk to move data via bulk-only transport
    IOReturn botTransfer(UInt32 slotId,
                         const void *cbwCB, UInt8 cbwLen,
                         UInt32 dataLen, bool dataIn,
                         IOMemoryDescriptor *buffer, UInt64 bufOff,
                         bool allowShort = false);

private:
    // one tracked USB device slot that turned out to be mass storage
    struct MSCDevice {
        bool     valid;
        UInt32   slotId;
        UInt8    portNum;        // 0-based root hub port
        UInt8    bulkInEp;       // endpoint number, 1-15
        UInt8    bulkOutEp;
        UInt16   bulkMaxPacket;
        UInt64   capacityBlocks;
        UInt32   blockSize;
        char     vendor[9];
        char     product[17];
        UInt8    iface;          // bInterfaceNumber, for the bulk-only reset
        UInt8    cfgValue;       // bConfigurationValue, set again after a port reset
        UInt16   bulkOutMaxPacket;
    };

    // one tracked interrupt-IN endpoint. a boot HID device can leave a transfer pending until
    // its next state change, so the TD and buffer persist across idle polls
    struct InterruptInEndpoint {
        bool                       valid;
        UInt32                     slotId;
        UInt8                      rootPort;     // 0-based root hub port
        UInt8                      intrEp;       // interrupt IN endpoint number
        UInt16                     intrMaxPkt;
        bool                       tdOutstanding; // an interrupt-IN TD is armed
        bool                       loggedPollArm;
        bool                       loggedFirstCompletion;
        UInt32                     transactionErrors;
        IOBufferMemoryDescriptor * reportMem;
        volatile UInt8           * reportVirt;
        UInt64                     reportPhys;
        IOLock                   * lock;
        IOBufferMemoryDescriptor * ringMem;
        volatile XHCITRB          * ring;
        UInt32                     enqueue;
        UInt8                      cycle;
    };

    // a generic bulk endpoint with its own ring and queued async transfers
    enum { kBulkMaxTDs = 64 };
    struct BulkTD {
        IOUSBCompletion            completion;
        IOMemoryDescriptor       * client;
        IOBufferMemoryDescriptor * bounce;
        UInt32                     len;
        UInt64                     trbPhys;
        bool                       done;
        UInt8                      cc;
        UInt32                     residual;
    };
    struct BulkEndpoint {
        bool                       valid, in;
        UInt32                     slotId, dci;
        UInt16                     maxPkt;
        IOBufferMemoryDescriptor * ringMem;
        volatile XHCITRB          * ring;
        UInt32                     enqueue;
        UInt8                      cycle;
        UInt32                     head, count;
        BulkTD                     td[kBulkMaxTDs];
    };

    IOService          * fProvider;
    IOPCIDevice        * fPCI;
    // dma below this mask, the a733 dwc3 gets 32-bit buffers
    UInt64                fDMAMask;
    // set when the controller is a non-coherent soc block (dwc3 on the a733)
    bool                  fNonCoherent;
    IOMemoryMap         * fSoCMaps[6];
    IOMemoryDescriptor * fBARDesc;
    IOMemoryMap         * fBARMap;
    UInt64                fBARLength;
    volatile UInt8      * fCapRegs;
    volatile UInt8      * fOpRegs;
    volatile UInt8      * fRTRegs;
    volatile UInt8      * fDBRegs;
    UInt32                fMaxSlots;
    UInt32                fMaxPorts;
    UInt32                fContextSize; // 32 or 64 bytes/context
    IOLock              * fCmdLock;

    // interrupt-driven event ring (MSI or a device tree interrupt). waiters drain the ring, then sleep
    // until fEventGeneration moves, their deadline or a 2 ms backstop, without an interrupt 1 ms polling
    IOWorkLoop            * fWorkLoop;
    IOInterruptEventSource * fInterruptSource;
    bool                   fInterruptsEnabled;
    bool                   fPlatformInterrupt;
    IOLock               * fEventWaitLock;
    UInt32                 fEventWaitChannel;
    UInt32                 fEventGeneration;    // under fEventWaitLock, bumped when events are filed
    // under fEventLock: interrupts taken, events drained by them and by polling, and torn event reads
    UInt32                 fIntrCount;
    UInt32                 fIntrEvents;
    UInt32                 fPolledEvents;
    UInt32                 fTornEvents;
    uint64_t               fLastIntrTime;

    // IOUSBController-shaped view of this port, backing IOUSBDevice/IOUSBInterface for slots
    // enumerateSlotDevice() doesn't handle as hub or mass storage (see RavynXHCIUSBBus)
    RavynXHCIUSBBus      * fUSBBus;

    // USB2/USB3 root port pairing from the Supported Protocol caps, by 0-based port. a SuperSpeed
    // device must be enumerated through its USB3-view port or every transfer fails. 0xFF = no pairing
    UInt8                  fPortMajorRev[64];   // 2, 3, or 0 if unknown
    UInt8                  fPairedPort[64];     // 0xFF if none

    // hotplug: set once a root port has had enumeration attempted, so the poll thread doesn't repeat
    // Enable Slot/Address Device for one connect. cleared when CCS drops
    bool                   fPortOccupied[64];
    UInt8                  fResetTries[64];
    uint64_t               fVbusOnAt;
    volatile bool          fHotplugRunning;
    // a port status change event wakes the hotplug thread early, under fEventWaitLock
    bool                   fPortChangePending;
    UInt32                 fHotplugWaitChannel;
    // set once the controller is halted for a restart or the watchdog deadline
    volatile bool          fQuiesced;
    IONotifier           * fRestartNotifier;
    static void hotplugThread(void *arg, wait_result_t);
    void hotplugLoop();
    void pollHubPorts();
    void disconnectUSBDevices(UInt32 rootPort, UInt32 route, UInt32 mask);
    void handleRootPortDisconnect(UInt32 port0based, UInt32 portsc);

    // DCBAA: 64-bit device context pointers indexed by slot ID (0 unused)
    IOBufferMemoryDescriptor * fDCBAAMem;
    volatile UInt64           * fDCBAA;

    // scratchpad buffers (HCSPARAMS2), controller-owned memory referenced via DCBAA[0], all NULL if none needed
    IOBufferMemoryDescriptor * fScratchpadArrayMem;
    IOBufferMemoryDescriptor ** fScratchpadBufMem;
    UInt32                      fScratchpadCount;

    // command ring: single segment, linked to itself via a LINK TRB
    IOBufferMemoryDescriptor * fCmdRingMem;
    volatile XHCITRB          * fCmdRing;
    UInt32                      fCmdRingEnqueue;
    UInt8                       fCmdRingCycle;

    // primary event ring: single segment + one-entry segment table
    IOBufferMemoryDescriptor * fEventRingMem;
    volatile XHCITRB          * fEventRing;
    IOBufferMemoryDescriptor * fERSTMem;
    UInt32                      fEventRingDequeue;
    UInt8                       fEventRingCycle;

    // serviceEventRing() is the sole event ring consumer (under fEventLock) and files each completion
    // here for its waiter, so concurrent waiters never discard each other's events
    IOLock              * fEventLock;
    struct XferCompletion {
        volatile bool pending;
        UInt8         cc;
        UInt64        param;
        UInt32        status;
        UInt32        control;
        UInt32        shortResidual;
        UInt64        shortTRB;
        // when set, only events whose trb lies in the td being waited for are kept
        bool          matchActive;
        UInt64        matchRingBase;
        UInt32        matchStart;
        UInt32        matchCount;
    };
    XferCompletion fXferDone[64][32]; // [slotId][DCI]
    // last command completion (commands are serialized under fCmdLock)
    volatile UInt64 fCmdDoneParam;
    volatile bool   fCmdDonePending;
    UInt8           fCmdDoneCC;
    UInt32          fCmdDoneSlot;

    // per-slot device context, input context and transfer rings (EP0, bulk IN/OUT)
    struct SlotResources {
        IOBufferMemoryDescriptor * deviceCtxMem;
        IOBufferMemoryDescriptor * inputCtxMem;
        IOBufferMemoryDescriptor * ep0RingMem;
        volatile XHCITRB          * ep0Ring;
        UInt32                      ep0Enqueue;
        UInt8                       ep0Cycle;
        IOBufferMemoryDescriptor * bulkInRingMem;
        volatile XHCITRB          * bulkInRing;
        UInt32                      bulkInEnqueue;
        UInt8                       bulkInCycle;
        IOBufferMemoryDescriptor * bulkOutRingMem;
        volatile XHCITRB          * bulkOutRing;
        UInt32                      bulkOutEnqueue;
        UInt8                       bulkOutCycle;
    };
    SlotResources fSlots[64]; // index by slot ID, 0 unused
    UInt8 fSlotRootPort[64];  // 0-based root hub port for each slot
    IOUSBDevice *fUSBDeviceNubs[64];
    UInt32 fSlotRoute[64];
    struct HubDevice {
        bool valid, superSpeed;
        UInt8 rootPort, numPorts, depth;
        UInt32 route, connected, attempted;
        UInt8 retries[32];
    };
    HubDevice fHubs[64];

    MSCDevice fMSC[16];
    RavynXHCIMassStorageDisk * fDiskNubs[16];
    // one bulk-only command at a time, and unplug takes it to retire an fMSC entry
    IOLock                   * fMSCLock;

    InterruptInEndpoint fIntrIn[64][16]; // [slotId][endpoint number]

    BulkEndpoint * fBulk[64][32]; // [slotId][dci], guarded by fEventLock
    volatile bool  fBulkThreadStarted;
    volatile bool  fBulkStop;
    static void bulkThread(void *arg, wait_result_t);
    void deliverBulkCompletions();
    bool recordBulkEvent(UInt32 slot, UInt32 dci, UInt64 param, UInt32 status);

    // context addressing is byte offsets scaled by fContextSize (HCCPARAMS1.CSZ, 32 or 64 bytes).
    // input context = control, slot, then endpoints by DCI 1..31, a device context lacks the control
    inline UInt32 inputCtxBytes(void) const  { return 33 * fContextSize; }
    inline UInt32 deviceCtxBytes(void) const { return 32 * fContextSize; }
    inline XHCIInputControlContext *inputCtl(void *ic) const
        { return (XHCIInputControlContext *)ic; }
    inline XHCISlotContext *inputSlot(void *ic) const
        { return (XHCISlotContext *)((UInt8 *)ic + fContextSize); }
    inline XHCIEndpointContext *inputEp(void *ic, UInt32 dci) const
        { return (XHCIEndpointContext *)((UInt8 *)ic + (1 + dci) * fContextSize); }
    inline XHCISlotContext *deviceSlot(void *dc) const
        { return (XHCISlotContext *)dc; }
    inline XHCIEndpointContext *deviceEp(void *dc, UInt32 dci) const
        { return (XHCIEndpointContext *)((UInt8 *)dc + dci * fContextSize); }

    inline UInt32 capRead32(UInt32 off) const
        { return *(volatile UInt32 *)(fCapRegs + off); }
    inline UInt32 opRead32(UInt32 off) const
        { return *(volatile UInt32 *)(fOpRegs + off); }
    inline void opWrite32(UInt32 off, UInt32 v) const
        { *(volatile UInt32 *)(fOpRegs + off) = v; }
    // 64-bit registers (DCBAAP, CRCR, ERSTBA, ERDP) go as two ordered 32-bit accesses, low first.
    // real hardware dropped single 64-bit writes (CRCR read back zero), as in linux lo_hi_writeq
    inline UInt64 opRead64(UInt32 off) const
        { UInt32 lo = opRead32(off); UInt32 hi = opRead32(off + 4); return ((UInt64)hi << 32) | lo; }
    inline void opWrite64(UInt32 off, UInt64 v) const
        { opWrite32(off, (UInt32)v); opWrite32(off + 4, (UInt32)(v >> 32)); }
    inline UInt32 rtRead32(UInt32 off) const
        { return *(volatile UInt32 *)(fRTRegs + off); }
    inline void rtWrite32(UInt32 off, UInt32 v) const
        { *(volatile UInt32 *)(fRTRegs + off) = v; }
    inline UInt64 rtRead64(UInt32 off) const
        { UInt32 lo = rtRead32(off); UInt32 hi = rtRead32(off + 4); return ((UInt64)hi << 32) | lo; }
    inline void rtWrite64(UInt32 off, UInt64 v) const
        { rtWrite32(off, (UInt32)v); rtWrite32(off + 4, (UInt32)(v >> 32)); }
    inline UInt32 portRead32(UInt32 port0based, UInt32 reg) const
        { return opRead32(XHCI_PORTREGS_BASE + port0based * XHCI_PORTREGS_SIZE + reg); }
    inline void portWrite32(UInt32 port0based, UInt32 reg, UInt32 v) const
        { opWrite32(XHCI_PORTREGS_BASE + port0based * XHCI_PORTREGS_SIZE + reg, v); }
    inline void ringDoorbell(UInt32 slot, UInt32 target) const
        {
            // make the TRB stores visible before the doorbell write, or the controller can sample a stale
            // ring, go idle and strand the queued TD
            __sync_synchronize();
            *(volatile UInt32 *)(fDBRegs + slot * 4) = target;
        }

    bool startA733(IOService *provider);
    bool startDWC3(IOService *provider);
    bool startTegra(IOService *provider);
    void quiesce(const char *why);
    static IOReturn restartHandler(void *target, void *refCon, UInt32 messageType,
                                   IOService *provider, void *messageArgument, vm_size_t argSize);
    volatile UInt32 *mapSoC(UInt64 phys, UInt64 len, int idx);
    // clean and invalidate cpu lines for memory the controller reads or writes
    void dmaFlush(const volatile void *va, UInt64 len) const;
    void dmaFlushContexts();

    bool resetController();
    void claimBIOSOwnership();
    void parseExtendedCapabilities();
    bool setupDCBAA();
    bool setupCommandRing();
    bool setupEventRing();
    void scanPorts();
    bool resetAndEnumeratePort(UInt32 port0based);

    bool allocRing(IOBufferMemoryDescriptor **outMem, volatile XHCITRB **outVirt, UInt32 trbCount);
    void pushTRB(volatile XHCITRB *ring, UInt32 &enqueue, UInt8 &cycle, UInt32 ringSizeTRBs,
                UInt64 param, UInt32 status, UInt32 control);

    // ring a command TRB and poll for its completion, returns true and fills outCC/outSlotId on success
    bool doCommand(UInt64 param, UInt32 status, UInt32 controlNoCycle,
                   UInt8 *outCC, UInt32 *outSlotId, UInt64 timeoutMs = 500);

    // poll the event ring for a transfer event on the given slot/endpoint
    bool waitTransferEvent(UInt32 slotId, UInt32 epDCI, UInt8 *outCC, UInt32 timeoutMs = 1000,
                           UInt32 *outResidual = NULL, UInt64 *outTRB = NULL);

    // sole event ring consumer: drains available events under fEventLock into fXferDone[][]/fCmdDone*
    // for their waiters. safe from multiple threads
    void serviceEventRing(bool fromInterrupt = false);
    void logInterruptState(const char *why);
    bool eventInTD(UInt32 slot, UInt32 dci, UInt64 trb) const;
    UInt32 eventGeneration();
    void sleepForEvent(UInt32 generation, uint64_t until);

    void interruptOccurred(IOInterruptEventSource *sender, int count);
    static void interruptOccurredStatic(OSObject *owner, IOInterruptEventSource *sender, int count);

    // each interrupt endpoint owns a ring and a persistent dma report buffer
    bool configureInterruptInEndpoint(UInt32 slotId, UInt8 epNum, UInt16 maxPkt, UInt8 interval);
    IOReturn interruptTransfer(UInt32 slotId, UInt8 epNum, IOMemoryDescriptor *buffer,
                               UInt32 len, UInt32 timeoutMs, UInt32 *done = NULL);
    IOReturn abortInterruptEndpoint(UInt32 slotId, UInt8 epNum, bool reset = false);

    bool enableSlot(UInt32 *outSlotId);
    bool disableSlot(UInt32 slotId);
    void freeSlotResources(UInt32 slotId);
    bool addressDevice(UInt32 slotId, UInt32 port0based, UInt32 routeString,
                       UInt32 speed, UInt16 &maxPacket0,
                       UInt32 parentHubSlot = 0, UInt32 parentPortNum = 0);
    bool sendAddressDeviceCommand(UInt32 slotId, UInt32 port0based, UInt32 routeString,
                                  UInt32 speed, UInt16 maxPkt, bool bsr,
                                  UInt32 parentHubSlot, UInt32 parentPortNum);
    bool controlTransfer(UInt32 slotId, const USBSetupPacket &setup,
                         void *buf, UInt16 len, bool in, UInt32 *done = NULL);
    bool configureBulkEndpoints(UInt32 slotId, UInt8 inEp, UInt16 inMaxPkt,
                                UInt8 outEp, UInt16 outMaxPkt);
    // after a stall: reset the endpoint, move its dequeue past the halted td, clear the device halt
    void recoverHaltedEndpoint(UInt32 slotId, UInt8 epNum, bool in);
    void resetHaltedRing(UInt32 slotId, UInt32 dci, UInt64 deq, UInt8 cycle, UInt8 *cc1, UInt8 *cc2);
    bool stopRing(UInt32 slotId, UInt32 dci, UInt64 deq, UInt8 cycle);
    // on success *outActual is the byte count moved, on failure *outCC is the completion code (0 = timeout)
    bool bulkTransfer(UInt32 slotId, UInt8 epNum, bool in,
                      IOBufferMemoryDescriptor *xferMem, UInt32 len, UInt32 timeoutMs,
                      UInt32 *outActual = NULL, UInt8 *outCC = NULL);
    // stop or reset the endpoint, move its dequeue past the abandoned td, then clear the device halt
    bool abortFailedTD(UInt32 slotId, UInt8 epNum, bool in);
    // bulk-only mass storage reset recovery (bot 5.3.4)
    void botResetRecovery(UInt32 mscIndex);
    bool botPortReset(UInt32 mscIndex);
    void abortCommandRing(unsigned type);
    IOReturn botTransferLocked(UInt32 slotId, const void *cbwCB, UInt8 cbwLen,
                               UInt32 dataLen, bool dataIn,
                               IOMemoryDescriptor *buffer, UInt64 bufOff, bool allowShort);

public:
    // generic bulk endpoints for the IOUSBController bus, one ring each
    bool openBulkEndpoint(UInt32 slotId, UInt8 epNum, bool in, UInt16 maxPkt);
    IOReturn submitBulk(UInt32 slotId, UInt8 epNum, bool in, IOMemoryDescriptor *buffer,
                        UInt32 len, IOUSBCompletion *completion);
    IOReturn bulkSync(UInt32 slotId, UInt8 epNum, bool in, IOMemoryDescriptor *buffer,
                      UInt32 len, UInt32 timeoutMs, UInt32 *done);
    IOReturn abortBulk(UInt32 slotId, UInt8 epNum, bool in, bool reset);
private:

    // set the slot context Hub/NumberOfPorts/TTT via Configure Endpoint (only the A0 flag) so the xHC
    // accepts route strings for downstream devices
    bool markSlotAsHub(UInt32 slotId, UInt8 numPorts, bool multiTT,
                       UInt8 intrEp, UInt16 intrMaxPkt, UInt8 intrInterval);

    // hub class control requests (recipient = other/port)
    bool hubGetDescriptor(UInt32 slotId, bool superSpeed, void *buf, UInt16 len);
    bool hubSetDepth(UInt32 slotId, UInt16 depth);
    bool hubSetPortFeature(UInt32 slotId, UInt8 port1based, UInt16 feature);
    bool hubClearPortFeature(UInt32 slotId, UInt8 port1based, UInt16 feature);
    bool hubGetPortStatus(UInt32 slotId, UInt8 port1based, UInt32 *outStatus);

    // reset and enable one hub downstream port and enumerate its device with a hub-relative route,
    // depth-limited against a mis-parsed hub descriptor
    bool enumerateHubPort(UInt32 hubSlotId, UInt32 rootPort0based, UInt32 hubRouteString,
                          UInt8 port1based, bool superSpeedHub, int depth);

    // for an addressed slot, fetch the device descriptor, then recurse into hub ports or look for a
    // bulk-only mass storage interface and publish a disk nub
    bool enumerateSlotDevice(UInt32 slotId, UInt32 rootPort0based, UInt32 routeString,
                             UInt32 speed, int depth, UInt16 maxPkt0);
    bool evaluateEP0MaxPacket(UInt32 slotId, UInt16 maxPkt);

    bool tryEnumerateMassStorage(UInt32 port0based, UInt32 speed);
};

#endif // _RAVYN_XHCI_PORT_H
