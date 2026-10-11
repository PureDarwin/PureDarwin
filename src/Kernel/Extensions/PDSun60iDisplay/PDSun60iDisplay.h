#ifndef _PUREDARWIN_PDSUN60IDISPLAY_H
#define _PUREDARWIN_PDSUN60IDISPLAY_H

#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <kern/thread.h>

#include "pd_display.h"

// scanout on the allwinner a733 display engine: the firmware (vendor u-boot) has the mixer, tv tcon and hdmi running, so this only takes over layer 0's buffer address.
// flips are written from the tcon's vblank interrupt, so a buffer changes only between frames

class PDSun60iDisplay : public IOService
{
	OSDeclareDefaultStructors(PDSun60iDisplay)

public:
	IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;
	IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
	    void *param1, void *param2, void *param3, void *param4) APPLE_KEXT_OVERRIDE;

private:
	IOMemoryMap       *fTconMap;
	IOMemoryMap       *fDEMap;
	volatile uint8_t  *fTcon;
	volatile uint8_t  *fDE;
	IOService         *fNub;
	IOWorkLoop        *fWorkLoop;
	IOFilterInterruptEventSource *fIntSource;
	IOLock            *fLock;
	IOLock            *fFlipLock;

	uint32_t           fWidth, fHeight, fPitch, fFormat;
	uint64_t           fBootAddr;
	uint32_t           fTotalLines, fRefreshMHz;

	// written by flip(), consumed by the interrupt filter at the next vblank
	volatile uint64_t  fPendingAddr;
	volatile uint32_t  fPendingPitch;
	volatile bool      fPending;
	volatile uint64_t  fVblanks;
	volatile uint64_t  fVblankTime;
	volatile uint32_t  fVblankLine;
	volatile uint32_t  fFirstFlags;
	volatile uint64_t  fStormStart;
	volatile uint32_t  fStormCount;
	volatile bool      fIrqOK;

	uint32_t tcon(uint32_t off) const { return *(volatile uint32_t *)(fTcon + off); }
	void     tconWrite(uint32_t off, uint32_t v) const
	{
		*(volatile uint32_t *)(fTcon + off) = v;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	uint32_t de(uint32_t off) const { return *(volatile uint32_t *)(fDE + off); }
	void     deWrite(uint32_t off, uint32_t v) const
	{
		*(volatile uint32_t *)(fDE + off) = v;
		__asm__ volatile ("dsb sy" ::: "memory");
	}

	uint32_t line(void) const;
	void     latch(uint64_t addr, uint32_t pitch);
	void     measure(void);
	bool     attachInterrupt(void);
	bool     vblankFilter(IOFilterInterruptEventSource *src);
	void     vblankAction(IOInterruptEventSource *src, int count);
	IOReturn flip(uint64_t addr, uint32_t pitch, struct pd_display_flip *out);
	IOReturn pollFlip(uint64_t addr, uint32_t pitch);
	void     fillInfo(struct pd_display_info *info);
	static void irqThread(void *arg, wait_result_t wr);
};

#endif
