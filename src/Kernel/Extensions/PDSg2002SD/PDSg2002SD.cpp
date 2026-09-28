#include "PDSg2002SD.h"
#include "PDSg2002SDDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

#define super IOService
OSDefineMetaClassAndStructors(PDSg2002SD, IOService);

// the bootrom already muxed the pins, powered the slot and clocked the controller
// only offsets below 0x100 and the vendor block at 0x200 exist, anything else hangs the bus

// standard sdhci registers
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
#define kPRESENT_CARD_IN	(1u << 16)
#define kPRESENT_DAT0		(1u << 20)
#define kHOST_CTRL		0x28
#define kHOST_CTRL_4BIT		(1u << 1)
#define kPOWER			0x29
#define kPOWER_ON_33V		0x0f
#define kCLOCK			0x2c
#define kCLOCK_INT_EN		(1u << 0)
#define kCLOCK_INT_STABLE	(1u << 1)
#define kCLOCK_CARD_EN		(1u << 2)
#define kTIMEOUT		0x2e
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

// designware vendor block, the same timing the vendor u-boot and linux program for sd
#define kMSHC_CTRL		0x200
#define kPHY_TX_RX_DLY		0x240
#define kPHY_CONFIG		0x24c

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
#define kR7			(kRsp | kRspCrc | kRspIndex)

// sd commands
#define kCMD_GO_IDLE		0
#define kCMD_ALL_SEND_CID	2
#define kCMD_SEND_REL_ADDR	3
#define kCMD_SELECT_CARD	7
#define kCMD_SEND_IF_COND	8
#define kCMD_SEND_CSD		9
#define kCMD_SET_BLOCKLEN	16
#define kCMD_READ_SINGLE	17
#define kCMD_READ_MULTI		18
#define kCMD_WRITE_SINGLE	24
#define kCMD_WRITE_MULTI	25
#define kCMD_APP		55
#define kACMD_SET_BUS_WIDTH	6
#define kACMD_SD_SEND_OP_COND	41

// the vendor runs the controller from a 375 mhz pll, the caps claim 200
// dividing for the faster one keeps both cases at or under the target
#define kBaseClockHz		375000000u
#define kIdentClockHz		400000
#define kTransferClockHz	25000000
#define kMaxBlocksPerCmd	128

bool
PDSg2002SD::start(IOService *provider)
{

	if (!super::start(provider))
		return false;

	fRegMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	if (fRegMap == NULL) {
		IOLog("PDSg2002SD: cannot map registers\n");
		return false;
	}
	fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();

	fLock = IOLockAlloc();
	fBounce = (uint32_t *)IOMalloc(kMaxBlocksPerCmd * 512);
	if (fLock == NULL || fBounce == NULL)
		return false;

	fReadOnly = false;

	if (!bringUpController() || !identifyCard()) {
		IOLog("PDSg2002SD: no usable card\n");
		return false;
	}

	IOLog("PDSg2002SD: %llu blocks of 512 bytes (%llu MB), %s addressing\n",
	    fBlockCount, (fBlockCount * 512) / (1024 * 1024),
	    fBlockAddressed ? "block" : "byte");

	fDisk = new PDSg2002SDDisk;
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
PDSg2002SD::stop(IOService *provider)
{
	if (fDisk != NULL) {
		fDisk->detach(this);
		OSSafeReleaseNULL(fDisk);
	}
	super::stop(provider);
}

void
PDSg2002SD::free(void)
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
	super::free();
}

bool
PDSg2002SD::reset(uint8_t mask)
{
	wr8(kSW_RESET, mask);
	for (int i = 0; i < 1000; i++) {
		if ((rd8(kSW_RESET) & mask) == 0)
			return true;
		IODelay(10);
	}
	IOLog("PDSg2002SD: reset %x timed out\n", mask);
	return false;
}

// default and high speed timing, without it reads work and every write fails
void
PDSg2002SD::setPhy(void)
{
	wr32(kMSHC_CTRL, rd32(kMSHC_CTRL) | (1u << 1) | (1u << 8) | (1u << 9));
	wr32(kPHY_TX_RX_DLY, 0x01000100);
	wr32(kPHY_CONFIG, 1);
}

bool
PDSg2002SD::bringUpController(void)
{
	if ((rd32(kPRESENT) & kPRESENT_CARD_IN) == 0) {
		IOLog("PDSg2002SD: no card in the slot\n");
		return false;
	}
	if (!reset(kRESET_ALL))
		return false;
	setPhy();

	wr8(kPOWER, kPOWER_ON_33V);
	wr8(kTIMEOUT, 0x0e);
	wr8(kHOST_CTRL, 0);
	wr16(kHOST_CTRL2, 0);
	// status bits latch without raising the interrupt line, everything here is polled
	wr32(kINT_ENABLE, 0xffffffffu);
	wr32(kINT_SIGNAL, 0);
	wr32(kINT_STATUS, 0xffffffffu);

	return setClock(kIdentClockHz);
}

bool
PDSg2002SD::setClock(uint32_t hz)
{
	uint32_t n = (kBaseClockHz + 2 * hz - 1) / (2 * hz);
	uint16_t clk;

	wr16(kCLOCK, 0);
	if (n > 0x3ff)
		n = 0x3ff;
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
	IOLog("PDSg2002SD: clock never stabilised\n");
	return false;
}

bool
PDSg2002SD::waitInt(uint32_t bits, uint32_t timeoutMs)
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
PDSg2002SD::waitInhibit(uint32_t bits)
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
PDSg2002SD::pio(bool write, uint32_t nblks)
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
PDSg2002SD::command(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp,
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
		IOLog("PDSg2002SD: cmd%u stuck behind present %08x\n", idx, rd32(kPRESENT));
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
		if (idx != kCMD_SEND_IF_COND)
			IOLog("PDSg2002SD: cmd%u arg %08x failed int %08x present %08x\n",
			    idx, arg, rd32(kINT_STATUS), rd32(kPRESENT));
		reset(kRESET_CMD | kRESET_DATA);
	}
	wr32(kINT_STATUS, 0xffffffffu);
	return ok;
}

bool
PDSg2002SD::appCommand(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp)
{
	uint32_t r[4];

	if (!command(kCMD_APP, fRCA, kR1, r))
		return false;
	return command(idx, arg, flags, resp);
}

// csd[0] holds bits 127:96, positions below follow the sd physical layer spec
bool
PDSg2002SD::decodeCSD(const uint32_t *csd)
{
	uint32_t structure = csd[0] >> 30;

	if (structure == 1) {
		uint32_t csize = ((csd[1] & 0x3f) << 16) | (csd[2] >> 16);
		fBlockCount = ((uint64_t)csize + 1) * 1024;
	} else if (structure == 0) {
		uint32_t csize = ((csd[1] & 0x3ff) << 2) | (csd[2] >> 30);
		uint32_t mult = (csd[2] >> 15) & 0x7;
		uint32_t readbl = (csd[1] >> 16) & 0xf;
		fBlockCount = (((uint64_t)csize + 1) << (mult + 2) << readbl) / 512;
	} else {
		IOLog("PDSg2002SD: unknown csd structure %u\n", structure);
		return false;
	}
	return fBlockCount != 0;
}

bool
PDSg2002SD::identifyCard(void)
{
	uint32_t resp[4], ocr = 0;
	bool v2, ready = false;

	fRCA = 0;
	command(kCMD_GO_IDLE, 0, 0, NULL);
	IODelay(2000);

	// only sd 2.0 and later echo the check pattern, those may be high capacity
	v2 = command(kCMD_SEND_IF_COND, 0x1aa, kR7, resp) && (resp[0] & 0xfff) == 0x1aa;

	for (int i = 0; i < 1000; i++) {
		uint32_t arg = 0x00ff8000u | (v2 ? (1u << 30) : 0u);
		if (!appCommand(kACMD_SD_SEND_OP_COND, arg, kR3, resp))
			return false;
		ocr = resp[0];
		if (ocr & (1u << 31)) {
			ready = true;
			break;
		}
		IOSleep(10);
	}
	if (!ready) {
		IOLog("PDSg2002SD: card never left initialisation\n");
		return false;
	}
	fBlockAddressed = (ocr & (1u << 30)) != 0;

	if (!command(kCMD_ALL_SEND_CID, 0, kR2, resp))
		return false;
	if (!command(kCMD_SEND_REL_ADDR, 0, kR1, resp))
		return false;
	fRCA = resp[0] & 0xffff0000u;

	if (!command(kCMD_SEND_CSD, fRCA, kR2, resp) || !decodeCSD(resp))
		return false;
	if (!command(kCMD_SELECT_CARD, fRCA, kR1b, resp))
		return false;

	if (appCommand(kACMD_SET_BUS_WIDTH, 2, kR1, resp))
		wr8(kHOST_CTRL, rd8(kHOST_CTRL) | kHOST_CTRL_4BIT);
	if (!command(kCMD_SET_BLOCKLEN, 512, kR1, resp))
		return false;

	return setClock(kTransferClockHz);
}

IOReturn
PDSg2002SD::transferBlocks(bool write, UInt64 block, UInt32 nblks,
    IOMemoryDescriptor *buffer, UInt64 bufferOffset)
{
	uint32_t idx, arg, resp[4];
	IOByteCount bytes = (IOByteCount)nblks * 512;

	if (write) {
		idx = nblks > 1 ? kCMD_WRITE_MULTI : kCMD_WRITE_SINGLE;
		if (buffer->readBytes(bufferOffset, fBounce, bytes) != bytes)
			return kIOReturnIOError;
	} else {
		idx = nblks > 1 ? kCMD_READ_MULTI : kCMD_READ_SINGLE;
	}
	arg = fBlockAddressed ? (uint32_t)block : (uint32_t)(block * 512);

	if (!command(idx, arg, kR1 | kData | (write ? kDataWrite : 0), resp, nblks))
		return kIOReturnIOError;

	// the card keeps dat0 low while it programs a write
	if (write) {
		int ms = 0;
		while ((rd32(kPRESENT) & kPRESENT_DAT0) == 0 && ms++ < 2000)
			IODelay(1000);
		if (ms >= 2000)
			return kIOReturnIOError;
	}

	if (!write && buffer->writeBytes(bufferOffset, fBounce, bytes) != bytes)
		return kIOReturnIOError;
	return kIOReturnSuccess;
}

IOReturn
PDSg2002SD::readWrite(bool write, UInt64 block, UInt64 nblks,
    IOMemoryDescriptor *buffer)
{
	IOReturn ret = kIOReturnSuccess;
	UInt64 done = 0;

	if (fRegs == NULL || buffer == NULL)
		return kIOReturnNoDevice;
	if (write && fReadOnly)
		return kIOReturnNotWritable;
	if (block + nblks > fBlockCount)
		return kIOReturnBadArgument;

	IOLockLock(fLock);
	while (done < nblks) {
		UInt32 chunk = (UInt32)IOMin(nblks - done, (UInt64)kMaxBlocksPerCmd);

		ret = transferBlocks(write, block + done, chunk, buffer, done * 512);
		if (ret != kIOReturnSuccess) {
			IOLog("PDSg2002SD: %s failed at block %llu\n",
			    write ? "write" : "read", block + done);
			break;
		}
		done += chunk;
	}
	IOLockUnlock(fLock);
	return ret;
}
