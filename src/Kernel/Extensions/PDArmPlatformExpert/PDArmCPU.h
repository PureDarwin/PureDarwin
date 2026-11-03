#ifndef _PUREDARWIN_PDARMCPU_H
#define _PUREDARWIN_PDARMCPU_H

#include <IOKit/IOCPU.h>

class PDArmCPU : public IOCPU {
	OSDeclareDefaultStructors(PDArmCPU);

private:
	IOCPUInterruptController *cpuIC;
	bool startCommonCompleted;
	uint32_t fMPIDR;

public:
	virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void initCPU(bool boot) APPLE_KEXT_OVERRIDE;
	virtual void quiesceCPU(void) APPLE_KEXT_OVERRIDE;
	virtual kern_return_t startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr) APPLE_KEXT_OVERRIDE;
	virtual void haltCPU(void) APPLE_KEXT_OVERRIDE;
	virtual const OSSymbol *getCPUName(void) APPLE_KEXT_OVERRIDE;
	virtual void signalCPU(IOCPU *target) APPLE_KEXT_OVERRIDE;
	bool startCommon(void);
#if defined(__arm__) && !defined(__arm64__)
	// generic a7 smp: every core registered with mach and started through its reset vector
	enum StartMethod { kStartNone, kStartPSCI, kStartRockchipMailbox };
	static unsigned startSMP(StartMethod method);
	void handleIPI(void);
#endif
};

class PDArmCPUInterruptController : public IOCPUInterruptController {
	OSDeclareDefaultStructors(PDArmCPUInterruptController);

public:
	virtual IOReturn handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
};

#endif /* _PUREDARWIN_PDARMCPU_H */
