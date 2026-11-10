// SPDX-License-Identifier: MIT
// msm8916 emmc controller: init, card identify, block transfers
#include "PDQcomSDHCI.h"
#include "PDQcomSDHCIDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <pexpert/pexpert.h>

#define super IOService
OSDefineMetaClassAndStructors(PDQcomSDHCI, IOService);

// the vendor core block (reg 1): which interface the controller speaks
#define kCORE_HC_MODE		0x78
#define kHC_MODE_EN		(1u << 0)	// SDHCI rather than Qualcomm's legacy MCI interface
#define kFF_CLK_SW_RST_DIS	(1u << 13)	// set with the SDHCI mode, lk2nd platform/msm_shared/sdhci_msm.c
#define kCORE_POWER		0x00
#define kCORE_SW_RST		(1u << 7)
// the power control handshake: a bus power or signalling change waits for an ack here
#define kCORE_PWRCTL_STATUS	0xdc
#define kCORE_PWRCTL_MASK	0xe0
#define kCORE_PWRCTL_CLEAR	0xe4
#define kCORE_PWRCTL_CTL	0xe8
#define kPWRCTL_BUS_OFF		(1u << 0)
#define kPWRCTL_BUS_ON		(1u << 1)
#define kPWRCTL_IO_LOW		(1u << 2)
#define kPWRCTL_IO_HIGH		(1u << 3)
#define kPWRCTL_BUS_OK		(1u << 0)
#define kPWRCTL_IO_OK		(1u << 2)
// the vendor register in the hc block, back to its reset value as lk does
#define kVENDOR_SPEC		0x10c
#define kVENDOR_SPEC_POR	0xa1c

// standard sdhci registers (reg 0)
#define kBLKSIZE		0x04
#define kBLKCNT			0x06
#define kARG			0x08
#define kXFER_MODE		0x0c
#define kXFER_BLKCNT_EN		(1u << 1)
#define kXFER_AUTO_CMD12	(1u << 2)
#define kXFER_READ		(1u << 4)
#define kXFER_MULTI		(1u << 5)
#define kCMD			0x0e
#define kCMD_RESP_136		0x01
#define kCMD_RESP_48		0x02
#define kCMD_RESP_48_BUSY	0x03
#define kCMD_CRC		(1u << 3)
#define kCMD_INDEX		(1u << 4)
#define kCMD_DATA		(1u << 5)
#define kRESP0			0x10
#define kBUFFER			0x20
#define kPRESENT		0x24
#define kPRESENT_CMD_INHIBIT	(1u << 0)
#define kPRESENT_DAT_INHIBIT	(1u << 1)
#define kPRESENT_DAT0		(1u << 20)
#define kHOST_CTRL		0x28
#define kHOST_CTRL_4BIT		(1u << 1)
#define kHOST_CTRL_HS		(1u << 2)
#define kHOST_CTRL_DMA_MASK	(3u << 3)
#define kHOST_CTRL_8BIT		(1u << 5)
#define kCLOCK			0x2c
#define kCLOCK_INT_EN		(1u << 0)
#define kCLOCK_INT_STABLE	(1u << 1)
#define kCLOCK_CARD_EN		(1u << 2)
#define kTIMEOUT		0x2e
#define kPOWER			0x29
#define kPOWER_ON		(1u << 0)
#define kPOWER_180		(5u << 1)
#define kSW_RESET		0x2f
#define kRESET_ALL		(1u << 0)
#define kRESET_CMD		(1u << 1)
#define kRESET_DATA		(1u << 2)
#define kINT_STATUS		0x30
#define kINT_CMD_DONE		(1u << 0)
#define kINT_XFER_DONE		(1u << 1)
#define kINT_WRITE_READY	(1u << 4)
#define kINT_READ_READY		(1u << 5)
#define kINT_ERROR		(1u << 15)
#define kINT_ENABLE		0x34
#define kINT_SIGNAL		0x38
#define kHOST_CTRL2		0x3e
#define kCAPS			0x40

// command flags for command()
#define kRsp			(1u << 0)
#define kRsp136			(1u << 1)
#define kRspCrc			(1u << 2)
#define kRspBusy		(1u << 3)
#define kData			(1u << 4)
#define kDataWrite		(1u << 5)
#define kRspIndex		(1u << 6)
#define kR1			(kRsp | kRspCrc | kRspIndex)
#define kR1b			(kRsp | kRspCrc | kRspIndex | kRspBusy)
#define kR2			(kRsp | kRsp136 | kRspCrc)
#define kR3			(kRsp)

// mmc commands
#define kCMD_GO_IDLE		0
#define kCMD_SEND_OP_COND	1
#define kCMD_ALL_SEND_CID	2
#define kCMD_SET_REL_ADDR	3
#define kCMD_SWITCH		6
#define kCMD_SELECT_CARD	7
#define kCMD_SEND_EXT_CSD	8
#define kCMD_SEND_CSD		9
#define kCMD_SEND_STATUS	13
#define kCMD_SET_BLOCKLEN	16
#define kCMD_READ_SINGLE	17
#define kCMD_READ_MULTI		18
#define kCMD_WRITE_SINGLE	24
#define kCMD_WRITE_MULTI	25

#define kEXT_CSD_BUS_WIDTH	183
#define kEXT_CSD_SEC_COUNT	212
#define kOCR_BUSY		(1u << 31)
#define kOCR_SECTOR_MODE	(1u << 30)
#define kSTATUS_READY		(1u << 8)
#define kSTATUS_SWITCH_ERROR	(1u << 7)
#define kSTATUS_STATE(s)	(((s) >> 9) & 0xf)
#define kSTATE_TRAN		4

#define kIdentClockHz		400000
#define kTransferClockHz	25000000
#define kMaxBlocksPerCmd	128
#define kRCA			(2u << 16)	// the address this host gives the card
// the first block of pdroot in the stick's GPT (xnu-port/gpt/gpt_pdroot.bin): below it
// sit the boot chain, the modem calibration and Debian
#define kDefaultWriteFrom	5378048ULL

// sdhc_1 when the device tree gives no memory: core at the page base, hc 0x900 into it
#define kSDHC1_PHYS		0x07824000u
#define kSDHC1_HC_OFFSET	0x900

bool
PDQcomSDHCI::start(IOService *provider)
{
	if (!super::start(provider))
		return false;

	fRegMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	fCoreMap = provider->mapDeviceMemoryWithIndex(1, kIOMapInhibitCache);
	if (fRegMap != NULL && fCoreMap != NULL) {
		fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();
		fCore = (volatile uint8_t *)fCoreMap->getVirtualAddress();
	} else {
		// nothing published for the node (as with the usb core): both blocks share one page
		OSSafeReleaseNULL(fRegMap);
		OSSafeReleaseNULL(fCoreMap);
		IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(kSDHC1_PHYS, 0x1000, kIODirectionInOut);
		fRegMap = md != NULL ? md->map(kIOMapInhibitCache) : NULL;
		OSSafeReleaseNULL(md);
		if (fRegMap == NULL) {
			IOLog("PDQcomSDHCI: cannot map the hc and core register blocks\n");
			return false;
		}
		fCore = (volatile uint8_t *)fRegMap->getVirtualAddress();
		fRegs = fCore + kSDHC1_HC_OFFSET;
	}

	fLock = IOLockAlloc();
	fBounce = (uint32_t *)IOMalloc(kMaxBlocksPerCmd * 512);
	if (fLock == NULL || fBounce == NULL)
		return false;
	fWriteFrom = kDefaultWriteFrom;
	PE_parse_boot_argn("pdemmcwritefrom", &fWriteFrom, sizeof(fWriteFrom));

	if (!bringUpController() || !identifyCard()) {
		IOLog("PDQcomSDHCI: no usable eMMC\n");
		return false;
	}
	IOLog("PDQcomSDHCI: %llu blocks of 512 bytes (%llu MB), %u-bit bus, writes allowed from block %llu\n",
	    fBlockCount, (fBlockCount * 512) / (1024 * 1024), fBusWidth, fWriteFrom);

	fDisk = new PDQcomSDHCIDisk;
	if (fDisk == NULL || !fDisk->initWithController(this)) {
		OSSafeReleaseNULL(fDisk);
		return false;
	}
	if (!fDisk->attach(this)) {
		OSSafeReleaseNULL(fDisk);
		return false;
	}
	fDisk->registerService();
	registerService();
	return true;
}

void
PDQcomSDHCI::stop(IOService *provider)
{
	if (fDisk != NULL) {
		fDisk->detach(this);
		OSSafeReleaseNULL(fDisk);
	}
	super::stop(provider);
}

void
PDQcomSDHCI::free(void)
{
	if (fBounce != NULL) {
		IOFree(fBounce, kMaxBlocksPerCmd * 512);
		fBounce = NULL;
	}
	if (fLock != NULL) {
		IOLockFree(fLock);
		fLock = NULL;
	}
	OSSafeReleaseNULL(fRegMap);
	OSSafeReleaseNULL(fCoreMap);
	super::free();
}

// acks whatever the power control block is waiting on, true when there was something
bool
PDQcomSDHCI::pwrctlService(void)
{
	uint32_t st = core32(kCORE_PWRCTL_STATUS), ack = 0;

	if (st == 0)
		return false;
	setCore32(kCORE_PWRCTL_CLEAR, st);
	if (st & (kPWRCTL_BUS_ON | kPWRCTL_BUS_OFF))
		ack |= kPWRCTL_BUS_OK;
	if (st & (kPWRCTL_IO_LOW | kPWRCTL_IO_HIGH))
		ack |= kPWRCTL_IO_OK;
	setCore32(kCORE_PWRCTL_CTL, ack);
	IOLog("PDQcomSDHCI: pwrctl status %x acked %x\n", st, ack);
	return true;
}

// waits up to ms for a power control request, acking it
bool
PDQcomSDHCI::pwrctlWait(uint32_t ms)
{
	for (uint32_t i = 0; i < ms * 10; i++) {
		if (pwrctlService())
			return true;
		IODelay(100);
	}
	return false;
}

bool
PDQcomSDHCI::reset(uint8_t mask)
{
	wr8(kSW_RESET, mask);
	for (int i = 0; i < 10000; i++) {
		// a full reset turns the bus off, which the controller holds until acked
		pwrctlService();
		if ((rd8(kSW_RESET) & mask) == 0)
			return true;
		IODelay(10);
	}
	IOLog("PDQcomSDHCI: reset %x timed out\n", mask);
	return false;
}

// lk's sdhci_msm_init: core reset, sdhci mode, power on through the handshake
bool
PDQcomSDHCI::bringUpController(void)
{
	IOLog("PDQcomSDHCI: hc_mode 0x%08x present 0x%08x host_ctrl 0x%08x caps 0x%08x vendor 0x%08x\n",
	    core32(kCORE_HC_MODE), rd32(kPRESENT), rd32(kHOST_CTRL), rd32(kCAPS), rd32(kVENDOR_SPEC));

	setCore32(kCORE_HC_MODE, core32(kCORE_HC_MODE) & ~kHC_MODE_EN);
	setCore32(kCORE_POWER, core32(kCORE_POWER) | kCORE_SW_RST);
	IODelay(1000);
	setCore32(kCORE_HC_MODE, core32(kCORE_HC_MODE) | kHC_MODE_EN);
	setCore32(kCORE_HC_MODE, core32(kCORE_HC_MODE) | kFF_CLK_SW_RST_DIS);
	setCore32(kCORE_PWRCTL_MASK, 0xf);
	wr32(kVENDOR_SPEC, kVENDOR_SPEC_POR);
	IODelay(100);

	if (!reset(kRESET_ALL))
		return false;
	pwrctlWait(5);

	// 1.8 V bus power, acked through the power control block
	wr8(kPOWER, kPOWER_180);
	wr8(kPOWER, kPOWER_180 | kPOWER_ON);
	if (!pwrctlWait(50))
		IOLog("PDQcomSDHCI: no power control request for bus on\n");
	IODelay(1000);

	wr8(kTIMEOUT, 0x0e);
	wr8(kHOST_CTRL, rd8(kHOST_CTRL) & ~(kHOST_CTRL_4BIT | kHOST_CTRL_8BIT | kHOST_CTRL_HS | kHOST_CTRL_DMA_MASK));
	wr16(kHOST_CTRL2, 0);
	// status bits latch without raising the interrupt line, everything here is polled
	wr32(kINT_ENABLE, 0xffffffffu);
	wr32(kINT_SIGNAL, 0);
	wr32(kINT_STATUS, 0xffffffffu);
	if (!setClock(kIdentClockHz))
		return false;
	IOLog("PDQcomSDHCI: up, present 0x%08x power 0x%02x clock 0x%04x\n",
	    rd32(kPRESENT), rd8(kPOWER), rd16(kCLOCK));
	return true;
}

// the sdhci 3.0 10-bit divider from the base clock in the capabilities (200 MHz here)
bool
PDQcomSDHCI::setClock(uint32_t hz)
{
	uint32_t base = ((rd32(kCAPS) >> 8) & 0xff) * 1000000u, n;
	uint16_t clk;

	if (base == 0)
		base = 200000000u;
	n = (base + 2 * hz - 1) / (2 * hz);
	if (n > 0x3ff)
		n = 0x3ff;
	wr16(kCLOCK, 0);
	clk = (uint16_t)(((n & 0xff) << 8) | (((n >> 8) & 0x3) << 6) | kCLOCK_INT_EN);
	wr16(kCLOCK, clk);
	for (int i = 0; i < 2000; i++) {
		if (rd16(kCLOCK) & kCLOCK_INT_STABLE) {
			wr16(kCLOCK, clk | kCLOCK_CARD_EN);
			IODelay(1000);
			return true;
		}
		IODelay(10);
	}
	IOLog("PDQcomSDHCI: clock never stabilised\n");
	return false;
}

bool
PDQcomSDHCI::waitInt(uint32_t bits, uint32_t timeoutMs)
{
	for (uint32_t us = 0; us < timeoutMs * 1000; us += 10) {
		uint32_t st = rd32(kINT_STATUS);
		if (st & kINT_ERROR)
			return false;
		if (st & bits) {
			wr32(kINT_STATUS, st & bits);
			return true;
		}
		IODelay(10);
	}
	return false;
}

bool
PDQcomSDHCI::waitInhibit(uint32_t bits)
{
	for (int i = 0; i < 100000; i++) {
		if ((rd32(kPRESENT) & bits) == 0)
			return true;
		IODelay(10);
	}
	return false;
}

// one buffer ready event per 512 byte block, 128 words through the data port
bool
PDQcomSDHCI::pio(bool write, uint32_t nblks)
{
	uint32_t *p = fBounce;

	for (uint32_t b = 0; b < nblks; b++) {
		if (!waitInt(write ? kINT_WRITE_READY : kINT_READ_READY, 1000))
			return false;
		for (int w = 0; w < 128; w++) {
			if (write)
				*(volatile uint32_t *)(fRegs + kBUFFER) = *p++;
			else
				*p++ = *(volatile uint32_t *)(fRegs + kBUFFER);
		}
	}
	return true;
}

bool
PDQcomSDHCI::command(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp,
    uint32_t nblks)
{
	uint16_t cmd = (uint16_t)(idx << 8);
	uint32_t inhibit = kPRESENT_CMD_INHIBIT;
	bool ok;

	if (flags & kRsp136)
		cmd |= kCMD_RESP_136;
	else if (flags & kRspBusy)
		cmd |= kCMD_RESP_48_BUSY;
	else if (flags & kRsp)
		cmd |= kCMD_RESP_48;
	if (flags & kRspCrc)
		cmd |= kCMD_CRC;
	if (flags & kRspIndex)
		cmd |= kCMD_INDEX;
	if ((flags & (kData | kRspBusy)) != 0)
		inhibit |= kPRESENT_DAT_INHIBIT;

	if (!waitInhibit(inhibit)) {
		IOLog("PDQcomSDHCI: cmd%u stuck behind present %08x\n", idx, rd32(kPRESENT));
		reset(kRESET_CMD | kRESET_DATA);
		return false;
	}
	wr32(kINT_STATUS, 0xffffffffu);

	if (flags & kData) {
		uint16_t mode = 0;

		cmd |= kCMD_DATA;
		if (!(flags & kDataWrite))
			mode |= kXFER_READ;
		if (nblks > 1)
			mode |= kXFER_MULTI | kXFER_BLKCNT_EN | kXFER_AUTO_CMD12;
		wr16(kBLKSIZE, 512);
		wr16(kBLKCNT, (uint16_t)nblks);
		wr16(kXFER_MODE, mode);
	} else {
		wr16(kXFER_MODE, 0);
	}
	wr32(kARG, arg);
	wr16(kCMD, cmd);

	ok = waitInt(kINT_CMD_DONE, 1000);
	if (ok && resp != NULL) {
		if (flags & kRsp136) {
			// the controller drops the crc byte, shift it back so resp[0] holds bits 127:96
			for (int i = 0; i < 4; i++) {
				uint32_t off = kRESP0 + (3 - i) * 4;
				resp[i] = rd32(off) << 8;
				if (i != 3)
					resp[i] |= rd8(off - 1);
			}
		} else {
			resp[0] = rd32(kRESP0);
		}
	}
	if (ok && (flags & kData))
		ok = pio(flags & kDataWrite, nblks);
	if (ok && (flags & (kData | kRspBusy)))
		ok = waitInt(kINT_XFER_DONE, 2000);

	if (!ok) {
		IOLog("PDQcomSDHCI: cmd%u arg %08x failed int %08x present %08x\n",
		    idx, arg, rd32(kINT_STATUS), rd32(kPRESENT));
		reset(kRESET_CMD | kRESET_DATA);
	}
	wr32(kINT_STATUS, 0xffffffffu);
	return ok;
}

// back in the transfer state and ready for data, after a switch or a write
bool
PDQcomSDHCI::waitReady(uint32_t timeoutMs)
{
	uint32_t status = 0;

	for (uint32_t ms = 0; ms < timeoutMs; ms++) {
		if (command(kCMD_SEND_STATUS, fRCA, kR1, &status) &&
		    (status & kSTATUS_READY) && kSTATUS_STATE(status) == kSTATE_TRAN) {
			return (status & kSTATUS_SWITCH_ERROR) == 0;
		}
		IODelay(1000);
	}
	IOLog("PDQcomSDHCI: card not ready, status %08x\n", status);
	return false;
}

// CMD6 writing one EXT_CSD byte
bool
PDQcomSDHCI::switchExtCSD(uint8_t index, uint8_t value)
{
	uint32_t resp[4];

	if (!command(kCMD_SWITCH, (3u << 24) | ((uint32_t)index << 16) | ((uint32_t)value << 8), kR1b, resp))
		return false;
	return waitReady(1000);
}

bool
PDQcomSDHCI::identifyCard(void)
{
	uint32_t resp[4], ocr = 0;
	bool ready = false;

	fRCA = 0;
	// lk2nd left the card asleep: CMD0 is one of the few commands a sleeping card takes
	command(kCMD_GO_IDLE, 0, 0, NULL);
	IODelay(2000);
	for (int i = 0; i < 200; i++) {
		// 1.70-1.95 V and 2.7-3.6 V, sector addressing
		if (!command(kCMD_SEND_OP_COND, 0x40ff8080u, kR3, resp))
			return false;
		ocr = resp[0];
		if (ocr & kOCR_BUSY) {
			ready = true;
			break;
		}
		IOSleep(10);
	}
	if (!ready) {
		IOLog("PDQcomSDHCI: card never left initialisation, ocr %08x\n", ocr);
		return false;
	}
	if ((ocr & kOCR_SECTOR_MODE) == 0) {
		IOLog("PDQcomSDHCI: byte addressed eMMC, not handled\n");
		return false;
	}
	if (!command(kCMD_ALL_SEND_CID, 0, kR2, resp))
		return false;
	if (!command(kCMD_SET_REL_ADDR, kRCA, kR1, resp))
		return false;
	fRCA = kRCA;
	if (!command(kCMD_SEND_CSD, fRCA, kR2, resp))
		return false;
	if (!command(kCMD_SELECT_CARD, fRCA, kR1b, resp))
		return false;

	// capacity from EXT_CSD, the one 512 byte block CMD8 returns
	if (!command(kCMD_SEND_EXT_CSD, 0, kR1 | kData, resp, 1))
		return false;
	const uint8_t *ext = (const uint8_t *)fBounce;
	fBlockCount = (uint64_t)ext[kEXT_CSD_SEC_COUNT] | ((uint64_t)ext[kEXT_CSD_SEC_COUNT + 1] << 8) |
	    ((uint64_t)ext[kEXT_CSD_SEC_COUNT + 2] << 16) | ((uint64_t)ext[kEXT_CSD_SEC_COUNT + 3] << 24);
	if (fBlockCount == 0) {
		IOLog("PDQcomSDHCI: EXT_CSD reports no sectors\n");
		return false;
	}
	if (!command(kCMD_SET_BLOCKLEN, 512, kR1, resp))
		return false;

	// the widest bus that takes: 8 bits (EXT_CSD 2), then 4 (1), else stay at 1
	fBusWidth = 1;
	if (switchExtCSD(kEXT_CSD_BUS_WIDTH, 2)) {
		wr8(kHOST_CTRL, (rd8(kHOST_CTRL) & ~kHOST_CTRL_4BIT) | kHOST_CTRL_8BIT);
		fBusWidth = 8;
	} else if (switchExtCSD(kEXT_CSD_BUS_WIDTH, 1)) {
		wr8(kHOST_CTRL, (rd8(kHOST_CTRL) & ~kHOST_CTRL_8BIT) | kHOST_CTRL_4BIT);
		fBusWidth = 4;
	}
	if (!setClock(kTransferClockHz))
		return false;
	// the protective MBR proves the new width and clock before anything else reads
	if (!command(kCMD_READ_SINGLE, 0, kR1 | kData, resp, 1))
		return false;
	const uint8_t *mbr = (const uint8_t *)fBounce;
	IOLog("PDQcomSDHCI: block 0 signature %02x%02x\n", mbr[510], mbr[511]);
	return mbr[510] == 0x55 && mbr[511] == 0xaa;
}

IOReturn
PDQcomSDHCI::transferBlocks(bool write, UInt64 block, UInt32 nblks,
    IOMemoryDescriptor *buffer, UInt64 bufferOffset)
{
	uint32_t idx, resp[4];
	IOByteCount bytes = (IOByteCount)nblks * 512;

	if (write) {
		idx = nblks > 1 ? kCMD_WRITE_MULTI : kCMD_WRITE_SINGLE;
		if (buffer->readBytes(bufferOffset, fBounce, bytes) != bytes)
			return kIOReturnIOError;
	} else {
		idx = nblks > 1 ? kCMD_READ_MULTI : kCMD_READ_SINGLE;
	}
	if (!command(idx, (uint32_t)block, kR1 | kData | (write ? kDataWrite : 0), resp, nblks))
		return kIOReturnIOError;

	// the card keeps dat0 low while it programs a write, then reports back
	if (write) {
		int ms = 0;
		while ((rd32(kPRESENT) & kPRESENT_DAT0) == 0 && ms++ < 2000)
			IODelay(1000);
		if (ms >= 2000 || !waitReady(2000))
			return kIOReturnIOError;
	}
	if (!write && buffer->writeBytes(bufferOffset, fBounce, bytes) != bytes)
		return kIOReturnIOError;
	return kIOReturnSuccess;
}

IOReturn
PDQcomSDHCI::readWrite(bool write, UInt64 block, UInt64 nblks,
    IOMemoryDescriptor *buffer)
{
	IOReturn ret = kIOReturnSuccess;
	UInt64 done = 0;

	if (fRegs == NULL || buffer == NULL)
		return kIOReturnNoDevice;
	if (block + nblks > fBlockCount)
		return kIOReturnBadArgument;
	if (write && block < fWriteFrom) {
		IOLog("PDQcomSDHCI: refused a write at block %llu (below %llu)\n", block, fWriteFrom);
		return kIOReturnNotWritable;
	}

	IOLockLock(fLock);
	while (done < nblks) {
		UInt32 chunk = (UInt32)IOMin(nblks - done, (UInt64)kMaxBlocksPerCmd);

		ret = transferBlocks(write, block + done, chunk, buffer, done * 512);
		if (ret != kIOReturnSuccess) {
			IOLog("PDQcomSDHCI: %s failed at block %llu\n",
			    write ? "write" : "read", block + done);
			break;
		}
		done += chunk;
	}
	IOLockUnlock(fLock);
	return ret;
}
