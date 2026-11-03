#include "PDRockchipSFC.h"
#include "PDRockchipSFCDisk.h"

#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>

#define super IOService
OSDefineMetaClassAndStructors(PDRockchipSFC, IOService);

// FSPI registers (RV1106 TRM part 1, 18.4)
enum {
	kCTRL0 = 0x000, kIMR = 0x004, kICLR = 0x008, kRCVR = 0x010, kABIT0 = 0x018,
	kFSR = 0x020, kSR = 0x024, kVER = 0x02c, kMODE = 0x05c, kLENCTRL = 0x088, kLENEXT = 0x08c, kCMD = 0x100, kADDR = 0x104, kDATA = 0x108,
};
#define FSPI_PHYS  0xffac0000u
#define CMD_TRB(n)   ((uint32_t)(n) << 16)
#define CMD_ADDRB(n) ((uint32_t)(n) << 14)	// 0 none, 1 24 bits, 2 32 bits, 3 from ABIT0
#define CMD_WR       (1u << 12)
#define CMD_DUMM(n)  ((uint32_t)(n) << 8)
#define FSR_RXLVL(v) (((v) >> 16) & 0x1f)
#define FSR_TXLVL(v) (((v) >> 8) & 0x1f)	// entries left

// SPI NAND (Winbond W25N01GV/KV): 2 KiB pages, 64 pages a block, status and config registers
enum { kOpReset = 0xff, kOpReadID = 0x9f, kOpGetFeature = 0x0f, kOpSetFeature = 0x1f,
	kOpPageRead = 0x13, kOpReadCache = 0x03, kOpWriteEnable = 0x06, kOpProgramLoad = 0x02,
	kOpProgramExecute = 0x10, kOpBlockErase = 0xd8 };
enum { kRegProtect = 0xa0, kRegStatus = 0xc0, kRegConfig = 0xb0 };
#define ST_BUSY    0x01
#define ST_EFAIL   0x04
#define ST_PFAIL   0x08
#define ST_ECC(s)  (((s) >> 4) & 3)	// 0 clean, 1 corrected, 2 uncorrectable
#define CFG_ECCE   0x10
#define CFG_BUF    0x08

// The board's rootfs partition (mtdparts: 41 MiB in, 85 MiB), in 128 KiB blocks;
// pdsfcpart=<first block>,<blocks> picks another
#define PART_FIRST 328
#define PART_BLOCKS 680

uint32_t
PDRockchipSFC::reg(uint32_t off) const
{
	return *(volatile uint32_t *)(fRegs + off);
}

void
PDRockchipSFC::setReg(uint32_t off, uint32_t v)
{
	*(volatile uint32_t *)(fRegs + off) = v;
}

bool
PDRockchipSFC::waitIdle(void)
{
	for (uint32_t n = 0; n < 20000; n++) {
		if ((reg(kSR) & 1) == 0) {
			return true;
		}
		if (n > 100) {
			IODelay(1);
		}
	}
	return false;
}

void
PDRockchipSFC::recover(void)
{
	setReg(kRCVR, 1);
	waitIdle();
	drain();
}

// Words left in the receive FIFO would be read as the next command's data
void
PDRockchipSFC::drain(void)
{
	for (uint32_t n = 0; n < 64 && FSR_RXLVL(reg(kFSR)) != 0; n++) {
		(void)reg(kDATA);
	}
}

IOReturn
PDRockchipSFC::command(uint8_t op, uint32_t abits, uint32_t addr, uint32_t dummy,
    uint8_t *in, const uint8_t *out, uint32_t len)
{
	uint32_t cmd = op | CMD_TRB(len) | CMD_DUMM(dummy);

	if (len > 0x3fff || dummy > 15 || (in != NULL && out != NULL)) {
		return kIOReturnBadArgument;
	}
	if (!waitIdle()) {
		recover();
	}
	drain();
	// The length comes from LEN_EXT (TRB_SEL): U-Boot leaves that selected with its own
	// last length, which every command would otherwise read
	setReg(kLENCTRL, 1);
	setReg(kLENEXT, len);
	if (abits == 24) {
		cmd |= CMD_ADDRB(1);
	} else if (abits == 32) {
		cmd |= CMD_ADDRB(2);
	} else if (abits != 0) {
		setReg(kABIT0, abits - 1);
		cmd |= CMD_ADDRB(3);
	}
	if (out != NULL) {
		cmd |= CMD_WR;
	}
	setReg(kCMD, cmd);
	if (abits != 0) {
		setReg(kADDR, addr);
	}
	for (uint32_t done = 0; out != NULL && done < len; done += 4) {
		uint32_t w = 0, n = 0;

		while (FSR_TXLVL(reg(kFSR)) == 0) {
			if (++n > 200000) {
				IOLog("PDRockchipSFC: op 0x%02x FIFO stuck at byte %u of %u (FSR 0x%x SR 0x%x)\n", op, done,
				    len, reg(kFSR), reg(kSR));
				recover();
				return kIOReturnTimeout;
			}
		}
		memcpy(&w, out + done, len - done < 4 ? len - done : 4);
		setReg(kDATA, w);
	}
	for (uint32_t done = 0; in != NULL && done < len; done += 4) {
		uint32_t w, n = 0;

		while (FSR_RXLVL(reg(kFSR)) == 0) {
			if (++n > 200000) {
				IOLog("PDRockchipSFC: op 0x%02x FIFO stuck at byte %u of %u (FSR 0x%x SR 0x%x)\n", op, done,
				    len, reg(kFSR), reg(kSR));
				recover();
				return kIOReturnTimeout;
			}
		}
		w = reg(kDATA);
		memcpy(in + done, &w, len - done < 4 ? len - done : 4);
	}
	return waitIdle() ? kIOReturnSuccess : kIOReturnTimeout;
}

IOReturn
PDRockchipSFC::getFeature(uint8_t addr, uint8_t *value)
{
	return command(kOpGetFeature, 8, addr, 0, value, NULL, 1);
}

IOReturn
PDRockchipSFC::setFeature(uint8_t addr, uint8_t value)
{
	return command(kOpSetFeature, 8, addr, 0, NULL, &value, 1);
}

IOReturn
PDRockchipSFC::waitReady(uint8_t *status)
{
	// Block erase takes up to 10 ms (tBE), page program 700 us
	for (uint32_t n = 0; n < 50000; n++) {
		uint8_t s = 0;
		IOReturn kr = getFeature(kRegStatus, &s);

		if (kr != kIOReturnSuccess) {
			return kr;
		}
		if ((s & ST_BUSY) == 0) {
			if (status != NULL) {
				*status = s;
			}
			return kIOReturnSuccess;
		}
		IODelay(1);
	}
	return kIOReturnTimeout;
}

// Page into the chip's cache: 13h, a dummy byte and the 16 bit row as one 24 bit address
IOReturn
PDRockchipSFC::loadPage(uint32_t row)
{
	uint8_t s = 0;
	IOReturn kr = command(kOpPageRead, 24, row & 0xffff, 0, NULL, NULL, 0);

	if (kr == kIOReturnSuccess) {
		kr = waitReady(&s);
	}
	if (kr == kIOReturnSuccess && ST_ECC(s) == 2) {
		IOLog("PDRockchipSFC: uncorrectable ECC in page 0x%x\n", row);
		kr = kIOReturnIOError;
	}
	return kr;
}

// Out of the cache: 03h, the 16 bit column, then 8 dummy clocks (the low address byte)
IOReturn
PDRockchipSFC::readCache(uint16_t column, uint8_t *buf, uint32_t len)
{
	return command(kOpReadCache, 24, (uint32_t)column << 8, 0, buf, NULL, len);
}

IOReturn
PDRockchipSFC::writeEnable(void)
{
	return command(kOpWriteEnable, 0, 0, 0, NULL, NULL, 0);
}

// 02h loads the cache from column 0 (the spare area stays 0xff), 10h programs it into row
IOReturn
PDRockchipSFC::programPage(uint32_t row, const uint8_t *data)
{
	uint8_t s = 0;
	IOReturn kr = writeEnable();

	if (kr == kIOReturnSuccess) {
		kr = command(kOpProgramLoad, 16, 0, 0, NULL, data, fPageSize);
	}
	if (kr == kIOReturnSuccess) {
		kr = command(kOpProgramExecute, 24, row & 0xffff, 0, NULL, NULL, 0);
	}
	if (kr == kIOReturnSuccess) {
		kr = waitReady(&s);
	}
	if (kr == kIOReturnSuccess && (s & ST_PFAIL) != 0) {
		IOLog("PDRockchipSFC: program failed at page 0x%x (status 0x%02x)\n", row, s);
		kr = kIOReturnIOError;
	}
	return kr;
}

IOReturn
PDRockchipSFC::eraseBlock(uint32_t row)
{
	uint8_t s = 0;
	IOReturn kr = writeEnable();

	if (kr == kIOReturnSuccess) {
		kr = command(kOpBlockErase, 24, row & 0xffff, 0, NULL, NULL, 0);
	}
	if (kr == kIOReturnSuccess) {
		kr = waitReady(&s);
	}
	if (kr == kIOReturnSuccess && (s & ST_EFAIL) != 0) {
		IOLog("PDRockchipSFC: erase failed at page 0x%x (status 0x%02x)\n", row, s);
		kr = kIOReturnIOError;
	}
	return kr;
}

// Factory bad block marker: first spare byte of the block's first page not 0xff
bool
PDRockchipSFC::blockIsBad(uint32_t block)
{
	uint8_t oob[2] = { 0, 0 };

	if (loadPage(block * fPagesPerBlock) != kIOReturnSuccess ||
	    readCache((uint16_t)fPageSize, oob, sizeof(oob)) != kIOReturnSuccess) {
		return true;
	}
	return oob[0] != 0xff;
}

bool
PDRockchipSFC::start(IOService *provider)
{
	uint8_t raw[4] = { 0, 0, 0, 0 }, *id = raw + 1, cfg = 0;
	uint32_t part[2] = { PART_FIRST, PART_BLOCKS }, bad = 0;
	char arg[32];

	if (!super::start(provider)) {
		return false;
	}
	fPageSize = 2048;
	fPagesPerBlock = 64;
	if (PE_parse_boot_argn("pdsfcpart", arg, sizeof(arg))) {
		char *end = NULL;

		part[0] = (uint32_t)strtoul(arg, &end, 0);
		if (end != NULL && *end == ',') {
			part[1] = (uint32_t)strtoul(end + 1, NULL, 0);
		}
	}
	fFirstBlock = part[0];
	fBlocks = part[1];

	fLock = IOLockAlloc();
	fPageBuf = (uint8_t *)IOMalloc(fPageSize);
	fBlockMap = (uint32_t *)IOMalloc(sizeof(uint32_t) * fBlocks);
	fRegsMD = IOMemoryDescriptor::withPhysicalAddress(FSPI_PHYS, 0x1000, kIODirectionInOut);
	fRegsMap = fRegsMD != NULL ? fRegsMD->map(kIOMapInhibitCache) : NULL;
	if (fLock == NULL || fPageBuf == NULL || fBlockMap == NULL || fRegsMap == NULL) {
		IOLog("PDRockchipSFC: out of resources\n");
		return false;
	}
	fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();

	IOLog("PDRockchipSFC: regs at %p: VER 0x%x MODE 0x%x CTRL0 0x%x SR 0x%x FSR 0x%x LEN_CTRL 0x%x LEN_EXT 0x%x\n",
	    fRegs, reg(kVER), reg(kMODE), reg(kCTRL0), reg(kSR), reg(kFSR), reg(kLENCTRL), reg(kLENEXT));
	// U-Boot may leave memory-mapped mode on: a dummy read, then indirect mode (TRM 18.4)
	if (reg(kMODE) & 1) {
		(void)*(volatile uint32_t *)fRegs;
		setReg(kMODE, 0);
	}
	// Polled, every interrupt masked; one line for command, address and data, but U-Boot's
	// sampling phase and idle cycles kept (it samples on the falling edge at its clock)
	recover();
	setReg(kIMR, 0xffffffff);
	setReg(kICLR, 0xffffffff);
	setReg(kCTRL0, reg(kCTRL0) & ~0x3f00u);

	IOLog("PDRockchipSFC: reset\n");
	command(kOpReset, 0, 0, 0, NULL, NULL, 0);
	IODelay(2000);
	IOLog("PDRockchipSFC: reset done (SR 0x%x FSR 0x%x), reading id\n", reg(kSR), reg(kFSR));
	waitReady(NULL);
	// 9Fh, 8 dummy clocks, then the id: the controller applies no dummy cycles to a command
	// without an address, so the dummy byte is read as the first data byte (W25N01KV: EF AE 21)
	if (command(kOpReadID, 0, 0, 0, raw, NULL, sizeof(raw)) != kIOReturnSuccess || id[0] == 0 ||
	    id[0] == 0xff) {
		IOLog("PDRockchipSFC: no SPI NAND answers (id %02x %02x %02x)\n", id[0], id[1], id[2]);
		return false;
	}
	// On-chip ECC and buffer read mode, both the part's defaults
	if (getFeature(kRegConfig, &cfg) == kIOReturnSuccess && (cfg & (CFG_ECCE | CFG_BUF)) != (CFG_ECCE | CFG_BUF)) {
		setFeature(kRegConfig, cfg | CFG_ECCE | CFG_BUF);
	}

	// Block protection comes up covering the whole array: clear it so erase and program work
	{
		uint8_t prot = 0;

		if (getFeature(kRegProtect, &prot) == kIOReturnSuccess && (prot & 0x7c) != 0) {
			setFeature(kRegProtect, prot & ~0x7c);
			getFeature(kRegProtect, &prot);
		}
		IOLog("PDRockchipSFC: protection register 0x%02x\n", prot);
	}

	IOLog("PDRockchipSFC: id %02x %02x %02x config 0x%02x, scanning %u blocks\n", id[0], id[1], id[2], cfg, fBlocks);
	// Logical blocks skip bad ones in order, as nandwrite lays an image down
	for (uint32_t b = 0; b < fBlocks; b++) {
		if (blockIsBad(fFirstBlock + b)) {
			bad++;
			continue;
		}
		fBlockMap[fGoodBlocks++] = b;
	}
	IOLog("PDRockchipSFC: flash id %02x %02x %02x config 0x%02x; blocks %u..%u, %u bad, %llu KiB exposed\n",
	    id[0], id[1], id[2], cfg, fFirstBlock, fFirstBlock + fBlocks - 1, bad,
	    pageCount() * fPageSize / 1024);

	fDisk = OSTypeAlloc(PDRockchipSFCDisk);
	if (fDisk == NULL || !fDisk->initWithController(this) || !fDisk->attach(this)) {
		OSSafeReleaseNULL(fDisk);
		return false;
	}
	fDisk->registerService();
	registerService();
	return true;
}

IOReturn
PDRockchipSFC::readPages(UInt64 page, UInt64 count, IOMemoryDescriptor *buffer)
{
	IOReturn kr = kIOReturnSuccess;

	IOLockLock(fLock);
	for (UInt64 i = 0; i < count && kr == kIOReturnSuccess; i++) {
		UInt64 lp = page + i, lb = lp / fPagesPerBlock;
		uint32_t row;

		if (lb >= fGoodBlocks) {
			kr = kIOReturnBadArgument;
			break;
		}
		row = (fFirstBlock + fBlockMap[lb]) * fPagesPerBlock + (uint32_t)(lp % fPagesPerBlock);
		kr = loadPage(row);
		if (kr == kIOReturnSuccess) {
			kr = readCache(0, fPageBuf, fPageSize);
		}
		if (kr == kIOReturnSuccess && buffer->writeBytes(i * fPageSize, fPageBuf, fPageSize) != fPageSize) {
			kr = kIOReturnDMAError;
		}
	}
	IOLockUnlock(fLock);
	return kr;
}

IOReturn
PDRockchipSFC::readPage(UInt64 page, uint8_t *buf)
{
	UInt64 lb = page / fPagesPerBlock;
	IOReturn kr;

	if (lb >= fGoodBlocks) {
		return kIOReturnBadArgument;
	}
	IOLockLock(fLock);
	kr = loadPage((fFirstBlock + fBlockMap[lb]) * fPagesPerBlock + (uint32_t)(page % fPagesPerBlock));
	if (kr == kIOReturnSuccess) {
		kr = readCache(0, buf, fPageSize);
	}
	IOLockUnlock(fLock);
	return kr;
}

IOReturn
PDRockchipSFC::writeBlock(UInt64 block, const uint8_t *data)
{
	uint32_t first;
	IOReturn kr;

	if (block >= fGoodBlocks) {
		return kIOReturnBadArgument;
	}
	first = (fFirstBlock + fBlockMap[block]) * fPagesPerBlock;
	IOLockLock(fLock);
	kr = eraseBlock(first);
	for (uint32_t i = 0; kr == kIOReturnSuccess && i < fPagesPerBlock; i++) {
		kr = programPage(first + i, data + (size_t)i * fPageSize);
	}
	IOLockUnlock(fLock);
	return kr;
}

void
PDRockchipSFC::stop(IOService *provider)
{
	if (fDisk != NULL) {
		fDisk->detach(this);
		OSSafeReleaseNULL(fDisk);
	}
	super::stop(provider);
}

void
PDRockchipSFC::free(void)
{
	OSSafeReleaseNULL(fRegsMap);
	OSSafeReleaseNULL(fRegsMD);
	if (fPageBuf != NULL) {
		IOFree(fPageBuf, fPageSize);
	}
	if (fBlockMap != NULL) {
		IOFree(fBlockMap, sizeof(uint32_t) * fBlocks);
	}
	if (fLock != NULL) {
		IOLockFree(fLock);
	}
	super::free();
}
