#ifndef PD_USB_NCM_H
#define PD_USB_NCM_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <kern/thread_call.h>

// Puts a device that offers a CDC-NCM configuration into it, so PDUSBNCM can match its interfaces.
// Devices like the RTL8156 come up in a vendor configuration and keep NCM as an alternative
class PDUSBNCMConfig : public IOService {
    OSDeclareDefaultStructors(PDUSBNCMConfig);

public:
    virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;

private:
    UInt8 fConfigValue;
};

// A USB CDC Network Control Model Ethernet function, NTB16 framing
class PDUSBNCM : public IOEthernetController {
    OSDeclareDefaultStructors(PDUSBNCM);

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free(void) APPLE_KEXT_OVERRIDE;

    virtual IOReturn enable(IONetworkInterface *netif) APPLE_KEXT_OVERRIDE;
    virtual IOReturn disable(IONetworkInterface *netif) APPLE_KEXT_OVERRIDE;
    virtual UInt32 outputPacket(mbuf_t m, void *param) APPLE_KEXT_OVERRIDE;
    virtual IOReturn getHardwareAddress(IOEthernetAddress *addr) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setPromiscuousMode(bool active) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setMulticastMode(bool active) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setMulticastList(IOEthernetAddress *addrs, UInt32 count) APPLE_KEXT_OVERRIDE;
    virtual IOOutputQueue *createOutputQueue(void) APPLE_KEXT_OVERRIDE;
    virtual bool createWorkLoop(void) APPLE_KEXT_OVERRIDE;
    virtual IOWorkLoop *getWorkLoop(void) const APPLE_KEXT_OVERRIDE;
    virtual const OSString *newVendorString(void) const APPLE_KEXT_OVERRIDE;
    virtual const OSString *newModelString(void) const APPLE_KEXT_OVERRIDE;

private:
    enum { kRxBuffers = 4, kTxBuffers = 16 };

    struct Xfer {
        PDUSBNCM *owner;
        IOBufferMemoryDescriptor *mem;
        IOUSBCompletion completion;
        bool busy;
        thread_call_t again;
    };

    bool findDataInterface(void);
    bool readMACAddress(void);
    bool readNtbParameters(void);
    IOReturn classRequest(UInt8 dirIn, UInt8 request, UInt16 value, void *data, UInt16 len);
    void setFilter(void);
    void publishMedia(void);

    void submitRx(Xfer *x);
    void submitStatus(void);
    void parseNtb(const UInt8 *buf, UInt32 len);
    void handleStatus(const UInt8 *buf, UInt32 len);

    static void rxDone(void *target, void *param, IOReturn status, UInt32 remaining);
    static void txDone(void *target, void *param, IOReturn status, UInt32 remaining);
    static void statusDone(void *target, void *param, IOReturn status, UInt32 remaining);
    static void resubmit(thread_call_param_t self, thread_call_param_t xfer);

    IOUSBInterface *fControl;
    IOUSBInterface *fData;
    IOUSBDevice *fDevice;
    IOUSBPipe *fBulkIn;
    IOUSBPipe *fBulkOut;
    IOUSBPipe *fIntrIn;
    IOWorkLoop *fWorkLoop;
    IOEthernetInterface *fNetif;
    IOEthernetAddress fMac;

    UInt32 fNtbInMax;
    UInt32 fNtbOutMax;
    UInt16 fOutDivisor;
    UInt16 fOutRemainder;
    UInt16 fOutAlign;
    UInt16 fTxSeq;
    UInt16 fFilter;
    UInt8 fDataAlt;

    Xfer fRx[kRxBuffers];
    Xfer fTx[kTxBuffers];
    Xfer fStatus;
    volatile bool fRunning;
    bool fLinkUp;
    UInt64 fSpeed;
};

#endif
