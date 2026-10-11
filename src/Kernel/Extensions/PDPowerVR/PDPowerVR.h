#ifndef _PUREDARWIN_PDPOWERVR_H
#define _PUREDARWIN_PDPOWERVR_H

#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <kern/thread.h>

// the powervr bxm-4-64 on the allwinner a733 (orange pi zero 4). milestone one: power, clock
// and reset it, read the bvnc, load the mips firmware into its heap and wait for the boot handshake

struct pvr_device;

class PDPowerVR : public IOService
{
	OSDeclareDefaultStructors(PDPowerVR)

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;
	IOReturn message(UInt32 type, IOService *provider, void *argument = NULL) APPLE_KEXT_OVERRIDE;
	// the booted device for user clients, NULL until the firmware is up
	struct pvr_device *device(void) const { return fReady ? fDev : NULL; }

private:
	IOMemoryMap       *fRegMap;
	IOMemoryMap       *fCCUMap;
	IOMemoryMap       *fPCKMap;
	volatile uint8_t  *fRegs;
	volatile uint8_t  *fCCU;
	volatile uint8_t  *fPCK;
	uint64_t           fRegsPhys;
	uint32_t           fCoreClockHz;
	struct pvr_device *fDev;
	bool               fReady;
	bool               fQuiesced;
	IONotifier        *fRestartNotifier;
	IOLock            *fQuiesceLock;
	IOService         *fNub;
	IOWorkLoop        *fWorkLoop;
	IOFilterInterruptEventSource *fIntSource;

	uint32_t ccu(uint32_t off) const { return *(volatile uint32_t *)(fCCU + off); }
	void     ccuWrite(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fCCU + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}
	uint32_t pck(uint32_t off) const { return *(volatile uint32_t *)(fPCK + off); }
	void     pckWrite(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fPCK + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}

	void logPowerState(const char *when);
	bool powerDomainOn(uint32_t domain);
	bool enableGpuPll(void);
	bool powerUp(void);
	void powerDownGpu(void);
	void quiesce(const char *why);
	static IOReturn restartHandler(void *target, void *refCon, UInt32 messageType,
	    IOService *provider, void *messageArgument, vm_size_t argSize);
	void bringUp(void);
	bool attachInterrupt(void);
	bool fwInterruptFilter(IOFilterInterruptEventSource *src);
	void fwInterrupt(IOInterruptEventSource *src, int count);
	void exerciseKCCB(void);
	void exerciseVM(void);
	static void bringUpThread(void *arg, wait_result_t wr);
};

#endif
