// SPDX-License-Identifier: MIT
// the tegra x1 xusb host: partitions, clocks, pads and the falcon, then the xhci goes to RavynXHCIPort

#ifndef _PD_TEGRA_XUSB_H_
#define _PD_TEGRA_XUSB_H_

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

// the nub RavynXHCIPort attaches to: the xhci window and its interrupt, nothing else
class PDTegraXHCINub : public IOService
{
	OSDeclareDefaultStructors(PDTegraXHCINub)
};

class PDTegraXUSB : public IOService
{
	OSDeclareDefaultStructors(PDTegraXUSB)

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free(void) APPLE_KEXT_OVERRIDE;

private:
	static void bringupThread(void *arg, wait_result_t wr);
	static void mailboxThread(void *arg, wait_result_t wr);

	bool readReg(IOService *provider);
	bool mapBlocks(void);
	bool powerPartitions(void);
	void setupPads(void);
	void vbusOn(void);
	void smmuCheck(void);
	bool loadFirmware(void);
	bool publishNub(void);
	void bringup(void);

	IOService *fProvider;
	uint64_t fRegPhys[4];
	uint64_t fRegSize[4];
	int fNumReg;

	IOMemoryMap *fCARMap;
	IOMemoryMap *fPadMap;
	IOMemoryMap *fFPCIMap;
	IOMemoryMap *fIPFSMap;
	IOMemoryMap *fFuseMap;
	IOMemoryMap *fGPIOMap;
	IOMemoryMap *fMCMap;
	volatile uint32_t *fCAR;
	volatile uint32_t *fPad;
	volatile uint32_t *fFPCI;
	volatile uint32_t *fIPFS;
	volatile uint32_t *fFuse;
	volatile uint32_t *fGPIO;
	volatile uint32_t *fMC;

	IOBufferMemoryDescriptor *fFirmware;
	PDTegraXHCINub *fNub;
	bool fMailboxRunning;
};

#endif
