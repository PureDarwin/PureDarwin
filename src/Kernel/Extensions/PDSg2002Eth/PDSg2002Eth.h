#pragma once

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>

// sophgo sg2002 designware gmac 3.70a with the internal 10/100 phy, the licheerv nano e's port
// chained normal descriptors in one uncached region, rx and tx reclaim polled from a timer

#define kSgEthRxCount    32
#define kSgEthTxCount    32
#define kSgEthBufSize    2048

struct SgEthDesc {
	volatile uint32_t status;
	volatile uint32_t control;
	volatile uint32_t buffer;
	volatile uint32_t next;
};

class PDSg2002Eth : public IOEthernetController
{
	OSDeclareDefaultStructors(PDSg2002Eth);

public:
	bool init(OSDictionary *properties) override;
	void free() override;
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;

	IOReturn enable(IONetworkInterface *interface) override;
	IOReturn disable(IONetworkInterface *interface) override;

	UInt32 outputPacket(mbuf_t m, void *param) override;

	IOReturn getHardwareAddress(IOEthernetAddress *addr) override;
	IOReturn setPromiscuousMode(bool active) override;
	IOReturn setMulticastMode(bool active) override;
	IOReturn setMulticastList(IOEthernetAddress *addrs, UInt32 count) override;

	const OSString *newVendorString() const override;
	const OSString *newModelString() const override;

	IOOutputQueue *createOutputQueue() override;

private:
	IOMemoryMap          *fRegMap;
	volatile uint8_t     *fRegs;
	IOWorkLoop           *fWorkLoop;
	IOTimerEventSource   *fPollTimer;
	IOEthernetInterface  *fInterface;

	IOBufferMemoryDescriptor *fDmaMem;
	uint8_t              *fDma;          // uncached kernel view of fDmaMem
	uint32_t              fDmaPhys;
	SgEthDesc            *fRxDesc;
	SgEthDesc            *fTxDesc;
	uint8_t              *fRxBuf;
	uint8_t              *fTxBuf;
	uint32_t              fRxNext;
	uint32_t              fTxNext;
	uint32_t              fTxClean;

	IOEthernetAddress     fMACAddress;
	int                   fPhyAddr;
	bool                  fEnabled;
	bool                  fLinkUp;
	uint32_t              fPolls;
	uint32_t              fRxPackets;
	uint32_t              fTxPackets;
	uint32_t              fRxErrors;
	uint32_t              fTxDrops;

	uint32_t rd(uint32_t off) const { return *(volatile uint32_t *)(fRegs + off); }
	void     wr(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fRegs + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}

	bool     enableClocksAndPhy();
	int      mdioRead(int phy, int reg);
	bool     mdioWrite(int phy, int reg, uint16_t val);
	bool     findPhy();
	bool     allocRings();
	void     initRings();
	bool     resetMac();
	void     checkLink();
	bool     publishMedia();
	void     pollReceive();
	void     reclaimTx();
	static void pollTimerAction(OSObject *owner, IOTimerEventSource *sender);
};
