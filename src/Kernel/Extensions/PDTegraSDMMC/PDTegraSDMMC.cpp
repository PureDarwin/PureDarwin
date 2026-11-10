// SPDX-License-Identifier: MIT
// tegra210 sdmmc (sdhci 3.0) driver for the jetson tx1 emmc and sd slot, read-only unless pdtegrammcrw=1

#include "PDTegraSDMMC.h"
#include "PDTegraSDMMCDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

#define super IOService
OSDefineMetaClassAndStructors(PDTegraSDMMC, IOService);

// sdhci registers, all reached as 32-bit words
#define kBLKSIZE	0x04    // block size, block count in the upper half
#define kARG		0x08
#define kXFER_CMD	0x0c    // transfer mode, command in the upper half: one write issues it
#define kRESP0		0x10
#define kBUFFER		0x20
#define kPRESENT	0x24
#define kHOSTCTL	0x28    // host control, power control in byte 1
#define kCLKCTL		0x2c    // clock control, timeout in byte 2, software reset in byte 3
#define kINTSTS		0x30
#define kINTEN		0x34
#define kSIGEN		0x38
#define kCAPS		0x40
#define kVERSION	0xfc
#define kVENDOR_CLK	0x100

#define kPRESENT_CMD	(1u << 0)
#define kPRESENT_DAT	(1u << 1)

#define kINT_CMD	(1u << 0)
#define kINT_XFER	(1u << 1)
#define kINT_BUFWR	(1u << 4)
#define kINT_BUFRD	(1u << 5)
#define kINT_ERR	(1u << 15)

#define kXFER_BLKCNT	(1u << 1)
#define kXFER_ACMD12	(1u << 2)
#define kXFER_READ	(1u << 4)
#define kXFER_MULTI	(1u << 5)

#define kHOST_4BIT	(1u << 1)
#define kHOST_8BIT	(1u << 5)

#define kCLK_INT_EN	(1u << 0)
#define kCLK_STABLE	(1u << 1)
#define kCLK_SD_EN	(1u << 2)
#define kRESET_CMD	(1u << 25)
#define kRESET_DAT	(1u << 26)

// command flags: response kind in the low bits as sdhci encodes them
#define kRspNone	0x00u
#define kRsp136		0x01u
#define kRsp48		0x02u
#define kRsp48Busy	0x03u
#define kRspCRC		0x08u
#define kRspIdx		0x10u
#define kData		0x20u
#define kDataWrite	0x100u
#define kNoCheck	0x200u    // r6/r7 carry no card status word
#define kR1		(kRsp48 | kRspCRC | kRspIdx)
#define kR1b		(kRsp48Busy | kRspCRC | kRspIdx)
#define kR2		(kRsp136 | kRspCRC)
#define kR3		(kRsp48)

#define kR1_ERRORS	0xfdffe008u

// car: sdmmc4 is bit 15 and sdmmc1 bit 14 of the l bank, source registers take pll_p at 408 mhz
#define kCAR_PHYS		0x60006000ULL
#define kCAR_RST_L		0x004
#define kCAR_ENB_L		0x010
#define kCAR_SRC_SDMMC1		0x150
#define kCAR_SRC_SDMMC4		0x164
#define kCAR_RST_L_CLR		0x304
#define kCAR_ENB_L_SET		0x320
#define kCAR_SDMMC1_BIT		(1u << 14)
#define kCAR_SDMMC4_BIT		(1u << 15)
#define kSDMMC1_PHYS		0x700b0000ULL
#define kSDMMC4_PHYS		0x700b0600ULL
// pll_p / 8.5, the 7.1 divider field is 2 * 8.5 - 2
#define kCAR_SRC_PLLP_48M	15u
#define kBaseHz			48000000u

#define kIdentHz		400000u
#define kTransferHz		24000000u
#define kMaxBlocksPerCmd	128u

static volatile uint8_t *
map_phys(IOPhysicalAddress phys, IOByteCount size, IOMemoryMap **outMap)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(phys, size, kIODirectionOutIn);
	IOMemoryMap *map;

	if (desc == NULL) return NULL;
	map = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (map == NULL) return NULL;
	*outMap = map;
	return (volatile uint8_t *)map->getVirtualAddress();
}

bool
PDTegraSDMMC::mapRegisters(IOService *provider)
{
	OSData *reg = OSDynamicCast(OSData, provider->getProperty("reg"));

	fRegMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	if (fRegMap != NULL)
		fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();
	else if (reg != NULL && reg->getLength() >= sizeof(uint64_t))
		fRegs = map_phys(*(const uint64_t *)reg->getBytesNoCopy(), 0x1000, &fRegMap);
	fCAR = map_phys(kCAR_PHYS, 0x1000, &fCARMap);
	return fRegs != NULL && fCAR != NULL;
}

bool
PDTegraSDMMC::start(IOService *provider)
{
	OSData *status = OSDynamicCast(OSData, provider->getProperty("status"));
	uint32_t width = 8;

	OSData *bw = OSDynamicCast(OSData, provider->getProperty("bus-width"));
	uint32_t cells = bw != NULL && bw->getLength() >= 4 ? *(const uint32_t *)bw->getBytesNoCopy() : 0;

	OSData *reg = OSDynamicCast(OSData, provider->getProperty("reg"));
	uint64_t phys = 0;
	uint32_t slot = 0;

	if (!super::start(provider)) return false;

	if (status != NULL && strncmp((const char *)status->getBytesNoCopy(), "okay", status->getLength()) != 0) {
		IOLog("PDTegraSDMMC: %s is not okay, skipped\n", provider->getName());
		return false;
	}
	// the slot comes from the reg address, a big-endian 64-bit cell pair
	if (reg != NULL && reg->getLength() >= 8) {
		const uint8_t *p = (const uint8_t *)reg->getBytesNoCopy();

		for (int i = 0; i < 8; i++) phys = phys << 8 | p[i];
		if (phys != kSDMMC1_PHYS && phys != kSDMMC4_PHYS) phys = __builtin_bswap64(phys);
	}
	if (phys == kSDMMC1_PHYS) {
		fSD = true;
		fCARBit = kCAR_SDMMC1_BIT;
		fCARSrc = kCAR_SRC_SDMMC1;
	} else if (phys == kSDMMC4_PHYS || provider->getProperty("nvidia,is-emmc") != NULL ||
	    cells == 8 || cells == 0x08000000u) {
		fCARBit = kCAR_SDMMC4_BIT;
		fCARSrc = kCAR_SRC_SDMMC4;
	} else {
		IOLog("PDTegraSDMMC: %s (0x%llx) is neither the emmc nor the sd slot, skipped\n",
		    provider->getName(), phys);
		return false;
	}
	// pdtegrammcslot=1 attaches only the sd card, 4 only the emmc
	if (PE_parse_boot_argn("pdtegrammcslot", &slot, sizeof(slot)) && slot != (fSD ? 1u : 4u)) {
		IOLog("PDTegraSDMMC: %s skipped, pdtegrammcslot=%u\n", provider->getName(), slot);
		return false;
	}

	if (!mapRegisters(provider)) {
		IOLog("PDTegraSDMMC: cannot map registers\n");
		return false;
	}
	IOLog("PDTegraSDMMC: %s at 0x%llx\n", provider->getName(),
	    fRegMap != NULL ? (unsigned long long)fRegMap->getPhysicalAddress() : 0ULL);

	fLock = IOLockAlloc();
	fBounce = (uint32_t *)IOMalloc(kMaxBlocksPerCmd * 512);
	if (fLock == NULL || fBounce == NULL) return false;

	// pdtegrammcrw=1 allows writes, anything else keeps the emmc read-only
	{
		uint32_t rw = 0;

		fWritable = PE_parse_boot_argn("pdtegrammcrw", &rw, sizeof(rw)) && rw == 1;
	}
	// pdtegrammcbw=1, 4 or 8 caps the data bus width
	if (PE_parse_boot_argn("pdtegrammcbw", &width, sizeof(width)) && width != 1 && width != 4)
		width = 8;
	fBusWidth = fSD && width == 8 ? 4 : width;

	if (!bringUpController() || !(fSD ? identifySD() : identifyCard())) {
		IOLog("PDTegraSDMMC: no usable %s\n", fSD ? "sd card" : "emmc");
		return false;
	}
	IOLog("PDTegraSDMMC: %s %s, %llu blocks of 512 bytes (%llu MB), %u-bit at %u khz, %s\n",
	    fSD ? "sd" : "emmc", fProduct, fBlockCount, (fBlockCount * 512) / (1024 * 1024), fBusWidth, kTransferHz / 1000,
	    fWritable ? "read-write" : "read-only");

	fDisk = new PDTegraSDMMCDisk;
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
PDTegraSDMMC::stop(IOService *provider)
{
	if (fDisk != NULL) {
		fDisk->detach(this);
		OSSafeReleaseNULL(fDisk);
	}
	super::stop(provider);
}

void
PDTegraSDMMC::free(void)
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
	OSSafeReleaseNULL(fCARMap);
	super::free();
}

// u-boot normally leaves the clock running and the pads calibrated, a cold emmc controller gets its
// clock started here and runs with the pads uncalibrated
bool
PDTegraSDMMC::bringUpController(void)
{
	volatile uint32_t *car = (volatile uint32_t *)fCAR;
	bool off = (car[kCAR_ENB_L / 4] & fCARBit) == 0 || (car[kCAR_RST_L / 4] & fCARBit) != 0;

	IOLog("PDTegraSDMMC: car enb 0x%x rst 0x%x src 0x%x, present 0x%x host 0x%x clk 0x%x caps 0x%x ver 0x%x\n",
	    car[kCAR_ENB_L / 4], car[kCAR_RST_L / 4], car[fCARSrc / 4],
	    off ? 0 : rd(kPRESENT), off ? 0 : rd(kHOSTCTL), off ? 0 : rd(kCLKCTL),
	    off ? 0 : rd(kCAPS), off ? 0 : rd(kVERSION));

	// the sd pads need u-boot's 3.3 v setup, so a cold sd slot is not brought up here
	if (off && fSD) {
		IOLog("PDTegraSDMMC: sd slot clock is off, u-boot did not set it up\n");
		return false;
	}
	// the card clock stops before its source changes
	if (!off) rmw(kCLKCTL, kCLK_SD_EN, 0);
	car[fCARSrc / 4] = (0u << 29) | kCAR_SRC_PLLP_48M;
	__asm__ volatile ("dsb sy" ::: "memory");
	if (off) {
		car[kCAR_ENB_L_SET / 4] = fCARBit;
		IODelay(5);
		car[kCAR_RST_L_CLR / 4] = fCARBit;
		__asm__ volatile ("dsb sy" ::: "memory");
		IODelay(10);
		IOLog("PDTegraSDMMC: clock was off, started it, the pads stay uncalibrated\n");
	}
	IOLog("PDTegraSDMMC: vendor clk 0x%x\n", rd(kVENDOR_CLK));

	resetLines(kRESET_CMD | kRESET_DAT);

	// bus power on at 3.3 v (sdhci power control), then statuses latched without raising interrupts
	if ((rd(kHOSTCTL) & (1u << 8)) == 0) rmw(kHOSTCTL, 0xff00u, 0x0f00u);
	rmw(kHOSTCTL, kHOST_4BIT | kHOST_8BIT | (1u << 2) | (3u << 3), 0);
	wr(kSIGEN, 0);
	wr(kINTEN, 0xffff0033u);
	wr(kINTSTS, 0xffffffffu);
	return setClock(kIdentHz);
}

bool
PDTegraSDMMC::setClock(uint32_t hz)
{
	uint32_t div = hz >= kBaseHz ? 0 : (kBaseHz + 2 * hz - 1) / (2 * hz);
	uint64_t end;

	if (div > 0x3ff) div = 0x3ff;
	rmw(kCLKCTL, kCLK_SD_EN, 0);
	rmw(kCLKCTL, 0x000fffffu, ((div & 0xff) << 8) | (((div >> 8) & 3) << 6) | kCLK_INT_EN | (0xeu << 16));
	clock_interval_to_deadline(20, kMillisecondScale, &end);
	while ((rd(kCLKCTL) & kCLK_STABLE) == 0) {
		if (mach_absolute_time() > end) {
			IOLog("PDTegraSDMMC: internal clock not stable (0x%x)\n", rd(kCLKCTL));
			return false;
		}
		IODelay(10);
	}
	rmw(kCLKCTL, 0, kCLK_SD_EN);
	IODelay(100);
	return true;
}

void
PDTegraSDMMC::resetLines(uint32_t lines)
{
	uint64_t end;

	rmw(kCLKCTL, 0, lines);
	clock_interval_to_deadline(100, kMillisecondScale, &end);
	while ((rd(kCLKCTL) & lines) != 0 && mach_absolute_time() < end) IODelay(10);
}

bool
PDTegraSDMMC::waitStatus(uint32_t bits, uint32_t timeoutMs, uint32_t *got)
{
	uint64_t end;
	uint32_t st = 0;

	clock_interval_to_deadline(timeoutMs, kMillisecondScale, &end);
	for (;;) {
		st = rd(kINTSTS);
		if ((st & (bits | kINT_ERR)) != 0) break;
		if (mach_absolute_time() > end) break;
		IODelay(2);
	}
	if (got != NULL) *got = st;
	return (st & kINT_ERR) == 0 && (st & bits) != 0;
}

bool
PDTegraSDMMC::command(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp, uint32_t nblks)
{
	uint32_t inhibit = kPRESENT_CMD, mode = 0, st = 0;
	bool data = (flags & kData) != 0, busy = (flags & 0x3u) == kRsp48Busy;
	uint64_t end;

	if (data || busy) inhibit |= kPRESENT_DAT;
	clock_interval_to_deadline(100, kMillisecondScale, &end);
	while ((rd(kPRESENT) & inhibit) != 0) {
		if (mach_absolute_time() > end) {
			IOLog("PDTegraSDMMC: cmd%u: lines busy (0x%x)\n", idx, rd(kPRESENT));
			resetLines(kRESET_CMD | kRESET_DAT);
			return false;
		}
		IODelay(5);
	}

	wr(kINTSTS, 0xffffffffu);
	if (data) {
		wr(kBLKSIZE, (nblks << 16) | 512u);
		mode = ((flags & kDataWrite) != 0 ? 0 : kXFER_READ) | kXFER_BLKCNT;
		if (nblks > 1) mode |= kXFER_MULTI | kXFER_ACMD12;
	}
	wr(kARG, arg);
	wr(kXFER_CMD, ((idx << 8 | (flags & 0x3fu)) << 16) | mode);

	if (!waitStatus(kINT_CMD, 100, &st)) goto fail;
	wr(kINTSTS, kINT_CMD);
	if ((flags & 0x3u) == kRsp136) {
		for (int i = 0; i < 4; i++) resp[i] = rd(kRESP0 + 4 * i);
	} else if ((flags & 0x3u) != kRspNone) {
		resp[0] = rd(kRESP0);
	}

	for (uint32_t b = 0; data && b < nblks; b++) {
		uint32_t *buf = fBounce + b * 128;
		uint32_t ready = (flags & kDataWrite) != 0 ? kINT_BUFWR : kINT_BUFRD;

		if (!waitStatus(ready, 1000, &st)) goto fail;
		wr(kINTSTS, ready);
		if (ready == kINT_BUFWR) {
			for (int w = 0; w < 128; w++) wr(kBUFFER, buf[w]);
		} else {
			for (int w = 0; w < 128; w++) buf[w] = rd(kBUFFER);
		}
	}
	// a write ends when the card leaves busy, which can take a while
	if ((data || busy) && !waitStatus(kINT_XFER, busy || (flags & kDataWrite) != 0 ? 3000 : 1000, &st)) goto fail;
	wr(kINTSTS, 0xffffffffu);

	if ((flags & (kRspIdx | kNoCheck)) == kRspIdx && (resp[0] & kR1_ERRORS) != 0) {
		IOLog("PDTegraSDMMC: cmd%u arg 0x%x: card status 0x%x\n", idx, arg, resp[0]);
		return false;
	}
	return true;

fail:
	IOLog("PDTegraSDMMC: cmd%u arg 0x%x failed, status 0x%x present 0x%x\n", idx, arg, st, rd(kPRESENT));
	wr(kINTSTS, 0xffffffffu);
	resetLines(kRESET_CMD | kRESET_DAT);
	return false;
}

bool
PDTegraSDMMC::readExtCSD(uint8_t *ext)
{
	uint32_t resp[4];

	if (!command(8, 0, kR1 | kData, resp, 1)) return false;
	memcpy(ext, fBounce, 512);
	return true;
}

// the switch only touches the volatile bus width byte of ext_csd, the card's data stays as it is
bool
PDTegraSDMMC::setBusWidth(uint32_t width, const uint8_t *ref)
{
	uint32_t resp[4], val = width == 8 ? 2 : width == 4 ? 1 : 0;
	uint32_t host = width == 8 ? kHOST_8BIT : width == 4 ? kHOST_4BIT : 0;
	uint8_t *ext = (uint8_t *)IOMalloc(512);
	bool same;

	if (ext == NULL) return false;
	same = command(6, (3u << 24) | (183u << 16) | (val << 8), kR1b, resp);
	rmw(kHOSTCTL, kHOST_4BIT | kHOST_8BIT, same ? host : 0);
	// the properties segment of ext_csd reads back the same at the new width
	same = same && readExtCSD(ext) && memcmp(ext + 192, ref + 192, 320) == 0;
	IOFree(ext, 512);
	return same;
}

bool
PDTegraSDMMC::identifyCard(void)
{
	uint32_t resp[4], ocr = 0;
	uint8_t *ext, cid[16];
	uint64_t end;
	bool ok;

	(void)command(0, 0, kRspNone, resp);
	IODelay(1000);

	// sector addressing and both voltage windows, until the card leaves busy
	clock_interval_to_deadline(1000, kMillisecondScale, &end);
	do {
		if (!command(1, 0x40ff8080u, kR3, resp)) return false;
		ocr = resp[0];
		if ((ocr & (1u << 31)) != 0) break;
		IODelay(1000);
	} while (mach_absolute_time() < end);
	if ((ocr & (1u << 31)) == 0) {
		IOLog("PDTegraSDMMC: emmc stayed busy, ocr 0x%x\n", ocr);
		return false;
	}

	if (!command(2, 0, kR2, resp)) return false;
	// sdhci drops the crc byte, so cid byte n sits at byte n-1 of the response words
	memcpy(cid, resp, 16);
	for (int i = 0; i < 6; i++) {
		char c = (char)cid[11 - i];
		fProduct[i] = (c >= 0x20 && c < 0x7f) ? c : '?';
	}
	fProduct[6] = '\0';

	if (!command(3, 1u << 16, kR1, resp)) return false;
	if (!command(7, 1u << 16, kR1b, resp)) return false;

	ext = (uint8_t *)IOMalloc(512);
	if (ext == NULL) return false;
	ok = readExtCSD(ext) && setClock(kTransferHz) && readExtCSD(ext);
	if (ok) {
		fBlockCount = (uint64_t)ext[212] | (uint64_t)ext[213] << 8 |
		    (uint64_t)ext[214] << 16 | (uint64_t)ext[215] << 24;
		IOLog("PDTegraSDMMC: ocr 0x%x, ext_csd rev %u, boot partitions %u KB, card type 0x%x, partition config 0x%x\n",
		    ocr, ext[192], ext[226] * 128, ext[196], ext[179]);
		// widest bus that reads ext_csd back unchanged
		while (fBusWidth > 1 && !setBusWidth(fBusWidth, ext)) {
			IOLog("PDTegraSDMMC: %u-bit bus failed\n", fBusWidth);
			fBusWidth = fBusWidth == 8 ? 4 : 1;
		}
		if (fBusWidth == 1) ok = setBusWidth(1, ext);
	}
	IOFree(ext, 512);
	return ok && fBlockCount != 0;
}

bool
PDTegraSDMMC::appCommand(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp)
{
	if (!command(55, fRCA << 16, kR1, resp)) return false;
	return command(idx, arg, flags, resp);
}

// acmd6 switches the card, then block 0 has to read back the same at the new width
bool
PDTegraSDMMC::setSDBusWidth(uint32_t width, const uint8_t *ref)
{
	uint32_t resp[4];
	bool same;

	same = appCommand(6, width == 4 ? 2 : 0, kR1, resp);
	rmw(kHOSTCTL, kHOST_4BIT | kHOST_8BIT, same && width == 4 ? kHOST_4BIT : 0);
	same = same && command(17, 0, kR1 | kData, resp, 1) && memcmp(fBounce, ref, 512) == 0;
	return same;
}

// sd init: cmd0, cmd8, acmd41 until ready, cid, rca, csd for the size, select, then the bus width
bool
PDTegraSDMMC::identifySD(void)
{
	uint32_t resp[4], ocr = 0, csd[4], hcs = 0;
	uint8_t cid[16], *blk0;
	uint64_t end;
	bool ok;

	(void)command(0, 0, kRspNone, resp);
	IODelay(1000);

	// a v2 card echoes the check pattern and may then be high capacity
	if (command(8, 0x1aa, kR1 | kNoCheck, resp) && (resp[0] & 0xfff) == 0x1aa) hcs = 1u << 30;
	IOLog("PDTegraSDMMC: sd cmd8 %s (0x%x)\n", hcs != 0 ? "v2" : "no answer", resp[0]);

	clock_interval_to_deadline(1500, kMillisecondScale, &end);
	do {
		if (!appCommand(41, hcs | 0x00ff8000u, kR3, resp)) return false;
		ocr = resp[0];
		if ((ocr & (1u << 31)) != 0) break;
		IODelay(5000);
	} while (mach_absolute_time() < end);
	if ((ocr & (1u << 31)) == 0) {
		IOLog("PDTegraSDMMC: sd card stayed busy, ocr 0x%x\n", ocr);
		return false;
	}
	fByteAddr = (ocr & (1u << 30)) == 0;

	if (!command(2, 0, kR2, resp)) return false;
	memcpy(cid, resp, 16);
	for (int i = 0; i < 5; i++) {
		char c = (char)cid[11 - i];
		fProduct[i] = (c >= 0x20 && c < 0x7f) ? c : '?';
	}
	fProduct[5] = '\0';

	if (!command(3, 0, kR1 | kNoCheck, resp)) return false;
	fRCA = resp[0] >> 16;
	if (!command(9, fRCA << 16, kR2, resp)) return false;
	memcpy(csd, resp, 16);

	// the response words hold csd bits 127:8 shifted down by 8
	{
		const uint8_t *b = (const uint8_t *)csd;
		auto bits = [b](int hi, int lo) -> uint64_t {
			uint64_t v = 0;

			for (int i = hi; i >= lo; i--) v = v << 1 | ((b[(i - 8) / 8] >> ((i - 8) % 8)) & 1);
			return v;
		};

		if (bits(127, 126) == 1) {
			fBlockCount = (bits(69, 48) + 1) * 1024;
		} else {
			uint64_t bytes = (bits(73, 62) + 1) << (bits(49, 47) + 2 + bits(83, 80));

			fBlockCount = bytes / 512;
		}
	}
	IOLog("PDTegraSDMMC: sd ocr 0x%x rca 0x%x, %s addressing\n", ocr, fRCA, fByteAddr ? "byte" : "block");

	if (!command(7, fRCA << 16, kR1b, resp)) return false;
	if (fByteAddr && !command(16, 512, kR1, resp)) return false;

	blk0 = (uint8_t *)IOMalloc(512);
	if (blk0 == NULL) return false;
	ok = setClock(kTransferHz) && command(17, 0, kR1 | kData, resp, 1);
	if (ok) {
		memcpy(blk0, fBounce, 512);
		if (fBusWidth == 4 && !setSDBusWidth(4, blk0)) {
			IOLog("PDTegraSDMMC: 4-bit bus failed\n");
			fBusWidth = 1;
		}
		if (fBusWidth != 4) {
			fBusWidth = 1;
			ok = setSDBusWidth(1, blk0);
		}
	}
	IOFree(blk0, 512);
	return ok && fBlockCount != 0;
}

IOReturn
PDTegraSDMMC::write(UInt64 block, UInt64 nblks, IOMemoryDescriptor *buffer)
{
	IOReturn ret = kIOReturnSuccess;
	UInt64 done = 0;
	uint32_t resp[4];

	if (!fWritable) return kIOReturnNotWritable;
	if (fRegs == NULL || buffer == NULL) return kIOReturnNoDevice;
	if (block + nblks > fBlockCount) return kIOReturnBadArgument;

	IOLockLock(fLock);
	while (done < nblks && ret == kIOReturnSuccess) {
		UInt32 n = (UInt32)min(nblks - done, (UInt64)kMaxBlocksPerCmd);
		uint32_t idx = n > 1 ? 25 : 24;
		IOByteCount bytes = (IOByteCount)n * 512;

		if (buffer->readBytes(done * 512, fBounce, bytes) != bytes) {
			ret = kIOReturnIOError;
		} else if (!command(idx, cardAddr(block + done), kR1 | kData | kDataWrite, resp, n) &&
		    !command(idx, cardAddr(block + done), kR1 | kData | kDataWrite, resp, n)) {
			IOLog("PDTegraSDMMC: write failed at block %llu\n", block + done);
			ret = kIOReturnIOError;
		}
		done += n;
	}
	IOLockUnlock(fLock);
	return ret;
}

IOReturn
PDTegraSDMMC::read(UInt64 block, UInt64 nblks, IOMemoryDescriptor *buffer)
{
	IOReturn ret = kIOReturnSuccess;
	UInt64 done = 0;
	uint32_t resp[4];

	if (fRegs == NULL || buffer == NULL) return kIOReturnNoDevice;
	if (block + nblks > fBlockCount) return kIOReturnBadArgument;

	IOLockLock(fLock);
	while (done < nblks && ret == kIOReturnSuccess) {
		UInt32 n = (UInt32)min(nblks - done, (UInt64)kMaxBlocksPerCmd);
		uint32_t idx = n > 1 ? 18 : 17;
		IOByteCount bytes = (IOByteCount)n * 512;

		// one retry after the line reset a failed command already did
		if (!command(idx, cardAddr(block + done), kR1 | kData, resp, n) &&
		    !command(idx, cardAddr(block + done), kR1 | kData, resp, n)) {
			IOLog("PDTegraSDMMC: read failed at block %llu\n", block + done);
			ret = kIOReturnIOError;
		} else if (buffer->writeBytes(done * 512, fBounce, bytes) != bytes) {
			ret = kIOReturnIOError;
		}
		done += n;
	}
	IOLockUnlock(fLock);
	return ret;
}
