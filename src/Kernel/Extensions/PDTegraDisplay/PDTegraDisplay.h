// SPDX-License-Identifier: MIT
// hdmi scanout on the tegra x1: display controller a or b driving sor1 in tmds mode

#ifndef _PUREDARWIN_PDTEGRADISPLAY_H
#define _PUREDARWIN_PDTEGRADISPLAY_H

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOFramebuffer.h>

struct pd_tegra_mode {
	uint32_t clock_khz;
	uint32_t hactive, hfp, hsw, hbp;
	uint32_t vactive, vfp, vsw, vbp;
	bool     hpos, vpos;
};

class PDTegraDisplay : public IOFramebuffer
{
	OSDeclareDefaultStructors(PDTegraDisplay)

public:
	IOService *probe(IOService *provider, SInt32 *score) override;
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;
	void free(void) override;

	IOReturn enableController(void) override;
	const char *getPixelFormats(void) override;
	IOItemCount getDisplayModeCount(void) override;
	IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) override;
	IOReturn getInformationForDisplayMode(IODisplayModeID mode, IODisplayModeInformation *info) override;
	UInt64 getPixelFormatsForDisplayMode(IODisplayModeID mode, IOIndex depth) override;
	IOReturn getPixelInformation(IODisplayModeID mode, IOIndex depth, IOPixelAperture aperture,
	    IOPixelInformation *info) override;
	IOReturn getCurrentDisplayMode(IODisplayModeID *mode, IOIndex *depth) override;
	IOReturn setDisplayMode(IODisplayModeID mode, IOIndex depth) override;
	IODeviceMemory *getApertureRange(IOPixelAperture aperture) override;
	IODeviceMemory *getVRAMRange(void) override;
	IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type, IOUserClient **handler) override;

	// for the presenter user client
	uint32_t width(void) const { return fWidth; }
	uint32_t height(void) const { return fHeight; }
	uint32_t pitch(void) const { return fPitch; }
	IOBufferMemoryDescriptor *flipBuffer(UInt32 i) const { return i < 2 ? fFlip[i] : NULL; }
	uint64_t flipPhys(UInt32 i) const { return i < 2 ? fFlipPhys[i] : 0; }
	void flip(int buffer, uint64_t *seq, uint64_t *ns);
	void presenterGone(void);

private:
	IOMemoryMap *fDCMap, *fSORMap, *fCARMap, *fAuxMap, *fI2C2Map, *fI2C4Map, *fMCMap, *fGPIOMap;
	volatile uint32_t *fDC, *fSOR, *fCAR, *fAux, *fI2C2, *fI2C4, *fMC, *fGPIO;

	IOBufferMemoryDescriptor *fBuf;
	IOMemoryMap *fFBMap;
	volatile uint32_t *fFB;
	uint64_t fFBPhys;
	uint32_t fWidth, fHeight, fPitch, fRefreshHz;

	// the presenter's flip pair, scanned out in place of fBuf while a compositor holds it
	IOBufferMemoryDescriptor *fFlip[2];
	uint64_t fFlipPhys[2];
	IOLock *fFlipLock;
	uint64_t fFlipSeq;

	uint32_t fHead;
	IORegistryEntry *fSORNode;
	bool fDTBigEndian;
	struct pd_tegra_mode fMode;
	uint8_t fEDID[128];
	bool fHaveEDID;

	uint32_t dc(uint32_t word) const { return fDC[word]; }
	void dcw(uint32_t word, uint32_t v) const
	{
		fDC[word] = v;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	uint32_t sor(uint32_t off) const { return fSOR[off / 4]; }
	void sorw(uint32_t off, uint32_t v) const
	{
		fSOR[off / 4] = v;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	uint32_t car(uint32_t off) const { return fCAR[off / 4]; }
	void carw(uint32_t off, uint32_t v) const
	{
		fCAR[off / 4] = v;
		__asm__ volatile ("dsb sy" ::: "memory");
	}

	bool mapBlocks(IOService *provider);
	void dumpState(const char *when);
	bool powerUp(void);
	bool i2cInit(volatile uint32_t *regs, uint32_t clkIdx, uint32_t srcOff, const char *name);
	void hdmi5V(void);
	void readEDID(void);
	void pickMode(void);
	bool setPixelClock(void);
	bool allocFramebuffer(void);
	IOBufferMemoryDescriptor *allocScanout(uint64_t *phys);
	void scanFrom(uint64_t phys, bool log);
	void fillPattern(void);
	void setupDC(void);
	void applyProd(void);
	bool setupSOR(void);
	void sorUpdate(uint32_t reg);

	uint32_t dtCell(OSData *d, unsigned int i) const;
	IORegistryEntry *dtChild(IORegistryEntry *parent, const char *name) const;
};

#endif
