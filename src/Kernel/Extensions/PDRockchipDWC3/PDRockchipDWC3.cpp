#include "PDRockchipDWC3.h"
#include "PDGadgetACM.h"
#include "PDGadgetADB.h"

#include <IOKit/IOLib.h>
#include <kern/thread.h>
#include <libkern/OSAtomic.h>
#include <pexpert/pexpert.h>

#define super PDGadgetController
OSDefineMetaClassAndStructors(PDRockchipDWC3, PDGadgetController);
OSDefineMetaClassAndAbstractStructors(PDGadgetController, IOService);
OSDefineMetaClassAndAbstractStructors(PDGadgetFunction, OSObject);

#define DWC3_PHYS     0xffb0c000u	// global and device registers of the core at 0xffb00000
#define PERICRU_PHYS  0xff3b2000u	// GATE_CON04 0x810, GATE_CON05 0x814, SOFTRST_CON04 0xa10
#define PERIGRF_PHYS  0xff000000u	// USBPHY_CON0 0x50, USBPHY_CON2 0x58

// Core registers, relative to DWC3_PHYS
enum {
	kGCTL = 0x110, kGUCTL1 = 0x11c, kGSNPSID = 0x120, kGUSB2PHYCFG = 0x200,
	kGEVNTADRLO = 0x400, kGEVNTADRHI = 0x404, kGEVNTSIZ = 0x408, kGEVNTCOUNT = 0x40c,
	kDCFG = 0x700, kDCTL = 0x704, kDEVTEN = 0x708, kDSTS = 0x70c, kDALEPENA = 0x720,
};
#define DEPCMDPAR2(n) (0x800 + 0x10 * (n))
#define DEPCMDPAR1(n) (0x804 + 0x10 * (n))
#define DEPCMDPAR0(n) (0x808 + 0x10 * (n))
#define DEPCMD(n)     (0x80c + 0x10 * (n))

#define GCTL_PRTCAP_MASK   (3u << 12)
#define GCTL_PRTCAP_DEVICE (2u << 12)
#define GCTL_SCALEDOWN     (3u << 4)
#define PHYCFG_PHYIF16     (1u << 3)
#define PHYCFG_SUSPHY      (1u << 6)
#define PHYCFG_ENBLSLPM    (1u << 8)
#define PHYCFG_TRDTIM(n)   ((n) << 10)
#define PHYCFG_FREECLK     (1u << 30)
#define DCTL_RUN           (1u << 31)
#define DCTL_CSFTRST       (1u << 30)
#define DSTS_HALTED        (1u << 22)

enum { kCmdSetEpConfig = 1, kCmdSetXferResource = 2, kCmdSetStall = 4, kCmdClearStall = 5,
	   kCmdStartTransfer = 6, kCmdEndTransfer = 8, kCmdStartConfig = 9 };
#define CMD_ACTIVE  (1u << 10)
#define CMD_FORCERM (1u << 11)
#define CMD_PARAM(n) ((uint32_t)(n) << 16)

#define CFG_TYPE(n)     ((n) << 1)
#define CFG_MPS(n)      ((uint32_t)(n) << 3)
#define CFG_FIFO(n)     ((uint32_t)(n) << 17)
#define CFG_MODIFY      (2u << 30)
#define CFG_XFERCMPL    (1u << 8)
#define CFG_XFERNRDY    (1u << 10)
#define CFG_INTERVAL(n) ((uint32_t)(n) << 16)
#define CFG_EPNUM(n)    ((uint32_t)(n) << 25)

#define TRB_HWO  (1u << 0)
#define TRB_LST  (1u << 1)
#define TRB_ISP  (1u << 10)
#define TRB_IOC  (1u << 11)
#define TRB_TYPE(n) ((n) << 4)
enum { kTrbNormal = 1, kTrbSetup = 2, kTrbStatus2 = 3, kTrbStatus3 = 4, kTrbData = 5 };

enum { kEvXferComplete = 1, kEvXferNotReady = 3 };
enum { kDevDisconnect = 0, kDevReset = 1, kDevConnectDone = 2 };

#define USB_VENDOR  0x1209	// pid.codes test range
#define USB_PRODUCT 0x0001

// BOOT_BROM_DOWNLOAD in the reboot-mode register asks the boot ROM for USB download
#define BOOT_BROM_DOWNLOAD 0xef08a53cu
#define CRU_GLB_SRST_FST 0x0c08
#define CRU_GLB_RST_CON 0x0c10
#define GLB_RST_KEEP_PMU (1u << 3)	// global reset leaves the PMU (and OS_REG0) alone
// PMU_GRF_OS_REG0 (DT reboot-mode offset 0x20200 in the GRF syscon), and the TOPCRU
#define RV1106_PMUGRF_OSREG0 0xff020200u
#define RV1106_TOPCRU 0xff3b0000u

void
PDGadgetController::romDownloadReset(uint32_t osreg, uint32_t cru)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(osreg & ~0xfffu, 0x1000, kIODirectionInOut);
	IOMemoryMap *map = md != NULL ? md->map(kIOMapInhibitCache) : NULL;

	if (map != NULL) {
		*(volatile uint32_t *)(map->getVirtualAddress() + (osreg & 0xfff)) = BOOT_BROM_DOWNLOAD;
	}
	OSSafeReleaseNULL(map);
	OSSafeReleaseNULL(md);
	// Straight to the CRU's global soft reset: a RAM root has nothing to flush, and the
	// orderly path spends ~45 s in power management first
	md = IOMemoryDescriptor::withPhysicalAddress(cru, 0x1000, kIODirectionInOut);
	map = md != NULL ? md->map(kIOMapInhibitCache) : NULL;
	if (map != NULL) {
		volatile uint32_t *regs = (volatile uint32_t *)map->getVirtualAddress();

		regs[CRU_GLB_RST_CON / 4] = GLB_RST_KEEP_PMU;
		regs[CRU_GLB_SRST_FST / 4] = 0xfdb9;
		IODelay(1000);
	}
}

void
PDRockchipDWC3::rebootToMaskrom(void)
{
	romDownloadReset(RV1106_PMUGRF_OSREG0, RV1106_TOPCRU);
}

static void
writeMasked(uint32_t phys, uint32_t off, uint32_t mask, uint32_t value)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(phys, 0x1000, kIODirectionInOut);
	IOMemoryMap *map = md != NULL ? md->map(kIOMapInhibitCache) : NULL;

	// Rockchip CRU/GRF registers: the high half enables writing the matching low bits
	if (map != NULL) {
		*(volatile uint32_t *)(map->getVirtualAddress() + off) = (mask << 16) | (value & mask);
	}
	OSSafeReleaseNULL(map);
	OSSafeReleaseNULL(md);
}

void
PDRockchipDWC3::setupClocksEarly(void)
{
	// aclk_usbotg, clk_ref_usbotg, clk_utmi_usbotg and the PHY's pclk/ref clock on
	writeMasked(PERICRU_PHYS, 0x810, (1u << 7) | (1u << 8) | (1u << 9), 0);
	writeMasked(PERICRU_PHYS, 0x814, (1u << 1) | (1u << 2), 0);
	writeMasked(PERICRU_PHYS, 0xa10, 1u << 7, 0);
	IODelay(10);
}

void
PDRockchipDWC3::setupClocks(void)
{
	uint32_t keep = 0;

	// PHY out of reset (left as it is), the controller pulsed unless pdusbkeep=1
	writeMasked(PERICRU_PHYS, 0xa14, 0xe, 0);
	if (!PE_parse_boot_argn("pdusbkeep", &keep, sizeof(keep)) || keep == 0) {
		writeMasked(PERICRU_PHYS, 0xa10, 1u << 7, 1u << 7);
		IODelay(20);
		writeMasked(PERICRU_PHYS, 0xa10, 1u << 7, 0);
		IODelay(20);
	}
	// The board is bus powered: session valid always, ID pin reads as device
	writeMasked(PERIGRF_PHYS, 0x58, 0xc000, 0xc000);
	writeMasked(PERIGRF_PHYS, 0x50, 0x0600, 0x0600);
	// Mainline U-Boot leaves the PHY suspended (USBPHY_CON0[8:0] = 0x1d1) when fastboot exits
	writeMasked(PERIGRF_PHYS, 0x50, 0x01ff, 0);
}

void *
PDRockchipDWC3::dmaAlloc(uint32_t size, uint32_t align, uint32_t *phys)
{
	uint32_t off = (fDMAUsed + align - 1) & ~(align - 1);

	if (off + size > fDMA->getLength()) {
		return NULL;
	}
	fDMAUsed = off + size;
	*phys = fDMAPhys + off;
	bzero(fDMABase + off, size);
	return fDMABase + off;
}

bool
PDRockchipDWC3::epCommand(uint8_t phys, uint32_t cmd, uint32_t p0, uint32_t p1, uint32_t p2)
{
	setReg(DEPCMDPAR0(phys), p0);
	setReg(DEPCMDPAR1(phys), p1);
	setReg(DEPCMDPAR2(phys), p2);
	setReg(DEPCMD(phys), cmd | CMD_ACTIVE);
	for (uint32_t n = 0; n < 100000; n++) {
		uint32_t v = reg(DEPCMD(phys));

		if ((v & CMD_ACTIVE) == 0) {
			if (((v >> 12) & 0xf) != 0) {
				IOLog("PDRockchipDWC3: ep %u command %u status %u\n", phys, cmd & 0xf, (v >> 12) & 0xf);
				return false;
			}
			return true;
		}
		IODelay(1);
	}
	IOLog("PDRockchipDWC3: ep %u command %u timed out\n", phys, cmd & 0xf);
	return false;
}

bool
PDRockchipDWC3::epConfigure(uint8_t phys, uint32_t action)
{
	Ep *ep = &fEps[phys];
	uint16_t mps = ep->maxPacket;
	uint32_t p0, p1;

	if (!fHighSpeed && ep->type == kPDEpBulk && mps > 64) {
		mps = 64;
	}
	p0 = CFG_TYPE(ep->type) | CFG_MPS(mps) | action;
	if (phys & 1) {
		p0 |= CFG_FIFO(phys >> 1);
	}
	p1 = CFG_XFERCMPL | CFG_EPNUM(phys);
	if (ep->type == kPDEpControl) {
		p1 |= CFG_XFERNRDY;
	}
	if (ep->type == kPDEpInterrupt && ep->interval > 0) {
		p1 |= CFG_INTERVAL(ep->interval - 1);
	}
	if (!epCommand(phys, kCmdSetEpConfig, p0, p1, 0)) {
		return false;
	}
	if (action == 0 && !epCommand(phys, kCmdSetXferResource, 1, 0, 0)) {
		return false;
	}
	setReg(kDALEPENA, reg(kDALEPENA) | (1u << phys));
	ep->enabled = true;
	return true;
}

bool
PDRockchipDWC3::startTransfer(uint8_t phys, uint32_t bufPhys, uint32_t length, uint32_t trbType)
{
	Ep *ep = &fEps[phys];

	ep->trb->bpl = bufPhys;
	ep->trb->bph = 0;
	ep->trb->size = length;
	ep->trb->ctrl = TRB_TYPE(trbType) | TRB_LST | TRB_IOC | TRB_ISP | TRB_HWO;
	OSMemoryBarrier();
	if (!epCommand(phys, kCmdStartTransfer, 0, ep->trbPhys, 0)) {
		return false;
	}
	ep->resource = (reg(DEPCMD(phys)) >> 16) & 0x7f;
	ep->requested = length;
	ep->busy = true;
	return true;
}

void
PDRockchipDWC3::endTransfer(uint8_t phys)
{
	Ep *ep = &fEps[phys];

	if (ep->busy) {
		epCommand(phys, kCmdEndTransfer | CMD_FORCERM | CMD_PARAM(ep->resource), 0, 0, 0);
		IODelay(100);
		ep->busy = false;
	}
}

bool
PDRockchipDWC3::initCore(void)
{
	uint32_t id = reg(kGSNPSID), v, g;

	IOLog("PDRockchipDWC3: core id 0x%08x\n", id);
	if ((id & 0xffff0000) != 0x55330000) {
		return false;
	}
	setReg(kDCTL, DCTL_CSFTRST);
	for (v = 0; v < 500 && (reg(kDCTL) & DCTL_CSFTRST) != 0; v++) {
		IOSleep(1);
	}
	if (reg(kDCTL) & DCTL_CSFTRST) {
		IOLog("PDRockchipDWC3: soft reset stuck (no PHY clock?)\n");
		return false;
	}
	setReg(kGCTL, (reg(kGCTL) & ~(GCTL_PRTCAP_MASK | GCTL_SCALEDOWN)) | GCTL_PRTCAP_DEVICE);
	// 16-bit UTMI+ (utmi_wide), PHY never suspended by the core; pdusbphycfg= overrides
	v = reg(kGUSB2PHYCFG);
	IOLog("PDRockchipDWC3: GUSB2PHYCFG 0x%08x at boot, 0x%08x now; GUCTL1 0x%08x at boot, 0x%08x now\n",
	    fBootPhyCfg, v, fBootGuctl1, reg(kGUCTL1));
	if (PE_parse_boot_argn("pdusbguctl1", &g, sizeof(g))) {
		setReg(kGUCTL1, g);
	}
	if (!PE_parse_boot_argn("pdusbphycfg", &v, sizeof(v))) {
		v &= ~(PHYCFG_SUSPHY | PHYCFG_ENBLSLPM | PHYCFG_TRDTIM(0xf) | PHYCFG_FREECLK);
		v |= PHYCFG_PHYIF16 | PHYCFG_TRDTIM(5);
	}
	setReg(kGUSB2PHYCFG, v);

	setReg(kGEVNTADRLO, fDMAPhys);	// the event buffer opens the DMA pool
	setReg(kGEVNTADRHI, 0);
	setReg(kGEVNTSIZ, kEventBuffer | (1u << 31));	// polled: interrupt masked
	setReg(kGEVNTCOUNT, reg(kGEVNTCOUNT));
	fEventPos = 0;

	setReg(kDCFG, reg(kDCFG) & ~(0x7u | (0x7fu << 3) | (1u << 22)));	// high speed, address 0, no LPM
	setReg(kDEVTEN, (1u << kDevDisconnect) | (1u << kDevReset) | (1u << kDevConnectDone));

	fHighSpeed = true;
	for (uint8_t phys = 0; phys < 2; phys++) {
		fEps[phys].type = kPDEpControl;
		fEps[phys].maxPacket = 64;
	}
	if (!epCommand(0, kCmdStartConfig | CMD_PARAM(0), 0, 0, 0) ||
	    !epConfigure(0, 0) || !epConfigure(1, 0)) {
		return false;
	}
	ep0Setup();
	setReg(kDCTL, reg(kDCTL) | DCTL_RUN);
	IOLog("PDRockchipDWC3: device mode running, GUSB2PHYCFG 0x%08x DSTS 0x%08x\n",
	    reg(kGUSB2PHYCFG), reg(kDSTS));
	return true;
}

void
PDRockchipDWC3::addFunction(PDGadgetFunction *fn)
{
	if (fFunctionCount < kMaxFunctions) {
		fFunctions[fFunctionCount++] = fn;
		fInterfaceCount += fn->interfaceCount();
	}
}

PDRockchipDWC3::Ep *
PDRockchipDWC3::endpointFor(PDGadgetFunction *fn, uint8_t index, uint8_t *phys)
{
	for (uint8_t p = 2; p < sizeof(fEps) / sizeof(fEps[0]); p++) {
		if (fEps[p].fn == fn && fEps[p].index == index) {
			if (phys != NULL) {
				*phys = p;
			}
			return &fEps[p];
		}
	}
	return NULL;
}

uint8_t *
PDRockchipDWC3::buffer(PDGadgetFunction *fn, uint8_t index)
{
	Ep *ep = endpointFor(fn, index, NULL);

	return ep != NULL ? ep->buf : NULL;
}

bool
PDRockchipDWC3::busy(PDGadgetFunction *fn, uint8_t index)
{
	Ep *ep = endpointFor(fn, index, NULL);

	return ep == NULL || !ep->enabled || ep->busy;
}

bool
PDRockchipDWC3::queue(PDGadgetFunction *fn, uint8_t index, uint32_t length)
{
	uint8_t phys;
	Ep *ep = endpointFor(fn, index, &phys);

	if (ep == NULL || !ep->enabled || ep->busy || length > ep->bufferSize) {
		return false;
	}
	return startTransfer(phys, ep->bufPhys, length, kTrbNormal);
}

// Device, configuration and interface descriptors; the functions follow behind IADs
static const uint8_t sDeviceDescriptor[18] = {
	18, 1, 0x00, 0x02, 0xef, 0x02, 0x01, 64,
	USB_VENDOR & 0xff, USB_VENDOR >> 8, USB_PRODUCT & 0xff, USB_PRODUCT >> 8,
	0x00, 0x01, 1, 2, 3, 1,
};
static const char *const sStrings[] = { NULL, "PureDarwin", "PureDarwin", "0001" };

uint32_t
PDRockchipDWC3::configDescriptor(uint8_t *out)
{
	uint32_t len = 9;
	uint8_t iface = 0, in = 1, outEp = 1;

	for (uint8_t f = 0; f < fFunctionCount; f++) {
		PDGadgetFunction *fn = fFunctions[f];
		uint8_t addr[kMaxEndpoints];

		for (uint8_t i = 0; i < fn->endpointCount() && i < kMaxEndpoints; i++) {
			addr[i] = fn->endpoint(i).in ? (0x80 | in++) : outEp++;
		}
		len += fn->descriptors(out + len, iface, addr, fHighSpeed);
		iface += fn->interfaceCount();
	}
	out[0] = 9;
	out[1] = 2;
	out[2] = len & 0xff;
	out[3] = len >> 8;
	out[4] = iface;
	out[5] = 1;
	out[6] = 0;
	out[7] = 0x80;		// bus powered
	out[8] = 250;		// 500 mA
	return len;
}

bool
PDRockchipDWC3::setConfiguration(uint8_t value)
{
	uint8_t in = 1, out = 1;

	clearConfiguration();
	if (value == 0) {
		return true;
	}
	if (value != 1) {
		return false;
	}
	// Transfer resources restart after ep0's two
	if (!epCommand(0, kCmdStartConfig | CMD_PARAM(2), 0, 0, 0)) {
		return false;
	}
	for (uint8_t f = 0; f < fFunctionCount; f++) {
		PDGadgetFunction *fn = fFunctions[f];

		for (uint8_t i = 0; i < fn->endpointCount(); i++) {
			PDGadgetFunction::Endpoint d = fn->endpoint(i);
			uint8_t phys = d.in ? (in++ * 2 + 1) : (out++ * 2);
			Ep *ep = &fEps[phys];

			if (phys >= sizeof(fEps) / sizeof(fEps[0])) {
				return false;
			}
			if (ep->buf == NULL) {
				ep->trb = (volatile Trb *)dmaAlloc(sizeof(Trb), 16, &ep->trbPhys);
				ep->buf = (uint8_t *)dmaAlloc(d.bufferSize, 64, &ep->bufPhys);
				if (ep->trb == NULL || ep->buf == NULL) {
					IOLog("PDRockchipDWC3: out of DMA memory\n");
					return false;
				}
			}
			ep->fn = fn;
			ep->index = i;
			ep->type = d.type;
			ep->maxPacket = d.maxPacket;
			ep->interval = d.interval;
			ep->bufferSize = d.bufferSize;
			if (!epConfigure(phys, 0)) {
				return false;
			}
		}
	}
	fConfiguration = value;
	in = 1;
	out = 1;
	for (uint8_t f = 0; f < fFunctionCount; f++) {
		uint8_t first = 0;

		endpointFor(fFunctions[f], 0, &first);
		fFunctions[f]->configured(this, first);
	}
	IOLog("PDRockchipDWC3: configured (%s speed)\n", fHighSpeed ? "high" : "full");
	return true;
}

void
PDRockchipDWC3::clearConfiguration(void)
{
	if (fConfiguration != 0) {
		for (uint8_t f = 0; f < fFunctionCount; f++) {
			fFunctions[f]->deconfigured();
		}
	}
	for (uint8_t p = 2; p < sizeof(fEps) / sizeof(fEps[0]); p++) {
		if (fEps[p].enabled) {
			endTransfer(p);
			setReg(kDALEPENA, reg(kDALEPENA) & ~(1u << p));
			fEps[p].enabled = false;
		}
	}
	fConfiguration = 0;
}

void
PDRockchipDWC3::ep0Setup(void)
{
	fEp0State = kSetup;
	fStatusReady[0] = fStatusReady[1] = false;
	startTransfer(0, fSetupPhys, 8, kTrbSetup);
}

void
PDRockchipDWC3::ep0Stall(void)
{
	epCommand(0, kCmdSetStall, 0, 0, 0);
	ep0Setup();
}

void
PDRockchipDWC3::ep0Status(uint8_t phys)
{
	fEp0State = (phys & 1) ? kStatusIn : kStatusOut;
	if (fStatusReady[phys & 1]) {
		startTransfer(phys, fEp0Phys, 0, fSetup.wLength != 0 ? kTrbStatus3 : kTrbStatus2);
	}
}

int
PDRockchipDWC3::standardRequest(const PDUSBSetup *s, uint8_t *data)
{
	switch (s->bRequest) {
	case 0x00:	// GET_STATUS
		data[0] = data[1] = 0;
		return 2;
	case 0x01:	// CLEAR_FEATURE (endpoint halt)
		if ((s->bmRequestType & 0x1f) == 2 && s->wValue == 0) {
			uint8_t addr = s->wIndex & 0xff;
			uint8_t phys = (addr & 0x7f) * 2 + ((addr & 0x80) ? 1 : 0);

			if (phys >= 2 && phys < sizeof(fEps) / sizeof(fEps[0]) && fEps[phys].enabled) {
				epCommand(phys, kCmdClearStall, 0, 0, 0);
			}
		}
		return 0;
	case 0x03:	// SET_FEATURE
		return 0;
	case 0x05:	// SET_ADDRESS: the core wants it before the status stage
		setReg(kDCFG, (reg(kDCFG) & ~(0x7fu << 3)) | ((s->wValue & 0x7f) << 3));
		return 0;
	case 0x06: {	// GET_DESCRIPTOR
		uint8_t type = s->wValue >> 8, index = s->wValue & 0xff;

		switch (type) {
		case 1:
			bcopy(sDeviceDescriptor, data, sizeof(sDeviceDescriptor));
			if (fPerInterfaceClass) {
				data[4] = data[5] = data[6] = 0;
			}
			return sizeof(sDeviceDescriptor);
		case 2:
			return configDescriptor(data);
		case 3:
			if (index == 0) {
				data[0] = 4;
				data[1] = 3;
				data[2] = 0x09;
				data[3] = 0x04;
				return 4;
			}
			if (index < sizeof(sStrings) / sizeof(sStrings[0])) {
				const char *str = sStrings[index];
				uint32_t n = 0;

				while (str[n] != 0 && n < 100) {
					data[2 + 2 * n] = str[n];
					data[3 + 2 * n] = 0;
					n++;
				}
				data[0] = 2 + 2 * n;
				data[1] = 3;
				return data[0];
			}
			return -1;
		case 6: {	// device qualifier
			static const uint8_t q[10] = { 10, 6, 0x00, 0x02, 0xef, 0x02, 0x01, 64, 1, 0 };

			bcopy(q, data, sizeof(q));
			if (fPerInterfaceClass) {
				data[4] = data[5] = data[6] = 0;
			}
			return sizeof(q);
		}
		default:
			return -1;
		}
	}
	case 0x08:	// GET_CONFIGURATION
		data[0] = fConfiguration;
		return 1;
	case 0x09:	// SET_CONFIGURATION
		return setConfiguration(s->wValue & 0xff) ? 0 : -1;
	case 0x0a:	// GET_INTERFACE
		data[0] = 0;
		return 1;
	case 0x0b:	// SET_INTERFACE: alternate 0 only
		return s->wValue == 0 ? 0 : -1;
	default:
		return -1;
	}
}

void
PDRockchipDWC3::ep0Event(uint8_t phys, uint32_t type, uint32_t status)
{
	if (type == kEvXferNotReady) {
		// Control status stage requested by the host on this direction
		if ((status & 3) == 2) {
			fStatusReady[phys & 1] = true;
			if ((fEp0State == kStatusIn && phys == 1) || (fEp0State == kStatusOut && phys == 0)) {
				startTransfer(phys, fEp0Phys, 0, fSetup.wLength != 0 ? kTrbStatus3 : kTrbStatus2);
			}
		}
		return;
	}
	if (type != kEvXferComplete) {
		return;
	}
	fEps[phys].busy = false;
	switch (fEp0State) {
	case kSetup: {
		PDUSBSetup *s = &fSetup;
		int len = -1;

		bcopy(fSetupBuf, s, sizeof(*s));
		if ((s->bmRequestType & 0x60) == 0) {
			len = standardRequest(s, fEp0Buf);
		} else if ((s->bmRequestType & 0x1f) == 1) {
			uint8_t first = 0;

			for (uint8_t f = 0; f < fFunctionCount; f++) {
				uint8_t count = fFunctions[f]->interfaceCount();

				if ((s->wIndex & 0xff) >= first && (s->wIndex & 0xff) < first + count) {
					len = fFunctions[f]->setup(s, fEp0Buf);
					break;
				}
				first += count;
			}
		}
		if (fTrace > 0) {
			IOLog("PDRockchipDWC3: setup %02x %02x %04x %04x %u -> %d\n", s->bmRequestType, s->bRequest,
			    s->wValue, s->wIndex, s->wLength, len);
		}
		if (len < 0) {
			IOLog("PDRockchipDWC3: stall %02x %02x %04x %04x %u\n", s->bmRequestType, s->bRequest,
			    s->wValue, s->wIndex, s->wLength);
			ep0Stall();
		} else if (s->wLength == 0) {
			ep0Status(1);
		} else if (s->bmRequestType & 0x80) {
			fEp0State = kDataIn;
			startTransfer(1, fEp0Phys, (uint32_t)len < s->wLength ? len : s->wLength, kTrbData);
		} else {
			fEp0State = kDataOut;
			startTransfer(0, fEp0Phys, 64, kTrbData);
		}
		break;
	}
	case kDataIn:
		ep0Status(0);
		break;
	case kDataOut: {
		uint32_t got = 64 - (fEps[0].trb->size & 0xffffff);

		for (uint8_t f = 0, first = 0; f < fFunctionCount; f++) {
			uint8_t count = fFunctions[f]->interfaceCount();

			if ((fSetup.wIndex & 0xff) >= first && (fSetup.wIndex & 0xff) < first + count) {
				fFunctions[f]->setupData(&fSetup, fEp0Buf, got);
				break;
			}
			first += count;
		}
		ep0Status(1);
		break;
	}
	case kStatusIn:
	case kStatusOut:
		ep0Setup();
		break;
	}
}

void
PDRockchipDWC3::deviceEvent(uint32_t ev)
{
	switch ((ev >> 8) & 0xf) {
	case kDevDisconnect:
		IOLog("PDRockchipDWC3: disconnected\n");
		clearConfiguration();
		break;
	case kDevReset:
		setReg(kDCFG, reg(kDCFG) & ~(0x7fu << 3));
		clearConfiguration();
		if (fEp0State != kSetup) {
			endTransfer(0);
			endTransfer(1);
			ep0Setup();
		}
		break;
	case kDevConnectDone:
		fHighSpeed = (reg(kDSTS) & 7) == 0;
		epConfigure(0, CFG_MODIFY);
		epConfigure(1, CFG_MODIFY);
		IOLog("PDRockchipDWC3: connected, %s speed\n", fHighSpeed ? "high" : "full");
		break;
	}
}

void
PDRockchipDWC3::handleEvent(uint32_t ev)
{
	if (fTrace > 0) {
		fTrace--;
		IOLog("PDRockchipDWC3: event 0x%08x ep0 state %d\n", ev, fEp0State);
	}
	if (ev & 1) {
		if (((ev >> 1) & 0x7f) == 0) {
			deviceEvent(ev);
		}
		return;
	}

	uint8_t phys = (ev >> 1) & 0x1f;
	uint32_t type = (ev >> 6) & 0xf, status = (ev >> 12) & 0xf;

	if (phys < 2) {
		ep0Event(phys, type, status);
		return;
	}
	if (type == kEvXferComplete && phys < sizeof(fEps) / sizeof(fEps[0]) && fEps[phys].fn != NULL) {
		Ep *ep = &fEps[phys];
		uint32_t actual = ep->requested - (ep->trb->size & 0xffffff);

		ep->busy = false;
		ep->fn->transferDone(ep->index, actual);
	}
}

void
PDRockchipDWC3::pollLoop(void)
{
	while (fRunning) {
		uint32_t count = reg(kGEVNTCOUNT) & 0xfffc;

		for (uint32_t done = 0; done < count; done += 4) {
			uint32_t ev = fEvents[fEventPos / 4];

			fEventPos = (fEventPos + 4) % kEventBuffer;
			handleEvent(ev);
		}
		if (count != 0) {
			setReg(kGEVNTCOUNT, count);
		}
		for (uint8_t f = 0; f < fFunctionCount; f++) {
			if (fConfiguration != 0) {
				fFunctions[f]->poll();
			}
		}
		IOSleep(1);
	}
}

void
PDRockchipDWC3::pollThread(void *arg, wait_result_t wr)
{
	PDRockchipDWC3 *self = (PDRockchipDWC3 *)arg;

	(void)wr;
	if (self->initCore()) {
		self->pollLoop();
	}
	self->release();
	thread_terminate(current_thread());
}

bool
PDRockchipDWC3::start(IOService *provider)
{
	thread_t th;
	uint32_t off = 0;

	if (!super::start(provider)) {
		return false;
	}
	if (PE_parse_boot_argn("pdusboff", &off, sizeof(off)) && off != 0) {
		return false;
	}
	fRegsMD = IOMemoryDescriptor::withPhysicalAddress(DWC3_PHYS, 0x1000, kIODirectionInOut);
	fRegsMap = fRegsMD != NULL ? fRegsMD->map(kIOMapInhibitCache) : NULL;
	if (fRegsMap == NULL) {
		return false;
	}
	fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();
	// Clocks may still be gated here, so only trust a reading that looks like the core
	setupClocksEarly();
	if ((reg(kGSNPSID) >> 16) == 0x5533) {
		fBootPhyCfg = reg(kGUSB2PHYCFG);
		fBootGuctl1 = reg(kGUCTL1);
	}
	setupClocks();
	// One physically contiguous pool, used through an uncached mapping only
	fDMA = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous, 64 * 1024, 0xfffff000ULL);
	if (fRegsMap == NULL || fDMA == NULL) {
		return false;
	}
	// Mapping the buffer itself in the kernel hands back its cached address, so map the
	// pages by physical address: normal memory, uncached
	fDMAPhys = (uint32_t)fDMA->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
	fDMA->performOperation(kIOMemoryIncoherentIOFlush, 0, fDMA->getLength());
	fDMAPhysMD = IOMemoryDescriptor::withPhysicalAddress(fDMAPhys, fDMA->getLength(), kIODirectionInOut);
	fDMAMap = fDMAPhysMD != NULL ? fDMAPhysMD->map(kIOMapWriteCombineCache) : NULL;
	if (fDMAMap == NULL) {
		return false;
	}
	PE_parse_boot_argn("pdusbtrace", &fTrace, sizeof(fTrace));
	fDMABase = (uint8_t *)fDMAMap->getVirtualAddress();

	fEvents = (volatile uint32_t *)dmaAlloc(kEventBuffer, 4096, &off);
	fSetupBuf = (uint8_t *)dmaAlloc(64, 64, &fSetupPhys);
	fEp0Buf = (uint8_t *)dmaAlloc(kEp0Buffer, 64, &fEp0Phys);
	for (uint8_t phys = 0; phys < 2; phys++) {
		fEps[phys].trb = (volatile Trb *)dmaAlloc(sizeof(Trb), 16, &fEps[phys].trbPhys);
	}
	if (fEvents == NULL || fEps[1].trb == NULL) {
		return false;
	}
	IOLog("PDRockchipDWC3: DMA pool phys 0x%08x at %p (buffer %p)\n", fDMAPhys, fDMABase, fDMA->getBytesNoCopy());

	PDGadgetACM *acm = OSTypeAlloc(PDGadgetACM);
	if (acm == NULL || !acm->init()) {
		OSSafeReleaseNULL(acm);
		return false;
	}
	addFunction(acm);
	// pdusbadb=0 leaves the ADB interface out
	uint32_t wantADB = 1;
	PE_parse_boot_argn("pdusbadb", &wantADB, sizeof(wantADB));
	if (wantADB) {
		PDGadgetADB *adb = OSTypeAlloc(PDGadgetADB);
		if (adb != NULL && adb->init()) {
			addFunction(adb);
			// adb only looks at devices whose class is defined per interface
			fPerInterfaceClass = true;
		} else {
			OSSafeReleaseNULL(adb);
		}
	}

	registerService();
	fRunning = true;
	retain();
	if (kernel_thread_start(&PDRockchipDWC3::pollThread, this, &th) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(th);
	return true;
}

void
PDRockchipDWC3::free(void)
{
	for (uint8_t f = 0; f < fFunctionCount; f++) {
		OSSafeReleaseNULL(fFunctions[f]);
	}
	OSSafeReleaseNULL(fDMAMap);
	OSSafeReleaseNULL(fDMAPhysMD);
	OSSafeReleaseNULL(fDMA);
	OSSafeReleaseNULL(fRegsMap);
	OSSafeReleaseNULL(fRegsMD);
	super::free();
}
