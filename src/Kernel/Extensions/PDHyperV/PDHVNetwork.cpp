#include "PDHVNetwork.h"

#include <IOKit/IOLib.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/network/IOOutputQueue.h>
#include <libkern/OSAtomic.h>

extern "C" {
#include <sys/kpi_mbuf.h>
#include <kern/thread_call.h>
}

#define super IOEthernetController
OSDefineMetaClassAndStructors(PDHVNetwork, IOEthernetController);

#define NET_LOG(fmt, ...) IOLog("PDHVNetwork: " fmt "\n", ##__VA_ARGS__)

// NVS transaction ids live above the slot range so completions can tell them apart
#define HN_XACT_BASE        0x10000
#define HN_XACT_TIMEOUT_S   10

static const uint32_t kNvsVersions[] = {
	HN_NVS_VERSION_61, HN_NVS_VERSION_6, HN_NVS_VERSION_5, HN_NVS_VERSION_4, HN_NVS_VERSION_2,
};

static inline void
put32(uint8_t *p, uint32_t off, uint32_t v)
{
	bcopy(&v, p + off, sizeof(v));
}

static inline uint32_t
get32(const uint8_t *p, uint32_t off)
{
	uint32_t v;
	bcopy(p + off, &v, sizeof(v));
	return v;
}

void
PDHVNetwork::channelCallback(OSObject *target, PDVMBusChannel *chan)
{
	(void)chan;
	PDHVNetwork *self = OSDynamicCast(PDHVNetwork, target);
	if (self != NULL && self->fRxSource != NULL) {
		self->fRxSource->interruptOccurred(NULL, NULL, 0);
	}
}

int
PDHVNetwork::allocSlot(void)
{
	for (int i = 0; i < HN_TX_SLOTS; i++) {
		UInt64 cur = fSlotUsed[i / 64];
		UInt64 bit = 1ULL << (i % 64);
		if ((cur & bit) == 0 && OSCompareAndSwap64(cur, cur | bit, (UInt64 *)&fSlotUsed[i / 64])) {
			return i;
		}
	}
	return -1;
}

void
PDHVNetwork::freeSlot(int slot)
{
	if (slot < 0 || slot >= HN_TX_SLOTS) {
		return;
	}
	UInt64 cur;
	do {
		cur = fSlotUsed[slot / 64];
	} while (!OSCompareAndSwap64(cur, cur & ~(1ULL << (slot % 64)), (UInt64 *)&fSlotUsed[slot / 64]));
}

// The RNDIS message already sits in the slot's page. Point the host at it
IOReturn
PDHVNetwork::sendRndis(int slot, uint32_t mtype, uint32_t len)
{
	struct hn_nvs_msg msg;
	bzero(&msg, sizeof(msg));
	put32(msg.bytes, 0, HN_NVS_TYPE_RNDIS);
	put32(msg.bytes, 4, mtype);
	put32(msg.bytes, 8, HN_NVS_CHIM_IDX_INVALID);
	put32(msg.bytes, 12, 0);

	uint64_t pfn = (fTxPool.pa >> 12) + (uint64_t)slot;
	return fChannel->sendPages(&pfn, 1, 0, len, &msg, sizeof(msg), (uint64_t)slot + 1);
}

IOReturn
PDHVNetwork::nvsTransact(const struct hn_nvs_msg *req, uint32_t respType, uint8_t *resp, uint32_t respLen)
{
	IOLockLock(fXactLock);
	uint64_t xact = HN_XACT_BASE + fNextRid++;
	fWaitXact = xact;
	fWaitRid = 0;
	fReplyReady = false;
	IOLockUnlock(fXactLock);

	IOReturn ret = fChannel->send(VMBUS_CHANPKT_TYPE_INBAND, VMBUS_CHANPKT_FLAG_RC, req, sizeof(*req), xact);
	if (ret != kIOReturnSuccess) {
		return ret;
	}

	IOLockLock(fXactLock);
	if (!fReplyReady) {
		AbsoluteTime deadline;
		clock_interval_to_deadline(HN_XACT_TIMEOUT_S, kSecondScale, &deadline);
		IOLockSleepDeadline(fXactLock, &fReplyReady, deadline, THREAD_UNINT);
	}
	bool ready = fReplyReady;
	uint32_t got = fReplyLen;
	if (ready) {
		bcopy(fReply, resp, got < respLen ? got : respLen);
	}
	fWaitXact = 0;
	IOLockUnlock(fXactLock);

	if (!ready) {
		return kIOReturnTimeout;
	}
	if (got < 8 || get32(resp, 0) != respType) {
		return kIOReturnIOError;
	}
	return kIOReturnSuccess;
}

// Query or set one OID. The completion comes back through the receive buffer
IOReturn
PDHVNetwork::rndisTransact(uint32_t type, uint32_t oid, const void *info, uint32_t infoLen,
    uint8_t *reply, uint32_t *replyLen)
{
	int slot = allocSlot();
	if (slot < 0) {
		return kIOReturnNoResources;
	}
	uint8_t *req = (uint8_t *)fTxPool.va + (vm_size_t)slot * PAGE_SIZE;

	IOLockLock(fXactLock);
	uint32_t rid = fNextRid++;
	fWaitRid = rid;
	fWaitXact = 0;
	fReplyReady = false;
	IOLockUnlock(fXactLock);

	uint32_t len;
	bzero(req, PAGE_SIZE);
	put32(req, 0, type);
	put32(req, 8, rid);
	if (type == REMOTE_NDIS_INITIALIZE_MSG) {
		len = 24;
		put32(req, 12, 1);              // RNDIS 1.0
		put32(req, 16, 0);
		put32(req, 20, 0x4000);         // Max transfer size
	} else {
		len = 28 + infoLen;
		put32(req, 12, oid);
		put32(req, 16, infoLen);
		put32(req, 20, 20);             // info buffer, relative to rm_rid
		if (infoLen > 0) {
			bcopy(info, req + 28, infoLen);
		}
	}
	put32(req, 4, len);

	IOReturn ret = sendRndis(slot, HN_NVS_RNDIS_MTYPE_CTRL, len);
	if (ret != kIOReturnSuccess) {
		freeSlot(slot);
		return ret;
	}

	IOLockLock(fXactLock);
	if (!fReplyReady) {
		AbsoluteTime deadline;
		clock_interval_to_deadline(HN_XACT_TIMEOUT_S, kSecondScale, &deadline);
		IOLockSleepDeadline(fXactLock, &fReplyReady, deadline, THREAD_UNINT);
	}
	bool ready = fReplyReady;
	uint32_t got = fReplyLen;
	if (ready) {
		uint32_t n = got < *replyLen ? got : *replyLen;
		bcopy(fReply, reply, n);
		*replyLen = n;
	}
	fWaitRid = 0;
	IOLockUnlock(fXactLock);

	if (!ready) {
		return kIOReturnTimeout;
	}
	if (*replyLen < 16 || get32(reply, 12) != RNDIS_STATUS_SUCCESS) {
		return kIOReturnIOError;
	}
	return kIOReturnSuccess;
}

void
PDHVNetwork::handleRndis(const uint8_t *msg, uint32_t len)
{
	if (len < 8) {
		return;
	}
	uint32_t type = get32(msg, 0);

	if (type == REMOTE_NDIS_PACKET_MSG && len >= sizeof(struct rndis_packet_msg)) {
		uint32_t off = get32(msg, 8) + 8;
		uint32_t dlen = get32(msg, 12);
		if (!fEnabled || fInterface == NULL || off > len || dlen > len - off || dlen == 0) {
			return;
		}
		mbuf_t m = allocatePacket(dlen);
		if (m == NULL) {
			return;
		}
		if (mbuf_copyback(m, 0, dlen, msg + off, MBUF_DONTWAIT) != 0) {
			freePacket(m);
			return;
		}
		fInterface->inputPacket(m, dlen, IONetworkInterface::kInputOptionQueuePacket);
		return;
	}

	if (type == REMOTE_NDIS_INDICATE_STATUS_MSG && len >= 12) {
		uint32_t status = get32(msg, 8);
		if (status == RNDIS_STATUS_MEDIA_CONNECT) {
			setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, getCurrentMedium());
		} else if (status == RNDIS_STATUS_MEDIA_DISCONNECT) {
			setLinkStatus(kIONetworkLinkValid, NULL);
		}
		return;
	}

	if ((type & REMOTE_NDIS_CMPLT_FLAG) && len >= 16) {
		IOLockLock(fXactLock);
		if (fWaitRid != 0 && get32(msg, 8) == fWaitRid && !fReplyReady) {
			fReplyLen = len < sizeof(fReply) ? len : sizeof(fReply);
			bcopy(msg, fReply, fReplyLen);
			fReplyReady = true;
			IOLockWakeup(fXactLock, &fReplyReady, false);
		}
		IOLockUnlock(fXactLock);
	}
}

void
PDHVNetwork::receiveAction(IOInterruptEventSource *src, int count)
{
	(void)src;
	(void)count;
	bool received = false;

	for (;;) {
		uint32_t len = HN_PKTBUF_LEN;
		IOReturn ret = fChannel->recvPacket(fPktBuf, &len);
		if (ret == kIOReturnNoSpace) {
			NET_LOG("dropping oversized channel packet (%u bytes)", len);
			break;
		}
		if (ret != kIOReturnSuccess) {
			break;
		}
		const struct vmbus_chanpkt_hdr *hdr = (const struct vmbus_chanpkt_hdr *)fPktBuf;
		uint32_t hlen = (uint32_t)hdr->hlen << 3;
		const uint8_t *data = fPktBuf + hlen;
		uint32_t dlen = len - hlen;

		switch (hdr->type) {
		case VMBUS_CHANPKT_TYPE_COMP:
			if (hdr->xactid >= 1 && hdr->xactid <= HN_TX_SLOTS) {
				freeSlot((int)hdr->xactid - 1);
			} else {
				IOLockLock(fXactLock);
				if (fWaitXact != 0 && hdr->xactid == fWaitXact && !fReplyReady) {
					fReplyLen = dlen < sizeof(fReply) ? dlen : sizeof(fReply);
					bcopy(data, fReply, fReplyLen);
					fReplyReady = true;
					IOLockWakeup(fXactLock, &fReplyReady, false);
				}
				IOLockUnlock(fXactLock);
			}
			break;

		case VMBUS_CHANPKT_TYPE_RXBUF: {
			// rxbuf_id, count, then (len, offset) pairs into the receive buffer
			if (hlen < 24 || dlen < 4 || get32(data, 0) != HN_NVS_TYPE_RNDIS) {
				break;
			}
			uint16_t sig;
			bcopy(fPktBuf + 16, &sig, sizeof(sig));
			uint32_t n = get32(fPktBuf, 20);
			if (sig != HN_NVS_RXBUF_SIG || 24 + (uint64_t)n * 8 > hlen) {
				break;
			}
			for (uint32_t i = 0; i < n; i++) {
				uint32_t rlen = get32(fPktBuf, 24 + i * 8);
				uint32_t roff = get32(fPktBuf, 28 + i * 8);
				if ((uint64_t)roff + rlen > (uint64_t)HN_RXBUF_PAGES * PAGE_SIZE) {
					continue;
				}
				handleRndis((const uint8_t *)fRxBuf.va + roff, rlen);
				received = true;
			}
			struct hn_nvs_msg ack;
			bzero(&ack, sizeof(ack));
			put32(ack.bytes, 0, HN_NVS_TYPE_RNDIS_ACK);
			put32(ack.bytes, 4, HN_NVS_STATUS_OK);
			for (int tries = 0; tries < 100; tries++) {
				if (fChannel->send(VMBUS_CHANPKT_TYPE_COMP, 0, &ack, sizeof(ack), hdr->xactid) != kIOReturnNoSpace) {
					break;
				}
				IODelay(100);
			}
			break;
		}

		default:
			break;          // NVS notifications (send table, VF association) are not used
		}
	}
	fChannel->recvDone();
	if (received && fInterface != NULL) {
		fInterface->flushInputQueue();
	}
}

bool
PDHVNetwork::publishMedium(void)
{
	OSDictionary *dict = OSDictionary::withCapacity(1);
	IONetworkMedium *medium = IONetworkMedium::medium(kIOMediumEthernetAuto | kIOMediumOptionFullDuplex,
	    10000000000ULL);
	bool ok = dict != NULL && medium != NULL && IONetworkMedium::addMedium(dict, medium) &&
	    publishMediumDictionary(dict) && setCurrentMedium(medium);
	OSSafeReleaseNULL(medium);
	OSSafeReleaseNULL(dict);
	return ok;
}

void
PDHVNetwork::setPacketFilter(void)
{
	uint32_t filter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST |
	    NDIS_PACKET_TYPE_ALL_MULTICAST | NDIS_PACKET_TYPE_BROADCAST;
	if (fPromiscuous) {
		filter |= NDIS_PACKET_TYPE_PROMISCUOUS;
	}
	if (!fEnabled) {
		filter = 0;
	}
	uint8_t reply[64];
	uint32_t replyLen = sizeof(reply);
	IOReturn ret = rndisTransact(REMOTE_NDIS_SET_MSG, OID_GEN_CURRENT_PACKET_FILTER, &filter, sizeof(filter),
	    reply, &replyLen);
	if (ret != kIOReturnSuccess) {
		NET_LOG("setting packet filter 0x%x failed: 0x%x", filter, ret);
	} else {
		NET_LOG("packet filter 0x%x", filter);
	}
}

bool
PDHVNetwork::start(IOService *provider)
{
	if (!super::start(provider)) {
		return false;
	}
	fChannel = OSDynamicCast(PDVMBusChannel, provider);
	fLock = IOLockAlloc();
	fXactLock = IOLockAlloc();
	fPktBuf = (uint8_t *)IOMalloc(HN_PKTBUF_LEN);
	fNextRid = 1;
	fFilterCall = thread_call_allocate(filterCall, this);
	if (fChannel == NULL || fLock == NULL || fXactLock == NULL || fPktBuf == NULL ||
	    !PDHyperV::allocPages(&fRxBuf, HN_RXBUF_PAGES) || !PDHyperV::allocPages(&fTxPool, HN_TX_SLOTS)) {
		NET_LOG("out of memory");
		return false;
	}

	IOWorkLoop *wl = getWorkLoop();
	fRxSource = IOInterruptEventSource::interruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventAction, this, &PDHVNetwork::receiveAction));
	if (wl == NULL || fRxSource == NULL || wl->addEventSource(fRxSource) != kIOReturnSuccess) {
		return false;
	}
	fRxSource->enable();

	IOReturn ret = fChannel->open(HN_RING_PAGES, HN_RING_PAGES, this, channelCallback);
	if (ret != kIOReturnSuccess) {
		NET_LOG("channel %u open failed: 0x%x", fChannel->channelID(), ret);
		return false;
	}

	uint8_t resp[64];
	for (size_t i = 0; i < sizeof(kNvsVersions) / sizeof(kNvsVersions[0]); i++) {
		struct hn_nvs_msg init;
		bzero(&init, sizeof(init));
		put32(init.bytes, 0, HN_NVS_TYPE_INIT);
		put32(init.bytes, 4, kNvsVersions[i]);
		put32(init.bytes, 8, kNvsVersions[i]);
		if (nvsTransact(&init, HN_NVS_TYPE_INIT_RESP, resp, sizeof(resp)) == kIOReturnSuccess &&
		    get32(resp, 12) == HN_NVS_STATUS_OK) {
			fNvsVersion = kNvsVersions[i];
			break;
		}
	}
	if (fNvsVersion == 0) {
		NET_LOG("host accepted no NVS version");
		return false;
	}
	uint32_t ndis = fNvsVersion <= HN_NVS_VERSION_4 ? HN_NDIS_VERSION_6_1 : HN_NDIS_VERSION_6_30;

	struct hn_nvs_msg msg;
	bzero(&msg, sizeof(msg));
	put32(msg.bytes, 0, HN_NVS_TYPE_NDIS_CONF);
	put32(msg.bytes, 4, HN_MAX_FRAME);
	uint64_t caps = HN_NVS_NDIS_CONF_VLAN;
	bcopy(&caps, msg.bytes + 12, sizeof(caps));
	(void)fChannel->send(VMBUS_CHANPKT_TYPE_INBAND, 0, &msg, sizeof(msg), 0);

	bzero(&msg, sizeof(msg));
	put32(msg.bytes, 0, HN_NVS_TYPE_NDIS_INIT);
	put32(msg.bytes, 4, ndis >> 16);
	put32(msg.bytes, 8, ndis & 0xffff);
	(void)fChannel->send(VMBUS_CHANPKT_TYPE_INBAND, 0, &msg, sizeof(msg), 0);

	// The host copies received frames into this buffer and tells us where
	ret = fChannel->bus()->gpadlConnect(fChannel->channelID(), &fRxBuf, &fRxGpadl);
	if (ret != kIOReturnSuccess) {
		NET_LOG("receive buffer GPADL failed: 0x%x", ret);
		return false;
	}
	bzero(&msg, sizeof(msg));
	put32(msg.bytes, 0, HN_NVS_TYPE_RXBUF_CONN);
	put32(msg.bytes, 4, fRxGpadl);
	uint16_t sig = HN_NVS_RXBUF_SIG;
	bcopy(&sig, msg.bytes + 8, sizeof(sig));
	if (nvsTransact(&msg, HN_NVS_TYPE_RXBUF_CONNRESP, resp, sizeof(resp)) != kIOReturnSuccess ||
	    get32(resp, 4) != HN_NVS_STATUS_OK) {
		NET_LOG("host refused the receive buffer");
		return false;
	}

	uint8_t reply[128];
	uint32_t replyLen = sizeof(reply);
	ret = rndisTransact(REMOTE_NDIS_INITIALIZE_MSG, 0, NULL, 0, reply, &replyLen);
	if (ret != kIOReturnSuccess) {
		NET_LOG("RNDIS initialize failed: 0x%x", ret);
		return false;
	}
	replyLen = sizeof(reply);
	ret = rndisTransact(REMOTE_NDIS_QUERY_MSG, OID_802_3_PERMANENT_ADDRESS, NULL, 0, reply, &replyLen);
	uint32_t infoLen = replyLen >= 24 ? get32(reply, 16) : 0;
	uint32_t infoOff = replyLen >= 24 ? get32(reply, 20) + 8 : 0;
	if (ret != kIOReturnSuccess || infoLen < 6 || infoOff + 6 > replyLen) {
		NET_LOG("MAC address query failed: 0x%x", ret);
		return false;
	}
	bcopy(reply + infoOff, fMAC.bytes, 6);
	NET_LOG("NVS 0x%x, NDIS %u.%u, MAC %02x:%02x:%02x:%02x:%02x:%02x", fNvsVersion, ndis >> 16, ndis & 0xffff,
	    fMAC.bytes[0], fMAC.bytes[1], fMAC.bytes[2], fMAC.bytes[3], fMAC.bytes[4], fMAC.bytes[5]);

	if (!publishMedium() || !attachInterface((IONetworkInterface **)&fInterface, true)) {
		NET_LOG("interface attach failed");
		return false;
	}
	setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, getCurrentMedium());
	registerService();
	return true;
}

void
PDHVNetwork::stop(IOService *provider)
{
	if (fEnabled) {
		disable(fInterface);
	}
	if (fInterface != NULL) {
		detachInterface(fInterface, true);
		OSSafeReleaseNULL(fInterface);
	}
	if (fRxSource != NULL) {
		fRxSource->disable();
		getWorkLoop()->removeEventSource(fRxSource);
		OSSafeReleaseNULL(fRxSource);
	}
	super::stop(provider);
}

// enable/disable run on the work loop that also delivers RNDIS completions,
// so waiting there for the filter reply could only time out. Update it from a thread call
void
PDHVNetwork::filterCall(thread_call_param_t p0, thread_call_param_t p1)
{
	(void)p1;
	((PDHVNetwork *)p0)->setPacketFilter();
}

IOReturn
PDHVNetwork::enable(IONetworkInterface *interface)
{
	(void)interface;
	fEnabled = true;
	thread_call_enter(fFilterCall);
	return kIOReturnSuccess;
}

IOReturn
PDHVNetwork::disable(IONetworkInterface *interface)
{
	(void)interface;
	fEnabled = false;
	thread_call_enter(fFilterCall);
	return kIOReturnSuccess;
}

UInt32
PDHVNetwork::outputPacket(mbuf_t m, void *param)
{
	(void)param;
	size_t len = mbuf_pkthdr_len(m);
	int slot = len > 0 && len <= HN_MAX_FRAME ? allocSlot() : -1;
	if (slot < 0) {
		freePacket(m);
		return kIOReturnOutputDropped;
	}

	uint8_t *page = (uint8_t *)fTxPool.va + (vm_size_t)slot * PAGE_SIZE;
	struct rndis_packet_msg *pkt = (struct rndis_packet_msg *)page;
	bzero(pkt, sizeof(*pkt));
	pkt->rm_type = REMOTE_NDIS_PACKET_MSG;
	pkt->rm_len = (uint32_t)(sizeof(*pkt) + len);
	pkt->rm_dataoffset = sizeof(*pkt) - 8;     // Relative to rm_dataoffset
	pkt->rm_datalen = (uint32_t)len;
	mbuf_copydata(m, 0, len, page + sizeof(*pkt));
	freePacket(m);

	if (sendRndis(slot, HN_NVS_RNDIS_MTYPE_DATA, pkt->rm_len) != kIOReturnSuccess) {
		freeSlot(slot);
		return kIOReturnOutputDropped;
	}
	return kIOReturnOutputSuccess;
}

IOReturn
PDHVNetwork::getHardwareAddress(IOEthernetAddress *addr)
{
	if (addr == NULL) {
		return kIOReturnBadArgument;
	}
	*addr = fMAC;
	return kIOReturnSuccess;
}

IOReturn
PDHVNetwork::setPromiscuousMode(bool active)
{
	fPromiscuous = active;
	if (fEnabled) {
		thread_call_enter(fFilterCall);
	}
	return kIOReturnSuccess;
}

// The filter always accepts all multicast, so the list itself does not need programming
IOReturn
PDHVNetwork::setMulticastMode(bool active)
{
	(void)active;
	return kIOReturnSuccess;
}

IOReturn
PDHVNetwork::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
	(void)addrs;
	(void)count;
	return kIOReturnSuccess;
}

const OSString *
PDHVNetwork::newVendorString() const
{
	return OSString::withCString("Microsoft");
}

const OSString *
PDHVNetwork::newModelString() const
{
	return OSString::withCString("Hyper-V synthetic network adapter");
}

// Like the other PureDarwin network drivers, output goes straight to outputPacket
IOOutputQueue *
PDHVNetwork::createOutputQueue()
{
	return NULL;
}
