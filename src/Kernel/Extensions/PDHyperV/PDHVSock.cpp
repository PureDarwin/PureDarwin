#include "PDHVSock.h"

#include <IOKit/IOLib.h>

extern "C" {
#include <sys/kpi_mbuf.h>
#include <sys/vsock.h>
#include <sys/vsock_transport.h>
#include <kern/thread_call.h>
}

#define HVS_LOG(fmt, ...) IOLog("PDHVSock: " fmt "\n", ##__VA_ARGS__)

#define HVS_GUEST_CID   3
#define HVS_RING_PAGES  6
#define HVS_MTU         16384
#define HVS_BUF_ALLOC   0x7fffffffU
#define HVS_MAX_CONN    64

struct vmpipe_hdr {
	uint32_t type;
	uint32_t size;
} __attribute__((packed));

struct vmbus_chanmsg_tl_connect {
	struct vmbus_chanmsg_hdr hdr;
	struct hv_guid guest_endpoint;
	struct hv_guid host_service;
} __attribute__((packed));

struct vmbus_chanmsg_tl_result {
	struct vmbus_chanmsg_hdr hdr;
	struct hv_guid guest_endpoint;
	struct hv_guid host_service;
	uint32_t status;
} __attribute__((packed));

struct vmbus_chanmsg_chanid {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t chanid;
} __attribute__((packed));

enum { HVS_FREE, HVS_CONNECTING, HVS_OPEN };

struct hvs_conn {
	uint32_t state;
	bool from_host;
	struct vsock_address local;     // guest end
	struct vsock_address remote;    // host end
	struct vmbus_chanmsg_choffer offer;
	PDVMBusChannel *chan;

	// The guest socket's receive credit, as last reported by the vsock domain
	uint32_t peer_buf_alloc;
	uint32_t peer_fwd;
	uint32_t rx_delivered;
	uint32_t tx_accepted;

	bool job_open, job_close;
	bool need_request, need_response, need_credit, need_reset;
	bool fin_sent, fin_rcvd;

	// A host packet read off the ring but not yet accepted by the socket
	uint8_t *rxbuf;
	uint32_t rx_len;

	IOLock *pump_lock;
	thread_call_t pump;
};

// 00000000-facb-11e6-bd58-64006a7986d3. The first four bytes carry the port
static const uint8_t kSrvTemplate[16] = {
	0x00, 0x00, 0x00, 0x00, 0xcb, 0xfa, 0xe6, 0x11,
	0xbd, 0x58, 0x64, 0x00, 0x6a, 0x79, 0x86, 0xd3,
};

static PDHyperV *gBus;
static IOLock *gLock;
static struct hvs_conn gConns[HVS_MAX_CONN];
static thread_call_t gWorker;
static uint32_t gNextHostPort = 0x80000000U;

static void
make_srv_id(struct hv_guid *g, uint32_t port)
{
	bcopy(kSrvTemplate, g->b, sizeof(g->b));
	bcopy(&port, g->b, sizeof(port));
}

static bool
srv_id_port(const struct hv_guid *g, uint32_t *port)
{
	if (bcmp(g->b + 4, kSrvTemplate + 4, 12) != 0) {
		return false;
	}
	bcopy(g->b, port, sizeof(*port));
	return true;
}

// Callers hold gLock
static struct hvs_conn *
find_conn(struct vsock_address local, struct vsock_address remote)
{
	for (int i = 0; i < HVS_MAX_CONN; i++) {
		struct hvs_conn *c = &gConns[i];
		if (c->state != HVS_FREE && c->local.port == local.port &&
		    c->remote.port == remote.port) {
			return c;
		}
	}
	return NULL;
}

static struct hvs_conn *
alloc_conn(void)
{
	for (int i = 0; i < HVS_MAX_CONN; i++) {
		struct hvs_conn *c = &gConns[i];
		if (c->state == HVS_FREE && !c->job_open && !c->job_close && !c->need_reset) {
			uint8_t *rxbuf = c->rxbuf;
			IOLock *pl = c->pump_lock;
			thread_call_t pump = c->pump;
			bzero(c, sizeof(*c));
			c->rxbuf = rxbuf;
			c->pump_lock = pl;
			c->pump = pump;
			return c;
		}
	}
	return NULL;
}

static struct hvs_conn *
conn_for_chanid(uint32_t chanid)
{
	for (int i = 0; i < HVS_MAX_CONN; i++) {
		struct hvs_conn *c = &gConns[i];
		if (c->chan != NULL && c->offer.chanid == chanid) {
			return c;
		}
	}
	return NULL;
}

static IOReturn
send_pipe(struct hvs_conn *c, PDVMBusChannel *chan, const uint8_t *data, uint32_t len)
{
	uint8_t *pkt = (uint8_t *)IOMalloc(sizeof(struct vmpipe_hdr) + len);
	if (pkt == NULL) {
		return kIOReturnNoMemory;
	}
	struct vmpipe_hdr hdr = { 1, len };
	bcopy(&hdr, pkt, sizeof(hdr));
	if (len > 0) {
		bcopy(data, pkt + sizeof(hdr), len);
	}
	IOReturn ret;
	// Ring full: the host drains it on its own schedule, so poll for up to a minute
	for (int tries = 0; tries < 30000; tries++) {
		ret = chan->send(VMBUS_CHANPKT_TYPE_INBAND, 0, pkt, sizeof(hdr) + len, 0);
		if (ret != kIOReturnNoSpace) {
			break;
		}
		IOSleep(2);
	}
	IOFree(pkt, sizeof(struct vmpipe_hdr) + len);
	return ret;
}

static void
kick(struct hvs_conn *c)
{
	thread_call_enter(c->pump);
}

static void
channel_event(OSObject *target, PDVMBusChannel *chan)
{
	(void)target;
	IOLockLock(gLock);
	struct hvs_conn *c = conn_for_chanid(chan->channelID());
	IOLockUnlock(gLock);
	if (c != NULL) {
		kick(c);
	}
}

static errno_t
hvs_get_cid(void *provider, uint32_t *cid)
{
	(void)provider;
	*cid = HVS_GUEST_CID;
	return 0;
}

static errno_t
hvs_attach_socket(void *provider)
{
	(void)provider;
	return 0;
}

static errno_t
hvs_detach_socket(void *provider)
{
	(void)provider;
	return 0;
}

static errno_t
hvs_put_message(void *provider, struct vsock_address src, struct vsock_address dst,
    enum vsock_operation op, uint32_t buf_alloc, uint32_t fwd_cnt, mbuf_t m)
{
	(void)provider;
	errno_t error = 0;

	IOLockLock(gLock);
	struct hvs_conn *c = find_conn(src, dst);
	if (c != NULL) {
		c->peer_buf_alloc = buf_alloc;
		c->peer_fwd = fwd_cnt;
	}

	switch (op) {
	case VSOCK_REQUEST: {
		if (c != NULL || (c = alloc_conn()) == NULL) {
			IOLockUnlock(gLock);
			error = EADDRINUSE;
			break;
		}
		c->state = HVS_CONNECTING;
		c->local = src;
		c->remote = dst;
		c->peer_buf_alloc = buf_alloc;
		c->peer_fwd = fwd_cnt;
		IOLockUnlock(gLock);

		struct vmbus_chanmsg_tl_connect req;
		bzero(&req, sizeof(req));
		req.hdr.type = VMBUS_CHANMSG_TL_CONN;
		make_srv_id(&req.guest_endpoint, src.port);
		make_srv_id(&req.host_service, dst.port);
		if (gBus->postMessage(&req, sizeof(req)) != 0) {
			IOLockLock(gLock);
			c->state = HVS_FREE;
			IOLockUnlock(gLock);
			error = EIO;
		}
		break;
	}
	case VSOCK_PAYLOAD: {
		PDVMBusChannel *chan = (c != NULL && c->state == HVS_OPEN) ? c->chan : NULL;
		IOLockUnlock(gLock);
		if (chan == NULL) {
			error = ENOTCONN;
			break;
		}
		uint8_t chunk[1024];
		size_t total = mbuf_pkthdr_len(m);
		for (size_t off = 0; off < total && error == 0;) {
			uint32_t n = (uint32_t)(total - off < HVS_MTU ? total - off : HVS_MTU);
			uint8_t *buf = n <= sizeof(chunk) ? chunk : (uint8_t *)IOMalloc(n);
			if (buf == NULL || mbuf_copydata(m, off, n, buf) != 0 ||
			    send_pipe(c, chan, buf, n) != kIOReturnSuccess) {
				error = EIO;
			}
			if (buf != NULL && buf != chunk) {
				IOFree(buf, n);
			}
			off += n;
		}
		if (error == 0) {
			IOLockLock(gLock);
			c->tx_accepted += (uint32_t)total;
			IOLockUnlock(gLock);
		}
		break;
	}
	case VSOCK_SHUTDOWN:
	case VSOCK_SHUTDOWN_SEND:
	case VSOCK_RESET: {
		PDVMBusChannel *chan = NULL;
		if (c != NULL && c->state == HVS_OPEN && !c->fin_sent) {
			c->fin_sent = true;
			chan = c->chan;
		} else if (c != NULL && c->state == HVS_CONNECTING && op == VSOCK_RESET && !c->job_open) {
			c->state = HVS_FREE;
		}
		IOLockUnlock(gLock);
		if (chan != NULL) {
			send_pipe(c, chan, NULL, 0);
		}
		break;
	}
	case VSOCK_CREDIT_REQUEST:
		if (c != NULL) {
			c->need_credit = true;
		}
		IOLockUnlock(gLock);
		if (c != NULL) {
			kick(c);
		}
		break;
	case VSOCK_RESPONSE:
	case VSOCK_CREDIT_UPDATE:
		IOLockUnlock(gLock);
		if (c != NULL) {
			kick(c);
		}
		break;
	default:
		IOLockUnlock(gLock);
		break;
	}

	if (m != NULL) {
		mbuf_freem_list(m);
	}
	return error;
}

static struct vsock_transport gTransport = {
	.protocol = VSOCK_PROTO_STANDARD,
	.provider = NULL,
	.get_cid = hvs_get_cid,
	.attach_socket = hvs_attach_socket,
	.detach_socket = hvs_detach_socket,
	.put_message = hvs_put_message,
};

static void
post_chanid(uint32_t type, uint32_t chanid)
{
	struct vmbus_chanmsg_chanid msg;
	bzero(&msg, sizeof(msg));
	msg.hdr.type = type;
	msg.chanid = chanid;
	gBus->postMessage(&msg, sizeof(msg));
}

static void
worker(thread_call_param_t p0, thread_call_param_t p1)
{
	(void)p0;
	(void)p1;
	for (int i = 0; i < HVS_MAX_CONN; i++) {
		struct hvs_conn *c = &gConns[i];

		IOLockLock(gLock);
		bool open = c->job_open;
		c->job_open = false;
		struct vmbus_chanmsg_choffer offer = c->offer;
		IOLockUnlock(gLock);
		if (open) {
			PDVMBusChannel *chan = OSTypeAlloc(PDVMBusChannel);
			IOReturn ret = kIOReturnNoMemory;
			if (chan != NULL && chan->initWithOffer(gBus, &offer)) {
				IOLockLock(gLock);
				c->chan = chan;
				IOLockUnlock(gLock);
				ret = chan->open(HVS_RING_PAGES, HVS_RING_PAGES, chan, channel_event);
			}
			IOLockLock(gLock);
			if (ret == kIOReturnSuccess) {
				c->state = HVS_OPEN;
				if (c->from_host) {
					c->need_request = true;
				} else {
					c->need_response = true;
				}
			} else {
				HVS_LOG("opening chan %u failed: 0x%x", offer.chanid, ret);
				c->need_reset = true;
			}
			IOLockUnlock(gLock);
			kick(c);
		}

		IOLockLock(gLock);
		bool close = c->job_close;
		c->job_close = false;
		PDVMBusChannel *chan = close ? c->chan : NULL;
		IOLockUnlock(gLock);
		if (close) {
			// The host has rescinded the offer. deliver what is left, then tear down
			thread_call_enter(c->pump);
			IOLockLock(c->pump_lock);
			IOLockUnlock(c->pump_lock);
			if (chan != NULL) {
				gBus->unregisterChannel(offer.chanid);
				post_chanid(VMBUS_CHANMSG_CHCLOSE, offer.chanid);
			}
			post_chanid(VMBUS_CHANMSG_CHFREE, offer.chanid);
			IOLockLock(gLock);
			c->chan = NULL;
			c->need_reset = true;
			IOLockUnlock(gLock);
			kick(c);
			// Rings stay allocated: without a GPADL teardown the host may still map them
		}
	}
}

static void
deliver(struct hvs_conn *c, enum vsock_operation op, mbuf_t m)
{
	IOLockLock(gLock);
	struct vsock_address local = c->local, remote = c->remote;
	uint32_t fwd = c->tx_accepted;
	IOLockUnlock(gLock);
	vsock_put_message(remote, local, op, HVS_BUF_ALLOC, fwd, m, VSOCK_PROTO_STANDARD);
}

static bool
drain(struct hvs_conn *c)
{
	bool progress = false;

	for (;;) {
		IOLockLock(gLock);
		PDVMBusChannel *chan = c->state == HVS_OPEN ? c->chan : NULL;
		bool fin = c->fin_rcvd;
		uint32_t pending = c->rx_len;
		bool room = c->peer_buf_alloc != 0 &&
		    c->rx_delivered - c->peer_fwd + pending <= c->peer_buf_alloc;
		IOLockUnlock(gLock);
		if (chan == NULL || fin) {
			return progress;
		}

		if (pending > 0) {
			if (!room) {
				return progress;
			}
			mbuf_t m = NULL;
			if (mbuf_allocpacket(MBUF_WAITOK, pending, NULL, &m) != 0 ||
			    mbuf_copyback(m, 0, pending, c->rxbuf + sizeof(struct vmpipe_hdr), MBUF_WAITOK) != 0) {
				if (m != NULL) {
					mbuf_freem(m);
				}
				return progress;
			}
			deliver(c, VSOCK_PAYLOAD, m);
			IOLockLock(gLock);
			c->rx_delivered += pending;
			c->rx_len = 0;
			IOLockUnlock(gLock);
			progress = true;
			continue;
		}

		uint32_t len = HVS_MTU + sizeof(struct vmpipe_hdr);
		uint64_t xactid;
		uint16_t type;
		IOReturn ret = chan->recv(c->rxbuf, &len, &xactid, &type);
		chan->recvDone();
		if (ret != kIOReturnSuccess) {
			return progress;
		}
		progress = true;
		if (type != VMBUS_CHANPKT_TYPE_INBAND || len < sizeof(struct vmpipe_hdr)) {
			continue;
		}
		struct vmpipe_hdr hdr;
		bcopy(c->rxbuf, &hdr, sizeof(hdr));
		if (hdr.size == 0) {
			IOLockLock(gLock);
			c->fin_rcvd = true;
			IOLockUnlock(gLock);
			deliver(c, VSOCK_SHUTDOWN_SEND, NULL);
			return progress;
		}
		IOLockLock(gLock);
		c->rx_len = hdr.size < len - sizeof(hdr) ? hdr.size : len - (uint32_t)sizeof(hdr);
		IOLockUnlock(gLock);
	}
}

static void
pump(thread_call_param_t p0, thread_call_param_t p1)
{
	struct hvs_conn *c = (struct hvs_conn *)p0;
	(void)p1;

	IOLockLock(c->pump_lock);
	for (bool again = true; again;) {
		again = false;
		IOLockLock(gLock);
		bool request = c->need_request, response = c->need_response, credit = c->need_credit;
		c->need_request = c->need_response = c->need_credit = false;
		IOLockUnlock(gLock);

		if (request) {
			deliver(c, VSOCK_REQUEST, NULL);
			again = true;
		}
		if (response) {
			deliver(c, VSOCK_RESPONSE, NULL);
			again = true;
		}
		if (credit) {
			deliver(c, VSOCK_CREDIT_UPDATE, NULL);
		}
		if (drain(c)) {
			again = true;
		}

		IOLockLock(gLock);
		bool reset = c->need_reset && c->chan == NULL && !c->job_open && !c->job_close;
		if (reset) {
			c->need_reset = false;
		}
		IOLockUnlock(gLock);
		if (reset) {
			deliver(c, VSOCK_RESET, NULL);
			IOLockLock(gLock);
			c->state = HVS_FREE;
			IOLockUnlock(gLock);
		}
	}
	IOLockUnlock(c->pump_lock);
}

void
pd_hvsock_init(PDHyperV *bus)
{
	gBus = bus;
	gLock = IOLockAlloc();
	gWorker = thread_call_allocate(worker, NULL);
	for (int i = 0; i < HVS_MAX_CONN; i++) {
		gConns[i].rxbuf = (uint8_t *)IOMalloc(HVS_MTU + sizeof(struct vmpipe_hdr));
		gConns[i].pump_lock = IOLockAlloc();
		gConns[i].pump = thread_call_allocate(pump, &gConns[i]);
	}
	gTransport.provider = bus;
	errno_t err = vsock_add_transport(&gTransport);
	HVS_LOG("vsock transport %s (%d)", err == 0 ? "registered" : "not registered", err);
}

void
pd_hvsock_offer(const struct vmbus_chanmsg_choffer *offer)
{
	uint32_t port, remote;
	bool from_host = offer->udata[4] != 0;

	IOLockLock(gLock);
	struct hvs_conn *c = NULL;
	if (from_host) {
		// The type GUID names the guest port the host dialled
		if (srv_id_port(&offer->chtype, &port) && (c = alloc_conn()) != NULL) {
			if (!srv_id_port(&offer->chinst, &remote)) {
				remote = gNextHostPort++;
			}
			c->state = HVS_CONNECTING;
			c->from_host = true;
			c->local.cid = HVS_GUEST_CID;
			c->local.port = port;
			c->remote.cid = VMADDR_CID_HOST;
			c->remote.port = remote;
		}
	} else if (srv_id_port(&offer->chinst, &port)) {
		for (int i = 0; i < HVS_MAX_CONN; i++) {
			if (gConns[i].state == HVS_CONNECTING && !gConns[i].from_host &&
			    gConns[i].chan == NULL && gConns[i].local.port == port) {
				c = &gConns[i];
				break;
			}
		}
	}
	if (c != NULL) {
		c->offer = *offer;
		c->job_open = true;
	}
	IOLockUnlock(gLock);

	if (c == NULL) {
		HVS_LOG("unclaimed socket offer chan %u (%s host)", offer->chanid, from_host ? "from" : "to");
		return;
	}
	thread_call_enter(gWorker);
}

void
pd_hvsock_rescind(uint32_t chanid)
{
	IOLockLock(gLock);
	struct hvs_conn *c = NULL;
	for (int i = 0; i < HVS_MAX_CONN; i++) {
		if (gConns[i].state != HVS_FREE && gConns[i].offer.chanid == chanid &&
		    (gConns[i].chan != NULL || gConns[i].job_open)) {
			c = &gConns[i];
			c->job_close = true;
			break;
		}
	}
	IOLockUnlock(gLock);
	if (c != NULL) {
		thread_call_enter(gWorker);
	}
}

void
pd_hvsock_tl_result(const uint8_t *data, uint32_t len)
{
	struct vmbus_chanmsg_tl_result res;
	uint32_t port;

	if (len < sizeof(res)) {
		return;
	}
	bcopy(data, &res, sizeof(res));
	if (res.status == 0 || !srv_id_port(&res.guest_endpoint, &port)) {
		return;
	}
	HVS_LOG("host refused connection from guest port %u: 0x%x", port, res.status);
	IOLockLock(gLock);
	struct hvs_conn *c = NULL;
	for (int i = 0; i < HVS_MAX_CONN; i++) {
		if (gConns[i].state == HVS_CONNECTING && !gConns[i].from_host &&
		    gConns[i].chan == NULL && gConns[i].local.port == port) {
			c = &gConns[i];
			c->need_reset = true;
			break;
		}
	}
	IOLockUnlock(gLock);
	if (c != NULL) {
		kick(c);
	}
}
