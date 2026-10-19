// SPDX-License-Identifier: MIT
// the iokit face of a FreeBSD network driver, an IOEthernetController whose
// packets cross into the driver's ifnet through the fbsdkpi boundary
#ifndef PD_FBSD_ETHERNET_H
#define PD_FBSD_ETHERNET_H

#include <IOKit/IOService.h>
#include <kern/thread_call.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOOutputQueue.h>

#include "pd_fbsd_xnu.h"

// wifi drivers sit on IO80211Controller, which answers the apple80211 requests
#ifdef PDFBSD_WLAN
#include "IO80211Controller.h"
#include "pd_wlan.h"
#define PDFBSD_BASE IO80211Controller
#else
#define PDFBSD_BASE IOEthernetController
#endif

class PDFBSD_CLASS : public PDFBSD_BASE
{
	OSDeclareDefaultStructors(PDFBSD_CLASS);

public:
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;

	IOReturn enable(IONetworkInterface *interface) override;
	IOReturn disable(IONetworkInterface *interface) override;
	UInt32 outputPacket(mbuf_t m, void *param) override;
	IOOutputQueue *createOutputQueue() override;

	IOReturn getHardwareAddress(IOEthernetAddress *addr) override;
	IOReturn setPromiscuousMode(bool active) override;
	IOReturn setMulticastMode(bool active) override;
	IOReturn setMulticastList(IOEthernetAddress *addrs, UInt32 count) override;
	IOReturn getMaxPacketSize(UInt32 *maxSize) const override;
	IOReturn selectMedium(const IONetworkMedium *medium) override;

	const OSString *newVendorString() const override;
	const OSString *newModelString() const override;

	// called by the FreeBSD side through the seam
	void inputFrame(const void *frame, size_t len);
	void inputChain(void *chain);
	void linkChanged(bool up, uint64_t baudrate);
	void refreshLink();
	static void linkCall(thread_call_param_t owner, thread_call_param_t);
	static void detachCall(thread_call_param_t owner, thread_call_param_t);

#ifdef PDFBSD_WLAN
	SInt32 apple80211Request(bool set, int type, int value, void *data, uint32_t *len) override;
#endif

private:
	bool publishMedia();

	IOPCIDevice			*fPCI;
	IOEthernetInterface	*fInterface;
	void				*fIfp;
	bool				fEnabled;
	thread_call_t		fLinkCall;
	thread_call_t		fDetachCall;
	uint8_t				fTxBuffer[16384];
	uint8_t				fRxBuffer[16384];
#ifdef PDFBSD_WLAN
	// filled when SCAN_RESULT asks for index 0, later indexes read from it
	struct pd_wlan_bss	fScanCache[64];
	int					fScanCount;
#endif
};

#endif
