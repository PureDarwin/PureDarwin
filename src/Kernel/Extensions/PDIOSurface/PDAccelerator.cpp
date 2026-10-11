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
uint8_t sIntrospected[PD_MAX_SHADERS];
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
	uint32_t tgmem[4];
	uint32_t imageblock[2], pad[2];	// setImageblockWidth:height:, 0 = the threadgroup's (not decoded yet)
	PDDrawBuffer buf[PD_CS_BUFS];
	PDDrawTex tex[16];
	uint32_t samp[16];
};
static_assert(PD_DRAW_REQ + sizeof(PDComputeRequest) <= PD_DRAW_DATA, "compute request overlaps its data");
// a tile dispatch (kind 4): the tile kernel, threads per tile and the tile grid, what is bound to
// the tile stage, and the pass's colour attachments (its implicit imageblock). All come back after
enum { PD_TILE_SIZE = 32 };
struct PDTileRequest {
	uint32_t fn, mOff, mSize, pad0;
	uint32_t tpt[3], tilesX, tilesY, pad1;
	uint32_t tgmem[4];
	PDDrawBuffer buf[PD_CS_BUFS];
	PDDrawTex tex[16];
	uint32_t samp[16];
	PDDrawTex rt[4];
};
static_assert(PD_DRAW_REQ + sizeof(PDTileRequest) <= PD_DRAW_DATA, "tile request overlaps its data");
IOBufferMemoryDescriptor *sBridge;
// Bytes a pixel of each daemon texture format takes (see texFmt)
static uint32_t
pdTexBpp(uint32_t fmt)
{
	static const uint8_t bpp[] = { 4, 8, 1, 1, 4, 2, 4, 4, 8, 8, 2, 4, 2, 4, 4, 8, 16, 4, 4, 4, 4, 2, 4, 8 };

	return fmt < sizeof(bpp) ? bpp[fmt] : 4;
}

// heap texture size and alignment from the 0x1d5 / sel 261 kind 0x38 descriptor: u8 type +0, u16 format +1,
// u32 usage +3, width +7, height +0xb, depth +0xf, u16 mips +0x13, samples +0x15, array length +0x17
static void
pdHeapTexSize(const uint8_t *d, uint64_t *ans)
{
	uint32_t w, h, depth, type = d[0] & 0x3f, bpp, faces;
	uint16_t pf, mips, samples, slices;
	uint64_t size = 0;
	bool bc;

	memcpy(&pf, d + 1, 2);
	memcpy(&w, d + 7, 4);
	memcpy(&h, d + 0xb, 4);
	memcpy(&depth, d + 0xf, 4);
	memcpy(&mips, d + 0x13, 2);
	memcpy(&samples, d + 0x15, 2);
	memcpy(&slices, d + 0x17, 2);
	bpp = pdMtlBpp(pf);
	bc = pf >= 130 && pf < 250;
	faces = type == 5 || type == 6 ? 6 : 1;
	for (uint32_t l = 0; l < (mips ? mips : 1) && l < 16; l++) {
		uint64_t lw = w >> l ? w >> l : 1, lh = h >> l ? h >> l : 1, ld = depth >> l ? depth >> l : 1;

		if (bc) {
			lw = (lw + 3) / 4;
			lh = (lh + 3) / 4;
		}
		size += lw * bpp * lh * ld;
	}
	size *= (uint64_t)(samples ? samples : 1) * (slices ? slices : 1) * faces;
	ans[0] = size == 0 ? 0x80 : (size + 0x7f) & ~0x7fULL;
	// a real host aligns arrays and cubes to 16 KB, everything else to 128 bytes
	ans[1] = faces > 1 || slices > 1 ? 0x4000 : 0x80;
}

// protected by sBridgeLock. A timeout does not cancel the daemon's access to
// the shared request, reclaim it only after the daemon publishes completion.
static bool sBridgeAbandoned;
#define PD_BRIDGE_ABANDON_MS 900000

void
pdBridgeReclaim(void)
{
	if (!sBridgeAbandoned || sBridge == NULL) return;
	volatile PDBridgeHeader *h = (volatile PDBridgeHeader *)sBridge->getBytesNoCopy();
	if (h->state != 2 && h->state != 3) return;
	__sync_synchronize();
	h->kind = 0;
	h->state = 0;
	sBridgeAbandoned = false;
}

void
pdBridgeRelease(PDBridgeHeader *h)
{
	__sync_synchronize();
	if (h->state == 1) {
		sBridgeAbandoned = true;
		return;
	}
	h->kind = 0;
	h->state = 0;
}

// The daemon takes one job at a time: a draw arriving while it introspects a new shader
// waits for it rather than being dropped (a dropped frame may never be drawn again)
static bool
pdBridgeIdle(uint32_t ms)
{
	volatile PDBridgeHeader *h = (volatile PDBridgeHeader *)sBridge->getBytesNoCopy();

	for (uint32_t n = 0; n <= ms; n++) {
		pdBridgeReclaim();
		if (h->status == 0x44525652 && h->state == 0) {
			return true;
		}
		if (h->status != 0x44525652) {
			return false;
		}
		// a timed-out job still runs (cpu draws can take minutes): wait it out, dropping draws loses layers
		if (sBridgeAbandoned && n == ms && ms < PD_BRIDGE_ABANDON_MS) {
			IOLog("PDIOSurface: shader daemon still on a timed-out job, waiting\n");
			ms = PD_BRIDGE_ABANDON_MS;
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

// a job's memory stays held while the daemon runs it, the client can free its textures meanwhile
static void
pdHoldMemory(IOBufferMemoryDescriptor **md, uint32_t n, bool hold)
{
	for (uint32_t i = 0; i < n; i++) {
		if (md[i] == NULL) continue;
		if (hold) md[i]->retain();
		else md[i]->release();
	}
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
	OSArray *signals;
	void (*perform)(void *);
	void *argument;
	uint32_t surfWrites[PD_SURF_MAX / 32];	// surfaces the job may write, for a CPU lock to wait on
	uint32_t surfUses[PD_SURF_MAX / 32];	// and those it reads or writes
};
static IOLock *sAsyncLock;
static struct PDAsyncJob *sAsyncHead, *sAsyncTail;
static thread_t sAsyncThread;
static uint32_t sAsyncBusy;
static uint16_t sSurfPending[PD_SURF_MAX];	// queued or running jobs that may write each surface
static uint16_t sSurfUsePending[PD_SURF_MAX];	// and that read or write it
static bool sAsyncRendering;
static uint64_t sAsyncStarted;

bool
pdAsyncIdle(void)
{
	if (sAsyncLock == NULL)
		return true;
	IOLockLock(sAsyncLock);
	bool idle = sAsyncHead == NULL && !sAsyncBusy;
	IOLockUnlock(sAsyncLock);
	return idle;
}

// cursor motion can use the immutable completed frame while a long render waits.
bool
pdAsyncRenderingSlow(void)
{
	if (sAsyncLock == NULL)
		return false;
	IOLockLock(sAsyncLock);
	uint64_t elapsed = 0;
	if (sAsyncBusy && sAsyncRendering)
		absolutetime_to_nanoseconds(mach_absolute_time() - sAsyncStarted, &elapsed);
	IOLockUnlock(sAsyncLock);
	return elapsed > 50000000;
}

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

// wait until no queued or running job may still write surface id (a CPU lock of it)
void
pdAsyncDrainSurface(uint32_t id, uint32_t ms, bool readOnly)
{
	uint64_t deadline;

	if (sAsyncLock == NULL || id >= PD_SURF_MAX || current_thread() == sAsyncThread) {
		return;
	}
	clock_interval_to_deadline(ms, kMillisecondScale, &deadline);
	IOLockLock(sAsyncLock);
	// a read-only lock waits for the writers, a writing one for every job that still reads the surface too
	while ((readOnly ? sSurfPending[id] : sSurfUsePending[id]) != 0 && mach_absolute_time() < deadline) {
		IOLockSleepDeadline(sAsyncLock, &sSurfUsePending[id], deadline, THREAD_UNINT);
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

// a non-negative whole-number clear value from its double bits, clamped to max
static uint32_t
uintFromDoubleBits(uint64_t bits, uint32_t max)
{
	int64_t e = (int64_t)((bits >> 52) & 0x7ff) - 1023;
	uint64_t v;

	if ((bits >> 63) || e < 0)
		return 0;
	if (e >= 32)
		return max;
	v = ((bits & ((1ULL << 52) - 1)) | (1ULL << 52)) >> (52 - e);
	return v > max ? max : (uint32_t)v;
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
		sAsyncRendering = job->perform == NULL;
		sAsyncStarted = mach_absolute_time();
		IOLockUnlock(sAsyncLock);

		uint32_t writes[PD_SURF_MAX / 32], uses[PD_SURF_MAX / 32];

		memcpy(writes, job->surfWrites, sizeof(writes));
		memcpy(uses, job->surfUses, sizeof(uses));
		if (job->perform != NULL) {
			job->perform(job->argument);
			IOFree(job, sizeof(*job));
		} else {
			job->client->runAsyncJob(job);
		}

		IOLockLock(sAsyncLock);
		for (uint32_t i = 0; i < PD_SURF_MAX; i++) {
			if ((writes[i / 32] >> (i % 32) & 1) && sSurfPending[i] != 0)
				sSurfPending[i]--;
			if ((uses[i / 32] >> (i % 32) & 1) && sSurfUsePending[i] != 0) {
				sSurfUsePending[i]--;
				IOLockWakeup(sAsyncLock, &sSurfUsePending[i], false);
			}
		}
		sAsyncBusy = 0;
		sAsyncRendering = false;
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

// heap texture sizes (0x1d5) are answered at submit, before the queued job runs: a client that reads one
// before the job ran placed a 9 MB texture in a 4 MB heap and every draw into it was skipped
void
IOSurfaceRootUserClient::answerQueriesNow(IOBufferMemoryDescriptor **snap)
{
	for (uint32_t e = 0; e < 32; e++) {
		const uint8_t *p;
		uint32_t total, off = 8;

		if (snap[e] == NULL)
			continue;
		p = (const uint8_t *)snap[e]->getBytesNoCopy();
		memcpy(&total, p, 4);
		if (total < 16 || total > snap[e]->getLength())
			continue;
		while (off + 8 <= total) {
			uint32_t op, clen;

			memcpy(&op, p + off, 4);
			memcpy(&clen, p + off + 4, 4);
			if (clen < 8 || off + clen > total)
				break;
			if (op == 0x1d5 && clen >= 0x3c) {
				uint32_t qbuf = 0;
				uint64_t qoff = 0, ans[2];

				pdHeapTexSize(p + off + 8, ans);
				memcpy(&qbuf, p + off + 0x30, 4);
				memcpy(&qoff, p + off + 0x34, 8);
				answerQuery(qbuf, qoff, ans, sizeof(ans));
			}
			off += clen;
		}
	}
}

// the surfaces a submission may write: its passes' attachments, blit and copy destinations and compute
// textures, each through its texture's (or view's) surface
void
IOSurfaceRootUserClient::surfaceWrites(IOBufferMemoryDescriptor **snap, uint32_t *mask, uint32_t *uses)
{
	for (uint32_t e = 0; e < 32; e++) {
		const uint8_t *p;
		uint32_t total, off = 8;

		if (snap[e] == NULL)
			continue;
		p = (const uint8_t *)snap[e]->getBytesNoCopy();
		memcpy(&total, p, 4);
		if (total < 16 || total > snap[e]->getLength())
			continue;
		while (off + 8 <= total) {
			uint32_t op, clen, ids[4] = { 0, 0, 0, 0 }, n = 0, rd[16], nr = 0;

			memcpy(&op, p + off, 4);
			memcpy(&clen, p + off + 4, 4);
			if (clen < 8 || off + clen > total)
				break;
			if (op == 0x1a && clen >= 0x250) {
				for (uint32_t k = 0; k < 4; k++) {
					memcpy(&ids[n++], p + off + 0x50 + k * 0x3c + 4, 4);
				}
			} else if ((op == 0x12c && clen >= 0x50) || (op == 0x12f && clen >= 0x60) || (op == 0x13e && clen >= 0x10)) {
				memcpy(&ids[n++], p + off + 12, 4);
			} else if (op == 0xce && clen >= 0x14) {
				memcpy(&ids[n++], p + off + 16, 4);
			}
			// what it reads: fragment textures, blit sources and tile textures
			if (op == 0x72 && clen >= 0x14) {
				memcpy(&rd[nr++], p + off + 16, 4);
			} else if ((op == 0x12e && clen >= 0x58) || (op == 0x12f && clen >= 0x60) || (op == 0x13e && clen >= 0x10)) {
				memcpy(&rd[nr++], p + off + 8, 4);
			} else if (op == 0xa1 && clen >= 0x14) {
				uint32_t cnt = 0;

				memcpy(&cnt, p + off + 12, 4);
				for (uint32_t k = 0; k < cnt && nr < 16 && 16 + 4 * (k + 1) <= clen; k++) {
					memcpy(&rd[nr++], p + off + 16 + 4 * k, 4);
				}
			}
			for (uint32_t k = 0; k < n + nr; k++) {
				uint32_t id = k < n ? ids[k] : rd[k - n];
				uint32_t sid = id != 0 ? texInfo(texSlot(id)).surf : 0;

				if (sid == 0 || sid >= PD_SURF_MAX)
					continue;
				if (k < n)
					mask[sid / 32] |= 1u << (sid % 32);
				uses[sid / 32] |= 1u << (sid % 32);
			}
			off += clen;
		}
	}
}

static void pdAsyncEnqueue(struct PDAsyncJob *job);

bool
pdAsyncPerform(void (*fn)(void *), void *arg)
{
	struct PDAsyncJob *job = (struct PDAsyncJob *)IOMallocZero(sizeof(*job));
	if (job == NULL)
		return false;
	job->perform = fn;
	job->argument = arg;
	pdAsyncEnqueue(job);
	return true;
}


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
	answerQueriesNow(job->snap);
	surfaceWrites(job->snap, job->surfWrites, job->surfUses);
	job->signals = OSArray::withCapacity(8);
	if (job->signals)
		snapshotSignals(job->signals);
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
	for (uint32_t i = 0; i < PD_SURF_MAX; i++) {
		if ((job->surfWrites[i / 32] >> (i % 32) & 1) && sSurfPending[i] != 0xffff)
			sSurfPending[i]++;
		if ((job->surfUses[i / 32] >> (i % 32) & 1) && sSurfUsePending[i] != 0xffff)
			sSurfUsePending[i]++;
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
	if (job->signals) {
		for (uint32_t i = 0; i + 1 < job->signals->getCount(); i += 2) {
			OSNumber *event = OSDynamicCast(OSNumber, job->signals->getObject(i));
			OSNumber *value = OSDynamicCast(OSNumber, job->signals->getObject(i + 1));
			if (event && value)
				pdEventSet(event->unsigned32BitValue() & ~kPDEventTag, value->unsigned64BitValue());
		}
		job->signals->release();
	}
	end = mach_absolute_time();
	if (fSel2MD != NULL && job->queue - 1 < 256) {
		((volatile uint32_t *)fSel2MD->getBytesNoCopy())[job->queue - 1] = job->stamp << 8;
	}
	if (fQueues != NULL && job->block[0] != 0) {
		queueCompletion(job->queue, job->block[0], job->block[1], start, end);
	}
	if (fTraced)
		IOLog("PD-gputrace: %s job queue %u stamp 0x%x blocks 0x%llx 0x%llx queues %u%s\n", fTag, job->queue,
		    job->stamp, (unsigned long long)job->block[0], (unsigned long long)job->block[1],
		    fQueues != NULL ? fQueues->getCount() : 0, fSel2MD != NULL ? " stamped" : "");
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
	idDrop(kIdRid, rid);
	// the object's own state only while it still names this resource: its id may already be reused
	uint32_t o = texSlot(objid);
	if (o != 0 && texInfo(o).res == rid) {
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
		idDrop(kIdObj, objid);
	}
}

// pdobjmap=1 logs the sel 259 destroys no map handles yet
bool
pdObjMapLog(void)
{
	static int state = -1;

	if (state < 0) {
		uint32_t on = 0;

		state = PE_parse_boot_argn("pdobjmap", &on, sizeof(on)) && on;
	}
	return state != 0;
}

// the slot of guest id in kind's map, 0 if it has none (slot 0's record reads zeros)
uint32_t
IOSurfaceRootUserClient::idSlot(uint32_t kind, uint32_t id)
{
	uint32_t s = 0;

	if (id == 0)
		return 0;
	resLock();
	switch (kind) {
	case kIdObj: s = fTexMap.find(id); break;
	case kIdPipe: s = fPipeMap.find(id); break;
	case kIdFn: s = fFnMap.find(id); break;
	case kIdSamp: s = fSampMap.find(id); break;
	case kIdRid: s = fRidMap.find(id); break;
	}
	resUnlock();
	return s;
}

// id's slot, a zeroed one if id is new. 0 when the map is at its cap or out of memory: the object then
// reads as unknown, so that is logged every time
uint32_t
IOSurfaceRootUserClient::idSlotNew(uint32_t kind, uint32_t id)
{
	static const char *names[kIdKinds] = { "object", "pipeline", "function", "sampler", "resource" };
	uint32_t s = 0, count = 0;
	bool fresh;

	if (id == 0 || kind >= kIdKinds)
		return 0;
	resLock();
	switch (kind) {
	case kIdObj: s = fTexMap.get(id, &fresh); count = fTexMap.count; break;
	case kIdPipe: s = fPipeMap.get(id, &fresh); count = fPipeMap.count; break;
	case kIdFn: s = fFnMap.get(id, &fresh); count = fFnMap.count; break;
	case kIdSamp: s = fSampMap.get(id, &fresh); count = fSampMap.count; break;
	case kIdRid: s = fRidMap.get(id, &fresh); count = fRidMap.count; break;
	}
	resUnlock();
	if (s == 0)
		IOLog("PDIOSurface: %s %s id %u has no slot: map full (%u ids) or out of memory\n", fTag, names[kind], id, count);
	return s;
}

// the guest destroyed id: its slot and record go
void
IOSurfaceRootUserClient::idDrop(uint32_t kind, uint32_t id)
{
	if (id == 0)
		return;
	resLock();
	switch (kind) {
	case kIdObj: fTexMap.drop(id); break;
	case kIdPipe: fPipeMap.drop(id); break;
	case kIdFn: fFnMap.drop(id); break;
	case kIdSamp: fSampMap.drop(id); break;
	case kIdRid: fRidMap.drop(id); break;
	}
	resUnlock();
}

void
IOSurfaceRootUserClient::free(void)
{
	fTexMap.destroy();
	fPipeMap.destroy();
	fFnMap.destroy();
	fSampMap.destroy();
	fRidMap.destroy();
	IOUserClient::free();
}

void
IOSurfaceRootUserClient::queueCompletion(uint64_t queue, uint64_t block0, uint64_t block1, uint64_t start, uint64_t end)
{
	uint64_t deadline;

	if (fPendLock == NULL) {
		fPendLock = IOLockAlloc();
	}
	if (fCompletionCall == NULL) {
		// once: two runs at a time would be two writers on one notification ring
		fCompletionCall = thread_call_allocate_with_options(&IOSurfaceRootUserClient::completionFired, this,
		    THREAD_CALL_PRIORITY_HIGH, THREAD_CALL_OPTIONS_ONCE);
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
			bool ok;

			if (pc.block[b] == 0) {
				continue;
			}
			ok = q->enqueue(entry, sizeof(entry));
			if (fTraced)
				IOLog("PD-gputrace: %s completion queue %llu notify %llu key %s block 0x%llx -> %s\n", fTag,
				    (unsigned long long)pc.queue, (unsigned long long)notify,
				    ksym != NULL ? ksym->getCStringNoCopy() : "?", (unsigned long long)pc.block[b], ok ? "ok" : "full");
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

// Snapshot a submission's sideband list (type 1 shmem) of {u32 kind, u32 size, u32 event port,
// u32, u64 value}. Kind 3 signals set the event, kind 4 waits need nothing as execution is in order
void
IOSurfaceRootUserClient::snapshotSignals(OSArray *signals)
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
		snapshotSidebandPair(hdr, (const uint8_t *)fSbPair[i][1]->getBytesNoCopy(),
		    (uint32_t)fSbPair[i][1]->getLength(), signals);
	}
}

void
IOSurfaceRootUserClient::snapshotSidebandPair(const uint8_t *hdr, const uint8_t *list, uint32_t len, OSArray *signals)
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
				OSNumber *signalValue = OSNumber::withNumber(value, 64);
				if (signalValue) {
					signals->setObject(num);
					signals->setObject(signalValue);
					signalValue->release();
				}
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
    const uint32_t *cSamp, const uint32_t *cTg, const uint64_t *grid, const uint64_t *tpg, bool threads)
{
	uint32_t ps = pipeSlot(pipeline), function = pipeInfo(ps).fn;
	IOBufferMemoryDescriptor *bmd[PD_CS_BUFS] = {}, *tmd[16] = {};
	uint64_t tOff[16] = {};

	// a type 0xe pipeline message names a compute pipeline's kernel as tag 1, like a vertex function
	if (function == 0) {
		function = pipeInfo(ps).vertexFn;
	}
	char key[16];

	if (sBridge == NULL || shader(function) == NULL) {
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
	for (uint32_t si = 0; si < 16; si++) {
		r->samp[si] = fSampMap.at(sampSlot(cSamp[si]));
	}
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
	// pdcstex=1: why a dispatch's texture slots go out empty: its first 8 dispatches per function, every
	// dispatch with no texture set and every skipped slot (up to 512 lines)
	static int cstexState = -1;
	static uint8_t cstexSeen[1024];
	static uint32_t cstexBudget = 512;
	if (cstexState < 0) {
		uint32_t on = 0;
		cstexState = PE_parse_boot_argn("pdcstex", &on, sizeof(on)) && on;
	}
	bool cstexNone = true;
	for (uint32_t ti = 0; ti < 16; ti++) {
		if (cTex[ti] != 0) cstexNone = false;
	}
	uint32_t fs = idSlot(kIdFn, function);
	if (fs >= 1024) fs = 0;
	bool cstexLog = cstexState && cstexBudget > 0 && (cstexSeen[fs] < 8 || cstexNone);
	if (cstexLog) {
		if (cstexSeen[fs] < 8) cstexSeen[fs]++;
		cstexBudget--;
		IOLog("PDIOSurface: compute fn %u key %u pipeline %u texture ids %u %u %u %u %u %u %u %u\n", function, r->fn,
		    pipeline, cTex[0], cTex[1], cTex[2], cTex[3], cTex[4], cTex[5], cTex[6], cTex[7]);
	}
	if (cstexState && cstexBudget > 0) cstexLog = true;
	for (uint32_t ti = 0; ti < 16; ti++) {
		uint32_t id = cTex[ti], o = texSlot(id), w, hgt, fmt, bpr;
		uint64_t bytes;

		if (id == 0) continue;
		if (texInfo(o).w == 0) {
			if (cstexLog && cstexBudget > 0) {
				cstexBudget--;
				IOLog("PDIOSurface: compute fn %u key %u slot %u texture %u skipped: %s (resource %u)\n", function,
				    r->fn, ti, id, o == 0 ? "no slot" : "no size known", texInfo(o).res);
			}
			continue;
		}
		// a view's memory is its parent's, a heap texture's its part of the heap: ids are reused, so
		// a buffer left under the texture's own id belongs to whatever object had it before
		tmd[ti] = texMemory(id, &tOff[ti]);
		if (tmd[ti] == NULL) {
			if (cstexLog && cstexBudget > 0) {
				cstexBudget--;
				IOLog("PDIOSurface: compute fn %u key %u slot %u texture %u (%ux%u) skipped: no memory (alias %u surface %u)\n",
				    function, r->fn, ti, id, texInfo(o).w, texInfo(o).h, texInfo(o).alias, texInfo(o).surf);
			}
			continue;
		}
		w = texInfo(o).w; hgt = texInfo(o).h;
		fmt = texFmt(id, tmd[ti]->getLength());
		bpr = w * pdTexBpp(fmt);
		if (texInfo(o).bpr >= bpr && texInfo(o).bpr < 0x10000) bpr = texInfo(o).bpr;
		if (texInfo(o).surf != 0 && sSurfaces[texInfo(o).surf].bytesPerRow >= bpr && sSurfaces[texInfo(o).surf].bytesPerRow < 0x10000) {
			bpr = (uint32_t)sSurfaces[texInfo(o).surf].bytesPerRow;
		}
		bytes = (uint64_t)bpr * hgt;
		if (tOff[ti] > tmd[ti]->getLength() || bytes > tmd[ti]->getLength() - tOff[ti] || pos + bytes > PD_BRIDGE_BYTES) {
			IOLog("PDIOSurface: compute texture %u (%ux%u) does not fit\n", id, w, hgt);
			tmd[ti] = NULL;
			continue;
		}
		r->tex[ti].id = id; r->tex[ti].off = pos; r->tex[ti].w = w; r->tex[ti].h = hgt;
		r->tex[ti].bpr = bpr; r->tex[ti].fmt = fmt;
		memcpy(base + pos, (const uint8_t *)tmd[ti]->getBytesNoCopy() + tOff[ti], bytes);
		pos = (pos + (uint32_t)bytes + 63) & ~63U;
	}
	h->serial = ++sBridgeSerial; h->functionID = r->fn; h->pipelineID = pipeline; h->kind = 3;
	pdHoldMemory(bmd, PD_CS_BUFS, true);
	pdHoldMemory(tmd, 16, true);
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
				memcpy((uint8_t *)tmd[ti]->getBytesNoCopy() + tOff[ti], base + r->tex[ti].off,
				    (uint64_t)r->tex[ti].bpr * r->tex[ti].h);
			}
		}
	} else {
		IOLog("PDIOSurface: compute serial %u failed, state %u: %s\n", h->serial, h->state, h->error);
	}
	pdHoldMemory(bmd, PD_CS_BUFS, false);
	pdHoldMemory(tmd, 16, false);
	pdBridgeRelease(h);
}

void
IOSurfaceRootUserClient::runTile(uint32_t pipeline, const uint32_t *tBuf, const uint64_t *tOff, const uint32_t *tTex,
    const uint32_t *tSamp, const uint32_t *tTg, uint32_t target, const uint32_t *att, const bool *scratch,
    const uint64_t *tpt)
{
	uint32_t function = pipeInfo(pipeSlot(pipeline)).tileFn, to = texSlot(target), tw, th;
	IOBufferMemoryDescriptor *bmd[PD_CS_BUFS] = {}, *tmd[16] = {}, *amd[4] = {};
	uint64_t texOff[16] = {}, attOff[4] = {};
	char key[16];

	if (sBridge == NULL || function == 0 || shader(function) == NULL) {
		IOLog("PDIOSurface: tile dispatch skipped, pipeline %u has no tile kernel (function %u)\n", pipeline, function);
		return;
	}
	if (texInfo(to).w == 0 || texInfo(to).h == 0) {
		IOLog("PDIOSurface: tile dispatch skipped, pipeline %u: no colour attachment 0 (target %u)\n", pipeline, target);
		return;
	}
	if (tpt[0] == 0 || tpt[1] == 0 || tpt[0] * tpt[1] > 1024) {
		IOLog("PDIOSurface: tile dispatch skipped, %llux%llu threads per tile\n", (unsigned long long)tpt[0],
		    (unsigned long long)tpt[1]);
		return;
	}
	PDBridgeGuard bridgeGuard;
	if (!pdBridgeIdle(2000)) {
		IOLog("PDIOSurface: tile dispatch skipped, shader daemon busy or absent\n");
		return;
	}
	PDBridgeHeader *h = (PDBridgeHeader *)sBridge->getBytesNoCopy();
	uint8_t *base = (uint8_t *)h;
	PDTileRequest *r = (PDTileRequest *)(base + PD_DRAW_REQ);
	uint32_t pos = PD_DRAW_DATA;

	tw = texInfo(to).w;
	th = texInfo(to).h;
	bzero(r, sizeof(*r));
	r->fn = shaderKey(function);
	r->mOff = pos; r->mSize = (uint32_t)shader(function)->getLength();
	memcpy(base + pos, shader(function)->getBytesNoCopy(), r->mSize);
	pos = (pos + r->mSize + 63) & ~63U;
	r->tpt[0] = (uint32_t)tpt[0]; r->tpt[1] = (uint32_t)tpt[1]; r->tpt[2] = 1;
	r->tilesX = (tw + PD_TILE_SIZE - 1) / PD_TILE_SIZE;
	r->tilesY = (th + PD_TILE_SIZE - 1) / PD_TILE_SIZE;
	memcpy(r->tgmem, tTg, sizeof(r->tgmem));
	for (uint32_t si = 0; si < 16; si++) {
		r->samp[si] = fSampMap.at(sampSlot(tSamp[si]));
	}
	for (uint32_t bi = 0; bi < PD_CS_BUFS; bi++) {
		uint64_t size;

		if (tBuf[bi] == 0) continue;
		snprintf(key, sizeof(key), "%u", tBuf[bi]);
		bmd[bi] = lookupBuffer(key);
		if (bmd[bi] == NULL || tOff[bi] >= bmd[bi]->getLength()) {
			bmd[bi] = NULL;
			continue;
		}
		size = bmd[bi]->getLength() - tOff[bi];
		if (size > 0x2000000) size = 0x2000000;
		if (pos + size > PD_BRIDGE_BYTES / 2) {
			bmd[bi] = NULL;
			continue;
		}
		r->buf[bi].id = tBuf[bi]; r->buf[bi].off = pos; r->buf[bi].size = (uint32_t)size;
		memcpy(base + pos, (const uint8_t *)bmd[bi]->getBytesNoCopy() + tOff[bi], size);
		pos = (pos + (uint32_t)size + 63) & ~63U;
	}
	for (uint32_t ti = 0; ti < 16; ti++) {
		uint32_t id = tTex[ti], o = texSlot(id), fmt, bpr;
		uint64_t bytes;

		if (id == 0 || texInfo(o).w == 0) continue;
		tmd[ti] = texMemory(id, &texOff[ti]);
		if (tmd[ti] == NULL) continue;
		fmt = texFmt(id, tmd[ti]->getLength());
		bpr = texInfo(o).w * pdTexBpp(fmt);
		if (texInfo(o).bpr >= bpr && texInfo(o).bpr < 0x10000) bpr = texInfo(o).bpr;
		if (texInfo(o).surf != 0 && sSurfaces[texInfo(o).surf].bytesPerRow >= bpr && sSurfaces[texInfo(o).surf].bytesPerRow < 0x10000)
			bpr = (uint32_t)sSurfaces[texInfo(o).surf].bytesPerRow;
		bytes = (uint64_t)bpr * texInfo(o).h;
		if (texOff[ti] > tmd[ti]->getLength() || bytes > tmd[ti]->getLength() - texOff[ti] || pos + bytes > PD_BRIDGE_BYTES / 2) {
			IOLog("PDIOSurface: tile texture %u (%ux%u) does not fit\n", id, texInfo(o).w, texInfo(o).h);
			tmd[ti] = NULL;
			continue;
		}
		r->tex[ti].id = id; r->tex[ti].off = pos; r->tex[ti].w = texInfo(o).w; r->tex[ti].h = texInfo(o).h;
		r->tex[ti].bpr = bpr; r->tex[ti].fmt = fmt;
		memcpy(base + pos, (const uint8_t *)tmd[ti]->getBytesNoCopy() + texOff[ti], bytes);
		pos = (uint32_t)((pos + bytes + 63) & ~63ULL);
	}
	// the implicit imageblock: attachment 0 and attachments 1-3 (scratch when memoryless), as a draw sees them
	for (uint32_t k = 0; k < 4; k++) {
		uint32_t id = k == 0 ? target : att[k], o = texSlot(id), w, hgt, fmt;
		uint64_t bytes;

		if (id == 0) continue;
		if (k > 0 && scratch[k]) {
			amd[k] = fAttScratch[k];
			w = tw; hgt = th; fmt = 1;
		} else {
			if (texInfo(o).w == 0 || texInfo(o).h == 0) continue;
			amd[k] = texMemory(id, &attOff[k]);
			if (amd[k] == NULL) continue;
			w = texInfo(o).w; hgt = texInfo(o).h;
			fmt = texFmt(id, amd[k]->getLength());
		}
		if (amd[k] == NULL) continue;
		bytes = (uint64_t)w * pdTexBpp(fmt) * hgt;
		if (attOff[k] > amd[k]->getLength() || bytes > amd[k]->getLength() - attOff[k] || pos + bytes > PD_BRIDGE_BYTES) {
			IOLog("PDIOSurface: tile attachment %u (texture %u, %ux%u) does not fit\n", k, id, w, hgt);
			amd[k] = NULL;
			continue;
		}
		r->rt[k].id = id; r->rt[k].off = pos; r->rt[k].w = w; r->rt[k].h = hgt;
		r->rt[k].bpr = w * pdTexBpp(fmt); r->rt[k].fmt = fmt;
		memcpy(base + pos, (const uint8_t *)amd[k]->getBytesNoCopy() + attOff[k], bytes);
		pos = (uint32_t)((pos + bytes + 63) & ~63ULL);
	}
	h->serial = ++sBridgeSerial; h->functionID = r->fn; h->pipelineID = pipeline; h->kind = 4;
	pdHoldMemory(bmd, PD_CS_BUFS, true);
	pdHoldMemory(tmd, 16, true);
	pdHoldMemory(amd, 4, true);
	pdBridgeRing(h);
	pdBridgeWait(h, 60000);
	__sync_synchronize();
	if (h->state == 2) {
		for (uint32_t bi = 0; bi < PD_CS_BUFS; bi++) {
			if (bmd[bi] != NULL && r->buf[bi].size)
				memcpy((uint8_t *)bmd[bi]->getBytesNoCopy() + tOff[bi], base + r->buf[bi].off, r->buf[bi].size);
		}
		for (uint32_t ti = 0; ti < 16; ti++) {
			if (tmd[ti] != NULL && r->tex[ti].w)
				memcpy((uint8_t *)tmd[ti]->getBytesNoCopy() + texOff[ti], base + r->tex[ti].off,
				    (uint64_t)r->tex[ti].bpr * r->tex[ti].h);
		}
		for (uint32_t k = 0; k < 4; k++) {
			if (amd[k] != NULL && r->rt[k].w)
				memcpy((uint8_t *)amd[k]->getBytesNoCopy() + attOff[k], base + r->rt[k].off,
				    (uint64_t)r->rt[k].bpr * r->rt[k].h);
		}
	} else {
		IOLog("PDIOSurface: tile serial %u failed, state %u: %s\n", h->serial, h->state, h->error);
	}
	pdHoldMemory(bmd, PD_CS_BUFS, false);
	pdHoldMemory(tmd, 16, false);
	pdHoldMemory(amd, 4, false);
	pdBridgeRelease(h);
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
		uint32_t currentPipeline = 0, cBuf[PD_CS_BUFS] = {}, cTex[16] = {}, cSamp[16] = {}, cTg[4] = {};
		uint64_t cOff[PD_CS_BUFS] = {};
		// Render encoder state: pipeline, vertex and fragment buffers, fragment textures
		uint32_t rPipeline = 0, vBuf[PD_DRAW_BUFS] = {}, fBuf[PD_DRAW_BUFS] = {}, fTex[16] = {}, fSamp[16] = {}, rTarget = 0;
		uint32_t rAtt[4] = {};	// the pass's colour attachments 1-3 (RenderBox: coverage 1, layer 2)
		bool rScratch[4] = {};	// attachment k has no memory of the target's size: it uses fAttScratch[k]
		uint64_t vOff[PD_DRAW_BUFS] = {}, fOff[PD_DRAW_BUFS] = {};
		// the tile stage's bindings (0x9c-0xa1), used by dispatchThreadsPerTile (0x9b)
		uint32_t tBuf[PD_CS_BUFS] = {}, tTex[16] = {}, tSamp[16] = {}, tTg[4] = {};
		uint64_t tOff[PD_CS_BUFS] = {};
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
			} else if (op == 0x1c2 && clen >= 0x18) {
				// compute pipeline info: {pipeline, buffer, u64 offset} -> {u64 maxTotalThreadsPerThreadgroup,
				// threadExecutionWidth, staticThreadgroupMemoryLength, 0}, left zero, MPS dispatches empty grids
				uint32_t pso = 0, qbuf = 0;
				uint64_t qoff = 0, ans[4] = { 1024, 32, 0, 0 };

				memcpy(&pso, p + off + 8, 4);
				memcpy(&qbuf, p + off + 12, 4);
				memcpy(&qoff, p + off + 16, 8);
				if (pipeInfo(pipeSlot(pso)).maxThreads != 0)
					ans[0] = pipeInfo(pipeSlot(pso)).maxThreads;
				answerQuery(qbuf, qoff, ans, sizeof(ans));
			} else if (op == 0xa4 && clen >= 0x14) {
				// the render encoder's tileWidth/tileHeight: {buffer, u64 offset} -> {u16 32, u16 32}
				uint32_t qbuf = 0;
				uint64_t qoff = 0;
				uint16_t tile[2] = { 32, 32 };

				memcpy(&qbuf, p + off + 8, 4);
				memcpy(&qoff, p + off + 12, 8);
				answerQuery(qbuf, qoff, tile, sizeof(tile));
			} else if (op == 0x1d5 && clen >= 0x3c) {
				// heapTextureSizeAndAlignWithDescriptor: {0x28 byte descriptor, buffer, u64 offset} -> {u64 size,
				// u64 align}, for the linear rows a heap texture (sel 261 kind 0x38) gets here
				uint32_t qbuf = 0;
				uint64_t qoff = 0, ans[2];

				pdHeapTexSize(p + off + 8, ans);
				memcpy(&qbuf, p + off + 0x30, 4);
				memcpy(&qoff, p + off + 0x34, 8);
				answerQuery(qbuf, qoff, ans, sizeof(ans));
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
			} else if (op == 0xcc && clen >= 0x14) {
				// setSamplerState {index, kind, sampler id}, laid out like 0xce
				uint32_t index = 0, id = 0, w[4] = {};
				static uint32_t logged;

				memcpy(&index, p + off + 8, 4);
				memcpy(&id, p + off + 16, 4);
				if (index < 16) cSamp[index] = id;
				if (logged < 8) {
					logged++;
					memcpy(w, p + off + 8, clen - 8 < sizeof(w) ? clen - 8 : sizeof(w));
					IOLog("PDIOSurface: compute sampler bind len 0x%x %08x %08x %08x %08x flags 0x%x\n", clen, w[0], w[1],
					    w[2], w[3], fSampMap.at(sampSlot(id)));
				}
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
					runCompute(currentPipeline, cBuf, cOff, cTex, cSamp, cTg, grid, tpg, false);
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
						if (texInfo(texSlot(cTex[ti])).h > ext)
							ext = texInfo(texSlot(cTex[ti])).h;
					}
					tpg[1] = tpg[0] != 0 && tpg[0] < 1024 ? 1024 / tpg[0] : 1;
					grid[1] = op == 0xca ? ext : (ext + tpg[1] - 1) / tpg[1];
				}
				runCompute(currentPipeline, cBuf, cOff, cTex, cSamp, cTg, grid, tpg, op == 0xca);
			} else if (op == 0xd8 || op == 0xdb || op == 0xd7 || op == 0xd1 || op == 0x86) {
				// imageblock size, encoder begin and end
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
				// a pass without colour attachment 0 (depth or stencil only) must not draw into the last pass's target
				rTarget = 0;
				for (uint32_t k = 0; k < 4; k++) {
					const uint8_t *a = p + off + 0x50 + k * 0x3c;
					uint32_t id;
					uint16_t load;
					uint64_t c[4], toff = 0, tlen = 0;
					IOBufferMemoryDescriptor *tex;

					memcpy(&id, a + 4, 4);
					memcpy(&load, a + 0x18, 2);
					memcpy(c, a + 0x20, sizeof(c));
					if (k > 0)
						rAtt[k] = id;
					// an attachment without memory of attachment 0's size is memoryless: scratch, cleared
					if (k > 0 && rAtt[k] != 0 && rTarget != 0) {
						IOBufferMemoryDescriptor *have;
						uint32_t to = texSlot(rTarget), ao = texSlot(rAtt[k]);
						uint64_t need = (uint64_t)texInfo(to).w * texInfo(to).h * 8, hoff;

						have = texMemory(rAtt[k], &hoff);
						if (have == NULL || texInfo(ao).w != texInfo(to).w || texInfo(ao).h != texInfo(to).h) {
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
					if (id == 0) {
						continue;
					}
					if (k == 0)
						rTarget = id;
					markUsed(id, true);
					tex = texMemory(id, &toff);
					// a texture on shared memory (a heap) owns only its own rows of it
					if (tex != NULL && toff < tex->getLength()) {
						tlen = tex->getLength() - toff;
						uint32_t o = texSlot(id);

						if (texInfo(o).alias != 0 && texInfo(o).bpr != 0 && (uint64_t)texInfo(o).bpr * texInfo(o).h < tlen)
							tlen = (uint64_t)texInfo(o).bpr * texInfo(o).h;
					}
					// dontCare (0) leaves the contents undefined: zeros, so last frame never shows through
					if (tlen != 0 && load == 0) {
						bzero((uint8_t *)tex->getBytesNoCopy() + toff, tlen);
					} else if (tlen != 0 && load == 2) {
						// in the texture's own format: half RGBA, BGRA8, RGBA8, or one byte
						uint32_t fmt = texFmt(id, tex->getLength());
						uint8_t px[16] = {};
						uint32_t bpp = pdTexBpp(fmt);
						uint8_t *t = (uint8_t *)tex->getBytesNoCopy() + toff;

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
						} else if (fmt >= 6 && fmt <= 11) {
							// integer formats: RG16, R32, RG32, RGBA16, R16, RGBA8 unsigned lanes
							static const uint8_t lanes[] = { 2, 1, 2, 4, 1, 4 }, width[] = { 2, 4, 4, 2, 2, 1 };
							uint32_t n = lanes[fmt - 6], w = width[fmt - 6];

							for (uint32_t ch = 0; ch < n; ch++) {
								uint32_t v = uintFromDoubleBits(c[ch], w == 4 ? 0xffffffffU : (1U << (w * 8)) - 1);
								memcpy(px + ch * w, &v, w);
							}
						} else if (fmt == 12 || fmt == 13) {
							for (uint32_t ch = 0; ch < fmt - 11; ch++) {
								uint16_t h = halfFromDoubleBits(c[ch]);
								memcpy(px + ch * 2, &h, 2);
							}
						} else {
							for (int ch = 0; ch < 4; ch++) {
								px[ch == 0 ? 2 : (ch == 2 ? 0 : ch)] = unorm8FromDoubleBits(c[ch]);
							}
						}
						for (uint64_t i = 0; i + bpp <= tlen; i += bpp) {
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
				if (tex != NULL && texInfo(texSlot(tid)).w != 0 && texInfo(texSlot(tid)).h != 0) {
					uint8_t *base = (uint8_t *)tex->getBytesNoCopy();
					uint64_t len = tex->getLength(), at = 0;
					uint32_t w = texInfo(texSlot(tid)).w, h = texInfo(texSlot(tid)).h;

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
				uint64_t sbase = 0, dbase = 0;
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
				} else {
					w = texInfo(texSlot(src)).w;
					h = texInfo(texSlot(src)).h;
				}
				sb = texMemory(src, &sbase);
				db = texMemory(dst, &dbase);
				if (sb != NULL && db != NULL && texInfo(texSlot(src)).w != 0 && texInfo(texSlot(dst)).w != 0) {
					// each side in its own format and row pitch: copied as packed BGRA8 rows, a half-float or
					// padded surface source moved half a row at the wrong pitch (black backdrops)
					uint32_t sbpp = pdTexBpp(texFmt(src, sb->getLength())), dbpp = pdTexBpp(texFmt(dst, db->getLength()));
					uint32_t ss = texSlot(src), ds = texSlot(dst);
					uint32_t sw = texInfo(ss).w, sh = texInfo(ss).h, dw = texInfo(ds).w, dh = texInfo(ds).h;
					uint64_t so = sbase, doff = dbase, spitch = texPitch(src, sbpp), dpitch = texPitch(dst, dbpp);

					// level n of a packed chain starts after levels 0..n-1 (after the texture's own offset)
					for (uint16_t l = 0; l < lv[1] && (sw > 1 || sh > 1); l++) {
						so += spitch * sh; sw = sw > 1 ? sw / 2 : 1; sh = sh > 1 ? sh / 2 : 1; spitch = (uint64_t)sw * sbpp;
					}
					for (uint16_t l = 0; l < lv[3] && (dw > 1 || dh > 1); l++) {
						doff += dpitch * dh; dw = dw > 1 ? dw / 2 : 1; dh = dh > 1 ? dh / 2 : 1; dpitch = (uint64_t)dw * dbpp;
					}
					if (sbpp == dbpp && w <= sw && sx <= sw - w && h <= sh && sy <= sh - h && w <= dw && dx <= dw - w &&
					    h <= dh && dy <= dh - h && h != 0 && so + spitch * (sy + h - 1) + (sx + w) * sbpp <= sb->getLength() &&
					    doff + dpitch * (dy + h - 1) + (dx + w) * dbpp <= db->getLength()) {
						for (uint64_t y = 0; y < h; y++) {
							memcpy((uint8_t *)db->getBytesNoCopy() + doff + (dy + y) * dpitch + dx * dbpp,
							    (const uint8_t *)sb->getBytesNoCopy() + so + (sy + y) * spitch + sx * sbpp, w * sbpp);
						}
					} else if (h != 0) {
						IOLog("PDIOSurface: %s copyTexture %u (%ux%u, %u B/px) -> %u (%ux%u, %u B/px) %llux%llu rejected\n", fTag,
						    src, sw, sh, sbpp, dst, dw, dh, dbpp, (unsigned long long)w, (unsigned long long)h);
					}
				}
			} else if (op == 0x12c && clen >= 0x50) {
				// copyFromBuffer:...toTexture: rows of the source into the texture, in its format
				uint32_t src, dst;
				uint64_t soff, bpr, w, h, ox, oy, tbase = 0;
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
				tex = texMemory(dst, &tbase);
				uint32_t tbpp = tex != NULL ? pdTexBpp(texFmt(dst, tex->getLength())) : 4, ds = texSlot(dst);
				uint64_t tpitch = texPitch(dst, tbpp);

				if (sb != NULL && tex != NULL && texInfo(ds).w != 0 && w <= texInfo(ds).w &&
				    ox <= texInfo(ds).w - w && h <= texInfo(ds).h && oy <= texInfo(ds).h - h && bpr >= w * tbpp &&
				    soff <= sb->getLength() && h * bpr <= sb->getLength() - soff && h != 0 &&
				    tbase + tpitch * (oy + h - 1) + (ox + w) * tbpp <= tex->getLength()) {
					for (uint64_t y = 0; y < h; y++) {
						const uint8_t *row = (const uint8_t *)sb->getBytesNoCopy() + soff + y * bpr;

						memcpy((uint8_t *)tex->getBytesNoCopy() + tbase + (oy + y) * tpitch + ox * tbpp, row, w * tbpp);
					}
				} else {
					IOLog("PDIOSurface: %s copyBufferToTexture %u -> %u rejected\n", fTag, src, dst);
				}
			} else if (op == 0x12e && clen >= 0x58) {
				// copyFromTexture:...toBuffer: the texture's rows out, in its format
				uint32_t src, dst;
				uint64_t ox, oy, w, h, doff, bpr, tbase = 0;
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
				tex = texMemory(src, &tbase);
				snprintf(key, sizeof(key), "%u", dst);
				db = lookupBuffer(key);
				uint32_t tbpp = tex != NULL ? pdTexBpp(texFmt(src, tex->getLength())) : 4, ss = texSlot(src);
				uint64_t tpitch = texPitch(src, tbpp);

				if (tex != NULL && db != NULL && texInfo(ss).w != 0 && w <= texInfo(ss).w &&
				    ox <= texInfo(ss).w - w && h <= texInfo(ss).h && oy <= texInfo(ss).h - h && bpr >= w * tbpp &&
				    doff <= db->getLength() && h * bpr <= db->getLength() - doff && h != 0 &&
				    tbase + tpitch * (oy + h - 1) + (ox + w) * tbpp <= tex->getLength()) {
					for (uint64_t y = 0; y < h; y++) {
						memcpy((uint8_t *)db->getBytesNoCopy() + doff + y * bpr,
						    (const uint8_t *)tex->getBytesNoCopy() + tbase + (oy + y) * tpitch + ox * tbpp, w * tbpp);
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
					// 0x09 is {prim, index buffer, u16 count, u16 byte offset, u16 instances, u16 often 0x100}.
					// RenderBox's index buffer holds 0 1 2 3 .. at 0 and quad strips 1 0 2 3 ffff .. at +0x100
					if (op == 0x09 && clen >= 0x18) {
						uint16_t inst = 0;

						memcpy(&inst, p + off + 20, 2);
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
					uint32_t rps = pipeSlot(rPipeline), rto = texSlot(rTarget);
					uint32_t vfn = pipeInfo(rps).vertexFn, ffn = pipeInfo(rps).fragmentFn;
					char key[16];
					IOBufferMemoryDescriptor *tgt;
					uint64_t tgtOff = 0;
					const char *missing = NULL;
					uint32_t missingId = 0;

					// a target made on a surface draws into the surface, one in a heap into its part of the heap
					tgt = texMemory(rTarget, &tgtOff);
					if (sBridge == NULL) {
						missing = "no shader daemon";
					} else if (shader(vfn) == NULL) {
						missing = "vertex function unknown:"; missingId = vfn;
					} else if (ffn == 0) {
						missing = "pipeline has no fragment function";
					} else if (shader(ffn) == NULL) {
						missing = "fragment function unknown:"; missingId = ffn;
					} else if (rTarget == 0) {
						missing = "no colour attachment 0";
					} else if (texInfo(rto).w == 0 || texInfo(rto).h == 0) {
						missing = "target size unknown";
					} else if (tgt == NULL) {
						missing = "target memory unknown, alias"; missingId = texInfo(rto).alias;
					} else if (prim > 4) {
						missing = "primitive not a point, line or triangle type";
					}
					if (missing != NULL) {
						IOLog("PDIOSurface: draw skipped, pipeline %u target %u prim %u incomplete: %s %u\n", rPipeline,
						    rTarget, prim, missing, missingId);
					} else if (!pdBridgeIdle(2000)) {
						IOLog("PDIOSurface: draw skipped, shader daemon busy or absent\n");
					} else {
						PDBridgeHeader *h = (PDBridgeHeader *)sBridge->getBytesNoCopy();
						uint8_t *base = (uint8_t *)h;
						PDDrawRequest *r = (PDDrawRequest *)(base + PD_DRAW_REQ);
						uint32_t pos = PD_DRAW_DATA, tw = texInfo(rto).w, th = texInfo(rto).h;
						uint32_t tfmt = texFmt(rTarget, tgt->getLength());
						uint32_t tbpr = tw * pdTexBpp(tfmt);
						uint64_t tbytes = (uint64_t)tbpr * th;
						uint64_t rtOff[4] = { 0, 0, 0, 0 };	// where attachments 1-3 start in their memory

						if (tgtOff > tgt->getLength() || tbytes > tgt->getLength() - tgtOff) {
							IOLog("PDIOSurface: draw target %u (%ux%u fmt %u) is larger than its buffer: off 0x%llx of 0x%llx, alias %u surf %u\n",
							    rTarget, tw, th, tfmt, (unsigned long long)tgtOff, (unsigned long long)tgt->getLength(),
							    texInfo(rto).alias, texInfo(rto).surf);
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
								uint32_t id = fTex[ti], o = texSlot(id);
								IOBufferMemoryDescriptor *tmd;
								uint32_t w, hgt, fmt, bpr;
								uint64_t bytes, toff;

								if (id == 0) continue;
								if (texInfo(o).w == 0) {
									IOLog("PDIOSurface: draw texture slot %u id %u dropped, size unknown\n", ti, id);
									continue;
								}
								// a view, buffer or heap texture reads the memory it was made on (from its offset),
								// a surface texture the surface: never a buffer left under its reused id
								tmd = texMemory(id, &toff);
								if (tmd == NULL) {
									IOLog("PDIOSurface: draw texture slot %u id %u dropped, memory unknown, alias %u surf %u\n", ti, id,
									    texInfo(o).alias, texInfo(o).surf);
									continue;
								}
								w = texInfo(o).w; hgt = texInfo(o).h;
								fmt = texFmt(id, tmd->getLength());
								bpr = w * pdTexBpp(fmt);
								if (texInfo(o).bpr >= bpr && texInfo(o).bpr < 0x10000) {
									bpr = texInfo(o).bpr;
								}
								// A surface pads its rows: take its own pitch
								if (texInfo(o).surf != 0 && sSurfaces[texInfo(o).surf].bytesPerRow >= bpr &&
								    sSurfaces[texInfo(o).surf].bytesPerRow < 0x10000) {
									bpr = (uint32_t)sSurfaces[texInfo(o).surf].bytesPerRow;
								}
								bytes = (uint64_t)bpr * hgt;
								// the last row needs only its pixels: an offset texture may end short of a full pitch
								if (toff + bytes > tmd->getLength() && hgt != 0 &&
								    toff + (uint64_t)bpr * (hgt - 1) + (uint64_t)w * pdTexBpp(fmt) <= tmd->getLength())
									bytes = tmd->getLength() - toff;
								if (toff + bytes > tmd->getLength() || bytes > 0x2000000 || pos + bytes > PD_BRIDGE_BYTES / 2) {
									IOLog("PDIOSurface: draw texture %u (%ux%u bpr %u) does not fit: off 0x%llx of 0x%llx, alias %u\n",
									    id, w, hgt, bpr, (unsigned long long)toff, (unsigned long long)tmd->getLength(),
									    texInfo(o).alias);
									continue;
								}
								r->ftex[ti].id = id; r->ftex[ti].off = pos; r->ftex[ti].w = w; r->ftex[ti].h = hgt;
								r->ftex[ti].bpr = bpr; r->ftex[ti].fmt = fmt;
								memcpy(base + pos, (const uint8_t *)tmd->getBytesNoCopy() + toff, bytes);
								pos = (uint32_t)((pos + bytes + 63) & ~63ULL);
							}
							r->prim = prim; r->count = a; r->indexed = (op == 0x07 || op == 0x09 || op == 0x06);
							for (uint32_t si = 0; si < 16; si++) {
								r->samp[si] = fSampMap.at(sampSlot(fSamp[si]));
							}
							r->instances = instances > 4096 ? 4096 : instances;
							r->pad0 = pipeInfo(rps).blend;   // blend state for the daemon
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
							memcpy(base + pos, (const uint8_t *)tgt->getBytesNoCopy() + tgtOff, tbytes);
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
										uint32_t ao = texSlot(aid);

										if (texInfo(ao).w == 0 || texInfo(ao).h == 0) continue;
										amd = texMemory(aid, &rtOff[k]);
										if (amd == NULL) continue;
										aw = texInfo(ao).w; ah = texInfo(ao).h;
										afmt = texFmt(aid, amd->getLength());
									}
									if (amd == NULL) continue;
									abpr = aw * pdTexBpp(afmt);
									abytes = (uint64_t)abpr * ah;
									if (rtOff[k] > amd->getLength() || abytes > amd->getLength() - rtOff[k] ||
									    apos + abytes > PD_BRIDGE_BYTES) continue;
									r->rt[k].id = aid; r->rt[k].off = apos; r->rt[k].w = aw; r->rt[k].h = ah;
									r->rt[k].bpr = abpr; r->rt[k].fmt = afmt;
									r->rtBlend[k] = pipeInfo(rps).blendRT[k];
									memcpy(base + apos, (const uint8_t *)amd->getBytesNoCopy() + rtOff[k], abytes);
									apos = (uint32_t)((apos + abytes + 63) & ~63ULL);
								}
							}
							memcpy(r->viewport, viewport, sizeof(viewport));
							if (r->viewport[2] == 0) { r->viewport[2] = tw; r->viewport[3] = th; }
							h->serial = ++sBridgeSerial; h->functionID = r->vfn; h->pipelineID = rPipeline; h->kind = 2;
							pdHoldMemory(&tgt, 1, true);
							pdBridgeRing(h);
							// cold Vulkan pipelines can take over a minute, retain the first frame.
							pdBridgeWait(h, 180000);
							__sync_synchronize();
							if (h->state == 2) {
								memcpy((uint8_t *)tgt->getBytesNoCopy() + tgtOff, base + pos, tbytes);
								for (uint32_t k = 1; k < 4; k++) {
									IOBufferMemoryDescriptor *amd;
									uint64_t abytes = (uint64_t)r->rt[k].bpr * r->rt[k].h, aoff = 0;

									if (r->rt[k].w == 0) continue;
									amd = rScratch[k] ? fAttScratch[k] : texMemory(r->rt[k].id, &aoff);
									if (amd != NULL && aoff <= amd->getLength() && abytes <= amd->getLength() - aoff)
										memcpy((uint8_t *)amd->getBytesNoCopy() + aoff, base + r->rt[k].off, abytes);
								}
							} else {
								IOLog("PDIOSurface: draw serial %u failed, state %u: %s\n", h->serial, h->state, h->error);
							}
							pdHoldMemory(&tgt, 1, false);
							pdBridgeRelease(h);
						} else {
							IOLog("PDIOSurface: draw skipped, target %u (%ux%u) too large or daemon busy\n", rTarget, tw, th);
						}
					}
				}
			} else if (op == 0x9d && clen >= 0x1c) {
				// setTileBuffer(s) and setTileBytes (staged in a buffer): {index, count, ids, u64 offsets}
				uint32_t index = 0, n = 0;

				memcpy(&index, p + off + 8, 4);
				memcpy(&n, p + off + 12, 4);
				for (uint32_t k = 0; k < n && index + k < PD_CS_BUFS && 16 + 4 * n + 8 * (k + 1) <= clen; k++) {
					memcpy(&tBuf[index + k], p + off + 16 + 4 * k, 4);
					memcpy(&tOff[index + k], p + off + 16 + 4 * n + 8 * k, 8);
				}
			} else if (op == 0x9e && clen >= 0x14) {
				// setTileBufferOffset {index, u64 offset}
				uint32_t index = 0;

				memcpy(&index, p + off + 8, 4);
				if (index < PD_CS_BUFS)
					memcpy(&tOff[index], p + off + 12, 8);
			} else if ((op == 0x9f || op == 0xa0 || op == 0xa1) && clen >= 0x14) {
				// tile sampler (0x9f, 0xa0 with LOD clamps after it) and texture (0xa1): {index, count, ids}
				uint32_t index = 0, n = 0, *dst = op == 0xa1 ? tTex : tSamp;

				memcpy(&index, p + off + 8, 4);
				memcpy(&n, p + off + 12, 4);
				for (uint32_t k = 0; k < n && index + k < 16 && 16 + 4 * (k + 1) <= clen; k++) {
					memcpy(&dst[index + k], p + off + 16 + 4 * k, 4);
				}
			} else if (op == 0x9c && clen >= 0x1c) {
				// setThreadgroupMemoryLength:offset:atIndex: {u64 length, u64 offset, u32 index}
				uint64_t len = 0;
				uint32_t index = 0;

				memcpy(&len, p + off + 8, 8);
				memcpy(&index, p + off + 0x18, 4);
				if (index < 4)
					tTg[index] = len < 0x10000 ? (uint32_t)len : 0x10000;
			} else if (op == 0x9b && clen >= 0x20) {
				// dispatchThreadsPerTile {u64 width, height, depth}: the tile pipeline set by 0x74
				uint64_t tpt[3];

				memcpy(tpt, p + off + 8, sizeof(tpt));
				for (uint32_t k = 0; k < PD_CS_BUFS; k++) {
					markUsed(tBuf[k], false);
				}
				for (uint32_t k = 0; k < 16; k++) {
					markUsed(tTex[k], true);
				}
				markUsed(rTarget, true);
				runTile(rPipeline, tBuf, tOff, tTex, tSamp, tTg, rTarget, rAtt, rScratch, tpt);
			} else if (op == 0x75) {
				// scissor, always the full target so far
			} else {
				IOLog("PDIOSurface: %s unknown opcode 0x%x, 0x%x bytes\n", fTag, op, clen);
			}
			off += clen;
		}
	}
}
