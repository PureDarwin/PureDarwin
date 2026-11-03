#include "PDRockchipDWC2.h"
#include "PDGadgetACM.h"
#include "PDGadgetADB.h"

#include <IOKit/IOLib.h>
#include <kern/thread.h>
#include <pexpert/pexpert.h>

#define super PDGadgetController
OSDefineMetaClassAndStructors(PDRockchipDWC2, PDGadgetController);

#define DWC2_PHYS   0xff740000u	// usb otg0, the port the lyra's usb-c connector reaches
#define DWC2_SIZE   0x20000u	// registers, then a 4 KB fifo window per endpoint

// core and device registers
enum {
	kGAHBCFG = 0x008, kGUSBCFG = 0x00c, kGRSTCTL = 0x010, kGINTSTS = 0x014, kGINTMSK = 0x018,
	kGRXSTSP = 0x020, kGRXFSIZ = 0x024, kGNPTXFSIZ = 0x028, kGSNPSID = 0x040,
	kGHWCFG2 = 0x048, kGHWCFG3 = 0x04c, kGHWCFG4 = 0x050,
	kDCFG = 0x800, kDCTL = 0x804, kDSTS = 0x808, kDIEPMSK = 0x810, kDOEPMSK = 0x814,
	kDAINTMSK = 0x81c, kPCGCCTL = 0xe00,
};
#define DIEPTXF(n)  (0x100 + 4 * (n))
#define DIEPCTL(n)  (0x900 + 0x20 * (n))
#define DIEPINT(n)  (0x908 + 0x20 * (n))
#define DIEPTSIZ(n) (0x910 + 0x20 * (n))
#define DTXFSTS(n)  (0x918 + 0x20 * (n))
#define DOEPCTL(n)  (0xb00 + 0x20 * (n))
#define DOEPINT(n)  (0xb08 + 0x20 * (n))
#define DOEPTSIZ(n) (0xb10 + 0x20 * (n))
#define FIFO(n)     (0x1000 * ((n) + 1))

#define GRSTCTL_CSFTRST     (1u << 0)
#define GRSTCTL_RXFFLSH     (1u << 4)
#define GRSTCTL_TXFFLSH     (1u << 5)
#define GRSTCTL_TXFNUM(n)   ((uint32_t)(n) << 6)
#define GRSTCTL_CSFTRSTDONE (1u << 29)
#define GRSTCTL_AHBIDLE     (1u << 31)
#define GUSBCFG_FORCEHOST   (1u << 29)
#define GUSBCFG_FORCEDEV    (1u << 30)
#define GINTSTS_RXFLVL      (1u << 4)
#define GINTSTS_USBSUSP     (1u << 11)
#define GINTSTS_USBRST      (1u << 12)
#define GINTSTS_ENUMDONE    (1u << 13)
#define DCTL_SFTDISCON      (1u << 1)
#define DCFG_DEVADDR(n)     ((uint32_t)(n) << 4)
#define DCFG_DEVADDR_MASK   (0x7fu << 4)

#define EPCTL_MPS_MASK      0x7ffu
#define EPCTL_USBACTEP      (1u << 15)
#define EPCTL_TYPE(n)       ((uint32_t)(n) << 18)
#define EPCTL_STALL         (1u << 21)
#define EPCTL_TXFNUM(n)     ((uint32_t)(n) << 22)
#define EPCTL_CNAK          (1u << 26)
#define EPCTL_SNAK          (1u << 27)
#define EPCTL_SETD0PID      (1u << 28)
#define EPCTL_EPDIS         (1u << 30)
#define EPCTL_EPENA         (1u << 31)
#define EPINT_XFERCOMPL     (1u << 0)
#define EPINT_SETUP         (1u << 3)
#define TSIZ_PKTCNT(n)      ((uint32_t)(n) << 19)
#define TSIZ_SUPCNT(n)      ((uint32_t)(n) << 29)

enum { kRxOutData = 2, kRxSetupData = 6 };

#define USB_VENDOR  0x1209	// pid.codes test range, as on the pico
#define USB_PRODUCT 0x0001
#define EP0_MPS     64

// the rk3506's reboot-mode register (PMU_GRF 0x200) and its cru
#define RK3506_PMUGRF_OSREG 0xff910200u
#define RK3506_CRU          0xff9a0000u

void
PDRockchipDWC2::rebootToMaskrom(void)
{
	romDownloadReset(RK3506_PMUGRF_OSREG, RK3506_CRU);
}

static uint32_t
minU32(uint32_t a, uint32_t b)
{
	return a < b ? a : b;
}

bool
PDRockchipDWC2::initCore(void)
{
	uint32_t id = reg(kGSNPSID), usbcfg, v = 0, n;

	IOLog("PDRockchipDWC2: core id 0x%08x, hwcfg 0x%08x 0x%08x 0x%08x\n", id,
	    reg(kGHWCFG2), reg(kGHWCFG3), reg(kGHWCFG4));
	if ((id >> 16) != 0x4f54) {
		return false;
	}
	// off the bus while the core resets, so the host sees u-boot's fastboot device leave
	setReg(kDCTL, reg(kDCTL) | DCTL_SFTDISCON);
	IOSleep(10);
	// phy width and turnaround as u-boot set them for fastboot
	usbcfg = reg(kGUSBCFG);
	for (n = 0; n < 1000 && (reg(kGRSTCTL) & GRSTCTL_AHBIDLE) == 0; n++) {
		IODelay(10);
	}
	setReg(kGRSTCTL, GRSTCTL_CSFTRST);
	for (n = 0; n < 1000; n++) {
		v = reg(kGRSTCTL);
		// older cores clear the bit when done, 4.20a and later raise a done bit instead
		if ((v & GRSTCTL_CSFTRST) == 0 || (v & GRSTCTL_CSFTRSTDONE) != 0) {
			break;
		}
		IODelay(10);
	}
	if (v & GRSTCTL_CSFTRSTDONE) {
		setReg(kGRSTCTL, (v & ~GRSTCTL_CSFTRST) | GRSTCTL_CSFTRSTDONE);
	} else if (v & GRSTCTL_CSFTRST) {
		IOLog("PDRockchipDWC2: soft reset stuck (no phy clock?)\n");
		return false;
	}
	for (n = 0; n < 1000 && (reg(kGRSTCTL) & GRSTCTL_AHBIDLE) == 0; n++) {
		IODelay(10);
	}
	setReg(kGUSBCFG, (usbcfg & ~GUSBCFG_FORCEHOST) | GUSBCFG_FORCEDEV);
	IOSleep(25);
	setReg(kPCGCCTL, 0);
	// slave mode with the interrupt line off: the thread polls every status register
	setReg(kGAHBCFG, 0);
	setReg(kGINTMSK, 0);
	setReg(kDCTL, reg(kDCTL) | DCTL_SFTDISCON);
	setReg(kDCFG, reg(kDCFG) & ~(0x3u | DCFG_DEVADDR_MASK));	// high speed, address 0
	sizeFifos();
	flushFifos();
	setReg(kDIEPMSK, 0);
	setReg(kDOEPMSK, 0);
	setReg(kDAINTMSK, 0);
	for (n = 0; n < 16; n++) {
		setReg(DIEPINT(n), ~0u);
		setReg(DOEPINT(n), ~0u);
	}
	setReg(kGINTSTS, ~0u);
	fHighSpeed = true;
	busReset();
	setReg(kDCTL, reg(kDCTL) & ~DCTL_SFTDISCON);
	IOLog("PDRockchipDWC2: device mode running, GUSBCFG 0x%08x DSTS 0x%08x\n", reg(kGUSBCFG), reg(kDSTS));
	return true;
}

void
PDRockchipDWC2::sizeFifos(void)
{
	// fifo ram in 32-bit words: rx for two high speed packets, ep0 in, then one per in endpoint
	uint32_t total = reg(kGHWCFG3) >> 16, rx = 0x120, np = 0x40, tx, addr;

	tx = total > rx + np ? (total - rx - np) / 3 : 0x10;
	tx = minU32(tx, 0x100) & ~0x3u;
	setReg(kGRXFSIZ, rx);
	setReg(kGNPTXFSIZ, (np << 16) | rx);
	addr = rx + np;
	for (uint32_t n = 1; n <= 3; n++) {
		setReg(DIEPTXF(n), (tx << 16) | addr);
		addr += tx;
	}
	IOLog("PDRockchipDWC2: fifo %u words: rx %u, ep0 in %u, in 1-3 %u each\n", total, rx, np, tx);
}

void
PDRockchipDWC2::flushFifos(void)
{
	uint32_t n;

	setReg(kGRSTCTL, GRSTCTL_TXFFLSH | GRSTCTL_TXFNUM(0x10));
	for (n = 0; n < 1000 && (reg(kGRSTCTL) & GRSTCTL_TXFFLSH) != 0; n++) {
		IODelay(1);
	}
	setReg(kGRSTCTL, GRSTCTL_RXFFLSH);
	for (n = 0; n < 1000 && (reg(kGRSTCTL) & GRSTCTL_RXFFLSH) != 0; n++) {
		IODelay(1);
	}
}

void
PDRockchipDWC2::busReset(void)
{
	setReg(kDCFG, reg(kDCFG) & ~DCFG_DEVADDR_MASK);
	clearConfiguration();
	flushFifos();
	fEp0In.num = fEp0Out.num = 0;
	fEp0In.in = true;
	fEp0Out.in = false;
	fEp0In.maxPacket = fEp0Out.maxPacket = EP0_MPS;
	fEp0In.enabled = fEp0Out.enabled = true;
	fEp0In.busy = fEp0Out.busy = false;
	fEp0Out.buf = fEp0Buf;
	fEp0Out.bufferSize = EP0_MPS;
	fEp0State = kSetup;
	setReg(DIEPCTL(0), reg(DIEPCTL(0)) & ~0x3u);	// 64 byte packets
	ep0ArmOut();
}

void
PDRockchipDWC2::enumDone(void)
{
	fHighSpeed = ((reg(kDSTS) >> 1) & 3) == 0;
	setReg(DIEPCTL(0), reg(DIEPCTL(0)) & ~0x3u);
	IOLog("PDRockchipDWC2: connected, %s speed\n", fHighSpeed ? "high" : "full");
}

// ep0 out is kept armed: it takes the next setup, a data stage or the status stage alike
void
PDRockchipDWC2::ep0ArmOut(void)
{
	fEp0Out.done = 0;
	setReg(DOEPTSIZ(0), TSIZ_SUPCNT(3) | TSIZ_PKTCNT(1) | EP0_MPS);
	setReg(DOEPCTL(0), reg(DOEPCTL(0)) | EPCTL_EPENA | EPCTL_CNAK);
}

void
PDRockchipDWC2::fillTxFifo(Ep *ep)
{
	volatile uint32_t *fifo = (volatile uint32_t *)(fRegs + FIFO(ep->num));

	// whole packets only, and only while the endpoint's fifo has room for one
	while (ep->done < ep->requested) {
		uint32_t n = minU32(ep->maxPacket, ep->requested - ep->done), words = (n + 3) / 4;
		const uint8_t *src = ep->buf + ep->done;

		if ((reg(DTXFSTS(ep->num)) & 0xffff) < words) {
			break;
		}
		for (uint32_t i = 0; i < words; i++) {
			uint32_t w = 0;

			for (uint32_t b = 0; b < 4 && i * 4 + b < n; b++) {
				w |= (uint32_t)src[i * 4 + b] << (8 * b);
			}
			*fifo = w;
		}
		ep->done += n;
	}
}

bool
PDRockchipDWC2::startIn(Ep *ep, uint32_t length)
{
	uint32_t pkts = length == 0 ? 1 : (length + ep->maxPacket - 1) / ep->maxPacket;

	ep->requested = length;
	ep->done = 0;
	ep->busy = true;
	setReg(DIEPTSIZ(ep->num), TSIZ_PKTCNT(pkts) | length);
	setReg(DIEPCTL(ep->num), reg(DIEPCTL(ep->num)) | EPCTL_EPENA | EPCTL_CNAK);
	fillTxFifo(ep);
	return true;
}

bool
PDRockchipDWC2::startOut(Ep *ep, uint32_t length)
{
	uint32_t pkts = length == 0 ? 1 : (length + ep->maxPacket - 1) / ep->maxPacket;

	ep->requested = length;
	ep->done = 0;
	ep->busy = true;
	setReg(DOEPTSIZ(ep->num), TSIZ_PKTCNT(pkts) | (pkts * ep->maxPacket));
	setReg(DOEPCTL(ep->num), reg(DOEPCTL(ep->num)) | EPCTL_EPENA | EPCTL_CNAK);
	return true;
}

// ep0 in stages go out from fEp0Buf, in one transfer when the 7-bit size field allows,
// otherwise a packet at a time
void
PDRockchipDWC2::ep0In(const uint8_t *data, uint32_t length)
{
	uint32_t left = length - fEp0InSent;

	if (data != NULL && data != fEp0Buf) {
		bcopy(data, fEp0Buf, length);
	}
	fEp0In.buf = fEp0Buf + fEp0InSent;
	startIn(&fEp0In, left <= 127 ? left : EP0_MPS);
	fEp0InSent += fEp0In.requested;
}

void
PDRockchipDWC2::ep0Stall(void)
{
	// both directions, cleared by the core on the next setup
	setReg(DIEPCTL(0), reg(DIEPCTL(0)) | EPCTL_STALL);
	setReg(DOEPCTL(0), reg(DOEPCTL(0)) | EPCTL_STALL);
	fEp0State = kSetup;
	ep0ArmOut();
}

void
PDRockchipDWC2::addFunction(PDGadgetFunction *fn)
{
	if (fFunctionCount < kMaxFunctions) {
		fFunctions[fFunctionCount++] = fn;
		fInterfaceCount += fn->interfaceCount();
	}
}

PDRockchipDWC2::Ep *
PDRockchipDWC2::endpointFor(PDGadgetFunction *fn, uint8_t index)
{
	for (uint8_t i = 0; i < fEpCount; i++) {
		if (fEps[i].fn == fn && fEps[i].index == index) {
			return &fEps[i];
		}
	}
	return NULL;
}

uint8_t *
PDRockchipDWC2::buffer(PDGadgetFunction *fn, uint8_t index)
{
	Ep *ep = endpointFor(fn, index);

	return ep != NULL ? ep->buf : NULL;
}

bool
PDRockchipDWC2::busy(PDGadgetFunction *fn, uint8_t index)
{
	Ep *ep = endpointFor(fn, index);

	return ep == NULL || !ep->enabled || ep->busy;
}

bool
PDRockchipDWC2::queue(PDGadgetFunction *fn, uint8_t index, uint32_t length)
{
	Ep *ep = endpointFor(fn, index);

	if (ep == NULL || !ep->enabled || ep->busy || length > ep->bufferSize) {
		return false;
	}
	return ep->in ? startIn(ep, length) : startOut(ep, length);
}

// device, configuration and interface descriptors, laid out as the dwc3 driver does
static const uint8_t sDeviceDescriptor[18] = {
	18, 1, 0x00, 0x02, 0xef, 0x02, 0x01, EP0_MPS,
	USB_VENDOR & 0xff, USB_VENDOR >> 8, USB_PRODUCT & 0xff, USB_PRODUCT >> 8,
	0x00, 0x01, 1, 2, 3, 1,
};
static const char *const sStrings[] = { NULL, "PureDarwin", "PureDarwin", "0001" };

uint32_t
PDRockchipDWC2::configDescriptor(uint8_t *out)
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
PDRockchipDWC2::setConfiguration(uint8_t value)
{
	uint8_t in = 1, out = 1;

	clearConfiguration();
	if (value == 0) {
		return true;
	}
	if (value != 1) {
		return false;
	}
	// endpoint slots keep their buffers across configurations: the functions never change
	for (uint8_t f = 0; f < fFunctionCount; f++) {
		PDGadgetFunction *fn = fFunctions[f];

		for (uint8_t i = 0; i < fn->endpointCount(); i++) {
			PDGadgetFunction::Endpoint d = fn->endpoint(i);
			Ep *ep;
			uint32_t ctl;

			if (fEpCount >= sizeof(fEps) / sizeof(fEps[0])) {
				return false;
			}
			ep = &fEps[fEpCount++];
			if (ep->buf == NULL) {
				ep->buf = (uint8_t *)IOMalloc(d.bufferSize);
				if (ep->buf == NULL) {
					return false;
				}
				ep->bufferSize = d.bufferSize;
			}
			ep->fn = fn;
			ep->index = i;
			ep->type = d.type;
			ep->in = d.in;
			ep->num = d.in ? in++ : out++;
			ep->maxPacket = (!fHighSpeed && d.type == kPDEpBulk && d.maxPacket > 64) ? 64 : d.maxPacket;
			ep->busy = false;
			ctl = ep->maxPacket | EPCTL_USBACTEP | EPCTL_TYPE(d.type) | EPCTL_SETD0PID | EPCTL_SNAK;
			if (d.in) {
				setReg(DIEPCTL(ep->num), ctl | EPCTL_TXFNUM(ep->num));
			} else {
				setReg(DOEPCTL(ep->num), ctl);
			}
			ep->enabled = true;
		}
	}
	fConfiguration = value;
	for (uint8_t f = 0, first = 0; f < fFunctionCount; f++) {
		fFunctions[f]->configured(this, first);
		first += fFunctions[f]->endpointCount();
	}
	IOLog("PDRockchipDWC2: configured (%s speed)\n", fHighSpeed ? "high" : "full");
	return true;
}

void
PDRockchipDWC2::clearConfiguration(void)
{
	if (fConfiguration != 0) {
		for (uint8_t f = 0; f < fFunctionCount; f++) {
			fFunctions[f]->deconfigured();
		}
	}
	for (uint8_t i = 0; i < fEpCount; i++) {
		Ep *ep = &fEps[i];
		uint32_t off = ep->in ? DIEPCTL(ep->num) : DOEPCTL(ep->num);
		uint32_t v = reg(off);

		if (ep->enabled) {
			setReg(off, (v & ~EPCTL_USBACTEP) | EPCTL_SNAK | ((v & EPCTL_EPENA) ? EPCTL_EPDIS : 0));
		}
		ep->enabled = false;
		ep->busy = false;
	}
	fEpCount = 0;
	fConfiguration = 0;
}

int
PDRockchipDWC2::standardRequest(const PDUSBSetup *s, uint8_t *data)
{
	switch (s->bRequest) {
	case 0x00:	// GET_STATUS
		data[0] = data[1] = 0;
		return 2;
	case 0x01:	// CLEAR_FEATURE (endpoint halt)
		if ((s->bmRequestType & 0x1f) == 2 && s->wValue == 0) {
			uint8_t addr = s->wIndex & 0xff;

			for (uint8_t i = 0; i < fEpCount; i++) {
				Ep *ep = &fEps[i];

				if (ep->num == (addr & 0x7f) && ep->in == ((addr & 0x80) != 0)) {
					uint32_t off = ep->in ? DIEPCTL(ep->num) : DOEPCTL(ep->num);

					setReg(off, (reg(off) & ~EPCTL_STALL) | EPCTL_SETD0PID);
				}
			}
		}
		return 0;
	case 0x03:	// SET_FEATURE
		return 0;
	case 0x05:	// SET_ADDRESS: the core answers the status stage from the old address
		setReg(kDCFG, (reg(kDCFG) & ~DCFG_DEVADDR_MASK) | DCFG_DEVADDR(s->wValue & 0x7f));
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
			static const uint8_t q[10] = { 10, 6, 0x00, 0x02, 0xef, 0x02, 0x01, EP0_MPS, 1, 0 };

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
PDRockchipDWC2::ep0SetupDone(void)
{
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
		IOLog("PDRockchipDWC2: setup %02x %02x %04x %04x %u -> %d\n", s->bmRequestType, s->bRequest,
		    s->wValue, s->wIndex, s->wLength, len);
	}
	if (len < 0) {
		IOLog("PDRockchipDWC2: stall %02x %02x %04x %04x %u\n", s->bmRequestType, s->bRequest,
		    s->wValue, s->wIndex, s->wLength);
		ep0Stall();
		return;
	}
	ep0ArmOut();
	fEp0InSent = 0;
	if (s->wLength == 0) {
		fEp0State = kStatusIn;
		fEp0InLen = 0;
		fEp0InZlp = false;
		ep0In(NULL, 0);
	} else if (s->bmRequestType & 0x80) {
		fEp0State = kDataIn;
		fEp0InLen = minU32((uint32_t)len, s->wLength);
		// a short answer ending on a packet boundary needs a zero length packet to end it
		fEp0InZlp = fEp0InLen < s->wLength && fEp0InLen != 0 && (fEp0InLen % EP0_MPS) == 0;
		ep0In(NULL, fEp0InLen);
	} else {
		fEp0State = kDataOut;
	}
}

void
PDRockchipDWC2::readPacket(void)
{
	uint32_t st = reg(kGRXSTSP), bcnt = (st >> 4) & 0x7ff, kind = (st >> 17) & 0xf;
	uint8_t num = st & 0xf;
	volatile uint32_t *fifo = (volatile uint32_t *)(fRegs + FIFO(0));
	uint8_t *dst = NULL;
	uint32_t keep = 0;

	if (kind == kRxSetupData) {
		dst = fSetupBuf;
		keep = minU32(bcnt, sizeof(fSetupBuf));
	} else if (kind == kRxOutData) {
		Ep *ep = NULL;

		if (num == 0) {
			ep = &fEp0Out;
		} else {
			for (uint8_t i = 0; i < fEpCount; i++) {
				if (!fEps[i].in && fEps[i].num == num && fEps[i].enabled) {
					ep = &fEps[i];
				}
			}
		}
		if (ep != NULL && ep->buf != NULL && ep->done < ep->bufferSize) {
			dst = ep->buf + ep->done;
			keep = minU32(bcnt, ep->bufferSize - ep->done);
			ep->done += keep;
		}
	}
	// the whole packet leaves the fifo even when there is nowhere to put it
	for (uint32_t i = 0; i < (bcnt + 3) / 4; i++) {
		uint32_t w = *fifo;

		for (uint32_t b = 0; b < 4 && dst != NULL && i * 4 + b < keep; b++) {
			dst[i * 4 + b] = (uint8_t)(w >> (8 * b));
		}
	}
}

void
PDRockchipDWC2::outEndpointEvents(void)
{
	uint32_t v = reg(DOEPINT(0));

	if (v != 0) {
		setReg(DOEPINT(0), v);
		if (fTrace > 0) {
			fTrace--;
			IOLog("PDRockchipDWC2: ep0 out 0x%x state %d\n", v, fEp0State);
		}
		if (v & EPINT_SETUP) {
			ep0SetupDone();
		} else if (v & EPINT_XFERCOMPL) {
			if (fEp0State == kDataOut) {
				for (uint8_t f = 0, first = 0; f < fFunctionCount; f++) {
					uint8_t count = fFunctions[f]->interfaceCount();

					if ((fSetup.wIndex & 0xff) >= first && (fSetup.wIndex & 0xff) < first + count) {
						fFunctions[f]->setupData(&fSetup, fEp0Buf, fEp0Out.done);
						break;
					}
					first += count;
				}
				fEp0State = kStatusIn;
				fEp0InSent = fEp0InLen = 0;
				fEp0InZlp = false;
				ep0In(NULL, 0);
			} else if (fEp0State == kStatusOut) {
				fEp0State = kSetup;
			}
			ep0ArmOut();
		}
	}
	for (uint8_t i = 0; i < fEpCount; i++) {
		Ep *ep = &fEps[i];

		if (ep->in || !ep->enabled) {
			continue;
		}
		v = reg(DOEPINT(ep->num));
		if (v == 0) {
			continue;
		}
		setReg(DOEPINT(ep->num), v);
		if ((v & EPINT_XFERCOMPL) && ep->busy) {
			ep->busy = false;
			ep->fn->transferDone(ep->index, ep->done);
		}
	}
}

void
PDRockchipDWC2::inEndpointEvents(void)
{
	uint32_t v = reg(DIEPINT(0));

	if (fEp0In.busy) {
		fillTxFifo(&fEp0In);
	}
	if (v != 0) {
		setReg(DIEPINT(0), v);
		if (fTrace > 0 && (v & EPINT_XFERCOMPL)) {
			fTrace--;
			IOLog("PDRockchipDWC2: ep0 in 0x%x state %d sent %u/%u\n", v, fEp0State, fEp0InSent, fEp0InLen);
		}
		if ((v & EPINT_XFERCOMPL) && fEp0In.busy) {
			fEp0In.busy = false;
			if (fEp0State == kDataIn) {
				if (fEp0InSent < fEp0InLen) {
					ep0In(NULL, fEp0InLen);
				} else if (fEp0InZlp) {
					fEp0InZlp = false;
					ep0In(NULL, fEp0InLen);
				} else {
					fEp0State = kStatusOut;
				}
			} else if (fEp0State == kStatusIn) {
				fEp0State = kSetup;
			}
		}
	}
	for (uint8_t i = 0; i < fEpCount; i++) {
		Ep *ep = &fEps[i];

		if (!ep->in || !ep->enabled) {
			continue;
		}
		if (ep->busy) {
			fillTxFifo(ep);
		}
		v = reg(DIEPINT(ep->num));
		if (v == 0) {
			continue;
		}
		setReg(DIEPINT(ep->num), v);
		if ((v & EPINT_XFERCOMPL) && ep->busy) {
			ep->busy = false;
			ep->fn->transferDone(ep->index, ep->requested);
		}
	}
}

void
PDRockchipDWC2::pollLoop(void)
{
	uint32_t idle = 0;

	while (fRunning) {
		uint32_t g = reg(kGINTSTS), handled = 0;

		if (g & GINTSTS_USBRST) {
			setReg(kGINTSTS, GINTSTS_USBRST);
			busReset();
			handled++;
		}
		if (g & GINTSTS_ENUMDONE) {
			setReg(kGINTSTS, GINTSTS_ENUMDONE);
			enumDone();
			handled++;
		}
		if (g & GINTSTS_USBSUSP) {
			setReg(kGINTSTS, GINTSTS_USBSUSP);
		}
		// packets first, so a completion finds its data already copied out
		while (reg(kGINTSTS) & GINTSTS_RXFLVL) {
			readPacket();
			handled++;
		}
		outEndpointEvents();
		inEndpointEvents();
		if (fConfiguration != 0) {
			for (uint8_t f = 0; f < fFunctionCount; f++) {
				fFunctions[f]->poll();
			}
		}
		// spin while the bus is busy, then settle back to a millisecond tick
		idle = handled != 0 ? 0 : idle + 1;
		if (idle > 64) {
			IOSleep(1);
		}
	}
}

void
PDRockchipDWC2::pollThread(void *arg, wait_result_t wr)
{
	PDRockchipDWC2 *self = (PDRockchipDWC2 *)arg;

	(void)wr;
	if (self->initCore()) {
		self->pollLoop();
	}
	self->release();
	thread_terminate(current_thread());
}

bool
PDRockchipDWC2::start(IOService *provider)
{
	thread_t th;
	uint32_t off = 0;

	if (!super::start(provider)) {
		return false;
	}
	if (PE_parse_boot_argn("pdusboff", &off, sizeof(off)) && off != 0) {
		return false;
	}
	fRegsMD = IOMemoryDescriptor::withPhysicalAddress(DWC2_PHYS, DWC2_SIZE, kIODirectionInOut);
	fRegsMap = fRegsMD != NULL ? fRegsMD->map(kIOMapInhibitCache) : NULL;
	if (fRegsMap == NULL) {
		return false;
	}
	fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();
	fEp0Buf = (uint8_t *)IOMalloc(kEp0Buffer);
	if (fEp0Buf == NULL) {
		return false;
	}
	PE_parse_boot_argn("pdusbtrace", &fTrace, sizeof(fTrace));

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
	if (kernel_thread_start(&PDRockchipDWC2::pollThread, this, &th) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(th);
	return true;
}

void
PDRockchipDWC2::free(void)
{
	for (uint8_t f = 0; f < fFunctionCount; f++) {
		OSSafeReleaseNULL(fFunctions[f]);
	}
	for (uint8_t i = 0; i < sizeof(fEps) / sizeof(fEps[0]); i++) {
		if (fEps[i].buf != NULL) {
			IOFree(fEps[i].buf, fEps[i].bufferSize);
		}
	}
	if (fEp0Buf != NULL) {
		IOFree(fEp0Buf, kEp0Buffer);
	}
	OSSafeReleaseNULL(fRegsMap);
	OSSafeReleaseNULL(fRegsMD);
	super::free();
}
