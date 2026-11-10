// SPDX-License-Identifier: MIT
// tegra210 sdmmc (sdhci 3.0) driver for the jetson tx1 emmc and sd slot, read-only unless pdtegrammcrw=1

#ifndef _PUREDARWIN_PDTEGRASDMMC_H
#define _PUREDARWIN_PDTEGRASDMMC_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>

// polled pio through the buffer data port, sdmmc4 (emmc) and sdmmc1 (sd card)

class PDTegraSDMMCDisk;

class PDTegraSDMMC : public IOService
{
	OSDeclareDefaultStructors(PDTegraSDMMC)

public:
	bool     start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     free(void) APPLE_KEXT_OVERRIDE;

	// backing for the block storage nub
	IOReturn read(UInt64 block, UInt64 nblks, IOMemoryDescriptor *buffer);
	IOReturn write(UInt64 block, UInt64 nblks, IOMemoryDescriptor *buffer);
	bool     writable(void) const { return fWritable; }
	UInt64   blockCount(void) const { return fBlockCount; }
	UInt32   blockSize(void) const  { return 512; }
	const char *product(void) const { return fProduct; }
	bool     isSD(void) const { return fSD; }

private:
	IOMemoryMap      *fRegMap;
	IOMemoryMap      *fCARMap;
	volatile uint8_t *fRegs;
	volatile uint8_t *fCAR;
	IOLock           *fLock;
	uint32_t         *fBounce;
	PDTegraSDMMCDisk *fDisk;
	uint64_t          fBlockCount;
	uint32_t          fBusWidth;
	bool              fWritable;
	bool              fSD;
	bool              fByteAddr;
	uint32_t          fRCA;
	uint32_t          fCARBit;
	uint32_t          fCARSrc;
	char              fProduct[8];

	uint32_t rd(uint32_t off) const
	{
		return *(volatile uint32_t *)(fRegs + off);
	}
	void wr(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fRegs + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	// the controller takes 32-bit accesses only, narrower fields go through a read-modify-write
	void rmw(uint32_t off, uint32_t clear, uint32_t set) const
	{
		wr(off, (rd(off) & ~clear) | set);
	}

	bool     mapRegisters(IOService *provider);
	bool     bringUpController(void);
	bool     setClock(uint32_t hz);
	bool     waitStatus(uint32_t bits, uint32_t timeoutMs, uint32_t *got);
	void     resetLines(uint32_t lines);
	bool     command(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp,
	    uint32_t nblks = 0);
	bool     identifyCard(void);
	bool     readExtCSD(uint8_t *ext);
	bool     setBusWidth(uint32_t width, const uint8_t *ref);
	bool     identifySD(void);
	// sdsc cards take byte addresses, everything else sector numbers
	uint32_t cardAddr(UInt64 block) const { return (uint32_t)(fByteAddr ? block * 512 : block); }
	bool     appCommand(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp);
	bool     setSDBusWidth(uint32_t width, const uint8_t *ref);
};

#endif
