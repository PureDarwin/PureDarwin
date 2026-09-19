// Hyper-V guest support: hypercalls, the synthetic interrupt controller, VMBus and its rings.
// Formats follow FreeBSD's BSD-licensed sys/dev/hyperv (Microsoft, NetApp, Citrix)
#ifndef PD_HYPERV_H
#define PD_HYPERV_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOLocks.h>

#define HV_MSR_GUEST_OS_ID      0x40000000
#define HV_MSR_HYPERCALL        0x40000001
#define HV_MSR_VP_INDEX         0x40000002
#define HV_MSR_SCONTROL         0x40000080
#define HV_MSR_SIEFP            0x40000082
#define HV_MSR_SIMP             0x40000083
#define HV_MSR_EOM              0x40000084
#define HV_MSR_SINT0            0x40000090

#define HV_CPUID_MAXLEAF        0x40000000
#define HV_CPUID_INTERFACE      0x40000001
#define HV_CPUID_IDENTITY       0x40000002
#define HV_CPUID_FEATURES       0x40000003
#define HV_IFACE_HYPERV         0x31237648      /* "Hv#1" */
#define HV_FEATURE_HYPERCALL    (1U << 5)
#define HV_FEATURE_SYNIC        (1U << 2)

#define HV_HYPERCALL_POST_MESSAGE 0x005c
#define HV_HYPERCALL_SIGNAL_EVENT 0x005d

#define HV_SINT_MESSAGE         2
#define HV_CONNID_MESSAGE       1
#define HV_CONNID_EVENT         2
#define HV_MSGTYPE_CHANNEL      1

#define VMBUS_MSG_SIZE          256
#define VMBUS_MSG_DSIZE_MAX     240
#define VMBUS_CHAN_MAX          256

struct hv_message {
	uint32_t type;
	uint8_t  dsize;
	uint8_t  flags;
	uint16_t rsvd;
	uint64_t id;
	uint8_t  data[VMBUS_MSG_DSIZE_MAX];
} __attribute__((packed));

struct hv_postmsg_in {
	uint32_t connid;
	uint32_t rsvd;
	uint32_t msgtype;
	uint32_t dsize;
	uint8_t  data[VMBUS_MSG_DSIZE_MAX];
} __attribute__((packed));

struct hv_mon_param {
	uint32_t connid;
	uint16_t evtflag_ofs;
	uint16_t rsvd;
} __attribute__((packed));

struct hv_guid {
	uint8_t b[16];
} __attribute__((packed));

#define VMBUS_CHANMSG_CHOFFER          1
#define VMBUS_CHANMSG_CHREQUEST        3
#define VMBUS_CHANMSG_CHOFFER_DONE     4
#define VMBUS_CHANMSG_CHOPEN           5
#define VMBUS_CHANMSG_CHOPEN_RESP      6
#define VMBUS_CHANMSG_GPADL_CONN       8
#define VMBUS_CHANMSG_GPADL_SUBCONN    9
#define VMBUS_CHANMSG_GPADL_CONNRESP   10
#define VMBUS_CHANMSG_CONNECT          14
#define VMBUS_CHANMSG_CONNECT_RESP     15

#define VMBUS_VERSION_WIN10     ((4 << 16) | 0)
#define VMBUS_VERSION_WIN8_1    ((3 << 16) | 0)
#define VMBUS_VERSION_WIN8      ((2 << 16) | 4)
#define VMBUS_VERSION_WIN7      ((1 << 16) | 1)

#define VMBUS_GPADL_CONN_PGMAX     26
#define VMBUS_GPADL_SUBCONN_PGMAX  28

struct vmbus_chanmsg_hdr {
	uint32_t type;
	uint32_t rsvd;
} __attribute__((packed));

struct vmbus_chanmsg_connect {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t ver;
	uint32_t rsvd;
	uint64_t evtflags;
	uint64_t mnf1;
	uint64_t mnf2;
} __attribute__((packed));

struct vmbus_chanmsg_connect_resp {
	struct vmbus_chanmsg_hdr hdr;
	uint8_t done;
} __attribute__((packed));

struct vmbus_chanmsg_choffer {
	struct vmbus_chanmsg_hdr hdr;
	struct hv_guid chtype;
	struct hv_guid chinst;
	uint64_t chlat;
	uint32_t chrev;
	uint32_t svrctx_sz;
	uint16_t chflags;
	uint16_t mmio_sz;
	uint8_t  udata[120];
	uint16_t subidx;
	uint16_t rsvd;
	uint32_t chanid;
	uint8_t  montrig;
	uint8_t  flags1;
	uint16_t flags2;
	uint32_t connid;
} __attribute__((packed));

struct vmbus_chanmsg_chopen {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t chanid;
	uint32_t openid;
	uint32_t gpadl;
	uint32_t vcpuid;
	uint32_t txbr_pgcnt;
	uint8_t  udata[120];
} __attribute__((packed));

struct vmbus_chanmsg_chopen_resp {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t chanid;
	uint32_t openid;
	uint32_t status;
} __attribute__((packed));

struct vmbus_chanmsg_gpadl_conn {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t chanid;
	uint32_t gpadl;
	uint16_t range_len;
	uint16_t range_cnt;
	uint32_t gpa_len;
	uint32_t gpa_ofs;
	uint64_t gpa_page[VMBUS_GPADL_CONN_PGMAX];
} __attribute__((packed));

struct vmbus_chanmsg_gpadl_subconn {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t msgno;
	uint32_t gpadl;
	uint64_t gpa_page[VMBUS_GPADL_SUBCONN_PGMAX];
} __attribute__((packed));

struct vmbus_chanmsg_gpadl_connresp {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t chanid;
	uint32_t gpadl;
	uint32_t status;
} __attribute__((packed));

// A physically contiguous, page-aligned buffer the hypervisor can address
struct pd_hv_mem {
	IOBufferMemoryDescriptor *md;
	void *va;
	uint64_t pa;
	uint32_t pages;
};

class PDVMBusChannel;

class PDHyperV : public IOService
{
	OSDeclareDefaultStructors(PDHyperV);

public:
	virtual bool start(IOService *provider) override;

	static bool allocPages(struct pd_hv_mem *mem, uint32_t pages);

	// Control-channel operations used by channels. Each blocks until the host answers
	uint64_t postMessage(const void *data, uint32_t len);
	IOReturn gpadlConnect(uint32_t chanid, const struct pd_hv_mem *mem, uint32_t *gpadl);
	IOReturn openChannel(uint32_t chanid, uint32_t gpadl, uint32_t txPages);
	void signalChannel(uint32_t chanid, uint32_t connid);
	void registerChannel(uint32_t chanid, PDVMBusChannel *chan);
	void unregisterChannel(uint32_t chanid);

private:
	bool identify(void);
	bool setupHypercall(void);
	bool setupSynic(void);
	bool connectVMBus(void);
	void requestOffers(void);
	void publishChannels(void);
	void publishChannel(const struct vmbus_chanmsg_choffer *offer);
	uint64_t hypercall(uint64_t control, uint64_t inPA);

	static int synicInterrupt(void *state);
	void messageAction(IOInterruptEventSource *src, int count);
	void eventAction(IOInterruptEventSource *src, int count);
	void handleMessage(const struct hv_message *msg);
	IOReturn waitReply(uint32_t type, uint32_t key, struct hv_message *reply);

	IOWorkLoop *fWorkLoop;
	IOInterruptEventSource *fMsgSource;
	IOInterruptEventSource *fEventSource;
	IOLock *fLock;          // hypercall input page
	IOLock *fCtlLock;       // One control transaction at a time

	struct pd_hv_mem fHypercallPage;
	struct pd_hv_mem fInputPage;
	struct pd_hv_mem fMessagePage;
	struct pd_hv_mem fEventPage;
	struct pd_hv_mem fIntrPage;
	struct pd_hv_mem fMonitor1;
	struct pd_hv_mem fMonitor2;

	bool fUseVmmcall;
	uint32_t fVersion;
	uint32_t fNextGpadl;

	// Control replies: the waiter names the type and key (chanid or gpadl) it wants
	bool fReplyReady;
	uint32_t fReplyType;
	uint32_t fReplyKey;
	struct hv_message fReply;

	bool fOffersDone;
	bool fPublished;          // Initial offers published. Later ones go out as they arrive
	uint32_t fOfferCount;
	struct vmbus_chanmsg_choffer fOffers[32];

	PDVMBusChannel *fChannels[VMBUS_CHAN_MAX];

	friend class PDVMBusChannel;
};

// VMBus channel packets (header lengths in units of 8 bytes).
// The ring buffer pair is one GPADL: TX rings first, RX rings follow, each with a page header
#define VMBUS_CHANPKT_TYPE_INBAND  0x0006
#define VMBUS_CHANPKT_TYPE_RXBUF   0x0007
#define VMBUS_CHANPKT_TYPE_GPA     0x0009
#define VMBUS_CHANPKT_TYPE_COMP    0x000b
#define VMBUS_CHANPKT_FLAG_RC      0x0001

struct vmbus_chanpkt_hdr {
	uint16_t type;
	uint16_t hlen;
	uint16_t tlen;
	uint16_t flags;
	uint64_t xactid;
} __attribute__((packed));

struct vmbus_bufring {
	volatile uint32_t windex;
	volatile uint32_t rindex;
	volatile uint32_t imask;
	volatile uint32_t pending_snd_sz;
	uint32_t rsvd1[12];
	uint32_t feature_bits;
	uint8_t  rsvd2[4020];
	uint64_t g2h_intr_cnt;
} __attribute__((packed));

typedef void (*PDVMBusChannelCallback)(OSObject *target, PDVMBusChannel *chan);

class PDVMBusChannel : public IOService
{
	OSDeclareDefaultStructors(PDVMBusChannel);

public:
	bool initWithOffer(PDHyperV *bus, const struct vmbus_chanmsg_choffer *offer);

	// Allocate rings, connect them and open the channel. Callback runs on the bus work loop
	IOReturn open(uint32_t txPages, uint32_t rxPages, OSObject *target, PDVMBusChannelCallback cb);

	IOReturn send(uint16_t type, uint16_t flags, const void *data, uint32_t len, uint64_t xactid);
	// One GPA range of whole pages starting at offset within the first page
	IOReturn sendPages(const uint64_t *pfns, uint32_t npages, uint32_t offset, uint32_t byteLen,
	    const void *data, uint32_t len, uint64_t xactid);
	// Copies up to *len payload bytes. Returns kIOReturnNoFrames when the ring is empty
	IOReturn recv(void *data, uint32_t *len, uint64_t *xactid, uint16_t *type);
	// Copies a whole packet, header included. kIOReturnNoSpace leaves it queued and sets *len
	IOReturn recvPacket(void *packet, uint32_t *len);

	// Wake a host writer that is waiting for room in our receive ring
	void recvDone(void);
	void handleEvent(void);
	uint32_t channelID(void) const { return fOffer.chanid; }
	PDHyperV *bus(void) const { return fBus; }

private:
	IOReturn writeRing(const void *const *parts, const uint32_t *lens, int count);

	PDHyperV *fBus;
	struct vmbus_chanmsg_choffer fOffer;
	struct pd_hv_mem fRing;
	uint32_t fTxPages;
	uint32_t fGpadl;
	IOLock *fTxLock;
	OSObject *fTarget;
	PDVMBusChannelCallback fCallback;
};

#endif
