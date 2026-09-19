#include "PDHyperV.h"
#include "PDHVSock.h"

#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>

extern "C" {
#include <i386/proc_reg.h>
#include <i386/cpuid.h>

uint64_t pd_hv_vmcall(uint64_t control, uint64_t in_pa, uint64_t out_pa);
uint64_t pd_hv_vmmcall(uint64_t control, uint64_t in_pa, uint64_t out_pa);
int pd_hyperv_set_intr_func(int (*func)(void *state));
}

#define super IOService
OSDefineMetaClassAndStructors(PDHyperV, IOService);

#define HV_LOG(fmt, ...) IOLog("PDHyperV: " fmt "\n", ##__VA_ARGS__)

// Messages copied out of the SynIC page by the interrupt, drained on the work loop
#define HV_MSGQ_LEN 64
static struct hv_message gMsgQueue[HV_MSGQ_LEN];
static volatile uint32_t gMsgHead, gMsgTail;
// Channel ids whose event bit the host set, collected by the interrupt
static volatile uint64_t gPendingEvents[VMBUS_CHAN_MAX / 64];
static PDHyperV *gHyperV;

static const uint32_t kVMBusVersions[] = {
	VMBUS_VERSION_WIN10, VMBUS_VERSION_WIN8_1, VMBUS_VERSION_WIN8, VMBUS_VERSION_WIN7,
};

static void
guid_to_str(const struct hv_guid *g, char *buf, size_t len)
{
	const uint8_t *d = g->b;

	snprintf(buf, len, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    d[3], d[2], d[1], d[0], d[5], d[4], d[7], d[6], d[8], d[9],
	    d[10], d[11], d[12], d[13], d[14], d[15]);
}

static const char *
guid_name(const char *s)
{
	static const struct { const char *guid, *name; } known[] = {
		{ "ba6163d9-04a1-4d29-b605-72e2ffb1dc7f", "storage (SCSI)" },
		{ "32412632-86cb-44a2-9b5c-50d1417354f5", "storage (IDE)" },
		{ "f8615163-df3e-46c5-913f-f2d2f965ed0e", "network" },
		{ "cfa8b69e-5b4a-4cc0-b98b-8ba1a1f3f95a", "input (mouse)" },
		{ "f912ad6d-2b17-48ea-bd65-f927a61c7684", "input (keyboard)" },
		{ "da0a7802-e377-4aac-8e77-0558eb1073f8", "video" },
		{ "0e0b6031-5213-4934-818b-38d90ced39db", "shutdown" },
		{ "9527e630-d0ae-497b-adce-e80ab0175caf", "time sync" },
		{ "57164f39-9115-4e78-ab55-382f3bd5422d", "heartbeat" },
		{ "a9a0f4e7-5a45-4d96-b827-8a841e8c03e6", "kvp" },
		{ "525074dc-8985-46e2-8057-a307dc18a502", "dynamic memory" },
		{ "44c4f61d-4444-4400-9d52-802e27ede19f", "PCI passthrough" },
	};
	for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
		if (strcmp(s, known[i].guid) == 0) {
			return known[i].name;
		}
	}
	return "?";
}

bool
PDHyperV::allocPages(struct pd_hv_mem *mem, uint32_t pages)
{
	mem->md = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous, (vm_size_t)pages * PAGE_SIZE,
	    0x0000FFFFFFFFF000ULL);
	if (mem->md == NULL) {
		return false;
	}
	mem->pages = pages;
	mem->va = mem->md->getBytesNoCopy();
	bzero(mem->va, (vm_size_t)pages * PAGE_SIZE);
	mem->pa = mem->md->getPhysicalAddress();
	return mem->pa != 0;
}

bool
PDHyperV::identify(void)
{
	uint32_t r[4];

	do_cpuid(HV_CPUID_MAXLEAF, r);
	if (r[eax] < 0x40000005) {
		return false;
	}
	do_cpuid(HV_CPUID_INTERFACE, r);
	if (r[eax] != HV_IFACE_HYPERV) {
		return false;
	}
	do_cpuid(HV_CPUID_FEATURES, r);
	if (!(r[eax] & HV_FEATURE_HYPERCALL) || !(r[eax] & HV_FEATURE_SYNIC)) {
		HV_LOG("Hyper-V without hypercalls or SynIC (features 0x%x)", r[eax]);
		return false;
	}
	uint32_t features = r[eax];
	do_cpuid(HV_CPUID_IDENTITY, r);
	HV_LOG("Hyper-V %u.%u build %u, features 0x%x", r[ebx] >> 16, r[ebx] & 0xffff, r[eax], features);
	return true;
}

bool
PDHyperV::setupHypercall(void)
{
	// Open-source guest, OS type "other"
	wrmsr64(HV_MSR_GUEST_OS_ID, 0x8000000000000000ULL | (0x7fULL << 56));

	if (!allocPages(&fHypercallPage, 1)) {
		return false;
	}
	uint64_t orig = rdmsr64(HV_MSR_HYPERCALL);
	wrmsr64(HV_MSR_HYPERCALL, (fHypercallPage.pa & ~0xfffULL) | (orig & 0xffeULL) | 1);
	if (!(rdmsr64(HV_MSR_HYPERCALL) & 1)) {
		HV_LOG("hypercall page did not enable");
		return false;
	}

	// The page now holds the host's hypercall sequence. Issue the same instruction
	const uint8_t *p = (const uint8_t *)fHypercallPage.va;
	int kind = 0;
	for (int i = 0; i + 2 < 32 && kind == 0; i++) {
		if (p[i] == 0x0f && p[i + 1] == 0x01 && p[i + 2] == 0xd9) {
			kind = 2;
		} else if (p[i] == 0x0f && p[i + 1] == 0x01 && p[i + 2] == 0xc1) {
			kind = 1;
		}
	}
	if (kind == 0) {
		HV_LOG("unrecognised hypercall page %02x %02x %02x %02x", p[0], p[1], p[2], p[3]);
		return false;
	}
	fUseVmmcall = (kind == 2);
	return allocPages(&fInputPage, 1);
}

uint64_t
PDHyperV::hypercall(uint64_t control, uint64_t inPA)
{
	return fUseVmmcall ? pd_hv_vmmcall(control, inPA, 0) : pd_hv_vmcall(control, inPA, 0);
}

int
PDHyperV::synicInterrupt(void *state)
{
	PDHyperV *hv = gHyperV;
	(void)state;

	if (hv == NULL) {
		return 0;
	}

	// Channel events: the host sets one bit per channel id in our SINT's flag block
	volatile uint64_t *flags = (volatile uint64_t *)((char *)hv->fEventPage.va +
	    HV_SINT_MESSAGE * VMBUS_MSG_SIZE);
	bool events = false;
	for (int i = 0; i < VMBUS_CHAN_MAX / 64; i++) {
		uint64_t v = flags[i];
		while (v != 0 && !OSCompareAndSwap64(v, 0, (UInt64 *)&flags[i])) {
			v = flags[i];
		}
		if (v != 0) {
			gPendingEvents[i] |= v;
			events = true;
		}
	}
	if (events && hv->fEventSource != NULL) {
		hv->fEventSource->interruptOccurred(NULL, NULL, 0);
	}

	struct hv_message *msg = (struct hv_message *)((char *)hv->fMessagePage.va +
	    HV_SINT_MESSAGE * VMBUS_MSG_SIZE);
	if (msg->type != 0) {
		uint32_t next = (gMsgHead + 1) % HV_MSGQ_LEN;
		if (next != gMsgTail) {
			bcopy(msg, &gMsgQueue[gMsgHead], sizeof(*msg));
			gMsgHead = next;
		}
		msg->type = 0;
		wrmsr64(HV_MSR_EOM, 0);
		if (hv->fMsgSource != NULL) {
			hv->fMsgSource->interruptOccurred(NULL, NULL, 0);
		}
	}
	return 1;
}

bool
PDHyperV::setupSynic(void)
{
	if (!allocPages(&fMessagePage, 1) || !allocPages(&fEventPage, 1)) {
		return false;
	}
	int vector = pd_hyperv_set_intr_func(synicInterrupt);

	uint64_t orig = rdmsr64(HV_MSR_SIMP);
	wrmsr64(HV_MSR_SIMP, (fMessagePage.pa & ~0xfffULL) | (orig & 0xffeULL) | 1);
	orig = rdmsr64(HV_MSR_SIEFP);
	wrmsr64(HV_MSR_SIEFP, (fEventPage.pa & ~0xfffULL) | (orig & 0xffeULL) | 1);

	uint32_t sint = HV_MSR_SINT0 + HV_SINT_MESSAGE;
	orig = rdmsr64(sint);
	wrmsr64(sint, (uint64_t)vector | (orig & 0xfffffffffffcff00ULL));

	orig = rdmsr64(HV_MSR_SCONTROL);
	wrmsr64(HV_MSR_SCONTROL, (orig & ~1ULL) | 1);
	HV_LOG("SynIC enabled, SINT%u -> vector 0x%x", HV_SINT_MESSAGE, vector);
	return true;
}

uint64_t
PDHyperV::postMessage(const void *data, uint32_t len)
{
	if (len > VMBUS_MSG_DSIZE_MAX) {
		return ~0ULL;
	}
	uint64_t status = ~0ULL;

	IOLockLock(fLock);
	struct hv_postmsg_in *in = (struct hv_postmsg_in *)fInputPage.va;
	for (int attempt = 0; attempt < 20; attempt++) {
		bzero(in, sizeof(*in));
		in->connid = HV_CONNID_MESSAGE;
		in->msgtype = HV_MSGTYPE_CHANNEL;
		in->dsize = len;
		bcopy(data, in->data, len);
		status = hypercall(HV_HYPERCALL_POST_MESSAGE, fInputPage.pa) & 0xffff;
		if (status == 0) {
			break;
		}
		IOSleep(1 << (attempt < 10 ? attempt : 10));
	}
	IOLockUnlock(fLock);
	return status;
}

void
PDHyperV::signalChannel(uint32_t chanid, uint32_t connid)
{
	volatile uint64_t *tx = (volatile uint64_t *)((char *)fIntrPage.va + PAGE_SIZE / 2);
	UInt64 cur;
	do {
		cur = tx[chanid / 64];
	} while (!OSCompareAndSwap64(cur, cur | (1ULL << (chanid & 63)), (UInt64 *)&tx[chanid / 64]));

	IOLockLock(fLock);
	struct hv_mon_param *prm = (struct hv_mon_param *)fInputPage.va;
	bzero(prm, sizeof(*prm));
	prm->connid = connid;
	hypercall(HV_HYPERCALL_SIGNAL_EVENT, fInputPage.pa);
	IOLockUnlock(fLock);
}

IOReturn
PDHyperV::waitReply(uint32_t type, uint32_t key, struct hv_message *reply)
{
	(void)type;
	(void)key;
	IOLockLock(fLock);
	if (!fReplyReady) {
		AbsoluteTime deadline;
		clock_interval_to_deadline(10, kSecondScale, &deadline);
		IOLockSleepDeadline(fLock, &fReplyReady, deadline, THREAD_UNINT);
	}
	bool ready = fReplyReady;
	if (ready) {
		bcopy(&fReply, reply, sizeof(*reply));
	}
	fReplyType = 0;
	IOLockUnlock(fLock);
	return ready ? kIOReturnSuccess : kIOReturnTimeout;
}

static void
arm_reply(IOLock *lock, bool *ready, uint32_t *rtype, uint32_t *rkey, uint32_t type, uint32_t key)
{
	IOLockLock(lock);
	*ready = false;
	*rtype = type;
	*rkey = key;
	IOLockUnlock(lock);
}

IOReturn
PDHyperV::gpadlConnect(uint32_t chanid, const struct pd_hv_mem *mem, uint32_t *gpadl)
{
	struct hv_message reply;
	uint32_t pages = mem->pages;
	uint64_t pfn = mem->pa >> 12;

	IOLockLock(fCtlLock);
	uint32_t id = fNextGpadl++;
	uint32_t cnt = pages > VMBUS_GPADL_CONN_PGMAX ? VMBUS_GPADL_CONN_PGMAX : pages;

	struct vmbus_chanmsg_gpadl_conn conn;
	bzero(&conn, sizeof(conn));
	conn.hdr.type = VMBUS_CHANMSG_GPADL_CONN;
	conn.chanid = chanid;
	conn.gpadl = id;
	conn.range_len = (uint16_t)(8 + 8 * pages);
	conn.range_cnt = 1;
	conn.gpa_len = pages * PAGE_SIZE;
	conn.gpa_ofs = 0;
	for (uint32_t i = 0; i < cnt; i++) {
		conn.gpa_page[i] = pfn++;
	}
	arm_reply(fLock, &fReplyReady, &fReplyType, &fReplyKey, VMBUS_CHANMSG_GPADL_CONNRESP, id);
	uint64_t status = postMessage(&conn, 28 + 8 * cnt);
	uint32_t left = pages - cnt;
	while (status == 0 && left > 0) {
		struct vmbus_chanmsg_gpadl_subconn sub;
		uint32_t n = left > VMBUS_GPADL_SUBCONN_PGMAX ? VMBUS_GPADL_SUBCONN_PGMAX : left;

		bzero(&sub, sizeof(sub));
		sub.hdr.type = VMBUS_CHANMSG_GPADL_SUBCONN;
		sub.gpadl = id;
		for (uint32_t i = 0; i < n; i++) {
			sub.gpa_page[i] = pfn++;
		}
		status = postMessage(&sub, 16 + 8 * n);
		left -= n;
	}
	IOReturn ret = status == 0 ? waitReply(VMBUS_CHANMSG_GPADL_CONNRESP, id, &reply) : kIOReturnIOError;
	IOLockUnlock(fCtlLock);
	if (ret != kIOReturnSuccess) {
		HV_LOG("GPADL for chan %u: no reply (post status 0x%llx)", chanid, status);
		return ret;
	}
	const struct vmbus_chanmsg_gpadl_connresp *resp =
	    (const struct vmbus_chanmsg_gpadl_connresp *)reply.data;
	if (resp->status != 0) {
		HV_LOG("GPADL for chan %u refused, status 0x%x", chanid, resp->status);
		return kIOReturnIOError;
	}
	*gpadl = id;
	return kIOReturnSuccess;
}

IOReturn
PDHyperV::openChannel(uint32_t chanid, uint32_t gpadl, uint32_t txPages)
{
	struct hv_message reply;
	struct vmbus_chanmsg_chopen req;

	bzero(&req, sizeof(req));
	req.hdr.type = VMBUS_CHANMSG_CHOPEN;
	req.chanid = chanid;
	req.openid = chanid;
	req.gpadl = gpadl;
	req.vcpuid = 0;
	req.txbr_pgcnt = txPages;

	IOLockLock(fCtlLock);
	arm_reply(fLock, &fReplyReady, &fReplyType, &fReplyKey, VMBUS_CHANMSG_CHOPEN_RESP, chanid);
	uint64_t status = postMessage(&req, sizeof(req));
	IOReturn ret = status == 0 ? waitReply(VMBUS_CHANMSG_CHOPEN_RESP, chanid, &reply) : kIOReturnIOError;
	IOLockUnlock(fCtlLock);
	if (ret != kIOReturnSuccess) {
		HV_LOG("open chan %u: no reply (post status 0x%llx)", chanid, status);
		return ret;
	}
	const struct vmbus_chanmsg_chopen_resp *resp = (const struct vmbus_chanmsg_chopen_resp *)reply.data;
	if (resp->status != 0) {
		HV_LOG("open chan %u refused, status 0x%x", chanid, resp->status);
		return kIOReturnIOError;
	}
	return kIOReturnSuccess;
}

void
PDHyperV::registerChannel(uint32_t chanid, PDVMBusChannel *chan)
{
	if (chanid < VMBUS_CHAN_MAX) {
		fChannels[chanid] = chan;
	}
}

void
PDHyperV::unregisterChannel(uint32_t chanid)
{
	if (chanid < VMBUS_CHAN_MAX) {
		fChannels[chanid] = NULL;
	}
}

void
PDHyperV::handleMessage(const struct hv_message *msg)
{
	const struct vmbus_chanmsg_hdr *hdr = (const struct vmbus_chanmsg_hdr *)msg->data;

	switch (hdr->type) {
	case VMBUS_CHANMSG_CONNECT_RESP:
	case VMBUS_CHANMSG_GPADL_CONNRESP:
	case VMBUS_CHANMSG_CHOPEN_RESP: {
		// chanid (open) and gpadl (connresp) both sit where the waiter expects its key
		uint32_t key = 0;
		if (hdr->type == VMBUS_CHANMSG_GPADL_CONNRESP) {
			key = ((const struct vmbus_chanmsg_gpadl_connresp *)msg->data)->gpadl;
		} else if (hdr->type == VMBUS_CHANMSG_CHOPEN_RESP) {
			key = ((const struct vmbus_chanmsg_chopen_resp *)msg->data)->chanid;
		}
		IOLockLock(fLock);
		if (fReplyType == hdr->type && (hdr->type == VMBUS_CHANMSG_CONNECT_RESP || fReplyKey == key)) {
			bcopy(msg, &fReply, sizeof(fReply));
			fReplyReady = true;
			IOLockWakeup(fLock, &fReplyReady, false);
		}
		IOLockUnlock(fLock);
		break;
	}
	case VMBUS_CHANMSG_CHOFFER: {
		const struct vmbus_chanmsg_choffer *off =
		    (const struct vmbus_chanmsg_choffer *)msg->data;
		char type[40];
		guid_to_str(&off->chtype, type, sizeof(type));
		if (!(off->chflags & VMBUS_CHAN_TLNPI_PROVIDER_OFFER) &&
		    fOfferCount < sizeof(fOffers) / sizeof(fOffers[0])) {
			bcopy(off, &fOffers[fOfferCount], sizeof(*off));
		}
		if (!(off->chflags & VMBUS_CHAN_TLNPI_PROVIDER_OFFER)) {
			fOfferCount++;
		}
		if (off->chflags & VMBUS_CHAN_TLNPI_PROVIDER_OFFER) {
			pd_hvsock_offer(off);
			break;
		}
		HV_LOG("offer chan %u %s [%s] connid %u montrig %u flags1 0x%x",
		    off->chanid, type, guid_name(type), off->connid, off->montrig, off->flags1);
		// Devices hot-added after the initial offers (WSL's NIC,
		// for one) are published as they come
		if (fOffersDone && fPublished) {
			publishChannel(off);
		}
		break;
	}
	case VMBUS_CHANMSG_CHOFFER_DONE:
		HV_LOG("offers done: %u channel(s)", fOfferCount);
		IOLockLock(fLock);
		fOffersDone = true;
		IOLockWakeup(fLock, &fOffersDone, false);
		IOLockUnlock(fLock);
		break;
	case VMBUS_CHANMSG_CHRESCIND:
		pd_hvsock_rescind(((const struct vmbus_chanmsg_chopen_resp *)msg->data)->chanid);
		break;
	case VMBUS_CHANMSG_TL_RESULT:
		pd_hvsock_tl_result(msg->data, msg->dsize);
		break;
	default:
		HV_LOG("control message type %u (%u bytes)", hdr->type, msg->dsize);
		break;
	}
}

void
PDHyperV::messageAction(IOInterruptEventSource *src, int count)
{
	(void)src;
	(void)count;
	for (;;) {
		struct hv_message msg;
		boolean_t istate = ml_set_interrupts_enabled(FALSE);
		if (gMsgTail == gMsgHead) {
			ml_set_interrupts_enabled(istate);
			break;
		}
		bcopy(&gMsgQueue[gMsgTail], &msg, sizeof(msg));
		gMsgTail = (gMsgTail + 1) % HV_MSGQ_LEN;
		ml_set_interrupts_enabled(istate);
		handleMessage(&msg);
	}
}

void
PDHyperV::eventAction(IOInterruptEventSource *src, int count)
{
	(void)src;
	(void)count;
	for (int i = 0; i < VMBUS_CHAN_MAX / 64; i++) {
		boolean_t istate = ml_set_interrupts_enabled(FALSE);
		uint64_t bits = gPendingEvents[i];
		gPendingEvents[i] = 0;
		ml_set_interrupts_enabled(istate);
		while (bits != 0) {
			int bit = __builtin_ctzll(bits);
			bits &= bits - 1;
			PDVMBusChannel *chan = fChannels[i * 64 + bit];
			if (chan != NULL) {
				chan->handleEvent();
			}
		}
	}
}

bool
PDHyperV::connectVMBus(void)
{
	if (!allocPages(&fIntrPage, 1) || !allocPages(&fMonitor1, 1) || !allocPages(&fMonitor2, 1)) {
		return false;
	}
	for (size_t i = 0; i < sizeof(kVMBusVersions) / sizeof(kVMBusVersions[0]); i++) {
		struct vmbus_chanmsg_connect req;
		struct hv_message reply;

		bzero(&req, sizeof(req));
		req.hdr.type = VMBUS_CHANMSG_CONNECT;
		req.ver = kVMBusVersions[i];
		req.evtflags = fIntrPage.pa;
		req.mnf1 = fMonitor1.pa;
		req.mnf2 = fMonitor2.pa;

		IOLockLock(fCtlLock);
		arm_reply(fLock, &fReplyReady, &fReplyType, &fReplyKey, VMBUS_CHANMSG_CONNECT_RESP, 0);
		uint64_t status = postMessage(&req, sizeof(req));
		IOReturn ret = status == 0 ? waitReply(VMBUS_CHANMSG_CONNECT_RESP, 0, &reply) : kIOReturnIOError;
		IOLockUnlock(fCtlLock);
		if (ret != kIOReturnSuccess) {
			HV_LOG("no reply to VMBus connect %u.%u (post status 0x%llx)", req.ver >> 16,
			    req.ver & 0xffff, status);
			return false;
		}
		if (((const struct vmbus_chanmsg_connect_resp *)reply.data)->done) {
			fVersion = req.ver;
			HV_LOG("VMBus version %u.%u", fVersion >> 16, fVersion & 0xffff);
			return true;
		}
	}
	HV_LOG("host refused every VMBus version");
	return false;
}

void
PDHyperV::requestOffers(void)
{
	struct vmbus_chanmsg_hdr req = { VMBUS_CHANMSG_CHREQUEST, 0 };

	uint64_t status = postMessage(&req, sizeof(req));
	if (status != 0) {
		HV_LOG("request offers failed, status 0x%llx", status);
		return;
	}
	IOLockLock(fLock);
	if (!fOffersDone) {
		AbsoluteTime deadline;
		clock_interval_to_deadline(10, kSecondScale, &deadline);
		IOLockSleepDeadline(fLock, &fOffersDone, deadline, THREAD_UNINT);
	}
	IOLockUnlock(fLock);
}

void
PDHyperV::publishChannel(const struct vmbus_chanmsg_choffer *offer)
{
	PDVMBusChannel *chan = OSTypeAlloc(PDVMBusChannel);
	if (chan == NULL || !chan->initWithOffer(this, offer) || !chan->attach(this)) {
		OSSafeReleaseNULL(chan);
		return;
	}
	chan->registerService();
	chan->release();
}

void
PDHyperV::publishChannels(void)
{
	uint32_t n = fOfferCount < 32 ? fOfferCount : 32;

	for (uint32_t i = 0; i < n; i++) {
		publishChannel(&fOffers[i]);
	}
}

bool
PDHyperV::start(IOService *provider)
{
	if (!super::start(provider)) {
		return false;
	}
	if (!identify()) {
		return false;
	}
	fLock = IOLockAlloc();
	fCtlLock = IOLockAlloc();
	fWorkLoop = IOWorkLoop::workLoop();
	fNextGpadl = 0xe1e10;
	if (fLock == NULL || fCtlLock == NULL || fWorkLoop == NULL) {
		return false;
	}
	fMsgSource = IOInterruptEventSource::interruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventAction, this, &PDHyperV::messageAction));
	fEventSource = IOInterruptEventSource::interruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventAction, this, &PDHyperV::eventAction));
	if (fMsgSource == NULL || fEventSource == NULL ||
	    fWorkLoop->addEventSource(fMsgSource) != kIOReturnSuccess ||
	    fWorkLoop->addEventSource(fEventSource) != kIOReturnSuccess) {
		return false;
	}
	fMsgSource->enable();
	fEventSource->enable();
	gHyperV = this;

	if (!setupHypercall() || !setupSynic()) {
		HV_LOG("hypercall/SynIC setup failed");
		return false;
	}
	if (!connectVMBus()) {
		return false;
	}
	pd_hvsock_init(this);
	requestOffers();
	registerService();
	publishChannels();
	fPublished = true;
	return true;
}

OSDefineMetaClassAndStructors(PDVMBusChannel, IOService);

bool
PDVMBusChannel::initWithOffer(PDHyperV *bus, const struct vmbus_chanmsg_choffer *offer)
{
	if (!IOService::init()) {
		return false;
	}
	char type[40], inst[40];

	fBus = bus;
	bcopy(offer, &fOffer, sizeof(fOffer));
	fTxLock = IOLockAlloc();
	guid_to_str(&offer->chtype, type, sizeof(type));
	guid_to_str(&offer->chinst, inst, sizeof(inst));
	setProperty("hv-type", type);
	setProperty("hv-instance", inst);
	setProperty("hv-channel", offer->chanid, 32);
	char name[32];
	snprintf(name, sizeof(name), "vmbus-chan%u", offer->chanid);
	setName(name);
	return fTxLock != NULL;
}

IOReturn
PDVMBusChannel::open(uint32_t txPages, uint32_t rxPages, OSObject *target, PDVMBusChannelCallback cb)
{
	if (!PDHyperV::allocPages(&fRing, txPages + rxPages)) {
		return kIOReturnNoMemory;
	}
	fTxPages = txPages;
	// Both rings honour pending_snd_sz, so a blocked writer gets signalled
	((struct vmbus_bufring *)fRing.va)->feature_bits = 1;
	((struct vmbus_bufring *)((char *)fRing.va + txPages * PAGE_SIZE))->feature_bits = 1;
	fTarget = target;
	fCallback = cb;
	IOReturn ret = fBus->gpadlConnect(fOffer.chanid, &fRing, &fGpadl);
	if (ret != kIOReturnSuccess) {
		return ret;
	}
	fBus->registerChannel(fOffer.chanid, this);
	return fBus->openChannel(fOffer.chanid, fGpadl, txPages);
}

static inline uint32_t
ring_used(uint32_t r, uint32_t w, uint32_t size)
{
	return w >= r ? w - r : size - (r - w);
}

IOReturn
PDVMBusChannel::writeRing(const void *const *parts, const uint32_t *lens, int count)
{
	struct vmbus_bufring *br = (struct vmbus_bufring *)fRing.va;
	uint8_t *data = (uint8_t *)fRing.va + PAGE_SIZE;
	uint32_t size = fTxPages * PAGE_SIZE - PAGE_SIZE;
	uint32_t total = sizeof(uint64_t);

	for (int i = 0; i < count; i++) {
		total += lens[i];
	}

	IOLockLock(fTxLock);
	uint32_t r = br->rindex, w = br->windex;
	if (size - ring_used(r, w, size) <= total) {
		IOLockUnlock(fTxLock);
		return kIOReturnNoSpace;
	}
	uint32_t old = w;
	for (int i = 0; i <= count; i++) {
		uint64_t trailer = (uint64_t)old << 32;
		const uint8_t *src = i < count ? (const uint8_t *)parts[i] : (const uint8_t *)&trailer;
		uint32_t len = i < count ? lens[i] : sizeof(trailer);
		uint32_t first = len < size - w ? len : size - w;
		if (src != NULL) {
			bcopy(src, data + w, first);
			bcopy(src + first, data, len - first);
		} else {
			bzero(data + w, first);
			bzero(data, len - first);
		}
		w = (w + len) % size;
	}
	br->windex = w;
	bool signal = br->imask == 0 && old == br->rindex;
	IOLockUnlock(fTxLock);

	if (signal) {
		fBus->signalChannel(fOffer.chanid, fOffer.connid);
	}
	return kIOReturnSuccess;
}

IOReturn
PDVMBusChannel::send(uint16_t type, uint16_t flags, const void *data, uint32_t len, uint64_t xactid)
{
	struct vmbus_chanpkt_hdr hdr;
	uint32_t pktlen = sizeof(hdr) + len;
	uint32_t padded = (pktlen + 7) & ~7U;

	hdr.type = type;
	hdr.flags = flags;
	hdr.hlen = sizeof(hdr) >> 3;
	hdr.tlen = (uint16_t)(padded >> 3);
	hdr.xactid = xactid;

	const void *parts[3] = { &hdr, data, NULL };
	uint32_t lens[3] = { sizeof(hdr), len, padded - pktlen };
	return writeRing(parts, lens, 3);
}

IOReturn
PDVMBusChannel::sendPages(const uint64_t *pfns, uint32_t npages, uint32_t offset, uint32_t byteLen,
    const void *data, uint32_t len, uint64_t xactid)
{
	struct {
		struct vmbus_chanpkt_hdr hdr;
		uint32_t rsvd;
		uint32_t range_cnt;
		uint32_t gpa_len;
		uint32_t gpa_ofs;
	} __attribute__((packed)) pkt;
	uint32_t hlen = sizeof(pkt) + 8 * npages;
	uint32_t pktlen = hlen + len;
	uint32_t padded = (pktlen + 7) & ~7U;

	pkt.hdr.type = VMBUS_CHANPKT_TYPE_GPA;
	pkt.hdr.flags = VMBUS_CHANPKT_FLAG_RC;
	pkt.hdr.hlen = (uint16_t)(hlen >> 3);
	pkt.hdr.tlen = (uint16_t)(padded >> 3);
	pkt.hdr.xactid = xactid;
	pkt.rsvd = 0;
	pkt.range_cnt = 1;
	pkt.gpa_len = byteLen;
	pkt.gpa_ofs = offset;

	const void *parts[4] = { &pkt, pfns, data, NULL };
	uint32_t lens[4] = { sizeof(pkt), 8 * npages, len, padded - pktlen };
	return writeRing(parts, lens, 4);
}

IOReturn
PDVMBusChannel::recv(void *out, uint32_t *len, uint64_t *xactid, uint16_t *type)
{
	struct vmbus_bufring *br = (struct vmbus_bufring *)((char *)fRing.va + fTxPages * PAGE_SIZE);
	uint8_t *data = (uint8_t *)br + PAGE_SIZE;
	uint32_t size = (fRing.pages - fTxPages) * PAGE_SIZE - PAGE_SIZE;
	uint32_t r = br->rindex, w = br->windex;
	uint32_t used = ring_used(r, w, size);
	struct vmbus_chanpkt_hdr hdr;

	if (used < sizeof(hdr) + sizeof(uint64_t)) {
		return kIOReturnNoFrames;
	}
	for (uint32_t i = 0; i < sizeof(hdr); i++) {
		((uint8_t *)&hdr)[i] = data[(r + i) % size];
	}
	uint32_t hlen = (uint32_t)hdr.hlen << 3, tlen = (uint32_t)hdr.tlen << 3;
	if (hlen < sizeof(hdr) || hlen > tlen || used < tlen + sizeof(uint64_t)) {
		return kIOReturnIOError;
	}
	uint32_t dlen = tlen - hlen;
	uint32_t copy = dlen < *len ? dlen : *len;
	for (uint32_t i = 0; i < copy; i++) {
		((uint8_t *)out)[i] = data[(r + hlen + i) % size];
	}
	br->rindex = (r + tlen + sizeof(uint64_t)) % size;
	*len = copy;
	*xactid = hdr.xactid;
	*type = hdr.type;
	return kIOReturnSuccess;
}

IOReturn
PDVMBusChannel::recvPacket(void *packet, uint32_t *len)
{
	struct vmbus_bufring *br = (struct vmbus_bufring *)((char *)fRing.va + fTxPages * PAGE_SIZE);
	uint8_t *data = (uint8_t *)br + PAGE_SIZE;
	uint32_t size = (fRing.pages - fTxPages) * PAGE_SIZE - PAGE_SIZE;
	uint32_t r = br->rindex, w = br->windex;
	uint32_t used = ring_used(r, w, size);
	struct vmbus_chanpkt_hdr hdr;

	if (used < sizeof(hdr) + sizeof(uint64_t)) {
		return kIOReturnNoFrames;
	}
	for (uint32_t i = 0; i < sizeof(hdr); i++) {
		((uint8_t *)&hdr)[i] = data[(r + i) % size];
	}
	uint32_t tlen = (uint32_t)hdr.tlen << 3;
	if ((uint32_t)hdr.hlen << 3 < sizeof(hdr) || used < tlen + sizeof(uint64_t)) {
		return kIOReturnIOError;
	}
	if (tlen > *len) {
		*len = tlen;
		return kIOReturnNoSpace;
	}
	uint32_t first = tlen < size - r ? tlen : size - r;
	bcopy(data + r, packet, first);
	bcopy(data, (uint8_t *)packet + first, tlen - first);
	br->rindex = (r + tlen + sizeof(uint64_t)) % size;
	*len = tlen;
	return kIOReturnSuccess;
}

void
PDVMBusChannel::recvDone(void)
{
	struct vmbus_bufring *br = (struct vmbus_bufring *)((char *)fRing.va + fTxPages * PAGE_SIZE);
	uint32_t size = (fRing.pages - fTxPages) * PAGE_SIZE - PAGE_SIZE;
	uint32_t want = br->pending_snd_sz;

	if (want != 0 && size - ring_used(br->rindex, br->windex, size) >= want) {
		fBus->signalChannel(fOffer.chanid, fOffer.connid);
	}
}

void
PDVMBusChannel::handleEvent(void)
{
	if (fCallback != NULL) {
		fCallback(fTarget, this);
	}
}
