#include "PDSun50iMMC.h"
#include "PDSun50iMMCDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

#define super IOService
OSDefineMetaClassAndStructors(PDSun50iMMC, IOService);

// register sequences follow u-boot drivers/mmc/sunxi_mmc.c for the h6 generation

#define kCCUPhys		0x03001000ULL
#define kPIOPhys		0x0300b000ULL

// ccu, smhc0 module clock and bus gate/reset
#define kCCU_MMC0_CLK		0x830
#define kMMC_CLK_ENABLE		(1u << 31)
#define kMMC_CLK_SRC_OSC24M	(0u << 24)
#define kMMC_CLK_SRC_PERIPH0	(1u << 24)
#define kMMC_CLK_N(n)		((n) << 8)
#define kMMC_CLK_M(m)		((m) - 1)
#define kCCU_MMC_BGR		0x84c
#define kMMC_BGR_GATE0		(1u << 0)
#define kMMC_BGR_RESET0		(1u << 16)

// port f, sd pins pf0-pf5 on function 2
#define kPIO_PF			0xb4
#define kPIO_CFG0		0x00
#define kPIO_DRV0		0x14
#define kPIO_PULL0		0x1c
#define kPF_FUNC_SDC0		2

// smhc registers
#define kGCTRL			0x00
#define kGCTRL_SOFT_RESET	(1u << 0)
#define kGCTRL_FIFO_RESET	(1u << 1)
#define kGCTRL_DMA_RESET	(1u << 2)
#define kGCTRL_RESET		(kGCTRL_SOFT_RESET | kGCTRL_FIFO_RESET | kGCTRL_DMA_RESET)
#define kGCTRL_AHB_ACCESS	(1u << 31)
#define kCLKCR			0x04
#define kCLKCR_ENABLE		(1u << 16)
#define kCLKCR_DIV_MASK		0xffu
#define kTIMEOUT		0x08
#define kWIDTH			0x0c
#define kBLKSZ			0x10
#define kBYTECNT		0x14
#define kCMD			0x18
#define kARG			0x1c
#define kRESP0			0x20
#define kRESP1			0x24
#define kRESP2			0x28
#define kRESP3			0x2c
#define kRINT			0x38
#define kSTATUS			0x3c
#define kNTSR			0x5c
#define kNTSR_MODE_NEW		(1u << 31)
#define kHWRST			0x78
#define kTHLDC			0x100
#define kTHLDC_READ_EN		(1u << 0)
#define kTHLDC_WRITE_EN		(1u << 2)
#define kTHLDC_READ_THLD(x)	(((x) & 0xfffu) << 16)
#define kSAMP_DL		0x144
#define kSAMP_DL_SW_EN		(1u << 7)
#define kFIFO			0x200

// cmd register
#define kCMD_RESP_EXPIRE	(1u << 6)
#define kCMD_LONG_RESP		(1u << 7)
#define kCMD_CHECK_CRC		(1u << 8)
#define kCMD_DATA_EXPIRE	(1u << 9)
#define kCMD_WRITE		(1u << 10)
#define kCMD_AUTO_STOP		(1u << 12)
#define kCMD_WAIT_PRE_OVER	(1u << 13)
#define kCMD_SEND_INIT_SEQ	(1u << 15)
#define kCMD_UPCLK_ONLY		(1u << 21)
#define kCMD_START		(1u << 31)

// raw interrupt status
#define kRINT_CMD_DONE		(1u << 2)
#define kRINT_DATA_OVER		(1u << 3)
#define kRINT_AUTO_CMD_DONE	(1u << 14)
#define kRINT_ERRORS		0xbfc2u

// status register
#define kST_FIFO_EMPTY		(1u << 2)
#define kST_FIFO_FULL		(1u << 3)
#define kST_CARD_BUSY		(1u << 9)
#define kST_FIFO_LEVEL(s)	(((s) >> 17) & 0x3fffu)

// command flags for command()
#define kRsp			(1u << 0)
#define kRsp136			(1u << 1)
#define kRspCrc			(1u << 2)
#define kRspBusy		(1u << 3)
#define kData			(1u << 4)
#define kDataWrite		(1u << 5)
#define kR1			(kRsp | kRspCrc)
#define kR1b			(kRsp | kRspCrc | kRspBusy)
#define kR2			(kRsp | kRsp136 | kRspCrc)
#define kR3			(kRsp)

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

#define kIdentClockHz		400000
#define kTransferClockHz	25000000
#define kPeriph0Hz		600000000u
#define kMaxBlocksPerCmd	128

static volatile uint8_t *
map_phys(IOPhysicalAddress phys, IOByteCount size, IOMemoryMap **outMap)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		phys, size, kIODirectionOutIn);
	IOMemoryMap *map;

	if (desc == NULL)
		return NULL;
	map = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (map == NULL)
		return NULL;
	*outMap = map;
	return (volatile uint8_t *)map->getVirtualAddress();
}

static inline void
mmio_rmw(volatile uint8_t *base, uint32_t off, uint32_t clear, uint32_t set)
{
	volatile uint32_t *r = (volatile uint32_t *)(base + off);
	*r = (*r & ~clear) | set;
	__asm__ volatile ("dsb sy" ::: "memory");
}

bool
PDSun50iMMC::start(IOService *provider)
{
	if (!super::start(provider))
		return false;

	fRegMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	fCCU = map_phys(kCCUPhys, 0x1000, &fCCUMap);
	fPIO = map_phys(kPIOPhys, 0x1000, &fPIOMap);
	if (fRegMap == NULL || fCCU == NULL || fPIO == NULL) {
		IOLog("PDSun50iMMC: cannot map registers\n");
		return false;
	}
	fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();

	fLock = IOLockAlloc();
	fBounce = (uint32_t *)IOMalloc(kMaxBlocksPerCmd * 512);
	if (fLock == NULL || fBounce == NULL)
		return false;

	fReadOnly = false;

	if (!bringUpController() || !identifyCard()) {
		IOLog("PDSun50iMMC: no usable card\n");
		return false;
	}

	IOLog("PDSun50iMMC: %llu blocks of 512 bytes (%llu MB), %s addressing\n",
	    fBlockCount, (fBlockCount * 512) / (1024 * 1024),
	    fBlockAddressed ? "block" : "byte");

	fDisk = new PDSun50iMMCDisk;
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
PDSun50iMMC::stop(IOService *provider)
{
	if (fDisk != NULL) {
		fDisk->detach(this);
		OSSafeReleaseNULL(fDisk);
	}
	super::stop(provider);
}

void
PDSun50iMMC::free(void)
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
	OSSafeReleaseNULL(fCCUMap);
	OSSafeReleaseNULL(fPIOMap);
	super::free();
}

bool
PDSun50iMMC::bringUpController(void)
{
	// pf0-pf5 to the sd function with pull-ups and drive level 2
	for (uint32_t pin = 0; pin <= 5; pin++) {
		mmio_rmw(fPIO, kPIO_PF + kPIO_CFG0, 0xfu << (pin * 4), kPF_FUNC_SDC0 << (pin * 4));
		mmio_rmw(fPIO, kPIO_PF + kPIO_DRV0, 0x3u << (pin * 2), 2u << (pin * 2));
		mmio_rmw(fPIO, kPIO_PF + kPIO_PULL0, 0x3u << (pin * 2), 1u << (pin * 2));
	}

	mmio_rmw(fCCU, kCCU_MMC_BGR, 0, kMMC_BGR_GATE0);
	mmio_rmw(fCCU, kCCU_MMC_BGR, 0, kMMC_BGR_RESET0);

	wr(kGCTRL, kGCTRL_RESET);
	IODelay(1000);
	wr(kHWRST, 0);
	IODelay(10);
	wr(kHWRST, 1);
	IODelay(300);
	// the h616 fifo needs explicit thresholds
	wr(kTHLDC, kTHLDC_READ_THLD(512) | kTHLDC_WRITE_EN | kTHLDC_READ_EN);

	wr(kTIMEOUT, 0xffffffff);
	wr(kWIDTH, 0);
	wr(kRINT, 0xffffffff);

	return setClock(kIdentClockHz);
}

bool
PDSun50iMMC::updateClock(void)
{
	wr(kCMD, kCMD_START | kCMD_UPCLK_ONLY | kCMD_WAIT_PRE_OVER);
	for (int i = 0; i < 2000; i++) {
		if ((rd(kCMD) & kCMD_START) == 0) {
			wr(kRINT, rd(kRINT));
			return true;
		}
		IODelay(1000);
	}
	IOLog("PDSun50iMMC: clock update timed out\n");
	return false;
}

bool
PDSun50iMMC::setClock(uint32_t hz)
{
	uint32_t src = kMMC_CLK_SRC_OSC24M, parent = 24000000, div, n = 0;
	uint32_t clkcr = rd(kCLKCR) & ~kCLKCR_ENABLE;

	wr(kCLKCR, clkcr);
	if (!updateClock())
		return false;

	// periph0 is fed doubled with a fixed /2 behind it, so it counts as 600 mhz
	if (hz > 24000000) {
		src = kMMC_CLK_SRC_PERIPH0;
		parent = kPeriph0Hz;
	}
	div = (parent + hz - 1) / hz;
	while (div > 16) {
		n++;
		div = (div + 1) / 2;
	}
	if (n > 3)
		return false;
	*(volatile uint32_t *)(fCCU + kCCU_MMC0_CLK) =
	    kMMC_CLK_ENABLE | src | kMMC_CLK_N(n) | kMMC_CLK_M(div);
	wr(kNTSR, rd(kNTSR) | kNTSR_MODE_NEW);

	clkcr &= ~kCLKCR_DIV_MASK;
	wr(kCLKCR, clkcr);
	wr(kSAMP_DL, kSAMP_DL_SW_EN);
	wr(kCLKCR, clkcr | kCLKCR_ENABLE);
	return updateClock();
}

bool
PDSun50iMMC::waitRint(uint32_t done, uint32_t timeoutMs)
{
	for (uint32_t us = 0; us < timeoutMs * 1000; us += 10) {
		uint32_t st = rd(kRINT);
		if (st & kRINT_ERRORS)
			return false;
		if (st & done)
			return true;
		IODelay(10);
	}
	return false;
}

void
PDSun50iMMC::recover(void)
{
	wr(kGCTRL, kGCTRL_RESET);
	IODelay(100);
	updateClock();
}

// pio through the fifo using fBounce, the level field lets reads drain in bursts
bool
PDSun50iMMC::pio(bool write, uint32_t words)
{
	uint32_t i = 0;

	wr(kGCTRL, rd(kGCTRL) | kGCTRL_AHB_ACCESS);
	while (i < words) {
		uint32_t st;
		int spins = 0;

		while ((st = rd(kSTATUS)) & (write ? kST_FIFO_FULL : kST_FIFO_EMPTY)) {
			if (rd(kRINT) & kRINT_ERRORS || ++spins > 2000000)
				return false;
		}
		if (write) {
			*(volatile uint32_t *)(fRegs + kFIFO) = fBounce[i++];
			continue;
		}
		uint32_t level = kST_FIFO_LEVEL(st);
		if (level == 0 && (st & kST_FIFO_FULL))
			level = 32;
		while (level-- > 0 && i < words)
			fBounce[i++] = *(volatile uint32_t *)(fRegs + kFIFO);
	}
	return true;
}

bool
PDSun50iMMC::command(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp,
    uint32_t nblks)
{
	uint32_t cmd = kCMD_START | idx;
	bool ok = true;

	if (idx == kCMD_GO_IDLE)
		cmd |= kCMD_SEND_INIT_SEQ;
	if (flags & kRsp)
		cmd |= kCMD_RESP_EXPIRE;
	if (flags & kRsp136)
		cmd |= kCMD_LONG_RESP;
	if (flags & kRspCrc)
		cmd |= kCMD_CHECK_CRC;
	if (flags & kData) {
		cmd |= kCMD_DATA_EXPIRE | kCMD_WAIT_PRE_OVER;
		if (flags & kDataWrite)
			cmd |= kCMD_WRITE;
		if (nblks > 1)
			cmd |= kCMD_AUTO_STOP;
		wr(kBLKSZ, 512);
		wr(kBYTECNT, nblks * 512);
	}

	wr(kRINT, 0xffffffff);
	wr(kARG, arg);
	wr(kCMD, cmd);

	if (flags & kData)
		ok = pio(flags & kDataWrite, nblks * 128);
	if (ok)
		ok = waitRint(kRINT_CMD_DONE, 1000);
	if (ok && (flags & kData))
		ok = waitRint(nblks > 1 ? kRINT_AUTO_CMD_DONE : kRINT_DATA_OVER, 2000);
	if (ok && (flags & (kRspBusy | kDataWrite))) {
		int ms = 0;
		while ((rd(kSTATUS) & kST_CARD_BUSY) && ms++ < 2000)
			IODelay(1000);
		ok = ms < 2000;
	}

	if (ok && resp != NULL) {
		if (flags & kRsp136) {
			// resp[0] carries bits 127:96, as in the csd and cid tables
			resp[0] = rd(kRESP3);
			resp[1] = rd(kRESP2);
			resp[2] = rd(kRESP1);
			resp[3] = rd(kRESP0);
		} else {
			resp[0] = rd(kRESP0);
		}
	}

	if (!ok) {
		if (idx != kCMD_SEND_IF_COND)
			IOLog("PDSun50iMMC: cmd%u arg %08x failed rint %08x status %08x\n",
			    idx, arg, rd(kRINT), rd(kSTATUS));
		recover();
	}
	wr(kRINT, 0xffffffff);
	wr(kGCTRL, rd(kGCTRL) | kGCTRL_FIFO_RESET);
	return ok;
}

bool
PDSun50iMMC::appCommand(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp)
{
	uint32_t r[4];

	if (!command(kCMD_APP, fRCA, kR1, r))
		return false;
	return command(idx, arg, flags, resp);
}

// csd[0] holds bits 127:96, positions below follow the sd physical layer spec
bool
PDSun50iMMC::decodeCSD(const uint32_t *csd)
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
		IOLog("PDSun50iMMC: unknown csd structure %u\n", structure);
		return false;
	}
	return fBlockCount != 0;
}

bool
PDSun50iMMC::identifyCard(void)
{
	uint32_t resp[4], ocr = 0;
	bool v2, ready = false;

	fRCA = 0;
	if (!command(kCMD_GO_IDLE, 0, 0, NULL))
		return false;

	// only sd 2.0 and later echo the check pattern, those may be high capacity
	v2 = command(kCMD_SEND_IF_COND, 0x1aa, kR1, resp) && (resp[0] & 0xfff) == 0x1aa;

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
		IOLog("PDSun50iMMC: card never left initialisation\n");
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
		wr(kWIDTH, 1);
	if (!command(kCMD_SET_BLOCKLEN, 512, kR1, resp))
		return false;

	return setClock(kTransferClockHz);
}

IOReturn
PDSun50iMMC::transferBlocks(bool write, UInt64 block, UInt32 nblks,
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

	if (!write && buffer->writeBytes(bufferOffset, fBounce, bytes) != bytes)
		return kIOReturnIOError;
	return kIOReturnSuccess;
}

IOReturn
PDSun50iMMC::readWrite(bool write, UInt64 block, UInt64 nblks,
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
		UInt32 chunk = (UInt32)min(nblks - done, (UInt64)kMaxBlocksPerCmd);

		ret = transferBlocks(write, block + done, chunk, buffer, done * 512);
		if (ret != kIOReturnSuccess) {
			IOLog("PDSun50iMMC: %s failed at block %llu\n",
			    write ? "write" : "read", block + done);
			break;
		}
		done += chunk;
	}
	IOLockUnlock(fLock);
	return ret;
}
