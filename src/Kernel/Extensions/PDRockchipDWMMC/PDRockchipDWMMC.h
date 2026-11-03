#ifndef _PUREDARWIN_PDROCKCHIPDWMMC_H
#define _PUREDARWIN_PDROCKCHIPDWMMC_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLocks.h>

/*
 * RV1106 SDMMC host (TRM part 1, chapter 22; a DesignWare mobile storage host) driving
 * an SD card in polled FIFO mode. Clocks, resets and pins come from VI_CRU and GPIO3_IOC.
 */
class PDRockchipDWMMCDisk;

class PDRockchipDWMMC : public IOService
{
	OSDeclareDefaultStructors(PDRockchipDWMMC);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;

	IOReturn readWrite(bool write, UInt64 block, UInt64 count, IOMemoryDescriptor *buffer);
	UInt64 blockCount(void) const { return fPartBlocks; }
	bool isReadOnly(void) const { return !fWritable; }

private:
	static void initThread(void *arg, wait_result_t wr);
	bool initCard(void);
	void setupClocksAndPins(void);
	bool resetController(void);
	bool updateClock(uint32_t div);
	IOReturn command(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp);
	IOReturn appCommand(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t *resp);
	IOReturn transfer(bool write, uint32_t block, uint32_t count, uint8_t *buf);
	bool waitNotBusy(void);

	uint32_t reg(uint32_t off) const { return *(volatile uint32_t *)(fRegs + off); }
	void setReg(uint32_t off, uint32_t v) { *(volatile uint32_t *)(fRegs + off) = v; }

	IOMemoryDescriptor *fRegsMD;
	IOMemoryMap *fRegsMap;
	volatile uint8_t *fRegs;
	IOLock *fLock;
	PDRockchipDWMMCDisk *fDisk;
	uint8_t *fBounce;

	uint32_t fRCA;
	bool fHighCapacity, fWritable;
	UInt64 fBlocks;
	UInt64 fPartStart, fPartBlocks;	// the window exposed as the disk (pdsdpart), else the card
};

#endif /* _PUREDARWIN_PDROCKCHIPDWMMC_H */
