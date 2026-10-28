// SPDX-License-Identifier: MIT
// msm8916 chipidea device controller, the hardware under PDGadgetController
#include "PDQcomCIUDC.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <kern/thread.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

#define super PDGadgetController
OSDefineMetaClassAndStructors(PDQcomCIUDC, PDGadgetController);

// the core when the platform expert gave no "reg" (msm8916 usb@78d9000)
#define CI_PHYS 0x078d9000u
#define CI_SIZE 0x200u

// chipIdea registers, from the identification block at 0 to the endpoint controls
enum {
	kID = 0x000, kHWDEVICE = 0x00c, kSBUSCFG = 0x090,
	kGENCONFIG2 = 0x0a0,	// a qualcomm addition to the core
	kDCCPARAMS = 0x124,
	kUSBCMD = 0x140, kUSBSTS = 0x144, kUSBINTR = 0x148, kDEVICEADDR = 0x154, kENDPTLISTADDR = 0x158,
	kULPIVIEWPORT = 0x170,
	kPORTSC = 0x184, kOTGSC = 0x1a4, kUSBMODE = 0x1a8,
	kENDPTSETUPSTAT = 0x1ac, kENDPTPRIME = 0x1b0, kENDPTFLUSH = 0x1b4, kENDPTSTAT = 0x1b8,
	kENDPTCOMPLETE = 0x1bc,
};
#define ENDPTCTRL(n) (0x1c0 + 4 * (n))

#define USBCMD_RS           (1u << 0)
#define USBCMD_RST          (1u << 1)
#define USBCMD_SUTW         (1u << 13)
#define USBCMD_SESS_VLD     (1u << 25)	// qualcomm: VBUS session valid from software, not the PHY
#define USBSTS_UI           (1u << 0)
#define USBSTS_UEI          (1u << 1)
#define USBSTS_PCI          (1u << 2)
#define USBSTS_URI          (1u << 6)
#define USBSTS_SLI          (1u << 8)
#define USBMODE_DEVICE      2u
#define USBMODE_SLOM        (1u << 3)	// setup lockout off: setups are read under the tripwire
#define USBMODE_SDIS        (1u << 4)	// stream disable, stream mode on measured ~10 MB/s here
#define DEVICEADDR_ADVANCE  (1u << 24)	// the new address takes effect after the status stage
#define PORTSC_PSPD(v)      (((v) >> 26) & 3)
#define ULPI_RUN            (1u << 30)
#define ULPI_WRITE          (1u << 29)
#define ULPI_ADDR(a)        ((uint32_t)(a) << 16)
// the PHY's VBUS valid from software: nothing on this board wires VBUS to it
#define ULPI_MISC_A_SET     0x97
#define ULPI_MISC_A_VBUSVLDEXTSEL (1u << 1)
#define ULPI_MISC_A_VBUSVLDEXT    (1u << 0)
#define GENCONFIG2_SESS_VLD_CTRL_EN     (1u << 7)

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
PDQcomCIUDC::rebootToMaskrom(void)
{
	// back to lk2nd, the stick's way to be flashed
	IOLog("PDQcomCIUDC: resetting into the boot loader\n");
	PEHaltRestart(kPERestartCPU);
}

void *
PDQcomCIUDC::dmaAlloc(uint32_t size, uint32_t align, uint32_t *phys)
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
PDQcomCIUDC::hwAllocEndpoint(Ep *ep, uint32_t bufferSize)
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

// a PHY register through the core's ULPI viewport
static bool
ulpiWrite(volatile uint8_t *regs, uint8_t addr, uint8_t value)
{
	volatile uint32_t *vp = (volatile uint32_t *)(regs + kULPIVIEWPORT);

	*vp = ULPI_RUN | ULPI_WRITE | ULPI_ADDR(addr) | value;
	for (uint32_t n = 0; n < 10000; n++) {
		if ((*vp & ULPI_RUN) == 0) {
			return true;
		}
		IODelay(1);
	}
	return false;
}

bool
PDQcomCIUDC::initCore(void)
{
	uint32_t n;

	IOLog("PDQcomCIUDC: core id 0x%08x, hwdevice 0x%08x, dccparams 0x%08x\n", reg(kID), reg(kHWDEVICE),
	    reg(kDCCPARAMS));
	if ((reg(kID) & 0x3f) != ((~reg(kID) >> 8) & 0x3f) || (reg(kDCCPARAMS) & (1u << 7)) == 0) {
		IOLog("PDQcomCIUDC: not a device-capable ChipIdea core\n");
		return false;
	}
	// off the bus, then a controller reset (the PHY keeps what lk2nd set up)
	setReg(kUSBCMD, reg(kUSBCMD) & ~USBCMD_RS);
	IOSleep(10);
	setReg(kUSBCMD, reg(kUSBCMD) | USBCMD_RST);
	for (n = 0; n < 1000 && (reg(kUSBCMD) & USBCMD_RST) != 0; n++) {
		IODelay(10);
	}
	if (reg(kUSBCMD) & USBCMD_RST) {
		IOLog("PDQcomCIUDC: reset stuck\n");
		return false;
	}
	// the reset already leaves the ulpi phy type and ahb mode this board runs with (observations/ci-regs.txt)
	// pdusbburst and pdusbsdis=0 are for measuring the bus: burst config, stream mode back on
	uint32_t burst = 0, sdis = 1;
	PE_parse_boot_argn("pdusbburst", &burst, sizeof(burst));
	PE_parse_boot_argn("pdusbsdis", &sdis, sizeof(sdis));
	setReg(kSBUSCFG, burst);		// the board dt has ahb-burst-config = <0>
	setReg(kUSBMODE, USBMODE_DEVICE | USBMODE_SLOM | (sdis ? USBMODE_SDIS : 0));
	for (n = 0; n < 1000 && (reg(kUSBMODE) & 3) != USBMODE_DEVICE; n++) {
		IODelay(10);
	}
	// nothing here reports VBUS: claim a valid session to the PHY and the link both, as
	// lk's msm8916 usb init does for its own fastboot
	if (!ulpiWrite(fRegs, ULPI_MISC_A_SET, ULPI_MISC_A_VBUSVLDEXTSEL | ULPI_MISC_A_VBUSVLDEXT)) {
		IOLog("PDQcomCIUDC: ULPI write timed out\n");
	}
	setReg(kGENCONFIG2, reg(kGENCONFIG2) | GENCONFIG2_SESS_VLD_CTRL_EN);
	setReg(kUSBINTR, 0);
	setReg(kUSBSTS, ~0u);
	setReg(kENDPTSETUPSTAT, reg(kENDPTSETUPSTAT));
	setReg(kENDPTCOMPLETE, reg(kENDPTCOMPLETE));
	setReg(kENDPTLISTADDR, fQhPhys);
	setHighSpeed(true);
	coreBusReset();
	barrier();
	setReg(kUSBCMD, (reg(kUSBCMD) & ~(0xffu << 16)) | USBCMD_SESS_VLD | USBCMD_RS);
	IOLog("PDQcomCIUDC: device mode running, USBMODE 0x%08x PORTSC 0x%08x OTGSC 0x%08x\n", reg(kUSBMODE),
	    reg(kPORTSC), reg(kOTGSC));
	return true;
}

void
PDQcomCIUDC::flush(uint32_t bits)
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
PDQcomCIUDC::coreBusReset(void)
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
PDQcomCIUDC::portChange(void)
{
	setHighSpeed(PORTSC_PSPD(reg(kPORTSC)) == 2);
	IOLog("PDQcomCIUDC: connected, %s speed\n", fHighSpeed ? "high" : "full");
}

// kTdBytes per descriptor, each starting on a page of the page-aligned buffer
bool
PDQcomCIUDC::hwQueue(Ep *ep, uint32_t offset, uint32_t length)
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
PDQcomCIUDC::leftBytes(Ep *ep)
{
	Hw *h = hw(ep);
	uint32_t left = 0;

	for (uint8_t k = 0; k < (h->tdUsed ? h->tdUsed : 1); k++) {
		left += TD_BYTES(h->td[k].token);
	}
	return left;
}

void
PDQcomCIUDC::hwEnableEndpoint(Ep *ep)
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
PDQcomCIUDC::hwDisableEndpoint(Ep *ep)
{
	flush(bit(ep->num, ep->in));
	uint32_t ctl = reg(ENDPTCTRL(ep->num));
	setReg(ENDPTCTRL(ep->num), ep->in ? (ctl & ~EPCTRL_TXE) : (ctl & ~EPCTRL_RXE));
}

void
PDQcomCIUDC::hwEp0Stall(void)
{
	// both directions, cleared by the core when the next setup arrives
	setReg(ENDPTCTRL(0), reg(ENDPTCTRL(0)) | EPCTRL_TXS | EPCTRL_RXS);
}

// latched now, applied by the core after the status stage
void
PDQcomCIUDC::hwSetAddress(uint8_t address)
{
	setReg(kDEVICEADDR, ((uint32_t)(address & 0x7f) << 25) | DEVICEADDR_ADVANCE);
}

// unstall and reset the data toggle
void
PDQcomCIUDC::hwClearHalt(uint8_t num, bool in)
{
	if (num >= kHwEndpoints) {
		return;
	}
	uint32_t ctl = reg(ENDPTCTRL(num));
	setReg(ENDPTCTRL(num), in ? ((ctl & ~EPCTRL_TXS) | EPCTRL_TXR) : ((ctl & ~EPCTRL_RXS) | EPCTRL_RXR));
}

// a setup on ep0: read it under the tripwire, drop what ep0 still had queued, hand it up
void
PDQcomCIUDC::readSetup(void)
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
PDQcomCIUDC::completions(void)
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
			IOLog("PDQcomCIUDC: ep %u%s token 0x%08x\n", ep->num, ep->in ? "in" : "out", token);
		}
		transferDone(ep, ep->requested - leftBytes(ep));
	}
}

void
PDQcomCIUDC::pollLoop(void)
{
	uint32_t idle = 0;

	while (fRunning) {
		uint32_t seen = fKicks;
		uint32_t st = reg(kUSBSTS), handled = 0;

		if (st != 0) {
			setReg(kUSBSTS, st);
		}
		if (st & USBSTS_URI) {
			if (fTrace > 0) {
				IOLog("PDQcomCIUDC: bus reset\n");
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
PDQcomCIUDC::kick(void)
{
	OSIncrementAtomic((SInt32 *)&fKicks);
	thread_wakeup((event_t)&fKicks);
}

void
PDQcomCIUDC::pollThread(void *arg, wait_result_t wr)
{
	PDQcomCIUDC *self = (PDQcomCIUDC *)arg;

	(void)wr;
	if (self->initCore()) {
		self->pollLoop();
	}
	self->release();
	thread_terminate(current_thread());
}

bool
PDQcomCIUDC::start(IOService *provider)
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
		return false;
	}
	fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();

	// one physically contiguous pool below 4 GB, used through an uncached mapping only
	fDMA = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous, kPoolSize, 0xfffff000ULL);
	if (fDMA == NULL) {
		return false;
	}
	fDMAPhys = (uint32_t)fDMA->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
	fDMA->performOperation(kIOMemoryIncoherentIOFlush, 0, fDMA->getLength());
	fDMAPhysMD = IOMemoryDescriptor::withPhysicalAddress(fDMAPhys, fDMA->getLength(), kIODirectionInOut);
	fDMAMap = fDMAPhysMD != NULL ? fDMAPhysMD->map(kIOMapWriteCombineCache) : NULL;
	if (fDMAMap == NULL) {
		return false;
	}
	fDMABase = (uint8_t *)fDMAMap->getVirtualAddress();
	// the queue head list is 2 KB aligned, 16 heads for 8 endpoints both ways
	fQh = (volatile Qh *)dmaAlloc(sizeof(Qh) * kHwEndpoints * 2, 2048, &fQhPhys);
	uint8_t *ep0Buf = (uint8_t *)dmaAlloc(kEp0Buffer, 4096, &ep0Phys);
	if (fQh == NULL || ep0Buf == NULL || !initGadget(ep0Buf, "PureDarwin msm8916")) {
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
	IOLog("PDQcomCIUDC: registers at %p, DMA pool phys 0x%08x\n", fRegs, fDMAPhys);

	// the stick has no display, so the pdfb stream is on unless pdusbfb=0
	if (!addStandardFunctions(true)) {
		return false;
	}
	registerService();
	fRunning = true;
	retain();
	if (kernel_thread_start(&PDQcomCIUDC::pollThread, this, &th) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(th);
	return true;
}

void
PDQcomCIUDC::free(void)
{
	OSSafeReleaseNULL(fDMAMap);
	OSSafeReleaseNULL(fDMAPhysMD);
	OSSafeReleaseNULL(fDMA);
	OSSafeReleaseNULL(fRegsMap);
	super::free();
}
