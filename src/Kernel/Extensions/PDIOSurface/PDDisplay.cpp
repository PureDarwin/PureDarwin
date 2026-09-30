// paravirtual display services: display nub, framebuffer, mobile framebuffer, CLCD and their helpers

#include "PDIOSurface.h"

OSDefineMetaClassAndStructors(AppleCLCD, IOService);
OSDefineMetaClassAndStructors(AppleDisplayManager, IOService);
OSDefineMetaClassAndStructors(DCPAVCECInterfaceProxy, IOService);
OSDefineMetaClassAndStructors(AppleParavirtDisplay, IOService);
OSDefineMetaClassAndStructors(AppleParavirtFramebuffer, IOService);

static void
pd_publish_display_geometry(IOService *service)
{
	PE_Video console;

	if (service->getPlatform()->getConsoleInfo(&console) != kIOReturnSuccess) {
		return;
	}
	service->setProperty("width", console.v_width, 32);
	service->setProperty("height", console.v_height, 32);
	service->setProperty("depth", console.v_depth, 32);
	service->setProperty("rowBytes", console.v_rowBytes, 32);
	service->setProperty("IOFBMemorySize", console.v_rowBytes * console.v_height, 64);
}

bool
AppleParavirtDisplay::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	setName("AppleParavirtDisplay");
	pd_publish_display_geometry(this);
	setProperty("display-id", 1ULL, 32);
	registerService();
	return true;
}

IOReturn
AppleParavirtDisplay::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

bool
AppleParavirtFramebuffer::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	setName("AppleParavirtFramebuffer");
	pd_publish_display_geometry(this);
	registerService();
	return true;
}

IOReturn
AppleParavirtFramebuffer::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

bool
DCPAVCECInterfaceProxy::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	setName("DCPAVCECInterfaceProxy");
	registerService();
	return true;
}

IOReturn
DCPAVCECInterfaceProxy::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

OSDefineMetaClassAndStructors(IOPortTransportStateDisplayPort, IOService);

// 800x600@60 EDID 1.4 identifying as an Apple panel:
// PnP vendor APP (0x0610), product 0xA040, "Color LCD". Checksum included
static const uint8_t kEDID[128] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x06, 0x10, 0x40, 0xA0,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x24, 0x01, 0x04, 0xA5, 0x1A, 0x14, 0x78,
	0x0A, 0x78, 0xF2, 0xB3, 0xA5, 0x55, 0x49, 0x9B, 0x26, 0x0D, 0x47, 0x01,
	0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xA0, 0x0F, 0x20, 0x00, 0x31, 0x58,
	0x1C, 0x20, 0x28, 0x80, 0x14, 0x00, 0x1A, 0x00, 0x14, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xFC, 0x00, 0x43, 0x6F, 0x6C, 0x6F, 0x72, 0x20, 0x4C,
	0x43, 0x44, 0x0A, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x32,
	0x4B, 0x1E, 0x50, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0xC9,
};

bool
AppleDisplayManager::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	setName("AppleDisplayManager");
	registerService();
	return true;
}

IOReturn
AppleDisplayManager::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

bool
IOPortTransportStateDisplayPort::start(IOService *provider)
{
	OSData *edid = OSData::withBytes(kEDID, sizeof(kEDID));

	if (!IOService::start(provider)) {
		return false;
	}
	setName("IOPortTransportStateDisplayPort");
	if (edid != NULL) {
		setProperty("EDID", edid);
		edid->release();
	}
	// Identify as an Apple display: the standard IOGraphics keys plus the
	// attribute dictionary the display stack reads a panel's identity from
	{
		OSData *copy = OSData::withBytes(kEDID, sizeof(kEDID));

		if (copy != NULL) {
			setProperty("IODisplayEDID", copy);
			copy->release();
		}
	}
	setProperty("DisplayVendorID", 0x610ULL, 32);
	setProperty("DisplayProductID", 0xA040ULL, 32);
	setProperty("DisplaySerialNumber", 0ULL, 32);
	{
		OSDictionary *product = OSDictionary::withCapacity(6);
		OSDictionary *attrs = OSDictionary::withCapacity(1);
		OSNumber *vendor = OSNumber::withNumber(0x610ULL, 32);
		OSNumber *pid = OSNumber::withNumber(0xA040ULL, 32);
		OSNumber *year = OSNumber::withNumber(2026ULL, 32);
		OSString *name = OSString::withCString("Color LCD");

		if (product != NULL && attrs != NULL && vendor != NULL && pid != NULL &&
		    year != NULL && name != NULL) {
			product->setObject("ManufacturerID", vendor);
			product->setObject("ProductID", pid);
			product->setObject("YearOfManufacture", year);
			product->setObject("ProductName", name);
			attrs->setObject("ProductAttributes", product);
			setProperty("DisplayAttributes", attrs);
		}
		OSSafeReleaseNULL(product);
		OSSafeReleaseNULL(attrs);
		OSSafeReleaseNULL(vendor);
		OSSafeReleaseNULL(pid);
		OSSafeReleaseNULL(year);
		OSSafeReleaseNULL(name);
	}

	// The notification matches on IOPropertyMatch {EDIDChanged: true}
	setProperty("EDIDChanged", kOSBooleanTrue);
	// The attributes the display stack reads off a DisplayPort transport:
	// one sink on port 0, four lanes at HBR2, tunneling off
	setProperty("Index", 0ULL, 32);
	setProperty("Role", 1ULL, 32);
	setProperty("SinkCount", 1ULL, 32);
	setProperty("LaneCount", 4ULL, 32);
	setProperty("MaxLaneCount", 4ULL, 32);
	setProperty("LinkRate", 0x14ULL, 32);
	setProperty("ParentPortNumber", 0ULL, 32);
	setProperty("ParentPortType", 0ULL, 32);
	setProperty("Tunneled", kOSBooleanFalse);
	{
		OSDictionary *metadata = OSDictionary::withCapacity(1);

		if (metadata != NULL) {
			setProperty("Metadata", metadata);
			metadata->release();
		}
	}
	registerService();
	// The stack read our attributes but never the EDID.
	// A real port announces a display by changing state, so re-announce once the system is up
	fHotplug = thread_call_allocate(&IOPortTransportStateDisplayPort::hotplugFired, this);
	if (fHotplug != NULL) {
		uint64_t deadline;

		clock_interval_to_deadline(600, NSEC_PER_SEC, &deadline);  // After WindowServer is up
		thread_call_enter_delayed(fHotplug, deadline);
	}
	return true;
}

void
IOPortTransportStateDisplayPort::hotplugFired(thread_call_param_t self,
    thread_call_param_t unused)
{
	IOPortTransportStateDisplayPort *me = (IOPortTransportStateDisplayPort *)self;

	me->setProperty("EDIDChanged", kOSBooleanTrue);
	me->messageClients(kIOMessageServicePropertyChange);
	me->registerService();
}

IOReturn
IOPortTransportStateDisplayPort::newUserClient(task_t owningTask, void *securityID,
    UInt32 type, IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

OSDefineMetaClassAndStructors(PDDisplayNub, IOService);
OSDefineMetaClassAndStructors(IOMobileFramebuffer, IOService);

bool
IOMobileFramebuffer::start(IOService *provider)
{
	PE_Video console;

	if (!IOService::start(provider)) {
		return false;
	}
	setName("IOMobileFramebuffer");
	if (getPlatform()->getConsoleInfo(&console) == kIOReturnSuccess) {
		setProperty("width", console.v_width, 32);
		setProperty("height", console.v_height, 32);
		setProperty("depth", console.v_depth, 32);
		setProperty("rowBytes", console.v_rowBytes, 32);
	}
	// WindowServer reads these after every swap. Nothing ever underruns here
	setProperty("underrun", 0ULL, 64);
	setProperty("GPBandwidth", 0ULL, 64);
	setProperty("decomp_fail", 0ULL, 64);
	setProperty("downscale_fail", 0ULL, 64);
	// The property set a real AppleParavirtDisplay carries (a macOS 15 VM's registry), sized to
	// this panel. WindowServer reads TimingElements, ColorElements and DisplayAttributes
	{
		static const char *const plist =
		    "<plist version=\"1.0\"><dict>"
		    "<key>external</key><true/>"
		    "<key>CursorPlane</key><integer>1</integer>"
		    "<key>DPTimingModeId</key><integer>1000</integer>"
		    "<key>DisplayWidth</key><integer>800</integer>"
		    "<key>DisplayHeight</key><integer>600</integer>"
		    "<key>IOMFBUUID</key><string>5044494F-5355-5246-4143-450000000001</string>"
		    "<key>ParavirtDisplayPrefs</key><dict><key>scaleFactor</key><integer>1</integer><key>configEpoch</key><integer>0</integer></dict>"
		    "<key>Transport</key><dict><key>Upstream</key><string>Other</string><key>Downstream</key><string>Other</string></dict>"
		    "<key>IOMFBScalingLimits</key><dict><key>RGBLayer_MaxScale</key><integer>4</integer><key>RGBLayer_MinScaleFraction</key><integer>4</integer></dict>"
		    "<key>IOMFBMaxSrcPixels</key><dict><key>MaxSrcBufferWidth</key><integer>16384</integer><key>MaxSrcRectWidth</key><integer>16384</integer>"
		    "<key>MaxSrcRectTotal</key><integer>0</integer><key>IOMFBMaxCompressedSizeInBytes</key><integer>0</integer><key>MaxSrcBufferHeight</key><integer>16384</integer></dict>"
		    "<key>DisplayAttributes</key><dict>"
		    "<key>NativeFormatHorizontalPixels</key><integer>800</integer><key>NativeFormatVerticalPixels</key><integer>600</integer>"
		    "<key>Chromaticity</key><dict><key>Red</key><dict><key>X</key><integer>41943</integer><key>Y</key><integer>21627</integer><key>Gamma</key><integer>65536</integer></dict>"
		    "<key>Green</key><dict><key>X</key><integer>19661</integer><key>Y</key><integer>39322</integer><key>Gamma</key><integer>65536</integer></dict>"
		    "<key>Blue</key><dict><key>X</key><integer>9830</integer><key>Y</key><integer>3932</integer><key>Gamma</key><integer>65536</integer></dict></dict>"
		    "<key>DefaultWhitePoint</key><dict><key>X</key><integer>20495</integer><key>Y</key><integer>21563</integer><key>Gamma</key><integer>65536</integer></dict>"
		    "<key>SupportsStandby</key><true/><key>SupportsPQEOTF</key><false/><key>DefaultColorSpaceIsSRGB</key><false/>"
		    "<key>MaxHorizontalImageSize</key><integer>66</integer><key>MaxVerticalImageSize</key><integer>42</integer>"
		    "<key>PreciseAspectRatio</key><integer>87381</integer><key>ContinuousFrequencySupport</key><integer>3</integer>"
		    "<key>ProductAttributes</key><dict><key>ManufacturerID</key><string>APP</string><key>SerialNumber</key><integer>305419896</integer>"
		    "<key>WeekOfManufacture</key><integer>24</integer><key>YearOfManufacture</key><integer>2021</integer><key>ProductName</key><string>Apple Virtual</string><key>ProductID</key><integer>0</integer></dict>"
		    "</dict>"
		    "<key>ColorElements</key><array><dict><key>StandardType</key><integer>2</integer><key>Score</key><integer>100</integer><key>IsVirtual</key><false/>"
		    "<key>ID</key><integer>100</integer><key>PixelEncoding</key><integer>0</integer><key>ElementType</key><integer>1</integer><key>SupportsDSC</key><false/>"
		    "<key>EOTF</key><integer>0</integer><key>Colorimetry</key><integer>16</integer><key>DynamicRange</key><integer>0</integer><key>Depth</key><integer>8</integer>"
		    "<key>ElementData</key><data>CAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAA=</data></dict></array>"
		    "<key>TimingElements</key><array><dict><key>StandardType</key><integer>2</integer><key>ElementType</key><integer>0</integer>"
		    "<key>IsSplit</key><false/><key>IsPromoted</key><false/><key>IsInterlaced</key><false/><key>IsOverscanned</key><false/><key>IsVirtual</key><false/><key>IsPreferred</key><false/>"
		    "<key>HorizontalAttributes</key><dict><key>Total</key><integer>0</integer><key>Active</key><integer>800</integer><key>PixelRepetition</key><integer>0</integer>"
		    "<key>PreciseSyncRate</key><integer>278134784</integer><key>BackPorch</key><integer>0</integer><key>SyncWidth</key><integer>0</integer><key>FrontPorch</key><integer>0</integer>"
		    "<key>SyncPolarity</key><integer>0</integer><key>SyncRate</key><integer>278134784</integer></dict>"
		    "<key>VerticalAttributes</key><dict><key>Total</key><integer>0</integer><key>Active</key><integer>600</integer><key>PixelRepetition</key><integer>0</integer>"
		    "<key>PreciseSyncRate</key><integer>3932160</integer><key>BackPorch</key><integer>0</integer><key>SyncWidth</key><integer>0</integer><key>FrontPorch</key><integer>0</integer>"
		    "<key>SyncPolarity</key><integer>0</integer><key>SyncRate</key><integer>3932160</integer></dict>"
		    "<key>UnsafeColorElementIDs</key><array/><key>DSCRequiredColorElementIDs</key><array/><key>Score</key><integer>1000</integer>"
		    "<key>ColorModes</key><array><dict><key>StandardType</key><integer>2</integer><key>Score</key><integer>100</integer><key>IsVirtual</key><false/>"
		    "<key>ID</key><integer>100</integer><key>PixelEncoding</key><integer>0</integer><key>ElementType</key><integer>1</integer><key>SupportsDSC</key><false/>"
		    "<key>EOTF</key><integer>0</integer><key>Colorimetry</key><integer>16</integer><key>DynamicRange</key><integer>0</integer><key>Depth</key><integer>8</integer>"
		    "<key>ElementData</key><data>CAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAA=</data></dict></array>"
		    "<key>ScanInformation</key><integer>0</integer><key>TimingType</key><integer>3</integer><key>TimingStandard</key><integer>4</integer>"
		    "<key>ID</key><integer>1000</integer><key>AspectRatio</key><integer>13</integer><key>ValidPixelEncodings</key><integer>8189</integer>"
		    "<key>PreciseAspectRatio</key><integer>87381</integer></dict></array>"
		    "</dict></plist>";
		OSObject *obj = OSUnserializeXML(plist, NULL);
		OSDictionary *d = OSDynamicCast(OSDictionary, obj);

		if (d != NULL) {
			OSCollectionIterator *it = OSCollectionIterator::withCollection(d);
			OSSymbol *k;

			while (it != NULL && (k = (OSSymbol *)it->getNextObject()) != NULL) {
				setProperty(k, d->getObject(k));
			}
			OSSafeReleaseNULL(it);
		} else {
			IOLog("PDIOSurface: IOMobileFramebuffer property plist did not parse\n");
		}
		OSSafeReleaseNULL(obj);
	}
	registerService();
	return true;
}

IOReturn
IOMobileFramebuffer::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

void
PDDisplayNub::publish(IOService *owner)
{
	publishOne(owner, "disp0", "disp0,t8103", "edp");
	publishOne(owner, "dispext0", "dispext0,t8103", "hdmi");
}

void
PDDisplayNub::publishOne(IOService *owner, const char *name, const char *compatible,
    const char *displayType)
{
	static const char kType[] = "display";
	IORegistryEntry *armio = IORegistryEntry::fromPath("IODeviceTree:/arm-io");
	PDDisplayNub *nub = OSTypeAlloc(PDDisplayNub);
	OSData *type = OSData::withBytes(kType, sizeof(kType));
	OSData *compat = OSData::withBytes(compatible, (unsigned)strlen(compatible) + 1);
	OSData *dtype = OSData::withBytes(displayType, (unsigned)strlen(displayType) + 1);

	if (nub == NULL || type == NULL || compat == NULL || dtype == NULL || !nub->init()) {
		goto out;
	}
	nub->setName(name);
	// Device-tree properties are data: QuartzCore reads them with -getBytes:range:
	nub->setProperty("device_type", type);
	nub->setProperty("compatible", compat);
	nub->setProperty("display-type", dtype);
	// WindowServer reads the canvas size from the display node it paired with the framebuffer
	{
		uint32_t w = 800, h = 600;
		OSData *dw = OSData::withBytes(&w, sizeof(w));
		OSData *dh = OSData::withBytes(&h, sizeof(h));

		if (dw != NULL && dh != NULL) {
			nub->setProperty("canvas-width", dw);
			nub->setProperty("canvas-height", dh);
		}
		OSSafeReleaseNULL(dw);
		OSSafeReleaseNULL(dh);
	}
	if (!nub->attach(owner)) {
		goto out;
	}
	if (armio != NULL) {
		nub->attachToParent(armio, gIODTPlane);
	}
	nub->registerService();
	nub = NULL;

out:
	OSSafeReleaseNULL(armio);
	OSSafeReleaseNULL(type);
	OSSafeReleaseNULL(compat);
	OSSafeReleaseNULL(dtype);
	OSSafeReleaseNULL(nub);
}

IOReturn
PDDisplayNub::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

bool
AppleCLCD::start(IOService *provider)
{
	PE_Video console;

	if (!IOService::start(provider)) {
		return false;
	}
	setName("AppleCLCD");
	// The only property WindowServer read off this service before walking away
	setProperty("APTDevice", kOSBooleanTrue);
	if (getPlatform()->getConsoleInfo(&console) == kIOReturnSuccess) {
		setProperty("IOFBWidth", console.v_width, 32);
		setProperty("IOFBHeight", console.v_height, 32);
		setProperty("IOFBDepth", console.v_depth, 32);
		setProperty("IOFBRowBytes", console.v_rowBytes, 32);
	}
	registerService();
	return true;
}

IOReturn
AppleCLCD::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}
