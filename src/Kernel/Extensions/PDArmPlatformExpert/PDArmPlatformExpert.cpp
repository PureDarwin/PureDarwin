#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include "PDArmCPU.h"
#include "PDArmGIC.h"
#include "PDAppleAIC.h"
#include "PDBcm2835IC.h"
#include "PDSun50iUSB.h"
#include "PDSun50iWatchdog.h"
#include "PDSg2002.h"

class PDArmPlatformExpert : public IODTPlatformExpert
{
	OSDeclareDefaultStructors(PDArmPlatformExpert)

private:
	PDArmCPU *bootCPU;
	IOService *fEMMCNub;
	IOService *fFramebufferNub;
	IOMemoryMap *fRTCMap;
	volatile uint32_t *fRTC;
	thread_call_t fRTCPublish;

public:
	IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void processTopLevel(IORegistryEntry *root) APPLE_KEXT_OVERRIDE;
	const char *deleteList(void) APPLE_KEXT_OVERRIDE;
	const char *excludeList(void) APPLE_KEXT_OVERRIDE;
	bool getMachineName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
	bool getModelName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
	long getGMTTimeOfDay(void) APPLE_KEXT_OVERRIDE;
	void setGMTTimeOfDay(long secs) APPLE_KEXT_OVERRIDE;

protected:
	virtual bool initPlatformInterrupts(void);
	virtual void initPlatformInterruptsLate(void);
	virtual const char *platformModelName(void);
	void publishBcm2835EMMC(void);
	void publishBcm283xFramebuffer(void);
	void startQemuRTC(void);
	static void publishRTC(thread_call_param_t self, thread_call_param_t);
};

static bool pd_platform_is_bcm283x(void);

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

	// One IOCPU per processor in xnu's topology.
	// The boot CPU finishes its setup here, the others wait for PE_cpu_start() to power them on
	{
		const ml_topology_info_t *topo = ml_get_topology_info();
		unsigned int ncpu = (topo != NULL) ? topo->num_cpus : 1;

		PDArmCPU::setCPUCount(ncpu);

		for (unsigned int i = 0; i < ncpu; i++) {
			PDArmCPU *cpu = new PDArmCPU;
			uint32_t phys_id = (topo != NULL) ? topo->cpus[i].phys_id : 0;

			if (cpu == NULL) return false;
			cpu->init();
			cpu->attach(0);
			if (!cpu->startForCPU(i, phys_id, i == 0)) return false;
			if (i == 0) bootCPU = cpu;
		}
		IOLog("PDArmPlatformExpert: %u processor(s)\n", ncpu);
	}
	if (bootCPU == NULL) return false;

	initPlatformInterruptsLate();

#if defined(__arm__) && !defined(__arm64__)
	publishBcm2835EMMC();
#endif

	publishBcm283xFramebuffer();
	startQemuRTC();

#if defined(__arm64__)
	if (PDSun50i_isPlatform()) {
		PDSun50iWatchdog_start();
		PDSun50iMMC_publish(this);
		if (PDSun50iUSB_init())
			PDSun50iUSB_publish(this);
	} else if (PDSg2002_isPlatform()) {
		PDSg2002Watchdog_start();
		PDSg2002_publish(this);
	}
#endif

	return true;
}

// QEMU virt's PL031 at 0x09010000 counts seconds since 1970 from the host clock. Without an IORTC
// the calendar starts at 1970 and IOKitInitializeTime waits 30 s for one
#define PD_QEMU_PL031_PHYS 0x09010000ULL
#define PD_PL031_DR 0       // data: current seconds
#define PD_PL031_LR 2       // load: set seconds

void
PDArmPlatformExpert::startQemuRTC(void)
{
	IORegistryEntry *armio = IORegistryEntry::fromPath("/arm-io", gIODTPlane);
	OSData *type = armio != NULL ? OSDynamicCast(OSData, armio->getProperty("device_type")) : NULL;
	IOMemoryDescriptor *md;

	if (type == NULL || strncmp((const char *)type->getBytesNoCopy(), "qemuvirt-io", type->getLength()) != 0) {
		OSSafeReleaseNULL(armio);
		return;
	}
	OSSafeReleaseNULL(armio);
	md = IOMemoryDescriptor::withPhysicalAddress(PD_QEMU_PL031_PHYS, 0x1000, kIODirectionInOut);
	if (md == NULL) {
		return;
	}
	fRTCMap = md->map(kIOMapInhibitCache);
	md->release();
	if (fRTCMap == NULL) {
		return;
	}
	fRTC = (volatile uint32_t *)fRTCMap->getVirtualAddress();
	// Root mount relied on the 30 s IOKitInitializeTime spent waiting for this: APFS still holds the
	// container a few seconds after IOKit goes quiet, so IORTC is published 10 s later
	{
		uint64_t deadline;

		fRTCPublish = thread_call_allocate(&PDArmPlatformExpert::publishRTC, this);
		if (fRTCPublish == NULL) {
			publishResource("IORTC", this);
			return;
		}
		clock_interval_to_deadline(10, kSecondScale, &deadline);
		thread_call_enter_delayed(fRTCPublish, deadline);
	}
}

void
PDArmPlatformExpert::publishRTC(thread_call_param_t self, thread_call_param_t)
{
	PDArmPlatformExpert *pe = (PDArmPlatformExpert *)self;

	pe->publishResource("IORTC", pe);
}

long
PDArmPlatformExpert::getGMTTimeOfDay(void)
{
	return fRTC != NULL ? (long)fRTC[PD_PL031_DR] : 0;
}

void
PDArmPlatformExpert::setGMTTimeOfDay(long secs)
{
	if (fRTC != NULL) {
		fRTC[PD_PL031_LR] = (uint32_t)secs;
	}
}

/*
 * Firmware framebuffers are described by boot_args rather than an
 * architecture-specific device-tree node. Publish a matching nub whenever the
 * loader supplied valid geometry. This covers both the Pi framebuffer and
 * QEMU's GOP-backed ramfb.
 */
void
PDArmPlatformExpert::publishBcm283xFramebuffer(void)
{
	PE_Video console;
	IOService *nub = NULL;

	if (fFramebufferNub != NULL) return;

	if (getConsoleInfo(&console) != kIOReturnSuccess ||
	    console.v_baseAddr == 0 || console.v_width == 0 || console.v_height == 0) {
		IOLog("PDArmPlatformExpert: no boot framebuffer to publish\n");
		return;
	}

	nub = OSTypeAlloc(IOPlatformDevice);
	if (nub == NULL) goto fail;
	if (!nub->init()) goto fail;
	nub->setName("framebuffer");
	if (!nub->attach(this)) goto fail;

	fFramebufferNub = nub;
	nub->registerService();
	IOLog("PDArmPlatformExpert: published framebuffer nub, %ux%u\n",
	    (unsigned)console.v_width, (unsigned)console.v_height);
	return;

fail:
	IOLog("PDArmPlatformExpert: could not publish the framebuffer nub\n");
	OSSafeReleaseNULL(nub);
}

#if defined(__arm__) && !defined(__arm64__)
// The Pi's bootloader device tree describes only gpio and uart, so nothing ever
// provides for the SD host controller. Publish the nub ourselves; PDBcm2835SD
// hardcodes the register address and needs the provider only to match against.
//
// Service plane only: matching uses IOProviderClass/IONameMatch, and a nub in
// the device-tree plane would be fed to IODTPlatformExpert::getNubResources()
// to have a "reg" property resolved that this one does not have.
void
PDArmPlatformExpert::publishBcm2835EMMC(void)
{
	if (fEMMCNub != NULL) return;

	IOService *nub = OSTypeAlloc(IOPlatformDevice);
	if (nub == NULL) goto fail;

	if (!nub->init()) goto fail;
	nub->setName("emmc");
	if (!nub->attach(this)) goto fail;

	fEMMCNub = nub;
	nub->registerService();
	IOLog("PDArmPlatformExpert: published emmc nub for the SD host\n");
	return;

fail:
	IOLog("PDArmPlatformExpert: could not publish the emmc nub\n");
	OSSafeReleaseNULL(nub);
}
#endif

static bool pd_platform_is_bcm283x(void);

void
PDArmPlatformExpert::processTopLevel(IORegistryEntry *root)
{
	super::processTopLevel(root);
	// QEMU virt stands in for Apple's virtual platform: MobileGestalt's hardware model (and
	// mobileactivationd's hactivation) comes from the root target-type, as on a VMA2MACOS guest
	if (root != NULL && !pd_platform_is_bcm283x() && root->getProperty("target-type") == NULL) {
		OSData *tt = OSData::withBytes("VMA2MACOS", sizeof("VMA2MACOS"));

		if (tt != NULL) {
			root->setProperty("target-type", tt);
			tt->release();
		}
	}
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

/*
 * True when the device tree describes a Broadcom BCM283x board. The 64-bit
 * build serves both QEMU virt (GICv3) and the Raspberry Pi (legacy ARMCTRL),
 * so the interrupt controller has to be chosen at runtime: writing
 * ICC_SRE_EL1 on a Pi is an undefined instruction and panics the kernel.
 */
static bool
pd_platform_is_bcm283x(void)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	bool isBcm = false;

	if (root == NULL) return false;

	OSData *compat = OSDynamicCast(OSData, root->getProperty("compatible"));
	if (compat != NULL) {
		const char *str = (const char *)compat->getBytesNoCopy();
		unsigned len = compat->getLength();

		/* "compatible" is a list of NUL-separated strings. */
		for (unsigned i = 0; str != NULL && i < len; ) {
			const char *entry = str + i;
			unsigned remain = len - i;

			if (strncmp(entry, "raspberrypi", 11) == 0 ||
			    strncmp(entry, "bcm2837", 7) == 0 ||
			    strncmp(entry, "bcm2835", 7) == 0) {
				isBcm = true;
				break;
			}
			unsigned n = 0;
			while (n < remain && entry[n] != '\0') n++;
			i += n + 1;
		}
	}
	root->release();
	return isBcm;
}

bool
PDArmPlatformExpert::initPlatformInterrupts(void)
{
	/* BCM2835 has the legacy ARMCTRL controller; the QEMU GIC mapping is
	 * invalid on the Pi Zero. Mask every source now, before PDArmCPU installs
	 * the CPU interrupt handler: ARMCTRL has no end-of-interrupt, so anything
	 * the booter left enabled would re-enter forever once delivery starts. */
	if (pd_platform_is_bcm283x()) {
		if (!PDBcm2835IC_maskAll()) {
			IOLog("PDArmPlatformExpert: could not reach the BCM2835 interrupt "
			    "controller\n");
			return false;
		}
		return true;
	}
	PDArmGIC_init();
	return true;
}

void
PDArmPlatformExpert::initPlatformInterruptsLate(void)
{
	/* The CPU interrupt controller exists by now, so the ARMCTRL controller
	 * can attach to it and start dispatching. */
	if (pd_platform_is_bcm283x()) {
		if (!PDBcm2835IC_init()) {
			IOLog("PDArmPlatformExpert: BCM2835 interrupt controller unavailable; "
			    "device interrupts will not be delivered\n");
		}
	}
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
