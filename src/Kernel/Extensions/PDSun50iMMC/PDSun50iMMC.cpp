#include "PDSun50iMMC.h"
#include "PDSun50iMMCDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>
#include <libkern/OSAtomic.h>

#define super IOService
OSDefineMetaClassAndStructors(PDSun50iMMC, IOService);

// register sequences follow u-boot drivers/mmc/sunxi_mmc.c for the h6 generation

// ccu module clock fields, the same on the h616 and the a733
#define kMMC_CLK_ENABLE		(1u << 31)
#define kMMC_CLK_SRC_OSC24M	(0u << 24)
#define kMMC_CLK_N(n)		((n) << 8)
#define kMMC_CLK_M(m)		((m) - 1)
#define kMMC_BGR_GATE0		(1u << 0)
#define kMMC_BGR_RESET0		(1u << 16)

// where each soc keeps smhc0's clock and pins. the a733 values were read back from a running
// board: module clock at ccu 0xd00, gate and reset at 0xd0c. linux feeds it from source 4,
// pll-peri1's 300 mhz output, and vendor u-boot may leave that pll gated, so it is turned back on
struct PDSunxiMMCSoC {
	const char *name;
	uint64_t ccuPhys;
	uint64_t pioPhys;       // 0: firmware left the pins set up, leave them
	uint32_t modClk;
	uint32_t bgr;
	uint32_t fastSrc;
	uint32_t fastParentHz;
	uint32_t fastPll;       // 0: the fast parent is always running
	uint32_t fastPllCfg;    // its factors as linux programs them, anything else is left alone
};

static const PDSunxiMMCSoC kH616 = {
	"h616", 0x03001000ULL, 0x0300b000ULL, 0x830, 0x84c, 1u << 24, 600000000u, 0, 0,
};

static const PDSunxiMMCSoC kA733 = {
	"a733", 0x02002000ULL, 0, 0xd00, 0xd0c, 4u << 24, 150000000u, 0x0c0, 0x00126310,
};

// pll control: enable, ldo, lock enable, lock, and the output gates in the top byte
#define kPLL_ON_BITS		0xff000000u
#define kPLL_LOCK		(1u << 28)

// the a733's micro sd slot; its other smhcs (sdio wifi, emmc) share the compatible
#define kA733SMHC0Phys		0x04020000ULL

// the vendor and the mainline compatible for the a733's smhc
static bool
pd_is_a733(IOService *provider)
{
	OSData *compat = OSDynamicCast(OSData, provider->getProperty("compatible"));
	const char *s = compat != NULL ? (const char *)compat->getBytesNoCopy() : NULL;
	unsigned int len = compat != NULL ? compat->getLength() : 0;

	for (unsigned int o = 0; s != NULL && o < len; o += strnlen(s + o, len - o) + 1) {
		if (strncmp(s + o, "allwinner,sunxi-mmc-v5p3x", len - o) == 0 ||
		    strncmp(s + o, "allwinner,sun60i-a733-mmc", len - o) == 0) {
			return true;
		}
	}
	return false;
}

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
#define kCMD_STOP_ABORT		(1u << 14)
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
#define kCMD_SWITCH_FUNC	6
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
#define kHighSpeedClockHz	50000000
#define kMaxBlocksPerCmd	1024
#define kACMD_SET_WR_ERASE	23
// the write queue: at most this much data, written early once past the kick or the window
#define kWQMaxBytes		(32u << 20)
#define kWQKickBytes		(4u << 20)
#define kWQWindowMs		20
// merged writes stop at this size: a read waits behind at most one of them
#define kWQRunBlocks		256

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

	fSoC = pd_is_a733(provider) ? &kA733 : &kH616;
	fRegMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	fCCU = map_phys(fSoC->ccuPhys, 0x1000 + (fSoC->modClk & ~0xfffu), &fCCUMap);
	if (fSoC->pioPhys != 0)
		fPIO = map_phys(fSoC->pioPhys, 0x1000, &fPIOMap);
	if (fRegMap != NULL && fSoC == &kA733 && fRegMap->getPhysicalAddress() != kA733SMHC0Phys) {
		OSSafeReleaseNULL(fRegMap);
		return false;
	}
	if (fRegMap == NULL || fCCU == NULL || (fSoC->pioPhys != 0 && fPIO == NULL)) {
		IOLog("PDSun50iMMC: cannot map registers\n");
		return false;
	}
	fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();

	fLock = IOLockAlloc();
	fWQLock = IOLockAlloc();
	fBounce = (uint32_t *)IOMalloc(kMaxBlocksPerCmd * 512);
	if (fLock == NULL || fWQLock == NULL || fBounce == NULL)
		return false;
	// pdmmcsync=1 keeps every write synchronous against the card
	{
		uint32_t sync = 0;

		fWriteBack = !(PE_parse_boot_argn("pdmmcsync", &sync, sizeof(sync)) && sync != 0);
	}

	fReadOnly = false;

	if (!bringUpController() || !identifyCard()) {
		IOLog("PDSun50iMMC: no usable card\n");
		return false;
	}

	IOLog("PDSun50iMMC: %llu blocks of 512 bytes (%llu MB), %s addressing\n",
	    fBlockCount, (fBlockCount * 512) / (1024 * 1024),
	    fBlockAddressed ? "block" : "byte");

	if (fWriteBack) {
		thread_t th = NULL;

		if (kernel_thread_start(&PDSun50iMMC::writeWorker, this, &th) == KERN_SUCCESS)
			thread_deallocate(th);
		else
			fWriteBack = false;
	}

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

	// the controller moves data by pio, so a restart only needs the queued writes on the card
	setProperty("pd-quiesce", true);
	fRestartNotifier = registerPrioritySleepWakeInterest(&PDSun50iMMC::restartHandler, this, NULL);

	registerService();
	return true;
}

void
PDSun50iMMC::quiesce(const char *why)
{
	if (fQuiesced || fWQLock == NULL)
		return;
	fQuiesced = true;
	IOLog("PDSun50iMMC: quiesced for %s, write queue flushed (%d)\n", why, flushWrites());
}

IOReturn
PDSun50iMMC::restartHandler(void *target, void *, UInt32 messageType, IOService *, void *, vm_size_t)
{
	if (messageType == kIOMessageSystemWillRestart || messageType == kIOMessageSystemWillPowerOff)
		((PDSun50iMMC *)target)->quiesce(messageType == kIOMessageSystemWillRestart ? "restart" : "power off");
	return kIOReturnSuccess;
}

IOReturn
PDSun50iMMC::message(UInt32 type, IOService *provider, void *argument)
{
	if (type == kIOMessageSystemWillRestart || type == kIOMessageSystemWillPowerOff) {
		quiesce("watchdog deadline");
		return kIOReturnSuccess;
	}
	return super::message(type, provider, argument);
}

void
PDSun50iMMC::stop(IOService *provider)
{
	if (fRestartNotifier != NULL) {
		fRestartNotifier->remove();
		fRestartNotifier = NULL;
	}
	if (fWQLock != NULL) {
		flushWrites();
		IOLockLock(fWQLock);
		fWQStop = true;
		IOLockWakeup(fWQLock, &fWQ, false);
		IOLockUnlock(fWQLock);
	}
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
	if (fWQLock != NULL) {
		IOLockFree(fWQLock);
		fWQLock = NULL;
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
	for (uint32_t pin = 0; fPIO != NULL && pin <= 5; pin++) {
		mmio_rmw(fPIO, kPIO_PF + kPIO_CFG0, 0xfu << (pin * 4), kPF_FUNC_SDC0 << (pin * 4));
		mmio_rmw(fPIO, kPIO_PF + kPIO_DRV0, 0x3u << (pin * 2), 2u << (pin * 2));
		mmio_rmw(fPIO, kPIO_PF + kPIO_PULL0, 0x3u << (pin * 2), 1u << (pin * 2));
	}

	IOLog("PDSun50iMMC: %s smhc0, module clock 0x%08x, gctrl 0x%08x clkcr 0x%08x cmd 0x%08x status 0x%08x ntsr 0x%08x\n",
	    fSoC->name, *(volatile uint32_t *)(fCCU + fSoC->modClk), rd(kGCTRL), rd(kCLKCR), rd(kCMD), rd(kSTATUS), rd(kNTSR));
	mmio_rmw(fCCU, fSoC->bgr, 0, kMMC_BGR_GATE0);
	mmio_rmw(fCCU, fSoC->bgr, 0, kMMC_BGR_RESET0);
	// a controller with no running module clock never takes a command, not even the clock update
	if (fSoC->fastPll)
		*(volatile uint32_t *)(fCCU + fSoC->modClk) = kMMC_CLK_ENABLE | kMMC_CLK_SRC_OSC24M;
	fFastOK = fSoC->fastPll == 0 || enableFastPll();

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
PDSun50iMMC::enableFastPll(void)
{
	volatile uint32_t *pll = (volatile uint32_t *)(fCCU + fSoC->fastPll);
	uint32_t was = *pll;

	if ((was & ~kPLL_ON_BITS) != fSoC->fastPllCfg) {
		IOLog("PDSun50iMMC: fast pll 0x%08x not as expected, staying on 24 mhz\n", was);
		return false;
	}

	*pll = was | kPLL_ON_BITS;
	for (int i = 0; i < 1000 && (*pll & kPLL_LOCK) == 0; i++) {
		IODelay(10);
	}
	IOLog("PDSun50iMMC: fast pll 0x%08x -> 0x%08x\n", was, *pll);
	return (*pll & kPLL_LOCK) != 0;
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
	IOLog("PDSun50iMMC: clock update timed out, gctrl 0x%08x clkcr 0x%08x cmd 0x%08x rint 0x%08x status 0x%08x ntsr 0x%08x\n",
	    rd(kGCTRL), rd(kCLKCR), rd(kCMD), rd(kRINT), rd(kSTATUS), rd(kNTSR));
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

	// h616: periph0 is fed doubled with a fixed /2 behind it, so it counts as 600 mhz
	// a733: pll-peri1's 300 mhz output, halved by new timing mode, counts as 150 mhz
	if (hz > 24000000 && fFastOK) {
		src = fSoC->fastSrc;
		parent = fSoC->fastParentHz;
	}
	div = (parent + hz - 1) / hz;
	while (div > 16) {
		n++;
		div = (div + 1) / 2;
	}
	if (n > 3)
		return false;
	*(volatile uint32_t *)(fCCU + fSoC->modClk) =
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
PDSun50iMMC::recover(bool abortData)
{
	// a data error can leave the card transferring after the automatic stop failed, abort it
	// without WAIT_PRE_OVER, which would wait for the very transfer being ended
	uint32_t gctrl = rd(kGCTRL) & ~kGCTRL_RESET;
	uint32_t clkcr = rd(kCLKCR), width = rd(kWIDTH), timeout = rd(kTIMEOUT);
	uint32_t thldc = rd(kTHLDC), ntsr = rd(kNTSR), sample = rd(kSAMP_DL);
	if (abortData) {
		wr(kRINT, 0xffffffff);
		wr(kARG, 0);
		wr(kCMD, kCMD_START | kCMD_STOP_ABORT | kCMD_RESP_EXPIRE | kCMD_CHECK_CRC | 12);
		bool stopped = waitRint(kRINT_CMD_DONE, 1000);
		for (unsigned ms = 0; stopped && (rd(kSTATUS) & kST_CARD_BUSY); ++ms) {
			if (ms == 2000) { stopped = false; break; }
			IOSleep(1);
		}
		if (!stopped)
			IOLog("PDSun50iMMC: stop after data error failed, rint %08x status %08x\n",
			    rd(kRINT), rd(kSTATUS));
	}
	wr(kGCTRL, kGCTRL_RESET);
	IODelay(100);
	// soft reset clears the host configuration but the card keeps its bus width and speed,
	// so restore them before another data command
	wr(kGCTRL, gctrl);
	wr(kCLKCR, clkcr);
	wr(kWIDTH, width);
	wr(kTIMEOUT, timeout);
	wr(kTHLDC, thldc);
	wr(kNTSR, ntsr);
	wr(kSAMP_DL, sample);
	wr(kRINT, 0xffffffff);
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
    uint32_t nblks, uint32_t blksz)
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
		wr(kBLKSZ, blksz);
		wr(kBYTECNT, nblks * blksz);
	}

	wr(kRINT, 0xffffffff);
	wr(kARG, arg);
	wr(kCMD, cmd);

	if (flags & kData)
		ok = pio(flags & kDataWrite, nblks * blksz / 4);
	if (ok)
		ok = waitRint(kRINT_CMD_DONE, 1000);
	if (ok && (flags & kData))
		ok = waitRint(nblks > 1 ? kRINT_AUTO_CMD_DONE : kRINT_DATA_OVER, 2000);
	if (ok && (flags & (kRspBusy | kDataWrite))) {
		int ms = 0;
		// sleep through the card's programming time, a spin here starves a single cpu
		while ((rd(kSTATUS) & kST_CARD_BUSY) && ms++ < 2000)
			IOSleep(1);
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
		recover((flags & kData) != 0);
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

	if (!setClock(kTransferClockHz))
		return false;
	if (fFastOK && switchHighSpeed())
		return true;
	return setClock(kTransferClockHz);
}

// cmd6 mode 1 selects function 1 (high speed) of group 1, byte 16 of the 64-byte status names
// the function now in effect. 50 mhz is kept only if block 0 reads back as it did at 25 mhz
bool
PDSun50iMMC::switchHighSpeed(void)
{
	uint32_t resp[4], *ref;
	uint8_t fn;
	bool same;

	if (!command(kCMD_SWITCH_FUNC, 0x80fffff1u, kR1 | kData, resp, 1, 64))
		return false;
	fn = ((uint8_t *)fBounce)[16] & 0xf;
	if (fn != 1) {
		IOLog("PDSun50iMMC: card stays at default speed (group 1 function %u)\n", fn);
		return false;
	}
	ref = (uint32_t *)IOMalloc(512);
	if (ref == NULL)
		return false;
	if (!command(kCMD_READ_SINGLE, 0, kR1 | kData, resp, 1)) {
		IOFree(ref, 512);
		return false;
	}
	memcpy(ref, fBounce, 512);
	same = setClock(kHighSpeedClockHz) && command(kCMD_READ_SINGLE, 0, kR1 | kData, resp, 1) &&
	    memcmp(ref, fBounce, 512) == 0;
	IOFree(ref, 512);
	IOLog("PDSun50iMMC: high speed %s\n", same ? "at 50 mhz" : "read back wrong, back to 25 mhz");
	fHighSpeed = same;
	return same;
}

IOReturn
PDSun50iMMC::transferBlocks(bool write, UInt64 block, UInt32 nblks,
    IOMemoryDescriptor *buffer, UInt64 bufferOffset)
{
	uint32_t idx, arg, resp[4];
	IOByteCount bytes = (IOByteCount)nblks * 512;

	// a null buffer means fBounce already holds the data, or keeps what was read
	if (write) {
		idx = nblks > 1 ? kCMD_WRITE_MULTI : kCMD_WRITE_SINGLE;
		if (buffer != NULL && buffer->readBytes(bufferOffset, fBounce, bytes) != bytes)
			return kIOReturnIOError;
		// pre-erasing the blocks a multi-block write is about to fill speeds it up on most cards
		if (nblks > 1)
			(void)appCommand(kACMD_SET_WR_ERASE, nblks, kR1, resp);
	} else {
		idx = nblks > 1 ? kCMD_READ_MULTI : kCMD_READ_SINGLE;
	}
	arg = fBlockAddressed ? (uint32_t)block : (uint32_t)(block * 512);

	if (!command(idx, arg, kR1 | kData | (write ? kDataWrite : 0), resp, nblks))
		return kIOReturnIOError;

	if (!write && buffer != NULL && buffer->writeBytes(bufferOffset, fBounce, bytes) != bytes)
		return kIOReturnIOError;
	return kIOReturnSuccess;
}

// one transfer of at most kMaxBlocksPerCmd under fLock: one retry as is, then for good at 25 mhz
// when high speed is what failed. counted for the minute's stats
IOReturn
PDSun50iMMC::transferRetry(bool write, UInt64 block, UInt32 nblks,
    IOMemoryDescriptor *buffer, UInt64 bufferOffset)
{
	uint64_t t0 = mach_absolute_time(), ns;
	IOReturn ret;

	ret = transferBlocks(write, block, nblks, buffer, bufferOffset);
	if (ret != kIOReturnSuccess)
		ret = transferBlocks(write, block, nblks, buffer, bufferOffset);
	if (ret != kIOReturnSuccess && fHighSpeed) {
		fHighSpeed = false;
		IOLog("PDSun50iMMC: %s errors at 50 mhz, back to 25 mhz\n", write ? "write" : "read");
		if (setClock(kTransferClockHz))
			ret = transferBlocks(write, block, nblks, buffer, bufferOffset);
	}
	if (ret != kIOReturnSuccess)
		IOLog("PDSun50iMMC: %s failed at block %llu\n", write ? "write" : "read", block);
	absolutetime_to_nanoseconds(mach_absolute_time() - t0, &ns);
	fStatCmds[write]++;
	fStatBlocks[write] += nblks;
	fStatUs[write] += ns / 1000;
	return ret;
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

	if (!write)
		OSIncrementAtomic(&fReadWaiters);
	IOLockLock(fLock);
	if (!write)
		OSDecrementAtomic(&fReadWaiters);
	while (done < nblks && ret == kIOReturnSuccess) {
		UInt32 chunk = (UInt32)min(nblks - done, (UInt64)kMaxBlocksPerCmd);

		ret = transferRetry(write, block + done, chunk, buffer, done * 512);
		done += chunk;
	}
	// queued writes are newer than the card: under fLock, so none can land in between
	if (!write && ret == kIOReturnSuccess && fWriteBack)
		overlayQueued(block, nblks, buffer);
	IOLockUnlock(fLock);
	logStats();
	return ret;
}

// copy queued data over what a read got from the card: the batch in flight first, then the queue,
// which is newer. queued writes that overlap hold the same data where they do
void
PDSun50iMMC::overlayQueued(UInt64 block, UInt64 nblks, IOMemoryDescriptor *buffer)
{
	IOLockLock(fWQLock);
	for (int l = 0; l < 2; l++) {
		for (PendingWrite *w = l ? fWQ : fWQFlight; w != NULL; w = w->next) {
			uint64_t lo = max(block, w->block), hi = min(block + nblks, w->block + w->nblks);

			if (w->done || lo >= hi)
				continue;
			buffer->writeBytes((lo - block) * 512, w->data + (lo - w->block) * 512, (hi - lo) * 512);
		}
	}
	IOLockUnlock(fWQLock);
}

// a write completes once its data is copied. it also updates every queued write it overlaps, so
// those agree with it and nothing waits, and the batch in flight is older and lands first anyway
IOReturn
PDSun50iMMC::queueWrite(UInt64 block, UInt64 nblks, IOMemoryDescriptor *buffer)
{
	uint32_t bytes = (uint32_t)nblks * 512;
	PendingWrite *w;

	if (!fWriteBack || nblks > kMaxBlocksPerCmd) {
		if (fWriteBack)
			flushWrites();
		return readWrite(true, block, nblks, buffer);
	}
	if (fRegs == NULL || buffer == NULL)
		return kIOReturnNoDevice;
	if (fReadOnly)
		return kIOReturnNotWritable;
	if (block + nblks > fBlockCount)
		return kIOReturnBadArgument;

	w = (PendingWrite *)IOMallocZero(sizeof(*w));
	if (w != NULL)
		w->data = (uint8_t *)IOMalloc(bytes);
	if (w == NULL || w->data == NULL || buffer->readBytes(0, w->data, bytes) != bytes) {
		if (w != NULL && w->data != NULL)
			IOFree(w->data, bytes);
		if (w != NULL)
			IOFree(w, sizeof(*w));
		flushWrites();
		return readWrite(true, block, nblks, buffer);
	}
	w->block = block;
	w->nblks = (uint32_t)nblks;

	IOLockLock(fWQLock);
	bool covered = false;

	for (PendingWrite *q = fWQ; q != NULL; q = q->next) {
		uint64_t lo = max(block, q->block), hi = min(block + nblks, q->block + q->nblks);

		if (lo >= hi)
			continue;
		memcpy(q->data + (lo - q->block) * 512, w->data + (lo - block) * 512, (hi - lo) * 512);
		if (lo == block && hi == block + nblks)
			covered = true;
	}
	if (covered) {
		IOLockUnlock(fWQLock);
		IOFree(w->data, bytes);
		IOFree(w, sizeof(*w));
		return kIOReturnSuccess;
	}
	while (fWQBytes + bytes > kWQMaxBytes) {
		fWQFlush = true;
		IOLockWakeup(fWQLock, &fWQ, false);
		IOLockSleep(fWQLock, &fWQBytes, THREAD_UNINT);
	}
	if (fWQTail != NULL)
		fWQTail->next = w;
	else
		fWQ = w;
	fWQTail = w;
	fWQBytes += bytes;
	IOLockWakeup(fWQLock, &fWQ, false);
	IOLockUnlock(fWQLock);
	return kIOReturnSuccess;
}

// everything queued so far on the card, returns the error a queued write hit since the last one
IOReturn
PDSun50iMMC::flushWrites(void)
{
	uint64_t t0 = mach_absolute_time(), ns;
	IOReturn ret;

	if (!fWriteBack || fWQLock == NULL)
		return kIOReturnSuccess;
	IOLockLock(fWQLock);
	while (fWQ != NULL || fWQBusy) {
		fWQFlush = true;
		IOLockWakeup(fWQLock, &fWQ, false);
		IOLockSleep(fWQLock, &fWQBytes, THREAD_UNINT);
	}
	ret = fWQError;
	fWQError = kIOReturnSuccess;
	absolutetime_to_nanoseconds(mach_absolute_time() - t0, &ns);
	fStatSyncs++;
	fStatSyncUs += ns / 1000;
	IOLockUnlock(fWQLock);
	return ret;
}

void
PDSun50iMMC::writeWorker(void *arg, wait_result_t wr)
{
	PDSun50iMMC *me = (PDSun50iMMC *)arg;

	(void)wr;
	IOLockLock(me->fWQLock);
	while (!me->fWQStop) {
		uint64_t deadline;

		if (me->fWQ == NULL) {
			IOLockSleep(me->fWQLock, &me->fWQ, THREAD_UNINT);
			continue;
		}
		// gather for a moment unless someone waits or plenty is queued
		clock_interval_to_deadline(kWQWindowMs, kMillisecondScale, &deadline);
		while (!me->fWQFlush && !me->fWQStop && me->fWQBytes < kWQKickBytes &&
		    mach_absolute_time() < deadline)
			IOLockSleepDeadline(me->fWQLock, &me->fWQ, deadline, THREAD_UNINT);
		me->writeBatch();
	}
	IOLockUnlock(me->fWQLock);
	thread_terminate(current_thread());
}

// called and returns with fWQLock held, writes the queue sorted by block as contiguous runs, each
// written and marked done under fLock so reads see it either queued or on the card
void
PDSun50iMMC::writeBatch(void)
{
	PendingWrite *list = fWQ, *sorted = NULL, *w, *next;
	uint32_t freed = 0;

	fWQ = fWQTail = NULL;
	fWQBusy = true;
	fWQFlush = false;
	// insertion sort by block: batches are a few hundred writes, mostly already near order
	for (w = list; w != NULL; w = next) {
		PendingWrite **pp = &sorted;

		next = w->next;
		while (*pp != NULL && (*pp)->block < w->block)
			pp = &(*pp)->next;
		w->next = *pp;
		*pp = w;
	}
	fWQFlight = sorted;
	IOLockUnlock(fWQLock);

	for (w = sorted; w != NULL;) {
		PendingWrite *run = w, *end = w;
		uint32_t n = w->nblks;
		IOReturn ret;

		while (end->next != NULL && end->next->block == end->block + end->nblks &&
		    n + end->next->nblks <= kWQRunBlocks) {
			end = end->next;
			n += end->nblks;
		}
		// a waiting read goes first: apfs holds its own lock across it
		for (int spins = 0; fReadWaiters > 0 && spins < 50; spins++)
			IOSleep(1);
		IOLockLock(fLock);
		uint32_t off = 0;

		for (PendingWrite *p = run;; p = p->next) {
			memcpy((uint8_t *)fBounce + off, p->data, p->nblks * 512);
			off += p->nblks * 512;
			if (p == end)
				break;
		}
		ret = transferRetry(true, run->block, n, NULL, 0);
		IOLockLock(fWQLock);
		for (PendingWrite *p = run;; p = p->next) {
			p->done = true;
			if (p == end)
				break;
		}
		if (ret != kIOReturnSuccess)
			fWQError = ret;
		IOLockUnlock(fWQLock);
		IOLockUnlock(fLock);
		w = end->next;
	}

	IOLockLock(fWQLock);
	fWQFlight = NULL;
	for (w = sorted; w != NULL; w = next) {
		next = w->next;
		freed += w->nblks * 512;
		IOFree(w->data, w->nblks * 512);
		IOFree(w, sizeof(*w));
	}
	fWQBytes -= freed;
	fWQBusy = false;
	IOLockWakeup(fWQLock, &fWQBytes, false);
	IOLockUnlock(fWQLock);
	logStats();
	IOLockLock(fWQLock);
}

// once a minute: commands, blocks per command and time on the card each way, and synchronizes
void
PDSun50iMMC::logStats(void)
{
	uint64_t now = mach_absolute_time(), ns;

	uint64_t last = fStatLast;

	absolutetime_to_nanoseconds(now - last, &ns);
	if (last != 0 && ns < 60ULL * NSEC_PER_SEC)
		return;
	// the reader and the write worker both get here: one of them logs
	if (!OSCompareAndSwap64(last, now, (volatile UInt64 *)&fStatLast))
		return;
	if (last != 0)
		IOLog("PDSun50iMMC: minute: reads %llu cmds %llu blocks %llu ms, writes %llu cmds %llu blocks %llu ms, "
		    "%llu syncs %llu ms\n", fStatCmds[0], fStatBlocks[0], fStatUs[0] / 1000, fStatCmds[1], fStatBlocks[1],
		    fStatUs[1] / 1000, fStatSyncs, fStatSyncUs / 1000);
	fStatCmds[0] = fStatCmds[1] = fStatBlocks[0] = fStatBlocks[1] = fStatUs[0] = fStatUs[1] = 0;
	fStatSyncs = fStatSyncUs = 0;
}
