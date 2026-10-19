// SPDX-License-Identifier: MIT
// the iokit face of a FreeBSD network driver

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/network/IONetworkMedium.h>
#include <sys/kpi_mbuf.h>
#include <pexpert/pexpert.h>

#include "PDFBSDEthernet.h"
#include "pd_fbsd_os.h"
#include "pd_fbsd_attach.h"

#define super PDFBSD_BASE
OSDefineMetaClassAndStructors(PDFBSD_CLASS, PDFBSD_BASE);

#define PDFBSD_STR2(x)	#x
#define PDFBSD_STR(x)	PDFBSD_STR2(x)

static IOLock *sTxLock;
static IOLock *sAttachLock;

bool
PDFBSD_CLASS::start(IOService *provider)
{
	if (!super::start(provider))
		return false;

	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL)
		return false;
	fPCI->retain();
	if (!fPCI->open(this)) {
		IOLog(PDFBSD_STR(PDFBSD_CLASS) ": cannot open the pci device\n");
		return false;
	}
	fPCI->setBusMasterEnable(true);
	fPCI->setMemoryEnable(true);
	fPCI->setIOEnable(true);

	pdos_xnu_init();
	if (sTxLock == NULL)
		sTxLock = IOLockAlloc();
	if (sAttachLock == NULL)
		sAttachLock = IOLockAlloc();

	IOLog(PDFBSD_STR(PDFBSD_CLASS) ": attaching the FreeBSD driver\n");

	// attaching runs the whole FreeBSD probe and attach, one device at a time
	IOLockLock(sAttachLock);
	fIfp = pd_fbsd_attach_pci(fPCI);
	IOLockUnlock(sAttachLock);
	if (fIfp == NULL) {
		IOLog(PDFBSD_STR(PDFBSD_CLASS) ": the FreeBSD driver did not attach\n");
		fPCI->close(this);
		return false;
	}
	pd_fbsd_set_cookie(fIfp, this);
	fLinkCall = thread_call_allocate(&PDFBSD_CLASS::linkCall, this);

	uint8_t mac[6];
	pd_fbsd_get_lladdr(fIfp, mac);
	IOLog(PDFBSD_STR(PDFBSD_CLASS) ": attached, address %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	if (!publishMedia())
		return false;
	if (!attachInterface((IONetworkInterface **)&fInterface, true)) {
		IOLog(PDFBSD_STR(PDFBSD_CLASS) ": attachInterface failed\n");
		return false;
	}
	registerService();

	// fbsdkpi_detach_test=<seconds> terminates the controller once to test
	// the FreeBSD detach path, since qemu cannot hot-unplug these nics here
	int seconds = 0;
	if (PE_parse_boot_argn("fbsdkpi_detach_test", &seconds, sizeof(seconds))
			&& seconds > 0) {
		fDetachCall = thread_call_allocate(&PDFBSD_CLASS::detachCall, this);
		uint64_t deadline;
		clock_interval_to_deadline(seconds, NSEC_PER_SEC, &deadline);
		retain();
		thread_call_enter_delayed(fDetachCall, deadline);
	}
	return true;
}

void
PDFBSD_CLASS::detachCall(thread_call_param_t owner, thread_call_param_t)
{
	PDFBSD_CLASS *self = (PDFBSD_CLASS *)owner;
	IOLog(PDFBSD_STR(PDFBSD_CLASS) ": detach test, terminating\n");
	self->terminate(kIOServiceSynchronous);
	self->release();
}

void
PDFBSD_CLASS::stop(IOService *provider)
{
	IOLog(PDFBSD_STR(PDFBSD_CLASS) ": detaching\n");
	if (fEnabled)
		disable(fInterface);
	if (fInterface != NULL) {
		detachInterface(fInterface, true);
		OSSafeReleaseNULL(fInterface);
	}
	if (fLinkCall != NULL) {
		thread_call_cancel_wait(fLinkCall);
		thread_call_free(fLinkCall);
		fLinkCall = NULL;
	}
	if (fIfp != NULL) {
		pd_fbsd_set_cookie(fIfp, NULL);
		pd_fbsd_detach(fIfp);
		fIfp = NULL;
	}
	if (fPCI != NULL) {
		fPCI->close(this);
		OSSafeReleaseNULL(fPCI);
	}
	super::stop(provider);
}

static IOMediumType
medium_type(const pd_fbsd_medium &m)
{
	IOMediumType type;
	switch (m.speed) {
		case 0: type = kIOMediumEthernetAuto; break;
		case 10: type = kIOMediumEthernet10BaseT; break;
		case 100: type = kIOMediumEthernet100BaseTX; break;
		case 1000: type = kIOMediumEthernet1000BaseT; break;
		default: type = kIOMediumEthernetAuto; break;
	}
	if (m.speed != 0)
		type |= m.full_duplex ? kIOMediumOptionFullDuplex : kIOMediumOptionHalfDuplex;
	return type;
}

// the driver's media become iokit media, each keeping the FreeBSD media word
// as its index so selectMedium can hand it back
bool
PDFBSD_CLASS::publishMedia()
{
	pd_fbsd_medium list[24];
	int count = pd_fbsd_media_list(fIfp, list, 24);
	OSDictionary *media = OSDictionary::withCapacity(count + 1);
	if (media == NULL)
		return false;

	IONetworkMedium *autoMedium = NULL;
	for (int i = 0; i < count; i++) {
		IONetworkMedium *medium = IONetworkMedium::medium(medium_type(list[i]),
			(UInt64)list[i].speed * 1000000ULL, 0, (UInt32)list[i].word);
		if (medium == NULL)
			continue;
		IONetworkMedium::addMedium(media, medium);
		if (list[i].speed == 0 && autoMedium == NULL) {
			autoMedium = medium;
			autoMedium->retain();
		}
		medium->release();
	}
	if (autoMedium == NULL) {
		// drivers without a media list still get an automatic entry
		autoMedium = IONetworkMedium::medium(kIOMediumEthernetAuto, 0);
		if (autoMedium != NULL)
			IONetworkMedium::addMedium(media, autoMedium);
	}
	bool ok = autoMedium != NULL && publishMediumDictionary(media)
		&& setCurrentMedium(autoMedium);
	OSSafeReleaseNULL(autoMedium);
	media->release();
	return ok;
}

IOReturn
PDFBSD_CLASS::selectMedium(const IONetworkMedium *medium)
{
	if (medium == NULL || fIfp == NULL)
		return kIOReturnBadArgument;
	int error = pd_fbsd_set_media(fIfp, (int32_t)medium->getIndex());
	if (error != 0)
		return kIOReturnUnsupported;
	setCurrentMedium(medium);
	return kIOReturnSuccess;
}

IOReturn
PDFBSD_CLASS::enable(IONetworkInterface *interface)
{
	if (fEnabled)
		return kIOReturnSuccess;
	if (pd_fbsd_up(fIfp) != 0)
		IOLog(PDFBSD_STR(PDFBSD_CLASS) ": the driver did not come up cleanly\n");
	fEnabled = true;

	refreshLink();
	return kIOReturnSuccess;
}

IOReturn
PDFBSD_CLASS::disable(IONetworkInterface *interface)
{
	if (!fEnabled)
		return kIOReturnSuccess;
	pd_fbsd_down(fIfp);
	fEnabled = false;
	setLinkStatus(kIONetworkLinkValid, NULL);
	return kIOReturnSuccess;
}

UInt32
PDFBSD_CLASS::outputPacket(mbuf_t m, void *param)
{
	size_t len = mbuf_pkthdr_len(m);
	if (!fEnabled || len == 0 || len > sizeof(fTxBuffer)) {
		freePacket(m);
		return kIOReturnOutputDropped;
	}
	IOLockLock(sTxLock);
	mbuf_copydata(m, 0, len, fTxBuffer);
	int error = pd_fbsd_transmit(fIfp, fTxBuffer, len);
	IOLockUnlock(sTxLock);
	freePacket(m);
	return error == 0 ? kIOReturnOutputSuccess : kIOReturnOutputDropped;
}

IOOutputQueue *
PDFBSD_CLASS::createOutputQueue()
{
	// as in PDE1000, packets go straight to outputPacket
	return NULL;
}

IOReturn
PDFBSD_CLASS::getHardwareAddress(IOEthernetAddress *addr)
{
	if (addr == NULL || fIfp == NULL)
		return kIOReturnBadArgument;
	pd_fbsd_get_lladdr(fIfp, addr->bytes);
	return kIOReturnSuccess;
}

IOReturn
PDFBSD_CLASS::setPromiscuousMode(bool active)
{
	pd_fbsd_set_promisc(fIfp, active);
	return kIOReturnSuccess;
}

IOReturn
PDFBSD_CLASS::setMulticastMode(bool active)
{
	pd_fbsd_set_allmulti(fIfp, active);
	return kIOReturnSuccess;
}

IOReturn
PDFBSD_CLASS::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
	for (UInt32 i = 0; i < count; i++)
		pd_fbsd_add_multi(fIfp, addrs[i].bytes);
	return kIOReturnSuccess;
}

IOReturn
PDFBSD_CLASS::getMaxPacketSize(UInt32 *maxSize) const
{
	*maxSize = kIOEthernetMaxPacketSize;
	return kIOReturnSuccess;
}

const OSString *
PDFBSD_CLASS::newVendorString() const
{
	return OSString::withCString("FreeBSD driver");
}

const OSString *
PDFBSD_CLASS::newModelString() const
{
	return OSString::withCString(PDFBSD_STR(PDFBSD_CLASS));
}

void
PDFBSD_CLASS::inputFrame(const void *frame, size_t len)
{
	if (fInterface == NULL || len == 0)
		return;
	mbuf_t m = allocatePacket((UInt32)len);
	if (m == NULL)
		return;
	if (mbuf_copyback(m, 0, len, frame, MBUF_DONTWAIT) != 0) {
		freePacket(m);
		return;
	}
	fInterface->inputPacket(m, (UInt32)len);
}

void
PDFBSD_CLASS::inputChain(void *chain)
{
	int len = pd_fbsd_mbuf_copydata(chain, fRxBuffer, sizeof(fRxBuffer));
	inputFrame(fRxBuffer, len);
}

// link changes arrive inside the driver's own locks, so the media query that
// follows them runs later on a thread call
void
PDFBSD_CLASS::linkChanged(bool up, uint64_t baudrate)
{
	if (fLinkCall != NULL)
		thread_call_enter(fLinkCall);
}

void
PDFBSD_CLASS::linkCall(thread_call_param_t owner, thread_call_param_t)
{
	((PDFBSD_CLASS *)owner)->refreshLink();
}

void
PDFBSD_CLASS::refreshLink()
{
	int up = 0;
	uint64_t baudrate = 0;
	if (fIfp == NULL || !fEnabled || pd_fbsd_get_link(fIfp, &up, &baudrate) != 0
			|| !up) {
		setLinkStatus(kIONetworkLinkValid, NULL);
		return;
	}
	// report the negotiated medium when the media table has it
	const IONetworkMedium *active = getCurrentMedium();
	pd_fbsd_medium m;
	if (pd_fbsd_active_media(fIfp, &m) == 0) {
		const OSDictionary *media = getMediumDictionary();
		IONetworkMedium *found = media != NULL
			? IONetworkMedium::getMediumWithType(media, medium_type(m)) : NULL;
		if (found != NULL)
			active = found;
		if (m.speed != 0)
			baudrate = (uint64_t)m.speed * 1000000ULL;
	}
	setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, active, baudrate);
}

// the seam's network callbacks

extern "C" void
pdos_net_input(void *cookie, const void *frame, size_t len)
{
	((PDFBSD_CLASS *)cookie)->inputFrame(frame, len);
}

extern "C" void
pdos_net_input_chain(void *cookie, void *chain)
{
	((PDFBSD_CLASS *)cookie)->inputChain(chain);
}

extern "C" void
pdos_net_link(void *cookie, int up, uint64_t baudrate)
{
	((PDFBSD_CLASS *)cookie)->linkChanged(up != 0, baudrate);
}
