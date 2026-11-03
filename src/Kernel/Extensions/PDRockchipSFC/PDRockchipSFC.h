#ifndef _PUREDARWIN_PDROCKCHIPSFC_H
#define _PUREDARWIN_PDROCKCHIPSFC_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLocks.h>

/*
 * Rockchip FSPI (serial flash controller, RV1106 TRM chapter 18) driving a SPI NAND
 * (Winbond W25N01GV command set) in polled indirect mode, one data line. Exposes one
 * partition of the flash as a block device, pages mapped past bad blocks.
 */
class PDRockchipSFCDisk;

class PDRockchipSFC : public IOService
{
	OSDeclareDefaultStructors(PDRockchipSFC);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;

	IOReturn readPages(UInt64 page, UInt64 count, IOMemoryDescriptor *buffer);
	IOReturn readPage(UInt64 page, uint8_t *buf);
	// Erase one logical block and program all of its pages from data (pagesPerBlock pages)
	IOReturn writeBlock(UInt64 block, const uint8_t *data);
	UInt32 pagesPerBlock(void) const { return fPagesPerBlock; }
	UInt64 pageCount(void) const { return (UInt64)fGoodBlocks * fPagesPerBlock; }
	UInt32 pageSize(void) const { return fPageSize; }

private:
	uint32_t reg(uint32_t off) const;
	void setReg(uint32_t off, uint32_t v);
	bool waitIdle(void);
	void recover(void);
	void drain(void);

	// One indirect command: opcode, address of abits bits (0 = none), dummy cycles, then
	// len bytes read into in or written from out
	IOReturn command(uint8_t op, uint32_t abits, uint32_t addr, uint32_t dummy,
	    uint8_t *in, const uint8_t *out, uint32_t len);
	IOReturn getFeature(uint8_t addr, uint8_t *value);
	IOReturn setFeature(uint8_t addr, uint8_t value);
	IOReturn waitReady(uint8_t *status);
	IOReturn loadPage(uint32_t row);
	IOReturn readCache(uint16_t column, uint8_t *buf, uint32_t len);
	IOReturn writeEnable(void);
	IOReturn programPage(uint32_t row, const uint8_t *data);
	IOReturn eraseBlock(uint32_t row);
	bool blockIsBad(uint32_t block);

	IOMemoryDescriptor *fRegsMD;
	IOMemoryMap *fRegsMap;
	volatile uint8_t *fRegs;
	IOLock *fLock;
	PDRockchipSFCDisk *fDisk;
	uint8_t *fPageBuf;

	uint32_t fPageSize, fPagesPerBlock, fFirstBlock, fBlocks;
	uint32_t fGoodBlocks;
	uint32_t *fBlockMap;	// logical block -> physical block within the partition
};

#endif /* _PUREDARWIN_PDROCKCHIPSFC_H */
