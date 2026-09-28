#include "PDSg2002Eth.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/network/IOOutputQueue.h>

#define super IOEthernetController
OSDefineMetaClassAndStructors(PDSg2002Eth, IOEthernetController);

// register use follows u-boot drivers/net/designware.c, which drives this mac on the board

// mac block
#define kMAC_CONF		0x0000
#define kCONF_RX_EN		(1u << 2)
#define kCONF_TX_EN		(1u << 3)
#define kCONF_FULL_DUPLEX	(1u << 11)
#define kCONF_DISABLE_RX_OWN	(1u << 13)
#define kCONF_FES_100		(1u << 14)
#define kCONF_PORT_MII		(1u << 15)
#define kCONF_FRAME_BURST	(1u << 21)
#define kMAC_FRAME_FILTER	0x0004
#define kFILTER_PROMISC		(1u << 0)
#define kFILTER_ALL_MULTI	(1u << 4)
#define kMAC_MII_ADDR		0x0010
#define kMII_BUSY		(1u << 0)
#define kMII_WRITE		(1u << 1)
#define kMII_CLK_150_250M	(0x4u << 2)
#define kMAC_MII_DATA		0x0014
#define kMAC_ADDR0_HI		0x0040
#define kMAC_ADDR0_LO		0x0044

// dma block
#define kDMA_BUS_MODE		0x1000
#define kBUS_SOFT_RESET		(1u << 0)
#define kBUS_PBL_8		(8u << 8)
#define kBUS_PRIO_41		(3u << 14)
#define kBUS_FIXED_BURST	(1u << 16)
#define kDMA_TX_POLL		0x1004
#define kDMA_RX_POLL		0x1008
#define kDMA_RX_LIST		0x100c
#define kDMA_TX_LIST		0x1010
#define kDMA_STATUS		0x1014
#define kDMA_OP_MODE		0x1018
#define kOP_RX_START		(1u << 1)
#define kOP_TX_START		(1u << 13)
#define kOP_FLUSH_TX_FIFO	(1u << 20)
#define kOP_STORE_FORWARD	(1u << 21)
#define kDMA_INT_EN		0x101c

// normal descriptors in chained mode
#define kDESC_OWN		(1u << 31)
#define kRX_FRAME_LEN(s)	(((s) >> 16) & 0x3fffu)
#define kRX_ERROR		(1u << 15)
#define kRX_FIRST		(1u << 9)
#define kRX_LAST		(1u << 8)
#define kRX_CTRL_CHAIN		(1u << 24)
#define kTX_CTRL_LAST		(1u << 30)
#define kTX_CTRL_FIRST		(1u << 29)
#define kTX_CTRL_CHAIN		(1u << 24)
#define kMAX_FRAME		1600

// internal phy, its registers are memory mapped until handed to the mdio bus
#define kEPHY_BASE_PHYS		0x03009000ULL
#define kEPHY_CTL		0x800
#define kEPHY_APB_RW_SEL	0x804
#define kEPHY_PAGE_SEL		(0x1f * 4)
#define kEPHY_PD_EN_CTL		(0x10 * 4)
#define kEPHY_ID1		(0x02 * 4)
#define kEPHY_ID2		(0x03 * 4)

// clock gates for the gmac's axi and 500 mhz clocks
#define kCLK_BASE_PHYS		0x03002000ULL
#define kCLK_EN_0		0x000
#define kCLK_EN_ETH0		((1u << 25) | (1u << 26))

// standard mii registers
#define kMII_BMCR		0
#define kBMCR_ANRESTART		(1u << 9)
#define kBMCR_ANENABLE		(1u << 12)
#define kMII_BMSR		1
#define kBMSR_LINK		(1u << 2)
#define kBMSR_ANEG_DONE		(1u << 5)
#define kMII_ANLPAR		5
#define kLPA_10HALF		(1u << 5)
#define kLPA_10FULL		(1u << 6)
#define kLPA_100HALF		(1u << 7)
#define kLPA_100FULL		(1u << 8)
#define kMII_ANAR		4

#define kPollMS			1
#define kLinkCheckPolls		500

static volatile uint8_t *
map_phys(IOPhysicalAddress phys, IOByteCount size, IOMemoryMap **outMap)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(phys, size,
	    kIODirectionOutIn);
	IOMemoryMap *map;

	if (desc == NULL)
		return NULL;
	map = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (map == NULL)
		return NULL;
	*outMap = map;
	return (volatile uint8_t *)map->getVirtualAddress();
}

static inline void
dmaBarrier(void)
{
	__asm__ volatile ("dsb sy" ::: "memory");
}

bool
PDSg2002Eth::init(OSDictionary *properties)
{
	if (!super::init(properties))
		return false;
	fRegMap = NULL;
	fRegs = NULL;
	fWorkLoop = NULL;
	fPollTimer = NULL;
	fInterface = NULL;
	fDmaMem = NULL;
	fDma = NULL;
	fPhyAddr = -1;
	fEnabled = false;
	fLinkUp = false;
	fPolls = 0;
	fRxPackets = fTxPackets = fRxErrors = fTxDrops = 0;
	fRxNext = fTxNext = fTxClean = 0;
	// locally administered, fixed so dhcp leases survive reboots
	static const uint8_t mac[6] = { 0x02, 0x5d, 0x6e, 0x20, 0x02, 0x01 };
	bcopy(mac, fMACAddress.bytes, 6);
	return true;
}

// clocks on, then the phy bring-up u-boot's board code does, before the mdio bus owns it
bool
PDSg2002Eth::enableClocksAndPhy()
{
	IOMemoryMap *clkMap = NULL, *ephyMap = NULL;
	volatile uint8_t *clk = map_phys(kCLK_BASE_PHYS, 0x1000, &clkMap);
	volatile uint8_t *ephy = map_phys(kEPHY_BASE_PHYS, 0x1000, &ephyMap);
	uint32_t v;

	if (clk == NULL || ephy == NULL) {
		OSSafeReleaseNULL(clkMap);
		OSSafeReleaseNULL(ephyMap);
		return false;
	}
	*(volatile uint32_t *)(clk + kCLK_EN_0) |= kCLK_EN_ETH0;
	dmaBarrier();

	*(volatile uint32_t *)(ephy + kEPHY_APB_RW_SEL) = 1;
	v = *(volatile uint32_t *)(ephy + kEPHY_CTL);
	v &= ~1u;			// shutdown off
	v |= (1u << 1) | (1u << 2) | (1u << 3);	// analog, digital and main out of reset
	*(volatile uint32_t *)(ephy + kEPHY_CTL) = v;

	// page 5 holds the power-down and enable bits for the analog front end
	*(volatile uint32_t *)(ephy + kEPHY_PAGE_SEL) = 5u << 8;
	v = *(volatile uint32_t *)(ephy + kEPHY_PD_EN_CTL);
	v &= ~((1u << 13) | (1u << 12) | (1u << 9) | (1u << 8));
	v |= (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6);
	*(volatile uint32_t *)(ephy + kEPHY_PD_EN_CTL) = v;

	// the id registers reset to zero, give it one so a scan can tell it from an empty address
	*(volatile uint32_t *)(ephy + kEPHY_PAGE_SEL) = 0;
	*(volatile uint32_t *)(ephy + kEPHY_ID1) = 0;
	*(volatile uint32_t *)(ephy + kEPHY_ID2) = 1;

	*(volatile uint32_t *)(ephy + kEPHY_APB_RW_SEL) = 0;
	dmaBarrier();

	OSSafeReleaseNULL(clkMap);
	OSSafeReleaseNULL(ephyMap);
	return true;
}

int
PDSg2002Eth::mdioRead(int phy, int reg)
{
	wr(kMAC_MII_ADDR, ((uint32_t)phy << 11) | ((uint32_t)reg << 6) | kMII_CLK_150_250M | kMII_BUSY);
	for (int i = 0; i < 10000; i++) {
		if ((rd(kMAC_MII_ADDR) & kMII_BUSY) == 0)
			return (int)(rd(kMAC_MII_DATA) & 0xffff);
		IODelay(10);
	}
	return -1;
}

bool
PDSg2002Eth::mdioWrite(int phy, int reg, uint16_t val)
{
	wr(kMAC_MII_DATA, val);
	wr(kMAC_MII_ADDR, ((uint32_t)phy << 11) | ((uint32_t)reg << 6) | kMII_CLK_150_250M |
	    kMII_WRITE | kMII_BUSY);
	for (int i = 0; i < 10000; i++) {
		if ((rd(kMAC_MII_ADDR) & kMII_BUSY) == 0)
			return true;
		IODelay(10);
	}
	return false;
}

bool
PDSg2002Eth::findPhy()
{
	for (int addr = 0; addr < 32; addr++) {
		int id1 = mdioRead(addr, 2), id2 = mdioRead(addr, 3), bmsr = mdioRead(addr, kMII_BMSR);

		if (bmsr <= 0 || bmsr == 0xffff)
			continue;
		if (id1 == 0xffff && id2 == 0xffff)
			continue;
		fPhyAddr = addr;
		return true;
	}
	IOLog("PDSg2002Eth: no phy answered on mdio\n");
	return false;
}

// descriptors first, then one 2 KB buffer per descriptor, all under 4 GB for the 32 bit dma
bool
PDSg2002Eth::allocRings()
{
	IOByteCount descBytes = (kSgEthRxCount + kSgEthTxCount) * sizeof(SgEthDesc);
	IOByteCount total = round_page(descBytes) + (kSgEthRxCount + kSgEthTxCount) * kSgEthBufSize;

	// normal non-cacheable from the start, a cacheable alias of dma rings on this
	// non-coherent soc lets stale lines hide the mac's descriptor write-backs
	fDmaMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMapWriteCombineCache,
	    total, 0xFFFFF000ULL);
	if (fDmaMem == NULL)
		return false;
	fDma = (uint8_t *)fDmaMem->getBytesNoCopy();
	bzero(fDma, total);
	dmaBarrier();
	fDmaPhys = (uint32_t)fDmaMem->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);

	fRxDesc = (SgEthDesc *)fDma;
	fTxDesc = fRxDesc + kSgEthRxCount;
	fRxBuf = fDma + round_page(descBytes);
	fTxBuf = fRxBuf + kSgEthRxCount * kSgEthBufSize;
	return true;
}

void
PDSg2002Eth::initRings()
{
	uint32_t descPhys = fDmaPhys;
	uint32_t rxBufPhys = fDmaPhys + (uint32_t)(fRxBuf - fDma);
	uint32_t txBufPhys = fDmaPhys + (uint32_t)(fTxBuf - fDma);
	uint32_t txDescPhys = descPhys + kSgEthRxCount * sizeof(SgEthDesc);

	for (uint32_t i = 0; i < kSgEthRxCount; i++) {
		fRxDesc[i].buffer = rxBufPhys + i * kSgEthBufSize;
		fRxDesc[i].next = descPhys + ((i + 1) % kSgEthRxCount) * sizeof(SgEthDesc);
		fRxDesc[i].control = kRX_CTRL_CHAIN | kMAX_FRAME;
		fRxDesc[i].status = kDESC_OWN;
	}
	for (uint32_t i = 0; i < kSgEthTxCount; i++) {
		fTxDesc[i].buffer = txBufPhys + i * kSgEthBufSize;
		fTxDesc[i].next = txDescPhys + ((i + 1) % kSgEthTxCount) * sizeof(SgEthDesc);
		fTxDesc[i].control = kTX_CTRL_CHAIN;
		fTxDesc[i].status = 0;
	}
	dmaBarrier();
	wr(kDMA_RX_LIST, descPhys);
	wr(kDMA_TX_LIST, txDescPhys);
	fRxNext = fTxNext = fTxClean = 0;
}

bool
PDSg2002Eth::resetMac()
{
	// rmii resets with the port select clear, retry with it set in case the phy wants mii
	for (int attempt = 0; attempt < 2; attempt++) {
		uint32_t conf = rd(kMAC_CONF);

		wr(kMAC_CONF, attempt ? (conf | kCONF_PORT_MII) : (conf & ~kCONF_PORT_MII));
		wr(kDMA_BUS_MODE, rd(kDMA_BUS_MODE) | kBUS_SOFT_RESET);
		for (int i = 0; i < 1000; i++) {
			if ((rd(kDMA_BUS_MODE) & kBUS_SOFT_RESET) == 0)
				goto done;
			IODelay(1000);
		}
	}
	IOLog("PDSg2002Eth: dma reset timed out\n");
	return false;

done:
	// the reset clears the address filter
	const uint8_t *m = fMACAddress.bytes;
	wr(kMAC_ADDR0_HI, m[4] | (m[5] << 8));
	wr(kMAC_ADDR0_LO, m[0] | (m[1] << 8) | (m[2] << 16) | ((uint32_t)m[3] << 24));

	initRings();
	wr(kDMA_BUS_MODE, kBUS_FIXED_BURST | kBUS_PRIO_41 | kBUS_PBL_8);
	wr(kDMA_INT_EN, 0);
	wr(kDMA_STATUS, 0xffffffffu);
	wr(kDMA_OP_MODE, rd(kDMA_OP_MODE) | kOP_FLUSH_TX_FIFO | kOP_STORE_FORWARD);
	wr(kDMA_OP_MODE, rd(kDMA_OP_MODE) | kOP_RX_START | kOP_TX_START);
	wr(kMAC_FRAME_FILTER, kFILTER_ALL_MULTI);
	return true;
}

// speed and duplex from what the link partner advertised, 10/100 only on this phy
void
PDSg2002Eth::checkLink()
{
	int bmsr, lpa;
	bool up;
	uint32_t conf;

	if (fPhyAddr < 0)
		return;
	mdioRead(fPhyAddr, kMII_BMSR);	// latched low, the second read is current
	bmsr = mdioRead(fPhyAddr, kMII_BMSR);
	up = bmsr > 0 && (bmsr & kBMSR_LINK) && (bmsr & kBMSR_ANEG_DONE);
	if (up == fLinkUp)
		return;
	fLinkUp = up;

	if (!up) {
		IOLog("PDSg2002Eth: link down\n");
		setLinkStatus(kIONetworkLinkValid, NULL);
		return;
	}

	lpa = mdioRead(fPhyAddr, kMII_ANLPAR) & mdioRead(fPhyAddr, kMII_ANAR);
	bool fast = (lpa & (kLPA_100FULL | kLPA_100HALF)) != 0;
	bool full = fast ? (lpa & kLPA_100FULL) != 0 : (lpa & kLPA_10FULL) != 0;

	conf = rd(kMAC_CONF) | kCONF_FRAME_BURST | kCONF_DISABLE_RX_OWN | kCONF_PORT_MII;
	conf = fast ? (conf | kCONF_FES_100) : (conf & ~kCONF_FES_100);
	conf = full ? (conf | kCONF_FULL_DUPLEX) : (conf & ~kCONF_FULL_DUPLEX);
	wr(kMAC_CONF, conf | kCONF_RX_EN | kCONF_TX_EN);

	IOLog("PDSg2002Eth: link up, %s %s duplex\n", fast ? "100" : "10", full ? "full" : "half");
	IOMediumType type = (fast ? kIOMediumEthernet100BaseTX : kIOMediumEthernet10BaseT) |
	    (full ? kIOMediumOptionFullDuplex : kIOMediumOptionHalfDuplex);
	setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive,
	    IONetworkMedium::getMediumWithType(getMediumDictionary(), type),
	    fast ? 100000000ULL : 10000000ULL);
}

bool
PDSg2002Eth::publishMedia()
{
	OSDictionary *dict = OSDictionary::withCapacity(5);
	static const IOMediumType types[] = {
		kIOMediumEthernetAuto,
		kIOMediumEthernet10BaseT | kIOMediumOptionHalfDuplex,
		kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex,
		kIOMediumEthernet100BaseTX | kIOMediumOptionHalfDuplex,
		kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex,
	};
	static const uint64_t speeds[] = { 0, 10000000ULL, 10000000ULL, 100000000ULL, 100000000ULL };
	bool ok = dict != NULL;

	for (unsigned i = 0; ok && i < sizeof(types) / sizeof(types[0]); i++) {
		IONetworkMedium *m = IONetworkMedium::medium(types[i], speeds[i]);
		ok = m != NULL && IONetworkMedium::addMedium(dict, m);
		OSSafeReleaseNULL(m);
	}
	ok = ok && publishMediumDictionary(dict);
	if (ok)
		setCurrentMedium(IONetworkMedium::getMediumWithType(dict, kIOMediumEthernetAuto));
	OSSafeReleaseNULL(dict);
	return ok;
}

bool
PDSg2002Eth::start(IOService *provider)
{
	if (!super::start(provider))
		return false;

	fRegMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	if (fRegMap == NULL) {
		IOLog("PDSg2002Eth: cannot map registers\n");
		return false;
	}
	fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();

	if (!enableClocksAndPhy() || !allocRings() || !resetMac() || !findPhy())
		return false;

	// restart autonegotiation so the link state below is fresh
	int bmcr = mdioRead(fPhyAddr, kMII_BMCR);
	if (bmcr >= 0)
		mdioWrite(fPhyAddr, kMII_BMCR, (uint16_t)(bmcr | kBMCR_ANENABLE | kBMCR_ANRESTART));


	fWorkLoop = getWorkLoop();
	if (fWorkLoop == NULL)
		return false;
	fPollTimer = IOTimerEventSource::timerEventSource(this, &PDSg2002Eth::pollTimerAction);
	if (fPollTimer == NULL || fWorkLoop->addEventSource(fPollTimer) != kIOReturnSuccess)
		return false;

	if (!publishMedia())
		return false;
	if (!attachInterface((IONetworkInterface **)&fInterface, true)) {
		IOLog("PDSg2002Eth: attachInterface failed\n");
		return false;
	}
	setLinkStatus(kIONetworkLinkValid, NULL);
	fPollTimer->setTimeoutMS(kPollMS);

	registerService();
	return true;
}

void
PDSg2002Eth::stop(IOService *provider)
{
	if (fPollTimer != NULL)
		fPollTimer->cancelTimeout();
	if (fRegs != NULL) {
		wr(kMAC_CONF, rd(kMAC_CONF) & ~(kCONF_RX_EN | kCONF_TX_EN));
		wr(kDMA_OP_MODE, rd(kDMA_OP_MODE) & ~(kOP_RX_START | kOP_TX_START));
	}
	if (fInterface != NULL) {
		detachInterface(fInterface, true);
		OSSafeReleaseNULL(fInterface);
	}
	if (fWorkLoop != NULL && fPollTimer != NULL)
		fWorkLoop->removeEventSource(fPollTimer);
	super::stop(provider);
}

void
PDSg2002Eth::free()
{
	OSSafeReleaseNULL(fPollTimer);
	OSSafeReleaseNULL(fDmaMem);
	OSSafeReleaseNULL(fRegMap);
	super::free();
}

IOReturn
PDSg2002Eth::enable(IONetworkInterface *interface)
{
	fEnabled = true;
	return kIOReturnSuccess;
}

IOReturn
PDSg2002Eth::disable(IONetworkInterface *interface)
{
	fEnabled = false;
	return kIOReturnSuccess;
}

void
PDSg2002Eth::pollTimerAction(OSObject *owner, IOTimerEventSource *sender)
{
	PDSg2002Eth *self = OSDynamicCast(PDSg2002Eth, owner);

	if (self == NULL)
		return;
	if (self->fPolls++ % kLinkCheckPolls == 0)
		self->checkLink();
	self->pollReceive();
	self->reclaimTx();
	sender->setTimeoutMS(kPollMS);
}

void
PDSg2002Eth::pollReceive()
{
	bool got = false;

	for (int budget = 0; budget < kSgEthRxCount; budget++) {
		SgEthDesc *d = &fRxDesc[fRxNext];
		uint32_t st = d->status;

		if (st & kDESC_OWN)
			break;
		__asm__ volatile ("dmb ld" ::: "memory");

		uint32_t len = kRX_FRAME_LEN(st);
		bool whole = (st & (kRX_FIRST | kRX_LAST)) == (kRX_FIRST | kRX_LAST);
		if (!whole || (st & kRX_ERROR) || len < 18 || len > kMAX_FRAME) {
			fRxErrors++;
		} else if (fEnabled && fInterface != NULL) {
			// the length includes the frame check sequence
			uint32_t pktLen = len - 4;
			mbuf_t m = allocatePacket(pktLen);
			if (m != NULL) {
				mbuf_copyback(m, 0, pktLen, fRxBuf + fRxNext * kSgEthBufSize, MBUF_DONTWAIT);
				fInterface->inputPacket(m, pktLen, IONetworkInterface::kInputOptionQueuePacket);
				fRxPackets++;
				got = true;
			}
		}
		d->status = kDESC_OWN;
		fRxNext = (fRxNext + 1) % kSgEthRxCount;
	}
	dmaBarrier();
	wr(kDMA_RX_POLL, 1);
	if (got)
		fInterface->flushInputQueue();
}

void
PDSg2002Eth::reclaimTx()
{
	while (fTxClean != fTxNext && (fTxDesc[fTxClean].status & kDESC_OWN) == 0)
		fTxClean = (fTxClean + 1) % kSgEthTxCount;
}

UInt32
PDSg2002Eth::outputPacket(mbuf_t m, void *param)
{
	size_t len = mbuf_pkthdr_len(m);
	uint32_t slot = fTxNext;
	SgEthDesc *d = &fTxDesc[slot];

	reclaimTx();
	if (!fLinkUp || len == 0 || len > kMAX_FRAME - 4 ||
	    (slot + 1) % kSgEthTxCount == fTxClean || (d->status & kDESC_OWN)) {
		fTxDrops++;
		freePacket(m);
		return kIOReturnOutputDropped;
	}

	uint8_t *buf = fTxBuf + slot * kSgEthBufSize;
	mbuf_copydata(m, 0, len, buf);
	if (len < 60) {
		bzero(buf + len, 60 - len);
		len = 60;
	}
	d->control = kTX_CTRL_CHAIN | kTX_CTRL_FIRST | kTX_CTRL_LAST | (uint32_t)len;
	dmaBarrier();
	d->status = kDESC_OWN;
	dmaBarrier();
	wr(kDMA_TX_POLL, 1);

	fTxNext = (slot + 1) % kSgEthTxCount;
	fTxPackets++;
	freePacket(m);
	return kIOReturnOutputSuccess;
}

IOReturn
PDSg2002Eth::getHardwareAddress(IOEthernetAddress *addr)
{
	if (addr == NULL)
		return kIOReturnBadArgument;
	*addr = fMACAddress;
	return kIOReturnSuccess;
}

IOReturn
PDSg2002Eth::setPromiscuousMode(bool active)
{
	uint32_t f = rd(kMAC_FRAME_FILTER);
	wr(kMAC_FRAME_FILTER, active ? (f | kFILTER_PROMISC) : (f & ~kFILTER_PROMISC));
	return kIOReturnSuccess;
}

// every multicast frame is let through already
IOReturn
PDSg2002Eth::setMulticastMode(bool active)
{
	return kIOReturnSuccess;
}

IOReturn
PDSg2002Eth::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
	return kIOReturnSuccess;
}

const OSString *
PDSg2002Eth::newVendorString() const
{
	return OSString::withCString("Sophgo");
}

const OSString *
PDSg2002Eth::newModelString() const
{
	return OSString::withCString("SG2002 GMAC (PDSg2002Eth)");
}

// the same direct output path PDE1000 and IOVirtIONet use
IOOutputQueue *
PDSg2002Eth::createOutputQueue()
{
	return NULL;
}
