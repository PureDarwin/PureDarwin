#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include "PDArmCPU.h"
#include "PDSoftRTC.h"
#include <sys/sysctl.h>
#include "PDArmGIC.h"
#include "PDAppleAIC.h"
#include "PDBcm2835IC.h"

// the rockchip watchdogs, and the crus whose global reset registers sit at the same offsets
#define PD_RV1106_WDT_BASE	0xff5a0000
#define PD_RV1106_CRU_BASE	0xff3b0000
#define PD_RK3506_WDT_BASE	0xff260000	// wdt0
#define PD_RK3506_CRU_BASE	0xff9a0000

class PDArmPlatformExpert : public IODTPlatformExpert
{
	OSDeclareDefaultStructors(PDArmPlatformExpert)

private:
	PDArmCPU *bootCPU;
	IOService *fEMMCNub;
	IOService *fFramebufferNub;
	IOMemoryMap *fWdtMap;
	IOMemoryMap *fCruMap;
	IOTimerEventSource *fWdtTimer;
	PDSoftRTC *fRTC;

public:
	IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void processTopLevel(IORegistryEntry *root) APPLE_KEXT_OVERRIDE;
	const char *deleteList(void) APPLE_KEXT_OVERRIDE;
	const char *excludeList(void) APPLE_KEXT_OVERRIDE;
	bool getMachineName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
	bool getModelName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
	int haltRestart(unsigned int type) APPLE_KEXT_OVERRIDE;
	long getGMTTimeOfDay(void) APPLE_KEXT_OVERRIDE;
	void setGMTTimeOfDay(long secs) APPLE_KEXT_OVERRIDE;

protected:
	virtual bool initPlatformInterrupts(void);
	virtual void initPlatformInterruptsLate(void);
	virtual const char *platformModelName(void);
	void publishBcm2835EMMC(void);
	bool boardIs(const char *compatible);
	void publishNub(const char *name);
	void publishBcm283xFramebuffer(void);
	void startWatchdogKick(uint32_t wdtBase, uint32_t cruBase);
	void startTemperature(void);
	static void kickWatchdog(OSObject *owner, IOTimerEventSource *sender);
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
	// None of these boards keeps time: the calendar starts from a software clock
	fRTC = OSTypeAlloc(PDSoftRTC);
	if (fRTC != NULL && (!fRTC->init() || !fRTC->attach(this) || !fRTC->start(this))) {
		OSSafeReleaseNULL(fRTC);
	}
	registerService();

	if (!initPlatformInterrupts()) return false;

#if defined(__arm__) && !defined(__arm64__)
	// qemu's virt a7 starts cores through psci, the rk3506 through its boot rom's mailbox;
	// pdsmp=0 keeps the rk3506 on one core
	if (boardIs("puredarwin,virt-a7") || boardIs("puredarwin,rk3506")) {
		uint32_t smp = 1;
		PDArmCPU::StartMethod method = PDArmCPU::kStartPSCI;

		if (boardIs("puredarwin,rk3506")) {
			PE_parse_boot_argn("pdsmp", &smp, sizeof(smp));
			method = smp != 0 ? PDArmCPU::kStartRockchipMailbox : PDArmCPU::kStartNone;
		}
		if (PDArmCPU::startSMP(method) == 0) {
			return false;
		}
	} else
#endif
	{
		bootCPU = new PDArmCPU;
		if (bootCPU == NULL) return false;
		bootCPU->init();
		bootCPU->attach(0);
		if (!bootCPU->startCommon()) return false;
	}

	initPlatformInterruptsLate();

#if defined(__arm__) && !defined(__arm64__)
	// The RV1103 boots from SPI NAND behind the Rockchip FSPI, the Pi Zero from its SD host;
	// the RV1103 OTG port also runs as a USB serial console
	if (boardIs("puredarwin,rv1103")) {
		publishNub("rockchip-sfc");
		publishNub("rockchip-dwmmc");
		publishNub("rockchip-dwc3");
		startWatchdogKick(PD_RV1106_WDT_BASE, PD_RV1106_CRU_BASE);
		startTemperature();
	} else if (boardIs("puredarwin,rk3506")) {
		// the lyra's usb-c port is the rk3506's dwc2 otg, run as the adb and serial gadget
		publishNub("rockchip-dwc2");
		startWatchdogKick(PD_RK3506_WDT_BASE, PD_RK3506_CRU_BASE);
	} else {
		publishBcm2835EMMC();
	}
#endif

	publishBcm283xFramebuffer();

	return true;
}

// RV1106 TS-ADC (TRM ch. 11): sensor enable in VO_GRF, channel 0 in auto mode, read on demand
#define PD_RV1106_TSADC_BASE	0xff3c8000
#define PD_RV1106_VOGRF_BASE	0xff060000
#define PD_TSADC_AUTO_CON	0x04
#define PD_TSADC_AUTO_SRC	0x0c
#define PD_TSADC_DATA0		0x2c
#define PD_VOGRF_TSADC_CON	0x0c

static IOMemoryMap *sTsadcMap;

// TRM Table 11-2, typical code per temperature; linear between points
static const struct { int16_t celsius; int16_t code; } sTsadcTable[] = {
	{ -40, 628 }, { -35, 620 }, { -30, 612 }, { 0, 561 }, { 5, 553 }, { 10, 545 }, { 25, 520 },
	{ 30, 512 }, { 80, 427 }, { 85, 419 }, { 115, 368 }, { 120, 359 }, { 125, 351 },
};

static int
pdTsadcMilliCelsius(uint32_t code)
{
	const unsigned n = sizeof(sTsadcTable) / sizeof(sTsadcTable[0]);

	if ((int)code >= sTsadcTable[0].code) {
		return sTsadcTable[0].celsius * 1000;
	}
	for (unsigned i = 1; i < n; i++) {
		if ((int)code >= sTsadcTable[i].code) {
			int c0 = sTsadcTable[i - 1].code, c1 = sTsadcTable[i].code;
			int t0 = sTsadcTable[i - 1].celsius * 1000, t1 = sTsadcTable[i].celsius * 1000;

			return t0 + (t1 - t0) * (c0 - (int)code) / (c0 - c1);
		}
	}
	return sTsadcTable[n - 1].celsius * 1000;
}

static int
pdTemperatureSysctl SYSCTL_HANDLER_ARGS
{
	int mc;

	if (sTsadcMap == NULL) {
		return ENODEV;
	}
	mc = pdTsadcMilliCelsius(*(volatile uint32_t *)(sTsadcMap->getVirtualAddress() + PD_TSADC_DATA0) & 0xfff);
	return SYSCTL_OUT(req, &mc, sizeof(mc));
}

SYSCTL_DECL(_machdep);
SYSCTL_PROC(_machdep, OID_AUTO, temperature, CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED, NULL, 0,
    pdTemperatureSysctl, "I", "SoC temperature in millidegrees Celsius");

void
PDArmPlatformExpert::startTemperature(void)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(PD_RV1106_VOGRF_BASE, 0x1000, kIODirectionInOut);
	IOMemoryMap *grf = md != NULL ? md->map(kIOMapInhibitCache) : NULL;
	volatile uint32_t *t;

	OSSafeReleaseNULL(md);
	if (grf == NULL) {
		return;
	}
	// tsadc_tsen_en plus tsadc_ana_reg0..2
	*(volatile uint32_t *)(grf->getVirtualAddress() + PD_VOGRF_TSADC_CON) = 0x01ff0107;
	grf->release();
	md = IOMemoryDescriptor::withPhysicalAddress(PD_RV1106_TSADC_BASE, 0x1000, kIODirectionInOut);
	sTsadcMap = md != NULL ? md->map(kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(md);
	if (sTsadcMap == NULL) {
		return;
	}
	t = (volatile uint32_t *)sTsadcMap->getVirtualAddress();
	t[PD_TSADC_AUTO_SRC / 4] = 1;
	t[PD_TSADC_AUTO_CON / 4] = 0x00010001;
	IODelay(2000);
	sysctl_register_oid(&sysctl__machdep_temperature);
	IOLog("PDArmPlatformExpert: TS-ADC code %u, %d mC\n", t[PD_TSADC_DATA0 / 4] & 0xfff,
	    pdTsadcMilliCelsius(t[PD_TSADC_DATA0 / 4] & 0xfff));
}

long
PDArmPlatformExpert::getGMTTimeOfDay(void)
{
	return fRTC != NULL ? fRTC->getGMTTimeOfDay() : super::getGMTTimeOfDay();
}

void
PDArmPlatformExpert::setGMTTimeOfDay(long secs)
{
	if (fRTC != NULL) {
		fRTC->setGMTTimeOfDay(secs);
	}
}

#define PD_WDT_CR		0x00
#define PD_WDT_TORR		0x04
#define PD_WDT_CRR		0x0c
#define PD_WDT_KICK_MS		5000
// TOPCRU global reset control (TRM 2.4): out of reset the watchdog does not reset the chip
#define PD_CRU_GLB_SRST_FST	0x0c08
#define PD_CRU_GLB_RST_CON	0x0c10
#define PD_GLB_RST_WDT_FIRST	(1u << 11)	// wdt0 triggers the first global reset
#define PD_GLB_RST_WDT_EN	(1u << 6)
#define PD_GLB_RST_KEEP_PMU	(1u << 3)	// keeps PMU_GRF_OS_REG0 (reboot mode) across it

// The loader arms the DesignWare watchdog (about 21 s) and it cannot be stopped once on
void
PDArmPlatformExpert::startWatchdogKick(uint32_t wdtBase, uint32_t cruBase)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(wdtBase, 0x100, kIODirectionInOut);
	IOWorkLoop *wl = IOWorkLoop::workLoop();

	if (md == NULL || wl == NULL) return;
	fWdtMap = md->map(kIOMapInhibitCache);
	md->release();
	if (fWdtMap == NULL) return;
	md = IOMemoryDescriptor::withPhysicalAddress(cruBase, 0x1000, kIODirectionInOut);
	fCruMap = md != NULL ? md->map(kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(md);
	if (fCruMap != NULL) {
		volatile uint32_t *con = (volatile uint32_t *)(fCruMap->getVirtualAddress() + PD_CRU_GLB_RST_CON);

		volatile uint32_t *cru = (volatile uint32_t *)fCruMap->getVirtualAddress();
		uint32_t con0, con1, fbdiv, refdiv, post1, post2;

		// Reads back unreliably, so write the whole value
		*con = PD_GLB_RST_WDT_FIRST | PD_GLB_RST_WDT_EN | PD_GLB_RST_KEEP_PMU;
		IOLog("PDArmPlatformExpert: watchdog now resets the chip\n");
		// Core clock for hw.cpufrequency: APLL = 24 MHz * fbdiv / refdiv / postdiv1 / postdiv2
		// the rk3506 keeps its plls elsewhere in the cru, so only the rv1106 reads them here
		con0 = cruBase == PD_RV1106_CRU_BASE ? cru[0] : (1u << 15);
		con1 = cru[1];
		fbdiv = con0 & 0xfff;
		post1 = (con0 >> 12) & 7;
		post2 = (con1 >> 6) & 7;
		refdiv = con1 & 0x3f;
		if (!(con0 & (1u << 15)) && fbdiv && post1 && post2 && refdiv) {
			uint64_t hz = 24000000ULL * fbdiv / refdiv / post1 / post2;

			gPEClockFrequencyInfo.cpu_frequency_hz = hz;
			gPEClockFrequencyInfo.cpu_frequency_min_hz = hz;
			gPEClockFrequencyInfo.cpu_frequency_max_hz = hz;
			gPEClockFrequencyInfo.cpu_clock_rate_hz = (uint32_t)hz;
			IOLog("PDArmPlatformExpert: APLL at %llu MHz\n", hz / 1000000);
		}
	}
	fWdtTimer = IOTimerEventSource::timerEventSource(this, &PDArmPlatformExpert::kickWatchdog);
	if (fWdtTimer == NULL || wl->addEventSource(fWdtTimer) != kIOReturnSuccess) return;
	kickWatchdog(this, fWdtTimer);
	IOLog("PDArmPlatformExpert: servicing the watchdog every %u ms\n", PD_WDT_KICK_MS);
}

void
PDArmPlatformExpert::kickWatchdog(OSObject *owner, IOTimerEventSource *sender)
{
	PDArmPlatformExpert *pe = OSDynamicCast(PDArmPlatformExpert, owner);
	static uint32_t kicks, limit = ~0U;

	if (pe == NULL || pe->fWdtMap == NULL) return;
	// pdwdtsecs=N: stop after N seconds so an unattended board resets back to U-Boot
	if (kicks == 0) {
		uint32_t secs = 0;
		if (PE_parse_boot_argn("pdwdtsecs", &secs, sizeof(secs)) && secs != 0) {
			IOLog("PDArmPlatformExpert: watchdog left to expire after %u s\n", secs);
			limit = secs * 1000 / PD_WDT_KICK_MS;
		}
	}
	if (kicks++ >= limit) return;
	*(volatile uint32_t *)(pe->fWdtMap->getVirtualAddress() + PD_WDT_CRR) = 0x76;
	sender->setTimeoutMS(PD_WDT_KICK_MS);
}

// The RV1106 and RK3506 reset through the CRU's first global soft reset; the watchdog at its shortest
// timeout (2^16 pclk cycles) is the fallback
int
PDArmPlatformExpert::haltRestart(unsigned int type)
{
	if ((type == kPERestartCPU || type == kPEPanicRestartCPU) && fCruMap != NULL) {
		IOLog("PDArmPlatformExpert: global soft reset\n");
		*(volatile uint32_t *)(fCruMap->getVirtualAddress() + PD_CRU_GLB_SRST_FST) = 0xfdb9;
		IODelay(1000);
	}
	if ((type == kPERestartCPU || type == kPEPanicRestartCPU) && fWdtMap != NULL) {
		volatile uint32_t *wdt = (volatile uint32_t *)fWdtMap->getVirtualAddress();

		IOLog("PDArmPlatformExpert: restarting through the watchdog\n");
		wdt[PD_WDT_TORR / 4] = 0;
		wdt[PD_WDT_CRR / 4] = 0x76;
		wdt[PD_WDT_CR / 4] = 1;
		for (;;) {
		}
	}
	return super::haltRestart(type);
}

/*
 * The Pi's device tree describes no display: the VideoCore firmware allocates
 * the framebuffer and the loader passes its geometry in boot_args, which is
 * where IOGOPFramebuffer reads it from anyway. All this nub has to do is give
 * that driver something to match on, since there is no IOPCIDevice here.
 */
void
PDArmPlatformExpert::publishBcm283xFramebuffer(void)
{
	PE_Video console;
	IOService *nub = NULL;

	if (fFramebufferNub != NULL) return;
	if (!pd_platform_is_bcm283x()) return;

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

// Root device tree node's compatible names this board
bool
PDArmPlatformExpert::boardIs(const char *compatible)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	OSData *c = root != NULL ? OSDynamicCast(OSData, root->getProperty("compatible")) : NULL;
	bool match = false;

	if (c != NULL) {
		const char *p = (const char *)c->getBytesNoCopy(), *end = p + c->getLength();

		for (; p < end && !match; p += strnlen(p, (size_t)(end - p)) + 1) {
			match = strncmp(p, compatible, (size_t)(end - p)) == 0;
		}
	}
	OSSafeReleaseNULL(root);
	return match;
}

// A plain service-plane nub for a driver to match by name, like the emmc one
void
PDArmPlatformExpert::publishNub(const char *name)
{
	IOService *nub = OSTypeAlloc(IOPlatformDevice);

	if (nub == NULL || !nub->init() || (nub->setName(name), !nub->attach(this))) {
		IOLog("PDArmPlatformExpert: could not publish the %s nub\n", name);
		OSSafeReleaseNULL(nub);
		return;
	}
	nub->registerService();
	nub->release();
	IOLog("PDArmPlatformExpert: published %s nub\n", name);
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
	// hw.machine / uname -m
#if defined(__arm64__)
	strlcpy(name, "arm64", (size_t)maxLength);
#else
	// One armv6 kernel runs on ARM11 and Cortex-A cores: name what this core implements
	uint32_t midr;

	__asm__ volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r"(midr));
	strlcpy(name, ((midr >> 4) & 0xf00) == 0xc00 ? "armv7" : "armv6", (size_t)maxLength);
#endif
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
	if (pd_platform_is_bcm283x() && !PDBcm2835IC_init()) {
		IOLog("PDArmPlatformExpert: BCM2835 interrupt controller unavailable; "
		    "device interrupts will not be delivered\n");
	}
}

const char *
PDArmPlatformExpert::platformModelName(void)
{
#if defined(__arm__) && !defined(__arm64__)
	// The arm32 loader names the board in the device tree root (e.g. "Luckfox Pico (RV1103)")
	static char model[64];
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	OSData *d = root != NULL ? OSDynamicCast(OSData, root->getProperty("model")) : NULL;

	if (d != NULL && d->getLength() > 1) {
		size_t n = d->getLength() < sizeof(model) ? d->getLength() : sizeof(model) - 1;

		bcopy(d->getBytesNoCopy(), model, n);
		model[n] = 0;
		OSSafeReleaseNULL(root);
		return model;
	}
	OSSafeReleaseNULL(root);
#endif
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
