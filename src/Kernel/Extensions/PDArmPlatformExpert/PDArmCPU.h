#ifndef _PUREDARWIN_PDARMCPU_H
#define _PUREDARWIN_PDARMCPU_H

#include <IOKit/IOCPU.h>

class PDArmCPU : public IOCPU {
	OSDeclareDefaultStructors(PDArmCPU);

private:
	IOCPUInterruptController *cpuIC;
	bool startCommonCompleted;
	unsigned int pdCpuNumber;	// Logical cpu number from xnu's topology
	uint32_t pdPhysId;		// MPIDR affinity, the PSCI target
	bool pdIsBoot;

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
	// Set before any CPU is created: sizes the shared interrupt controller
	static void setCPUCount(unsigned int count);
	// Register this processor with xnu. The boot CPU also finishes its setup
	bool startForCPU(unsigned int cpu, uint32_t phys_id, bool boot);
};

class PDArmCPUInterruptController : public IOCPUInterruptController {
	OSDeclareDefaultStructors(PDArmCPUInterruptController);

public:
	virtual IOReturn handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
};

#endif /* _PUREDARWIN_PDARMCPU_H */
