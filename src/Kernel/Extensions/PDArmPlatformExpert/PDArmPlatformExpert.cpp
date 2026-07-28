#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include "PDArmCPU.h"
#include "PDArmGIC.h"

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

	PDArmGIC_init();

	bootCPU = new PDArmCPU;
	if (bootCPU == NULL) return false;
	bootCPU->init();
	bootCPU->attach(0);
	if (!bootCPU->startCommon()) return false;

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
	strlcpy(name, "QEMU Virtual ARM64", (size_t)maxLength);
	return true;
}
