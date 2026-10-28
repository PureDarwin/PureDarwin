// SPDX-License-Identifier: MIT
// tegra210 legacy usb1 chipidea device controller, the hardware under PDGadgetController
#include "PDTegraUDC.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <kern/thread.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

#define super PDGadgetController
OSDefineMetaClassAndStructors(PDTegraUDC, PDGadgetController);

// the core when the platform expert gave no "reg" (tegra210 udc@7d000000)
#define CI_PHYS 0x7d000000u
#define CI_SIZE 0x4000u

// chipIdea registers, from the identification block at 0 to the endpoint controls
enum {
	kID = 0x000, kHWDEVICE = 0x00c,
	kDCCPARAMS = 0x124,
	// caplength reads 0x30 on the tx1, so the operational block starts at 0x130 (u-boot md.l 0x7d000100)
	kUSBCMD = 0x130, kUSBSTS = 0x134, kUSBINTR = 0x138, kDEVICEADDR = 0x144, kENDPTLISTADDR = 0x148,
	// tegra has hostpc, so otgsc onwards sit 0x50 higher than on the msm cores
	kPORTSC = 0x174, kHOSTPC1_DEVLC = 0x1b4, kOTGSC = 0x1f4, kUSBMODE = 0x1f8, kUSB_SUSP_CTRL = 0x400,
	kENDPTSETUPSTAT = 0x208, kENDPTPRIME = 0x20c, kENDPTFLUSH = 0x210, kENDPTSTAT = 0x214,
	kENDPTCOMPLETE = 0x218,
};
#define ENDPTCTRL(n) (0x21c + 4 * (n))

// car: usbd is bit 22 of the l bank
#define CAR_PHYS            0x60006000ull
#define CAR_RST_L           0x004
#define CAR_ENB_L           0x010
#define CAR_RST_L_CLR       0x304
#define CAR_ENB_L_SET       0x320
#define CAR_USBD_BIT        (1u << 22)

#define USBCMD_RS           (1u << 0)
#define USBCMD_RST          (1u << 1)
#define USBCMD_SUTW         (1u << 13)
#define USBSTS_UI           (1u << 0)
#define USBSTS_UEI          (1u << 1)
#define USBSTS_PCI          (1u << 2)
#define USBSTS_URI          (1u << 6)
#define USBSTS_SLI          (1u << 8)
#define USBSTS_W1C          0x000001ffu
#define USBMODE_DEVICE      2u
#define USBMODE_SLOM        (1u << 3)	// setup lockout off: setups are read under the tripwire
#define USBMODE_SDIS        (1u << 4)	// stream disable
#define DEVICEADDR_ADVANCE  (1u << 24)	// the new address takes effect after the status stage
// usb_susp_ctrl in the tegra phy block
#define SUSP_CLR            (1u << 5)
#define SUSP_PHY_CLK_VALID  (1u << 7)

// utmip pad registers of usb1, their power downs are cleared at power on
#define UTMIP_VBUS_SENSORS  0x404
#define UTMIP_VBUS_WAKEUP   0x408
#define UTMIP_XCVR_CFG0     0x808
#define UTMIP_BIAS_CFG0     0x80c
#define UTMIP_TX_CFG0       0x820
#define UTMIP_MISC_CFG0     0x824
#define UTMIP_BAT_CHRG_CFG0 0x830
#define UTMIP_XCVR_CFG1     0x838
#define XCVR0_FORCE_PD      ((1u << 14) | (1u << 16) | (1u << 18))
#define XCVR1_FORCE_PD      ((1u << 0) | (1u << 2) | (1u << 4))
#define BIAS_PD             ((1u << 10) | (1u << 11))
#define BAT_CHRG_PD         (1u << 0)

// tegra moved the port speed from portsc to hostpc1_devlc
#define DEVLC_PSPD(v)       (((v) >> 25) & 3)

// pmc_scratch0 bit 1 sends the next reset into rcm, through the firmware's sip call
#define TEGRA_SIP_PMC       0xc2fffe00ull
#define TEGRA_SIP_READ      0xaa
#define TEGRA_SIP_WRITE     0xbb
#define TEGRA_PMC_SCRATCH0  0x50

#define EPCTRL_RXS          (1u << 0)
#define EPCTRL_RXT(t)       ((uint32_t)(t) << 2)
#define EPCTRL_RXR          (1u << 6)
#define EPCTRL_RXE          (1u << 7)
#define EPCTRL_TXS          (1u << 16)
#define EPCTRL_TXT(t)       ((uint32_t)(t) << 18)
#define EPCTRL_TXR          (1u << 22)
#define EPCTRL_TXE          (1u << 23)

#define QH_IOS              (1u << 15)	// interrupt on setup (ep0 out)
#define QH_MAXPKT(n)        ((uint32_t)(n) << 16)
#define QH_ZLT              (1u << 29)	// no automatic zero length termination
#define TD_TERMINATE        1u
#define TD_ACTIVE           (1u << 7)
#define TD_HALTED           (1u << 6)
#define TD_ERRORS           ((1u << 6) | (1u << 5) | (1u << 3))
#define TD_IOC              (1u << 15)
#define TD_BYTES(t)         (((t) >> 16) & 0x7fff)
#define EP0_MPS     64


static void
barrier(void)
{
	__asm__ volatile ("dsb sy" ::: "memory");
}

void
PDTegraUDC::rebootToMaskrom(void)
{
	// recovery mode: the rcm loop sends the next boot from the host
	register uint64_t x0 __asm__("x0") = TEGRA_SIP_PMC;
	register uint64_t x1 __asm__("x1") = TEGRA_SIP_READ;
	register uint64_t x2 __asm__("x2") = TEGRA_PMC_SCRATCH0;
	register uint64_t x3 __asm__("x3") = 0;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
	x3 = (x0 == 0 ? x1 : 0) | (1u << 1);
	x0 = TEGRA_SIP_PMC;
	x1 = TEGRA_SIP_WRITE;
	x2 = TEGRA_PMC_SCRATCH0;
	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
	IOLog("PDTegraUDC: resetting into recovery mode\n");
	PEHaltRestart(kPERestartCPU);
}

void *
PDTegraUDC::dmaAlloc(uint32_t size, uint32_t align, uint32_t *phys)
{
	uint32_t off = (fDMAUsed + align - 1) & ~(align - 1);

	if (off + size > kPoolSize) {
		return NULL;
	}
	fDMAUsed = off + size;
	*phys = fDMAPhys + off;
	bzero(fDMABase + off, size);
	return fDMABase + off;
}

// page aligned, so a descriptor's five page pointers cover 20 KB of it
bool
PDTegraUDC::hwAllocEndpoint(Ep *ep, uint32_t bufferSize)
{
	Hw *h = hw(ep);

	if (ep->buf == NULL) {
		// an out stays one descriptor: a short packet ends a chain without its completion
		h->ntd = ep->in && bufferSize > kTdBytes ? (uint8_t)((bufferSize + kTdBytes - 1) / kTdBytes) : 1;
		ep->buf = (uint8_t *)dmaAlloc(bufferSize, 4096, &h->bufPhys);
		h->td = (volatile Td *)dmaAlloc(sizeof(Td) * h->ntd, 32, &h->tdPhys);
		if (ep->buf == NULL || h->td == NULL) {
			return false;
		}
	}
	return true;
}

// usbd clock on and out of reset before any core register is read
bool
PDTegraUDC::clockUp(void)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(CAR_PHYS, 0x1000, kIODirectionInOut);
	IOMemoryMap *map = md != NULL ? md->map(kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(md);
	if (map == NULL) {
		IOLog("PDTegraUDC: cannot map the car\n");
		return false;
	}
	volatile uint32_t *car = (volatile uint32_t *)map->getVirtualAddress();
	uint32_t enb = car[CAR_ENB_L / 4], rst = car[CAR_RST_L / 4];

	IOLog("PDTegraUDC: car enb_l 0x%08x rst_l 0x%08x, usbd clock %s, reset %s\n", enb, rst,
	    (enb & CAR_USBD_BIT) ? "on" : "off", (rst & CAR_USBD_BIT) ? "asserted" : "released");
	if ((enb & CAR_USBD_BIT) == 0) {
		car[CAR_ENB_L_SET / 4] = CAR_USBD_BIT;
		IODelay(2);
	}
	if (rst & CAR_USBD_BIT) {
		car[CAR_RST_L_CLR / 4] = CAR_USBD_BIT;
		IODelay(2);
	}
	enb = car[CAR_ENB_L / 4];
	rst = car[CAR_RST_L / 4];
	map->release();
	IOLog("PDTegraUDC: car now enb_l 0x%08x rst_l 0x%08x\n", enb, rst);
	return (enb & CAR_USBD_BIT) != 0 && (rst & CAR_USBD_BIT) == 0;
}

void
PDTegraUDC::utmipDump(const char *when)
{
	IOLog("PDTegraUDC: utmip %s: vbus_sensors 0x%08x wakeup_id 0x%08x xcvr0 0x%08x xcvr1 0x%08x bias0 0x%08x "
	    "tx0 0x%08x misc0 0x%08x batchrg 0x%08x\n", when, reg(UTMIP_VBUS_SENSORS), reg(UTMIP_VBUS_WAKEUP),
	    reg(UTMIP_XCVR_CFG0), reg(UTMIP_XCVR_CFG1), reg(UTMIP_BIAS_CFG0), reg(UTMIP_TX_CFG0), reg(UTMIP_MISC_CFG0),
	    reg(UTMIP_BAT_CHRG_CFG0));
}

void
PDTegraUDC::utmipPowerUp(void)
{
	utmipDump("as found");
	setReg(UTMIP_XCVR_CFG0, reg(UTMIP_XCVR_CFG0) & ~XCVR0_FORCE_PD);
	setReg(UTMIP_XCVR_CFG1, reg(UTMIP_XCVR_CFG1) & ~XCVR1_FORCE_PD);
	setReg(UTMIP_BIAS_CFG0, reg(UTMIP_BIAS_CFG0) & ~BIAS_PD);
	setReg(UTMIP_BAT_CHRG_CFG0, reg(UTMIP_BAT_CHRG_CFG0) & ~BAT_CHRG_PD);
	IODelay(10);
	utmipDump("powered");
}

// first 60 s after run: log port state whenever it changes
void
PDTegraUDC::watchPort(void)
{
	if (fWatchLeft == 0) return;
	uint64_t now = mach_absolute_time(), step;
	nanoseconds_to_absolutetime(500 * NSEC_PER_MSEC, &step);
	if (now - fWatchLast < step) return;
	fWatchLast = now;
	fWatchLeft--;
	uint32_t cmd = reg(kUSBCMD), st = reg(kUSBSTS), portsc = reg(kPORTSC), otgsc = reg(kOTGSC), devlc = reg(kHOSTPC1_DEVLC);
	uint32_t susp = reg(kUSB_SUSP_CTRL), vbus = reg(UTMIP_VBUS_SENSORS), misc = reg(UTMIP_MISC_CFG0);
	uint32_t key = cmd ^ (st * 3) ^ (portsc * 5) ^ (otgsc * 7) ^ (devlc * 11) ^ (susp * 13) ^ (vbus * 17) ^ (misc * 19);
	if (key == fWatchKey && fWatchLeft % 20 != 0) return;
	fWatchKey = key;
	IOLog("PDTegraUDC: watch usbcmd 0x%08x usbsts 0x%08x portsc 0x%08x otgsc 0x%08x devlc 0x%08x susp 0x%08x vbus 0x%08x misc 0x%08x\n",
	    cmd, st, portsc, otgsc, devlc, susp, vbus, misc);
}

bool
PDTegraUDC::initCore(void)
{
	uint32_t n;

	if (!clockUp()) return false;
	IOLog("PDTegraUDC: core id 0x%08x, hwdevice 0x%08x, dccparams 0x%08x\n", reg(kID), reg(kHWDEVICE),
	    reg(kDCCPARAMS));
	if ((reg(kID) & 0x3f) != ((~reg(kID) >> 8) & 0x3f) || (reg(kDCCPARAMS) & (1u << 7)) == 0) {
		IOLog("PDTegraUDC: not a device-capable ChipIdea core\n");
		return false;
	}
	// the operational registers stall the bus while the utmi phy clock is stopped, which
	// u-boot leaves it as at booti. a suspend clear pulse restarts it
	uint32_t susp = reg(kUSB_SUSP_CTRL);
	IOLog("PDTegraUDC: susp_ctrl 0x%08x\n", susp);
	if ((susp & SUSP_PHY_CLK_VALID) == 0) {
		setReg(kUSB_SUSP_CTRL, susp | SUSP_CLR);
		IODelay(10);
		setReg(kUSB_SUSP_CTRL, susp & ~SUSP_CLR);
		for (n = 0; n < 100000 && (reg(kUSB_SUSP_CTRL) & SUSP_PHY_CLK_VALID) == 0; n++) {
			IODelay(1);
		}
		IOLog("PDTegraUDC: phy clock %s, susp_ctrl 0x%08x\n",
		    (reg(kUSB_SUSP_CTRL) & SUSP_PHY_CLK_VALID) != 0 ? "running" : "still stopped", reg(kUSB_SUSP_CTRL));
		if ((reg(kUSB_SUSP_CTRL) & SUSP_PHY_CLK_VALID) == 0) return false;
	}
	utmipPowerUp();
	IOLog("PDTegraUDC: usbcmd 0x%08x portsc 0x%08x devlc 0x%08x\n", reg(kUSBCMD), reg(kPORTSC), reg(kHOSTPC1_DEVLC));
	// off the bus, then a controller reset (the utmi phy keeps what u-boot set up)
	setReg(kUSBCMD, reg(kUSBCMD) & ~USBCMD_RS);
	IOSleep(10);
	setReg(kUSBCMD, reg(kUSBCMD) | USBCMD_RST);
	for (n = 0; n < 1000 && (reg(kUSBCMD) & USBCMD_RST) != 0; n++) {
		IODelay(10);
	}
	if (reg(kUSBCMD) & USBCMD_RST) {
		IOLog("PDTegraUDC: reset stuck\n");
		return false;
	}
	IOLog("PDTegraUDC: core reset done, susp_ctrl 0x%08x devlc 0x%08x\n", reg(kUSB_SUSP_CTRL), reg(kHOSTPC1_DEVLC));
	// pdusbsdis=0 turns stream mode back on, for measuring the bus
	uint32_t sdis = 1;
	PE_parse_boot_argn("pdusbsdis", &sdis, sizeof(sdis));
	setReg(kUSBMODE, USBMODE_DEVICE | USBMODE_SLOM | (sdis ? USBMODE_SDIS : 0));
	for (n = 0; n < 1000 && (reg(kUSBMODE) & 3) != USBMODE_DEVICE; n++) {
		IODelay(10);
	}
	IOLog("PDTegraUDC: usbmode 0x%08x\n", reg(kUSBMODE));
	setReg(kUSBINTR, 0);
	setReg(kUSBSTS, ~0u);
	setReg(kENDPTSETUPSTAT, reg(kENDPTSETUPSTAT));
	setReg(kENDPTCOMPLETE, reg(kENDPTCOMPLETE));
	setReg(kENDPTLISTADDR, fQhPhys);
	setHighSpeed(true);
	coreBusReset();
	barrier();
	setReg(kUSBCMD, (reg(kUSBCMD) & ~(0xffu << 16)) | USBCMD_RS);
	IOLog("PDTegraUDC: usbcmd after rs 0x%08x usbsts 0x%08x\n", reg(kUSBCMD), reg(kUSBSTS));
	IOLog("PDTegraUDC: device mode running, USBMODE 0x%08x PORTSC 0x%08x OTGSC 0x%08x DEVLC 0x%08x\n",
	    reg(kUSBMODE), reg(kPORTSC), reg(kOTGSC), reg(kHOSTPC1_DEVLC));
	return true;
}

void
PDTegraUDC::flush(uint32_t bits)
{
	for (uint32_t tries = 0; tries < 100; tries++) {
		setReg(kENDPTFLUSH, bits);
		for (uint32_t n = 0; n < 1000 && (reg(kENDPTFLUSH) & bits) != 0; n++) {
			IODelay(1);
		}
		if ((reg(kENDPTSTAT) & bits) == 0) {
			return;
		}
	}
}

// the core's side of a bus reset: no setups or completions left, address 0, ep0 both ways
void
PDTegraUDC::coreBusReset(void)
{
	setReg(kENDPTSETUPSTAT, reg(kENDPTSETUPSTAT));
	setReg(kENDPTCOMPLETE, reg(kENDPTCOMPLETE));
	for (uint32_t n = 0; n < 1000 && reg(kENDPTPRIME) != 0; n++) {
		IODelay(1);
	}
	flush(~0u);
	setReg(kDEVICEADDR, 0);
	busReset();
	// the out queue head interrupts on setup
	volatile Qh *o = qh(0, false), *i = qh(0, true);
	o->config = QH_IOS | QH_MAXPKT(EP0_MPS) | QH_ZLT;
	o->next = TD_TERMINATE;
	o->token = 0;
	i->config = QH_MAXPKT(EP0_MPS) | QH_ZLT;
	i->next = TD_TERMINATE;
	i->token = 0;
	barrier();
}

void
PDTegraUDC::portChange(void)
{
	setHighSpeed(DEVLC_PSPD(reg(kHOSTPC1_DEVLC)) == 2);
	IOLog("PDTegraUDC: connected, %s speed\n", fHighSpeed ? "high" : "full");
}

// kTdBytes per descriptor, each starting on a page of the page-aligned buffer
bool
PDTegraUDC::hwQueue(Ep *ep, uint32_t offset, uint32_t length)
{
	Hw *h = hw(ep);
	volatile Qh *q = qh(ep->num, ep->in);
	uint32_t n = h->ntd > 1 ? (length + kTdBytes - 1) / kTdBytes : 1;

	if (n == 0) {
		n = 1;
	}
	for (uint32_t k = 0; k < n; k++) {
		volatile Td *td = &h->td[k];
		uint32_t start = h->bufPhys + offset + k * kTdBytes;
		uint32_t len = k + 1 < n ? kTdBytes : length - k * kTdBytes;

		td->next = k + 1 < n ? h->tdPhys + (uint32_t)sizeof(Td) * (k + 1) : TD_TERMINATE;
		td->token = (len << 16) | (k + 1 == n ? TD_IOC : 0) | TD_ACTIVE;
		for (uint32_t p = 0; p < 5; p++) {
			td->page[p] = p == 0 ? start : ((start & ~0xfffu) + 0x1000 * p);
		}
	}
	h->tdUsed = (uint8_t)n;
	q->next = h->tdPhys;
	q->token &= ~(TD_ACTIVE | TD_HALTED);
	fLastXfer = mach_absolute_time();
	barrier();
	setReg(kENDPTPRIME, bit(ep->num, ep->in));
	return true;
}

uint32_t
PDTegraUDC::leftBytes(Ep *ep)
{
	Hw *h = hw(ep);
	uint32_t left = 0;

	for (uint8_t k = 0; k < (h->tdUsed ? h->tdUsed : 1); k++) {
		left += TD_BYTES(h->td[k].token);
	}
	return left;
}

void
PDTegraUDC::hwEnableEndpoint(Ep *ep)
{
	volatile Qh *q = qh(ep->num, ep->in);

	q->config = QH_MAXPKT(ep->maxPacket) | QH_ZLT;
	q->next = TD_TERMINATE;
	q->token = 0;
	barrier();
	uint32_t ctl = reg(ENDPTCTRL(ep->num));
	if (ep->in) {
		ctl = (ctl & 0x0000ffffu) | EPCTRL_TXE | EPCTRL_TXR | EPCTRL_TXT(ep->type);
	} else {
		ctl = (ctl & 0xffff0000u) | EPCTRL_RXE | EPCTRL_RXR | EPCTRL_RXT(ep->type);
	}
	setReg(ENDPTCTRL(ep->num), ctl);
}

void
PDTegraUDC::hwDisableEndpoint(Ep *ep)
{
	flush(bit(ep->num, ep->in));
	uint32_t ctl = reg(ENDPTCTRL(ep->num));
	setReg(ENDPTCTRL(ep->num), ep->in ? (ctl & ~EPCTRL_TXE) : (ctl & ~EPCTRL_RXE));
}

void
PDTegraUDC::hwEp0Stall(void)
{
	// both directions, cleared by the core when the next setup arrives
	setReg(ENDPTCTRL(0), reg(ENDPTCTRL(0)) | EPCTRL_TXS | EPCTRL_RXS);
}

// latched now, applied by the core after the status stage
void
PDTegraUDC::hwSetAddress(uint8_t address)
{
	setReg(kDEVICEADDR, ((uint32_t)(address & 0x7f) << 25) | DEVICEADDR_ADVANCE);
}

// unstall and reset the data toggle
void
PDTegraUDC::hwClearHalt(uint8_t num, bool in)
{
	if (num >= kHwEndpoints) {
		return;
	}
	uint32_t ctl = reg(ENDPTCTRL(num));
	setReg(ENDPTCTRL(num), in ? ((ctl & ~EPCTRL_TXS) | EPCTRL_TXR) : ((ctl & ~EPCTRL_RXS) | EPCTRL_RXR));
}

// a setup on ep0: read it under the tripwire, drop what ep0 still had queued, hand it up
void
PDTegraUDC::readSetup(void)
{
	PDUSBSetup s;
	volatile Qh *q = qh(0, false);

	do {
		setReg(kUSBCMD, reg(kUSBCMD) | USBCMD_SUTW);
		for (uint32_t i = 0; i < sizeof(s); i++) {
			((uint8_t *)&s)[i] = q->setup[i];
		}
	} while ((reg(kUSBCMD) & USBCMD_SUTW) == 0);
	setReg(kUSBCMD, reg(kUSBCMD) & ~USBCMD_SUTW);
	setReg(kENDPTSETUPSTAT, 1u);
	flush(bit(0, false) | bit(0, true));
	setupReceived(&s);
}

void
PDTegraUDC::completions(void)
{
	uint32_t done = reg(kENDPTCOMPLETE), ack;

	if (done == 0) {
		return;
	}
	// a bit stays set until its descriptor is retired: one still reading active is seen again
	ack = done & (bit(0, false) | bit(0, true));
	for (uint8_t i = 0; i < fEpCount; i++) {
		Ep *ep = &fEps[i];
		uint32_t b = bit(ep->num, ep->in);

		if ((done & b) != 0 && (!ep->enabled || !ep->busy || (lastTd(ep)->token & TD_ACTIVE) == 0)) {
			ack |= b;
		}
	}
	setReg(kENDPTCOMPLETE, ack);
	barrier();
	if ((ack & bit(0, false)) && fEp0Out.busy) {
		ep0Done(false, fEp0Out.requested - leftBytes(&fEp0Out));
	}
	if ((ack & bit(0, true)) && fEp0In.busy) {
		ep0Done(true, fEp0In.requested - leftBytes(&fEp0In));
	}
	for (uint8_t i = 0; i < fEpCount; i++) {
		Ep *ep = &fEps[i];

		if (!ep->enabled || !ep->busy || (ack & bit(ep->num, ep->in)) == 0) {
			continue;
		}
		uint32_t token = lastTd(ep)->token;
		if (token & TD_ACTIVE) {
			continue;
		}
		fLastXfer = mach_absolute_time();
		if ((token & TD_ERRORS) && fTrace > 0) {
			IOLog("PDTegraUDC: ep %u%s token 0x%08x\n", ep->num, ep->in ? "in" : "out", token);
		}
		transferDone(ep, ep->requested - leftBytes(ep));
	}
}

void
PDTegraUDC::pollLoop(void)
{
	uint32_t idle = 0;

	while (fRunning) {
		uint32_t seen = fKicks;
		uint32_t st = reg(kUSBSTS) & USBSTS_W1C, handled = 0;

		if (st != 0) {
			setReg(kUSBSTS, st);
		}
		watchPort();
		if (st & USBSTS_URI) {
			if (fTrace > 0) {
				IOLog("PDTegraUDC: bus reset\n");
			}
			coreBusReset();
			handled++;
		}
		if (st & USBSTS_PCI) {
			portChange();
			handled++;
		}
		if (reg(kENDPTSETUPSTAT) & 1u) {
			readSetup();
			handled++;
		}
		if (reg(kENDPTCOMPLETE) != 0) {
			completions();
			handled++;
		}
		pollFunctions();
		// completions are polled: while an in is in flight keep looking, but an in nobody reads
		// (the console with no terminal open) must not pin a cpu for longer than 2 ms
		bool inflight = inFlight();
		if (inflight) {
			uint64_t limit;
			nanoseconds_to_absolutetime(2 * NSEC_PER_MSEC, &limit);
			inflight = mach_absolute_time() - fLastXfer < limit;
		}
		idle = handled != 0 || inflight ? 0 : idle + 1;
		if (idle > 64) {
			assert_wait_timeout((event_t)&fKicks, THREAD_UNINT, 1, kMillisecondScale);
			// a kick since this pass began: end our own wait so the block returns at once
			if (seen != fKicks) {
				thread_wakeup((event_t)&fKicks);
			}
			thread_block(THREAD_CONTINUE_NULL);
			idle = 0;
		}
	}
}

void
PDTegraUDC::kick(void)
{
	OSIncrementAtomic((SInt32 *)&fKicks);
	thread_wakeup((event_t)&fKicks);
}

void
PDTegraUDC::pollThread(void *arg, wait_result_t wr)
{
	PDTegraUDC *self = (PDTegraUDC *)arg;

	(void)wr;
	if (self->initCore()) {
		self->pollLoop();
	}
	self->release();
	thread_terminate(current_thread());
}

bool
PDTegraUDC::start(IOService *provider)
{
	thread_t th;
	uint32_t off = 0, ep0Phys;

	if (!super::start(provider)) {
		return false;
	}
	if (PE_parse_boot_argn("pdusboff", &off, sizeof(off)) && off != 0) {
		return false;
	}
	// the core's window from the device tree, as published by the platform expert
	fRegsMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	if (fRegsMap == NULL) {
		IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(CI_PHYS, CI_SIZE, kIODirectionInOut);
		fRegsMap = md != NULL ? md->map(kIOMapInhibitCache) : NULL;
		OSSafeReleaseNULL(md);
	}
	if (fRegsMap == NULL) {
		IOLog("PDTegraUDC: cannot map registers\n");
		return false;
	}
	fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();

	// one physically contiguous pool below 4 GB, used through an uncached mapping only. the loader
	// hands xnu only the bank above 4 GB, so pdudcpool=PHYS names a free spot in the low bank
	uint64_t pool = 0;
	if (PE_parse_boot_argn("pdudcpool", &pool, sizeof(pool)) && pool != 0 && pool + kPoolSize <= 0x100000000ULL) {
		fDMAPhys = (uint32_t)pool;
	} else {
		fDMA = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
		    kIODirectionInOut | kIOMemoryPhysicallyContiguous, kPoolSize, 0xfffff000ULL);
		if (fDMA == NULL) {
			IOLog("PDTegraUDC: no memory below 4 GB for the DMA pool, set pdudcpool\n");
			return false;
		}
		fDMAPhys = (uint32_t)fDMA->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
		fDMA->performOperation(kIOMemoryIncoherentIOFlush, 0, fDMA->getLength());
	}
	fDMAPhysMD = IOMemoryDescriptor::withPhysicalAddress(fDMAPhys, kPoolSize, kIODirectionInOut);
	fDMAMap = fDMAPhysMD != NULL ? fDMAPhysMD->map(kIOMapWriteCombineCache) : NULL;
	if (fDMAMap == NULL) {
		IOLog("PDTegraUDC: cannot map the DMA pool at 0x%08x\n", fDMAPhys);
		return false;
	}
	fDMABase = (uint8_t *)fDMAMap->getVirtualAddress();
	// the queue head list is 2 KB aligned, 16 heads for 8 endpoints both ways
	fQh = (volatile Qh *)dmaAlloc(sizeof(Qh) * kHwEndpoints * 2, 2048, &fQhPhys);
	uint8_t *ep0Buf = (uint8_t *)dmaAlloc(kEp0Buffer, 4096, &ep0Phys);
	if (fQh == NULL || ep0Buf == NULL || !initGadget(ep0Buf, "PureDarwin Jetson TX1")) {
		IOLog("PDTegraUDC: gadget setup failed\n");
		return false;
	}
	// ep0 shares one buffer both ways, each direction its own descriptor
	Ep *ep0[2] = { &fEp0Out, &fEp0In };
	for (int i = 0; i < 2; i++) {
		Hw *h = hw(ep0[i]);
		h->bufPhys = ep0Phys;
		h->ntd = 1;
		h->td = (volatile Td *)dmaAlloc(sizeof(Td), 32, &h->tdPhys);
		if (h->td == NULL) {
			return false;
		}
	}
	IOLog("PDTegraUDC: registers at %p, DMA pool phys 0x%08x\n", fRegs, fDMAPhys);

	// no display driver yet either, so the pdfb stream is on unless pdusbfb=0
	if (!addStandardFunctions(true)) {
		return false;
	}
	registerService();
	fRunning = true;
	retain();
	if (kernel_thread_start(&PDTegraUDC::pollThread, this, &th) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(th);
	return true;
}

void
PDTegraUDC::free(void)
{
	OSSafeReleaseNULL(fDMAMap);
	OSSafeReleaseNULL(fDMAPhysMD);
	OSSafeReleaseNULL(fDMA);
	OSSafeReleaseNULL(fRegsMap);
	super::free();
}
