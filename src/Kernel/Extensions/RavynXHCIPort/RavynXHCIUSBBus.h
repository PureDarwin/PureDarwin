/*
 * RavynXHCIUSBBus: concrete IOUSBController backed directly by
 * RavynXHCIPort's existing, already-working per-slot transfer primitives
 * (controlTransfer/bulkTransfer/configureBulkEndpoints/
 * configureInterruptInEndpoint). Lets IOUSBFamily's IOUSBDevice/
 * IOUSBInterface/IOUSBPipe - and anything that matches on them, like
 * IOUSBCompositeDriver - work against xHCI devices RavynXHCIPort doesn't
 * already special-case as hub/mass-storage/keyboard.
 *
 * Scope limit inherited from RavynXHCIPort: configureBulkEndpoints()
 * configures a single bulk-in/out ring pair per slot (not indexed by
 * endpoint number), and configureInterruptInEndpoint() a single
 * interrupt-in ring per endpoint. This bus supports one bulk IN + one bulk
 * OUT pair and multiple interrupt IN endpoints per device, not arbitrary multi-endpoint
 * topologies. USBDeviceAddress == the device's xHCI slot ID directly (no
 * separate bus-address abstraction - we own both ends).
 */

#ifndef _RAVYN_XHCI_USB_BUS_H
#define _RAVYN_XHCI_USB_BUS_H

#include <IOKit/usb/IOUSBController.h>

class RavynXHCIPort;

class RavynXHCIUSBBus : public IOUSBController
{
    OSDeclareDefaultStructors(RavynXHCIUSBBus)

public:
    bool initWithPort(RavynXHCIPort *port);

    virtual IOReturn UIMOpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint) APPLE_KEXT_OVERRIDE;
    virtual IOReturn UIMClosePipe(USBDeviceAddress address, Endpoint *endpoint) APPLE_KEXT_OVERRIDE;
    virtual IOReturn UIMAbortPipe(USBDeviceAddress address, Endpoint *endpoint) APPLE_KEXT_OVERRIDE;
    virtual IOReturn UIMClearPipeStall(USBDeviceAddress address, Endpoint *endpoint) APPLE_KEXT_OVERRIDE;

    virtual IOReturn UIMDeviceRequest(IOUSBDevRequest *request, USBDeviceAddress address) APPLE_KEXT_OVERRIDE;

    virtual IOReturn Read(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                          Endpoint *endpoint, IOUSBCompletion *completion = 0) APPLE_KEXT_OVERRIDE;
    virtual IOReturn Read(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                          Endpoint *endpoint, IOUSBCompletion *completion,
                          UInt32 noDataTimeout, UInt32 completionTimeout,
                          IOByteCount reqCount) APPLE_KEXT_OVERRIDE;

    virtual IOReturn UIMReadWrite(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                                   Endpoint *endpoint, bool isWrite) APPLE_KEXT_OVERRIDE;

private:
    RavynXHCIPort *fPort;
    bool fBulkConfigured[64];
};

#endif /* !_RAVYN_XHCI_USB_BUS_H */
