#ifndef _PUREDARWIN_PDSG2002SD_H
#define _PUREDARWIN_PDSG2002SD_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>

// sophgo sg2002 sd0, a designware mshc speaking plain sdhci, the licheerv nano's micro sd slot
// polled pio through the buffer data port, sd cards only

class PDSg2002SDDisk;

class PDSg2002SD : public IOService
{
	OSDeclareDefaultStructors(PDSg2002SD)

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
	volatile uint8_t *fRegs;
	IOLock           *fLock;
	uint32_t         *fBounce;
	PDSg2002SDDisk   *fDisk;

	uint32_t fRCA;              // card address, already shifted for the arg
	uint64_t fBlockCount;
	bool     fBlockAddressed;   // sdhc and sdxc address in blocks
	bool     fReadOnly;

	uint32_t rd32(uint32_t off) const
	{
		return *(volatile uint32_t *)(fRegs + off);
	}
	uint16_t rd16(uint32_t off) const
	{
		return *(volatile uint16_t *)(fRegs + off);
	}
	uint8_t rd8(uint32_t off) const
	{
		return *(volatile uint8_t *)(fRegs + off);
	}
	void wr32(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fRegs + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	void wr16(uint32_t off, uint16_t val) const
	{
		*(volatile uint16_t *)(fRegs + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	void wr8(uint32_t off, uint8_t val) const
	{
		*(volatile uint8_t *)(fRegs + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}

	bool     reset(uint8_t mask);
	void     setPhy(void);
	bool     bringUpController(void);
	bool     setClock(uint32_t hz);
	bool     waitInt(uint32_t bits, uint32_t timeoutMs);
	bool     waitInhibit(uint32_t bits);
	bool     command(uint32_t idx, uint32_t arg, uint32_t flags,
	    uint32_t *resp, uint32_t nblks = 0);
	bool     appCommand(uint32_t idx, uint32_t arg, uint32_t flags,
	    uint32_t *resp);
	bool     pio(bool write, uint32_t nblks);
	bool     identifyCard(void);
	bool     decodeCSD(const uint32_t *csd);
	IOReturn transferBlocks(bool write, UInt64 block, UInt32 nblks,
	    IOMemoryDescriptor *buffer, UInt64 bufferOffset);
};

#endif
