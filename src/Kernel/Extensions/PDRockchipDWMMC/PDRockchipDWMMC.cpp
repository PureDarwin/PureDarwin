#include "PDRockchipDWMMC.h"
#include "PDRockchipDWMMCDisk.h"

#include <IOKit/IOLib.h>
#include <kern/thread.h>
#include <pexpert/pexpert.h>

#define super IOService
OSDefineMetaClassAndStructors(PDRockchipDWMMC, IOService);

// Host registers (TRM 22.4)
enum {
	kCTRL = 0x00, kPWREN = 0x04, kCLKDIV = 0x08, kCLKSRC = 0x0c, kCLKENA = 0x10, kTMOUT = 0x14,
	kCTYPE = 0x18, kBLKSIZ = 0x1c, kBYTCNT = 0x20, kINTMASK = 0x24, kCMDARG = 0x28, kCMD = 0x2c,
	kRESP0 = 0x30, kRINTSTS = 0x44, kSTATUS = 0x48, kFIFOTH = 0x4c, kCDETECT = 0x50,
	kVERID = 0x6c, kHCON = 0x70, kRSTN = 0x78, kBMOD = 0x80, kFIFO = 0x200,
};
#define SDMMC_PHYS   0xffaa0000u
#define VICRU_PHYS   0xff3b4000u	// CLKSEL_CON01 0x304, GATE_CON01 0x804, SOFTRST_CON01 0xa04
#define GPIO3IOC_PHYS 0xff558000u	// GPIO3A_IOMUX_SEL_L 0x40, _H 0x44

#define CTRL_RESETS     0x7		// controller, FIFO, DMA interface
#define CMD_START       (1u << 31)
#define CMD_HOLD        (1u << 29)
#define CMD_UPDCLK      (1u << 21)
#define CMD_INIT        (1u << 15)	// 80 clocks first, for CMD0
#define CMD_WAITPRV     (1u << 13)
#define CMD_AUTOSTOP    (1u << 12)
#define CMD_WRITE       (1u << 10)
#define CMD_DATA        (1u << 9)
#define CMD_CRC         (1u << 8)
#define CMD_LONG        (1u << 7)
#define CMD_RESP        (1u << 6)
#define R1  (CMD_RESP | CMD_CRC)
#define R2  (CMD_RESP | CMD_LONG | CMD_CRC)
#define R3  (CMD_RESP)				// OCR: no CRC

#define INT_RE    (1u << 1)
#define INT_CD    (1u << 2)
#define INT_DTO   (1u << 3)
#define INT_TXDR  (1u << 4)
#define INT_RXDR  (1u << 5)
#define INT_RCRC  (1u << 6)
#define INT_DCRC  (1u << 7)
#define INT_RTO   (1u << 8)
#define INT_DRTO  (1u << 9)
#define INT_HTO   (1u << 10)
#define INT_FRUN  (1u << 11)
#define INT_HLE   (1u << 12)
#define INT_SBE   (1u << 13)
#define INT_EBE   (1u << 15)
#define INT_CMDERR  (INT_RE | INT_RCRC | INT_RTO | INT_HLE)
#define INT_DATAERR (INT_DCRC | INT_DRTO | INT_HTO | INT_FRUN | INT_SBE | INT_EBE)
#define STATUS_BUSY      (1u << 9)
#define STATUS_FIFO(v)   (((v) >> 17) & 0x1fff)

// cclk_in is the 24 MHz oscillator: /60 for identification, /2 after
#define DIV_IDENT 30
#define DIV_XFER  1
#define BOUNCE_BLOCKS 64

static void
writeMasked(uint32_t phys, uint32_t off, uint32_t mask, uint32_t value)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(phys, 0x1000, kIODirectionInOut);
	IOMemoryMap *map = md != NULL ? md->map(kIOMapInhibitCache) : NULL;

	// Rockchip CRU/IOC registers: the high half enables writing the matching low bits
	if (map != NULL) {
		*(volatile uint32_t *)(map->getVirtualAddress() + off) = (mask << 16) | (value & mask);
	}
	OSSafeReleaseNULL(map);
	OSSafeReleaseNULL(md);
}

void
PDRockchipDWMMC::setupClocksAndPins(void)
{
	// cclk_src_sdmmc from xin_osc0, undivided; ungate hclk/cclk and the detect filter
	writeMasked(VICRU_PHYS, 0x304, 0x7f00, 1u << 14);
	writeMasked(VICRU_PHYS, 0x804, (1u << 11) | (1u << 12) | (1u << 13), 0);
	// Pulse the host's bus reset
	writeMasked(VICRU_PHYS, 0xa04, 1u << 12, 1u << 12);
	IODelay(10);
	writeMasked(VICRU_PHYS, 0xa04, 1u << 12, 0);
	IODelay(10);
	// GPIO3_A1 DET, A2 D1, A3 D0 / A4 CLK, A5 CMD, A6 D3, A7 D2: function 1
	writeMasked(GPIO3IOC_PHYS, 0x40, 0x7770, 0x1110);
	writeMasked(GPIO3IOC_PHYS, 0x44, 0x7777, 0x1111);
}

bool
PDRockchipDWMMC::resetController(void)
{
	setReg(kCTRL, reg(kCTRL) | CTRL_RESETS);
	for (uint32_t n = 0; n < 100000; n++) {
		if ((reg(kCTRL) & CTRL_RESETS) == 0) {
			return true;
		}
		IODelay(1);
	}
	IOLog("PDRockchipDWMMC: controller reset stuck, CTRL 0x%x\n", reg(kCTRL));
	return false;
}

// Clock registers load only through an update-clock command (TRM 22.6.2.3)
static bool
sendClockCommand(PDRockchipDWMMC *self, volatile uint8_t *regs)
{
	for (uint32_t tries = 0; tries < 10; tries++) {
		*(volatile uint32_t *)(regs + kRINTSTS) = INT_HLE;
		*(volatile uint32_t *)(regs + kCMD) = CMD_START | CMD_UPDCLK | CMD_WAITPRV | CMD_HOLD;
		for (uint32_t n = 0; n < 100000; n++) {
			if ((*(volatile uint32_t *)(regs + kCMD) & CMD_START) == 0) {
				return true;
			}
			if ((*(volatile uint32_t *)(regs + kRINTSTS) & INT_HLE) != 0) {
				break;
			}
			IODelay(1);
		}
	}
	(void)self;
	return false;
}

bool
PDRockchipDWMMC::updateClock(uint32_t div)
{
	setReg(kCLKENA, 0);
	if (!sendClockCommand(this, fRegs)) {
		return false;
	}
	setReg(kCLKDIV, div);
	setReg(kCLKSRC, 0);
	if (!sendClockCommand(this, fRegs)) {
		return false;
	}
	setReg(kCLKENA, 1);
	return sendClockCommand(this, fRegs);
}

bool
PDRockchipDWMMC::waitNotBusy(void)
{
	for (uint32_t n = 0; n < 500000; n++) {
		if ((reg(kSTATUS) & STATUS_BUSY) == 0) {
			return true;
		}
		IODelay(2);
	}
	return false;
}

IOReturn
PDRockchipDWMMC::command(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp)
{
	uint32_t st = 0;

	setReg(kRINTSTS, 0xffffffff);
	setReg(kCMDARG, arg);
	setReg(kCMD, CMD_START | CMD_HOLD | (flags & CMD_INIT ? 0 : CMD_WAITPRV) | flags | (idx & 0x3f));
	for (uint32_t n = 0; n < 200000 && (reg(kCMD) & CMD_START) != 0; n++) {
		IODelay(1);
	}
	for (uint32_t n = 0; n < 500000; n++) {
		st = reg(kRINTSTS);
		if ((st & (INT_CD | INT_HLE)) != 0) {
			break;
		}
		IODelay(1);
	}
	if ((st & INT_CD) == 0 || (st & INT_CMDERR) != 0) {
		return (st & INT_RTO) != 0 ? kIOReturnTimeout : kIOReturnIOError;
	}
	if (resp != NULL) {
		resp[0] = reg(kRESP0);
		if (flags & CMD_LONG) {
			resp[1] = reg(kRESP0 + 4);
			resp[2] = reg(kRESP0 + 8);
			resp[3] = reg(kRESP0 + 12);
		}
	}
	return kIOReturnSuccess;
}

IOReturn
PDRockchipDWMMC::appCommand(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp)
{
	uint32_t r[4];
	IOReturn kr = command(55, fRCA << 16, R1, r);

	return kr != kIOReturnSuccess ? kr : command(idx, arg, flags, resp);
}

bool
PDRockchipDWMMC::initCard(void)
{
	uint32_t r[4] = {}, ocr = 0, v2 = 0;
	IOReturn kr;

	setupClocksAndPins();
	IOLog("PDRockchipDWMMC: VERID 0x%x HCON 0x%x CDETECT 0x%x\n", reg(kVERID), reg(kHCON), reg(kCDETECT));
	if (!resetController()) {
		return false;
	}
	setReg(kPWREN, 1);
	setReg(kRSTN, 1);
	IOSleep(20);
	setReg(kINTMASK, 0);
	setReg(kRINTSTS, 0xffffffff);
	setReg(kCTRL, reg(kCTRL) & ~((1u << 4) | (1u << 25)));	// no interrupts, no internal DMA
	setReg(kBMOD, 0);
	setReg(kTMOUT, 0xffffff40);
	setReg(kCTYPE, 0);
	// 256-word FIFO: request past half full / at half empty
	setReg(kFIFOTH, (2u << 28) | (127u << 16) | 128u);
	if (!updateClock(DIV_IDENT)) {
		IOLog("PDRockchipDWMMC: identification clock would not load\n");
		return false;
	}
	IOSleep(5);

	command(0, 0, CMD_INIT, NULL);
	IOSleep(2);
	kr = command(8, 0x1aa, R1, r);
	v2 = kr == kIOReturnSuccess && (r[0] & 0xfff) == 0x1aa;
	IOLog("PDRockchipDWMMC: CMD8 0x%x resp 0x%x -> %s\n", kr, r[0], v2 ? "SD 2.0+" : "SD 1.x or none");
	fRCA = 0;
	for (uint32_t n = 0; n < 200; n++) {
		kr = appCommand(41, (v2 ? (1u << 30) : 0) | 0x00ff8000, R3, r);
		if (kr != kIOReturnSuccess) {
			IOLog("PDRockchipDWMMC: ACMD41 failed 0x%x (RINTSTS 0x%x): no card?\n", kr, reg(kRINTSTS));
			return false;
		}
		ocr = r[0];
		if (ocr & (1u << 31)) {
			break;
		}
		IOSleep(10);
	}
	if ((ocr & (1u << 31)) == 0) {
		IOLog("PDRockchipDWMMC: card stayed busy, OCR 0x%x\n", ocr);
		return false;
	}
	fHighCapacity = (ocr & (1u << 30)) != 0;

	if (command(2, 0, R2, r) != kIOReturnSuccess) {
		IOLog("PDRockchipDWMMC: CMD2 failed\n");
		return false;
	}
	IOLog("PDRockchipDWMMC: CID %08x %08x %08x %08x\n", r[3], r[2], r[1], r[0]);
	if (command(3, 0, R1, r) != kIOReturnSuccess) {
		IOLog("PDRockchipDWMMC: CMD3 failed\n");
		return false;
	}
	fRCA = r[0] >> 16;
	if (command(9, fRCA << 16, R2, r) != kIOReturnSuccess) {
		IOLog("PDRockchipDWMMC: CMD9 failed\n");
		return false;
	}
	IOLog("PDRockchipDWMMC: CSD %08x %08x %08x %08x\n", r[3], r[2], r[1], r[0]);
	// CSD in r[3] (bits 127:96) .. r[0] (31:0)
	if ((r[3] >> 30) == 1) {
		uint32_t csize = ((r[2] & 0x3f) << 16) | (r[1] >> 16);	// bits 69:48

		fBlocks = ((UInt64)csize + 1) * 1024;
	} else {
		uint32_t blen = (r[2] >> 16) & 0xf;			// bits 83:80
		uint32_t csize = ((r[2] & 0x3ff) << 2) | (r[1] >> 30);	// bits 73:62
		uint32_t mult = (r[1] >> 15) & 0x7;			// bits 49:47

		fBlocks = ((UInt64)csize + 1) << (mult + 2) << blen >> 9;
	}
	if (command(7, fRCA << 16, R1, r) != kIOReturnSuccess || !waitNotBusy()) {
		IOLog("PDRockchipDWMMC: CMD7 failed\n");
		return false;
	}
	if (!PE_parse_boot_argn("pdsd1bit", &v2, sizeof(v2))) {
		if (appCommand(6, 2, R1, r) == kIOReturnSuccess) {
			setReg(kCTYPE, 1);
		} else {
			IOLog("PDRockchipDWMMC: ACMD6 failed, staying 1-bit\n");
		}
	}
	if (!fHighCapacity) {
		command(16, 512, R1, r);
	}
	setReg(kBLKSIZ, 512);
	if (!updateClock(DIV_XFER)) {
		IOLog("PDRockchipDWMMC: transfer clock would not load\n");
		return false;
	}
	// pdsdpart=<first block>,<blocks>: expose one region, as the Rockchip SD layout keeps its
	// partition table in the U-Boot environment rather than on the card (0 blocks = to the end)
	fPartStart = 0;
	fPartBlocks = fBlocks;
	{
		char arg[48];

		if (PE_parse_boot_argn("pdsdpart", arg, sizeof(arg))) {
			const char *c = arg;
			UInt64 first = 0, count = 0;

			while (*c >= '0' && *c <= '9') first = first * 10 + (UInt64)(*c++ - '0');
			if (*c == ',') {
				c++;
				while (*c >= '0' && *c <= '9') count = count * 10 + (UInt64)(*c++ - '0');
			}
			if (first < fBlocks) {
				fPartStart = first;
				fPartBlocks = count != 0 && first + count <= fBlocks ? count : fBlocks - first;
			}
			IOLog("PDRockchipDWMMC: exposing blocks %llu..%llu\n", fPartStart, fPartStart + fPartBlocks - 1);
		}
	}
	IOLog("PDRockchipDWMMC: RCA 0x%x, %s, %llu blocks (%llu MiB), %u-bit\n", fRCA,
	    fHighCapacity ? "SDHC/SDXC" : "SDSC", fBlocks, fBlocks / 2048, reg(kCTYPE) ? 4 : 1);
	return fPartBlocks != 0;
}

IOReturn
PDRockchipDWMMC::transfer(bool write, uint32_t block, uint32_t count, uint8_t *buf)
{
	uint32_t words = count * 128, done = 0, st = 0, cmd;
	uint32_t *w = (uint32_t *)buf;
	IOReturn kr;

	if (!waitNotBusy()) {
		return kIOReturnBusy;
	}
	setReg(kCTRL, reg(kCTRL) | 2);	// FIFO reset
	for (uint32_t n = 0; n < 10000 && (reg(kCTRL) & 2) != 0; n++) {
		IODelay(1);
	}
	setReg(kBYTCNT, count * 512);
	setReg(kBLKSIZ, 512);
	cmd = R1 | CMD_DATA | (write ? CMD_WRITE : 0) | (count > 1 ? CMD_AUTOSTOP : 0);
	kr = command(write ? (count > 1 ? 25 : 24) : (count > 1 ? 18 : 17),
	    fHighCapacity ? block : block * 512, cmd, NULL);
	if (kr != kIOReturnSuccess) {
		return kr;
	}
	for (uint32_t n = 0; n < 2000000; n++) {
		st = reg(kRINTSTS);
		if (st & INT_DATAERR) {
			break;
		}
		if (!write) {
			for (uint32_t c = STATUS_FIFO(reg(kSTATUS)); c > 0 && done < words; c--) {
				w[done++] = reg(kFIFO);
			}
			setReg(kRINTSTS, INT_RXDR);
		} else if (st & INT_TXDR) {
			for (uint32_t c = 256 - STATUS_FIFO(reg(kSTATUS)); c > 0 && done < words; c--) {
				setReg(kFIFO, w[done++]);
			}
			setReg(kRINTSTS, INT_TXDR);
		}
		if ((st & INT_DTO) && (write || done >= words)) {
			break;
		}
		if (n > 1000) {
			IODelay(1);
		}
	}
	if ((st & INT_DATAERR) || !(st & INT_DTO) || done < words) {
		static uint32_t sErrLogs;

		if (sErrLogs++ < 20) {
			IOLog("PDRockchipDWMMC: %s block %u x%u failed: RINTSTS 0x%x STATUS 0x%x, %u/%u words\n",
			    write ? "write" : "read", block, count, st, reg(kSTATUS), done, words);
		}
		resetController();
		return (st & INT_DRTO) ? kIOReturnTimeout : kIOReturnIOError;
	}
	return write && !waitNotBusy() ? kIOReturnTimeout : kIOReturnSuccess;
}

IOReturn
PDRockchipDWMMC::readWrite(bool write, UInt64 block, UInt64 count, IOMemoryDescriptor *buffer)
{
	IOReturn kr = kIOReturnSuccess;

	if (write && !fWritable) {
		return kIOReturnNotWritable;
	}
	if (block + count > fPartBlocks) {
		return kIOReturnBadArgument;
	}
	block += fPartStart;
	IOLockLock(fLock);
	for (UInt64 off = 0; off < count && kr == kIOReturnSuccess;) {
		uint32_t n = (uint32_t)(count - off < BOUNCE_BLOCKS ? count - off : BOUNCE_BLOCKS);

		if (write && buffer->readBytes(off * 512, fBounce, n * 512) != n * 512) {
			kr = kIOReturnDMAError;
			break;
		}
		kr = transfer(write, (uint32_t)(block + off), n, fBounce);
		if (kr == kIOReturnSuccess && !write && buffer->writeBytes(off * 512, fBounce, n * 512) != n * 512) {
			kr = kIOReturnDMAError;
		}
		off += n;
	}
	IOLockUnlock(fLock);
	return kr;
}

// Identification sleeps for seconds at 400 kHz and the SD disk must not become disk0
// ahead of the SPI NAND root, so the card comes up on its own thread, later
void
PDRockchipDWMMC::initThread(void *arg, wait_result_t wr)
{
	PDRockchipDWMMC *self = (PDRockchipDWMMC *)arg;
	uint32_t delayMs = 8000;

	(void)wr;
	PE_parse_boot_argn("pdsddelay", &delayMs, sizeof(delayMs));
	IOSleep(delayMs);
	IOLockLock(self->fLock);
	bool ok = self->initCard();
	IOLockUnlock(self->fLock);
	if (ok) {
		uint8_t *b = self->fBounce;

		if (self->transfer(false, 0, 1, b) == kIOReturnSuccess) {
			IOLog("PDRockchipDWMMC: block 0: %02x %02x %02x %02x .. signature %02x%02x\n",
			    b[0], b[1], b[2], b[3], b[510], b[511]);
		}
		self->fDisk = OSTypeAlloc(PDRockchipDWMMCDisk);
		if (self->fDisk != NULL && self->fDisk->initWithController(self) && self->fDisk->attach(self)) {
			self->fDisk->registerService();
		} else {
			OSSafeReleaseNULL(self->fDisk);
		}
	}
	self->release();
	thread_terminate(current_thread());
}

bool
PDRockchipDWMMC::start(IOService *provider)
{
	thread_t th;
	uint32_t rw = 0;

	if (!super::start(provider)) {
		return false;
	}
	fRegsMD = IOMemoryDescriptor::withPhysicalAddress(SDMMC_PHYS, 0x1000, kIODirectionInOut);
	fRegsMap = fRegsMD != NULL ? fRegsMD->map(kIOMapInhibitCache) : NULL;
	fLock = IOLockAlloc();
	fBounce = (uint8_t *)IOMalloc(BOUNCE_BLOCKS * 512);
	if (fRegsMap == NULL || fLock == NULL || fBounce == NULL) {
		return false;
	}
	fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();
	// Writes stay off unless asked for: the card may hold someone's data
	fWritable = PE_parse_boot_argn("pdsdrw", &rw, sizeof(rw)) && rw != 0;
	registerService();
	retain();
	if (kernel_thread_start(&PDRockchipDWMMC::initThread, this, &th) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(th);
	return true;
}

void
PDRockchipDWMMC::free(void)
{
	OSSafeReleaseNULL(fDisk);
	OSSafeReleaseNULL(fRegsMap);
	OSSafeReleaseNULL(fRegsMD);
	if (fBounce != NULL) {
		IOFree(fBounce, BOUNCE_BLOCKS * 512);
	}
	if (fLock != NULL) {
		IOLockFree(fLock);
	}
	super::free();
}
