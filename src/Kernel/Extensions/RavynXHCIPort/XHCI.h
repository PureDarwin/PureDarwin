/*
 * XHCI register/TRB/context definitions, from Intel xHCI spec rev 1.2.
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

#ifndef _XHCI_H
#define _XHCI_H

#include <IOKit/IOTypes.h>

// capability registers (BAR0 + 0x00)
#define XHCI_CAPLENGTH   0x00   // u8: offset to operational regs
#define XHCI_HCIVERSION  0x02   // u16
#define XHCI_HCSPARAMS1  0x04
#define XHCI_HCSPARAMS2  0x08
#define XHCI_HCSPARAMS3  0x0C
#define XHCI_HCCPARAMS1  0x10
#define XHCI_DBOFF       0x14   // offset to doorbell array
#define XHCI_RTSOFF      0x18   // offset to runtime regs
#define XHCI_HCCPARAMS2  0x1C

#define XHCI_HCSP1_MAXSLOTS(v)   ((v) & 0xFF)
#define XHCI_HCSP1_MAXINTRS(v)   (((v) >> 8) & 0x7FF)
#define XHCI_HCSP1_MAXPORTS(v)   (((v) >> 24) & 0xFF)

#define XHCI_HCCP1_AC64(v)       ((v) & 1)              // 64-bit addressing
#define XHCI_HCCP1_CSZ(v)        (((v) >> 2) & 1)        // context size: 0=32B 1=64B
#define XHCI_HCCP1_XECP(v)       (((v) >> 16) & 0xFFFF)  // extended caps ptr, in 32b words

// operational registers (BAR0 + CAPLENGTH)
#define XHCI_USBCMD      0x00
#define XHCI_USBSTS      0x04
#define XHCI_PAGESIZE    0x08
#define XHCI_DNCTRL      0x14
#define XHCI_CRCR        0x18   // 64-bit
#define XHCI_DCBAAP      0x30   // 64-bit
#define XHCI_CONFIG      0x38

#define XHCI_USBCMD_RS    (1U << 0)  // run/stop
#define XHCI_USBCMD_HCRST (1U << 1)  // host controller reset
#define XHCI_USBCMD_INTE  (1U << 2)  // interrupter enable

#define XHCI_USBSTS_HCH   (1U << 0)  // HC halted
#define XHCI_USBSTS_HSE   (1U << 2)  // host system error
#define XHCI_USBSTS_EINT  (1U << 3)  // event interrupt, RW1C
#define XHCI_USBSTS_CNR   (1U << 11) // controller not ready
#define XHCI_USBSTS_HCE   (1U << 12) // host controller error

#define XHCI_CRCR_RCS     (1ULL << 0) // ring cycle state
#define XHCI_CRCR_CS      (1ULL << 1) // command stop
#define XHCI_CRCR_CA      (1ULL << 2) // command abort
#define XHCI_CRCR_CRR     (1ULL << 3) // command ring running

// per-port registers (BAR0 + CAPLENGTH + 0x400 + port*0x10), 0-based here, the spec numbers ports from 1
#define XHCI_PORTREGS_BASE 0x400
#define XHCI_PORTREGS_SIZE 0x10

#define XHCI_PORTSC   0x00
#define XHCI_PORTPMSC 0x04
#define XHCI_PORTLI   0x08

#define XHCI_PORTSC_CCS   (1U << 0)  // current connect status
#define XHCI_PORTSC_PED   (1U << 1)  // port enabled/disabled
#define XHCI_PORTSC_PR    (1U << 4)  // port reset
#define XHCI_PORTSC_PLS(v) (((v) >> 5) & 0xF) // port link state
#define XHCI_PORTSC_PP    (1U << 9)  // port power
#define XHCI_PORTSC_SPEED(v) (((v) >> 10) & 0xF)
#define XHCI_PORTSC_PRC   (1U << 21) // port reset change
#define XHCI_PORTSC_CSC   (1U << 17) // connect status change
#define XHCI_PORTSC_CAS   (1U << 24) // cold attach status: warm reset required
#define XHCI_PORTSC_WPR   (1U << 31) // warm port reset (SuperSpeed only)
// RW1CS bits written as 0 in a read-modify-write so they aren't cleared
#define XHCI_PORTSC_RW1CS (XHCI_PORTSC_CSC | (1U<<18) | (1U<<19) | (1U<<20) | \
                           XHCI_PORTSC_PRC | (1U<<22) | (1U<<23))
// writing PED as 1 disables the port, so a read-modify-write that clears change bits must
// force these bits to 0
#define XHCI_PORTSC_WRITEBACK_CLEAR (XHCI_PORTSC_RW1CS | XHCI_PORTSC_PED)

// extended capabilities (BAR0 + XECP*4), a linked list relative to the MMIO base, unlike the operational regs
#define XHCI_XECP_ID(dw0)          ((dw0) & 0xFF)
#define XHCI_XECP_NEXT(dw0)        (((dw0) >> 8) & 0xFF)   // in 32-bit DWORDs, 0 = end of list
#define XHCI_XECP_ID_LEGACY_SUPPORT      1
#define XHCI_XECP_ID_SUPPORTED_PROTOCOL  2

// USB legacy support capability
#define XHCI_LEGACY_BIOS_OWNED     (1U << 16)
#define XHCI_LEGACY_OS_OWNED       (1U << 24)
#define XHCI_LEGACY_CONTROL_STATUS 0x04

// supported protocol capability (xhci 1.2 7.2), one per USB major revision, each covering a range of root ports
#define XHCI_SP_MAJOR_REV(dw0)     (((dw0) >> 24) & 0xFF)  // 2 = USB2, 3 = USB3
#define XHCI_SP_PORT_OFFSET(dw2)   ((dw2) & 0xFF)          // 1-based
#define XHCI_SP_PORT_COUNT(dw2)    (((dw2) >> 8) & 0xFF)

// runtime registers (BAR0 + RTSOFF)
#define XHCI_RT_IR0        0x20   // interrupter register set 0
#define XHCI_IR_IMAN       0x00
#define XHCI_IR_IMOD       0x04
#define XHCI_IR_ERSTSZ     0x08
#define XHCI_IR_ERSTBA     0x10   // 64-bit
#define XHCI_IR_ERDP       0x18   // 64-bit

#define XHCI_IMAN_IP   (1U << 0) // interrupt pending
#define XHCI_IMAN_IE   (1U << 1) // interrupt enable

#define XHCI_ERDP_EHB  (1ULL << 3) // event handler busy

// doorbell array (BAR0 + DBOFF), one u32 per slot (0 = command ring)
#define XHCI_DB_TARGET_CONTROL_EP0  1  // control endpoint doorbell target

// TRB (transfer request block): 16 bytes
typedef struct {
    UInt64 param;
    UInt32 status;
    UInt32 control;
} __attribute__((packed)) XHCITRB;

#define TRB_CYCLE          (1U << 0)
#define TRB_TC             (1U << 1)  // toggle cycle (link TRB) / evaluate next TRB
#define TRB_ENT             (1U << 1) // evaluate next TRB (transfer TRBs)
#define TRB_ISP            (1U << 2)  // interrupt on short packet
#define TRB_TD_SIZE(n)     ((UInt32)(n) << 17) // normal TRB status[21:17], packets left after this TRB
#define TRB_CH             (1U << 4)  // chain bit
#define TRB_IOC            (1U << 5)  // interrupt on completion
#define TRB_IDT            (1U << 6)  // immediate data (setup stage)
#define TRB_TYPE_SHIFT     10
#define TRB_TYPE(v)        (((v) >> TRB_TYPE_SHIFT) & 0x3F)
#define TRB_SET_TYPE(t)    ((UInt32)(t) << TRB_TYPE_SHIFT)

#define TRB_SLOT_SHIFT     24
#define TRB_SET_SLOT(s)    ((UInt32)(s) << TRB_SLOT_SHIFT)
#define TRB_GET_SLOT(v)    (((v) >> TRB_SLOT_SHIFT) & 0xFF)
#define TRB_EP_SHIFT       16
#define TRB_SET_EP(e)      ((UInt32)(e) << TRB_EP_SHIFT)

// TRB types
enum {
    TRB_TYPE_NORMAL           = 1,
    TRB_TYPE_SETUP_STAGE      = 2,
    TRB_TYPE_DATA_STAGE       = 3,
    TRB_TYPE_STATUS_STAGE     = 4,
    TRB_TYPE_LINK             = 6,
    TRB_TYPE_ENABLE_SLOT      = 9,
    TRB_TYPE_DISABLE_SLOT     = 10,
    TRB_TYPE_ADDRESS_DEVICE   = 11,
    TRB_TYPE_CONFIGURE_EP     = 12,
    TRB_TYPE_EVALUATE_CONTEXT = 13,
    TRB_TYPE_RESET_EP         = 14,
    TRB_TYPE_STOP_EP          = 15,
    TRB_TYPE_SET_TR_DEQUEUE   = 16,
    TRB_TYPE_RESET_DEVICE     = 17,
    TRB_TYPE_NOOP_CMD         = 23,
    TRB_TYPE_TRANSFER_EVENT   = 32,
    TRB_TYPE_CMD_COMPLETION   = 33,
    TRB_TYPE_PORT_STATUS_CHG  = 34,
};

// completion codes (event TRB status[31:24])
#define TRB_CC(status)       (((status) >> 24) & 0xFF)
#define TRB_CC_SUCCESS        1
#define TRB_CC_TRANSACTION    4
#define TRB_CC_STALL          6
#define TRB_CC_SHORT_PACKET   13
#define TRB_CC_CONTEXT_STATE  19

// setup stage TRT (transfer type) field, control[17:16]
#define TRB_SETUP_TRT_NO_DATA     0
#define TRB_SETUP_TRT_OUT_DATA    2
#define TRB_SETUP_TRT_IN_DATA     3
#define TRB_SETUP_TRT_SHIFT       16

// data/status stage direction bit, control[16]
#define TRB_DIR_IN         (1U << 16)

// address device BSR (block set address request), control[9]. BSR=1 sets up EP0 without sending
// SET_ADDRESS, a second command with BSR=0 assigns the address once bMaxPacketSize0 is known
#define TRB_ADDR_DEV_BSR   (1U << 9)

// slot context (first 32B of a 32B-context device context)
typedef struct {
    UInt32 dword0;   // route string[19:0], speed[23:20], ctx entries[31:27]
    UInt32 dword1;   // max exit latency[15:0], root hub port[23:16], num ports[31:24]
    UInt32 dword2;   // target slot? interrupter target[31:22]
    UInt32 dword3;   // usb device address[7:0], slot state[31:27]
    UInt32 reserved[4];
} __attribute__((packed)) XHCISlotContext;

#define SLOT_CTX_ENTRIES_SHIFT   27
#define SLOT_CTX_SPEED_SHIFT     20
#define SLOT_CTX_ROOTPORT_SHIFT  16
// dword0: hub/MTT flags for devices that are themselves USB hubs
#define SLOT_CTX_MTT_BIT         (1U << 25)
#define SLOT_CTX_HUB_BIT         (1U << 26)
// dword1[31:24]: number of ports (hub bNbrPorts), only meaningful with SLOT_CTX_HUB_BIT
#define SLOT_CTX_NUMPORTS_SHIFT  24
// dword2: parent hub slot[7:0]/parent port[15:8], only for LS/FS behind a HS hub (TT routing),
// and TT think time[17:16], only for hubs
#define SLOT_CTX_PARENT_SLOT_SHIFT 0
#define SLOT_CTX_PARENT_PORT_SHIFT 8
#define SLOT_CTX_TTT_SHIFT         16

// endpoint context
typedef struct {
    UInt32 dword0;   // ep state[2:0], interval[23:16]
    UInt32 dword1;   // ep type[5:3], max packet size[31:16], max burst[15:8]
    UInt64 trDequeuePtr; // + DCS in bit0
    UInt32 avgTrbLen_maxEsitLo;
    UInt32 reserved[3];
} __attribute__((packed)) XHCIEndpointContext;

#define EP_TYPE_CONTROL       4
#define EP_TYPE_BULK_OUT      2
#define EP_TYPE_BULK_IN       6
#define EP_CTX_CERR_SHIFT     1
#define EP_CTX_CERR(c)        ((UInt32)(c) << EP_CTX_CERR_SHIFT)
#define EP_CTX_TYPE_SHIFT     3
#define EP_CTX_MAXPKT_SHIFT   16

// input control context (first 32B of an input context)
typedef struct {
    UInt32 dropFlags;
    UInt32 addFlags;
    UInt32 reserved[5];
    UInt32 configInfo; // configuration value[7:0], interface number[15:8], alt setting[23:16]
} __attribute__((packed)) XHCIInputControlContext;

// layout reference and CSZ=0 sizing only. with 64-byte contexts entries are 64 bytes apart, so
// the driver uses the fContextSize helpers in RavynXHCIPort.h
typedef struct {
    XHCIInputControlContext control;
    XHCISlotContext         slot;
    XHCIEndpointContext     ep[31];
} __attribute__((packed)) XHCIInputContext;

typedef struct {
    XHCISlotContext         slot;
    XHCIEndpointContext     ep[31];
} __attribute__((packed)) XHCIDeviceContext;

// USB standard descriptors/requests (subset)
typedef struct {
    UInt8  bmRequestType;
    UInt8  bRequest;
    UInt16 wValue;
    UInt16 wIndex;
    UInt16 wLength;
} __attribute__((packed)) USBSetupPacket;

#define USB_REQ_GET_DESCRIPTOR   6
#define USB_REQ_SET_CONFIGURATION 9
#define USB_REQ_SET_ADDRESS      5

#define USB_DESC_DEVICE          1
#define USB_DESC_CONFIGURATION   2

typedef struct {
    UInt8  bLength;
    UInt8  bDescriptorType;
    UInt16 bcdUSB;
    UInt8  bDeviceClass;
    UInt8  bDeviceSubClass;
    UInt8  bDeviceProtocol;
    UInt8  bMaxPacketSize0;
    UInt16 idVendor;
    UInt16 idProduct;
    UInt16 bcdDevice;
    UInt8  iManufacturer;
    UInt8  iProduct;
    UInt8  iSerialNumber;
    UInt8  bNumConfigurations;
} __attribute__((packed)) USBDeviceDescriptor;

typedef struct {
    UInt8  bLength;
    UInt8  bDescriptorType;
    UInt16 wTotalLength;
    UInt8  bNumInterfaces;
    UInt8  bConfigurationValue;
    UInt8  iConfiguration;
    UInt8  bmAttributes;
    UInt8  bMaxPower;
} __attribute__((packed)) USBConfigDescriptor;

typedef struct {
    UInt8  bLength;
    UInt8  bDescriptorType; // 4
    UInt8  bInterfaceNumber;
    UInt8  bAlternateSetting;
    UInt8  bNumEndpoints;
    UInt8  bInterfaceClass;
    UInt8  bInterfaceSubClass;
    UInt8  bInterfaceProtocol;
    UInt8  iInterface;
} __attribute__((packed)) USBInterfaceDescriptor;

typedef struct {
    UInt8  bLength;
    UInt8  bDescriptorType; // 5
    UInt8  bEndpointAddress; // bit7: dir, bits3:0: number
    UInt8  bmAttributes;     // bits1:0: transfer type (2=bulk)
    UInt16 wMaxPacketSize;
    UInt8  bInterval;
} __attribute__((packed)) USBEndpointDescriptor;

#define USB_IF_CLASS_MASS_STORAGE   0x08
#define USB_IF_SUBCLASS_SCSI        0x06
#define USB_IF_PROTOCOL_BULK_ONLY   0x50

// xHCI endpoint context type for an interrupt IN endpoint (table 6-9)
#define EP_TYPE_INTERRUPT_IN        7

// USB hub class (0x09), for devices behind onboard hubs. bDeviceProtocol 0/1/2 is a USB2 hub, 3 a
// SuperSpeed hub, the requests match and only the descriptor type and port status layout differ
#define USB_DEV_CLASS_HUB           0x09
#define USB_HUB_PROTO_SUPERSPEED    3

#define USB_DESC_HUB                0x29 // USB2 hub descriptor
#define USB_DESC_HUB_SS             0x2A // USB3 SuperSpeed hub descriptor

// bmRequestType for hub class requests: 0xA0/0x23/0xA3 = class, device/other(port) recipient, in/out
#define USB_HUB_REQTYPE_GET_HUB_DESC   0xA0
#define USB_HUB_REQTYPE_SET_PORT_FEAT  0x23
#define USB_HUB_REQTYPE_CLEAR_PORT_FEAT 0x23
#define USB_HUB_REQTYPE_GET_PORT_STATUS 0xA3
#define USB_HUB_REQTYPE_SET_HUB_DEPTH  0x20 // host-to-device, class, device
#define USB_REQ_SET_HUB_DEPTH          12   // SS-hub-only class request

#define USB_HUB_FEAT_PORT_CONNECTION   0
#define USB_HUB_PORT_CONNECTION        (1U << 0)
#define USB_HUB_FEAT_PORT_RESET        4
#define USB_HUB_FEAT_PORT_POWER        8
#define USB_HUB_FEAT_C_PORT_CONNECTION 16
#define USB_HUB_FEAT_C_PORT_RESET      20
#define USB_HUB_FEAT_C_BH_PORT_RESET   29 // SS hub: warm-reset change

// GET_PORT_STATUS reply: wPortStatus | (wPortChange << 16), only the bits shared by USB2 and USB3 hubs that we use
#define USB_PORTSTATUS_CONNECTION      (1U << 0)
#define USB_PORTSTATUS_ENABLE          (1U << 1)
#define USB_PORTSTATUS_PLS(v)          (((v) >> 5) & 0xF) // SS hubs only: port link state
#define USB_PORTSTATUS_LOW_SPEED       (1U << 9)  // USB2 hubs only
#define USB_PORTSTATUS_HIGH_SPEED      (1U << 10) // USB2 hubs only
#define USB_PORTCHANGE_C_RESET         (1U << 20)
#define USB_PORTCHANGE_C_BH_RESET      (1U << 21) // SS hubs only: warm-reset change
#define USB_PORT_LINK_STATE_U0         0

#define USB_EP_DIR_IN    0x80
#define USB_EP_ADDR_MASK 0x0F
#define USB_EP_TYPE_MASK 0x03
#define USB_EP_TYPE_BULK 0x02

// USB bulk-only transport (mass storage)
typedef struct {
    UInt32 dCBWSignature;      // 0x43425355 "USBC"
    UInt32 dCBWTag;
    UInt32 dCBWDataTransferLength;
    UInt8  bmCBWFlags;         // bit7: 1=IN, 0=OUT
    UInt8  bCBWLUN;            // bits3:0
    UInt8  bCBWCBLength;       // bits4:0, 1..16
    UInt8  CBWCB[16];
} __attribute__((packed)) USBBOTCommandBlockWrapper;

#define BOT_CBW_SIGNATURE 0x43425355U
#define BOT_CSW_SIGNATURE 0x53425355U

typedef struct {
    UInt32 dCSWSignature;
    UInt32 dCSWTag;
    UInt32 dCSWDataResidue;
    UInt8  bCSWStatus;         // 0=pass 1=fail 2=phase error
} __attribute__((packed)) USBBOTCommandStatusWrapper;

// xhci intervals are powers of two microframes, fs/ls descriptors count frames
static inline uint8_t
pdUSBInterruptInterval(uint32_t speed, uint8_t interval)
{
    if (speed >= 3) {
        if (interval < 1) interval = 1;
        if (interval > 16) interval = 16;
        return interval - 1;
    }
    if (!interval) interval = 1;
    uint8_t exponent = 3;
    while (interval > 1) {
        interval >>= 1;
        ++exponent;
    }
    return exponent;
}

static inline uint32_t
pdUSBTransferredLength(uint32_t requested, uint32_t residual)
{
    return residual <= requested ? requested - residual : 0;
}


// a route's next nibble selects a hub port, the mask identifies its subtree
static inline bool
pdUSBChildRoute(uint32_t parent, uint8_t port, uint32_t *route, uint32_t *mask)
{
    if (!port || port > 15 || !route || !mask) return false;
    for (uint32_t depth = 0; depth < 5; ++depth) {
        if (((parent >> (depth * 4)) & 15) == 0) {
            *route = parent | ((uint32_t)port << (depth * 4));
            *mask = (1U << ((depth + 1) * 4)) - 1;
            return true;
        }
    }
    return false;
}

#endif // _XHCI_H
