/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2000 Apple Computer, Inc.  All rights reserved. 
 *
 * Intel PIIX/PIIX3/PIIX4 PCI IDE controller.
 * PIIX = PCI-ISA-IDE-Xelerator. (also USB on newer controllers)
 *
 * Notes:
 * 
 * PIIX  introduced in the "Triton" chipset.
 * PIIX3 supports different timings for Master/Slave devices on both channels.
 * PIIX4 adds support for Ultra DMA/33.
 *
 * Be sure to download and read the PIIX errata from Intel's web site at
 * developer.intel.com.
 *
 * HISTORY:
 *
 */

#ifndef _APPLEINTELPIIXATAHW_H
#define _APPLEINTELPIIXATAHW_H

/*
 * PCI Device/Vendor ID for supported PIIX ATA devices.
 */
enum {
    kPCI_ID_PIIX   = 0x12308086,
    kPCI_ID_PIIX3  = 0x70108086,
    kPCI_ID_PIIX4  = 0x71118086,
    kPCI_ID_ICH    = 0x24118086,
    kPCI_ID_ICH0   = 0x24218086,
    kPCI_ID_ICH2_M = 0x244a8086,
    kPCI_ID_ICH2   = 0x244b8086,
    kPCI_ID_NONE   = 0xffffffff
};

/*
 * I/O port addresses for primary and secondary channels.
 */
enum {
    kPIIX_P_CMD_ADDR = 0x1f0,
    kPIIX_P_CTL_ADDR = 0x3f4,
    kPIIX_S_CMD_ADDR = 0x170,
    kPIIX_S_CTL_ADDR = 0x374
};

/*
 * IRQ assigned to primary and secondary channels.
 */
enum {
    kPIIX_P_IRQ = 14,
    kPIIX_S_IRQ = 15
};

/*
 * PIIX supports two ATA channels.
 */
enum {
    kPIIX_CHANNEL_PRIMARY = 0,
    kPIIX_CHANNEL_SECONDARY
};

/*
 * PIIX power management states.
 */
enum {
    kPIIXPowerStateOff = 0,
    kPIIXPowerStateDoze,
    kPIIXPowerStateOn,
    kPIIXPowerStateCount
};

/*
 * PIIX ATA PCI config space registers.
 * Register size (bits) in parenthesis.
 */
enum {
    kPIIX_PCI_CFID       = 0x00,   // (32) PCI Device/Vendor ID
    kPIIX_PCI_PCICMD     = 0x04,   // (16) PCI command register
    kPIIX_PCI_PCISTS     = 0x06,   // (16) PCI device status register
    kPIIX_PCI_RID        = 0x08,   // (8)  Revision ID register
    kPIIX_PCI_PI         = 0x09,   // (8)  Programming interface
    kPIIX_PCI_MLT        = 0x0d,   // (8)  Master latency timer register
    kPIIX_PCI_HEDT       = 0x0e,   // (8)  Header type register
    kPIIX_PCI_BMIBA      = 0x20,   // (32) Bus-Master base address
    kPIIX_PCI_IDETIM     = 0x40,   // (16) IDE timing registers (pri)
    kPIIX_PCI_IDETIM_S   = 0x42,   // (16) IDE timing registers (sec)
    kPIIX_PCI_SIDETIM    = 0x44,   // (8)  Slave IDE timing register
    kPIIX_PCI_UDMACTL    = 0x48,   // (8)  Ultra DMA/33 control register
    kPIIX_PCI_UDMATIM    = 0x4a,   // (16) Ultra DMA/33 timing register
    kPIIX_PCI_IDECONFIG  = 0x54,   // (16) IDE I/O Config register
    kPIIX_PCI_MAP        = 0x90,   // (8)  SATA port mapping register
    kPIIX_PCI_PCS        = 0x92    // (8)  SATA port control and status
};

#define kPIIX_PCI_BMIBA_RTE     0x01    // resource type indicator (I/O)
#define kPIIX_PCI_BMIBA_MASK    0xfff0  // base address mask
#define kPIIX_PCI_PCICMD_IOSE   0x01    // I/O space enable
#define kPIIX_PCI_PCICMD_BME    0x04    // bus-master enable

/* PCI Programming Interface register */
#define kPIIX_PCI_PRI_NATIVE_ENABLED       0x01
#define kPIIX_PCI_PRI_NATIVE_SUPPORTED     0x02
#define kPIIX_PCI_SEC_NATIVE_ENABLED       0x04
#define kPIIX_PCI_SEC_NATIVE_SUPPORTED     0x08
#define kPIIX_PCI_BUS_MASTER_SUPPORTED     0x80

#define kPIIX_PCI_PRI_NATIVE_MASK         (kPIIX_PCI_PRI_NATIVE_ENABLED | \
                                           kPIIX_PCI_PRI_NATIVE_SUPPORTED)

#define kPIIX_PCI_SEC_NATIVE_MASK         (kPIIX_PCI_SEC_NATIVE_ENABLED | \
                                           kPIIX_PCI_SEC_NATIVE_SUPPORTED)

/*
 * PIIX/ATA PCI configuration space register definition.
 *
 * PIIX_IDETIM - IDE timing register.
 *
 * Address:
 * 0x40:0x41 - Primary channel
 * 0x42:0x43 - Secondary channel
 */
#define kPIIX_PCI_IDETIM_IDE           0x8000   // IDE decode enable
#define kPIIX_PCI_IDETIM_SITRE         0x4000   // slave timing register enable

#define kPIIX_PCI_IDETIM_ISP_MASK      0x3000
#define kPIIX_PCI_IDETIM_ISP_SHIFT     12
#define kPIIX_PCI_IDETIM_ISP_5         0x0000   // IORDY sample point
#define kPIIX_PCI_IDETIM_ISP_4         0x1000   // (PCI clocks)
#define kPIIX_PCI_IDETIM_ISP_3         0x2000
#define kPIIX_PCI_IDETIM_ISP_2         0x3000

#define kPIIX_PCI_IDETIM_RTC_MASK      0x0300
#define kPIIX_PCI_IDETIM_RTC_SHIFT     8
#define kPIIX_PCI_IDETIM_RTC_4         0x0000   // recovery time (PCI clocks)
#define kPIIX_PCI_IDETIM_RTC_3         0x0100
#define kPIIX_PCI_IDETIM_RTC_2         0x0200
#define kPIIX_PCI_IDETIM_RTC_1         0x0300

#define kPIIX_PCI_IDETIM_DTE1          0x0080   // DMA timing enable only
#define kPIIX_PCI_IDETIM_PPE1          0x0040   // prefetch and posting enabled
#define kPIIX_PCI_IDETIM_IE1           0x0020   // IORDY sample point enable
#define kPIIX_PCI_IDETIM_TIME1         0x0010   // fast timing enable
#define kPIIX_PCI_IDETIM_DTE0          0x0008   // same as above for drive 0
#define kPIIX_PCI_IDETIM_PPE0          0x0004
#define kPIIX_PCI_IDETIM_IE0           0x0002
#define kPIIX_PCI_IDETIM_TIME0         0x0001

/*
 * PIIX/ATA PCI configuration space register definition.
 *
 * PIIX_SIDETIM - Slave IDE timing register.
 *
 * Address: 0x44
 */
#define kPIIX_PCI_SIDETIM_SISP1_MASK   0xc0
#define kPIIX_PCI_SIDETIM_SISP1_SHIFT  6
#define kPIIX_PCI_SIDETIM_SRTC1_MASK   0x30
#define kPIIX_PCI_SIDETIM_SRTC1_SHIFT  4
#define kPIIX_PCI_SIDETIM_PISP1_MASK   0x0c
#define kPIIX_PCI_SIDETIM_PISP1_SHIFT  2
#define kPIIX_PCI_SIDETIM_PRTC1_MASK   0x03
#define kPIIX_PCI_SIDETIM_PRTC1_SHIFT  0

/*
 * PIIX/ATA PCI configuration space register definition.
 *
 * PIIX_UDMACTL - Ultra DMA/33 control register
 *
 * Address: 0x48
 */
#define kPIIX_PCI_UDMACTL_SSDE1        0x08    // Enable UDMA/33 Sec/Drive1
#define kPIIX_PCI_UDMACTL_SSDE0        0x04    // Enable UDMA/33 Sec/Drive0
#define kPIIX_PCI_UDMACTL_PSDE1        0x02    // Enable UDMA/33 Pri/Drive1
#define kPIIX_PCI_UDMACTL_PSDE0        0x01    // Enable UDMA/33 Pri/Drive0

/*
 * PIIX/ATA PCI configuration space register definition.
 *
 * PIIX_UDMATIM - Ultra DMA/33 timing register
 *
 * Address: 0x4a-0x4b
 */
#define kPIIX_PCI_UDMATIM_PCT0_MASK    0x0003
#define kPIIX_PCI_UDMATIM_PCT0_SHIFT   0
#define kPIIX_PCI_UDMATIM_PCT1_MASK    0x0030
#define kPIIX_PCI_UDMATIM_PCT1_SHIFT   4
#define kPIIX_PCI_UDMATIM_SCT0_MASK    0x0300
#define kPIIX_PCI_UDMATIM_SCT0_SHIFT   8
#define kPIIX_PCI_UDMATIM_SCT1_MASK    0x3000
#define kPIIX_PCI_UDMATIM_SCT1_SHIFT   12

/*
 * PIIX/ATA PCI configuration space register definition.
 *
 * IDE I/O Configuration register
 *
 * Address: 0x54-0x55
 */
#define kPIIX_PCI_IDECONFIG_PCB0       0x0001
#define kPIIX_PCI_IDECONFIG_PCB1       0x0002
#define kPIIX_PCI_IDECONFIG_SCB0       0x0004
#define kPIIX_PCI_IDECONFIG_SCB1       0x0008
#define kPIIX_PCI_IDECONFIG_PCR0       0x0010
#define kPIIX_PCI_IDECONFIG_PCR1       0x0020
#define kPIIX_PCI_IDECONFIG_SCR0       0x0040
#define kPIIX_PCI_IDECONFIG_SCR1       0x0080
#define kPIIX_PCI_IDECONFIG_WR_PP_EN   0x0400
#define kPIIX_PCI_IDECONFIG_FAST_PCB0  0x1000
#define kPIIX_PCI_IDECONFIG_FAST_PCB1  0x2000
#define kPIIX_PCI_IDECONFIG_FAST_SCB0  0x4000
#define kPIIX_PCI_IDECONFIG_FAST_SCB1  0x8000

/*
 * ICHx/ATA PCI configuration space register definition.
 *
 * Port control and status register (ICH5/SATA)
 *
 * Address: 0x92
 */
enum {
    kPIIX_PCI_PCS_P0E = 0x01,
    kPIIX_PCI_PCS_P1E = 0x02,
    kPIIX_PCI_PCS_P2E = 0x04,
    kPIIX_PCI_PCS_P3E = 0x08,
    kPIIX_PCI_PCS_P0P = 0x10,
    kPIIX_PCI_PCS_P1P = 0x20,
    kPIIX_PCI_PCS_P2P = 0x40,
    kPIIX_PCI_PCS_P3P = 0x80
};

/*
 * PIIX/ATA IO space register offsets. Base address is set in kPIIX_PCI_BMIBA.
 * Register size (bits) in parenthesis.
 *
 * Note:
 * For the primary channel, the base address is stored in kPIIX_PCI_BMIBA.
 * For the secondary channel, an offset (PIIX_IO_BM_OFFSET) is added to
 * the value stored in kPIIX_PCI_BMIBA.
 */
enum {
    kPIIX_IO_BMICX     = 0x00,    // (8) Bus master command register
    kPIIX_IO_BMISX     = 0x02,    // (8) Bus master status register
    kPIIX_IO_BMIDTPX   = 0x04,    // (32) Descriptor table register
    kPIIX_IO_BM_OFFSET = 0x08,    // fixed offset to channel 1 registers
    kPIIX_IO_BM_MASK   = 0xfff0   // BMIBA mask to get I/O base address
};

/*
 * PIIX/ATA IO space register definition.
 *
 * BMICX - Bus master IDE command register
 */
#define kPIIX_IO_BMICX_SSBM      0x01    // 1=Start, 0=Stop
#define kPIIX_IO_BMICX_RWCON     0x08    // 0=Read, 1=Write (drive not host)

/*
 * PIIX/ATA IO space register definition.
 *
 * PIIX_BMISX - Bus master IDE status register
 */
#define kPIIX_IO_BMISX_DMA1CAP   0x40    // drive 1 is capable of DMA transfers
#define kPIIX_IO_BMISX_DMA0CAP   0x20    // drive 0 is capable of DMA transfers
#define kPIIX_IO_BMISX_IDEINTS   0x04    // IDE device asserted its interrupt
#define kPIIX_IO_BMISX_ERROR     0x02    // DMA error (cleared by writing a 1)
#define kPIIX_IO_BMISX_BMIDEA    0x01    // bus master active bit

/*
 * ICHx ATA channel can support several modes of operation.
 * SATA modes added for ICH5. Devices before ICH5 only support
 * kChannelModePATA. ICH6 added two more SATA ports 2 and 3,
 * but ICH6-M only has ports 0 and 2 (missing 1 and 3).
 */
enum {
    kChannelModeDisabled,
    kChannelModePATA,
    kChannelModeSATAPort0,
    kChannelModeSATAPort1,
    kChannelModeSATAPort01,
    kChannelModeSATAPort10,
    kChannelModeSATAPort02,
    kChannelModeSATAPort13,
    kChannelModeCount
};

/*
 * Serial ATA controllers define ports for point-to-point connection to
 * serial ATA devices.
 */
enum {
    kSerialATAPort0,
    kSerialATAPort1,
    kSerialATAPort2,
    kSerialATAPort3,
    kSerialATAPortX  /* invalid port */
};

#endif /* !_APPLEINTELPIIXATAHW_H */
