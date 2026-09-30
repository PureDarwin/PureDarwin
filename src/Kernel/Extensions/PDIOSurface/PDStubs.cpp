// small placeholder services: AppleARMIODevice, Bluetooth HCI and the M2 scaler drivers

#include "PDIOSurface.h"

OSDefineMetaClassAndStructors(AppleARMIODevice, IOService);
OSDefineMetaClassAndStructors(AppleM2ScalerParavirtDriver, IOService);
OSDefineMetaClassAndStructors(AppleM2ScalerCSCDriver, IOService);
OSDefineMetaClassAndStructors(IOBluetoothHCIController, IOService);

bool
IOBluetoothHCIController::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	setName("IOBluetoothHCIController");
	setProperty("BluetoothTransportConnected", kOSBooleanTrue);
	setProperty("Built-In", kOSBooleanTrue);
	registerService();
	return true;
}

IOReturn
IOBluetoothHCIController::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

// _detectDisplays walks IODeviceTree:/arm-io for device_type display children and reads compatible
bool
AppleARMIODevice::compareName(OSString *name, OSString **matched) const
{
	return IODTCompareNubName(this, name, matched);
}

void
AppleARMIODevice::publishGPU(IOService *owner)
{
	static const char kCompat[] = "paravirtualizedgraphics,gpu";
	IORegistryEntry *armio = IORegistryEntry::fromPath("IODeviceTree:/arm-io");
	AppleARMIODevice *nub = OSTypeAlloc(AppleARMIODevice);
	OSData *compat = OSData::withBytes(kCompat, sizeof(kCompat));

	if (nub != NULL && compat != NULL && nub->init()) {
		nub->setName("gpu0");
		nub->setProperty("compatible", compat);
		if (nub->attach(owner)) {
			if (armio != NULL) {
				nub->attachToParent(armio, gIODTPlane);
			}
			nub->registerService();
		}
	}
	OSSafeReleaseNULL(armio);
	OSSafeReleaseNULL(compat);
	OSSafeReleaseNULL(nub);
}


#define PD_SCALER_IMPL(Name) \
bool \
Name::start(IOService *provider) \
{ \
	if (!IOService::start(provider)) { \
		return false; \
	} \
	setName(#Name); \
	{ \
		OSDictionary *caps = OSDictionary::withCapacity(4); \
		OSNumber *one = OSNumber::withNumber(1ULL, 32); \
		OSNumber *big = OSNumber::withNumber(16384ULL, 32); \
		if (caps != NULL && one != NULL && big != NULL) { \
			caps->setObject("SupportsScaling", kOSBooleanTrue); \
			caps->setObject("SupportsColorConversion", kOSBooleanTrue); \
			caps->setObject("MaxWidth", big); \
			caps->setObject("MaxHeight", big); \
			caps->setObject("Version", one); \
			setProperty("IOSurfaceAcceleratorCapabilitiesDict", caps); \
		} \
		OSSafeReleaseNULL(caps); \
		OSSafeReleaseNULL(one); \
		OSSafeReleaseNULL(big); \
	} \
	registerService(); \
	return true; \
} \
IOReturn \
Name::newUserClient(task_t owningTask, void *securityID, UInt32 type, IOUserClient **handler) \
{ \
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler); \
}
PD_SCALER_IMPL(AppleM2ScalerParavirtDriver)
PD_SCALER_IMPL(AppleM2ScalerCSCDriver)
