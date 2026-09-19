// Hyper-V synthetic network adapter (netvsc), NVS and RNDIS over a VMBus channel.
// Layouts follow FreeBSD's BSD-licensed netvsc and OpenBSD's ISC-licensed sys/net/rndis.h
#ifndef PD_HV_NETWORK_H
#define PD_HV_NETWORK_H

#include <IOKit/IOInterruptEventSource.h>
#include <kern/thread_call.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include "PDHyperV.h"

#define HN_NVS_VERSION_61           0x60001
#define HN_NVS_VERSION_6            0x60000
#define HN_NVS_VERSION_5            0x50000
#define HN_NVS_VERSION_4            0x40000
#define HN_NVS_VERSION_2            0x30002
#define HN_NDIS_VERSION_6_30        0x0006001e
#define HN_NDIS_VERSION_6_1         0x00060001

#define HN_NVS_TYPE_INIT            1
#define HN_NVS_TYPE_INIT_RESP       2
#define HN_NVS_TYPE_NDIS_INIT       100
#define HN_NVS_TYPE_RXBUF_CONN      101
#define HN_NVS_TYPE_RXBUF_CONNRESP  102
#define HN_NVS_TYPE_RNDIS           107
#define HN_NVS_TYPE_RNDIS_ACK       108
#define HN_NVS_TYPE_NDIS_CONF       125
#define HN_NVS_STATUS_OK            1
#define HN_NVS_RXBUF_SIG            0xcafe
#define HN_NVS_CHIM_IDX_INVALID     0xffffffff
#define HN_NVS_RNDIS_MTYPE_DATA     0
#define HN_NVS_RNDIS_MTYPE_CTRL     1
#define HN_NVS_NDIS_CONF_VLAN       0x0008
#define HN_NVS_REQSIZE              40      /* the host ignores shorter NVS requests */

#define REMOTE_NDIS_PACKET_MSG          0x00000001
#define REMOTE_NDIS_INITIALIZE_MSG      0x00000002
#define REMOTE_NDIS_QUERY_MSG           0x00000004
#define REMOTE_NDIS_SET_MSG             0x00000005
#define REMOTE_NDIS_INDICATE_STATUS_MSG 0x00000007
#define REMOTE_NDIS_CMPLT_FLAG          0x80000000
#define RNDIS_STATUS_SUCCESS            0x00000000
#define RNDIS_STATUS_MEDIA_CONNECT      0x4001000B
#define RNDIS_STATUS_MEDIA_DISCONNECT   0x4001000C
#define OID_GEN_CURRENT_PACKET_FILTER   0x0001010E
#define OID_GEN_MEDIA_CONNECT_STATUS    0x00010114
#define OID_802_3_PERMANENT_ADDRESS     0x01010101
#define NDIS_PACKET_TYPE_DIRECTED       0x00000001
#define NDIS_PACKET_TYPE_MULTICAST      0x00000002
#define NDIS_PACKET_TYPE_ALL_MULTICAST  0x00000004
#define NDIS_PACKET_TYPE_BROADCAST      0x00000008
#define NDIS_PACKET_TYPE_PROMISCUOUS    0x00000020

// NVS requests are fixed-size. Fields are packed at the offsets FreeBSD's structs give
struct hn_nvs_msg {
	uint8_t bytes[HN_NVS_REQSIZE];
};

struct rndis_packet_msg {
	uint32_t rm_type;
	uint32_t rm_len;
	uint32_t rm_dataoffset;
	uint32_t rm_datalen;
	uint32_t rm_oobdataoffset;
	uint32_t rm_oobdatalen;
	uint32_t rm_oobdataelements;
	uint32_t rm_pktinfooffset;
	uint32_t rm_pktinfolen;
	uint32_t rm_vchandle;
	uint32_t rm_reserved;
} __attribute__((packed));

#define HN_RING_PAGES       64
#define HN_RXBUF_PAGES      1024    /* 4 MiB */
#define HN_TX_SLOTS         128     /* one page per in-flight packet or control request */
#define HN_MAX_FRAME        1514
#define HN_PKTBUF_LEN       16384

class PDHVNetwork : public IOEthernetController
{
	OSDeclareDefaultStructors(PDHVNetwork);

public:
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
	static void channelCallback(OSObject *target, PDVMBusChannel *chan);
	static void filterCall(thread_call_param_t p0, thread_call_param_t p1);
	void receiveAction(IOInterruptEventSource *src, int count);
	void handleRndis(const uint8_t *msg, uint32_t len);

	IOReturn nvsTransact(const struct hn_nvs_msg *req, uint32_t respType, uint8_t *resp, uint32_t respLen);
	IOReturn rndisTransact(uint32_t type, uint32_t oid, const void *info, uint32_t infoLen,
	    uint8_t *reply, uint32_t *replyLen);
	int allocSlot(void);
	void freeSlot(int slot);
	IOReturn sendRndis(int slot, uint32_t mtype, uint32_t len);
	bool publishMedium(void);
	void setPacketFilter(void);

	PDVMBusChannel *fChannel;
	IOEthernetInterface *fInterface;
	IOInterruptEventSource *fRxSource;
	IOLock *fLock;
	IOEthernetAddress fMAC;
	struct pd_hv_mem fRxBuf;
	struct pd_hv_mem fTxPool;
	uint32_t fRxGpadl;
	uint32_t fNvsVersion;
	uint8_t *fPktBuf;
	bool fEnabled;
	bool fPromiscuous;
	thread_call_t fFilterCall;

	// Slots in use. a data or control send owns its page until the host completes it
	uint64_t fSlotUsed[(HN_TX_SLOTS + 63) / 64];

	// One NVS or RNDIS control exchange at a time
	IOLock *fXactLock;
	uint64_t fWaitXact;
	uint32_t fWaitRid;
	bool fReplyReady;
	uint8_t fReply[512];
	uint32_t fReplyLen;
	uint32_t fNextRid;
};

#endif
