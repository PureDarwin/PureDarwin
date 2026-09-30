// PDAccelerator, AppleParavirtGPU and the submit path: bridge, events, async jobs and command stream execution

#include "PDIOSurface.h"

OSDefineMetaClassAndStructors(PDAccelerator, IOAccelerator);
OSDefineMetaClassAndStructors(AppleParavirtGPU, PDAccelerator);


bool
PDAccelerator::start(IOService *provider)
{
	if (!IOAccelerator::start(provider)) {
		return false;
	}
	// Metal loads /System/Library/Extensions/<MetalPluginName>.bundle for this device.
	// Naming Apple's paravirtual plugin makes it drive this service
	setProperty("MetalPluginName", "AppleParavirtGPUMetalIOGPUFamily");
	setProperty("MetalPluginClassName", "AppleParavirtDevice");
	// WindowServer finds AppleParavirtGPU by class and counts paravirtual displays by DisplayPortCount
	setProperty("DisplayPortCount", 1ULL, 32);
	registerService();
	return true;
}

IOReturn
PDAccelerator::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}
uint8_t sIntrospected[256];
// A draw for the daemon (kind 2): both stage metallibs, the buffers bound by index, the
// indices and the target, all copied into the bridge. The target comes back
struct PDDrawBuffer { uint32_t id, off, size, pad; };
struct PDDrawTex { uint32_t id, off, w, h, bpr, fmt; };
struct PDDrawRequest {
	uint32_t vfn, ffn, vmOff, vmSize, fmOff, fmSize;
	uint32_t prim, count, indexed, indexOff, indexBytes, pad0;
	PDDrawBuffer vbuf[PD_DRAW_BUFS], fbuf[PD_DRAW_BUFS];
	uint32_t targetOff, tw, th, tbpr, tfmt, pad1;
	double viewport[6];
	PDDrawTex ftex[16];
	uint32_t instances, pad2;
	PDDrawTex rt[4];	// colour attachments 1-3, entry 0 unused: w 0 when absent
	uint32_t rtBlend[4];	// their packed blend states, as pad0 is for attachment 0
	uint32_t samp[16];	// each bound fragment sampler's flags (sel 257 type 3), 0 unknown
};
// the metallibs and buffers are copied from PD_DRAW_DATA on, and the request is filled in after
static_assert(PD_DRAW_REQ + sizeof(PDDrawRequest) <= PD_DRAW_DATA, "draw request overlaps its data");
// A compute dispatch (kind 3), same place: the kernel, the grid (threads when dispatchThreads,
// threadgroups otherwise) and what is bound. Buffers and textures come back after the run
enum { PD_CS_BUFS = 31 };
struct PDComputeRequest {
	uint32_t fn, mOff, mSize, threads;
	uint64_t grid[3], tpg[3];
	uint32_t tgmem[4], pad[4];
	PDDrawBuffer buf[PD_CS_BUFS];
	PDDrawTex tex[16];
	uint32_t samp[16];
};
static_assert(PD_DRAW_REQ + sizeof(PDComputeRequest) <= PD_DRAW_DATA, "compute request overlaps its data");
IOBufferMemoryDescriptor *sBridge;
// Bytes a pixel of each daemon texture format takes (see texFmt)
static uint32_t
pdTexBpp(uint32_t fmt)
{
	return fmt == 1 ? 8 : (fmt == 2 || fmt == 3) ? 1 : fmt == 5 ? 2 : 4;
}

// The daemon takes one job at a time: a draw arriving while it introspects a new shader
// waits for it rather than being dropped (a dropped frame may never be drawn again)
static bool
pdBridgeIdle(uint32_t ms)
{
	volatile PDBridgeHeader *h = (volatile PDBridgeHeader *)sBridge->getBytesNoCopy();

	for (uint32_t n = 0; n <= ms; n++) {
		if (h->status == 0x44525652 && h->state == 0) {
			return true;
		}
		if (h->status != 0x44525652) {
			return false;
		}
		IOSleep(1);
	}
	return false;
}
static uint32_t sBridgeJobEvent, sBridgeDoneEvent;

void
pdBridgeRing(PDBridgeHeader *h)
{
	__sync_synchronize();
	h->state = 1;
	__sync_synchronize();

	if (sBridgeBell == NULL)
		return;

	IOLockLock(sBridgeBell);
	IOLockWakeup(sBridgeBell, &sBridgeJobEvent, false);
	IOLockUnlock(sBridgeBell);
}

void
pdBridgeWait(PDBridgeHeader *h, uint32_t ms)
{
	uint64_t deadline;

	clock_interval_to_deadline(ms, kMillisecondScale, &deadline);

	if (sBridgeBell == NULL) {
		while (h->state == 1 && mach_absolute_time() < deadline) {
			IOSleep(10);
		}
		return;
	}

	IOLockLock(sBridgeBell);
	while (h->state == 1 && mach_absolute_time() < deadline) {
		uint64_t slice;

		clock_interval_to_deadline(10, kMillisecondScale, &slice);
		IOLockSleepDeadline(sBridgeBell, &sBridgeDoneEvent, slice < deadline ? slice : deadline, THREAD_UNINT);
	}
	IOLockUnlock(sBridgeBell);
	__sync_synchronize();
}

// the daemon's side of the doorbells
IOReturn
pdBridgeDoorbell(uint32_t selector)
{
	if (sBridge == NULL || sBridgeBell == NULL)
		return kIOReturnNotReady;

	PDBridgeHeader *h = (PDBridgeHeader *)sBridge->getBytesNoCopy();

	IOLockLock(sBridgeBell);
	if (selector == PD_BRIDGE_SEL_DONE) {
		IOLockWakeup(sBridgeBell, &sBridgeDoneEvent, false);
	} else {
		uint64_t deadline;

		clock_interval_to_deadline(20, kMillisecondScale, &deadline);
		while (h->state != 1 && mach_absolute_time() < deadline) {
			IOLockSleepDeadline(sBridgeBell, &sBridgeJobEvent, deadline, THREAD_UNINT);
		}
	}
	IOLockUnlock(sBridgeBell);
	return kIOReturnSuccess;
}

// Colour attachment att's packed blend state from a type 0xe pipeline message, its (tag, 4, u32)
// group starting (0, 4, att). 0 when absent: CoreAnimation's pipelines composite premultiplied over
uint32_t
pdParseBlend(const uint8_t *m, uint32_t len, uint32_t att)
{
	for (uint32_t i = 0x15; i + 12 <= len; i++) {
		uint32_t v0, n;
		uint32_t en = 0, srgb = 1, drgb = 0, op = 0, sa = 1, da = 0, aop = 0, mask = 0xf, saw = 0;

		memcpy(&v0, m + i + 2, 4);
		if (m[i] != 0 || m[i + 1] != 4 || v0 != att || m[i + 6] != 1 || m[i + 7] != 4) {
			continue;
		}
		n = m[i - 1];
		if (n < 2 || n > 12) {
			continue;
		}
		for (uint32_t k = 0, pos = i; k < n && pos + 6 <= len && m[pos + 1] == 4; k++, pos += 6) {
			uint32_t v;

			memcpy(&v, m + pos + 2, 4);
			switch (m[pos]) {
			case 2: en = v != 0; saw = 1; break;
			case 3: srgb = v & 31; break;
			case 4: drgb = v & 31; break;
			case 5: op = v & 7; break;
			case 6: sa = v & 31; break;
			case 7: da = v & 31; break;
			case 8: aop = v & 7; break;
			case 9: mask = v & 15; break;
			default: break;
			}
		}
		if (!saw) {
			return 0;
		}
		return 1 | en << 1 | srgb << 2 | drgb << 7 | sa << 12 | da << 17 | op << 22 | aop << 25 | mask << 28;
	}
	return 0;
}
uint32_t sBridgeSerial;
static uint32_t sSubmitStamp = 0x100;   // host submission counter in usage records
// MTLSharedEvent values (IOSurfaceRoot sel 36/38, trap 7): one u64 per event, in a page every
// process that uses events maps. A port names OSNumber(kPDEventTag | index), as surfaces do
IOBufferMemoryDescriptor *sEventPage;
uint32_t sNextEvent;
// each slot's object: once no process holds its port only this reference is left and the slot
// is free. A drawable takes a new event every frame, so slots have to come back
OSNumber *sEventObj[kPDEventMax];
IOLock *sEventAllocLock;
PDEventListener sListeners[kPDListenMax];
uint32_t sListenCount;
IOLock *sListenLock;

void
pdEventSet(uint32_t index, uint64_t value)
{
	volatile uint64_t *page;

	if (sEventPage == NULL || index >= kPDEventMax) {
		return;
	}
	page = (volatile uint64_t *)sEventPage->getBytesNoCopy();
	page[index] = value;
	if (sListenLock == NULL) {
		return;
	}
	IOLockLock(sListenLock);
	for (uint32_t i = 0; i < sListenCount;) {
		PDEventListener *l = &sListeners[i];

		if (l->index != index || value < l->value) {
			i++;
			continue;
		}
		io_user_reference_t a[4] = { l->echo[0], l->echo[1], l->echo[2], (io_user_reference_t)value };
		IOSurfaceRootUserClient::sendAsync(l->ref, a, 4);
		sListeners[i] = sListeners[--sListenCount];
	}
	IOLockUnlock(sListenLock);
}
// A drawable's event is signalled when the compositor releases its surface. Nothing here tracks
// that, so a listener still waiting at the next vblank is fired at its value
void
pdFireDueListeners(uint64_t frame)
{
	uint32_t idx[32], nfire = 0;
	uint64_t val[32];

	if (sListenLock == NULL) {
		return;
	}
	// one frame per client per vblank: its lowest waiting value only. Listener values are a frame
	// counter, so firing them all at once tells the client several frames went by in one vblank
	IOLockLock(sListenLock);
	{
		uint64_t port[32];

		for (uint32_t i = 0; i < sListenCount; i++) {
			const PDEventListener *l = &sListeners[i];
			uint32_t k;

			if (l->since >= frame) {
				continue;
			}
			for (k = 0; k < nfire && port[k] != l->ref[0]; k++) {
			}
			if (k == nfire) {
				if (nfire == 32) {
					continue;
				}
				port[nfire++] = l->ref[0];
				idx[k] = l->index;
				val[k] = l->value;
			} else if (l->value < val[k]) {
				idx[k] = l->index;
				val[k] = l->value;
			}
		}
	}
	IOLockUnlock(sListenLock);
	for (uint32_t i = 0; i < nfire; i++) {
		pdEventSet(idx[i], val[i]);
	}
}
// Each job fills the shared bridge then waits for the daemon, clients take turns
IOLock *sBridgeLock;


// Trap 0 snapshots the changed encoder storages and returns, and one kernel thread runs the
// submissions in order, as a GPU would. The client's threads never wait out the raster
struct PDAsyncJob {
	struct PDAsyncJob *next;
	IOSurfaceRootUserClient *client;
	IOBufferMemoryDescriptor *snap[32];
	uint32_t queue, stamp;
	uint64_t block[2];
};
static IOLock *sAsyncLock;
static struct PDAsyncJob *sAsyncHead, *sAsyncTail;
static thread_t sAsyncThread;
static uint32_t sAsyncBusy;

// wait until every queued submission has run: a swap must not scan out a surface still being drawn
void
pdAsyncDrain(uint32_t ms)
{
	uint64_t deadline;

	if (sAsyncLock == NULL || current_thread() == sAsyncThread) {
		return;
	}
	clock_interval_to_deadline(ms, kMillisecondScale, &deadline);
	IOLockLock(sAsyncLock);
	while ((sAsyncHead != NULL || sAsyncBusy) && mach_absolute_time() < deadline) {
		IOLockSleepDeadline(sAsyncLock, &sAsyncBusy, deadline, THREAD_UNINT);
	}
	IOLockUnlock(sAsyncLock);
}

// A double in [0, 1] to an 8-bit channel, from its bit pattern: no floating point in the kernel
static uint8_t
unorm8FromDoubleBits(uint64_t bits)
{
	int64_t e = (int64_t)((bits >> 52) & 0x7ff) - 1023;
	uint64_t mant = (bits & ((1ULL << 52) - 1)) | (1ULL << 52);
	int64_t shift = 52 - e;

	if ((bits >> 63) != 0 || ((bits >> 52) & 0x7ff) == 0) {
		return 0;
	}
	if (e >= 0) {
		return 255;
	}
	if (shift >= 62) {
		return 0;
	}
	return (uint8_t)(((mant * 255) + (1ULL << (shift - 1))) >> shift);
}

// A double to IEEE half, from its bit pattern (truncating, clear colours are exact anyway)
static uint16_t
halfFromDoubleBits(uint64_t bits)
{
	uint16_t sign = (uint16_t)((bits >> 48) & 0x8000);
	int64_t e = (int64_t)((bits >> 52) & 0x7ff) - 1023 + 15;
	uint64_t mant = bits & ((1ULL << 52) - 1);

	if (((bits >> 52) & 0x7ff) == 0 || e <= 0) {
		return sign;
	}
	if (e >= 31) {
		return sign | 0x7c00;
	}
	return sign | (uint16_t)(e << 10) | (uint16_t)(mant >> 42);
}

// Buffer id standing for "this encoder's own storage" (set{Fragment,Vertex}Bytes payloads)
#define PD_INLINE_BYTES 0xfffffffeU

static void
pdAsyncWorker(void *unused, wait_result_t wr)
{
	(void)unused;
	(void)wr;
	for (;;) {
		struct PDAsyncJob *job;

		IOLockLock(sAsyncLock);
		while (sAsyncHead == NULL) {
			IOLockSleep(sAsyncLock, &sAsyncHead, THREAD_UNINT);
		}
		job = sAsyncHead;
		sAsyncHead = job->next;
		if (sAsyncHead == NULL) {
			sAsyncTail = NULL;
		}
		sAsyncBusy = 1;
		IOLockUnlock(sAsyncLock);

		job->client->runAsyncJob(job);

		IOLockLock(sAsyncLock);
		sAsyncBusy = 0;
		if (sAsyncHead == NULL) {
			IOLockWakeup(sAsyncLock, &sAsyncBusy, false);
		}
		IOLockUnlock(sAsyncLock);
	}
}

// the encoder storages the pending submission's 0x10000 records name, in record order: the
// command buffer's own encoder order, which storage slots do not keep
uint32_t
IOSurfaceRootUserClient::sidebandOrder(uint32_t *order, uint32_t max)
{
	uint32_t n = 0;

	if (fEncoders == NULL || fResources == NULL)
		return 0;

	// the submission's own header and list when it named them, else every pair whose stamp moved
	for (uint32_t i = 0; i < (fSubHdr != 0 ? 1 : fSbCount); i++) {
		const uint8_t *hdr, *list;
		uint32_t stamp, start, count, len;

		if (fSubHdr != 0) {
			hdr = (const uint8_t *)fShmemById[fSubHdr]->getBytesNoCopy();
			list = (const uint8_t *)fShmemById[fSubList]->getBytesNoCopy();
			len = (uint32_t)fShmemById[fSubList]->getLength();
		} else {
			if (fSbPair[i][0] == NULL || fSbPair[i][1] == NULL)
				continue;

			hdr = (const uint8_t *)fSbPair[i][0]->getBytesNoCopy();
			list = (const uint8_t *)fSbPair[i][1]->getBytesNoCopy();
			len = (uint32_t)fSbPair[i][1]->getLength();
			memcpy(&stamp, hdr, 4);
			if (stamp == fSbStamp[i])
				continue;
		}

		memcpy(&start, list + 4, 4);
		memcpy(&count, hdr + 0x20, 4);
		for (uint32_t off = start, r = 0; r < count && r < 1024 && off >= 8 && off + 0x14 <= len; r++) {
			uint32_t kind, size, id;
			char key[16];
			OSObject *md;

			memcpy(&kind, list + off, 4);
			memcpy(&size, list + off + 4, 4);
			if (size < 8)
				break;

			if (kind == 0x10000) {
				memcpy(&id, list + off + 12, 4);
				snprintf(key, sizeof(key), "%u", id);
				md = fResources->getObject(key);
				for (uint32_t e = 0; md != NULL && e < fEncoders->getCount() && e < 32; e++) {
					bool seen = false;

					if (fEncoders->getObject(e) != md)
						continue;

					for (uint32_t k = 0; k < n; k++) {
						seen = seen || order[k] == e;
					}
					if (!seen && n < max)
						order[n++] = e;
				}
			}
			off += size;
		}
	}
	return n;
}

void
IOSurfaceRootUserClient::snapshotStreams(IOBufferMemoryDescriptor **snap)
{
	uint32_t order[32], n, k = 0;
	bool done[32] = {};

	n = sidebandOrder(order, 32);
	// only the storages this command buffer names, in its order, since another thread may already
	// have encoded the next command buffer. Without records, every changed storage
	for (uint32_t i = 0; i < (n != 0 ? n : 32) && k < 32; i++) {
		uint32_t e = n != 0 ? order[i] : i;
		IOBufferMemoryDescriptor *md;
		const uint8_t *p;
		uint32_t total, sum = 0;

		if (fEncoders == NULL || e >= fEncoders->getCount() || e >= 32 || done[e])
			continue;

		done[e] = true;
		md = (IOBufferMemoryDescriptor *)fEncoders->getObject(e);
		p = (const uint8_t *)md->getBytesNoCopy();
		memcpy(&total, p, 4);
		if (total < 16 || total > md->getLength()) {
			continue;
		}
		for (uint32_t j = 0; j < total; j++) {
			sum = sum * 31 + p[j];
		}
		if (sum == fExecuted[e]) {
			continue;
		}
		fExecuted[e] = sum;
		snap[k] = IOBufferMemoryDescriptor::withBytes(p, md->getLength(), kIODirectionInOut);
		if (snap[k] != NULL) {
			k++;
		}
	}
}

static void pdAsyncEnqueue(struct PDAsyncJob *job);

void
IOSurfaceRootUserClient::submitAsync(uint32_t queue)
{
	struct PDAsyncJob *job = (struct PDAsyncJob *)IOMallocZero(sizeof(*job));
	uint32_t qi = queue - 1;

	if (job == NULL) {
		return;
	}
	if (sAsyncLock == NULL) {
		IOLock *l = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, l, (void * volatile *)&sAsyncLock)) {
			IOLockFree(l);
		}
	}
	job->client = this;
	retain();
	job->queue = queue;
	job->stamp = qi < 256 ? ++fQueueStamp[qi] : ++sSubmitStamp;
	job->block[0] = fSubmitBlocks[0];
	job->block[1] = fSubmitBlocks[1];
	snapshotStreams(job->snap);
	applySignals();
	pdAsyncEnqueue(job);
}

// queue order is execution order, across every client
static void
pdAsyncEnqueue(struct PDAsyncJob *job)
{
	if (sAsyncLock == NULL) {
		IOLock *l = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, l, (void * volatile *)&sAsyncLock)) {
			IOLockFree(l);
		}
	}
	IOLockLock(sAsyncLock);
	if (sAsyncThread == NULL) {
		thread_t th = NULL;

		if (kernel_thread_start(&pdAsyncWorker, NULL, &th) == KERN_SUCCESS) {
			sAsyncThread = th;
			thread_deallocate(th);
		}
	}
	if (sAsyncTail != NULL) {
		sAsyncTail->next = job;
	} else {
		sAsyncHead = job;
	}
	sAsyncTail = job;
	IOLockWakeup(sAsyncLock, &sAsyncHead, true);
	IOLockUnlock(sAsyncLock);
}

void
IOSurfaceRootUserClient::runAsyncJob(struct PDAsyncJob *job)
{
	uint64_t start, end;

	fSubmitQueue = job->queue;
	fCurStamp = job->stamp;
	start = mach_absolute_time();
	executeStreams(job->snap);
	end = mach_absolute_time();
	if (fSel2MD != NULL && job->queue - 1 < 256) {
		((volatile uint32_t *)fSel2MD->getBytesNoCopy())[job->queue - 1] = job->stamp << 8;
	}
	if (fQueues != NULL && job->block[0] != 0) {
		queueCompletion(job->queue, job->block[0], job->block[1], start, end);
	}
	for (uint32_t e = 0; e < 32; e++) {
		OSSafeReleaseNULL(job->snap[e]);
	}
	release();
	IOFree(job, sizeof(*job));
}

// trap 1 {rid}: the client released the resource the sel 9 reply named rid. Its memory is wired
// and mapped into the client: kept, a Language Chooser run leaked gigabytes of it
void
IOSurfaceRootUserClient::releaseResource(uint32_t rid)
{
	char rkey[16], key[16];
	uint32_t objid = 0;
	OSNumber *num;

	snprintf(rkey, sizeof(rkey), "%u", rid);
	if (fResources != NULL) {
		fResources->removeObject(rkey);
	}
	if (fRecords != NULL) {
		fRecords->removeObject(rkey);
	}
	if (fResObj != NULL && (num = OSDynamicCast(OSNumber, fResObj->getObject(rkey))) != NULL) {
		objid = num->unsigned32BitValue();
		fResObj->removeObject(rkey);
	}
	if (fResMaps != NULL) {
		char mkey[16];

		snprintf(mkey, sizeof(mkey), "r%u", rid);
		fResMaps->removeObject(mkey);
	}
	// the object's own state only while it still names this resource: its id may already be reused
	if (objid != 0 && objid < PD_MAX_OBJ && fObjRes[objid] == rid) {
		fObjRes[objid] = 0;
		snprintf(key, sizeof(key), "%u", objid);
		if (fUserBuffers != NULL) {
			IOMemoryDescriptor *um = OSDynamicCast(IOMemoryDescriptor, fUserBuffers->getObject(key));

			if (um != NULL) {
				um->complete();
				fUserBuffers->removeObject(key);
			}
		}
		if (fBuffers != NULL) {
			fBuffers->removeObject(key);
		}
		if (fResMaps != NULL) {
			fResMaps->removeObject(key);
		}
		fTexW[objid] = fTexH[objid] = 0;
		fTexPf[objid] = 0;
		fTexSurf[objid] = 0;
		fTexAlias[objid] = 0;
		fTexBpr[objid] = 0;
	}
}

void
IOSurfaceRootUserClient::queueCompletion(uint64_t queue, uint64_t block0, uint64_t block1, uint64_t start, uint64_t end)
{
	uint64_t deadline;

	if (fPendLock == NULL) {
		fPendLock = IOLockAlloc();
	}
	if (fCompletionCall == NULL) {
		fCompletionCall = thread_call_allocate(&IOSurfaceRootUserClient::completionFired, this);
	}
	if (fPendLock == NULL || fCompletionCall == NULL) {
		return;
	}
	IOLockLock(fPendLock);
	if (fPendCount >= kPendMax) {
		IOLog("PDIOSurface: %s completion dropped, %u pending on queue %llu\n", fTag, fPendCount,
		    (unsigned long long)queue);
	} else {
		PendingCompletion *pc = &fPend[fPendCount++];

		pc->queue = queue;
		pc->block[0] = block0;
		pc->block[1] = block1;
		pc->start = start;
		pc->end = end;
	}
	IOLockUnlock(fPendLock);
	clock_interval_to_deadline(1, kMillisecondScale, &deadline);
	retain();
	if (thread_call_enter_delayed(fCompletionCall, deadline)) {
		release();	// already pending: that call holds the reference
	}
}

void
IOSurfaceRootUserClient::completionFired(thread_call_param_t self, thread_call_param_t unused)
{
	IOSurfaceRootUserClient *c = (IOSurfaceRootUserClient *)self;
	// drained 64 at a time, oldest first: the whole array would not fit on a kernel stack
	PendingCompletion batch[64];
	uint32_t n;

	(void)unused;
	do {
		IOLockLock(c->fPendLock);
		n = c->fPendCount < 64 ? c->fPendCount : 64;
		memcpy(batch, c->fPend, n * sizeof(batch[0]));
		memmove(c->fPend, c->fPend + n, (c->fPendCount - n) * sizeof(batch[0]));
		c->fPendCount -= n;
		IOLockUnlock(c->fPendLock);
		for (uint32_t i = 0; i < n; i++) {
			c->postCompletion(batch[i]);
		}
	} while (n == 64);
	c->release();
}

// One entry per record block, in order: block0 (scheduled) then block1 (completed), each
// {block, GPU start, GPU end, 0, 0}, on the notification queue bound to the submitting queue
void
IOSurfaceRootUserClient::postCompletion(const PendingCompletion &pc)
{
	OSCollectionIterator *it;
	OSObject *k;
	uint64_t notify = 0;
	char want[24];

	if (fQueues == NULL) {
		return;
	}
	for (uint32_t i = 0; i < 16; i++) {
		if (fBoundQueue[i] == pc.queue) {
			notify = fBoundNotify[i];
		}
	}
	snprintf(want, sizeof(want), "%llu", (unsigned long long)notify);
	it = OSCollectionIterator::withCollection(fQueues);
	while (it != NULL && (k = it->getNextObject()) != NULL) {
		const OSSymbol *ksym = OSDynamicCast(OSSymbol, k);
		IOSharedDataQueue *q = OSDynamicCast(IOSharedDataQueue, fQueues->getObject(ksym));

		if (notify != 0 && (ksym == NULL || !ksym->isEqualTo(want))) {
			continue;
		}
		for (uint32_t b = 0; q != NULL && b < 2; b++) {
			uint64_t entry[5] = { pc.block[b], pc.start, pc.end, 0, 0 };

			if (pc.block[b] == 0) {
				continue;
			}
			q->enqueue(entry, sizeof(entry));
		}
	}
	OSSafeReleaseNULL(it);
}

uint64_t
IOSurfaceRootUserClient::eventAddress(uint32_t index)
{
	if (sEventPage == NULL || index >= kPDEventMax) {
		return 0;
	}
	if (fEventMap == NULL) {
		fEventMap = sEventPage->createMappingInTask(fTask, 0, kIOMapAnywhere);
	}
	return fEventMap != NULL ? fEventMap->getAddress() + 8ULL * index : 0;
}

// After a submission: its sideband list (type 1 shmem) of {u32 kind, u32 size, u32 event port,
// u32, u64 value}. Kind 3 signals set the event, kind 4 waits need nothing as execution is in order
void
IOSurfaceRootUserClient::applySignals(void)
{
	if (sEventPage == NULL) {
		return;
	}
	// The submission doesn't name its pair: take every pair whose header stamp moved
	for (uint32_t i = 0; i < fSbCount; i++) {
		const uint8_t *hdr;
		uint32_t stamp;

		if (fSbPair[i][0] == NULL || fSbPair[i][1] == NULL) {
			continue;
		}
		hdr = (const uint8_t *)fSbPair[i][0]->getBytesNoCopy();
		memcpy(&stamp, hdr, 4);
		if (stamp == fSbStamp[i]) {
			continue;
		}
		fSbStamp[i] = stamp;
		applySidebandPair(hdr, (const uint8_t *)fSbPair[i][1]->getBytesNoCopy(),
		    (uint32_t)fSbPair[i][1]->getLength());
	}
}

void
IOSurfaceRootUserClient::applySidebandPair(const uint8_t *hdr, const uint8_t *list, uint32_t len)
{
	uint32_t start, count;

	// {u32 kind, u32 size, ...} records from the start offset, as many as the header's +0x20
	// count: resources (0x10000, 0x14 bytes), signals (3) and waits (4) of 0x18
	memcpy(&start, list + 4, 4);
	memcpy(&count, hdr + 0x20, 4);
	for (uint32_t off = start, r = 0; r < count && r < 1024 && off >= 8 && off + 8 <= len; r++) {
		uint32_t kind, size, name;
		uint64_t value;
		OSObject *obj = NULL;

		memcpy(&kind, list + off, 4);
		memcpy(&size, list + off + 4, 4);
		if (size < 8 || off + size > len) {
			break;
		}
		if (size < 0x18) {
			off += size;
			continue;
		}
		memcpy(&name, list + off + 8, 4);
		memcpy(&value, list + off + 0x10, 8);
		off += size;
		if (kind != 3 || IOUserClient::copyObjectForPortNameInTask(fTask, name, &obj) != kIOReturnSuccess) {
			continue;
		}
		OSNumber *num = OSDynamicCast(OSNumber, obj);
		if (num != NULL && (num->unsigned32BitValue() & kPDEventTag) != 0) {
			uint32_t index = num->unsigned32BitValue() & ~kPDEventTag;

			if (index < kPDEventMax) {
				pdEventSet(index, value);
			}
		}
		OSSafeReleaseNULL(obj);
	}
}

// A resource's record in region B, where a real host keeps it
uint8_t *
IOSurfaceRootUserClient::recordFor(uint32_t id)
{
	if (fRegionB == NULL || id == 0 || (uint64_t)(id + 1) * 0xc0 > fRegionB->getLength()) {
		return NULL;
	}
	return (uint8_t *)fRegionB->getBytesNoCopy() + id * 0xc0;
}

// The host stamps every resource a submission touched: slot 0 when read, all three when
// written, {1, (submission stamp << 8) | command queue index}
void
IOSurfaceRootUserClient::markUsed(uint32_t id, bool written)
{
	char key[16];
	IOBufferMemoryDescriptor *rmd;

	if (id == 0) {
		return;
	}
	uint8_t *rec = recordFor(id);

	if (rec == NULL) {
		if (fRecords == NULL) {
			return;
		}
		snprintf(key, sizeof(key), "%u", id);
		rmd = OSDynamicCast(IOBufferMemoryDescriptor, fRecords->getObject(key));
		if (rmd == NULL || rmd->getLength() < 0xc0) {
			return;
		}
		rec = (uint8_t *)rmd->getBytesNoCopy();
	}
	for (uint32_t slot = 0; slot < (written ? 3U : 1U); slot++) {
		volatile uint32_t *w = (volatile uint32_t *)(rec + slot * 0x40);

		w[1] = (fCurStamp << 8) | ((fSubmitQueue - 1) & 0xff);
		w[0] = 1;
	}
}

// A compute dispatch through the daemon: the kernel, the bound buffers and textures copied
// into the bridge, run, and copied back (a kernel may write any of them)
void
IOSurfaceRootUserClient::runCompute(uint32_t pipeline, const uint32_t *cBuf, const uint64_t *cOff, const uint32_t *cTex,
    const uint32_t *cTg, const uint64_t *grid, const uint64_t *tpg, bool threads)
{
	uint32_t function = pipeline < PD_MAX_OBJ ? fPipelineFunction[pipeline] : 0;
	IOBufferMemoryDescriptor *bmd[PD_CS_BUFS] = {}, *tmd[16] = {};

	// a type 0xe pipeline message names a compute pipeline's kernel as tag 1, like a vertex function
	if (function == 0 && pipeline < PD_MAX_OBJ) {
		function = fRenderVertexFn[pipeline];
	}
	char key[16];

	if (sBridge == NULL || function >= PD_MAX_OBJ || shader(function) == NULL) {
		IOLog("PDIOSurface: compute skipped, pipeline %u has no shader\n", pipeline);
		return;
	}
	PDBridgeGuard bridgeGuard;
	if (!pdBridgeIdle(2000)) {
		IOLog("PDIOSurface: compute skipped, shader daemon busy or absent\n");
		return;
	}
	PDBridgeHeader *h = (PDBridgeHeader *)sBridge->getBytesNoCopy();
	uint8_t *base = (uint8_t *)h;
	PDComputeRequest *r = (PDComputeRequest *)(base + PD_DRAW_REQ);
	uint32_t pos = PD_DRAW_DATA;

	bzero(r, sizeof(*r));
	r->fn = shaderKey(function);
	r->mOff = pos; r->mSize = (uint32_t)shader(function)->getLength();
	memcpy(base + pos, shader(function)->getBytesNoCopy(), r->mSize);
	pos = (pos + r->mSize + 63) & ~63U;
	r->threads = threads ? 1 : 0;
	memcpy(r->grid, grid, sizeof(r->grid));
	memcpy(r->tpg, tpg, sizeof(r->tpg));
	memcpy(r->tgmem, cTg, sizeof(r->tgmem));
	for (uint32_t bi = 0; bi < PD_CS_BUFS; bi++) {
		uint64_t size;

		if (cBuf[bi] == 0) continue;
		snprintf(key, sizeof(key), "%u", cBuf[bi]);
		bmd[bi] = lookupBuffer(key);
		if (bmd[bi] == NULL || cOff[bi] >= bmd[bi]->getLength()) {
			bmd[bi] = NULL;
			continue;
		}
		size = bmd[bi]->getLength() - cOff[bi];
		if (size > 0x2000000) size = 0x2000000;
		if (pos + size > PD_BRIDGE_BYTES) {
			bmd[bi] = NULL;
			continue;
		}
		r->buf[bi].id = cBuf[bi]; r->buf[bi].off = pos; r->buf[bi].size = (uint32_t)size;
		memcpy(base + pos, (const uint8_t *)bmd[bi]->getBytesNoCopy() + cOff[bi], size);
		pos = (pos + (uint32_t)size + 63) & ~63U;
	}
	for (uint32_t ti = 0; ti < 16; ti++) {
		uint32_t id = cTex[ti], w, hgt, fmt, bpr;
		uint64_t bytes;

		if (id == 0 || id >= PD_MAX_OBJ || fTexW[id] == 0) continue;
		// a view's memory is its parent's: ids are reused, so a buffer left under the view's own
		// id belongs to whatever object had it before
		snprintf(key, sizeof(key), "%u", fTexAlias[id] != 0 ? fTexAlias[id] : id);
		// a texture made on a surface reads the surface, never a buffer left under its reused id
		tmd[ti] = fTexAlias[id] == 0 && fTexSurf[id] != 0 && sSurfaces[fTexSurf[id]].md != NULL ?
		    sSurfaces[fTexSurf[id]].md : lookupBuffer(key);
		if (tmd[ti] == NULL) {
			continue;
		}
		w = fTexW[id]; hgt = fTexH[id];
		fmt = texFmt(id, tmd[ti]->getLength());
		bpr = w * pdTexBpp(fmt);
		if (fTexBpr[id] >= bpr && fTexBpr[id] < 0x10000) bpr = fTexBpr[id];
		if (fTexSurf[id] != 0 && sSurfaces[fTexSurf[id]].bytesPerRow >= bpr && sSurfaces[fTexSurf[id]].bytesPerRow < 0x10000) {
			bpr = (uint32_t)sSurfaces[fTexSurf[id]].bytesPerRow;
		}
		bytes = (uint64_t)bpr * hgt;
		if (bytes > tmd[ti]->getLength() || pos + bytes > PD_BRIDGE_BYTES) {
			IOLog("PDIOSurface: compute texture %u (%ux%u) does not fit\n", id, w, hgt);
			tmd[ti] = NULL;
			continue;
		}
		r->tex[ti].id = id; r->tex[ti].off = pos; r->tex[ti].w = w; r->tex[ti].h = hgt;
		r->tex[ti].bpr = bpr; r->tex[ti].fmt = fmt;
		memcpy(base + pos, tmd[ti]->getBytesNoCopy(), bytes);
		pos = (pos + (uint32_t)bytes + 63) & ~63U;
	}
	h->serial = ++sBridgeSerial; h->functionID = r->fn; h->pipelineID = pipeline; h->kind = 3;
	pdBridgeRing(h);
	pdBridgeWait(h, 60000);
	__sync_synchronize();
	if (h->state == 2) {
		for (uint32_t bi = 0; bi < PD_CS_BUFS; bi++) {
			if (bmd[bi] != NULL && r->buf[bi].size) {
				memcpy((uint8_t *)bmd[bi]->getBytesNoCopy() + cOff[bi], base + r->buf[bi].off, r->buf[bi].size);
			}
		}
		for (uint32_t ti = 0; ti < 16; ti++) {
			if (tmd[ti] != NULL && r->tex[ti].w) {
				memcpy(tmd[ti]->getBytesNoCopy(), base + r->tex[ti].off, (uint64_t)r->tex[ti].bpr * r->tex[ti].h);
			}
		}
	} else {
		IOLog("PDIOSurface: compute serial %u failed, state %u: %s\n", h->serial, h->state, h->error);
	}
	h->state = 0;
}

// Encoder storage is [stream bytes][encoder kind] then commands of [opcode][command bytes,
// header included][packed arguments]. Each snapshot runs as is, in submission order
void
IOSurfaceRootUserClient::executeStreams(IOBufferMemoryDescriptor **snap)
{
	for (uint32_t e = 0; e < 32; e++) {
		IOBufferMemoryDescriptor *md = snap[e];

		if (md == NULL) {
			continue;
		}
		const uint8_t *p = (const uint8_t *)md->getBytesNoCopy();
		uint32_t total, off = 8;
		uint32_t currentPipeline = 0, cBuf[PD_CS_BUFS] = {}, cTex[16] = {}, cTg[4] = {};
		uint64_t cOff[PD_CS_BUFS] = {};
		// Render encoder state: pipeline, vertex and fragment buffers, fragment textures
		uint32_t rPipeline = 0, vBuf[PD_DRAW_BUFS] = {}, fBuf[PD_DRAW_BUFS] = {}, fTex[16] = {}, fSamp[16] = {}, rTarget = 0;
		uint32_t rAtt[4] = {};	// the pass's colour attachments 1-3 (RenderBox: coverage 1, layer 2)
		bool rScratch[4] = {};	// attachment k has no memory of the target's size: it uses fAttScratch[k]
		uint64_t vOff[PD_DRAW_BUFS] = {}, fOff[PD_DRAW_BUFS] = {};
		double viewport[6] = {};

		memcpy(&total, p, 4);
		if (total < 16 || total > md->getLength()) {
			continue;
		}
		while (off + 8 <= total) {
			uint32_t op, clen;

			memcpy(&op, p + off, 4);
			memcpy(&clen, p + off + 4, 4);
			if (clen < 8 || off + clen > total) {
				IOLog("PDIOSurface: %s bad command length 0x%x at +0x%x\n", fTag, clen, off);
				break;
			}
			if (op == 0xd0 && clen >= 0x0c) {
				memcpy(&currentPipeline, p + off + 8, 4);
			} else if (op == 0xcb && clen >= 0x1c) {
				// {index, kind, id, u64 offset}: setBuffer and setBytes alike (bytes land in a staging buffer)
				uint32_t index = 0, id = 0;
				uint64_t boff = 0;

				memcpy(&index, p + off + 8, 4);
				memcpy(&id, p + off + 16, 4);
				memcpy(&boff, p + off + 20, 8);
				if (index < PD_CS_BUFS) { cBuf[index] = id; cOff[index] = boff; }
			} else if (op == 0xce && clen >= 0x14) {
				// {index, kind, texture id}
				uint32_t index = 0, id = 0;

				memcpy(&index, p + off + 8, 4);
				memcpy(&id, p + off + 16, 4);
				if (index < 16) cTex[index] = id;
			} else if (op == 0xd3 && clen >= 0x14) {
				// setThreadgroupMemoryLength {u32 length, u64 index}
				uint32_t length = 0, index = 0;

				memcpy(&length, p + off + 8, 4);
				memcpy(&index, p + off + 16, 4);
				if (index < 4) cTg[index] = length;
			} else if (op == 0xcf && clen >= 0x14) {
				// setBufferOffset {index, u64 offset}
				uint32_t index = 0;
				uint64_t boff = 0;

				memcpy(&index, p + off + 8, 4);
				memcpy(&boff, p + off + 12, 8);
				if (index < PD_CS_BUFS) cOff[index] = boff;
			} else if (op == 0xc9 && clen >= 0x2c) {
				// dispatchThreadgroupsWithIndirectBuffer {u64 threadsPerGroup[3], u64 offset, u32 buffer}:
				// the threadgroup counts are three u32s in the buffer
				uint64_t tpg[3], ioff = 0, grid[3] = { 0, 0, 0 };
				uint32_t ibuf = 0, counts[3] = { 0, 0, 0 };
				char key[16];
				IOBufferMemoryDescriptor *imd;

				memcpy(tpg, p + off + 8, sizeof(tpg));
				memcpy(&ioff, p + off + 32, 8);
				memcpy(&ibuf, p + off + 40, 4);
				snprintf(key, sizeof(key), "%u", ibuf);
				imd = lookupBuffer(key);
				if (imd != NULL && ioff + sizeof(counts) <= imd->getLength()) {
					memcpy(counts, (const uint8_t *)imd->getBytesNoCopy() + ioff, sizeof(counts));
					grid[0] = counts[0]; grid[1] = counts[1]; grid[2] = counts[2];
					runCompute(currentPipeline, cBuf, cOff, cTex, cTg, grid, tpg, false);
				} else {
					IOLog("PDIOSurface: %s indirect dispatch buffer %u+0x%llx unknown\n", fTag, ibuf, (unsigned long long)ioff);
				}
			} else if ((op == 0xc8 || op == 0xca) && clen >= 0x38) {
				// dispatchThreadgroups (0xc8) and dispatchThreads (0xca): {u64 grid[3], u64 threadsPerGroup[3]}
				uint64_t grid[3], tpg[3];

				memcpy(grid, p + off + 8, sizeof(grid));
				memcpy(tpg, p + off + 32, sizeof(tpg));
				// an axis of 0 threads per group asks for an infinite grid (a pipeline thread limit that
				// reads 0 somewhere): run it 1024/width threads high over the bound textures instead
				if (tpg[1] == 0 && grid[1] > 0xffffff) {
					uint64_t ext = 1;

					for (uint32_t ti = 0; ti < 16; ti++) {
						if (cTex[ti] != 0 && cTex[ti] < PD_MAX_OBJ && fTexH[cTex[ti]] > ext)
							ext = fTexH[cTex[ti]];
					}
					tpg[1] = tpg[0] != 0 && tpg[0] < 1024 ? 1024 / tpg[0] : 1;
					grid[1] = op == 0xca ? ext : (ext + tpg[1] - 1) / tpg[1];
				}
				runCompute(currentPipeline, cBuf, cOff, cTex, cTg, grid, tpg, op == 0xca);
			} else if (op == 0xcc || op == 0xd8 || op == 0xdb || op == 0xd7 || op == 0xd1 || op == 0x86) {
				// sampler (the daemon samples one way), imageblock size, encoder begin and end
			} else if (op == 0x132 && clen >= 0x20) {
				uint32_t id, value;
				uint64_t start, count;
				char key[16];
				IOBufferMemoryDescriptor *buf;

				memcpy(&id, p + off + 8, 4);
				memcpy(&start, p + off + 12, 8);
				memcpy(&count, p + off + 20, 8);
				memcpy(&value, p + off + 28, 4);
				snprintf(key, sizeof(key), "%u", id);
				buf = lookupBuffer(key);
				if (buf != NULL && start <= buf->getLength() && count <= buf->getLength() - start) {
					memset((uint8_t *)buf->getBytesNoCopy() + start, (int)value, count);
				} else {
					IOLog("PDIOSurface: %s fillBuffer %u out of range or unknown\n", fTag, id);
				}
			} else if (op == 0x12d && clen >= 0x28) {
				uint32_t src, dst;
				uint64_t soff, doff, count;
				char key[16];
				IOBufferMemoryDescriptor *sb, *db;

				memcpy(&src, p + off + 8, 4);
				memcpy(&dst, p + off + 12, 4);
				memcpy(&soff, p + off + 16, 8);
				memcpy(&doff, p + off + 24, 8);
				memcpy(&count, p + off + 32, 8);
				snprintf(key, sizeof(key), "%u", src);
				sb = lookupBuffer(key);
				snprintf(key, sizeof(key), "%u", dst);
				db = lookupBuffer(key);
				if (sb != NULL && db != NULL && soff <= sb->getLength() && count <= sb->getLength() - soff &&
				    doff <= db->getLength() && count <= db->getLength() - doff) {
					memmove((uint8_t *)db->getBytesNoCopy() + doff,
					    (const uint8_t *)sb->getBytesNoCopy() + soff, count);
				} else {
					IOLog("PDIOSurface: %s copyBuffer %u -> %u out of range or unknown\n", fTag, src, dst);
				}
			} else if (op == 0x1a && clen >= 0x250) {
				// Render pass begin. Colour attachment k sits at +0x50 + k * 0x3c (packed):
				// texture id +4, load action u16 +0x18, clear doubles +0x20
				for (uint32_t k = 1; k < 4; k++) {
					rScratch[k] = false;
				}
				for (uint32_t k = 0; k < 4; k++) {
					const uint8_t *a = p + off + 0x50 + k * 0x3c;
					uint32_t id;
					uint16_t load;
					uint64_t c[4];
					char key[16];
					IOBufferMemoryDescriptor *tex;

					memcpy(&id, a + 4, 4);
					memcpy(&load, a + 0x18, 2);
					memcpy(c, a + 0x20, sizeof(c));
					if (k > 0)
						rAtt[k] = id < PD_MAX_OBJ ? id : 0;
					// an attachment without memory of attachment 0's size is memoryless: scratch, cleared
					if (k > 0 && rAtt[k] != 0 && rTarget != 0 && rTarget < PD_MAX_OBJ) {
						char sk[16];
						IOBufferMemoryDescriptor *have;
						uint64_t need = (uint64_t)fTexW[rTarget] * fTexH[rTarget] * 8;

						snprintf(sk, sizeof(sk), "%u", rAtt[k]);
						have = lookupBuffer(sk);
						if (have == NULL || fTexW[rAtt[k]] != fTexW[rTarget] || fTexH[rAtt[k]] != fTexH[rTarget]) {
							if (fAttScratch[k] == NULL || fAttScratch[k]->getLength() < need) {
								OSSafeReleaseNULL(fAttScratch[k]);
								fAttScratch[k] = IOBufferMemoryDescriptor::withCapacity(need, kIODirectionInOut);
								if (fAttScratch[k] != NULL)
									fAttScratch[k]->setLength(need);
							}
							if (fAttScratch[k] != NULL) {
								rScratch[k] = true;
								if (load != 1)
									bzero(fAttScratch[k]->getBytesNoCopy(), fAttScratch[k]->getLength());
							}
							continue;
						}
					}
					if (id == 0 || id >= PD_MAX_OBJ) {
						continue;
					}
					if (k == 0)
						rTarget = id;
					markUsed(id, true);
					snprintf(key, sizeof(key), "%u", id);
					tex = lookupBuffer(key);
					if (tex == NULL && fTexSurf[id] != 0 && sSurfaces[fTexSurf[id]].md != NULL) {
						tex = sSurfaces[fTexSurf[id]].md;
					}
					// dontCare (0) leaves the contents undefined: zeros, so last frame never shows through
					if (tex != NULL && load == 0) {
						bzero(tex->getBytesNoCopy(), tex->getLength());
					} else if (tex != NULL && load == 2) {
						// in the texture's own format: half RGBA, BGRA8, RGBA8, or one byte
						uint32_t fmt = texFmt(id, tex->getLength());
						uint8_t px[8];
						uint32_t bpp = pdTexBpp(fmt);
						uint8_t *t = (uint8_t *)tex->getBytesNoCopy();

						if (fmt == 1) {
							for (int ch = 0; ch < 4; ch++) {
								uint16_t h = halfFromDoubleBits(c[ch]);
								memcpy(px + ch * 2, &h, 2);
							}
						} else if (fmt == 4) {
							for (int ch = 0; ch < 4; ch++) {
								px[ch] = unorm8FromDoubleBits(c[ch]);
							}
						} else if (fmt == 2) {
							px[0] = unorm8FromDoubleBits(c[3]);
						} else if (fmt == 3) {
							px[0] = unorm8FromDoubleBits(c[0]);
						} else {
							for (int ch = 0; ch < 4; ch++) {
								px[ch == 0 ? 2 : (ch == 2 ? 0 : ch)] = unorm8FromDoubleBits(c[ch]);
							}
						}
						for (uint64_t i = 0; i + bpp <= tex->getLength(); i += bpp) {
							memcpy(t + i, px, bpp);
						}
					}
				}
			} else if ((op == 0x133 || op == 0x134 || op == 0x135) && clen >= 0x0c) {
				// generateMipmapsForTexture (0x133) and optimizeContentsFor{CPU,GPU}Access (0x134/5):
				// {texture}. Mip levels are packed after level 0, a 2x2 box filter fills them
				uint32_t tid = 0;
				char key[16];
				IOBufferMemoryDescriptor *tex;

				memcpy(&tid, p + off + 8, 4);
				markUsed(tid, true);
				snprintf(key, sizeof(key), "%u", tid);
				tex = op == 0x133 ? lookupBuffer(key) : NULL;
				if (tex != NULL && tid < PD_MAX_OBJ && fTexW[tid] != 0 && fTexH[tid] != 0) {
					uint8_t *base = (uint8_t *)tex->getBytesNoCopy();
					uint64_t len = tex->getLength(), at = 0;
					uint32_t w = fTexW[tid], h = fTexH[tid];

					while ((w > 1 || h > 1) && at + (uint64_t)w * h * 4 <= len) {
						uint32_t nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
						uint64_t next = at + (uint64_t)w * h * 4;

						if (next + (uint64_t)nw * nh * 4 > len) {
							break;
						}
						for (uint32_t y = 0; y < nh; y++) {
							for (uint32_t x = 0; x < nw; x++) {
								for (uint32_t c = 0; c < 4; c++) {
									uint32_t x1 = w > 1 ? 2 * x + 1 : 0, y1 = h > 1 ? 2 * y + 1 : 0;
									uint32_t sum = base[at + ((uint64_t)(2 * y) * w + 2 * x) * 4 + c] +
									    base[at + ((uint64_t)(2 * y) * w + x1) * 4 + c] +
									    base[at + ((uint64_t)y1 * w + 2 * x) * 4 + c] +
									    base[at + ((uint64_t)y1 * w + x1) * 4 + c];

									base[next + ((uint64_t)y * nw + x) * 4 + c] = (uint8_t)((sum + 2) / 4);
								}
							}
						}
						at = next;
						w = nw;
						h = nh;
					}
				}
			} else if ((op == 0x12f && clen >= 0x60) || (op == 0x13e && clen >= 0x10)) {
				// copyFromTexture: 0x12f {src, dst, u64 src origin xyz, size whd, dst origin xyz,
				// u16 src slice, src level, dst slice, dst level}, 0x13e {src, dst} whole texture
				uint32_t src = 0, dst = 0;
				uint64_t sx = 0, sy = 0, w = 0, h = 0, dx = 0, dy = 0;
				uint16_t lv[4] = { 0, 0, 0, 0 };
				char key[16];
				IOBufferMemoryDescriptor *sb, *db;

				memcpy(&src, p + off + 8, 4);
				memcpy(&dst, p + off + 12, 4);
				markUsed(src, false);
				markUsed(dst, true);
				if (op == 0x12f) {
					memcpy(&sx, p + off + 0x10, 8);
					memcpy(&sy, p + off + 0x18, 8);
					memcpy(&w, p + off + 0x28, 8);
					memcpy(&h, p + off + 0x30, 8);
					memcpy(&dx, p + off + 0x40, 8);
					memcpy(&dy, p + off + 0x48, 8);
					memcpy(lv, p + off + 0x58, sizeof(lv));
				} else if (src < PD_MAX_OBJ) {
					w = fTexW[src];
					h = fTexH[src];
				}
				snprintf(key, sizeof(key), "%u", src);
				sb = lookupBuffer(key);
				snprintf(key, sizeof(key), "%u", dst);
				db = lookupBuffer(key);
				if (sb != NULL && db != NULL && src < PD_MAX_OBJ && dst < PD_MAX_OBJ && fTexW[src] != 0 && fTexW[dst] != 0) {
					// Level n of a packed chain starts after levels 0..n-1
					uint64_t so = 0, doff = 0;
					uint32_t sw = fTexW[src], sh = fTexH[src], dw = fTexW[dst], dh = fTexH[dst];

					for (uint16_t l = 0; l < lv[1] && (sw > 1 || sh > 1); l++) {
						so += (uint64_t)sw * sh * 4; sw = sw > 1 ? sw / 2 : 1; sh = sh > 1 ? sh / 2 : 1;
					}
					for (uint16_t l = 0; l < lv[3] && (dw > 1 || dh > 1); l++) {
						doff += (uint64_t)dw * dh * 4; dw = dw > 1 ? dw / 2 : 1; dh = dh > 1 ? dh / 2 : 1;
					}
					if (w <= sw && sx <= sw - w && h <= sh && sy <= sh - h && w <= dw && dx <= dw - w && h <= dh &&
					    dy <= dh - h && so + (uint64_t)sw * sh * 4 <= sb->getLength() &&
					    doff + (uint64_t)dw * dh * 4 <= db->getLength()) {
						for (uint64_t y = 0; y < h; y++) {
							memcpy((uint8_t *)db->getBytesNoCopy() + doff + ((dy + y) * dw + dx) * 4,
							    (const uint8_t *)sb->getBytesNoCopy() + so + ((sy + y) * sw + sx) * 4, w * 4);
						}
					}
				}
			} else if (op == 0x12c && clen >= 0x50) {
				// copyFromBuffer:...toTexture: rows of a BGRA8 source into the texture
				uint32_t src, dst;
				uint64_t soff, bpr, w, h, ox, oy;
				char key[16];
				IOBufferMemoryDescriptor *sb, *tex;

				memcpy(&src, p + off + 8, 4);
				memcpy(&dst, p + off + 12, 4);
				memcpy(&soff, p + off + 0x10, 8);
				memcpy(&bpr, p + off + 0x18, 8);
				memcpy(&w, p + off + 0x28, 8);
				memcpy(&h, p + off + 0x30, 8);
				memcpy(&ox, p + off + 0x40, 8);
				memcpy(&oy, p + off + 0x48, 8);
				snprintf(key, sizeof(key), "%u", src);
				sb = lookupBuffer(key);
				snprintf(key, sizeof(key), "%u", dst);
				tex = lookupBuffer(key);
				if (sb != NULL && tex != NULL && dst < PD_MAX_OBJ && fTexW[dst] != 0 && w <= fTexW[dst] &&
				    ox <= fTexW[dst] - w && h <= fTexH[dst] && oy <= fTexH[dst] - h && bpr >= w * 4 &&
				    soff <= sb->getLength() && h * bpr <= sb->getLength() - soff) {
					for (uint64_t y = 0; y < h; y++) {
						const uint8_t *row = (const uint8_t *)sb->getBytesNoCopy() + soff + y * bpr;

						memcpy((uint8_t *)tex->getBytesNoCopy() + ((oy + y) * fTexW[dst] + ox) * 4, row, w * 4);
					}
				} else {
					IOLog("PDIOSurface: %s copyBufferToTexture %u -> %u rejected\n", fTag, src, dst);
				}
			} else if (op == 0x12e && clen >= 0x58) {
				// copyFromTexture:...toBuffer: BGRA8 rows out of the texture
				uint32_t src, dst;
				uint64_t ox, oy, w, h, doff, bpr;
				char key[16];
				IOBufferMemoryDescriptor *tex, *db;

				memcpy(&src, p + off + 8, 4);
				memcpy(&dst, p + off + 12, 4);
				memcpy(&ox, p + off + 0x10, 8);
				memcpy(&oy, p + off + 0x18, 8);
				memcpy(&w, p + off + 0x28, 8);
				memcpy(&h, p + off + 0x30, 8);
				memcpy(&doff, p + off + 0x40, 8);
				memcpy(&bpr, p + off + 0x48, 8);
				snprintf(key, sizeof(key), "%u", src);
				tex = lookupBuffer(key);
				snprintf(key, sizeof(key), "%u", dst);
				db = lookupBuffer(key);
				if (tex != NULL && db != NULL && src < PD_MAX_OBJ && fTexW[src] != 0 && w <= fTexW[src] &&
				    ox <= fTexW[src] - w && h <= fTexH[src] && oy <= fTexH[src] - h && bpr >= w * 4 &&
				    doff <= db->getLength() && h * bpr <= db->getLength() - doff) {
					for (uint64_t y = 0; y < h; y++) {
						memcpy((uint8_t *)db->getBytesNoCopy() + doff + y * bpr,
						    (const uint8_t *)tex->getBytesNoCopy() + ((oy + y) * fTexW[src] + ox) * 4, w * 4);
					}
				} else {
					IOLog("PDIOSurface: %s copyTextureToBuffer %u -> %u rejected\n", fTag, src, dst);
				}
			} else if (op == 0x74 && clen >= 0x0c) {
				memcpy(&rPipeline, p + off + 8, 4);
			} else if (op == 0x82 && clen >= 0x38) {
				memcpy(viewport, p + off + 8, sizeof(viewport));
			} else if ((op == 0x6e || op == 0x7d) && clen >= 0x1c) {
				uint32_t index = 0, id = 0;
				uint64_t boff = 0;

				memcpy(&index, p + off + 8, 4);
				memcpy(&id, p + off + 16, 4);
				memcpy(&boff, p + off + 20, 8);
				// 0x7d binds a vertex buffer, 0x6e a fragment buffer (the pipelines' air
				// metadata says which index each stage reads)
				if (index < PD_DRAW_BUFS) {
					if (op == 0x7d) { vBuf[index] = id; vOff[index] = boff; }
					else { fBuf[index] = id; fOff[index] = boff; }
				}
			} else if ((op == 0x6f || op == 0x7e) && clen >= 0x10) {
				// set{Fragment,Vertex}BufferOffset:atIndex: {index, u64 offset}: only the offset of
				// the buffer bound at index moves (set*Bytes arrive as ordinary 0x6e/0x7d binds)
				uint32_t index = 0;
				uint64_t boff = 0;

				memcpy(&index, p + off + 8, 4);
				memcpy(&boff, p + off + 12, 8);
				if (index < PD_DRAW_BUFS) {
					if (op == 0x7e) vOff[index] = boff;
					else fOff[index] = boff;
				}
			} else if ((op == 0x72 || op == 0x70) && clen >= 0x14) {
				uint32_t index = 0, id = 0;

				memcpy(&index, p + off + 8, 4);
				memcpy(&id, p + off + 16, 4);
				if (index < 16) {
					if (op == 0x72) fTex[index] = id; else fSamp[index] = id;
				}
			} else if ((op == 0x68 && clen >= 0x0c) || (op == 0x77 && clen >= 0x10)) {
				// setDepthStencilState {id} and stencil reference values {front, back}: the
				// rasteriser has no stencil buffer yet, masked draws come out unclipped
			} else if ((op == 0x07 || op == 0x09 || op == 0x01 || op == 0x03 || (op == 0x06 && clen >= 0x20)) && clen >= 0x10) {
				// A draw. 0x03 is the instanced form {u16 start, count, instances, primitive}, 0x09 is
				// 0x07 with an instance count after it (drawIndexedPrimitives:...:instanceCount:)
				uint32_t prim = 0, ibuf = 0, a = 0, b = 0, instances = 1;
				uint16_t c16 = 0, d16 = 0;
				bool idx32 = false;

				memcpy(&prim, p + off + 8, 4);
				if (op == 0x03) {
					uint16_t inst = 0, prim16 = 0;

					memcpy(&c16, p + off + 8, 2);
					memcpy(&d16, p + off + 10, 2);
					memcpy(&inst, p + off + 12, 2);
					memcpy(&prim16, p + off + 14, 2);
					prim = prim16;
					a = d16; b = c16;
					if (inst > 1)
						instances = inst;
				} else if (op == 0x06) {
					// drawIndexedPrimitives with wide fields: {prim, index buffer, u32 count,
					// u32 index type (1: 32-bit), u64 byte offset}
					uint32_t itype = 0;
					uint64_t ioff = 0;

					memcpy(&ibuf, p + off + 12, 4);
					memcpy(&a, p + off + 16, 4);
					memcpy(&itype, p + off + 20, 4);
					memcpy(&ioff, p + off + 24, 8);
					b = ioff < 0xffffffffULL ? (uint32_t)ioff : 0xffffffffu;
					idx32 = itype == 1;
				} else if ((op == 0x07 || op == 0x09)) {
					memcpy(&ibuf, p + off + 12, 4);
					memcpy(&c16, p + off + 16, 2);
					memcpy(&d16, p + off + 18, 2);
					a = c16; b = d16;
					// 0x09 is {prim, index buffer, u16 count, u16 byte offset, instances}. RenderBox's
					// index buffer holds 0 1 2 3 .. at 0 and quad strips 1 0 2 3 ffff .. at +0x100
					if (op == 0x09 && clen >= 0x18) {
						uint32_t inst = 0;

						memcpy(&inst, p + off + 20, 4);
						if (inst > 1)
							instances = inst;
					}
				} else {
					memcpy(&c16, p + off + 12, 2);
					memcpy(&d16, p + off + 14, 2);
					a = d16; b = c16;
				}
				// What this draw reads and writes, for the host's per-resource usage records
				for (uint32_t bi = 0; bi < PD_DRAW_BUFS; bi++) {
					markUsed(vBuf[bi], false);
					markUsed(fBuf[bi], false);
				}
				for (uint32_t ti = 0; ti < 16; ti++) {
					markUsed(fTex[ti], false);
				}
				markUsed(rTarget, true);
				// Hand it to the daemon when every piece is at hand
				{
					PDBridgeGuard bridgeGuard;
					uint32_t vfn = rPipeline < PD_MAX_OBJ ? fRenderVertexFn[rPipeline] : 0;
					uint32_t ffn = rPipeline < PD_MAX_OBJ ? fRenderFragmentFn[rPipeline] : 0;
					char key[16];
					IOBufferMemoryDescriptor *tgt;

					snprintf(key, sizeof(key), "%u", rTarget);
					// a target made on a surface draws into the surface, as its textures read it
					tgt = rTarget < PD_MAX_OBJ && fTexAlias[rTarget] == 0 && fTexSurf[rTarget] != 0 &&
					    sSurfaces[fTexSurf[rTarget]].md != NULL ? sSurfaces[fTexSurf[rTarget]].md : lookupBuffer(key);
					if (!(sBridge != NULL && shader(vfn) != NULL && shader(ffn) != NULL && tgt != NULL &&
					    rTarget < PD_MAX_OBJ && fTexW[rTarget] != 0 && fTexH[rTarget] != 0 && (prim == 3 || prim == 4))) {
						IOLog("PDIOSurface: draw skipped, pipeline %u target %u prim %u incomplete\n", rPipeline, rTarget, prim);
					} else if (!pdBridgeIdle(2000)) {
						IOLog("PDIOSurface: draw skipped, shader daemon busy or absent\n");
					} else {
						PDBridgeHeader *h = (PDBridgeHeader *)sBridge->getBytesNoCopy();
						uint8_t *base = (uint8_t *)h;
						PDDrawRequest *r = (PDDrawRequest *)(base + PD_DRAW_REQ);
						uint32_t pos = PD_DRAW_DATA, tw = fTexW[rTarget], th = fTexH[rTarget];
						uint32_t tfmt = texFmt(rTarget, tgt->getLength());
						uint32_t tbpr = tw * pdTexBpp(tfmt);
						uint64_t tbytes = (uint64_t)tbpr * th;

						if (tbytes > tgt->getLength()) {
							IOLog("PDIOSurface: draw target %u (%ux%u) is larger than its buffer\n", rTarget, tw, th);
						} else if (h->status == 0x44525652 && h->state == 0 && tbytes <= PD_BRIDGE_BYTES / 2) {
							bzero(r, sizeof(*r));
							r->vfn = shaderKey(vfn); r->ffn = shaderKey(ffn);
							r->vmOff = pos; r->vmSize = (uint32_t)shader(vfn)->getLength();
							memcpy(base + pos, shader(vfn)->getBytesNoCopy(), r->vmSize); pos = (pos + r->vmSize + 63) & ~63U;
							r->fmOff = pos; r->fmSize = (uint32_t)shader(ffn)->getLength();
							memcpy(base + pos, shader(ffn)->getBytesNoCopy(), r->fmSize); pos = (pos + r->fmSize + 63) & ~63U;
							for (uint32_t st = 0; st < 2; st++) {
								for (uint32_t bi = 0; bi < PD_DRAW_BUFS; bi++) {
									uint32_t id = st ? fBuf[bi] : vBuf[bi];
									uint64_t boff = st ? fOff[bi] : vOff[bi];
									IOBufferMemoryDescriptor *bmd;
									PDDrawBuffer *slot = st ? &r->fbuf[bi] : &r->vbuf[bi];

									if (id == 0) continue;
									snprintf(key, sizeof(key), "%u", id);
									bmd = id == PD_INLINE_BYTES ? md : lookupBuffer(key);
									if (bmd == NULL || boff >= bmd->getLength()) continue;
									slot->id = id; slot->off = pos;
									slot->size = (uint32_t)((bmd->getLength() - boff) < 0x10000 ? bmd->getLength() - boff : 0x10000);
									memcpy(base + pos, (const uint8_t *)bmd->getBytesNoCopy() + boff, slot->size);
									pos = (pos + slot->size + 63) & ~63U;
								}
							}
							// Fragment textures, in the format the texture request named
							for (uint32_t ti = 0; ti < 16; ti++) {
								uint32_t id = fTex[ti];
								IOBufferMemoryDescriptor *tmd;
								uint32_t w, hgt, fmt, bpr;
								uint64_t bytes, toff;

								if (id == 0) continue;
								if (id >= PD_MAX_OBJ || fTexW[id] == 0) continue;
								// a view or buffer texture reads the memory it was made on, never a buffer
								// left under its reused id
								snprintf(key, sizeof(key), "%u", fTexAlias[id] != 0 ? fTexAlias[id] : id);
								// a texture made on a surface reads the surface, never a buffer left under its reused id
								tmd = fTexAlias[id] == 0 && fTexSurf[id] != 0 && sSurfaces[fTexSurf[id]].md != NULL ?
								    sSurfaces[fTexSurf[id]].md : lookupBuffer(key);
								if (tmd == NULL) continue;
								w = fTexW[id]; hgt = fTexH[id];
								fmt = texFmt(id, tmd->getLength());
								// a texture on a buffer starts at its offset
								toff = fTexAlias[id] != 0 ? fTexOff[id] : 0;
								bpr = w * pdTexBpp(fmt);
								if (fTexBpr[id] >= bpr && fTexBpr[id] < 0x10000) {
									bpr = fTexBpr[id];
								}
								// A surface pads its rows: take its own pitch
								if (fTexSurf[id] != 0 && sSurfaces[fTexSurf[id]].bytesPerRow >= bpr &&
								    sSurfaces[fTexSurf[id]].bytesPerRow < 0x10000) {
									bpr = (uint32_t)sSurfaces[fTexSurf[id]].bytesPerRow;
								}
								bytes = (uint64_t)bpr * hgt;
								// the last row needs only its pixels: an offset texture may end short of a full pitch
								if (toff + bytes > tmd->getLength() && hgt != 0 &&
								    toff + (uint64_t)bpr * (hgt - 1) + (uint64_t)w * pdTexBpp(fmt) <= tmd->getLength())
									bytes = tmd->getLength() - toff;
								if (toff + bytes > tmd->getLength() || bytes > 0x2000000 || pos + bytes > PD_BRIDGE_BYTES / 2) {
									IOLog("PDIOSurface: draw texture %u (%ux%u) does not fit\n", id, w, hgt);
									continue;
								}
								r->ftex[ti].id = id; r->ftex[ti].off = pos; r->ftex[ti].w = w; r->ftex[ti].h = hgt;
								r->ftex[ti].bpr = bpr; r->ftex[ti].fmt = fmt;
								memcpy(base + pos, (const uint8_t *)tmd->getBytesNoCopy() + toff, bytes);
								pos = (uint32_t)((pos + bytes + 63) & ~63ULL);
							}
							r->prim = prim; r->count = a; r->indexed = (op == 0x07 || op == 0x09 || op == 0x06);
							for (uint32_t si = 0; si < 16; si++) {
								r->samp[si] = fSamp[si] < PD_MAX_OBJ ? fSampFlags[fSamp[si]] : 0;
							}
							r->instances = instances > 4096 ? 4096 : instances;
							r->pad0 = rPipeline < PD_MAX_OBJ ? fPipeBlend[rPipeline] : 0;   // blend state for the daemon
							if ((op == 0x07 || op == 0x09 || op == 0x06)) {
								IOBufferMemoryDescriptor *imd;

								snprintf(key, sizeof(key), "%u", ibuf);
								imd = lookupBuffer(key);
								if (imd != NULL && (uint64_t)b + a * (idx32 ? 4 : 2) <= imd->getLength()) {
									r->indexOff = pos; r->indexBytes = a * 2;
									if (idx32) {
										// the daemon reads 16-bit indices: narrow, keeping the strip restart marker
										const uint32_t *src = (const uint32_t *)((const uint8_t *)imd->getBytesNoCopy() + b);
										uint16_t *dst = (uint16_t *)(base + pos);

										for (uint32_t ii = 0; ii < a; ii++) {
											dst[ii] = src[ii] == 0xffffffffu ? 0xffff : (uint16_t)src[ii];
										}
									} else {
										memcpy(base + pos, (const uint8_t *)imd->getBytesNoCopy() + b, a * 2);
									}
									pos = (pos + a * 2 + 63) & ~63U;
								} else {
									r->count = 0;
								}
							}
							r->targetOff = pos; r->tw = tw; r->th = th; r->tbpr = tbpr; r->tfmt = tfmt;
							memcpy(base + pos, tgt->getBytesNoCopy(), tbytes);
							// attachments 1-3 after the target, in their own formats
							{
								uint32_t apos = (uint32_t)((pos + tbytes + 63) & ~63ULL);

								for (uint32_t k = 1; k < 4; k++) {
									IOBufferMemoryDescriptor *amd;
									uint32_t aid = rAtt[k], afmt, abpr;
									uint64_t abytes;

									uint32_t aw, ah;

									if (aid == 0) continue;
									if (rScratch[k]) {
										// memoryless: the pass's scratch, RGBA half at the target's size
										amd = fAttScratch[k];
										aw = tw; ah = th; afmt = 1;
									} else {
										if (fTexW[aid] == 0 || fTexH[aid] == 0) continue;
										snprintf(key, sizeof(key), "%u", aid);
										amd = fTexAlias[aid] == 0 && fTexSurf[aid] != 0 && sSurfaces[fTexSurf[aid]].md != NULL ?
										    sSurfaces[fTexSurf[aid]].md : lookupBuffer(key);
										if (amd == NULL) continue;
										aw = fTexW[aid]; ah = fTexH[aid];
										afmt = texFmt(aid, amd->getLength());
									}
									if (amd == NULL) continue;
									abpr = aw * pdTexBpp(afmt);
									abytes = (uint64_t)abpr * ah;
									if (abytes > amd->getLength() || apos + abytes > PD_BRIDGE_BYTES) continue;
									r->rt[k].id = aid; r->rt[k].off = apos; r->rt[k].w = aw; r->rt[k].h = ah;
									r->rt[k].bpr = abpr; r->rt[k].fmt = afmt;
									r->rtBlend[k] = rPipeline < PD_MAX_OBJ ? fPipeBlendRT[rPipeline][k] : 0;
									memcpy(base + apos, amd->getBytesNoCopy(), abytes);
									apos = (uint32_t)((apos + abytes + 63) & ~63ULL);
								}
							}
							memcpy(r->viewport, viewport, sizeof(viewport));
							if (r->viewport[2] == 0) { r->viewport[2] = tw; r->viewport[3] = th; }
							h->serial = ++sBridgeSerial; h->functionID = r->vfn; h->pipelineID = rPipeline; h->kind = 2;
							pdBridgeRing(h);
							pdBridgeWait(h, 60000);
							__sync_synchronize();
							if (h->state == 2) {
								memcpy(tgt->getBytesNoCopy(), base + pos, tbytes);
								for (uint32_t k = 1; k < 4; k++) {
									IOBufferMemoryDescriptor *amd;

									if (r->rt[k].w == 0) continue;
									snprintf(key, sizeof(key), "%u", r->rt[k].id);
									amd = rScratch[k] ? fAttScratch[k] :
									    fTexAlias[r->rt[k].id] == 0 && fTexSurf[r->rt[k].id] != 0 &&
									    sSurfaces[fTexSurf[r->rt[k].id]].md != NULL ? sSurfaces[fTexSurf[r->rt[k].id]].md : lookupBuffer(key);
									if (amd != NULL && (uint64_t)r->rt[k].bpr * r->rt[k].h <= amd->getLength())
										memcpy(amd->getBytesNoCopy(), base + r->rt[k].off, (uint64_t)r->rt[k].bpr * r->rt[k].h);
								}
							} else {
								IOLog("PDIOSurface: draw serial %u failed, state %u: %s\n", h->serial, h->state, h->error);
							}
							h->state = 0; h->kind = 0;
						} else {
							IOLog("PDIOSurface: draw skipped, target %u (%ux%u) too large or daemon busy\n", rTarget, tw, th);
						}
					}
				}
			} else if (op == 0x75) {
				// scissor, always the full target so far
			} else {
				IOLog("PDIOSurface: %s unknown opcode 0x%x, 0x%x bytes\n", fTag, op, clen);
			}
			off += clen;
		}
	}
}
