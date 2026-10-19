// SPDX-License-Identifier: MIT
// the iokit face of a drm-kmod gpu driver: it owns the IOPCIDevice and runs
// the linux driver's probe and attach through the fbsdkpi layer

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <pexpert/pexpert.h>

#include "pd_fbsd_os.h"
#include "pd_fbsd_attach.h"
#include "pd_fbsd_xnu.h"
#include "pd_drm_fb.h"

#define PDDRM_STR2(x)	#x
#define PDDRM_STR(x)	PDDRM_STR2(x)

// key bring-up lines go to serial too, the console moves to the screen partway through boot
// with debug=0x8 kprintf alone reaches serial, screen and log, IOLog too would print twice
static bool
pd_pddrm_log_kprintf(void)
{
	int debug = 0;
	return PE_parse_boot_argn("debug", &debug, sizeof(debug)) && (debug & 0x8) != 0;
}
#define PDDRM_LOG(fmt, ...) do { if (pd_pddrm_log_kprintf()) kprintf(fmt, ##__VA_ARGS__); else IOLog(fmt, ##__VA_ARGS__); } while (0)

class PDFBSD_CLASS : public IOService
{
	OSDeclareDefaultStructors(PDFBSD_CLASS);

public:
#if defined(PDDRM_OPT_IN_BOOTARG) || defined(PDDRM_OPT_OUT_BOOTARG)
	IOService *probe(IOService *provider, SInt32 *score) override;
#endif
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;
	void systemWillShutdown(IOOptionBits specifier) override;

private:
	IOPCIDevice *fPCI;
	void *fDevice;
};

OSDefineMetaClassAndStructors(PDFBSD_CLASS, IOService);

static IOLock *sAttachLock;

// an unclean stop leaves the PSP ring behind and the next create fails, so destroy rings
// through MP0 C2PMSG_64 first. False when the PSP does not answer, then the gpu is left alone.
static bool
pd_amd_psp_ready(IOPCIDevice *pci)
{
	const IOByteCount mp0 = 0x16000 * 4;
	const IOByteCount msg64 = mp0 + 0x80 * 4, msg81 = mp0 + 0x91 * 4;
	IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress5,
		kIOMapInhibitCache);
	if (map == NULL || map->getLength() < msg81 + 4) {
		OSSafeReleaseNULL(map);
		return false;
	}
	volatile uint32_t *c64 = (volatile uint32_t *)(map->getVirtualAddress() + msg64);
	volatile uint32_t *c81 = (volatile uint32_t *)(map->getVirtualAddress() + msg81);
	uint32_t sos = *c81, before = *c64;
	bool ok = false;

	// the secure os reports its version in C2PMSG_81 once it runs
	if (sos != 0 && sos != 0xffffffff) {
		*c64 = 0x30000;
		for (int i = 0; i < 200 && (*c64 & 0x80000000) == 0; i++)
			IOSleep(10);
		ok = (*c64 & 0x80000000) != 0;
	}
	IOLog(PDDRM_STR(PDFBSD_CLASS) ": psp sos 0x%08x, destroy rings C2PMSG_64 0x%08x -> 0x%08x, %s\n",
		sos, before, *c64, ok ? "ready" : "NOT ANSWERING, leaving the gpu alone");
	map->release();
	return ok;
}

#if defined(PDDRM_OPT_IN_BOOTARG) || defined(PDDRM_OPT_OUT_BOOTARG)
// an opt-in driver stays out of the way unless its boot-arg asks for it, an opt-out one
// attaches unless its boot-arg is 0
#ifdef PDDRM_OPT_IN_BOOTARG
#define PDDRM_GATE_BOOTARG PDDRM_OPT_IN_BOOTARG
#define PDDRM_GATE_DEFAULT 0
#else
#define PDDRM_GATE_BOOTARG PDDRM_OPT_OUT_BOOTARG
#define PDDRM_GATE_DEFAULT 1
#endif
IOService *
PDFBSD_CLASS::probe(IOService *provider, SInt32 *score)
{
	int on = PDDRM_GATE_DEFAULT;
	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);

	PE_parse_boot_argn(PDDRM_GATE_BOOTARG, &on, sizeof(on));
	PDDRM_LOG(PDDRM_STR(PDFBSD_CLASS) ": probing %04x:%04x, %s=%d\n",
		pci != NULL ? pci->configRead16(kIOPCIConfigVendorID) : 0,
		pci != NULL ? pci->configRead16(kIOPCIConfigDeviceID) : 0, PDDRM_GATE_BOOTARG, on);
	if (on == 0) {
		PDDRM_LOG(PDDRM_STR(PDFBSD_CLASS) ": not attaching, %s=%d\n", PDDRM_GATE_BOOTARG, on);
		return NULL;
	}
	return IOService::probe(provider, score);
}
#endif

#ifdef PDDRM_INTEL_STOLEN
extern "C" void pd_i915_set_stolen(uint64_t base, uint64_t size);

// the gpu's stolen memory as the bios set it up, i915 reads it from the resource it gets
static void
pd_intel_stolen(IOPCIDevice *pci)
{
	uint64_t base = pci->configRead32(0x5c) & 0xfff00000u, size;
	uint16_t ggc = pci->configRead16(0x50);
	uint32_t gms;

	// gen11 and later keep the base in the 64 bit bdsm
	if (base == 0)
		base = ((uint64_t)pci->configRead32(0xc4) << 32) | (pci->configRead32(0xc0) & 0xfff00000u);
	// gen6 and gen7 (device ids below 0x1000): gms in ggc bits 7:3, 32 MB units.
	// gen8 and later: bits 15:8, 32 MB units below 0xf0 and 4 MB units from there
	if (pci->configRead16(kIOPCIConfigDeviceID) < 0x1000) {
		gms = (ggc >> 3) & 0x1f;
		size = (uint64_t)gms << 25;
	} else {
		gms = (ggc >> 8) & 0xff;
		size = gms < 0xf0 ? (uint64_t)gms << 25 : (uint64_t)(gms - 0xf0 + 1) << 22;
	}
	// a passed-through igd in a vm (qemu's upt mode) has no stolen memory behind it,
	// i915 then runs without stolen objects
	if (base == 0)
		size = 0;
	PDDRM_LOG(PDDRM_STR(PDFBSD_CLASS) ": stolen memory 0x%llx size 0x%llx (gms 0x%x)\n",
		base, size, gms);
	pd_i915_set_stolen(base, size);
}
#endif

bool
PDFBSD_CLASS::start(IOService *provider)
{
	if (!IOService::start(provider))
		return false;
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL)
		return false;
	fPCI->retain();

	// on the power plane, so the system shutdown reaches systemWillShutdown
	static IOPMPowerState states[2] = {
		{ kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ kIOPMPowerStateVersion1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn,
		  0, 0, 0, 0, 0, 0, 0, 0 },
	};
	PMinit();
	provider->joinPMtree(this);
	registerPowerDriver(this, states, 2);

	if (!fPCI->open(this)) {
		PDDRM_LOG(PDDRM_STR(PDFBSD_CLASS) ": cannot open the pci device\n");
		return false;
	}
	fPCI->setBusMasterEnable(true);
	fPCI->setMemoryEnable(true);
	fPCI->setIOEnable(true);

	if (fPCI->configRead16(kIOPCIConfigVendorID) == 0x1002 && !pd_amd_psp_ready(fPCI)) {
		fPCI->close(this);
		return false;
	}

#ifdef PDDRM_INTEL_STOLEN
	if (fPCI->configRead16(kIOPCIConfigVendorID) == 0x8086)
		pd_intel_stolen(fPCI);
#endif

	pdos_xnu_init();
	if (sAttachLock == NULL)
		sAttachLock = IOLockAlloc();

	PDDRM_LOG(PDDRM_STR(PDFBSD_CLASS) ": attaching the linux driver\n");
	IOLockLock(sAttachLock);
	fDevice = pd_fbsd_attach_pci_device(fPCI);
	IOLockUnlock(sAttachLock);
	if (fDevice == NULL) {
		PDDRM_LOG(PDDRM_STR(PDFBSD_CLASS) ": the linux driver did not attach\n");
		fPCI->close(this);
		return false;
	}
	PDDRM_LOG(PDDRM_STR(PDFBSD_CLASS) ": attached\n");
	registerService();
	return true;
}

// the linux driver's shutdown leaves the gpu firmware ready for the next boot
void
PDFBSD_CLASS::systemWillShutdown(IOOptionBits specifier)
{
	if (fDevice != NULL) {
		// the scanout buffer goes before the driver puts the gpu to rest
		if (pd_drm_fb != NULL)
			pd_drm_fb->release();
		IOLog(PDDRM_STR(PDFBSD_CLASS) ": shutting the gpu down\n");
		pd_fbsd_shutdown_device(fDevice);
		IOLog(PDDRM_STR(PDFBSD_CLASS) ": gpu shut down\n");
	}
	IOService::systemWillShutdown(specifier);
}

void
PDFBSD_CLASS::stop(IOService *provider)
{
	// detaching a gpu driver is not supported yet, the device stays up
	if (fPCI != NULL) {
		fPCI->close(this);
		OSSafeReleaseNULL(fPCI);
	}
	IOService::stop(provider);
}
