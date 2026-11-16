#ifndef _IOKIT_USBEHCI_H
#define _IOKIT_USBEHCI_H

#include <IOKit/IOTypes.h>

typedef IOPhysicalAddress32 USBPhysicalAddress32;

#ifndef HostToUSBLong
#define HostToUSBLong(x) ((UInt32)(x))
#endif
#ifndef USBToHostLong
#define USBToHostLong(x) ((UInt32)(x))
#endif
#ifndef HostToUSBWord
#define HostToUSBWord(x) ((UInt16)(x))
#endif
#ifndef USBToHostWord
#define USBToHostWord(x) ((UInt16)(x))
#endif

enum {
    kEHCIPageSize = 4096,
    kEHCIFrameListEntries = 1024,
    kEHCIMaxPollingInterval = 32
};

enum {
    kEHCICapLength = 0x00,
    kEHCIHCIVersion = 0x02,
    kEHCIHCSParams = 0x04,
    kEHCIHCCParams = 0x08,

    kEHCIUSBCmd = 0x00,
    kEHCIUSBSts = 0x04,
    kEHCIUSBIntr = 0x08,
    kEHCIFrameIndex = 0x0c,
    kEHCICtrlDSegment = 0x10,
    kEHCIPeriodicListBase = 0x14,
    kEHCIAsyncListAddr = 0x18,
    kEHCIConfigFlag = 0x40,
    kEHCIPortSCBase = 0x44
};

enum {
    kEHCIUSBCmdRunStop = 1U << 0,
    kEHCIUSBCmdHCReset = 1U << 1,
    kEHCIUSBCmdFrameListSize1024 = 0U << 2,
    kEHCIUSBCmdPeriodicEnable = 1U << 4,
    kEHCIUSBCmdAsyncEnable = 1U << 5,

    kEHCIUSBStsHalted = 1U << 12,
    kEHCIUSBStsPeriodicStatus = 1U << 14,
    kEHCIUSBStsAsyncStatus = 1U << 15,

    // HCCPARAMS bit 0: data structure pointers are 64-bit, upper half coming
    // from CTRLDSSEGMENT. Set on Intel PCH parts, clear on QEMU's EHCI.
    kEHCIHCCParams64Bit = 1U << 0
};

enum {
    kEHCIPortSCConnect = 1U << 0,
    kEHCIPortSCConnectChange = 1U << 1,
    kEHCIPortSCEnabled = 1U << 2,
    kEHCIPortSCEnableChange = 1U << 3,
    kEHCIPortSCOverCurrentChange = 1U << 5,
    kEHCIPortSCReset = 1U << 8,
    kEHCIPortSCPower = 1U << 12,
    kEHCIPortSCOwner = 1U << 13,
    kEHCIPortSCResetChange = 1U << 20,
    kEHCIPortSCChangeMask = kEHCIPortSCConnectChange |
                            kEHCIPortSCEnableChange |
                            kEHCIPortSCOverCurrentChange |
                            kEHCIPortSCResetChange
};

enum {
    kEHCILinkTerminate = 1U << 0,
    kEHCILinkTypeQH = 1U << 1,
    kEHCILinkPointerMask = 0xffffffe0U
};

enum {
    kEHCIQHEndpointSpeedFull = 0U << 12,
    kEHCIQHEndpointSpeedLow = 1U << 12,
    kEHCIQHEndpointSpeedHigh = 2U << 12,
    kEHCIQHControlEndpoint = 1U << 27,
    kEHCIQHHubAddrShift = 16,
    kEHCIQHPortShift = 23,
    kEHCIQHDTC = 1U << 14,
    kEHCIQHHead = 1U << 15,
    kEHCIQHMaxPacketShift = 16,
    kEHCIQHCMaskShift = 8,
    kEHCIQHMultShift = 30
};

enum {
    kEHCIqTDTerminate = 1U << 0,
    kEHCIqTDStatusHalted = 1U << 6,
    kEHCIqTDStatusActive = 1U << 7,
    kEHCIqTDPIDOut = 0U << 8,
    kEHCIqTDPIDIn = 1U << 8,
    kEHCIqTDPIDSetup = 2U << 8,
    kEHCIqTDCerrShift = 10,
    kEHCIqTDIOC = 1U << 15,
    kEHCIqTDBytesShift = 16,
    kEHCIqTDBytesMask = 0x7fffU << 16,
    kEHCIqTDStatusMask = 0xffU,
    kEHCIqTDDataToggle = 1U << 31
};

#define EHCI_HCS_N_PORTS(x) ((x) & 0x0fU)
#define EHCI_HCC_EECP(x) (((x) >> 8) & 0xffU)

struct EHCIQueueHeadShared
{
    volatile USBPhysicalAddress32 horizLinkPtr;
    volatile UInt32 endpointCaps;
    volatile UInt32 endpointSplitCaps;
    volatile USBPhysicalAddress32 currentqTDPtr;
    volatile USBPhysicalAddress32 nextqTDPtr;
    volatile USBPhysicalAddress32 altqTDPtr;
    volatile UInt32 qTDFlags;
    volatile USBPhysicalAddress32 bufferPtr[5];
    volatile USBPhysicalAddress32 extBufferPtr[5];
};
typedef EHCIQueueHeadShared *EHCIQueueHeadSharedPtr;

typedef EHCIQueueHeadShared EHCIAsyncQueueHead;
typedef EHCIAsyncQueueHead *EHCIAsyncQueueHeadPtr;

struct EHCIGeneralTransferDescriptorShared
{
    volatile USBPhysicalAddress32 nextTD;
    volatile USBPhysicalAddress32 altTD;
    volatile UInt32 flags;
    volatile USBPhysicalAddress32 bufferPtr[5];
    volatile USBPhysicalAddress32 extBufferPtr[5];
};
typedef EHCIGeneralTransferDescriptorShared *EHCIGeneralTransferDescriptorSharedPtr;

struct EHCIIsochTransferDescriptorShared
{
    volatile USBPhysicalAddress32 nextLink;
    volatile UInt32 transaction[8];
    volatile USBPhysicalAddress32 bufferPage[7];
    volatile USBPhysicalAddress32 extBufferPage[7];
};
typedef EHCIIsochTransferDescriptorShared *EHCIIsochTransferDescriptorSharedPtr;

struct EHCISplitIsochTransferDescriptorShared
{
    volatile USBPhysicalAddress32 nextLink;
    volatile UInt32 transaction[2];
    volatile USBPhysicalAddress32 bufferPage[2];
    volatile USBPhysicalAddress32 backPointer;
    volatile USBPhysicalAddress32 extBufferPage[2];
};
typedef EHCISplitIsochTransferDescriptorShared *EHCISplitIsochTransferDescriptorSharedPtr;

#endif
