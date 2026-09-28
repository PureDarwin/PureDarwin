#ifndef _PUREDARWIN_PDSUN50IMMC_H
#define _PUREDARWIN_PDSUN50IMMC_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>

// allwinner h616/h618 smhc0, the micro sd slot on the orange pi zero 3
// polled pio through the fifo, sd cards only

class PDSun50iMMCDisk;

class PDSun50iMMC : public IOService
{
	OSDeclareDefaultStructors(PDSun50iMMC)

public:
	bool     start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     free(void) APPLE_KEXT_OVERRIDE;

	// backing for the block storage nub
	IOReturn readWrite(bool write, UInt64 block, UInt64 nblks,
	    IOMemoryDescriptor *buffer);
	UInt64   blockCount(void) const { return fBlockCount; }
	UInt32   blockSize(void) const  { return 512; }
	bool     isReadOnly(void) const { return fReadOnly; }

private:
	IOMemoryMap      *fRegMap;
	IOMemoryMap      *fCCUMap;
	IOMemoryMap      *fPIOMap;
	volatile uint8_t *fRegs;
	volatile uint8_t *fCCU;
	volatile uint8_t *fPIO;
	IOLock           *fLock;
	uint32_t         *fBounce;
	PDSun50iMMCDisk  *fDisk;

	uint32_t fRCA;              // card address, already shifted for the arg
	uint64_t fBlockCount;
	bool     fBlockAddressed;   // sdhc and sdxc address in blocks
	bool     fReadOnly;

	uint32_t rd(uint32_t off) const
	{
		return *(volatile uint32_t *)(fRegs + off);
	}
	void wr(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fRegs + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}

	bool     bringUpController(void);
	bool     updateClock(void);
	bool     setClock(uint32_t hz);
	bool     waitRint(uint32_t done, uint32_t timeoutMs);
	bool     command(uint32_t idx, uint32_t arg, uint32_t flags,
	    uint32_t *resp, uint32_t nblks = 0);
	bool     appCommand(uint32_t idx, uint32_t arg, uint32_t flags,
	    uint32_t *resp);
	bool     pio(bool write, uint32_t words);
	void     recover(void);
	bool     identifyCard(void);
	bool     decodeCSD(const uint32_t *csd);
	IOReturn transferBlocks(bool write, UInt64 block, UInt32 nblks,
	    IOMemoryDescriptor *buffer, UInt64 bufferOffset);
};

#endif
