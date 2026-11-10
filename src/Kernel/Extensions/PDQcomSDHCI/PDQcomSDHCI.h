// SPDX-License-Identifier: MIT
// msm8916 emmc controller (sdhc_1, qcom,sdhci-msm-v4), polled pio
#ifndef _PUREDARWIN_PDQCOMSDHCI_H
#define _PUREDARWIN_PDQCOMSDHCI_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>

// lk2nd leaves it in legacy mode with the card asleep: redo lk's sdhci_msm_init and identify from cmd0
// bus power changes wait on the qualcomm pwrctl handshake, polled and acked here

class PDQcomSDHCIDisk;

class PDQcomSDHCI : public IOService
{
	OSDeclareDefaultStructors(PDQcomSDHCI)

public:
	bool     start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     free(void) APPLE_KEXT_OVERRIDE;

	// backing for the block storage nub
	IOReturn readWrite(bool write, UInt64 block, UInt64 nblks,
	    IOMemoryDescriptor *buffer);
	UInt64   blockCount(void) const { return fBlockCount; }
	UInt32   blockSize(void) const  { return 512; }
	bool     isReadOnly(void) const { return false; }

private:
	IOMemoryMap      *fRegMap, *fCoreMap;
	volatile uint8_t *fRegs, *fCore;
	IOLock           *fLock;
	uint32_t         *fBounce;
	PDQcomSDHCIDisk  *fDisk;

	uint32_t fRCA;              // card address, already shifted for the arg
	uint64_t fBlockCount;
	uint64_t fWriteFrom;        // writes below this block are refused (pdemmcwritefrom=)
	uint8_t  fBusWidth;

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

	uint32_t core32(uint32_t off) const
	{
		return *(volatile uint32_t *)(fCore + off);
	}
	void setCore32(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fCore + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}

	bool     pwrctlService(void);
	bool     pwrctlWait(uint32_t ms);
	bool     reset(uint8_t mask);
	bool     bringUpController(void);
	bool     setClock(uint32_t hz);
	bool     waitInt(uint32_t bits, uint32_t timeoutMs);
	bool     waitInhibit(uint32_t bits);
	bool     waitReady(uint32_t timeoutMs);
	bool     command(uint32_t idx, uint32_t arg, uint32_t flags,
	    uint32_t *resp, uint32_t nblks = 0);
	bool     pio(bool write, uint32_t nblks);
	bool     switchExtCSD(uint8_t index, uint8_t value);
	bool     identifyCard(void);
	IOReturn transferBlocks(bool write, UInt64 block, UInt32 nblks,
	    IOMemoryDescriptor *buffer, UInt64 bufferOffset);
};

#endif
