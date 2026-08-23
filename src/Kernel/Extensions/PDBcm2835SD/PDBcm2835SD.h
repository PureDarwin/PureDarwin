#ifndef _PUREDARWIN_PDBCM2835SD_H
#define _PUREDARWIN_PDBCM2835SD_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>

/*
 * The BCM2835 EMMC block is an Arasan SD/MMC host controller. It is close
 * enough to the SDHCI standard register layout to be driven as one, with the
 * documented deviation that its registers may only be accessed 32 bits at a
 * time - byte and halfword accesses that a stock SDHCI driver would use for
 * BLKSIZECNT or CONTROL0 do not work here.
 *
 * Transfers use the PIO data FIFO and everything is polled through the
 * INTERRUPT register. The datasheet warns that STATUS changes with latency
 * across clock domains and is unsafe to poll, whereas INTERRUPT implements a
 * handshake that cannot be missed; polling also keeps this driver independent
 * of interrupt routing, which matters because the root filesystem has to be
 * readable before much else works.
 */

class PDBcm2835SDDisk;

class PDBcm2835SD : public IOService
{
	OSDeclareDefaultStructors(PDBcm2835SD)

public:
	bool     start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     free(void) APPLE_KEXT_OVERRIDE;

	/* Backing for the IOBlockStorageDevice nub. */
	IOReturn readWrite(bool write, UInt64 block, UInt64 nblks,
	    IOMemoryDescriptor *buffer);
	UInt64   blockCount(void) const { return fBlockCount; }
	UInt32   blockSize(void) const  { return 512; }
	bool     isReadOnly(void) const { return fReadOnly; }

	/* Used by the bcm2835_emmc_bringup ops table. */
	uint32_t bringupRead(int blk, uint32_t off) const;
	void     bringupWrite(int blk, uint32_t off, uint32_t val) const;

private:
	/* SoC peripheral base: 0x20000000 on BCM2835, 0x3f000000 on BCM2837. */
	uint32_t          fPeriphBase;
	IOMemoryMap      *fRegMap;
	volatile uint8_t *fRegs;
	volatile uint8_t *fGPIORegs;
	volatile uint8_t *fCPRMANRegs;
	IOMemoryMap      *fGPIOMap;
	IOMemoryMap      *fCPRMANMap;
	uint32_t          fBaseClockHz;
	IOLock           *fLock;
	PDBcm2835SDDisk  *fDisk;

	uint32_t fRCA;              /* card address, already shifted for the arg */
	uint64_t fBlockCount;
	bool     fBlockAddressed;   /* SDHC/SDXC address in blocks, not bytes */
	bool     fReadOnly;

	uint32_t read32(uint32_t off) const
	{
		return *(volatile uint32_t *)(fRegs + off);
	}
	void write32(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fRegs + off) = val;
	}

	bool     bringUpController(void);
	volatile uint8_t *blockBase(int blk) const;
	bool     setClock(uint32_t targetHz);
	bool     waitForInterrupt(uint32_t mask, uint32_t timeoutUs);
	bool     sendCommand(uint32_t cmd, uint32_t arg, uint32_t *resp);
	bool     sendAppCommand(uint32_t cmd, uint32_t arg, uint32_t *resp);
	bool     identifyCard(void);
	bool     decodeCSD(const uint32_t *resp);
	IOReturn transferBlocks(bool write, UInt64 block, UInt32 nblks,
	    IOMemoryDescriptor *buffer, UInt64 bufferOffset);
};

#endif /* _PUREDARWIN_PDBCM2835SD_H */
