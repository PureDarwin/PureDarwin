#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include "PDArmCPU.h"
#include "PDArmGIC.h"
#include "PDAppleAIC.h"

class PDArmPlatformExpert : public IODTPlatformExpert
{
	OSDeclareDefaultStructors(PDArmPlatformExpert)

private:
	PDArmCPU *bootCPU;

public:
	IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void processTopLevel(IORegistryEntry *root) APPLE_KEXT_OVERRIDE;
	const char *deleteList(void) APPLE_KEXT_OVERRIDE;
	const char *excludeList(void) APPLE_KEXT_OVERRIDE;
	bool getMachineName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
	bool getModelName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;

protected:
	virtual bool initPlatformInterrupts(void);
	virtual void initPlatformInterruptsLate(void);
	virtual const char *platformModelName(void);
};

#define super IODTPlatformExpert
OSDefineMetaClassAndStructors(PDArmPlatformExpert, IODTPlatformExpert);

IOService *
PDArmPlatformExpert::probe(IOService *provider, SInt32 *score)
{
	IOService *result = super::probe(provider, score);
	if (result != 0 && score != 0) *score = 10000;
	return result;
}

bool
PDArmPlatformExpert::start(IOService *provider)
{
	if (!super::start(provider)) return false;
	registerService();

	if (!initPlatformInterrupts()) return false;

	bootCPU = new PDArmCPU;
	if (bootCPU == NULL) return false;
	bootCPU->init();
	bootCPU->attach(0);
	if (!bootCPU->startCommon()) return false;

	initPlatformInterruptsLate();

	return true;
}

void
PDArmPlatformExpert::processTopLevel(IORegistryEntry *root)
{
	super::processTopLevel(root);
}

const char *
PDArmPlatformExpert::deleteList(void)
{
	return "('pd-delete-none')";
}

const char *
PDArmPlatformExpert::excludeList(void)
{
	return NULL;
}

bool
PDArmPlatformExpert::getMachineName(char *name, int maxLength)
{
	if (name == 0 || maxLength <= 0) return false;
	strlcpy(name, "arm64", (size_t)maxLength);
	return true;
}

bool
PDArmPlatformExpert::getModelName(char *name, int maxLength)
{
	if (name == 0 || maxLength <= 0) return false;
	strlcpy(name, platformModelName(), (size_t)maxLength);
	return true;
}

bool
PDArmPlatformExpert::initPlatformInterrupts(void)
{
	PDArmGIC_init();
	return true;
}

void
PDArmPlatformExpert::initPlatformInterruptsLate(void)
{
}

const char *
PDArmPlatformExpert::platformModelName(void)
{
	return "QEMU Virtual ARM64";
}

/*
 * Apple SoCs. The device tree root of every one of them carries "AppleARM" in
 * its compatible property, alongside the board and product names, so a single
 * personality covers the family.
 */
class PDAppleARMPlatformExpert : public PDArmPlatformExpert
{
	OSDeclareDefaultStructors(PDAppleARMPlatformExpert)

protected:
	bool initPlatformInterrupts(void) APPLE_KEXT_OVERRIDE;
	void initPlatformInterruptsLate(void) APPLE_KEXT_OVERRIDE;
	const char *platformModelName(void) APPLE_KEXT_OVERRIDE;
};

#undef super
#define super PDArmPlatformExpert
OSDefineMetaClassAndStructors(PDAppleARMPlatformExpert, PDArmPlatformExpert);

bool
PDAppleARMPlatformExpert::initPlatformInterrupts(void)
{
	/* The AIC attaches to the CPU's interrupt controller; see the late hook. */
	return true;
}

void
PDAppleARMPlatformExpert::initPlatformInterruptsLate(void)
{
	if (!PDAppleAIC_init()) {
		IOLog("PDAppleARMPlatformExpert: no interrupt controller; "
		    "device interrupts will not be delivered\n");
	}
}

const char *
PDAppleARMPlatformExpert::platformModelName(void)
{
	return "Apple ARM";
}
