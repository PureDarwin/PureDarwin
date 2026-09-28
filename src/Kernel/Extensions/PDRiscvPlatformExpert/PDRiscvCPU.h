#ifndef _PUREDARWIN_PDRISCVCPU_H
#define _PUREDARWIN_PDRISCVCPU_H

#include <IOKit/IOCPU.h>

class PDRiscvCPU : public IOCPU {
	OSDeclareDefaultStructors(PDRiscvCPU);

private:
	IOCPUInterruptController *cpuIC;
	const OSSymbol *pdName;
	bool startCommonCompleted;
	bool pdInterruptInstalled;
	unsigned int pdCpuNumber;	// logical cpu number from xnu's topology
	uint32_t pdHartId;		// the hart id, what sbi hsm and the plic contexts go by
	bool pdIsBoot;

public:
	virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	virtual void free(void) APPLE_KEXT_OVERRIDE;
	virtual void initCPU(bool boot) APPLE_KEXT_OVERRIDE;
	virtual void quiesceCPU(void) APPLE_KEXT_OVERRIDE;
	virtual kern_return_t startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr) APPLE_KEXT_OVERRIDE;
	virtual void haltCPU(void) APPLE_KEXT_OVERRIDE;
	virtual const OSSymbol *getCPUName(void) APPLE_KEXT_OVERRIDE;
	bool startCommon(void);
	// set before any cpu is created, it sizes the shared interrupt controller
	static void setCPUCount(unsigned int count);
	// register this processor with xnu, the boot cpu also finishes its setup
	bool startForCPU(unsigned int cpu, uint32_t hart, bool boot);
};

class PDRiscvCPUInterruptController : public IOCPUInterruptController {
	OSDeclareDefaultStructors(PDRiscvCPUInterruptController);

public:
	virtual IOReturn handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
};

#endif /* _PUREDARWIN_PDRISCVCPU_H */
